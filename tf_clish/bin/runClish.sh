#!/bin/ash

export CLISH_PATH=/etc/tfortis_klish/
if [[ "${CLISH_PATH}" != "/etc/tfortis_klish/" ]]; then
        echo "CLISH_PATH not set"
else
        clish
fi


