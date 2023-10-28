#!/bin/bash

mkdir -p "bin/"

cd ./bin/

gcc -g -Walloc-size-larger-than=3221225472 ../src/main.c -o main -lm -lpthread -lfftw3

cd ..
