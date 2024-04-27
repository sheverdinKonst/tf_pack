#!/usr/bin/lua

print("*************** show poe config  ver. 0.2 ***************")
local uci = require("luci.model.uci").cursor()
dofile("/etc/tfortis_klish/scripts/lua/global_function.lua")

function show_poe_config()
    uci:foreach("poe", "port",
    function(s)
        -- local portStr, portNum = separateLettersAndNumbers(s[".name"])
        -- print(portStr .. " " .. portNum)
        -- print("\tid       " .. "  \t" .. s["id"])
        -- print("\tname     " .. "  \t" .. s["name"])
        -- print("\tpoe_plus " .. "  \t" .. s["poe_plus"])
        -- print("\tpriority " .. "  \t" .. s["priority"])
        -- print("\tenable   " .. "  \t" .. s["enable"])
        print("----------------------------------------")

        for key , value in pairs(s) do
            if (key == ".name") then
                local portStr, portNum = separateLettersAndNumbers(s[".name"])
                print(portStr .. " " .. portNum)
            elseif (key == ".anonymous")  then
                local a=1
            elseif (key == ".index")  then
                local a=1
            elseif (key == ".type")  then
                local a=1
            else
                print("\t" .. key .. "      \t" .. value)
            end
        end
    end)
end

show_poe_config()