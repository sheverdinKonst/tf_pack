#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

#include "registers.h"

#define errcheck(rc, funcName) \
	if (rc == -1) { perror(funcName); return EXIT_FAILURE; }

#define I2C_ADAPTER     "/dev/i2c-0"
#define I2C_ADDR		0x20

static int read_buffer(int fd, unsigned char regaddr, int len, unsigned char *buffer);
static int write_buffer(int fd, unsigned char regaddr, int len, unsigned char *buffer);


int main(int argc, char **argv) {
    int i2c_fd;
	unsigned char read_buf[100] = { 0x00 };
	char msg[100] = {0};
	int serverAddressLen;
	struct sock_msg_t sock_msg;
    static const char serverPath[] = "/var/run/hwsys-uds.sock";
	

	printf("Start I2C Daemon\n");

	int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	errcheck(sock_fd, "socket");
	printf("created socket fd: %d on path %s\n", sock_fd,serverPath);


	struct sockaddr_un serverAddress = {0};
	serverAddress.sun_family = AF_UNIX;
	strcpy(serverAddress.sun_path,serverPath);
	serverAddressLen = sizeof(serverAddress);

	/*
	 * Open I2C file descriptor.
	 */
	i2c_fd = open(I2C_ADAPTER, O_RDWR);
	if (i2c_fd < 0) {
		printf("Unable to open i2c file\n");
		return EXIT_FAILURE;
	}
	printf("open i2c interface\n");

	//bind socket to Path
	int rc = bind(sock_fd,(const struct sockaddr *)(&serverAddress),sizeof(serverAddress));
	if(rc == -1 && errno == EADDRINUSE){
		rc = unlink(serverPath);
		int rc = bind(sock_fd,(const struct sockaddr *)(&serverAddress),sizeof(serverAddress));
		errcheck(rc,"bind");
	}
	else if(rc == -1 ) {
		errcheck(rc,"bind");
	}

	rc = listen(sock_fd,  10);
	errcheck(rc,"listen");

	printf("Start accepting connections\n");

	while (1) {
		int con_fd = accept(sock_fd, NULL, NULL);
		errcheck(con_fd,"accept");
		printf("Connections accepted %d\n",con_fd);
        while(1){
            int bytes = read(con_fd, &sock_msg, sizeof(sock_msg));
            if(bytes <= 0) {
                printf("Connection closed\n");
                break;
            }
            //read i2c
            if(sock_msg.opcode == I2C_OPCODE_READ){
				if (read_buffer(i2c_fd, sock_msg.addr, sock_msg.len, sock_msg.value) == EXIT_SUCCESS) {
					printf("read i2c:  addr = %x, len = %d, value[0]=%x\n", sock_msg.addr,sock_msg.len,sock_msg.value[0]);
                    sock_msg.opcode = I2C_OPCODE_RESPONSE;
				    write(con_fd,&sock_msg,sizeof(sock_msg));
				}

            }
            //write i2c
            else if(sock_msg.opcode == I2C_OPCODE_WRITE){
				if (write_buffer(i2c_fd, sock_msg.addr, sock_msg.len, sock_msg.value) == EXIT_SUCCESS) {
					printf("write i2c:  addr = %x, len = %d, value[0]=%x\n", sock_msg.addr,sock_msg.len,sock_msg.value[0]);
				}
            }
            close(con_fd);
        }
	}

	close(sock_fd);
	unlink(serverPath);





	return 0;
}



//read I2C data
static int read_buffer(int fd, unsigned char regaddr, int len, unsigned char *buffer)
{
        struct i2c_rdwr_ioctl_data data;
        struct i2c_msg messages[2];
        unsigned char regaddr_[2];
        regaddr_[0] = regaddr;
        regaddr_[1] = len;

        /* 
         * .addr - 
         * .flags -  (0 - w, 1 - r)
         * .len - 
         * .buf - 
         */
        messages[0].addr = I2C_ADDR;
        messages[0].flags = 0;//write
        messages[0].len = 2;
        messages[0].buf = regaddr_;

        messages[1].addr = I2C_ADDR;
        messages[1].flags = 1;//read
        messages[1].len = len;
        messages[1].buf = buffer;
 
        data.msgs = messages;
        data.nmsgs = 2;
 
        if (ioctl(fd, I2C_RDWR, &data) < 0){
            printf("Read: cant send data!\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
}

//write I2C data
static int write_buffer(int fd, unsigned char regaddr, int len, unsigned char *buffer)
{
        unsigned char tmp[10];
        struct i2c_rdwr_ioctl_data data;
        struct i2c_msg messages[1];
       
        /* 
         * .addr - 
         * .flags -  (0 - w, 1 - r)
         * .len - 
         * .buf - 
         */


        messages[0].addr = I2C_ADDR;
        messages[0].flags = 0;//write
        messages[0].len = len+2;
        messages[0].buf = tmp;
        messages[0].buf[0] = regaddr;
        messages[0].buf[1] = len;
        for(uint8_t i=0;i<len;i++)
         	messages[0].buf[i+2] = buffer[i]; 
 
        data.msgs = messages;
        data.nmsgs = 1;

        if (ioctl(fd, I2C_RDWR, &data) < 0){
            printf("Write: cant send data!\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
}
