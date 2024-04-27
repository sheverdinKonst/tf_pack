//
// Created by sheverdin on 3/27/24.
//

#include "critical_module.h"
#include "syslog.h"

static void sent_log(const char *name, const char *errorName, REGISTER_ADDR  addr, uint32_t value);

CRITICAL_EVENT_t criticalHandler(uint8_t pause_sec)
{
    I2C_ERROR_t i2c_error;
    CRITICAL_EVENT_t criticalEvent = 0;
    uint8_t count = 0;
    i2c_error.event = 0;
    i2c_error.addr = 0;
    i2c_error.value = 0;

    i2c_critical_handler(&i2c_error, pause_sec);


    if (i2c_error.event ==  I2C_CRITICAL_ERROR)
    {
        criticalEvent |= iomcu_reset;
        sent_log(i2c_error.name, i2c_error.errorName, i2c_error.addr, i2c_error.value);
    }
    else if (i2c_error.event ==  I2C_OVER_MAX)
    {
        criticalEvent |= CRITICAL_IDLE;
        sent_log(i2c_error.name, i2c_error.errorName, i2c_error.addr, i2c_error.value);
    }
    else if (i2c_error.event ==  I2C_LESS_MIN){
        criticalEvent |= CRITICAL_IDLE;
        sent_log(i2c_error.name, i2c_error.errorName, i2c_error.addr, i2c_error.value);
    }
    else if (i2c_error.event == I2C_CHANGED_TO_CLOSE) {
        criticalEvent |= CRITICAL_IDLE;
        sent_log(i2c_error.name, i2c_error.errorName, i2c_error.addr, i2c_error.value);
    }
    else if (i2c_error.event == I2C_CHANGED_TO_OPEN) {
        criticalEvent |= CRITICAL_IDLE;
        sent_log(i2c_error.name, i2c_error.errorName, i2c_error.addr, i2c_error.value);
    }
    else if (i2c_error.event == I2C_LONG_PRESSED) {
        printf("F I R S T _ B O O T  and  R E B O O T\n");
        sent_log(i2c_error.name, i2c_error.errorName, i2c_error.addr, i2c_error.value);
        //criticalEvent = CRITICAL_IDLE;
        criticalEvent |= iomcu_reset;
        criticalEvent |= firstBoot;
        criticalEvent |= reBoot;
        return criticalEvent;
    }
    else if (i2c_error.event == I2C_CHANGED_TO_VAC || i2c_error.event == I2C_CHANGED_TO_BAT)
    {
        sent_log(i2c_error.name, i2c_error.errorName, i2c_error.addr, i2c_error.value);
        criticalEvent |= CRITICAL_IDLE;
    }
    //printf("criticalHandler criticalEvent = %d\n", criticalEvent);
    return criticalEvent;
}

static void sent_log(const char *name, const char *errorName, REGISTER_ADDR  addr, uint32_t value)
{
    //openlog(name, LOG_PID, LOG_USER);
    //syslog(LOG_ERR, error, "REG addr %d, value %d", addr, value);
    //closelog();

    printf("L O G -> %s - event - %s -- reg.addr - %d -- value: %d\n", name, errorName, addr, value);
}
































































































































































