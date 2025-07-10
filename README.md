<p align=center>
  <a href="https://git.codingworkshop.eu.org/xt-sys/xtchain">
    <img alt="GIT Repository" src="https://img.shields.io/badge/Source-GIT-purple">
  </a>
  <a href="https://git.codingworkshop.eu.org/xt-sys/xtchain/actions">
    <img alt="Build Status" src="https://codingworkshop.eu.org/actions.php?project=xt-sys/xtchain">
  </a>
  <a href="https://github.com/xt-sys/xtchain/releases">
    <img alt="Releases" src="https://img.shields.io/github/v/release/xt-sys/xtchain?label=Release&amp;color=blueviolet">
  </a>
  <a href="https://git.codingworkshop.eu.org/xt-sys/xtchain/src/branch/master/COPYING.md">
    <img alt="License" src="https://img.shields.io/badge/License-GPLv3-blue.svg">
  </a>
  <a href="https://github.com/sponsors/xt-sys/">
    <img alt="Sponsors" src="https://img.shields.io/badge/Sponsor-%E2%9D%A4-red?logo=GitHub">
  </a>
  <a href="https://discord.com/invite/zBzJ5qMGX7">
    <img alt="Discord" src="https://img.shields.io/badge/Chat-Join%20Discord-success">
  </a>
</p>

---

## XT Toolchain
The XT Toolchain is a build environment based on LLVM/Clang/LLD. It currently supports C and C++, and includes
a variety of auxiliary tools such as IDL, message, and resource compilers. The XT Toolchain is the official
build system for compiling XT software, including the XT OS. It is currently available for Linux and Windows
host systems.

Key Benefits of using an LLVM-based Toolchain:
 * Unified toolchain for multiple target architectures: i686, x86_64, armv7, and aarch64
 * Support for generating debug information in PDB format
 * Ability to target ARM/AArch64 architectures and produce ARM binaries

This toolchain includes the following software:
 * CMake
 * LLVM
 * Ninja
 * Wine

**Note:** This toolchain is based on the [LLVM MinGW Toolchain](https://github.com/mstorsjo/llvm-mingw).

## Licensing
The XTchain project includes scripts for building and assembling the toolchain, as well as the environment
shell. These components are licensed under the GNU GPLv3 license. This license applies only to the parts
provided directly by XTchain. For detailed information, please refer to the COPYING.md file.

The final pre-built toolchain is subject to the licenses of the individual third-party projects it includes.
A complete list of the external software used in this toolchain can be found in the README.md file.
