local util = require "luci.util"
local util = require "luci.util"
local fs = require "nixio.fs"
local sys = require "luci.sys"
local http = require "luci.http"
local dispatcher = require "luci.dispatcher"
local http = require "luci.http"
local sys = require "luci.sys"
local uci = require "luci.model.uci".cursor()

local poe_ports = tostring(uci:get("poe", "global","ports"))

m=Map("poe",translate("PoE"))

s = m:section(TypedSection, "port", "PoE Settings") -- Especially the "interface"-sections

p = s:option(ListValue, "enable", "State") 
p:value("1", "Enable") 
p:value("0", "Disable")
p.default = "1"

s:option(Value, "priority", "PoE Priority", "PoE Priority") -- This will give a simple textbox

p=s:option(DummyValue, "current_state", translate("Current State"))
p.rows = 1
p.rmempty = false
function p.cfgvalue(self, section)
  local portn = uci:get("poe",section, "name")
  if portn ~= nil then
    local portstate = util.ubus('poe', 'info')
    if portstate ~= nil then
      local st = portstate['ports'][portn]
      if st ~= nil then
        return st['status']
      end        
    end
  end
end





p=s:option(DummyValue, "current_pow", translate("Power"))
p.rows = 1
p.rmempty = false
function p.cfgvalue(self, section)
  local portn = uci:get("poe",section, "name")
  if portn ~= nil then
    local portstate = util.ubus('poe', 'info')
    if portstate ~= nil then
      local st = portstate['ports'][portn]
      if st ~= nil then
        return st['consumption']
      end        
    end
  end
end
return m;
