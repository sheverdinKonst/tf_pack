#!/bin/bash

echo "--->> set_snmp_write_com.sh <<---"

echo "write community:        " "${1}"

uci set snmpd.write.community="${1}"
uci set snmpd.write_group.group="${1}"
uci set snmpd.private_access.group="${1}"