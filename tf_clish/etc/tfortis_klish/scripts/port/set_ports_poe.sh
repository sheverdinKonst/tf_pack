#!/bin/bash

echo "--->> set_ports_poe.sh <<---"

echo "port Start:        " "${1}"
echo "PoE value:         " "${2}"

strarr=($(echo "${1}"  | tr "-" "\n"))
# Use for loop to print the split string
x=${strarr[0]}
y=${strarr[1]}

if [ "$x" -lt "$y" ]; then
  echo "$x less  $y"
elif [ "$x" -ge "$y" ]; then
 echo "$x great or equal $y"
 export RES=1
 exit 1
fi

echo "x=" "${x}"
echo "y=" "${y}"

str1="port.lan"
str2=".poe"

echo "set port poe Value"
for ((i=x; i<=y; i++))
  do
     echo "$i"
     cmdFull=""
     cmdFull+=$str1
     cmdFull+=$i
     cmdFull+=$str2
     echo "${cmdFull}"
     uci set "${cmdFull}"="${2}"
     cmdFull+="\n"
  done