/*****************************************************************************
 * @file  xmodem-main.c
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
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "gl_hal.h"
#include "gl_uart.h"
#include "silabs_msg.h"
#include "bg_types.h"
#include "host_gecko.h"
#include "xmodem.h"
#include "xmodem_main.h"

typedef enum {
    THREAD_VERSION_CHECK,
    BLE_VERSION_CHECK,
    START_DFU,
    GET_BOOT_INFO,
    START_UPLOAD,
    START_TRANS,
    DEBUG_TEST,
} dfu_step_e;

static char TTY_NUM[32] = { 0 };
static char TURN_ON_RESET[64];
static char TURN_OFF_RESET[64];
static char TURN_ON_DFU_ENABLE[64];
static char TURN_OFF_DFU_ENABLE[64];
int g_verbose = 0;
static int thread_ver_check = 0;
static int ble_ver_check = 0;
static char g_filepath[256] = { 0 };

char new_ot_efr32_commit_hash[16] = { 0 };

uint8_t spinel_get_module_version_cmd[7] = { 0x7e, 0x82, 0x02, 0x02, 0x3a, 0x6f, 0x7e };
uint8_t spinel_get_module_version_rsp[128] = { 0 };

static dfu_step_e step;

bool thread_check_is_need_to_upgrade(char* rcp_version)
{
    char* end_str1;
    char* str1 = strtok_r(rcp_version, ";", &end_str1);
    char* rcp_ver = strrchr(str1, '_');

    if (g_verbose) {
        printf("Current rcp version: %s\n", str1);
        printf("Upgrade file: %s\n", g_filepath);
    }

    if (rcp_ver == NULL)
        return true;

    char* end_str2;
    char* str2 = strrchr(g_filepath, '_');
    char* fw_ver = strtok_r(str2, ".", &end_str2);
    
    if (!strcmp(rcp_ver, fw_ver))
        return false;
    return true;
}


bool ble_check_is_need_to_upgrade(char* rcp_version)
{
    char* end_str1;
    char* str1 = strtok_r(g_filepath, "v", &end_str1);
    char* end_str2;
    char* str2 = strtok_r(end_str1, ".", &end_str2);

    if (g_verbose) {
        printf("Current version: %s\n", rcp_version);
        printf("Upgrade version: %s\n", str2);
    }
    
    if (!strcmp(rcp_version, str2))
        return false;
    return true;
}

int dfu_process(uint8_t* out_file, uint32_t file_size)
{
    int ret;

    switch (step) {
        case THREAD_VERSION_CHECK: {
            int32_t rsp_len = 0;
            ret = uartTx(7, spinel_get_module_version_cmd);
            if (7 != ret) {
                printf("uartTx error! %d\n", ret);
            }
            rsp_len = uartRxTimeout(128, spinel_get_module_version_rsp);
            if (rsp_len > 0) {
                int offset = 0;
                uint8_t rsp_head[4] = { 0x7e, 0x82, 0x06, 0x02 };
                for (; offset < rsp_len; offset++) {
                    if (spinel_get_module_version_rsp[offset] == 0x7e) {
                        if (0 == memcmp(&spinel_get_module_version_rsp[offset], rsp_head, 4)) {
                            break;
                        }
                    }
                }
                if (offset > 64) {
                    printf("Get old firmware version failed! Start DFU...\n");
                    goto start_dfu;
                    break;
                }
                if (!thread_check_is_need_to_upgrade(spinel_get_module_version_rsp)) {
                    printf("No need to upgrade. DFU end!\n");
                    return 1;
                }
            } else {
                printf("Get version response failed!\n");
            }

        start_dfu:
            hal_destroy();
            usleep(100000);
            hal_init(TTY_NUM, 115200, 0);
            step = START_DFU;
            break;
        }
        case BLE_VERSION_CHECK: {
            uint32_t header = 0;
            uint16_t major;
            uint16_t minor;
            uint16_t patch;
            char rcp_version[128];

            int dataToRead = 4;
            uint8_t* header_p = (uint8_t*)&header;

            system(TURN_OFF_RESET);
            usleep(100*1000);
            system(TURN_ON_RESET);

            struct timeval time;
            uint32_t backpu_time;
            gettimeofday(&time,NULL);
            backpu_time = time.tv_sec;

            while(1)
            {
                while(1)
                {
                    ret = uartRxNonBlocking(1, header_p);
                    if(ret == 1)
                    {
                        if(*header_p == 0xa0)
                        {
                            break;
                        }
                    }
                }
                dataToRead--;
                header_p++;

                while(dataToRead)
                {
                    ret = uartRxNonBlocking(dataToRead, header_p);
                    if(ret != -1)
                    {
                        dataToRead -= ret;
                        header_p += ret;
                    }
                }

                if(ENDIAN)
                {
                    reverse_endian((uint8_t*)&header, 4);
                } 

                if(header == 0x000112a0)
                {
                    uartRxNonBlocking(2, (uint8_t*)&major);
                    uartRxNonBlocking(2, (uint8_t*)&minor);
                    uartRxNonBlocking(2, (uint8_t*)&patch);

                    if(ENDIAN)
                    {
                        reverse_endian((uint8_t*)&major, 2);
                        reverse_endian((uint8_t*)&minor, 2);
                        reverse_endian((uint8_t*)&patch, 2);
                    }
                    sprintf(rcp_version, "%d_%d_%d", major, minor, patch);
                    if(!ble_check_is_need_to_upgrade(rcp_version))
                    {
                        printf("No need to upgrade. DFU end!\n");
                        return 1;
                    }else 
                    {
                        step = START_DFU;
                        break;
                    }
                }

                gettimeofday(&time,NULL);
                printf("time:%d\n", time.tv_sec - backpu_time);
                if(30 <= time.tv_sec - backpu_time) //timeout set 30s
                {
                    printf("Get old firmware version timeout! Start DFU...\n");
                    step = START_DFU;
                    break;
                }
            }
            break;
        }

        case START_DFU: {
            /* 拉低GPIO 复位target */
            system(TURN_OFF_RESET);
            usleep(100 * 1000);
            system(TURN_OFF_DFU_ENABLE);
            usleep(100 * 1000);
            system(TURN_ON_RESET);
            usleep(100 * 1000);

            step = GET_BOOT_INFO;
            break;
        }
        case GET_BOOT_INFO: {
            uint8_t boot_info[100] = { 0 };
            uint32_t size = 0;

            size = uartRxTimeout(100, boot_info);
            // size = size;
            if (strstr((const char*)boot_info, "1. upload gbl")) {
                system(TURN_ON_DFU_ENABLE);
                step = START_UPLOAD;
            } else {
                step = START_DFU;
            }
            // printf("boot_info=%s\r\n",boot_info);
            uartCacheClean(); /* 清除其他未读取出来的数据 */
            break;
        }
        case START_UPLOAD: {
            uint8_t upload_info[20] = { 0 };
            uint32_t size = 0;
            uint8_t write_msg[5] = { 0 };

            write_msg[0] = '1'; /* 启动 upload */
            uartTx(1, write_msg);

            usleep(300 * 1000);

            size = uartRxTimeout(19, upload_info);
            // printf("upload_info=%s\r\n",upload_info);
            if (strstr((const char*)upload_info, "begin upload")) {
                step = START_TRANS;
            } else {
                step = START_DFU;
                usleep(100*1000);
            }
            break;
        }
        case START_TRANS: {
            printf("start upload...\n");
            ret = xmodemTransmit(out_file, file_size);
            if (ret > 0) {
                printf("upload ok. total size:%d\r\n", ret);
                printf("reset chip now...\r\n");
                system(TURN_OFF_RESET);
                usleep(100 * 1000);
                system(TURN_ON_RESET);
                return 1; /* upload 成功 */
            } else {
                printf("upload failed. total size:%d\r\n", ret);
                step = START_DFU;
            }
            break;
        }
        default:
            break;
    }

    return 0;
}

int xmodem_main(int argc, char* argv[])
{
    int ret;
    int try = 10;

    if (argc < 6) {
        printf("gl-silabs-dfu xmodem [Upgrade file path] [Uart] [Reset IO] [DFU enable IO] [-v] [version check]\n");
        return -1;
    }
    if ((argv[7] != NULL && !strncmp(argv[7], "-v", 2)) || (argv[6] != NULL && !strncmp(argv[6], "-v", 2))) {
        g_verbose = 1;
    }

    if ((argv[7] != NULL && !strncmp(argv[7], "-cb", 3)) || (argv[6] != NULL && !strncmp(argv[6], "-cb", 3))){
        ble_ver_check = 1;
        ++try;
    }
    else if ((argv[7] != NULL && !strncmp(argv[7], "-c", 2)) || (argv[6] != NULL && !strncmp(argv[6], "-c", 2))) {
        thread_ver_check = 1; // only for openthread module firmware build from GL-inet
        ++try;
    }
    

#if 0
    printf("argc=%d\n",argc);
    for (int i = 0;i < argc;i++) {
        printf("argv[%d]=%s\r\n",i,argv[i]);
    }
#endif

    sprintf(TURN_ON_RESET, "echo 1 > /sys/class/gpio/gpio%s/value", argv[4]);
    sprintf(TURN_OFF_RESET, "echo 0 > /sys/class/gpio/gpio%s/value", argv[4]);
    sprintf(TURN_ON_DFU_ENABLE, "echo 1 > /sys/class/gpio/gpio%s/value", argv[5]);
    sprintf(TURN_OFF_DFU_ENABLE, "echo 0 > /sys/class/gpio/gpio%s/value", argv[5]);

    FILE* fp = NULL;
    fp = fopen(argv[2], "r");
    if (fp == NULL) {
        printf("not find file\r\n");
        return -1;
    }
    strcpy(g_filepath, argv[2]);

    uint8_t buffer[1024 * 1024];
    size_t read_size;
    read_size = fread(buffer, 1, 1024 * 1024, fp); /* 每次读取1个字节，最多读取10个，这样返回得read_size才是读到的字节数 */
    printf("file size:%ld\n", read_size);
    fclose(fp);

    strcpy(TTY_NUM, argv[3]);
    if (thread_ver_check ) {
        ret = hal_init(TTY_NUM, 1000000, 1);
        step = THREAD_VERSION_CHECK;
    } 
    else if (ble_ver_check) {
        ret = hal_init(TTY_NUM, 115200, 0);
        step = BLE_VERSION_CHECK; 
    }
    else {
        ret = hal_init(TTY_NUM, 115200, 0);
        step = START_DFU;
    }
    if (ret < 0) {
        fprintf(stderr, "hal_init failed\n");
        fprintf(stderr, "dfu failed\n");
        return -1;
    }

    while (try) {
        ret = dfu_process(buffer, read_size);
        if (ret != 0) {
            printf("dfu success!\r\n");
            hal_destroy();
            break;
        }
        else if (try == 1) {
            printf("dfu failed!\r\n");
            hal_destroy();
        }
        if(step == START_DFU) {
            --try;
        }
    }
            
    return 0;
}
