/*****************************************************************************
 * @file 
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

#ifndef HOST_GECKO_H
#define HOST_GECKO_H

#include <string.h>
#include "bg_types.h"
#include "bg_errorcodes.h"



/* Compatibility */
#ifndef PACKSTRUCT
/*Default packed configuration*/
#ifdef __GNUC__
#ifdef _WIN32
#define PACKSTRUCT( decl ) decl __attribute__((__packed__,gcc_struct))
#else
#define PACKSTRUCT( decl ) decl __attribute__((__packed__))
#endif
#define ALIGNED __attribute__((aligned(0x4)))
#elif __IAR_SYSTEMS_ICC__

#define PACKSTRUCT( decl ) __packed decl

#define ALIGNED
#elif _MSC_VER  /*msvc*/

#define PACKSTRUCT( decl ) __pragma( pack(push, 1) ) decl __pragma( pack(pop) )
#define ALIGNED
#else 
#define PACKSTRUCT(a) a PACKED 
#endif
#endif


#define BGLIB_DEPRECATED_API __attribute__((deprecated))
#define BGLIB_MSG_ID(HDR) ((HDR)&0xffff00f8)
#define BGLIB_MSG_HEADER_LEN (4)
#define BGLIB_MSG_LEN(HDR) ((((HDR)&0x7)<<8)|(((HDR)&0xff00)>>8))

/**
 * The maximum BGAPI command payload size.
 */
#define BGLIB_MSG_MAX_PAYLOAD 256

/*Pass in the endian*/
extern unsigned char ENDIAN;

/*Reverse the byte order*/
void reverse_endian(uint8_t* header,uint8_t length);

#define BGLIB_BIT_ENCRYPTED (1 << 6) // Bit indicating whether the packet is encrypted
#define BGLIB_MSG_ENCRYPTED(HDR) ((HDR) & BGLIB_BIT_ENCRYPTED)

enum gecko_parameter_types
{
    gecko_msg_parameter_uint8=2,
    gecko_msg_parameter_int8=3,
    gecko_msg_parameter_uint16=4,
    gecko_msg_parameter_int16=5,
    gecko_msg_parameter_uint32=6,
    gecko_msg_parameter_int32=7,
    gecko_msg_parameter_uint8array=8,
    gecko_msg_parameter_string=9,
    gecko_msg_parameter_hwaddr=10,
    gecko_msg_parameter_uint16array=11
};

enum gecko_msg_types
{
    gecko_msg_type_cmd=0x00,
    gecko_msg_type_rsp=0x00,
    gecko_msg_type_evt=0x80
};
enum gecko_dev_types
{
    gecko_dev_type_gecko   =0x20
};

#define gecko_cmd_dfu_reset_id                                        (((uint32)gecko_dev_type_gecko)|gecko_msg_type_cmd|0x00000000)
#define gecko_cmd_dfu_flash_set_address_id                            (((uint32)gecko_dev_type_gecko)|gecko_msg_type_cmd|0x01000000)
#define gecko_cmd_dfu_flash_upload_id                                 (((uint32)gecko_dev_type_gecko)|gecko_msg_type_cmd|0x02000000)
#define gecko_cmd_dfu_flash_upload_finish_id                          (((uint32)gecko_dev_type_gecko)|gecko_msg_type_cmd|0x03000000)

#define gecko_rsp_dfu_flash_set_address_id                            (((uint32)gecko_dev_type_gecko)|gecko_msg_type_rsp|0x01000000)
#define gecko_rsp_dfu_flash_upload_id                                 (((uint32)gecko_dev_type_gecko)|gecko_msg_type_rsp|0x02000000)
#define gecko_rsp_dfu_flash_upload_finish_id                          (((uint32)gecko_dev_type_gecko)|gecko_msg_type_rsp|0x03000000)

#define gecko_evt_dfu_boot_id                                         (((uint32)gecko_dev_type_gecko)|gecko_msg_type_evt|0x00000000)
#define gecko_evt_dfu_boot_failure_id                                 (((uint32)gecko_dev_type_gecko)|gecko_msg_type_evt|0x01000000)
#define gecko_evt_system_boot_id                                      (((uint32)gecko_dev_type_gecko)|gecko_msg_type_evt|0x00010000)

PACKSTRUCT( struct gecko_msg_dfu_reset_cmd_t
{
    uint8               dfu;
});
PACKSTRUCT( struct gecko_msg_dfu_flash_set_address_cmd_t
{
    uint32              address;
});
PACKSTRUCT( struct gecko_msg_dfu_flash_set_address_rsp_t
{
    uint16              result;
});
PACKSTRUCT( struct gecko_msg_dfu_flash_upload_cmd_t
{
    uint8array          data;
});
PACKSTRUCT( struct gecko_msg_dfu_flash_upload_rsp_t
{
    uint16              result;
});
PACKSTRUCT( struct gecko_msg_dfu_flash_upload_finish_rsp_t
{
    uint16              result;
});
PACKSTRUCT( struct gecko_msg_dfu_boot_evt_t
{
    uint32              version;
});
PACKSTRUCT( struct gecko_msg_dfu_boot_failure_evt_t
{
    uint16              reason;
});
PACKSTRUCT( struct gecko_msg_system_boot_evt_t
{
    uint16              major;
    uint16              minor;
    uint16              patch;
    uint16              build;
    uint32              bootloader;
    uint16              hw;
    uint32              hash;
});

PACKSTRUCT( struct gecko_cmd_packet
{
    uint32   header;

union{
    uint8 handle;
    struct gecko_msg_dfu_reset_cmd_t                             cmd_dfu_reset;
    struct gecko_msg_dfu_flash_set_address_cmd_t                 cmd_dfu_flash_set_address;
    struct gecko_msg_dfu_flash_set_address_rsp_t                 rsp_dfu_flash_set_address;
    struct gecko_msg_dfu_flash_upload_cmd_t                      cmd_dfu_flash_upload;
    struct gecko_msg_dfu_flash_upload_rsp_t                      rsp_dfu_flash_upload;
    struct gecko_msg_dfu_flash_upload_finish_rsp_t               rsp_dfu_flash_upload_finish;
    struct gecko_msg_dfu_boot_evt_t                              evt_dfu_boot;
    struct gecko_msg_dfu_boot_failure_evt_t                      evt_dfu_boot_failure;
    struct gecko_msg_system_boot_evt_t                           evt_system_boot;

    uint8 payload[BGLIB_MSG_MAX_PAYLOAD];
}data;

});


void gecko_handle_command(uint32_t,void*);
void gecko_handle_command_noresponse(uint32_t,void*);

extern struct gecko_cmd_packet*  gecko_cmd_msg;
extern struct gecko_cmd_packet*  gecko_rsp_msg;

/** 
*
* gecko_cmd_dfu_reset
*
* This command can be used to reset the system. This command does not have a response, but it triggers one of the boot events (normal reset or boot to DFU mode) after re-boot.  
*
* @param dfu   Boot mode:                     
*  - 0: Normal reset
*  - 1: Boot to UART DFU mode
*  - 2: Boot to OTA DFU mode
* 
*
* Events generated
*
* gecko_evt_system_boot - Sent after the device has booted into normal mode
* gecko_evt_dfu_boot - Sent after the device has booted into UART DFU mode    
*
**/

static inline void* gecko_cmd_dfu_reset(uint8 dfu)
{
    
    gecko_cmd_msg->data.cmd_dfu_reset.dfu=dfu;
    gecko_cmd_msg->header=((gecko_cmd_dfu_reset_id+((1)<<8)));
    
    gecko_handle_command_noresponse(gecko_cmd_msg->header,&gecko_cmd_msg->data.payload);
    return 0;
}

/** 
*
* gecko_cmd_dfu_flash_set_address
*
* After re-booting the local device into DFU mode, this command can be used to define the starting address on the flash to where the new firmware will be written in. 
*
* @param address   The offset in the flash where the new firmware is uploaded to. Always use the value 0x00000000.    
*
**/

static inline struct gecko_msg_dfu_flash_set_address_rsp_t* gecko_cmd_dfu_flash_set_address(uint32 address)
{
    if(ENDIAN) reverse_endian((uint8*)&address,4);
    
    gecko_cmd_msg->data.cmd_dfu_flash_set_address.address=address;
    gecko_cmd_msg->header=((gecko_cmd_dfu_flash_set_address_id+((4)<<8)));
    
    gecko_handle_command(gecko_cmd_msg->header,&gecko_cmd_msg->data.payload);
    
    return &gecko_rsp_msg->data.rsp_dfu_flash_set_address;
}

/** 
*
* gecko_cmd_dfu_flash_upload
*
* This command can be used to upload the whole firmware image file into the Bluetooth device. The passed data length must be a multiple of 4 bytes. As the BGAPI command payload size is limited, multiple commands need to be issued one after the other until the whole .bin firmware image file is uploaded to the device. The next address of the flash sector in memory to write to is automatically updated by the bootloader after each individual command. 
* 
* @param data   An array of data which will be written onto the flash.    
*
**/

static inline struct gecko_msg_dfu_flash_upload_rsp_t* gecko_cmd_dfu_flash_upload(uint8 data_len, const uint8* data_data)
{   
    gecko_cmd_msg->data.cmd_dfu_flash_upload.data.len=data_len;
    memcpy(gecko_cmd_msg->data.cmd_dfu_flash_upload.data.data,data_data,data_len);
    gecko_cmd_msg->header=((gecko_cmd_dfu_flash_upload_id+((1+data_len)<<8)));
    
    gecko_handle_command(gecko_cmd_msg->header,&gecko_cmd_msg->data.payload);
    
    return &gecko_rsp_msg->data.rsp_dfu_flash_upload;
}

/** 
*
* gecko_cmd_dfu_flash_upload_finish
*
* This command can be used to tell to the device that the DFU file has been fully uploaded. To return the device back to normal mode the command "DFU Reset " must be issued next. 
*    
*
**/

static inline struct gecko_msg_dfu_flash_upload_finish_rsp_t* gecko_cmd_dfu_flash_upload_finish()
{
    
    gecko_cmd_msg->header=((gecko_cmd_dfu_flash_upload_finish_id+((0)<<8)));
    
    gecko_handle_command(gecko_cmd_msg->header,&gecko_cmd_msg->data.payload);
    
    return &gecko_rsp_msg->data.rsp_dfu_flash_upload_finish;//.rsp_dfu_flash_upload_finish;
}


#endif
