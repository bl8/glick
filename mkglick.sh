#!/bin/sh
cp $1 image
ld -r -b binary -o image.o image
rm image
objcopy --rename-section .data=.rodata,alloc,load,readonly,data,cont image.o
gcc header.a image.o `pkg-config --libs fuse ext2fs` -o $2

