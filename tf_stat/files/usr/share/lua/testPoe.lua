#!/usr/bin/lua
---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by sheverdin.
--- DateTime: 3/13/24 3:12 PM
---
local tf        = require "tf_module"
local utilities = require "tf_utilities"
local tf_snmp   = require "tf_snmp_module"
--local log       = require "tf_log"
local tf_he_sensors = require "hw_sensors_module"

local inputOid = ".1.3.6.1.4.1.42019.3.2.2.8.3"

local endIndex = string.len(inputOid)

local portNum       = ""
local modifiedOid   = ""
modifiedOid = string.sub(inputOid, 2, endIndex)
if tf_snmp.mib_list[modifiedOid] == nil then
    modifiedOid = string.sub(inputOid, 2, endIndex - 2)
    portNum = string.sub(inputOid, endIndex, endIndex)
elseif tf_snmp.mib_list[inputOid] ~= nil then
    modifiedOid = inputOid
end

local mib_obj = tf_snmp.mib_list[modifiedOid]

print("name = " .. mib_obj.name)
local handler_obj = tf_snmp.handlers[mib_obj.name]
local res = handler_obj[2](portNum)
print("res = " .. res)

print(modifiedOid)
