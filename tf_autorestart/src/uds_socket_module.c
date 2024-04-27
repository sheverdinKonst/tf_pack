//
// Created by sheverdin on 12/25/23.
//

#include "uds_socket_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "mainHandler.h"

//#define SOCKET_PATH "/var/run/poe_restart.sock"
#define SOCKET_PATH  "/tmp/mysocket.sock"

#define BUFFER_SIZE 1024

static int create_socket(void);
static int bind_socket(void);
static int select_socket(void);
static int accept_socket(void);
static int listen_socket(void);

ssize_t num_bytes = 0;
char buffer[BUFFER_SIZE];
int socket_fd, client_fd;
struct sockaddr_un server_address, client_address;
fd_set listen_fds;
fd_set write_fds;

static int create_socket(void)
{
    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        return (EXIT_FAILURE);
    }
    memset(&server_address, 0, sizeof(server_address));
    server_address.sun_family = AF_UNIX;
    strcpy(server_address.sun_path, SOCKET_PATH);
    return EXIT_SUCCESS;
}

static int bind_socket(void)
{
    if (bind(socket_fd, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        perror("bind");
        close(socket_fd);
        return (EXIT_FAILURE);
    }
    return (EXIT_SUCCESS);
}

static int select_socket(void)
{
    FD_ZERO(&listen_fds);
    //FD_ZERO(&write_fds);
    FD_SET(socket_fd, &listen_fds);
    //FD_SET(socket_fd, &write_fds);
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    int activity = select(socket_fd + 1, &listen_fds, NULL, NULL, NULL);
    return EXIT_SUCCESS;
}

static int listen_socket(void)
{
    if (listen(socket_fd, 5) == -1) {
        perror("listen err");
        return (EXIT_FAILURE);
    }
    perror("listen ok");
    return (EXIT_SUCCESS);
}
static int accept_socket(void)
{
    int err = EXIT_SUCCESS;
    //socklen_t len = sizeof(struct sockaddr_un);
    socklen_t addrlen = sizeof(client_address);
    client_fd = accept(socket_fd, (struct sockaddr *) &client_address, &addrlen);
    if (client_fd == -1) {
        //perror("accept");
        err =  (EXIT_FAILURE);
    }
    perror("accept");
    return err;
}

