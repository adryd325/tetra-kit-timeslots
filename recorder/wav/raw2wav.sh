#!/bin/bash
#
# 2021-05-05  LT  0.0  first release
#

cp -f ../raw/*.raw .

for FILE in *.raw; do
    BASE=${FILE%.raw};
    echo "* $FILE -> $BASE.wav";
    sox -r 8k -e signed -b 16 "$BASE.raw" "$BASE.wav"
done

rm -f *.raw 
