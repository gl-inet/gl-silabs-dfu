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

#include "xmodem_main.h"
#include "bgapi_main.h"

int main(int argc, char *argv[])
{
    int ret;

    if (argc < 2) {
        printf("gl-silabs-dfu missing parameters\n");
		return -1;
    }

    if(0 == strncmp(argv[1], "bgapi", strlen("bgapi")))
    {
        bgapi_main(argc, argv);
    }else if(0 == strncmp(argv[1], "xmodem", strlen("xmodem"))) {
        xmodem_main(argc, argv);
    }else{
        printf("Unsupported dfu type! Support dfu type: bgapi | xmodem  \n");
    }

    return 0;
}
