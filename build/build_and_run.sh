#!/bin/bash
cd "${BASH_SOURCE%/*}/" || exit 1

BUILD_CONFIG=$1
BUILD_TARGET="test_scene"
[[ $# -eq 2 ]] && BUILD_TARGET=$2

./build.sh $BUILD_CONFIG $BUILD_TARGET
[[ $? -eq 0 ]] || exit 1

./out/$1/bin/${BUILD_TARGET}
