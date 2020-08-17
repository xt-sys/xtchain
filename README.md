## XT Toolchain
This is a GNU-based mingw-w64 toolchain. It currently supports C and C++, and provides a variety of
tools including IDL, message and resource compilers. The XT Toolchain is also the official build
environment for compiling XT software, including the FerretOS. Currently, it is targeted at Linux
host only, however it should be possible to build it in MSYS2 as well.

This software includes:
 * Binutils
 * CMake
 * GCC
 * Make
 * Mingw-w64
 * Ninja
 * Wine

## Licensing
The XTchain project includes the scripts for building and assembling a toolchain as well as
environmental shell. These are licensed under the GPLv3 license. It covers only mentioned
components that are provided by XTchain directly. For more information on that, refer to the
COPYING.md file. The final pre-built toolchain is covered by the licenses of the individual,
external projects. The full list of software incorporated into this toolchain is available in the
README.md file.
