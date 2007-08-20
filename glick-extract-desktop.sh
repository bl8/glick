#!/bin/sh
if [ $# != 2 ]; then
  echo "Usage: glick-extract-icon.sh <glick file> <filename>"
  exit 1
fi
objcopy -j .xdg.desktop -O binary  --set-section-flags .xdg.desktop= $1 $2
