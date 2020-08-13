## XT Toolchain
This is a LLVM/Clang/LLD based mingw-w64 toolchain. It currently supports C and C++, and provides
a variety of tools including IDL, message and resource compilers. The XT Toolchain is also the
official build environment for compiling XT software, including the FerretOS. Currently, it is
targeted at Linux host only, however it should be possible to build it in MSYS2 as well.

Benefits of a LLVM based MinGW toolchain are:
 * Single toolchain targeting all architectures (i686, x86_64, armv7 and aarch64),
 * Support for generating debug info in PDB format,
 * Support for targeting ARM/AARCH64 architectures and ability to produce Windows ARM binaries.

This software includes:
 * CMake
 * LLVM
 * Make
 * Mingw-w64
 * Ninja
 * Wine

This software is based on ["LLVM MinGW Toolchain"](https://github.com/mstorsjo/llvm-mingw).

## Licensing
The XTchain project includes the scripts for building and assembling a toolchain as well as wrappers
for LLVM tools and environmental shell. These are licensed under the GPLv3 license. It covers only
mentioned components that are provided by XTchain directly. For more information on that, refer to
the COPYING.md file. The final pre-built toolchain is covered by the licenses of the individual,
external projects. The full list of software incorporated into this toolchain is available in the
README.md file.
