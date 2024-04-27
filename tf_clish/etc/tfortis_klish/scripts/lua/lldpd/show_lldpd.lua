#!/usr/bin/lua


dofile("/etc/tfortis_klish/scripts/lua/global_function.lua")
dofile("/etc/tfortis_klish/scripts/lua/lldpd/lldp-neighbors.lua")
dofile("/etc/tfortis_klish/scripts/lua/lldpd/lldp_statistics.lua")
dofile("/etc/tfortis_klish/scripts/lua/lldpd/lldp_chassis.lua")
dofile("/etc/tfortis_klish/scripts/lua/lldpd/lldp_interfaces.lua")
-- dofile("/etc/tfortis_klish/scripts/lua/lldpd/lldp_utils.lua")

--print("*************** show_lldpd ver.-0.1 ***************")

local portRange = {0,0}
local lldpd_Status = {}

local interfaces_e = {}

local lldp_mian = {
    interfaces = interfaces_e,
    statistics = statistics_e,
    neighbors  = neighbors_e,
    chassis    = chassis_e
}

local cmdList = {
    lldpd_Status = "ubus call lldpd getStatus"
}

function Interfaces()
    print(" > > > Interfaces: ")
    local i_portCount = 0
    for key, lldp_1_interface in pairs(lldpd_Status.interfaces.lldp[1].interface) do
        if (lldp_1_interface ~= nil) then
            i_portCount = parsingInterfaces(lldp_1_interface, portRange)
        end
    end
    if i_portCount == 0 then
        print("According to your conditions, no Interfaces have been found")
    end
end

function Statistics()
    print(" > > Statistics: ")
    local s_portCount = 0
    for key, lldp_1_interface in pairs(lldpd_Status.statistics.lldp[1].interface) do
        if (lldp_1_interface ~= nil) then
            s_portCount = parsingStatistics(lldp_1_interface, portRange)
        end
    end
    if s_portCount == 0 then
        print("According to your conditions, Statistics are not presented")
    end
end

function Neighbors()
    print(" > > Neighbors: ")
    local n_portCount = 0
    for key, lldp_interface in pairs(lldpd_Status.neighbors.lldp[1].interface) do
        if (lldp_interface ~= nil) then
            n_portCount = parsingNeighbors(lldp_interface, portRange)
        end
    end

    if n_portCount == 0 then
        print("According to your conditions, no Neighbors have been found")
    end
end

function Chassis(ports)
    print("> > > Chassis")
    for key, local_chassis_1_chassis in pairs(lldpd_Status.chassis["local-chassis"][1].chassis) do
        if (local_chassis_1_chassis ~= nil) then
            parsingChassis(local_chassis_1_chassis)
        end
    end
end

function main_show_lldp()
    local portRange_in    = arg[1]
    local cmd_in = ""
    if (portRange_in ~= nil) then
        cmd_in          = arg[2]
    else
        cmd_in          = arg[1]
    end

    if (cmd_in == nil) then
        print("Command is wrong")
    end

    portRange = checkPortRange(portRange_in)

    if (portRange ~=nil) then
        lldpd_Status = collectJsonTable(cmdList.lldpd_Status)
        if cmd_in == "local" then
            Chassis(portRange)
        elseif cmd_in == "remote" then
            Neighbors(portRange)
        elseif cmd_in == "statistics" then
            Statistics(portRange)
        elseif cmd_in == "interfaces" then
            Interfaces()
        else
            print("\t >>> Command is wrong <<<")
        end
    else
        print("error: >> port range is wrong <<")
    end
end

main_show_lldp()



