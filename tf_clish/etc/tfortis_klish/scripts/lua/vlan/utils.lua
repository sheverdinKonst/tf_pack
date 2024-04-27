#!/usr/bin/lua
local uci = require("luci.model.uci").cursor()

function getUciInterfaceLan()
    local manageVlan = {}
    local device = uci:get("network", "lan", "device")
    local result = {}
    for value in device:gmatch("[^.]+") do
        table.insert(result, value)
    end
    manageVlan.device = result[1]
    manageVlan.id     = result[2]
    return manageVlan
end

function getVlanPortsInfo(vlan_local)
    if vlan_local ~= nil then
        if vlan_local.ports ~= nil then
            for n, portStr in pairs(vlan_local.ports) do
                local port = {}
                port.state = ""
                port.portNumber = 0
                port.id = 0
                port = getPort(portStr)
                port.id = vlan_local.num
                if port.state == "tagged"  then
                    print("\t" ..  port.portNumber .. " \tTagged")
                elseif port.state == "untagged"  then
                    print("\t" ..  port.portNumber .. "\tUntagged")
                end
            end
        else
            print("For Vlan " .. vlan_local.num .. " Ports not configured")
        end
    end
end

function printBrVlanInfo(brVlanList, id)
    local vlan = {}
    local key = ""
    local mainVlan = getUciInterfaceLan()
    if (tonumber(id) ~= 0 ) then
        vlan = brVlanList[id]

        if (vlan ~= nil) then
            if (id == mainVlan.id) then
                print("vlan id:\t" ..  id .. " - Management Vlan")
            else
                print("vlan id:\t" .. id)
            end
            if (vlan.state ~= nil) then
                print("state:\t\t" ..  vlan.state)
            else
                print("state:\t\t" .. "-")
            end

            if (vlan.descr ~= nil) then
                print("description:\t" .. vlan.descr)
            else
                print("description:\t" .. "-")
            end
            getVlanPortsInfo(vlan)
        else
            print("Vlan id " .. id .. " does not exist, chose *show all* vlan to inspect existing Vlan")
        end

    elseif (tonumber(id) == 0) then
        for i, vlan_i in pairs(brVlanList) do
            key = tostring(i)
            print("----------------------------------------")
            if (i == mainVlan.id) then
                print("vlan id:\t" ..  key .. " - Management Vlan")
            else
                print("vlan id:\t" .. key)
            end
            if (vlan_i.state ~= nil) then
                print("state:\t\t" ..  vlan_i.state)
            else
                print("state:\t\t" .. "-")
            end

            if (vlan_i.descr ~= nil) then
                print("description:\t" .. vlan_i.descr)
            else
                print("description:\t" .. "-")
            end
            print("Ports:")
            vlan = brVlanList[i]
            getVlanPortsInfo(vlan)
        end
    end
end








