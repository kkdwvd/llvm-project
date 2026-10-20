//===--- BPF.cpp - BPF ToolChain Implementation ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "BPF.h"
#include "clang/Config/config.h"
#include "clang/Driver/Action.h"
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

/// The directory of a BPF sysroot, resource directory or installation that
/// holds the target's files is named by the canonical triple, with the
/// vendor as wasi-sdk keeps it (include/wasm32-wasi is wasi-sdk's own
/// shortening): --target=bpf is the host's byte order, and bpfel, bpf-none
/// and bpfel-unknown-none all name bpfel-unknown-none.
std::string BPF::getMultiarchTriple(const Driver &D,
                                    const llvm::Triple &TargetTriple,
                                    StringRef SysRoot) const {
  return (llvm::Triple::getArchTypeName(TargetTriple.getArch()) +
          "-unknown-none")
      .str();
}

/// The sysroot is --sysroot, else the DEFAULT_BPF_SYSROOT the compiler was
/// built with, else share/bpf-sysroot next to the installation, as wasi-sdk
/// puts its sysroot in share/wasi-sysroot. Only the first of those that is
/// given counts, and it is a BPF sysroot only if it has include/<triple>:
/// a --sysroot without it is a root for the kernel's headers, which C BPF
/// programs took their <linux/*.h> from under it before this toolchain, and
/// still do.
std::string BPF::findSysRoot() const {
  const Driver &D = getDriver();
  SmallString<128> Path;
  if (!D.SysRoot.empty()) {
    Path = D.SysRoot;
  } else if (StringRef(DEFAULT_BPF_SYSROOT) != "") {
    Path = DEFAULT_BPF_SYSROOT;
  } else {
    Path = D.Dir;
    llvm::sys::path::append(Path, "..", "share", "bpf-sysroot");
  }

  SmallString<128> IncludeDir(Path);
  llvm::sys::path::append(IncludeDir, "include", MultiarchTriple);
  if (!getVFS().exists(IncludeDir))
    return "";
  return std::string(Path);
}

BPF::BPF(const Driver &D, const llvm::Triple &Triple, const ArgList &Args)
    : ToolChain(D, Triple, Args),
      MultiarchTriple(getMultiarchTriple(D, Triple, D.SysRoot)),
      SysRoot(findSysRoot()) {
  // bpftool next to clang comes before the one on PATH.
  getProgramPaths().push_back(D.Dir);

  // The base class named the per-target runtime directory after the triple
  // as it was spelled, which for --target=bpf is lib/bpf: use the canonical
  // one, where compiler-rt's builtins are, and after it the sysroot's
  // per-target lib, where a runtimes build with the sysroot as its prefix
  // installs them beside libc.a, so that a sysroot built apart from the
  // compiler carries its own, as the AIX toolchain takes the sysroot's
  // /usr/lib for the runtimes.
  SmallString<128> RuntimeDir(D.ResourceDir);
  llvm::sys::path::append(RuntimeDir, "lib", MultiarchTriple);
  getLibraryPaths().clear();
  getLibraryPaths().push_back(std::string(RuntimeDir));

  // Where -l libraries, libc and the C++ standard library are: the sysroot's
  // per-target lib, then the runtime directory. Nothing is taken from next to
  // the installation or the host.
  getFilePaths().clear();
  if (!SysRoot.empty()) {
    SmallString<128> LibDir(SysRoot);
    llvm::sys::path::append(LibDir, "lib", MultiarchTriple);
    getFilePaths().push_back(std::string(LibDir));
    getLibraryPaths().push_back(std::string(LibDir));
  }
  if (getVFS().exists(RuntimeDir))
    getFilePaths().push_back(std::string(RuntimeDir));
}

std::string BPF::computeSysRoot() const {
  return SysRoot.empty() ? getDriver().SysRoot : SysRoot;
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
  // With a sysroot the headers are its own, never the host's /usr/include
  // that the frontend would add for a target without an OS. Without one the
  // frontend goes on adding them, as it did before the toolchain existed.
  if (!SysRoot.empty())
    CC1Args.push_back("-nostdsysteminc");

  if (!DriverArgs.hasFlag(options::OPT_fuse_init_array,
                          options::OPT_fno_use_init_array, true))
    CC1Args.push_back("-fno-use-init-array");
}

void BPF::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                    ArgStringList &CC1Args) const {
  if (SysRoot.empty() || DriverArgs.hasArg(options::OPT_nostdinc))
    return;

  if (!DriverArgs.hasArg(options::OPT_nobuiltininc)) {
    SmallString<128> P(getDriver().ResourceDir);
    llvm::sys::path::append(P, "include");
    addSystemInclude(DriverArgs, CC1Args, P);
  }

  if (DriverArgs.hasArg(options::OPT_nostdlibinc))
    return;

  // The target's headers first, then what the sysroot shares between its
  // targets, as wasi-sdk and the baremetal layouts order them.
  SmallString<128> P(SysRoot);
  llvm::sys::path::append(P, "include", MultiarchTriple);
  addSystemInclude(DriverArgs, CC1Args, P);
  P = SysRoot;
  llvm::sys::path::append(P, "include");
  addSystemInclude(DriverArgs, CC1Args, P);
}

void BPF::AddClangCXXStdlibIncludeArgs(const ArgList &DriverArgs,
                                       ArgStringList &CC1Args) const {
  if (SysRoot.empty() ||
      DriverArgs.hasArg(options::OPT_nostdinc, options::OPT_nostdlibinc,
                        options::OPT_nostdincxx))
    return;
  if (GetCXXStdlibType(DriverArgs) != ToolChain::CST_Libcxx)
    return;

  // libc++'s headers are in include/<triple>/c++/v1 with their __config_site,
  // or that directory holds the __config_site alone over the headers in
  // include/c++/v1, as a runtimes build with per-target directories installs
  // them. Either way the per-target directory comes first.
  SmallString<128> TargetDir(SysRoot);
  llvm::sys::path::append(TargetDir, "include", MultiarchTriple);
  SmallString<128> SharedDir(SysRoot);
  llvm::sys::path::append(SharedDir, "include");

  std::string CXXABIHeader;
  for (StringRef Dir : {StringRef(TargetDir), StringRef(SharedDir)}) {
    std::string Version = detectLibcxxVersion(Dir);
    if (Version.empty())
      continue;
    SmallString<128> P(Dir);
    llvm::sys::path::append(P, "c++", Version);
    addSystemInclude(DriverArgs, CC1Args, P);
    if (CXXABIHeader.empty()) {
      llvm::sys::path::append(P, "cxxabi.h");
      if (getVFS().exists(P))
        CXXABIHeader = std::string(P);
    }
  }

  // libbpf links a call of a function in another object through BTF, which
  // clang derives from the declaration the unit has of the function; the
  // calls it makes on its own for a throw, a catch, a dynamic_cast or a
  // vtable's pure virtual slot have one only if the unit declares the runtime
  // function, and a unit that throws need not include a header. The C++
  // ABI's own header, which libc++abi installs with libc++'s headers,
  // declares them with the types the runtime is built with, so every unit
  // includes it, as the CUDA, HIP and OpenMP toolchains include their
  // runtimes' declarations.
  if (!CXXABIHeader.empty()) {
    CC1Args.push_back("-include");
    CC1Args.push_back(DriverArgs.MakeArgString(CXXABIHeader));
  }
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
  // libc++abi's personality routine reads the landing pad context that
  // libunwind keeps, as on every target it uses libunwind's _Unwind_*.
  CmdArgs.push_back(Args.MakeArgString(findLibrary("unwind", {})));
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
  const Driver &D = TC.getDriver();
  const char *Exec = Args.MakeArgString(getLinkerPath(Args));

  // libbpf links an object through its BTF, which the compiler builds from
  // debug info: a source compiled without -g gives an object without any,
  // which bpftool refuses once everything is compiled ("failed to find BTF
  // info for object"). Say so first, for a source this invocation compiles;
  // an object given as an input is bpftool's to check.
  const Arg *Debug = Args.getLastArg(options::OPT_g_Group);
  if (!Debug || Debug->getOption().matches(options::OPT_g0) ||
      Debug->getOption().matches(options::OPT_ggdb0)) {
    for (const auto &II : Inputs) {
      if (!II.isFilename() || !II.getAction() ||
          isa<InputAction>(II.getAction()))
        continue;
      D.Diag(diag::err_drv_bpf_link_without_debug_info) << II.getBaseInput();
      break;
    }
  }

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

  // The default libraries are the sysroot's: without one there is nothing
  // to link but what was named. They follow the inputs in the order they
  // depend on each other, since libbpf links an archive against the
  // references before it only: the C++ standard library, its ABI library and
  // the unwinding library when the driver is clang++, then compiler-rt's
  // builtins, libc, and the builtins again, since the two call each other,
  // libc the 128-bit division and the builtins malloc and memcpy for
  // emulated TLS. That is GCC's link sequence, libgcc, libc, libgcc, and the
  // GNU driver's, which lists the runtime libraries around -lc. -nostdlib++
  // leaves out the first, -nolibc libc, -nostdlib and -nodefaultlibs all of
  // them.
  bool NoDefaultLibs =
      Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs);
  bool NoLibc = Args.hasArg(options::OPT_nolibc);
  if (!TC.getSysRoot().empty() && !NoDefaultLibs) {
    if (TC.ShouldLinkCXXStdlib(Args))
      TC.AddCXXStdlibLibArgs(Args, CmdArgs);
    AddRunTimeLibs(TC, D, CmdArgs, Args);
    if (!NoLibc) {
      CmdArgs.push_back(Args.MakeArgString(TC.findLibrary("c", {})));
      AddRunTimeLibs(TC, D, CmdArgs, Args);
    }
  }

  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileCurCP(),
                                         Exec, CmdArgs, Inputs, Output));
}
