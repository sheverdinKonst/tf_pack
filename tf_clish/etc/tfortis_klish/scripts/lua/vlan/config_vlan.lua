#!/usr/bin/lua

print("*************** config vlan  ver.-1.0 ***************")
local uci = require("luci.model.uci").cursor()
bit = require "bit"

dofile("/etc/tfortis_klish/scripts/lua/vlan/utils.lua")

vlanRange_in     = arg[1]
cmd_in           = arg[2]
taggedType_in    = arg[3]
portList_in      = arg[4]
device_in        = "switch"

local debug_print = 1

local bridgeVlanList = {}
local portRange      = {}
local vlanRange      = {}
local busyPortList   = {}
local busyUntaggedPortList = {}
local mainVlan = {}

function printParam()
    print("vlanId = " .. vlanRange_in .. " cmd = " .. cmd_in .. " taggedType = " .. taggedType_in .. " portList = " .. portList_in)
end

function saveUntaggedPorts()
    for _, vlan  in pairs(bridgeVlanList) do
        local busyList = {}
        local key_in = vlanRange[1]
        key_in = tostring(key_in)

        if vlan.ports ~= nil then
            for n, portStr in pairs(vlan.ports) do
                local port = {}
                port.state = ""
                port.portNumber = 0
                port.id = 0
                port = getPort(portStr)
                port.id = vlan.num

                if port.state == "tagged" and vlan.num~=key_in then
                    print("Port *" ..  port.portNumber .. "* tagged in Vlan *" .. vlan.num .. "*")
                else
                    table.insert(busyList, port)
                end

                --print(" port.id = ",    port.id)
                --print(" port.state = ", port.state)
                --print(" port.num = ",   port.portNumber)
            end
            busyUntaggedPortList[vlan.num] =busyList
        else
            print("For Vlan " .. vlan.num .. " Ports not configured")
        end
    end
end

-- //////////////////////////////////////////////////////////
function vlan_add_notMemberPorts()
    print("---------------  Remove Port from Vlan ------------------- ")
    local portList = {}

    local portRangeMin = 0
    local portRangeMax = 0

    local key_in = vlanRange[1]
    key_in = tostring(key_in)
    local thisVlan = bridgeVlanList[key_in]
    local vlanId = tonumber(key_in)
    local mainVlanId = tonumber(mainVlan.id)
    -- if vlanId == mainVlanId then
        --print("Vlan *" .. vlanId .. "*  \"Management Vlan\" ports can not be deleted")
    --elseif vlanId ~= mainVlanId then
    --if vlanId ~= mainVlanId then
        if thisVlan == nil then
            print("This VlanId " .. key_in .. " not corrected. Choose another VlanId and try again")
        else
            if portRange == nil then
                print("Error, the port range is specified incorrectly")
            else
                portRangeMin = tonumber(portRange[1])
                portRangeMax = tonumber(portRange[2])

                local portsForAdd = {}
                if thisVlan.ports ~= nil then
                    for _, port in pairs(thisVlan.ports) do
                        local p = getPort(port)

                        portNumber = tonumber(p.portNumber)
                        if portNumber < portRangeMin or portNumber > portRangeMax then
                            local lanStr = ""
                            if p.state == "untagged" then
                                lanStr = "lan" .. p.portNumber
                            elseif p.state == "tagged" then
                                lanStr = "lan" .. tostring(p.portNumber) .. ":t"
                            end
                            table.insert(portsForAdd, port)
                        else
                            print("Port " .. port .. " was deleted")
                        end
                    end
                end

                if #portsForAdd ~= 0 then
                    print("portsForAdd length = ", #portsForAdd)
                    res = uci:set_list("network", thisVlan.id, "ports", portsForAdd)
                    if res == false then
                        print("CMD -> uci:set_list(network, thisVlan.id, ports) " .. thisVlan.id .. " is failed")
                    elseif res == true then
                        print("CMD -> uci:set_list(network, thisVlan.id, ports) " .. thisVlan.id .. " Done " )
                    end
                elseif #portsForAdd == 0 then
                    vlan_delete()
                end
            end
            -- uci:commit("network")
        end
    --end
end

function vlan_add_untagged()
    local bridge_vlanID = 0
    local lanList = {}
    local flag = tonumber(vlanRange[3])
    if flag == 0 then
        print("Error, the Vlan range not possible for set untagget port.\nYou need set only specified Vlan Id")
    else
        if portRange == nil then
            print("Error, the port range is specified incorrectly")
        else
            local portRangeMin = tonumber(portRange[1])
            local portRangeMax = tonumber(portRange[2])
            local lanPorts = {}

            local key_in = vlanRange[1]
            key_in = tostring(key_in)
            saveUntaggedPorts()
            local vlan = bridgeVlanList[key_in]

            if vlan == nil then
                print("Vlan with id = " .. key_in .. " not in Use. Was added new Bridge Vlan device")
                bridge_vlanID = uci:add("network", "bridge-vlan")
                uci:set("network", bridge_vlanID, "device", "switch")
                uci:set("network", bridge_vlanID, "vlan", key_in)
            elseif vlan ~= nil then
                -- printVlanByKey(key_in)
                print("-------- Vlan with id = " .. key_in .. " in Use ------ Add only new ports")
                bridge_vlanID = vlan.id
            end

            for vlanId, l in pairs(busyUntaggedPortList) do
                print("vlanId = ", vlanId)
                for i, port in pairs(l) do
                    -- print("i = ", i)
                    -- print("port.portNumber = ", port.portNumber)
                    -- print("port.state      = ", port.state)
                    -- print("port.id         = ", port.id)
                    if vlanId == key_in then
                        local portNum = tonumber(port.portNumber)
                        local lanStr = ""
                        -- print("portNum", portNum)
                        -- print("portRangeMin", portRangeMin)
                        -- print("portRangeMax", portRangeMax)
                        if portNum < portRangeMin or portNum > portRangeMax then
                            -- print(" port in busyList and out of range")

                            if port.state == "tagged" then
                                -- print("port is tagget")
                                lanStr = "lan" .. tostring(portNum) .. ":t"
                            elseif port.state == "untagged" then
                                -- print("port untagged")
                                lanStr = "lan" .. tostring(portNum)
                            end

                            --print("len lanStr =", #lanStr)
                            if lanStr == "" then
                                -- print("len is Empty str")
                            else
                                table.insert(lanList, lanStr)
                            end
                        end
                    end
                end
            end

            --if vlan.ports ~= nil then
            local portFlag  = 0
            local portState = ""
            for p = portRangeMin, portRangeMax do
                portFlag = 0
                portState = ""
                local lanStr = "lan" .. tostring(p)
                for vlanId, l in pairs(busyUntaggedPortList) do
                    for _, port in pairs(l) do
                        local portNum = tonumber(port.portNumber)
                        if portNum == p then
                            if vlanId == key_in then
                                if port.state == "tagged" then
                                    if cmd_in == "add" then
                                        print("Port *" .. p .. "* is Busy in this Vlan *" .. vlanId .. "* type tagged")
                                        print("For edit this port chose command EDIT")
                                        lanStr = "lan" .. tostring(p) .. ":t"
                                        -- portFlag  = 0
                                        portState = "tagged"
                                    elseif cmd_in == "edit" then
                                        print("Port *" .. p .. "* was changed from *tagged* to *untagged*")
                                        --portFlag  = 0
                                    end
                                elseif port.state == "untagged" then
                                    -- portFlag = 0
                                end
                            end

                            if vlanId ~= key_in and port.state == "untagged" then
                                print("Port *" .. p .. "* can not be added. This port is Busy in Vlan *" .. vlanId .. "*")
                                portFlag = 1
                            end
                        end
                    end
                end

                if portFlag == 0 then
                    table.insert(lanList, lanStr)
                end
            end

            for i, p1 in pairs(lanList) do
                print("Added port = ", p1)
            end

            --else
            --print("PORT not configured")
            -- end

            print("length lanList = ", #lanList)
            print("bridge_vlanID = ", bridge_vlanID)
            if #lanList > 0 then
                local res = uci:set_list("network", bridge_vlanID, "ports", lanList)
                if res == false then
                    print("CMD -> uci:set_list(network, thisVlan.id, ports) " .. bridge_vlanID .. " is failed")
                elseif res == true then
                    print("CMD -> uci:set_list(network, thisVlan.id, ports) " .. bridge_vlanID .. " Done " )
                end
            else
                print("------------- NO PORT to add in this Vlan -------------")
            end

            -- res = uci:changes("network")
            -- print("uci:changes:", "network" )
            -- for _, t in pairs(res) do
            --     for _, l in pairs(t) do
            --         print("l = ", l)
            --     end
            -- end
            -- uci:commit("network")
        end
    end
end

function vlan_add_tagged()
    print("************ add_tagged *****************")
    local bridge_vlanID = 0
    local lanList = {}
    if vlanRange == nil then
        print("Error, the Vlan range is specified incorrectly")
    else
        if portRange == nil then
            print("Error, the port range is specified incorrectly")
        else
            local portRangeMin = tonumber(portRange[1])
            local portRangeMax = tonumber(portRange[2])

            for key_in = vlanRange[1], vlanRange[2] do
                local lanPorts = {}
                key_in = tostring(key_in)
                local vlan = bridgeVlanList[key_in]

                if vlan == nil then
                    print("Vlan with id = " .. key_in .. " not in Use. Was added new Bridge Vlan device")
                    bridge_vlanID = uci:add("network", "bridge-vlan")
                    uci:set("network", bridge_vlanID, "device", "switch")
                    uci:set("network", bridge_vlanID, "vlan", key_in)
                else
                    bridge_vlanID = vlan.id
                end

                print("-------- Vlan with id = " .. key_in .. " in Use ------ Add only new ports")
                if vlan.ports ~= nil then
                    busyPortList[vlan.num] = vlan.ports
                end

                local portFlag  = 0
                local portState = ""
                for p = portRangeMin, portRangeMax do
                    portFlag = 0
                    portState = ""
                    --print("p = ", p)
                    local lanStr = "lan" .. tostring(p) .. ":t"
                    for _, port in pairs(busyPortList[key_in]) do
                        local pStruct = getPort(port)
                        local portNumber = pStruct.portNumber
                        --print("portNumber = ", portNumber)
                        if portNumber == p then
                            if vlan.num == key_in then
                                if pStruct.state == "tagged" then
                                    if cmd_in == "add" then
                                        portState = "tagged"
                                    end
                                elseif pStruct.state == "untagged" then
                                    if cmd_in == "add" then
                                        print("Port *" .. p .. "* is Busy in this Vlan *" .. vlan.num .. "* type - untagged")
                                        print("For edit this port chose command: EDIT")
                                        lanStr = "lan" .. tostring(p)
                                    elseif cmd_in == "edit" then
                                        print("Port *" .. p .. "* was changed from *untagged* to *tagged*")
                                    end
                                end
                            end

                            if vlan.num ~= key_in and pStruct.state == "untagged" then
                                print("Port *" .. p .. "* can not be added. This port is Busy in Vlan *" .. vlan.num .. "*")
                                portFlag = 1
                            end
                        end
                    end

                    if portFlag == 0 then
                        --print("lanStr = ", lanStr)
                        table.insert(lanList, lanStr)
                    else
                        print("Can not add port" .. lanStr .. "This port is busy")
                    end
                end

                for _, port in pairs(busyPortList[key_in]) do
                    -- print("port " ..  port .. " from BusyList")
                    local p = getPort(port)
                    local portNumber = p.portNumber
                    portNumber = tonumber(portNumber)
                    -- print("portNumber", portNumber)
                    -- print("portRangeMin", portRangeMin)
                    -- print("portRangeMax", portRangeMax)

                    if portNumber < portRangeMin or  portNumber > portRangeMax then
                        local lanStr = port
                        table.insert(lanList, lanStr)
                    end
                end

                for _, str in pairs(lanList) do
                    print("port to be added is: ", str)
                end

                local res = uci:set_list("network", bridge_vlanID, "ports", lanList)
                print("RES uci set list: ", res)

            end
        end
    end
end

function vlan_delete()
    if vlanRange == nil then
        print("Error, the Vlan range is specified incorrectly")
    else
        for key_in = vlanRange[1], vlanRange[2] do
            key_in = tostring(key_in)
            local vlanId = tonumber(key_in)
            local mainVlanId = tonumber(mainVlan.id)
            if vlanId == mainVlanId then
                print("Vlan *" .. vlanId .. "*  \"Management Vlan\" cannot be deleted")
            elseif vlanId ~= mainVlanId then
                local vlan = bridgeVlanList[key_in]
                local res = uci:delete("network", vlan.id)
                if res == true then
                    print("bridge-vlan *", vlan.id .. "* was deleted")
                elseif res == false then
                    print("Error bridge-vlan *", vlan.id .. "* can not be deleted")
                end
            end
        end
    end
end

function vlan_set_param(paramStr)
    print("Set name")
    if vlanRange == nil then
        print("Error, the Vlan range is specified incorrectly")
    else
        for key_in = vlanRange[1], vlanRange[2] do
            key_in = tostring(key_in)
            local vlan = bridgeVlanList[key_in]
            print("vlan.id = " .. vlan.id .. " name = " .. taggedType_in)
            local res = uci:set("network", vlan.id, paramStr, taggedType_in)
            if res == true then
                print("bridge-vlan *", vlan.id .. "* " .. paramStr .. " changed")
            elseif res == false then
                print("Error bridge-vlan *", vlan.id .. "* " ..  paramStr " can not be changed")
            end
        end
    end
end

function  vlan_mngt()
    local flag = tonumber(vlanRange[3])
    if flag == 0 then
        print("Error, the Vlan range not possible for set mngt_Vlan.\nYou need set only specified Vlan Id")
    else
        local key_in = vlanRange[1]
        key_in = tostring(key_in)
        local vlan = bridgeVlanList[key_in]
        if vlan == nil then
            print("Can not to change \"Management Vlan\" to *" .. key_in .. "* This Vlan not presented")
        elseif vlan.ports == nil then
            print("Can not to change \"Management Vlan\" to " .. key_in .. "For This Vlan ports are not configured")
        else
            local devName =  "switch." .. key_in
            local res = uci:set("network", "lan", "device", devName)
            if res == true then
                print("\"Management Vlan\" is: " .. devName)
            elseif res == false then
                print("Can not to change \"Management Vlan\"")
            end
        end
    end
end

-- ////////////////////////////////////////////////////////
function main()
    bridgeVlanList = getUciBridgeVlanList()
    if (bridgeVlanList == nill) then
        print("ERROR to read config bridge-vlan")
    end

    mainVlan = getUciInterfaceLan()

    if debug_print == 1 then
        --printParam()
        printBrVlanInfo(bridgeVlanList, 0)
    end

    portRange = checkPortRange(portList_in)
    vlanRange = checkPortRange(vlanRange_in)

    if cmd_in == "add" then
        if taggedType_in        == "untagged" then
            vlan_add_untagged()
        elseif taggedType_in    == "tagged" then
            vlan_add_tagged()
        end
    elseif cmd_in == "edit" then
        if taggedType_in        == "not_memb" then
            vlan_add_notMemberPorts()
        elseif taggedType_in    == "untagged" then
            vlan_add_untagged()
        elseif taggedType_in    == "tagged" then
            vlan_add_tagged()
        end
    elseif cmd_in == "delete" then
        vlan_delete()
    elseif cmd_in == "name" then
        vlan_set_param("descr")
    elseif cmd_in == "state" then
        vlan_set_param("state")
    elseif cmd_in == "mngt_vlan" then
        vlan_mngt()
    elseif cmd_in == "show" then
        printTable(bridgeVlanList)
        testVlanTable()
    else
        print("ELSE")
    end
end

main()