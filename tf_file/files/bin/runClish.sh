#!/bin/bash

export CLISH_PATH=/etc/tfortis_klish/clish
if [[ "${CLISH_PATH}" != "/etc/tfortis_klish/clish" ]]; then
        echo "CLISH_PATH not set"
else
        clish
fi

