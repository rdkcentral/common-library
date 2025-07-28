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

    module:	bree_spo_srm.c

        For BSP Run-time Execution Engine (BREE) object,
        BroadWay Service Delivery System

    ---------------------------------------------------------------

    description:

        This module implements the advanced management functions
        of the BREE object.


    ---------------------------------------------------------------

    environment:

        platform independent

    ---------------------------------------------------------------

    author:

        Kang Quan

    ---------------------------------------------------------------

    revision:

        01/17/04    initial revision.

**********************************************************************/


#include "bree_spo_global.h"


#ifdef   _BREE_SPO_USE_SRMO


int
BreeSpoCookedResOpen
    (
        const char                  *filename,
        int                         oflag,
        int                         pmode
    )
{
    ANSC_HANDLE                     hRes;
    char                            *pResPath;
    PBREE_SPO_OBJECT                pBreeSpo   = (PBREE_SPO_OBJECT)g_pBreeSrmo->hOwnerContext;

    if (!g_pBreeSrmo)
    {
        return -1;
    }

    /* we assume the web root path is empty string */

    pResPath = (char *)filename;

    hRes = (ANSC_HANDLE)g_pBreeSrmo->MapCookedResource((ANSC_HANDLE)g_pBreeSrmo, pResPath);

    return (int)hRes;
}


int
BreeSpoCookedResClose
    (
        int                         handle
    )
{
    PBREE_SRM_RES_ITEM              pRes    = (PBREE_SRM_RES_ITEM)handle;

    return 0;
}


int 
BreeSpoCookedResRead
    (
        int                         handle, 
        void                        *buffer, 
        unsigned int                count 
    )
{
    PBREE_SRM_RES_ITEM              pRes    = (PBREE_SRM_RES_ITEM)handle;

    if (!pRes || count != pRes->ulContentLen)
    {
        return -1;
    }

    AnscCopyMemory(buffer, pRes->pContent, count);

    return count;
}


long 
BreeSpoGetCookedResLength
    ( 
        int                         handle 
    )
{
    PBREE_SRM_RES_ITEM              pRes    = (PBREE_SRM_RES_ITEM)handle;

    if (!pRes)
    {
        return 0;
    }
    else
    {
        return (long)pRes->ulContentLen;
    }
}


long 
BreeSpoCookedResSeek
    ( 
        int                         handle, 
        long                        offset, 
        int                         origin 
    )
{
    return (long)-1L;
}


int 
BreeSpoCookedResWrite
    ( 
        int                         handle, 
        const void                  *buffer, 
        unsigned int                count 
    )
{
    return -1;
}


#endif

