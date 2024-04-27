#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
//#include <openssl/evp.h>
#include <mbedtls/base64.h>
#include <mbedtls/md5.h>
#include <fcntl.h>

#define errcheck(rc, funcName) \
	if (rc == -1) { perror(funcName); return EXIT_FAILURE; }

#define OUTPUT_MAX_LENGTH 256
#define DM_TOK_BUFSIZE 64

typedef struct
{
    char **tokens;
    uint8_t splitLineMax;
}splited_line_t;

int split_line(char *line, const char* delim, splited_line_t *splitedLine)
{
    int bufsize = DM_TOK_BUFSIZE;
    splitedLine->tokens = malloc(bufsize * sizeof(char*));
    splitedLine->splitLineMax = 0;
    char *token;

    if (!splitedLine->tokens)
    {
        fprintf(stderr, "memory error 1\n");
        return EXIT_FAILURE;
    }
    token = strtok(line, delim);

    while (token != NULL)
    {
        splitedLine->tokens[splitedLine->splitLineMax] = token;
        splitedLine->splitLineMax++;

        if (splitedLine->splitLineMax >= bufsize)
        {
            bufsize += DM_TOK_BUFSIZE;
            splitedLine->tokens = realloc(splitedLine->tokens, bufsize * sizeof(char*));
            if (!splitedLine->tokens)
            {
                fprintf(stderr, "memory error 2\n");
                return EXIT_FAILURE;
            }
        }
        token = strtok(NULL, delim);
    }
    splitedLine->tokens[splitedLine->splitLineMax] = NULL;
    splitedLine->splitLineMax--;
    return EXIT_SUCCESS;
}


static FILE *openPipe()
{
    FILE *pipe = popen("ifconfig", "r");
    if (!pipe)
    {
        printf("Error to run command\n");
    }
    return pipe;
}

static void closePipe(FILE *pipe)
{
    int status = pclose(pipe);
    if (status == -1)
    {
        printf("Error close process\n");
    }
    else if (status != 0)
    {
        printf("any error, Status != 0\n");
    }
}

static unsigned int calculate_md5(unsigned char* inbuf, unsigned int inbuf_size,unsigned char* outbuf)
{
    mbedtls_md5_context md5_ctx;
    mbedtls_md5_init(&md5_ctx);
    mbedtls_md5_starts_ret(&md5_ctx);    
    mbedtls_md5_update_ret(&md5_ctx, inbuf, inbuf_size);
    mbedtls_md5_finish_ret(&md5_ctx, outbuf);
    mbedtls_md5_free(&md5_ctx);
    return 0;   
}

int getMACinfo(char *mac)
{
    char find_interface[] = "switch.11";
    char find_MAC[] = "HWaddr";
    char output[OUTPUT_MAX_LENGTH];
    int interfaceFlag = 0;
    FILE *pipe = openPipe();
    while (fgets(output, OUTPUT_MAX_LENGTH, pipe) != NULL)
    {
        if (strstr(output, find_interface) != NULL)
        {
            interfaceFlag = 1;
        }
        if (strlen(output) == 1)
        {
          interfaceFlag = 0;
        }

        if(interfaceFlag)
        {
            if (strstr(output, find_MAC) != NULL)
            {
                splited_line_t splitLineMAC;
                split_line(output, " ", &splitLineMAC);
                split_line(splitLineMAC.tokens[4], ":", &splitLineMAC);
                
                sprintf(mac,"%02lX%02lX%02lX%02lX%02lX%02lX",strtol(splitLineMAC.tokens[0], NULL, 16),strtol(splitLineMAC.tokens[1], NULL, 16),strtol(splitLineMAC.tokens[2], NULL, 16),
                strtol(splitLineMAC.tokens[3], NULL, 16),strtol(splitLineMAC.tokens[4], NULL, 16),strtol(splitLineMAC.tokens[5], NULL, 16));
                                    
            }
        }
    }
    closePipe(pipe);
    return 0;
}

int main(int argc, char **argv) {
    char buffer[1024];
    char mac[20];
    unsigned char md5[16];

    if(argc < 3){
        printf("DMHASH: incorrect arguments\n");
		return -1;
    }
   
    FILE *fp;
    fp  = fopen ("/etc/dmhash", "w");
    if(fp == NULL){
        printf("DMHASH: error open file\n");
        return -1;
    }

    getMACinfo(mac);

    sprintf(buffer,"%s+%s+%s",mac,argv[1],argv[2]);    

    calculate_md5((unsigned char*)buffer,strlen(buffer),md5);

    char md5_str[128];
    int i=0;
    int index = 0;
    for (i=0; i<16; i++)
        index += sprintf(&md5_str[index], "%02x", md5[i]);
    fputs(md5_str, fp);
    fclose(fp);
	return 0;
}



