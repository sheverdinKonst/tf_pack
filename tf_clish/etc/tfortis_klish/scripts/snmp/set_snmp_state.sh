#!/bin/bash

echo "--->> set_snmp_state.sh <<---"

echo "state:        " "${1}"

uci set snmpd.general.enabled="${1}"

