//===------------------ BPFTargetLoweringObjectFile.cpp -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "BPFTargetLoweringObjectFile.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"

using namespace llvm;

void BPFTargetLoweringObjectFileELF::initialize(MCContext &Ctx,
                                                const TargetMachine &TM,
                                                const Module &M) {
  TargetLoweringObjectFileELF::initialize(Ctx, TM, M);
  // No CFI names the personality routine or the exception table: a landing
  // pad calls the personality itself and hands it the table's address
  // (BPFUnwind.cpp). The table's pointers to type_info objects are absolute,
  // as in a static link.
  PersonalityEncoding = dwarf::DW_EH_PE_omit;
  LSDAEncoding = dwarf::DW_EH_PE_omit;
}

// The exception table is data the personality routine reads through a pointer
// a landing pad hands it, with pointers to type_info objects in it: read-only
// data with relocations, which libbpf loads as it loads the type_info objects
// and vtables there. Not .gcc_except_table, which libbpf would have to learn,
// and not a section of the function's COMDAT group: a group member joins a
// base section only when named after a symbol it defines, which the table,
// named after the function's number, is not. The table of a function in a
// group stays with each copy of the function, and the loader drops nothing
// but the group.
MCSection *BPFTargetLoweringObjectFileELF::getSectionForLSDA(
    const Function &F, const MCSymbol &FnSym, const TargetMachine &TM) const {
  return DataRelROSection;
}

MCSection *BPFTargetLoweringObjectFileELF::getSectionForJumpTable(
    const Function &F, const TargetMachine &TM,
    const MachineJumpTableEntry *JTE) const {
  return getContext().getELFSection(".jumptables", ELF::SHT_PROGBITS, 0);
}
