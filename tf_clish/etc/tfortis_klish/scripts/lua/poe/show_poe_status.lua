#!/usr/bin/lua

print("*************** show poe config  ver.-0.2 ***************")
local uci = require("luci.model.uci").cursor()
local tf_poe = require "poe_module"

function show_poe_status()
    local poeInfo = tf_poe.getPoeStatusInfo()

    print("budget:     \t" .. poeInfo.budget .. " W")
    print("consumption:\t" .. poeInfo.consumption .. " W")
    uci:foreach("poe", "port",
       function(s)
       local resultLetters, resultNumbers = tf.separateLettersAndNumbers(s[".name"])
       local portNum = ""
       print("----------------------------------------")
       print(resultLetters .. " " .. resultNumbers)
       portNum = s["name"]
       print("\tname   " .. "  \t" .. portNum)
       print("\tid     " .. "  \t" .. s["id"])
       print("\tenable " .. "  \t" .. s["enable"])
       for key, value in pairs(poeInfo.ports[portNum]) do
           print("\t" .. key .. "  \t" .. value)
       end
    end
    )
end

show_poe_status()


