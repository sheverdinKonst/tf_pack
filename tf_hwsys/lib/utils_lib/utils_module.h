//
// Created by sheverdin on 3/21/24.
//


#ifndef TF_HWSYS_UTILS_MODULE_H
#define TF_HWSYS_UTILS_MODULE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

void setTestMode(char testMode);
uint8_t isDebugMode(void);
void runTimer(uint32_t *timeNow, uint8_t delay_sec);


#endif //TF_HWSYS_UTILS_MODULE_H
