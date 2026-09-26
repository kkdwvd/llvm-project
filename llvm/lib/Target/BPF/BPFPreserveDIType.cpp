//===----------- BPFPreserveDIType.cpp - Preserve DebugInfo Types ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Preserve Debuginfo types encoded in __builtin_btf_type_id() and
// __builtin_bpf_typed_arena_cast() metadata.
//
//===----------------------------------------------------------------------===//

#include "BPF.h"
#include "BPFCORE.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/DebugInfo/BTF/BTF.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#define DEBUG_TYPE "bpf-preserve-di-type"

using namespace llvm;

namespace {

static bool BPFPreserveDITypeImpl(Function &F) {
  LLVM_DEBUG(dbgs() << "********** preserve debuginfo type **********\n");

  Module *M = F.getParent();

  // Bail out if no debug info.
  if (M->debug_compile_units().empty())
    return false;

  std::vector<CallInst *> PreserveDITypeCalls;
  std::vector<CallInst *> TypedArenaCastCalls;

  for (auto &BB : F) {
    for (auto &I : BB) {
      auto *Call = dyn_cast<CallInst>(&I);
      if (!Call)
        continue;

      const auto *GV = dyn_cast<GlobalValue>(Call->getCalledOperand());
      if (!GV)
        continue;

      if (GV->getName().starts_with("llvm.bpf.btf.type.id")) {
        if (!Call->getMetadata(LLVMContext::MD_preserve_access_index))
          report_fatal_error(
              "Missing metadata for llvm.bpf.btf.type.id intrinsic");
        PreserveDITypeCalls.push_back(Call);
      } else if (GV->getName().starts_with("llvm.bpf.typed.arena.cast")) {
        if (!Call->getMetadata(LLVMContext::MD_preserve_access_index))
          report_fatal_error(
              "Missing metadata for llvm.bpf.typed.arena.cast intrinsic");
        TypedArenaCastCalls.push_back(Call);
      }
    }
  }

  if (PreserveDITypeCalls.empty() && TypedArenaCastCalls.empty())
    return false;

  std::string BaseName = "llvm.btf_type_id.";
  static int Count = 0;
  for (auto *Call : PreserveDITypeCalls) {
    const ConstantInt *Flag = dyn_cast<ConstantInt>(Call->getArgOperand(1));
    assert(Flag);
    uint64_t FlagValue = Flag->getValue().getZExtValue();

    if (FlagValue >= BPFCoreSharedInfo::MAX_BTF_TYPE_ID_FLAG)
      report_fatal_error("Incorrect flag for llvm.bpf.btf.type.id intrinsic");

    MDNode *MD = Call->getMetadata(LLVMContext::MD_preserve_access_index);

    uint32_t Reloc;
    if (FlagValue == BPFCoreSharedInfo::BTF_TYPE_ID_LOCAL_RELOC) {
      Reloc = BTF::BTF_TYPE_ID_LOCAL;
    } else {
      Reloc = BTF::BTF_TYPE_ID_REMOTE;
    }
    DIType *Ty = cast<DIType>(MD);
    while (auto *DTy = dyn_cast<DIDerivedType>(Ty)) {
      unsigned Tag = DTy->getTag();
      if (Tag != dwarf::DW_TAG_const_type && Tag != dwarf::DW_TAG_volatile_type)
        break;
      Ty = DTy->getBaseType();
    }

    MD = Ty;

    BasicBlock *BB = Call->getParent();
    IntegerType *VarType = Type::getInt64Ty(BB->getContext());
    std::string GVName =
        BaseName + std::to_string(Count) + "$" + std::to_string(Reloc);
    GlobalVariable *GV = new GlobalVariable(
        *M, VarType, false, GlobalVariable::ExternalLinkage, nullptr, GVName);
    GV->addAttribute(BPFCoreSharedInfo::TypeIdAttr);
    GV->setMetadata(LLVMContext::MD_preserve_access_index, MD);

    // Load the global variable which represents the type info.
    auto *LDInst = new LoadInst(Type::getInt64Ty(BB->getContext()), GV, "",
                                Call->getIterator());
    Instruction *PassThroughInst =
        BPFCoreSharedInfo::insertPassThrough(M, BB, LDInst, Call);
    Call->replaceAllUsesWith(PassThroughInst);
    Call->eraseFromParent();
    Count++;
  }

  // A typed arena cast keeps its call; its type ID operand becomes a global
  // carrying the local type ID relocation, one per record type in the
  // function, so that casts of one value to one type can be merged and casts
  // to different types cannot.
  DenseMap<MDNode *, GlobalVariable *> TypedArenaGlobals;
  for (auto *Call : TypedArenaCastCalls) {
    MDNode *MD = Call->getMetadata(LLVMContext::MD_preserve_access_index);
    DIType *Ty = cast<DIType>(MD);
    while (auto *DTy = dyn_cast<DIDerivedType>(Ty)) {
      unsigned Tag = DTy->getTag();
      if (Tag != dwarf::DW_TAG_const_type && Tag != dwarf::DW_TAG_volatile_type)
        break;
      Ty = DTy->getBaseType();
    }

    GlobalVariable *&GV = TypedArenaGlobals[Ty];
    if (!GV) {
      IntegerType *VarType = Type::getInt64Ty(F.getContext());
      std::string GVName = BaseName + std::to_string(Count) + "$" +
                           std::to_string(BTF::BTF_TYPE_ID_LOCAL);
      GV = new GlobalVariable(*M, VarType, false,
                              GlobalVariable::ExternalLinkage, nullptr, GVName);
      GV->addAttribute(BPFCoreSharedInfo::TypeIdAttr);
      GV->setMetadata(LLVMContext::MD_preserve_access_index, Ty);
      Count++;
    }
    Call->setArgOperand(1, GV);
  }

  return true;
}
} // End anonymous namespace

PreservedAnalyses BPFPreserveDITypePass::run(Function &F,
                                             FunctionAnalysisManager &AM) {
  return BPFPreserveDITypeImpl(F) ? PreservedAnalyses::none()
                                  : PreservedAnalyses::all();
}
