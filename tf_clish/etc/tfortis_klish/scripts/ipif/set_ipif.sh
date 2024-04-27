#!/bin/bash

echo "--->> set_ipif.sh <<---"

echo "if_name:""${1}"
echo "proto:""${2}"
echo "ip_address:""${3}"
echo "netmask:""${4}"
echo "gateway:""${5}"

case $2 in

  "dhcp")
    echo "PROTO = DHCP"
    uci del network."${1}".ipaddr
    uci del network."${1}".netmask
    uci del network."${1}".ip6assign
    uci set network."${1}".proto='dhcp'
    ;;

  "static")
    echo "PROTO = STATIC"
    uci set network."${1}".proto='static'
    if [[  -n "${3}" ]]; then
      uci set network."${1}".ipaddr="${3}"
    fi

    if [[  -n "${4}" ]]; then
      uci set network."${1}".netmask="${4}"
    fi

    if [[  -n "${5}" ]]; then
      uci set network."${1}".gateway="${5}"
    fi

    if [[  -n "${6}" ]]; then
      uci set network."${1}".dns="${6}"
    fi
    ;;
esac

uci changes