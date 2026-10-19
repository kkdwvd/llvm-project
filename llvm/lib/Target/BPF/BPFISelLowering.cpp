//===-- BPFISelLowering.cpp - BPF DAG Lowering Implementation  ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that BPF uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#include "BPFISelLowering.h"
#include "BPF.h"
#include "BPFMachineFunctionInfo.h"
#include "BPFSubtarget.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/EHPersonalities.h"
#include "llvm/IR/IntrinsicsBPF.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/RuntimeLibcalls.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "bpf-lower"

static cl::opt<bool> BPFExpandMemcpyInOrder("bpf-expand-memcpy-in-order",
  cl::Hidden, cl::init(false),
  cl::desc("Expand memcpy into load/store pairs in order"));

static cl::opt<unsigned> BPFMinimumJumpTableEntries(
    "bpf-min-jump-table-entries", cl::init(13), cl::Hidden,
    cl::desc("Set minimum number of entries to use a jump table on BPF"));

static cl::opt<bool> BPFAllowsLibcalls(
    "bpf-allows-libcalls", cl::Hidden, cl::init(false),
    cl::desc("Allow libcalls instead of rejecting unsupported built-in "
             "functions"));

static void fail(const SDLoc &DL, SelectionDAG &DAG, const Twine &Msg,
                 SDValue Val = {}) {
  std::string Str;
  if (Val) {
    raw_string_ostream OS(Str);
    Val->print(OS);
    OS << ' ';
  }
  MachineFunction &MF = DAG.getMachineFunction();
  DAG.getContext()->diagnose(DiagnosticInfoUnsupported(
      MF.getFunction(), Twine(Str).concat(Msg), DL.getDebugLoc()));
}

BPFTargetLowering::BPFTargetLowering(const TargetMachine &TM,
                                     const BPFSubtarget &STI)
    : TargetLowering(TM, STI) {

  // Set up the register classes.
  addRegisterClass(MVT::i64, &BPF::GPRRegClass);
  if (STI.getHasAlu32())
    addRegisterClass(MVT::i32, &BPF::GPR32RegClass);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(BPF::R11);

  setOperationAction(ISD::BR_CC, MVT::i64, Custom);
  setOperationAction(ISD::BR_JT, MVT::Other, Expand);
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);

  if (!STI.hasGotox())
    setOperationAction(ISD::BRIND, MVT::Other, Expand);

  setOperationAction(ISD::TRAP, MVT::Other, Custom);

  setOperationAction({ISD::GlobalAddress, ISD::ConstantPool}, MVT::i64, Custom);
  if (STI.hasGotox())
    setOperationAction({ISD::JumpTable, ISD::BlockAddress}, MVT::i64, Custom);
  // The action of an intrinsic is looked up with MVT::Other.
  setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::Other, Custom);

  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i64, Custom);
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);

  // Set unsupported atomic operations as Custom so
  // we can emit better error messages than fatal error
  // from selectiondag.
  for (auto VT : {MVT::i8, MVT::i16, MVT::i32}) {
    if (VT == MVT::i32) {
      if (STI.getHasAlu32())
        continue;
    } else {
      setOperationAction(ISD::ATOMIC_LOAD_ADD, VT, Custom);
    }

    setOperationAction(ISD::ATOMIC_LOAD_AND, VT, Custom);
    setOperationAction(ISD::ATOMIC_LOAD_OR, VT, Custom);
    setOperationAction(ISD::ATOMIC_LOAD_XOR, VT, Custom);
    setOperationAction(ISD::ATOMIC_SWAP, VT, Custom);
    setOperationAction(ISD::ATOMIC_CMP_SWAP_WITH_SUCCESS, VT, Custom);
  }

  for (auto VT : {MVT::i32, MVT::i64}) {
    setOperationAction(ISD::ATOMIC_LOAD, VT, Custom);
    setOperationAction(ISD::ATOMIC_STORE, VT, Custom);
  }

  setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Custom);

  for (auto VT : { MVT::i32, MVT::i64 }) {
    if (VT == MVT::i32 && !STI.getHasAlu32())
      continue;

    setOperationAction(ISD::SDIVREM, VT, Expand);
    setOperationAction(ISD::UDIVREM, VT, Expand);
    if (!STI.hasSdivSmod()) {
      setOperationAction(ISD::SDIV, VT, Custom);
      setOperationAction(ISD::SREM, VT, Custom);
    }
    setOperationAction(ISD::MULHU, VT, Expand);
    setOperationAction(ISD::MULHS, VT, Expand);
    setOperationAction(ISD::UMUL_LOHI, VT, Expand);
    setOperationAction(ISD::SMUL_LOHI, VT, Expand);
    setOperationAction(ISD::ROTR, VT, Expand);
    setOperationAction(ISD::ROTL, VT, Expand);
    setOperationAction(ISD::SHL_PARTS, VT, Custom);
    setOperationAction(ISD::SRL_PARTS, VT, Custom);
    setOperationAction(ISD::SRA_PARTS, VT, Custom);
    setOperationAction(ISD::CTPOP, VT, Expand);
    setOperationAction(ISD::CTTZ, VT, Expand);
    setOperationAction(ISD::CTLZ, VT, Expand);
    setOperationAction(ISD::CTTZ_ZERO_POISON, VT, Expand);
    setOperationAction(ISD::CTLZ_ZERO_POISON, VT, Expand);

    setOperationAction(ISD::SETCC, VT, Expand);
    setOperationAction(ISD::SELECT, VT, Expand);
    setOperationAction(ISD::SELECT_CC, VT, Custom);
  }

  if (STI.getHasAlu32()) {
    setOperationAction(ISD::BSWAP, MVT::i32, Promote);
    setOperationAction(ISD::BR_CC, MVT::i32,
                       STI.getHasJmp32() ? Custom : Promote);
  }

  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  if (!STI.hasMovsx()) {
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i32, Expand);
  }

  // Extended load operations for i1 types must be promoted
  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::EXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i1, Promote);

    if (!STI.hasLdsx()) {
      setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i8, Expand);
      setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i16, Expand);
      setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i32, Expand);
    }
  }

  setBooleanContents(ZeroOrOneBooleanContent);
  setMaxAtomicSizeInBitsSupported(64);
  setMinimumJumpTableEntries(BPFMinimumJumpTableEntries);

  // Function alignments
  setMinFunctionAlignment(Align(8));
  setPrefFunctionAlignment(Align(8));

  if (BPFExpandMemcpyInOrder) {
    // LLVM generic code will try to expand memcpy into load/store pairs at this
    // stage which is before quite a few IR optimization passes, therefore the
    // loads and stores could potentially be moved apart from each other which
    // will cause trouble to memcpy pattern matcher inside kernel eBPF JIT
    // compilers.
    //
    // When -bpf-expand-memcpy-in-order specified, we want to defer the expand
    // of memcpy to later stage in IR optimization pipeline so those load/store
    // pairs won't be touched and could be kept in order. Hence, we set
    // MaxStoresPerMem* to zero to disable the generic getMemcpyLoadsAndStores
    // code path, and ask LLVM to use target expander EmitTargetCodeForMemcpy.
    MaxStoresPerMemset = MaxStoresPerMemsetOptSize = 0;
    MaxStoresPerMemcpy = MaxStoresPerMemcpyOptSize = 0;
    MaxStoresPerMemmove = MaxStoresPerMemmoveOptSize = 0;
    MaxLoadsPerMemcmp = 0;
  } else {
    // inline memcpy() for kernel to see explicit copy
    unsigned CommonMaxStores =
      STI.getSelectionDAGInfo()->getCommonMaxStoresPerMemFunc();

    MaxStoresPerMemset = MaxStoresPerMemsetOptSize = CommonMaxStores;
    MaxStoresPerMemcpy = MaxStoresPerMemcpyOptSize = CommonMaxStores;
    MaxStoresPerMemmove = MaxStoresPerMemmoveOptSize = CommonMaxStores;
    MaxLoadsPerMemcmp = MaxLoadsPerMemcmpOptSize = CommonMaxStores;
  }

  // CPU/Feature control
  HasAlu32 = STI.getHasAlu32();
  HasJmp32 = STI.getHasJmp32();
  HasJmpExt = STI.getHasJmpExt();
  HasMovsx = STI.hasMovsx();

  AllowsMisalignedMemAccess = STI.getAllowsMisalignedMemAccess();
}

bool BPFTargetLowering::allowsMisalignedMemoryAccesses(EVT VT, unsigned, Align,
                                                       MachineMemOperand::Flags,
                                                       unsigned *Fast) const {
  // allows-misaligned-mem-access is disabled
  if (!AllowsMisalignedMemAccess)
    return false;

  // only allow misalignment for simple value types
  if (!VT.isSimple())
    return false;

  // always assume fast mode when misalignment is allowed
  if (Fast)
    *Fast = true;

  return true;
}

bool BPFTargetLowering::isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const {
  return false;
}

bool BPFTargetLowering::isTruncateFree(Type *Ty1, Type *Ty2) const {
  if (!Ty1->isIntegerTy() || !Ty2->isIntegerTy())
    return false;
  unsigned NumBits1 = Ty1->getPrimitiveSizeInBits();
  unsigned NumBits2 = Ty2->getPrimitiveSizeInBits();
  return NumBits1 > NumBits2;
}

bool BPFTargetLowering::isTruncateFree(EVT VT1, EVT VT2) const {
  if (!VT1.isInteger() || !VT2.isInteger())
    return false;
  unsigned NumBits1 = VT1.getSizeInBits();
  unsigned NumBits2 = VT2.getSizeInBits();
  return NumBits1 > NumBits2;
}

bool BPFTargetLowering::isZExtFree(Type *Ty1, Type *Ty2) const {
  if (!getHasAlu32() || !Ty1->isIntegerTy() || !Ty2->isIntegerTy())
    return false;
  unsigned NumBits1 = Ty1->getPrimitiveSizeInBits();
  unsigned NumBits2 = Ty2->getPrimitiveSizeInBits();
  return NumBits1 == 32 && NumBits2 == 64;
}

bool BPFTargetLowering::isZExtFree(EVT VT1, EVT VT2) const {
  if (!getHasAlu32() || !VT1.isInteger() || !VT2.isInteger())
    return false;
  unsigned NumBits1 = VT1.getSizeInBits();
  unsigned NumBits2 = VT2.getSizeInBits();
  return NumBits1 == 32 && NumBits2 == 64;
}

bool BPFTargetLowering::isZExtFree(SDValue Val, EVT VT2) const {
  EVT VT1 = Val.getValueType();
  if (Val.getOpcode() == ISD::LOAD && VT1.isSimple() && VT2.isSimple()) {
    MVT MT1 = VT1.getSimpleVT().SimpleTy;
    MVT MT2 = VT2.getSimpleVT().SimpleTy;
    if ((MT1 == MVT::i8 || MT1 == MVT::i16 || MT1 == MVT::i32) &&
        (MT2 == MVT::i32 || MT2 == MVT::i64))
      return true;
  }
  return TargetLoweringBase::isZExtFree(Val, VT2);
}

unsigned BPFTargetLowering::getJumpTableEncoding() const {
  return MachineJumpTableInfo::EK_BlockAddress;
}

BPFTargetLowering::ConstraintType
BPFTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    default:
      break;
    case 'w':
      return C_RegisterClass;
    }
  }

  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
BPFTargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                StringRef Constraint,
                                                MVT VT) const {
  if (Constraint.size() == 1) {
    // GCC Constraint Letters
    switch (Constraint[0]) {
    case 'r': // GENERAL_REGS
      return std::make_pair(0U, &BPF::GPRRegClass);
    case 'w':
      if (HasAlu32)
        return std::make_pair(0U, &BPF::GPR32RegClass);
      break;
    default:
      break;
    }
  }

  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

void BPFTargetLowering::ReplaceNodeResults(
  SDNode *N, SmallVectorImpl<SDValue> &Results, SelectionDAG &DAG) const {
  const char *Msg;
  uint32_t Opcode = N->getOpcode();
  switch (Opcode) {
  default:
    report_fatal_error("unhandled custom legalization: " + Twine(Opcode));
  case ISD::ATOMIC_LOAD_ADD:
  case ISD::ATOMIC_LOAD_AND:
  case ISD::ATOMIC_LOAD_OR:
  case ISD::ATOMIC_LOAD_XOR:
  case ISD::ATOMIC_SWAP:
  case ISD::ATOMIC_CMP_SWAP_WITH_SUCCESS:
    if (HasAlu32 || Opcode == ISD::ATOMIC_LOAD_ADD)
      Msg = "unsupported atomic operation, please use 32/64 bit version";
    else
      Msg = "unsupported atomic operation, please use 64 bit version";
    break;
  case ISD::ATOMIC_LOAD:
  case ISD::ATOMIC_STORE:
    return;
  }

  SDLoc DL(N);
  // We'll still produce a fatal error downstream, but this diagnostic is more
  // user-friendly.
  fail(DL, DAG, Msg);
}

SDValue BPFTargetLowering::LowerOperation(SDValue Op, SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  default:
    report_fatal_error("unimplemented opcode: " + Twine(Op.getOpcode()));
  case ISD::BR_CC:
    return LowerBR_CC(Op, DAG);
  case ISD::JumpTable:
    return LowerJumpTable(Op, DAG);
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::ConstantPool:
    return LowerConstantPool(Op, DAG);
  case ISD::BlockAddress:
    return LowerBlockAddress(Op, DAG);
  case ISD::SELECT_CC:
    return LowerSELECT_CC(Op, DAG);
  case ISD::SDIV:
  case ISD::SREM:
    return LowerSDIVSREM(Op, DAG);
  case ISD::SHL_PARTS:
  case ISD::SRL_PARTS:
  case ISD::SRA_PARTS:
    return LowerShiftParts(Op, DAG);
  case ISD::DYNAMIC_STACKALLOC:
    return LowerDYNAMIC_STACKALLOC(Op, DAG);
  case ISD::ATOMIC_LOAD:
  case ISD::ATOMIC_STORE:
    return LowerATOMIC_LOAD_STORE(Op, DAG);
  case ISD::ATOMIC_FENCE:
    return LowerATOMIC_FENCE(Op, DAG);
  case ISD::TRAP:
    return LowerTRAP(Op, DAG);
  case ISD::INTRINSIC_WO_CHAIN:
    return LowerINTRINSIC_WO_CHAIN(Op, DAG);
  }
}

SDValue BPFTargetLowering::LowerINTRINSIC_WO_CHAIN(SDValue Op,
                                                   SelectionDAG &DAG) const {
  switch (Op.getConstantOperandVal(0)) {
  default:
    return SDValue();
  case Intrinsic::bpf_lsda: {
    // The function's exception table, by the name EHStreamer gives it, as
    // wasm.lsda is lowered: a landing pad hands the personality routine the
    // address (BPFUnwind.cpp).
    MachineFunction &MF = DAG.getMachineFunction();
    const char *SymName = MF.createExternalSymbolName(
        "GCC_except_table" + std::to_string(MF.getFunctionNumber()));
    SDValue Sym = DAG.getTargetExternalSymbol(SymName, MVT::i64);
    return SDValue(
        DAG.getMachineNode(BPF::LD_imm64, SDLoc(Op), MVT::i64, Sym), 0);
  }
  }
}

// Calling Convention Implementation
#define GET_CALLING_CONV_IMPL
#include "BPFGenCallingConv.inc"

// Apply AssertSext/AssertZext and truncate based on VA's LocInfo.
static SDValue convertLocValType(SelectionDAG &DAG, const SDLoc &DL,
                                 const CCValAssign &VA, EVT RegVT,
                                 SDValue ArgValue) {
  if (VA.getLocInfo() == CCValAssign::SExt)
    ArgValue = DAG.getNode(ISD::AssertSext, DL, RegVT, ArgValue,
                           DAG.getValueType(VA.getValVT()));
  else if (VA.getLocInfo() == CCValAssign::ZExt)
    ArgValue = DAG.getNode(ISD::AssertZext, DL, RegVT, ArgValue,
                           DAG.getValueType(VA.getValVT()));
  if (VA.getLocInfo() != CCValAssign::Full)
    ArgValue = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), ArgValue);
  return ArgValue;
}

SDValue BPFTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  switch (CallConv) {
  default:
    report_fatal_error("unimplemented calling convention: " + Twine(CallConv));
  case CallingConv::C:
  case CallingConv::Fast:
    break;
  }

  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, getHasAlu32() ? CC_BPF32 : CC_BPF64);

  for (size_t I = 0; I < ArgLocs.size(); ++I) {
    auto &VA = ArgLocs[I];
    EVT RegVT = VA.getLocVT();

    if (VA.isRegLoc()) {
      // Arguments passed in registers
      MVT::SimpleValueType SimpleTy = RegVT.getSimpleVT().SimpleTy;
      switch (SimpleTy) {
      default: {
        std::string Str;
        {
          raw_string_ostream OS(Str);
          RegVT.print(OS);
        }
        report_fatal_error("unhandled argument type: " + Twine(Str));
      }
      case MVT::i32:
      case MVT::i64:
        Register VReg = RegInfo.createVirtualRegister(
            SimpleTy == MVT::i64 ? &BPF::GPRRegClass : &BPF::GPR32RegClass);
        RegInfo.addLiveIn(VA.getLocReg(), VReg);
        SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, RegVT);
        InVals.push_back(convertLocValType(DAG, DL, VA, RegVT, ArgValue));
        break;
      }
      continue;
    }

    if (VA.isMemLoc()) {
      // For example, two stack arguments,
      //   arg1:  Off = 8
      //   arg2:  off = 16
      int Off = VA.getLocMemOffset() + 8;
      if (Off > INT16_MAX) {
        fail(DL, DAG, "extra parameter stack depth exceeded limit");
        break;
      }

      // The slot is always 64 bits. The load is no node of the DAG: it is
      // emitted at the entry of the function by emitFunctionEntryCode, where
      // the verifier wants it, so here the argument is a virtual register, as
      // an argument in a register is.
      Register VReg = RegInfo.createVirtualRegister(&BPF::GPRRegClass);
      MF.getInfo<BPFMachineFunctionInfo>()->addIncomingStackArg(VReg, Off);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, MVT::i64);
      InVals.push_back(convertLocValType(DAG, DL, VA, MVT::i64, ArgValue));
      continue;
    }
  }

  if (IsVarArg)
    fail(DL, DAG, "variadic functions are not supported");
  return Chain;
}

static void resetRegMaskBit(const TargetRegisterInfo *TRI, uint32_t *RegMask,
                            MCRegister Reg) {
  for (MCPhysReg SubReg : TRI->subregs_inclusive(Reg))
    RegMask[SubReg / 32] &= ~(1u << (SubReg % 32));
}

static uint32_t *regMaskFromTemplate(const TargetRegisterInfo *TRI,
                                     MachineFunction &MF,
                                     const uint32_t *BaseRegMask) {
  uint32_t *RegMask = MF.allocateRegMask();
  unsigned RegMaskSize = MachineOperand::getRegMaskSize(TRI->getNumRegs());
  memcpy(RegMask, BaseRegMask, sizeof(RegMask[0]) * RegMaskSize);
  return RegMask;
}

/// The function that a libcall of the backend calls, declared with the debug
/// info of its C prototype, for BTF: libbpf links the call to a definition in
/// another object through BTF, and matches the prototypes, so the types are
/// those of a definition in C, as compiler-rt or a libc writes one. Nothing in
/// the IR names the function, so the backend declares it, as it declares
/// __bpf_trap. A function the module defines is taken as it is, and so is a
/// declaration that has debug info; a declaration without gets the
/// prototype's.
Function *llvm::getOrCreateBPFLibcall(Module *M, StringRef Name) {
  Function *Fn = M->getFunction(Name);
  if (Fn && (!Fn->isDeclaration() || Fn->getSubprogram()))
    return Fn;

  LLVMContext &C = M->getContext();
  Type *Ptr = PointerType::getUnqual(C);
  Type *I32 = Type::getInt32Ty(C);
  Type *I64 = Type::getInt64Ty(C);
  Type *I128 = Type::getInt128Ty(C);
  DIBuilder DB(*M);
  DIType *VoidPtr = DB.createPointerType(nullptr, 64);
  DIType *ConstVoidPtr = DB.createPointerType(
      DB.createQualifiedType(dwarf::DW_TAG_const_type, nullptr), 64);
  DIType *ULong = DB.createBasicType("unsigned long", 64, dwarf::DW_ATE_unsigned);
  DIType *Int = DB.createBasicType("int", 32, dwarf::DW_ATE_signed);
  DIType *Int128 = DB.createBasicType("__int128", 128, dwarf::DW_ATE_signed);
  DIType *UInt128 =
      DB.createBasicType("unsigned __int128", 128, dwarf::DW_ATE_unsigned);

  FunctionType *FT;
  SmallVector<Metadata *, 4> DITypes;
  if (Name == "memcpy" || Name == "memmove") {
    // void *memcpy(void *dst, const void *src, unsigned long n)
    FT = FunctionType::get(Ptr, {Ptr, Ptr, I64}, false);
    DITypes = {VoidPtr, VoidPtr, ConstVoidPtr, ULong};
  } else if (Name == "memset") {
    // void *memset(void *dst, int c, unsigned long n)
    FT = FunctionType::get(Ptr, {Ptr, I32, I64}, false);
    DITypes = {VoidPtr, VoidPtr, Int, ULong};
  } else if (Name == "__multi3" || Name == "__divti3" || Name == "__modti3") {
    // __int128 __multi3(__int128 a, __int128 b)
    FT = FunctionType::get(I128, {I128, I128}, false);
    DITypes = {Int128, Int128, Int128};
  } else if (Name == "__udivti3" || Name == "__umodti3") {
    // unsigned __int128 __udivti3(unsigned __int128 a, unsigned __int128 b)
    FT = FunctionType::get(I128, {I128, I128}, false);
    DITypes = {UInt128, UInt128, UInt128};
  } else if (Name == RTLIB::RuntimeLibcallsInfo::getLibcallImplName(
                         RTLIB::impl__Unwind_GetBPFLPadContext)) {
    // struct _Unwind_LandingPadContext *_Unwind_GetBPFLPadContext(void), as
    // libunwind's Unwind-bpf.c defines it, the struct declared as above.
    FT = FunctionType::get(Ptr, {}, false);
    DIType *Context =
        DB.createForwardDecl(dwarf::DW_TAG_structure_type,
                             "_Unwind_LandingPadContext", nullptr, nullptr, 0);
    DITypes = {DB.createPointerType(Context, 64)};
  } else if (Name == getEHPersonalityName(EHPersonality::BPF_CXX)) {
    // int __gxx_bpf_personality_v0(void *exception_object), as libc++abi
    // defines the personality routine a landing pad calls (BPFUnwind.cpp),
    // with the prototype the frontend declares every personality with.
    FT = FunctionType::get(I32, {Ptr}, false);
    DITypes = {Int, VoidPtr};
  } else {
    return nullptr;
  }

  if (!Fn) {
    Fn = Function::Create(FT, GlobalValue::ExternalLinkage, Name, M);
    Fn->setDSOLocal(true);
  }
  if (M->debug_compile_units().empty())
    return Fn;

  DISubroutineType *FuncType =
      DB.createSubroutineType(DB.getOrCreateTypeArray(DITypes));
  DICompileUnit *CU = *M->debug_compile_units_begin();
  Fn->setSubprogram(DB.createFunction(CU, Name, Name, nullptr, 0, FuncType, 0,
                                      DINode::FlagPrototyped,
                                      DISubprogram::SPFlagZero));
  return Fn;
}

/// Whether V is an undefined value as the DAG builder hands one to LowerCall
/// or LowerReturn: an UNDEF node, or a result of the MERGE_VALUES that the
/// elements of an aggregate go through.
static bool isUndefValue(SDValue V) {
  while (V.getOpcode() == ISD::MERGE_VALUES)
    V = V.getOperand(V.getResNo());
  return V.isUndef();
}

SDValue BPFTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                     SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  auto &Outs = CLI.Outs;
  auto &OutVals = CLI.OutVals;
  auto &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  bool &IsTailCall = CLI.IsTailCall;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;
  MachineFunction &MF = DAG.getMachineFunction();

  // BPF target does not support tail call optimization.
  IsTailCall = false;

  switch (CallConv) {
  default:
    report_fatal_error("unsupported calling convention: " + Twine(CallConv));
  case CallingConv::Fast:
  case CallingConv::C:
    break;
  }

  // Analyze operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());

  CCInfo.AnalyzeCallOperands(Outs, getHasAlu32() ? CC_BPF32 : CC_BPF64);

  unsigned NumBytes = CCInfo.getStackSize();

  for (auto &Arg : Outs) {
    ISD::ArgFlagsTy Flags = Arg.Flags;
    if (!Flags.isByVal())
      continue;
    fail(CLI.DL, DAG, "pass by value not supported", Callee);
    break;
  }

  auto PtrVT = getPointerTy(MF.getDataLayout());
  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, CLI.DL);

  SmallVector<std::pair<unsigned, SDValue>, 8> RegsToPass;

  // Walk arg assignments
  for (size_t i = 0; i < OutVals.size(); ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue &Arg = OutVals[i];

    // The verifier refuses a call that reads a register nothing wrote, and
    // the optimizer passes an undefined value where it has shown that the
    // result it wants does not depend on it: the high half of a 128-bit
    // operand of __multi3 when only the low half of the product is used.
    // Pass zero, which is what every initialized register would be to the
    // verifier anyway.
    if (isUndefValue(Arg))
      Arg = DAG.getConstant(0, CLI.DL, Arg.getValueType());

    // Promote the value if needed.
    switch (VA.getLocInfo()) {
    default:
      report_fatal_error("unhandled location info: " + Twine(VA.getLocInfo()));
    case CCValAssign::Full:
      break;
    case CCValAssign::SExt:
      Arg = DAG.getNode(ISD::SIGN_EXTEND, CLI.DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::ZExt:
      Arg = DAG.getNode(ISD::ZERO_EXTEND, CLI.DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::AExt:
      Arg = DAG.getNode(ISD::ANY_EXTEND, CLI.DL, VA.getLocVT(), Arg);
      break;
    }

    // Push arguments into RegsToPass vector
    if (VA.isRegLoc()) {
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
      continue;
    }

    if (VA.isMemLoc()) {
      int Off = -8 - VA.getLocMemOffset();
      if (Off < INT16_MIN) {
        fail(CLI.DL, DAG, "extra parameter stack depth exceeded limit");
        break;
      }

      // STORE_STACK_ARG requires i64 operands. With ALU32 mode, the CC
      // promotion may only extend to i32, so extend to i64 if needed.
      if (Arg.getValueType() != MVT::i64)
        Arg = DAG.getNode(ISD::ANY_EXTEND, CLI.DL, MVT::i64, Arg);

      SDValue OffVal = DAG.getConstant(Off, CLI.DL, MVT::i64);
      Chain = DAG.getNode(BPFISD::STORE_STACK_ARG, CLI.DL, MVT::Other, Chain,
                          OffVal, Arg);
      continue;
    }

    report_fatal_error("unhandled argument location");
  }

  // The unwinder takes the exception object in r0, where unwind_exit returns
  // it and a landing pad expects it. BPFUnwindLower makes the call one or the
  // other.
  if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee))
    if (isBPFUnwindCallee(*G->getGlobal(), *this) && !RegsToPass.empty() &&
        RegsToPass.front().first == BPF::R1)
      RegsToPass.front().first = BPF::R0;

  SDValue InGlue;

  // Build a sequence of copy-to-reg nodes chained together with token chain and
  // flag operands which copy the outgoing args into registers.  The InGlue in
  // necessary since all emitted instructions must be stuck together.
  for (auto &Reg : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, CLI.DL, Reg.first, Reg.second, InGlue);
    InGlue = Chain.getValue(1);
  }

  // If the callee is a GlobalAddress node (quite common, every direct call is)
  // turn it into a TargetGlobalAddress node so that legalize doesn't hack it.
  // Likewise ExternalSymbol -> TargetExternalSymbol.
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), CLI.DL, PtrVT,
                                        G->getOffset(), 0);
  } else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    StringRef Sym = E->getSymbol();
    if (!BPFAllowsLibcalls && Sym != BPF_TRAP && Sym != "__multi3" &&
        Sym != "__divti3" && Sym != "__modti3" && Sym != "__udivti3" &&
        Sym != "__umodti3" && Sym != "memcpy" && Sym != "memset" &&
        Sym != "memmove")
      fail(
          CLI.DL, DAG,
          Twine("A call to built-in function '" + Sym + "' is not supported."));
    // A libcall is called as the function its C prototype declares, so that
    // it has BTF, which libbpf links the call to a definition in another
    // object through.
    if (Function *F =
            getOrCreateBPFLibcall(MF.getFunction().getParent(), Sym))
      Callee = DAG.getTargetGlobalAddress(F, CLI.DL, PtrVT, 0, 0);
    else
      Callee = DAG.getTargetExternalSymbol(E->getSymbol(), PtrVT, 0);
  }

  // Returns a chain & a flag for retval copy to use.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add argument registers to the end of the list so that they are
  // known live into the call.
  for (auto &Reg : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg.first, Reg.second.getValueType()));

  bool HasFastCall =
      (CLI.CB && isa<CallInst>(CLI.CB) && CLI.CB->hasFnAttr("bpf_fastcall"));
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  if (HasFastCall) {
    uint32_t *RegMask = regMaskFromTemplate(
        TRI, MF, TRI->getCallPreservedMask(MF, CallingConv::PreserveAll));
    for (auto const &RegPair : RegsToPass)
      resetRegMaskBit(TRI, RegMask, RegPair.first);
    if (!CLI.CB->getType()->isVoidTy())
      resetRegMaskBit(TRI, RegMask, BPF::R0);
    Ops.push_back(DAG.getRegisterMask(RegMask));
  } else {
    Ops.push_back(
        DAG.getRegisterMask(TRI->getCallPreservedMask(MF, CLI.CallConv)));
  }

  if (InGlue.getNode())
    Ops.push_back(InGlue);

  Chain = DAG.getNode(BPFISD::CALL, CLI.DL, NodeTys, Ops);
  InGlue = Chain.getValue(1);

  DAG.addNoMergeSiteInfo(Chain.getNode(), CLI.NoMerge);

  // Create the CALLSEQ_END node.
  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, InGlue, CLI.DL);
  InGlue = Chain.getValue(1);

  // Handle result values, copying them out of physregs into vregs that we
  // return.
  return LowerCallResult(Chain, InGlue, CallConv, IsVarArg, Ins, CLI.DL, DAG,
                         InVals);
}

SDValue
BPFTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::OutputArg> &Outs,
                               const SmallVectorImpl<SDValue> &OutVals,
                               const SDLoc &DL, SelectionDAG &DAG) const {
  unsigned Opc = BPFISD::RET_GLUE;

  // CCValAssign - represent the assignment of the return value to a location
  SmallVector<CCValAssign, 16> RVLocs;
  MachineFunction &MF = DAG.getMachineFunction();

  // CCState - Info about the registers and stack slot.
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());

  // Analize return values.
  CCInfo.AnalyzeReturn(Outs, getHasAlu32() ? RetCC_BPF32 : RetCC_BPF64);

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  // Copy the result values into the output registers.
  for (size_t i = 0; i != RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];
    if (!VA.isRegLoc())
      report_fatal_error("stack return values are not supported");

    // The verifier reads r0 at every exit, and the copy of an undefined
    // value into it is dropped, so a function that returns one would leave
    // the register unwritten: return zero, as an undefined argument is
    // passed as zero.
    SDValue Val = OutVals[i];
    if (isUndefValue(Val))
      Val = DAG.getConstant(0, DL, Val.getValueType());

    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Val, Glue);

    // Guarantee that all emitted copies are stuck together,
    // avoiding something bad.
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  // The verifier reads r0 at every exit, whatever the function returns, and
  // refuses a pointer into the stack there. A function that returns an
  // aggregate through the pointer its caller passed has that pointer in
  // hand, and the register allocator may leave it in r0, which is free in a
  // function that returns nothing. Return zero from such a function.
  if (RVLocs.empty() && MF.getFunction().hasStructRetAttr()) {
    Chain = DAG.getCopyToReg(Chain, DL, BPF::R0,
                             DAG.getConstant(0, DL, MVT::i64), Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(BPF::R0, MVT::i64));
  }

  RetOps[0] = Chain; // Update chain.

  // Add the glue if we have it.
  if (Glue.getNode())
    RetOps.push_back(Glue);

  return DAG.getNode(Opc, DL, MVT::Other, RetOps);
}

SDValue BPFTargetLowering::LowerCallResult(
    SDValue Chain, SDValue InGlue, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {

  MachineFunction &MF = DAG.getMachineFunction();
  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());

  CCInfo.AnalyzeCallResult(Ins, getHasAlu32() ? RetCC_BPF32 : RetCC_BPF64);

  // Copy all of the result registers out of their specified physreg.
  for (auto &Val : RVLocs) {
    Chain = DAG.getCopyFromReg(Chain, DL, Val.getLocReg(),
                               Val.getValVT(), InGlue).getValue(1);
    InGlue = Chain.getValue(2);
    InVals.push_back(Chain.getValue(0));
  }

  return Chain;
}

static void NegateCC(SDValue &LHS, SDValue &RHS, ISD::CondCode &CC) {
  switch (CC) {
  default:
    break;
  case ISD::SETULT:
  case ISD::SETULE:
  case ISD::SETLT:
  case ISD::SETLE:
    CC = ISD::getSetCCSwappedOperands(CC);
    std::swap(LHS, RHS);
    break;
  }
}

SDValue BPFTargetLowering::LowerSDIVSREM(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  fail(DL, DAG,
       "unsupported signed division, please convert to unsigned div/mod.");
  return DAG.getUNDEF(Op->getValueType(0));
}

SDValue BPFTargetLowering::LowerShiftParts(SDValue Op,
                                           SelectionDAG &DAG) const {
  SDValue Lo, Hi;
  expandShiftParts(Op.getNode(), Lo, Hi, DAG);
  return DAG.getMergeValues({Lo, Hi}, SDLoc(Op));
}

SDValue BPFTargetLowering::LowerDYNAMIC_STACKALLOC(SDValue Op,
                                                   SelectionDAG &DAG) const {
  SDLoc DL(Op);
  fail(DL, DAG, "unsupported dynamic stack allocation");
  auto Ops = {DAG.getConstant(0, SDLoc(), Op.getValueType()), Op.getOperand(0)};
  return DAG.getMergeValues(Ops, SDLoc());
}

SDValue BPFTargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);
  SDLoc DL(Op);

  if (!getHasJmpExt())
    NegateCC(LHS, RHS, CC);

  return DAG.getNode(BPFISD::BR_CC, DL, Op.getValueType(), Chain, LHS, RHS,
                     DAG.getConstant(CC, DL, LHS.getValueType()), Dest);
}

SDValue BPFTargetLowering::LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue TrueV = Op.getOperand(2);
  SDValue FalseV = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  SDLoc DL(Op);

  if (!getHasJmpExt())
    NegateCC(LHS, RHS, CC);

  SDValue TargetCC = DAG.getConstant(CC, DL, LHS.getValueType());
  SDValue Ops[] = {LHS, RHS, TargetCC, TrueV, FalseV};

  return DAG.getNode(BPFISD::SELECT_CC, DL, Op.getValueType(), Ops);
}

SDValue BPFTargetLowering::LowerATOMIC_LOAD_STORE(SDValue Op,
                                                  SelectionDAG &DAG) const {
  SDNode *N = Op.getNode();
  SDLoc DL(N);

  if (cast<AtomicSDNode>(N)->getMergedOrdering() ==
      AtomicOrdering::SequentiallyConsistent)
    fail(DL, DAG,
         "sequentially consistent (seq_cst) "
         "atomic load/store is not supported");

  return Op;
}

// BPF has no fence instruction, but its atomics that fetch are fully ordered,
// as the kernel's atomics that return a value are: the interpreter runs them
// as atomic64_fetch_add() and friends, which the kernel's memory model orders
// as if by smp_mb() before and after, the x86 JIT emits lock xadd and xchg,
// the arm64 JIT ldaddal and swpal (or an exclusive loop ending in dmb ish),
// the powerpc JIT puts sync around them. Which location such an atomic works
// on does not change what it orders, so a full barrier is a fetch-add of zero
// on a stack slot of the function's own, which no other CPU touches: the
// smp_mb() of the selftests' bpf_atomic.h, and what X86 does with a locked
// operation on the stack when it avoids mfence (emitLockedStackOp). Before
// ALU32 (-mcpu=v3) a fetch-add is not selected (an add whose result is unused
// is the non-fetching one, which orders nothing on arm64), so it is an
// exchange, fully ordered all the same. The slot is written first: the
// verifier refuses to read a stack slot that was never written.
SDValue BPFTargetLowering::emitFence(SelectionDAG &DAG, const SDLoc &DL,
                                     SDValue Chain) const {
  MachineFunction &MF = DAG.getMachineFunction();
  int FI = MF.getInfo<BPFMachineFunctionInfo>()->getFenceFrameIndex(MF);
  MachinePointerInfo PtrInfo = MachinePointerInfo::getFixedStack(MF, FI);
  SDValue Slot = DAG.getFrameIndex(FI, MVT::i64);
  SDValue Zero = DAG.getConstant(0, DL, MVT::i64);

  Chain = DAG.getStore(Chain, DL, Zero, Slot, PtrInfo, Align(8));
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      PtrInfo,
      MachineMemOperand::MOLoad | MachineMemOperand::MOStore |
          MachineMemOperand::MOVolatile,
      LLT::scalar(64), Align(8), MMOMetadata(), SyncScope::System,
      AtomicOrdering::SequentiallyConsistent);
  unsigned Opcode = HasAlu32 ? ISD::ATOMIC_LOAD_ADD : ISD::ATOMIC_SWAP;
  return DAG.getAtomic(Opcode, DL, MVT::i64, Chain, Slot, Zero, MMO)
      .getValue(1);
}

SDValue BPFTargetLowering::LowerATOMIC_FENCE(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SyncScope::ID FenceSSID =
      static_cast<SyncScope::ID>(Op.getConstantOperandVal(2));

  // A signal fence orders nothing between CPUs: MEMBARRIER is a compiler
  // barrier; it codegens to a no-op.
  if (FenceSSID == SyncScope::SingleThread)
    return DAG.getNode(ISD::MEMBARRIER, DL, MVT::Other, Op.getOperand(0));

  // BPF has no barrier weaker than a full one, and the same program runs on
  // x86 and arm64, so acquire, release and acq_rel fences are full barriers.
  return emitFence(DAG, DL, Op.getOperand(0));
}

static Function *createBPFUnreachable(Module *M) {
  if (auto *Fn = M->getFunction(BPF_TRAP))
    return Fn;

  FunctionType *FT = FunctionType::get(Type::getVoidTy(M->getContext()), false);
  Function *NewF =
      Function::Create(FT, GlobalValue::ExternalWeakLinkage, BPF_TRAP, M);
  NewF->setDSOLocal(true);
  NewF->setCallingConv(CallingConv::C);
  NewF->setSection(".ksyms");

  if (M->debug_compile_units().empty())
    return NewF;

  DIBuilder DBuilder(*M);
  DITypeArray ParamTypes =
      DBuilder.getOrCreateTypeArray({nullptr /*void return*/});
  DISubroutineType *FuncType = DBuilder.createSubroutineType(ParamTypes);
  DICompileUnit *CU = *M->debug_compile_units_begin();
  DISubprogram *SP =
      DBuilder.createFunction(CU, BPF_TRAP, BPF_TRAP, nullptr, 0, FuncType, 0,
                              DINode::FlagZero, DISubprogram::SPFlagZero);
  NewF->setSubprogram(SP);
  return NewF;
}

SDValue BPFTargetLowering::LowerTRAP(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  TargetLowering::CallLoweringInfo CLI(DAG);
  SmallVector<SDValue> InVals;
  SDNode *N = Op.getNode();
  SDLoc DL(N);

  Function *Fn = createBPFUnreachable(MF.getFunction().getParent());
  auto PtrVT = getPointerTy(MF.getDataLayout());
  CLI.Callee = DAG.getTargetGlobalAddress(Fn, DL, PtrVT);
  CLI.Chain = N->getOperand(0);
  CLI.IsTailCall = false;
  CLI.CallConv = CallingConv::C;
  CLI.IsVarArg = false;
  CLI.DL = std::move(DL);
  CLI.NoMerge = false;
  CLI.DoesNotReturn = true;
  return LowerCall(CLI, InVals);
}

SDValue BPFTargetLowering::LowerJumpTable(SDValue Op, SelectionDAG &DAG) const {
  JumpTableSDNode *N = cast<JumpTableSDNode>(Op);
  return getAddr(N, DAG);
}

static SDValue getTargetNode(ConstantPoolSDNode *N, const SDLoc &DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetConstantPool(N->getConstVal(), Ty, N->getAlign(),
                                   N->getOffset(), Flags);
}

static SDValue getTargetNode(JumpTableSDNode *N, const SDLoc &DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetJumpTable(N->getIndex(), Ty, Flags);
}

template <class NodeTy>
SDValue BPFTargetLowering::getAddr(NodeTy *N, SelectionDAG &DAG,
                                   unsigned Flags) const {
  SDLoc DL(N);

  SDValue GA = getTargetNode(N, DL, MVT::i64, DAG, Flags);

  return DAG.getNode(BPFISD::Wrapper, DL, MVT::i64, GA);
}

SDValue BPFTargetLowering::LowerGlobalAddress(SDValue Op,
                                              SelectionDAG &DAG) const {
  GlobalAddressSDNode *N = cast<GlobalAddressSDNode>(Op);
  if (N->getOffset() != 0)
    report_fatal_error("invalid offset for global address: " +
                       Twine(N->getOffset()));

  const GlobalValue *GVal = N->getGlobal();
  SDLoc DL(Op);

  // Wrap it in a TargetGlobalAddress
  SDValue Addr = DAG.getTargetGlobalAddress(GVal, DL, MVT::i64);

  // Emit pseudo instruction
  return SDValue(DAG.getMachineNode(BPF::LDIMM64, DL, MVT::i64, Addr), 0);
}

SDValue BPFTargetLowering::LowerConstantPool(SDValue Op,
                                             SelectionDAG &DAG) const {
  ConstantPoolSDNode *N = cast<ConstantPoolSDNode>(Op);

  return getAddr(N, DAG);
}

SDValue BPFTargetLowering::LowerBlockAddress(SDValue Op,
                                             SelectionDAG &DAG) const {
  const BlockAddress *BA = cast<BlockAddressSDNode>(Op)->getBlockAddress();
  SDLoc DL(Op);

  // Wrap it in a TargetBlockAddress
  SDValue Addr = DAG.getTargetBlockAddress(BA, MVT::i64);

  // Emit pseudo instruction
  return SDValue(DAG.getMachineNode(BPF::LDIMM64, DL, MVT::i64, Addr), 0);
}

unsigned
BPFTargetLowering::EmitSubregExt(MachineInstr &MI, MachineBasicBlock *BB,
                                 unsigned Reg, bool isSigned) const {
  const TargetInstrInfo &TII = *BB->getParent()->getSubtarget().getInstrInfo();
  const TargetRegisterClass *RC = getRegClassFor(MVT::i64);
  int RShiftOp = isSigned ? BPF::SRA_ri : BPF::SRL_ri;
  MachineFunction *F = BB->getParent();
  DebugLoc DL = MI.getDebugLoc();

  MachineRegisterInfo &RegInfo = F->getRegInfo();

  if (!isSigned) {
    Register PromotedReg0 = RegInfo.createVirtualRegister(RC);
    BuildMI(BB, DL, TII.get(BPF::MOV_32_64), PromotedReg0).addReg(Reg);
    return PromotedReg0;
  }
  Register PromotedReg0 = RegInfo.createVirtualRegister(RC);
  Register PromotedReg1 = RegInfo.createVirtualRegister(RC);
  Register PromotedReg2 = RegInfo.createVirtualRegister(RC);
  if (HasMovsx) {
    BuildMI(BB, DL, TII.get(BPF::MOVSX_rr_32), PromotedReg0).addReg(Reg);
  } else {
    BuildMI(BB, DL, TII.get(BPF::MOV_32_64), PromotedReg0).addReg(Reg);
    BuildMI(BB, DL, TII.get(BPF::SLL_ri), PromotedReg1)
      .addReg(PromotedReg0).addImm(32);
    BuildMI(BB, DL, TII.get(RShiftOp), PromotedReg2)
      .addReg(PromotedReg1).addImm(32);
  }

  return PromotedReg2;
}

MachineBasicBlock *
BPFTargetLowering::EmitInstrWithCustomInserterMemcpy(MachineInstr &MI,
                                                     MachineBasicBlock *BB)
                                                     const {
  MachineFunction *MF = MI.getParent()->getParent();
  MachineRegisterInfo &MRI = MF->getRegInfo();
  MachineInstrBuilder MIB(*MF, MI);
  unsigned ScratchReg;

  // This function does custom insertion during lowering BPFISD::MEMCPY which
  // only has two register operands from memcpy semantics, the copy source
  // address and the copy destination address.
  //
  // Because we will expand BPFISD::MEMCPY into load/store pairs, we will need
  // a third scratch register to serve as the destination register of load and
  // source register of store.
  //
  // The scratch register here is with the Define | Dead | EarlyClobber flags.
  // The EarlyClobber flag has the semantic property that the operand it is
  // attached to is clobbered before the rest of the inputs are read. Hence it
  // must be unique among the operands to the instruction. The Define flag is
  // needed to coerce the machine verifier that an Undef value isn't a problem
  // as we anyway is loading memory into it. The Dead flag is needed as the
  // value in scratch isn't supposed to be used by any other instruction.
  ScratchReg = MRI.createVirtualRegister(&BPF::GPRRegClass);
  MIB.addReg(ScratchReg,
             RegState::Define | RegState::Dead | RegState::EarlyClobber);

  return BB;
}

MachineBasicBlock *BPFTargetLowering::EmitInstrWithCustomInserterLDimm64(
    MachineInstr &MI, MachineBasicBlock *BB) const {
  MachineFunction *MF = BB->getParent();
  const BPFInstrInfo *TII = MF->getSubtarget<BPFSubtarget>().getInstrInfo();
  const TargetRegisterClass *RC = getRegClassFor(MVT::i64);
  MachineRegisterInfo &RegInfo = MF->getRegInfo();
  DebugLoc DL = MI.getDebugLoc();

  // Build address taken map for Global Varaibles and BlockAddresses
  DenseMap<const BasicBlock *, MachineBasicBlock *> AddressTakenBBs;
  for (MachineBasicBlock &MBB : *MF) {
    if (const BasicBlock *BB = MBB.getBasicBlock())
      if (BB->hasAddressTaken())
        AddressTakenBBs[BB] = &MBB;
  }

  MachineOperand &MO = MI.getOperand(1);
  assert(MO.isBlockAddress() || MO.isGlobal());

  Register ResultReg = MI.getOperand(0).getReg();
  Register TmpReg = RegInfo.createVirtualRegister(RC);

  std::vector<MachineBasicBlock *> Targets;
  unsigned JTI;

  if (MO.isBlockAddress()) {
    auto *BA = MO.getBlockAddress();
    MachineBasicBlock *TgtMBB = AddressTakenBBs[BA->getBasicBlock()];
    assert(TgtMBB);

    Targets.push_back(TgtMBB);
    JTI = MF->getOrCreateJumpTableInfo(getJumpTableEncoding())
              ->createJumpTableIndex(Targets);

    BuildMI(*BB, MI, DL, TII->get(BPF::LD_imm64), TmpReg)
        .addJumpTableIndex(JTI);
    BuildMI(*BB, MI, DL, TII->get(BPF::LDD), ResultReg)
        .addReg(TmpReg)
        .addImm(0);
    MI.eraseFromParent();
    return BB;
  }

  // Helper: emit LD_imm64 with operand GlobalAddress or JumpTable
  auto emitLDImm64 = [&](const GlobalValue *GV = nullptr, unsigned JTI = -1) {
    auto MIB = BuildMI(*BB, MI, DL, TII->get(BPF::LD_imm64), ResultReg);
    if (GV)
      MIB.addGlobalAddress(GV);
    else
      MIB.addJumpTableIndex(JTI);
    MI.eraseFromParent();
    return BB;
  };

  // Must be a global at this point
  const GlobalValue *GVal = MO.getGlobal();
  const auto *GV = dyn_cast<GlobalVariable>(GVal);

  if (!GV || GV->getLinkage() != GlobalValue::PrivateLinkage ||
      !GV->isConstant() || !GV->hasInitializer())
    return emitLDImm64(GVal);

  const auto *CA = dyn_cast<ConstantArray>(GV->getInitializer());
  if (!CA)
    return emitLDImm64(GVal);

  for (const Use &Op : CA->operands()) {
    if (!isa<BlockAddress>(Op))
      return emitLDImm64(GVal);
    auto *BA = cast<BlockAddress>(Op);
    MachineBasicBlock *TgtMBB = AddressTakenBBs[BA->getBasicBlock()];
    assert(TgtMBB);
    Targets.push_back(TgtMBB);
  }

  JTI = MF->getOrCreateJumpTableInfo(getJumpTableEncoding())
            ->createJumpTableIndex(Targets);
  return emitLDImm64(nullptr, JTI);
}

MachineBasicBlock *
BPFTargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                               MachineBasicBlock *BB) const {
  const TargetInstrInfo &TII = *BB->getParent()->getSubtarget().getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();
  unsigned Opc = MI.getOpcode();
  bool isSelectRROp = (Opc == BPF::Select ||
                       Opc == BPF::Select_64_32 ||
                       Opc == BPF::Select_32 ||
                       Opc == BPF::Select_32_64);

  bool isMemcpyOp = Opc == BPF::MEMCPY;
  bool isLDimm64Op = Opc == BPF::LDIMM64;

#ifndef NDEBUG
  bool isSelectRIOp = (Opc == BPF::Select_Ri ||
                       Opc == BPF::Select_Ri_64_32 ||
                       Opc == BPF::Select_Ri_32 ||
                       Opc == BPF::Select_Ri_32_64);

  if (!(isSelectRROp || isSelectRIOp || isMemcpyOp || isLDimm64Op))
    report_fatal_error("unhandled instruction type: " + Twine(Opc));
#endif

  if (isMemcpyOp)
    return EmitInstrWithCustomInserterMemcpy(MI, BB);

  if (isLDimm64Op)
    return EmitInstrWithCustomInserterLDimm64(MI, BB);

  bool is32BitCmp = (Opc == BPF::Select_32 ||
                     Opc == BPF::Select_32_64 ||
                     Opc == BPF::Select_Ri_32 ||
                     Opc == BPF::Select_Ri_32_64);

  // To "insert" a SELECT instruction, we actually have to insert the diamond
  // control-flow pattern.  The incoming instruction knows the destination vreg
  // to set, the condition code register to branch on, the true/false values to
  // select between, and a branch opcode to use.
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator I = ++BB->getIterator();

  // ThisMBB:
  // ...
  //  TrueVal = ...
  //  jmp_XX r1, r2 goto Copy1MBB
  //  fallthrough --> Copy0MBB
  MachineBasicBlock *ThisMBB = BB;
  MachineFunction *F = BB->getParent();
  MachineBasicBlock *Copy0MBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *Copy1MBB = F->CreateMachineBasicBlock(LLVM_BB);

  F->insert(I, Copy0MBB);
  F->insert(I, Copy1MBB);
  // Update machine-CFG edges by transferring all successors of the current
  // block to the new block which will contain the Phi node for the select.
  Copy1MBB->splice(Copy1MBB->begin(), BB,
                   std::next(MachineBasicBlock::iterator(MI)), BB->end());
  Copy1MBB->transferSuccessorsAndUpdatePHIs(BB);
  // Next, add the true and fallthrough blocks as its successors.
  BB->addSuccessor(Copy0MBB);
  BB->addSuccessor(Copy1MBB);

  // Insert Branch if Flag
  int CC = MI.getOperand(3).getImm();
  int NewCC;
  switch (CC) {
#define SET_NEWCC(X, Y) \
  case ISD::X: \
    if (is32BitCmp && HasJmp32) \
      NewCC = isSelectRROp ? BPF::Y##_rr_32 : BPF::Y##_ri_32; \
    else \
      NewCC = isSelectRROp ? BPF::Y##_rr : BPF::Y##_ri; \
    break
  SET_NEWCC(SETGT, JSGT);
  SET_NEWCC(SETUGT, JUGT);
  SET_NEWCC(SETGE, JSGE);
  SET_NEWCC(SETUGE, JUGE);
  SET_NEWCC(SETEQ, JEQ);
  SET_NEWCC(SETNE, JNE);
  SET_NEWCC(SETLT, JSLT);
  SET_NEWCC(SETULT, JULT);
  SET_NEWCC(SETLE, JSLE);
  SET_NEWCC(SETULE, JULE);
  default:
    report_fatal_error("unimplemented select CondCode " + Twine(CC));
  }

  Register LHS = MI.getOperand(1).getReg();
  bool isSignedCmp = (CC == ISD::SETGT ||
                      CC == ISD::SETGE ||
                      CC == ISD::SETLT ||
                      CC == ISD::SETLE);

  // eBPF at the moment only has 64-bit comparison. Any 32-bit comparison need
  // to be promoted, however if the 32-bit comparison operands are destination
  // registers then they are implicitly zero-extended already, there is no
  // need of explicit zero-extend sequence for them.
  //
  // We simply do extension for all situations in this method, but we will
  // try to remove those unnecessary in BPFMIPeephole pass.
  if (is32BitCmp && !HasJmp32)
    LHS = EmitSubregExt(MI, BB, LHS, isSignedCmp);

  if (isSelectRROp) {
    Register RHS = MI.getOperand(2).getReg();

    if (is32BitCmp && !HasJmp32)
      RHS = EmitSubregExt(MI, BB, RHS, isSignedCmp);

    BuildMI(BB, DL, TII.get(NewCC)).addReg(LHS).addReg(RHS).addMBB(Copy1MBB);
  } else {
    int64_t imm32 = MI.getOperand(2).getImm();
    // Check before we build J*_ri instruction.
    if (!isInt<32>(imm32))
      report_fatal_error("immediate overflows 32 bits: " + Twine(imm32));
    BuildMI(BB, DL, TII.get(NewCC))
        .addReg(LHS).addImm(imm32).addMBB(Copy1MBB);
  }

  // Copy0MBB:
  //  %FalseValue = ...
  //  # fallthrough to Copy1MBB
  BB = Copy0MBB;

  // Update machine-CFG edges
  BB->addSuccessor(Copy1MBB);

  // Copy1MBB:
  //  %Result = phi [ %FalseValue, Copy0MBB ], [ %TrueValue, ThisMBB ]
  // ...
  BB = Copy1MBB;
  BuildMI(*BB, BB->begin(), DL, TII.get(BPF::PHI), MI.getOperand(0).getReg())
      .addReg(MI.getOperand(5).getReg())
      .addMBB(Copy0MBB)
      .addReg(MI.getOperand(4).getReg())
      .addMBB(ThisMBB);

  MI.eraseFromParent(); // The pseudo instruction is gone now.
  return BB;
}

EVT BPFTargetLowering::getSetCCResultType(const DataLayout &, LLVMContext &,
                                          EVT VT) const {
  return getHasAlu32() ? MVT::i32 : MVT::i64;
}

MVT BPFTargetLowering::getScalarShiftAmountTy(const DataLayout &DL,
                                              EVT VT) const {
  return (getHasAlu32() && VT == MVT::i32) ? MVT::i32 : MVT::i64;
}

bool BPFTargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                              const AddrMode &AM, Type *Ty,
                                              unsigned AS,
                                              Instruction *I) const {
  // No global is ever allowed as a base.
  if (AM.BaseGV)
    return false;

  switch (AM.Scale) {
  case 0: // "r+i" or just "i", depending on HasBaseReg.
    break;
  case 1:
    if (!AM.HasBaseReg) // allow "r+i".
      break;
    return false; // disallow "r+r" or "r+r+i".
  default:
    return false;
  }

  return true;
}

bool BPFTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type *RetTy) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, getHasAlu32() ? RetCC_BPF32 : RetCC_BPF64);
}
