#!/usr/bin/env bash

SCRIPT_DIR=$(dirname $(realpath "${BASH_SOURCE:-0}"))
SCRIPT_NAME=$(basename "${BASH_SOURCE:-0}")
BUILD_ROOT="$SCRIPT_DIR/out"
SRC_ROOT="$SCRIPT_DIR/.."

echo "---- $SCRIPT_NAME ----"

if [[ $# -eq 0 ]]; then
    echo "Usage: $0 configuration-name c-compiler cxx-compiler [args]"
    echo "Arguments:"
    echo "    --debug Debug build (default is release)"
    echo "    --use-sanitisers Enable runtime debug sanitisers"
    echo "    --use-gfx-debug-groups Enable debug groups (for use with e.g. apitrace)"
    echo "    --clang-tidy-command <command> Run clang-tidy when building using the given command."
    echo "    --generator <generator> Which generator to use (corresponds to CMake option -G)."
    echo "Example: $0 clang-deb clang-8 clang++-8 --debug --use-sanitisers --use-gfx-debug-groups"
    echo "Note: this script can be invoked multiple times to set up different configurations, as long as the configuration-names differ."
    exit
fi

CONFNAME=$1
C_COMPILER=$2
CXX_COMPILER=$3
shift 3

BUILD_TYPE=Release
USE_SANITISERS=0
USE_GFX_DEBUG_GROUPS=0
CLANG_TIDY_COMMAND=""
GENERATOR=""

POSITIONAL=()
while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
        --debug)
        BUILD_TYPE=Debug
        shift
        ;;
        --use-sanitisers|--use-sanitizers)
        USE_SANITISERS=1
        shift
        ;;
        --use-gfx-debug-groups)
        USE_GFX_DEBUG_GROUPS=1
        shift
        ;;
        --generator)
        GENERATOR="$2"
        shift
        shift
        ;;
        --clang-tidy-command)
        CLANG_TIDY_COMMAND="$2"
        shift
        shift
        ;;
        *)
        echo "Unknown option: $1" >&2
        exit 1
        ;;
    esac
done

if [ ! -d $BUILD_ROOT ]; then
    mkdir $BUILD_ROOT || exit 1
fi

echo "Configuring '$CONFNAME' with C compiler '$C_COMPILER' and C++ compiler '$CXX_COMPILER'"
echo "Build type: $BUILD_TYPE."
echo "Sanitisers enabled: $USE_SANITISERS"
echo "Use gfx debug groups: $USE_GFX_DEBUG_GROUPS"
if [[ $CLANG_TIDY_COMMAND != "" ]]; then
    echo "Clang-tidy enabled with command: $CLANG_TIDY_COMMAND"
fi

GENERATOR_PART=""
if [[ $GENERATOR != "" ]]; then
    echo "Using generator: $GENERATOR"
    GENERATOR_PART="-G $GENERATOR"
fi

rm -r "$BUILD_ROOT/$CONFNAME"
mkdir "$BUILD_ROOT/$CONFNAME" || exit 1

(cd "$BUILD_ROOT/$CONFNAME" &&
cmake "$SRC_ROOT" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER="$C_COMPILER" -DCMAKE_CXX_COMPILER="$CXX_COMPILER" -DMG_DEBUG_SANITISERS=$USE_SANITISERS \
    -DMG_ENABLE_GFX_DEBUG_GROUPS=$USE_GFX_DEBUG_GROUPS \
    -DCMAKE_CXX_CLANG_TIDY="$CLANG_TIDY_COMMAND" \
    $GENERATOR_PART)
