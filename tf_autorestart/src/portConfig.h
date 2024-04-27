//
// Created by sheverdin on 12/8/23.
//

#ifndef TF_AUTORESTATRT_PORTCONFIG_H
#define TF_AUTORESTATRT_PORTCONFIG_H

#include "mainParam.h"

typedef enum
{
    disable = 0,
    enable  = 1
}PORT_STATE_e;

typedef enum
{
    time_up         = 0,
    time_down       = 1,
    time_alarm_max  = 2
}TIME_ALARM_INDEX_e;

typedef struct
{
    uint8_t time_H;
    uint8_t time_M;
    uint32_t targetTime;
    uint32_t remainTime;
}time_h_m;

typedef struct
{
    TYPE_OF_TEST_t  test;
    uint8_t         portNum;
    uint8_t         resetCount;
    uint8_t         totalResetCount;
    uint8_t         errorTestCount;
    char            host[16];
    char            timeStr[32];
    time_h_m        timeAlarm[time_alarm_max];
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

typedef enum
{
    IDLE_STATE       = 0,
    REGULAR_SATE     = 1,
    REBOOT_STATE     = 2
}AR_STATE;

typedef enum
{
    IDLE_EVENT       = 0x0000,
    FAST_TIMER       = 0x0001,
    CRITICAL_VALUE   = 0x0002,
    CRITICAL_ALARM   = 0x0004,
    CANCELED_ALARM   = 0x0008,
    AUTO_RESTART          = 0x0010,
    POWER_ON         = 0x0020,
    MANUAL_RESTART   = 0x0040,
}AR_EVENT;

typedef struct
{
    paramPort_t paramPort;
    AR_EVENT event;
    AR_STATE state;
}portInfo_t;

typedef struct
{
    error_code_t    errorCode;
    uint8_t         portNum;
    uint8_t         resetCount;
    uint32_t        time;
    char            timeStr[32];
    TYPE_OF_TEST_t  testType;
}resetPort_t;

typedef enum
{
    POE_UP      = 0x01,
    POE_DOWN    = 0x02,
    POE_RESTART = 0x03
}POE_CONTROL;

typedef enum
{
    REBOOT_STATE_IDLE        = 0,
    REBOOT_STATE_INIT        = 1,
    REBOOT_STATE_POE_DOWN    = 2,
    REBOOT_STATE_POE_UP      = 3
}REBOOT_STATE_e;

typedef enum
{
    REBOOT_EVENT_IDLE         = 0x00,
    REBOOT_EVENT_START        = 0x01,
    REBOOT_EVENT_TIMER_START  = 0x02,
    REBOOT_EVENT_TIMER_STOP   = 0x04,
    REBOOT_EVENT_DONE         = 0x08
}REBOOT_EVENT_e;

typedef struct
{
    REBOOT_STATE_e  rebootState;
    REBOOT_EVENT_e  rebootEvent;
    uint32_t        timeDelay;
    uint32_t        timeStartReboot;
}REBOOT_PARAM_t;

void port_setDisableParam(paramPort_t *paramPort);
void port_setLinkParam(paramPort_t *paramPort);
void port_setHostParam(paramPort_t *paramPort);
void port_setSpeedParam(paramPort_t *paramPort);
void port_setTimeParam(paramPort_t *paramPort);

void port_runTestDisable(paramPort_t *paramPort);
void port_runTestLink(paramPort_t *paramPort);
void port_runTestPing(paramPort_t *paramPort);
void port_runTestSpeed(paramPort_t *paramPort);
void port_runTestTime(paramPort_t *paramPort);

AR_STATE get_PortAndPoeState(paramPort_t *paramPort);

void rebootHandler(portInfo_t *portInfo, DEMON_PARAM_t *demonParam);
void rebootParamInit(void);
void poe_Control(uint8_t port, POE_CONTROL poeControl);


#endif //TF_AUTORESTATRT_PORTCONFIG_H
