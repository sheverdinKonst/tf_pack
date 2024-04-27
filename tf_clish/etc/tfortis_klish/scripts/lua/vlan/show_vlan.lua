#!/usr/bin/lua

--print("*************** show_vlan ver.-0.0 ***************")
local uci = require("luci.model.uci").cursor()

vlanId     = arg[1]
dofile("/etc/tfortis_klish/scripts/lua/vlan/utils.lua")
dofile("/etc/tfortis_klish/scripts/lua/global_function.lua")

local bridgeVlanList = getUciBridgeVlanList()
if (bridgeVlanList == nill) then
    print("ERROR to read config bridge-vlan")
end

printBrVlanInfo(bridgeVlanList, vlanId)

