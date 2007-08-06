#!/bin/sh
cp $1 image
ld -r -b binary -o image.o image
rm image
objcopy --rename-section .data=.rodata,alloc,load,readonly,data,cont image.o
# Dynamic ext2fs
EXT2FS_LIB=`pkg-config --libs ext2fs`
# Static ext2fs
EXT2FS_LIB="/usr/lib/libext2fs.a /usr/lib/libcom_err.a" 
gcc header.a image.o $EXT2FS_LIB `pkg-config --libs fuse` -o $2

