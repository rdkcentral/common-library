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

    module:	ansc_base64.c

        For Advanced Networking Service Container (ANSC),
        BroadWay Service Delivery System

    ---------------------------------------------------------------

    description:

        This module implements Base64 encoding and decoding 
        functions.

        *   AnscBase64Decode
        *   AnscBase64Encode

    ---------------------------------------------------------------

    environment:

        platform independent

    ---------------------------------------------------------------

    author:

        Kang Quan

    ---------------------------------------------------------------

    revision:

        03/24/03    initial revision.
        11/24/03    zhubin modified Base64 to decode whole data instead
                    of line by line, in order to support DingHua's 
                    requirement of decoding base64 with 75 chars per line

**********************************************************************/


#include "ansc_platform.h"
#include <trower-base64/base64.h>
#include "stdlib.h"
#include "string.h"

#define  ANSC_BASE64_CODES      \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"


#define ANSC_BASE64_ENCODE_OVERRUN_SIZE             3
#define ANSC_BASE64_DECODE_OVERRUN_SIZE             3


/**********************************************************************

    caller:     owner of this object

    prototype:

        PUCHAR
        AnscBase64DecodeLine
            (
                const PUCHAR                pString,
                PUCHAR                      pData,
                PULONG                      pulSize
            )

    description:

        This function is called to decode a line of 
        Base64 encode message to original text. Users
        should not call this function directly. Instead
        users should call AnscBase64Decode.

    argument:   const PUCHAR                pString
                Buffer to Base64 encoded message.

                PUCHAR                      pData
                Buffer to decoded text.

                PULONG                      pulSize [OUT]
                It contains the length of decoded text after
                this functions successfully returns.

    return:     Buffer that contains decoded text, no need 
                to be free. 

**********************************************************************/

PUCHAR
AnscBase64DecodeLine
    (
        const PUCHAR                pString,
        PUCHAR                      pData,
        PULONG                      pulSize
    )
{
    int size = 0;
    if (pData == NULL || pulSize == NULL)
    {
        return NULL;
    }
    size = b64_decode( (const uint8_t*)pString,(size_t) *pulSize, (uint8_t *)pData );
    CcspTraceWarning(("base64 decoded data contains %d bytes\n",size));
    if (pulSize)
    {
        *pulSize    = (ULONG)size;
    }
    return pData;
}


/**********************************************************************

    caller:     owner of this object

    prototype:

        PUCHAR
        AnscBase64Decode
            (
                PUCHAR                      pEncode,
                PULONG                      pulSize
            );

    description:

        This function is called to decode Base64 encode
        message to original text.

    argument:   PUCHAR                      pEncode
                Buffer to Base64 encoded message.

                PULONG                      pulSize [OUT]
                It contains the length of decoded text after
                this functions successfully returns.

    return:     Buffer that contains decoded text, needs to
                be free after use.

**********************************************************************/

PUCHAR
AnscBase64Decode
    (
        PUCHAR                      pEncode,
        PULONG                      pulSize
    )
{
    PUCHAR                          pDecode, pBuf;
    ULONG                           ulEncodedSize;

    pBuf            = pEncode;

    /* allocate big enough memory to avoid memory reallocation */
    ulEncodedSize   = AnscSizeOfString((const char*)pEncode);
    pDecode         = (PUCHAR)AnscAllocateMemory(ulEncodedSize);

    if( AnscBase64DecodeLine(pBuf, pDecode, &ulEncodedSize) == NULL)
    {
        AnscTrace("Failed to decode the Base64 data.\n");

        AnscFreeMemory(pDecode);

        return NULL;
    }

    if (pulSize)
    {
        *pulSize    = ulEncodedSize;
    }

    return pDecode;
}


/**********************************************************************

    caller:     owner of this object

    prototype:

        PUCHAR
        AnscBase64Encode
            (
                const PUCHAR                pBuf,
                ULONG                       ulSize
            );

    description:

        This function is called to encode given text in Base64.

    argument:   const PUCHAR                pBuf
                Buffer to text needs to be encoded.

                ULONG                       ulSize
                Size of buffer.

    return:     Buffer to encoded message, should be free
                by caller after use.

**********************************************************************/

PUCHAR
AnscBase64Encode
    (
        const PUCHAR                pBuf,
        ULONG                       ulSize
    )
{
    int                             div         = ulSize / 3;
    int                             rem         = ulSize % 3;
    int                             chars       = div * 4 + rem + 1;
    PUCHAR                          pString;
    PUCHAR                          pData       = pBuf;
    PUCHAR                          pBase64Codes;

    pBase64Codes    = (PUCHAR)ANSC_BASE64_CODES;

    pString         = 
        (PUCHAR)AnscAllocateMemory(chars + ANSC_BASE64_ENCODE_OVERRUN_SIZE);

    if (pString)
    {
        PUCHAR                      buf = pString;

        while (div > 0)
        {
            buf[0]  = pBase64Codes[ (pData[0] >> 2) & 0x3F];
            buf[1]  = pBase64Codes[((pData[0] << 4) & 0x30) + ((pData[1] >> 4) & 0xF)];
            buf[2]  = pBase64Codes[((pData[1] << 2) & 0x3C) + ((pData[2] >> 6) & 0x3)];
            buf[3]  = pBase64Codes[  pData[2] & 0x3F];
            pData   += 3;
            buf     += 4;
            div --;
        }

        switch (rem)
        {
            case    2:

                    buf[0]  = pBase64Codes[ (pData[0] >> 2) & 0x3F];
                    buf[1]  = pBase64Codes[((pData[0] << 4) & 0x30) + ((pData[1] >> 4) & 0xF)];
                    buf[2]  = pBase64Codes[ (pData[1] << 2) & 0x3C];
                    buf[3]  = '=';
                    buf     += 4;

                break;

            case    1:

                    buf[0]  = pBase64Codes[ (pData[0] >> 2) & 0x3F];
                    buf[1]  = pBase64Codes[ (pData[0] << 4) & 0x30];
                    buf[2]  = '=';
                    buf[3]  = '=';
                    buf     += 4;

                break;
        }

        *buf = '\0';
    }

    return pString;
}