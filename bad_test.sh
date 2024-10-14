#!/bin/env bash

# this is a very simple scripts that builds xab in debug in release mode to test it, this is a really bad test
echo "testing debug mode"
make clean
make compile -j 4
echo "debug mode - done"

echo "testing release mode"
make clean
make compile RELEASE=1 -j 4
echo "release mode - done"

echo "going back to normal"
make clean
make compile RELEASE=0 -j 4
make compile_commands.json RELEASE=0
