#!/bin/bash
cd "${BASH_SOURCE%/*}/" || exit 1

./build.sh $*
[[ $? -eq 0 ]] || exit 1

BUILD_DIR="out/$1"
(cd $BUILD_DIR && CTEST_OUTPUT_ON_FAILURE=1 ctest) || exit 1
