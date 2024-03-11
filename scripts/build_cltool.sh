#!/bin/bash

args="${@:1}" # All arguments

pushd "$(dirname "$(realpath $0)")" > /dev/null

source ./build_test_cmake.sh

###############################################################################
#  Builds and Tests
###############################################################################

build_cmake "cltool" ../cltool

popd > /dev/null

# Return results: 0 = pass, 0 != fail
exit $((BUILD_EXIT_CODE+TESTS_EXIT_CODE))