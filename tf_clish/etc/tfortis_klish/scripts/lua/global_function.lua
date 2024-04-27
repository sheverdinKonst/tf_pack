
local dkjson = require("dkjson")
local uci = require("luci.model.uci").cursor()

stp_cmdList = {
    get_mstpdConfig = "ubus call uci get \'{\"config\":\"mstpd\"}\'",
    get_netConfig   = "ubus call uci get \'{\"config\":\"network\"}\'"
}

function executeCommand(command)
    local handle = io.popen(command)
    local result = handle:read("*a")
    handle:close()
    return result
end

function collectJsonTable(ubusCmd)
    local ubusRes = executeCommand(ubusCmd)
    local jsonData = dkjson.decode(ubusRes, 1)
    return jsonData
end

function convertSeconds(seconds)
    local date = {}
    date.days = math.floor(seconds / 86400)
    seconds = seconds % 86400
     date.hours = math.floor(seconds / 3600)
    seconds = seconds % 3600
    date.minutes = math.floor(seconds / 60)
    date.sec = seconds % 60
    return date
end

function getUciBridgeVlanList()
    local brVlanList = {}
    uci:foreach("network", "bridge-vlan",
            function(v)
                local vlan = {}
                if v.ports ~= nil then
                    local portNumber = 0
                    vlan.ports  =  v.ports
                    vlan.id     = v[".name"]
                    vlan.num    = v.vlan
                    vlan.device = v.device

                    if (v.state ~= nil) then
                        vlan.state  = v.state
                    else
                        print("vlan.state is empty")
                    end

                    if (v.descr ~= nil) then
                        vlan.descr  = v.descr
                    else
                        print("vlan.descr is empty")
                    end

                    vlan.index  = v[".index"]
                    brVlanList[v.vlan] = vlan

                elseif v.ports == nil then
                    print("bridge-vlan *", v[".name"] .. "* not have any port was deleted")
                    local res = uci:delete("network", v[".name"])
                    print("res = ", res)
                end
            end)
    if (brVlanList ~= nil) then
        return brVlanList
    elseif (brVlanList == nil) then
        return nil
    end
end

function checkPortRange(range)
    local result = {}
    if (range == "all") then
        result[1] = 1
        result[2] = 1
        result[3] = "all"
    elseif (range == "switch") then
        result[1] = 1
        result[2] = 1
        result[3] = "switch"
    else
        for value in string.gmatch(range, "[^-]+") do
            value = tonumber(value)
            table.insert(result, value)
            result[3] = 0
        end

        if result[2] == nil then
            result[2] = result[1]
            result[3] = 1
        end
        if result[1] > result[2] then
            result = nil
        end
    end
    return result
end

function needToPrint(portStr, name)
    local isPrint = 0
    if (portStr[3] == "all") then
        isPrint = 1
    else
        local str, num =name:match("(%a+)(%d+)")
        portNumber = tonumber(num)
        local portRangeMin = tonumber(portStr[1])
        local portRangeMax = tonumber(portStr[2])
        if str == "lan" then
            if   portNumber  >= portRangeMin and portNumber <= portRangeMax then
                isPrint = 1
            end
        end
    end
    return isPrint
end

function getPortList(config, arg1, arg2 )
    local configSwitch  = config[arg1]
    local portList      = configSwitch[arg2]
    -- printList(portList)
    return portList
end

function printList (pList)
    for k,v in pairs(pList) do
        if (k == nil) then
            print("key = nill")
        elseif type(k) == "table" then
            print("key is table")
        else
            io.write("key = ", tostring(k), "\n")
        end
        if (v == nil) then
            print("value = nill")
        elseif type(v) == "table" then
            print("value is table")
        else
            io.write("value = ", tostring(v), "\n")
        end
    end
end

function capFirstLetter(inputString)
    if inputString == nil or inputString == "" then
        return inputString
    end
    local firstChar = string.sub(inputString, 1, 1)
    local capitalFirstChar = string.upper(firstChar)
    local restOfString = string.sub(inputString, 2)
    return capitalFirstChar .. restOfString
end

function getPort(portStr)
    port = {}
    local portNumber = 0
    local state = "untagged"

    local portStr_1 = tostring(portStr)
    if string.find(portStr, ":t") then
        local str, num, isTagget = portStr_1:match("(%w+)(%d+):(%a+)")
        portNumber = tonumber(num)
        state = "tagged"
    else
        local str, num = portStr_1:match("(%a+)(%d+)")
        portNumber = tonumber(num)
    end
    port.state      = state
    port.portNumber = portNumber
    return port
end
