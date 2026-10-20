//===-- BPFAlignAtomics.cpp - Align the words of narrow atomics -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// BPF has atomic read-modify-write instructions of 32 and 64 bits only, and
// AtomicExpand works an atomic of 8 or 16 bits on the 32-bit word around it.
// When the instruction's alignment is below the word's, it finds the word by
// masking the address, p & -4, and the offset in it as p & 3. The verifier
// refuses a bitwise operation on a pointer to the stack or to a map value; and
// where a privileged program may see pointers as numbers, the kernel instead
// makes the masked pointer a number, which a program loaded with
// BPF_F_ARENA_SCALAR then accesses as an address in its arena: the atomic
// works on other memory.
//
// So before AtomicExpand, give such an atomic the alignment that its address
// is known to have, as InferAlignment does for loads and stores, first
// raising the alignment of the stack object or global it points into to the
// word's where that is allowed, as CodeGenPrepare does for the pointer
// arguments that a target wants aligned (shouldAlignPointerArgs). An address
// aligned to the word needs no masking. One at an odd offset in such an
// object is masked by AtomicExpand still, but the mask then clears bits that
// are known zero in the object's address and the DAG folds it into the
// offset (combineANDOfAlignedAdd in BPFISelLowering.cpp). One whose
// alignment is not known, such as a pointer a function is passed, is masked
// as before.
//
//===----------------------------------------------------------------------===//

#include "BPF.h"
#include "BPFSubtarget.h"
#include "BPFTargetMachine.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Local.h"

#define DEBUG_TYPE "bpf-align-atomics"

using namespace llvm;

// The alignment of Ptr, after raising that of the stack object or global it
// points into to the word's, where that is allowed. At an offset that keeps
// the word aligned Ptr then has the word's alignment and AtomicExpand masks
// nothing. At an odd offset Ptr keeps its own, and AtomicExpand masks the
// address, but the object is aligned now, so the mask clears bits the DAG
// knows to be zero in the object's address and folds it into the offset
// (combineANDOfAlignedAdd in BPFISelLowering.cpp): (r10 - 7) & -8 is
// r10 - 8, and (g + 1) & -4 is g.
static Align alignAddress(Value *Ptr, Align Word, const DataLayout &DL,
                          bool &Changed) {
  APInt Offset(DL.getIndexTypeSizeInBits(Ptr->getType()), 0);
  Value *Base = Ptr->stripAndAccumulateInBoundsConstantOffsets(DL, Offset);
  Align Before = getKnownAlignment(Base, DL);
  if (getOrEnforceKnownAlignment(Base, Word, DL) > Before)
    Changed = true;
  return getKnownAlignment(Ptr, DL);
}

static bool alignAtomics(Function &F, const TargetLowering &TLI) {
  unsigned WordBits = TLI.getMinCmpXchgSizeInBits();
  if (!WordBits)
    return false;
  Align Word(WordBits / 8);
  const DataLayout &DL = F.getDataLayout();

  bool Changed = false;
  for (Instruction &I : instructions(F)) {
    Value *Ptr;
    Type *ValTy;
    Align OldAlign;
    if (auto *RMW = dyn_cast<AtomicRMWInst>(&I)) {
      Ptr = RMW->getPointerOperand();
      ValTy = RMW->getValOperand()->getType();
      OldAlign = RMW->getAlign();
    } else if (auto *CAS = dyn_cast<AtomicCmpXchgInst>(&I)) {
      Ptr = CAS->getPointerOperand();
      ValTy = CAS->getNewValOperand()->getType();
      OldAlign = CAS->getAlign();
    } else {
      continue;
    }
    if (DL.getTypeStoreSize(ValTy) >= Word.value() || OldAlign >= Word)
      continue;

    Align NewAlign = alignAddress(Ptr, Word, DL, Changed);
    if (NewAlign <= OldAlign)
      continue;
    if (auto *RMW = dyn_cast<AtomicRMWInst>(&I))
      RMW->setAlignment(NewAlign);
    else
      cast<AtomicCmpXchgInst>(&I)->setAlignment(NewAlign);
    Changed = true;
  }
  return Changed;
}

PreservedAnalyses BPFAlignAtomicsPass::run(Function &F,
                                           FunctionAnalysisManager &AM) {
  const TargetLowering &TLI = *TM->getSubtargetImpl(F)->getTargetLowering();
  if (!alignAtomics(F, TLI))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

namespace {
class BPFAlignAtomicsLegacy final : public FunctionPass {
public:
  static char ID;
  BPFAlignAtomicsLegacy() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    auto &TM = getAnalysis<TargetPassConfig>().getTM<TargetMachine>();
    return alignAtomics(F, *TM.getSubtargetImpl(F)->getTargetLowering());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetPassConfig>();
    AU.setPreservesCFG();
  }
};
} // namespace

char BPFAlignAtomicsLegacy::ID = 0;
INITIALIZE_PASS_BEGIN(BPFAlignAtomicsLegacy, DEBUG_TYPE,
                      "BPF Align Narrow Atomics", false, false)
INITIALIZE_PASS_DEPENDENCY(TargetPassConfig)
INITIALIZE_PASS_END(BPFAlignAtomicsLegacy, DEBUG_TYPE,
                    "BPF Align Narrow Atomics", false, false)

FunctionPass *llvm::createBPFAlignAtomicsLegacyPass() {
  return new BPFAlignAtomicsLegacy();
}
