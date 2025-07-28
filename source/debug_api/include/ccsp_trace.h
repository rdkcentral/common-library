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

    module: ccsp_debug.h

        For CCSP trace related APIs

    ---------------------------------------------------------------

    description:

        This API header file defines all the CCSP trace related
        functions.

    ---------------------------------------------------------------

    environment:

        platform independent

    ---------------------------------------------------------------

    author:

        Hui Ma

    ---------------------------------------------------------------

    revision:

        04/25/11    initial revision.

**********************************************************************/

#ifndef _CCSP_TRACE_H_
#define _CCSP_TRACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "user_base.h"
#include "ansc_time.h"
#include "user_time.h"

extern int g_iTraceLevel;
extern char *g_TraceLevelStr[8];
extern char *pComponentName;

/*
 *  Whether a debug trace is output depends on the following factors:
 *
 *    1) The trace level passed in the trace statement is higher (smaller value)
 *       than the trace level configured.
 */

/***********************************************************
                   CONSTANT DEFINITIONS
***********************************************************/

#if defined(FEATURE_SUPPORT_RDKLOG)

#include <rdk_debug.h>

#define CCSP_TRACE_LEVEL_EMERGENCY                 RDK_LOG_FATAL        /* system is going to die                               */
#define CCSP_TRACE_LEVEL_ALERT                     RDK_LOG_FATAL        /* system is unable to perform its functionality        */
#define CCSP_TRACE_LEVEL_CRITICAL                  RDK_LOG_FATAL        /* handled errors                                       */
#define CCSP_TRACE_LEVEL_ERROR                     RDK_LOG_ERROR        /* handled errors                                       */
#define CCSP_TRACE_LEVEL_WARNING                   RDK_LOG_WARN         /* warning about abnormal situation -- default level    */
#define CCSP_TRACE_LEVEL_NOTICE                    RDK_LOG_NOTICE
#define CCSP_TRACE_LEVEL_INFO                      RDK_LOG_INFO         /* informational trace in regular routines              */
#define CCSP_TRACE_LEVEL_DEBUG                     RDK_LOG_DEBUG

#elif defined(FEATURE_SUPPORT_SYSLOG)

#include <syslog.h>

#define CCSP_TRACE_LEVEL_EMERGENCY                 LOG_EMERG            /* system is going to die                               */
#define CCSP_TRACE_LEVEL_ALERT                     LOG_ALERT            /* system is unable to perform its functionality        */
#define CCSP_TRACE_LEVEL_CRITICAL                  LOG_CRIT             /* handled errors                                       */
#define CCSP_TRACE_LEVEL_ERROR                     LOG_ERR              /* handled errors                                       */
#define CCSP_TRACE_LEVEL_WARNING                   LOG_WARNING          /* warning about abnormal situation -- default level    */
#define CCSP_TRACE_LEVEL_NOTICE                    LOG_NOTICE
#define CCSP_TRACE_LEVEL_INFO                      LOG_INFO             /* informational trace in regular routines              */
#define CCSP_TRACE_LEVEL_DEBUG                     LOG_DEBUG            /* say whatever you like, level 1                       */

#else

#define CCSP_TRACE_LEVEL_EMERGENCY                 0                    /* system is going to die                               */
#define CCSP_TRACE_LEVEL_ALERT                     1                    /* system is unable to perform its functionality        */
#define CCSP_TRACE_LEVEL_CRITICAL                  2                    /* handled errors                                       */
#define CCSP_TRACE_LEVEL_ERROR                     3                    /* handled errors                                       */
#define CCSP_TRACE_LEVEL_WARNING                   4                    /* warning about abnormal situation -- default level    */
#define CCSP_TRACE_LEVEL_NOTICE                    5
#define CCSP_TRACE_LEVEL_INFO                      6                    /* informational trace in regular routines              */
#define CCSP_TRACE_LEVEL_DEBUG                     7                    /* say whatever you like, level 1                       */

#endif

#define CCSP_TRACE_INVALID_LEVEL                  -1

/***********************************************************
            FUNCTION AND MACRO PROTOTYPE ROUTING
***********************************************************/

#if defined(FEATURE_SUPPORT_RDKLOG)

#define EXTRACT_ARGS(msg ...) msg

#define CcspTraceExec(pComponentName, level, msg) \
    RDK_LOG(level, CcspTraceGetRdkLogModule(pComponentName), EXTRACT_ARGS msg);

#elif defined(FEATURE_SUPPORT_SYSLOG)

#define CcspTraceBase(arg ...) \
    do { \
        snprintf(pTempChar, 4095, arg); \
    } while (0)

#define CcspTraceShortenFileName(buf, maxlen, fn) \
    do { \
        int nLen = strlen(fn); \
        if (nLen < maxlen) { \
            memcpy(buf, fn, nLen + 1); \
        } \
        else { \
            strcpy(buf, fn + nLen - maxlen + 1); \
        } \
    } while (0)

#define CcspTraceExec(pComponentName, level, msg) \
    if (level <= g_iTraceLevel) \
    { \
        char *pTempChar = malloc(4096); \
        if (pTempChar) \
        { \
            char sfn[256]; \
            CcspTraceBase msg; \
            CcspTraceShortenFileName(sfn, sizeof(sfn), __FILE__); \
            openlog(NULL, LOG_PID, LOG_USER); \
            syslog(LOG_USER|level, "%s:%d %s %s", sfn, __LINE__, pComponentName, pTempChar); \
            closelog(); \
            free(pTempChar); \
        } \
    }

#else

#define CcspTraceBase(arg ...) \
    do { \
        fprintf(stderr, arg); \
    } while (0)

#define AnscGetSubComponentName(desName, srcName) \
    { \
        int index = strlen(srcName); \
        for ( ; index > 0; --index) \
        { \
            if (srcName[index-1] == '.') \
                break; \
        } \
        strcpy(desName, &srcName[index]); \
    }

#define CcspTraceExec(pComponentName, level, msg) \
    if (level <= g_iTraceLevel) \
    { \
        ANSC_UNIVERSAL_TIME ut; \
        char subComponentName[64]; \
        char *TraceLevelStr[] = { "EMERGENCY", "ALERT", "CRITICAL", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG" }; \
        AnscGetSubComponentName(subComponentName, pComponentName); \
        AnscGetLocalTime(&ut); \
        fprintf(stderr, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d-%s-%s-", ut.Year, ut.Month, ut.DayOfMonth, ut.Hour, ut.Minute, ut.Second, subComponentName, TraceLevelStr[level]); \
        CcspTraceBase msg; \
    }

#endif

/**********************************************************************************************
  When use the following macros, msg should be wrapped by parenthesis.
  For example, CcspTraceEmergency(pComponentName, ("print info:%s\n", pPrintInfo))
***********************************************************************************************/
#define CcspTraceEmergency2(pComponentName, msg)    CcspTraceExec(pComponentName, CCSP_TRACE_LEVEL_EMERGENCY, msg)
#define CcspTraceAlert2(pComponentName, msg)        CcspTraceExec(pComponentName, CCSP_TRACE_LEVEL_ALERT, msg)
#define CcspTraceCritical2(pComponentName, msg)     CcspTraceExec(pComponentName, CCSP_TRACE_LEVEL_CRITICAL, msg)
#define CcspTraceError2(pComponentName, msg)        CcspTraceExec(pComponentName, CCSP_TRACE_LEVEL_ERROR, msg)
#define CcspTraceError3(pComponentName, msg)        CcspTraceExec(pComponentName, CCSP_TRACE_LEVEL_ERROR, msg)
#define CcspTraceWarning2(pComponentName, msg)      CcspTraceExec(pComponentName, CCSP_TRACE_LEVEL_WARNING, msg)
#define CcspTraceNotice2(pComponentName, msg)       CcspTraceExec(pComponentName, CCSP_TRACE_LEVEL_NOTICE, msg)
#define CcspTraceDebug2(pComponentName, msg)        CcspTraceExec(pComponentName, CCSP_TRACE_LEVEL_DEBUG, msg)
#define CcspTraceInfo2(pComponentName, msg)         CcspTraceExec(pComponentName, CCSP_TRACE_LEVEL_INFO, msg)

#define CcspTraceEmergency(msg)                     CcspTraceEmergency2(pComponentName, msg)
#define CcspTraceAlert(msg)                         CcspTraceAlert2(pComponentName, msg)
#define CcspTraceCritical(msg)                      CcspTraceCritical2(pComponentName, msg)
#define CcspTraceError(msg)                         CcspTraceError2(pComponentName, msg)
#define CcspTraceWarning(msg)                       CcspTraceWarning2(pComponentName, msg)
#define CcspTraceNotice(msg)                        CcspTraceNotice2(pComponentName, msg)
#define CcspTraceInfo(msg)                          CcspTraceInfo2(pComponentName, msg)

#ifdef NDEBUG
#define CcspTraceDebug(msg)
#else
#define CcspTraceDebug(msg)                         CcspTraceDebug2(pComponentName, msg)
#endif

void AnscSetTraceLevel (int traceLevel);
const char *CcspTraceGetRdkLogModule (const char *pComponentName);
void Ccsplog3 (char *pComponentName, char *LogMsg);

#ifdef __cplusplus
}
#endif

#endif
