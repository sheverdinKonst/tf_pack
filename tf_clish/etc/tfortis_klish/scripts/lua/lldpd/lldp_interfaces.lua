
dofile("/etc/tfortis_klish/scripts/lua/lldpd/lldp_chassis.lua")
dofile("/etc/tfortis_klish/scripts/lua/lldpd/lldp_utils.lua")


local i_portCount = 0
local interface_port_e = {
    id      = "id",
    descr   = "descr",
    id_t    = id_e
}

local lldp_interface_e = {
    name    = "name",
    status  = "status",
    chassis = "chassis",
    port    = "port",
    ttl     = "ttl",
    port_t  = interface_port_e
}

function parsingInterfaces(lldp_interface, ports)
    --print("---> interfaces")

    local isPrint = 0
    isPrint = needToPrint(ports, lldp_interface[lldp_interface_e.name])
    if isPrint == 1 then
        print(" ---------------------------------- ")
        i_portCount = i_portCount + 1
        print(lldp_interface_e.name .. ":   \t" ..  lldp_interface[lldp_interface_e.name])

        for _, interface in pairs(lldp_interface[lldp_interface_e.status]) do
            print(lldp_interface_e.status .. ": \t" .. interface["value"])
        end

        for _, interface in pairs(lldp_interface[lldp_interface_e.chassis]) do
            print("chassis: ")
            parsingChassis(interface)
        end

        print("port: ")
        for _, interface_port in pairs(lldp_interface[lldp_interface_e.port]) do
            if interface_port ~= nil then

                local id_type   = lldp_interface_e.port_t.id_t.type
                local id_value  = lldp_interface_e.port_t.id_t.value
                local id = lldp_interface_e.port_t.id
                local descr = lldp_interface_e.port_t.descr
                if (interface_port[id] ~= nil) then
                    for _, port_v in pairs(interface_port[id])  do
                        print("\tid:\t\t" .. port_v[id_type]  .. "\t" .. port_v[id_value])
                    end
                end

                if (interface_port[descr] ~= nil) then
                    for _, descr_v in pairs(interface_port[descr])  do
                        print("\tdescription: " .. "\t" .. descr_v["value"] )
                    end
                end
            end
        end

        for key, ttl_v in pairs(lldp_interface[lldp_interface_e.ttl]) do
            print(lldp_interface_e.ttl  .. ":\t" ..  ttl_v[lldp_interface_e.ttl])
        end

    end
    return i_portCount
end