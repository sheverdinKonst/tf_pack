 /****************
* HwSys Controller
* Make request to Unix Domain Socket to get/set param by string name
*
* argv[1] - parametrs name
* argv[2] - operation code [get, set, reset]
* argv[3] - value

* example: /usr/bin/request_ctrl REG_RPS_BAT_VOLTAGE get --get UPS voltage, return voltage value
* output: "24.4"
*
* example: /usr/bin/request_ctrl RELAY1 set 1 --set relay state to open state, if no error return 'ok'
* output: "ok"
*
* example: /usr/bin/request_ctrl SYS_RESET reset
* output: "ok"
*
* example: /usr/bin/request_ctrl IOMCU_RESET set 0
* output: "ok"

* Realtek`s line:
* SYS_LED
* SYS_RESET
* IOMCU_RESET
* POE_RESET
* BOOT0
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
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <syslog.h>
#include <gpiod.h>

#include "registers.h"

#define IF_CMP(x) if(strcmp(x,name)==0)

#define errcheck(rc, funcName) \
	if (rc == -1) { perror(funcName); return EXIT_FAILURE; }

int get_i2c_addr_by_name(char *name);
int get_gpio_by_name(char *name);

int get_len_by_addr(int addr);
int get_type_by_addr(int addr);


int main(int argc, char **argv) {
	int regaddr;
    int32_t i32val;
	uint16_t tmp16;
    char *end;
	struct gpiod_chip *chip;
	struct gpiod_line *line;
    //struct gpiod_chip_info *gpioChipInfo;
    //struct gpiod_line_info *gpioLineInfo;

    //const char *chipName;
    //const char *chipLabel;
    //size_t chipNumLines;
    //bool isChip;

    //unsigned int lineOffset;
    //const char * lineName;
    //const char * lineConsumer;
    //int lineDirection;
    //int lineActiveState;
    //int lineValue;
    //bool lineIsUsed;
    //bool lineOpenSource;

    int line_id;
    int value,req;
	static const char serverPath[] = "/var/run/hwsys-uds.sock";
	struct sock_msg_t sock_msg;

	if(argc < 3){
		printf("Incorrect arguments\n");
		return -1;
	}



	regaddr = get_i2c_addr_by_name(argv[1]);
	if(regaddr != -1){
		//use UDS Socket to communicate
		int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		errcheck(sock_fd, "socket");

		struct sockaddr_un serverAddress = {0};
		serverAddress.sun_family = AF_UNIX;
		strcpy(serverAddress.sun_path,serverPath);



		//bind socket to Path
		int rc = connect(sock_fd,(const struct sockaddr *)(&serverAddress),sizeof(serverAddress));
		if(rc == -1 ) {
			printf("I2C Daemon not started?\n");
			errcheck(rc,"connect");
		}
		//use 
		sock_msg.addr = regaddr;
		//get value
		if(strcmp(argv[2],"get")==0){
			
			sock_msg.opcode = I2C_OPCODE_READ;
			sock_msg.len = get_len_by_addr(sock_msg.addr);
			sock_msg.type = get_type_by_addr(sock_msg.addr);
			write(sock_fd,&sock_msg,sizeof(sock_msg));
			rc = read(sock_fd,&sock_msg,sizeof(sock_msg));
			if(rc < 0){
				printf("error read\n");
				close(sock_fd);
				return -1;
			}

			if(sock_msg.opcode == I2C_OPCODE_RESPONSE){

				if(sock_msg.value[0] == 0xAA && sock_msg.value[1] == 0xAA){
					printf("error response\n");
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
						printf("%d\n",(uint32_t)(sock_msg.value[0] | sock_msg.value[1]<<8 | sock_msg.value[2]<<16 | sock_msg.value[3]<<24));
						break;
					default:
						printf("%s\n",sock_msg.value);
				}
				//printf("recieve: len=%d  [0]=%x [1]=%x [2]=%x [3]=%x\n",sock_msg.len, sock_msg.value[0], sock_msg.value[1], sock_msg.value[2], sock_msg.value[3]);
			}
			else{
				printf("error opcode: %x\n",sock_msg.opcode);
			}
		}
		//set value
		else if(strcmp(argv[2],"set")==0){
			sock_msg.opcode = I2C_OPCODE_WRITE;
			sock_msg.len = get_len_by_addr(sock_msg.addr);
			if(argv[3]!=NULL)
				i32val = (uint8_t)strtol(argv[3],&end,10);
			else
				i32val = 0;
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


	//if Realtek GPIO manage
	line_id = get_gpio_by_name(argv[1]);
	if(line_id != -1){
		// *************************** LINE *********************************
		chip = gpiod_chip_open("/dev/gpiochip0");
		if (!chip){
			openlog("gpio_ctrl", LOG_PID, LOG_USER);
			syslog(LOG_ERR, "ERROR gpiod_chip_open" );
			closelog();
			return -1;
		}				
	
		line = gpiod_chip_get_line(chip, line_id);
		if (!line) {
			openlog("gpio_ctrl", LOG_PID, LOG_USER);
			syslog(LOG_ERR, "ERROR gpiod_chip_get_line %d\n", line_id);
			closelog();
			gpiod_chip_close(chip);
			return -1;
		}

		//GET opcode
		if(strcmp(argv[2],"get")==0){
			req = gpiod_line_request_input(line, "gpio");
			if (req) {
				openlog("gpio_ctrl", LOG_PID, LOG_USER);
				syslog(LOG_ERR, "ERROR gpiod_line_request_input %d\n",line_id);
				closelog();
				gpiod_chip_close(chip);
				return -1;
			}
			value = gpiod_line_get_value(line);			
			req = gpiod_line_request_output(line, "gpio", value);
			printf("%d\n",value);
		}
		else if(strcmp(argv[2],"set")==0){
			if(argc < 4){
				printf("Incorrect set value\n");
				gpiod_chip_close(chip);
				return -1;
			}

			req = gpiod_line_request_output(line, "gpio",0);
			if (req) {
				openlog("gpio_ctrl", LOG_PID, LOG_USER);
				syslog(LOG_ERR, "ERROR gpiod_line_request_output %d\n",line_id);
				closelog();
				gpiod_chip_close(chip);
				return -1;
			}
			value = strtoul(argv[3],NULL,10);
			gpiod_line_set_value(line, value);
			printf("ok\n");
		}
		else if(strcmp(argv[2],"reset")==0){
			req = gpiod_line_request_output(line, "gpio",0);
			if (req) {
				openlog("gpio_ctrl", LOG_PID, LOG_USER);
				syslog(LOG_ERR, "ERROR gpiod_line_request_output %d\n",line_id);
				closelog();
				gpiod_chip_close(chip);
				return -1;
			}
			gpiod_line_set_value(line,0);
			sleep(1);
			gpiod_line_set_value(line,1);
			printf("ok\n");
		}
		else{
			openlog("gpio_ctrl", LOG_PID, LOG_USER);
			syslog(LOG_ERR, "ERROR: unknown command %s\n",argv[2]);
			closelog();
			gpiod_chip_close(chip);
			return -1;
		}
		gpiod_chip_close(chip);
	}
	else{
		printf("Unknown command\n");
	}
	return 0;
}

int get_gpio_by_name(char *name){
	IF_CMP("SYS_LED") return RTL_GPIO_SYS_LED;
	IF_CMP("SYS_RESET") return RTL_GPIO_SYS_RESET;
	IF_CMP("IOMCU_RESET") return RTL_GPIO_IOMCU_RESET;
	IF_CMP("POE_RESET") return RTL_GPIO_POE_RESET;
	IF_CMP("BOOT0") return RTL_GPIO_BOOT0;
	return -1;
}


int get_i2c_addr_by_name(char *name){

	IF_CMP(REG_INTSTAT_NAME) return REG_INTSTAT;
	IF_CMP(REG_INTMASK_NAME) return REG_INTMASK;
	IF_CMP(REG_HW_VERS_NAME) return REG_HW_VERS;
	IF_CMP(REG_SW_VERS_NAME) return REG_SW_VERS;
	IF_CMP(REG_ADC_CH1_NAME) return REG_ADC_CH1;
	IF_CMP(REG_ADC_CH2_NAME) return REG_ADC_CH2;
	IF_CMP(REG_ADC_CH3_NAME) return REG_ADC_CH3;

	IF_CMP(REG_TAMPER_NAME) return REG_TAMPER;
	IF_CMP(REG_SENSOR1_NAME) return REG_SENSOR1;
	IF_CMP(REG_SENSOR2_NAME) return REG_SENSOR2;
	IF_CMP(REG_RELAY1_NAME) return REG_RELAY1;
	IF_CMP(REG_DEFAULT_BUTTON_NAME) return REG_DEFAULT_BUTTON;
	IF_CMP(REG_DEFAULT_LED_NAME) return REG_DEFAULT_LED;
	IF_CMP(REG_DEFAULT_LONG_PRESSED_NAME) return REG_DEFAULT_LONG_PRESSED;

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
	IF_CMP(REG_SFP1_IDENTIFIER_NAME) return REG_SFP1_IDENTIFIER;
	IF_CMP(REG_SFP1_CONNECTOR_NAME) return REG_SFP1_CONNECTOR;
	IF_CMP(REG_SFP1_TYPE_NAME) return REG_SFP1_TYPE;
	IF_CMP(REG_SFP1_LINK_LEN_NAME) return REG_SFP1_LINK_LEN;
	IF_CMP(REG_SFP1_FIBER_TEC_NAME) return REG_SFP1_FIBER_TEC;
	IF_CMP(REG_SFP1_MEDIA_NAME) return REG_SFP1_MEDIA;
	IF_CMP(REG_SFP1_SPEED_NAME) return REG_SFP1_SPEED;
	IF_CMP(REG_SFP1_ENCODING_NAME) return REG_SFP1_ENCODING;
	IF_CMP(REG_SFP1_WAVELEN_NAME) return REG_SFP1_WAVELEN;
	IF_CMP(REG_SFP1_NBR_NAME) return REG_SFP1_NBR;
	IF_CMP(REG_SFP1_LEN9_NAME) return REG_SFP1_LEN9;
	IF_CMP(REG_SFP1_LEN50_NAME) return REG_SFP1_LEN50;
	IF_CMP(REG_SFP1_LEN62_NAME) return REG_SFP1_LEN62;
	IF_CMP(REG_SFP1_LENC_NAME) return REG_SFP1_LENC;
	IF_CMP(REG_SFP1_TEMPER_NAME) return REG_SFP1_TEMPER;
	IF_CMP(REG_SFP1_VOLTAGE_NAME) return REG_SFP1_VOLTAGE;
	IF_CMP(REG_SFP1_CURRENT_NAME) return REG_SFP1_CURRENT;
	IF_CMP(REG_SFP1_TX_BIAS_NAME) return REG_SFP1_TX_BIAS;
	IF_CMP(REG_SFP1_TX_POWER_NAME) return REG_SFP1_TX_POWER;
	IF_CMP(REG_SFP1_RX_POWER_NAME) return REG_SFP1_RX_POWER;

	IF_CMP(REG_SFP2_PRESENT_NAME) return REG_SFP2_PRESENT;
	IF_CMP(REG_SFP2_LOS_NAME) return REG_SFP2_LOS;
	IF_CMP(REG_SFP2_VENDOR_NAME) return REG_SFP2_VENDOR;
	IF_CMP(REG_SFP2_VENDOR_OUI_NAME) return REG_SFP2_VENDOR_OUI;
	IF_CMP(REG_SFP2_VENDOR_PN_NAME) return REG_SFP2_VENDOR_PN;
	IF_CMP(REG_SFP2_VENDOR_REV_NAME) return REG_SFP2_VENDOR_REV;
	IF_CMP(REG_SFP2_IDENTIFIER_NAME) return REG_SFP2_IDENTIFIER;
	IF_CMP(REG_SFP2_CONNECTOR_NAME) return REG_SFP2_CONNECTOR;
	IF_CMP(REG_SFP2_TYPE_NAME) return REG_SFP2_TYPE;
	IF_CMP(REG_SFP2_LINK_LEN_NAME) return REG_SFP2_LINK_LEN;
	IF_CMP(REG_SFP2_FIBER_TEC_NAME) return REG_SFP2_FIBER_TEC;
	IF_CMP(REG_SFP2_MEDIA_NAME) return REG_SFP2_MEDIA;
	IF_CMP(REG_SFP2_SPEED_NAME) return REG_SFP2_SPEED;
	IF_CMP(REG_SFP2_ENCODING_NAME) return REG_SFP2_ENCODING;
	IF_CMP(REG_SFP2_WAVELEN_NAME) return REG_SFP2_WAVELEN;
	IF_CMP(REG_SFP2_NBR_NAME) return REG_SFP2_NBR;
	IF_CMP(REG_SFP2_LEN9_NAME) return REG_SFP2_LEN9;
	IF_CMP(REG_SFP2_LEN50_NAME) return REG_SFP2_LEN50;
	IF_CMP(REG_SFP2_LEN62_NAME) return REG_SFP2_LEN62;
	IF_CMP(REG_SFP2_LENC_NAME) return REG_SFP2_LENC;
	IF_CMP(REG_SFP2_TEMPER_NAME) return REG_SFP2_TEMPER;
	IF_CMP(REG_SFP2_VOLTAGE_NAME) return REG_SFP2_VOLTAGE;
	IF_CMP(REG_SFP2_CURRENT_NAME) return REG_SFP2_CURRENT;
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
		return 2;


	case REG_TAMPER:
	case REG_SENSOR1:
	case REG_SENSOR2:
	case REG_RELAY1:
	case REG_DEFAULT_BUTTON:
	case REG_DEFAULT_LED:
		return 1;
	case REG_DEFAULT_LONG_PRESSED:
		return 2;
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
		return 1;
	case REG_RPS_CPU_ID:
		return 2;
	case REG_RPS_LTC4151_OK:
		return 1;
	case REG_RPS_ADC_BAT_VOLT:
	case REG_RPS_ADC_BAT_CURR:
		return 2;
	case REG_RPS_TEST_MODE:
		return 1;
	//SHT
	case REG_SHT_CONNECTED:
		return 1;
	case REG_SHT_TYPE:
		return 10;
	case REG_SHT_TEMPERATURE:
		return 2;
	case REG_SHT_HUMIDITY:
		return 1;
	//SFP1
	case REG_SFP1_PRESENT:
	case REG_SFP1_LOS:
		return 1;
	case REG_SFP1_VENDOR:
		return 16;
	case REG_SFP1_VENDOR_OUI:
		return 3;
	case REG_SFP1_VENDOR_PN:
		return 16;
	case REG_SFP1_VENDOR_REV:
	case REG_SFP1_IDENTIFIER:
		return 4;
	case REG_SFP1_CONNECTOR:
		return 30;
	case REG_SFP1_TYPE:
		return 16;	
	case REG_SFP1_LINK_LEN:
		return 25;
	case REG_SFP1_FIBER_TEC:
	case REG_SFP1_MEDIA:
		return 32;
	case REG_SFP1_SPEED:
	case REG_SFP1_ENCODING:
		return 16;
	case REG_SFP1_WAVELEN:
	case REG_SFP1_NBR:
	case REG_SFP1_LEN9:
	case REG_SFP1_LEN50:
	case REG_SFP1_LEN62:
	case REG_SFP1_LENC:
	case  REG_SFP1_TEMPER:
	case  REG_SFP1_VOLTAGE:
	case  REG_SFP1_TX_BIAS:
		return 2;
	case  REG_SFP1_TX_POWER:
	case  REG_SFP1_RX_POWER:
		return 16;

	//SFP2
	case  REG_SFP2_PRESENT:
	case  REG_SFP2_LOS:
		return 1;
	case REG_SFP2_VENDOR:	
		return 16;
	case  REG_SFP2_VENDOR_OUI:
		return 3;
	case  REG_SFP2_VENDOR_PN:
		return 16;
	case  REG_SFP2_VENDOR_REV:
	case REG_SFP2_IDENTIFIER:
		return 4;
	case  REG_SFP2_CONNECTOR:
		return 30;
	case REG_SFP2_TYPE:
		return 16;	
	case REG_SFP2_LINK_LEN:
		return 25;
	case REG_SFP2_FIBER_TEC:
	case REG_SFP2_MEDIA:
		return 32;
	case REG_SFP2_SPEED:
	case REG_SFP2_ENCODING:
		return 16;
	case REG_SFP2_WAVELEN:
	case REG_SFP2_NBR:
	case REG_SFP2_LEN9:
	case REG_SFP2_LEN50:
	case REG_SFP2_LEN62:
	case REG_SFP2_LENC:
	case  REG_SFP2_TEMPER:
	case  REG_SFP2_VOLTAGE:
	case  REG_SFP2_TX_BIAS:
		return 2;
	case  REG_SFP2_TX_POWER:
	case  REG_SFP2_RX_POWER:
		return 16;

	//RTC
	case  REG_RTC_STATUS:	
	case  REG_RTC_YEAR:	
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
	return 1;
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
	return TYPE_UINT;
}

