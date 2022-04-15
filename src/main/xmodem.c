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
#include <string.h>
#include <stdio.h>
#include "gl_uart.h"
#include "xmodem.h"


#define SOH  0x01
#define STX  0x02
#define EOT  0x04
#define ACK  0x06
#define NAK  0x15
#define CAN  0x18
#define CTRLZ 0x1A
 
#define DLY_1S 1000
#define MAXRETRANS 25
static int last_error = 0;
extern int g_verbose;
static unsigned short crc16_ccitt(unsigned char *buf, int len);

void port_outbyte(unsigned char trychar)
{
    unsigned char buf[2];
    buf[0] = trychar;
    uartTx(1,buf);
}
 

unsigned char port_inbyte(unsigned int time_out)
{
    unsigned char ch;
    last_error = 0;
 
    if(uartRx(1,&ch) == 1)
        return ch;
 
    last_error = 1;
    return ch;
}


static void flushinput(void)
{
 //while (port_inbyte(((DLY_1S)*3)>>1) >= 0)
    ;
    uartCacheClean();
}


void process_bar(uint32_t pro)
{
    static char buf[102] = {0};

    //buf[0] = '=';
    //buf[1] = '>';

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


int xmodemTransmit(unsigned char *src, int srcsz)
{
    unsigned char xbuff[1030];
    int bufsz, crc = -1;
    unsigned char packetno = 1;
    int i, c, len = 0;
    int retry;
 
    for(;;) 
    {
        for( retry = 0; retry < 16; ++retry)
        {
            c = port_inbyte((DLY_1S)<<1);
            if (last_error == 0)
            {
                switch (c)
                {
                case 'C':
                    crc = 1;
                    goto start_trans;
                case NAK:
                    crc = 0;
                    goto start_trans;
                case CAN:
                    c = port_inbyte(DLY_1S);
                    if (c == CAN)
                    {
                        port_outbyte(ACK);
                        flushinput();
                        return -1;
                    }
                    break;
                default:
                    break;
                }
            }
        }
        port_outbyte(CAN);
        port_outbyte(CAN);
        port_outbyte(CAN);
        flushinput();
        return -2;
 
        for(;;)
        {
start_trans:
            xbuff[0] = SOH; bufsz = 128;
            xbuff[1] = packetno;
            xbuff[2] = ~packetno;
            c = srcsz - len;

            float pro = (float)len/srcsz;
            pro = pro * 100;
            uint32_t show = pro;
            if (g_verbose)
                process_bar(show);  /* 打印进度条 */

            if (c > bufsz) 
                c = bufsz;
            if (c >= 0)
            {
                memset (&xbuff[3], 0, bufsz);
                if (c == 0)
                {
                    xbuff[3] = CTRLZ;
                }
                else
                {
                    memcpy (&xbuff[3], &src[len], c);
                    if (c < bufsz) 
                        xbuff[3+c] = CTRLZ;
                }
                if (crc)
                {
                    unsigned short ccrc = crc16_ccitt(&xbuff[3], bufsz);
                    xbuff[bufsz+3] = (ccrc>>8) & 0xFF;
                    xbuff[bufsz+4] = ccrc & 0xFF;
                }
                else
                {
                    unsigned char ccks = 0;
                    for (i = 3; i < bufsz+3; ++i)
                    {
                        ccks += xbuff[i];
                    }
                    xbuff[bufsz+3] = ccks;
                }
                for (retry = 0; retry < MAXRETRANS; ++retry)
                {
                    for (i = 0; i < bufsz+4+(crc?1:0); ++i)
                    {
                        port_outbyte(xbuff[i]);
                    }
                    c = port_inbyte(DLY_1S);
                    if (last_error == 0 )
                    {
                        switch (c)
                        {
                        case ACK:
                            ++packetno;
                            len += bufsz;
                            goto start_trans;
                        case CAN:
                            c = port_inbyte(DLY_1S);
                            if ( c == CAN)
                            {
                                port_outbyte(ACK);
                                flushinput();
                                return -1;
                            }
                            break;
                        case NAK:
                        default:
                            break;
                        }
                    }
                }
                port_outbyte(CAN);
                port_outbyte(CAN);
                port_outbyte(CAN);
                flushinput();
                return -4;
            }
            else
            {
                for (retry = 0; retry < 10; ++retry)
                {
                    port_outbyte(EOT);
                    c = port_inbyte((DLY_1S)<<1);
                    if (c == ACK) 
                        break;
                }
                flushinput();
                return (c == ACK)?len:-5;
            }
        }
    }
}





static const unsigned short crc16tab[256]= {
 0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
 0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
 0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,
 0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
 0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,
 0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
 0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,
 0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
 0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,
 0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,
 0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
 0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,
 0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,
 0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,
 0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,
 0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,
 0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,
 0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
 0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,
 0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
 0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,
 0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
 0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,
 0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
 0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,
 0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
 0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,
 0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
 0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,
 0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
 0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,
 0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
};


static unsigned short crc16_ccitt(unsigned char *buf, int len)
{
    register int counter;
    register unsigned short crc = 0;
    for( counter = 0; counter < len; counter++)
        crc = (crc<<8) ^ crc16tab[((crc>>8) ^ *(char *)buf++)&0x00FF];
    return crc;
}



