local s_portCount = 0
statistics_e = {
    name                = "statistics",
    nameif              = "name",
    tx                  = "tx",
    rx                  = "rx",
    rx_discarded_cnt    = "rx_discarded_cnt",
    rx_unrecognized_cnt = "rx_unrecognized_cnt",
    ageout_cnt          = "ageout_cnt",
    insert_cnt          = "insert_cnt",
    delete_cnt          = "delete_cnt",
}

function parsingStatistics(lldp_statistics, ports)
    --print("---> Statistics")

    local isPrint = 0
    isPrint = needToPrint(ports, lldp_statistics[statistics_e.nameif])

    if isPrint == 1 then
        s_portCount = s_portCount + 1
        print("--------------------------------")
        print(statistics_e.nameif .. ":  \t\t\t" ..  lldp_statistics[statistics_e.nameif])

        for _, value in pairs(lldp_statistics[statistics_e.tx]) do
            print("TX" .. ":  \t\t\t" .. value[statistics_e.tx])
        end

        for _, value in pairs(lldp_statistics[statistics_e.rx]) do
            print("RX" .. ":  \t\t\t" .. value[statistics_e.rx])
        end

        for _, value in pairs(lldp_statistics[statistics_e.rx_discarded_cnt]) do
            print("  discarded cnt" .. ": \t" .. value[statistics_e.rx_discarded_cnt])
        end

        for _, value in pairs(lldp_statistics[statistics_e.rx_unrecognized_cnt]) do
            print("  unrecognized cnt" .. ": \t" .. value[statistics_e.rx_unrecognized_cnt])
        end

        print("Counts: ")
        for _, value in pairs(lldp_statistics[statistics_e.ageout_cnt]) do
            print("  ageout - " .. " \t\t" .. value[statistics_e.ageout_cnt])
        end

        for _, value in pairs(lldp_statistics[statistics_e.insert_cnt]) do
            print("  insert - " .. " \t\t" .. value[statistics_e.insert_cnt])
        end

        for _, value in pairs(lldp_statistics[statistics_e.delete_cnt]) do
            print("  delete - " .. " \t\t" .. value[statistics_e.delete_cnt])
        end
    end
    return s_portCount
end