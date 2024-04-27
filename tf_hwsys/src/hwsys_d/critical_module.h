//
// Created by sheverdin on 3/27/24.
//

#ifndef TF_HWSYS_CRITICAL_MODULE_H
#define TF_HWSYS_CRITICAL_MODULE_H
#include "global_include.h"

typedef enum
{
    CRITICAL_IDLE   = 0,
    firstBoot       = 0x001,
    reBoot          = 0x002,
    iomcu_reset     = 0x004,
}CRITICAL_EVENT_t;

CRITICAL_EVENT_t criticalHandler(uint8_t pause_sec);


#endif //TF_HWSYS_CRITICAL_MODULE_H
