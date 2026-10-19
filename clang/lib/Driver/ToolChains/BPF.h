//===--- BPF.h - BPF ToolChain Implementations ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_BPF_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_BPF_H

#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace tools {
namespace bpf {

/// Links BPF objects with libbpf's static linker, bpftool gen object.
class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  explicit Linker(const ToolChain &TC) : Tool("bpf::Linker", "linker", TC) {}
  bool isLinkJob() const override { return true; }
  bool hasIntegratedCPP() const override { return false; }
  std::string getLinkerPath(const llvm::opt::ArgList &Args) const;
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

} // end namespace bpf
} // end namespace tools

namespace toolchains {

/// The toolchain of the BPF targets, bpfel-unknown-none and bpfeb-unknown-none
/// however they are spelled. It compiles as the generic ELF toolchain did and
/// links with bpftool.
class LLVM_LIBRARY_VISIBILITY BPF final : public ToolChain {
public:
  BPF(const Driver &D, const llvm::Triple &Triple,
      const llvm::opt::ArgList &Args);

  /// The archive for -l<Name>, searched for in Dirs and then the toolchain's
  /// own directories, or the bare file name if it is in none of them.
  std::string findLibrary(StringRef Name, ArrayRef<std::string> Dirs) const;

  bool HasNativeLLVMSupport() const override { return true; }
  bool isPICDefault() const override { return false; }
  bool isPIEDefault(const llvm::opt::ArgList &Args) const override {
    return false;
  }
  bool isPICDefaultForced() const override { return false; }
  bool isCrossCompiling() const override { return true; }
  bool SupportsProfiling() const override { return false; }

  const char *getDefaultLinker() const override { return "bpftool"; }
  RuntimeLibType GetDefaultRuntimeLibType() const override {
    return ToolChain::RLT_CompilerRT;
  }
  CXXStdlibType GetDefaultCXXStdlibType() const override {
    return ToolChain::CST_Libcxx;
  }
  CXXStdlibType GetCXXStdlibType(const llvm::opt::ArgList &Args) const override;

  std::string getMultiarchTriple(const Driver &D,
                                 const llvm::Triple &TargetTriple,
                                 StringRef SysRoot) const override;
  std::string getCompilerRTPath() const override;

  void
  addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                        llvm::opt::ArgStringList &CC1Args, BoundArch BA,
                        Action::OffloadKind DeviceOffloadKind) const override;
  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;
  void AddClangCXXStdlibIncludeArgs(
      const llvm::opt::ArgList &DriverArgs,
      llvm::opt::ArgStringList &CC1Args) const override;
  void AddCXXStdlibLibArgs(const llvm::opt::ArgList &Args,
                           llvm::opt::ArgStringList &CmdArgs) const override;

protected:
  Tool *buildLinker() const override;

private:
  /// bpfel-unknown-none or bpfeb-unknown-none: the name of the per-target
  /// directories, whatever the triple was spelled as.
  std::string MultiarchTriple;
};

} // end namespace toolchains
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_BPF_H
