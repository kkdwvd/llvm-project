//===-- CodeGen/AsmPrinter/WasmException.cpp - Wasm Exception Impl --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains support for writing WebAssembly exception info into asm
// files.
//
//===----------------------------------------------------------------------===//

#include "WasmException.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
using namespace llvm;

// The call-site table is keyed by the landing pad index that WasmEHPrepare
// assigned: EHStreamer::computeIndexedCallSiteTable computes it for the
// function, since its pads carry indices.
void WasmException::endFunction(const MachineFunction *MF) {
  if (!MF->hasAnyWasmLandingPadIndex())
    return;
  MCSymbol *LSDALabel = emitExceptionTable();
  assert(LSDALabel && ".GCC_exception_table has not been emitted!");

  // Wasm requires every data section symbol to have a .size set. So we emit an
  // end marker and set the size as the difference between the start end the end
  // marker.
  MCSymbol *LSDAEndLabel = Asm->createTempSymbol("GCC_except_table_end");
  Asm->OutStreamer->emitLabel(LSDAEndLabel);
  MCContext &OutContext = Asm->OutStreamer->getContext();
  const MCExpr *SizeExp = MCBinaryExpr::createSub(
      MCSymbolRefExpr::create(LSDAEndLabel, OutContext),
      MCSymbolRefExpr::create(LSDALabel, OutContext), OutContext);
  Asm->OutStreamer->emitELFSize(LSDALabel, SizeExp);
}
