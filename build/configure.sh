#!/bin/bash

SCRIPT_DIR=$(dirname $(realpath "${BASH_SOURCE:-0}"))
BUILD_ROOT=$SCRIPT_DIR/out
SRC_ROOT=$SCRIPT_DIR/..

if [ ! -d $BUILD_ROOT/deps-install ]; then
    echo "Dependencies not installed to $BUILD_ROOT/deps-install. Run build-dependencies.sh first."
    exit
fi

if [[ $# -eq 0 ]]; then
    echo "Usage: $0 configuration-name c-compiler cxx-compiler [flags]"
    echo "Flags: --debug: debug build (default is release), --use-sanitisers: enable runtime debug sanitisers, --use-gfx-debug-groups: enable debug groups (for use with e.g. apitrace)"
    echo "Example: $0 clang-deb clang-8 clang++-8 --debug --use-sanitisers --use-gfx-debug-groups"
    echo "Note: this script can be invoked multiple times to set up different configurations, as long as the configuration-names differ."
    exit
fi

if [[ $* == *--use-sanitisers* ]]; then
    USE_SANITISERS=1
else
    USE_SANITISERS=0
fi

if [[ $* == *--use-gfx-debug-groups* ]]; then
    USE_GFX_DEBUG_GROUPS=1
else
    USE_GFX_DEBUG_GROUPS=0
fi

if [[ $* == *--debug* ]]; then
    BUILD_TYPE=Debug
else
    BUILD_TYPE=Release
fi

CONFNAME=$1
C_COMPILER=$2
CXX_COMPILER=$3

echo "Configuring '$CONFNAME' with C compiler '$C_COMPILER' and C++ compiler '$CXX_COMPILER'. Build type: $BUILD_TYPE. Sanitisers enabled: $USE_SANITISERS. Use gfx debug groups: $USE_GFX_DEBUG_GROUPS"

rm -rf $BUILD_ROOT/$CONFNAME
mkdir $BUILD_ROOT/$CONFNAME
(cd $BUILD_ROOT/$CONFNAME &&
cmake $SRC_ROOT -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_PREFIX_PATH=$BUILD_ROOT/deps-install \
    -DCMAKE_C_COMPILER=$C_COMPILER -DCMAKE_CXX_COMPILER=$CXX_COMPILER -DMG_DEBUG_SANITISERS=$USE_SANITISERS \
    -DMG_ENABLE_GFX_DEBUG_GROUPS=$USE_GFX_DEBUG_GROUPS)
