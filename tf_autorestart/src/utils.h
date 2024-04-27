//
// Created by sheverdin on 12/8/23.
//

#ifndef TF_AUTORESTATRT_UTILS_H
#define TF_AUTORESTATRT_UTILS_H

#include "mainParam.h"

TYPE_OF_TEST_t getCmdKey(const char * str);
void getParameters(void);

FILE *openPipe(const char *cmdStr);
void closePipe(FILE *pipe, char *str);
void intToString(int num, char *str);
void checkValidIp(char ipStr[], int *ipIsValid);
error_code_t isValidTime(const char* timeString, struct tm * time);
int getTimeDate(char timeDateStr[32]);

void setTestMode(char testMode);
uint8_t isDebugMode(void);

#endif //TF_AUTORESTATRT_UTILS_H
