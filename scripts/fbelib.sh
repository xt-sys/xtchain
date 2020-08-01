#!/usr/bin/env bash

# Sets the target architecture
charch()
{
    if [ "x${1}" == "x" ]; then
        echo "Syntax: charch [architecture]"
        return
    fi
    case ${1} in
        "i386"|"i486"|"i586"|"i686"|"x86")
            export TARGET="i386"
            ;;
        "amd64"|"x64"|"x86_64")
            export TARGET="amd64"
            ;;
        *)
            export TARGET="UNKNOWN"
    esac
    echo "Target Architecture: ${TARGET}"
}
export -f charch

# Displays version banner
version()
{
    echo "###############################################################################"
    echo "#                  FerretOS Build Environment v${FBEVER} for Linux                  #"
    echo "#               by Rafal Kupiec <belliash@codingworkshop.eu.org>              #"
    echo "###############################################################################"
    echo
    echo
    echo "Binutils Version: $(${FBEDIR}/bin/i686-w64-mingw32-ld -v | cut -d' ' -f5)"
    echo "GCC Version: $(${FBEDIR}/bin/i686-w64-mingw32-gcc -v 2>&1| grep 'gcc version' | cut -d' ' -f3)"
    echo "IDL Compiler Version: $(${FBEDIR}/bin/i686-w64-mingw32-widl -V | grep 'version' | cut -d' ' -f5)"
    charch ${TARGET}
    echo
    echo
}
export -f version
