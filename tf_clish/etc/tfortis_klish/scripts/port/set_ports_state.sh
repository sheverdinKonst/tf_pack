#!/bin/bash

echo "--->> set_ports_state.sh <<---"

echo "port Start:        " "${1}"
echo "State value:       " "${2}"

strarr=($(echo "${1}"  | tr "-" "\n"))
# Use for loop to print the split string
x=${strarr[0]}
y=${strarr[1]}

if [[ -z  "${y}" ]]; then
  y="${x}"
fi  

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
str2=".state"
 # ============================ state ======================================== #

cmdPort+=$str1
echo "set port state"
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
