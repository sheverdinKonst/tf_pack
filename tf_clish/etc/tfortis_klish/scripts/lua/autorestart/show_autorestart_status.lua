#!/usr/bin/lua

print("*************** show_autorestart ver.-.0.4 ***************")
local uci = require("luci.model.uci").cursor()
dofile("/etc/tfortis_klish/scripts/lua/global_function.lua")

local text = executeCommand("ubus call autorestart getStatus")
local jsonText = text
local json_table = require("json").decode(jsonText)

for _, port in ipairs(json_table.port) do
   -- parsing and print
end


