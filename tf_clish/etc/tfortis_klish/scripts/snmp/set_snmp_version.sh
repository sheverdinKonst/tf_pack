#!/bin/bash

echo "--->> set_snmp_version.sh <<---"

echo "version:        " "${1}"

uci set snmpd.read_group.version="${1}"
uci set snmpd.write_group.version="${1}"
uci set snmpd.public_access.version="${1}"
uci set snmpd.private_access.version="${1}"
