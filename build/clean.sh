#!/bin/bash
cd "${BASH_SOURCE%/*}/" || exit 1

if [[ $# -eq 0 ]]; then
    echo "Usage:"
    echo "$0 configuration"
    echo "  where configuration matches configuration name previously specified when running configure.sh"
    exit 1
fi

BUILD_DIR="out/$1"

if [[ ! -d $BUILD_DIR ]]; then
    echo "No such directory: $BUILD_DIR"
    echo "Invalid configuration."
    exit 2
fi

(cd $BUILD_DIR && cmake --build . --target clean) || exit 1
