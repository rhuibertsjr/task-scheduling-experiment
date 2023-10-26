#!/bin/bash

mkdir -p "bin/"

cd ./bin/

gcc -g -Walloc-size-larger-than=3221225472 ../src/main.c -o main -lm -lfftw3

cd ..
