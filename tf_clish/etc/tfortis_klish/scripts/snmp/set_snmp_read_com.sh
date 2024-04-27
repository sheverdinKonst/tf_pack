#!/bin/bash

echo "--->> set_snmp_read_com.sh <<---"

echo "read community:        " "${1}"

uci set snmpd.read.community="${1}"
uci set snmpd.read_group.group="${1}"
uci set snmpd.public_access.group="${1}"


