#!/bin/bash

echo "--->> set_description.sh <<---"

echo "parameter:""${1}"
echo "arg_1:""${2}"
echo "arg_2:""${3}"
echo "arg_3:""${4}"
echo "arg_4:""${5}"
echo "arg_5:""${6}"
echo "arg_6:""${7}"

str1="system.@system[0]."
cmd=""
cmd+=$str1
cmd+=$1

arg=""
if [[ -n $2 ]]; then
  arg+=$2
fi

if [[ -n $3 ]]; then
  arg+=" "
  arg+=$3
fi

if [[ -n $4 ]]; then
  arg+=" "
  arg+=$4
fi

if [[ -n $5 ]]; then
  arg+=" "
  arg+=$5
fi

if [[ -n $6 ]]; then
  arg+=" "
  arg+=$6
fi

if [[ -n $7 ]]; then
  arg+=" "
  arg+=$7
fi

echo "arg: " "${arg}"
echo "cmd: " "${cmd}"

uci set "${cmd}"="${arg}"