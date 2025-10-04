#!/usr/bin/env bash
MG_ROOT="$(dirname ${BASH_SOURCE[0]})/.."
THIS_FILENAME="$(basename ${BASH_SOURCE[0]})"
find "$MG_ROOT" -type f \
    -and -not \( -path "$MG_ROOT/external/*" -or -path "$MG_ROOT/build/*" -or -name "catch.hpp" -or -name "$THIS_FILENAME" -or -path "$MG_ROOT/.git/*" \) \
    -exec egrep -HIin '\W(TODO|HACK|FIXME)' {} \; | sed -E 's/(:[0-9]+:)\s*(.*)/\1\n\t\2\n/'
