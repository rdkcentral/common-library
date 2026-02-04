#!/bin/sh
##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2026 RDK Management. 
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

if [ -e /etc/log_timestamp.sh ]
then
    . /etc/log_timestamp.sh
else
    echo_t()
    {
	    echo "$(date +"%y%m%d-%T.%6N") $1"
    }
fi

LOG_FILE=/rdklogs/logs/Consolelog.txt.0
BBHM_DEF_CFG_FILE=/usr/ccsp/config/bbhm_def_cfg.xml

get_psm_value() {
    local param=$1

    echo_t "[get_psm_value] $param" >> $LOG_FILE
    # Check if the file exists
    if [ ! -f "$BBHM_DEF_CFG_FILE" ]; then
        echo_t "[get_psm_value] FILE NOT FOUND!" >> $LOG_FILE
        return 1
    fi

    # Search for the parameter and extract the value using grep and sed
    local value=$(grep "<Record name=\"$param\"" "$BBHM_DEF_CFG_FILE" | sed -n 's/.*<Record[^>]*>\([^<]*\)<\/Record>/\1/p')

    # If a result is found, return it otherwise return empty
    if [ -n "$value" ]; then
        echo "$value"
    else
        echo ""
    fi
}

echo_t "[DHCP Manager PSM Check] Running the script to check psm records" >> $LOG_FILE

wanifcount=$(psmcli get dmsb.wanmanager.wan.interfacecount)
if ! echo "$wanifcount" | grep -qE "^[0-9]+$"; then
    echo_t "[DHCP Manager PSM Check] Invalid interface count: '$wanifcount'" >> $LOG_FILE
    exit 1
fi

echo_t "[DHCP Manager PSM Check] Number of wan interface is ${wanifcount}" >> $LOG_FILE

cnt=1
while [ "$cnt" -le "$wanifcount" ]
do
    dhcp4_obj_path="dmsb.wanmanager.if.${cnt}.VirtualInterface.1.IP.DHCPV4Interface"
    dhcp6_obj_path="dmsb.wanmanager.if.${cnt}.VirtualInterface.1.IP.DHCPV6Interface"
    psmDHCPv4=$(psmcli get ${dhcp4_obj_path})
    psmDHCPv6=$(psmcli get ${dhcp6_obj_path})

    if [ -z "$psmDHCPv4" ]; then
        psmDHCPv4_value=$(get_psm_value "${dhcp4_obj_path}")
        if [ -z "$psmDHCPv4_value" ]; then
            echo_t "[DHCP Manager PSM Check] PSM value NOT found in "$BBHM_DEF_CFG_FILE" for record \"${dhcp4_obj_path}\"" >> $LOG_FILE
            psmcli set ${dhcp4_obj_path} Device.DHCPv4.Client.${cnt} 2> /dev/null
            echo_t "[DHCP Manager PSM Check] PSM record \"${dhcp4_obj_path}\" is set with \"Device.DHCPv4.Client.${cnt}\"" >> $LOG_FILE
        else
            echo_t "[DHCP Manager PSM Check] PSM value found in "$BBHM_DEF_CFG_FILE" for record \"${dhcp4_obj_path}\" is \"${psmDHCPv4_value}\"" >> $LOG_FILE
            psmcli set ${dhcp4_obj_path} ${psmDHCPv4_value} 2> /dev/null
            echo_t "[DHCP Manager PSM Check] PSM record \"${dhcp4_obj_path}\" is set with \"${psmDHCPv4_value}\"" >> $LOG_FILE
        fi
    else
        echo_t "[DHCP Manager PSM Check] PSM record \"${dhcp4_obj_path}\" already exists with value: \"${psmDHCPv4}\"" >> $LOG_FILE
    fi

    if [ -z "$psmDHCPv6" ]; then
        psmDHCPv6_value=$(get_psm_value "${dhcp6_obj_path}")
        if [ -z "$psmDHCPv6_value" ]; then
            echo_t "[DHCP Manager PSM Check] PSM value NOT found in "$BBHM_DEF_CFG_FILE" for record \"${dhcp6_obj_path}\"" >> $LOG_FILE
            psmcli set ${dhcp6_obj_path} Device.DHCPv6.Client.${cnt} 2> /dev/null
            echo_t "[DHCP Manager PSM Check] PSM record \"${dhcp6_obj_path}\" is set with \"Device.DHCPv6.Client.${cnt}\"" >> $LOG_FILE
        else
            echo_t "[DHCP Manager PSM Check] PSM value found in "$BBHM_DEF_CFG_FILE" for record \"${dhcp6_obj_path}\" is \"${psmDHCPv6_value}\"" >> $LOG_FILE
            psmcli set ${dhcp6_obj_path} ${psmDHCPv6_value} 2> /dev/null
            echo_t "[DHCP Manager PSM Check] PSM record \"${dhcp6_obj_path}\" is set with \"${psmDHCPv6_value}\"" >> $LOG_FILE
        fi
    else
        echo_t "[DHCP Manager PSM Check] PSM record \"${dhcp6_obj_path}\" already exists with value: \"${psmDHCPv6}\"" >> $LOG_FILE
    fi
    cnt=$((cnt + 1))
done
echo_t "[DHCP Manager PSM Check] End Of the Script" >> $LOG_FILE

