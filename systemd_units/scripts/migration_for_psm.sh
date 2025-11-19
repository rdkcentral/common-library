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

##########################################################################
# Script Description:
# -------------------
# This script handles PSM (Persistent Storage Manager) configuration 
# migration for RDK-B devices during system boot.
#
# Purpose:
# --------
# Validates and synchronizes PSM configuration entries between the 
# default configuration (bbhm_def_cfg.xml) and current configuration 
# (bbhm_cur_cfg.xml) to ensure consistency across firmware updates 
# or configuration changes.
#
# Key Actions:
# ------------
# 1. Checks reboot reason - skips migration if factory-reset detected
# 2. Validates presence of both default and current PSM config files
# 3. Compares entry counts for critical subsystem parameters:
#    - WAN Manager interface count
#    - WAN Manager group count
#    - DHCP Manager v4 client count
#    - DHCP Manager v6 client count
# 4. If count mismatch detected:
#    - Creates temporary working copy of default config
#    - Removes mismatched entry from the working copy
#    - Replaces current config with corrected version
#    - Cleans up temporary files
# 5. Logs all operations with PSM_MIGRATION prefix for debugging
#
# Exit Conditions:
# ----------------
# - Skips migration on factory-reset
# - Exits if config files are missing
# - Exits on file operation failures (copy/sed)
#
# Future Extensibility:
# ---------------------
# This script is designed to be easily extended for additional PSM 
# entries. To add new PSM entry migration:
# 1. Call migrate_psm_entry() function with appropriate parameters:
#    migrate_psm_entry "<psm.entry.path>" "<Description>"
# 2. Add the call in the main execution section (after existing entries)
# 3. Example: migrate_psm_entry "dmsb.component.parameterCount" "Component Parameter"
#
# The migrate_psm_entry() function is reusable and handles all validation,
# comparison, and migration logic automatically for any PSM entry.
#
# Dependencies:
# -------------
# - /etc/utopia/service.d/log_capture_path.sh (logging functions)
# - /etc/device.properties (device-specific properties)
# - syscfg utility (reading system configuration)
##########################################################################

source /etc/utopia/service.d/log_capture_path.sh
source /etc/device.properties

SYSCFG_TMP_LOCATION=/tmp
PSM_CUR_XML_CONFIG_FILE_NAME="$SYSCFG_TMP_LOCATION/bbhm_cur_cfg.xml"
PSM_DEF_XML_CONFIG_FILE_NAME="/usr/ccsp/config/bbhm_def_cfg.xml"
PSM_DUP_CUR_XML_CONFIG_FILE_NAME="$SYSCFG_TMP_LOCATION/.bbhm_dup_cur_cfg.xml"

rebootReason=`syscfg get X_RDKCENTRAL-COM_LastRebootReason`

migrate_psm_entry() {
    local entry_name="$1"
    local description="$2"

    local cur_count=$(sed -n "/$entry_name/p" "$PSM_CUR_XML_CONFIG_FILE_NAME" | awk -F"[><]" '{print $3}')
    if [ "$cur_count" != "" ]; then
        local def_count=$(sed -n "/$entry_name/p" "$PSM_DEF_XML_CONFIG_FILE_NAME" | awk -F"[><]" '{print $3}')
        if [ "$def_count" != "" ]; then
            echo_t "PSM_MIGRATION: No. of $description from $PSM_DEF_XML_CONFIG_FILE_NAME: $def_count"
            echo_t "PSM_MIGRATION: No. of $description from $PSM_CUR_XML_CONFIG_FILE_NAME: $cur_count"
            if [ "$cur_count" != "$def_count" ]; then
                #copy the default config to tmp location
                cp "$PSM_DEF_XML_CONFIG_FILE_NAME" "$PSM_DUP_CUR_XML_CONFIG_FILE_NAME"
                status=$?    # capture cp exit status
                if [ $status -ne 0 ]; then
                    echo_t "PSM_MIGRATION: Failed to copy $PSM_DEF_XML_CONFIG_FILE_NAME to $PSM_DUP_CUR_XML_CONFIG_FILE_NAME, cp exit status: $status"
                    return 0
                fi
                
                #delete the entry from the tmp duplicate config file
                sed -i "/$entry_name/d" "$PSM_DUP_CUR_XML_CONFIG_FILE_NAME"
                status=$?    # capture sed exit status
                if [ $status -ne 0 ]; then
                    echo_t "PSM_MIGRATION: Failed to delete $entry_name entry from $PSM_DUP_CUR_XML_CONFIG_FILE_NAME, sed exit status: $status"
                    return 0
                fi

                #copy the modified tmp config back to current config
                cp "$PSM_DUP_CUR_XML_CONFIG_FILE_NAME" "$PSM_CUR_XML_CONFIG_FILE_NAME"
                status=$?    # capture cp exit status
                if [ $status -ne 0 ]; then
                    echo_t "PSM_MIGRATION: Failed to copy $PSM_DUP_CUR_XML_CONFIG_FILE_NAME to $PSM_CUR_XML_CONFIG_FILE_NAME, cp exit status: $status"
                    return 0
                fi

                echo_t "PSM_MIGRATION: $description mismatched so deleting this $entry_name entry from $PSM_CUR_XML_CONFIG_FILE_NAME"
            else
                echo_t "PSM_MIGRATION: $description is same so no migration required"
            fi
        fi
    fi
}

echo_t "PSM_MIGRATION: **********************<Entry> Migration Handling for PSM ******************************"
echo_t "PSM_MIGRATION: Reboot Reason:<$rebootReason>"

if [ "$rebootReason" != "factory-reset" ]; then
    if [ ! -f "$PSM_DEF_XML_CONFIG_FILE_NAME" ]; then
       echo_t "PSM_MIGRATION: Default PSM config file not found, skipping migration"
       exit 0
    fi
    if [ ! -f "$PSM_CUR_XML_CONFIG_FILE_NAME" ]; then
       echo_t "PSM_MIGRATION: Current PSM config file not found, skipping migration"
       exit 0
    fi

    migrate_psm_entry "dmsb.wanmanager.wan.interfacecount" "WAN Interface"
    migrate_psm_entry "dmsb.wanmanager.group.Count" "WAN Group"
    migrate_psm_entry "dmsb.dhcpmanager.ClientNoOfEntries" "DHCP MGR v4 client count"
    migrate_psm_entry "dmsb.dhcpmanager.dhcpv6.ClientNoOfEntries" "DHCP MGR v6 client count"

    #remove the tmp duplicate file
    if [ -f "$PSM_DUP_CUR_XML_CONFIG_FILE_NAME" ]; then
        rm -f "$PSM_DUP_CUR_XML_CONFIG_FILE_NAME"
    fi
fi
echo_t "PSM_MIGRATION: **********************<Exit> Migration Handling for PSM ******************************"
