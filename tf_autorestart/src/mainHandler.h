//
// Created by sheverdin on 12/8/23.
//

#ifndef TF_AUTORESTATRT_MAINHANDLER_H
#define TF_AUTORESTATRT_MAINHANDLER_H

#include "mainParam.h"
#include "portConfig.h"

//typedef struct
//{
//    paramPort_t paramPort;
//    AR_EVENT event;
//    AR_STATE state;
//}portInfo_t;

typedef enum
{
    cmd_idle    = 0,
    cmd_status  = 1,
    cmd_reboot  = 2,
    max_cmd = 3
}CMD_TYPE_e;

typedef struct
{
    CMD_TYPE_e cmdType;
    uint8_t port;
}sock_msg_t;

typedef void *(*runTest_t)(paramPort_t *paramPort);
typedef void (*setTestConfig_t)(uint8_t portNum);

void mainApp(void);
void mainInit(int argc, char **argv);
void mainHandler(void);

#endif //TF_AUTORESTATRT_MAINHANDLER_H
