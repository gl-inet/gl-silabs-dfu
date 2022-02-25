/*****************************************************************************
 * @file  hal.c
 * @brief Hardware interface adaptation
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
#include <stdlib.h>
#include <string.h>
#include <uci.h>
#include <unistd.h>   
#include <fcntl.h>   
#include "gl_uart.h"
#include "gl_hal.h"


unsigned char ENDIAN;


char model[20] = {0};

static int check_endian(void);

static int check_endian(void)
{
  int x = 1;
  if(*((char *)&x) == 1) 
  {
	ENDIAN = 0;   
	// printf("little endian\n");
  }else{
	ENDIAN = 1;   
	// printf("big endian\n");
  }

  return 0;
}

int hal_init(char *port, uint32_t baudRate, uint32_t flowcontrol)
{
    check_endian();

	// Manual clean uart cache: fix tcflush() not work on some model
	uartCacheClean();

    int serialFd = uartOpen((int8_t*)port, baudRate, flowcontrol, 100);
    if( serialFd < 0 )
    {
        fprintf(stderr,"Hal initilized failed.\n");
        return -1;
    }

    return serialFd;
}

int hal_destroy(void)
{
	return uartClose();
}
