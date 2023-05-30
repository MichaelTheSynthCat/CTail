#!/bin/sh

# Test suite for the ctail.
# Compares the output of ctail and system tail (should be used on GNU tail).

SHOW_OUTPUT=false

run_diff_test() {
    if [ -z "$2" ]; then
        lines=10
    else
        lines=$2
    fi
    tail -n "${lines}" "tests/$1" >tmp/unix.txt
    ./ctail -n "${lines}" "tests/$1" >tmp/mytail.txt
    ret=$(diff tmp/unix.txt tmp/mytail.txt)
    if $ret; then
        echo "PASS" "$1"
    else
        echo "FAIL" "$1"
    fi
    if ${SHOW_OUTPUT}; then
        cat tmp/mytail.txt
    fi
}

if ! [ -f "ctail" ]; then
    echo "Compile ctail first!"
    exit 1
fi

if [ $# -gt 0 ]; then
    if [ "$1" = "-o" ]; then
        SHOW_OUTPUT=true
    fi
fi

mkdir -p tmp

run_diff_test file1.txt 25
run_diff_test file2.txt 
run_diff_test file3.txt 4 
run_diff_test file3.txt
run_diff_test file4.txt 

if [ -d "tmp" ]; then
    rm -r tmp
fi