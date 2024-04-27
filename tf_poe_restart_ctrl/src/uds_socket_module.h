//
// Created by sheverdin on 12/25/23.
//

#ifndef TF_POE_RESTART_CTRL_UDS_SOCKET_MODULE_H
#define TF_POE_RESTART_CTRL_UDS_SOCKET_MODULE_H

#include "mainParam.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>

#define IF_CMP(x) if(strcmp(x,name)==0)

#define errcheck(rc, funcName) \
	if (rc == -1) { perror(funcName); return EXIT_FAILURE; }

typedef struct
{
    CMD_TYPE_e cmdType;
    uint8_t port;
}sock_msg_t;

uint8_t main_socket(char *cmdStr, char *port);

#endif //TF_POE_RESTART_CTRL_UDS_SOCKET_MODULE_H
