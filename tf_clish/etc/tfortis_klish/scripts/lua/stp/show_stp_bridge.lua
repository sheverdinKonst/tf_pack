#!/usr/bin/lua

dofile("/etc/tfortis_klish/scripts/lua/global_function.lua")

local bridgeList = executeCommand("mstpctl showbridgelist")

print("bridgeList = " .. bridgeList)
print("size = " .. #bridgeList .. " type = " .. type(bridgeList))

-- uci set network.switch.stp='1'


-- Removing package luci-i18n-tn-mstpd-ru from root...
-- Removing package luci-app-tn-mstpd from root...
-- Removing package mstpd from root...



