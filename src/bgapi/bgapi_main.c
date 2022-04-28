/*****************************************************************************
 * @file  bgapi-main.c
 * @brief 
 *******************************************************************************
 Copyright 2020 GL-iNet. https://www.gl-inet.com/

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   
#include <fcntl.h>   
#include <signal.h>
#include <stdbool.h>

#include "gl_uart.h"
#include "gl_hal.h"
#include "bgapi_main.h"
#include "host_gecko.h"
#include "silabs_msg.h"

static void process_bar(uint32_t pro);

bool module_work = false;
bool sync_dfu_boot = false;
static int g_verbose = 0;

#define MAX_DFU_PACKET 		(96)
#define MAX_WAIT_RESET_TIME	(3 * 10) // unit: 100ms
#define MAX_WAIT_SYNC_TIME	(3 * 10) // unit: 100ms

char TURN_ON_RESET[64];
char TURN_OFF_RESET[64];
char TURN_ON_DFU_ENABLE[64];
char TURN_OFF_DFU_ENABLE[64];


#define WARNNING_MSG		"gl-silabs-dfu bgapi [dfu mode] [Upgrade file path] [Uart] [Reset IO] [DFU enable IO] [-v]"
#define SOFT_WARNNING_MSG	"gl-silabs-dfu bgapi soft [Upgrade file path] [Uart] [Reset IO] [-v]"
#define HARD_WARNNING_MSG	"gl-silabs-dfu bgapi hard [Upgrade file path] [Uart] [Reset IO] [DFU enable IO] [-v]"

static void init_sys_gpio(char* gpio_num);
static void destroy_sys_gpio(char* gpio_num);


int bgapi_main(int argc, char *argv[])
{
    bool hardware_reset_dfu = true;

	if (argc < 5)
	{
		printf("%s\n", WARNNING_MSG);
		return -1;
	}

    if(0 == strncmp(argv[2], "soft", strlen("soft")))
    {
        hardware_reset_dfu = false;
		if(argc < 6)
		{
			printf("%s\n", SOFT_WARNNING_MSG);
			return -1;
		}
		if (argv[6] != NULL && !strncmp(argv[6], "-v", 2))
		{
			g_verbose = 1;
		}
    }else if(0 == strncmp(argv[2], "hard", strlen("hard"))) {
        hardware_reset_dfu = true;
		if(argc < 7)
		{
			printf("%s\n", HARD_WARNNING_MSG);
			return -1;
		}
		if (argv[7] != NULL && !strncmp(argv[7], "-v", 2))
		{
			g_verbose = 1;
		}		
		init_sys_gpio(argv[6]);
		sprintf(TURN_ON_DFU_ENABLE, "echo 1 > /sys/class/gpio/gpio%s/value", argv[6]);
		sprintf(TURN_OFF_DFU_ENABLE, "echo 0 > /sys/class/gpio/gpio%s/value", argv[6]);
    }else {
        printf("Unsupport bgapi dfu mode! Support bgapi dfu mode: soft | hard\n");
        return -1;
    }

	init_sys_gpio(argv[5]);
    sprintf(TURN_ON_RESET, "echo 1 > /sys/class/gpio/gpio%s/value", argv[5]);
    sprintf(TURN_OFF_RESET, "echo 0 > /sys/class/gpio/gpio%s/value", argv[5]);

	char file_path[128] = {0};
	strncpy(file_path, argv[3], strlen(argv[3]));
	
	// open upgrade file
	FILE *dfu_file = NULL;
	dfu_file = fopen(file_path, "rb");
	if (dfu_file == NULL) {
		printf("cannot open file %s\n", argv[3]);
		return -1;
	}

	uint8_t dfu_data[MAX_DFU_PACKET];
	size_t dfu_toload = 0;
	size_t dfu_total = 0;

	// get file size
	if (fseek(dfu_file, 0L, SEEK_END)) {
		goto exit1;
	}
	dfu_total = dfu_toload = ftell(dfu_file);
	if (fseek(dfu_file, 0L, SEEK_SET)) {
		goto exit1;
	}
	printf("Get upgrade firmware size: %d\n", dfu_total);

	// init uart
	int uart_handle = hal_init(argv[4], 115200, 0);
	if(-1 == uart_handle)
	{
		printf("Uart init error!\n");
		goto exit1;
	}

	// create recv thread
	int ret = -1;
	pthread_t ble_driver_thread_ctx;
	ret = pthread_create(&ble_driver_thread_ctx, NULL, silabs_driver, NULL);

	// hard reset module
	system(TURN_OFF_RESET);
	usleep(100*1000);
	system(TURN_ON_RESET);

	// wait for module first boot
	int wait_msec = 0;
	while (!module_work)
	{
		usleep(100000); // 100ms
		wait_msec++;
		if(wait_msec > MAX_WAIT_RESET_TIME)
		{
			printf("Wait for module reboot timeout!\n");
			goto exit;
		}
	}

	// dfu reset and wait boot sync
    if(hardware_reset_dfu)
    {
        system(TURN_OFF_RESET);
        usleep(100*1000);
        system(TURN_OFF_DFU_ENABLE);
        usleep(100*1000);
        system(TURN_ON_RESET);
    }else{
        gecko_cmd_dfu_reset(1);
    }

	wait_msec = 0;
	while(!sync_dfu_boot)
	{
		usleep(100000); // 100ms
		wait_msec++;
		if(wait_msec > MAX_WAIT_SYNC_TIME)
		{
			printf("Wait for bootloader sync timeout!\n");
			goto exit;
		}
	}

	// set firmware address
	struct gecko_msg_dfu_flash_set_address_rsp_t* set_addr_rsp = gecko_cmd_dfu_flash_set_address(0);
	if (set_addr_rsp->result)
	{
		printf("gl_ble_dfu_flash_set_address failed: %d\n", set_addr_rsp->result);
		goto exit;
	}

	// upload firmware
	size_t dfu_size = 0;
	size_t current_pos = 0;
	int num = 0;
	float pro = 0;
	uint32_t show = 0;
	while(dfu_toload > 0) 
	{
		dfu_size = dfu_toload > sizeof(dfu_data) ? sizeof(dfu_data) : dfu_toload;
		if (fread(dfu_data, 1, dfu_size, dfu_file) != dfu_size) 
		{
			printf("%llu:%llu:%llu\n", current_pos, dfu_size, dfu_total);
			printf("File read failure\n");
			goto exit;
		}
		
		if(g_verbose)
		{
			pro = (float)current_pos/dfu_total;
			pro = pro * 100;
			show = pro;
			process_bar(show);
		}

		struct gecko_msg_dfu_flash_upload_rsp_t* flash_upload_rsp = gecko_cmd_dfu_flash_upload(dfu_size, dfu_data);
		if (flash_upload_rsp->result) 
		{
			//error
			printf("\nError in upload %d\n", flash_upload_rsp->result);
			goto exit;
		}

		current_pos += dfu_size;
		dfu_toload -= dfu_size;
		usleep(10000);
	}

	usleep(100000);
	// finish upload
	struct gecko_msg_dfu_flash_upload_finish_rsp_t* upload_finish_rsp = gecko_cmd_dfu_flash_upload_finish();
	if (upload_finish_rsp->result) 
	{
		printf("\nError in finish upload %d\n", upload_finish_rsp->result);
		goto exit;
	}else{
		printf("*** DFU END! ***\n");
	}

	module_work = false;
	usleep(100000);
	gecko_cmd_dfu_reset(0);
	wait_msec = 0;
	while (!module_work)
	{
		usleep(100000); // 100ms
		wait_msec++;
		if(wait_msec > MAX_WAIT_RESET_TIME)
		{
			printf("Wait for module reboot timeout!\n");
			goto exit;
		}
	}

	printf("Module reset finish, please check firmware version.\n");
	usleep(100000);

exit:
	destroy_sys_gpio(argv[5]);
	if(hardware_reset_dfu)
	{
		destroy_sys_gpio(argv[6]);
	}
	pthread_cancel(ble_driver_thread_ctx);
	pthread_join(ble_driver_thread_ctx, 0);
	hal_destroy();
exit1:
	fclose(dfu_file);

	return 0;
}

static void process_bar(uint32_t pro)
{
    static char buf[102] = {0};

    if (pro <= 100) {
        printf("\rprocess:[%-100s]%d%% ", buf, pro);
        fflush(stdout);
        buf[pro] = '=';
        buf[pro+1] = '>';

        if (pro == 100) {
            printf("\n");
        }
    }
}

static void init_sys_gpio(char* gpio_num)
{
	char init_gpio_num[50] = {0};
	char init_gpio_dir[50] = {0};

	sprintf(init_gpio_num, "echo %s > /sys/class/gpio/export", gpio_num);
	system(init_gpio_num);

	sprintf(init_gpio_dir, "echo out > /sys/class/gpio/gpio%s/direction", gpio_num);
	system(init_gpio_dir);

	return ;
}

static void destroy_sys_gpio(char* gpio_num)
{
	char init_gpio_num[50] = {0};
	char init_gpio_dir[50] = {0};

	sprintf(init_gpio_num, "echo %s > /sys/class/gpio/unexport", gpio_num);
	system(init_gpio_num);

	return ;
}
