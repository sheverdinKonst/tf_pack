//
// Created by sheverdin on 12/25/23.
//

#include "uds_socket_module.h"
#include "parsing_command.h"

typedef struct serverAddress serverAddress;

static int create_socket(void);
static int connect_socket(void);
static int write_socket(CMD_TYPE_e cmdType, uint8_t portNum);
static int read_socket(CMD_TYPE_e cmd);

#define SOCKET_PATH "/tmp/mysocket.sock"

int sock_fd = -1;
uint8_t main_socket(char *cmdStr, char *portStr)
{
    CMD_TYPE_e cmdType;
    uint8_t portNum;
    uint8_t errorCode = 0;
    getCmdType(cmdStr, &cmdType);

    if (portStr == NULL) {
        portNum = 0;
    }else{
        errorCode = getPortNum(portStr, &portNum);
    }
    if(cmdType == cmd_idle || errorCode == 4) {
        errorCode |= 1;
    } else {
        create_socket();
        connect_socket();
        // TODO Konst need to add check if argument reboot with  port num, status without port
        write_socket(cmdType, portNum);
        read_socket(cmdType);
    }
    return errorCode;
}

///////////////////////////////////////////////////////////////////////////

static int create_socket(void)
{
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        perror("create_socket() failed");
        return 1;
    } else {
        //printf("socket created\n");
    }
    return 0;
}

static int connect_socket(void)
{
    struct sockaddr_un server_sockaddr;
    memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));
    server_sockaddr.sun_family = AF_UNIX;
    strcpy(server_sockaddr.sun_path, SOCKET_PATH);

    int rc = connect(sock_fd, (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr));
    
    if (rc < 0) {
        //perror("connect_socket() failed");
        close(sock_fd);
        return 1;
    }
    //printf("Connection to server successful\n");
    return 0;
}

static int write_socket(CMD_TYPE_e cmd, uint8_t portNum)
{
    sock_msg_t sockMsg =
    {
        .cmdType = cmd,
        .port    = portNum
    };
    //printf("data for Send  cmd type = %d port num = %d\n", sockMsg.cmdType, sockMsg.port);
    ssize_t num_bytes_sent = write(sock_fd,  &sockMsg,sizeof(sockMsg));
    if (num_bytes_sent == -1) {
        //perror("error write status");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int read_socket(CMD_TYPE_e cmd)
{
    resetPort_t resetPort[NUM_PORTS];
    //printf("READ FROM SOCKET ------------------------->\n");
    if (cmd == cmd_status)
    {
        ssize_t receivedSize = recv(sock_fd, &resetPort, sizeof(resetPort), 0);
        if (receivedSize < 0)
        {
            //perror("read_socket() status failed");
            close(sock_fd);
            return -1;
        } else
        {
            printBufferJson(resetPort);
        }
    }
    else if (cmd == cmd_reboot)
    {
        char buf[128];
        ssize_t receivedSize = recv(sock_fd, &buf, sizeof(buf), 0);
        if (receivedSize < 0)
        {
            //perror("read_socket() reboot failed");
            close(sock_fd);
            return -1;
        } else
        {
            //printf("msg from server: %s\n", buf);
        }
    }
    return 0;
}






