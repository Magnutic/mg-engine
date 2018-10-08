#!/bin/bash
find $1 -name "mg_*" | xargs clang-format -i && git commit -am "Reformat $1"
