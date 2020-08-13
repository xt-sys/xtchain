## XT Toolchain
This is an the XT toolchain based on MinGW-W64. It currently supports C and C++, and provides
a variety of tools. It can be used to build both Windows and XT software, including FerretOS.

This repository contains 2 branches:
 * gnu-toolchain: This is the GCC/Binutils based mingw-w64 toolchain.
 * llvm-toolchain: This is the LLVM/Clang/LLD based mingw-w64 toolchain.

## Licensing
The XTchain project includes the scripts for building and assembling a toolchain as well as other
scripts and wrappers. These are licensed under the GPLv3 license. It covers only mentioned
components that are provided by XTchain directly. For more information on that, refer to
the COPYING.md file. The final pre-built toolchain is covered by the licenses of the individual,
external projects. The full list of software incorporated into this toolchain is available in the
README.md file in the corresponding branch.
