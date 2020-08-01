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

# Invoke shell
bash --rcfile <(echo 'cd ${SRCDIR}')
