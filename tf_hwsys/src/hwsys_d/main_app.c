//
// Created by sheverdin on 3/21/24.
//

#include <signal.h>
#include "main_app.h"
#include "client_comm.h"
#include "critical_module.h"

static void getDemonParam(void);
static void mainHandler(int i2c_fd);

static void* runMainHandler(void *args);
static void* runComHandler(void *args);
static void* runCriticalHandler(void *args);
static void do_gpio_reset(void);

uint8_t ResetState = 0;
CRITICAL_EVENT_t criticalEvent = CRITICAL_IDLE;

mainThreads_t mainThreads[] =
{
    &runMainHandler,
    &runComHandler,
    &runCriticalHandler
};

app_param appParam = {
    .debugMode  = 0,
    .delay      = 3,
    .timeAlarm  = 0
};

void setResetState(uint8_t resetState)
{
    if (resetState == 1 || resetState == 0){
        ResetState = resetState;
    }
    else
    {
        printf("Error reset State value\n");
    }
}

uint32_t pause_sec = 0;

void setPause(uint32_t sec)
{
    pause_sec = sec;
    printf("SECOND %d\n", pause_sec);
}

void mainInit(int argc, char **argv)
{
    if (argc >= 2){
        if (argv[1][0] == '-'){
            printf("argv[1][1] == \'-\' \n");
            setTestMode(argv[1][1]);
        }

        printf("argv[2] = %s\n", argv[2]);
        uint32_t pauseSec = (uint32_t)strtol(argv[2],NULL, 10);

        printf("pauseSec = %d\n", pauseSec);
        setPause(pauseSec);
    }
    if(isDebugMode())
    {
        printf("tf_hwsys_demon VERSION: %s\n", VERSION);
    }

    getDemonParam();
    runTimer(&appParam.timeAlarm ,appParam.delay);
    int len2cLenMsg = i2c_get_MsgSize();
    printf("len i2c Len Msg = %d\n", len2cLenMsg);
}

void mainApp(void)
{
    printf(" --------------- >>> Main App HW_SYS_D\n");

    uint8_t p_err = createThread(THREADS_NUM, mainThreads);
    if (p_err != 0)
    {
        printf("ERROR Create thread\n");
    }
    p_err = 0;
    p_err = joinThread(THREADS_NUM);
    if (p_err != 0)
    {
        printf("ERROR Join thread\n");
    }
}

static void* runMainHandler(void *args)
{
    int i2c_fd;
    open_i2c(&i2c_fd);
    while (1)
    {
        //usleep(50000);
        mainHandler(i2c_fd);
    }
    return EXIT_SUCCESS;
}

static void mainHandler(int i2c_fd)
{
    //printf("================ mainHandler =================\n");
    static uint8_t errorCode = 0;
    static uint8_t resetCount = 0;
    static uint8_t prevResetCount = 0;

    static CRITICAL_EVENT_t main_prevResetState = 0xFF;
    if (criticalEvent != CRITICAL_IDLE)
    {
        if (main_prevResetState != criticalEvent) {
            printf("mainHandler -- Reset state\n");
            main_prevResetState = criticalEvent;
        }
    }
    else {
        errorCode = read_buffer(i2c_fd, pause_sec);
        if (errorCode)
        {
            resetCount++;
        }
        if (prevResetCount != resetCount){

            printf("reset Count %d\n", resetCount);
            prevResetCount = resetCount;
        }
        if (resetCount > MAX_I2C_ERROR)
        {
            ResetState = 1;
            criticalEvent |= iomcu_reset;
        }
    }
    //test();
}

static void* runComHandler(void *args)
{
    int sock_fd, client_fd;
    socket_asyncServer(&sock_fd);
    CLIENT_STATE clientState = SOCKET_READ_OK;
    static CRITICAL_EVENT_t com_prevResetState = reBoot;

    while (1)
    {
        printf("before accept\n");
        accept_socket(sock_fd, &client_fd);
        printf("after accept\n");
        if (client_fd == -1)
        {
            continue;
        }
        fprintf(stderr, "accept_socket\n");
        clientState = clientHandler(client_fd);
        fprintf(stderr, "MAIN_APP clientState = %d\n", clientState);

        if (clientState == SOCKET_RESET)
        {
            printf("clientState == SOCKET_RESET\n");
            criticalEvent |= iomcu_reset;
        }
    }
}

static void* runCriticalHandler(void *args)
{
   while (1)
   {
       uint8_t  crit_pause_sec = 3;
       //usleep(50000);

       static CRITICAL_EVENT_t crit_prevResetState = reBoot;
       static CRITICAL_EVENT_t prev_critEvent = reBoot;

       if (criticalEvent == CRITICAL_IDLE)
       {
           criticalEvent |= criticalHandler(crit_pause_sec);
           if (prev_critEvent != criticalEvent)
           {
               printf("criticalEvent = %d\n", criticalEvent);
               prev_critEvent = criticalEvent;
           }
       }
       else
       {
           if (criticalEvent & iomcu_reset)
           {
               printf("criticalEvent & iomcu_reset\n");
               sleep(1);
               gpio_resetHandler();
           }
           else if (criticalEvent & firstBoot)
           {
               printf("F I R S T  B O O T   &  R E B O O T\n");
               //sleep(5);
           }
           else if (criticalEvent & reBoot)
           {
               printf("R E B O O T\n");
               //sleep(5);
           }
           sleep(5);
           criticalEvent = CRITICAL_IDLE;
           printf("SET CRITICAL IDLE criticalEvent %d \n", criticalEvent);
       }


   }
    return EXIT_SUCCESS;
}

static void getDemonParam(void)
{
    printf("get demon param\n");
}






