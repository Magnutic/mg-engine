#!/bin/bash
cd "${BASH_SOURCE%/*}/" || exit 1

if [[ $# -eq 0 ]]; then
    echo "Usage:"
    echo "$0 configuration [target]"
    echo "  where configuration matches configuration name previously specified when running configure.sh"
    exit 1
fi

BUILD_DIR="out/$1"

TARGET_PART=""
if [[ $# -eq 2 ]]; then
    TARGET_PART="--target $2"
fi

if [[ ! -d $BUILD_DIR ]]; then
    echo "No such directory: $BUILD_DIR
Invalid configuration."
    exit 2
fi

(cd $BUILD_DIR && cmake --build . ${TARGET_PART} -j $(nproc)) || exit 1
