cmake_minimum_required(VERSION 3.26)
project(dm_client C)

set(CMAKE_C_STANDARD 11)

add_executable(dm_client main.c
                         net_utils.c
                         net_utils.h
                         dm_mainHeader.h
                         run_CLIcmd.c
                         run_CLIcmd.h
                         parsing.c
                         parsing.h
                         state_handler.c
                         state_handler.h
                         settings_module.c
                         settings_module.h
                        )

INSTALL(TARGETS dm_client RUNTIME DESTINATION /usr/bin/)
