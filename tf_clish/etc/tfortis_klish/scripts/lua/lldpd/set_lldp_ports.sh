#!/bin/bash

echo "--->> set_lldp_ports.sh ver 0.2 <<---"

port_list="${1}"
stateValue="${2}"

echo "lldp portlist:" "${port_list}"
echo "lldp stateValue:" "${stateValue}"

# parsing port list
# //////////////////////////////////////////////
lanstr="lan"
IFS='-' read -ra xy <<< "$port_list"
      x="${xy[0]}"
      y="${xy[1]}"

# //////////////////////////////////////////////

 if [[ -z "${y}" ]]; then
   y=$x
 fi

if [[ "${x}" -gt "${y}" ]]; then
  echo "Error first value great then second value"
  exit 1
else
  echo "x = " "${x}"
  echo "y = " "${y}"
  res=$(uci show lldpd.config.interface)
  echo "res: " "${res}"

  IFS='=' read -ra resArr <<< "$res"
  cmd="${resArr[0]}"
  iflist="${resArr[1]}"

  IFS=' ' read -ra ports <<< "$iflist"
  for port in "${ports[@]}"; do
    echo "port arr = "  "$port"
  done

  uci del lldpd.config.interface
  echo "uci changes:"
  uci changes
  echo "uci commit"
  uci commit
  echo "result: "
  uci show lldpd.config.interface

  case $2 in
  "enable")
    echo "STATE ENABLE"

    for (( i=x; i<=y; i++ )); do
      portvalue="${lanstr}""${i}"
      echo "set port value: " "${portvalue}"
      uci add_list lldpd.config.interface="${portvalue}"
    done

    for port in "${ports[@]}"; do
        port="${port%\'*}"
        port="${port#\'}"
        str="${port:0:3}"
        value="${port:3}"
        echo "str: ${str}"
        echo "value: ${value}"

        if [[ "${value}" -ge $x && "${value}" -le $y ]]; then
          echo "this port is presented"
        else
          uci add_list lldpd.config.interface="${port}"
        fi
    done
  ;;

  "disable")
    echo "STATE DISABLE"
      for port in "${ports[@]}"; do
        portTemp="${port}"
        portTemp="${portTemp%\'*}"
        portTemp="${portTemp#\'}"
        str="${portTemp:0:3}"
        value="${portTemp:3}"
        echo "value: ${value}"

        if [[ "${value}" -lt $x || "${value}" -gt $y ]]; then
          echo "this port must be added"
          echo "PORT -> " "${port}"
          uci add_list lldpd.config.interface="${str}""${value}"
        elif [[ "${value}" -ge $x && "${value}" -le $y ]]; then
           echo "this port will be deleted"
        fi
      done
  ;;
  esac

  echo "uci changes:"
  uci changes
  echo "uci commit"
  uci commit
  echo "result: "
  uci show lldpd.config.interface
  exit 0
fi






