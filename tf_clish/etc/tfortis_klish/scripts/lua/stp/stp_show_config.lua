#!/usr/bin/lua

dofile("/etc/tfortis_klish/scripts/lua/global_function.lua")

local values_t = {
    values_e        = "values",
    global_e        = "global",
    switch_e        = "switch",
    switch_ports_e  = "ports"
}

local global_loglevel_e = {
    "disable",
    "error",
    "info",
    "debug",
    "state machine transition"
}

local global_enabled = {
    "no",
    "yes"
}

local switch_forcevers = {
    "STP",
    "RSTP",
    "MSTP"
}

local switch_treeprio = {
    "0",
    "4096",
    "8192",
    "12288",
    "16384",
    "20480",
    "24576",
    "28672",
    "32768",
    "36864",
    "40960",
    "45056",
    "49152",
    "53248",
    "57344",
    "61440",
}

local mstpd_config = {
    global_config =
    {
        {".name",       ""},
        {"enabled",     "\tEnabled:         "},
        {"loglevel",    "\tLoglevel:        "},
        {"bridge",      "\tBridge list:\n"}
    },
    switch_config =
    {
        {".name",       ""},
        {".type",       "\tDevice type:     "},
        {"forcevers",   "\tProtocol:        "},
        {"treeprio",    "\tPriority:        "},
        {"hello",       "\tHello time:      "},
        {"fdelay",      "\tFw delay time:   "},
        {"maxage",      "\tMax age:         "},
        {"ageing",      "\tAgeing:          "},
        {"txholdcount", "\tTX hold count:   "},
    },
    lan_config =
    {
        {".name",      ""},
        {".type",      "\tType:             "},
        {"treeprio",   "\tPriority:         "},
        {"pathcost",   "\tPath  cost:       "},
        {"adminedge",  "\tAdmin edge:       "},
        {"autoedge",   "\tAuto  edge:       "},
        {"bpdufilter", "\tBPDU  filter:     "},
        {"bpduguard",  "\tBPDU  guard:      "}
    }
}

function printStpConfigGlobal(mstpdConfigValues)
    local mstpd_global_list = mstpdConfigValues[values_t.global_e]
    local mstpd_switch_list = mstpdConfigValues[values_t.switch_e]

    for _, value_g in pairs(mstpd_config.global_config) do

        if mstpd_global_list[value_g[1]] ~= nil then
            if value_g[1] == ".name" then
                local name = capFirstLetter(mstpd_global_list[value_g[1]])
                io.write(name, ":\n")
            elseif value_g[1] == "enabled" then
                local enabledIndex = mstpd_global_list[value_g[1]]
                local enabled_v = global_enabled[enabledIndex + 1]
                io.write(value_g[2],  enabled_v, "\n")
            elseif value_g[1] == "loglevel" then
                local loglevelIndex = mstpd_global_list[value_g[1]]
                local loglevel_v = global_loglevel_e[loglevelIndex + 1]
                io.write(value_g[2], loglevel_v, "\n")
            elseif value_g[1] == "bridge" then
                io.write(value_g[2])
                for i,j in pairs(mstpd_global_list[value_g[1]]) do
                    io.write("\t\t", j, "\n")
                end
            end
        end
    end

    for _, value_sw in pairs(mstpd_config.switch_config) do
        if mstpd_switch_list[value_sw[1]] ~= nil then
            if value_sw[1] == ".name" then
                local name = capFirstLetter(mstpd_switch_list[value_sw[1]])
                io.write(name, ":\n")
            elseif value_sw[1] == "treeprio"  then
                local treeprioIndex = mstpd_switch_list[value_sw[1]]
                local treeprio_v = switch_treeprio[treeprioIndex + 1]
                io.write(value_sw[2], treeprio_v, "\n")
            else
                io.write(value_sw[2], mstpd_switch_list[value_sw[1]], "\n")
            end
        end
    end
end

function  printStpConfigPort(portList, range, mstpdConfig)
    for _, portStr in pairs(portList) do
        port = getPort(portStr)
        local minPort  =  tonumber(range[1])
        local maxPort  =  tonumber(range[2])
        if   port.portNumber >=  minPort and  port.portNumber <=  maxPort  then
            local mstpdPortConfig = mstpdConfig[portStr]
            for _, value_lan in pairs(mstpd_config.lan_config) do
                if value_lan[1] == ".name" then
                    local name = capFirstLetter(mstpdPortConfig[value_lan[1]])
                    io.write("-----------------------------------------------\n")
                    io.write(name, ":\n")
                elseif value_lan[1] == "treeprio"  then
                    local treeprioIndex = mstpdPortConfig[value_lan[1]]
                    local treeprio_v    = switch_treeprio[treeprioIndex + 1]
                    io.write(value_lan[2], treeprio_v, "\n")
                else
                    io.write(value_lan[2], mstpdPortConfig[value_lan[1]], "\n")
                end
            end
        end
    end
end

function stp_main()
    local cmd_in        = arg[1]
    local portRange_in  = arg[2]
    local portRange     = {}
    local portList      = {}
    local mstpdConfig   = {}
    local netConfig     = {}

    local errorCode = 0;

    if (cmd_in == nil) then
        io.write("error: >> Command is wrong")
        errorCode = 1
    end

    if  portRange_in == nil then
        io.write("error: >> port range is wrong")
        errorCode = 1
    else
        portRange = checkPortRange(portRange_in)
        if portRange == nil then
            io.write("error: >> Port range is wrong\n")
            errorCode = 1
        end
    end

    if errorCode == 0 then
        netConfig = collectJsonTable(stp_cmdList.get_netConfig)
        if  netConfig == nil then
            io.write("error: >> Net config is empty\n")
            errorCode = 1
        end

        mstpdConfig = collectJsonTable(stp_cmdList.get_mstpdConfig)
        if mstpdConfig == nil then
            io.write("error: >> MSTPD config is empty\n")
            errorCode = 1
        end

        portList = getPortList(netConfig[values_t.values_e], values_t.switch_e, values_t.switch_ports_e)
        if  portList == nil then
            io.write("error: >> Port in switch device List is empty\n")
            errorCode = 1
        end
    end

    if errorCode == 0 then
        if cmd_in == "bridge" then
            printStpConfigGlobal(mstpdConfig[values_t.values_e])
        elseif cmd_in == "ports" then
            printStpConfigPort(portList, portRange, mstpdConfig[values_t.values_e])
        else
            io.write("error: >> Command not valid")
        end
    end
end

stp_main()