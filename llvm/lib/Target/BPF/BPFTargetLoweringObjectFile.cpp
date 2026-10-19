//===------------------ BPFTargetLoweringObjectFile.cpp -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "BPFTargetLoweringObjectFile.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/IR/Comdat.h"
#include "llvm/IR/GlobalObject.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/Target/TargetMachine.h"

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

// libbpf takes the name of an executable section for the type of the programs
// in it, and .text for the functions that programs call. So a function goes
// to .text unless its section attribute names another, whatever ELF lowering
// would otherwise choose: no .text.hot. or .text.unlikely. for a function that
// is hot or cold, and no section of its own for -ffunction-sections. A
// function in a COMDAT group gets a section of its own in the group, named
// .text.<symbol>, which libbpf's linker joins to .text.
MCSection *BPFTargetLoweringObjectFileELF::SelectSectionForGlobal(
    const GlobalObject *GO, SectionKind Kind, const TargetMachine &TM) const {
  if (!Kind.isText())
    return TargetLoweringObjectFileELF::SelectSectionForGlobal(GO, Kind, TM);

  const Comdat *C = GO->getComdat();
  if (!C)
    return getTextSection();

  SmallString<128> Name(".text.");
  TM.getNameWithPrefix(Name, GO, getMangler(), /*MayAlwaysUsePrivate=*/true);
  return getContext().getELFSection(
      Name, ELF::SHT_PROGBITS,
      ELF::SHF_ALLOC | ELF::SHF_EXECINSTR | ELF::SHF_GROUP, /*EntrySize=*/0,
      C->getName(), C->getSelectionKind() == Comdat::Any);
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
