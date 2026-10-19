//===--- BPF.cpp - BPF ToolChain Implementation ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "BPF.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Options/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang::driver::toolchains;
using namespace clang;
using namespace llvm::opt;

/// The directory of a resource directory or installation that holds the
/// target's files is named by the canonical triple, with the vendor as
/// wasi-sdk keeps it (include/wasm32-wasi is wasi-sdk's own shortening):
/// --target=bpf is the host's byte order, and bpfel, bpf-none and
/// bpfel-unknown-none all name bpfel-unknown-none.
std::string BPF::getMultiarchTriple(const Driver &D,
                                    const llvm::Triple &TargetTriple,
                                    StringRef SysRoot) const {
  return (llvm::Triple::getArchTypeName(TargetTriple.getArch()) +
          "-unknown-none")
      .str();
}

BPF::BPF(const Driver &D, const llvm::Triple &Triple, const ArgList &Args)
    : ToolChain(D, Triple, Args),
      MultiarchTriple(getMultiarchTriple(D, Triple, D.SysRoot)) {
  // bpftool next to clang comes before the one on PATH.
  getProgramPaths().push_back(D.Dir);

  // The base class named the per-target runtime directory after the triple
  // as it was spelled, which for --target=bpf is lib/bpf: use the canonical
  // one, where compiler-rt's builtins are.
  SmallString<128> RuntimeDir(D.ResourceDir);
  llvm::sys::path::append(RuntimeDir, "lib", MultiarchTriple);
  getLibraryPaths().clear();
  getLibraryPaths().push_back(std::string(RuntimeDir));

  // Where -l libraries are: the runtime directory. Nothing is taken from next
  // to the installation or the host.
  getFilePaths().clear();
  if (getVFS().exists(RuntimeDir))
    getFilePaths().push_back(std::string(RuntimeDir));
}

std::string BPF::getCompilerRTPath() const {
  return getLibraryPaths().front();
}

Tool *BPF::buildLinker() const { return new tools::bpf::Linker(*this); }

ToolChain::CXXStdlibType BPF::GetCXXStdlibType(const ArgList &Args) const {
  if (Arg *A = Args.getLastArg(options::OPT_stdlib_EQ)) {
    StringRef Value = A->getValue();
    if (Value != "libc++")
      getDriver().Diag(diag::err_drv_invalid_stdlib_name)
          << A->getAsString(Args);
  }
  return ToolChain::CST_Libcxx;
}

void BPF::addClangTargetOptions(const ArgList &DriverArgs,
                                ArgStringList &CC1Args, BoundArch BA,
                                Action::OffloadKind) const {
  if (!DriverArgs.hasFlag(options::OPT_fuse_init_array,
                          options::OPT_fno_use_init_array, true))
    CC1Args.push_back("-fno-use-init-array");
}

void BPF::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                    ArgStringList &CC1Args) const {
  // The driver adds no directory: the frontend adds the host's, as it does
  // for any target without an OS, which is what BPF programs are built with.
}

void BPF::AddClangCXXStdlibIncludeArgs(const ArgList &DriverArgs,
                                       ArgStringList &CC1Args) const {
  // There is no C++ standard library to add.
}

std::string BPF::findLibrary(StringRef Name,
                             ArrayRef<std::string> Dirs) const {
  std::string File = ("lib" + Name + ".a").str();
  for (StringRef Dir : Dirs) {
    SmallString<128> P(Dir);
    llvm::sys::path::append(P, File);
    if (getVFS().exists(P))
      return std::string(P);
  }
  return GetFilePath(File.c_str());
}

void BPF::AddCXXStdlibLibArgs(const ArgList &Args,
                              ArgStringList &CmdArgs) const {
  // bpftool takes files, not -l: the archives themselves, in the order the
  // libraries depend on each other.
  if (GetCXXStdlibType(Args) != ToolChain::CST_Libcxx)
    return;
  CmdArgs.push_back(Args.MakeArgString(findLibrary("c++", {})));
  if (Args.hasArg(options::OPT_fexperimental_library))
    CmdArgs.push_back(Args.MakeArgString(findLibrary("c++experimental", {})));
  CmdArgs.push_back(Args.MakeArgString(findLibrary("c++abi", {})));
}

/// bpftool next to clang, on PATH or from -B, like other tools; --ld-path=
/// names another executable, and -fuse-ld= may name an absolute one, or
/// bpftool or ld for the default.
std::string bpf::Linker::getLinkerPath(const ArgList &Args) const {
  const ToolChain &TC = getToolChain();
  const Driver &D = TC.getDriver();

  if (const Arg *A = Args.getLastArg(options::OPT_ld_path_EQ)) {
    std::string Path(A->getValue());
    if (!Path.empty()) {
      if (llvm::sys::path::parent_path(Path).empty())
        Path = TC.GetProgramPath(A->getValue());
      if (llvm::sys::fs::can_execute(Path))
        return Path;
    }
    D.Diag(diag::err_drv_invalid_linker_name) << A->getAsString(Args);
    return TC.GetProgramPath(TC.getDefaultLinker());
  }

  if (const Arg *A = Args.getLastArg(options::OPT_fuse_ld_EQ)) {
    StringRef UseLinker = A->getValue();
    if (!UseLinker.empty() && UseLinker != "ld" && UseLinker != "bpftool") {
      if (llvm::sys::path::is_absolute(UseLinker) &&
          llvm::sys::fs::can_execute(UseLinker))
        return std::string(UseLinker);
      D.Diag(diag::err_drv_invalid_linker_name) << A->getAsString(Args);
    }
  }

  return TC.GetProgramPath(TC.getDefaultLinker());
}

void bpf::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                               const InputInfo &Output,
                               const InputInfoList &Inputs,
                               const ArgList &Args,
                               const char *LinkingOutput) const {
  const auto &TC = static_cast<const toolchains::BPF &>(getToolChain());
  const char *Exec = Args.MakeArgString(getLinkerPath(Args));

  // Nothing the compiler is told about the output applies to the linker.
  Args.ClaimAllArgs(options::OPT_g_Group);
  Args.ClaimAllArgs(options::OPT_emit_llvm);
  Args.ClaimAllArgs(options::OPT_w);
  Args.ClaimAllArgs(options::OPT_stdlib_EQ);

  // bpftool [OPTIONS] gen object OUTPUT INPUT...: the options are bpftool's
  // own, from -Wl, and -Xlinker, and -l names an archive that is passed as an
  // input, in its place among them, from -L directories first.
  ArgStringList CmdArgs;
  ArgStringList InputArgs;
  std::vector<std::string> LibDirs = Args.getAllArgValues(options::OPT_L);
  for (const auto &II : Inputs) {
    if (II.isFilename()) {
      InputArgs.push_back(II.getFilename());
      continue;
    }
    if (II.isNothing())
      continue;
    const Arg &A = II.getInputArg();
    if (A.getOption().matches(options::OPT_Wl_COMMA) ||
        A.getOption().matches(options::OPT_Xlinker)) {
      A.renderAsInput(Args, CmdArgs);
    } else if (A.getOption().matches(options::OPT_l)) {
      InputArgs.push_back(
          Args.MakeArgString(TC.findLibrary(A.getValue(), LibDirs)));
    } else if (A.getOption().matches(options::OPT_Z_reserved_lib_stdcxx)) {
      TC.AddCXXStdlibLibArgs(Args, InputArgs);
    } else {
      A.renderAsInput(Args, InputArgs);
    }
  }

  CmdArgs.push_back("gen");
  CmdArgs.push_back("object");
  CmdArgs.push_back(Output.getFilename());
  CmdArgs.append(InputArgs);

  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileCurCP(),
                                         Exec, CmdArgs, Inputs, Output));
}
