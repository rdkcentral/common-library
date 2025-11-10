#!/bin/sh
##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2025 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

source /etc/utopia/service.d/log_capture_path.sh
source /etc/device.properties

SYSCFG_TMP_LOCATION=/tmp
PSM_CUR_XML_CONFIG_FILE_NAME="$SYSCFG_TMP_LOCATION/bbhm_cur_cfg.xml"
PSM_DEF_XML_CONFIG_FILE_NAME="/usr/ccsp/config/bbhm_def_cfg.xml"

rebootReason=`syscfg get X_RDKCENTRAL-COM_LastRebootReason`

migrate_psm_entry() {
    local entry_name="$1"
    local description="$2"

    local cur_count=$(sed -n "/$entry_name/p" "$PSM_CUR_XML_CONFIG_FILE_NAME" | awk -F"[><]" '{print $3}')
    if [ "$cur_count" != "" ]; then
        local def_count=$(sed -n "/$entry_name/p" "$PSM_DEF_XML_CONFIG_FILE_NAME" | awk -F"[><]" '{print $3}')
        if [ "$def_count" != "" ]; then
            echo_t "No. of $description from $PSM_DEF_XML_CONFIG_FILE_NAME: $def_count"
            echo_t "No. of $description from $PSM_CUR_XML_CONFIG_FILE_NAME: $cur_count"
            if [ "$cur_count" != "$def_count" ]; then
                sed -i "/$entry_name/d" "$PSM_CUR_XML_CONFIG_FILE_NAME"
                echo_t "$description mismatched so deleting this $entry_name entry from $PSM_CUR_XML_CONFIG_FILE_NAME"
            else
                echo_t "$description is same so no migration required"
            fi
        fi
    fi
}

echo_t "**********************<Entry> Migration Handling for PSM ******************************"
echo_t "Reboot Reason:<$rebootReason>"

if [ "$rebootReason" != "factory-reset" ]; then
    migrate_psm_entry "dmsb.wanmanager.wan.interfacecount" "WAN Interface"
    migrate_psm_entry "dmsb.wanmanager.group.Count" "WAN Group"
    migrate_psm_entry "dmsb.dhcpmanager.ClientNoOfEntries" "DHCP MGR v4 client count"
    migrate_psm_entry "dmsb.dhcpmanager.dhcpv6.ClientNoOfEntries" "DHCP MGR v6 client count"
fi

echo_t "**********************<Exit> Migration Handling for PSM ******************************"
