#!/bin/bash

echo "--->> set_port.sh <<---"

 echo "port Start:        " "${1}"
 echo "State value:       " "${2}"
 echo "Speed value:       " "${3}"
 echo "Flow control value:" "${4}"
 echo "PoE value:         " "${5}"

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
str1_2=".state"
str1_3="speed"
str1_4=".flow"
str1_5=".poe"
strPort+=$str1
for ((i=x; i <=y; i++))
  do
    echo "$i"
    cmdFull=""
    cmdPort+=$i

# ============================ state ======================================== #
    if [[  -n "${2}" ]]; then         #state
      echo "set port state"
      cmdFull+=$cmdPort
      cmdFull+=$str1_2
      echo "${cmdFull}"
      uci set "${cmdFull}"="${2}"
      cmdFull+="\n"
      cmdFull=""
    fi
# ============================ speedValue ========================================== #
    if [[  -n "${3}" ]]; then          #speedValue
          echo "set port speedValue"

         cmdFull+=$cmdPort
         cmdFull+=$str1_3
         echo "${cmdFull}"

         speedValue0="auto"
         speedValue1="1000 duplex full'"
         speedValue2="100 duplex full"
         speedValue3="100 duplex half"
         speedValue4="10 duplex full"
         speedValue5="10 duplex half"

          case $3 in
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
          cmdFull=""
    fi

# ============================ Flow Control ========================================== #
    if [[  -n "${4}" ]]; then
      echo "set port flow_controlValue"
      cmdFull+=$cmdPort
      cmdFull+=$str1_4
      echo "${cmdFull}"
      uci set "${cmdFull}"="${4}"
    fi

# ============================ PoE ========================================== #
    if [[  -n "${5}" ]]; then
      echo "set port poeValue"
      cmdFull+=$cmdPort
      cmdFull+=$str1_5
      echo "${cmdFull}"
      uci set "${cmdFull}"="${5}"
    fi
  done

uci changes

