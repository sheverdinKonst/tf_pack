#!/bin/bash

echo "--->> checkPortInterval.sh <<---"
mystr=${1}
echo "${mystr}"
# shellcheck disable=SC2207
strarr=($(echo "$mystr"  | tr "-" "\n"))
# Use for loop to print the split string
x=${strarr[0]}
y=${strarr[1]}

echo "x=" "${x}"
echo "y=" "${y}"

if [ "$x" -lt "$y" ]; then
 echo "$x less  $y"
 export PORT_START=${x}
 export PORT_END=${y}
 export RES=0
elif [ "$x" -ge "$y" ]; then
 echo "$x great or equal $y"
 export RES=1
 exit 1
fi

