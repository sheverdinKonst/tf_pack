#!/usr/bin/lua

dofile("/etc/tfortis_klish/scripts/lua/global_function.lua")

local cmd_in    = ""
local portRange = {}

local stp_cmdList = {
    stp_status = "ubus call mstpd getStatus \'{\"bridge\":\"switch\"}\'"
}

local mstpdParam = {
    bridges = {
        {"bridge",                      "\tBridge name:                "},      --  1
        {"enabled",                     "\tBridge enabled:             "},      --  2
        {"bridge-id",                   "\tBridge ID:                  "},      --  3
        {"root-port",                   "\tRoot port:                  "},      --  4
        {"designated-root",             "\tDesignated root:            "},      --  5
        {"regional-root",               "\tRegional root:              "},      --  6
        {"topology-change-port",        "\tTopology change port:       "},      --  7
        {"last-topology-change-port",   "\tLast topology change port:  "},      --  8
        {"topology-change",             "\tTopology change:            "},      --  9
        {"topology-change-count",       "\tTopology change count:      "},      --  10
        {"time-since-topology-change",  "\tTime since topology change: "},      --  11
        {"bridge-max-age",              "\tBridge max age:             "},      --  12
        {"hello-time",                  "\tHello time:                 "},      --  13
        {"tx-hold-count",               "\tTX hold count:              "},      --  14
        {"path-cost",                   "\tPath cost:                  "},      --  15
        {"internal-path-cost",          "\tInternal path cost:         "},      --  16
        {"max-hops",                    "\tMax hops:                   "},      --  17
        {"max-age",                     "\tMax age:                    "},      --  20
        {"ageing-time",                 "\tAgeing time:                "},      --  18
        {"forward-delay",               "\tForward delay:              "},      --  19
        {"bridge-forward-delay",        "\tBridge forward delay:       "},      --  21
        {"force-protocol-version",      "\tForce protocol version:     "},      --  22
        {"ports",                       "Ports:                        "}       --  23
    },

    br_port  =
    {
        {"port",                    "Port:  "},                     --  1
        {"role",                    "\tRole:                 "},    --  2
        {"state",                   "\tState:                "},    --  3
        {"bridge",                  "\tBridge:               "},    --  4
        {"port-id",                 "\tID:                   "},    --  5
        {"designated-bridge",       "\tDesignated bridge:    "},    --  6
        {"network-port",            "\tNetwork port:         "},    --  7
        {"enabled",                 "\tPort enabled:         "},    --  8
        {"designated-port",         "\tDesignated-port:      "},    --  9
        {"designated-root",         "\tDesignated root:      "},    --  10
        {"port-hello-time",         "\tHello time:           "},    --  11
        {"bpdu-filter-port",        "\tBPDU filter port:     "},    --  12
        {"bpdu-guard-port",         "\tBPDU guard port:      "},    --  13
        {"bpdu-guard-error",        "\tBPDU guard error:     "},    --  14
        {"received-bpdu",           "\tReceived BPDU:        "},    --  15
        {"received-tcn",            "\tReceived TCN:         "},    --  16
        {"num-rx-bpdu",             "\tNum RX BPDU:          "},    --  17
        {"num-rx-tcn",              "\tNum RX TCN:           "},    --  20
        {"num-rx-bpdu-filtered",    "\tNum RX BPDU filtered: "},    --  18
        {"num-tx-bpdu",             "\tNum TX BPDU:          "},    --  19
        {"send-rstp",               "\tSend RSTP:            "},    --  21
        {"received-rstp",           "\tReceived RSTP:        "},    --  22
        {"received-stp",            "\tReceived STP:         "},    --  23
        {"dsgn-regional-root",      "\tDSGN regional root:   "},    --  24
        {"dsgn-external-cost",      "\tDSGN external cost:   "},    --  25
        {"dsgn-internal-cost",      "\tDSGN internal cost:   "},    --  26
        {"admin-edge-port",         "\tAdmin edge port:      "},    --  27
        {"admin-internal-cost",     "\tAdmin internal cost:  "},    --  28
        {"admin-external-cost",     "\tAdmin external cost:  "},    --  29
        {"point-to-point",          "\tPoint to point:       "},    --  30
        {"restricted-TCN",          "\tRestricted TCN;       "},    --  31
        {"ba-inconsistent",         "\tBA inconsistent:      "},    --  32
        {"oper-edge-port",          "\tOper edge port:       "},    --  33
        {"topology-change-ack",     "\tTopology change ack:  "},    --  34
        {"external-port-cost",      "\tExternal port cost;   "},    --  35
        {"internal-port-cost",      "\tInternal port cost:   "},    --  36
        {"num-transition-fwd",      "\tNum transition fwd:   "},    --  37
        {"received-tc-ack",         "\tReceived tc ack:      "},    --  38
        {"admin-point-to-point",    "\tAdmin point to point: "},    --  39
        {"disputed",                "\tDisputed:             "},    --  40
        {"num-transition-blk",      "\tNum transition blk:   "},    --  41
        {"num-tx-tcn",              "\tNum TX TCN:           "},    --  42
        {"auto-edge-port",          "\tAuto edge port:       "},    --  43
        {"restricted-role",         "\tRestricted role:      "},    --  44
    }
}

function printBridge(br)
    for _, value in pairs(mstpdParam.bridges) do
        if (br[value[1]] ~= nil) then
            if  (value[1] ~= "ports") then
                io.write(value[2], "\t", br[value[1]], "\n")
            end
        end
    end
end

function printPorts(ports)
    local isPrint = 0
    for k, port in pairs(ports) do
        isPrint = needToPrint(portRange, port["port"])
        if isPrint == 1 then
            for _, value in pairs(mstpdParam.br_port)  do
                if port[value[1]] == nil then
                    print("no data for this port")
                else
                    if value[1] == "port" then
                        io.write(value[2], "*", port[value[1]], "*    ---------------------------------------\n")
                    else
                        io.write(value[2], "\t", port[value[1]], "\n")
                    end
                end
            end
        end
    end
end

function printStpStatus(stp_bridges, cmd)
    for k, bridgeArr in pairs(stp_bridges.bridges) do
        if cmd == "bridge" then
            io.write("Bridge status:", "\n")
            printBridge(bridgeArr)
        elseif (cmd == "ports") then
            io.write("Ports status:", "\n")
            printPorts(bridgeArr.ports)
        end
    end
end

function stp_main()
    local cmd_in         = arg[1]
    local portRange_in   = arg[2]
    portRange = checkPortRange(portRange_in)

    if (cmd_in == nil) then
        print("error: >> Command is wrong")
    elseif  portRange_in == nil then
        print("error: >> port range is wrong")
    else
        local stp_status = collectJsonTable(stp_cmdList.stp_status)
        if (stp_status == nil) then
            print("error: >> no data to present STP information")
        else
            if cmd_in == "bridge" then
                printStpStatus(stp_status, "bridge")
            elseif cmd_in == "ports" then
                printStpStatus(stp_status, "ports")
            else
                print("error: >> Command not valid")
            end
        end
    end
end

stp_main()
