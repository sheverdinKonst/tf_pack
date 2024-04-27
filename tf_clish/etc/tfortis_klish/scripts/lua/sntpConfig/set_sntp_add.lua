#!/usr/bin/lua

print("*************** set_sntp_add.lua ***************")

local function uciSaveChanges()
    print("uci changes:")
    os.execute("uci changes")
    os.execute("uci commit")
    os.execute("reload_config")
end

local hostname = arg[1]
print("hostname=", hostname)

local hostnames = {}
local ntpserverid = io.popen("uci add ntpclient ntpserver"):read("*a")
ntpserverid = ntpserverid:gsub("\n", "")
-- print("ntpserverid = ", ntpserverid)

local uciaddsntpServer = "uci set ntpclient.@ntpserver[-1].hostname=" .. hostname
io.popen(uciaddsntpServer):read("*a")

uciSaveChanges()

local res = io.popen("uci show ntpclient"):read("*a")

local function isEmpty(s)
  return s == nil or s == ''
end

local function removeDuplicates(arr)
    local result   = {}
    local hash     = {}

    for i, value in pairs(arr) do
        if isEmpty(value) then
             print("hostname is empty")
             local cmd = "uci del ntpclient." .. i
             local fd = io.popen(cmd):read("*a")
        else
            if not hash[value] then
                result[i]=value
                hash[value] = true
            else
                print("*********** Duplicates ********")
                print("i = ", i, "Value = ", value)
                local cmd = "uci del ntpclient." .. i
                local fd = io.popen(cmd):read("*a")
            end
        end
    end
    return result
end

for line in res:gmatch("[^\n]+") do
    if line:find("hostname") then
        table.insert(hostnames, line)
    end
end

local hostdic = {}

for i, line in ipairs(hostnames) do
    --print(line)
    ---print("index = ", i, "line = ", line)
    -- ntpclient.@ntpserver[2].hostname='1.1.1.2'
    local eqSeparator = "="
    local dotSeparator = "."
    local parts   = {}
    local idParts = {}

    for uciPart in string.gmatch(line, "[^" .. eqSeparator .. "]+") do
        table.insert(parts, uciPart)
    end
    -- parts[1] = ntpclient.@ntpserver[2]
    local cmd = "uci show " .. parts[1]
    local idstr = io.popen(cmd):read("*a")

    for idpart in idstr.gmatch(idstr, "[^" .. dotSeparator .. "]+") do
        table.insert(idParts, idpart)
    end

    -- local index = tonumber(string.match(line, "%[(%d+)%]"))
    local str = string.match(line, "=%s*'(.+)'")

    hostdic[idParts[2]] = str
end

for i, host in pairs(hostdic) do
    --print("index = ", i, "Hostname = ", host)
end

local uniqueArray = removeDuplicates(hostdic)

--print("unique Hostname list")
for i, value in pairs(uniqueArray) do
    --print("i = ", i, "value = ", value)
end

uciSaveChanges()



