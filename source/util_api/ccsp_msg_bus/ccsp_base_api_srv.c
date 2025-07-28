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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ccsp_message_bus.h>
#include <ccsp_base_api.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <dslh_definitions_database.h>
#include "ccsp_trace.h"
#include <rbus_message_bus.h>

#define MAX_EVENT_NAME_LEN 128

extern void* COSAGetMessageBusHandle();
extern int   CcspBaseIf_timeout_seconds;
extern int   CcspBaseIf_timeout_getval_seconds;

int   CcspBaseIf_timeout_protect_plus_seconds    = 5;
int   CcspBaseIf_deadlock_detection_time_normal_seconds = -1; //will be reassigned by dbus initiate function
int   CcspBaseIf_deadlock_detection_time_getval_seconds = -1; //will be reassigned by dbus initiate function

int deadlock_detection_enable                           = 0;
CCSP_DEADLOCK_DETECTION_INFO    deadlock_detection_info = {{{0}}, NULL, NULL, 0, 0, 0, 0};;
DEADLOCK_ARRAY*  deadlock_detection_log                 = NULL; //this variable is protected by mutex lock in deadlock_detection_info
unsigned int deadlock_detection_log_index               = 0;

void CcspBaseIf_deadlock_detection_log_save();

static inline time_t GetCurrentTime(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec;
#else
    return time(NULL); 
#endif
}

/* These two macro are used to transfer information for deadlock detection thread */
#define CCSP_DEADLOCK_DETECTION_ENTER(messageType1, parameterArray, size1)     \
    if ( deadlock_detection_enable )                                     \
    {                                                                    \
        if ( !deadlock_detection_info.messageType )                      \
        {                                                                \
            pthread_mutex_lock(&(deadlock_detection_info.info_mutex));   \
            deadlock_detection_info.messageType   = messageType1;         \
            deadlock_detection_info.parameterInfo = (void *)parameterArray; \
            deadlock_detection_info.size          = size1;                \
            deadlock_detection_info.enterTime     = GetCurrentTime();           \
            if ( strstr("getParameterValues", messageType1 ) )            \
            {    deadlock_detection_info.detectionDuration = CcspBaseIf_deadlock_detection_time_getval_seconds; } \
            else                                                         \
            {    deadlock_detection_info.detectionDuration = CcspBaseIf_deadlock_detection_time_normal_seconds; } \
            pthread_mutex_unlock(&(deadlock_detection_info.info_mutex)); \
            CcspBaseIf_deadlock_detection_log_save();                   \
        }else                                                            \
        {                                                                \
            CcspTraceError(("CCSP_DEADLOCK_DETECTION_EXIT -- error call. last detection doesn't exit(%s)\n", deadlock_detection_info.messageType)); \
            syslog(LOG_ERR, "CCSP_DEADLOCK_DETECTION_EXIT -- error call. last detection doesn't exit(%s)\n", deadlock_detection_info.messageType); \
        }                                                                \
    }                                                                    \

#define CCSP_DEADLOCK_DETECTION_EXIT(messageType1)                        \
    if ( deadlock_detection_enable )                                     \
    {                                                                    \
        if ( strstr(deadlock_detection_info.messageType, messageType1 ))  \
        {                                                                 \
            pthread_mutex_lock(&(deadlock_detection_info.info_mutex));    \
            deadlock_detection_info.messageType   = NULL;               \
            deadlock_detection_info.parameterInfo = NULL;               \
            deadlock_detection_info.size          = 0;                  \
            deadlock_detection_info.enterTime     = 0;                  \
            deadlock_detection_info.detectionDuration = 0;              \
            pthread_mutex_unlock(&(deadlock_detection_info.info_mutex));  \
        }else                                                            \
        {                                                                \
            CcspTraceError(("CCSP_DEADLOCK_DETECTION_EXIT -- error call. it's not current message type(%s)\n", deadlock_detection_info.messageType)); \
            syslog(LOG_ERR, "CCSP_DEADLOCK_DETECTION_EXIT -- error call. it's not current message type(%s)\n", deadlock_detection_info.messageType); \
        }                                                                \
    }                                                                    \
    
/* These two macroes are used to hide complex log system of deadlock detection */
#define  CCSP_DEADLOCK_PRINT_INTERNAL(args...)       \
    do {                                            \
        snprintf(oneline, 1023, args);            \
    }while(0);                                  \

#define CCSP_DEADLOCK_PRINT(aaa)        \
    {                                 \
        char oneline[1024] = {0};     \
        CCSP_DEADLOCK_PRINT_INTERNAL aaa; \
        CcspTraceError(("%s", oneline));  \
        syslog(LOG_ERR, oneline); \
        if ( fd ) fputs( oneline, fd ); \
    }   \


#define CCSP_DEADLOCK_INFO_PRINT(args...)     \
{                                             \
    snprintf( (*deadlock_detection_log)[index], deadlock_detection_log_linelen-1, args);  \
    if ( ++index >= deadlock_detection_log_linenum ) \
        index = 0;                                   \
}                                                    \


void CcspBaseIf_SetCallback
(
    void* bus_handle,
    CCSP_Base_Func_CB*  func
)
{
    CCSP_MESSAGE_BUS_INFO *bus_info = (CCSP_MESSAGE_BUS_INFO *)bus_handle;
    if(!bus_info->CcspBaseIf_func)
    	bus_info->CcspBaseIf_func = bus_info->mallocfunc(sizeof(CCSP_Base_Func_CB));
    *((CCSP_Base_Func_CB*)(bus_info->CcspBaseIf_func)) = *func;
}

void CcspBaseIf_SetCallback2
(
    void* bus_handle,
    char *name,
    void*  func,
    void * user_data
)
{
    CCSP_MESSAGE_BUS_INFO *bus_info = (CCSP_MESSAGE_BUS_INFO *)bus_handle;
    if(!bus_info->CcspBaseIf_func)
    {
	    bus_info->CcspBaseIf_func = bus_info->mallocfunc(sizeof(CCSP_Base_Func_CB));
	    memset(bus_info->CcspBaseIf_func, 0, sizeof(CCSP_Base_Func_CB));
    }   
    CCSP_Base_Func_CB* cb = (CCSP_Base_Func_CB*)bus_info->CcspBaseIf_func;
    if(!strcmp("setParameterValues", name))
    {
	    cb->setParameterValues = func;
	    cb->setParameterValues_data = user_data;
    }   
       
    else if(!strcmp("setCommit", name))
    {
	    cb->setCommit = func;
	    cb->setCommit_data = user_data;
    }   
       
    else if(!strcmp("getParameterValues", name))
    {
        cb->getParameterValues = func;
	    cb->getParameterValues_data = user_data;
    }   
       
    else if(!strcmp("setParameterAttributes", name))
    {
       cb->setParameterAttributes = func;
       cb->setParameterAttributes_data = user_data;
    }   
       
    else if(!strcmp("getParameterAttributes", name))
    {
       cb->getParameterAttributes = func;
	   cb->getParameterAttributes_data = user_data;
    }   
       
    else if(!strcmp("AddTblRow", name))
    {
       cb->AddTblRow = func;
	   cb->AddTblRow_data = user_data;
    }   
       
    else if(!strcmp("DeleteTblRow", name))
    {
       cb->DeleteTblRow = func;
	   cb->DeleteTblRow_data = user_data;
    }   
       
    else if(!strcmp("getParameterNames", name))
    {
       cb->getParameterNames = func;
	   cb->getParameterNames_data = user_data;
    }   
       
    else if(!strcmp("freeResources", name))
    {
       cb->freeResources = func;
	   cb->freeResources_data = user_data;
    }   
       
    else if(!strcmp("busCheck", name))
    {
       cb->busCheck = func;
	   cb->busCheck_data = user_data;
    }   
       
    else if(!strcmp("initialize", name))
    {
       cb->initialize = func;
	   cb->initialize_data = user_data;
    }   
       
    else if(!strcmp("finalize", name))
    {
       cb->finalize = func;
	   cb->finalize_data = user_data;
    }   
       
    else if(!strcmp("componentStartDie", name))
    {
        cb->componentDie = func;
	    cb->componentDie_data = user_data;
    }   
       
    else if(!strcmp("parameterValueChangeSignal", name))
    {
       cb->parameterValueChangeSignal = func;
	   cb->parameterValueChangeSignal_data = user_data;
    }   
       
    else if(!strcmp("deviceProfileChangeSignal", name))
    {
        cb->deviceProfileChangeSignal = func;
	    cb->deviceProfileChangeSignal_data = user_data;
    }   
       
    else if(!strcmp("currentSessionIDSignal", name))
    {
        cb->currentSessionIDSignal = func;
	    cb->currentSessionIDSignal_data = user_data;
    }   
      
    else if(!strncmp("telemetryDataSignal", name, MAX_EVENT_NAME_LEN))
    {
        cb->telemetryDataSignal = func;
        cb->telemetryDataSignal_data = user_data;
    }
 
    else if(!strcmp("diagCompleteSignal", name))
    {
        cb->diagCompleteSignal = func;
	    cb->diagCompleteSignal_data = user_data;
    }   
       
    else if(!strcmp("systemReadySignal", name))
    {
        cb->systemReadySignal = func;
	    cb->systemReadySignal_data = user_data;
    }

    else if(!strcmp("systemRebootSignal", name))
    {
        cb->systemRebootSignal = func;
	    cb->systemRebootSignal_data = user_data;
    }
    else if(!strcmp(STBSERVICE_CDL_DLC_SIGNAL, name))
    {
        cb->dlCompleteSignal = func;
        cb->dlCompleteSignal_data = user_data;
    }
    else if(!strcmp("webconfigSignal", name))
    {
        cb->webconfigSignal = func;
        cb->webconfigSignal_data = user_data;
    }
        else if(!strcmp("multiCompBroadCastSignal", name))
        {
            cb->multiCompBroadCastSignal = func;
            cb->multiCompBroadCastSignal_data = user_data;
        }

        else if(!strcmp("multiCompMasterProcessSignal", name))
        {
            cb->multiCompMasterProcessSignal = func;
            cb->multiCompMasterProcessSignal_data = user_data;
        }
        
        else if(!strcmp("multiCompSlaveProcessSignal", name))
        {
            cb->multiCompSlaveProcessSignal = func;
            cb->multiCompSlaveProcessSignal_data = user_data;
        }
        else if(!strcmp("TunnelStatus", name))
        {
            cb->TunnelStatus = func;
            cb->TunnelStatus_data = user_data;
        }
        else if(!strcmp("WifiDbStatus", name))
        {
            cb->WifiDbStatus = func;
            cb->WifiDbStatus_data = user_data;
        }

}

#define CCSP_DBUS_LARGE_REPLY_SIZE_MIN 75000  // bytes
#define MAX_DBUS_PARAM_SIZE 50000

DBusHandlerResult
CcspBaseIf_base_path_message_func (DBusConnection  *conn,
                                   DBusMessage     *message,
                                   DBusMessage     *reply,
                                   const char *interface,
                                   const char *method,
                                   CCSP_MESSAGE_BUS_INFO *bus_info)
{
    UNREFERENCED_PARAMETER(conn);
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(reply);
    UNREFERENCED_PARAMETER(interface);
    UNREFERENCED_PARAMETER(method);
    UNREFERENCED_PARAMETER(bus_info);

    CcspTraceError(("came inside %s in rbus it should be handled in thread_path_message_func_rbus\n",__FUNCTION__));

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult
CcspBaseIf_evt_callback (DBusConnection  *conn,
              DBusMessage     *message,
              void            *user_data)
{
    UNREFERENCED_PARAMETER(conn);
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(user_data);

    CcspTraceInfo(("came inside %s in rbus it will be handled in CcspBaseIf_evt_callback_rbus\n",__FUNCTION__));

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* Handle a value change event from an rbus 2.0 api component
 * As we migrate components from Ccsp to rbus2, more and will be based on rbus2
 */
int handleValueChangeEvent_rbus(const char * object_name, const char * event_name, rbusMessage message, void * user_data)
{
    UNREFERENCED_PARAMETER(object_name);
    CCSP_MESSAGE_BUS_INFO *bus_info =(CCSP_MESSAGE_BUS_INFO *) user_data;
    CCSP_Base_Func_CB* func = (CCSP_Base_Func_CB* )bus_info->CcspBaseIf_func;
    char const* msg_event_name;
    int msg_event_type;
    char const* msg_obj_name;
    int msg_obj_type;
    int msg_num_props;
    char const* msg_prop1_name;
    int msg_prop1_type;
    uint32_t msg_prop1_len;
    uint8_t const* msg_prop1_data;

    /*get data from the message the specific way rbus 2.0 packs it*/
    rbusMessage_GetString(message, (char const**) &msg_event_name);
    rbusMessage_GetInt32(message, (int*) &msg_event_type);
    rbusMessage_GetString(message, &msg_obj_name);
    rbusMessage_GetInt32(message, &msg_obj_type);
    rbusMessage_GetInt32(message, (int*) &msg_num_props);
    rbusMessage_GetString(message, (char const**) &msg_prop1_name);
    rbusMessage_GetInt32(message, (int*) &msg_prop1_type);
    rbusMessage_GetBytes(message, &msg_prop1_data, &msg_prop1_len);

    if(!strcmp(event_name, "Device.CR.SystemReady") && func->systemReadySignal)
    {
        /*expect to get a 1 bytes boolean*/
        if(msg_prop1_type == RBUS_BOOLEAN && msg_prop1_len == 1)
        {
            if(msg_prop1_data[0])
            {
                CcspTraceInfo(("System Ready Event Received\n"));

                func->systemReadySignal(func->systemReadySignal_data);
            }
            else
            {
                CcspTraceInfo(("%s %s unexpected value %d\n", __FUNCTION__, event_name, (int)msg_prop1_data[0]));
            }
        }
        else
        {
            CcspTraceError(("%s %s unexpected type %d\n", __FUNCTION__, event_name, msg_prop1_type));
        }
    }
    return RBUSCORE_SUCCESS;
}

int CcspBaseIf_evt_callback_rbus(const char * object_name, const char * event_name, rbusMessage message, void * user_data)
{
    UNREFERENCED_PARAMETER(object_name);
    CCSP_MESSAGE_BUS_INFO *bus_info =(CCSP_MESSAGE_BUS_INFO *) user_data;
    CCSP_Base_Func_CB* func = (CCSP_Base_Func_CB* )bus_info->CcspBaseIf_func;

    if(!strcmp(event_name,"parameterValueChangeSignal") && func->parameterValueChangeSignal)
    {
        parameterSigStruct_t *val = 0;
        int param_size = 0, i = 0;

        rbusMessage_GetInt32(message, (int32_t*)&param_size);

        if(param_size)
        {
            val = bus_info->mallocfunc(param_size*sizeof(parameterSigStruct_t ));
            memset(val, 0, param_size*sizeof(parameterSigStruct_t ));
        }

        for(i = 0; i < param_size; i++)
        {
            rbusMessage_GetString(message, &val[i].parameterName);
            rbusMessage_GetString(message, (const char**)&val[i].oldValue);
            rbusMessage_GetString(message, (const char**)&val[i].newValue);
            rbusMessage_GetInt32(message, (int32_t*)&val[i].type);
            rbusMessage_GetString(message, &val[i].subsystem_prefix);
            rbusMessage_GetInt32(message, (int32_t*)&val[i].writeID);
        }

        func->parameterValueChangeSignal(val, param_size,func->parameterValueChangeSignal_data);

        bus_info->freefunc(val);
    }
    else if(!strcmp(event_name,"deviceProfileChangeSignal") && func->deviceProfileChangeSignal)
    {
        int32_t isAvailable;
        char *component_name = 0;
        char *component_dbus_path = 0;
        rbusMessage_GetString(message, (const char**)&component_name);
        rbusMessage_GetString(message, (const char**)&component_dbus_path);
        rbusMessage_GetInt32(message, &isAvailable);
        func->deviceProfileChangeSignal(component_name,component_dbus_path,isAvailable,func->deviceProfileChangeSignal_data);
    }

    else if(!strcmp(event_name,"currentSessionIDSignal") && func->currentSessionIDSignal)
    {
        int32_t sessionID;
        int32_t priority;
        rbusMessage_GetInt32(message, &priority);
        rbusMessage_GetInt32(message, &sessionID);
        func->currentSessionIDSignal(priority, sessionID, func->currentSessionIDSignal_data);
    }
    else if(!strcmp(event_name,"diagCompleteSignal") && func->diagCompleteSignal)
    {
        func->diagCompleteSignal(func->diagCompleteSignal_data);
    }
    else if(!strcmp(event_name, "Device.CR.SystemReady") && func->systemReadySignal)
    {
        handleValueChangeEvent_rbus(object_name, event_name, message, user_data);
    }
    else if(!strcmp(event_name,"systemRebootSignal") && func->systemRebootSignal)
    {
        func->systemRebootSignal(func->systemRebootSignal_data);
    }
    else
    {
        CcspTraceError(("%s %s event not handled\n",__FUNCTION__, event_name));
        return RBUSCORE_ERROR_UNSUPPORTED_EVENT;
    }

    return RBUSCORE_SUCCESS;
}

void  CcspBaseIf_Set_Default_Event_Callback
(
    void* bus_handle,
    DBusObjectPathMessageFunction   callback
)
{
    CCSP_MESSAGE_BUS_INFO *bus_info = (CCSP_MESSAGE_BUS_INFO *)bus_handle;
    bus_info->default_sig_callback = callback;
    
}

/*
    This function is used to save dbus call in a round queue.
*/
void CcspBaseIf_deadlock_detection_log_save 
( 
    void
)
{
    CCSP_DEADLOCK_DETECTION_INFO* info           = (CCSP_DEADLOCK_DETECTION_INFO*)(&deadlock_detection_info);
    parameterValStruct_t *        parameterVal   = 0;     //setParameterValues
    char *                        parameterName  = 0;     //getParameterNames
    char **                       parameterNames = 0;     //getParameterValues / getParameterAttributes
    parameterAttributeStruct_t  * parameterAttribute = 0; //setParameterAttributes
    char *                        str            = NULL;  //AddTblRow / DeleteTblRow
    unsigned long                 size           = 0;
    unsigned long                 index          = deadlock_detection_log_index;
    unsigned int                  i              = 0;
    char                          timestr[128]   = {0};
    time_t                        t1             = GetCurrentTime();
    
    if ( !deadlock_detection_enable )
        return;

    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&t1));   
    
    pthread_mutex_lock(&(info->info_mutex));

    if ( info->messageType )
    {
        size = info->size;
        if ( strstr("setParameterValues", info->messageType ) )
        {
            parameterVal = (parameterValStruct_t *)(info->parameterInfo);
            
            CCSP_DEADLOCK_INFO_PRINT("%s **Ccsp Dbus call:setParameterValues.\n", timestr);
            for ( i = 0; i<size; i++ )
            {
                CCSP_DEADLOCK_INFO_PRINT("parameters[%d] : %s/%s\n", i, parameterVal[i].parameterName, parameterVal[i].parameterValue);
            }
        }
        else if ( strstr("getParameterValues", info->messageType ))
        {
            parameterNames = (char **)(info->parameterInfo);

            CCSP_DEADLOCK_INFO_PRINT("%s **Ccsp Dbus call:getParameterValues.\n", timestr);
            for ( i = 0; i<size; i++ )
            {
                CCSP_DEADLOCK_INFO_PRINT("parameters[%d] : %s\n", i, parameterNames[i]);            
            }
        }
        else if ( strstr("getParameterNames", info->messageType ))
        {
            parameterName = (char *)(info->parameterInfo);

            CCSP_DEADLOCK_INFO_PRINT("%s **Ccsp Dbus call:getParameterNames.\n", timestr);
            CCSP_DEADLOCK_INFO_PRINT("parameterName : %s\n", parameterName);            
        }
        else if ( strstr("getParameterAttributes", info->messageType ))
        {
            parameterNames = (char **)(info->parameterInfo);

            CCSP_DEADLOCK_INFO_PRINT("%s **Ccsp Dbus call:getParameterAttributes.\n", timestr);
            for ( i = 0; i<size; i++ )
            {
                CCSP_DEADLOCK_INFO_PRINT("parameters[%d] : %s\n", i, parameterNames[i]);            
            }
        }
        else if ( strstr("setParameterAttributes", info->messageType ))
        {
            parameterAttribute = (parameterAttributeStruct_t *)(info->parameterInfo);

            CCSP_DEADLOCK_INFO_PRINT("%s **Ccsp Dbus call:setParameterAttributes.\n", timestr);
            for ( i = 0; i<size; i++ )
            {
                CCSP_DEADLOCK_INFO_PRINT("parameters[%d] : %s, NtfChg:%d,Ntf:%d,Acc:%u,AcCtlChg:%d,AcCtlBitm:%u\n", \
                                     i, \
                                     parameterAttribute[i].parameterName,\
                                     parameterAttribute[i].notificationChanged,\
                                     parameterAttribute[i].notification,\
                                     parameterAttribute[i].access,\
                                     parameterAttribute[i].accessControlChanged,\
                                     parameterAttribute[i].accessControlBitmask);
            }
        }
        else if ( strstr("AddTblRow", info->messageType ))
        {
            str = (char * )(info->parameterInfo);
            CCSP_DEADLOCK_INFO_PRINT("%s **Ccsp Dbus call:AddTblRow.\n", timestr);
            CCSP_DEADLOCK_INFO_PRINT("row: %s\n", str);         
        }
        else if ( strstr("DeleteTblRow", info->messageType ))
        {
            str = (char * )(info->parameterInfo);
            CCSP_DEADLOCK_INFO_PRINT("%s **Ccsp Dbus call:DeleteTblRow.\n", timestr);
            CCSP_DEADLOCK_INFO_PRINT("row: %s\n", str);
        }
        else if ( strstr("setCommit", info->messageType ))
        {
            CCSP_DEADLOCK_INFO_PRINT("%s **Ccsp Dbus call:setCommit.\n", timestr);
        }

    }

    deadlock_detection_log_index = index;
    
    pthread_mutex_unlock(&(info->info_mutex));

    return;
}

/*
    This function is used to trigger log cache print.
    When some signals are received, call this funtion.
    When deadlock happened, call this function.
*/
void
CcspBaseIf_deadlock_detection_log_print
(
    int sig
)
{
    CCSP_DEADLOCK_DETECTION_INFO* info           = (CCSP_DEADLOCK_DETECTION_INFO*)(&deadlock_detection_info);
    FILE *                        fd             = NULL;
    int                           i              = 0;
    char                          timestr[128]   = {0};
    time_t                        t1             = GetCurrentTime();
    
    if ( !deadlock_detection_enable )
        return;

    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&t1));   


    pthread_mutex_lock(&(info->info_mutex));

    fd = fopen( deadlock_detection_log_file, "a+" );
    if(fd) /*RDKB-6233, CID-32959 , null check before use*/
    {
        fseek(fd,0L,SEEK_END);

        if ( ftell(fd) > 10000)
        {
            fclose(fd);
            fd = fopen( deadlock_detection_log_file, "w+" );
        }
    }

    CCSP_DEADLOCK_PRINT(("\n%s CCSP Dbus Call stack trace printing  --  ************************************\n", timestr))

    /*write previous logs */
    i = deadlock_detection_log_index;
    do
    {
        if ( (*deadlock_detection_log)[i][0] )
            CCSP_DEADLOCK_PRINT(((*deadlock_detection_log)[i]));

        i++;

        if ( i >= deadlock_detection_log_linenum )
            i = 0;

        if ( (unsigned int)i == deadlock_detection_log_index )
            break;
    }while(1);

    /* When signal trigger this function. Print the following */
    if ( sig == SIGINT ) {
    	CCSP_DEADLOCK_PRINT(("%s **SIGINT received! exiting now !!!!!!!!  The last accessing call is above.\n", timestr));
    }
    else if ( sig == SIGUSR1 ) {
    	CCSP_DEADLOCK_PRINT(("%s **SIGUSR1 received! The last accessing call is last one.\n", timestr));
    }
    else if ( sig == SIGUSR2 ) {
    	CCSP_DEADLOCK_PRINT(("%s **SIGUSR2 received! The last accessing call is last one.\n", timestr));
    }
    else if ( sig == SIGCHLD ) {
    	CCSP_DEADLOCK_PRINT(("%s **SIGCHLD received! The last accessing call is last one.\n", timestr));
    }
    else if ( sig == SIGPIPE ) {
    	CCSP_DEADLOCK_PRINT(("%s **SIGPIPE received! The last accessing call is last one.\n", timestr));
    }
    else if ( sig == SIGTERM )
    {
        CCSP_DEADLOCK_PRINT(("%s **SIGTERM received! exiting now !!!!!!!!  The last accessing call is last one.\n", timestr));
    }
    else if ( sig == SIGKILL )
    {
        CCSP_DEADLOCK_PRINT(("%s **SIGKILL received! exiting now !!!!!!!!  The last accessing call is last one.\n", timestr));
    }
    else if ( sig != 0 ) {
    	CCSP_DEADLOCK_PRINT(("%s **Signal %d received! exiting now !!!!!!!!  The last accessing call is last one.\n", timestr, sig));
    }
    
    if ( fd ) 
        fclose(fd);

    pthread_mutex_unlock(&(info->info_mutex));

    return;
    
}


void sig_empty_handler(int sig)
{
    UNREFERENCED_PARAMETER(sig);
    return;
}

/*
    Create a thread to monitor dbus call. 
    When deadlock happened, trigger to print trace stack
*/
void *
CcspBaseIf_Deadlock_Detection_Thread
(
    void * user_data
)
{
    UNREFERENCED_PARAMETER(user_data);
    CCSP_DEADLOCK_DETECTION_INFO* info           = (CCSP_DEADLOCK_DETECTION_INFO*)(&deadlock_detection_info);
    FILE *                        fd             = NULL;

    struct timespec               time1          = {0};
    unsigned long                 currentTime    = 0;
    unsigned long                 deadLockHappen = 0;
    
    time1.tv_sec = 3;
    time1.tv_nsec = 0;

    while( deadlock_detection_enable )
    {
        if ( info->messageType )
        {
            pthread_mutex_lock(&(info->info_mutex));

            currentTime = GetCurrentTime();
            if ( (currentTime - info->enterTime) >= info->detectionDuration )
            {
               // (2*(info->detectionDuration)) This check is just for checking invalid time difference  
                if (( currentTime - info->enterTime ) > (2*(info->detectionDuration))) 
                {
                    //This is invalid time difference. Just neglect
                    info->enterTime = currentTime - info->timepassed ;
                    CcspTraceWarning((" **** info->timepassed %lu ******\n",info->timepassed));
                    CcspTraceWarning((" **** info->enterTime %lu ******\n",info->enterTime));
                    CcspTraceWarning((" **** currentTime %lu ******\n",currentTime));
                }
                else
                {
                    deadLockHappen = 1;
                }  
            }
            
            pthread_mutex_unlock(&(info->info_mutex));
        }

        if (deadLockHappen)
        {
            break;
        }
    
        nanosleep(&time1, NULL);
    }

    if (deadLockHappen)
    {  
        char                          timestr[128]   = {0};
        time_t                        t1             = GetCurrentTime();
        
        strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&t1));   
    
        CcspBaseIf_deadlock_detection_log_print(0);

        pthread_mutex_lock(&(info->info_mutex));
        
        fd = fopen( deadlock_detection_log_file, "a+" );

        if(fd) /*RDKB-6233, CID-33441, null check before use*/
        {
            if ( ftell(fd) > 10000) rewind(fd);
        }

        CCSP_DEADLOCK_PRINT(("%s **CCSP Deadlock happened. Exiting after(5 sec)!!!!!!!!     The last fail accessing call is last one.\n", timestr));

        time1.tv_sec = 5;
        nanosleep(&time1, NULL);

        if ( fd ) 
            fclose(fd);
        
        pthread_mutex_unlock(&(info->info_mutex));
        
        // TODO: When a memory access violation occurs, the empty signal handler simply returns and execution continues.
        // This immediately triggers another SIGSEGV and the cycle repeats endlessly.
        // The obvious workaround it to comment out these 4 lines. We are trying to terminate the process anyway,
        // so we should let the SIGSEGV crash the process instead of trying to recover.
        //signal(SIGSEGV, sig_empty_handler);
        //signal(SIGBUS, sig_empty_handler);
        //signal(SIGFPE, sig_empty_handler);
        //signal(SIGILL, sig_empty_handler);

        
        exit(-1);
    }

    
    return NULL;
}

