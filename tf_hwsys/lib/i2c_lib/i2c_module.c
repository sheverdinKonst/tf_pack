//
// Created by sheverdin on 3/19/24.
//

#include "i2c_module.h"
#include "time.h"
#include "syslog.h"

static void i2c_parsingData(REGISTER_ADDR addr);

static I2C_EVENT_t check_out_of_range(uint32_t value, uint32_t prevValue, uint32_t max, uint32_t min);
static void i2c_sendError(I2C_ERROR_t *i2cError, REGISTER_ADDR addr, uint32_t value, I2C_EVENT_t i2c_event);

static I2C_EVENT_t check_changed_state(uint32_t value, uint32_t preValue, uint32_t state_1, uint32_t state_2);
static uint16_t get_16bitValue(const uint8_t *val1);
static uint32_t get_32bitValue(const uint8_t *val1);

const char i2c_name[MAX_SENSORS][I2C_NAME_MAX_LEN] =
{
    {"INTSTAT"},                // 0
    {"INTMASK"},                // 1
    {"HW_VERS"},                // 2
    {"SW_VERS"},                // 3
    {"ADC_CH1"},                // 4
    {"ADC_CH2"},                // 5
    {"ADC_CH3"},                // 6
    {"empty"},                  // 7
    {"empty"},                  // 8
    {"empty"},                  // 9
    {"inputTamper"},
    {"inputSensor1"},
    {"inputSensor2"},
    {"inputRelay1"},
    {"DEFAULT_BUTTON"},
    {"DEFAULT_LED"},
    {"DEFAULT_LONG_PRESSED"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"upsModeAvalible"},
    {"RPS_HW_VERS"},
    {"RPS_SW_VERS"},
    {"upsPwrSource"},
    {"upsBatteryVoltage"},
    {"RPS_CHRG_VOLTAGE"},
    {"RPS_BAT_CURRENT"},
    {"RPS_TEMPER"},
    {"RPS_LED_STATE"},
    {"RPS_BAT_KEY"},
    {"RPS_CHRG_KEY"},
    {"RPS_REL_STATE"},
    {"RPS_MIN_VOLTAGE"},
    {"RPS_DISCH_VOLTAGE"},
    {"upsBatteryTime"},
    {"RPS_TEST_OK"},

    {"RPS_CPU_ID"},
    {"RPS_LTC4151_OK"},
    {"RPS_ADC_BAT_VOLT"},
    {"RPS_ADC_BAT_CURR"},
    {"RPS_TEST_MODE"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"sensorConnected"},
    {"SHT_TYPE"},
    {"sensorTemperature"},
    {"sensorHumidity"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"portSfpPresent_1"},
    {"portSfpSignalDetect_1"},
    {"portSfpVendor_1"},
    {"portSfpOui_1"},
    {"portSfpPartNumber_1"},
    {"portSfpRevision_1"},
    {"SFP_IDENTIFIER_1"},
    {"SFP_CONNECTOR_1"},
    {"SFP_TYPE_1"},
    {"SFP_LINK_LEN_1"},
    {"SFP_FIBER_TEC_1"},
    {"SFP_MEDIA_1"},
    {"SFP_SPEED_1"},
    {"SFP_ENCODING_1"},
    {"SFP_WAVELEN_1"},
    {"SFP_NBR_1"},
    {"SFP_LEN9_1"},
    {"SFP_LEN50_1"},
    {"SFP_LEN62_1"},
    {"SFP_LENC_1"},
    {"portSfpTemperature_1"},
    {"portSfpVoltage_1"},
    {"SFP_CURRENT_1"},
    {"portSfpBiasCurrent_1"},
    {"portSfpTxOutPower_1"},
    {"portSfpRxOutPower_1"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"portSfpPresent_2"},
    {"portSfpSignalDetect_2"},
    {"portSfpVendor_2"},
    {"portSfpOui_2"},
    {"portSfpPartNumber_2"},
    {"portSfpRevision_2"},
    {"SFP_IDENTIFIER_2"},
    {"SFP_CONNECTOR_2"},
    {"SFP_TYPE_2"},
    {"SFP_LINK_LEN_2"},
    {"SFP_FIBER_TEC_2"},
    {"SFP_MEDIA_2"},
    {"SFP_SPEED_2"},
    {"SFP_ENCODING_2"},
    {"SFP_WAVELEN_2"},
    {"SFP_NBR_2"},
    {"SFP_LEN9_2"},
    {"SFP_LEN50_2"},
    {"SFP_LEN62_2"},             
    {"SFP_LENC_2"},
    {"portSfpTemperature_2"},
    {"portSfpVoltage_2"},
    {"SFP_CURRENT_2"},
    {"portSfpBiasCurrent_2"},
    {"portSfpTxOutPower_2"},
    {"portSfpRxOutPower_2"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"empty"},
    {"RTC_STATUS"},
    {"RTC_YEAR"},
    {"RTC_MONTH"},
    {"RTC_DAY"},
    {"RTC_WEEKDAY"},
    {"RTC_HOUR"},
    {"RTC_MINUTE"},
    {"RTC_SECOND"},
    {"empty"},
    {"empty"},
    {"POE_ID"},
    {"POE_STATE"},
    {"POE_BANK"},
    {"POE_MODE"},
};

const char i2c_eventDict[I2C_MAX_EVENT][I2C_ERROR_NAME_MAX_LEN] = {
    "\n",
    "ERR_OVER_MAX\n",
    "ERR_LESS_MIN\n",
    "LONG_PRESSED\n",
    "CHANGED_TO_VAC\n",
    "CHANGED_TO_BAT\n",
    "CHANGED_TO_OPEN\n",
    "CHANGED_TO_CLOSE\n",
    "empty\n",
    "empty\n",
    "CRITICAL_ERROR - RESET\n"
};

i2c_data_t sock_msgArr[MAX_SENSORS] =
{
    // MAIN
    {.addr = REG_INTSTAT,   .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_INTMASK,   .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_HW_VERS,   .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_SW_VERS,   .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_ADC_CH1,   .lenData = 2, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = critical_REG_ADC_CH1 },
    {.addr = REG_ADC_CH2,   .lenData = 2, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = critical_REG_ADC_CH2 },
    {.addr = REG_ADC_CH3,   .lenData = 2, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = critical_REG_ADC_CH3 },

    {.addr = REG_IDLE7,     .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE8,     .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE9,     .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},

    // SENSOR
    {.addr = REG_TAMPER,               .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = critical, .criticalHandler = critical_REG_TAMPER},
    {.addr = REG_SENSOR1,              .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_SENSOR1},
    {.addr = REG_SENSOR2,              .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_SENSOR2},
    {.addr = REG_RELAY1,               .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_RELAY1},
    {.addr = REG_DEFAULT_BUTTON,       .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL},
    {.addr = REG_DEFAULT_LED,          .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL},
    {.addr = REG_DEFAULT_LONG_PRESSED, .lenData = 2, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = critical, .criticalHandler = critical_REG_DEFAULT_LONG_PRESSED},

    {.addr = REG_IDLE17,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE18,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE19,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},

    // UPS
    {.addr = REG_RPS_CONNECTED,      .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL     },
    {.addr = REG_RPS_HW_VERS,        .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL       },
    {.addr = REG_RPS_SW_VERS,        .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL       },
    {.addr = REG_RPS_VAC,            .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = critical_REG_RPS_VAC           },
    {.addr = REG_RPS_BAT_VOLTAGE,    .lenData = 2, .type = TYPE_PSEUDO_FLOAT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = critical_REG_RPS_BAT_VOLTAGE   },
    {.addr = REG_RPS_CHRG_VOLTAGE,   .lenData = 2, .type = TYPE_PSEUDO_FLOAT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = critical_REG_RPS_CHRG_VOLTAGE  },
    {.addr = REG_RPS_BAT_CURRENT,    .lenData = 2, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = critical_REG_RPS_BAT_CURRENT   },
    {.addr = REG_RPS_TEMPER,         .lenData = 1, .type = TYPE_UINT,         .value = 123, .opcode = I2C_OPCODE_IDLE, .isCritical = critical,   .criticalHandler = critical_REG_RPS_TEMPER        },
    {.addr = REG_RPS_LED_STATE,      .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL     },
    {.addr = REG_RPS_BAT_KEY,        .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL },
    {.addr = REG_RPS_CHRG_KEY,       .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL },
    {.addr = REG_RPS_REL_STATE,      .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL },
    {.addr = REG_RPS_MIN_VOLTAGE,    .lenData = 2, .type = TYPE_PSEUDO_FLOAT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL },
    {.addr = REG_RPS_DISCH_VOLTAGE,  .lenData = 2, .type = TYPE_PSEUDO_FLOAT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL },
    {.addr = REG_RPS_REMAIN_TIME,    .lenData = 2, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = critical,     .criticalHandler = NULL },
    {.addr = REG_RPS_TEST_OK,        .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL },
    {.addr = REG_RPS_CPU_ID,         .lenData = 2, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL },
    {.addr = REG_RPS_LTC4151_OK,     .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL },
    {.addr = REG_RPS_ADC_BAT_VOLT,   .lenData = 2, .type = TYPE_PSEUDO_FLOAT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL },
    {.addr = REG_RPS_ADC_BAT_CURR,   .lenData = 2, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL },
    {.addr = REG_RPS_TEST_MODE,      .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,      .criticalHandler = NULL },

    {.addr = REG_IDLE41,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE42,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE43,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE44,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE45,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE46,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE47,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE48,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE49,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},

    // SHT
    {.addr = REG_SHT_CONNECTED,    .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_SHT_CONNECTED   },
    {.addr = REG_SHT_TYPE,         .lenData = 10, .type = TYPE_UINT,        .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_SHT_TYPE        },
    {.addr = REG_SHT_TEMPERATURE,  .lenData = 2, .type = TYPE_PSEUDO_FLOAT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = critical, .criticalHandler = critical_REG_SHT_TEMPERATURE },
    {.addr = REG_SHT_HUMIDITY,     .lenData = 1, .type = TYPE_UINT,         .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_SHT_HUMIDITY    },

    {.addr = REG_IDLE54,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE55,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE56,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE57,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE58,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE59,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},

    // SPF_1
    {.addr = REG_SFP1_PRESENT,     .lenData = 1,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = critical_REG_SFP1_PRESENT },
    {.addr = REG_SFP1_LOS,         .lenData = 1,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = critical_REG_SFP1_LOS     },
    {.addr = REG_SFP1_VENDOR,      .lenData = 16, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_VENDOR_OUI,  .lenData = 3,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_VENDOR_PN,   .lenData = 16, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_VENDOR_REV,  .lenData = 4,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_IDENTIFIER,  .lenData = 4,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_CONNECTOR,   .lenData = 30, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_TYPE,        .lenData = 16, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_LINK_LEN,    .lenData = 25, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_FIBER_TEC,   .lenData = 32, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_MEDIA,       .lenData = 32, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_SPEED,       .lenData = 16, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_ENCODING,    .lenData = 16, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_WAVELEN,     .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_NBR,         .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_LEN9,        .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_LEN50,       .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_LEN62,       .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_LENC,        .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_SFP1_TEMPER,      .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = critical_REG_SFP1_TEMPER   },
    {.addr = REG_SFP1_VOLTAGE,     .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = critical_REG_SFP1_VOLTAGE  },
    {.addr = REG_SFP1_CURRENT,     .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = critical_REG_SFP1_CURRENT  },
    {.addr = REG_SFP1_TX_BIAS,     .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = critical_REG_SFP1_TX_BIAS  },
    {.addr = REG_SFP1_TX_POWER,    .lenData = 2, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = critical_REG_SFP1_TX_POWER },
    {.addr = REG_SFP1_RX_POWER,    .lenData = 2, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = critical_REG_SFP1_RX_POWER },

    {.addr = REG_IDLE86,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE87,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE88,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE89,    .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},

    //SPF_2
    {.addr = REG_SFP2_PRESENT,      .lenData = 1,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler =  critical_REG_SFP2_PRESENT },
    {.addr = REG_SFP2_LOS,          .lenData = 1,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler =  critical_REG_SFP2_LOS     },
    {.addr = REG_SFP2_VENDOR,       .lenData = 16, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL  },
    {.addr = REG_SFP2_VENDOR_OUI,   .lenData = 3,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL  },
    {.addr = REG_SFP2_VENDOR_PN,    .lenData = 16, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL  },
    {.addr = REG_SFP2_VENDOR_REV,   .lenData = 4,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL  },
    {.addr = REG_SFP2_IDENTIFIER,   .lenData = 4,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL  },
    {.addr = REG_SFP2_CONNECTOR,    .lenData = 30, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL  },
    {.addr = REG_SFP2_TYPE,         .lenData = 16, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL  },
    {.addr = REG_SFP2_LINK_LEN,     .lenData = 25, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL  },
    {.addr = REG_SFP2_FIBER_TEC,    .lenData = 32, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL },
    {.addr = REG_SFP2_MEDIA,        .lenData = 32, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL },
    {.addr = REG_SFP2_SPEED,        .lenData = 16, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL },
    {.addr = REG_SFP2_ENCODING,     .lenData = 16, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL },
    {.addr = REG_SFP2_WAVELEN,      .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL },
    {.addr = REG_SFP2_NBR,          .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL },
    {.addr = REG_SFP2_LEN9,         .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL },
    {.addr = REG_SFP2_LEN50,        .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL },
    {.addr = REG_SFP2_LEN62,        .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL },
    {.addr = REG_SFP2_LENC,         .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = NULL },
    {.addr = REG_SFP2_TEMPER,       .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_SFP2_TEMPER   },
    {.addr = REG_SFP2_VOLTAGE,      .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_SFP2_VOLTAGE  },
    {.addr = REG_SFP2_CURRENT,      .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_SFP2_CURRENT  },
    {.addr = REG_SFP2_TX_BIAS,      .lenData = 2,  .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_SFP2_TX_BIAS  },
    {.addr = REG_SFP2_TX_POWER,      .lenData = 2, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_SFP2_TX_POWER },
    {.addr = REG_SFP2_RX_POWER,      .lenData = 2, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular,  .criticalHandler = critical_REG_SFP2_RX_POWER },

    {.addr = REG_IDLE116,   .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE117,   .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE118,   .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr = REG_IDLE119,   .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},

    // RTC
    {.addr = REG_RTC_STATUS,    .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_RTC_YEAR,      .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_RTC_MONTH,     .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_RTC_DAY,       .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_RTC_WEEKDAY,   .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_RTC_HOUR,      .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_RTC_MINUTE,    .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_RTC_SECOND,    .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },

    {.addr =  REG_IDLE128,  .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},
    {.addr =  REG_IDLE129,  .lenData = 0, .type = TYPE_IDLE, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL},

    // POE
    {.addr = REG_POE_ID,    .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_POE_STATE, .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_POE_BANK,  .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
    {.addr = REG_POE_MODE,  .lenData = 1, .type = TYPE_UINT, .value = 0, .opcode = I2C_OPCODE_IDLE, .isCritical = regular, .criticalHandler = NULL },
};

void i2c_test(void)
{
    printf("Hello from I2C\n");
}

REGISTER_ADDR get_i2c_addr_by_name(char *reg_name, i2c_param *i2CParam)
{
    REGISTER_ADDR registerAdr = SENSOR_IDLE;
    for(int i = 0; i < MAX_SENSORS; i++)
    {
        //printf("i = %d reg_name = %s -- i2c_name %s\n", i, reg_name, i2c_name[i]);
        if(strcmp(reg_name, i2c_name[i]) == 0)
        {
            //printf("FOUND i = %d gpio_name = %s\n", i, i2c_name[i]);
            registerAdr = (REGISTER_ADDR) i;
            i2CParam->i2c_data.opcode   = sock_msgArr[i].opcode;
            i2CParam->i2c_data.addr     = sock_msgArr[i].addr;
            i2CParam->i2c_data.lenData  = sock_msgArr[i].lenData;
            i2CParam->i2c_data.type     = sock_msgArr[i].type;
            i = MAX_SENSORS;
        }
    }
    return registerAdr;
}

void open_i2c(int *i2c_fd)
{
    *i2c_fd = open(I2C_ADAPTER, O_RDWR);
    if (*i2c_fd < 0) {
        perror("open i2c");
        printf("Unable to open i2c file\n");
        exit (EXIT_FAILURE);
    }
    printf("open i2c interface\n");
}

uint8_t read_buffer(int fd, uint8_t pause_sec)
{
    struct i2c_rdwr_ioctl_data data;
    struct i2c_msg messages[2];
    unsigned char regaddr_[2];
    static REGISTER_ADDR currAddr = SENSOR_IDLE;
    uint8_t errorCode = 0;

    /*
     * .addr -
     * .flags -  (0 - w, 1 - r)
     * .len -
     * .buf -
     */

    if (currAddr < MAX_SENSORS)
    {
        sleep(0);
        uint8_t i2c_regAddr = sock_msgArr[currAddr].addr;
        //long timeNow1 = time(NULL);
        //usleep(10000);
        //long timeNow2 = time(NULL);
        //printf("delta %ld\n", (timeNow2 - timeNow1));

        //if (strcmp(i2c_name[i2c_regAddr], "empty") == 0) {
        //    //printf("regAdr = %d is empty\n", regAddr);
        //}

        if (sock_msgArr[currAddr].lenData != 0)
        {
            regaddr_[0] = currAddr;
            regaddr_[1] = sock_msgArr[currAddr].lenData;
            messages[0].addr = I2C_ADDR;
            messages[0].flags = 0; //write
            messages[0].len = 2;
            messages[0].buf = regaddr_;

            messages[1].addr = I2C_ADDR;
            messages[1].flags = 1;//read
            messages[1].len = sock_msgArr[currAddr].lenData;
            messages[1].buf = sock_msgArr[currAddr].value;

            data.msgs = messages;
            data.nmsgs = 2;

            int rc = ioctl(fd, I2C_RDWR, &data);
            if (rc < 0)
            {
                printf("%s -- %d Read i2c error \n", i2c_name[i2c_regAddr], i2c_regAddr);
                errorCode = 1;
                //return EXIT_FAILURE;
            }
            else
            {
                //printf("%s -- \t%d -> \n", i2c_name[currAddr], sock_msgArr[currAddr].addr);
                if (sock_msgArr[currAddr].value[0] == 0xAA && sock_msgArr[currAddr].value[1] == 0xAA)
                {
                    printf("error response\n");
                }
                if (currAddr == REG_RPS_CONNECTED)
                {
                    printf("SHT_CONNECTED value = %d\n", sock_msgArr[currAddr].value[0]);
                }
            }
        }
        currAddr++;
    }
    else if (currAddr >= MAX_SENSORS)
    {
        long Time;
        sleep(pause_sec);
        //printf("MAX_SENSOR \t");
        //Time = time(NULL);
        //printf("------------------ >>>>>>>>>>>>>>>> run main Handler\n");
        currAddr = REG_INTSTAT;
    }
    return errorCode;
}

//write I2C data
int write_buffer(int fd)
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

    //messages[0].addr = I2C_ADDR;
    //messages[0].flags = 0;//write
    //messages[0].len = len+2;
    //messages[0].buf = tmp;
    //messages[0].buf[0] = regAddr;
    //messages[0].buf[1] = len;
    //for(int i=0; i < len; i++)
    //    messages[0].buf[i+2] = buffer[i];
    //
    //data.msgs = messages;
    //data.nmsgs = 1;
    //
    //if (ioctl(fd, I2C_RDWR, &data) < 0){
    //    printf("Write: cant send data!\n");
    //    return EXIT_FAILURE;
    //}
    return EXIT_SUCCESS;
}

int i2c_get_MsgSize(void)
{
    int len = sizeof(i2c_data_t);
    return len;
}

void i2c_getData(i2c_data_t *i2CData, REGISTER_ADDR addr, int len)
{
    sock_msgArr[addr].lenMSG = len;
    memcpy(i2CData, &sock_msgArr[addr], len);
    printf(" ------------------------------------- \n");
}

void i2c_critical_handler(I2C_ERROR_t *i2cError, uint8_t pause_sec)
{
    static REGISTER_ADDR currSensorAddr = SENSOR_IDLE;
    static uint8_t delayStart = 0;

    if (currSensorAddr < MAX_SENSORS)
    {
        if (sock_msgArr[currSensorAddr].criticalHandler != NULL)
        {
            //printf("currSensorAddr = %d\n", currSensorAddr);
            if (delayStart >= DELAY_START)
            {
                sock_msgArr[currSensorAddr].criticalHandler(i2cError);
            }
        }
        currSensorAddr++;
    }
    else if (currSensorAddr >= MAX_SENSORS)
    {
        sleep(pause_sec);
        long Time;
        //printf("MAX_SENSOR \t");
        //Time = time(NULL);
        //printf("------------------ >>>>>>>>>>>>>>>>  Time  : %ld \n", Time);
        //printf("currSensorAddr >= MAX_SENSORS = %d\n", currSensorAddr);
        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
        currSensorAddr = REG_INTSTAT;
        if (delayStart < DELAY_START) {
            delayStart++;
        }
    }
}

static void i2c_parsingData(REGISTER_ADDR addr)
{
    switch (sock_msgArr[addr].lenData)
    {
        case 1:
            //uint8
            printf("%d\n", sock_msgArr[addr].value[0]);
            break;
        case 2:
        {
            uint16_t tmp16;
            if (sock_msgArr[addr].type == TYPE_PSEUDO_FLOAT)
            {
                tmp16 = sock_msgArr[addr].value[0] | sock_msgArr[addr].value[1] << 8;
                if (tmp16 < 1000)
                    printf("%d.%d\n", tmp16 / 10, tmp16 % 10);
                else
                    printf("%d.%d\n", tmp16 / 1000, tmp16 % 1000);
            }
            else
            {
                //uint16
                printf("%d\n", sock_msgArr[addr].value[0] | sock_msgArr[addr].value[1] << 8);
            }
        }
            break;
        case 4:
            //uint32
            printf("%d\n", (uint32_t) (sock_msgArr[addr].value[0] |
                                       sock_msgArr[addr].value[1] << 8 |
                                       sock_msgArr[addr].value[2] << 16 |
                                       sock_msgArr[addr].value[3] << 24));
            break;
        default:
            printf("%s\n", sock_msgArr[addr].value);
    }
}

I2C_EVENT_t critical_REG_ADC_CH1(I2C_ERROR_t *i2cError)
{
    static uint16_t ch1_prevValue = 0xFFFF;
    uint16_t value = get_16bitValue(&sock_msgArr[REG_ADC_CH1].value[0]);
    I2C_EVENT_t i2c_event = I2C_ERR_OK;
    //printf("ADC_CH1 value = %d\n", value);

    i2c_event = check_out_of_range(value, ch1_prevValue, REG_ADC_CH1_max, REG_ADC_CH1_min);

    if (i2c_event == I2C_ERR_CHANGE_VALUE)
    {
        ch1_prevValue = value;
    }

    if (i2c_event == I2C_OVER_MAX || i2c_event == I2C_LESS_MIN)
    {
        i2c_sendError(i2cError, REG_ADC_CH1, value, i2c_event);
        ch1_prevValue = value;
    }
    return i2c_event;
}

I2C_EVENT_t critical_REG_ADC_CH2(I2C_ERROR_t *i2cError)
{
    static uint16_t ch2_prevValue = 0xFFFF;
    uint16_t value = get_16bitValue(&sock_msgArr[REG_ADC_CH2].value[0]);
    I2C_EVENT_t i2c_error = I2C_ERR_OK;

    i2c_error = check_out_of_range(value,ch2_prevValue,REG_ADC_CH2_max,REG_ADC_CH2_min);

    if (i2c_error == I2C_ERR_CHANGE_VALUE) {
        ch2_prevValue = value;
    }

    if (i2c_error & I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
    {
        i2c_sendError(i2cError, REG_ADC_CH2, value, i2c_error);
        ch2_prevValue = value;
    }

    return i2c_error;
}

I2C_EVENT_t critical_REG_ADC_CH3(I2C_ERROR_t *i2cError)
{
    //printf("ADC_CH3 value = %d\n", value);
    static uint16_t ch3_prevValue = 0xFFFF;
    uint16_t value = get_16bitValue(&sock_msgArr[REG_ADC_CH3].value[0]);
    I2C_EVENT_t i2c_error = I2C_ERR_OK;

    i2c_error = check_out_of_range(value,ch3_prevValue,REG_ADC_CH3_max,REG_ADC_CH3_min);

    if (i2c_error & I2C_ERR_CHANGE_VALUE) {
        ch3_prevValue = value;
    }

    if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
    {
        i2c_sendError(i2cError, REG_ADC_CH3, value, i2c_error);
        ch3_prevValue = value;
    }
    return i2c_error;
}

// SENSORS
I2C_EVENT_t critical_REG_TAMPER(I2C_ERROR_t *i2cError)
{
    static uint8_t tamper1_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint8_t value = sock_msgArr[REG_TAMPER].value[0];

    if (value != tamper1_prevValue)
    {
        if(tamper1_prevValue != 0xFF)
        {
            i2c_error = check_changed_state(value, tamper1_prevValue, REG_TAMPER_close, REG_TAMPER_open);
        }
        tamper1_prevValue = value;
    }

    if (i2c_error != I2C_ERR_OK) {
        i2c_sendError(i2cError, REG_TAMPER, value, i2c_error);
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SENSOR1(I2C_ERROR_t *i2cError)
{
    static uint8_t sen1_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint8_t value = sock_msgArr[REG_SENSOR1].value[0];
    //printf("REG_SENSOR1 value = %d\n", value);
    if (value != sen1_prevValue)
    {
        if(sen1_prevValue != 0xFF)
        {
            i2c_error = check_changed_state(value, sen1_prevValue, REG_TAMPER_close, REG_TAMPER_open );
        }
        sen1_prevValue = value;
    }

    if (i2c_error != I2C_ERR_OK) {
        i2c_sendError(i2cError, REG_SENSOR1, value, i2c_error);
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SENSOR2(I2C_ERROR_t *i2cError)
{
    static uint8_t sen2_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint8_t value = sock_msgArr[REG_SENSOR2].value[0];
    if (value != sen2_prevValue)
    {
        if(sen2_prevValue != 0xFF)
        {
            i2c_error = check_changed_state(value, sen2_prevValue, REG_TAMPER_close, REG_TAMPER_open );
        }
        sen2_prevValue = value;
    }

    if (i2c_error != I2C_ERR_OK) {
        i2c_sendError(i2cError, REG_SENSOR2, value, i2c_error);
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_RELAY1(I2C_ERROR_t *i2cError)
{
    static uint8_t sen3_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint8_t value = sock_msgArr[REG_RELAY1].value[0];
    if (value != sen3_prevValue)
    {
        if(sen3_prevValue != 0xFF)
        {
            i2c_error = check_changed_state(value, sen3_prevValue, REG_TAMPER_close, REG_TAMPER_open );
        }
        sen3_prevValue = value;
    }

    if (i2c_error != I2C_ERR_OK) {
        i2c_sendError(i2cError, REG_RELAY1, value, i2c_error);
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_DEFAULT_LONG_PRESSED(I2C_ERROR_t *i2cError)
{
    static uint16_t longPress_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint16_t value = get_16bitValue(&sock_msgArr[REG_DEFAULT_LONG_PRESSED].value[0]);
    //printf("REG_DEFAULT_LONG_PRESSED value = %d\n", value);
    //printf("LONG_PRESSED value = %d\n", value);
    if (value != longPress_prevValue)
    {
        if(longPress_prevValue != 0xFF)
        {
            i2c_error = check_changed_state(value, longPress_prevValue, REG_DEFAULT_LONG_PRESSED_reset, 0);
        }
        longPress_prevValue = value;
    }

    if (i2c_error != I2C_ERR_OK) {
        i2c_error = (i2c_error==I2C_CHANGED_TO_CLOSE)?I2C_LONG_PRESSED:I2C_ERR_OK;
        i2c_sendError(i2cError, REG_DEFAULT_LONG_PRESSED, value, i2c_error);
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_RPS_VAC(I2C_ERROR_t *i2cError)
{
    static uint8_t vac_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint8_t value = sock_msgArr[REG_RPS_VAC].value[0];
    //printf("REG_RPS_VAC value = %d\n", value);
   // printf("value = %d, vac_prevValue = %d\n",  value, vac_prevValue);
    if (value != vac_prevValue)
    {
        if(vac_prevValue != 0xFF)
        {
            i2c_error = check_changed_state(value, vac_prevValue, REG_RPS_VAC_vac, REG_RPS_VAC_battary);
           // printf("i2c_error = %d\n", i2c_error);
        }
        vac_prevValue = value;
    }
    if (i2c_error != I2C_ERR_OK) {
        i2c_error = (i2c_error==I2C_CHANGED_TO_CLOSE)?I2C_CHANGED_TO_VAC:I2C_CHANGED_TO_BAT;
        i2c_sendError(i2cError, REG_RPS_VAC, value, i2c_error);
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_RPS_BAT_VOLTAGE(I2C_ERROR_t *i2cError)
{
    static uint16_t batVolt_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint16_t value = get_16bitValue(&sock_msgArr[REG_RPS_BAT_VOLTAGE].value[0]);
    //printf("REG_RPS_BAT_VOLTAGE value = %d\n", value);
    if (value != batVolt_prevValue)
    {
        i2c_error = check_out_of_range(value,batVolt_prevValue,REG_RPS_BAT_VOLTAGE_max,REG_RPS_BAT_VOLTAGE_min);
    }

    if (i2c_error == I2C_ERR_CHANGE_VALUE) {
        batVolt_prevValue = value;
    }

    if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
    {
        i2c_sendError(i2cError, REG_RPS_BAT_VOLTAGE, value, i2c_error);
        batVolt_prevValue = value;
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_RPS_CHRG_VOLTAGE(I2C_ERROR_t *i2cError)
{
    static uint16_t chgVolt_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint16_t value = get_16bitValue(&sock_msgArr[REG_RPS_CHRG_VOLTAGE].value[0]);
    //printf("REG_RPS_CHRG_VOLTAGE value = %d\n", value);

    i2c_error = check_out_of_range(value, chgVolt_prevValue,REG_RPS_CHRG_VOLTAGE_max,REG_RPS_CHRG_VOLTAGE_min);

    if (i2c_error == I2C_ERR_CHANGE_VALUE) {
        chgVolt_prevValue = value;
    }

    if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
    {
        i2c_sendError(i2cError, REG_RPS_CHRG_VOLTAGE, value, i2c_error);
        chgVolt_prevValue = value;
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_RPS_BAT_CURRENT(I2C_ERROR_t *i2cError)
{
    static uint16_t batCur_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint16_t value = get_16bitValue(&sock_msgArr[REG_RPS_BAT_CURRENT].value[0]);
    //printf("REG_RPS_BAT_CURRENT value = %d\n", value);

    i2c_error = check_out_of_range(value, batCur_prevValue,REG_RPS_BAT_CURRENT_max,REG_RPS_BAT_CURRENT_min);

    if (i2c_error == I2C_ERR_CHANGE_VALUE) {
        batCur_prevValue = value;
    }

    if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
    {
        i2c_sendError(i2cError, REG_RPS_BAT_CURRENT, value, i2c_error);
        batCur_prevValue = value;
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_RPS_TEMPER(I2C_ERROR_t *i2cError)
{
    static uint8_t temper_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint8_t value = sock_msgArr[REG_RPS_TEMPER].value[0];
    //printf("REG_RPS_TEMPER value = %d\n", value);

    i2c_error = check_out_of_range(value, temper_prevValue,REG_RPS_TEMPER_max,REG_RPS_TEMPER_min);

    if (i2c_error == I2C_ERR_CHANGE_VALUE) {
        temper_prevValue = value;
    }
    if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
    {
        i2c_sendError(i2cError, REG_RPS_TEMPER, value, i2c_error);
        temper_prevValue = value;
    }
    return i2c_error;
}

// SHT
I2C_EVENT_t critical_REG_SHT_TYPE(I2C_ERROR_t *i2cError)
{
    //printf("REG_SHT_TYPE value = %s\n", sock_msgArr[REG_SHT_TYPE].value);
    return 0;
}

I2C_EVENT_t critical_REG_SHT_CONNECTED(I2C_ERROR_t *i2cError)
{
    static uint8_t sht_connected_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint8_t  value = sock_msgArr[REG_SHT_CONNECTED].value[0];

    if (value != sht_connected_prevValue)
    {
        if(sht_connected_prevValue != 0xFF)
        {
            i2c_error = check_changed_state(value, sht_connected_prevValue,REG_SHT_CONNECTED_yes,REG_SHT_CONNECTED_no);
        }
        sht_connected_prevValue = value;
    }

    if (i2c_error != I2C_ERR_OK) {
        i2c_sendError(i2cError, REG_SHT_CONNECTED, value, i2c_error);
    }

    return i2c_error;

}

I2C_EVENT_t critical_REG_SHT_TEMPERATURE(I2C_ERROR_t *i2cError)
{
    static uint16_t shtTemper_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    //printf("REG_SHT_TEMPERATURE value = %d\n", value);
    if (sock_msgArr[REG_SHT_CONNECTED].value[0] == REG_SHT_CONNECTED_yes)
    {

        uint16_t value = get_16bitValue(&sock_msgArr[REG_SHT_TEMPERATURE].value[0]);
        if (value > REG_SHT_TEMPERATURE_max*2)
        {
            i2c_error = I2C_CRITICAL_ERROR;
        }
        else {
            i2c_error = check_out_of_range(value, shtTemper_prevValue, REG_SHT_TEMPERATURE_max, REG_SHT_TEMPERATURE_min);
        }

        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            shtTemper_prevValue = value;
        }

        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN ||  i2c_error == I2C_CRITICAL_ERROR)
        {
            i2c_sendError(i2cError, REG_SHT_TEMPERATURE, value, i2c_error);
            shtTemper_prevValue = value;
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SHT_HUMIDITY(I2C_ERROR_t *i2cError)
{
    static uint8_t shtHim_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;

    //printf("REG_SHT_HUMIDITY value = %d\n", value);
    if (sock_msgArr[REG_SHT_CONNECTED].value[0] == REG_SHT_CONNECTED_yes)
    {

        uint8_t value = sock_msgArr[REG_SHT_HUMIDITY].value[0];
        i2c_error = check_out_of_range(value, shtHim_prevValue, REG_SHT_HUMIDITY_max, REG_SHT_HUMIDITY_min);

        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            shtHim_prevValue = value;
        }

        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SHT_HUMIDITY, value, i2c_error);
            shtHim_prevValue = value;
        }
    }
    return i2c_error;
}

// SPF_1
I2C_EVENT_t critical_REG_SFP1_PRESENT(I2C_ERROR_t *i2cError)
{
    static uint8_t spf1_present_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint8_t  value = sock_msgArr[REG_SFP1_PRESENT].value[0];
    //printf("REG_SFP1_PRESENT value = %d\n", value);

    if (value != spf1_present_prevValue)
    {
        if(spf1_present_prevValue != 0xFF)
        {
            i2c_error = check_changed_state(value, spf1_present_prevValue,REG_SFP_PRESENT_yes,REG_SFP_PRESENT_no);
        }
        spf1_present_prevValue = value;
    }
    if (i2c_error != I2C_ERR_OK) {
        i2c_sendError(i2cError, REG_SFP1_PRESENT, value, i2c_error);
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP2_PRESENT(I2C_ERROR_t *i2cError)
{
    static uint8_t spf2_present_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint8_t  value = sock_msgArr[REG_SFP2_PRESENT].value[0];
    //printf("REG_SFP2_PRESENT value = %d\n", value);
    if (value != spf2_present_prevValue)
    {
        if(spf2_present_prevValue != 0xFF)
        {
            i2c_error = check_changed_state(value, spf2_present_prevValue,REG_SFP_PRESENT_yes,REG_SFP_PRESENT_no);
        }
        spf2_present_prevValue = value;
    }
    if (i2c_error != I2C_ERR_OK) {
        i2c_sendError(i2cError, REG_SFP2_PRESENT, value, i2c_error);
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP1_LOS(I2C_ERROR_t *i2cError)
{
    static uint8_t spf1Los_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint8_t  value = sock_msgArr[REG_SFP1_LOS].value[0];
    //printf("REG_SFP1_LOS value = %d\n", value);

    if (value != spf1Los_prevValue)
    {
        if(spf1Los_prevValue != 0xFF)
        {
            i2c_error = check_changed_state(value, spf1Los_prevValue,REG_SFP_LOS_no,REG_SFP_LOS_yes);
        }
        spf1Los_prevValue = value;
    }
    if (i2c_error != I2C_ERR_OK) {
        i2c_sendError(i2cError, REG_SFP1_LOS, value, i2c_error);
    }

    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP2_LOS(I2C_ERROR_t *i2cError)
{
    static uint8_t spf2Los_prevValue = 0xFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    uint8_t value = sock_msgArr[REG_SFP2_LOS].value[0];
    //printf("REG_SFP2_LOS value = %d\n", value);
    if (value != spf2Los_prevValue)
    {
        if(spf2Los_prevValue != 0xFF)
        {
            i2c_error = check_changed_state(value, spf2Los_prevValue,REG_SFP_LOS_no,REG_SFP_LOS_yes);
        }
        spf2Los_prevValue = value;
    }
    if (i2c_error != I2C_ERR_OK) {
        i2c_sendError(i2cError, REG_SFP2_LOS, value, i2c_error);
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP1_TEMPER(I2C_ERROR_t *i2cError)
{
    static uint16_t spf1_temper_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;

    //printf("REG_SFP1_TEMPER value = %d\n", value);
    if (sock_msgArr[REG_SFP1_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t value = get_16bitValue(&sock_msgArr[REG_SFP1_TEMPER].value[0]);
        i2c_error = check_out_of_range(value, spf1_temper_prevValue, REG_SFP_TEMPER_max, REG_SFP_TEMPER_min);

        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf1_temper_prevValue = value;
        }

        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP1_TEMPER, value, i2c_error);
            spf1_temper_prevValue = value;
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP2_TEMPER(I2C_ERROR_t *i2cError)
{
    static uint16_t spf2_temper_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    //printf("REG_SFP2_TEMPER value = %d\n", value);
    if (sock_msgArr[REG_SFP2_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t value = get_16bitValue(&sock_msgArr[REG_SFP2_TEMPER].value[0]);
        i2c_error = check_out_of_range(value, spf2_temper_prevValue, REG_SFP_TEMPER_max, REG_SFP_TEMPER_min);
        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf2_temper_prevValue = value;
        }
        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP2_TEMPER, value, i2c_error);
            spf2_temper_prevValue = value;
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP1_VOLTAGE(I2C_ERROR_t *i2cError)
{
    static uint16_t spf1Volt_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;

    //printf("REG_SFP1_VOLTAGE value = %d\n", value);

    if (sock_msgArr[REG_SFP1_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t value = get_16bitValue(&sock_msgArr[REG_SFP1_VOLTAGE].value[0]);
        i2c_error = check_out_of_range(value, spf1Volt_prevValue, REG_SFP_VOLTAGE_max, REG_SFP_VOLTAGE_min);
        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf1Volt_prevValue = value;
        }

        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP1_VOLTAGE, value, i2c_error);
            spf1Volt_prevValue = value;
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP2_VOLTAGE(I2C_ERROR_t *i2cError)
{
    static uint16_t spf2Volt_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;

    //printf("REG_SFP2_VOLTAGE value = %d\n", value);

    if (sock_msgArr[REG_SFP2_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t value = get_16bitValue(&sock_msgArr[REG_SFP2_VOLTAGE].value[0]);
        i2c_error = check_out_of_range(value, spf2Volt_prevValue, REG_SFP_VOLTAGE_max, REG_SFP_VOLTAGE_min);

        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf2Volt_prevValue = value;
        }

        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP2_VOLTAGE, value, i2c_error);
            spf2Volt_prevValue = value;
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP1_CURRENT(I2C_ERROR_t *i2cError)
{
    static uint16_t spf1Current_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;

    //printf("REG_SFP1_CURRENT value = %d\n", value);
    if (sock_msgArr[REG_SFP1_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t value = get_16bitValue(&sock_msgArr[REG_SFP1_CURRENT].value[0]);
        i2c_error = check_out_of_range(value, spf1Current_prevValue, REG_SFP_CURRENT_max, REG_SFP_CURRENT_min);
        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf1Current_prevValue = value;
        }
        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP1_CURRENT, value, i2c_error);
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP2_CURRENT(I2C_ERROR_t *i2cError)
{
    static uint16_t spf2Current_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;

    //printf("REG_SFP2_CURRENT value = %d\n", value);

    if (sock_msgArr[REG_SFP2_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t  value = get_16bitValue(&sock_msgArr[REG_SFP2_CURRENT].value[0]);
        i2c_error = check_out_of_range(value, spf2Current_prevValue, REG_SFP_CURRENT_max, REG_SFP_CURRENT_min);
        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf2Current_prevValue = value;
        }
        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP1_CURRENT, value, i2c_error);
            spf2Current_prevValue = value;
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP1_TX_BIAS(I2C_ERROR_t *i2cError)
{
    static uint16_t spf1TXbias_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    if (sock_msgArr[REG_SFP1_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t value = get_16bitValue(&sock_msgArr[REG_SFP1_TX_BIAS].value[0]);
        //printf("REG_SFP1_TX_BIAS value = %d\n", value);
        i2c_error = check_out_of_range(value, spf1TXbias_prevValue, REG_SFP_TX_BIAS_max, REG_SFP_TX_BIAS_min);
        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf1TXbias_prevValue = value;
        }
        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP1_TX_BIAS, value, i2c_error);
            spf1TXbias_prevValue = value;
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP2_TX_BIAS(I2C_ERROR_t *i2cError)
{
    static uint16_t spf2TXbias_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    if (sock_msgArr[REG_SFP2_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t value = get_16bitValue(&sock_msgArr[REG_SFP2_TX_BIAS].value[0]);
        //printf("REG_SF2_TX_BIAS value = %d\n", value);
        i2c_error = check_out_of_range(value, spf2TXbias_prevValue, REG_SFP_TX_BIAS_max, REG_SFP_TX_BIAS_min);
        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf2TXbias_prevValue = value;
        }
        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP2_TX_BIAS, value, i2c_error);
            spf2TXbias_prevValue = value;
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP1_TX_POWER(I2C_ERROR_t *i2cError)
{
    static uint16_t spf1_txpower_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    if (sock_msgArr[REG_SFP1_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t value = get_16bitValue(&sock_msgArr[REG_SFP1_TX_POWER].value[0]);
        //printf("SFP1_TX_POWER = %d\n", value);
        i2c_error = check_out_of_range(value, spf1_txpower_prevValue, REG_SFP_TX_POWER_max, REG_SFP_TX_POWER_min);

        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf1_txpower_prevValue = value;
        }
        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP1_TX_POWER, value, i2c_error);
            spf1_txpower_prevValue = value;
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP2_TX_POWER(I2C_ERROR_t *i2cError)
{
    static uint16_t spf2_txpower_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    if (sock_msgArr[REG_SFP2_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t value = get_16bitValue(&sock_msgArr[REG_SFP2_TX_POWER].value[0]);
        //printf("SFP2_TX_POWER = %d\n", value);
        i2c_error = check_out_of_range(value, spf2_txpower_prevValue, REG_SFP_TX_POWER_max, REG_SFP_TX_POWER_min);
        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf2_txpower_prevValue = value;
        }
        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP2_TX_POWER, value, i2c_error);
            spf2_txpower_prevValue = value;
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP1_RX_POWER(I2C_ERROR_t *i2cError)
{
    static uint16_t spf1_rxpower_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    if (sock_msgArr[REG_SFP1_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t value = get_16bitValue(&sock_msgArr[REG_SFP1_RX_POWER].value[0]);
        //printf("SFP1_RX_POWER value = %d\n", value);
        i2c_error = check_out_of_range(value, spf1_rxpower_prevValue, REG_SFP_RX_POWER_max, REG_SFP_RX_POWER_min);
        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf1_rxpower_prevValue = value;
        }
        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP1_RX_POWER, value, i2c_error);
            spf1_rxpower_prevValue = value;
        }
    }
    return i2c_error;
}

I2C_EVENT_t critical_REG_SFP2_RX_POWER(I2C_ERROR_t *i2cError)
{
    static uint16_t spf2_rxpower_prevValue = 0xFFFF;
    I2C_EVENT_t i2c_error = I2C_ERR_OK;
    if (sock_msgArr[REG_SFP2_PRESENT].value[0] == REG_SFP_PRESENT_yes)
    {
        uint16_t value = get_16bitValue(&sock_msgArr[REG_SFP2_RX_POWER].value[0]);
        //printf("SFP2_RX_POWER value = %d\n", value);
        i2c_error = check_out_of_range(value, spf2_rxpower_prevValue, REG_SFP_RX_POWER_max, REG_SFP_RX_POWER_min);
        if (i2c_error == I2C_ERR_CHANGE_VALUE)
        {
            spf2_rxpower_prevValue = value;
        }
        if (i2c_error == I2C_OVER_MAX || i2c_error == I2C_LESS_MIN)
        {
            i2c_sendError(i2cError, REG_SFP2_RX_POWER, value, i2c_error);
            spf2_rxpower_prevValue = value;
        }
    }
    return i2c_error;
}

// RTC

// POE

static uint16_t get_16bitValue(const uint8_t *val1)
{
    uint16_t val16 = *val1 | *(val1+1) << 8;
    return val16;
}

static uint32_t get_32bitValue(const uint8_t *val1)
{
    uint32_t val32 = (uint32_t) (*val1 | *(val1+1) << 8 | *(val1+2) << 16 | *(val1+3) << 24);
    return val32;
}

static I2C_EVENT_t check_out_of_range(uint32_t value, uint32_t prevValue, uint32_t max, uint32_t min)
{
    I2C_EVENT_t event = I2C_ERR_OK;
    uint16_t dif = 0;

    if (value >= prevValue)
    {
        dif = value - prevValue;
    } else
        dif = prevValue - value;
    if (dif > STABLE_VALUE)
    {
        event = I2C_ERR_CHANGE_VALUE;
        if (prevValue != 0xFFFF)
        {
            if (value > max)
            {
                //printf("ERR_OVER_MAX \n");
                printf("value = %d -- max = %d\n", value, max);
                event = I2C_OVER_MAX;
            }
            else if (value < min)
            {
                //printf("ERR_LESS_MIN \n");
                printf("value = %d -- min = %d\n", value, min);
                event = I2C_LESS_MIN;
            }
        }
    }
    return event;
}

static I2C_EVENT_t check_changed_state(uint32_t value, uint32_t preValue, uint32_t state_1, uint32_t state_2)
{
    //printf("value - %d -- preValue - %d -- state_1 - %d -- state_2 - %d\n", value, preValue, state_1, state_2);
    if (value ==  state_1 && preValue == state_2)
    {
        //printf("CHANGED_TO_CLOSE\n");
        return I2C_CHANGED_TO_CLOSE;
    }
    if (value ==  state_2 && preValue == state_1)
    {
        //printf("CHANGED_TO_OPEN\n");
        return I2C_CHANGED_TO_OPEN;
    }
    return I2C_ERR_OK;
}

static void i2c_sendError(I2C_ERROR_t *i2cError, REGISTER_ADDR addr, uint32_t value, I2C_EVENT_t i2c_event)
{
    i2cError->value = value;
    i2cError->event = i2c_event;
    i2cError->addr = addr;
    memcpy(i2cError->name, i2c_name[addr], I2C_NAME_MAX_LEN);
    memcpy(i2cError->errorName,i2c_eventDict[i2c_event],I2C_ERROR_NAME_MAX_LEN);
}






