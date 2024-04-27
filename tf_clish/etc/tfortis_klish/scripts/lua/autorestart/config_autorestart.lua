#!/usr/bin/lua

print("*************** config_autorestart ver.-0.0.1 ***************")
local uci = require("luci.model.uci").cursor()
local portRange       = arg[1]
local cmd_in          = arg[2]
local value           = arg[3]

dofile("/etc/tfortis_klish/scripts/lua/global_function.lua")

function is_valid_time(time)
    local pattern = "(%d+):(%d+)"
    local hh, mm = string.match(time, pattern)

    if mm == nil or hh == nil then
        return false
    end
    mm = tonumber(mm)
    hh = tonumber(hh)
    if mm < 0 or mm > 59 or hh < 0 or hh > 24 then
        print("error: time is not valid")
        return false
    end
    return true
end

function is_valid_ip(ip)
    local parts = {ip:match("(%d+)%.(%d+)%.(%d+)%.(%d+)")}
    if #parts ~= 4 then
        print("IP not valid")
        return false
    end
    for _, part in ipairs(parts) do
        if tonumber(part) < 0 or tonumber(part) > 255 then
            print("IP not valid")
            return false
        end

        if string.len(part) > 1 and string.sub(part, 1, 1) == '0' then
            print("IP not valid")
            return false
        end
    end

    return true
end

function showChanges()
    local res = executeCommand("uci changes")
    print("changes:")
    print(res)
end

function setState(portRange_out)
    for i = portRange_out[1], portRange_out[2] do
        local setStateCmd = "uci set tf_autorestart.lan" .. tostring(i) .. "." .. tostring(cmd_in) .. "=" .. tostring(value)
        executeCommand(setStateCmd)
    end
end

function setHost(portRange_out)
    for i = portRange_out[1], portRange_out[2] do
        local setStateCmd = "uci set tf_autorestart.lan" .. tostring(i) .. "." .. tostring(cmd_in) .. "=" .. tostring(value)
        executeCommand(setStateCmd)
    end
end

function setMinSpeed(portRange_out)
    for i = portRange_out[1], portRange_out[2] do
        local setStateCmd = "uci set tf_autorestart.lan" .. tostring(i) .. "." .. tostring(cmd_in) .. "=" .. tostring(value)
        executeCommand(setStateCmd)
    end
end

function setMaxSpeed(portRange_out)
    for i = portRange_out[1], portRange_out[2] do
        local setStateCmd = "uci set tf_autorestart.lan" .. tostring(i) .. "." .. tostring(cmd_in) .. "=" .. tostring(value)
        executeCommand(setStateCmd)
    end
end

function setTimeUp(portRange_out)
    for i = portRange_out[1], portRange_out[2] do
        local setStateCmd = "uci set tf_autorestart.lan" .. tostring(i) .. "." .. tostring(cmd_in) .. "=" .. tostring(value)
        executeCommand(setStateCmd)
    end
end

function setTimeDown(portRange_out)
    for i = portRange_out[1], portRange_out[2] do
        local setStateCmd = "uci set tf_autorestart.lan" .. tostring(i) .. "." .. tostring(cmd_in) .. "=" .. tostring(value)
        executeCommand(setStateCmd)
    end
end

function main(portRange_out)
    if (cmd_in == "state") then
        setState(portRange_out)

    elseif (cmd_in == "host") then
        if (is_valid_ip(value)) then
            setHost(portRange_out)
        end
    elseif (cmd_in == "min_speed") then
        setMinSpeed(portRange_out)

    elseif (cmd_in == "max_speed") then
        setMaxSpeed(portRange_out)

    elseif (cmd_in == "timeUp") then
        if (is_valid_time(value) == true) then
            setTimeUp(portRange_out)
        end

    elseif (cmd_in == "timeDown") then
        if (is_valid_time(value)) then
            setTimeDown(portRange_out)
        end
    end
    showChanges()
end

local portRange_out = checkPortRange(portRange)

if (portRange_out) then
    main(portRange_out)
end





