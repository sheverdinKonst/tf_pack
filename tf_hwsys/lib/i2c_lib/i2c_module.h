//
// Created by sheverdin on 3/19/24.
//

#ifndef TF_HWSYS_I2C_MODULE_H
#define TF_HWSYS_I2C_MODULE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>

#define I2C_ADAPTER                     ("/dev/i2c-0")
#define I2C_ADDR		                (0x20)

#define I2C_NAME_MAX_LEN                (24)
#define I2C_ERROR_NAME_MAX_LEN          (24)
#define DELAY_START                     (10)

#define STABLE_VALUE                    (10)
#define REG_ADC_CH1_regular             (3300)
#define REG_ADC_CH1_max                 (3500)
#define REG_ADC_CH1_min                 (3000)

#define REG_ADC_CH2_regular             (1500)
#define REG_ADC_CH2_max                 (1700)
#define REG_ADC_CH2_min                 (1300)

#define REG_ADC_CH3_regular             (1100)
#define REG_ADC_CH3_max                 (1300)
#define REG_ADC_CH3_min                 (1000)
#define REG_TAMPER_open                 (0)
#define REG_TAMPER_close                (1)
#define REG_SENSOR_open                 (0)
#define REG_SENSOR_close                (1)
#define REG_DEFAULT_LONG_PRESSED_flag   (1)
#define REG_DEFAULT_LONG_PRESSED_reset  (0x475D)

#define REG_RPS_VAC_vac                 (1)
#define REG_RPS_VAC_battary             (0)
#define REG_RPS_BAT_VOLTAGE_nobattery   (0)      // TODO
#define REG_RPS_BAT_VOLTAGE_regular     (25000)      // TODO
#define REG_RPS_BAT_VOLTAGE_max         (30000)
#define REG_RPS_BAT_VOLTAGE_min         (20500)
#define REG_RPS_CHRG_VOLTAGE_regular    (26600)
#define REG_RPS_CHRG_VOLTAGE_max        (30000)
#define REG_RPS_CHRG_VOLTAGE_min        (20500)
#define REG_RPS_BAT_CURRENT_regular     (8)      // TODO
#define REG_RPS_BAT_CURRENT_max         (14)      // TODO
#define REG_RPS_BAT_CURRENT_min         (0)      // TODO
#define REG_RPS_TEMPER_regular          (28)      // TODO
#define REG_RPS_TEMPER_max              (85)      // TODO
#define REG_RPS_TEMPER_min              (-60)      // TODO

#define  REG_SHT_CONNECTED_yes          (1)
#define  REG_SHT_CONNECTED_no           (0)
#define  REG_SHT_TYPE_name              ("SHT-01")
#define  REG_SHT_TEMPERATURE_regular    (220)
#define  REG_SHT_TEMPERATURE_max        (500)
#define  REG_SHT_TEMPERATURE_min        (-600)
#define REG_SHT_HUMIDITY_regular        (50)
#define REG_SHT_HUMIDITY_max            (98)
#define REG_SHT_HUMIDITY_min             (0)

#define  REG_SFP_PRESENT_yes           (1)
#define  REG_SFP_PRESENT_no            (0)
#define  REG_SFP_LOS_yes               (1)
#define  REG_SFP_LOS_no                (0)
#define  REG_SFP_TEMPER_norm           (23)  //TODO check range value
#define  REG_SFP_TEMPER_max            (50)
#define  REG_SFP_TEMPER_min            (20)
#define  REG_SFP_VOLTAGE_norm          (3300)
#define  REG_SFP_VOLTAGE_max           (3600)
#define  REG_SFP_VOLTAGE_min           (3000)

#define REG_SFP_CURRENT_normal         (950)
#define REG_SFP_CURRENT_max            (1000)
#define REG_SFP_CURRENT_min            (900)
#define REG_SFP_TX_BIAS_normal         (32844)
#define REG_SFP_TX_BIAS_max            (33500)
#define REG_SFP_TX_BIAS_min            (32000)
#define REG_SFP_TX_POWER_normal        (60)
#define REG_SFP_TX_POWER_max           (65)
#define REG_SFP_TX_POWER_min           (55)
#define REG_SFP_RX_POWER_normal        (60)
#define REG_SFP_RX_POWER_max           (65)
#define REG_SFP_RX_POWER_min           (55)


typedef enum {
    I2C_OVER_MAX         = 1,
    I2C_LESS_MIN         = 2,
    I2C_LONG_PRESSED     = 3,
    I2C_CHANGED_TO_VAC   = 4,
    I2C_CHANGED_TO_BAT   = 5,
    I2C_CHANGED_TO_OPEN  = 6,
    I2C_CHANGED_TO_CLOSE = 7,
    I2C_ERR_CHANGE_VALUE = 8,
    I2C_ERR_OK           = 9,
    I2C_CRITICAL_ERROR   = 10,
    I2C_MAX_EVENT        = 11,
}I2C_EVENT_t;

typedef  enum  {
    // MAIN
    REG_INTSTAT	             = 0,
    REG_INTMASK	             = 1,
    REG_HW_VERS	             = 2,
    REG_SW_VERS	             = 3,
    REG_ADC_CH1	             = 4,
    REG_ADC_CH2	             = 5,
    REG_ADC_CH3	             = 6,
    REG_IDLE7	             = 7,
    REG_IDLE8	             = 8,
    REG_IDLE9	             = 9,

    // SENSORS
    REG_TAMPER			     = 10,
    REG_SENSOR1			     = 11,
    REG_SENSOR2			     = 12,
    REG_RELAY1			     = 13,
    REG_DEFAULT_BUTTON	     = 14,
    REG_DEFAULT_LED		     = 15,
    REG_DEFAULT_LONG_PRESSED = 16,
    REG_IDLE17               = 17,
    REG_IDLE18               = 18,
    REG_IDLE19               = 19,
    // UPS
    REG_RPS_CONNECTED	     = 20,
    REG_RPS_HW_VERS		     = 21,
    REG_RPS_SW_VERS		     = 22,
    REG_RPS_VAC			     = 23,
    REG_RPS_BAT_VOLTAGE	     = 24,
    REG_RPS_CHRG_VOLTAGE     = 25,
    REG_RPS_BAT_CURRENT	     = 26,
    REG_RPS_TEMPER		     = 27,
    REG_RPS_LED_STATE	     = 28,
    REG_RPS_BAT_KEY		     = 29,
    REG_RPS_CHRG_KEY	     = 30,
    REG_RPS_REL_STATE	     = 31,
    REG_RPS_MIN_VOLTAGE	     = 32,
    REG_RPS_DISCH_VOLTAGE    = 33,
    REG_RPS_REMAIN_TIME	     = 34,
    REG_RPS_TEST_OK		     = 35,
    REG_RPS_CPU_ID		     = 36,
    REG_RPS_LTC4151_OK	     = 37,
    REG_RPS_ADC_BAT_VOLT     = 38,
    REG_RPS_ADC_BAT_CURR     = 39,
    REG_RPS_TEST_MODE	     = 40,
    REG_IDLE41               = 41,
    REG_IDLE42               = 42,
    REG_IDLE43               = 43,
    REG_IDLE44               = 44,
    REG_IDLE45               = 45,

    
    REG_IDLE46               = 46,
    REG_IDLE47               = 47,
    REG_IDLE48               = 48,
    REG_IDLE49               = 49,
    // SHT
    REG_SHT_CONNECTED		 = 50,
    REG_SHT_TYPE			 = 51,
    REG_SHT_TEMPERATURE		 = 52,
    REG_SHT_HUMIDITY		 = 53,
    REG_IDLE54               = 54,
    REG_IDLE55               = 55,
    REG_IDLE56               = 56,
    REG_IDLE57               = 57,
    REG_IDLE58               = 58,
    REG_IDLE59               = 59,
    // SPF_1
    REG_SFP1_PRESENT	     = 60,
    REG_SFP1_LOS		     = 61,
    REG_SFP1_VENDOR		     = 62,
    REG_SFP1_VENDOR_OUI	     = 63,
    REG_SFP1_VENDOR_PN	     = 64,
    REG_SFP1_VENDOR_REV	     = 65,
    REG_SFP1_IDENTIFIER	     = 66,
    REG_SFP1_CONNECTOR	     = 67,
    REG_SFP1_TYPE		     = 68,
    REG_SFP1_LINK_LEN	     = 69,
    REG_SFP1_FIBER_TEC	     = 70,
    REG_SFP1_MEDIA		     = 71,
    REG_SFP1_SPEED		     = 72,
    REG_SFP1_ENCODING	     = 73,
    REG_SFP1_WAVELEN	     = 74,
    REG_SFP1_NBR		     = 75,
    REG_SFP1_LEN9		     = 76,
    REG_SFP1_LEN50		     = 77,
    REG_SFP1_LEN62		     = 78,
    REG_SFP1_LENC		     = 79,
    REG_SFP1_TEMPER		     = 80,
    REG_SFP1_VOLTAGE	     = 81,
    REG_SFP1_CURRENT	     = 82,
    REG_SFP1_TX_BIAS	     = 83,
    REG_SFP1_TX_POWER	     = 84,
    REG_SFP1_RX_POWER	     = 85,
    REG_IDLE86               = 86,
    REG_IDLE87               = 87,
    REG_IDLE88               = 88,
    REG_IDLE89               = 89,
    // SPF_2
    REG_SFP2_PRESENT	     = 90,
    REG_SFP2_LOS		     = 91,
    REG_SFP2_VENDOR		     = 92,
    REG_SFP2_VENDOR_OUI	     = 93,
    REG_SFP2_VENDOR_PN	     = 94,
    REG_SFP2_VENDOR_REV	     = 95,
    REG_SFP2_IDENTIFIER	     = 96,
    REG_SFP2_CONNECTOR	     = 97,
    REG_SFP2_TYPE		     = 98,
    REG_SFP2_LINK_LEN	     = 99,
    REG_SFP2_FIBER_TEC	     = 100,
    REG_SFP2_MEDIA		     = 101,
    REG_SFP2_SPEED		     = 102,
    REG_SFP2_ENCODING	     = 103,
    REG_SFP2_WAVELEN	     = 104,
    REG_SFP2_NBR		     = 105,
    REG_SFP2_LEN9		     = 106,
    REG_SFP2_LEN50		     = 107,
    REG_SFP2_LEN62		     = 108,
    REG_SFP2_LENC		     = 109,
    REG_SFP2_TEMPER		     = 110,
    REG_SFP2_VOLTAGE	     = 111,
    REG_SFP2_CURRENT	     = 112,
    REG_SFP2_TX_BIAS	     = 113,
    REG_SFP2_TX_POWER	     = 114,
    REG_SFP2_RX_POWER	     = 115,
    REG_IDLE116               = 116,
    REG_IDLE117               = 117,
    REG_IDLE118               = 118,
    REG_IDLE119               = 119,
    // RTC
    REG_RTC_STATUS		      = 120,
    REG_RTC_YEAR		      = 121,
    REG_RTC_MONTH		      = 122,
    REG_RTC_DAY			      = 123,
    REG_RTC_WEEKDAY		      = 124,
    REG_RTC_HOUR		      = 125,
    REG_RTC_MINUTE		      = 126,
    REG_RTC_SECOND		      = 127,
    REG_IDLE128               = 128,
    REG_IDLE129               = 129,

    // POE
    REG_POE_ID				  = 130,
    REG_POE_STATE			  = 131,
    REG_POE_BANK			  = 132,
    REG_POE_MODE			  = 133,

    MAX_SENSORS,
    SENSOR_IDLE
}REGISTER_ADDR;

typedef struct {
    I2C_EVENT_t     event;
    REGISTER_ADDR   addr;
    uint32_t        value;
    char            name[I2C_NAME_MAX_LEN];
    char            errorName[I2C_ERROR_NAME_MAX_LEN];
}I2C_ERROR_t;

typedef I2C_EVENT_t (*criticalHandler_t)(I2C_ERROR_t *i2cError);

typedef enum {
    TYPE_UINT           = 1,
    TYPE_PSEUDO_FLOAT	= 2,
    TYPE_IDLE       	= 3
}I2C_TYPE;

typedef enum {
    I2C_OPCODE_READ 	= 1,
    I2C_OPCODE_WRITE	= 2,
    I2C_OPCODE_RESPONSE	= 3,
    I2C_OPCODE_RESET    = 4,
    I2C_OPCODE_IDLE     = 5
}I2C_OPCODE;

typedef enum {
    regular = 0,
    critical = 1,
}CRITICAL_TYPE;

typedef struct
{
    unsigned char lenMSG;
    REGISTER_ADDR addr;
    I2C_OPCODE opcode;
    unsigned char lenData;
    I2C_TYPE type;
    CRITICAL_TYPE isCritical;
    criticalHandler_t criticalHandler;
    unsigned char value[32];
}i2c_data_t;

typedef union
{
    i2c_data_t i2c_data;
    unsigned char socketMsg[60];
}i2c_param;

void i2c_test(void);

REGISTER_ADDR get_i2c_addr_by_name(char *reg_name,  i2c_param *msg);
void open_i2c(int *i2c_fd);
uint8_t read_buffer(int fd, uint8_t pause_sec);
int write_buffer(int fd);
void i2c_getData(i2c_data_t *i2CData, REGISTER_ADDR addr, int  len);
int i2c_get_MsgSize(void);

// void critical_REG_INTSTAT (void);
// void critical_REG_INTMASK (void);
// void critical_REG_HW_VERS (void);
// void critical_REG_SW_VERS (void);
I2C_EVENT_t critical_REG_ADC_CH1 (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_ADC_CH2 (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_ADC_CH3 (I2C_ERROR_t *i2cError);

// SENSORS
I2C_EVENT_t critical_REG_TAMPER                (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SENSOR1               (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SENSOR2               (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_RELAY1                (I2C_ERROR_t *i2cError);
//void critical_REG_DEFAULT_BUTTON        (void);
//void critical_REG_DEFAULT_LED           (void);
I2C_EVENT_t critical_REG_DEFAULT_LONG_PRESSED  (I2C_ERROR_t *i2cError);

// UPS
// void critical_REG_RPS_CONNECTED          (void);
// void critical_REG_RPS_HW_VERS            (void);
// void critical_REG_RPS_SW_VERS            (void);
I2C_EVENT_t critical_REG_RPS_VAC           (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_RPS_BAT_VOLTAGE   (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_RPS_CHRG_VOLTAGE  (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_RPS_BAT_CURRENT   (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_RPS_TEMPER        (I2C_ERROR_t *i2cError);
//void critical_REG_RPS_LED_STATE          (void);
//void critical_REG_RPS_BAT_KEY            (void);
//void critical_REG_RPS_CHRG_KEY           (void);
//void critical_REG_RPS_REL_STATE          (void);
//void critical_REG_RPS_MIN_VOLTAGE        (void);
//void critical_REG_RPS_DISCH_VOLTAGE      (void);
//void critical_REG_RPS_REMAIN_TIME        (void);
//void critical_REG_RPS_TEST_OK            (void);
//void critical_REG_RPS_CPU_ID             (void);
//void critical_REG_RPS_LTC4151_OK         (void);
//void critical_REG_RPS_ADC_BAT_VOLT       (void);
//void critical_REG_RPS_ADC_BAT_CURR       (void);
//void critical_REG_RPS_TEST_MODE          (void);

// SHT
// void critical_REG_SHT_CONNECTED (void);
I2C_EVENT_t critical_REG_SHT_TYPE         (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SHT_CONNECTED(I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SHT_TEMPERATURE  (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SHT_HUMIDITY     (I2C_ERROR_t *i2cError);

// SPF_1
I2C_EVENT_t critical_REG_SFP1_PRESENT     (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP2_PRESENT     (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP1_LOS         (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP2_LOS         (I2C_ERROR_t *i2cError);
//void critical_REG_SFP_VENDOR            (void);
//void critical_REG_SFP_VENDOR_OUI        (void);
//void critical_REG_SFP_VENDOR_PN         (void);
//void critical_REG_SFP_VENDOR_REV        (void);
//void critical_REG_SFP_IDENTIFIER        (void);
//void critical_REG_SFP_CONNECTOR         (void);
//void critical_REG_SFP_TYPE              (void);
//void critical_REG_SFP_LINK_LEN          (void);
//void critical_REG_SFP_FIBER_TEC         (void);
//void critical_REG_SFP_MEDIA             (void);
//void critical_REG_SFP_SPEED             (void);
//void critical_REG_SFP_ENCODING          (void);
//void critical_REG_SFP_WAVELEN           (void);
//void critical_REG_SFP_NBR               (void);
//void critical_REG_SFP_LEN9              (void);
//void critical_REG_SFP_LEN50             (void);
//void critical_REG_SFP_LEN62             (void);
//void critical_REG_SFP_LENC              (void);
I2C_EVENT_t critical_REG_SFP1_TEMPER      (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP2_TEMPER      (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP1_VOLTAGE     (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP2_VOLTAGE     (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP1_CURRENT     (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP2_CURRENT     (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP1_TX_BIAS     (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP2_TX_BIAS     (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP1_TX_POWER    (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP2_TX_POWER    (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP1_RX_POWER    (I2C_ERROR_t *i2cError);
I2C_EVENT_t critical_REG_SFP2_RX_POWER    (I2C_ERROR_t *i2cError);

// RTC
// void critical_REG_RTC_STATUS             (void);
// void critical_REG_RTC_YEAR               (void);
// void critical_REG_RTC_MONTH              (void);
// void critical_REG_RTC_DAY                (void);
// void critical_REG_RTC_WEEKDAY            (void);
// void critical_REG_RTC_HOUR               (void);
// void critical_REG_RTC_MINUTE             (void);
// void critical_REG_RTC_SECOND             (void);
//
// // POE
// void critical_REG_POE_ID                 (void);
// void critical_REG_POE_STATE              (void);
// void critical_REG_POE_BANK               (void);
// void critical_REG_POE_MODE               (void);

void i2c_critical_handler(I2C_ERROR_t *i2cError, uint8_t pause_sec);

#endif //TF_HWSYS_I2C_MODULE_H
