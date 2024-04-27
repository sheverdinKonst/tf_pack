//
// Created by sheverdin on 12/8/23.
//

#include "portConfig.h"
#include "mainHandler.h"

int main(int argc, char **argv)
{
    openlog("tf_autoresart", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "MAIN TF AUTORESTART - %s", VERSION);
    closelog();
    mainInit(argc, argv);
    mainApp();
    return 0;
}