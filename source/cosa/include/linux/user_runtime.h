/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2015 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**********************************************************************
   Copyright [2014] [Cisco Systems, Inc.]
 
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
 
       http://www.apache.org/licenses/LICENSE-2.0
 
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
**********************************************************************/


/**********************************************************************

    module:	user_runtime.h

        For Advanced Networking Service Container (ANSC),
        BroadWay Service Delivery System

    ---------------------------------------------------------------

    description:

        This wrapper file defines the commonly used C runtime
        library functions.

    ---------------------------------------------------------------

    environment:

        platform independent

    ---------------------------------------------------------------

    author:

        Xuechen Yang
        Geng Yang
        Ying Lou

    ---------------------------------------------------------------

    revision:

        01/19/01    initial revision.
        09/14/01    adapted to linux kernel by Geng Yang
        04/03/02    adapted to linux user by Ying Loun.

**********************************************************************/

#ifndef  _USER_RUNTIME_
#define  _USER_RUNTIME_

#include <signal.h>
#include <dirent.h>

/***********************************************************
    PLATFORM DEPENDENT DATA TYPE AND MACRO DEFINITIONS
***********************************************************/

#define  _ansc_min(x,y) 			((x)<(y)?(x):(y))
#define  _ansc_max(x,y) 			((x)>(y)?(x):(y))

#define  _ansc_atof				atof
#define  _ansc_atoi				atoi
#define  _ansc_atol				atoll

char *_ansc_itoa(int value, char *string, int radix);

#define  _ansc_rand				rand
#define  _ansc_srand				srand

#define  _ansc_memchr				memchr
#define  _ansc_memicmp  			memcmp
#define  _ansc_memcmp				memcmp
#define  _ansc_memcpy				memcpy
#define  _ansc_memset				memset
#define  _ansc_memmove  			memmove

#define  _ansc_strcpy				strcpy
#define  _ansc_strncpy  			strncpy
#define  _ansc_strcat				strcat
#define  _ansc_strcmp				strcmp
#define  _ansc_strncmp  			strncmp
#define  _ansc_strlen				strlen
#define  _ansc_strchr				strchr

#define  _ansc_stricmp  			strcasecmp
#define  _ansc_strcspn  			strcspn
#define  _ansc_strspn				strspn
#define  _ansc_strstr				strstr
#define  _ansc_strtok				strtok

#define  _ansc_sprintf  			sprintf
#define  _ansc_snprintf 			snprintf
#define  _ansc_vsprintf 			vsprintf
#define  _ansc_sscanf				sscanf

#endif

