#!/bin/bash
# PROJECT:     XTchain
# LICENSE:     See the COPYING.md in the top level directory
# FILE:        build-linux.sh
# DESCRIPTION: Toolchain building and assembly script
# DEVELOPERS:  Rafal Kupiec <belliash@codingworkshop.eu.org>


# Working Directories
BINDIR="$(pwd)/binaries"
PCHDIR="$(pwd)/patches"
SRCDIR="$(pwd)/sources"
WRKDIR="$(pwd)"

# Architecture Settings
ARCHS="aarch64 armv7 i686 x86_64"
GENERIC="generic-w64-mingw32"

# CMake Settings
CMAKEDIR="${SRCDIR}/cmake"
CMAKETAG="v3.18.1"
CMAKEVCS="https://gitlab.kitware.com/cmake/cmake.git"

# LLVM Settings
LLVMDIR="${SRCDIR}/llvm"
LLVMTAG="llvmorg-11.0.0"
LLVMVCS="https://github.com/llvm/llvm-project.git"

# Make Settings
MAKEDIR="${SRCDIR}/make"
MAKETAG="4.3"
MAKEVCS="git://git.savannah.gnu.org/make"

# Mingw-w64 Settings
MINGWDIR="${SRCDIR}/mingw-w64"
MINGWLIB="ucrt"
MINGWTAG="v8.0.0"
MINGWNTV="0x601"
MINGWVCS="https://github.com/mirror/mingw-w64.git"

# NASM Settings
NASMDIR="${SRCDIR}/nasm"
NASMTAG="nasm-2.15.05"
NASMVCS="https://github.com/netwide-assembler/nasm.git"

# Ninja Settings
NINJADIR="${SRCDIR}/ninja"
NINJATAG="v1.10.0"
NINJAVCS="https://github.com/ninja-build/ninja.git"

# Wine Settings
WINEDIR="${SRCDIR}/wine"
WINETAG="wine-5.15"
WINEVCS="git://source.winehq.org/git/wine.git"


# This function applies a patches to the 3rd party project
apply_patches()
{
    local PACKAGE="${1}"
    local VERSION="${2}"
    if [ -d "${PCHDIR}/${PACKAGE}/${VERSION}" ]; then
        PATCHES="$(find ${PCHDIR}/${PACKAGE}/${VERSION} -name '*.diff' -o -name '*.patch' | sort -n)"
        echo ">>> Applying custom patches ..."
        for PATCH in ${PATCHES}; do
            if [ -f "${PATCH}" ] && [ -r "${PATCH}" ]; then
                for PREFIX in {0..5}; do
                    if patch -i${PATCH} -p${PREFIX} --silent --dry-run >/dev/null; then
                        patch -i${PATCH} -p${PREFIX} --silent
                        echo ">>> Patch ${PATCH} applied ..."
                        break;
                    elif [ ${PREFIX} -ge 5 ]; then
                        echo "Patch ${PATCH} does not fit. Failed applying patch ..."
                        return 1
                    fi
                done
            fi
        done
    fi
}

# This function compiles and installs CMAKE
cmake_build()
{
    echo ">>> Building CMAKE ..."
    [ -z ${CLEAN} ] || rm -rf ${CMAKEDIR}/build-${GENERIC}
    mkdir -p ${CMAKEDIR}/build-${GENERIC}
    cd ${CMAKEDIR}/build-${GENERIC}
    ../bootstrap \
        --prefix=${BINDIR} \
        --parallel=${CORES} \
        -- -DCMAKE_USE_OPENSSL=OFF
    make -j${CORES}
    make install
    cd ${WRKDIR}
}

# This function downloads CMAKE from VCS
cmake_fetch()
{
    if [ ! -d ${CMAKEDIR} ]; then
        echo ">>> Downloading CMAKE ..."
        git clone ${CMAKEVCS} ${CMAKEDIR}
        cd ${CMAKEDIR}
        git checkout tags/${CMAKETAG}
        apply_patches ${CMAKEDIR##*/} ${CMAKETAG}
        cd ${WRKDIR}
    fi
}

# This function compiles and install LLVM
llvm_build()
{
    echo ">>> Building LLVM ..."
    [ -z ${CLEAN} ] || rm -rf ${LLVMDIR}/llvm/build
    LLVM_ARCHS=()
    for ARCH in ${ARCHS}; do
        case ${ARCH} in
            "aarch64")
                LLVM_ARCHS+=( "AArch64" )
                ;;
            "armv7")
                LLVM_ARCHS+=( "ARM" )
                ;;
            "i686"|"x86_64")
                LLVM_ARCHS+=( "X86" )
                ;;
        esac
    done
    LLVM_ARCHS=( $(for ARCH in ${LLVM_ARCHS[@]}; do echo ${ARCH}; done | sort -u) )
    cd ${LLVMDIR}/llvm/tools
    for UTIL in clang lld lldb; do
        if [ ! -e ${UTIL} ]; then
            ln -sf ../../${UTIL} .
        fi
    done
    mkdir -p ${LLVMDIR}/llvm/build
    cd ${LLVMDIR}/llvm/build
    cmake \
        -DCMAKE_BUILD_TYPE="Release" \
        -DCMAKE_INSTALL_PREFIX=${BINDIR} \
        -DLLDB_ENABLE_CURSES=FALSE \
        -DLLDB_ENABLE_LIBEDIT=FALSE \
        -DLLDB_ENABLE_LUA=FALSE \
        -DLLDB_ENABLE_PYTHON=FALSE \
        -DLLDB_INCLUDE_TESTS=FALSE \
        -DLLVM_ENABLE_ASSERTIONS=FALSE \
        -DLLVM_INSTALL_TOOLCHAIN_ONLY=TRUE \
        -DLLVM_TARGETS_TO_BUILD="$(echo ${LLVM_ARCHS[@]} | tr ' ' ';')" \
        -DLLVM_TOOLCHAIN_TOOLS="clang;llvm-addr2line;llvm-ar;llvm-as;llvm-cov;llvm-cvtres;llvm-dlltool;llvm-nm;llvm-objdump;llvm-objcopy;llvm-pdbutil;llvm-profdata;llvm-ranlib;llvm-rc;llvm-readobj;llvm-strings;llvm-strip;llvm-symbolizer" \
        ..
        make -j${CORES} install/strip
        cd ${WRKDIR}
}

# This function compiles and install LIBCXX
llvm_build_libcxx()
{
    echo ">>> Building LLVM libraries (libcxx) ..."
    for ARCH in ${ARCHS}; do
        [ -z ${CLEAN} ] || rm -rf ${LLVMDIR}/libcxx/build-${ARCH}
        mkdir -p ${LLVMDIR}/libcxx/build-${ARCH}
        cd ${LLVMDIR}/libcxx/build-${ARCH}
        cmake \
            -DCMAKE_BUILD_TYPE="Release" \
            -DCMAKE_INSTALL_PREFIX=${BINDIR}/${ARCH}-w64-mingw32 \
            -DCMAKE_AR="${BINDIR}/bin/llvm-ar" \
            -DCMAKE_C_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang" \
            -DCMAKE_C_COMPILER_WORKS=1 \
            -DCMAKE_CXX_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang++" \
            -DCMAKE_CXX_COMPILER_WORKS=1 \
            -DCMAKE_CROSSCOMPILING=TRUE \
            -DCMAKE_RANLIB="${BINDIR}/bin/llvm-ranlib" \
            -DCMAKE_SHARED_LINKER_FLAGS="-lunwind" \
            -DCMAKE_SYSTEM_NAME="Windows" \
            -DLLVM_PATH="${LLVMDIR}/llvm" \
            -DLIBCXX_CXX_ABI="libcxxabi" \
            -DLIBCXX_CXX_ABI_INCLUDE_PATHS="../../libcxxabi/include" \
            -DLIBCXX_CXX_ABI_LIBRARY_PATH="../../libcxxabi/build-${ARCH}/lib" \
            -DLIBCXX_ENABLE_ABI_LINKER_SCRIPT=FALSE \
            -DLIBCXX_ENABLE_EXCEPTIONS=TRUE \
            -DLIBCXX_ENABLE_EXPERIMENTAL_LIBRARY=FALSE \
            -DLIBCXX_ENABLE_FILESYSTEM=FALSE \
            -DLIBCXX_ENABLE_MONOTONIC_CLOCK=TRUE \
            -DLIBCXX_ENABLE_SHARED=TRUE \
            -DLIBCXX_ENABLE_STATIC=TRUE \
            -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=TRUE \
            -DLIBCXX_ENABLE_THREADS=TRUE \
            -DLIBCXX_HAS_WIN32_THREAD_API=TRUE \
            -DLIBCXX_HAVE_CXX_ATOMICS_WITHOUT_LIB=TRUE \
            -DLIBCXX_INCLUDE_TESTS=FALSE \
            -DLIBCXX_INSTALL_HEADERS=TRUE \
            -DLIBCXX_LIBDIR_SUFFIX="" \
            -DLIBCXX_USE_COMPILER_RT=TRUE \
            ..
        make -j${CORES}
        make install
        ${BINDIR}/bin/llvm-ar qcsL \
            ${BINDIR}/${ARCH}-w64-mingw32/lib/libc++.dll.a \
            ${BINDIR}/${ARCH}-w64-mingw32/lib/libunwind.dll.a
        ${BINDIR}/bin/llvm-ar qcsL \
            ${BINDIR}/${ARCH}-w64-mingw32/lib/libc++.a \
            ${BINDIR}/${ARCH}-w64-mingw32/lib/libunwind.a
        if [ ! -e ${BINDIR}/${ARCH}-w64-mingw32/bin ]; then
            mkdir -p ${BINDIR}/${ARCH}-w64-mingw32/bin
        fi
        cp lib/libc++.dll ${BINDIR}/${ARCH}-w64-mingw32/bin/
    done
    cd ${WRKDIR}
}

# This function compiles LIBCXXABI
llvm_build_libcxxabi()
{
    echo ">>> Building LLVM libraries (libcxxabi) ..."
    for ARCH in ${ARCHS}; do
        [ -z ${CLEAN} ] || rm -rf ${LLVMDIR}/libcxxabi/build-${ARCH}
        mkdir -p ${LLVMDIR}/libcxxabi/build-${ARCH}
        cd ${LLVMDIR}/libcxxabi/build-${ARCH}
        cmake \
            -DCMAKE_BUILD_TYPE="Release" \
            -DCMAKE_INSTALL_PREFIX=${BINDIR}/${ARCH}-w64-mingw32 \
            -DCMAKE_AR="${BINDIR}/bin/llvm-ar" \
            -DCMAKE_C_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang" \
            -DCMAKE_C_COMPILER_WORKS=1 \
            -DCMAKE_CROSSCOMPILING=TRUE \
            -DCMAKE_CXX_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang++" \
            -DCMAKE_CXX_COMPILER_WORKS=1 \
            -DCMAKE_CXX_FLAGS="-D_LIBCPP_HAS_THREAD_API_WIN32 -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS" \
            -DCMAKE_RANLIB="${BINDIR}/bin/llvm-ranlib" \
            -DCMAKE_SYSTEM_NAME="Windows" \
            -DLLVM_PATH="${LLVMDIR}/llvm" \
            -DLIBCXXABI_ENABLE_EXCEPTIONS=TRUE \
            -DLIBCXXABI_ENABLE_NEW_DELETE_DEFINITIONS=FALSE \
            -DLIBCXXABI_ENABLE_SHARED=FALSE \
            -DLIBCXXABI_ENABLE_THREADS=TRUE \
            -DLIBCXXABI_LIBCXX_INCLUDES="../../libcxx/include" \
            -DLIBCXXABI_LIBDIR_SUFFIX="" \
            -DLIBCXXABI_TARGET_TRIPLE="${ARCH}-w64-mingw32" \
            -DLIBCXXABI_USE_COMPILER_RT=TRUE \
            ..
        make -j${CORES}
    done
    cd ${WRKDIR}
}

# This function compiles and installs LIBUNWIND
llvm_build_libunwind()
{
    echo ">>> Building LLVM libraries (libunwind) ..."
    for ARCH in ${ARCHS}; do
        [ -z ${CLEAN} ] || rm -rf ${LLVMDIR}/libunwind/build-${ARCH}
        mkdir -p ${LLVMDIR}/libunwind/build-${ARCH}
        cd ${LLVMDIR}/libunwind/build-${ARCH}
        cmake \
            -DCMAKE_BUILD_TYPE="Release" \
            -DCMAKE_INSTALL_PREFIX=${BINDIR}/${ARCH}-w64-mingw32 \
            -DCMAKE_AR="${BINDIR}/bin/llvm-ar" \
            -DCMAKE_C_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang" \
            -DCMAKE_C_COMPILER_WORKS=1 \
            -DCMAKE_C_FLAGS="-Wno-dll-attribute-on-redeclaration" \
            -DCMAKE_CROSSCOMPILING=TRUE \
            -DCMAKE_CXX_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang++" \
            -DCMAKE_CXX_COMPILER_WORKS=1 \
            -DCMAKE_CXX_FLAGS="-Wno-dll-attribute-on-redeclaration" \
            -DCMAKE_SYSTEM_NAME="Windows" \
            -DCMAKE_RANLIB="${BINDIR}/bin/llvm-ranlib" \
            -DLLVM_COMPILER_CHECKED=TRUE \
            -DLLVM_PATH="${LLVMDIR}/llvm" \
            -DLIBUNWIND_ENABLE_CROSS_UNWINDING=FALSE \
            -DLIBUNWIND_ENABLE_SHARED=TRUE \
            -DLIBUNWIND_ENABLE_STATIC=TRUE \
            -DLIBUNWIND_ENABLE_THREADS=TRUE \
            -DLIBUNWIND_USE_COMPILER_RT=TRUE \
            ..
        make -j${CORES}
        make install
        if [ ! -e ${BINDIR}/${ARCH}-w64-mingw32/bin ]; then
            mkdir -p ${BINDIR}/${ARCH}-w64-mingw32/bin
        fi
        cp lib/libunwind.dll ${BINDIR}/${ARCH}-w64-mingw32/bin/
    done
    cd ${WRKDIR}
}

# This function compiles and install LLVM runtime
llvm_build_runtime()
{
    echo ">>> Building LLVM compiler runtime ..."
    for ARCH in ${ARCHS}; do
        [ -z ${CLEAN} ] || rm -rf ${LLVMDIR}/compiler-rt/build-${ARCH}
        mkdir -p ${LLVMDIR}/compiler-rt/build-${ARCH}
        cd ${LLVMDIR}/compiler-rt/build-${ARCH}
        case ${ARCH} in
            "armv7")
                BARCH="armv7"
                LARCH="arm"
                ;;
            "i686")
                if [ ! -e ${BINDIR}/i386-w64-mingw32 ]; then
                    ln -sf i686-w64-mingw32 ${BINDIR}/i386-w64-mingw32
                fi
                BARCH="i386"
                LARCH="i386"
                ;;
            *)
                BARCH="${ARCH}"
                LARCH="${ARCH}"
                ;;
        esac
        cmake \
            -DCMAKE_BUILD_TYPE="Release" \
            -DCMAKE_INSTALL_PREFIX=${BINDIR}/${ARCH}-w64-mingw32 \
            -DCMAKE_AR="${BINDIR}/bin/llvm-ar" \
            -DCMAKE_C_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang" \
            -DCMAKE_C_COMPILER_TARGET="${BARCH}-windows-gnu" \
            -DCMAKE_C_COMPILER_WORKS=1 \
            -DCMAKE_CXX_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang++" \
            -DCMAKE_CXX_COMPILER_WORKS=1 \
            -DCMAKE_RANLIB="${BINDIR}/bin/llvm-ranlib" \
            -DCMAKE_SYSTEM_NAME="Windows" \
            -DCOMPILER_RT_DEFAULT_TARGET_ONLY=TRUE \
            -DCOMPILER_RT_USE_BUILTINS_LIBRARY=TRUE \
            ../lib/builtins
            make -j${CORES}
            mkdir -p ${BINDIR}/lib/clang/${LLVMTAG#*-}/lib/windows
            for LIB in lib/windows/libclang_rt.*.a; do
                cp ${LIB} ${BINDIR}/lib/clang/${LLVMTAG#*-}/lib/windows/$(basename ${LIB} | sed s/${BARCH}/${LARCH}/)
            done
    done
    cd ${WRKDIR}
}

# This function downloads LLVM from VCS
llvm_fetch()
{
    if [ ! -d ${LLVMDIR} ]; then
        echo ">>> Downloading LLVM ..."
        git clone ${LLVMVCS} ${LLVMDIR}
        cd ${LLVMDIR}
        git checkout tags/${LLVMTAG}
        apply_patches ${LLVMDIR##*/} ${LLVMTAG##*-}
        cd ${WRKDIR}
    fi
}

# This function compiles and install MAKE
make_build()
{
    echo ">>> Building Make ..."
    [ -z ${CLEAN} ] || rm -rf ${MAKEDIR}/build
    cd ${MAKEDIR}
    ./bootstrap
    sed -i "s/-Werror//" maintMakefile
    mkdir -p ${MAKEDIR}/build
    cd ${MAKEDIR}/build
    ../configure \
        --prefix=${BINDIR} \
        --disable-dependency-tracking \
        --disable-silent-rules \
        --program-prefix=g \
        --without-guile
    make -j${CORES}
    make install
    if [ ! -e ${BINDIR}/bin/make ]; then
        ln -sf gmake ${BINDIR}/bin/make
    fi
    cd ${WRKDIR}
}

# This function downloads MAKE from VCS
make_fetch()
{
    if [ ! -d ${MAKEDIR} ]; then
        echo ">>> Downloading Make ..."
        git clone ${MAKEVCS} ${MAKEDIR}
        cd ${MAKEDIR}
        git checkout tags/${MAKETAG}
        apply_patches ${MAKEDIR##*/} ${MAKETAG}
        cd ${WRKDIR}
    fi
}

# This function compiles and installs MINGW CRT
mingw_build_crt()
{
    for ARCH in ${ARCHS}; do
        echo ">>> Building Mingw-W64 (CRT) for ${ARCH} ..."
        [ -z ${CLEAN} ] || rm -rf ${MINGWDIR}/mingw-w64-crt/build-${ARCH}
        mkdir -p ${MINGWDIR}/mingw-w64-crt/build-${ARCH}
        cd ${MINGWDIR}/mingw-w64-crt/build-${ARCH}
        case ${ARCH} in
            "aarch64")
                FLAGS="--disable-lib32 --disable-lib64 --enable-libarm64"
                ;;
            "armv7")
                FLAGS="--disable-lib32 --disable-lib64 --enable-libarm32"
                ;;
            "i686")
                FLAGS="--enable-lib32 --disable-lib64"
                ;;
            "x86_64")
                FLAGS="--disable-lib32 --enable-lib64"
                ;;
        esac
        ORIGPATH="${PATH}"
        PATH="${BINDIR}/bin:${PATH}"
        ../configure \
            --host=${ARCH}-w64-mingw32 \
            --prefix=${BINDIR}/${ARCH}-w64-mingw32 \
            --with-sysroot=${BINDIR} \
            --with-default-msvcrt=${MINGWLIB} \
            ${FLAGS}
        make -j${CORES}
        make install
        PATH="${ORIGPATH}"
    done
    cd ${WRKDIR}
}

# This function compiles and installs MINGW headers
mingw_build_headers()
{
    echo ">>> Building Mingw-W64 (headers) ..."
    [ -z ${CLEAN} ] || rm -rf ${MINGWDIR}/mingw-w64-headers/build-${GENERIC}
    mkdir -p ${MINGWDIR}/mingw-w64-headers/build-${GENERIC}
    cd ${MINGWDIR}/mingw-w64-headers/build-${GENERIC}
    ../configure \
        --prefix=${BINDIR}/${GENERIC} \
        --enable-idl \
        --with-default-msvcrt=${MINGWLIB} \
        --with-default-win32-winnt=${MINGWNTV}
    make -j${CORES}
    make install
    for ARCH in ${ARCHS}; do
        mkdir -p ${BINDIR}/${ARCH}-w64-mingw32
        if [ ! -e ${BINDIR}/${ARCH}-w64-mingw32/include ]; then
            ln -sfn ../${GENERIC}/include ${BINDIR}/${ARCH}-w64-mingw32/include
        fi
    done
    cd ${WRKDIR}
}

# This function compiles and install MINGW libraries
mingw_build_libs()
{
    for LIB in libmangle winpthreads winstorecompat; do
        echo ">>> Building Mingw-w64 (libs) for ${ARCH} ..."
        for ARCH in ${ARCHS}; do
            [ -z ${CLEAN} ] || rm -rf ${MINGWDIR}/mingw-w64-libraries/${LIB}/build-${ARCH}
            mkdir -p ${MINGWDIR}/mingw-w64-libraries/${LIB}/build-${ARCH}
            cd ${MINGWDIR}/mingw-w64-libraries/${LIB}/build-${ARCH}
            ORIGPATH="${PATH}"
            PATH="${BINDIR}/bin:${PATH}"
            ../configure \
                --host=${ARCH}-w64-mingw32 \
                --prefix=${BINDIR}/${ARCH}-w64-mingw32 \
                --libdir=${BINDIR}/${ARCH}-w64-mingw32/lib
            make -j${CORES}
            make install
            PATH="${ORIGPATH}"
        done
    done
    cd ${WRKDIR}
}

# This function compiles and installs MINGW tools
mingw_build_tools()
{
    for TOOL in gendef genidl genlib genpeimg widl; do
        for ARCH in ${ARCHS}; do
            echo ">>> Building Mingw-w64 (tools) for ${ARCH} ..."
            [ -z ${CLEAN} ] || rm -rf ${MINGWDIR}/mingw-w64-tools/${TOOL}/build-${ARCH}
            mkdir -p ${MINGWDIR}/mingw-w64-tools/${TOOL}/build-${ARCH}
            cd ${MINGWDIR}/mingw-w64-tools/${TOOL}/build-${ARCH}
            ../configure \
                --target=${ARCH}-w64-mingw32 \
                --prefix=${BINDIR}
            make -j${CORES}
            make install
            if [ -e ${BINDIR}/bin/${TOOL} ]; then
                mv ${BINDIR}/bin/${TOOL} ${BINDIR}/bin/${ARCH}-w64-mingw32-${TOOL}
            fi
        done
    done
    cd ${WRKDIR}
}

# This function downloads MINGW from VCS
mingw_fetch()
{
    if [ ! -d ${MINGWDIR} ]; then
        echo ">>> Downloading MinGW-w64 ..."
        git clone ${MINGWVCS} ${MINGWDIR}
        cd ${MINGWDIR}
        git checkout tags/${MINGWTAG}
        apply_patches ${MINGWDIR##*/} ${MINGWTAG}
        cd ${WRKDIR}
    fi
}

# This function compiles and installs NASM
nasm_build()
{
    cd ${NASMDIR}
    ./autogen.sh
    ./configure
    make -j${CORES}
    install nasm ndisasm ${BINDIR}/bin/
    cd ${WRKDIR}
}

# This function downloads NASM from VCS
nasm_fetch()
{
    if [ ! -d ${NASMDIR} ]; then
        echo ">>> Downloading NASM ..."
        git clone ${NASMVCS} ${NASMDIR}
        cd ${NASMDIR}
        git checkout tags/${NASMTAG}
        apply_patches ${NASMDIR##*/} ${NASMTAG##*-}
        cd ${WRKDIR}
    fi
}

# This function compiles and installs NINJA
ninja_build()
{
    echo ">>> Building NINJA ..."
    [ -z ${CLEAN} ] || rm -rf ${NINJADIR}/build-${GENERIC}
    mkdir -p ${NINJADIR}/build-${GENERIC}
    cd ${NINJADIR}/build-${GENERIC}
    ../configure.py --bootstrap
    install ninja ${BINDIR}/bin/
    cd ${WRKDIR}
}

# This function downloads NINJA from VCS
ninja_fetch()
{
    if [ ! -d ${NINJADIR} ]; then
        echo ">>> Downloading NINJA ..."
        git clone ${NINJAVCS} ${NINJADIR}
        cd ${NINJADIR}
        git checkout tags/${NINJATAG}
        apply_patches ${NINJADIR##*/} ${NINJATAG}
        cd ${WRKDIR}
    fi
}

# This function compiles and install WINE tools
wine_build()
{
    echo ">>> Building Wine ..."
    mkdir -p ${WINEDIR}/build
    cd ${WINEDIR}/build
    ../configure \
        -enable-win64 \
        --without-x
    for TOOL in winedump wmc wrc; do
        make -j${CORES} tools/${TOOL}
        cp tools/${TOOL}/${TOOL} ${BINDIR}/bin/
        for ARCH in ${ARCHS}; do
            if [ ! -e ${BINDIR}/bin/${ARCH}-w64-mingw32-${TOOL} ]; then
                ln -sf ${TOOL} ${BINDIR}/bin/${ARCH}-w64-mingw32-${TOOL}
            fi
        done
    done
    cd ${WRKDIR}
}

# This function downloads WINE from VCS
wine_fetch()
{
    if [ ! -d ${WINEDIR} ]; then
        echo ">>> Downloading WINE ..."
        git clone ${WINEVCS} ${WINEDIR}
        cd ${WINEDIR}
        git checkout tags/${WINETAG}
        apply_patches ${WINEDIR##*/} ${WINETAG##*-}
        cd ${WRKDIR}
    fi
}

# This function installs XTCHAIN scripts, wrappers and symlinks
xtchain_build()
{
    echo ">>> Building XTchain ..."
    mkdir -p ${BINDIR}/bin
    mkdir -p ${BINDIR}/lib/xtchain
    mkdir -p ${BINDIR}/${GENERIC}/bin
    cp ${WRKDIR}/scripts/*-wrapper ${BINDIR}/${GENERIC}/bin
    for ARCH in ${ARCHS}; do
        for EXEC in c++ c11 c99 cc clang clang++ g++ gcc; do
            ln -sf ../${GENERIC}/bin/clang-target-wrapper ${BINDIR}/bin/${ARCH}-w64-mingw32-${EXEC}
        done
        for EXEC in addr2line ar as nm objcopy pdbutil ranlib rc strings strip; do
            ln -sf llvm-${EXEC} ${BINDIR}/bin/${ARCH}-w64-mingw32-${EXEC}
        done
        for EXEC in dlltool ld objdump; do
            ln -sf ../${GENERIC}/bin/${EXEC}-wrapper ${BINDIR}/bin/${ARCH}-w64-mingw32-${EXEC}
        done
        for EXEC in windres xtcspecc; do
            if [ ! -e ${BINDIR}/bin/${EXEC} ]; then
                gcc ${WRKDIR}/tools/${EXEC}.c -o ${BINDIR}/bin/${EXEC}
            fi
            ln -sf ${EXEC} ${BINDIR}/bin/${ARCH}-w64-mingw32-${EXEC}
        done
    done
    cp ${WRKDIR}/scripts/xtclib ${BINDIR}/lib/xtchain/
    cp ${WRKDIR}/scripts/xtchain ${BINDIR}/
}


# Exit immediately on any failure
set -e

# Check if script launched as root
if [ "$(whoami)" = "root" ]; then
    echo "This script cannot be run as root!"
    exit 1
fi

# Check number of CPU cores available
: ${CORES:=$(sysctl -n hw.ncpu 2>/dev/null)}
: ${CORES:=$(nproc 2>/dev/null)}
: ${CORES:=1}

# Create working directories
mkdir -p ${BINDIR}
mkdir -p ${SRCDIR}

# XTchain
xtchain_build

# Download LLVM
llvm_fetch

# Build and install LLVM
llvm_build

# Download NASM
nasm_fetch

# Build and install NASM
nasm_build

# Download Mingw-W64
mingw_fetch

# Build and install Mingw-W64 headers
mingw_build_headers

# Build and install Mingw-W64 CRT
mingw_build_crt

# Build and install LLVM compiler runtime
llvm_build_runtime

# Build and install Mingw-W64 libraries
mingw_build_libs

# Build and install Mingw-W64 tools
mingw_build_tools

# Build LLVM libraries
llvm_build_libunwind
llvm_build_libcxxabi
llvm_build_libcxx

# Download Wine
wine_fetch

# Build and install Wine tools
wine_build

# Download Make
make_fetch

# Build and install Make
make_build

# Download CMake
cmake_fetch

# Build and install CMake
cmake_build

# Download Ninja
ninja_fetch

# Build and install Ninja
ninja_build

# Remove unneeded files to save disk space
echo ">>> Removing unneeded files to save disk space ..."
rm -rf ${BINDIR}/{doc,include,share/{bash-completion,emacs,info,locale,man,vim}}
rm -rf ${BINDIR}/bin/{clang-{check,exdef-mapping,import-test,offload-*,rename,scan-deps},hmaptool,ld64.lld,wasm-ld}

# Save XT Toolchain version
cd ${WRKDIR}
: ${XTCVER:=$(git describe --exact-match --tags 2>/dev/null)}
: ${XTCVER:=DEV}
echo "${XTCVER}" > ${BINDIR}/Version

# Prepare archive
echo ">>> Creating toolchain archive ..."
tar -I 'zstd -19' -cpf xtchain-${XTCVER}-linux.tar.zst -C ${BINDIR} .
