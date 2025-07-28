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

    module: user_base_custom_linux.h

        For Advanced Networking Service Container (ANSC),
        BroadWay Service Delivery System

    ---------------------------------------------------------------

    description:

        This header file gives the custom ANSC wrapper base
        definitions.

    ---------------------------------------------------------------

    environment:

        platform dependent

    ---------------------------------------------------------------

    author:

        Ding Hua

**********************************************************************/

#ifndef  _USER_BASE_CUSTOM_LINUX_H_
#define  _USER_BASE_CUSTOM_LINUX_H_


/***************************************************************
        Platform/Compiler Specific Definitions
***************************************************************/

#if !defined(__BYTE_ORDER__)
#error "__BYTE_ORDER__ is not defined?"
#endif

#undef _ANSC_LITTLE_ENDIAN_

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define _ANSC_LITTLE_ENDIAN_
#endif

/*
 *  Indicates whether word access has to occur on word boundary
 */
/*#define  _ANSC_BYTE_ACCESS_*/

/***************************************************************
                OS Specific Definitions
***************************************************************/

/*
 *  Indicates whether TmGmt function is available
 */
/*#define  _ANSC_LINUX_NO_TM_GMT*/

/***************************************************************
            Individual Module Conditional Definitions
***************************************************************/

/*
 *  Leave them here temporarily, should move to individual module
 *  custom definition header files.
 */

/*#define  _ANSC_SLAP_LPC_*/
#define _SLAP_IPC_USE_TCP_SOCKET

/*#define  _ANSC_DIRTY_PDO_*/

/*#define  _ANSC_IP4S_IGMPV2_*/
/*#define  _ANSC_IP4S_IGMPV3_*/
/*#define  _ANSC_IGMP_PROXY_*/
/*#define  _ANSC_IGMP_SNOOPING_*/

/*#define  _ANSC_MIB2_PRINT_MIB_REG_*/

#endif

