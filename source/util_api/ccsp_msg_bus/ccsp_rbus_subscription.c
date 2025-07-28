/*
 * If not stated otherwise in this file or this component's Licenses.txt file
 * the following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
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
#define _GNU_SOURCE 1 //needed for pthread_mutexattr_settype
#include "rbus_message_bus.h"
#include "ccsp_base_api.h"
#include "ccsp_message_bus.h"
#include "ccsp_trace.h"
#include <rtmessage/rtVector.h>
#include <rbus/rbus.h>
#include <rbus/rbus_buffer.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include "ccsp_rbus_subscription.h"
#include "ccsp_rbus_value_change.h"
#include "ccsp_rbus_intervalsubscription.h"

#define CACHE_FILE_PATH_FORMAT "%s/ccsp_rbus_subs_%s"

static pthread_mutex_t  subrecord_mutex;
#define SUB_LOCK() {int rc = pthread_mutex_lock(&subrecord_mutex); (void)rc;}
#define SUB_UNLOCK() {pthread_mutex_unlock(&subrecord_mutex);}
extern void get_recursive_wildcard_parameterNames(void* bus_handle, char *parameterName, rbusMessage *req, int *param_size);
static rtVector subList = NULL;
static rtVector subCacheList = NULL;

int cssp_RbusSubscribe_handler(char const* object, char const* eventName, char const* listener, int added, int componentId, int interval, int duration, rbusFilter_t filter, void* bus_handle);
static pid_t ccsp_RbusSubscriptions_getListenerPid(char const* listener)
{
    pid_t pid;
    const char* p = listener + strlen(listener) - 1;
    while(*p != '.' && p != listener)
    {
        p--;
    }
    pid = atoi(p+1);
    if(pid == 0)
    {
        CcspTraceError(("pid not found in listener name %s", listener));
    }
    return pid;
}

static void ccsp_RbusSubscriptionFree(void* p)
{
    ccspRbusSubscription_t* sub = p;
    if(sub == NULL)
        return;
    free(sub->eventName);
    free(sub->listener);
    if(sub->filter)
        rbusFilter_Release(sub->filter);
    free(sub);
}

void Ccsp_RbusSubscriptions_saveCache(const char* componentName)
{
    FILE* file;
    rbusBuffer_t buff;
    char filePath[256];
    size_t i;
    snprintf(filePath, 256, CACHE_FILE_PATH_FORMAT, "/tmp", componentName);
    if (rtVector_Size(subList))
    {
        file = fopen(filePath, "wb");
        if(!file)
        {
            CcspTraceWarning(("failed to open %s", filePath));
            return;
        }
        rbusBuffer_Create(&buff);
        for(i = 0; i < rtVector_Size(subList); ++i)
        {
            ccspRbusSubscription_t* sub = (ccspRbusSubscription_t*)rtVector_At(subList, i);
            if(!sub)
            {
                rbusBuffer_Destroy(buff);
                fclose(file);
                return;
            }
            rbusBuffer_WriteStringTLV(buff, sub->listener, strlen(sub->listener)+1);
            rbusBuffer_WriteStringTLV(buff, sub->eventName, strlen(sub->eventName)+1);
            rbusBuffer_WriteInt32TLV(buff, sub->componentId);
            rbusBuffer_WriteInt32TLV(buff, sub->interval);
            rbusBuffer_WriteInt32TLV(buff, sub->duration);
            rbusBuffer_WriteInt32TLV(buff, sub->filter ? 1 : 0);
            if(sub->filter)
                rbusFilter_Encode(sub->filter, buff);
        }
        fwrite(buff->data, 1, buff->posWrite, file);
        rbusBuffer_Destroy(buff);
        fclose(file);
    }
    else
    {
      CcspTraceWarning(("no subs so removing file %s", filePath));
      if(remove(filePath) != 0)
          CcspTraceWarning(("failed to remove %s", filePath));
    }
}

int Ccsp_RbusSubscriptions_loadCache(const char* componentName)
{
    long size;
    uint16_t type, length;
    int32_t hasFilter;
    FILE* file = NULL;
    rbusBuffer_t buff = NULL;
    ccspRbusSubscription_t* sub = NULL;
    char filePath[256];
    bool needSave = false;

    snprintf(filePath, 256, CACHE_FILE_PATH_FORMAT, "/tmp", componentName);

    file = fopen(filePath, "rb");
    if(!file)
    {
        CcspTraceWarning(("failed to open file %s", filePath));
        goto remove_bad_file;
    }

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    if(size <= 0)
    {
        CcspTraceWarning(("file is empty %s", filePath));
        goto remove_bad_file;
    }

    rbusBuffer_Create(&buff);
    if(buff == NULL)
    {
        fclose(file);
        return 0;
    }
    rbusBuffer_Reserve(buff, size);

    fseek(file, 0, SEEK_SET);
    if(fread(buff->data, 1, size, file) != (size_t)size)
    {
        CcspTraceWarning(("failed to read entire file %s", filePath));
        goto remove_bad_file;
    }

    fclose(file);
    file = NULL;

    buff->posWrite += size;

    while(buff->posRead < buff->posWrite)
    {
        sub = (ccspRbusSubscription_t*)malloc(sizeof(ccspRbusSubscription_t));
        if(!sub)
        {
            CcspTraceWarning(("failed to malloc sub"));
            goto remove_bad_file;
        }
        //read listener
        if(rbusBuffer_ReadUInt16(buff, &type) < 0) goto remove_bad_file;
        if(rbusBuffer_ReadUInt16(buff, &length) < 0) goto remove_bad_file;
        if(type != RBUS_STRING || length >= RBUS_MAX_NAME_LENGTH) goto remove_bad_file;

        sub->listener = malloc(length);
        if(!sub->listener)
        {
            CcspTraceWarning(("failed to malloc %d bytes for listener", length));
            goto remove_bad_file;
        }
        memcpy(sub->listener, buff->data + buff->posRead, length);
        sub->listener[length-1] = '\0';
        buff->posRead += length;

        //read eventName
        if(rbusBuffer_ReadUInt16(buff, &type) < 0) goto remove_bad_file;
        if(rbusBuffer_ReadUInt16(buff, &length) < 0) goto remove_bad_file;
        if(type != RBUS_STRING || length >= RBUS_MAX_NAME_LENGTH) goto remove_bad_file;

        sub->eventName = malloc(length);
        if(!sub->eventName)
        {
            CcspTraceWarning(("failed to malloc %d bytes for eventName", length));
            goto remove_bad_file;
        }
        memcpy(sub->eventName, buff->data + buff->posRead, length);
        sub->eventName[length-1] = '\0';
        buff->posRead += length;

        //read componentId
        if(rbusBuffer_ReadUInt16(buff, &type) < 0) goto remove_bad_file;
        if(rbusBuffer_ReadUInt16(buff, &length) < 0) goto remove_bad_file;
        if(type != RBUS_INT32 && length != sizeof(int32_t)) goto remove_bad_file;
        if(rbusBuffer_ReadInt32(buff, &sub->componentId) < 0) goto remove_bad_file;

        //read interval
        if(rbusBuffer_ReadUInt16(buff, &type) < 0) goto remove_bad_file;
        if(rbusBuffer_ReadUInt16(buff, &length) < 0) goto remove_bad_file;
        if(type != RBUS_INT32 && length != sizeof(int32_t)) goto remove_bad_file;
        if(rbusBuffer_ReadInt32(buff, &sub->interval) < 0) goto remove_bad_file;

        //read duration
        if(rbusBuffer_ReadUInt16(buff, &type) < 0) goto remove_bad_file;
        if(rbusBuffer_ReadUInt16(buff, &length) < 0) goto remove_bad_file;
        if(type != RBUS_INT32 && length != sizeof(int32_t)) goto remove_bad_file;
        if(rbusBuffer_ReadInt32(buff, &sub->duration) < 0) goto remove_bad_file;

        //read hasFilter
        if(rbusBuffer_ReadUInt16(buff, &type) < 0) goto remove_bad_file;
        if(rbusBuffer_ReadUInt16(buff, &length) < 0) goto remove_bad_file;
        if(type != RBUS_INT32 && length != sizeof(int32_t)) goto remove_bad_file;
        if(rbusBuffer_ReadInt32(buff, &hasFilter) < 0) goto remove_bad_file;

        //read filter
        if(hasFilter)
        {
            if(rbusFilter_Decode(&sub->filter, buff) < 0) goto remove_bad_file;
        }
        else
        {
            sub->filter = NULL;
        }
        /*
            It's possible that we can load a sub from the cache for a listener whose process is no longer running.
            Example, this provider exited with active subscribers and thus still had those subs in its cache.
            Later those listener processes exit/crash.
            Then after that, this provider restarts and reads those now obsolete listeners. 
         */
        if (sub->listener)
        {
            if (!ccsp_RbusSubscriptions_getListenerPid(sub->listener))
            {
                CcspTraceWarning(("process no longer running for listener %s", sub->listener));
                ccsp_RbusSubscriptionFree(sub);
                needSave = true;
                continue;
            }
            else
            {
                char filename[RTMSG_HEADER_MAX_TOPIC_LENGTH];
                snprintf(filename, RTMSG_HEADER_MAX_TOPIC_LENGTH-1, "%s%d_%d", "/tmp/.rbus/",
                    ccsp_RbusSubscriptions_getListenerPid(sub->listener), sub->componentId);
                if(access(filename, F_OK) != 0)
                {
                    CcspTraceWarning(("file doesn't exist %s", filename));
                    ccsp_RbusSubscriptionFree(sub);
                    needSave = true;
                    continue;
                }
            }
        }
        if (subCacheList == NULL)
        {
            rtVector_Create(&subCacheList);
        }
        rtVector_PushBack(subCacheList, sub);
        CcspTraceDebug(("loaded %s %s", sub->listener, sub->eventName));
    }

    rbusBuffer_Destroy(buff);

    if(needSave)
        Ccsp_RbusSubscriptions_saveCache(componentName);

    return 1;

remove_bad_file:

    CcspTraceWarning(("removing corrupted file %s", filePath));

    if(file)
        fclose(file);

    if(buff)
        rbusBuffer_Destroy(buff);

    if(sub)
        ccsp_RbusSubscriptionFree(sub);

    if(remove(filePath) != 0)
        CcspTraceWarning(("failed to remove %s", filePath));
    return 0;
}

int Ccsp_RbusSubscriptions_create(const char* componentName)
{
    if (subList == NULL)
        rtVector_Create(&subList);
    return Ccsp_RbusSubscriptions_loadCache(componentName);
}

void Ccsp_RbusSubscriptionCacheList_destory()
{
    size_t i = 0;
    while(i < rtVector_Size(subCacheList))
    {
        ccspRbusSubscription_t *sub = (ccspRbusSubscription_t*)rtVector_At(subCacheList, i);
        if(sub)
        {
            rtVector_RemoveItem(subCacheList, sub, ccsp_RbusSubscriptionFree);
        }
        else
        {
           i++;
        }
    }

    if (rtVector_Size(subCacheList) == 0)
    {
        rtVector_Destroy(subCacheList, NULL);
        subCacheList = NULL;
    }
}

void Ccsp_RbusSubscriptions_destory(const char* componentName)
{
    SUB_LOCK()
    Ccsp_RbusSubscriptions_saveCache(componentName);
    size_t i = 0;
    while(i < rtVector_Size(subList))
    {
        ccspRbusSubscription_t *sub = (ccspRbusSubscription_t*)rtVector_At(subList, i);
        if(sub)
        {
            rtVector_RemoveItem(subList, sub, ccsp_RbusSubscriptionFree);
        }
        else
        {
           i++;
        }
    }

    if(rtVector_Size(subList) == 0)
    {
        rtVector_Destroy(subList, NULL);
        subList = NULL;
    }
    if (subCacheList != NULL)
        Ccsp_RbusSubscriptionCacheList_destory();

    SUB_UNLOCK()
}

ccspRbusSubscription_t*
Ccsp_RbusSubscriptions_getSubscription
(
    const char* listener,
    char const* eventName,
    int32_t componentId,
    rbusFilter_t filter,
    int32_t interval,
    int32_t duration
)
{
    size_t i;
    for(i = 0; i < rtVector_Size(subList); ++i)
    {
        ccspRbusSubscription_t *sub = (ccspRbusSubscription_t*)rtVector_At(subList, i);
        if( sub && (strcmp(sub->listener, listener) == 0) && (sub->componentId == componentId) &&
             (strcmp(sub->eventName, eventName) == 0) && (rbusFilter_Compare(sub->filter, filter) == 0) &&
                (sub->interval == interval) && (sub->duration == duration))
        {
            return sub;
        }
    }
    return NULL;
}

ccspRbusSubscription_t*
Ccsp_RbusSubscriptions_addSubscription
(
    const char* listener,
    const char* eventName,
    int32_t componentId,
    int32_t interval,
    int32_t duration,
    rbusFilter_t filter
)
{
    ccspRbusSubscription_t *sub = NULL;
    sub = (ccspRbusSubscription_t*)malloc(sizeof(ccspRbusSubscription_t));
    if (sub != NULL)
    {
        sub->listener = strdup(listener);
        sub->eventName = strdup(eventName);
        sub->componentId = componentId;
        sub->filter = filter;
        if(sub->filter)
            rbusFilter_Retain(sub->filter);
        sub->interval = interval;
        sub->duration = duration;
        rtVector_PushBack(subList, sub);
    }
    return sub;
}

/*remove an existing subscription*/
void Ccsp_RbusSubscriptions_removeSubscription(ccspRbusSubscription_t* sub)
{
    if (sub)
    {
        CcspTraceDebug(("%s: %s removing subscriber from listener %s componentId %d\n", __FUNCTION__, sub->eventName, sub->listener, sub->componentId));
        rtVector_RemoveItem(subList, sub, ccsp_RbusSubscriptionFree);
    }
}

/*
 *  Added to support rbus value-change detection
 *  This will check if eventName refers to a parameter (and not an event like CCSP_SYSTEM_READY_SIGNAL)
 *  If its a parameter it will assume value-change is desired and pass the data to Ccsp_RbusValueChange api to handle it
 */

int
cssp_RbusSubscribe_handler
(
    char const* object,
    char const* eventName,
    char const* listener,
    int added,
    int componentId,
    int interval,
    int duration,
    rbusFilter_t filter,
    void* bus_handle
)
{
    size_t slen;
    (void)object;
    ccspRbusSubscription_t *sub = NULL;
    CCSP_MESSAGE_BUS_INFO *bus_info = (CCSP_MESSAGE_BUS_INFO *)bus_handle;

    //determine if eventName is a parameter
    slen = strlen(eventName);
    if(slen < 7 || strncmp(eventName, "Device.", 7) != 0 || eventName[slen-1] == '!')
    {
        //not a parameter so return special error so rbus_core will search its registered event list for actual events like CCSP_SYSTEM_READY_SIGNAL
        CcspTraceError(("%s: ignored %s\n", __FUNCTION__, eventName));
        return RBUSCORE_ERROR_SUBSCRIBE_NOT_HANDLED;
    }

    /*verify bus handle is valid*/
    if( bus_info == NULL || bus_info->CcspBaseIf_func == NULL ||
            ((CCSP_Base_Func_CB* )(bus_info->CcspBaseIf_func))->getParameterValues == NULL)
    {
        CcspTraceError(("%s NULL bus info\n", __FUNCTION__));
        return CCSP_FAILURE;
    }

    if (interval && (duration != 0) && (duration < interval))
    {
        CcspTraceError(("Invalid parameter, the duration should be greater than the interval.\n"));
        return RBUSCORE_ERROR_INVALID_PARAM;
    }

    sub = Ccsp_RbusSubscriptions_getSubscription(listener, eventName, componentId, filter, interval, duration);

    if (sub && added)
    {
        CcspTraceError(("%s: ignoring duplicate subscribe for %s from listener %s componentId %d\n", __FUNCTION__,
                    sub->eventName, sub->listener, sub->componentId));
        return RBUS_ERROR_SUBSCRIPTION_ALREADY_EXIST;
    }

    if (added)
    {
        CcspTraceDebug(("%s: Add subscribe for %s from listener %s componentId %d\n", __FUNCTION__, eventName, listener, componentId));
        sub = Ccsp_RbusSubscriptions_addSubscription(listener, eventName, componentId, interval, duration, filter);
    }

    if (sub)
    {
        if (interval)
        {
            if (added)
                Ccsp_RbusInterval_Subscribe(bus_handle, listener, eventName, componentId, interval, duration, filter);
            else
                Ccsp_RbusInterval_Unsubscribe(bus_handle, listener, eventName, componentId, interval, duration, filter);
        }
        else
        {
            rbusMessage req;
            rbusMessage_Init(&req);
            int param_size = 0;
            /* Handling Wildcard subscription */
            char *tmpPtr = strstr (eventName, "*");
            if (tmpPtr)
            {
                char paramName[RBUS_MAX_NAME_LENGTH] = {0};
                snprintf(paramName, RBUS_MAX_NAME_LENGTH, "%s", (char *)eventName);
                get_recursive_wildcard_parameterNames(bus_info, paramName, &req, &param_size);
            }
            else
            {
                rbusMessage_SetString(req, eventName);
                param_size++;
            }
            for (int i = 0; i < param_size; i++)
            {
                char *parameter_name = 0;
                rbusMessage_GetString(req, (const char**)&parameter_name);
                if (added)
                {
                    Ccsp_RbusValueChange_Subscribe(bus_handle, listener, parameter_name, eventName, componentId, interval, duration, filter);
                }
                else
                {
                    Ccsp_RbusValueChange_Unsubscribe(bus_handle, listener, parameter_name, eventName, componentId, filter);
                }
            }

            if (!added)
            {
                CcspTraceDebug(("%s: Remove subscribe for %s from listener %s componentId %d\n", __FUNCTION__, eventName, listener, componentId));
                Ccsp_RbusSubscriptions_removeSubscription(sub);
            }
            rbusMessage_Release(req);
        }
        CcspTraceDebug(("%s:saveCache subscribe for %s from listener %s componentId %d\n", __FUNCTION__, eventName, listener, componentId));
        Ccsp_RbusSubscriptions_saveCache(bus_info->component_id);
    }
    return RBUS_ERROR_SUCCESS;
}

int ccsp_rbus_event_subscribe_override_handler
(
    char const* object,
    char const* eventName,
    char const* listener,
    int added,
    int componentId,
    int interval,
    int duration,
    rbusFilter_t filter,
    void* bus_handle
)
{
    SUB_LOCK()
    int err = cssp_RbusSubscribe_handler(object, eventName, listener, added, componentId, interval, duration, filter, bus_handle);
    SUB_UNLOCK()
    return err;
}

void Ccsp_RbusSubscriptions_resubscribeElementCache(void* bus_handle)
{
    SUB_LOCK()
    size_t i = 0;
    for(i = 0; i < rtVector_Size(subCacheList); ++i)
    {
        ccspRbusSubscription_t *sub = (ccspRbusSubscription_t*)rtVector_At(subCacheList, i);
        if(sub)
        {
            int err = cssp_RbusSubscribe_handler(NULL, sub->eventName, sub->listener, 1, sub->componentId, sub->interval, sub->duration, sub->filter, bus_handle);
            if (err == RBUS_ERROR_SUCCESS)
            {
                CcspTraceDebug(("%s:Resubscribed for %s from listener %s componentId %d\n", __FUNCTION__, sub->eventName, sub->listener, sub->componentId));
            }
        }
    }
    if (subCacheList != NULL)
        Ccsp_RbusSubscriptionCacheList_destory();

    SUB_UNLOCK()
}