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
CMAKETAG="v3.29.3"
CMAKEVCS="https://gitlab.kitware.com/cmake/cmake.git"

# LLVM Settings
LLVMDIR="${SRCDIR}/llvm"
LLVMTAG="llvmorg-18.1.7"
LLVMVCS="https://github.com/llvm/llvm-project.git"

# Make Settings
MAKEDIR="${SRCDIR}/make"
MAKETAG="4.4.1"
MAKEVCS="git://git.savannah.gnu.org/make"

# Mingw-w64 Settings
MINGWDIR="${SRCDIR}/mingw-w64"
MINGWLIB="ucrt"
MINGWTAG="master"
MINGWNTV="0x601"
MINGWVCS="https://github.com/mirror/mingw-w64.git"

# Ninja Settings
NINJADIR="${SRCDIR}/ninja"
NINJATAG="v1.12.1"
NINJAVCS="https://github.com/ninja-build/ninja.git"

# Wine Settings
WINEDIR="${SRCDIR}/wine"
WINETAG="wine-9.8"
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
        git clone --depth 1 --branch ${CMAKETAG} ${CMAKEVCS} ${CMAKEDIR}
        cd ${CMAKEDIR}
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
    for UTIL in clang lld; do
        if [ ! -e ${UTIL} ]; then
            ln -sf ../../${UTIL} .
        fi
    done
    mkdir -p ${LLVMDIR}/llvm/build
    cd ${LLVMDIR}/llvm/build
    cmake -G Ninja \
        -DCMAKE_BUILD_TYPE="Release" \
        -DCMAKE_INSTALL_PREFIX=${BINDIR} \
        -DLLDB_INCLUDE_TESTS=FALSE \
        -DLLVM_ENABLE_ASSERTIONS=FALSE \
        -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld;lldb" \
        -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON \
        -DLLVM_LINK_LLVM_DYLIB=ON \
        -DLLVM_TARGETS_TO_BUILD="$(echo ${LLVM_ARCHS[@]} | tr ' ' ';')" \
        -DLLVM_TOOLCHAIN_TOOLS="llvm-addr2line;llvm-ar;llvm-as;llvm-cov;llvm-cvtres;llvm-dlltool;llvm-lib;llvm-ml;llvm-nm;llvm-objdump;llvm-objcopy;llvm-pdbutil;llvm-profdata;llvm-ranlib;llvm-rc;llvm-readelf;llvm-readobj;llvm-strings;llvm-strip;llvm-symbolizer;llvm-windres" \
        ..
        ninja install/strip
        cd ${WRKDIR}
}

# This function compiles and install LIBCXX & LIBUNWIND
llvm_build_libs()
{
    echo ">>> Building LLVM libraries (libcxx) ..."
    for ARCH in ${ARCHS}; do
        [ -z ${CLEAN} ] || rm -rf ${LLVMDIR}/runtimes/build-${ARCH}
        mkdir -p ${LLVMDIR}/runtimes/build-${ARCH}
        cd ${LLVMDIR}/runtimes/build-${ARCH}
        cmake -G Ninja \
            -DCMAKE_BUILD_TYPE="Release" \
            -DCMAKE_INSTALL_PREFIX="${BINDIR}/${ARCH}-w64-mingw32" \
            -DCMAKE_AR="${BINDIR}/bin/llvm-ar" \
            -DCMAKE_C_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang" \
            -DCMAKE_C_COMPILER_WORKS=1 \
            -DCMAKE_C_FLAGS_INIT=-mguard=cf \
            -DCMAKE_CXX_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang++" \
            -DCMAKE_CXX_COMPILER_TARGET=${ARCH}-w64-windows-gnu \
            -DCMAKE_CXX_COMPILER_WORKS=1 \
            -DCMAKE_CXX_FLAGS_INIT=-mguard=cf \
            -DCMAKE_CROSSCOMPILING=TRUE \
            -DCMAKE_RANLIB="${BINDIR}/bin/llvm-ranlib" \
            -DCMAKE_SYSTEM_NAME="Windows" \
            -DLLVM_PATH="${LLVMDIR}/llvm" \
            -DLLVM_ENABLE_RUNTIMES="libunwind;libcxxabi;libcxx" \
            -DLIBUNWIND_USE_COMPILER_RT=TRUE \
            -DLIBUNWIND_ENABLE_SHARED=TRUE \
            -DLIBUNWIND_ENABLE_STATIC=TRUE \
            -DLIBCXX_USE_COMPILER_RT=ON \
            -DLIBCXX_ENABLE_SHARED=TRUE \
            -DLIBCXX_ENABLE_STATIC=TRUE \
            -DLIBCXX_ENABLE_EXPERIMENTAL_LIBRARY=OFF \
            -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=TRUE \
            -DLIBCXX_CXX_ABI="libcxxabi" \
            -DLIBCXX_LIBDIR_SUFFIX="" \
            -DLIBCXX_INCLUDE_TESTS=FALSE \
            -DLIBCXX_ENABLE_ABI_LINKER_SCRIPT=FALSE \
            -DLIBCXXABI_USE_COMPILER_RT=ON \
            -DLIBCXXABI_ENABLE_SHARED=OFF \
            -DLIBCXXABI_LIBDIR_SUFFIX="" \
            -DLIBCXXABI_USE_LLVM_UNWINDER=ON \
            ..
        ninja
        ninja install
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
        cmake -G Ninja \
            -DCMAKE_BUILD_TYPE="Release" \
            -DCMAKE_AR="${BINDIR}/bin/llvm-ar" \
            -DCMAKE_C_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang" \
            -DCMAKE_C_COMPILER_TARGET="${ARCH}-windows-gnu" \
            -DCMAKE_C_FLAGS_INIT="-mguard=cf" \
            -DCMAKE_CXX_COMPILER="${BINDIR}/bin/${ARCH}-w64-mingw32-clang++" \
            -DCMAKE_CXX_FLAGS_INIT="-mguard=cf" \
            -DCMAKE_FIND_ROOT_PATH="${BINDIR}/${ARCH}-w64-mingw32" \
            -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
            -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
            -DCMAKE_INSTALL_PREFIX=$(${BINDIR}/bin/${ARCH}-w64-mingw32-clang --print-resource-dir) \
            -DCMAKE_RANLIB="${BINDIR}/bin/llvm-ranlib" \
            -DCMAKE_SYSTEM_NAME="Windows" \
            -DCOMPILER_RT_BUILD_BUILTINS=TRUE \
            -DCOMPILER_RT_DEFAULT_TARGET_ONLY=TRUE \
            -DCOMPILER_RT_USE_BUILTINS_LIBRARY=TRUE \
            -DLLVM_CONFIG_PATH="" \
            -DSANITIZER_CXX_ABI="libc++" \
            ../lib/builtins
            ninja
            ninja install
    done
    cd ${WRKDIR}
}

# This function downloads LLVM from VCS
llvm_fetch()
{
    if [ ! -d ${LLVMDIR} ]; then
        echo ">>> Downloading LLVM ..."
        git clone --depth 1 --branch ${LLVMTAG} ${LLVMVCS} ${LLVMDIR}
        cd ${LLVMDIR}
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
        git clone --depth 1 --branch ${MAKETAG} ${MAKEVCS} ${MAKEDIR}
        cd ${MAKEDIR}
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
            --enable-cfguard \
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
                --libdir=${BINDIR}/${ARCH}-w64-mingw32/lib \
                CFLAGS="-O2 -mguard=cf" \
                CXXFLAGS="-O2 -mguard=cf"
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
        git clone --depth 1 --branch ${MINGWTAG} ${MINGWVCS} ${MINGWDIR}
        cd ${MINGWDIR}
        apply_patches ${MINGWDIR##*/} ${MINGWTAG}
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
        git clone --depth 1 --branch ${NINJATAG} ${NINJAVCS} ${NINJADIR}
        cd ${NINJADIR}
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
        --without-freetype \
        --without-x
    for TOOL in winedump wmc wrc; do
        make -j${CORES} tools/${TOOL}/all
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
        git clone --depth 1 --branch ${WINETAG} ${WINEVCS} ${WINEDIR}
        cd ${WINEDIR}
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
        for EXEC in bin2c exetool windres xtcspecc; do
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

# Check number of CPU cores available
if [[ ! -n ${CORES} ]]; then
	: ${CORES:=$(sysctl -n hw.ncpu 2>/dev/null)}
	: ${CORES:=$(nproc 2>/dev/null)}
	: ${CORES:=1}
fi

# Create working directories
mkdir -p ${BINDIR}
mkdir -p ${SRCDIR}

# XTchain
xtchain_build

# Download LLVM
llvm_fetch

# Build and install LLVM
llvm_build

# Download Mingw-W64
mingw_fetch

# Build and install Mingw-W64 headers
mingw_build_headers

# Build and install Mingw-W64 CRT
mingw_build_crt

# Build and install LLVM compiler runtime
llvm_build_runtime

# Build and install LLVM compiler libraries
llvm_build_libs

# Build and install Mingw-W64 libraries
mingw_build_libs

# Build and install Mingw-W64 tools
mingw_build_tools

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
rm -rf ${BINDIR}/bin/amdgpu-arch,{clang-{check,exdef-mapping,import-test,offload-*,rename,scan-deps},diagtool,hmaptool,ld64.lld,modularize,nxptx-arch,wasm-ld}

# Save XT Toolchain version
cd ${WRKDIR}
: ${XTCVER:=$(git describe --exact-match --tags 2>/dev/null)}
: ${XTCVER:=DEVEL}
echo "${XTCVER}" > ${BINDIR}/Version

# Prepare archive
echo ">>> Creating toolchain archive ..."
tar -I 'zstd -19' -cpf xtchain-${XTCVER}-linux.tar.zst -C ${BINDIR} .
