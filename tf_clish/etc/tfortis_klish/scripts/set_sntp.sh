#!/bin/bash

echo "--->> set_sntp_timezone.sh <<---"

echo "timezone :        " "${1}"
TimeZonevalue="${1}"

cmdZoneName="system.@system[0].zonename=Etc/GMT"
cmdTimeZone="system.@system[0].timezone=<"

if [[ "${TimeZonevalue}" -lt 0 ]]; then
  echo "Time zone minus"
  number=$(( -1 * TimeZonevalue ))
  setZoneName="${cmdZoneName}""${TimeZonevalue}"
  if [[ "${number}" -lt 10 ]]; then
    setTimeZone="${cmdTimeZone}""+0""${number}"">""${TimeZonevalue}"
  else
    setTimeZone="${cmdTimeZone}""+""${number}"">""${TimeZonevalue}"
  fi
  echo "setZoneName: " "${setZoneName}"
  echo "setTimeZone: " "${setTimeZone}"
else
  echo "Time zone plus"
   setZoneName="${cmdZoneName}""+""${TimeZonevalue}"
   if [[ "${number}" -lt 10 ]]; then
       setTimeZone="${cmdTimeZone}""-0""${TimeZonevalue}"">""${TimeZonevalue}"
     else
       setTimeZone="${cmdTimeZone}""-""${TimeZonevalue}"">""${TimeZonevalue}"
     fi
fi

uci set "${setZoneName}"
uci set "${setTimeZone}"
echo "uci changes: "
uci changes
uci commit
reload_config

#uci set system.cfg01e48a.zonename='Etc/GMT-5'
#uci set system.cfg01e48a.timezone='<+05>-5'

 #uci set system.cfg01e48a.zonename='Etc/GMT-9'
 #uci set system.cfg01e48a.timezone='<+09>-9'
 #uci show system.@system[0].zonename

#uci set system.cfg01e48a.zonename='Etc/GMT+4'
#uci set system.cfg01e48a.timezone='<-04>4'

#uci set system.cfg01e48a.zonename='Etc/GMT-5'
#uci set system.cfg01e48a.timezone='<+05>-5'