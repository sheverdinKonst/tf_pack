
---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by sheverdin.
--- DateTime: 3/11/24 4:58 PM
---

local tf_poe            = require "poe_module"
local tf_hw_sensors     = require "hw_sensors_module"
local tf_ups            = require "ups_module"
local tf_arPoe          = require "poe_autorestart_module"

local snmp_module = {}

-- mib_json_path = "/home/sheverdin/Fort_Telecom/TFORTIS_APP/tf_statiscs/etc/tf_statistics/mib/FORT-TELECOM-MIB.json"

local mib_oid_array = {}
local mib_name_array = {}

function snmp_module.print_mib_main_array(oid)

    for i, j in pairs(mib_oid_array) do
        print("\t[\"" .. i .. "\"] = ")
        print("\t{")
        for k, l in pairs(j) do
            print("\t\t" .. k .. " = \"" .. l .. "\",")
        end
        print("\t},")
    end
    local oid_struct = {}
    oid_struct = mib_oid_array[oid]
    if oid_struct == nil then
        print("OID not found")
    else
        print("name = "         .. mib_oid_array[oid].name)
        -- print("class ="         .. mib_array[oid].class)
        -- print("nodetype = "     .. mib_array[oid].nodetype)
        -- print("valueType = "    .. mib_array[oid].valueType)
        -- print("maxaccess = "    .. mib_array[oid].maxaccess)
        -- print("description = "  .. mib_array[oid].description)
        -- print("Length  = "  .. mib_array[oid].oidLength)
    end
end

function snmp_module.getLuaStruct()
    local cmd = "cat " .. mib_json_path
    local mib_json = tf.collectJsonTable(cmd)

    for key, value in pairs(mib_json) do
        keyList[key] = key
        local mib_oid_struct = {}
        if value[mib_mainKey.oid] ~= nil then
            if value[mib_mainKey.syntax] ~= nil then
                local syntax = {}
                --print(key .. "\t" .. value[mib_mainKey.oid])
                --print(value[mib_mainKey.name])
                mib_oid_struct.name = value[mib_mainKey.name]
                --print("name Struct = " .. mib_main_struct.name)
                mib_oid_struct.class       = value[mib_mainKey.class]
                mib_oid_struct.nodetype    = value[mib_mainKey.nodetype]
                mib_oid_struct.maxaccess   = value[mib_mainKey.maxaccess]
                mib_oid_struct.description = value[mib_mainKey.description]
                local mib_oid = value[mib_mainKey.oid]
                syntax = value[mib_mainKey.syntax]
                mib_oid_struct.valueType = syntax.type
                mib_oid_struct.oidLength = utilities.countDots(mib_oid)
                mib_oid_array[mib_oid] = mib_oid_struct
            end
        end
        --print("name from Array = " .. mib_array[value[mib_mainKey.oid]].name)
    end
    print("------------------------")
end

snmp_module.handlers = {
    comfortStartTime       = { "1.3.6.1.4.1.42019.3.2.1.1.1",       nil },
    comfStIndex            = { "1.3.6.1.4.1.42019.3.2.1.1.2.1.1",   nil },
    comfStState            = { "1.3.6.1.4.1.42019.3.2.1.1.2.1.2",   nil },
    autoRstIndex           = { "1.3.6.1.4.1.42019.3.2.1.2.1.1.1",   nil },
    autoRstMode            = { "1.3.6.1.4.1.42019.3.2.1.2.1.1.2",   nil },
    autoRstDstIP           = { "1.3.6.1.4.1.42019.3.2.1.2.1.1.3",   nil },
    autoRstSpeedDown       = { "1.3.6.1.4.1.42019.3.2.1.2.1.1.4",   nil },
    autoRstSpeedUp         = { "1.3.6.1.4.1.42019.3.2.1.2.1.1.5",   nil },
    autoReStartTimeOnHour  = { "1.3.6.1.4.1.42019.3.2.1.2.1.1.6",   nil },
    autoReStartTimeOnMin   = { "1.3.6.1.4.1.42019.3.2.1.2.1.1.7",   nil },
    autoReStartTimeOffHour = { "1.3.6.1.4.1.42019.3.2.1.2.1.1.8",   nil },
    autoReStartTimeOffMin  = { "1.3.6.1.4.1.42019.3.2.1.2.1.1.9",   nil },
    portPoeIndex           = { "1.3.6.1.4.1.42019.3.2.1.3.1.1.1",   nil },
    portPoeState           = { "1.3.6.1.4.1.42019.3.2.1.3.1.1.2",   nil },
    state                  = { "1.3.6.1.4.1.42019.3.2.1.4.1",       nil },
    upsModeAvalible        = { "1.3.6.1.4.1.42019.3.2.2.1.1",       tf_ups.getUpsData },
    upsPwrSource           = { "1.3.6.1.4.1.42019.3.2.2.1.2",       tf_ups.getUpsData },
    upsBatteryVoltage      = { "1.3.6.1.4.1.42019.3.2.2.1.3",       tf_ups.getUpsData },
    upsBatteryTime         = { "1.3.6.1.4.1.42019.3.2.2.1.4",       tf_ups.getUpsData },
    inputIndex             = { "1.3.6.1.4.1.42019.3.2.2.2.1.1.1",   nil },
    inputType              = { "1.3.6.1.4.1.42019.3.2.2.2.1.1.2",   nil },
    inputState            = { "1.3.6.1.4.1.42019.3.2.2.2.1.1.3",   tf_hw_sensors.getInputSensor },
    inputAlarm             = { "1.3.6.1.4.1.42019.3.2.2.2.1.1.4",   nil },
    fwVersion              = { "1.3.6.1.4.1.42019.3.2.2.3.1",       nil },
    portPoeStatusIndex     = { "1.3.6.1.4.1.42019.3.2.2.5.1.1.1",   nil },
    portPoeStatusState     = { "1.3.6.1.4.1.42019.3.2.2.5.1.1.2",   tf_poe.getPoeStatus },
    portPoeStatusPower     = { "1.3.6.1.4.1.42019.3.2.2.5.1.1.3",   nil },
    arPortIndex            = { "1.3.6.1.4.1.42019.3.2.2.6.1.1.1.1", nil },
    arPortStatus           = { "1.3.6.1.4.1.42019.3.2.2.6.1.1.1.2", tf_arPoe.autorestartStatus },
    csPortStatus           = { "1.3.6.1.4.1.42019.3.2.2.6.2.1.1.2", nil },
    csPortIndex            = { "1.3.6.1.4.1.42019.3.2.2.6.2.1.1.1", nil },
    portSfpIndex           = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.1",   nil },
    portSfpPresent         = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.2",   nil },
    portSfpSignalDetect    = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.3",   nil },
    portSfpVendor          = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.4",   nil },
    portSfpOui             = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.5",   nil },
    portSfpPartNumber      = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.6",   nil },
    portSfpTemperature     = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.8",   nil },
    portSfpRevision        = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.7",   nil },
    portSfpVoltage         = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.9",   nil },
    portSfpBiasCurrent     = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.10",  nil },
    portSfpTxOutPower      = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.11",  nil },
    portSfpTxOutPowerDb    = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.12",  nil },
    portSfpRxOutPower      = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.13",  nil },
    portSfpRxOutPowerDb    = { "1.3.6.1.4.1.42019.3.2.2.7.1.1.14",  nil },
    sensorConnected        = { "1.3.6.1.4.1.42019.3.2.2.8.1",       nil },
    sensorTemperature      = { "1.3.6.1.4.1.42019.3.2.2.8.2",       tf_hw_sensors.getTemperature },
    sensorHumidity         = { "1.3.6.1.4.1.42019.3.2.2.8.3",       tf_hw_sensors.getHumidity },
    sensorTemperatureMin   = { "1.3.6.1.4.1.42019.3.2.2.8.4",       nil },
    sensorTemperatureMax   = { "1.3.6.1.4.1.42019.3.2.2.8.5",       nil },
    sensorHumidityMin      = { "1.3.6.1.4.1.42019.3.2.2.8.6",       nil },
    sensorHumidityMax      = { "1.3.6.1.4.1.42019.3.2.2.8.7",       nil },
    inputIndexTLP          = { "1.3.6.1.4.1.42019.4.2.2.1.1.1",     nil },
    inputStateTLP          = { "1.3.6.1.4.1.42019.4.2.2.1.1.2",     nil },
    outIndex               = { "1.3.6.1.4.1.42019.4.2.3.1.1.1",     nil },
    outState               = { "1.3.6.1.4.1.42019.4.2.3.1.1.2",     nil },
    fwVersionTLP           = { "1.3.6.1.4.1.42019.4.2.4.1",         nil },
}

snmp_module.mib_list = {

    ["1.3.6.1.4.1.42019.3.2.2.2.1.1.1"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Input Number",
        maxaccess = "read-write",
        name = "inputIndex",
    },
    ["1.3.6.1.4.1.42019.3.2.1.2.1.1.5"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Upper level of speed",
        maxaccess = "read-write",
        name = "autoRstSpeedUp",
    },
    ["1.3.6.1.4.1.42019.3.2.2.5.1.1.2"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "PoE State",
        maxaccess = "read-only",
        name = "portPoeStatusState",
    },
    ["1.3.6.1.4.1.42019.3.2.2.8.2"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Temperature value",
        maxaccess = "read-only",
        name = "sensorTemperature",
    },
    ["1.3.6.1.4.1.42019.3.2.1.2.1.1.8"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "Hour of PoE Disable",
        maxaccess = "read-write",
        name = "autoReStartTimeOffHour",
    },
    ["1.3.6.1.4.1.42019.3.2.2.5.1.1.1"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Port Number",
        maxaccess = "read-write",
        name = "portPoeStatusIndex",
    },
    ["1.3.6.1.4.1.42019.3.2.1.2.1.1.7"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "Minute of PoE Enable",
        maxaccess = "read-write",
        name = "autoReStartTimeOnMin",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.11"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "TX output optical power (uW)",
        maxaccess = "read-only",
        name = "portSfpTxOutPower",
    },
    ["1.3.6.1.4.1.42019.3.2.2.8.1"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Connection status of temperature/humidity sensor",
        maxaccess = "read-only",
        name = "sensorConnected",
    },
    ["1.3.6.1.4.1.42019.3.2.2.3.1"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "scalar",
        valueType = "OCTET STRING",
        description = "Firmware version",
        maxaccess = "read-only",
        name = "fwVersion",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.14"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "RX output optical power (dB)",
        maxaccess = "read-only",
        name = "portSfpRxOutPowerDb",
    },
    ["1.3.6.1.4.1.42019.3.2.2.6.2.1.1.1"] =
    {
        class = "objecttype",
        oidLength = "14",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Port Number",
        maxaccess = "read-write",
        name = "csPortIndex",
    },
    ["1.3.6.1.4.1.42019.3.2.2.6.1.1.1.2"] =
    {
        class = "objecttype",
        oidLength = "14",
        nodetype = "column",
        valueType = "INTEGER",
        description = "State of special function 'AutoRestart'. 'normal' - no error, 'noLink' - no link activity on port, 'noPing' - no answer to ping, 'lowSpeed' - low speed on port",
        maxaccess = "read-only",
        name = "arPortStatus",
    },
    ["1.3.6.1.4.1.42019.3.2.1.1.1"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "Comfort Start Time (in Hours)",
        maxaccess = "read-write",
        name = "comfortStartTime",
    },
    ["1.3.6.1.4.1.42019.3.2.2.6.2.1.1.2"] =
    {
        class = "objecttype",
        oidLength = "14",
        nodetype = "column",
        valueType = "INTEGER",
        description = "State of special function 'ComfortStart' on port. 'normal' - ComfortStart is ready, 'processing' - ComfortStart in processing",
        maxaccess = "read-only",
        name = "csPortStatus",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.1"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "SFP port number",
        maxaccess = "read-write",
        name = "portSfpIndex",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.6"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "DisplayString",
        description = "SFP module Part Number",
        maxaccess = "read-only",
        name = "portSfpPartNumber",
    },
    ["1.3.6.1.4.1.42019.3.2.2.2.1.1.3"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Input State",
        maxaccess = "read-only",
        name = "inputState",
    },
    ["1.3.6.1.4.1.42019.3.2.2.2.1.1.4"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Alarm state condition",
        maxaccess = "read-only",
        name = "inputAlarm",
    },
    ["1.3.6.1.4.1.42019.3.2.1.3.1.1.2"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "PoE State",
        maxaccess = "read-write",
        name = "portPoeState",
    },
    ["1.3.6.1.4.1.42019.3.2.2.5.1.1.3"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "PoE load power (in mW) ",
        maxaccess = "read-only",
        name = "portPoeStatusPower",
    },
    ["1.3.6.1.4.1.42019.3.2.2.1.3"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "This variable shows battery voltage. This option only for switches with UPS module.",
        maxaccess = "read-only",
        name = "upsBatteryVoltage",
    },
    ["1.3.6.1.4.1.42019.3.2.1.1.2.1.1"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        maxaccess = "read-write",
        name = "comfStIndex",
    },
    ["1.3.6.1.4.1.42019.3.2.1.2.1.1.1"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        maxaccess = "read-write",
        name = "autoRstIndex",
    },
    ["1.3.6.1.4.1.42019.3.2.2.1.4"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "This variable shows estimated operating time (in sec.) of the device after switching to battery power (only for devices with UPS module).",
        maxaccess = "read-only",
        name = "upsBatteryTime",
    },
    ["1.3.6.1.4.1.42019.3.2.1.4.1"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "Output Relay State",
        maxaccess = "read-write",
        name = "state",
    },
    ["1.3.6.1.4.1.42019.4.2.2.1.1.2"] =
    {
        class = "objecttype",
        oidLength = "12",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Input State",
        maxaccess = "read-only",
        name = "inputStateTLP",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.9"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Voltage in SFP module (mV)",
        maxaccess = "read-only",
        name = "portSfpVoltage",
    },
    ["1.3.6.1.4.1.42019.3.2.1.2.1.1.6"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "Hour of PoE Enable",
        maxaccess = "read-write",
        name = "autoReStartTimeOnHour",
    },
    ["1.3.6.1.4.1.42019.3.2.2.1.1"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "UPS option is avalible. 'true' for switches with UPS module.",
        maxaccess = "read-only",
        name = "upsModeAvalible",
    },
    ["1.3.6.1.4.1.42019.4.2.3.1.1.2"] =
    {
        class = "objecttype",
        oidLength = "12",
        nodetype = "column",
        valueType = "INTEGER",
        description = "State of Output",
        maxaccess = "read-write",
        name = "outState",
    },
    ["1.3.6.1.4.1.42019.4.2.3.1.1.1"] =
    {
        class = "objecttype",
        oidLength = "12",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Index of Output",
        maxaccess = "read-write",
        name = "outIndex",
    },
    ["1.3.6.1.4.1.42019.3.2.2.6.1.1.1.1"] =
    {
        class = "objecttype",
        oidLength = "14",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Port Number",
        maxaccess = "read-write",
        name = "arPortIndex",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.4"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "DisplayString",
        description = "SFP module Vendor",
        maxaccess = "read-only",
        name = "portSfpVendor",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.13"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "RX output optical power (uW)",
        maxaccess = "read-only",
        name = "portSfpRxOutPower",
    },
    ["1.3.6.1.4.1.42019.3.2.1.2.1.1.2"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Mode of Auto Restart controller",
        maxaccess = "read-write",
        name = "autoRstMode",
    },
    ["1.3.6.1.4.1.42019.3.2.2.2.1.1.2"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Input Type: 'buil-in' - inputs on main board, 'plc' - inputs on optional board",
        maxaccess = "read-only",
        name = "inputType",
    },
    ["1.3.6.1.4.1.42019.3.2.1.3.1.1.1"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Port Number",
        maxaccess = "read-write",
        name = "portPoeIndex",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.10"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "TX bias current (uA)",
        maxaccess = "read-only",
        name = "portSfpBiasCurrent",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.8"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Temperature of SFP module. In degrees Celsius",
        maxaccess = "read-only",
        name = "portSfpTemperature",
    },
    ["1.3.6.1.4.1.42019.3.2.1.2.1.1.9"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "Minute of PoE Disable",
        maxaccess = "read-write",
        name = "autoReStartTimeOffMin",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.12"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "TX output optical power (dB)",
        maxaccess = "read-only",
        name = "portSfpTxOutPowerDb",
    },
    ["1.3.6.1.4.1.42019.3.2.2.8.4"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Minimal value of temperature",
        maxaccess = "read-write",
        name = "sensorTemperatureMin",
    },
    ["1.3.6.1.4.1.42019.3.2.2.8.7"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "Maximal value of Humidity",
        maxaccess = "read-write",
        name = "sensorHumidityMax",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.5"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "DisplayString",
        description = "SFP module Vendor OUI",
        maxaccess = "read-only",
        name = "portSfpOui",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.3"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Optical Signal Detect of SFP module",
        maxaccess = "read-only",
        name = "portSfpSignalDetect",
    },
    ["1.3.6.1.4.1.42019.3.2.2.1.2"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "This variable shows the power source for switch. 'Battery' - power source is battery, no external AC voltage. 'AC' - power source is external AC voltage,normal operation. This option only for switches with UPS module.",
        maxaccess = "read-only",
        name = "upsPwrSource",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.2"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Present of SFP module in the case",
        maxaccess = "read-only",
        name = "portSfpPresent",
    },
    ["1.3.6.1.4.1.42019.3.2.2.8.6"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "scalar",
        valueType = "INTEGER",
        description = "Minimal value of Humidity",
        maxaccess = "read-write",
        name = "sensorHumidityMin",
    },
    ["1.3.6.1.4.1.42019.3.2.1.1.2.1.2"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        maxaccess = "read-write",
        name = "comfStState",
    },
    ["1.3.6.1.4.1.42019.3.2.1.2.1.1.3"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "IpAddress",
        description = "IP address of device",
        maxaccess = "read-write",
        name = "autoRstDstIP",
    },
    ["1.3.6.1.4.1.42019.3.2.2.7.1.1.7"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "DisplayString",
        description = "SFP module Revision",
        maxaccess = "read-only",
        name = "portSfpRevision",
    },
    ["1.3.6.1.4.1.42019.4.2.4.1"] =
    {
        class = "objecttype",
        oidLength = "10",
        nodetype = "scalar",
        valueType = "OCTET STRING",
        description = "Firmware version",
        maxaccess = "read-only",
        name = "fwVersionTLP",
    },
    ["1.3.6.1.4.1.42019.3.2.1.2.1.1.4"] =
    {
        class = "objecttype",
        oidLength = "13",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Lower level of speed",
        maxaccess = "read-write",
        name = "autoRstSpeedDown",
    },
    ["1.3.6.1.4.1.42019.3.2.2.8.3"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Humidity value",
        maxaccess = "read-only",
        name = "sensorHumidity",
    },
    ["1.3.6.1.4.1.42019.4.2.2.1.1.1"] =
    {
        class = "objecttype",
        oidLength = "12",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Input Number",
        maxaccess = "read-write",
        name = "inputIndexTLP",
    },
    ["1.3.6.1.4.1.42019.3.2.2.8.5"] =
    {
        class = "objecttype",
        oidLength = "11",
        nodetype = "column",
        valueType = "INTEGER",
        description = "Maximal value of temperature",
        maxaccess = "read-write",
        name = "sensorTemperatureMax",
    }
}

return snmp_module

