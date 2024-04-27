 /****************
* Request Controller
* Make request to Unix Domain Socket to get/set param by string name
*
* example: ./request_ctrl "REG_RPS_BAT_VOLTAGE" "get" --get UPS voltage, return voltage value
* output: "24.4"
*
* example: /usr/bin/request_ctrl RELAY1 set 1 --set relay state to open state, if no error return 'ok'
* output: "ok"
*****************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>

//#include "registers.h"
#include "/home/alex/tfortis_openwrt/openwrt_tfortis_packages/i2c_daemon/src/registers.h"

#define IF_CMP(x) if(strcmp(x,name)==0)

#define errcheck(rc, funcName) \
	if (rc == -1) { perror(funcName); return EXIT_FAILURE; }

int parse_command_name(char *name);
int get_len_by_addr(int addr);
int get_type_by_addr(int addr);

int main(int argc, char **argv) {
    int i2c_fd;
    int32_t i32val;
	uint16_t tmp16;
    char *end;
	static const char serverPath[] = "/var/run/i2c-uds.sock";

	struct sock_msg_t sock_msg;

	int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	errcheck(sock_fd, "socket");

	struct sockaddr_un serverAddress = {0};
	serverAddress.sun_family = AF_UNIX;
	strcpy(serverAddress.sun_path,serverPath);

	if(argc < 3){
		printf("Incorrect arguments\n");
		return -1;
	}

	//bind socket to Path
	int rc = connect(sock_fd,(const struct sockaddr *)(&serverAddress),sizeof(serverAddress));
	if(rc == -1 ) {
		printf("I2C Daemon not started?\n");
		errcheck(rc,"connect");
	}

	if(parse_command_name(argv[1])==-1){
		printf("Incorrect Name\n");
		return -1;
	}

	sock_msg.addr = parse_command_name(argv[1]);
	//get value
	if(strcmp(argv[2],"get")==0){

		sock_msg.opcode = I2C_OPCODE_READ;
		sock_msg.len = get_len_by_addr(sock_msg.addr);
		sock_msg.type = get_type_by_addr(sock_msg.addr);
		write(sock_fd,&sock_msg,sizeof(sock_msg));
		rc = read(sock_fd,&sock_msg,sizeof(sock_msg));
		if(rc < 0){
			printf("-1\n");
			close(sock_fd);
			return -1;
		}

		if(sock_msg.opcode == I2C_OPCODE_RESPONSE){

			if(sock_msg.value[0] == 0xAA && sock_msg.value[1] == 0xAA){
				printf("-1\n");
			}

			switch(sock_msg.len){
				case 1:
					//uint8
					printf("%d\n",sock_msg.value[0]);
					break;
				case 2:
					if(sock_msg.type == TYPE_PSEUDO_FLOAT){
						tmp16 = sock_msg.value[0] | sock_msg.value[1]<<8;
						if(tmp16 < 1000)
							printf("%d.%d\n",tmp16/10,tmp16%10);
						else
							printf("%d.%d\n",tmp16/1000,tmp16%1000);
					}
					else{
						//uint16
						printf("%d\n",sock_msg.value[0] | sock_msg.value[1]<<8);
					}
					break;
				case 4:
					//uint32
					printf("%lu\n",(uint32_t)(sock_msg.value[0] | sock_msg.value[1]<<8 | sock_msg.value[2]<<16 | sock_msg.value[3]<<24));
					break;
				default:

					printf("%s\n",sock_msg.value);
			}
			//printf("recieve: len=%d  [0]=%x [1]=%x [2]=%x [3]=%x\n",sock_msg.len, sock_msg.value[0], sock_msg.value[1], sock_msg.value[2], sock_msg.value[3]);

		}
		else{
			printf("-1\n");

		}
	}
	//set value
	else if(strcmp(argv[2],"set")==0){
		sock_msg.opcode = I2C_OPCODE_WRITE;
		sock_msg.len = get_len_by_addr(sock_msg.addr);
		i32val = (uint8_t)strtol(argv[3],&end,10);//todo
		sock_msg.value[0] = (uint8_t)i32val;
		sock_msg.value[1] = (uint8_t)(i32val<<8);
		sock_msg.value[2] = (uint8_t)(i32val<<16);
		sock_msg.value[3] = (uint8_t)(i32val<<24);
		write(sock_fd,&sock_msg,sizeof(sock_msg));
	}
	else{
		printf("Incorrect Type\n");
	}

	close(sock_fd);

	return 0;
}

int parse_command_name(char *name){

	IF_CMP(REG_INTSTAT_NAME) return REG_INTSTAT;
	IF_CMP(REG_INTMASK_NAME) return REG_INTMASK;
	IF_CMP(REG_HW_VERS_NAME) return REG_HW_VERS;
	IF_CMP(REG_SW_VERS_NAME) return REG_SW_VERS;
	IF_CMP(REG_ADC_CH1_NAME) return REG_ADC_CH1;

	IF_CMP(REG_TAMPER_NAME) return REG_TAMPER;
	IF_CMP(REG_SENSOR1_NAME) return REG_SENSOR1;
	IF_CMP(REG_SENSOR2_NAME) return REG_SENSOR2;
	IF_CMP(REG_RELAY1_NAME) return REG_RELAY1;
	IF_CMP(REG_DEFAULT_BUTTON_NAME) return REG_DEFAULT_BUTTON;
	IF_CMP(REG_DEFAULT_LED_NAME) return REG_DEFAULT_LED;

	IF_CMP(REG_RPS_CONNECTED_NAME) return REG_RPS_CONNECTED;
	IF_CMP(REG_RPS_HW_VERS_NAME) return REG_RPS_HW_VERS;
	IF_CMP(REG_RPS_SW_VERS_NAME) return REG_RPS_SW_VERS;
	IF_CMP(REG_RPS_VAC_NAME) return REG_RPS_VAC;
	IF_CMP(REG_RPS_BAT_VOLTAGE_NAME) return REG_RPS_BAT_VOLTAGE;
	IF_CMP(REG_RPS_CHRG_VOLTAGE_NAME) return REG_RPS_CHRG_VOLTAGE;
	IF_CMP(REG_RPS_BAT_CURRENT_NAME) return REG_RPS_BAT_CURRENT;
	IF_CMP(REG_RPS_TEMPER_NAME) return REG_RPS_TEMPER;
	IF_CMP(REG_RPS_LED_STATE_NAME) return REG_RPS_LED_STATE;
	IF_CMP(REG_RPS_BAT_KEY_NAME) return REG_RPS_BAT_KEY;
	IF_CMP(REG_RPS_CHRG_KEY_NAME) return REG_RPS_CHRG_KEY;
	IF_CMP(REG_RPS_REL_STATE_NAME) return REG_RPS_REL_STATE;
	IF_CMP(REG_RPS_MIN_VOLTAGE_NAME) return REG_RPS_MIN_VOLTAGE;
	IF_CMP(REG_RPS_DISCH_VOLTAGE_NAME) return REG_RPS_DISCH_VOLTAGE;
	IF_CMP(REG_RPS_REMAIN_TIME_NAME) return REG_RPS_REMAIN_TIME;
	IF_CMP(REG_RPS_TEST_OK_NAME) return REG_RPS_TEST_OK;
	IF_CMP(REG_RPS_CPU_ID_NAME) return REG_RPS_CPU_ID;
	IF_CMP(REG_RPS_LTC4151_OK_NAME) return REG_RPS_LTC4151_OK;
	IF_CMP(REG_RPS_ADC_BAT_VOLT_NAME) return REG_RPS_ADC_BAT_VOLT;
	IF_CMP(REG_RPS_ADC_BAT_CURR_NAME) return REG_RPS_ADC_BAT_CURR;
	IF_CMP(REG_RPS_TEST_MODE_NAME) return REG_RPS_TEST_MODE;

	IF_CMP(REG_SHT_CONNECTED_NAME) return REG_SHT_CONNECTED;
	IF_CMP(REG_SHT_TYPE_NAME) return REG_SHT_TYPE;
	IF_CMP(REG_SHT_TEMPERATURE_NAME) return REG_SHT_TEMPERATURE;
	IF_CMP(REG_SHT_HUMIDITY_NAME) return REG_SHT_HUMIDITY;

	IF_CMP(REG_SFP1_PRESENT_NAME) return REG_SFP1_PRESENT;
	IF_CMP(REG_SFP1_LOS_NAME) return REG_SFP1_LOS;
	IF_CMP(REG_SFP1_VENDOR_NAME) return REG_SFP1_VENDOR;
	IF_CMP(REG_SFP1_VENDOR_OUI_NAME) return REG_SFP1_VENDOR_OUI;
	IF_CMP(REG_SFP1_VENDOR_PN_NAME) return REG_SFP1_VENDOR_PN;
	IF_CMP(REG_SFP1_VENDOR_REV_NAME) return REG_SFP1_VENDOR_REV;
	IF_CMP(REG_SFP1_CONNECTOR_NAME) return REG_SFP1_CONNECTOR;
	IF_CMP(REG_SFP1_TEMPER_NAME) return REG_SFP1_TEMPER;
	IF_CMP(REG_SFP1_VOLTAGE_NAME) return REG_SFP1_VOLTAGE;
	IF_CMP(REG_SFP1_TX_BIAS_NAME) return REG_SFP1_TX_BIAS;
	IF_CMP(REG_SFP1_TX_POWER_NAME) return REG_SFP1_TX_POWER;
	IF_CMP(REG_SFP1_RX_POWER_NAME) return REG_SFP1_RX_POWER;

	IF_CMP(REG_SFP2_PRESENT_NAME) return REG_SFP2_PRESENT;
	IF_CMP(REG_SFP2_LOS_NAME) return REG_SFP2_LOS;
	IF_CMP(REG_SFP2_VENDOR_NAME) return REG_SFP2_VENDOR;
	IF_CMP(REG_SFP2_VENDOR_OUI_NAME) return REG_SFP2_VENDOR_OUI;
	IF_CMP(REG_SFP2_VENDOR_PN_NAME) return REG_SFP2_VENDOR_PN;
	IF_CMP(REG_SFP2_VENDOR_REV_NAME) return REG_SFP2_VENDOR_REV;
	IF_CMP(REG_SFP2_CONNECTOR_NAME) return REG_SFP2_CONNECTOR;
	IF_CMP(REG_SFP2_TEMPER_NAME) return REG_SFP2_TEMPER;
	IF_CMP(REG_SFP2_VOLTAGE_NAME) return REG_SFP2_VOLTAGE;
	IF_CMP(REG_SFP2_TX_BIAS_NAME) return REG_SFP2_TX_BIAS;
	IF_CMP(REG_SFP2_TX_POWER_NAME) return REG_SFP2_TX_POWER;
	IF_CMP(REG_SFP2_RX_POWER_NAME) return REG_SFP2_RX_POWER;

	IF_CMP(REG_RTC_STATUS_NAME) return REG_RTC_STATUS;
	IF_CMP(REG_RTC_YEAR_NAME) return REG_RTC_YEAR;
	IF_CMP(REG_RTC_MONTH_NAME) return REG_RTC_MONTH;
	IF_CMP(REG_RTC_DAY_NAME) return REG_RTC_DAY;
	IF_CMP(REG_RTC_WEEKDAY_NAME) return REG_RTC_WEEKDAY;
	IF_CMP(REG_RTC_HOUR_NAME) return REG_RTC_HOUR;
	IF_CMP(REG_RTC_MINUTE_NAME) return REG_RTC_MINUTE;
	IF_CMP(REG_RTC_SECOND_NAME) return REG_RTC_SECOND;

	IF_CMP(REG_POE_ID_NAME) return REG_POE_ID;
	IF_CMP(REG_POE_STATE_NAME) return REG_POE_STATE;
	IF_CMP(REG_POE_BANK_NAME) return REG_POE_BANK;
	IF_CMP(REG_POE_MODE_NAME) return REG_POE_MODE;



	return -1;
}

int get_len_by_addr(int addr){
	switch(addr){
	case REG_INTSTAT:
	case REG_INTMASK:
	case REG_HW_VERS:
	case REG_SW_VERS:
		return 1;
	case REG_ADC_CH1:
	case REG_ADC_CH2:
	case REG_ADC_CH3:
	case REG_ADC_CH4:
		return 2;


	case REG_TAMPER:
	case REG_SENSOR1:
	case REG_SENSOR2:
	case REG_RELAY1:
	case REG_DEFAULT_BUTTON:
	case REG_DEFAULT_LED:
		return 1;
	//UPS
	case REG_RPS_CONNECTED:
	case REG_RPS_HW_VERS:
	case REG_RPS_SW_VERS:
	case REG_RPS_VAC:
	case REG_RPS_TEMPER:
		return 1;
	case REG_RPS_BAT_VOLTAGE:
	case REG_RPS_CHRG_VOLTAGE:
	case REG_RPS_BAT_CURRENT:

		return 2;
	case REG_RPS_LED_STATE:
	case REG_RPS_BAT_KEY:
	case REG_RPS_CHRG_KEY:
	case REG_RPS_REL_STATE:
		return 1;
	case REG_RPS_MIN_VOLTAGE:
	case REG_RPS_DISCH_VOLTAGE:
	case REG_RPS_REMAIN_TIME:
		return 2;
	case REG_RPS_TEST_OK:
	case REG_RPS_CPU_ID:
	case REG_RPS_LTC4151_OK:
		return 1;
	case REG_RPS_ADC_BAT_VOLT:
	case REG_RPS_ADC_BAT_CURR:
		return 2;
	case REG_RPS_TEST_MODE:
		return 1;
	//SHT
	case REG_SHT_CONNECTED:
	case REG_SHT_TYPE:
		return 1;
	case REG_SHT_TEMPERATURE:
		return 2;
	case REG_SHT_HUMIDITY:
		return 1;
	//SFP1
	case  REG_SFP1_PRESENT:
	case  REG_SFP1_LOS:
		return 1;
	case  REG_SFP1_VENDOR:
		return 16;
	case  REG_SFP1_VENDOR_OUI:
		return 3;
	case  REG_SFP1_VENDOR_PN:
		return 16;
	case  REG_SFP1_VENDOR_REV:
		return 4;
	case  REG_SFP1_CONNECTOR:
		return 1;
	case  REG_SFP1_TEMPER:
	case  REG_SFP1_VOLTAGE:
	case  REG_SFP1_TX_BIAS:
	case  REG_SFP1_TX_POWER:
	case  REG_SFP1_RX_POWER:
		return 2;

	//SFP2
	case  REG_SFP2_PRESENT:
	case  REG_SFP2_LOS:
		return 1;
	case  REG_SFP2_VENDOR:
		return 16;
	case  REG_SFP2_VENDOR_OUI:
		return 3;
	case  REG_SFP2_VENDOR_PN:
		return 16;
	case  REG_SFP2_VENDOR_REV:
		return 4;
	case  REG_SFP2_CONNECTOR:
		return 1;
	case  REG_SFP2_TEMPER:
	case  REG_SFP2_VOLTAGE:
	case  REG_SFP2_TX_BIAS:
	case  REG_SFP2_TX_POWER:
	case  REG_SFP2_RX_POWER:
		return 2;

	//RTC
	case  REG_RTC_STATUS:
		return 1;
	case  REG_RTC_YEAR:
		return 2;
	case  REG_RTC_MONTH:
	case  REG_RTC_DAY:
	case  REG_RTC_WEEKDAY:
	case  REG_RTC_HOUR:
	case  REG_RTC_MINUTE:
	case  REG_RTC_SECOND:
		return 1;

	//POE
	case  REG_POE_ID:
	case  REG_POE_STATE:
	case  REG_POE_BANK:
	case  REG_POE_MODE:
		return 1;
	}
}



int get_type_by_addr(int addr){
	switch(addr){
		case REG_SHT_TEMPERATURE:
		case REG_RPS_BAT_VOLTAGE:
		case REG_RPS_CHRG_VOLTAGE:
		case REG_RPS_MIN_VOLTAGE:
		case REG_RPS_DISCH_VOLTAGE:
		case REG_RPS_ADC_BAT_VOLT:
			return TYPE_PSEUDO_FLOAT;
		default:
			return TYPE_UINT;
	}

}