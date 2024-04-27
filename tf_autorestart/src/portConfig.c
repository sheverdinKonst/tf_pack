//
// Created by sheverdin on 12/8/23.
//

#include "portConfig.h"
#include "utils.h"
#include "ping.h"

void *pingArgs[3];

REBOOT_PARAM_t rebootParam[NUM_PORTS];
static  error_code_t timeAlarm(time_h_m *timeAlarmInfo);

static REBOOT_STATE_e autoResetHandler(portInfo_t *portInfo, uint8_t maxReset);
static REBOOT_STATE_e manualResetHandler(portInfo_t *portInfo);
static REBOOT_EVENT_e checkTimePoeUP(uint32_t timeStartReboot);


void port_setDisableParam(paramPort_t *paramPort)
{
    //paramPort->min_speed        = 0;
    //paramPort->max_speed        = 0;
    //paramPort->timeUp.time_H    = 0;
    //paramPort->timeUp.time_M    = 0;
    //paramPort->timeDown.time_H  = 0;
    //paramPort->timeDown.time_M  = 0;
}


void port_setLinkParam(paramPort_t *paramPort)
{
    if (isDebugMode())
    {
        printf("set port Link Param -- port = %d\n", paramPort->portNum);
    }
    paramPort->errorCode = ERR_OK;
}

void port_setHostParam(paramPort_t *paramPort)
{
    int ipIsValid               = ERR_IP_NOT_VALID;
    paramPort->test             = test_ping;
    paramPort->min_speed        = 0;
    paramPort->max_speed        = 0;

    char output[OUTPUT_MAX_LENGTH];
    FILE *pipeHostName          = NULL;

    char cmd_getHostName[64]   = "";
    char getHostNamePrefix[] = "uci get tf_autorestart.lan";
    char getHostNameSuffix[]  = ".host";

    char portStr[2] = " ";
    intToString(paramPort->portNum, portStr);

    strcpy(cmd_getHostName, getHostNamePrefix);
    strcat(cmd_getHostName, portStr);
    strcat(cmd_getHostName, getHostNameSuffix);
    pipeHostName = openPipe(cmd_getHostName);

    fgets(output, OUTPUT_MAX_LENGTH, pipeHostName);

    closePipe(pipeHostName, cmd_getHostName);

    for (int i = 0; i < 16; i++){
        if (output[i] != '\n')
        {
            paramPort->host[i] = output[i];
        }
        else
            break;
    }
    if (isDebugMode())
    {
        printf("set port Host Param -- port = %d\n", paramPort->portNum);
        printf("Host name = %s", output);
        printf("strlen ip = %lu\n", strlen(paramPort->host));
    }

    checkValidIp(paramPort->host, &ipIsValid);
    paramPort->errorCode |=  ipIsValid;
}

void port_setSpeedParam(paramPort_t *paramPort)
{
    //printf("set port Speed Param -- port = %d\n", paramPort->portNum);
    paramPort->min_speed        = 0;
    paramPort->max_speed        = 0;
    paramPort->rx_delta_byte    = 0;
    paramPort->rx_byte_current  = 0;
    paramPort->rx_byte_prev     = 0;
    paramPort->time_current     = 0;
    paramPort->time_prev        = time (NULL);

    char min_speedStr[OUTPUT_MAX_LENGTH];
    char max_speedStr[OUTPUT_MAX_LENGTH];

    FILE *pipeSpeed                 = NULL;
    char cmd_getSpeed[64]           = "";
    char getHostNamePrefix[]        = "uci get tf_autorestart.lan";
    char getHostNameSuffixMin[]     = ".min_speed";
    char getHostNameSuffixMax[]     = ".max_speed";

    char portStr[2] = " ";
    intToString(paramPort->portNum, portStr);

    strcpy(cmd_getSpeed, getHostNamePrefix);
    strcat(cmd_getSpeed, portStr);
    strcat(cmd_getSpeed, getHostNameSuffixMin);
    //printf("cmd_get min Speed = %s\n", cmd_getSpeed);
    pipeSpeed = openPipe(cmd_getSpeed);
    fgets(min_speedStr, OUTPUT_MAX_LENGTH, pipeSpeed);
    //printf("min_speed = %s\n", min_speedStr);
    closePipe(pipeSpeed, cmd_getSpeed);

    strcpy(cmd_getSpeed, "");
    strcpy(cmd_getSpeed, getHostNamePrefix);
    strcat(cmd_getSpeed, portStr);
    strcat(cmd_getSpeed, getHostNameSuffixMax);
    //printf("cmd_get max Speed = %s\n", cmd_getSpeed);

    pipeSpeed = openPipe(cmd_getSpeed);

    fgets(max_speedStr, OUTPUT_MAX_LENGTH, pipeSpeed);
    //printf("max_speed = %s", max_speedStr);
    closePipe(pipeSpeed, cmd_getSpeed);

    paramPort->max_speed = (uint32_t)(strtol(max_speedStr, NULL, 10));
    paramPort->min_speed = (uint32_t)(strtol(min_speedStr, NULL, 10));
    //printf("Num -> max_speed = %d -- min_speed = %d\n",paramPort->max_speed, paramPort->min_speed);

    if (paramPort->max_speed != 0 && paramPort->max_speed > MAX_SPEED){
         paramPort->errorCode |= ERR_SPEED_VALUE;
    }
    if(paramPort->min_speed != 0 && paramPort->max_speed < MIN_SPEED){
       paramPort->errorCode |= ERR_SPEED_VALUE;
    }
}

void port_setTimeParam(paramPort_t *paramPort)
{
    struct tm timeInfo;
    struct tm *timeInfoPtr;
    time_t now = time(NULL);

    paramPort->min_speed        = 0;
    paramPort->max_speed        = 0;
    paramPort->rx_delta_byte    = 0;
    paramPort->rx_byte_current  = 0;
    paramPort->rx_byte_prev     = 0;
    paramPort->time_current     = 0;
    paramPort->time_prev        = time (NULL);

    char timeUpStr[OUTPUT_MAX_LENGTH];
    char timeDownStr[OUTPUT_MAX_LENGTH];

    FILE *pipeTime              = NULL;
    char cmd_getTime[64]        = "";
    char getTimePrefix[]        = "uci get tf_autorestart.lan";
    char getTimeUpSuffixMin[]   = ".timeUp";
    char getTimeDownSuffix[]    = ".timeDown";

    char portStr[2] = " ";
    intToString(paramPort->portNum, portStr);

    strcpy(cmd_getTime, getTimePrefix);
    strcat(cmd_getTime, portStr);
    strcat(cmd_getTime, getTimeUpSuffixMin);
    //printf("Time up cmd = %s\n", cmd_getTime);
    pipeTime = openPipe(cmd_getTime);
    fgets(timeUpStr, OUTPUT_MAX_LENGTH, pipeTime);
    closePipe(pipeTime, cmd_getTime);
    //printf("Time Up  = %s\n", timeUpStr);

    strcpy(cmd_getTime, "");
    strcpy(cmd_getTime, getTimePrefix);
    strcat(cmd_getTime, portStr);
    strcat(cmd_getTime, getTimeDownSuffix);
    //printf("Time down cmd = %s\n", cmd_getTime);
    pipeTime = openPipe(cmd_getTime);
    fgets(timeDownStr, OUTPUT_MAX_LENGTH, pipeTime);
    //printf("Time Down = %s", timeDownStr);
    closePipe(pipeTime, cmd_getTime);

    paramPort->errorCode |= isValidTime(timeUpStr, &timeInfo);

    if(paramPort->errorCode == ERR_OK) {
        paramPort->timeAlarm[time_up].time_H = timeInfo.tm_hour;
        paramPort->timeAlarm[time_up].time_M = timeInfo.tm_min;

        timeInfoPtr = localtime(&now);
        timeInfoPtr->tm_hour   = paramPort->timeAlarm[time_up].time_H;
        timeInfoPtr->tm_min    = paramPort->timeAlarm[time_up].time_M;
        timeInfoPtr->tm_sec    = 0;
        paramPort->timeAlarm[time_up].targetTime = mktime(timeInfoPtr);

        if(now >= paramPort->timeAlarm[time_up].targetTime)
        {
            timeInfoPtr->tm_mday += 1;
            paramPort->timeAlarm[time_up].targetTime = mktime(timeInfoPtr);
            //printf("target Time set to next Day\n");
        }
    }

    paramPort->errorCode |= isValidTime(timeDownStr, &timeInfo);

    if(paramPort->errorCode == ERR_OK) {
        paramPort->timeAlarm[time_down].time_H = timeInfo.tm_hour;
        paramPort->timeAlarm[time_down].time_M = timeInfo.tm_min;
        timeInfoPtr = localtime(&now);
        timeInfoPtr->tm_hour   = paramPort->timeAlarm[time_down].time_H;
        timeInfoPtr->tm_min    =  paramPort->timeAlarm[time_down].time_M;
        timeInfoPtr->tm_sec    = 0;
        paramPort->timeAlarm[time_down].targetTime = mktime(timeInfoPtr);

        if(now >= paramPort->timeAlarm[time_down].targetTime)
        {
            timeInfoPtr->tm_mday += 1;
            paramPort->timeAlarm[time_down].targetTime = mktime(timeInfoPtr);
            //printf("target Time set to next Day\n");
        }
        if(isDebugMode())
        {
            printf("set port Time Param -- port = %d\n", paramPort->portNum);
            printf("time Up: remain = %ld\n", paramPort->timeAlarm[time_up].targetTime - now);
            printf("time Up:    Hour = %d Min = %d \n", paramPort->timeAlarm[time_up].time_H, paramPort->timeAlarm[time_up].time_M);
            printf("time down: remain = %ld\n", paramPort->timeAlarm[time_down].targetTime - now);
            printf("time Down:  Hour = %d Min = %d \n", paramPort->timeAlarm[time_down].time_H, paramPort->timeAlarm[time_down].time_M);
        }
    }
}

////////////////////////////////////////////////////////////////////
void port_runTestDisable(paramPort_t *paramPort)
{
    //printf("runTestDisable\n");
    paramPort->errorCode &= (~ERR_OK);
    paramPort->errorCode |= ERR_TEST_DISABLE;
}

void port_runTestLink(paramPort_t *paramPort)
{
    char output[OUTPUT_MAX_LENGTH];
    //printf("TEST LINK \t - ");
    FILE *portState             = NULL;
    char str_up[]               = "up\n";
    char cmd_getOperstate[64]   = "";
    char path_OperstatePrefix[] = "cat /sys/class/net/lan\0";
    char path_OperstateSufix[]  = "/operstate\0";

    char portStr[2] = "";

    intToString(paramPort->portNum, portStr);
    strcpy(cmd_getOperstate, path_OperstatePrefix);
    strcat(cmd_getOperstate, portStr);
    strcat(cmd_getOperstate, path_OperstateSufix);
    //printf("cmd_getOperstate = %s\n", cmd_getOperstate);

    portState = openPipe(cmd_getOperstate);
    fgets(output, OUTPUT_MAX_LENGTH, portState);
    //printf("output = %s\n", output);
    //printf("length output = %lu\n", sizeof(output));
    closePipe(portState, cmd_getOperstate);

    paramPort->errorCode &= (~ERR_TEST_LINK);
    paramPort->errorCode |= ERR_OK;

    if (strcmp(output, str_up)!=0) {
        paramPort->errorCode &= (~ERR_OK);
        paramPort->errorCode |= ERR_TEST_LINK;
    }
}

void port_runTestPing(paramPort_t *paramPort)
{
    const unsigned long timeout = 1500;
    unsigned long reply_time    = 0;
    //printf("port_runTestPing: Port num =  %d\n", paramPort->portNum);
    int array[NUM_PORTS];
    //const int result = ping(pingArgs);
    uint8_t  portNum = paramPort->portNum;
    int status;
    int pingResult = ERR_TEST_PING;
    pingResult = ping(paramPort->host, timeout, &reply_time);
    if(isDebugMode())
    {
        printf("ping result = %d, port = %d\n", pingResult, paramPort->portNum);
    }
    if (pingResult == ERR_TEST_PING)
    {
        //printf("Host is not available or timeOut\n");
        paramPort->errorCode &= (~ERR_OK);
        paramPort->errorCode |= ERR_TEST_PING;
    }
    else if (pingResult == ERR_OK)
    {
        paramPort->reply_time = reply_time;
        paramPort->errorCode = ERR_OK;
    }
}

void port_runTestSpeed(paramPort_t *paramPort)
{
    //printf("Test Speed: %d\n", paramPort->portNum);

    char output[OUTPUT_MAX_LENGTH];
    FILE *portSpeed             = NULL;

    char cmd_get_rx_byte[64]    = "";
    char path_rx_bytePrefix[]   = "cat /sys/class/net/lan";
    //char path_rx_bytePrefix[]   = "cat /sys/class/net/enp";
    char path_rx_byteSufix[]    = "/statistics/rx_bytes";

    char str[2] = "";
    intToString(paramPort->portNum, str);

    strcpy(cmd_get_rx_byte, path_rx_bytePrefix);
    strcat(cmd_get_rx_byte, str);
    strcat(cmd_get_rx_byte, path_rx_byteSufix);
    //printf("cmd_get_rx_byte = %s\n", cmd_get_rx_byte);

    portSpeed = openPipe(cmd_get_rx_byte);

    fgets(output, OUTPUT_MAX_LENGTH, portSpeed);
    closePipe(portSpeed, cmd_get_rx_byte);
    //printf("rx byte =  %s", output);

    paramPort->rx_byte_current = (int32_t )strtol(output, NULL, 10);
    paramPort->rx_delta_byte = paramPort->rx_byte_current - paramPort->rx_byte_prev;
    //printf("RX_Byte:  current = %d - prev = %d - delta %d\n",paramPort->rx_byte_current, paramPort->rx_byte_prev, paramPort->rx_delta_byte);
    paramPort->rx_byte_prev = paramPort->rx_byte_current;

    paramPort->time_current = time (NULL);
    uint32_t delta_time = paramPort->time_current - paramPort->time_prev;
    //printf("Time: current = %d - prev = %d - delta %d\n",paramPort->time_current, paramPort->time_prev, delta_time);
    paramPort->time_prev = paramPort->time_current;

    uint32_t rx_delta_bit = paramPort->rx_delta_byte * 8;
    uint32_t rx_speed_Kbit = rx_delta_bit;
    if (delta_time == 0) {
        paramPort->errorCode &= (~ERR_OK);
        paramPort->errorCode |= ERR_TEST_SPEED;
    }
    else
    {
        rx_speed_Kbit = rx_delta_bit / (delta_time);
        rx_speed_Kbit = rx_speed_Kbit/1000;
        //printf("speed [KBit/sec] = %d\n", rx_speed_Kbit);

        if ((paramPort->min_speed != 0 && rx_speed_Kbit < paramPort->min_speed) ||
            (paramPort->max_speed != 0 && rx_speed_Kbit > paramPort->max_speed))
        {
            paramPort->errorCode &= (~ERR_OK);
            paramPort->errorCode |= ERR_TEST_SPEED;
        }
    }
    paramPort->rx_speed_Kbit = rx_speed_Kbit;
}

void port_runTestTime(paramPort_t *paramPort)
{
    //printf("Test Time = %d\n", paramPort->portNum);
    paramPort->errorCode &= (~ERR_OK);
    //printf("runTestTime Up: remain = %ld target = %ld\n", paramPort->timeUp.remainTime, paramPort->timeUp.targetTime);
    paramPort->errorCode |= timeAlarm(&paramPort->timeAlarm[time_up]);
    //printf("runTestTime Down: remain = %ld target = %ld\n", paramPort->timeDown.remainTime, paramPort->timeDown.targetTime);
    paramPort->errorCode |= timeAlarm(&paramPort->timeAlarm[time_down]);
}

//////////////////////////////////////////////////////////////
AR_STATE get_PortAndPoeState(paramPort_t *paramPort)
{
    AR_STATE  ar_state = REGULAR_SATE;
    char output[OUTPUT_MAX_LENGTH];
    char cmdPortState[24]   = "";
    char cmdPoEState[24]    = "";
    char s_enable[]         = "enable\n";
    char str1[]             = "uci get port.lan";
    char state[]            = ".state";
    char poe[]              = ".poe";

    //printf("get_PortAndPoeState: paramPort->portNum %d\n", paramPort->portNum);
    //printf("decimalNum[paramPort->portNum] %s\n", decimalNum[paramPort->portNum]);

    char portStr[2] = "";
    intToString(paramPort->portNum, portStr);

    strcpy(cmdPortState, str1);
    strcat(cmdPortState, portStr);
    strcat(cmdPortState, state);

    strcpy(cmdPoEState, str1);
    strcat(cmdPoEState, portStr);
    strcat(cmdPoEState, poe);

    FILE *portState = openPipe(cmdPortState);
    while (fgets(output, OUTPUT_MAX_LENGTH, portState))
    {
        if (strcmp (output, s_enable)==0) {
            paramPort->port_state = enable;
        }else {
            paramPort->port_state = disable;
            paramPort->errorCode &= (~ERR_OK);
            paramPort->errorCode |= ERR_PORT_DISABLE;
            ar_state = IDLE_STATE;
        }
    }
    //printf(" port state = %d \n", paramPort->port_state);
    closePipe(portState, cmdPortState);

    FILE *poeState = openPipe(cmdPoEState);
    while (fgets(output, OUTPUT_MAX_LENGTH, poeState))
    {
        if (strcmp (output, s_enable)==0) {
            //printf(" output true = %s \n", output);
            paramPort->poe_state = enable;
        } else {
           // printf(" output false = %s \n", output);
            paramPort->poe_state = disable;
            paramPort->errorCode &= (~ERR_OK);
            paramPort->errorCode |= ERR_PORT_DISABLE;
            ar_state = IDLE_STATE;
        }
    }
    closePipe(poeState, cmdPoEState);
    return ar_state;
}

static error_code_t timeAlarm(time_h_m *timeAlarmInfo)
{
    time_t now = time(NULL);
    struct tm *timeInfo = localtime(&now);

    timeInfo->tm_hour   = timeAlarmInfo->time_H;
    timeInfo->tm_min    = timeAlarmInfo->time_M;
    timeInfo->tm_sec    = 0;
    //printf("timeAlarm: remain = %ld target = %ld\n", timeAlarmInfo->remainTime, timeAlarmInfo->targetTime);
    now = time(NULL);
    if (now >= timeAlarmInfo->targetTime) {
        timeInfo = localtime(&now);
        timeInfo->tm_mday += 1; // add one day
        timeAlarmInfo->targetTime = mktime(timeInfo);
        timeAlarmInfo->remainTime = 0;
        return  ERR_TIME_ALARM;
    } else{
        timeAlarmInfo->remainTime = timeAlarmInfo->targetTime - now;
        //printf("timeRemain = %ld\n", timeAlarmInfo->remainTime);
        //printf("timeAlarmInfo.targetTime = %ld\n", timeAlarmInfo->targetTime);
        return ERR_OK;
    }
}

void rebootHandler(portInfo_t *portInfo, DEMON_PARAM_t *demonParam)
{
    uint8_t portNum = portInfo->paramPort.portNum;
    static uint8_t testCount = 0;
    static uint8_t errorCode = 0;

    if (isDebugMode())
    {
        if (testCount != portInfo->paramPort.errorTestCount)
        {
            printf("error TestCount %d\n", portInfo->paramPort.errorTestCount);
            testCount = portInfo->paramPort.errorTestCount;
        }
        if (errorCode != portInfo->paramPort.errorCode)
        {
            printf("error Code Hex %05x \n", portInfo->paramPort.errorCode);
            errorCode = portInfo->paramPort.errorCode;
        }
    }

    switch (rebootParam[portNum].rebootState)
    {
        case REBOOT_STATE_IDLE: {
            //printf("REBOOT_STATE_IDLE\n");
            if (portInfo->event & AUTO_RESTART)
            {
                portInfo->event &= (~AUTO_RESTART);
                rebootParam[portNum].rebootState = autoResetHandler(portInfo,  demonParam->max_reset);
            }

            if(portInfo->event & MANUAL_RESTART)
            {
                portInfo->event &= (~MANUAL_RESTART);
                rebootParam[portNum].rebootState = manualResetHandler(portInfo);
            }
        }
        break;

        case REBOOT_STATE_INIT: {
            uint32_t nowTime = time(NULL);
            openlog("tf_autoresart", LOG_PID, LOG_USER);
            syslog(LOG_INFO, "REBOOT  INIT, port - %d", portNum);
            closelog();
            rebootParam[portNum].rebootState = REBOOT_STATE_POE_DOWN;
            rebootParam[portNum].rebootEvent |= (REBOOT_EVENT_START);
            rebootParam[portNum].timeStartReboot = rebootParam[portNum].timeDelay + nowTime;
        }
        break;
        case REBOOT_STATE_POE_DOWN:
        {
            if(rebootParam[portNum].rebootEvent & REBOOT_EVENT_START)
            {
                //printf("REBOOT_STATE_POE_DOWN --- REBOOT_EVENT_START\n");
                openlog("tf_autoresart", LOG_PID, LOG_USER);
                syslog(LOG_INFO, "REBOOT STATE POE_DOWN - TIMER START, port - %d", portNum);
                closelog();
                rebootParam[portNum].rebootEvent &= (~REBOOT_EVENT_START);
                poe_Control(portNum, POE_DOWN);
            }
            rebootParam[portNum].rebootEvent |= checkTimePoeUP(rebootParam[portNum].timeStartReboot);
            if (rebootParam[portNum].rebootEvent & REBOOT_EVENT_TIMER_STOP) {
                //printf("REBOOT_STATE_POE_DOWN --- REBOOT_EVENT_TIMER_STOP\n");
                openlog("tf_autoresart", LOG_PID, LOG_USER);
                syslog(LOG_INFO, "REBOOT STATE POE_DOWN - TIMER STOP, port - %d", portNum);
                closelog();
                rebootParam[portNum].rebootState = REBOOT_STATE_POE_UP;
            }
         }
        break;
        case REBOOT_STATE_POE_UP: {
            if(rebootParam[portNum].rebootEvent & REBOOT_EVENT_TIMER_STOP)
            {
                rebootParam[portNum].rebootEvent &= (~REBOOT_EVENT_TIMER_STOP);
                //printf("REBOOT_STATE_POE_UP --- REBOOT_EVENT_TIMER_STOP\n");
                poe_Control(portNum, POE_UP);
                rebootParam[portNum].rebootEvent |= REBOOT_EVENT_DONE;
            }
            if(rebootParam[portNum].rebootEvent & REBOOT_EVENT_DONE)
            {
                //printf("REBOOT_STATE_POE_UP --- REBOOT_EVENT_DONE\n");
                openlog("tf_autoresart", LOG_PID, LOG_USER);
                syslog(LOG_INFO, "REBOOT STATE POE UP - DONE, port - %d", portNum);
                closelog();
                rebootParam[portNum].rebootState = REBOOT_STATE_IDLE;
                rebootParam[portNum].rebootEvent |= REBOOT_EVENT_IDLE;
                portInfo->paramPort.errorCode &= (~ERR_REBOOTING);
                portInfo->paramPort.errorCode &= (~ERR_MANUAL_REBOOT);
                portInfo->state = REGULAR_SATE;
            }
        }
        break;
    }
}

void poe_Control(uint8_t port, POE_CONTROL poeControl)
{
    FILE *pipe_poeRestart = NULL;
    char output[OUTPUT_MAX_LENGTH];
    char cmdPortRestart[64] = "";
    char str1[]             = "ubus call poe manage '{\"port\":\"lan"; //   1\", \"enable\":true}'";
    char strTrue[]          = "\", \"enable\":true}'";
    char strFalse[]         = "\", \"enable\":false}'";

    char portStr[2] = "";
    intToString(port, portStr);

    if(poeControl & POE_DOWN)
    {
        strcpy(cmdPortRestart, str1);
        strcat(cmdPortRestart, portStr);
        strcat(cmdPortRestart, strFalse);
        pipe_poeRestart = openPipe(cmdPortRestart);
        closePipe(pipe_poeRestart, cmdPortRestart);
        openlog("tf_autoresart", LOG_PID, LOG_USER);
        syslog(LOG_INFO, "POE DOWN, port - %d", port);
        closelog();
    }

    if(poeControl & POE_UP)
    {
        strcpy(cmdPortRestart, "");
        strcpy(cmdPortRestart, str1);
        strcat(cmdPortRestart, portStr);
        strcat(cmdPortRestart, strTrue);
        pipe_poeRestart = openPipe(cmdPortRestart);
        closePipe(pipe_poeRestart, cmdPortRestart);
        openlog("tf_autoresart", LOG_PID, LOG_USER);
        syslog(LOG_INFO, "POE UP, port - %d ", port);
        closelog();
    }
}

void rebootParamInit(void)
{
    for (int i = 1; i < NUM_PORTS; ++i)
    {
        rebootParam[i].rebootState     = REBOOT_STATE_IDLE;
        rebootParam[i].rebootEvent     = REBOOT_EVENT_IDLE;
        rebootParam[i].timeDelay       = 15;
        rebootParam[i].timeStartReboot = 0;
    }
}

static REBOOT_STATE_e autoResetHandler(portInfo_t *portInfo, uint8_t maxReset)
{
    REBOOT_STATE_e rebootState = REBOOT_STATE_IDLE;
    char  timeDateStr[32] = "";
    getTimeDate(timeDateStr);
    strcpy(portInfo->paramPort.timeStr,timeDateStr);
    portInfo->paramPort.errorCode &= (~ERR_OK);

    if (portInfo->paramPort.resetCount >= maxReset) {
        portInfo->paramPort.errorCode |= ERR_PORT_SHUTDOWN;
        rebootState = REBOOT_STATE_IDLE;
        portInfo->state = IDLE_STATE;
        syslog(LOG_INFO, "Reboot disable after max reset, port - %d", portInfo->paramPort.portNum);
        closelog();
    }
    else
    {
        openlog("tf_autorestart", LOG_PID, LOG_USER);
        syslog(LOG_INFO, "auto reset port = %d", portInfo->paramPort.portNum);
        syslog(LOG_INFO, "total reset count = %d", portInfo->paramPort.totalResetCount);
        closelog();
        portInfo->paramPort.errorTestCount = 0;
        portInfo->paramPort.resetCount++;
        portInfo->paramPort.totalResetCount++;
        portInfo->paramPort.errorCode |= ERR_REBOOTING;
        portInfo->paramPort.errorTestCount = 0;
        //poe_Control(portInfo->paramPort.portNum, POE_RESTART);
        printf("resetCount = %d -- totalResetCount = %d\n", portInfo->paramPort.resetCount, portInfo->paramPort.totalResetCount);
        rebootState = REBOOT_STATE_INIT;
    }
    if (isDebugMode())
    {
        printf("portNum         %d\n", portInfo->paramPort.portNum);
        printf("errorCode       %d\n", portInfo->paramPort.errorCode);
        printf("resetCount      %d\n", portInfo->paramPort.resetCount);
        printf("total Resets    %d\n", portInfo->paramPort.totalResetCount);
        printf("Current Time:   %s\n", portInfo->paramPort.timeStr);
        // portInfo->resetPort.errorCode |= portInfo->paramPort.errorCode;
    }
    return rebootState;
}

static REBOOT_STATE_e manualResetHandler(portInfo_t *portInfo)
{
    openlog("tf_autorestart", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "run manual reset - port = %d", portInfo->paramPort.portNum);
    closelog();

    char  timeDateStr[32] = "";
    getTimeDate(timeDateStr);
    strcpy(portInfo->paramPort.timeStr,timeDateStr);
    portInfo->paramPort.errorCode &= (~ERR_OK);
    portInfo->paramPort.errorCode &= (~ERR_PORT_SHUTDOWN);
    portInfo->paramPort.errorCode |= ERR_MANUAL_REBOOT;
    portInfo->paramPort.totalResetCount++;
    portInfo->paramPort.resetCount = 0;
    portInfo->paramPort.errorTestCount = 0;
    //poe_Control(portInfo->paramPort.portNum, POE_RESTART);

    if (isDebugMode())
    {
        printf("portNum         %d\n", portInfo->paramPort.portNum);
        printf("errorCode       %d\n", portInfo->paramPort.errorCode);
        printf("resetCount      %d\n", portInfo->paramPort.resetCount);
        printf("total Resets    %d\n", portInfo->paramPort.totalResetCount);
        printf("Current Time:   %s\n", portInfo->paramPort.timeStr);
    }
    return REBOOT_STATE_INIT;
}

static REBOOT_EVENT_e checkTimePoeUP(uint32_t timeStartReboot) {

    uint32_t nowTime = time(NULL);
    if (nowTime >= timeStartReboot)
    {
        return REBOOT_EVENT_TIMER_STOP;
    }
    return REBOOT_EVENT_IDLE;
}




