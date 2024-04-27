//
// Created by sheverdin on 10/3/23.
//

#include "dm_mainHeader.h"
#include "run_CLIcmd.h"
#include "parsing.h"
#include <ctype.h>
#include <string.h>

#define COMMAND_LEN (24)

const char *delimSpace  = " ";
const char *delim1 = "/";
const char *delimDot = ".";
const char *delimDoubleDot = ":";
const char *delimEqual = "=";

const char CommandTable[CMD_MAX][COMMAND_LEN] =
{
    {"ifconfig"},
    {"ubus call system info"},
    {"ubus call system board"},
    {"uci show system"},
    {"ip r"}
};

/// ===============================================================

int getNETinfo(search_out_msg_t *searchOutMsg)
{
    char find_interface[] = "switch.1";
    char find_MAC[] = "HWaddr";
    char find_ip[] = "inet addr";
    char output[OUTPUT_MAX_LENGTH];
    uint8_t interfaceFlag = 0;
    uint8_t findMacFlag   = 0;
    uint8_t findIpFlag    = 0;

    FILE *pipe = openPipe(CommandTable[CMD_NET_INFO]);
    while (fgets(output, OUTPUT_MAX_LENGTH, pipe) != NULL)
    {
        if (strstr(output, find_interface) != NULL)
        {
            interfaceFlag = 1;
        }
        if (strlen(output) == 1)
        {
            interfaceFlag = 2;
        }

        if(interfaceFlag == 1)
        {
            if (strstr(output, find_MAC) != NULL)
            {
                findMacFlag = 1;
                splited_line_t splitLineMAC;
                split_line(output, delimSpace, &splitLineMAC);

                split_line(splitLineMAC.tokens[4], delimDoubleDot, &splitLineMAC);
                for (int i = 0; i<MAC_LEN; i++)
                {
                    searchOutMsg->struct1.mac[i]=0;
                    searchOutMsg->struct1.mac[i] = strtol(splitLineMAC.tokens[i], NULL, 16);
                }
            }
            else if(strstr(output, find_ip) != NULL)
            {
                findIpFlag = 1;
                splited_line_t splitLineNET;
                splited_line_t splitLineIP;
                splited_line_t splitLineMask;

                replaceSymbols(output, ':', ' ');
                split_line(output, delimSpace, &splitLineNET);
                split_line(splitLineNET.tokens[2], delimDot, &splitLineIP);
                split_line(splitLineNET.tokens[6], delimDot, &splitLineMask);

                for (int i = 0; i<IPV4_LEN; i++)
                {
                    searchOutMsg->struct1.ip[i]   = 0;
                    searchOutMsg->struct1.mask[i] = 0;
                    searchOutMsg->struct1.ip[i]   = strtol(splitLineIP.tokens[i], NULL, 10);
                    searchOutMsg->struct1.mask[i] = strtol(splitLineMask.tokens[i], NULL, 10);
                }
            }
        }
    }
    if (!interfaceFlag)
    {
        openlog("dm_err", LOG_PID, LOG_USER);
        syslog(LOG_ERR, "Interface not found");
        closelog();
    }
    if (!findMacFlag)
    {
        openlog("dm_err", LOG_PID , LOG_USER);
        syslog(LOG_ERR, "MAC address not found");
        closelog();
    }
    if (!findIpFlag)
    {
        openlog("dm_err", LOG_PID, LOG_USER);
        syslog(LOG_ERR, "IP address not found");
        closelog();
    }
    interfaceFlag   = 0;
    findIpFlag      = 0;
    findMacFlag     = 0;
    closePipe(pipe);
    return 0;
}

int getSystemInfo(search_out_msg_t *searchOutMsg)
{
    splited_line_t splitLineSystem;

    char uptime[] = "uptime";
    char output[OUTPUT_MAX_LENGTH];
    uint8_t  uptimeFlag = 0;
    FILE *pipe = openPipe(CommandTable[CMD_SYS_INFO]);
    while (fgets(output, OUTPUT_MAX_LENGTH, pipe))
    {
        if (strstr(output, uptime) != NULL)
        {
            uptimeFlag = 1;
            replaceSymbols(output, ',', ' ');
            split_line(output, delimSpace, &splitLineSystem);
            uint32_t  d_uptime = strtol(splitLineSystem.tokens[1], NULL, 10);
            searchOutMsg->struct1.uptime[0] = (d_uptime)&(0xFF);
            searchOutMsg->struct1.uptime[1] = (d_uptime >> 8*1)&(0xFF);
            searchOutMsg->struct1.uptime[2] = (d_uptime >> 8*2)&(0xFF);
            searchOutMsg->struct1.uptime[3] = (d_uptime >> 8*3)&(0xFF);
        }
    }
    if(!uptimeFlag)
    {
        openlog("dm_err", LOG_PID, LOG_USER);
        syslog(LOG_ERR, "Uptime not found");
        closelog();
    }
    closePipe(pipe);
    return EXIT_SUCCESS;
}

int getBoardInfo(search_out_msg_t *searchOutMsg)
{
    char output[OUTPUT_MAX_LENGTH];
    char model[]        = "model";
    char version[]      = "version";
    uint8_t modelFlag   = 0;
    uint8_t versionFlag = 0;
    splited_line_t splitLineBoard;

    FILE *pipe = openPipe(CommandTable[CMD_BOARD_INFO]);
    while (fgets(output, OUTPUT_MAX_LENGTH, pipe))
    {
        if (strstr(output, model) != NULL)
        {
            char deviceType[16];
            modelFlag = 1;
            replaceSymbols(output, '\"', ' ');
            split_line(output, delimSpace, &splitLineBoard);
            size_t len = strlen(splitLineBoard.tokens[4]);
            strncpy(deviceType, splitLineBoard.tokens[4], len);
            deviceType[len] = '\0';
            // TODO Konst add all devices to the table;
            getDeviceType(searchOutMsg,deviceType);
        }
        else if (strstr(output, version) != NULL)
        {
            char versionStr[] = "12.34.56";
            char versionCat[6] = "";
            versionFlag = 1;
            uint32_t version_num = 0;
            //// TODO Konst: In this time this paert is sendBox. version number is const
            split_line(versionStr, delimDot, &splitLineBoard);
            searchOutMsg->struct1.firmware[2] = strtol(splitLineBoard.tokens[0], NULL, 10);
            searchOutMsg->struct1.firmware[1] = strtol(splitLineBoard.tokens[1], NULL, 10);
            searchOutMsg->struct1.firmware[0] = strtol(splitLineBoard.tokens[2], NULL, 10);
        }
    }
    if(!modelFlag)
    {
        openlog("dm_sys", LOG_PID, LOG_USER);
        syslog(LOG_ERR, "Model not found");
        closelog();
    }
    if(!versionFlag)
    {
        openlog("dm_sys", LOG_PID, LOG_USER);
        syslog(LOG_ERR, "Version not found");
        closelog();
    }

    closePipe(pipe);
    return 0;
}

int getUCInfo(search_out_msg_t *searchOutMsg)
{
    char output[OUTPUT_MAX_LENGTH];
    char description[]      = "description";
    char location[]         = "location";
    uint8_t locationFlag    = 0;
    uint8_t descriptionFlag = 0;
    splited_line_t splitLineUCI;

    FILE *pipe = openPipe(CommandTable[CMD_UCI_SYS]);
    while (fgets(output, OUTPUT_MAX_LENGTH, pipe))
    {
        if (strstr(output, location) != NULL)
        {
            locationFlag = 1;
            split_line(output, delimEqual, &splitLineUCI);
            splitLineUCI.tokens[1][strcspn(splitLineUCI.tokens[1], "\n")] = '\0';
            removeCharacter(splitLineUCI.tokens[1],'\'');
            for (int i = 0; i< MAX_LENGTH; i++ )
            {
                searchOutMsg->struct1.dev_loc[i] = '\0';
            }
            strncpy(searchOutMsg->struct1.dev_loc, splitLineUCI.tokens[1], strlen(splitLineUCI.tokens[1]));
        }
        else if (strstr(output, description) != NULL)
        {
            descriptionFlag = 1;
            split_line(output, delimEqual, &splitLineUCI);
            splitLineUCI.tokens[1][strcspn(splitLineUCI.tokens[1], "\n")] = '\0';
            removeCharacter(splitLineUCI.tokens[1],'\'');
            for (int i = 0; i< MAX_LENGTH; i++ )
            {
                searchOutMsg->struct1.dev_descr[i] = '\0';
            }
            strncpy(searchOutMsg->struct1.dev_descr, splitLineUCI.tokens[1],strlen(splitLineUCI.tokens[1]));
        }
    }
    if(!locationFlag)
    {
        openlog("dm_err", LOG_PID | LOG_PERROR, LOG_USER);
        syslog(LOG_ERR, "Location not found");
        closelog();
    }
    if(!descriptionFlag)
    {
        openlog("dm_err", LOG_PID | LOG_PERROR, LOG_USER);
        syslog(LOG_ERR, "Description not found");
        closelog();
    }
    closePipe(pipe);
    return 0;
}

int getGateway(search_out_msg_t *searchOutMsg)
{
    char gateway[]      = "default";
    uint8_t gatewayFlag = 0;
    char output[OUTPUT_MAX_LENGTH];
    splited_line_t splitLineGateway;
    splited_line_t splitLineIP;
    FILE *pipe = openPipe(CommandTable[CMD_GATE]);
    while (fgets(output, OUTPUT_MAX_LENGTH, pipe))
    {
        if (strstr(output, gateway) != NULL)
        {
            gatewayFlag = 1;
            split_line(output, delimSpace, &splitLineGateway);
            split_line(splitLineGateway.tokens[2], delimDot, &splitLineIP);

            for (int i = 0; i<IPV4_LEN; i++)
            {
                searchOutMsg->struct1.gate[i] = strtol(splitLineIP.tokens[i], NULL, 10);
            }
        }
    }
    if (!gatewayFlag)
    {
        openlog("dm_err", LOG_PID, LOG_USER);
        syslog(LOG_ERR, "gateway not found");
        closelog();
        for (int i = 0; i<IPV4_LEN; i++)
        {
            searchOutMsg->struct1.gate[i] = 0xFF;
        }
    }
    closePipe(pipe);
    return 0;
}

FILE *openPipe(const char *cmdStr)
{
    FILE *pipe = popen(cmdStr, "r");
    if (!pipe)
    {
        openlog("dm_err", LOG_PID | LOG_PERROR, LOG_USER);
        syslog(LOG_ERR, "Error to run command %s", cmdStr);
        closelog();
    }
    return pipe;
}

void closePipe(FILE *pipe)
{
    int status = pclose(pipe);

    if (!status)
    {
        return;
    }
    else
    {
        openlog("dm_err", LOG_PID | LOG_PERROR, LOG_USER);
        syslog(LOG_ERR, "Error close pipe, Status =  %d", status);
        closelog();
    }
}


