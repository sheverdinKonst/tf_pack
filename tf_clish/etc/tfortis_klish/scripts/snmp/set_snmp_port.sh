#!/bin/bash

echo "--->> set_snmp_port.sh <<---"

echo "port:        " "${1}"

cmd=""
str1="snmpd.@agent[0].agentaddres='UDP:"
str2=",UDP6:161"
cmd+=$str1
cmd+=$1
cmd+=str2
echo "command = " "${cmd}"
uci set "${cmd}"

