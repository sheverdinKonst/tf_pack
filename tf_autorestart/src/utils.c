//
// Created by sheverdin on 12/8/23.
//

#include "utils.h"
#include <regex.h>
#include <stdbool.h>

uint8_t globalDebugMode = 0;

const char paramTable[MAX_CMD][WORD_LENGTH] =
{
    {"state"},      // 0
    {"host"},       // 1
    {"min_speed"},  // 2
    {"max_speed"},  // 3
    {"time_up"},    // 4
    {"time_down"},  // 5
};

const char testTable[test_max][WORD_LENGTH] =
{
    {"disable\n"},    //  0
    {"link\n"},       //  1
    {"ping\n"},       //  2
    {"speed\n"},      //  3
    {"time\n"},       //  4
};

const char monthTable[13][WORD_LENGTH] =
    {
            {"\n"},             //  0
            {"January\0"},      //  1
            {"February\0"},     //  2
            {"March\0"},        //  3
            {"April\0"},        //  4
            {"May\0"},          //  5
            {"June\0"},         //  6
            {"Julie\0"},        //  7
            {"August\n0"},       //  8
            {"September\0"},    //  9
            {"October\0"},      //  10
            {"November\0"},     //  11
            {"December\0"},     //  12
    };

char decimalNum[NUM_PORTS][2] = {"0", "1", "2", "3", "4", "5", "6", "7", "8"};

TYPE_OF_TEST_t getCmdKey(const char * str)
{
    TYPE_OF_TEST_t cmdKey = test_max;
    for (int i = 0; i < test_max; i++)
    {
        if (strcmp(str, testTable[i]) == 0)
        {
            if (i == 0)
            {
                cmdKey = test_disable;
                i = test_max;
            } else if (i == 1)
            {
                cmdKey = test_link;
                i = test_max;
            } else if (i == 2)
            {
                cmdKey = test_ping;
                i = test_max;
            } else if (i == 3)
            {
                cmdKey = test_speed;
                i = test_max;
            } else if (i == 4)
            {
                cmdKey = test_time;
                i = test_max;
            }
        }
    }
    return cmdKey;
}

FILE *openPipe(const char *cmdStr)
{
    FILE *pipe = popen(cmdStr, "r");
    if (!pipe) {
        openlog("ar_err", LOG_PID | LOG_PERROR, LOG_USER);
        syslog(LOG_ERR, "Error to run command %s", cmdStr);
        closelog();
    }
    return pipe;
}

void closePipe(FILE *pipe, char *str)
{
    int status = pclose(pipe);

    if (!status) {
        return;
    } else{
        //printf("error str = %s\n", str);
        openlog("ar_err", LOG_PID | LOG_PERROR, LOG_USER);
        syslog(LOG_ERR, "Error close pipe, Status =  %d", status);
        syslog(LOG_ERR, "lError close pipe, cmd  %s, status %d\n", str, status);
        closelog();
    }
}

void intToString(int num, char str[])
{
    //str = decimalNum[num];
    strcpy(str, decimalNum[num]);
}

void checkValidIp(char ipStr[], int *ipIsValid)
{
    char ip[16];
    int  flag = ERR_OK;
    char pattern[] = "^([0-9]{1,3}\\.){3}[0-9]{1,3}$";
    regex_t regex;
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        flag = ERR_REG_COMPILE;
    } else {
        int result = regexec(&regex, ipStr, 0, NULL, 0);
        flag = (result == 0) ? ERR_OK : ERR_IP_NOT_VALID;
        regfree(&regex);
    }
    *ipIsValid = flag;
}

error_code_t isValidTime(const char* timeString, struct tm * time) {
    int hour   = time->tm_hour;
    int minute = time->tm_min;

    if (sscanf(timeString, "%d:%d", &hour, &minute) != 2) {
        return ERR_TIME_NOT_VALID;
    }

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        time->tm_hour   = 0;
        time->tm_min    = 0;
        return ERR_TIME_NOT_VALID;
    }

    time->tm_hour = hour;
    time->tm_min = minute;

    return ERR_OK;
}

int getTimeDate(char timeDateStr[32]) {
    time_t rawtime;
    struct tm* timeinfo;
    char dayStr[5]       = "";
    char yearStr[5]      = "";
    char timeStr[11]     = "";
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf(timeStr, "%02d:%02d:%02d, ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    strcat(timeDateStr, timeStr);
    sprintf(dayStr, "%d-", timeinfo->tm_mday);
    strcat(timeDateStr,dayStr);
    strcat(timeDateStr,monthTable[timeinfo->tm_mon + 1]);
    sprintf(yearStr, "-%d",  timeinfo->tm_year + 1900);
    strcat(timeDateStr,yearStr);
    return 0;
}

void setTestMode(char testMode)
{
    printf("testMode = %c\n", testMode);
    if (testMode == 'd'){
        globalDebugMode = 1;
    }
}

uint8_t isDebugMode(void)
{
    return globalDebugMode;
}











