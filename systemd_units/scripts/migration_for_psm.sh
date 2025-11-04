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

echo_t "**********************<Entry> Migration Handling for PSM ******************************"
echo_t "Reboot Reason:<$rebootReason>"

if [ "$rebootReason" != "factory-reset" ]; then
   #WAN Interface Count
   wanifcount=`sed -n "/dmsb.wanmanager.wan.interfacecount/p" $PSM_CUR_XML_CONFIG_FILE_NAME | awk -F"[><]" '{print $3}'`
   if [ "$wanifcount" != "" ]; then
      wanifdefcount=`sed -n "/dmsb.wanmanager.wan.interfacecount/p" $PSM_DEF_XML_CONFIG_FILE_NAME | awk -F"[><]" '{print $3}'`
      if [ "$wanifdefcount" != "" ]; then
         echo_t "No. of WAN Interface from $PSM_DEF_XML_CONFIG_FILE_NAME:"$wanifdefcount
         echo_t "No. of WAN Interface from $PSM_CUR_XML_CONFIG_FILE_NAME:"$wanifcount
         if [ "$wanifcount" != "$wanifdefcount" ]; then
            sed -i "/dmsb.wanmanager.wan.interfacecount/d" $PSM_CUR_XML_CONFIG_FILE_NAME
            echo_t "WAN interface count mismatched so deleting this dmsb.wanmanager.wan.interfacecount entry from $PSM_CUR_XML_CONFIG_FILE_NAME to make sure proper interface count"
         else
            echo_t "WAN interface count is same so no migration required"
         fi
      fi
   fi

   #WAN Group Count
   wangrpcount=`sed -n "/dmsb.wanmanager.group.Count/p" $PSM_CUR_XML_CONFIG_FILE_NAME | awk -F"[><]" '{print $3}'`
   if [ "$wangrpcount" != "" ]; then
      wangrpdefcount=`sed -n "/dmsb.wanmanager.group.Count/p" $PSM_DEF_XML_CONFIG_FILE_NAME | awk -F"[><]" '{print $3}'`
      if [ "$wangrpdefcount" != "" ]; then
         echo_t "No. of WAN Group from $PSM_DEF_XML_CONFIG_FILE_NAME:"$wangrpdefcount
         echo_t "No. of WAN Group from $PSM_CUR_XML_CONFIG_FILE_NAME:"$wangrpcount
         if [ "$wangrpcount" != "$wangrpdefcount" ]; then
            sed -i "/dmsb.wanmanager.group.Count/d" $PSM_CUR_XML_CONFIG_FILE_NAME
            echo_t "WAN group count mismatched so deleting this dmsb.wanmanager.group.Count entry from $PSM_CUR_XML_CONFIG_FILE_NAME to make sure proper group count"
         else
            echo_t "WAN group count is same so no migration required"
         fi
      fi
   fi

   #DHCP MGR v4 Client Count
   dhcpmgrCltcount=`sed -n "/dmsb.dhcpmanager.ClientNoOfEntries/p" $PSM_CUR_XML_CONFIG_FILE_NAME | awk -F"[><]" '{print $3}'`
   if [ "$dhcpmgrCltcount" != "" ]; then
      dhcpmgrCltdefcount=`sed -n "/dmsb.dhcpmanager.ClientNoOfEntries/p" $PSM_DEF_XML_CONFIG_FILE_NAME | awk -F"[><]" '{print $3}'`
      if [ "$dhcpmgrCltdefcount" != "" ]; then
         echo_t "No. of DHCP MGR v4 client count from $PSM_DEF_XML_CONFIG_FILE_NAME:"$dhcpmgrCltdefcount
         echo_t "No. of DHCP MGR v4 client count from $PSM_CUR_XML_CONFIG_FILE_NAME:"$dhcpmgrCltcount
         if [ "$dhcpmgrCltcount" != "$dhcpmgrCltdefcount" ]; then
            sed -i "/dmsb.dhcpmanager.ClientNoOfEntries/d" $PSM_CUR_XML_CONFIG_FILE_NAME
            echo_t "DHCP MGR v4 client count mismatched so deleting this dmsb.dhcpmanager.ClientNoOfEntries entry from $PSM_CUR_XML_CONFIG_FILE_NAME to make sure proper v4 client count"
         else
            echo_t "DHCP MGR v4 client count is same so no migration required"
         fi
      fi
   fi

   #DHCP MGR v6 Client Count
   dhcpmgrv6Cltcount=`sed -n "/dmsb.dhcpmanager.dhcpv6.ClientNoOfEntries/p" $PSM_CUR_XML_CONFIG_FILE_NAME | awk -F"[><]" '{print $3}'`
   if [ "$dhcpmgrv6Cltcount" != "" ]; then
      dhcpmgrv6Cltdefcount=`sed -n "/dmsb.dhcpmanager.dhcpv6.ClientNoOfEntries/p" $PSM_DEF_XML_CONFIG_FILE_NAME | awk -F"[><]" '{print $3}'`
      if [ "$dhcpmgrv6Cltdefcount" != "" ]; then
         echo_t "No. of DHCP MGR v6 client count from $PSM_DEF_XML_CONFIG_FILE_NAME:"$dhcpmgrv6Cltdefcount
         echo_t "No. of DHCP MGR v6 client count from $PSM_CUR_XML_CONFIG_FILE_NAME:"$dhcpmgrv6Cltcount
         if [ "$dhcpmgrv6Cltcount" != "$dhcpmgrv6Cltdefcount" ]; then
            sed -i "/dmsb.dhcpmanager.dhcpv6.ClientNoOfEntries/d" $PSM_CUR_XML_CONFIG_FILE_NAME
            echo_t "DHCP MGR v6 client count mismatched so deleting this dmsb.dhcpmanager.dhcpv6.ClientNoOfEntries entry from $PSM_CUR_XML_CONFIG_FILE_NAME to make sure proper v6 client count"
         else
            echo_t "DHCP MGR v6 client count is same so no migration required"
         fi
      fi
   fi
fi

echo_t "**********************<Exit> Migration Handling for PSM ******************************"
