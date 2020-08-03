#!/usr/bin/env bash

# Check if script launched as root
if [ "$(whoami)" = "root" ]; then
    echo "This script cannot be run as root!"
    exit 1
fi

# Get the absolute path to the FBE
export FBEDIR="$(realpath $(dirname ${0}))"

# Read the FBE version
export FBEVER="$(cat ${FBEDIR}/Version)"

# Load the library
source ${FBEDIR}/fbelib.sh

# Set the target architecture
: ${TARGET:=${1}}
: ${TARGET:=i386}

# Save the source directory
export SRCDIR="${2:-${PWD}}"

# Make sure the compiler flags are clean
export HOST=
export CFLAGS=
export CXXFLAGS=
export LDFLAGS=

# Update PATH
export PATH="${FBEDIR}/bin:${PATH}"

# Display banner
version

# Invoke shell with fancy prompt
export PFMAT1="\[\033[0;1;97;44m\]"
export PFMAT2="\[\033[0;34;104m\]"
export PFMAT3="\[\033[0;1;97;104m\]"
export PFMAT4="\[\033[0;94;49m\]"
export PFMAT5="\[\033[1;38;5;74m\]"
export PROMPT="\n${PFMAT1} FerretOS BE ${PFMAT2}${PFMAT3} \w ${PFMAT4}${PFMAT5} "
bash --rcfile <(echo 'export PS1="${PROMPT}" && source ~/.bashrc && cd ${SRCDIR}')
