#!/usr/bin/lua

print("*************** show switch ver.-0.2 ***************")
local uci = require("luci.model.uci").cursor()
dofile("/etc/tfortis_klish/scripts/lua/global_function.lua")

local cmdList = {
    systemBoard         = "ubus call system board",
    systemBootloader    = "ubus call uci get \'{\"config\":\"system\",\"section\":\"tfortis\", \"option\":\"bootloader\"}\'",
    systemDescription   = "ubus call uci get \'{\"config\":\"system\",\"section\":\"tfortis\" }\'",
    systemTimeInfo      = "ubus call system info",
    netLanInfo          = "ubus call uci get \'{\"config\":\"network\", \"section\":\"lan\"}\'",
    netSwitchInfo       = "ubus call uci get \'{\"config\":\"network\",\"section\":\"switch\",\"option\":\"macaddr\"}\'",
    gateWay             = "ip r"
}

local lanInfoArr = {}

local systemBoard
local systemInfo
local netInfo
local gateWay

function printBootloaderVersion()
    bootVer = collectJsonTable(cmdList.systemBootloader)
    for key, value in pairs(bootVer) do
        if (key == "value") then
            print("\tBootloader Version:" .. "\t" ..  value)
        end
    end
end

function printSyStemBoard()
    print("System Info:")
    systemBoard = collectJsonTable(cmdList.systemBoard)
    for key, value in pairs(systemBoard) do
        if key == "board_name" then
            print("\t" .. "Device type:" .. "\t\t" ..  value)
        end
        if key == "release" then
            for i,j in pairs(systemBoard.release) do
                if i == "version" then
                    print("\t" .. "Firmware Version: " .. "\t" ..  j)
                end
            end
            printBootloaderVersion()
        end
        if key == "hostname" then
            print("\t" .. "Host name:" .. "\t\t" ..  value)
        end
    end
end

function printSyStemDescription()
    systemInfo = collectJsonTable(cmdList.systemDescription)
    for key, value in pairs(systemInfo.values) do
        if (key == "description") then
            print("\t" .. "Device Description:" .. "\t" ..  value)
        end
        if (key == "notes") then
            print("\t" .. "Device Location:" .. "\t" ..  value)
        end
        if (key == "contacts") then
            print("\t" .. "Device Contact:" .. "\t\t" ..  value)
        end
    end
end

function printSystemInfo()
    print("Time Info:")
    timeInfo = collectJsonTable(cmdList.systemTimeInfo)
    for key, value in pairs(timeInfo) do
        if key == "localtime" then
            print("\t" .. "System local time" .. "\t" ..  os.date("%d-%B-%Y %X"))
        end
        if key == "uptime" then
            local date = convertSeconds(tonumber(value))
            print("\t" .. "System Uptime:" .. "\t\t" .. date.days .. " days and " .. date.hours .. ":" .. date.minutes .. ":"  .. date.sec .. " (HH:MM:SS)")
        end
    end
end

function getNetLanInfo()
    print("Net Info:")
    lanInfo = collectJsonTable(cmdList.netLanInfo)

    for key, value in pairs(lanInfo.values) do
        if (key == "ipaddr") then
            local dataIP = {}
            dataIP.value            = value
            dataIP.str              = "\tIP Address: \t\t"
            lanInfoArr["ipaddr"]    = dataIP
        end
        if (key == "netmask") then
            local dataMask = {}
            dataMask.value          = value
            dataMask.str            = "\tSubnet Mask: \t\t"
            lanInfoArr["netmask"]   = dataMask
        end
        if (key == "device") then
            local dataDev = {}
            dataDev.value           = value
            dataDev.str             = "\tDevice: \t\t"
            lanInfoArr["device"]    = dataDev
        end
        if (key == "proto") then
            local dataProto = {}
            dataProto.value         = value
            dataProto.str           = "\tProto: \t\t\t"
            lanInfoArr["proto"]     = dataProto
        end
    end
end

function getNetMacInfo()
    macInfo = collectJsonTable(cmdList.netSwitchInfo)
    for key, value in pairs(macInfo) do
        if (key == "value") then
            local data          = {}
            data.value          = value
            data.str            = "\tMAC Address: \t\t"
            lanInfoArr["mac"]   = data
        end
    end
end

function getGateWay()
    gateWay = executeCommand(cmdList.gateWay)
    local gateWay_ip    = gateWay:match("^([^/]+)")
    local data          = {}
    data.value          = gateWay_ip
    data.str            = "\tDefault Gateway: \t"
    lanInfoArr["gate"]  = data
end

function printNetInfo()
    io.write(tostring(lanInfoArr["device"].str),  lanInfoArr["device"].value,   "\n")
    io.write(tostring(lanInfoArr["proto"].str),   lanInfoArr["proto"].value,    "\n")
    io.write(tostring(lanInfoArr["mac"].str),     lanInfoArr["mac"].value,      "\n")
    io.write(tostring(lanInfoArr["ipaddr"].str),  lanInfoArr["ipaddr"].value,   "\n")
    io.write(tostring(lanInfoArr["netmask"].str), lanInfoArr["netmask"].value,  "\n")
    io.write(tostring(lanInfoArr["gate"].str),    lanInfoArr["gate"].value,     "\n")
end

function printVlanId()
    local vlanList =  getUciBridgeVlanList()
    local vlanId_str = ""
    if (vlanList ~= nil) then
        for vlanid, _ in pairs(vlanList) do
            if string.len(vlanId_str) == 0 then
                vlanId_str = tostring(vlanid)
            else
                vlanId_str = vlanId_str ..  ", " .. tostring(vlanid)
            end
        end
        print("\tVlan ID:" .. "\t\t" .. vlanId_str)
    else
        print("\tVlan ID:" .. "\t\t" .. "Vlan not configured correctly")
    end

end

printSyStemBoard()
printSyStemDescription()
printSystemInfo()
getNetLanInfo()
getNetMacInfo()
getGateWay()
printNetInfo()
printVlanId()

-- local function getDate2(sec)
--     local date = os.date("!*t", sec)
--     print(date["day"] .. ":".. date["month"] .. ":" .. tostring(date["year"]) .. ":" .. date["hour"] .. ":" ..  date["min"] .. ":".. date["sec"] )
-- end

