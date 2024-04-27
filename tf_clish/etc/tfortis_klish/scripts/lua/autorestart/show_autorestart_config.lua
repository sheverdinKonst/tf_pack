#!/usr/bin/lua

print("*************** show_autorestart ver.-.0.2 ***************")
local uci = require("luci.model.uci").cursor()
bit = require "bit"
dofile("/etc/tfortis_klish/scripts/lua/global_function.lua")

local text = ""

    local i_str = tostring(i)
    local cmd_state     = "uci get  tf_autorestart.lan" .. i_str .. ".state"
    local cmd_host      = "uci get  tf_autorestart.lan" .. i_str .. ".host"
    local cmd_min_speed = "uci get  tf_autorestart.lan" .. i_str .. ".min_speed"
    local cmd_max_speed = "uci get  tf_autorestart.lan" .. i_str .. ".max_speed"
    local cmd_timeUp    = "uci get  tf_autorestart.lan" .. i_str .. ".timeUp"
    local cmd_timeDown  = "uci get  tf_autorestart.lan" .. i_str .. ".timeDown"

    print("---------------------------------------")
    print("port " .. i_str)
    text = executeCommand(cmd_state)
    local res, _ = text:gsub("\n", "")
    print("State: " .. res)
    text = executeCommand(cmd_host)
    res, _ = text:gsub("\n", "")
    print("Host: " ..  res)
    text = executeCommand(cmd_min_speed)
    res, _ = text:gsub("\n", "")
    print("Min Speed: " ..  res)
    text = executeCommand(cmd_max_speed)
    res, _ = text:gsub("\n", "")
    print("max Speed: " .. res)
    text = executeCommand(cmd_timeUp)
    res, _ = text:gsub("\n", "")
    print("Time Up: " ..  res)
    text = executeCommand(cmd_timeDown)
    res, _ = text:gsub("\n", "")
    print("Time Down: " .. res)
end
