/*****************************************************************************
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
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "silabs_msg.h"
#include "bg_types.h"
#include "host_gecko.h"
#include "gl_uart.h"
#include "gl_hal.h"


BGLIB_DEFINE();
bool appBooted = false; // App booted flag

extern bool module_work;
extern bool sync_dfu_boot;

struct gecko_cmd_packet* evt = NULL;

struct gecko_cmd_packet* gecko_get_event(void);
struct gecko_cmd_packet* gecko_wait_event(void);
struct gecko_cmd_packet* gecko_wait_message(void); //wait for event from system

void silabs_event_handler(struct gecko_cmd_packet *p);
static void reverse_rev_payload(struct gecko_cmd_packet* pck);


void* silabs_driver(void* arg)
{
    while (1) {

        // Check for stack event.
		evt = gecko_get_event();

        // Run application and event handler.
        silabs_event_handler(evt);

        // set cancellation point 
        pthread_testcancel();
    }

    return NULL;
}

/*
* 
*/
struct gecko_cmd_packet* gecko_get_event(void)
{
    struct gecko_cmd_packet* p;

    while (1) {
        if (gecko_queue_w != gecko_queue_r) {
            p = &gecko_queue[gecko_queue_r];
            gecko_queue_r = (gecko_queue_r + 1) % BGLIB_QUEUE_LEN;
            return p;
        }

        p = gecko_wait_message();
        if (p) {
            return p;
        }

    }
}

struct gecko_cmd_packet* gecko_wait_message(void) //wait for event from system
{
    uint32_t msg_length;
    uint32_t header = 0;
    uint8_t* payload;
    struct gecko_cmd_packet *pck, *retVal = NULL;
    int ret;

    int dataToRead = BGLIB_MSG_HEADER_LEN;
    uint8_t* header_p = (uint8_t*)&header;

    // if(!appBooted)
    // {
    //     while(1)
    //     {
    //         ret = uartRxNonBlocking(1, header_p);
    //         if(ret == 1)
    //         {
    //             if(*header_p == 0xa0)
    //             {
    //                 break;
    //             }
    //         }
    //     }
    //     dataToRead--;
    //     header_p++;
    // }

    while(1)
    {
        ret = uartRxNonBlocking(1, header_p);
        if(ret == 1)
        {
            if((*header_p == 0xa0) || (*header_p == 0x20))
            {
                --dataToRead;
                ++header_p;
                break;
            }
        }
    }

    while(dataToRead)
    {
        ret = uartRxNonBlocking(dataToRead, header_p);
        if(ret != -1)
        {
            dataToRead -= ret;
            header_p += ret;
        }else{
            return 0;
        }
    }

    if(ENDIAN){
        reverse_endian((uint8_t*)&header,BGLIB_MSG_HEADER_LEN);
    } 

	if (ret < 0 || (header & 0x78) != gecko_dev_type_gecko){
        return 0;
    }

    msg_length = BGLIB_MSG_LEN(header);

    if (msg_length > BGLIB_MSG_MAX_PAYLOAD) {
        return 0;
    }

    if ((header & 0xf8) == (gecko_dev_type_gecko | gecko_msg_type_evt)) {
        //received event
        if ((gecko_queue_w + 1) % BGLIB_QUEUE_LEN == gecko_queue_r) {
            //drop packet
            if (msg_length) {
                uint8 tmp_payload[BGLIB_MSG_MAX_PAYLOAD];
                uartRx(msg_length, tmp_payload);
            }
            return 0; //NO ROOM IN QUEUE
        }
        pck = &gecko_queue[gecko_queue_w];
        gecko_queue_w = (gecko_queue_w + 1) % BGLIB_QUEUE_LEN;
    } else if ((header & 0xf8) == gecko_dev_type_gecko ) { 
        //response
        retVal = pck = gecko_rsp_msg;
    } else {
        //fail
        return 0;
    }
    pck->header = header;
    payload = (uint8_t*)&pck->data.payload;

   // Read the payload data if required and store it after the header.
    if (msg_length > 0) {

        ret = uartRx(msg_length, payload);
        if (ret < 0) {
			// log_err("recv fail\n");
            return 0;
        }
    }

	if(ENDIAN)  
	{
		reverse_rev_payload(pck);
	}

    // Using retVal avoid double handling of event msg types in outer function
    return retVal;
}



int rx_peek_timeout(int ms)
{
    int timeout = ms*10;
    while (timeout) {
        timeout--;
        if (BGLIB_MSG_ID(gecko_cmd_msg->header) == BGLIB_MSG_ID(gecko_rsp_msg->header))
        {
            return 0;
        }
        usleep(100);
    }

    return -1;
}


void gecko_handle_command(uint32_t hdr, void* data)
{
	uint32_t send_msg_length = BGLIB_MSG_HEADER_LEN + BGLIB_MSG_LEN(gecko_cmd_msg->header);
	if(ENDIAN) 
	{
		reverse_endian((uint8_t*)&gecko_cmd_msg->header,BGLIB_MSG_HEADER_LEN);
	}
	gecko_rsp_msg->header = 0;

	uartTx(send_msg_length, (uint8_t*)gecko_cmd_msg); // send cmd msg

	rx_peek_timeout(300); // wait for response
}

void gecko_handle_command_noresponse(uint32_t hdr,void* data)
{
	uint32_t send_msg_length = BGLIB_MSG_HEADER_LEN + BGLIB_MSG_LEN(gecko_cmd_msg->header);

	if(ENDIAN) 
	{
		reverse_endian((uint8_t*)&gecko_cmd_msg->header,BGLIB_MSG_HEADER_LEN);
	}

	uartTx(send_msg_length, (uint8_t*)gecko_cmd_msg); // send cmd msg
}

/*
 *	module events report 
 */
void silabs_event_handler(struct gecko_cmd_packet *p)
{
    // Do not handle any events until system is booted up properly.
    if ((BGLIB_MSG_ID(evt->header) != gecko_evt_system_boot_id) && !appBooted) 
    {
        // log_debug("Wait for system boot ... \n");
        // usleep(50000);
        return;
    }

    switch(BGLIB_MSG_ID(p->header)){
        case gecko_evt_system_boot_id:
		{
            appBooted = true;
			module_work = true;
			printf("* System boot event!\n* Module firmware version: %d.%d.%d\n* Build number:            %d\n", \
                                                    p->data.evt_system_boot.major,      \
													p->data.evt_system_boot.minor,      \
													p->data.evt_system_boot.patch,      \
													p->data.evt_system_boot.build);                                    
            break;
		}
        case gecko_evt_dfu_boot_id:
        {
			sync_dfu_boot = true;
			// printf("DFU boot event! Bootloader version: %d\n", p->data.evt_dfu_boot.version);
            break;
        }
        case gecko_evt_dfu_boot_failure_id:
		{
			printf("DFU failure event! Reason number: %d\n", p->data.evt_dfu_boot_failure.reason);
			break;
		}

        default:
            break;
    }

    return ;
}




static void reverse_rev_payload(struct gecko_cmd_packet* pck)
{
  uint32 p = BGLIB_MSG_ID(pck->header);

    switch (p)
    {
        case gecko_rsp_dfu_flash_set_address_id:
            reverse_endian((uint8*)&(pck->data.rsp_dfu_flash_set_address.result),2);
            break;
        case gecko_rsp_dfu_flash_upload_id:
            reverse_endian((uint8*)&(pck->data.rsp_dfu_flash_upload.result),2);
            break;
        case gecko_rsp_dfu_flash_upload_finish_id:
            reverse_endian((uint8*)&(pck->data.rsp_dfu_flash_upload_finish.result),2);
            break;
        case gecko_evt_dfu_boot_id:
            reverse_endian((uint8*)&(pck->data.evt_dfu_boot.version),4);
            break;
        case gecko_evt_dfu_boot_failure_id:
            reverse_endian((uint8*)&(pck->data.evt_dfu_boot_failure.reason),2);
            break;
        case gecko_evt_system_boot_id:
            reverse_endian((uint8*)&(pck->data.evt_system_boot.major),2);
            reverse_endian((uint8*)&(pck->data.evt_system_boot.minor),2);
            reverse_endian((uint8*)&(pck->data.evt_system_boot.patch),2);
            reverse_endian((uint8*)&(pck->data.evt_system_boot.build),2);
            reverse_endian((uint8*)&(pck->data.evt_system_boot.bootloader),4);
            reverse_endian((uint8*)&(pck->data.evt_system_boot.hw),2);
            reverse_endian((uint8*)&(pck->data.evt_system_boot.hash),4);
            break;
    }
}

void reverse_endian(uint8_t* header, uint8_t length) {
    uint8_t* tmp = (uint8_t*)malloc(length);
    memcpy(tmp, header, length);
    int i = length - 1;
    int j = 0;
    for (; i >= 0; i--, j++) {
        *(header + j) = *(tmp + i);
    }
    free(tmp);
    return;
}
