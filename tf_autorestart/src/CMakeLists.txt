cmake_minimum_required(VERSION 3.26)
project(tf_autorestart C)

set(CMAKE_C_STANDARD 11)

add_executable(tf_autorestart main.c
    mainParam.h
    mainHandler.h
    mainHandler.c
    portConfig.h
    portConfig.c
    utils.h
    utils.c
    ping.c
    ping.h
    uds_socket_module.c
    uds_socket_module.h
)

INSTALL(TARGETS tf_autorestart RUNTIME DESTINATION /usr/bin/)
