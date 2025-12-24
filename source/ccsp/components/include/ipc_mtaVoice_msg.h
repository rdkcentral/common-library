#include <stdio.h>
#include <stdbool.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>

#define MTA_AGENT_ADDR                 "tcp://127.0.0.1:60326"

#define BUFLEN_32                        32          //!< buffer length 32
#define BUFLEN_64                        64          //!< buffer length 64
#define BUFLEN_128                       128         //!< buffer length 128

typedef enum
{
    DHCPv4_STATE_CHANGED = 1,
    DHCPv6_STATE_CHANGED,
    NONE
}ipcMtaVoiceMsgType_t;

typedef struct _ipcMtaVoiceDhcpv4Data_t
{
    bool bIsAddrAssigned;               /** Have we been assigned an IP address ? */
    bool bIsIpExpired;                  /** Is the lease time expired ? */

    uint32_t ui32LeaseTime;              /** Lease time, , if addressAssigned==TRUE */
    uint32_t ui32RebindingTime;          /** Rebinding time, if addressAssigned==TRUE */
    uint32_t ui32RenewalTime;            /** Renewal Time, if addressAssigned==TRUE */
    int32_t  i32TimeOffset;              /** New time offset, if addressAssigned==TRUE */

    char cDhcpcInterface  [BUFLEN_64];    /** Dhcp interface name */
    char cIp              [BUFLEN_32];    /** New IP address, if addressAssigned==TRUE */
    char cBootfileName    [BUFLEN_64]; 
    char cDhcpServerName  [BUFLEN_128];
    char cDhcpServerId    [BUFLEN_64];    /** Dhcp server id */
    char cDhcpServerIp    [BUFLEN_32];
    char cMask            [BUFLEN_32];    /** New netmask, if addressAssigned==TRUE */
    char cRouterIp        [BUFLEN_32];    /** New router IP address, if addressAssigned==TRUE */
    char cDnsServer       [BUFLEN_64];    /** New dns Server, if addressAssigned==TRUE */
    char cDnsServer1      [BUFLEN_64];    /** New dns Server, if addressAssigned==TRUE */
    char cLogServer       [BUFLEN_32];
    char cDomainName      [BUFLEN_64];
    char cHostName        [BUFLEN_64];
    char cTimeServer      [BUFLEN_32]; 
    char cOption122       [BUFLEN_128];
    char cBroadcastIp     [BUFLEN_32];
    char cTimeZone        [BUFLEN_64];   /** New time zone, if addressAssigned==TRUE */
    char cTftpAddr        [BUFLEN_32];
}ipcMtaVoiceDhcpv4Data_t;

typedef struct _ipcMtaVoiceData_t
{
    ipcMtaVoiceMsgType_t eMsgType;
    union {
        ipcMtaVoiceDhcpv4Data_t sVoiceDhcpv4;
    }data;
}ipcMtaVoiceData_t;
