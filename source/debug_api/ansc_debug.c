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

    module:	ansc_debug.c

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

/***********************************************************
       BASIC OPERATIONS BY MACROS AND INLINE FUNCTIONS
***********************************************************/

#include <ctype.h>

#include "ccsp_trace.h"
#include "user_time.h"
#include "ansc_time.h"
#include "ansc_debug.h"
#include <rdk_debug.h>

/*Structure defined to get the log level type from the given Log Names */

typedef struct loglevel_pair {
  char     *name;
  int      level;
} LOGLEVEL_PAIR;

/**********************************************************************
                    VARIABLES FOR TRACE LEVEL
**********************************************************************/
INT  g_iTraceLevel = CCSP_TRACE_LEVEL_WARNING;

static void
AnscSetTraceLevel_ccsp
    (
        INT  traceLevel
    )
{
    g_iTraceLevel = traceLevel;
    if(g_iTraceLevel > CCSP_TRACE_LEVEL_DEBUG) g_iTraceLevel = CCSP_TRACE_LEVEL_DEBUG;
    else if(g_iTraceLevel < CCSP_TRACE_LEVEL_ALERT) g_iTraceLevel = CCSP_TRACE_LEVEL_ALERT;
}

static void
AnscSetTraceLevel_ansc
    (
        INT  traceLevel
    )
{
    int ansc_level = 0;

    if(traceLevel > CCSP_TRACE_LEVEL_DEBUG) traceLevel = CCSP_TRACE_LEVEL_DEBUG;
    else if(traceLevel < CCSP_TRACE_LEVEL_ALERT) traceLevel = CCSP_TRACE_LEVEL_ALERT;
    
    switch(traceLevel)
    {
    case CCSP_TRACE_LEVEL_EMERGENCY:
#ifndef FEATURE_SUPPORT_RDKLOG
        ansc_level = ANSC_TRACE_LEVEL_DEATH;
        break;
    case CCSP_TRACE_LEVEL_ALERT:
    case CCSP_TRACE_LEVEL_CRITICAL:
#endif
        ansc_level = ANSC_TRACE_LEVEL_CRITICAL;
        break;
    case CCSP_TRACE_LEVEL_ERROR:
        ansc_level = ANSC_TRACE_LEVEL_ERROR;
        break;
    case CCSP_TRACE_LEVEL_WARNING:
        ansc_level = ANSC_TRACE_LEVEL_WARNING;
        break;
    case CCSP_TRACE_LEVEL_NOTICE:
        ansc_level = ANSC_TRACE_LEVEL_TEST;
        break;
    case CCSP_TRACE_LEVEL_INFO:
        ansc_level = ANSC_TRACE_LEVEL_FLOW; /*for AnscTrace(), this macro compares with ANSC_TRACE_LEVEL_FLOW*/
        break;
    case CCSP_TRACE_LEVEL_DEBUG:
        ansc_level = ANSC_TRACE_LEVEL_VERBOSE;
        break;
    default:
        ansc_level = ANSC_TRACE_INVALID_LEVEL;
    }

    AnscTraceSetAllIdcfgLevels(ansc_level);
}

void
AnscSetTraceLevel
    (
        INT  traceLevel
    )
{
    AnscSetTraceLevel_ccsp(traceLevel);    
    AnscSetTraceLevel_ansc(traceLevel);    
}

void Ccsplog3(char *pComponentName, char *LogMsg)
{
    unsigned int i;
    int level = RDK_LOG_INFO;

    static const LOGLEVEL_PAIR loglevel_type_table[] = {
        { "RDK_LOG_ERROR",  RDK_LOG_ERROR  },
        { "RDK_LOG_WARN",   RDK_LOG_WARN   },
        { "RDK_LOG_NOTICE", RDK_LOG_NOTICE },
        { "RDK_LOG_INFO",   RDK_LOG_INFO   },
        { "RDK_LOG_DEBUG",  RDK_LOG_DEBUG  },
        { "RDK_LOG_FATAL",  RDK_LOG_FATAL  },
    };

    for (i = 0; i < (sizeof(loglevel_type_table)/sizeof(loglevel_type_table[0])); i++)
    {
        size_t len = strlen(loglevel_type_table[i].name);

        if (strncmp(LogMsg, loglevel_type_table[i].name, len) == 0)
        {
            level = loglevel_type_table[i].level;
            LogMsg += len;
            break;
        }
    }

    /*
       LogMsg is a string such as "RDK_LOG_ERROR, ..."
       Drop any leading commas or spaces which remain after stripping
       the log level prefix.
    */
    while ((*LogMsg == ',') || (*LogMsg == ' '))
    {
        LogMsg++;
    }

    CcspTraceExec(pComponentName, level, (LogMsg));
}

const char *CcspTraceGetRdkLogModule(const char *pComponentName)
{
    if (!pComponentName)
        return "LOG.RDK.Misc";

    if (strncmp(pComponentName, "com.cisco.spvtg.ccsp.", strlen("com.cisco.spvtg.ccsp.")) == 0) {
        pComponentName += strlen("com.cisco.spvtg.ccsp.");

        switch (tolower(*pComponentName))
        {
            case 'a':
                if (strcmp(pComponentName, "advsec") == 0)                  return "LOG.RDK.ADVSEC";
                break;
            case 'b':
                break;
            case 'c':
                if (strcmp(pComponentName, "CR") == 0)                      return "LOG.RDK.CR";
#if defined (FEATURE_RDKB_CELLULAR_MANAGER)
                if (strcmp(pComponentName, "cellularmanager") == 0)         return "LOG.RDK.CELLULARMANAGER";
#endif
                if (strcmp(pComponentName, "cm") == 0)                      return "LOG.RDK.CM";
                break;
            case 'd':
#if defined (FEATURE_RDKB_DHCP_MANAGER)
                if (strcmp(pComponentName, "dhcpmgr") == 0)                 return "LOG.RDK.DHCPMGR";
#endif
                if (strcmp(pComponentName, "dslagent") == 0)                return "LOG.RDK.DSLAGENT";
                break;
            case 'e':
#if defined (FEATURE_SUPPORT_EASYMESH_CONTROLLER)
                if (strcmp(pComponentName, "emctl") == 0)                   return "LOG.RDK.EASYMESHCTL";
#endif
                if (strcmp(pComponentName, "ethagent") == 0)                return "LOG.RDK.ETHAGENT";
                break;
            case 'f':
                if (strcmp(pComponentName, "fu") == 0)                      return "LOG.RDK.FU";
#if defined (FEATURE_RDKB_WAN_MANAGER) || defined (FEATURE_FWUPGRADE_MANAGER)
                if (strcmp(pComponentName, "fwupgrademanager") == 0)        return "LOG.RDK.FWUPGRADEMANAGER";
#endif
                break;
            case 'g':
#if defined (GATEWAY_FAILOVER_SUPPORTED)
                if (strcmp(pComponentName, "gatewaymanager") == 0)          return "LOG.RDK.GATEWAYMANAGER";
#endif
#if defined (FEATURE_RDKB_GPON_MANAGER)
                if (strcmp(pComponentName, "gponmanager") == 0)             return "LOG.RDK.GPONMANAGER";
#endif
                break;
            case 'h':
                if (strcmp(pComponentName, "harvester") == 0)               return "LOG.RDK.Harvester";
                if (strcmp(pComponentName, "hotspot") == 0)                 return "LOG.RDK.HOTSPOT";
                break;
            case 'i':
#if defined (FEATURE_RDKB_INTER_DEVICE_MANAGER)
                if (strcmp(pComponentName, "interdevicemanager") == 0)      return "LOG.RDK.INTERDEVICEMANAGER";
#endif
                break;
            case 'j':
                break;
            case 'k':
                break;
            case 'l':
#if defined (FEATURE_RDKB_WAN_MANAGER)
                if (strcmp(pComponentName, "ledmanager") == 0)              return "LOG.RDK.RDKLEDMANAGER";
#endif
                break;
            case 'm':
#if !defined (NO_MOCA_FEATURE_SUPPORT)
                if (strcmp(pComponentName, "moca") == 0)                    return "LOG.RDK.MOCA";
#endif
                if (strcmp(pComponentName, "mta") == 0)                     return "LOG.RDK.MTA";
                break;
            case 'n':
#if defined (FEATURE_RDKB_NFC_MANAGER)
                if (strcmp(pComponentName, "nfcmanager") == 0)              return "LOG.RDK.RDKNFCMANAGER";
#endif
                if (strcmp(pComponentName, "notifycomponent") == 0)         return "LOG.RDK.NOTIFY";
                break;
            case 'o':
                break;
            case 'p':
                if (strcmp(pComponentName, "pam") == 0)                     return "LOG.RDK.PAM";
#if defined (FEATURE_RDKB_WAN_MANAGER)
                if (strcmp(pComponentName, "platformmanager") == 0)         return "LOG.RDK.PLATFORMMANAGER";
                if (strcmp(pComponentName, "pppmanager") == 0)              return "LOG.RDK.PPPMANAGER";
#endif
                if (strcmp(pComponentName, "psm") == 0)                     return "LOG.RDK.PSM";
                break;
            case 'q':
                break;
            case 'r':
                break;
            case 's':
                if (strcmp(pComponentName, "ssd") == 0)                     return "LOG.RDK.SSD";
                break;
            case 't':
                if (strcmp(pComponentName, "tdm") == 0)                     return "LOG.RDK.TDM";
#if defined (FEATURE_RDKB_WAN_MANAGER) || defined (FEATURE_RDKB_TELCOVOICE_MANAGER)
                if (strcmp(pComponentName, "telcovoicemanager") == 0)       return "LOG.RDK.TELCOVOICEMANAGER";
#endif
                if (strcmp(pComponentName, "telcovoipagent") == 0)          return "LOG.RDK.TELCOVOIPAGENT";
                if (strcmp(pComponentName, "telemetry") == 0)               return "LOG.RDK.T2";
#if defined (FEATURE_RDKB_THERMAL_MANAGER)
                if (strcmp(pComponentName, "thermalmanager") == 0)          return "LOG.RDK.RDKTHERMALMANAGER";
#endif
                if (strcmp(pComponentName, "tr069pa") == 0)                 return "LOG.RDK.TR69";
                break;
            case 'u':
                break;
            case 'v':
                if (strcmp(pComponentName, "vlanagent") == 0)               return "LOG.RDK.VLANAGENT";
#if defined (FEATURE_RDKB_WAN_MANAGER)
                if (strcmp(pComponentName, "vlanmanager") == 0)             return "LOG.RDK.VLANMANAGER";
#endif
                break;
            case 'w':
                if (strcmp(pComponentName, "wanagent") == 0)                return "LOG.RDK.WANAGENT";
#if defined (FEATURE_RDKB_WAN_MANAGER)
                if (strcmp(pComponentName, "wanmanager") == 0)              return "LOG.RDK.WANMANAGER";
#endif
                if (strcmp(pComponentName, "wifi") == 0)                    return "LOG.RDK.WIFI";
                break;
            case 'x':
#if defined (XDNS_ENABLE)
                if (strcmp(pComponentName, "xdns") == 0)                    return "LOG.RDK.XDNS";
#endif
#if defined (FEATURE_RDKB_WAN_MANAGER)
                if (strcmp(pComponentName, "xdslmanager") == 0)             return "LOG.RDK.XDSLMANAGER";
#endif
                if (strcmp(pComponentName, "xtmagent") == 0)                return "LOG.RDK.XTMAGENT";
#if defined (FEATURE_RDKB_WAN_MANAGER)
                if (strcmp(pComponentName, "xtmmanager") == 0)              return "LOG.RDK.XTMMANAGER";
#endif
                break;
            case 'y':
                break;
            case 'z':
                break;

            default:
                break;
        }

        return "LOG.RDK.Misc";
    }

    if (strncmp(pComponentName, "LOG.RDK.", strlen("LOG.RDK.")) == 0)
        return pComponentName;

    if (strcmp(pComponentName, "CCSP_SNMNP_Plugin") == 0)                   return "LOG.RDK.SNMP";
    if (strcmp(pComponentName, "dhcp_snooperd") == 0)                       return "LOG.RDK.DHCPSNOOP";

    return  "LOG.RDK.Misc";
}

