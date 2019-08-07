#!/bin/sh

# THIS FILE IS TEMPORARY

#regular linux build
gcc -O3 -o brainheck_jit brainheck_jit.c -llightning

#debug linux build
#gcc -Wall -g -o brainheck_jit brainheck_jit.c -llightning

#windows build (done statically for now with sdl)
#x86_64-w64-mingw32-gcc -Wall -O3 -o brainheck_jit.exe brainheck_jit.c -llightning
