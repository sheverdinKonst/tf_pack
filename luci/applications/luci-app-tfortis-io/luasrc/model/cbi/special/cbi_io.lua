local fs = require "nixio.fs"
local sys = require "luci.sys"
require "ubus"
require "socket"
require "socket.unix"

m = Map("tfortis-io", translate("Inputs/Outputs")) -- cbi_file is the config file in /etc/config
d = m:section(TypedSection, "tfortis-io")  -- info is the section called info in cbi_file
d.anonymous = true
d.addremove = false


d:tab("tab_status", translate("Status"))
d:tab("tab_settings", translate("Settings"))


-- Статусная информация
a=d:taboption("tab_status", DummyValue, "state", translate("Tamper current state"))
a.rows = 1
a.rmempty = false
function a.cfgvalue()
    return luci.util.exec("/usr/bin/request_ctrl TAMPER get")
end

a=d:taboption("tab_status", DummyValue, "state", translate("Sensor1 current state"))
a.rows = 1
a.rmempty = false
function a.cfgvalue()
    return luci.util.exec("/usr/bin/request_ctrl SENSOR1 get")
end

a=d:taboption("tab_status", DummyValue, "state", translate("Sensor2 current state"))
a.rows = 1
a.rmempty = false
function a.cfgvalue()
    return luci.util.exec("/usr/bin/request_ctrl SENSOR2 get")
end

a=d:taboption("tab_status", DummyValue, "state", translate("Output current state"))
a.rows = 1
a.rmempty = false
function a.cfgvalue()
    return luci.util.exec("/usr/bin/request_ctrl RELAY1 get")
end



-- Настройки
b = d:taboption("tab_settings", Flag, "tamper_state", translate ("Tamper State"),translate ("Create events when box tamper state changes"))
b.widget = "checkbox"
b.default = b.disabled
b.optional = true

b = d:taboption("tab_settings", Flag, "input1_state", translate ("Input1 State"),translate ("Create events when Input1 state changes"))
b.widget = "checkbox"
b.default = b.disabled
b.optional = true

b = d:taboption("tab_settings", Flag, "input2_state", translate ("Input2 State"),translate ("Create events when Input1 state changes"))
b.widget = "checkbox"
b.default = b.disabled
b.optional = true

b = d:taboption("tab_settings", Value, "out1_state", translate("Relay1 State"),translate ("Manual control of Relay output"))
b:value("enable",  translate("Open"))
b:value("disable",  translate("Close"))
b.default = "enable"
b.addremove = false
b.optional = false



return m;
