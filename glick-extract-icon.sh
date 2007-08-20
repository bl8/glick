#!/bin/sh
if [ $# != 2 ]; then
  echo "Usage: glick-extract-icon.sh <glick file> <filename>"
  exit 1
fi
objcopy -j .xdg.icon.48 -O binary  --set-section-flags .xdg.icon.48= $1 $2
