//===-- BPFUnwind.cpp - Lower unwinding to unwind_goto and unwind_exit ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// There are two ways to leave a frame, and a call that may unwind is followed
// by an insn that tells them apart:
//   ret                               exit
//   invoke f() unwind pad             call f; unwind_goto pad
//   resume                            r0 = exn; unwind_exit
//   call _Unwind_RaiseException(ue)   r0 = ue; unwind_exit
//   invoke _Unwind_RaiseException(ue) r0 = ue; goto pad
// unwind_goto is taken when the callee left with unwind_exit. No register says
// so: how one insn tells the other is up to the JIT. Otherwise unwind_exit is
// exit, hence r0, and r2 as well where the return value takes r0 and r2: the
// verifier checks every register of it at both.
//
// The exception object travels in r0: unwind_exit returns it as exit returns
// a value, and a pad finds it there, in the exception pointer register. It is
// what was passed to _Unwind_RaiseException(), and a pad that resumes passes on
// what it got. A program is no special case: out of it, unwind_exit is exit,
// and the verifier's checks of the program's return value keep the kernel
// safe. The compiler does not pick a return value for an exception that
// escapes: 0 means one thing to one program type and another to the next
// (XDP_ABORTED, drop for cgroup skb, allow for LSM, CPU 0 for sched_ext's
// select_cpu), and which functions are programs is the loader's convention.
//
// _Unwind_RaiseException() is how a language runtime starts an unwind with the
// Itanium ABI, e.g. at the end of a panic handler. To start one and to carry
// one on is the same thing here: leave the frame with unwind_exit, after its
// own pad if it has one. It cannot fail, so it does not return.
//
// The kernel unwinds, so no personality routine runs between the frames, as
// none does in WebAssembly, where the VM unwinds: every pad is entered. A pad
// with catch clauses for types calls the personality routine itself, as a
// Wasm catch does after WasmEHPrepare, and the tables are the same as there,
// the standard LSDA with a call-site table keyed by landing pad index rather
// than by address, since the personality is told which pad it is in instead
// of where the call was. The pad stores its index and the address of the
// function's table into the landing pad context of the unwinding library
// (libunwind's Unwind-bpf.c), calls the personality with the exception
// object, and reads the selector back; the dispatch on it is the frontend's,
// as on every target. A pad with only cleanups or a catch-all needs no
// selector from a table and makes no call. A filter with types, of a dynamic
// exception specification, is not supported.
//
//===----------------------------------------------------------------------===//

#include "BPF.h"
#include "BPFInstrInfo.h"
#include "BPFSubtarget.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/EHPersonalities.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsBPF.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/RuntimeLibcalls.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

/// On other targets it is a function of the unwinder library, which is why
/// nothing else in LLVM knows the name.
static constexpr StringLiteral UnwindRaise = "_Unwind_RaiseException";

/// Not being nounwind is the default in IR, so that alone is not enough:
/// uwtable is how a frontend asks for unwinding through a function to work.
static bool canUnwind(const Function &F) {
  return F.hasUWTable() && !F.doesNotThrow();
}

bool llvm::isBPFUnwindCallee(const GlobalValue &Callee,
                             const TargetLowering &TLI) {
  // Code that defines them gets what it defines.
  if (!Callee.isDeclaration())
    return false;
  StringRef Name = Callee.getName();
  return Name == UnwindRaise ||
         Name == StringRef(TLI.getLibcallName(RTLIB::UNWIND_RESUME));
}

namespace {

/// Make every call that may unwind an invoke: it ends its block and has a pad.
/// Have every pad with catch clauses for types ask the personality routine
/// for its selector, as WasmEHPrepare has a catchpad ask.
struct BPFUnwindPrepare {
  /// libunwind's struct _Unwind_LandingPadContext, which Unwind-bpf.c
  /// defines with uintptr_t fields: the pad's index and the LSDA's address,
  /// in; the selector, out.
  static StructType *getLPadContextType(LLVMContext &C) {
    Type *I64 = Type::getInt64Ty(C);
    return StructType::get(I64, PointerType::getUnqual(C), I64);
  }

  /// The personality routine has run in no frame between the throw and the
  /// pad, so the pad has it run here: it stores its index and the function's
  /// LSDA into the landing pad context, calls the personality with the
  /// exception object, and reads the selector the personality set. The
  /// frontend compares the selector with the type ids of its handlers, and
  /// those of enclosing try blocks in the same function, whose clauses the
  /// landingpad lists after its own; that is what the LSDA encodes.
  bool preparePad(LandingPadInst &LP, unsigned &NextIndex) {
    Function &F = *LP.getFunction();
    Module &M = *F.getParent();
    LLVMContext &C = M.getContext();

    // A catch-all or an empty filter takes everything, so the clauses after
    // it never match; with no clause for a type, the selector needs no
    // table.
    bool Typed = false;
    enum { NoMatch, CatchAll, EmptyFilter } Otherwise = NoMatch;
    for (unsigned I = 0, E = LP.getNumClauses(); I != E && !Otherwise; ++I) {
      Constant *Clause = LP.getClause(I);
      if (LP.isFilter(I)) {
        // An empty filter lets nothing through, so it is a catch-all too. rustc
        // guards every function that must not unwind with one.
        if (!cast<ArrayType>(Clause->getType())->getNumElements()) {
          Otherwise = EmptyFilter;
          continue;
        }
        F.getContext().diagnose(DiagnosticInfoUnsupported(
            F,
            "BPF does not support exception filters with types, as of "
            "a dynamic exception specification",
            LP.getDebugLoc()));
        return false;
      }
      if (isa<ConstantPointerNull>(Clause->stripPointerCasts()))
        Otherwise = CatchAll;
      else
        Typed = true;
    }

    // A pad is entered also when no clause matches, as for a cleanup.
    // DwarfEHPrepare drops a resume that only pads without one reach.
    LP.setCleanup(true);
    auto *Ty = dyn_cast<StructType>(LP.getType());
    if (LP.use_empty() || !Ty || Ty->getNumElements() != 2 ||
        !Ty->getElementType(1)->isIntegerTy())
      return true;
    Type *SelTy = Ty->getElementType(1);

    // The calls need a location in a function with debug info.
    DebugLoc Loc = LP.getDebugLoc();
    if (!Loc)
      if (DISubprogram *SP = F.getSubprogram())
        Loc = DILocation::get(C, 0, 0, SP);
    IRBuilder<> B(std::next(LP.getIterator()));
    B.SetCurrentDebugLocation(Loc);
    Value *Exn = B.CreateExtractValue(&LP, 0, "exn");

    Value *Sel;
    if (!Typed) {
      switch (Otherwise) {
      case CatchAll:
        Sel = B.CreateCall(Intrinsic::getOrInsertDeclaration(
                               &M, Intrinsic::eh_typeid_for,
                               {PointerType::getUnqual(C)}),
                           {ConstantPointerNull::get(PointerType::getUnqual(C))});
        Sel = B.CreateIntCast(Sel, SelTy, true);
        break;
      case EmptyFilter:
        // Negative, as the selector of a filter is.
        Sel = ConstantInt::getSigned(SelTy, -1);
        break;
      case NoMatch:
        Sel = ConstantInt::get(SelTy, 0);
        break;
      }
    } else {
      EHPersonality Pers = classifyEHPersonality(F.getPersonalityFn());
      if (Pers != EHPersonality::BPF_CXX) {
        F.getContext().diagnose(DiagnosticInfoUnsupported(
            F,
            "BPF needs " + getEHPersonalityName(EHPersonality::BPF_CXX) +
                " as the personality of a function with a catch clause for "
                "a type",
            LP.getDebugLoc()));
        return false;
      }
      unsigned Index = NextIndex++;
      Type *Ptr = PointerType::getUnqual(C);
      StructType *CtxTy = getLPadContextType(C);

      // The index, for the instruction selector to record for the table's
      // emitter. Pseudocode: bpf.landingpad.index(Index);
      B.CreateCall(
          Intrinsic::getOrInsertDeclaration(&M, Intrinsic::bpf_landingpad_index),
          {B.getInt32(Index)});

      // Pseudocode: __bpf_lpad_context.lpad_index = Index;
      Function *GetContext = getOrCreateBPFLibcall(
          &M, RTLIB::RuntimeLibcallsInfo::getLibcallImplName(
                  RTLIB::impl__Unwind_GetBPFLPadContext));
      GetContext->setDoesNotThrow();
      CallInst *Ctx = B.CreateCall(GetContext, {}, "lpad.context");
      Ctx->setDoesNotThrow();
      B.CreateStore(B.getInt64(Index), Ctx);

      // Pseudocode: __bpf_lpad_context.lsda = bpf.lsda();
      Value *LSDA = B.CreateCall(
          Intrinsic::getOrInsertDeclaration(&M, Intrinsic::bpf_lsda), {},
          "lsda");
      B.CreateStore(LSDA,
                    B.CreateConstInBoundsGEP2_32(CtxTy, Ctx, 0, 1, "lsda.slot"));

      // Pseudocode: personality_fn(exn);
      Function *Personality =
          getOrCreateBPFLibcall(&M, getEHPersonalityName(Pers));
      Personality->setDoesNotThrow();
      CallInst *Call = B.CreateCall(
          FunctionCallee(FunctionType::get(Type::getInt32Ty(C), {Ptr}, false),
                         Personality),
          {Exn});
      Call->setDoesNotThrow();

      // Pseudocode: selector = __bpf_lpad_context.selector;
      Value *SelSlot =
          B.CreateConstInBoundsGEP2_32(CtxTy, Ctx, 0, 2, "selector.slot");
      Sel = B.CreateIntCast(
          B.CreateLoad(Type::getInt64Ty(C), SelSlot, "selector"), SelTy, true);
    }

    // The selector register is r0, as the exception pointer register is, and
    // nothing may read it for the selector.
    Value *Val = B.CreateInsertValue(PoisonValue::get(Ty), Exn, 0);
    Val = B.CreateInsertValue(Val, Sel, 1);
    LP.replaceUsesWithIf(Val, [&](Use &U) { return U.getUser() != Exn; });
    return true;
  }

  static bool isRaise(const CallBase &CB) {
    const auto *F = dyn_cast<Function>(CB.getCalledOperand());
    return F && F->isDeclaration() && F->getName() == UnwindRaise;
  }

  static bool mayUnwind(const Instruction &I) {
    const auto *CB = dyn_cast<CallBase>(&I);
    return CB && !CB->doesNotThrow() && !CB->isInlineAsm() &&
           !isa<IntrinsicInst>(CB);
  }

  bool run(Module &M) {
    bool Changed = false;

    for (Function &F : M) {
      if (F.isDeclaration())
        continue;
      // Not canUnwind(): a function that catches everything is nounwind.
      if (F.hasUWTable()) {
        SmallVector<LandingPadInst *, 8> Pads;
        for (BasicBlock &BB : F)
          if (LandingPadInst *LP = BB.getLandingPadInst())
            Pads.push_back(LP);
        unsigned NextIndex = 0;
        for (LandingPadInst *LP : Pads)
          Changed |= preparePad(*LP, NextIndex);
      }
      if (!canUnwind(F))
        continue;

      LLVMContext &C = M.getContext();
      SmallVector<CallInst *, 8> Calls;
      for (Instruction &I : instructions(F)) {
        auto *CB = dyn_cast<CallBase>(&I);
        if (CB && isRaise(*CB)) {
          CB->setDoesNotReturn();
          // Without a pad it leaves the frame.
          if (isa<CallInst>(CB))
            CB->setDoesNotThrow();
          Changed = true;
        } else if (isa<CallInst>(I) && mayUnwind(I)) {
          Calls.push_back(cast<CallInst>(&I));
        }
      }
      if (Calls.empty())
        continue;

      if (!F.hasPersonalityFn())
        F.setPersonalityFn(cast<Constant>(
            M.getOrInsertFunction("__gcc_personality_v0",
                                  FunctionType::get(Type::getInt32Ty(C), true))
                .getCallee()));
      BasicBlock *Pad = BasicBlock::Create(C, "unwind.resume", &F);
      IRBuilder<> B(Pad);
      LandingPadInst *LP = B.CreateLandingPad(
          StructType::get(PointerType::getUnqual(C), Type::getInt32Ty(C)), 0);
      LP->setCleanup(true);
      B.CreateResume(LP);
      for (CallInst *CI : Calls)
        changeToInvokeAndSplitBasicBlock(CI, Pad);
      Changed = true;
    }
    return Changed;
  }
};

/// After register allocation, which took call -> pad for an EH edge and so
/// keeps only r6-r9 and the stack alive over it, turn the edge into a branch.
struct BPFUnwindLower {
  const TargetLowering *TLI = nullptr;

  bool leavesFrame(const MachineInstr &MI) const {
    const MachineOperand &MO = MI.getOperand(0);
    return MO.isGlobal() && isBPFUnwindCallee(*MO.getGlobal(), *TLI);
  }

  /// Nothing after \p MI in its block runs: \p Succ, if any, is what follows.
  static void endBlockAfter(MachineInstr &MI, MachineBasicBlock *Succ) {
    MachineBasicBlock &MBB = *MI.getParent();
    MBB.erase(std::next(MI.getIterator()), MBB.end());
    while (!MBB.succ_empty())
      MBB.removeSuccessor(MBB.succ_begin());
    if (Succ)
      MBB.addSuccessor(Succ);
  }

  static const char *unsupported(const MachineFunction &MF) {
    if (MF.getLandingPads().empty())
      return nullptr;
    // Not canUnwind(): a function that catches everything is nounwind.
    if (!MF.getFunction().hasUWTable())
      return "BPF needs uwtable on a function with a landing pad";
    return nullptr;
  }

  bool run(MachineFunction &MF) {
    const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
    const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
    TLI = MF.getSubtarget().getTargetLowering();
    DenseMap<MCSymbol *, MachineBasicBlock *> Begin;
    SmallPtrSet<MCSymbol *, 8> PadLabels;
    SmallVector<std::pair<MachineInstr *, MachineBasicBlock *>, 8> Sites;
    SmallVector<MachineInstr *, 8> Labels, Resumes;

    if (const char *Msg = unsupported(MF)) {
      const Function &F = MF.getFunction();
      F.getContext().diagnose(DiagnosticInfoUnsupported(F, Msg));
      return false;
    }

    for (const LandingPadInfo &LP : MF.getLandingPads()) {
      for (MCSymbol *Sym : LP.BeginLabels)
        Begin[Sym] = LP.LandingPadBlock;
      if (MF.hasWasmLandingPadIndex(LP.LandingPadBlock))
        PadLabels.insert(LP.LandingPadLabel);
    }

    // The labels around a call that may unwind go: nothing is keyed by
    // address. The label of a pad the table lists stays, defined, which is
    // what the table's emitter asks of it.
    for (MachineBasicBlock &MBB : MF) {
      MachineBasicBlock *Pad = nullptr;
      for (MachineInstr &MI : MBB) {
        if (MI.isEHLabel()) {
          MCSymbol *Sym = MI.getOperand(0).getMCSymbol();
          if (PadLabels.count(Sym))
            continue;
          Pad = Begin.lookup(Sym);
          Labels.push_back(&MI);
        } else if (MI.isCall() && Pad) {
          Sites.push_back({&MI, Pad});
        } else if (MI.isCall() && leavesFrame(MI)) {
          Resumes.push_back(&MI);
        }
      }
    }
    if (Labels.empty() && Resumes.empty())
      return false;
    // There is no unwind_goto after the calls of it.
    if (!Resumes.empty() && !canUnwind(MF.getFunction())) {
      const Function &F = MF.getFunction();
      F.getContext().diagnose(DiagnosticInfoUnsupported(
          F, "BPF cannot unwind out of a function that is nounwind or has no "
             "uwtable"));
      return false;
    }
    for (MachineInstr *MI : Labels)
      MI->eraseFromParent();

    for (auto [Call, Pad] : Sites) {
      MachineBasicBlock *MBB = Call->getParent();
      DebugLoc DL = Call->getDebugLoc();
      const auto *II =
          dyn_cast_or_null<InvokeInst>(MBB->getBasicBlock()->getTerminator());

      if (leavesFrame(*Call)) {
        // LowerCall() passed the exception object in r0, where the pad
        // expects it.
        if (!Call->readsRegister(BPF::R0, &TRI))
          BuildMI(*MBB, Call, DL, TII.get(BPF::MOV_ri), BPF::R0).addImm(0);
        endBlockAfter(*Call, Pad);
        BuildMI(MBB, DL, TII.get(BPF::JMP)).addMBB(Pad);
        Call->eraseFromParent();
        continue;
      }

      // The callee sets r0 also when it unwinds: to the exception object.
      // Without the def, r0 would look live over the call into the pad.
      Call->addRegisterDefined(BPF::R0, &TRI);
      if (II && II->doesNotReturn()) {
        // Nothing sensible follows the call, but something has to.
        endBlockAfter(*Call, Pad);
        BuildMI(MBB, DL, TII.get(BPF::UNWIND_GOTO)).addMBB(Pad);
        BuildMI(MBB, DL, TII.get(BPF::JMP)).addMBB(Pad);
      } else {
        MachineBasicBlock *Rest = MBB->splitAt(*Call);
        if (Rest != MBB) {
          Rest->removeSuccessor(Pad);
          MBB->addSuccessor(Pad);
        }
        BuildMI(MBB, DL, TII.get(BPF::UNWIND_GOTO)).addMBB(Pad);
      }
    }

    // unwind_exit returns what exit returns: a value of more than 8 bytes is in
    // r0 and r2. r0 holds the exception object, which LowerCall() passed there.
    SmallVector<ISD::OutputArg, 2> Outs;
    const Function &F = MF.getFunction();
    GetReturnInfo(F.getCallingConv(), F.getReturnType(), F.getAttributes(),
                  Outs, *MF.getSubtarget().getTargetLowering(),
                  MF.getDataLayout());

    for (MachineInstr *Call : Resumes) {
      MachineBasicBlock *MBB = Call->getParent();
      DebugLoc DL = Call->getDebugLoc();

      if (!Call->readsRegister(BPF::R0, &TRI))
        BuildMI(*MBB, Call, DL, TII.get(BPF::MOV_ri), BPF::R0).addImm(0);
      MachineInstrBuilder Exit =
          BuildMI(*MBB, Call, DL, TII.get(BPF::UNWIND_EXIT))
              .addReg(BPF::R0, RegState::Implicit);
      if (Outs.size() > 1) {
        BuildMI(*MBB, *Exit, DL, TII.get(BPF::MOV_ri), BPF::R2).addImm(0);
        Exit.addReg(BPF::R2, RegState::Implicit);
      }
      endBlockAfter(*Exit, nullptr);
    }

    // The pads are plain blocks now. They stay listed with their clauses: the
    // table of a function whose pads carry indices is emitted from the list.
    for (MachineBasicBlock &MBB : MF)
      MBB.setIsEHPad(false);

    // What was behind a call that does not return. It can be a loop.
    df_iterator_default_set<MachineBasicBlock *> Reachable;
    for (MachineBasicBlock *MBB : depth_first_ext(&MF, Reachable))
      (void)MBB;
    for (MachineBasicBlock &MBB : make_early_inc_range(MF)) {
      if (Reachable.count(&MBB) || MBB.hasAddressTaken())
        continue;
      while (!MBB.succ_empty())
        MBB.removeSuccessor(MBB.succ_begin());
      MBB.eraseFromParent();
    }
    for (MachineBasicBlock &MBB : MF)
      if (!MBB.empty() && MBB.back().getOpcode() == BPF::JMP &&
          MBB.isLayoutSuccessor(MBB.back().getOperand(0).getMBB()))
        MBB.back().eraseFromParent();
    return true;
  }
};

struct BPFUnwindPrepareLegacy : public ModulePass {
  static char ID;
  BPFUnwindPrepareLegacy() : ModulePass(ID) {}
  StringRef getPassName() const override { return "BPF Unwind Prepare"; }
  bool runOnModule(Module &M) override { return BPFUnwindPrepare().run(M); }
};

struct BPFUnwindLowerLegacy : public MachineFunctionPass {
  static char ID;
  BPFUnwindLowerLegacy() : MachineFunctionPass(ID) {}
  StringRef getPassName() const override { return "BPF Unwind Lower"; }
  bool runOnMachineFunction(MachineFunction &MF) override {
    return BPFUnwindLower().run(MF);
  }
};

} // namespace

char BPFUnwindPrepareLegacy::ID = 0;
char BPFUnwindLowerLegacy::ID = 0;

INITIALIZE_PASS(BPFUnwindPrepareLegacy, "bpf-unwind-prepare",
                "BPF Unwind Prepare", false, false)
INITIALIZE_PASS(BPFUnwindLowerLegacy, "bpf-unwind-lower", "BPF Unwind Lower",
                false, false)

ModulePass *llvm::createBPFUnwindPrepareLegacyPass() {
  return new BPFUnwindPrepareLegacy();
}
FunctionPass *llvm::createBPFUnwindLowerLegacyPass() {
  return new BPFUnwindLowerLegacy();
}

PreservedAnalyses BPFUnwindPreparePass::run(Module &M,
                                            ModuleAnalysisManager &MAM) {
  return BPFUnwindPrepare().run(M) ? PreservedAnalyses::none()
                                   : PreservedAnalyses::all();
}

PreservedAnalyses
BPFUnwindLowerPass::run(MachineFunction &MF,
                        MachineFunctionAnalysisManager &MFAM) {
  return BPFUnwindLower().run(MF) ? getMachineFunctionPassPreservedAnalyses()
                                  : PreservedAnalyses::all();
}
