#!/bin/bash

if [[ $# -eq 0 ]]; then
    echo "Usage:"
    echo "$0 configuration"
    echo "  where configuration matches configuration name previously specified when running configure.sh"
    exit
fi

BUILD_DIR="out/$1"

if [[ ! -d $BUILD_DIR ]]; then
    echo "No such directory: $BUILD_DIR
Invalid configuration."
    exit 1
fi

(cd $BUILD_DIR && cmake --build . -j $(nproc)) || exit 1
$BUILD_DIR/bin/test_scene
