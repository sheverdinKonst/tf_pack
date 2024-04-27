//
// Created by sheverdin on 12/8/23.
//

#ifndef TF_POE_RESTART_MAINPARAM_H
#define TF_POE_RESTART_MAINPARAM_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>


#define WORD_LENGTH         (10)
#define NUM_PORTS           (8+1)
#define OUTPUT_MAX_LENGTH   (64)
#define MAX_ERROR_TEST      (6)
#define MAX_REST            (3)
#define TEST_PERIOD_SEC     (3)
#define MIN_IN_SECOND       (60)
#define MIN_SPEED           (10)
#define MAX_SPEED           (1000000)

typedef enum
{
    false   = 0,
    true    = 1
}bool_t;

typedef enum
{
    IDLE_EVENT       = 0x0000,
    FAST_TIMER       = 0x0001,
    CRITICAL_VALUE   = 0x0002,
    CRITICAL_ALARM   = 0x0004,
    CANCELED_ALARM   = 0x0008,
    RESTART          = 0x0010,
    POWER_ON         = 0x0020,
}AR_EVENT;

typedef enum
{
    IDLE_STATE      = 0,
    NORMAL          = 1,
    TEST            = 2,
    FROZEN          = 3,
}AR_STATE;

typedef enum
{
    ERR_OK                      = 0x10000,
    ERR_PORT_DISABLE            = 0x00001,
    ERR_TEST_TYPE               = 0x00002,
    ERR_TEST_LINK               = 0x00004,
    ERR_TEST_PING               = 0x00008,
    ERR_TEST_SPEED              = 0x00010,
    ERR_TIME_ALARM              = 0x00020,
    ERR_TEST_DISABLE            = 0x00040,
    ERR_OBJ_NOT_FOUND           = 0x00080,
    ERR_UNAVAILABLE_RESOURCE    = 0x00100,
    ERR_NULL_OBJ                = 0x00200,
    ERR_CREATE_THREAD           = 0x00400,
    ERR_JOIN_THREAD             = 0x00800,
    ERR_IP_NOT_VALID            = 0x01000,
    ERR_REG_COMPILE             = 0x02000,
    ERR_TIME_NOT_VALID          = 0x04000,
    ERR_SPEED_VALUE             = 0x08000,
    ERR_PORT_SHUTDOWN           = 0x20000,
    ERR_MANUAL_REBOOT           = 0x40000,
    ERR_REBOOTING               = 0x80000
}error_code_t;

typedef enum
{
    cmd_idle    = 0,
    cmd_status  = 1,
    cmd_reboot  = 2,
    max_cmd     = 3
}CMD_TYPE_e;

typedef struct
{
    uint8_t time_H;
    uint8_t time_M;
    uint32_t targetTime;
    uint32_t remainTime;
}time_h_m;

typedef enum
{
    disable   = 0,
    enable    = 1
}PORT_STATE_e;

typedef enum
{
    test_disable    = 0,
    test_link       = 1,
    test_ping       = 2,
    test_speed      = 3,
    test_time       = 4,
    test_max        = 5
}TYPE_OF_TEST_t;

typedef enum
{
    time_up         = 0,
    time_down       = 1,
    time_alarm_max  = 2
}TIME_ALARM_INDEX_e;

typedef struct
{
    TYPE_OF_TEST_t  test;
    uint8_t         portNum;
    uint8_t         resetCount;
    uint8_t         totalResetCount;
    uint8_t         errorTestCount;
    char            host[16];
    time_h_m        timeAlarm[time_alarm_max];
    //time_h_m        timeUp;
    //time_h_m        timeDown;
    uint32_t        min_speed;
    uint32_t        max_speed;
    uint32_t        reply_time;
    uint32_t        rx_byte_prev;
    uint32_t        rx_byte_current;
    uint32_t        rx_delta_byte;
    uint32_t        rx_speed_Kbit;
    uint32_t        time_current;
    uint32_t        time_prev;
    uint32_t        tx_byte_prev;
    uint32_t        tx_byte_current;
    uint32_t        tx_delta;
    PORT_STATE_e    linkState;
    PORT_STATE_e    port_state;
    PORT_STATE_e    poe_state;
    error_code_t    errorCode;
}paramPort_t;

typedef struct
{
    error_code_t    errorCode;
    uint8_t         portNum;
    uint8_t         resetCount;
    uint32_t        time;
    char            timeStr[32];
    TYPE_OF_TEST_t  testType;
}resetPort_t;

#endif //TF_POE_RESTART_MAINPARAM_H
