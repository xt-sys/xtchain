#!/bin/bash
set -e

# Working Directories
SRCDIR="$(pwd)/sources"
BINDIR="$(pwd)/binaries"
WRKDIR="$(pwd)"

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

# Mingw-w64 Settings
MINGWDIR="${SRCDIR}/mingw-w64"
MINGWTAG="v6.0.0"
MINGWVCS="https://github.com/mirror/mingw-w64.git"

# Ninja Settings
NINJADIR="${SRCDIR}/ninja"
NINJATAG="v1.10.0"
NINJAVCS="https://github.com/ninja-build/ninja.git"

# Architecture Settings
ARCHS="i686 x86_64"
GENERIC="generic-w64-mingw32"


binutils_build()
{
    for ARCH in ${ARCHS}; do
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
            --enable-lto \
            --enable-plugins
        make -j${CORES}
        make install
    done
    cd ${WRKDIR}
}

binutils_fetch()
{
    if [ ! -d ${BINUTILSDIR} ]; then
        git clone ${BINUTILSVCS} ${BINUTILSDIR}
        cd ${BINUTILSDIR}
        git checkout tags/${BINUTILSTAG}
        cd ${WRKDIR}
    fi
}

cmake_build()
{
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

cmake_fetch()
{
    if [ ! -d ${CMAKEDIR} ]; then
        git clone ${CMAKEVCS} ${CMAKEDIR}
        cd ${CMAKEDIR}
        git checkout tags/${CMAKETAG}
        cd ${WRKDIR}
    fi
}

gcc_build_phase1()
{
    for ARCH in ${ARCHS}; do
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

gcc_build_phase2()
{
    for ARCH in ${ARCHS}; do
        cd ${GCCDIR}/build-${ARCH}
        make -j${CORES}
        make install
    done
    cd ${WRKDIR}
}

gcc_fetch()
{
    if [ ! -d ${GCCDIR} ]; then
        git clone ${GCCVCS} ${GCCDIR}
        cd ${GCCDIR}
        git checkout tags/${GCCTAG}
        ./contrib/download_prerequisites
        cd ${WRKDIR}
    fi
}

mingw_build_crt()
{
    for ARCH in ${ARCHS}; do
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
            --with-default-msvcrt=msvcrt \
            ${FLAGS}
        make -j${CORES}
        make install
        PATH="${ORIGPATH}"
    done
    cd ${WRKDIR}
}

mingw_build_headers()
{
    [ -z ${CLEAN} ] || rm -rf ${MINGWDIR}/mingw-w64-headers/build-${GENERIC}
    mkdir -p ${MINGWDIR}/mingw-w64-headers/build-${GENERIC}
    cd ${MINGWDIR}/mingw-w64-headers/build-${GENERIC}
    ../configure \
        --prefix=${BINDIR}/${GENERIC} \
        --enable-idl \
        --with-default-msvcrt=msvcrt \
        --with-default-win32-winnt=0x502
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

mingw_build_libs()
{
    for LIB in libmangle winstorecompat; do
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

mingw_build_tools()
{
    for TOOL in gendef genidl genlib genpeimg widl; do
        for ARCH in ${ARCHS}; do
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

mingw_fetch()
{
    if [ ! -d ${MINGWDIR} ]; then
        git clone ${MINGWVCS} ${MINGWDIR}
        cd ${MINGWDIR}
        git checkout tags/${MINGWTAG}
        cd ${WRKDIR}
    fi
}

ninja_build()
{
    [ -z ${CLEAN} ] || rm -rf ${NINJADIR}/build-${GENERIC}
    mkdir -p ${NINJADIR}/build-${GENERIC}
    cd ${NINJADIR}/build-${GENERIC}
    ../configure.py --bootstrap
    install ninja ${BINDIR}/bin/
    cd ${WRKDIR}
}

ninja_fetch()
{
    if [ ! -d ${NINJADIR} ]; then
        git clone ${NINJAVCS} ${NINJADIR}
        cd ${NINJADIR}
        git checkout tags/${NINJATAG}
        cd ${WRKDIR}
    fi
}

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

# Download CMake
cmake_fetch

# Build and install CMake
cmake_build

# Download Ninja
ninja_fetch

# Build and install Ninja
ninja_build

# Remove unneeded files to save disk space
echo "Removing unneeded files to save disk space..."
rm -rf ${BINDIR}/{doc,include,mingw,share/{bash-completion,emacs,gcc*,info,man,vim}}

# Copy all scripts
echo "Copying scripts..."
cp -apf ${WRKDIR}/scripts/* ${BINDIR}/

# Save FBE version
cd ${WRKDIR}
: ${FBEVER:=$(git describe --exact-match --tags 2>/dev/null)}
: ${FBEVER:=DEV}
echo "${FBEVER}" > ${BINDIR}/Version

# Prepare archive
echo "Creating toolchain archive..."
tar --zstd -cf fbe-${FBEVER}-linux.tar.zst -C ${BINDIR} .
