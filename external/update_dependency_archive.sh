#!/bin/bash
cd "${BASH_SOURCE%/*}/" || exit 1

DEPENDENCY_ARCHIVE="mg_dependencies.zip"

if [ -f $DEPENDENCY_ARCHIVE ]; then
    echo "Deleting old archive."
    rm $DEPENDENCY_ARCHIVE
fi

git add submodules
echo "Initialising submodules."
for d in submodules/*/
do
    git submodule update --init -- "$d"
done

echo "Archiving dependencies to ${DEPENDENCY_ARCHIVE}."
(cd submodules && cmake -E tar "cf" ../$DEPENDENCY_ARCHIVE --format=zip -- ./*)

echo "Created ${DEPENDENCY_ARCHIVE}."
