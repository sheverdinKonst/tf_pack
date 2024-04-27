#!/bin/bash

echo "--->> set_ports_speed.sh <<---"

echo "port Start:        " "${1}"
echo "Speed value:       " "${2}"

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

cmdPort=""
str1="port.lan"
str2=".speed"


echo "set port speed Value"

speedValue0="auto"
speedValue1="1000 duplex full'"
speedValue2="100 duplex full"
speedValue3="100 duplex half"
speedValue4="10 duplex full"
speedValue5="10 duplex half"

for ((i=x; i<=y; i++))
  do
   echo "$i"
   cmdFull=""
   cmdFull+=$str1
   cmdFull+=$i
   cmdFull+=$str2
   echo "${cmdFull}"

    case $2 in
      "0")
        echo "speed = auto"
        uci set "${cmdFull}"="${speedValue0}"
      ;;

      "1")
        echo "speed = 1000_full"
        uci set "${cmdFull}"="${speedValue1}"
      ;;

      "2")
        echo "speed = 100 duplex full"
        uci set "${cmdFull}"="${speedValue2}"
      ;;

      "3")
        echo "speed = 100 duplex half"
        uci set "${cmdFull}"="${speedValue3}"
      ;;

      "4")
        echo "speed = 10 duplex full"
        uci set "${cmdFull}"="${speedValue4}"
      ;;

      "5")
        echo "speed = 10 duplex half"
        uci set "${cmdFull}"="${speedValue5}"
      ;;
    esac
    cmdFull+="\n"
  done
