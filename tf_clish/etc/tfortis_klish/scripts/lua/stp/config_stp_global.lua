#!/usr/bin/lua

dofile("/etc/tfortis_klish/scripts/lua/global_function.lua")

local uci = require("luci.model.uci").cursor()

-- uci set network.switch.stp='1'
-- Removing package luci-i18n-tn-mstpd-ru from root...
-- Removing package luci-app-tn-mstpd from root...
-- Removing package mstpd from root...


function configState()
    print("function configState()")
    print("value = " .. arg[2])
    local value = tonumber(arg[2])
    if value == 0 or value == 1 then
        uci:set("network", "switch" , "stp",  tostring(arg[2]))
    else
        io.write("error >> State value not corrected\n")
    end
end

function configLogLevel()
    print("function configLogLevel()")
    print("value = " .. arg[2])
    local value = tonumber(arg[2])
    if value >= 0 and value <= 4 then
        uci:set("mstpd", "global" , "loglevel", tostring(arg[2]))
    else
        io.write("error >> Log level not corrected\n")
    end
end

function configPriority()
    print("function configPriority()")
    print("value = " .. arg[2])
    local value = tonumber(arg[2])
    if value >= 0 and value <= 15 then
        uci:set("mstpd", "switch" , "treeprio", tostring(arg[2]))
    else
         io.write("error >> Priority value not corrected\n")
    end
end

function configProtocol()
    print("function configProtocol()")
    print("value = " .. arg[2])
    if arg[2] == "stp" or arg[2] == "rstp" then
        uci:set("mstpd", "switch" , "forcevers", tostring(arg[2]))
    else
        io.write("error >> Protocol value not corrected\n")
    end
end

function configAgeing()
     print("function configAgeing()")       
     print("value = " .. arg[2])
     local value = tonumber(arg[2])
    if value >= 10 and value <= 1000000 then
        uci:set("mstpd", "switch" , "ageing", tostring(arg[2]))
    else
         io.write("error >> Ageing value must be in range 10 – 1.0000.00 seconds\n")
    end
end

function configHello_time()
    print("function configHello_time()")
    print("value = " .. arg[2])
    local value = tonumber(arg[2])
    if value >= 1 and value <= 10 then
        uci:set("mstpd", "switch" , "hello", tostring(arg[2]))
    else
        io.write("error >> Hello time must be in range 1 – 10 seconds\n")
    end
end

function configForward_delay()
    print("function configForward_delay()")
    print("value = " .. arg[2])
    local value = tonumber(arg[2])
    if value >= 4 and value <= 30 then
        uci:set("mstpd", "switch" , "fdelay", tostring(arg[2]))
    else
        io.write("error >> Forward delay time must be in range 4 - 30 seconds\n")
    end
end

function configMax_age()
     print("function configMax_age()")
     print("value = " .. arg[2])
    local value = tonumber(arg[2])
    if value >= 6 and value <= 40 then
        uci:set("mstpd", "switch" , "maxage", tostring(arg[2]))
    else
        io.write("error >> Max age time must be in range 6 - 40 seconds\n")
    end
end

function configTX_hold_count()
    print("function configTX_hold_count(()")
    print("value = " .. arg[2])
    local value = tonumber(arg[2])
    if value >= 1 and value <= 10 then
        uci:set("mstpd", "switch" , "txholdcount", tostring(arg[2]))
    else
        io.write("error >> TX hold count must be in range 1 - 10 seconds\n")
    end
end

local cmdlist =
{
    {cmd = "state",         func = configState},
    {cmd = "loglevel",      func = configLogLevel},
    {cmd = "priority",      func = configPriority},
    {cmd = "protocol",      func = configProtocol},
    {cmd = "ageing",        func = configAgeing},
    {cmd = "hellotime",     func = configHello_time},
    {cmd = "forwarddelay",  func = configForward_delay},
    {cmd = "maxage",        func = configMax_age},
    {cmd = "txholdcount",   func = configTX_hold_count}
}

function validateParam()
    local errorCode = 0
    if arg[1]  == nil then
        io.write("error >>> Command not found\n")
        errorCode = 1
    end
    if arg[2] == nil then
        io.write("error >>> Command value not found\n")
        errorCode = 2
    end
    return errorCode;
end

function main_config_stp_global()
    local errorCode = 0
    errorCode = validateParam()
    if errorCode == 0 then
        for _, value in pairs(cmdlist) do
            if arg[1] == value.cmd then
                value.func()
            end
        end
    end
end

main_config_stp_global()


