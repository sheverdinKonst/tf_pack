//
// Created by Sheverdin on 12/8/23.
//

#include "mainHandler.h"
#include <pthread.h>
#include "portConfig.h"
#include "utils.h"
#include "time.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <fcntl.h>

static void getDemonParam(void);
static void getPortParam(void);

static void *runTestDisable(paramPort_t *paramPort);
static void *runTestLink(paramPort_t *paramPort);
static void *runTestPing(paramPort_t *paramPort);
static void *runTestSpeed(paramPort_t *paramPort);
static void *runTestTime(paramPort_t *paramPort);

static void setDisable(uint8_t portNum);
static void setLink(uint8_t portNum);
static void setHost(uint8_t portNum);
static void setSpeed(uint8_t portNum);
static void setTime(uint8_t portNum);

static void set_errorHandler(paramPort_t *paramPort);
static void test_ErrorHandler(portInfo_t *portInfo);

static void runTimer(void);
static error_code_t createTread(void);
static error_code_t joinThread(void);

portInfo_t portInfoArr[NUM_PORTS];
pthread_t threadsMain[NUM_PORTS];

DEMON_PARAM_t demonParam = {
    .test_delay = 3,
    .test_num   = 10,
    .max_reset  = 13,
    .mainTime   = 0
};

typedef enum
{
    TEST_DELAY = 0,
    TEST_NUM   = 1,
    MAX_RESET  = 2,
    MAX_PARM   = 3
}DEMON_PARAM_e;

const char demonParamName[MAX_PARM][11] = {
    "test_delay",
    "test_num",
    "max_reset",
};

runTest_t runTest[test_max] =
{
    &runTestDisable,
    &runTestLink,
    &runTestPing,
    &runTestSpeed,
    &runTestTime,
};

setTestConfig_t setTestConfig[test_max] =
{
    &setDisable,
    &setLink,
    &setHost,
    &setSpeed,
    &setTime
};

void mainInit(int argc, char **argv)
{
    if (argc == 2){
        if (argv[1][0] == '-'){
            printf("argv[1][1] == \'-\' \n");
            setTestMode(argv[1][1]);
        }
    }
    if(isDebugMode())
    {
        printf("tf_autorestart VERSION: %s\n", VERSION);
    }

    getDemonParam();
    getPortParam();
    rebootParamInit();
    runTimer();
}

void mainApp()
{
    int pipe_fd_A[2];
    int sockfd, client_fd;
    struct sockaddr_un server_addr, client_addr;

    printf("Main App\n");
    socklen_t addrlen = sizeof(client_addr);

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sockfd == -1)
    {
        perror("socket");
        //return 1;
    }

    const int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, SOCKET_PATH);
    unlink(SOCKET_PATH);

    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        //return 1;
    }
    if (listen(sockfd, MAX_CLIENTS) == -1)
    {
        perror("listen");
        //return 1;
    }

    if (pipe(pipe_fd_A) == -1)
    {
        perror("pipe");
        //return 1;
    }
    fd_set read_fds;
    pid_t pid;
    pid_t  pidChild;
    int status;
    printf("Before main While\n");
    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        FD_SET(pipe_fd_A[0], &read_fds);
        int max_fd = sockfd > pipe_fd_A[0] ? sockfd : pipe_fd_A[0];
        struct timeval timeout;
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        printf("Before select - %ld\n", time(NULL));
        if (select(max_fd + 1, &read_fds, NULL, NULL, &timeout) == -1)
        {
            perror("select");
            //return 1;
        }
        printf("After select - %ld\n", time(NULL));


        if (FD_ISSET(sockfd, &read_fds))
        {
            client_fd = accept(sockfd, (struct sockaddr *) &client_addr, &addrlen);
            if (client_fd == -1)
            {
                perror("accept");
                continue;
            }
            sigset_t sigset;
            pid = fork();
            //printf("pid = %d\n", pid);
            if (pid == -1)
            {
                perror("fork");
                close(client_fd);
                continue;
            }
            else if (pid == 0)
            {
                // Child Process
                sigemptyset(&sigset);
                pidChild = getpid();
                close(sockfd);
                sock_msg_t clientMsg;
                    ssize_t readBytes = read(client_fd, &clientMsg, sizeof(clientMsg));
                    if (readBytes == -1)
                    {
                        //perror("read 2:");
                        close(client_fd);
                        //return 1;
                    }
                    else if (readBytes == 0)
                    {
                        //printf("readBytes = %zd\n", readBytes);
                        //close(client_fd);
                        //client_fd = accept(sockfd, (struct sockaddr *) &client_addr, &addrlen);
                    }
                    else if (readBytes > 0)
                    {
                        if(isDebugMode())
                        {
                            printf("data from Client cmd type = %d port num = %d\n", clientMsg.cmdType, clientMsg.port);
                            printf("readBytes = %zd\n", readBytes);
                        }
                        if (clientMsg.cmdType == cmd_status)
                        {
                            //// TODO convert this block to function

                            resetPort_t resetPortBuf[NUM_PORTS];
                            for (int portNum = 1; portNum < NUM_PORTS; portNum++)
                            {
                                resetPortBuf[portNum].errorCode     = portInfoArr[portNum].paramPort.errorCode;
                                resetPortBuf[portNum].portNum       = portInfoArr[portNum].paramPort.portNum;
                                resetPortBuf[portNum].resetCount    = portInfoArr[portNum].paramPort.totalResetCount;
                                resetPortBuf[portNum].testType      = portInfoArr[portNum].paramPort.test;
                                strcpy(resetPortBuf[portNum].timeStr, portInfoArr[portNum].paramPort.timeStr);
                            }

                            if (send(client_fd, &resetPortBuf, sizeof(resetPortBuf), 0) == -1)
                            {
                                //perror("send");
                                close(client_fd);
                                //return 1;
                            }
                        }
                        else if (clientMsg.cmdType == cmd_reboot){
                            if(isDebugMode())
                            {
                                printf("****************clientMsg.cmdType == cmd_reboot****************\n");
                                printf("clientMsg  port = %d cmdType = %d\n", clientMsg.port, clientMsg.cmdType);
                            }
                            openlog("tf_autoresart", LOG_PID, LOG_USER);
                            syslog(LOG_INFO, "cmd manual reboot: port - %d", clientMsg.port);
                            closelog();
                            char buf[128] = "restart port\n";
                            if (send(client_fd, buf, strlen(buf), 0) == -1)
                            {
                                //perror("send");
                                close(client_fd);
                                //return 1;
                            }

                            static int d = 0;

                            if (write(pipe_fd_A[1], &clientMsg, sizeof(clientMsg)) == -1)
                            {
                                //perror("pipe_fd_A[1] ");
                                continue;
                            }
                        }
                        exit(0);
                    }
                close(client_fd);
            }
            pid_t  pidChild1 = wait(&status);
            close(client_fd);
        }

        if (FD_ISSET(pipe_fd_A[0], &read_fds))
        {
            sock_msg_t clientMsg;
            if (read(pipe_fd_A[0], &clientMsg, sizeof(clientMsg)) == -1)
            {
                //perror("read 3 ");
                continue;
            }
            else
            {
                //printf("cmdType = %d -- port = %d\n", clientMsg.cmdType, clientMsg.port);
                if (clientMsg.cmdType == cmd_reboot) {
                    openlog("tf_autoresart", LOG_PID, LOG_USER);
                    syslog(LOG_INFO, "set manual reboot port - %d", clientMsg.port);
                    closelog();
                    if (portInfoArr[clientMsg.port].state != REBOOT_STATE)
                    {
                        portInfoArr[clientMsg.port].state = REBOOT_STATE;
                        portInfoArr[clientMsg.port].event = MANUAL_RESTART;
                    }
                    else {
                        openlog("tf_autoresart", LOG_PID, LOG_USER);
                        syslog(LOG_INFO, "Manual reboot disable, port - %d in rebooting sate", clientMsg.port);
                        closelog();
                    }
                }
            }
        }
       mainHandler();
    }
}

static void runTimer(void)
{
    demonParam.mainTime = time(NULL) + demonParam.test_delay;
}

void mainHandler()
{
    static int count = 1;
    static  uint32_t nowTime;
    static AR_STATE state = IDLE_STATE;
    nowTime = time(NULL);
    if (nowTime >=  demonParam.mainTime)
    {
        demonParam.mainTime = demonParam.mainTime + demonParam.test_delay;
        error_code_t errorCode = 0;
        errorCode |= createTread();
        errorCode |= joinThread();

        if (isDebugMode())
        {
            printf("******************** globalCount = %d\n", count);
            if (errorCode & ERR_CREATE_THREAD)
            {
                printf("ERROR create thread\n");
            }
            if (errorCode & ERR_JOIN_THREAD)
            {
                printf("ERROR join thread\n");
            }
            printf("============= test_ErrorHandler ==============\n");
        }

        for (int port = 1; port < NUM_PORTS; port++)
        {
            if(portInfoArr[port].paramPort.errorCode & ERR_PORT_DISABLE)
            {

            }
            else
            {
                if (port == 1)
                {
                    if (state != portInfoArr[port].state)
                    {
                        printf("portInfoArr[port].state %d\n", portInfoArr[port].state);
                        state = portInfoArr[port].state;
                    }
                }

                switch (portInfoArr[port].state)
                {
                    case REBOOT_STATE:
                    {
                        rebootHandler(&portInfoArr[port], &demonParam);
                    }
                    break;

                    case REGULAR_SATE:
                        test_ErrorHandler(&portInfoArr[port]);
                        break;
                    case IDLE_STATE:

                    default:
                        break;
                }
            }
        }
        count++;
    }
}

static error_code_t createTread(void)
{
    for (int port = 1; port < NUM_PORTS; port++)
    {
        portInfoArr[port].paramPort.portNum = port;
        //portInfoArr[port].paramPort.errorCode = ERR_OK;
        AR_STATE state = get_PortAndPoeState(&portInfoArr[port].paramPort);
        if (state == IDLE_STATE)
        {
            portInfoArr[port].state = state;
        }

        if (pthread_create(&threadsMain[port], NULL, (void *(*)(void *)) runTest[portInfoArr[port].paramPort.test], &portInfoArr[port].paramPort))
        {
            portInfoArr[port].paramPort.errorCode |= ERR_CREATE_THREAD;
            return ERR_CREATE_THREAD;
        }
    }
    return ERR_OK;
}

static error_code_t  joinThread(void)
{
    for (int port = 1; port < NUM_PORTS; port++)
    {
        int status = pthread_join(threadsMain[port], NULL);

        if (status != 0) {
            //fprintf(stderr, "Error joining thread %d. Return code: %d\n", port, status);
            printf("ERR_JOIN_THREAD - port %d\n", port);
            portInfoArr[port].paramPort.errorCode |= ERR_JOIN_THREAD;
            return ERR_JOIN_THREAD;
        }
    }
    return ERR_OK;
}

static void getDemonParam(void)
{
    char output[OUTPUT_MAX_LENGTH];
    FILE *pipeDeamonParam = NULL;
    char str_cmd[64] = "";
    char str_uciGet[] = "uci get tf_autorestart.ar_demon.";

    uint8_t param[MAX_PARM] = {3,20,3};
    for (int i = 0; i < MAX_PARM; i++)
    {
        strcpy(str_cmd, "");
        strcpy(str_cmd, str_uciGet);
        strcat(str_cmd, demonParamName[i]);
        pipeDeamonParam = openPipe(str_cmd);
        fgets(output, OUTPUT_MAX_LENGTH, pipeDeamonParam);
        closePipe(pipeDeamonParam, str_cmd);
        param[i] = strtol(output, NULL,10);

        if (isDebugMode()){
            printf("str_cmd = %s\n", str_cmd);
            printf("output = %s\n", output);
            printf("parm [%d] = %d\n", i, param[i]);
        }
    }
    if (param[TEST_DELAY] > 0 && param[TEST_DELAY] < 60) {
        demonParam.test_delay = param[TEST_DELAY];
    }
    if (param[TEST_NUM] > 0 && param[TEST_NUM] < 255) {
        demonParam.test_num = param[TEST_NUM];
    }
    if (param[MAX_RESET] > 0 && param[MAX_RESET] < 255) {
        demonParam.max_reset  = param[MAX_RESET];
    }
}

static void getPortParam(void)
{
    for (int port = 1; port < NUM_PORTS; port++)
    {
        portInfoArr[port].paramPort.portNum = port;
        char output[OUTPUT_MAX_LENGTH];
        FILE *pipeTypeOfTest = NULL;

        char portStr[2] = " ";
        intToString(port, portStr);

        char str_TypeOfTestPrefix[] = "uci get tf_autorestart.lan";
        char cmd_getTypeOfTest[64]  = "";
        char str_TypeOfTestSuffix[] = ".state";

        strcpy(cmd_getTypeOfTest, str_TypeOfTestPrefix);
        strcat(cmd_getTypeOfTest, portStr);
        strcat(cmd_getTypeOfTest, str_TypeOfTestSuffix);

        pipeTypeOfTest = openPipe(cmd_getTypeOfTest);
        fgets(output, OUTPUT_MAX_LENGTH, pipeTypeOfTest);
        closePipe(pipeTypeOfTest, cmd_getTypeOfTest);

        portInfoArr[port].paramPort.errorCode = ERR_OK;
        portInfoArr[port].paramPort.totalResetCount = 0;
        portInfoArr[port].paramPort.resetCount = 0;
        TYPE_OF_TEST_t typeOfTest = getCmdKey(output);

        if (typeOfTest >= test_max){
            portInfoArr[port].paramPort.errorCode &= (~ERR_OK);
            portInfoArr[port].paramPort.errorCode |= ERR_TEST_TYPE;
        }else if (typeOfTest == test_disable) {
            portInfoArr[port].paramPort.errorCode &= (~ERR_OK);
            portInfoArr[port].paramPort.errorCode |= ERR_TEST_DISABLE;
        }else{
            portInfoArr[port].paramPort.test    = typeOfTest;
            portInfoArr[port].paramPort.portNum = port;
            portInfoArr[port].event             = IDLE_EVENT;
            setTestConfig[portInfoArr[port].paramPort.test](port);
        }
        portInfoArr[port].state = REGULAR_SATE;
        set_errorHandler(&portInfoArr[port].paramPort);
    }
}

static void setDisable(uint8_t portNum)
{
    //printf("set port Disable Param -- port = %d\n", paramPort->portNum);
    //port_setDisableParam(&portInfoArr[portNum].paramPort);
}

static void setLink(uint8_t portNum)
{
    port_setLinkParam(&portInfoArr[portNum].paramPort);
}

static void setHost(uint8_t portNum)
{
    port_setHostParam(&portInfoArr[portNum].paramPort);
}

static void setSpeed(uint8_t portNum)
{
    port_setSpeedParam(&portInfoArr[portNum].paramPort);
}

static void setTime(uint8_t portNum)
{
    port_setTimeParam(&portInfoArr[portNum].paramPort);
}

static void *runTestDisable(paramPort_t *paramPort)
{
    uint8_t portNum = paramPort->portNum;
    //printf("runTestDisable: portNum = %d\n", portNum);
    port_runTestDisable(paramPort);
    return NULL;
}

static void *runTestLink(paramPort_t *paramPort)
{
    if (paramPort->errorCode & ERR_PORT_DISABLE) {
        //printf("error PORT disable: port %d\t Error - 0x%05x - %d - \n", paramPort->portNum,  paramPort->errorCode, (paramPort->errorCode & ERR_PORT_DISABLE));
    }
    else{
        port_runTestLink(paramPort);
    }
    return NULL;
}

static void *runTestPing(paramPort_t *paramPort)
{
    if (isDebugMode()) {
        printf("-------------------------------------Run test Ping\n");
        printf("paramPort->port num = %d\n", paramPort->portNum);
        printf("paramPort->errorCode = %d\n", paramPort->errorCode);
    }

    if (paramPort->errorCode & ERR_PORT_DISABLE) {
        //printf("error PORT disable: port %d\t Error - 0x%05x - %d - \n", paramPort->portNum,  paramPort->errorCode, (paramPort->errorCode & ERR_PORT_DISABLE));
    }
    else{
        port_runTestLink(paramPort);
        if (paramPort->errorCode & ERR_TEST_LINK)
        {}
        else {
            port_runTestPing(paramPort);
        }
    }
    return NULL;
}

static void *runTestSpeed(paramPort_t *paramPort)
{
    if (paramPort->errorCode & ERR_PORT_DISABLE) {
        //printf("error PORT disable: port %d\t Error - 0x%05x - %d - \n", paramPort->portNum,  paramPort->errorCode, (paramPort->errorCode & ERR_PORT_DISABLE));
    }
    else{
        port_runTestLink(paramPort);
        if (paramPort->errorCode & ERR_TEST_LINK)
        {
            paramPort->errorCode |= ERR_TEST_SPEED;
        }
        else
        {
            port_runTestSpeed(paramPort);
        }
    }
    return NULL;
}

static void *runTestTime(paramPort_t *paramPort)
{
    if (paramPort->errorCode & ERR_PORT_DISABLE) {
        //printf("error PORT disable: port %d\t Error - 0x%05x - %d - \n", paramPort->portNum,  paramPort->errorCode, (paramPort->errorCode & ERR_PORT_DISABLE));
    }
    else{
        port_runTestTime(paramPort);
    }
    return NULL;
}

static void set_errorHandler(paramPort_t *paramPort)
{
    error_code_t  errorCode = paramPort->errorCode;
    uint8_t port            = paramPort->portNum;

    //printf("errorCode - 0x%05x\n", errorCode);

     if (errorCode & ERR_TEST_DISABLE) {
        portInfoArr[port].paramPort.test = test_disable;
        //printf("port %d\t Error - %d - ERR_TEST_DISABLE\n", port, (errorCode & ERR_TEST_DISABLE));
     }
     if (errorCode & ERR_IP_NOT_VALID) {
        portInfoArr[port].paramPort.test = test_disable;
        //printf("port %d\t Error - %d - ERR_IP_NOT_VALID\n", port, (errorCode & ERR_IP_NOT_VALID));
     }
     if (errorCode & ERR_REG_COMPILE) {
        portInfoArr[port].paramPort.test = test_disable;
        //printf("port %d\t Error - %d - ERR_REG_COMPILE\n", port, (errorCode & ERR_REG_COMPILE));
     }
     if (errorCode & ERR_TIME_NOT_VALID) {
        portInfoArr[port].paramPort.test = test_disable;
        //printf("port %d\t Error - %d - ERR_TIME_NOT_VALID\n", port, (errorCode & ERR_TIME_NOT_VALID));
     }
     if (errorCode & ERR_SPEED_VALUE) {
        portInfoArr[port].paramPort.test = test_disable;
        //printf("port %d\t Error - %d - ERR_SPEED_VALUE\n", port, (errorCode & ERR_SPEED_VALUE));
     }
}

static void test_ErrorHandler(portInfo_t *portInfo)
{
   // error_code_t  errorCode = ERR_OK;
    //uint8_t port            =  portInfo->paramPort.portNum;

    if (portInfo->paramPort.errorCode & ERR_OK)
    {
        if (portInfo->paramPort.errorTestCount > 0)
        {
            // portInfo->resetPort.errorCode = errorCode;
            // portInfo->resetPort.resetCount = portInfo->paramPort.resetCount;
            portInfo->paramPort.errorTestCount--;
        }
        //portInfo->paramPort.resetCount = (portInfo->paramPort.errorTestCount == 0) ? 0:portInfo->paramPort.resetCount;
    }

    if (portInfo->paramPort.errorCode & ERR_TEST_LINK | portInfo->paramPort.errorCode & ERR_TEST_SPEED | portInfo->paramPort.errorCode & ERR_TEST_PING)
    {
        if (portInfo->paramPort.errorTestCount < demonParam.test_num)
        {
            portInfo->paramPort.errorTestCount++;
        }

        if(portInfo->paramPort.errorTestCount == demonParam.test_num)
        {
            portInfo->event |= AUTO_RESTART;
            portInfo->state = REBOOT_STATE;
        }
    }

    if (portInfo->paramPort.errorCode & ERR_TIME_ALARM)
    {
        portInfo->paramPort.errorCode &= (~ERR_TIME_ALARM);
        //portInfo->resetPort.errorCode  |= portInfo->paramPort.errorCode;
        //portInfo->resetPort.portNum     = portInfo->paramPort.portNum;
        portInfo->paramPort.totalResetCount++;

        if (portInfo->paramPort.timeAlarm[time_up].remainTime == 0)
        {
            poe_Control(portInfo->paramPort.portNum, POE_UP);
        }

        if (portInfo->paramPort.timeAlarm[time_down].remainTime == 0)
        {
            poe_Control(portInfo->paramPort.portNum, POE_DOWN);
        }
    }

    if (isDebugMode())
    {
        if (portInfo->paramPort.errorCode & ERR_CREATE_THREAD)
        {
            printf("Error: ERR_CREATE_THREAD - port %d\tError - 0x%05x - %d\n", portInfo->paramPort.portNum, portInfo->paramPort.errorCode,
                   (portInfo->paramPort.errorCode & ERR_CREATE_THREAD));
        }

        if (portInfo->paramPort.errorCode & ERR_JOIN_THREAD)
        {
            printf("Error: ERR_JOIN_THREAD - port %d\tError - 0x%05x - %d\n", portInfo->paramPort.portNum, portInfo->paramPort.errorCode,
                   (portInfo->paramPort.errorCode & ERR_JOIN_THREAD));
        }

        if (portInfo->paramPort.errorCode & ERR_PORT_DISABLE)
        {
            //printf("Error: ERR_PORT_DISABLE - port %d\tError - 0x%05x - %d\n", port, errorCode,
            //       (errorCode & ERR_PORT_DISABLE));
        }

        switch (portInfo->paramPort.test)
        {
            case test_disable:
                if (portInfo->paramPort.errorCode & ERR_TEST_DISABLE)
                {
                    //printf("ERR_TEST_DISABLE port %d\t Error - 0x%05x - %d \n", port, errorCode, (errorCode & ERR_TEST_DISABLE));
                }
                break;

            case test_link:
                //printf("Test Link - ");
                if (portInfo->paramPort.errorCode & ERR_OK)
                {
                    //printf("Test OK: port = %d - - 0x%05x - %d  \n", port, errorCode, (errorCode & ERR_OK));
                }
                if (portInfo->paramPort.errorCode & ERR_TEST_LINK)
                {
                    //printf("Error: port %d\t Error - 0x%05x - %d - \n", port, errorCode, (errorCode & ERR_TEST_LINK));
                }
                break;

            case test_speed:
                printf("Test Speed - ");

                if (portInfo->paramPort.errorCode & ERR_OK)
                {
                    printf("OK: port =  %d - error 0x%05x - %d, speed: %d [Kbit/s]\n", portInfo->paramPort.portNum, portInfo->paramPort.errorCode,
                           (portInfo->paramPort.errorCode & ERR_OK), portInfo->paramPort.rx_speed_Kbit);
                }
                if (portInfo->paramPort.errorCode & ERR_TEST_SPEED)
                {
                    printf("Error: port %d\t Error - 0x%05x - %d - speed %d [Kbit/s]\n", portInfo->paramPort.portNum, portInfo->paramPort.errorCode,
                           (portInfo->paramPort.errorCode & ERR_TEST_SPEED), portInfo->paramPort.rx_speed_Kbit);
                }
                break;

            case test_ping:
                printf("Test Ping - ");
                if (portInfo->paramPort.errorCode & ERR_OK)
                {
                    printf("OK: port = %d, error  0x%05x - %d, reply_time: %d\n", portInfo->paramPort.portNum, portInfo->paramPort.errorCode,
                           (portInfo->paramPort.errorCode & ERR_OK),
                           portInfo->paramPort.reply_time);
                }
                if (portInfo->paramPort.errorCode & ERR_TEST_PING)
                {
                    printf("Error: port %d\t Error - 0x%05x - %d - \n", portInfo->paramPort.portNum, portInfo->paramPort.errorCode,
                           (portInfo->paramPort.errorCode & ERR_TEST_PING));
                }
                break;

            case test_time:
                printf("Test TIME ");
                uint32_t remainUp = portInfo->paramPort.timeAlarm[time_up].remainTime;
                uint32_t targetUp = portInfo->paramPort.timeAlarm[time_up].targetTime;
                uint32_t remainDown = portInfo->paramPort.timeAlarm[time_down].remainTime;
                uint32_t targetDown = portInfo->paramPort.timeAlarm[time_down].targetTime;
                if (portInfo->paramPort.errorCode & ERR_OK)
                {
                    printf("Up   - port = %d - remain = %u - target %u\n", portInfo->paramPort.portNum, remainUp, targetUp);
                    printf("Down - port = %d - remain = %u - target %u\n", portInfo->paramPort.portNum, remainDown, targetDown);
                }
                if (portInfo->paramPort.errorCode & ERR_TIME_ALARM)
                {
                    printf("Error: port %d\t Error - 0x%05x - %d -  \n", portInfo->paramPort.portNum, portInfo->paramPort.errorCode,
                           (portInfo->paramPort.errorCode & ERR_TIME_ALARM));
                    printf("Up   - port = %d - remain = %u - target %u\n", portInfo->paramPort.portNum, remainUp, targetUp);
                    printf("Down - port = %d - remain = %u - target %u\n", portInfo->paramPort.portNum, remainDown, targetDown);
                }
                break;

            case test_max:
            default:
                break;
        }
    }
}


