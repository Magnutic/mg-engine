#!/bin/bash
cd "${BASH_SOURCE%/*}/" || exit 1

./build.sh $1
[[ $? -eq 0 ]] || exit 1

BUILD_TARGET="test_scene"
[[ $# -eq 2 ]] && BUILD_TARGET=$2

./out/$1/bin/${BUILD_TARGET}
