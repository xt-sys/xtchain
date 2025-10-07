#!/bin/bash
# PROJECT:     XTchain
# LICENSE:     See the COPYING.md in the top level directory
# FILE:        build.sh
# DESCRIPTION: Toolchain crosscompilation and assembly script
# DEVELOPERS:  Rafal Kupiec <belliash@codingworkshop.eu.org>
#              Aiken Harris <harraiken91@gmail.com>

# Working Directories
BINDIR="$(pwd)/binaries"
PCHDIR="$(pwd)/patches"
SRCDIR="$(pwd)/sources"
WRKDIR="$(pwd)"

# Architecture Settings
ARCHS="aarch64 armv7 i686 x86_64"

# Default Configuration
BUILD_JOBS=0
BUILD_MINIMAL=0
CLEAN_BUILD=0
ENABLE_LLVM_ASSEMBLY=0
LLVM_DYNAMIC_LINK=ON
SYSTEM_NAME=Linux
TARGET_SYSTEM=linux

# CMake Settings
CMAKEDIR="${SRCDIR}/cmake"
CMAKETAG="v4.1.1"
CMAKEVCS="https://gitlab.kitware.com/cmake/cmake.git"

# LLVM Settings
LLVMDIR="${SRCDIR}/llvm"
LLVMTAG="llvmorg-21.1.3"
LLVMVCS="https://github.com/llvm/llvm-project.git"

# Mtools Settings
MTOOLSDIR="${SRCDIR}/mtools"
MTOOLSTAG="v4.0.49"
MTOOLSVCS="https://github.com/xt-sys/mtools.git"

# Ninja Settings
NINJADIR="${SRCDIR}/ninja"
NINJATAG="v1.13.1"
NINJAVCS="https://github.com/ninja-build/ninja.git"

# Wine Settings
WINEDIR="${SRCDIR}/wine"
WINETAG="wine-10.15"
WINEVCS="https://github.com/wine-mirror/wine.git"



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
    local CMAKE_PARAMETERS=""

    # Clean old build if necessary
    [ "${CLEAN_BUILD}" -eq 1 ] && rm -rf ${WINEDIR}/build-${SYSTEM_NAME}

    # Additional, target-specific configuration options
    case "${SYSTEM_NAME}" in
        Windows)
            CMAKE_PARAMETERS="${CMAKE_PARAMETERS} -DCMAKE_C_COMPILER=${SYSTEM_HOST}-gcc -DCMAKE_CXX_COMPILER=${SYSTEM_HOST}-g++"
            CMAKE_PARAMETERS="${CMAKE_PARAMETERS} -DCMAKE_RC_COMPILER=${SYSTEM_HOST}-windres"
            ;;
    esac

    # Build CMake
    echo ">>> Building CMAKE ..."
    mkdir -p ${CMAKEDIR}/build-${SYSTEM_NAME}
    cd ${CMAKEDIR}/build-${SYSTEM_NAME}
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        -DCMAKE_INSTALL_PREFIX=${BINDIR} \
        -DCMAKE_SYSTEM_NAME=${SYSTEM_NAME} \
        -DCMAKE_USE_OPENSSL=OFF \
        ${CMAKE_PARAMETERS} \
        ..
    cmake --build . --parallel ${BUILD_JOBS}
    cmake --install .
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
    local CMAKE_PARAMETERS=""
    local LLVM_ARCHS=()

    # Clean old build if necessary
    [ "${CLEAN_BUILD}" -eq 1 ] && rm -rf ${LLVMDIR}/llvm/build-${SYSTEM_NAME}

    # Set supported architectures
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

    # Disable LLVM assembly files (like BLAKE3 support) if not specified otherwise
    if [ "${ENABLE_LLVM_ASSEMBLY}" -ne 1 ]; then
        CMAKE_PARAMETERS="${CMAKE_PARAMETERS} -DLLVM_DISABLE_ASSEMBLY_FILES=ON"
    fi

    # Additional, target-specific configuration options
    case "${SYSTEM_NAME}" in
        Windows)
            CMAKE_PARAMETERS="${CMAKE_PARAMETERS} -DCMAKE_EXE_LINKER_FLAGS=-lpthread -DCMAKE_SHARED_LINKER_FLAGS=-lpthread"
            CMAKE_PARAMETERS="${CMAKE_PARAMETERS} -DCMAKE_C_COMPILER_TARGET=${SYSTEM_HOST} -DCMAKE_CXX_COMPILER_TARGET=${SYSTEM_HOST}"
            CMAKE_PARAMETERS="${CMAKE_PARAMETERS} -DCMAKE_RC_COMPILER=${SYSTEM_HOST}-windres -DLLVM_HOST_TRIPLE=${SYSTEM_HOST}"
            CMAKE_PARAMETERS="${CMAKE_PARAMETERS} -DCMAKE_FIND_ROOT=$(dirname $(readlink -f $(command -v ${SYSTEM_HOST}-windres)))/../${SYSTEM_HOST}"
            CMAKE_PARAMETERS="${CMAKE_PARAMETERS} -DCMAKE_CXX_FLAGS=-femulated-tls"
            ;;
    esac

    # Build LLVM
    echo ">>> Building LLVM ..."
    cd ${LLVMDIR}/llvm/tools
    for UTIL in clang lld; do
        if [ ! -e ${UTIL} ]; then
            ln -sf ../../${UTIL} .
        fi
    done
    mkdir -p ${LLVMDIR}/llvm/build-${SYSTEM_NAME}
    cd ${LLVMDIR}/llvm/build-${SYSTEM_NAME}
    cmake -G Ninja \
        -DCMAKE_BUILD_TYPE="Release" \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        -DCMAKE_INSTALL_PREFIX=${BINDIR} \
        -DCMAKE_SYSTEM_NAME="${SYSTEM_NAME}" \
        -DLLDB_INCLUDE_TESTS=OFF \
        -DLLVM_ENABLE_ASSERTIONS=OFF \
        -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld;lldb" \
        -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON \
        -DLLVM_BUILD_INSTRUMENTED=OFF \
        -DLLVM_LINK_LLVM_DYLIB=${LLVM_DYNAMIC_LINK} \
        -DLLVM_TARGETS_TO_BUILD="$(echo ${LLVM_ARCHS[@]} | tr ' ' ';')" \
        -DLLVM_TOOLCHAIN_TOOLS="llvm-addr2line;llvm-ar;llvm-as;llvm-cov;llvm-cvtres;llvm-cxxfilt;llvm-dlltool;llvm-lib;llvm-ml;llvm-nm;llvm-objdump;llvm-objcopy;llvm-pdbutil;llvm-profdata;llvm-ranlib;llvm-rc;llvm-readelf;llvm-readobj;llvm-size;llvm-strings;llvm-strip;llvm-symbolizer;llvm-windres" \
        -DLLVM_USE_LINKER=lld \
        -DLLDB_ENABLE_PYTHON=OFF \
        ${CMAKE_PARAMETERS} \
        ..
    ninja -j ${BUILD_JOBS} install/strip
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

# This function compiles and installs MTOOLS
mtools_build()
{
    local CONFIGURE_PARAMETERS=""
    local EXTENSION=""

    # Clean old build if necessary
    [ "${CLEAN_BUILD}" -eq 1 ] && rm -rf ${MTOOLSDIR}/build-${SYSTEM_NAME}

    # Additional, target-specific configuration options
    case "${SYSTEM_NAME}" in
        Windows)
            CONFIGURE_PARAMETERS="${CONFIGURE_PARAMETERS} --host=${SYSTEM_HOST}"
            EXTENSION=".exe"
            ;;
    esac

    # Build Mtools
    echo ">>> Building MTOOLS ..."
    mkdir -p ${MTOOLSDIR}/build-${SYSTEM_NAME}
    cd ${MTOOLSDIR}/build-${SYSTEM_NAME}
    ../configure ${CONFIGURE_PARAMETERS}
    make -j ${BUILD_JOBS}
    cp mtools${EXTENSION} ${BINDIR}/bin/
    for TOOL in mcat mcd mcopy mdel mdir mformat minfo mlabel mmd mmove mpartition mrd mren mshowfat mtype mzip; do
        cp mtools${EXTENSION} ${BINDIR}/bin/${TOOL}${EXTENSION}
    done
    cd ${WRKDIR}
}

# This function downloads MTOOLS from VCS
mtools_fetch()
{
    if [ ! -d ${MTOOLSDIR} ]; then
        echo ">>> Downloading MTOOLS ..."
        git clone --depth 1 --branch ${MTOOLSTAG} ${MTOOLSVCS} ${MTOOLSDIR}
        cd ${MTOOLSDIR}
        apply_patches ${MTOOLSDIR##*/} ${MTOOLSTAG}
        cd ${WRKDIR}
    fi
}

# This function compiles and installs NINJA
ninja_build()
{
    local EXTENSION=""
    local NINJA_CXX_COMPILER=""
    local NINJA_PLATFORM=""

    # Clean old build if necessary
    [ "${CLEAN_BUILD}" -eq 1 ] && rm -rf ${NINJADIR}/build-${SYSTEM_NAME}

    # Additional, target-specific configuration options
    case "${SYSTEM_NAME}" in
        Windows)
            EXTENSION=".exe"
            NINJA_CXX_COMPILER="${SYSTEM_HOST}-g++"
            NINJA_PLATFORM="mingw"
            ;;
        *)
            NINJA_CXX_COMPILER="clang++"
            NINJA_PLATFORM="linux"
            ;;
    esac

    # Build Ninja
    echo ">>> Building NINJA ..."
    mkdir -p ${NINJADIR}/build-${SYSTEM_NAME}
    cd ${NINJADIR}/build-${SYSTEM_NAME}
    CXX=${NINJA_CXX_COMPILER} ../configure.py --platform=${NINJA_PLATFORM}
    ninja -j ${BUILD_JOBS}
    install ninja${EXTENSION} ${BINDIR}/bin/
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

# Performs requirements check and prepares environment
prepare_environment()
{
    # Verify that the number of build jobs is a positive integer
    if [ -z "${BUILD_JOBS##*[!0-9]*}" ]; then
        echo "ERROR: Invalid number of jobs provided. Please enter a positive integer."
        exit 1
    fi

    # Verify that all required tools are installed
    for APP in {clang,clang++,cmake,lld,ninja,x86_64-w64-mingw32-windres}; do
        which ${APP} &> /dev/null
        if [ $? -ne 0 ]; then
            echo "ERROR: ${APP} not found. Please install the required tool."
            exit 2
        fi
    done

    # Set target-specific options
    case "${TARGET_SYSTEM}" in
        windows|*-mingw32)
            SYSTEM_NAME="Windows"
            SYSTEM_HOST="x86_64-w64-mingw32"
            ;;
        linux|*-linux-*)
            SYSTEM_NAME="Linux"
            ;;
        *)
            echo "ERROR: Invalid target system specified. Please choose a valid target system."
            exit 3
            ;;
    esac
}

# Prints usage help
print_usage()
{
    echo "USAGE: ${0} [--clean] [--enable-llvm-assembly] [--jobs=N] [--minimal] [--static-llvm] [--target={linux,windows}]"
    exit 1
}

# This function compiles and install WINE tools
wine_build()
{
    local CONFIGURE_PARAMETERS=""
    local EXTENSION=""

    # Clean old build if necessary
    [ "${CLEAN_BUILD}" -eq 1 ] && rm -rf ${WINEDIR}/{build-${SYSTEM_NAME},build-tools}

    # Additional, target-specific configuration options
    case "${SYSTEM_NAME}" in
        Windows)
            CONFIGURE_PARAMETERS="${CONFIGURE_PARAMETERS} --host=${SYSTEM_HOST}"
            EXTENSION=".exe"
            ;;
    esac

    # Build Wine (first configuration builds makedep)
    echo ">>> Building Wine ..."
    mkdir -p ${WINEDIR}/{build-${SYSTEM_NAME},build-tools}
    cd ${WINEDIR}/build-tools
    ../configure \
        --enable-win64 \
        --without-freetype \
        --without-x
    cd ${WINEDIR}/build-${SYSTEM_NAME}
    ../configure \
        --enable-tools \
        --enable-win64 \
        --with-wine-tools=${WINEDIR}/build-tools \
        --without-freetype \
        --without-x \
        ${CONFIGURE_PARAMETERS}
    for TOOL in widl wmc wrc; do
        make -j ${BUILD_JOBS} tools/${TOOL}/all
        cp tools/${TOOL}/${TOOL}${EXTENSION} ${BINDIR}/bin/
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

# This function installs XTCHAIN tools and scripts
xtchain_build()
{
    # Target-specific configuration options
    case "${SYSTEM_NAME}" in
        Windows)
            CCOMPILER="${SYSTEM_HOST}-gcc"
            ;;
        *)
            CCOMPILER="clang"
    esac

    # Build XTchain tools
    echo ">>> Building XTchain tools ..."
    mkdir -p ${BINDIR}/bin
    mkdir -p ${BINDIR}/lib/xtchain
    for EXEC in bin2c diskimg exetool xtcspecc; do
        if [ ! -e ${BINDIR}/bin/${EXEC} ]; then
            ${CCOMPILER} ${WRKDIR}/tools/${EXEC}.c -o ${BINDIR}/bin/${EXEC}
        fi
    done
    cp ${WRKDIR}/scripts/xtclib* ${BINDIR}/lib/xtchain/
    cp ${WRKDIR}/scripts/xtchain* ${BINDIR}/
}

# This function generates XTCHAIN version file and produces tarball archive
xtchain_tarball()
{
    local EXTENSION=""
    local LIBDIR=""

    # Additional, target-specific configuration options
    case "${SYSTEM_NAME}" in
        Windows)
            EXTENSION=".exe"
            ;;
    esac

    # Remove unneeded files to save disk space
    echo ">>> Removing unneeded files to save disk space ..."
    rm -rf ${BINDIR}/{doc,include,share/{bash-completion,emacs,info,locale,man,vim}}
    for EXEC in amdgpu-arch clang-check clang-exdef-mapping clang-import-test clang-offload-* clang-rename clang-scan-deps diagtool hmaptool ld64.lld modularize nxptx-arch wasm-ld; do
        rm -f ${BINDIR}/bin/${EXEC}${EXTENSION}
    done

    # Generate version file
    cd ${WRKDIR}
    : ${XTCVER:=$(git describe --exact-match --tags 2>/dev/null)}
    : ${XTCVER:=DEVEL}
    [ ${BUILD_MINIMAL} -eq 1 ] && XTCVER="${XTCVER}-lite"
    echo "${XTCVER}" > ${BINDIR}/Version

    # Windows target specific actions
    if [ "${SYSTEM_NAME}" == "Windows" ]; then
        # Replace symlinks with original files
        for LINK in $(find ${BINDIR}/bin -maxdepth 1 -type l); do
            cp -f --remove-destination $(readlink -e ${LINK}) ${LINK}
        done

        # Copy dynamic libraries
        if [ ${BUILD_MINIMAL} -eq 0 ]; then
            LIBDIR="$(dirname $(readlink -f $(command -v ${SYSTEM_HOST}-windres)))/../${SYSTEM_HOST}"
            for DLL in $(${SYSTEM_HOST}-objdump --private-headers ${BINDIR}/bin/cmake.exe | grep "DLL Name:" | cut -d' ' -f3 | grep "lib.*.dll"); do
                find ${LIBDIR} -type f -name ${DLL} -exec cp {} ${BINDIR}/bin \;
            done
        fi
    fi

    # Build tarball
    echo ">>> Creating toolchain archive ..."
    tar -I 'zstd -19' -cpf xtchain-${XTCVER}-${TARGET_SYSTEM}.tar.zst -C ${BINDIR} .
}



# Parse all arguments provided to the script
while [ $# -gt 0 ]; do
    case "$1" in
        --clean)
            # Performs clean build
            CLEAN_BUILD=1
            ;;
        --enable-llvm-assembly)
            # Enables LLVM asembly files compilation (like BLAKE3)
            ENABLE_LLVM_ASSEMBLY=1
            ;;
        --jobs=*)
            # Sets number of CPU cores used for compilation
            BUILD_JOBS="${1#*=}"
            ;;
        --minimal)
            BUILD_MINIMAL=1
            ;;
        --static-llvm)
            # Compiles LLVM statically
            LLVM_DYNAMIC_LINK=OFF
            ;;
        --target=*)
            # Sets the target system for built toolchain (Linux or Windows)
            TARGET_SYSTEM="${1#*=}"
            ;;
        *)
            # Prints help if any other parameter given
            print_usage
            ;;
    esac
    shift
done

# Prepare environment
prepare_environment

# Exit immediately on any failure
set -e

# Check number of CPU cores available
if [ ${BUILD_JOBS} -eq 0 ]; then
    unset BUILD_JOBS
    : ${BUILD_JOBS:=$(sysctl -n hw.ncpu 2>/dev/null)}
    : ${BUILD_JOBS:=$(nproc 2>/dev/null)}
    : ${BUILD_JOBS:=1}
fi

# Create working directories
mkdir -p ${BINDIR}
mkdir -p ${SRCDIR}

# Build XTchain tools
xtchain_build

# Download and build Wine tools
wine_fetch
wine_build

# Download and build GNU Mtools
mtools_fetch
mtools_build

if [ ${BUILD_MINIMAL} -eq 0 ]; then
    # Download and build LLVM
    llvm_fetch
    llvm_build

    # Download and build CMake
    cmake_fetch
    cmake_build

    # Download and build Ninja
    ninja_fetch
    ninja_build
fi

# Generate tarball archive
xtchain_tarball
