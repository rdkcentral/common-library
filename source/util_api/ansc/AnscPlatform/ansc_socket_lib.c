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

    module:	ansc_socket_lib.c

        For Advanced Networking Service Container (ANSC),
        BroadWay Service Delivery System

    ---------------------------------------------------------------

    description:

        This module implements the well-known bsd-compatible socket
        library functions.

        *   AnscSocketLibInitialize
        *   AnscSocketLibUnload
        *   AnscSocketLibAccept
        *   AnscSocketLibBind
        *   AnscSocketLibCloseSocket
        *   AnscSocketLibConnect
        *   AnscSocketLibGetHostByAddr
        *   AnscSocketLibGetHostByName
        *   AnscSocketLibGetHostName
        *   AnscSocketLibGetPeerName
        *   AnscSocketLibGetSockName
        *   AnscSocketLibGetSockOpt
        *   AnscSocketLibIoctlSocket
        *   AnscSocketLibInetAddr
        *   AnscSocketLibInetNtoa
        *   AnscSocketLibListen
        *   AnscSocketLibRecv
        *   AnscSocketLibRecvFrom
        *   AnscSocketLibSelect
        *   AnscSocketLibSend
        *   AnscSocketLibSendTo
        *   AnscSocketLibSetSockOpt
        *   AnscSocketLibShutdown
        *   AnscSocketLibSocket
        *   AnscSocketLibGetLastError
        *   AnscSocketLibFdsIsSet
        *   AnscSocketTlsInitialize
        *   AnscSocketTlsUnload
        *   AnscSocketTlsGetScsIf

    ---------------------------------------------------------------

    environment:

        platform independent

    ---------------------------------------------------------------

    author:

        Xuechen Yang

    ---------------------------------------------------------------

    revision:

        04/15/02    initial revision.

**********************************************************************/


#include "ansc_global.h"
#include "bss_ifo_bsd.h"
#include "safec_lib_common.h"

static  ANSC_HANDLE                 g_hTlsScsIf      = (ANSC_HANDLE)NULL;
static  BOOL                        g_bTlsScsStarted = FALSE;

#define  AnscSocketLibSetLastError(last_error)                                              \
         {                                                                                  \
            PANSC_TASK_RECORD       task_record = NULL;                                     \
                                                                                            \
            task_record = (PANSC_TASK_RECORD)AnscGetCurTaskRecord();                        \
                                                                                            \
            if ( task_record )                                                              \
            {                                                                               \
                AnscTaskSetSsocketError(task_record, last_error);                           \
            }                                                                               \
         }

#define  AnscSocketLibValidate1(error_code)                                                 \
         {                                                                                  \
            if ( !g_bBssBsdStarted || !g_hBssBsdIf )                                        \
            {                                                                               \
                error_code = ANSC_SOCKET_ERROR_LIB_UNAVAILABLE;                             \
                                                                                            \
                AnscSocketLibSetLastError(error_code);                                      \
                                                                                            \
                return  ANSC_SOCKET_INVALID_SOCKET;                                         \
            }                                                                               \
         }

#define  AnscSocketLibValidate2(error_code)                                                 \
         {                                                                                  \
            if ( !g_bBssBsdStarted || !g_hBssBsdIf )                                        \
            {                                                                               \
                error_code = ANSC_SOCKET_ERROR_LIB_UNAVAILABLE;                             \
                                                                                            \
                AnscSocketLibSetLastError(error_code);                                      \
                                                                                            \
                return  ANSC_SOCKET_ERROR;                                                  \
            }                                                                               \
         }

#define  AnscSocketLibValidate3(error_code)                                                 \
         {                                                                                  \
            if ( !g_bBssBsdStarted || !g_hBssBsdIf )                                        \
            {                                                                               \
                error_code = ANSC_SOCKET_ERROR_LIB_UNAVAILABLE;                             \
                                                                                            \
                AnscSocketLibSetLastError(error_code);                                      \
                                                                                            \
                return  NULL;                                                               \
            }                                                                               \
         }


void
AnscSocketTlsInitialize
    (
        ANSC_HANDLE                 hTlsScsIf
    )
{
    if ( !g_bTlsScsStarted || !g_hTlsScsIf )
    {
        g_hTlsScsIf      = hTlsScsIf;
        g_bTlsScsStarted = TRUE;
    }

    return;
}


void
AnscSocketTlsUnload
    (
        void
    )
{
    g_hTlsScsIf      = (ANSC_HANDLE)NULL;
    g_bTlsScsStarted = FALSE;

    return;
}


ANSC_HANDLE
AnscSocketTlsGetScsIf
    (
        void
    )
{
    return  g_hTlsScsIf;
}
