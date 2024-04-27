//
// Created by sheverdin on 22/12/23.
//
#include "mainParam.h"
#include "uds_socket_module.h"
#include "parsing_command.h"

#define VERSION ("2.0.4")

int main(int argc, char **argv)
{
    //printf("tf_poe_restart_ctrl VERSION: %s\n", VERSION);
    uint8_t  errorCode = 0;
    if (argc < 2 ){
        errorCode = 2;
        //printf("Need 1 arguments: status or reboot \"portnum\"\n");
    }else if (argc == 2) {
        errorCode = main_socket(argv[1], NULL);
    } else if (argc == 3){
        errorCode = main_socket(argv[1], argv[2]);
    }
    return errorCode;
}