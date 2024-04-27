cmake_minimum_required(VERSION 3.25)
project(tf_hwsys C)
set(CMAKE_C_STANDARD 17)
# Add the libraries

#set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_C_FLAGS_DEBUG -g)

add_library(i2c_module
        lib/i2c_lib/i2c_module.c
        lib/i2c_lib/i2c_module.h
        )

add_library(gpio_module
        ./lib/gpio_lib/gpio_module.c
        ./lib/gpio_lib/gpio_module.h
        )

add_library(socket_module
        lib/socket_lib/socket_module.c
        lib/socket_lib/socket_module.h
        )
add_library(utils_module
        lib/utils_lib/utils_module.c
        lib/utils_lib/utils_module.h
)

add_library(thread_module
        lib/thread_lib/thread_module.c
        lib/thread_lib/thread_module.h
)

# Add the include directories
target_include_directories(i2c_module    PUBLIC lib)
target_include_directories(gpio_module   PUBLIC lib)
target_include_directories(socket_module PUBLIC lib)
target_include_directories(utils_module  PUBLIC lib)
target_include_directories(thread_module  PUBLIC lib)


# Create the project_1 executable
add_executable(hwsys_ctrl
        tf_hwsys_ctrl/hwsys_ctrl.c
        tf_hwsys_ctrl/hwsys_ctrl.h
        tf_hwsys_ctrl/global_include.h
        )
target_link_libraries(hwsys_ctrl i2c_module gpio_module socket_module rt gpiod)
INSTALL(TARGETS hwsys_ctrl RUNTIME DESTINATION /usr/bin/)

# Create the project_2 executable
add_executable(tf_hwsys_d
        hwsys_d/hwsys_d.c
        hwsys_d/hwsys_d.h
        hwsys_d/client_comm.c
        hwsys_d/client_comm.h
        hwsys_d/main_app.c
        hwsys_d/main_app.h
        hwsys_d/critical_module.c
        hwsys_d/critical_module.h
        hwsys_d/global_include.h)

#link_directories(/home/sheverdin/Fort_Telecom/t_fortis/openWrt/openwrt/build_dir/target-mips_4kec_musl/libgpiod-1.6.4/lib/)
target_link_libraries(tf_hwsys_d i2c_module gpio_module socket_module utils_module thread_module rt gpiod)
INSTALL(TARGETS tf_hwsys_d RUNTIME DESTINATION /usr/bin/)