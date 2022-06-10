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
#include <time.h>
#include <unistd.h>

#include "gl_hal.h"
#include "gl_uart.h"
#include "xmodem.h"
#include "xmodem_main.h"

typedef enum {
    VERSION_CHECK,
    START_DFU,
    GET_BOOT_INFO,
    START_UPLOAD,
    START_TRANS,
    DEBUG_TEST,
} dfu_step_e;

char TTY_NUM[32] = { 0 };
char TURN_ON_RESET[64];
char TURN_OFF_RESET[64];
char TURN_ON_DFU_ENABLE[64];
char TURN_OFF_DFU_ENABLE[64];
int g_verbose = 0;
int ver_check = 0;
char g_filepath[256] = { 0 };

char new_ot_efr32_commit_hash[16] = { 0 };

uint8_t spinel_get_module_version_cmd[7] = { 0x7e, 0x82, 0x02, 0x02, 0x3a, 0x6f, 0x7e };
uint8_t spinel_get_module_version_rsp[128] = { 0 };

// static int get_new_firmware_commit_hash(const char* file_path, char* commit_hash);
// static int check_firmware_version(char* old_firmware_version, char* new_firmware_hash);

static dfu_step_e step;

uint64_t get_rcp_timestamp(char* rcp_version)
{
    char* end_str;
    char* str = strtok_r(rcp_version, ";", &end_str);
    str = strtok_r(NULL, ";", &end_str);
    str = strtok_r(NULL, ";", &end_str);

    struct tm timeinfo;
    strptime(str, " %b %d %Y %H:%M:%S", &timeinfo);

    timeinfo.tm_sec = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_hour = 0;

    time_t timestamp;
    timestamp = mktime(&timeinfo);
    if (g_verbose)
        printf("Current rcp timestamp: %ld\n", timestamp);

    return timestamp;
}

uint64_t get_file_timestamp(char* filepath)
{
    struct stat st;
    if (stat(filepath, &st) != 0) {
        return 0;
    }

    char* ret = strrchr(filepath, '-');
    char* end_str;
    char* str = strtok_r(ret + 1, ".", &end_str);

    time_t timestamp;
    timestamp = atol(str);
    if (g_verbose)
        printf("New rcp timestamp: %ld\n", timestamp);

    return timestamp;
}

int dfu_process(uint8_t* out_file, uint32_t file_size)
{
    int ret;

    switch (step) {
        case VERSION_CHECK: {
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
                if (get_rcp_timestamp(spinel_get_module_version_rsp) >= get_file_timestamp(g_filepath)) {
                    printf("No need to upgrade. DFU end!\n");
                    return 1;
                }
                // printf("spinel_get_module_version_rsp=%s\n", spinel_get_module_version_rsp);
                // if (0 == check_firmware_version(&spinel_get_module_version_rsp[offset + 4], new_ot_efr32_commit_hash)) {
                //     printf("New firmware version is same as ole firmware. DFU end!\n");
                //     return 1;
                // }
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
                sleep(1000);
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

    if (argc < 6) {
        printf("gl-silabs-dfu xmodem [Upgrade file path] [Uart] [Reset IO] [DFU enable IO] [-v] [version check]\n");
        return -1;
    }
    if ((argv[7] != NULL && !strncmp(argv[7], "-v", 2)) || (argv[6] != NULL && !strncmp(argv[6], "-v", 2))) {
        g_verbose = 1;
    }

    if ((argv[7] != NULL && !strncmp(argv[7], "-c", 2)) || (argv[6] != NULL && !strncmp(argv[6], "-c", 2))) {
        ver_check = 1; // only for openthread module firmware build from GL-inet
        // if (0 != get_new_firmware_commit_hash(argv[2], new_ot_efr32_commit_hash)) {
        //     printf("Get new firmware openthread commit hash failed!\n");
        //     return -1;
        // }
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
    if (ver_check) {
        ret = hal_init(TTY_NUM, 1000000, 1);
        step = VERSION_CHECK;
    } else {
        ret = hal_init(TTY_NUM, 115200, 0);
        step = START_DFU;
    }
    if (ret < 0) {
        fprintf(stderr, "hal_init failed\n");
        fprintf(stderr, "dfu failed\n");
        return -1;
    }
    while (1) {
        ret = dfu_process(buffer, read_size);
        if (ret != 0) {
            break;
        }
    }

    printf("dfu success!\r\n");

    hal_destroy();

    return 0;
}

// static int get_new_firmware_commit_hash(const char* file_path, char* commit_hash)
// {
//     char tmp_path[128] = { 0 };
//     strcpy(tmp_path, file_path);

//     const char str[2] = "/";
//     char file_name[64] = { 0 };
//     char* tmp_str = NULL;
//     tmp_str = strtok(tmp_path, str);
//     while (NULL != tmp_str) {
//         strcpy(file_name, tmp_str);
//         tmp_str = strtok(NULL, str);
//     }
//     // printf("Get file name: %s\n", file_name);

//     const char str1[2] = "-";
//     char openthread_commit_str[32] = { 0 };
//     tmp_str = strtok(file_name, str1);
//     while (NULL != tmp_str) {
//         strcpy(openthread_commit_str, tmp_str);
//         tmp_str = strtok(NULL, str1);
//     }
//     // printf("Get openthread & ot-efr32 commit hash: %s\n", openthread_commit_str);

//     const char str2[2] = "_";
//     char ot_efr32_commit_str[16] = { 0 };
//     tmp_str = strtok(openthread_commit_str, str2);
//     if (tmp_str != NULL) {
//         strcpy(ot_efr32_commit_str, tmp_str);
//     }
//     printf("Get ot-efr32 commit hash: %s\n", ot_efr32_commit_str);

//     int hash_len = strlen(ot_efr32_commit_str);
//     if (hash_len == 7) {
//         strncpy(commit_hash, ot_efr32_commit_str, hash_len);
//     } else {
//         return -1;
//     }

//     return 0;
// }

// static int check_firmware_version(char* old_firmware_version, char* new_firmware_hash)
// {
//     char tmp_version[128] = { 0 };
//     strcpy(tmp_version, old_firmware_version);

//     const char str[2] = ";";
//     char ot_version[64] = { 0 };
//     char* tmp_str = NULL;
//     tmp_str = strtok(tmp_version, str);
//     strcpy(ot_version, tmp_str);
//     // printf("Get old firmware ot version: %s\n", ot_version);

//     const char str1[2] = "-";
//     char ot_efr32_commit_hash[16] = { 0 };
//     tmp_str = NULL;
//     tmp_str = strtok(ot_version, str1);
//     while (NULL != tmp_str) {
//         strcpy(ot_efr32_commit_hash, tmp_str);
//         tmp_str = strtok(NULL, str1);
//     }
//     printf("Get old firmware ot commit hash: %s\n", ot_efr32_commit_hash);
//     if (7 != strlen(ot_efr32_commit_hash)) {
//         printf("Old firmware ot commit hash len failed!\n");
//         return -1;
//     }

//     return strncmp(ot_efr32_commit_hash, new_firmware_hash, 7);
// }
