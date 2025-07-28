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

#ifndef __CCSP_SUBSCRIPTION_H
#define __CCSP_SUBSCRIPTION_H

#include <stdint.h>

typedef struct _rbusFilter* rbusFilter_t;

#ifdef __cplusplus
extern "C" {
#endif
typedef struct _Subscription
{
    char* listener;
    char* eventName;
    int32_t componentId;
    rbusFilter_t filter;
    int32_t interval;
    int32_t duration;
} ccspRbusSubscription_t;

void Ccsp_RbusSubscriptions_saveCache(const char* componentName);
int Ccsp_RbusSubscriptions_loadCache(const char* componentName);

int Ccsp_RbusSubscriptions_create(const char* componentName);
void Ccsp_RbusSubscriptions_destory(const char* componentName);
void Ccsp_RbusSubscriptionCacheList_destory();

int ccsp_rbus_event_subscribe_override_handler(char const* object,  char const* eventName, char const* listener, int added, int componentId, int interval, int duration, rbusFilter_t filter, void* userData);
ccspRbusSubscription_t* Ccsp_RbusSubscriptions_getSubscription(const char* listener, char const* eventName, int32_t componentId, rbusFilter_t filter, int32_t interval, int32_t duration);
ccspRbusSubscription_t* Ccsp_RbusSubscriptions_addSubscription(const char* listener, const char* eventName, int32_t componentId, int32_t interval, int32_t duration, rbusFilter_t filter);
void Ccsp_RbusSubscriptions_removeSubscription(ccspRbusSubscription_t *sub);
void Ccsp_RbusSubscriptions_resubscribeElementCache(void* bus_handle);
#ifdef __cplusplus
}
#endif
#endif
