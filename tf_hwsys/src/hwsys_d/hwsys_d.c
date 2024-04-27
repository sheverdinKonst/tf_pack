//
// Created by sheverdin on 3/19/24.
//

#include "hwsys_d.h"

int main(int argc, char **argv)
{
    printf("Hello from main -- HWSYS_d\n");
    //gpio_test();
    //i2c_test();
    //socket_test();

    // =====================================
    mainInit(argc, argv);
    mainApp();

    return EXIT_SUCCESS;
}
