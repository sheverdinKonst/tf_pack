#!/usr/bin/lua

print("*************** set_sntp_del.lua ***************")

local function uciSaveChanges()
    print("uci changes:")
    os.execute("uci changes")
    os.execute("uci commit")
    os.execute("reload_config")
end

local function isEmpty(s)
  return s == nil or s == ''
end

local hostname = arg[1]
print("hostname=", hostname)

local hostnames = {}

local res = io.popen("uci show ntpclient"):read("*a")
print("Config : ")
print(res)

for line in res:gmatch("[^\n]+") do
    if line:find("hostname") then
        table.insert(hostnames, line)
    end
end

local hostdic   = {}
for i, line in ipairs(hostnames) do

    print("index = ", i, "line = ", line)
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

for id, host in pairs(hostdic) do
    print("index = ", id, "Hostname = ", host)
    if host == hostname then
        print ("STRING EQUAL")
        local uciCmd = "uci del ntpclient." .. id
        print("uciCmd = ", uciCmd)
        os.execute(uciCmd)
    else
        print ("STRING NOT EQUAL")
    end
end

uciSaveChanges()


