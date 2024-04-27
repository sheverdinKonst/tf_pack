
dofile("/etc/tfortis_klish/scripts/lua/lldpd/lldp_utils.lua")

function parsingChassis(chassis_s)

    for i, struct in pairs(chassis_s[chassis_e.name])  do
        print("\t" .. chassis_e.name .. ":\t\t" .. tostring(struct["value"]) )
    end

    local id_type  = chassis_e.id_t.type
    local id_value  = chassis_e.id_t.value

    for i, struct in pairs(chassis_s[chassis_e.id])  do
        print("\tid: " .. "\t\t" .. struct[id_type]  .. "\t" .. tostring(struct[id_value]))
    end

    for i, struct in pairs(chassis_s[chassis_e.descr])  do
        print("\tdescription: " .. "\t" ..  tostring(struct["value"]))
    end

    local capability_type       = chassis_e.capability_t.type
    local capability_enabled    = chassis_e.capability_t.enabled
    print("\t" .. chassis_e.capability .. ":")
    for i, struct in pairs(chassis_s[chassis_e.capability])  do
        if (struct[capability_enabled] == true) then
            print("\t\t" .. struct[capability_type])
        end
    end

    print("\tmgmt ip: ")
    for i, struct in pairs(chassis_s[chassis_e.mgmt_ip])  do
        print("\t\t" ..  tostring(struct["value"]))
    end

    print("\tmgmt iface: ")
    for i, struct in pairs(chassis_s[chassis_e.mgmt_iface])  do
        print("\t\t" ..  tostring(struct["value"]))
    end
end
