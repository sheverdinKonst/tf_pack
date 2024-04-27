//
// Created by sheverdin on 12/28/23.
//

#include "parsing_command.h"

const char cmdTable[max_cmd][COMMAND_LENGTH] =
{
    {" "},
    {"status"},
    {"reboot"}
};

uint8_t getCmdType(char *cmd, CMD_TYPE_e *cmdType)
{
    uint8_t errorCode = 1;
    *cmdType = cmd_idle;
    //printf("cmd = %s\n", cmd);
    for (CMD_TYPE_e i = 0; i < max_cmd; i++)
    {
        if (strcmp(cmd, cmdTable[i]) == 0)
        {
            *cmdType = i;
            i = max_cmd;
            errorCode = 0;
        }
    }
    //printf("cmdStr = %s, cmdType = %d\n", cmd, *cmdType);
    return errorCode;
}

uint8_t getPortNum(char *portStr, uint8_t *portNum)
{
    uint8_t errorCode = 0;
    uint8_t intValue = 0;
    intValue = (uint8_t)(strtol(portStr, NULL, 10));
    if (intValue < 1 || intValue > 8)
    {
        errorCode   = 4;
        *portNum    = 0;
    } else {
        *portNum = intValue;
    }
    return errorCode;
}

void printBufferJson(resetPort_t *resetStatic)
{
    size_t lenErrorMsg = 0;
    printf("{\n");
    printf("\"port\":[\n ");

    for (int portNum = 1; portNum < NUM_PORTS; portNum++)
    {
        char errorMsg[256];
         strcpy(errorMsg, "");
        if(resetStatic[portNum].errorCode == ERR_OK) {
            resetStatic[portNum].errorCode = 0;
            strcpy(errorMsg, "OK");
        }
        if(resetStatic[portNum].errorCode != ERR_OK)
        {
            if(resetStatic[portNum].errorCode & ERR_MANUAL_REBOOT) {
                strcat(errorMsg, "manual rebooting... ");
            }
            if(resetStatic[portNum].errorCode & ERR_REBOOTING) {
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "rebooting... ");
            }
            if(resetStatic[portNum].errorCode & ERR_OBJ_NOT_FOUND) {
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: obj not found");
            }
            if(resetStatic[portNum].errorCode & ERR_UNAVAILABLE_RESOURCE) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: unavailable resource");
            }
            if(resetStatic[portNum].errorCode & ERR_NULL_OBJ) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: obj null");
            }
            if(resetStatic[portNum].errorCode & ERR_CREATE_THREAD) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: create thread");
            }
            if(resetStatic[portNum].errorCode & ERR_JOIN_THREAD) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: join thread");
            }
            if(resetStatic[portNum].errorCode & ERR_REG_COMPILE) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: regular expiration");
            }
            if(resetStatic[portNum].errorCode & ERR_TEST_TYPE) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: test type");
            }
            if(resetStatic[portNum].errorCode & ERR_IP_NOT_VALID) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: ip not valid");
            }
            if(resetStatic[portNum].errorCode & ERR_TIME_NOT_VALID) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: time not valid");
            }
            if(resetStatic[portNum].errorCode & ERR_SPEED_VALUE) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: speed not in range");
            }
            if(resetStatic[portNum].errorCode & ERR_PORT_DISABLE) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "port disable");
            }
            if(resetStatic[portNum].errorCode & ERR_TEST_DISABLE) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "test disable");
            }
            if(resetStatic[portNum].errorCode & ERR_TEST_LINK) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: test link");
            }
            if(resetStatic[portNum].errorCode & ERR_TEST_PING) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: test ping");
            }
            if(resetStatic[portNum].errorCode & ERR_TEST_SPEED) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: test speed");
            }
            if(resetStatic[portNum].errorCode & ERR_TIME_ALARM) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: time alarm");
            }

            if(resetStatic[portNum].errorCode & ERR_PORT_SHUTDOWN) {
                lenErrorMsg = strlen(errorMsg);
                if (lenErrorMsg)
                    strcat(errorMsg, ", ");
                strcat(errorMsg, "error: reboot disabled after max resets");
            }
        }
        if (portNum < (NUM_PORTS-1))
        {
            printf("{ \n"
                   "\t\"id\":\"%d\",\n"
                   "\t\"error_Code\":\"%d\",\n"
                   "\t\"status\":\"%s\",\n"
                   "\t\"reboot_cnt\":\"%d\",\n"
                   "\t\"time\":\"%s\",\n"
                   "\t\"test_type\":\"%d\""
                   "\n },"
                   "\n",
                   resetStatic[portNum].portNum, resetStatic[portNum].errorCode, errorMsg,
                   resetStatic[portNum].resetCount, resetStatic[portNum].timeStr, resetStatic[portNum].testType);
        }
        else
        {
            printf("{ \n"
                   "\t\"id\":\"%d\",\n"
                   "\t\"error_Code\":\"%d\",\n"
                   "\t\"status\":\"%s\",\n"
                   "\t\"reboot_cnt\":\"%d\",\n"
                   "\t\"time\":\"%s\",\n"
                   "\t\"test_type\":\"%d\""
                   "\n }"
                   "\n",
                   resetStatic[portNum].portNum, resetStatic[portNum].errorCode, errorMsg,
                   resetStatic[portNum].resetCount, resetStatic[portNum].timeStr,resetStatic[portNum].testType);
        }
    }
    printf("]\n }\n");
}

