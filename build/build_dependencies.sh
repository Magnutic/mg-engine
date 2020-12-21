#!/bin/bash

if [[ $* == *--generate-archive* ]]; then
    read -p "Generating mg_dependencies.zip from submodules.
Press enter to continue (or cancel with Ctrl-C)"
    GENERATE_ARCHIVE=1
else
    read -p "Using bundled (or last generated) mg_dependencies.zip. To generate a new \
mg_dependencies.zip from submodules, cancel with Ctrl-C and invoke this script with the flag \
--generate-archive.
Press enter to continue."
    GENERATE_ARCHIVE=0
fi

if [[ -d out ]]; then
    rm -rf out/deps-install
    rm -rf out/deps-build
else
    mkdir out
fi

mkdir out/deps-build
(cd out/deps-build && cmake ../../.. -DCMAKE_BUILD_TYPE=Release -DMG_GENERATE_DEPENDENCY_ARCHIVE=$GENERATE_ARCHIVE \
    -DMG_BUILD_DEPENDENCIES=1 -DMG_BUILD_DEPENDENCIES_INSTALL_DIR=../deps-install \
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++)
