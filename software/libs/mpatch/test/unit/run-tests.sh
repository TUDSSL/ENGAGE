#!/bin/sh

testpath=$1
cd $testpath

for testname in ./test_*; do
    [ -f "$testname" ] || break
    echo -e "\e[7mRunning test: $testname\e[27m"
    ./$testname
    echo ""
done
echo -e "\e[7mRunning tests completed\e[27m"
