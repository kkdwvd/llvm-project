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

// The name of a section means something to libbpf. An executable section
// holds programs of the type it is named after, and .text the functions that
// programs call. A data section is a map, named after it: .data, .rodata and
// .bss are the maps of the global data, .data.<name>, .rodata.<name> and
// .bss.<name> the custom maps that a program asks for by naming them, and the
// mergeable .rodata.str1.<n> and .rodata.cst<n> of ELF lowering maps of their
// own. So a global object goes to the section of its kind unless its section
// attribute names another, whatever ELF lowering would otherwise choose: no
// .text.hot. or .text.unlikely. for a function that is hot or cold, no .hot
// or .unlikely prefix for data that -fpartition-static-data-sections finds
// hot or cold, and no section of its own for -ffunction-sections or
// -fdata-sections. A global object in a COMDAT group gets a section of its
// own in the group, as the group needs, named .text.<symbol>,
// .rodata.<symbol>, .data.<symbol> or .bss.<symbol>, which libbpf's linker
// and loader join to the base section.
MCSection *BPFTargetLoweringObjectFileELF::SelectSectionForGlobal(
    const GlobalObject *GO, SectionKind Kind, const TargetMachine &TM) const {
  const Comdat *C = GO->getComdat();

  if (Kind.isText()) {
    if (!C)
      return getTextSection();

    SmallString<128> Name(".text.");
    TM.getNameWithPrefix(Name, GO, getMangler(), /*MayAlwaysUsePrivate=*/true);
    return getContext().getELFSection(
        Name, ELF::SHT_PROGBITS,
        ELF::SHF_ALLOC | ELF::SHF_EXECINSTR | ELF::SHF_GROUP, /*EntrySize=*/0,
        C->getName(), C->getSelectionKind() == Comdat::Any);
  }

  // A group member gets its section of the group from ELF lowering, and a
  // mergeable string or constant the section of its entry size, which ELF
  // lowering shares with or without -fdata-sections.
  if (C || Kind.isMergeableCString() || Kind.isMergeableConst())
    return TargetLoweringObjectFileELF::SelectSectionForGlobal(GO, Kind, TM);

  if (Kind.isBSS())
    return BSSSection;
  if (Kind.isData())
    return DataSection;
  if (Kind.isReadOnly())
    return ReadOnlySection;
  if (Kind.isReadOnlyWithRel())
    return DataRelROSection;
  return TargetLoweringObjectFileELF::SelectSectionForGlobal(GO, Kind, TM);
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
