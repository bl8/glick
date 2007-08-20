#!/bin/sh
DIR=`mktemp -d`
cp $1 $DIR/image
(cd $DIR; ld -r -b binary -o image.o image)
objcopy --rename-section .data=.rodata,alloc,load,readonly,data,cont $DIR/image.o
# Dynamic ext2fs
EXT2FS_LIB=`pkg-config --libs ext2fs`
# Static ext2fs
EXT2FS_LIB="/usr/lib/libext2fs.a /usr/lib/libcom_err.a" 
gcc header.a $DIR/image.o $EXT2FS_LIB `pkg-config --libs fuse` -o $2
rm -rf $DIR
strip $2
