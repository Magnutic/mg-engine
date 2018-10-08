#!/bin/bash

TESTNAME=$1

if [[ ! $TESTNAME ]]; then
	echo "Usage: add-test <testname>"
	exit 1
fi

FILENAME=$TESTNAME."cpp"

if [[ -f $FILENAME ]]; then
	echo "$FILENAME already exists." >&2
	exit 1
fi

touch $FILENAME
echo \
"#include <UnitTest++.h>

SUITE($TESTNAME) {

TEST(stub) {
	CHECK(false);
}

} // SUITE($TESTNAME)

int main(int, const char* []) {
	return UnitTest::RunAllTests();
}
" >> $FILENAME

echo "add_mg_test($TESTNAME)" >> CMakeLists.txt

echo "Created test $FILENAME"

