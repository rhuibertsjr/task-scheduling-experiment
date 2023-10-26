#!/bin/bash

mkdir -p "bin/"

cd ./bin/

gcc -g -Walloc-size-larger-than=2147483648 ../src/main.c -o main -lm -lfftw3

cd ..
