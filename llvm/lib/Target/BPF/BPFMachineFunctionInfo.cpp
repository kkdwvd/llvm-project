//===-- BPFMachineFunctionInfo.cpp - BPF machine function info ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "BPFMachineFunctionInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"

using namespace llvm;

MachineFunctionInfo *BPFMachineFunctionInfo::clone(
    BumpPtrAllocator &Allocator, MachineFunction &DestMF,
    const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
    const {
  return DestMF.cloneInfo<BPFMachineFunctionInfo>(*this);
}

int BPFMachineFunctionInfo::getFenceFrameIndex(MachineFunction &MF) {
  if (!FenceFrameIndex)
    FenceFrameIndex = MF.getFrameInfo().CreateStackObject(8, Align(8), false);
  return *FenceFrameIndex;
}
