//
// Created by sheverdin on 3/21/24.
//

#include "utils_module.h"
#include <time.h>
uint8_t globalDebugMode = 0;

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

void runTimer(uint32_t *timeAlarm, uint8_t delay_sec)
{
    *timeAlarm = time(NULL) + delay_sec;
}


