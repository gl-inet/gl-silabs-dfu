/*****************************************************************************
 * @file  main.c
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
#include <signal.h>
#include <stdbool.h>

#include "gl_uart.h"
#include "gl_hal.h"
#include "xmodem.h"



typedef enum {
    START_dfu,
    GET_BOOT_INFO,
    START_UPLOAD,
    START_TRANS,
    DEBUG_TEST,
}dfu_step_e;

char TURN_ON_RESET[64];
char TURN_OFF_RESET[64];
char TURN_ON_DFU_ENABLE[64];
char TURN_OFF_DFU_ENABLE[64];


int dfu_process(uint8_t *out_file,uint32_t file_size)
{
    int ret;
    static dfu_step_e step = START_dfu;

    switch (step) {
    case START_dfu: {
        /* 拉低GPIO 复位target */
        system(TURN_OFF_RESET);
        usleep(100*1000);
        system(TURN_OFF_DFU_ENABLE);
        usleep(100*1000);
        system(TURN_ON_RESET);
        usleep(100*1000);

        step = GET_BOOT_INFO;
    }
    break;
    case GET_BOOT_INFO: {
        uint8_t boot_info[100] = {0};
        uint32_t size = 0;

        size = uartRxTimeout(100,boot_info);
        size = size;
        if (strstr((const char *)boot_info,"1. upload gbl")) {
            system(TURN_ON_DFU_ENABLE);
            step = START_UPLOAD;
        } else {
            step = START_dfu;
        }
        // printf("boot_info=%s\r\n",boot_info);
        uartCacheClean();               /* 清除其他未读取出来的数据 */
    }
    break;
    case START_UPLOAD: {
        uint8_t upload_info[20] = {0};
        uint32_t size = 0;
        uint8_t write_msg[5] = {0};

        write_msg[0] = '1';          /* 启动 upload */
        uartTx(1,write_msg);

        usleep(500*1000);

        size = uartRxTimeout(19,upload_info);
        // printf("upload_info=%s\r\n",upload_info);
        if (strstr((const char *)upload_info,"begin upload")) {
            step = START_TRANS;
        } else {
            step = START_dfu;
            sleep(1000);
        }
    }
    break;
    case START_TRANS: {
        printf("start upload...\n");
        ret = xmodemTransmit(out_file,file_size);
        if (ret > 0) {
            printf("upload ok. total size:%d\r\n",ret);
            printf("reset chip now...\r\n");
            system(TURN_OFF_RESET);
            usleep(100*1000);
            system(TURN_ON_RESET);
            return 1;               /* upload 成功 */
        }
    }
    break;
    case DEBUG_TEST: {
        uint8_t debug_info[100] = {0};
        uint32_t size = 0;
        size = uartRxTimeout(10,debug_info);
        for (int i = 0;i < size;i++) {
        //    printf("%x ",debug_info[i]);
        }
        printf("\r\nsize=%d debug=%s\r\n",size,debug_info);

        return 1;
    }
    default:
        break;
    }

    return 0;
}



int main(int argc, char *argv[])
{
    int ret;

    if ((argc != 5)) {
        printf("gl-silabs-dfu [Upgrade file path] [Uart] [Reset IO] [DFU enable IO] \n");
		return -1;
    }

#if 0
    printf("argc=%d\n",argc);
    for (int i = 0;i < argc;i++) {
        printf("argv[%d]=%s\r\n",i,argv[i]);
    }
#endif

    sprintf(TURN_ON_RESET, "echo 1 > /sys/class/gpio/gpio%s/value", argv[3]);
    sprintf(TURN_OFF_RESET, "echo 0 > /sys/class/gpio/gpio%s/value", argv[3]);
    sprintf(TURN_ON_DFU_ENABLE, "echo 1 > /sys/class/gpio/gpio%s/value", argv[4]);
    sprintf(TURN_OFF_DFU_ENABLE, "echo 0 > /sys/class/gpio/gpio%s/value", argv[4]);

    FILE * fp = NULL;
    fp = fopen (argv[1], "r");
    if (fp == NULL) {
        printf("not find file\r\n");
        return -1;
    }

    uint8_t buffer[1024*1024];
    size_t read_size;
    read_size = fread(buffer, 1, 1024*1024, fp);   /* 每次读取1个字节，最多读取10个，这样返回得read_size才是读到的字节数 */
    printf("file size:%ld\n", read_size);
    
    hal_init(argv[2], 115200, 0);

    // printf("start dfu\r\n");

    while (1) {
        ret = dfu_process(buffer,read_size);
        if (ret != 0 ) {
            break;
        }
    }

    printf("dfu success!\r\n");

    return 0;
}








