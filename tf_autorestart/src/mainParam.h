//
// Created by sheverdin on 12/8/23.
//

#ifndef TF_AUTORESTATRT_MAINPARAM_H
#define TF_AUTORESTATRT_MAINPARAM_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <time.h>

#define VERSION ("3.0.8")

#define WORD_LENGTH         (10)
#define NUM_PORTS           (8+1)
#define OUTPUT_MAX_LENGTH   (64)
#define MAX_ERROR_TEST      (6)
#define MAX_REST            (3)
#define TEST_PERIOD_SEC     (3)
#define MIN_IN_SECOND       (60)
#define MIN_SPEED           (10)
#define MAX_SPEED           (1000000)

#define SOCKET_PATH  "/tmp/mysocket.sock"
#define MAX_CLIENTS 10

typedef enum
{
    false   = 0,
    true    = 1
}bool_t;

typedef enum
{
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
    ERR_OK                      = 0x10000,
    ERR_PORT_SHUTDOWN           = 0x20000,
    ERR_MANUAL_REBOOT           = 0x40000,
    ERR_REBOOTING               = 0x80000
}error_code_t;

typedef enum
{
    argv_path       = 0,
    argv_portRange  = 1,
    argv_cmd        = 2,
    argv_param      = 3
}ARGV_NAME_e;

typedef struct
{
    uint8_t test_delay;
    uint8_t test_num;
    uint8_t max_reset;
    uint32_t mainTime;
}DEMON_PARAM_t;

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
    CMD_STATE       = 0,
    CMD_HOST        = 1,
    CMD_SPEED       = 2,
    CMD_TIME        = 3,
    CMD_IDLE_1      = 4,
    CMD_IDLE_2      = 5,
    MAX_CMD         = 6
}TYPE_OF_CONFIG_t;

#endif //TF_AUTORESTATRT_MAINPARAM_H
