local TimeZonevalue = arg[1]
local cmdZoneName = "Etc/GMT"
local cmdTimeZone = "<"

if tonumber(TimeZonevalue) < 0 then
    print("Time zone minus")
    local number = tonumber(TimeZonevalue) * -1
    local setZoneName = "system.@system[0].zonename=" .. cmdZoneName .. TimeZonevalue
    local setTimeZone = "system.@system[0].timezone=" .. cmdTimeZone

    if number < 10 then
        setTimeZone = setTimeZone .. "+0" .. number .. ">" .. TimeZonevalue
    else
        setTimeZone = setTimeZone .. "+" .. number .. ">" .. TimeZonevalue
    end
    print("setZoneName: ", setZoneName)
    print("setTimeZone: ", setTimeZone)
else
    print("Time zone plus")
    local setZoneName = "system.@system[0].zonename=" .. cmdZoneName .. "+" .. TimeZonevalue
    local setTimeZone = "system.@system[0].timezone=" .. cmdTimeZone

    if tonumber(TimeZonevalue) < 10 then
        setTimeZone = setTimeZone .. "-0" .. TimeZonevalue .. ">" .. TimeZonevalue
    else
        setTimeZone = setTimeZone .. "-" .. TimeZonevalue .. ">" .. TimeZonevalue
    end
end

os.execute("uci set " .. setZoneName)
os.execute("uci set " .. setTimeZone)
print("uci changes: ")
os.execute("uci changes")
os.execute("uci commit")
os.execute("reload_config")