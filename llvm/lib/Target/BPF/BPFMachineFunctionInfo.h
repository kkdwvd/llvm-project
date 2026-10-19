//===-- BPFMachineFunctionInfo.h - BPF machine function info ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares BPF-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_BPF_BPFMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_BPF_BPFMACHINEFUNCTIONINFO_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/Register.h"

namespace llvm {

class BPFMachineFunctionInfo : public MachineFunctionInfo {
  /// The virtual register each incoming stack argument is loaded into, with
  /// its offset from r11, recorded by LowerFormalArguments for
  /// emitFunctionEntryCode to emit the loads at the entry of the function,
  /// where the verifier wants them: before any call and any store to the
  /// outgoing argument area.
  SmallVector<std::pair<Register, int>, 4> IncomingStackArgs;

public:
  BPFMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI) {}

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override;

  void addIncomingStackArg(Register VReg, int Offset) {
    IncomingStackArgs.emplace_back(VReg, Offset);
  }
  SmallVector<std::pair<Register, int>, 4> takeIncomingStackArgs() {
    SmallVector<std::pair<Register, int>, 4> Args =
        std::move(IncomingStackArgs);
    IncomingStackArgs.clear();
    return Args;
  }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_BPF_BPFMACHINEFUNCTIONINFO_H
