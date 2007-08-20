#!/bin/sh
echo "arg is $1"
echo "file data:"
cat /proc/self/fd/1023/data.txt
