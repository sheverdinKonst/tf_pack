
#ifndef _REGISTERS_I2C_H
#define _REGISTERS_I2C_H

#define I2C_OPCODE_READ 			1
#define I2C_OPCODE_WRITE			2
#define I2C_OPCODE_RESPONSE			3

#define TYPE_UINT					1
#define TYPE_PSEUDO_FLOAT			2

struct sock_msg_t{
    unsigned char addr;//I2C Addr
    unsigned char opcode;//operate 1 - read, 2 - write
    unsigned char len;//length to read or write
    unsigned char value[255];//data payload
    unsigned char type;
};

//Main
#define REG_INTSTAT					0
#define REG_INTMASK					1
#define REG_HW_VERS					2
#define REG_SW_VERS					3
#define REG_ADC_CH1					4
#define REG_ADC_CH2					5
#define REG_ADC_CH3					6
#define REG_ADC_CH4					7


#define REG_TAMPER					10
#define REG_SENSOR1					11
#define REG_SENSOR2					12
#define REG_RELAY1					13
#define REG_DEFAULT_BUTTON			14
#define REG_DEFAULT_LED				15
//UPS
#define REG_RPS_CONNECTED			20
#define REG_RPS_HW_VERS				21
#define REG_RPS_SW_VERS				22
#define REG_RPS_VAC					23
#define REG_RPS_BAT_VOLTAGE			24
#define REG_RPS_CHRG_VOLTAGE		26
#define REG_RPS_BAT_CURRENT			28
#define REG_RPS_TEMPER				30
#define REG_RPS_LED_STATE			31
#define REG_RPS_BAT_KEY				32
#define REG_RPS_CHRG_KEY			33
#define REG_RPS_REL_STATE			34
#define REG_RPS_MIN_VOLTAGE			35
#define REG_RPS_DISCH_VOLTAGE		37
#define REG_RPS_REMAIN_TIME			39
#define REG_RPS_TEST_OK				41
#define REG_RPS_CPU_ID				42
#define REG_RPS_LTC4151_OK			43
#define REG_RPS_ADC_BAT_VOLT		44
#define REG_RPS_ADC_BAT_CURR		46
#define REG_RPS_TEST_MODE			48
//SHT
#define REG_SHT_CONNECTED			50
#define REG_SHT_TYPE				51
#define REG_SHT_TEMPERATURE			52
#define REG_SHT_HUMIDITY			54
//SFP1
#define REG_SFP1_PRESENT			60
#define REG_SFP1_LOS				61
#define REG_SFP1_VENDOR				62
#define REG_SFP1_VENDOR_OUI			78
#define REG_SFP1_VENDOR_PN			81
#define REG_SFP1_VENDOR_REV			97
#define REG_SFP1_CONNECTOR			101
#define REG_SFP1_TEMPER				102
#define REG_SFP1_VOLTAGE			104
#define REG_SFP1_TX_BIAS			106
#define REG_SFP1_TX_POWER			108
#define REG_SFP1_RX_POWER			110

//SFP2
#define REG_SFP2_PRESENT			120
#define REG_SFP2_LOS				121
#define REG_SFP2_VENDOR				122
#define REG_SFP2_VENDOR_OUI			138
#define REG_SFP2_VENDOR_PN			141
#define REG_SFP2_VENDOR_REV			157
#define REG_SFP2_CONNECTOR			161
#define REG_SFP2_TEMPER				162
#define REG_SFP2_VOLTAGE			164
#define REG_SFP2_TX_BIAS			166
#define REG_SFP2_TX_POWER			168
#define REG_SFP2_RX_POWER			170

//RTC
#define REG_RTC_STATUS				180
#define REG_RTC_YEAR				181
#define REG_RTC_MONTH				182
#define REG_RTC_DAY					183
#define REG_RTC_WEEKDAY				184
#define REG_RTC_HOUR				185
#define REG_RTC_MINUTE				186
#define REG_RTC_SECOND				187

//POE
#define REG_POE_ID					190
#define REG_POE_STATE				191
#define REG_POE_BANK				192
#define REG_POE_MODE				193



const char REG_INTSTAT_NAME[] = 			"INTSTAT";
const char REG_INTMASK_NAME[] = 			"INTMASK";
const char REG_HW_VERS_NAME[] = 			"HW_VERS";
const char REG_SW_VERS_NAME[] = 			"SW_VERS";
const char REG_ADC_CH1_NAME[] = 			"ADC_CH1";
const char REG_ADC_CH2_NAME[] = 			"ADC_CH2";
const char REG_ADC_CH3_NAME[] = 			"ADC_CH3";
const char REG_ADC_CH4_NAME[] = 			"ADC_CH4";


const char REG_TAMPER_NAME[] = 				"TAMPER";
const char REG_SENSOR1_NAME[] = 			"SENSOR1";
const char REG_SENSOR2_NAME[] = 			"SENSOR2";
const char REG_RELAY1_NAME[] = 				"RELAY1";
const char REG_DEFAULT_BUTTON_NAME[] = 		"DEFAULT_BUTTON";
const char REG_DEFAULT_LED_NAME[] = 		"DEFAULT_LED";
//UPS
const char REG_RPS_CONNECTED_NAME[] = 		"RPS_CONNECTED";
const char REG_RPS_HW_VERS_NAME[] = 		"RPS_HW_VERS";
const char REG_RPS_SW_VERS_NAME[] = 		"RPS_SW_VERS";
const char REG_RPS_VAC_NAME[] = 			"RPS_VAC";
const char REG_RPS_BAT_VOLTAGE_NAME[] = 	"RPS_BAT_VOLTAGE";
const char REG_RPS_CHRG_VOLTAGE_NAME[] = 	"RPS_CHRG_VOLTAGE";
const char REG_RPS_BAT_CURRENT_NAME[] = 	"RPS_BAT_CURRENT";
const char REG_RPS_TEMPER_NAME[] = 			"RPS_TEMPER";
const char REG_RPS_LED_STATE_NAME[] = 		"RPS_LED_STATE";
const char REG_RPS_BAT_KEY_NAME[] = 		"RPS_BAT_KEY";
const char REG_RPS_CHRG_KEY_NAME[] = 		"RPS_CHRG_KEY";
const char REG_RPS_REL_STATE_NAME[] = 		"RPS_REL_STATE";
const char REG_RPS_MIN_VOLTAGE_NAME[] = 	"RPS_MIN_VOLTAGE";
const char REG_RPS_DISCH_VOLTAGE_NAME[] = 	"RPS_DISCH_VOLTAGE";
const char REG_RPS_REMAIN_TIME_NAME[] = 	"RPS_REMAIN_TIME";
const char REG_RPS_TEST_OK_NAME[] = 		"RPS_TEST_OK";
const char REG_RPS_CPU_ID_NAME[] = 			"RPS_CPU_ID";
const char REG_RPS_LTC4151_OK_NAME[] = 		"RPS_LTC4151_OK";
const char REG_RPS_ADC_BAT_VOLT_NAME[] = 	"RPS_ADC_BAT_VOLT";
const char REG_RPS_ADC_BAT_CURR_NAME[] = 	"RPS_ADC_BAT_CURR";
const char REG_RPS_TEST_MODE_NAME[] = 		"RPS_TEST_MODE";
//SHT
const char REG_SHT_CONNECTED_NAME[] = 		"SHT_CONNECTED";
const char REG_SHT_TYPE_NAME[] = 			"SHT_TYPE";
const char REG_SHT_TEMPERATURE_NAME[] = 	"SHT_TEMPERATURE";
const char REG_SHT_HUMIDITY_NAME[] = 		"SHT_HUMIDITY";
//SFP1
const char REG_SFP1_PRESENT_NAME[] = 		"SFP1_PRESENT";
const char REG_SFP1_LOS_NAME[] = 			"SFP1_LOS";
const char REG_SFP1_VENDOR_NAME[] = 		"SFP1_VENDOR";
const char REG_SFP1_VENDOR_OUI_NAME[] = 	"SFP1_VENDOR_OUI";
const char REG_SFP1_VENDOR_PN_NAME[] = 		"SFP1_VENDOR_PN";
const char REG_SFP1_VENDOR_REV_NAME[] = 	"SFP1_VENDOR_REV";
const char REG_SFP1_CONNECTOR_NAME[] = 		"SFP1_CONNECTOR";
const char REG_SFP1_TEMPER_NAME[] = 		"SFP1_TEMPER";
const char REG_SFP1_VOLTAGE_NAME[] = 		"SFP1_VOLTAGE";
const char REG_SFP1_TX_BIAS_NAME[] = 		"SFP1_TX_BIAS";
const char REG_SFP1_TX_POWER_NAME[] = 		"SFP1_TX_POWER";
const char REG_SFP1_RX_POWER_NAME[] = 		"SFP1_RX_POWER";

//SFP2
const char REG_SFP2_PRESENT_NAME[] = 		"SFP2_PRESENT";
const char REG_SFP2_LOS_NAME[] = 			"SFP2_LOS";
const char REG_SFP2_VENDOR_NAME[] = 		"SFP2_VENDOR";
const char REG_SFP2_VENDOR_OUI_NAME[] = 	"SFP2_VENDOR_OUI";
const char REG_SFP2_VENDOR_PN_NAME[] = 		"SFP2_VENDOR_PN";
const char REG_SFP2_VENDOR_REV_NAME[] = 	"SFP2_VENDOR_REV";
const char REG_SFP2_CONNECTOR_NAME[] = 		"SFP2_CONNECTOR";
const char REG_SFP2_TEMPER_NAME[] = 		"SFP2_TEMPER";
const char REG_SFP2_VOLTAGE_NAME[] = 		"SFP2_VOLTAGE";
const char REG_SFP2_TX_BIAS_NAME[] = 		"SFP2_TX_BIAS";
const char REG_SFP2_TX_POWER_NAME[] = 		"SFP2_TX_POWER";
const char REG_SFP2_RX_POWER_NAME[] = 		"SFP2_RX_POWER";

//RTC
const char REG_RTC_STATUS_NAME[] = 			"RTC_STATUS";
const char REG_RTC_YEAR_NAME[] = 			"RTC_YEAR";
const char REG_RTC_MONTH_NAME[] = 			"RTC_MONTH";
const char REG_RTC_DAY_NAME[] = 			"RTC_DAY";
const char REG_RTC_WEEKDAY_NAME[] = 		"RTC_WEEKDAY";
const char REG_RTC_HOUR_NAME[] = 			"RTC_HOUR";
const char REG_RTC_MINUTE_NAME[] = 			"RTC_MINUTE";
const char REG_RTC_SECOND_NAME[] = 			"RTC_SECOND";

//POE
const char REG_POE_ID_NAME[] = 				"POE_ID";
const char REG_POE_STATE_NAME[] = 			"POE_STATE";
const char REG_POE_BANK_NAME[] = 			"POE_BANK";
const char REG_POE_MODE_NAME[] = 			"POE_MODE";

#endif
