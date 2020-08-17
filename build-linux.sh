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
ARCHS="i686 x86_64"
GENERIC="generic-w64-mingw32"

# Compiler Flags
CFLAGS="-march=x86-64 -mtune=generic -O2 -s -pipe"
CXXFLAGS="${CFLAGS}"

# Binutils Settings
BINUTILSDIR="${SRCDIR}/binutils"
BINUTILSTAG="binutils-2_35"
BINUTILSVCS="git://sourceware.org/git/binutils-gdb.git"

# CMake Settings
CMAKEDIR="${SRCDIR}/cmake"
CMAKETAG="v3.18.1"
CMAKEVCS="https://gitlab.kitware.com/cmake/cmake.git"

# GCC Settings
GCCDIR="${SRCDIR}/gcc"
GCCTAG="releases/gcc-9.3.0"
GCCVCS="git://gcc.gnu.org/git/gcc.git"

# Make Settings
MAKEDIR="${SRCDIR}/make"
MAKETAG="4.3"
MAKEVCS="git://git.savannah.gnu.org/make"

# Mingw-w64 Settings
MINGWDIR="${SRCDIR}/mingw-w64"
MINGWLIB="msvcrt"
MINGWTAG="v6.0.0"
MINGWNTV="0x502"
MINGWVCS="https://github.com/mirror/mingw-w64.git"

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

# This function compiles and installs BINUTILS
binutils_build()
{
    for ARCH in ${ARCHS}; do
        echo ">>> Building BINUTILS for ${ARCH} ..."
        [ -z ${CLEAN} ] || rm -rf ${BINUTILSDIR}/build-${ARCH}
        mkdir -p ${BINUTILSDIR}/build-${ARCH}
        cd ${BINUTILSDIR}/build-${ARCH}
        ../configure \
            --target=${ARCH}-w64-mingw32 \
            --prefix=${BINDIR} \
            --with-sysroot=${BINDIR} \
            --with-zlib=yes \
            --disable-multilib \
            --disable-nls \
            --disable-werror \
            --enable-gold \
            --enable-lto \
            --enable-plugins
        make -j${CORES}
        make install
    done
    cd ${WRKDIR}
}

# This function downloads BINUTILS from VCS
binutils_fetch()
{
    if [ ! -d ${BINUTILSDIR} ]; then
        echo ">>> Downloading BINUTILS ..."
        git clone ${BINUTILSVCS} ${BINUTILSDIR}
        cd ${BINUTILSDIR}
        git checkout tags/${BINUTILSTAG}
        apply_patches ${BINUTILSDIR##*/} ${BINUTILSTAG##*-}
        cd ${WRKDIR}
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

# This function compiles and install GCC (phase 1)
gcc_build_phase1()
{
    for ARCH in ${ARCHS}; do
        echo ">>> Building GCC (phase1) for ${ARCH} ..."
        [ -z ${CLEAN} ] || rm -rf ${GCCDIR}/build-${ARCH}
        mkdir -p ${GCCDIR}/build-${ARCH}
        cd ${GCCDIR}/build-${ARCH}
        ../configure \
            --target=${ARCH}-w64-mingw32 \
            --prefix=${BINDIR} \
            --with-sysroot=${BINDIR} \
            --with-pkgversion="FerretOS Build Environment" \
            --without-zstd \
            --disable-libstdcxx-verbose \
            --disable-multilib \
            --disable-nls \
            --disable-shared \
            --disable-werror \
            --disable-win32-registry \
            --enable-fully-dynamic-string \
            --enable-languages=c,c++ \
            --enable-lto \
            --enable-sjlj-exceptions \
            --enable-version-specific-runtime-libs
        make -j${CORES} all-gcc
        make install-gcc
        make install-lto-plugin 
    done
    cd ${WRKDIR}
}

# This function compiles and install GCC (phase 2)
gcc_build_phase2()
{
    for ARCH in ${ARCHS}; do
        echo ">>> Building GCC (phase2) for ${ARCH} ..."
        cd ${GCCDIR}/build-${ARCH}
        make -j${CORES}
        make install
    done
    cd ${WRKDIR}
}

# This function downloads GCC from VCS
gcc_fetch()
{
    if [ ! -d ${GCCDIR} ]; then
        echo ">>> Downloading GCC ..."
        git clone ${GCCVCS} ${GCCDIR}
        cd ${GCCDIR}
        git checkout tags/${GCCTAG}
        apply_patches ${GCCDIR##*/} ${GCCTAG##*-}
        ./contrib/download_prerequisites
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
        echo ">>> Building Mingw-w64 (CRT) for ${ARCH} ..."
        [ -z ${CLEAN} ] || rm -rf ${MINGWDIR}/mingw-w64-crt/build-${ARCH}
        mkdir -p ${MINGWDIR}/mingw-w64-crt/build-${ARCH}
        cd ${MINGWDIR}/mingw-w64-crt/build-${ARCH}
        case ${ARCH} in
            i686)
                FLAGS="--enable-lib32 --disable-lib64"
                ;;
            x86_64)
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
    echo ">>> Building Mingw-w64 (headers) ..."
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
    mkdir -p ${BINDIR}/mingw
    if [ ! -e ${BINDIR}/mingw/include ]; then
        ln -sfn ../${GENERIC}/include ${BINDIR}/mingw/include
    fi
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
    for LIB in libmangle winstorecompat; do
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
        echo ">>> Downloading Mingw-w64 ..."
        git clone ${MINGWVCS} ${MINGWDIR}
        cd ${MINGWDIR}
        git checkout tags/${MINGWTAG}
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
        -enable-win64
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
    for ARCH in ${ARCHS}; do
        for EXEC in xtcspecc; do
            if [ ! -e ${BINDIR}/bin/${EXEC} ]; then
                gcc ${WRKDIR}/tools/${EXEC}.c -o ${BINDIR}/bin/${EXEC}
            fi
            ln -sf ${EXEC} ${BINDIR}/bin/${ARCH}-w64-mingw32-${EXEC}
        done
    done
    cp ${WRKDIR}/scripts/xtclib ${BINDIR}/lib/xtchain/
    cp ${WRKDIR}/scripts/xtchain ${BINDIR}/
    cd ${WRKDIR}
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

# Export compiler flags
export CFLAGS
export CXXFLAGS

# XTchain
xtchain_build

# Download Mingw-W64
mingw_fetch

# Build and install Mingw-W64 headers
mingw_build_headers

# Download Binutils
binutils_fetch

# Build and install Binutils
binutils_build

# Download GCC
gcc_fetch

# Build and install minimal GCC
gcc_build_phase1

# Build and install MSVCRT
mingw_build_crt

# Build and install GCC
gcc_build_phase2

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
rm -rf ${BINDIR}/{doc,include,mingw,share/{bash-completion,emacs,gcc*,info,man,vim}}

# Save XT Toolchain version
cd ${WRKDIR}
: ${XTCVER:=$(git describe --exact-match --tags 2>/dev/null)}
: ${XTCVER:=DEV}
echo "${XTCVER}" > ${BINDIR}/Version

# Prepare archive
echo ">>> Creating toolchain archive ..."
tar -I 'zstd -19' -cpf xtchain-${XTCVER}-linux.tar.zst -C ${BINDIR} .
