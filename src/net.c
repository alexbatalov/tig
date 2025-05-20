#include "tig/net.h"

#include <stdio.h>

#include "tig/core.h"
#include "tig/debug.h"
#include "tig/file.h"
#include "tig/idxtable.h"
#include "tig/memory.h"
#include "tig/net_blacklist.h"
#include "tig/net_xfer.h"
#include "tig/timer.h"
#include "tig/video.h"

#define TIG_NET_PROTOCOL_VERSION 25
#define TIG_NET_SERVER_PORT 31434
#define TIG_NET_CLIENT_PORT 31435
#define TIG_NET_BUF_SIZE 0x2000

#define TIG_NET_CLIENT_CONNECTED 0x01
#define TIG_NET_CLIENT_LOADING 0x02
#define TIG_NET_CLIENT_WAITING 0x04
#define TIG_NET_CLIENT_SHUTTING_DOWN 0x08
#define TIG_NET_CLIENT_LAGGING 0x10

#define TIG_NET_UNUSED0 0
#define TIG_NET_GENERIC 1
#define TIG_NET_SERVER_CMD 2
#define TIG_NET_APP 3
#define TIG_NET_UNUSED1 4
#define TIG_NET_UNUSED2 5
#define TIG_NET_UNUSED3 6
#define TIG_NET_BCAST_PING 7
#define TIG_NET_ALIVE 8

#define TIG_NET_JOIN_REQUEST 0
#define TIG_NET_ALLOW_JOIN 1
#define TIG_NET_DISALLOW_JOIN 2
#define TIG_NET_CLIENT_DETAILS 3
#define TIG_NET_SERVER_DETAILS 4
#define TIG_NET_CLIENT_DISCONNECT 5
#define TIG_NET_SERVER_DISCONNECT 6

typedef int(__stdcall WS_WSAFDISSET)(SOCKET s, fd_set* set);
typedef SOCKET(__stdcall WS_ACCEPT)(SOCKET s, struct sockaddr* addr, int* addrlen);
typedef int(__stdcall WS_BIND)(SOCKET s, const struct sockaddr* name, int namelen);
typedef int(__stdcall WS_CLOSESOCKET)(SOCKET s);
typedef int(__stdcall WS_CONNECT)(SOCKET s, const struct sockaddr* name, int namelen);
typedef int(__stdcall WS_IOCTLSOCKET)(SOCKET s, int cmd, u_long* argp);
typedef int(__stdcall WS_GETPEERNAME)(SOCKET s, struct sockaddr* name, int* namelen);
typedef int(__stdcall WS_GETSOCKNAME)(SOCKET s, struct sockaddr* name, int* namelen);
typedef int(__stdcall WS_GETSOCKOPT)(SOCKET s, int level, int optname, char* optval, int* optlen);
typedef u_long(__stdcall WS_HTONL)(u_long hostlong);
typedef u_short(__stdcall WS_HTONS)(u_short hostshort);
typedef unsigned int(__stdcall WS_INET_ADDR)(const char* cp);
typedef char*(__stdcall WS_INET_NTOA)(struct in_addr in);
typedef int(__stdcall WS_LISTEN)(SOCKET s, int backlog);
typedef u_long(__stdcall WS_NTOHL)(u_long netlong);
typedef u_short(__stdcall WS_NTOHS)(u_short netshort);
typedef int(__stdcall WS_RECV)(SOCKET s, char* buf, int len, int flags);
typedef int(__stdcall WS_RECVFROM)(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen);
typedef int(__stdcall WS_SELECT)(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const struct timeval* timeout);
typedef int(__stdcall WS_SEND)(SOCKET s, const char* buf, int len, int flags);
typedef int(__stdcall WS_SENDTO)(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen);
typedef int(__stdcall WS_SETSOCKOPT)(SOCKET s, int level, int optname, const char* optval, int optlen);
typedef int(__stdcall WS_SHUTDOWN)(SOCKET s, int how);
typedef SOCKET(__stdcall WS_SOCKET)(int af, int type, int protocol);
typedef struct hostent*(__stdcall WS_GETHOSTBYADDR)(const char* addr, int len, int type);
typedef struct hostent*(__stdcall WS_GETHOSTBYNAME)(const char* name);
typedef int(__stdcall WS_GETHOSTNAME)(char* name, int namelen);
typedef struct servent*(__stdcall WS_GETSERVBYPORT)(int port, const char* proto);
typedef struct servent*(__stdcall WS_GETSERVBYNAME)(const char* name, const char* proto);
typedef struct protoent*(__stdcall WS_GETPROTOBYNUMBER)(int number);
typedef struct protoent*(__stdcall WS_GETPROTOBYNAME)(const char* name);
typedef int(__stdcall WS_WSASTARTUP)(WORD wVersionRequested, LPWSADATA lpWSAData);
typedef int(__stdcall WS_WSACLEANUP)();
typedef void(__stdcall WS_WSASETLASTERROR)(int iError);
typedef int(__stdcall WS_WSAGETLASTERROR)();
typedef BOOL(__stdcall WS_WSAISBLOCKING)();
typedef int(__stdcall WS_WSAUNHOOKBLOCKINGHOOK)();
typedef FARPROC(__stdcall WS_WSASETBLOCKINGHOOK)(FARPROC lpBlockFunc);
typedef int(__stdcall WS_WSACANCELBLOCKINGCALL)();
typedef HANDLE(__stdcall WS_WSAASYNCGETSERVBYNAME)(HWND hWnd, u_int wMsg, const char* name, const char* proto, char* buf, int buflen);
typedef HANDLE(__stdcall WS_WSAASYNCGETSERVBYPORT)(HWND hWnd, u_int wMsg, int port, const char* proto, char* buf, int buflen);
typedef HANDLE(__stdcall WS_WSAASYNCGETPROTOBYNAME)(HWND hWnd, u_int wMsg, const char* name, char* buf, int buflen);
typedef HANDLE(__stdcall WS_WSAASYNCGETPROTOBYNUMBER)(HWND hWnd, u_int wMsg, int number, char* buf, int buflen);
typedef HANDLE(__stdcall WS_WSAASYNCGETHOSTBYNAME)(HWND hWnd, u_int wMsg, const char* name, char* buf, int buflen);
typedef HANDLE(__stdcall WS_WSAASYNCGETHOSTBYADDR)(HWND hWnd, u_int wMsg, const char* addr, int len, int type, char* buf, int buflen);
typedef int(__stdcall WS_WSACANCELASYNCREQUEST)(HANDLE hAsyncTaskHandle);
typedef int(__stdcall WS_WSAASYNCSELECT)(SOCKET s, HWND hWnd, u_int wMsg, int lEvent);
typedef SOCKET(__stdcall WS_WSAACCEPT)(SOCKET s, struct sockaddr* addr, LPINT addrlen, LPCONDITIONPROC lpfnCondition, DWORD_PTR dwCallbackData);
typedef BOOL(__stdcall WS_WSACLOSEEVENT)(HANDLE hEvent);
typedef int(__stdcall WS_WSACONNECT)(SOCKET s, const struct sockaddr* name, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS);
typedef HANDLE(__stdcall WS_WSACREATEEVENT)();
typedef int(__stdcall WS_WSADUPLICATESOCKETA)(SOCKET s, DWORD dwProcessId, LPWSAPROTOCOL_INFOA lpProtocolInfo);
typedef int(__stdcall WS_WSADUPLICATESOCKETW)(SOCKET s, DWORD dwProcessId, LPWSAPROTOCOL_INFOW lpProtocolInfo);
typedef int(__stdcall WS_WSAENUMNETWORKEVENTS)(SOCKET s, HANDLE hEventObject, LPWSANETWORKEVENTS lpNetworkEvents);
typedef int(__stdcall WS_WSAENUMPROTOCOLSA)(LPINT lpiProtocols, LPWSAPROTOCOL_INFOA lpProtocolBuffer, LPDWORD lpdwBufferLength);
typedef int(__stdcall WS_WSAENUMPROTOCOLSW)(LPINT lpiProtocols, LPWSAPROTOCOL_INFOW lpProtocolBuffer, LPDWORD lpdwBufferLength);
typedef int(__stdcall WS_WSAEVENTSELECT)(SOCKET s, HANDLE hEventObject, int lNetworkEvents);
typedef BOOL(__stdcall WS_WSAGETQOSBYNAME)(SOCKET s, LPWSABUF lpQOSName, LPQOS lpQOS);
typedef int(__stdcall WS_WSAHTONL)(SOCKET s, u_long hostlong, u_long* lpnetlong);
typedef int(__stdcall WS_WSAHTONS)(SOCKET s, u_short hostshort, u_short* lpnetshort);
typedef int(__stdcall WS_WSAIOCTL)(SOCKET s, DWORD dwIoControlCode, LPVOID lpvInBuffer, DWORD cbInBuffer, LPVOID lpvOutBuffer, DWORD cbOutBuffer, LPDWORD lpcbBytesReturned, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef SOCKET(__stdcall WS_WSAJOINLEAF)(SOCKET s, const struct sockaddr* name, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS, DWORD dwFlags);
typedef int(__stdcall WS_WSANTOHL)(SOCKET s, u_long netlong, u_long* lphostlong);
typedef int(__stdcall WS_WSANTOHS)(SOCKET s, u_short netshort, u_short* lphostshort);
typedef int(__stdcall WS_WSARECV)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef int(__stdcall WS_WSARECVDISCONNECT)(SOCKET s, LPWSABUF lpInboundDisconnectData);
typedef int(__stdcall WS_WSARECVFROM)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr* lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef BOOL(__stdcall WS_WSARESETEVENT)(HANDLE hEvent);
typedef int(__stdcall WS_WSASEND)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef int(__stdcall WS_WSASENDDISCONNECT)(SOCKET s, LPWSABUF lpOutboundDisconnectData);
typedef int(__stdcall WS_WSASENDTO)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr* lpTo, int iTolen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef BOOL(__stdcall WS_WSASETEVENT)(HANDLE hEvent);
typedef SOCKET(__stdcall WS_WSASOCKETA)(int af, int type, int protocol, LPWSAPROTOCOL_INFOA lpProtocolInfo, GROUP g, DWORD dwFlags);
typedef SOCKET(__stdcall WS_WSASOCKETW)(int af, int type, int protocol, LPWSAPROTOCOL_INFOW lpProtocolInfo, GROUP g, DWORD dwFlags);
typedef DWORD(__stdcall WS_WSAWAITFORMULTIPLEEVENTS)(DWORD cEvents, const HANDLE* lphEvents, BOOL fWaitAll, DWORD dwTimeout, BOOL fAlertable);
typedef INT(__stdcall WS_WSAADDRESSTOSTRINGA)(LPSOCKADDR lpsaAddress, DWORD dwAddressLength, LPWSAPROTOCOL_INFOA lpProtocolInfo, LPSTR lpszAddressString, LPDWORD lpdwAddressStringLength);
typedef INT(__stdcall WS_WSAADDRESSTOSTRINGW)(LPSOCKADDR lpsaAddress, DWORD dwAddressLength, LPWSAPROTOCOL_INFOW lpProtocolInfo, LPWSTR lpszAddressString, LPDWORD lpdwAddressStringLength);
typedef INT(__stdcall WS_WSASTRINGTOADDRESSA)(LPSTR AddressString, INT AddressFamily, LPWSAPROTOCOL_INFOA lpProtocolInfo, LPSOCKADDR lpAddress, LPINT lpAddressLength);
typedef INT(__stdcall WS_WSASTRINGTOADDRESSW)(LPWSTR AddressString, INT AddressFamily, LPWSAPROTOCOL_INFOW lpProtocolInfo, LPSOCKADDR lpAddress, LPINT lpAddressLength);
typedef INT(__stdcall WS_WSALOOKUPSERVICEBEGINA)(LPWSAQUERYSETA lpqsRestrictions, DWORD dwControlFlags, LPHANDLE lphLookup);
typedef INT(__stdcall WS_WSALOOKUPSERVICEBEGINW)(LPWSAQUERYSETW lpqsRestrictions, DWORD dwControlFlags, LPHANDLE lphLookup);
typedef INT(__stdcall WS_WSALOOKUPSERVICENEXTA)(HANDLE hLookup, DWORD dwControlFlags, LPDWORD lpdwBufferLength, LPWSAQUERYSETA lpqsResults);
typedef INT(__stdcall WS_WSALOOKUPSERVICENEXTW)(HANDLE hLookup, DWORD dwControlFlags, LPDWORD lpdwBufferLength, LPWSAQUERYSETW lpqsResults);
typedef INT(__stdcall WS_WSALOOKUPSERVICEEND)(HANDLE hLookup);
typedef INT(__stdcall WS_WSAINSTALLSERVICECLASSA)(LPWSASERVICECLASSINFOA lpServiceClassInfo);
typedef INT(__stdcall WS_WSAINSTALLSERVICECLASSW)(LPWSASERVICECLASSINFOW lpServiceClassInfo);
typedef INT(__stdcall WS_WSAREMOVESERVICECLASS)(LPGUID lpServiceClassId);
typedef INT(__stdcall WS_WSAGETSERVICECLASSINFOA)(LPGUID lpProviderId, LPGUID lpServiceClassId, LPDWORD lpdwBufSize, LPWSASERVICECLASSINFOA lpServiceClassInfo);
typedef INT(__stdcall WS_WSAGETSERVICECLASSINFOW)(LPGUID lpProviderId, LPGUID lpServiceClassId, LPDWORD lpdwBufSize, LPWSASERVICECLASSINFOW lpServiceClassInfo);
typedef INT(__stdcall WS_WSAENUMNAMESPACEPROVIDERSA)(LPDWORD lpdwBufferLength, LPWSANAMESPACE_INFOA lpnspBuffer);
typedef INT(__stdcall WS_WSAENUMNAMESPACEPROVIDERSW)(LPDWORD lpdwBufferLength, LPWSANAMESPACE_INFOW lpnspBuffer);
typedef INT(__stdcall WS_WSAGETSERVICECLASSNAMEBYCLASSIDA)(LPGUID lpServiceClassId, LPSTR lpszServiceClassName, LPDWORD lpdwBufferLength);
typedef INT(__stdcall WS_WSAGETSERVICECLASSNAMEBYCLASSIDW)(LPGUID lpServiceClassId, LPWSTR lpszServiceClassName, LPDWORD lpdwBufferLength);
typedef INT(__stdcall WS_WSASETSERVICEA)(LPWSAQUERYSETA lpqsRegInfo, WSAESETSERVICEOP essoperation, DWORD dwControlFlags);
typedef INT(__stdcall WS_WSASETSERVICEW)(LPWSAQUERYSETW lpqsRegInfo, WSAESETSERVICEOP essoperation, DWORD dwControlFlags);

// NOTE: Functions appear ordered in the binary implying they were arranged in
// a struct. Only a small fraction of these functions are actually used.
typedef struct TigNetWinSockProcs {
    WS_WSAFDISSET* WSAFDIsSet;
    WS_ACCEPT* accept;
    WS_BIND* bind;
    WS_CLOSESOCKET* closesocket;
    WS_CONNECT* connect;
    WS_IOCTLSOCKET* ioctlsocket;
    WS_GETPEERNAME* getpeername;
    WS_GETSOCKNAME* getsockname;
    WS_GETSOCKOPT* getsockopt;
    WS_HTONL* htonl;
    WS_HTONS* htons;
    WS_INET_ADDR* inet_addr;
    WS_INET_NTOA* inet_ntoa;
    WS_LISTEN* listen;
    WS_NTOHL* ntohl;
    WS_NTOHS* ntohs;
    WS_RECV* recv;
    WS_RECVFROM* recvfrom;
    WS_SELECT* select;
    WS_SEND* send;
    WS_SENDTO* sendto;
    WS_SETSOCKOPT* setsockopt;
    WS_SHUTDOWN* shutdown;
    WS_SOCKET* socket;
    WS_GETHOSTBYADDR* gethostbyaddr;
    WS_GETHOSTBYNAME* gethostbyname;
    WS_GETHOSTNAME* gethostname;
    WS_GETSERVBYPORT* getservbyport;
    WS_GETSERVBYNAME* getservbyname;
    WS_GETPROTOBYNUMBER* getprotobynumber;
    WS_GETPROTOBYNAME* getprotobyname;
    WS_WSASTARTUP* WSAStartup;
    WS_WSACLEANUP* WSACleanup;
    WS_WSASETLASTERROR* WSASetLastError;
    WS_WSAGETLASTERROR* WSAGetLastError;
    WS_WSAISBLOCKING* WSAIsBlocking;
    WS_WSAUNHOOKBLOCKINGHOOK* WSAUnhookBlockingHook;
    WS_WSASETBLOCKINGHOOK* WSASetBlockingHook;
    WS_WSACANCELBLOCKINGCALL* WSACancelBlockingCall;
    WS_WSAASYNCGETSERVBYNAME* WSAAsyncGetServByName;
    WS_WSAASYNCGETSERVBYPORT* WSAAsyncGetServByPort;
    WS_WSAASYNCGETPROTOBYNAME* WSAAsyncGetProtoByName;
    WS_WSAASYNCGETPROTOBYNUMBER* WSAAsyncGetProtoByNumber;
    WS_WSAASYNCGETHOSTBYNAME* WSAAsyncGetHostByName;
    WS_WSAASYNCGETHOSTBYADDR* WSAAsyncGetHostByAddr;
    WS_WSACANCELASYNCREQUEST* WSACancelAsyncRequest;
    WS_WSAASYNCSELECT* WSAAsyncSelect;
    WS_WSAACCEPT* WSAAccept;
    WS_WSACLOSEEVENT* WSACloseEvent;
    WS_WSACONNECT* WSAConnect;
    WS_WSACREATEEVENT* WSACreateEvent;
    WS_WSADUPLICATESOCKETA* WSADuplicateSocketA;
    WS_WSADUPLICATESOCKETW* WSADuplicateSocketW;
    WS_WSAENUMNETWORKEVENTS* WSAEnumNetworkEvents;
    WS_WSAENUMPROTOCOLSA* WSAEnumProtocolsA;
    WS_WSAENUMPROTOCOLSW* WSAEnumProtocolsW;
    WS_WSAEVENTSELECT* WSAEventSelect;
    WS_WSAEVENTSELECT* WSAEventSelect_; // FIXME: Duplicate of `WSAEventSelect`.
    WS_WSAGETQOSBYNAME* WSAGetQOSByName;
    WS_WSAHTONL* WSAHtonl;
    WS_WSAHTONS* WSAHtons;
    WS_WSAIOCTL* WSAIoctl;
    WS_WSAJOINLEAF* WSAJoinLeaf;
    WS_WSANTOHL* WSANtohl;
    WS_WSANTOHS* WSANtohs;
    WS_WSARECV* WSARecv;
    WS_WSARECVDISCONNECT* WSARecvDisconnect;
    WS_WSARECVFROM* WSARecvFrom;
    WS_WSARESETEVENT* WSAResetEvent;
    WS_WSASEND* WSASend;
    WS_WSASENDDISCONNECT* WSASendDisconnect;
    WS_WSASENDTO* WSASendTo;
    WS_WSASETEVENT* WSASetEvent;
    WS_WSASOCKETA* WSASocketA;
    WS_WSASOCKETW* WSASocketW;
    WS_WSAWAITFORMULTIPLEEVENTS* WSAWaitForMultipleEvents;
    WS_WSAADDRESSTOSTRINGA* WSAAddressToStringA;
    WS_WSAADDRESSTOSTRINGW* WSAAddressToStringW;
    WS_WSASTRINGTOADDRESSA* WSAStringToAddressA;
    WS_WSASTRINGTOADDRESSW* WSAStringToAddressW;
    WS_WSALOOKUPSERVICEBEGINA* WSALookupServiceBeginA;
    WS_WSALOOKUPSERVICEBEGINW* WSALookupServiceBeginW;
    WS_WSALOOKUPSERVICENEXTA* WSALookupServiceNextA;
    WS_WSALOOKUPSERVICENEXTW* WSALookupServiceNextW;
    WS_WSALOOKUPSERVICEEND* WSALookupServiceEnd;
    WS_WSAINSTALLSERVICECLASSA* WSAInstallServiceClassA;
    WS_WSAINSTALLSERVICECLASSW* WSAInstallServiceClassW;
    WS_WSAREMOVESERVICECLASS* WSARemoveServiceClass;
    WS_WSAGETSERVICECLASSINFOA* WSAGetServiceClassInfoA;
    WS_WSAGETSERVICECLASSINFOW* WSAGetServiceClassInfoW;
    WS_WSAENUMNAMESPACEPROVIDERSA* WSAEnumNameSpaceProvidersA;
    WS_WSAENUMNAMESPACEPROVIDERSW* WSAEnumNameSpaceProvidersW;
    WS_WSAGETSERVICECLASSNAMEBYCLASSIDA* WSAGetServiceClassNameByClassIdA;
    WS_WSAGETSERVICECLASSNAMEBYCLASSIDW* WSAGetServiceClassNameByClassIdW;
    WS_WSASETSERVICEA* WSASetServiceA;
    WS_WSASETSERVICEW* WSASetServiceW;
} TigNetWsProcs;

typedef struct TigNetSocket {
    struct sockaddr_in addr;
    SOCKET s;
} TigNetSocket;

typedef struct TigNetClientStats {
    tig_timestamp_t accept_timestamp;
    int bytes_received;
    int bytes_sent;
    int num_read_messages;
    int num_read_calls;
    int num_write_messages;
    int num_write_calls;
    int packets_received;
    int packets_sent;
} TigNetClientStats;

static_assert(sizeof(TigNetClientStats) == 0x24, "wrong size");

typedef struct TigNetClient {
    int client_id;
    char name[TIG_NET_CLIENT_NAME_LENGTH];
    unsigned int flags;
    SOCKET s;
    struct sockaddr_in addr;
    uint8_t data[TIG_NET_BUF_SIZE];
    int field_2034;
    uint8_t* field_2038;
    uint8_t* field_203C;
    int field_2040;
    TigNetClientStats stats;
    int field_2068;
    unsigned int field_206C;
} TigNetClient;

static_assert(sizeof(TigNetClient) == 0x2070, "wrong size");

typedef struct TigNetPacketHeader {
    uint8_t version;
    uint8_t type;
    uint16_t size;
} TigNetPacketHeader;

// TIG_NET_SERVER_CMD / TIG_NET_JOIN_REQUEST
typedef struct TigNetJoinRequestMsg {
    /* 0000 */ int type;
    /* 0004 */ char name[TIG_NET_CLIENT_NAME_LENGTH - 1];
    /* 001B */ char password[TIG_NET_PASSWORD_NAME_SIZE - 1];
    /* 0034 */ int field_34;
    /* 0038 */ int extra_size;
} TigNetJoinRequestMsg;

typedef struct TigNetJoinRequestPacket {
    /* 0000 */ TigNetPacketHeader hdr;
    /* 0004 */ TigNetJoinRequestMsg msg;
} TigNetJoinRequestPacket;

static_assert(sizeof(TigNetJoinRequestPacket) == 0x40, "wrong size");

// TIG_NET_SERVER_CMD / TIG_NET_JOIN_REQUEST
typedef struct TigNetAllowJoinMsg {
    /* 0000 */ int type;
    /* 0004 */ int client_id;
    /* 0008 */ char client_names[TIG_NET_MAX_PLAYERS][TIG_NET_CLIENT_NAME_LENGTH - 1];
    /* 00C0 */ int client_flags[TIG_NET_MAX_PLAYERS];
    /* 00E0 */ int client_ids[TIG_NET_MAX_PLAYERS];
    /* 0100 */ int max_players;
    /* 0104 */ int server_options;
} TigNetAllowJoinMsg;

typedef struct TigNetAllowJoinPacket {
    /* 0000 */ TigNetPacketHeader hdr;
    /* 0004 */ TigNetAllowJoinMsg msg;
} TigNetAllowJoinPacket;

static_assert(sizeof(TigNetAllowJoinPacket) == 0x10C, "wrong size");

// TIG_NET_SERVER_CMD / TIG_NET_DISALLOW_JOIN
typedef struct TigNetDisallowJoinMsg {
    /* 0000 */ int type;
} TigNetDisallowJoinMsg;

typedef struct TigNetDisallowJoinPacket {
    /* 0000 */ TigNetPacketHeader hdr;
    /* 0004 */ TigNetDisallowJoinMsg msg;
} TigNetDisallowJoinPacket;

static_assert(sizeof(TigNetDisallowJoinPacket) == 0x8, "wrong size");

// TIG_NET_SERVER_CMD / TIG_NET_CLIENT_DETAILS
typedef struct TigNetClientDetailsMsg {
    /* 0000 */ int type;
    /* 0004 */ int client_id;
    /* 0008 */ char client_name[TIG_NET_CLIENT_NAME_LENGTH - 1];
    /* 0020 */ unsigned int client_flags;
} TigNetClientDetailsMsg;

typedef struct TigNetClientDetailsPacket {
    /* 0000 */ TigNetPacketHeader hdr;
    /* 0004 */ TigNetClientDetailsMsg msg;
} TigNetClientDetailsPacket;

static_assert(sizeof(TigNetClientDetailsPacket) == 0x28, "wrong size");

// TIG_NET_SERVER_CMD / TIG_NET_SERVER_DETAILS
typedef struct TigNetServerDetailsMsg {
    /* 0000 */ int type;
    /* 0004 */ TigNetServer info;
} TigNetServerDetailsMsg;

typedef struct TigNetServerDetailsPacket {
    /* 0000 */ TigNetPacketHeader hdr;
    /* 0004 */ TigNetServerDetailsMsg msg;
} TigNetServerDetailsPacket;

static_assert(sizeof(TigNetServerDetailsPacket) == 0x14C, "wrong size");

// TIG_NET_SERVER_CMD / TIG_NET_CLIENT_DISCONNECT
typedef struct TigNetClientDisconnectMsg {
    /* 0000 */ int type;
    /* 0004 */ int client_id;
    /* 0008 */ int reason;
} TigNetClientDisconnectMsg;

typedef struct TigNetClientDisconnectPacket {
    /* 0000 */ TigNetPacketHeader hdr;
    /* 0004 */ TigNetClientDisconnectMsg msg;
} TigNetClientDisconnectPacket;

static_assert(sizeof(TigNetClientDisconnectPacket) == 0x10, "wrong size");

// TIG_NET_SERVER_CMD / TIG_NET_SERVER_DISCONNECT
typedef struct TigNetServerDisconnectMsg {
    /* 0000 */ int type;
} TigNetServerDisconnectMsg;

typedef struct TigNetServerDisconnectPacket {
    /* 0000 */ TigNetPacketHeader hdr;
    /* 0004 */ TigNetServerDisconnectMsg msg;
} TigNetServerDisconnectPacket;

static_assert(sizeof(TigNetServerDisconnectPacket) == 0x8, "wrong size");

// TIG_NET_APP
typedef struct TigNetPacket {
    TigNetPacketHeader hdr;
    uint8_t data[8188];
} TigNetPacket;

static_assert(sizeof(TigNetPacket) == 0x2000, "wrong size");

// TIG_NET_BCAST_PING
typedef struct TigNetPingPacket {
    /* 0000 */ TigNetPacketHeader hdr;
    /* 0004 */ int type;
    /* 0008 */ int client_id;
    /* 000C */ tig_timestamp_t timestamp;
} TigNetPingPacket;

static_assert(sizeof(TigNetPingPacket) == 0x10, "wrong size");

// TIG_NET_ALIVE
typedef struct TigNetAlivePacket {
    /* 0000 */ TigNetPacketHeader hdr;
    /* 0004 */ int type;
    /* 0008 */ struct sockaddr_in addr;
    /* 0018 */ tig_timestamp_t time;
} TigNetAlivePacket;

static_assert(sizeof(TigNetAlivePacket) == 0x1C, "wrong size");

typedef struct TigNetRemoveServerListNode {
    int addr;
    struct TigNetRemoveServerListNode* next;
} TigNetRemoveServerListNode;

// 0x52A750
static_assert(sizeof(TigNetRemoveServerListNode) == 0x8, "wrong size");

static int __stdcall tig_net_fd_is_set(SOCKET s, fd_set* set);
static void tig_net_client_info_exit(TigNetClient* client);
static void tig_net_client_info_init(TigNetClient* client);
static void tig_net_print_client_stats(TigNetClientStats* stats);
static bool tig_net_ws_exit();
static void sub_527280();
static bool tig_net_ws_init();
static bool tig_net_ws_locate_procs_1_1();
static bool tig_net_ws_locate_procs_2_0();
static void sub_527AC0(int client_id);
static int sub_527AF0();
static bool tig_net_create_bcast_socket();
static void tig_net_bookmark_init(TigNetServer* bookmark);
static void sub_5280E0();
static bool tig_net_connect_internal(TigNetSocket* sock, const char* server_name);
static bool tig_net_is_ip_address(const char* cp);
static void tig_net_msg_read_all();
static void tig_net_msg_read(SOCKET s);
static TigNetClient* tig_net_get_client_by_socket(SOCKET s);
static void tig_net_close(SOCKET s);
static void tig_net_accept(HWND hWnd, WPARAM wParam);
static int tig_net_client_get_free_slot();
static bool tig_net_send_to_client_internal(TigNetPacketHeader* header, uint16_t size, int client_id);
static bool sub_529830(TigNetPacketHeader* header, uint16_t size);
static bool tig_net_send_to_socket(TigNetPacketHeader* header, int size, SOCKET s);
static bool tig_net_validate_packet(TigNetPacketHeader* header, const char* msg);
static bool tig_net_can_parse_packet(TigNetPacketHeader* header, const char* msg);
static bool tig_net_parse_packet(int client_id);
static bool tig_net_process_packet(int client_id);
static void sub_529C20(TigNetPacketHeader* hdr, int client_id);
static bool sub_52A460(int flags, int client_id);
static void sub_52A490(int client_id, int a2);
static void sub_52A4C0();
static void sub_52A4D0(int client_id);
static bool sub_52A500(TigNetClient* dst, TigNetClient* src);
static void tig_net_server_bcast_alive();
static void sub_52A630(TigNetServer* server);
static bool tig_net_remove_unresponsive_servers();
static bool sub_52A750(void* entry, int index, void* context);
static bool sub_52A7F0(void* entry, int index, void* context);
static int tig_net_num_active_clients();
static unsigned int tig_net_client_info_get_flags(int client_id);
static void sub_52AB10(int client_id);
static void tig_net_client_set_waiting(int client_id);
static void tig_net_client_unset_waiting(int client_id);
static bool tig_net_client_is_shutting_down(int client_id);
static void tig_net_bookmarks_load(const char* path);
static void tig_net_bookmarks_save(const char* path);
static bool tig_net_bookmarks_save_one(void* value, int index, void* context);
static void sub_52B2C0();
static void sub_52B2D0();

// 0x5C0648
static TigNetServerFilters tig_net_server_filter = TIG_NET_SERVER_FILTER_ALL;

// NOTE: This is unused but explains where some symbols came from.
//
// 0x5C0650
static const char* off_5C0650[] = {
    "UNUSED0",
    "GENERIC",
    "SERVER_CMD",
    "APP",
    "UNUSED1",
    "UNUSED2",
    "UNUSED3",
    "BCAST_PING",
    "ALIVE",
};

// 0x5C064C
static int dword_5C064C = -1;

// 0x6123A8
static TigNetSocket tig_net_bcast_socket;

// 0x6123C0
static TigNetServer tig_net_local_bookmark;

// 0x612508
static TigNetSocket tig_net_local_socket;

// 0x612520
static TigIdxTable tig_net_bookmarks;

// 0x0x612530
static TigNetWsProcs tig_net_ws_procs;

// 0x6126B0
static WSADATA tig_net_ws_data;

// 0x612840
static HMODULE tig_net_ws_dll;

// 0x614848
static TigNetClient tig_net_local_client;

// 0x6188B8
static TigNetClient tig_net_clients[TIG_NET_MAX_PLAYERS];

// 0x62AC38
static char* tig_net_local_server_password;

// 0x62AC3C
static int dword_62AC3C;

// 0x62AC40
static int tig_net_num_servers;

// 0x62AC44
static TigNetServer* tig_net_servers;

// 0x62AC48
static bool tig_net_bookmarks_changed;

// 0x62AC4C
static int dword_62AC4C;

// 0x62AC50
static int dword_62AC50;

// 0x62AC54
static TigNetOnNetworkEvent* tig_net_on_network_event_func;

// 0x62AC58
static TigNetOnMessage* tig_net_on_message_func;

// 0x62AC5C
static TigNetOnJoinResponse* tig_net_on_join_response_func;

// 0x62AC60
static TigNetOnServersChange* tig_net_on_servers_change_func;

// 0x62AC64
static TigNetOnMessageValidation* tig_net_on_message_validation_func;

// 0x62AC68
static TigNetOnServerBcast* tig_net_on_server_bcast_func;

// 0x62AC6C
unsigned int tig_net_flags;

// 0x62AC70
static bool tig_net_ws_initialized;

// 0x62AC74
static tig_timestamp_t tig_net_server_bcast_timestamp;

// 0x62AC78
static tig_timestamp_t dword_62AC78;

// 0x62AC7C
static tig_timestamp_t tig_net_remove_unresponsive_servers_timestamp;

// 0x62AC80
static tig_timestamp_t dword_62AC80;

// 0x62AC84
static TigNetRemoveServerListNode* tig_net_remove_server_list_head;

// 0x62AC88
static int dword_62AC88;

// NOTE: For unknown reason this function use `stdcall` convention.
//
// 0x526AB0
int __stdcall tig_net_fd_is_set(SOCKET s, fd_set* set)
{
    return tig_net_ws_procs.WSAFDIsSet(s, set);
}

// 0x526AD0
void tig_net_client_info_exit(TigNetClient* client)
{
    (void)client;
}

// 0x526AE0
bool tig_net_client_is_active(int client_id)
{
    return (tig_net_clients[client_id].flags & TIG_NET_CLIENT_CONNECTED) != 0;
}

// 0x526B00
bool tig_net_client_is_local(int client_id)
{
    return client_id == tig_net_local_client.client_id;
}

// 0x526B20
bool tig_net_client_is_loading(int client_id)
{
    if (tig_net_client_is_active(client_id)) {
        return (tig_net_clients[client_id].flags & TIG_NET_CLIENT_LOADING) != 0;
    } else {
        return false;
    }
}

// 0x526B50
const char* tig_net_client_info_get_name(int client_id)
{
    if (tig_net_client_is_active(client_id)) {
        return tig_net_clients[client_id].name;
    } else {
        tig_debug_printf("TCP-NET: Error: calling tig_net_client_info_get_name on inactive slot %d\n", client_id);
        return "*INACTIVE*";
    }
}

// 0x526B90
bool tig_net_client_info_set_name(int client_id, const char* name)
{
    strncpy(tig_net_clients[client_id].name, name, TIG_NET_CLIENT_NAME_LENGTH - 1);
    strncpy(tig_net_local_bookmark.players[client_id].name, name, TIG_NET_CLIENT_NAME_LENGTH - 1);
    return true;
}

// 0x526CB0
int tig_net_client_info_get_bytes_sent(int client_id)
{
    return tig_net_clients[client_id].stats.bytes_sent;
}

// 0x526CD0
int tig_net_client_info_get_bytes_received(int client_id)
{
    return tig_net_clients[client_id].stats.bytes_received;
}

// 0x526CF0
tig_duration_t sub_526CF0(int client_id)
{
    return tig_timer_elapsed(tig_net_clients[client_id].stats.accept_timestamp);
}

// 0x526D20
bool sub_526D20(int client_id)
{
    memset(&(tig_net_clients[client_id].stats), 0, sizeof(TigNetClientStats));
    tig_timer_now(&(tig_net_clients[client_id].stats.accept_timestamp));
    return true;
}

// 0x526D60
int sub_526D60(int client_id)
{
    // NOTE: Rare case - result of `tig_timer_elapsed` performs unsigned
    // compare.
    unsigned int elapsed;

    if (tig_net_clients[client_id].field_206C != 0) {
        elapsed = tig_timer_elapsed(tig_net_clients[client_id].field_206C);
        if (elapsed > 1400) {
            tig_net_clients[client_id].field_2068 = elapsed;
            if ((tig_net_clients[client_id].flags & TIG_NET_CLIENT_LAGGING) == 0) {
                tig_net_clients[client_id].flags |= TIG_NET_CLIENT_LAGGING;

                if (tig_net_on_network_event_func != NULL) {
                    tig_net_on_network_event_func(2, client_id, 0, 0);
                }
            }
        } else {
            if ((tig_net_clients[client_id].flags & TIG_NET_CLIENT_LAGGING) != 0) {
                tig_net_clients[client_id].flags &= ~TIG_NET_CLIENT_LAGGING;

                if (tig_net_on_network_event_func != NULL) {
                    tig_net_on_network_event_func(3, client_id, 0, 0);
                }
            }
        }
    }

    return tig_net_clients[client_id].field_2068;
}

// 0x526E00
void sub_526E00()
{
    tig_net_flags &= ~(TIG_NET_CONNECTED | TIG_NET_HOST | TIG_NET_FLAG_0x08);
}

// 0x526E10
int tig_net_inet_string(unsigned long addr, char* dest)
{
    return sprintf(dest, "%d.%d.%d.%d",
        addr & 0xFF,
        (addr >> 8) & 0xFF,
        (addr >> 16) & 0xFF,
        (addr >> 24) & 0xFF);
}

// 0x526E40
bool tig_net_inet_addr(const char* cp, unsigned long* addr)
{
    *addr = tig_net_ws_procs.inet_addr(cp);
    return *addr != -1;
}

// 0x526E60
int tig_net_init(TigInitInfo* init_info)
{
    int index;

    (void)init_info;

    tig_debug_printf("TIG-NET: Initializing...\n");

    tig_debug_printf("         %d Max Players, AutoJoin: %s, NoBroadcast: %s.\n",
        TIG_NET_MAX_PLAYERS,
        tig_net_auto_join_is_enabled() ? "On" : "Off",
        (tig_net_flags & TIG_NET_NO_BROADCAST) != 0 ? "On" : "Off");

    dword_62AC4C = 0;
    tig_net_server_filter = TIG_NET_SERVER_FILTER_ALL;

    for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
        tig_net_client_info_init(&(tig_net_clients[index]));
    }

    tig_net_client_info_init(&tig_net_local_client);

    tig_net_bcast_socket.s = INVALID_SOCKET;
    tig_net_local_socket.s = INVALID_SOCKET;

    tig_net_bookmark_init(&tig_net_local_bookmark);
    tig_net_local_server_set_name("Arcanum");
    tig_net_local_client_set_name("Player");

    tig_idxtable_init(&tig_net_bookmarks, sizeof(TigNetServer));

    tig_net_servers = NULL;
    tig_net_blacklist_init();
    tig_net_local_server_password = NULL;
    tig_debug_printf("         Done Initializing.\n");

    return TIG_OK;
}

// 0x526F50
void tig_net_client_info_init(TigNetClient* client)
{
    client->client_id = -1;
    client->name[0] = '\0';
    client->flags = 0;
    client->s = INVALID_SOCKET;
    client->field_2034 = sizeof(client->data);
    memset(client->data, 0, sizeof(client->data));
    client->field_2040 = client->field_2034;
    client->field_2038 = client->data;
    client->field_203C = client->data;
    memset(&(client->stats), 0, sizeof(TigNetClientStats));
    client->field_2068 = 0;
    client->field_206C = 0;
}

// 0x526FC0
int tig_net_reset()
{
    int index;

    tig_debug_printf("TIG-NET: Resetting...\n");

    tig_debug_printf("         %d Max Players, AutoJoin: %s, NoBroadcast: %s.\n",
        TIG_NET_MAX_PLAYERS,
        tig_net_auto_join_is_enabled() ? "On" : "Off",
        (tig_net_flags & TIG_NET_NO_BROADCAST) != 0 ? "On" : "Off");

    dword_62AC4C = 0;
    tig_net_server_filter = 127;

    for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
        tig_net_client_info_init(&(tig_net_clients[index]));
    }

    tig_net_client_info_init(&tig_net_local_client);

    tig_net_bcast_socket.s = INVALID_SOCKET;
    tig_net_local_socket.s = INVALID_SOCKET;
    tig_net_bookmark_init(&tig_net_local_bookmark);
    tig_net_local_server_set_name("Arcanum");
    tig_net_local_client_set_name("Player");

    tig_idxtable_exit(&tig_net_bookmarks);
    tig_idxtable_init(&tig_net_bookmarks, sizeof(TigNetServer));

    if (tig_net_local_server_password != NULL) {
        FREE(tig_net_local_server_password);
    }
    tig_net_local_server_password = NULL;

    tig_net_flags = 0;
    sub_5280F0();

    return TIG_OK;
}

// 0x5270C0
void tig_net_exit()
{
    int index;

    tig_net_bookmarks_save(".\\data\\bookmark.txt");

    for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
        if (tig_net_client_is_active(index)) {
            tig_net_print_client_stats(&(tig_net_clients[index].stats));
        }

        tig_net_client_info_exit(&(tig_net_clients[index]));
    }

    tig_net_client_info_exit(&tig_net_local_client);

    tig_net_ws_exit();
    tig_idxtable_exit(&tig_net_bookmarks);

    if (tig_net_servers != NULL) {
        FREE(tig_net_servers);
        tig_net_servers = NULL;
    }

    tig_net_flags = 0;

    tig_net_blacklist_exit();

    if (tig_net_local_server_password != NULL) {
        FREE(tig_net_local_server_password);
    }
}

// 0x527170
void tig_net_print_client_stats(TigNetClientStats* stats)
{
    tig_debug_printf("**** Traffic Statistics for %9.9p ****\n", stats);
    tig_debug_printf("** Connected at: %d, online for %d **\n", stats->accept_timestamp, tig_timer_elapsed(stats->accept_timestamp));
    tig_debug_printf("** Sent: Bytes[ %d ], Packets[ %d ] **\n", stats->bytes_sent, stats->packets_sent);
    tig_debug_printf("**       Write Calls[ %d ], Write Messages[ %d ] **\n", stats->num_write_calls, stats->num_write_messages);
    tig_debug_printf("** Read: Bytes[ %d ], Packets[ %d ] **\n", stats->bytes_received, stats->packets_received);
    tig_debug_printf("**       Read Calls[ %d ], Read Messages[ %d ] **\n", stats->num_read_calls, stats->num_read_messages);
    tig_debug_printf("****************************\n");
}

// 0x527180
bool tig_net_ws_exit()
{
    if (tig_net_ws_initialized) {
        tig_debug_printf("TCP-NET: Closing WinSock services...\n");

        tig_net_ws_procs.WSACleanup();

        FreeLibrary(tig_net_ws_dll);
        tig_net_ws_dll = NULL;

        tig_net_ws_initialized = false;
    }

    return true;
}

// 0x5271D0
int tig_net_start_client()
{
    if ((tig_net_flags & TIG_NET_CONNECTED) != 0) {
        return TIG_OK;
    }

    if (!tig_net_ws_init()) {
        return TIG_ERR_NET;
    }

    tig_net_bookmarks_load(".\\data\\bookmark.txt");

    if (sub_527AF0() == TIG_ERR_NET) {
        return TIG_ERR_NET;
    }

    if ((tig_net_flags & TIG_NET_NO_BROADCAST) == 0) {
        tig_net_bcast_socket.addr.sin_family = AF_INET;
        tig_net_bcast_socket.addr.sin_port = tig_net_ws_procs.htons(TIG_NET_SERVER_PORT);
        tig_net_bcast_socket.addr.sin_addr.s_addr = INADDR_ANY;
        memset(tig_net_bcast_socket.addr.sin_zero, 0, sizeof(tig_net_bcast_socket.addr.sin_zero));
    }

    sub_527280();
    sub_527AC0(0);
    tig_net_client_info_set_name(0, "ServerPlayer");

    tig_debug_printf("TCP-NET: [ Starting ]\n");
    if ((tig_net_flags & TIG_NET_NO_BROADCAST) == 0) {
        tig_net_create_bcast_socket();
    }

    return TIG_OK;
}

// 0x527280
void sub_527280()
{
    tig_net_flags |= TIG_NET_CONNECTED;
}

// 0x527290
bool tig_net_ws_init()
{
    if (tig_net_ws_initialized) {
        tig_debug_printf("TCP-NET: Error initializing winsock, it is already initialized, try again anyway.\n");
    }

    tig_net_ws_initialized = true;

    tig_net_ws_dll = LoadLibraryA("ws2_32.dll");
    if (tig_net_ws_dll == NULL) {
        tig_net_ws_dll = LoadLibraryA("wsock32.dll");
        if (tig_net_ws_dll == NULL) {
            tig_debug_printf("TCP-NET: Error no acceptible WinSock implementation available.\n");
            return true;
        }
    }

    tig_net_ws_procs.WSAStartup = (WS_WSASTARTUP*)GetProcAddress(tig_net_ws_dll, "WSAStartup");
    if (tig_net_ws_procs.WSAStartup == NULL) {
        tig_debug_printf("TCP-NET: Error no acceptible WinSock implementation available.");
        return false;
    }

    if (tig_net_ws_procs.WSAStartup(MAKEWORD(2, 0), &tig_net_ws_data) == 0) {
        if (LOBYTE(tig_net_ws_data.wVersion) == 2 && HIBYTE(tig_net_ws_data.wVersion) == 0) {
            tig_debug_printf("TCP-NET: WSAStartup complete, winsock version 2.0\n");
            tig_net_ws_locate_procs_1_1();
            tig_net_ws_locate_procs_2_0();
            return true;
        }
    }

    if (tig_net_ws_procs.WSAStartup(MAKEWORD(1, 1), &tig_net_ws_data) == 0) {
        if (LOBYTE(tig_net_ws_data.wVersion) == 1 && HIBYTE(tig_net_ws_data.wVersion) == 1) {
            tig_debug_printf("TCP-NET: WSAStartup complete, winsock version 1.1\n");
            tig_net_ws_locate_procs_1_1();
            return true;
        }
    }

    // FIXME: This pointer is not bound yet, calling it will crash the game.
    tig_net_ws_procs.WSACleanup();
    tig_debug_printf("TCP-NET: WSAStartup could not find a valid WinSock implementation\n");

    return false;
}

// 0x5273B0
bool tig_net_ws_locate_procs_1_1()
{
    tig_net_ws_procs.WSAFDIsSet = (WS_WSAFDISSET*)GetProcAddress(tig_net_ws_dll, "__WSAFDIsSet");
    tig_net_ws_procs.accept = (WS_ACCEPT*)GetProcAddress(tig_net_ws_dll, "accept");
    tig_net_ws_procs.bind = (WS_BIND*)GetProcAddress(tig_net_ws_dll, "bind");
    tig_net_ws_procs.closesocket = (WS_CLOSESOCKET*)GetProcAddress(tig_net_ws_dll, "closesocket");
    tig_net_ws_procs.connect = (WS_CONNECT*)GetProcAddress(tig_net_ws_dll, "connect");
    tig_net_ws_procs.ioctlsocket = (WS_IOCTLSOCKET*)GetProcAddress(tig_net_ws_dll, "ioctlsocket");
    tig_net_ws_procs.getpeername = (WS_GETPEERNAME*)GetProcAddress(tig_net_ws_dll, "getpeername");
    tig_net_ws_procs.getsockname = (WS_GETSOCKNAME*)GetProcAddress(tig_net_ws_dll, "getsockname");
    tig_net_ws_procs.getsockopt = (WS_GETSOCKOPT*)GetProcAddress(tig_net_ws_dll, "getsockopt");
    tig_net_ws_procs.htonl = (WS_HTONL*)GetProcAddress(tig_net_ws_dll, "htonl");
    tig_net_ws_procs.htons = (WS_HTONS*)GetProcAddress(tig_net_ws_dll, "htons");
    tig_net_ws_procs.inet_addr = (WS_INET_ADDR*)GetProcAddress(tig_net_ws_dll, "inet_addr");
    tig_net_ws_procs.inet_ntoa = (WS_INET_NTOA*)GetProcAddress(tig_net_ws_dll, "inet_ntoa");
    tig_net_ws_procs.listen = (WS_LISTEN*)GetProcAddress(tig_net_ws_dll, "listen");
    tig_net_ws_procs.ntohl = (WS_NTOHL*)GetProcAddress(tig_net_ws_dll, "ntohl");
    tig_net_ws_procs.ntohs = (WS_NTOHS*)GetProcAddress(tig_net_ws_dll, "ntohs");
    tig_net_ws_procs.recv = (WS_RECV*)GetProcAddress(tig_net_ws_dll, "recv");
    tig_net_ws_procs.recvfrom = (WS_RECVFROM*)GetProcAddress(tig_net_ws_dll, "recvfrom");
    tig_net_ws_procs.select = (WS_SELECT*)GetProcAddress(tig_net_ws_dll, "select");
    tig_net_ws_procs.send = (WS_SEND*)GetProcAddress(tig_net_ws_dll, "send");
    tig_net_ws_procs.sendto = (WS_SENDTO*)GetProcAddress(tig_net_ws_dll, "sendto");
    tig_net_ws_procs.setsockopt = (WS_SETSOCKOPT*)GetProcAddress(tig_net_ws_dll, "setsockopt");
    tig_net_ws_procs.shutdown = (WS_SHUTDOWN*)GetProcAddress(tig_net_ws_dll, "shutdown");
    tig_net_ws_procs.socket = (WS_SOCKET*)GetProcAddress(tig_net_ws_dll, "socket");
    tig_net_ws_procs.gethostbyaddr = (WS_GETHOSTBYADDR*)GetProcAddress(tig_net_ws_dll, "gethostbyaddr");
    tig_net_ws_procs.gethostbyname = (WS_GETHOSTBYNAME*)GetProcAddress(tig_net_ws_dll, "gethostbyname");
    tig_net_ws_procs.gethostname = (WS_GETHOSTNAME*)GetProcAddress(tig_net_ws_dll, "gethostname");
    tig_net_ws_procs.getservbyport = (WS_GETSERVBYPORT*)GetProcAddress(tig_net_ws_dll, "getservbyport");
    tig_net_ws_procs.getservbyname = (WS_GETSERVBYNAME*)GetProcAddress(tig_net_ws_dll, "getservbyname");
    tig_net_ws_procs.getprotobynumber = (WS_GETPROTOBYNUMBER*)GetProcAddress(tig_net_ws_dll, "getprotobynumber");
    tig_net_ws_procs.getprotobyname = (WS_GETPROTOBYNAME*)GetProcAddress(tig_net_ws_dll, "getprotobyname");
    tig_net_ws_procs.WSACleanup = (WS_WSACLEANUP*)GetProcAddress(tig_net_ws_dll, "WSACleanup");
    tig_net_ws_procs.WSASetLastError = (WS_WSASETLASTERROR*)GetProcAddress(tig_net_ws_dll, "WSASetLastError");
    tig_net_ws_procs.WSAGetLastError = (WS_WSAGETLASTERROR*)GetProcAddress(tig_net_ws_dll, "WSAGetLastError");
    tig_net_ws_procs.WSAIsBlocking = (WS_WSAISBLOCKING*)GetProcAddress(tig_net_ws_dll, "WSAIsBlocking");
    tig_net_ws_procs.WSAUnhookBlockingHook = (WS_WSAUNHOOKBLOCKINGHOOK*)GetProcAddress(tig_net_ws_dll, "WSAUnhookBlockingHook");
    tig_net_ws_procs.WSASetBlockingHook = (WS_WSASETBLOCKINGHOOK*)GetProcAddress(tig_net_ws_dll, "WSASetBlockingHook");
    tig_net_ws_procs.WSACancelBlockingCall = (WS_WSACANCELBLOCKINGCALL*)GetProcAddress(tig_net_ws_dll, "WSACancelBlockingCall");
    tig_net_ws_procs.WSAAsyncGetServByName = (WS_WSAASYNCGETSERVBYNAME*)GetProcAddress(tig_net_ws_dll, "WSAAsyncGetServByName");
    tig_net_ws_procs.WSAAsyncGetServByPort = (WS_WSAASYNCGETSERVBYPORT*)GetProcAddress(tig_net_ws_dll, "WSAAsyncGetServByPort");
    tig_net_ws_procs.WSAAsyncGetProtoByName = (WS_WSAASYNCGETPROTOBYNAME*)GetProcAddress(tig_net_ws_dll, "WSAAsyncGetProtoByName");
    tig_net_ws_procs.WSAAsyncGetProtoByNumber = (WS_WSAASYNCGETPROTOBYNUMBER*)GetProcAddress(tig_net_ws_dll, "WSAAsyncGetProtoByNumber");
    tig_net_ws_procs.WSAAsyncGetHostByName = (WS_WSAASYNCGETHOSTBYNAME*)GetProcAddress(tig_net_ws_dll, "WSAAsyncGetHostByName");
    tig_net_ws_procs.WSAAsyncGetHostByAddr = (WS_WSAASYNCGETHOSTBYADDR*)GetProcAddress(tig_net_ws_dll, "WSAAsyncGetHostByAddr");
    tig_net_ws_procs.WSACancelAsyncRequest = (WS_WSACANCELASYNCREQUEST*)GetProcAddress(tig_net_ws_dll, "WSACancelAsyncRequest");
    tig_net_ws_procs.WSAAsyncSelect = (WS_WSAASYNCSELECT*)GetProcAddress(tig_net_ws_dll, "WSAAsyncSelect");
    return true;
}

// 0x527720
bool tig_net_ws_locate_procs_2_0()
{
    tig_net_ws_procs.WSAAccept = (WS_WSAACCEPT*)GetProcAddress(tig_net_ws_dll, "WSAAccept");
    tig_net_ws_procs.WSACloseEvent = (WS_WSACLOSEEVENT*)GetProcAddress(tig_net_ws_dll, "WSACloseEvent");
    tig_net_ws_procs.WSAConnect = (WS_WSACONNECT*)GetProcAddress(tig_net_ws_dll, "WSAConnect");
    tig_net_ws_procs.WSACreateEvent = (WS_WSACREATEEVENT*)GetProcAddress(tig_net_ws_dll, "WSACreateEvent");
    tig_net_ws_procs.WSADuplicateSocketA = (WS_WSADUPLICATESOCKETA*)GetProcAddress(tig_net_ws_dll, "WSADuplicateSocketA");
    tig_net_ws_procs.WSADuplicateSocketW = (WS_WSADUPLICATESOCKETW*)GetProcAddress(tig_net_ws_dll, "WSADuplicateSocketW");
    tig_net_ws_procs.WSAEnumNetworkEvents = (WS_WSAENUMNETWORKEVENTS*)GetProcAddress(tig_net_ws_dll, "WSAEnumNetworkEvents");
    tig_net_ws_procs.WSAEnumProtocolsA = (WS_WSAENUMPROTOCOLSA*)GetProcAddress(tig_net_ws_dll, "WSAEnumProtocolsA");
    tig_net_ws_procs.WSAEnumProtocolsW = (WS_WSAENUMPROTOCOLSW*)GetProcAddress(tig_net_ws_dll, "WSAEnumProtocolsW");
    tig_net_ws_procs.WSAEventSelect = (WS_WSAEVENTSELECT*)GetProcAddress(tig_net_ws_dll, "WSAEventSelect");
    tig_net_ws_procs.WSAEventSelect_ = (WS_WSAEVENTSELECT*)GetProcAddress(tig_net_ws_dll, "WSAEventSelect");
    tig_net_ws_procs.WSAGetQOSByName = (WS_WSAGETQOSBYNAME*)GetProcAddress(tig_net_ws_dll, "WSAGetQOSByName");
    tig_net_ws_procs.WSAHtonl = (WS_WSAHTONL*)GetProcAddress(tig_net_ws_dll, "WSAHtonl");
    tig_net_ws_procs.WSAHtons = (WS_WSAHTONS*)GetProcAddress(tig_net_ws_dll, "WSAHtons");
    tig_net_ws_procs.WSAIoctl = (WS_WSAIOCTL*)GetProcAddress(tig_net_ws_dll, "WSAIoctl");
    tig_net_ws_procs.WSAJoinLeaf = (WS_WSAJOINLEAF*)GetProcAddress(tig_net_ws_dll, "WSAJoinLeaf");
    tig_net_ws_procs.WSANtohl = (WS_WSANTOHL*)GetProcAddress(tig_net_ws_dll, "WSANtohl");
    tig_net_ws_procs.WSANtohs = (WS_WSANTOHS*)GetProcAddress(tig_net_ws_dll, "WSANtohs");
    tig_net_ws_procs.WSARecv = (WS_WSARECV*)GetProcAddress(tig_net_ws_dll, "WSARecv");
    tig_net_ws_procs.WSARecvDisconnect = (WS_WSARECVDISCONNECT*)GetProcAddress(tig_net_ws_dll, "WSARecvDisconnect");
    tig_net_ws_procs.WSARecvFrom = (WS_WSARECVFROM*)GetProcAddress(tig_net_ws_dll, "WSARecvFrom");
    tig_net_ws_procs.WSAResetEvent = (WS_WSARESETEVENT*)GetProcAddress(tig_net_ws_dll, "WSAResetEvent");
    tig_net_ws_procs.WSASend = (WS_WSASEND*)GetProcAddress(tig_net_ws_dll, "WSASend");
    tig_net_ws_procs.WSASendDisconnect = (WS_WSASENDDISCONNECT*)GetProcAddress(tig_net_ws_dll, "WSASendDisconnect");
    tig_net_ws_procs.WSASendTo = (WS_WSASENDTO*)GetProcAddress(tig_net_ws_dll, "WSASendTo");
    tig_net_ws_procs.WSASetEvent = (WS_WSASETEVENT*)GetProcAddress(tig_net_ws_dll, "WSASetEvent");
    tig_net_ws_procs.WSASocketA = (WS_WSASOCKETA*)GetProcAddress(tig_net_ws_dll, "WSASocketA");
    tig_net_ws_procs.WSASocketW = (WS_WSASOCKETW*)GetProcAddress(tig_net_ws_dll, "WSASocketW");
    tig_net_ws_procs.WSAWaitForMultipleEvents = (WS_WSAWAITFORMULTIPLEEVENTS*)GetProcAddress(tig_net_ws_dll, "WSAWaitForMultipleEvents");
    tig_net_ws_procs.WSAAddressToStringA = (WS_WSAADDRESSTOSTRINGA*)GetProcAddress(tig_net_ws_dll, "WSAAddressToStringA");
    tig_net_ws_procs.WSAAddressToStringW = (WS_WSAADDRESSTOSTRINGW*)GetProcAddress(tig_net_ws_dll, "WSAAddressToStringW");
    tig_net_ws_procs.WSAStringToAddressA = (WS_WSASTRINGTOADDRESSA*)GetProcAddress(tig_net_ws_dll, "WSAStringToAddressA");
    tig_net_ws_procs.WSAStringToAddressW = (WS_WSASTRINGTOADDRESSW*)GetProcAddress(tig_net_ws_dll, "WSAStringToAddressW");
    tig_net_ws_procs.WSALookupServiceBeginA = (WS_WSALOOKUPSERVICEBEGINA*)GetProcAddress(tig_net_ws_dll, "WSALookupServiceBeginA");
    tig_net_ws_procs.WSALookupServiceBeginW = (WS_WSALOOKUPSERVICEBEGINW*)GetProcAddress(tig_net_ws_dll, "WSALookupServiceBeginW");
    tig_net_ws_procs.WSALookupServiceNextA = (WS_WSALOOKUPSERVICENEXTA*)GetProcAddress(tig_net_ws_dll, "WSALookupServiceNextA");
    tig_net_ws_procs.WSALookupServiceNextW = (WS_WSALOOKUPSERVICENEXTW*)GetProcAddress(tig_net_ws_dll, "WSALookupServiceNextW");
    tig_net_ws_procs.WSALookupServiceEnd = (WS_WSALOOKUPSERVICEEND*)GetProcAddress(tig_net_ws_dll, "WSALookupServiceEnd");
    tig_net_ws_procs.WSAInstallServiceClassA = (WS_WSAINSTALLSERVICECLASSA*)GetProcAddress(tig_net_ws_dll, "WSAInstallServiceClassA");
    tig_net_ws_procs.WSAInstallServiceClassW = (WS_WSAINSTALLSERVICECLASSW*)GetProcAddress(tig_net_ws_dll, "WSAInstallServiceClassW");
    tig_net_ws_procs.WSARemoveServiceClass = (WS_WSAREMOVESERVICECLASS*)GetProcAddress(tig_net_ws_dll, "WSARemoveServiceClass");
    tig_net_ws_procs.WSAGetServiceClassInfoA = (WS_WSAGETSERVICECLASSINFOA*)GetProcAddress(tig_net_ws_dll, "WSAGetServiceClassInfoA");
    tig_net_ws_procs.WSAGetServiceClassInfoW = (WS_WSAGETSERVICECLASSINFOW*)GetProcAddress(tig_net_ws_dll, "WSAGetServiceClassInfoW");
    tig_net_ws_procs.WSAEnumNameSpaceProvidersA = (WS_WSAENUMNAMESPACEPROVIDERSA*)GetProcAddress(tig_net_ws_dll, "WSAEnumNameSpaceProvidersA");
    tig_net_ws_procs.WSAEnumNameSpaceProvidersW = (WS_WSAENUMNAMESPACEPROVIDERSW*)GetProcAddress(tig_net_ws_dll, "WSAEnumNameSpaceProvidersW");
    tig_net_ws_procs.WSAGetServiceClassNameByClassIdA = (WS_WSAGETSERVICECLASSNAMEBYCLASSIDA*)GetProcAddress(tig_net_ws_dll, "WSAGetServiceClassNameByClassIdA");
    tig_net_ws_procs.WSAGetServiceClassNameByClassIdW = (WS_WSAGETSERVICECLASSNAMEBYCLASSIDW*)GetProcAddress(tig_net_ws_dll, "WSAGetServiceClassNameByClassIdW");
    tig_net_ws_procs.WSASetServiceA = (WS_WSASETSERVICEA*)GetProcAddress(tig_net_ws_dll, "WSASetServiceA");
    tig_net_ws_procs.WSASetServiceW = (WS_WSASETSERVICEW*)GetProcAddress(tig_net_ws_dll, "WSASetServiceW");
    return true;
}

// 0x527AC0
void sub_527AC0(int client_id)
{
    tig_net_clients[client_id].flags |= TIG_NET_CLIENT_CONNECTED;
    tig_net_clients[client_id].client_id = client_id;
}

// 0x527AF0
int sub_527AF0()
{
    char hostname[80];
    struct hostent* h;
    char ip_address[16];

    tig_net_local_socket.addr.sin_family = AF_INET;
    tig_net_local_socket.addr.sin_port = tig_net_ws_procs.htons(TIG_NET_CLIENT_PORT);
    tig_net_local_socket.addr.sin_addr.s_addr = INADDR_ANY;
    memset(tig_net_local_socket.addr.sin_zero, 0, sizeof(tig_net_local_socket.addr.sin_zero));

    tig_net_ws_procs.gethostname(hostname, sizeof(hostname));

    h = tig_net_ws_procs.gethostbyname(hostname);
    if (h == NULL) {
        tig_debug_printf("TCP-NET: Error: could not get hostname failing start\n");
        return TIG_ERR_NET;
    }

    memcpy(&(tig_net_local_socket.addr.sin_addr), h->h_addr, h->h_length);

    tig_net_inet_string(tig_net_local_socket.addr.sin_addr.s_addr, ip_address);
    tig_debug_printf("TCP-NET: Hostname: %s, Address: %s\n", hostname, ip_address);

    return TIG_OK;
}

// 0x527B90
void tig_net_bookmark_init(TigNetServer* bookmark)
{
    memset(bookmark, 0, sizeof(*bookmark));
    bookmark->max_players = TIG_NET_MAX_PLAYERS;
    bookmark->options = 0;
    bookmark->type = TIG_NET_SERVER_TYPE_FREE_FOR_ALL;
    bookmark->max_level = 50;
    tig_timer_now(&(bookmark->time));
}

// 0x527BD0
bool tig_net_create_bcast_socket()
{
    HWND wnd;

    if ((tig_net_flags & TIG_NET_NO_BROADCAST) != 0) {
        return true;
    }

    tig_net_bcast_socket.s = tig_net_ws_procs.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (tig_net_bcast_socket.s == -1) {
        int err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Could not create the broadcast socket, WSAError: %s\n",
            strerror(err));
        return false;
    }

    wnd = NULL;

    if (tig_net_ws_procs.WSAAsyncSelect(tig_net_bcast_socket.s, wnd, TIG_NET_BCAST_ASYNC_SELECT, FD_READ) == -1) {
        int err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Error: BCAST Socket WSAAsyncSelect failed, WSAError: %s\n",
            strerror(err));
        return false;
    }

    int broadcast = 1;
    if (tig_net_ws_procs.setsockopt(tig_net_bcast_socket.s, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast)) == -1) {
        int err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Error: BCAST Socket setsockopt failed, WSAError: %s\n",
            strerror(err));
        return false;
    }

    if (tig_net_ws_procs.bind(tig_net_bcast_socket.s, (struct sockaddr*)&tig_net_bcast_socket.addr, sizeof(tig_net_bcast_socket.addr)) == -1) {
        char address[16];
        tig_net_inet_string(tig_net_bcast_socket.addr.sin_addr.s_addr, address);

        int err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Error: BCAST Socket BIND to( %s/%d )failed, WSAError: %s(%d)\n",
            address,
            tig_net_bcast_socket.addr.sin_port,
            strerror(err),
            err);
        return false;
    }

    INTERFACE_INFO interfaces[30];
    DWORD bytes_returned;
    if (tig_net_ws_procs.WSAIoctl != NULL && tig_net_ws_procs.WSAIoctl(tig_net_bcast_socket.s, SIO_GET_INTERFACE_LIST, NULL, 0, interfaces, sizeof(interfaces), &bytes_returned, NULL, NULL) != -1) {
        DWORD index;
        for (index = 0; index < bytes_returned / sizeof(INTERFACE_INFO); index++) {
            if ((interfaces[index].iiFlags & IFF_BROADCAST) != 0 && (interfaces[index].iiFlags & IFF_LOOPBACK) == 0) {
                break;
            }
        }

        tig_net_bcast_socket.addr.sin_addr.s_addr = ~interfaces[index].iiNetmask.AddressIn.sin_addr.s_addr
            | (interfaces[index].iiNetmask.AddressIn.sin_addr.s_addr & interfaces[index].iiAddress.AddressIn.sin_addr.s_addr);
    } else {
        // FIXME: If `WSAIoctl` is null (winsock 1.1), the error code returned
        // from `WSAGetLastError` will refer to previous call (`bind` in this
        // case).
        int err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Error: Primary Broadcast Method Failed: WSA Error %s\n",
            strerror(err));

        tig_net_bcast_socket.addr.sin_addr.s_addr = INADDR_BROADCAST;
        memset(&(tig_net_local_bookmark.addr), 0, sizeof(tig_net_local_bookmark.addr));
    }

    char address[16];
    tig_net_inet_string(tig_net_bcast_socket.addr.sin_addr.s_addr, address);
    tig_debug_printf("TCP-NET: Successfully bound broadcast address to %s/%d.\n",
        address,
        tig_net_bcast_socket.addr.sin_port);

    return true;
}

// 0x527E80
bool tig_net_start_server()
{
    HWND wnd;
    int index;
    int tcp_no_delay;

    tig_debug_printf("TCP-NET: Changing to Host-Attempt State, creating sockets.\n");

    if ((tig_net_flags & TIG_NET_CONNECTED) == 0) {
        if (tig_net_start_client() != TIG_OK) {
            // FIXME: Should be `false`.
            return TIG_ERR_NET;
        }
    }

    if (sub_527AF0() == TIG_ERR_NET) {
        // FIXME: Should be `false`.
        return TIG_ERR_NET;
    }

    // Reset all clients.
    for (index = 1; index < TIG_NET_MAX_PLAYERS; index++) {
        tig_net_client_info_exit(&(tig_net_clients[index]));
        tig_net_client_info_init(&(tig_net_clients[index]));
        tig_net_client_info_set_name(index, "");
    }

    tig_net_local_socket.s = tig_net_ws_procs.socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tig_net_local_socket.s == INVALID_SOCKET) {
        int err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Error could not create listen socket, WSAError: %s\n",
            strerror(err));
        return false;
    }

    tcp_no_delay = 1;
    if (tig_net_ws_procs.setsockopt(tig_net_local_socket.s, IPPROTO_TCP, TCP_NODELAY, (char*)&tcp_no_delay, sizeof(tcp_no_delay)) == -1) {
        int err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Error, could not set TCP_NODELAY on local socket, WSAError: %s(%d).\n",
            strerror(err),
            err);
        // NOTE: Processing not halted, not sure if it's a bug or intended
        // behaviour.
    }

    wnd = NULL;

    if (tig_net_ws_procs.WSAAsyncSelect(tig_net_local_socket.s, wnd, TIG_NET_LISTEN_ASYNC_SELECT, FD_CLOSE | FD_ACCEPT | FD_WRITE | FD_READ) == -1) {
        int err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Error: Listen socket WSAAsyncSelect failed, WSAError: %s(%d).\n",
            strerror(err),
            err);
        tig_net_ws_procs.closesocket(tig_net_local_socket.s);
        tig_net_local_socket.s = INVALID_SOCKET;
        return false;
    }

    if (tig_net_ws_procs.bind(tig_net_local_socket.s, (struct sockaddr*)&tig_net_local_socket.addr, sizeof(tig_net_local_socket.addr)) == -1) {
        // NOTE: Calls `WSAGetLastError` twice.
        tig_debug_printf("TCP-NET: Error Listen Socket bind failed, WSAError: %s(%d)\n",
            strerror(tig_net_ws_procs.WSAGetLastError()),
            tig_net_ws_procs.WSAGetLastError());
        tig_net_ws_procs.closesocket(tig_net_local_socket.s);
        tig_net_local_socket.s = INVALID_SOCKET;
        return false;
    }

    if (tig_net_ws_procs.listen(tig_net_local_socket.s, 5) == -1) {
        int err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Error Listen Socket listen failed, WSAError: %s\n",
            strerror(err));
        tig_net_ws_procs.closesocket(tig_net_local_socket.s);
        tig_net_local_socket.s = INVALID_SOCKET;
        return false;
    }

    if (tig_net_local_bookmark.addr.sin_port == 0) {
        int length = sizeof(tig_net_local_bookmark.addr);
        tig_net_ws_procs.getsockname(tig_net_local_socket.s, (struct sockaddr*)&(tig_net_local_bookmark.addr), &length);
        tig_net_local_bookmark.addr.sin_family = AF_INET;
        tig_net_local_bookmark.addr.sin_port = tig_net_ws_procs.htons(TIG_NET_CLIENT_PORT);
    }

    sub_5280E0();

    return true;
}

// 0x5280E0
void sub_5280E0()
{
    tig_net_flags |= TIG_NET_HOST;
}

// 0x5280F0
bool sub_5280F0()
{
    int index;

    for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
        if ((tig_net_clients[index].flags & TIG_NET_CLIENT_CONNECTED) != 0) {
            tig_net_xfer_remove_all(index);
            tig_net_ws_procs.closesocket(tig_net_clients[index].s);
            tig_net_clients[index].s = INVALID_SOCKET;
            tig_net_clients[index].flags &= ~TIG_NET_CLIENT_CONNECTED;
        }
    }

    if (tig_net_local_socket.s != INVALID_SOCKET) {
        tig_net_ws_procs.closesocket(tig_net_local_socket.s);
        tig_net_local_socket.s = INVALID_SOCKET;
    }

    if (tig_net_bcast_socket.s != INVALID_SOCKET) {
        tig_net_ws_procs.closesocket(tig_net_bcast_socket.s);
        tig_net_bcast_socket.s = INVALID_SOCKET;
    }

    tig_net_ws_exit();
    sub_526E00();

    return true;
}

// 0x528170
bool tig_net_connect_to_server(int server_id, void* buf, int size)
{
    TigNetServer server;
    TigNetJoinRequestPacket* pkt;

    pkt = (TigNetJoinRequestPacket*)MALLOC(sizeof(TigNetJoinRequestPacket) + size);
    pkt->msg.field_34 = dword_62AC3C;

    if (tig_net_local_server_password != NULL) {
        strncpy(pkt->msg.password, tig_net_local_server_password, TIG_NET_PASSWORD_NAME_SIZE - 1);
    } else {
        pkt->msg.password[0] = '\0';
    }

    // NOTE: Signed compare.
    if (size > 0) {
        memcpy(pkt + 1, buf, size);
    }

    if (!tig_idxtable_get(&tig_net_bookmarks, server_id, &server)) {
        tig_debug_printf("TCP-NET: Unknown server index %d.\n", server_id);
        return false;
    }

    tig_net_local_socket.addr = server.addr;
    if (!tig_net_connect_internal(&tig_net_local_socket, server.name)) {
        return false;
    }

    pkt->hdr.size = (uint16_t)(sizeof(TigNetJoinRequestPacket) + size);
    pkt->hdr.type = TIG_NET_SERVER_CMD;
    pkt->hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt->msg.type = TIG_NET_JOIN_REQUEST;
    pkt->msg.extra_size = size;
    strncpy(pkt->msg.name, tig_net_local_client.name, TIG_NET_CLIENT_NAME_LENGTH - 1);

    sub_529830(&(pkt->hdr), pkt->hdr.size);

    return true;
}

// 0x5282A0
bool tig_net_connect_internal(TigNetSocket* sock, const char* server_name)
{
    HWND wnd;
    int err;
    int tcp_no_delay;

    if (tig_net_on_network_event_func != NULL) {
        tig_net_on_network_event_func(4, 0, 0, 0);
    }

    wnd = NULL;

    sock->s = tig_net_ws_procs.socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock->s == INVALID_SOCKET) {
        tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: could not create socket.\n");
        return false;
    }

    if (tig_net_ws_procs.connect(sock->s, (struct sockaddr*)&(sock->addr), 16) == -1) {
        err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Connect to %s failed, reason WSAError: %s\n",
            server_name,
            strerror(err));
        tig_net_ws_procs.closesocket(sock->s);
        sock->s = INVALID_SOCKET;

        return false;
    }

    tig_debug_printf("TCP-NET: Connected to server %s...\n", server_name);

    tcp_no_delay = 1;
    if (tig_net_ws_procs.setsockopt(sock->s, IPPROTO_TCP, TCP_NODELAY, (const char*)&tcp_no_delay, sizeof(tcp_no_delay)) == -1) {
        err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Error, could not set TCP_NODELAY on local socket, WSAError: %s(%d).\n",
            strerror(err),
            err);
    }

    return true;
}

// 0x5283C0
bool tig_net_connect_directly(const char* cp, void* buf, int size)
{
    TigNetJoinRequestPacket* pkt;
    unsigned long addr;

    pkt = (TigNetJoinRequestPacket*)MALLOC(sizeof(TigNetJoinRequestPacket) + size);
    pkt->msg.field_34 = dword_62AC3C;

    if (tig_net_local_server_password != NULL) {
        strncpy(pkt->msg.password, tig_net_local_server_password, TIG_NET_PASSWORD_NAME_SIZE - 1);
    } else {
        pkt->msg.password[0] = '\0';
    }

    if (size > 0) {
        memcpy(pkt + 1, buf, size);
    }

    tig_debug_printf("TCP-NET: Attempting to directly connect to %s.\n", cp);

    if (tig_net_inet_addr(cp, &addr) != 0) {
        if (!tig_net_is_ip_address(cp)
            && tig_net_ws_procs.gethostbyaddr((const char*)&addr, sizeof(addr), AF_INET) == NULL) {
            tig_debug_printf("TCP-NET: Connect to %s failed, reason gethostbyaddr failed, WSAError: %d\n", tig_net_ws_procs.WSAGetLastError());
            return false;
        }

        tig_debug_printf("TCP-NET: Host address %s valid, connecting...\n", cp);
        tig_net_local_socket.addr.sin_family = AF_INET;
        tig_net_local_socket.addr.sin_port = tig_net_ws_procs.htons(TIG_NET_CLIENT_PORT);
        tig_net_local_socket.addr.sin_addr.s_addr = INADDR_ANY;
        memset(tig_net_local_socket.addr.sin_zero, 0, sizeof(tig_net_local_socket.addr.sin_zero));

        if (!tig_net_connect_internal(&tig_net_local_socket, cp)) {
            return false;
        }
    }

    pkt->hdr.size = (uint16_t)(sizeof(TigNetJoinRequestPacket) + size);
    pkt->hdr.type = TIG_NET_SERVER_CMD;
    pkt->hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt->msg.type = TIG_NET_JOIN_REQUEST;
    pkt->msg.extra_size = size;
    strncpy(pkt->msg.name, tig_net_local_client.name, TIG_NET_CLIENT_NAME_LENGTH - 1);

    sub_529830(&(pkt->hdr), pkt->hdr.size);

    return true;
}

// 0x528510
bool tig_net_is_ip_address(const char* cp)
{
    while (*cp != '\0') {
        if (*cp != '.' && (*cp < '0' || *cp > '9')) {
            return false;
        }
    }

    return true;
}

// 0x528550
bool tig_net_reset_connection()
{
    TigNetServerDisconnectPacket server_disconnect_pkt;
    TigNetClientDisconnectPacket client_disconnect_pkt;
    int index;

    tig_debug_printf("TCP-NET: Resetting Connection.\n");

    if ((tig_net_flags & TIG_NET_HOST) != 0) {
        server_disconnect_pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
        server_disconnect_pkt.hdr.type = TIG_NET_SERVER_CMD;
        server_disconnect_pkt.hdr.size = sizeof(server_disconnect_pkt);
        server_disconnect_pkt.msg.type = TIG_NET_SERVER_DISCONNECT;

        for (index = 1; index < TIG_NET_MAX_PLAYERS; index++) {
            if (tig_net_client_is_active(index)) {
                tig_net_send_to_client_internal(&(server_disconnect_pkt.hdr), server_disconnect_pkt.hdr.size, index);
            }
        }

        for (index = 1; index < TIG_NET_MAX_PLAYERS; index++) {
            if (tig_net_client_is_active(index)) {
                tig_net_xfer_remove_all(index);
                tig_net_ws_procs.closesocket(tig_net_clients[index].s);
                tig_net_client_info_set_name(tig_net_clients[index].client_id, "");
                tig_net_client_info_exit(&(tig_net_clients[index]));
                tig_net_client_info_init(&(tig_net_clients[index]));
            }
        }

        tig_net_ws_procs.closesocket(tig_net_local_socket.s);
        tig_net_local_socket.s = INVALID_SOCKET;

        tig_net_ws_procs.closesocket(tig_net_bcast_socket.s);
        tig_net_bcast_socket.s = INVALID_SOCKET;
    } else {
        if (sub_529520() > 1 && tig_net_client_is_active(sub_529520())) {
            client_disconnect_pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
            client_disconnect_pkt.hdr.type = TIG_NET_SERVER_CMD;
            client_disconnect_pkt.hdr.size = sizeof(client_disconnect_pkt);

            tig_debug_printf("TCP-NET: Local Client #%d (%s) is Quitting.\n",
                sub_529520(),
                tig_net_client_info_get_name(sub_529520()));

            client_disconnect_pkt.msg.type = TIG_NET_CLIENT_DISCONNECT;
            client_disconnect_pkt.msg.reason = TIG_NET_DISCONNECT_REASON_LEAVE;
            client_disconnect_pkt.msg.client_id = sub_529520();
            tig_net_send_app_all(&(client_disconnect_pkt.msg), sizeof(client_disconnect_pkt.msg));
        } else {
            tig_net_xfer_remove_all(0);
            tig_net_ws_procs.closesocket(tig_net_local_socket.s);
        }
    }

    return true;
}

// 0x5286E0
bool sub_5286E0()
{
    tig_net_flags |= TIG_NET_WAITING;

    if (tig_net_on_network_event_func != NULL) {
        tig_net_on_network_event_func(11, -1, 0, 0);
    }

    return true;
}

// 0x528710
void tig_net_on_network_event(TigNetOnNetworkEvent* func)
{
    tig_net_on_network_event_func = func;
}

// 0x528720
void tig_net_on_message(TigNetOnMessage* func)
{
    tig_net_on_message_func = func;
}

// 0x528730
void tig_net_on_join_response(TigNetOnJoinResponse* func)
{
    tig_net_on_join_response_func = func;
}

// 0x528740
void tig_net_on_servers_change(TigNetOnServersChange* func)
{
    tig_net_on_servers_change_func = func;
}

// 0x528750
void tig_net_on_message_validation(TigNetOnMessageValidation* func)
{
    tig_net_on_message_validation_func = func;
}

// 0x528760
void tig_net_auto_join_enable()
{
    tig_net_flags |= TIG_NET_AUTO_JOIN;
}

// 0x528770
void tig_net_auto_join_disable()
{
    tig_net_flags &= ~TIG_NET_AUTO_JOIN;
}

// 0x528780
bool tig_net_auto_join_is_enabled()
{
    return (tig_net_flags & TIG_NET_AUTO_JOIN) != 0;
}

// 0x528790
void sub_528790()
{
    dword_62AC4C++;
}

// 0x5287A0
void sub_5287A0()
{
    dword_62AC4C--;
}

// 0x5287B0
void tig_net_ping()
{
    TigNetServer* servers;
    int num_servers;
    int index;
    TigNetPingPacket ping_pkt;

    if ((tig_net_flags & TIG_NET_CONNECTED) == 0
        || dword_62AC4C != 0) {
        return;
    }

    tig_net_msg_read_all();

    if ((tig_net_flags & TIG_NET_RESET) != 0) {
        tig_net_reset();
        return;
    }

    tig_net_xfer_ping();

    if ((tig_net_flags & TIG_NET_HOST) != 0) {
        if ((tig_net_flags & TIG_NET_NO_BROADCAST) == 0
            && tig_timer_between(tig_net_server_bcast_timestamp, tig_ping_timestamp) > 5000) {
            tig_net_server_bcast_timestamp = tig_ping_timestamp;
            tig_net_server_bcast_alive();
        }

        if (tig_timer_between(dword_62AC78, tig_ping_timestamp) > 350) {
            dword_62AC78 = tig_ping_timestamp;

            ping_pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
            ping_pkt.hdr.type = TIG_NET_BCAST_PING;
            ping_pkt.hdr.size = sizeof(ping_pkt);
            ping_pkt.type = 1;
            ping_pkt.client_id = tig_net_local_client.client_id;

            for (index = 1; index < TIG_NET_MAX_PLAYERS; index++) {
                if (tig_net_client_is_active(index)) {
                    ping_pkt.client_id = index;
                    tig_net_send_to_client_internal(&(ping_pkt.hdr), ping_pkt.hdr.size, index);
                }
                sub_526D60(index);
            }
        }
    } else {
        if ((tig_net_flags & TIG_NET_NO_BROADCAST) == 0
            && tig_timer_between(tig_net_remove_unresponsive_servers_timestamp, tig_ping_timestamp) > 5000) {
            tig_net_remove_unresponsive_servers_timestamp = tig_ping_timestamp;

            if (tig_net_remove_unresponsive_servers()) {
                if (tig_net_on_servers_change_func != NULL) {
                    tig_net_get_servers(&servers, &num_servers);
                    tig_net_on_servers_change_func(servers, num_servers);
                }
            }
        }

        if (tig_net_local_socket.s != INVALID_SOCKET
            && tig_timer_between(dword_62AC80, tig_ping_timestamp) > 350) {
            dword_62AC80 = tig_ping_timestamp;

            ping_pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
            ping_pkt.hdr.type = TIG_NET_BCAST_PING;
            ping_pkt.hdr.size = sizeof(ping_pkt);
            ping_pkt.type = 1;
            ping_pkt.client_id = tig_net_local_client.client_id;
            tig_timer_now(&(ping_pkt.timestamp));
            sub_529830(&(ping_pkt.hdr), ping_pkt.hdr.size);
            sub_526D60(0);
        }
    }
}

// 0x5289B0
void sub_5289B0(struct sockaddr_in* to)
{
    TigNetAlivePacket pkt;

    if ((tig_net_flags & TIG_NET_HOST) != 0) {
        return;
    }

    pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt.hdr.type = TIG_NET_ALIVE;
    pkt.hdr.size = sizeof(pkt);
    pkt.type = 1;
    pkt.addr = tig_net_local_socket.addr;
    pkt.addr.sin_port = tig_net_ws_procs.htons(TIG_NET_SERVER_PORT);
    tig_timer_now(&(pkt.time));

    tig_net_ws_procs.sendto(tig_net_bcast_socket.s,
        (char*)&(pkt.hdr),
        sizeof(pkt),
        0,
        (struct sockaddr*)to,
        sizeof(*to));
}

// 0x528A50
void tig_net_msg_read_all()
{
    int index;
    struct timeval timeout;
    fd_set readfds;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    readfds.fd_count = 0;

    if ((tig_net_flags & TIG_NET_HOST) != 0) {
        for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
            if (tig_net_clients[index].s != INVALID_SOCKET) {
                unsigned int fd_index;
                for (fd_index = 0; fd_index < readfds.fd_count; fd_index++) {
                    if (readfds.fd_array[fd_index] == tig_net_clients[index].s) {
                        break;
                    }
                }

                if (fd_index == readfds.fd_count && readfds.fd_count < FD_SETSIZE) {
                    readfds.fd_array[readfds.fd_count++] = tig_net_clients[index].s;
                }
            }
        }

        if (readfds.fd_count != 0) {
            tig_net_ws_procs.select(0, &readfds, NULL, NULL, &timeout);

            for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
                if (tig_net_clients[index].s != INVALID_SOCKET) {
                    // NOTE: Original code does something odd here.
                    if (tig_net_fd_is_set(tig_net_clients[index].s, &readfds)) {
                        tig_net_msg_read(tig_net_clients[index].s);
                    }
                }
            }
        }
    } else {
        if (tig_net_local_socket.s != INVALID_SOCKET) {
            readfds.fd_array[readfds.fd_count++] = tig_net_local_socket.s;
            tig_net_ws_procs.select(0, &readfds, NULL, NULL, &timeout);
            if (tig_net_fd_is_set(tig_net_local_socket.s, &readfds)) {
                tig_net_msg_read(tig_net_local_socket.s);
            }
        }
    }
}

// 0x528B70
void tig_net_msg_read(SOCKET s)
{
    TigNetClient* client;
    int recv_bytes;
    int err;
    int cnt;
    int parsed_bytes;

    client = tig_net_get_client_by_socket(s);
    if (client == NULL) {
        tig_debug_printf("TCP-NET: tig_net_msg_read: unknown client for socket %d.\n", s);
        return;
    }

    memset(client->field_2038, -1, client->field_2040);
    recv_bytes = tig_net_ws_procs.recv(s, (char*)client->field_2038, client->field_2040, 0);
    if (recv_bytes == -1) {
        err = tig_net_ws_procs.WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            tig_debug_printf("TCP-NET: Would Have Blocked In Read... Continuing.\n");
        } else if (err == WSANOTINITIALISED) {
            tig_debug_printf("TCP-NET: We're not initialized, I cannot read.\n");
        } else {
            if (err == WSAECONNRESET) {
                tig_debug_printf("TCP-NET: Remote has reset the connection.\n");
                if ((tig_net_flags & TIG_NET_HOST) != 0) {
                    tig_net_shutdown(client->client_id);
                } else {
                    tig_net_ws_procs.shutdown(client->client_id, SD_SEND);
                }
            }

            tig_debug_printf("TCP-NET: Error Reading: recv_bytes=%d, WSAError: %s (%d).\n",
                recv_bytes,
                strerror(err),
                err);
        }
        return;
    }

    if (recv_bytes == 0) {
        tig_debug_printf("TCP-NET: recv_bytes is 0, socket was gracefully closed.\n");
        tig_net_close(s);
        return;
    }

    client->field_2038 += recv_bytes;
    client->field_2040 -= recv_bytes;
    client->stats.bytes_received += recv_bytes;

    if ((tig_net_flags & TIG_NET_HOST) != 0) {
        tig_net_clients[0].stats.bytes_received += recv_bytes;
    }

    cnt = 0;
    while (tig_net_parse_packet(client->client_id)) {
        cnt++;
        tig_net_process_packet(client->client_id);
    }

    parsed_bytes = client->field_2038 - client->field_203C;
    if (client->client_id != -1) {
        memcpy(client->data, client->field_203C, parsed_bytes);
        client->field_203C = client->data;
        client->field_2040 = TIG_NET_BUF_SIZE - parsed_bytes;
        client->field_2038 = &(client->data[parsed_bytes]);

        if (tig_net_client_is_shutting_down(client->client_id)) {
            tig_net_client_info_set_name(client->client_id, "");
            tig_net_client_info_exit(client);
            tig_net_client_info_init(client);
        }
    }
}

// 0x528D60
TigNetClient* tig_net_get_client_by_socket(SOCKET s)
{
    int index;

    if ((tig_net_flags & TIG_NET_HOST) == 0 && s == tig_net_local_socket.s) {
        return &tig_net_local_client;
    }

    for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
        if (tig_net_client_is_active(index) && tig_net_clients[index].s == s) {
            return &(tig_net_clients[index]);
        }
    }

    return NULL;
}

// 0x528DD0
void tig_net_close(SOCKET s)
{
    TigNetClient* client = tig_net_get_client_by_socket(s);

    tig_net_ws_procs.closesocket(s);

    if (client != NULL) {
        int client_id;
        if ((tig_net_flags & TIG_NET_HOST) != 0) {
            client_id = client->client_id;
        } else {
            client = &(tig_net_clients[0]);
            client_id = 0;
        }

        if (tig_net_on_network_event_func != NULL) {
            if (client_id != -1) {
                tig_net_on_network_event_func(8, client_id, NULL, 0);
            } else {
                int v1 = 0;
                tig_net_on_network_event_func(8, -1, &v1, 4);
            }
        }

        tig_net_xfer_remove_all(client_id);

        tig_debug_printf("TCP-NET: Client %s (slot %d) is disconnecting.\n", client->name, client_id);

        tig_net_client_info_set_name(client_id, "");
        tig_net_client_info_exit(client);
        tig_net_client_info_init(client);

        if (tig_net_on_network_event_func != NULL) {
            tig_net_on_network_event_func(0, client_id, NULL, 0);
        }
    }
}

// 0x528E80
void tig_net_bcast_async_select(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    uint8_t buffer[1024];
    struct sockaddr from;
    int from_len = sizeof(from);

    (void)hWnd;

    if ((tig_net_flags & TIG_NET_NO_BROADCAST) != 0) {
        tig_debug_printf("TCP-NET: -BCAST- -mpnobcast is set, yet we're getting a bcast message. WHAT? Exiting. (remove for release).\n");
        exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE.
    }

    if (WSAGETSELECTERROR(lParam) != 0) {
        tig_debug_printf("TCP-NET: -BCAST- AsyncSelect Error: %s (%d).\n",
            strerror(WSAGETSELECTERROR(lParam)),
            WSAGETSELECTERROR(lParam));
    }

    switch (WSAGETSELECTEVENT(lParam)) {
    case FD_READ:
        if (tig_net_ws_procs.recvfrom((SOCKET)wParam, (char*)buffer, sizeof(buffer), 0, &from, &from_len) != -1) {
            sub_529C20((TigNetPacketHeader*)buffer, 0);
        } else {
            // NOTE: This value is ignored.
            tig_net_ws_procs.WSAGetLastError();
        }
        break;
    case FD_ACCEPT:
        tig_debug_printf("TCP-NET: -BCAST- AsyncMessage: Broadcast Should Not Receive An Accept Message: Socket %d.\n", (SOCKET)wParam);
        break;
    case FD_CONNECT:
        tig_debug_printf("TCP-NET: -BCAST- AsyncMessage: Broadcast Should Not Receive A Connect Message: Socket %d.\n", (SOCKET)wParam);
        break;
    case FD_CLOSE:
        tig_debug_printf("TCP-NET: -BCAST- AsyncMessage: Broadcast Should Not Receive A Close Message: Socket %d.\n", (SOCKET)wParam);
        break;
    }
}

// 0x528FD0
void tig_net_listen_async_select(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if (dword_62AC4C != 0) {
        PostMessageA(hWnd, TIG_NET_LISTEN_ASYNC_SELECT, wParam, lParam);
        return;
    }

    if (WSAGETSELECTERROR(lParam) != 0) {
        if (WSAGETSELECTERROR(lParam) == 0x2745) {
            return;
        }

        tig_debug_printf("TCP-NET: AsyncSelect Error: %s (%d).\n",
            strerror(WSAGETSELECTERROR(lParam)),
            WSAGETSELECTERROR(lParam));
    }

    switch (WSAGETSELECTEVENT(lParam)) {
    case FD_READ:
        tig_net_msg_read((SOCKET)wParam);
        break;
    case FD_WRITE:
        tig_debug_printf("TCP-NET: AsyncMessage: Socket %d Write.\n", (SOCKET)wParam);
        break;
    case FD_ACCEPT:
        tig_net_accept(hWnd, wParam);
        break;
    case FD_CONNECT:
        tig_debug_printf("TCP-NET: AsyncMessage: Socket %d Connect.\n", (SOCKET)wParam);
        break;
    case FD_CLOSE:
        tig_net_close((SOCKET)wParam);
        break;
    }
}

// 0x5290E0
void tig_net_accept(HWND hWnd, WPARAM wParam)
{
    int addr_len = sizeof(struct sockaddr_in);
    int client_id;
    int tcp_no_delay;
    char ip_address[16];
    TigNetDisallowJoinPacket disallow_join_pkt;
    struct sockaddr_in addr;
    SOCKET s;
    int err;

    (void)hWnd;

    client_id = tig_net_client_get_free_slot();
    if (client_id != -1) {
        tig_net_clients[client_id].s = tig_net_ws_procs.accept((SOCKET)wParam, (struct sockaddr*)&(tig_net_clients[client_id].addr), &addr_len);
        if (tig_net_clients[client_id].s == INVALID_SOCKET) {
            err = tig_net_ws_procs.WSAGetLastError();
            tig_debug_printf("TCP-NET: Could not accept new socket in slot %d, WSAError: %s\n",
                client_id,
                strerror(err));
            return;
        }

        tcp_no_delay = 1;
        if (tig_net_ws_procs.setsockopt(tig_net_clients[client_id].s, IPPROTO_TCP, TCP_NODELAY, (char*)&tcp_no_delay, sizeof(tcp_no_delay)) == -1) {
            err = tig_net_ws_procs.WSAGetLastError();
            tig_debug_printf("TCP-NET: Could not set TCP_NODELAY on socket, WSAError: %s(%d)\n",
                strerror(err),
                err);
            // NOTE: Processing not halted, not sure if it's a bug or intended
            // behaviour.
        }

        sub_527AC0(client_id);
        tig_net_client_set_waiting(client_id);
        tig_timer_now(&(tig_net_clients[client_id].stats.accept_timestamp));

        tig_net_inet_string(tig_net_clients[client_id].addr.sin_addr.s_addr, ip_address);
        tig_debug_printf("TCP-NET: New connection in slot %d, from %s\n.",
            client_id,
            ip_address);
    } else {
        disallow_join_pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
        disallow_join_pkt.hdr.type = TIG_NET_SERVER_CMD;
        disallow_join_pkt.hdr.size = sizeof(disallow_join_pkt);
        disallow_join_pkt.msg.type = TIG_NET_DISALLOW_JOIN;

        s = tig_net_ws_procs.accept((SOCKET)wParam, (struct sockaddr*)&addr, &addr_len);
        if (s == INVALID_SOCKET) {
            err = tig_net_ws_procs.WSAGetLastError();
            tig_debug_printf("TCP-NET: Could not fit a new player, but their socket could not connect, ignoring. WSAError: %s\n",
                strerror(err));
            return;
        }

        tcp_no_delay = 1;
        if (tig_net_ws_procs.setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&tcp_no_delay, sizeof(tcp_no_delay)) == -1) {
            err = tig_net_ws_procs.WSAGetLastError();
            tig_debug_printf("TCP-NET: Could not set TCP_NODELAY on socket, WSAError %s(%d)\n",
                strerror(err),
                err);
            // NOTE: As opposed to the case above the processing is halted here.
            // However it looks like we're leaking socket.
            return;
        }

        tig_net_send_to_socket(&(disallow_join_pkt.hdr), disallow_join_pkt.hdr.size, s);
        tig_net_ws_procs.shutdown(s, SD_SEND);
    }
}

// 0x5292B0
int tig_net_client_get_free_slot()
{
    int index;

    for (index = 0; index < tig_net_local_server_get_max_players(); index++) {
        if ((tig_net_clients[index].flags & TIG_NET_CONNECTED) == 0) {
            return index;
        }
    }

    return -1;
}

// 0x5292F0
bool tig_net_local_server_set_description(const char* description)
{
    strncpy(tig_net_local_bookmark.description, description, TIG_NET_DESCRIPTION_SIZE);
    return true;
}

// 0x529310
bool tig_net_local_server_set_max_players(int max_players)
{
    if (max_players <= 0 || max_players > TIG_NET_MAX_PLAYERS) {
        return false;
    }

    tig_net_local_bookmark.max_players = max_players;
    return true;
}

// 0x529330
int tig_net_local_server_get_max_players()
{
    return tig_net_local_bookmark.max_players;
}

// 0x529340
bool tig_net_local_client_set_name(const char* name)
{
    strncpy(tig_net_local_client.name, name, TIG_NET_CLIENT_NAME_LENGTH - 1);
    return true;
}

// 0x529360
const char* tig_net_local_client_get_name()
{
    return tig_net_local_client.name;
}

// 0x529370
void tig_net_local_server_set_name(const char* name)
{
    strncpy(tig_net_local_bookmark.name, name, TIG_NET_CLIENT_NAME_LENGTH - 1);
}

// 0x529390
void tig_net_local_server_get_name(char* dest, size_t size)
{
    strncpy(dest, tig_net_local_bookmark.name, size);
}

// 0x5293B0
void sub_5293B0(TigNetServer** bookmark_ptr)
{
    *bookmark_ptr = &tig_net_local_bookmark;
}

// 0x5293C0
void tig_net_local_server_set_password(const char* password)
{
    if (tig_net_local_server_password != NULL) {
        FREE(tig_net_local_server_password);
        tig_net_local_server_password = NULL;
        tig_net_local_server_disable_option(TIG_NET_SERVER_PASSWORD_PROTECTED);
    }

    if (password != NULL && *password != '\0') {
        tig_net_local_server_password = STRDUP(password);
        tig_net_local_server_enable_option(TIG_NET_SERVER_PASSWORD_PROTECTED);
    }
}

// 0x529410
size_t tig_net_local_server_get_password_length()
{
    return tig_net_local_server_password != NULL ? strlen(tig_net_local_server_password) : 0;
}

// 0x529430
void tig_net_local_server_clear_password()
{
    tig_net_local_server_set_password(NULL);
}

// 0x529440
void tig_net_local_server_get_level_range(int* min_level_ptr, int* max_level_ptr)
{
    if (min_level_ptr != NULL) {
        *min_level_ptr = tig_net_local_bookmark.min_level;
    }

    if (max_level_ptr != NULL) {
        *max_level_ptr = tig_net_local_bookmark.max_level;
    }
}

// 0x529470
void tig_net_local_server_set_level_range(int min_level, int max_level)
{
    if (min_level >= 0 && max_level > 0) {
        tig_net_local_bookmark.min_level = min_level > max_level ? max_level : min_level;
        tig_net_local_bookmark.max_level = max_level;
    }
}

// 0x5294A0
void tig_net_local_server_type_get(int* type_ptr)
{
    if (type_ptr != NULL) {
        *type_ptr = tig_net_local_bookmark.type;
    }
}

// 0x5294C0
void tig_net_local_bookmark_set_type(int type)
{
    tig_net_local_bookmark.type = type;
}

// 0x5294D0
int tig_net_local_server_get_options()
{
    return tig_net_local_bookmark.options;
}

// 0x5294E0
void tig_net_local_server_enable_option(int flag)
{
    tig_net_local_bookmark.options |= flag;
}

// 0x529500
void tig_net_local_server_disable_option(int flag)
{
    tig_net_local_bookmark.options &= ~flag;
}

// 0x529520
int sub_529520()
{
    return (tig_net_flags & TIG_NET_HOST) == 0 ? tig_net_local_client.client_id : 0;
}

// 0x529540
void tig_net_send_generic(void* data, int size, uint8_t type, int client_id)
{
    // 0x612844
    static TigNetPacket pkt;

    if (size + sizeof(pkt.hdr) > TIG_NET_BUF_SIZE) {
        tig_debug_printf("TCP-NET: send_generic(): too big a buffer requested %d!\n", size);
        return;
    }

    pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt.hdr.type = type;
    pkt.hdr.size = (uint16_t)(size + sizeof(pkt.hdr));
    memcpy(pkt.data, data, size);

    if ((tig_net_flags & TIG_NET_HOST) != 0) {
        if (tig_net_client_is_active(client_id)) {
            if (!tig_net_client_is_waiting(client_id)) {
                tig_net_send_to_client_internal(&(pkt.hdr), pkt.hdr.size, client_id);
            }
        }
    } else {
        sub_529830(&(pkt.hdr), pkt.hdr.size);
    }
}

// 0x5295F0
void tig_net_send_app_all(void* data, int size)
{
    // 0x628C38
    static TigNetPacket pkt;

    int index;

    pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt.hdr.type = TIG_NET_APP;
    pkt.hdr.size = (uint16_t)(size + sizeof(pkt.hdr));
    memcpy(pkt.data, data, size);

    if ((tig_net_flags & TIG_NET_HOST) != 0) {
        for (index = 1; index < TIG_NET_MAX_PLAYERS; index++) {
            if (tig_net_client_is_active(index)) {
                if (!tig_net_client_is_waiting(index)) {
                    tig_net_send_to_client_internal(&(pkt.hdr), pkt.hdr.size, index);
                }
            }
        }
    } else {
        sub_529830(&(pkt.hdr), pkt.hdr.size);
    }
}

// 0x529690
void tig_net_send_app_except(void* data, int size, int client_id)
{
    // 0x6168B8
    static TigNetPacket pkt;

    int index;

    pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt.hdr.type = TIG_NET_APP;
    pkt.hdr.size = (uint16_t)(size + sizeof(pkt.hdr));
    memcpy(pkt.data, data, size);

    if ((tig_net_flags & TIG_NET_HOST) != 0) {
        for (index = 1; index < TIG_NET_MAX_PLAYERS; index++) {
            if (index != client_id) {
                if (tig_net_client_is_active(index)) {
                    if (!tig_net_client_is_waiting(index)) {
                        tig_net_send_to_client_internal(&(pkt.hdr), pkt.hdr.size, index);
                    }
                }
            }
        }
    }
}

// 0x529730
void tig_net_send_app(void* data, int size, int client_id)
{
    // 0x6103A8
    static TigNetPacket pkt;

    pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt.hdr.type = TIG_NET_APP;
    pkt.hdr.size = (uint16_t)(size + sizeof(pkt.hdr));
    memcpy(pkt.data, data, size);

    tig_net_send_to_client_internal(&(pkt.hdr), pkt.hdr.size, client_id);
}

// 0x529790
bool tig_net_send_to_client_internal(TigNetPacketHeader* header, uint16_t size, int client_id)
{
    header->version = TIG_NET_PROTOCOL_VERSION;
    header->size = size;
    tig_net_clients[client_id].stats.bytes_sent += size;

    if ((tig_net_flags & TIG_NET_HOST) != 0) {
        tig_net_clients[0].stats.bytes_sent += size;
    }

    if (!tig_net_client_is_active(client_id)) {
        tig_debug_printf("TCP-NET: Error: tig_net_send_to_client_internal on non active client, slot %d\n", client_id);
        return false;
    }

    if (!tig_net_send_to_socket(header, size, tig_net_clients[client_id].s)) {
        tig_net_clients[client_id].s = INVALID_SOCKET;
        tig_net_xfer_remove_all(client_id);
        return false;
    }

    return true;
}

// 0x529830
bool sub_529830(TigNetPacketHeader* header, uint16_t size)
{
    header->version = TIG_NET_PROTOCOL_VERSION;
    header->size = size;

    if (!tig_net_send_to_socket(header, size, tig_net_local_socket.s)) {
        tig_net_local_socket.s = INVALID_SOCKET;
        tig_net_xfer_remove_all(0);

        return false;
    }

    return true;
}

// 0x529870
bool tig_net_send_to_socket(TigNetPacketHeader* header, int size, SOCKET s)
{
    char* bytes;
    int remaining_bytes;
    int send_bytes;
    int err;

    tig_net_validate_packet(header, "tig_net_send_to_socket");

    bytes = (char*)header;
    remaining_bytes = size;

    for (;;) {
        send_bytes = tig_net_ws_procs.send(s, bytes, remaining_bytes, 0);
        if (send_bytes >= size) {
            return true;
        }

        if (send_bytes == -1) {
            err = tig_net_ws_procs.WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                // Retry.
                continue;
            }

            if (err == WSAECONNRESET) {
                tig_debug_printf("TCP-NET: Cannot send to socket %d, it is being reset by peer.\n",
                    s);
                tig_net_close(s);
            } else {
                tig_debug_printf("TCP-NET: Cannot send to socket %d, WSAError %s(%d)...\n",
                    s,
                    strerror(err),
                    err);
            }

            return false;
        }

        bytes += send_bytes;
        remaining_bytes -= send_bytes;
    }
}

// 0x529920
bool tig_net_validate_packet(TigNetPacketHeader* header, const char* msg)
{
    bool valid;

    valid = tig_net_can_parse_packet(header, msg);

    switch (header->type) {
    case TIG_NET_GENERIC:
        tig_debug_printf("TCP-NET: Error: VALIDATION(%s): sending generic packet type.\n", msg);
        valid = false;
        break;
    case TIG_NET_SERVER_CMD:
        switch (*(int*)(header + 1)) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 7:
        case 8:
            break;
        default:
            valid = false;
            break;
        }
        break;
    case TIG_NET_APP:
        if (tig_net_on_message_validation_func != NULL && !tig_net_on_message_validation_func(header + 1)) {
            valid = false;
        }
        break;
    case TIG_NET_BCAST_PING:
    case TIG_NET_ALIVE:
    case 9:
    case 10:
    case 11:
        break;
    default:
        tig_debug_printf("TCP-NET: Error: VALIDATION(%s): sending packet with unhandled header type (%d).\n", header->type);
        break;
    }

    return valid;
}

// 0x529A00
bool tig_net_can_parse_packet(TigNetPacketHeader* header, const char* msg)
{
    bool can_parse = true;

    if (header->version != TIG_NET_PROTOCOL_VERSION) {
        tig_debug_printf("TCP-NET: Error: VALIDATION(%s): header->version (0x%X) != TIG_NET_PROTOCOL_VERSION (0x%X)\n",
            msg,
            header->version,
            TIG_NET_PROTOCOL_VERSION);
        can_parse = false;
    }

    if (header->size > TIG_NET_BUF_SIZE) {
        tig_debug_printf("TCP-NET: Error: VALIDATION(%s): header->size (%d) > TIG_NET_BUF_SIZE (%d)\n",
            msg,
            header->size,
            TIG_NET_BUF_SIZE);
        can_parse = false;
    } else if (header->size == 0) {
        tig_debug_printf("TCP-NET: Error: VALIDATION(%s): header->size == %d\n",
            msg,
            0);
        can_parse = false;
    }

    return can_parse;
}

// 0x529A80
bool tig_net_parse_packet(int client_id)
{
    TigNetPacketHeader* hdr;

    if (client_id == tig_net_local_client.client_id) {
        if (tig_net_local_client.field_2038 >= &(tig_net_local_client.data[4])
            && tig_net_local_client.field_203C != tig_net_local_client.field_2038) {
            hdr = (TigNetPacketHeader*)tig_net_local_client.field_203C;
            tig_net_can_parse_packet(hdr, "(HEADER)tig_net_can_parse_packet(HEADER)");

            if (tig_net_local_client.field_203C + hdr->size > tig_net_local_client.field_2038) {
                return false;
            }

            tig_net_validate_packet(hdr, "tig_net_can_parse_packet");
            return true;
        }
    } else if (tig_net_client_is_active(client_id)) {
        if (tig_net_clients[client_id].field_2038 >= &(tig_net_clients[client_id].data[4])
            && tig_net_clients[client_id].field_203C != tig_net_clients[client_id].field_2038) {
            hdr = (TigNetPacketHeader*)tig_net_clients[client_id].field_203C;
            tig_net_can_parse_packet(hdr, "(HEADER)tig_net_can_parse_packet(HEADER)");

            if (tig_net_clients[client_id].field_203C + hdr->size > tig_net_clients[client_id].field_2038) {
                return false;
            }

            tig_net_validate_packet(hdr, "tig_net_can_parse_packet");
            return true;
        }
    } else {
        tig_debug_printf("TCP-NET: Error: Inactive slot passed to tig_net_parse_packet, %d\n", client_id);
    }
    return false;
}

// 0x529B80
bool tig_net_process_packet(int client_id)
{
    int prev_client_id;
    uint16_t size;

    if (client_id == tig_net_local_client.client_id) {
        prev_client_id = dword_5C064C;
        size = ((TigNetPacketHeader*)tig_net_local_client.field_203C)->size;

        dword_5C064C = client_id;
        sub_529C20((TigNetPacketHeader*)tig_net_local_client.field_203C, client_id);
        dword_5C064C = prev_client_id;
        tig_net_local_client.field_203C += size;
    } else {
        prev_client_id = dword_5C064C;
        size = ((TigNetPacketHeader*)tig_net_clients[client_id].field_203C)->size;

        dword_5C064C = client_id;
        sub_529C20((TigNetPacketHeader*)tig_net_clients[client_id].field_203C, client_id);
        dword_5C064C = prev_client_id;
        tig_net_clients[client_id].field_203C += size;
    }

    return true;
}

// 0x529C20
void sub_529C20(TigNetPacketHeader* hdr, int client_id)
{
    switch (hdr->type) {
    case TIG_NET_GENERIC:
        return;
    case TIG_NET_SERVER_CMD:
        switch (*(int*)(hdr + 1)) {
        case TIG_NET_JOIN_REQUEST:
            if (1) {
                TigNetJoinRequestMsg* join_request_msg = (TigNetJoinRequestMsg*)(hdr + 1);
                TigNetClientDisconnectPacket client_disconnect_pkt;
                char ip_address[16];

                tig_debug_printf("TCP-NET: received join request from slot %d\n", client_id);
                tig_net_inet_string(tig_net_clients[client_id].addr.sin_addr.s_addr, ip_address);
                if (tig_net_blacklist_has(NULL, ip_address)) {
                    tig_debug_printf("TCP-NET: client from %s is banned. They cannot join.\n", ip_address);

                    client_disconnect_pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
                    client_disconnect_pkt.hdr.type = TIG_NET_SERVER_CMD;
                    client_disconnect_pkt.hdr.size = sizeof(client_disconnect_pkt);
                    client_disconnect_pkt.msg.type = TIG_NET_CLIENT_DISCONNECT;
                    client_disconnect_pkt.msg.reason = TIG_NET_DISCONNECT_REASON_BANNED;
                    client_disconnect_pkt.msg.client_id = client_id;
                    tig_net_send_to_client_internal(&(client_disconnect_pkt.hdr), sizeof(client_disconnect_pkt), client_id);
                    tig_net_shutdown(client_id);
                    return;
                }

                if (hdr->version != TIG_NET_PROTOCOL_VERSION) {
                    tig_debug_printf("TCP-NET: client from %s is using incompatible protocol. They cannot join.\n", ip_address);

                    client_disconnect_pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
                    client_disconnect_pkt.hdr.type = TIG_NET_SERVER_CMD;
                    client_disconnect_pkt.hdr.size = sizeof(client_disconnect_pkt);
                    client_disconnect_pkt.msg.type = TIG_NET_CLIENT_DISCONNECT;
                    client_disconnect_pkt.msg.reason = TIG_NET_DISCONNECT_REASON_INCOMPATIBLE_PROTOCOL;
                    client_disconnect_pkt.msg.client_id = client_id;
                    tig_net_send_to_client_internal(&(client_disconnect_pkt.hdr), sizeof(client_disconnect_pkt), client_id);
                    tig_net_shutdown(client_id);
                    return;
                }

                if (tig_net_local_server_password != NULL
                    && strcmp(tig_net_local_server_password, join_request_msg->password) != 0) {
                    tig_debug_printf("TCP-NET: client from %s did not provide the correct password. They cannot join.\n", ip_address);

                    client_disconnect_pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
                    client_disconnect_pkt.hdr.type = TIG_NET_SERVER_CMD;
                    client_disconnect_pkt.hdr.size = sizeof(client_disconnect_pkt);
                    client_disconnect_pkt.msg.type = TIG_NET_CLIENT_DISCONNECT;
                    client_disconnect_pkt.msg.reason = TIG_NET_DISCONNECT_REASON_WRONG_PASSWORD;
                    client_disconnect_pkt.msg.client_id = client_id;
                    tig_net_send_to_client_internal(&(client_disconnect_pkt.hdr), sizeof(client_disconnect_pkt), client_id);
                    tig_net_shutdown(client_id);
                    return;
                }

                tig_net_client_info_set_name(client_id, join_request_msg->name);
                sub_527AC0(client_id);

                if (tig_net_on_network_event_func != NULL
                    && !tig_net_on_network_event_func(5, client_id, join_request_msg + 1, join_request_msg->extra_size)) {
                    tig_debug_printf("TCP-NET: client from %s was not allowed by the APP level. They cannot join.\n", ip_address);

                    client_disconnect_pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
                    client_disconnect_pkt.hdr.type = TIG_NET_SERVER_CMD;
                    client_disconnect_pkt.hdr.size = sizeof(client_disconnect_pkt);
                    client_disconnect_pkt.msg.type = TIG_NET_CLIENT_DISCONNECT;
                    client_disconnect_pkt.msg.reason = TIG_NET_DISCONNECT_REASON_APP_SPECIFIC;
                    client_disconnect_pkt.msg.client_id = client_id;
                    tig_net_send_to_client_internal(&(client_disconnect_pkt.hdr), sizeof(client_disconnect_pkt), client_id);
                    tig_net_shutdown(client_id);
                    sub_52A4D0(client_id);
                    return;
                }

                if (tig_net_auto_join_is_enabled()) {
                    sub_52A9E0(client_id);
                }
            }
            return;
        case TIG_NET_ALLOW_JOIN:
            if (1) {
                TigNetAllowJoinMsg* allow_join_msg = (TigNetAllowJoinMsg*)(hdr + 1);
                int index;

                tig_debug_printf("TCP-NET: Info: got an allow join packet, my client slot is %d\n", allow_join_msg->client_id);
                sub_52A4C0();

                for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
                    strncpy(tig_net_clients[index].name, allow_join_msg->client_names[index], TIG_NET_CLIENT_NAME_LENGTH - 1);
                    tig_net_clients[index].flags = allow_join_msg->client_flags[index];
                    tig_net_clients[index].client_id = allow_join_msg->client_ids[index];
                }

                if (tig_net_local_client.client_id == -1) {
                    sub_52A500(&tig_net_local_client, &(tig_net_clients[allow_join_msg->client_id]));
                    tig_net_local_client.client_id = allow_join_msg->client_id;
                }

                tig_net_local_server_set_max_players(allow_join_msg->max_players);
                tig_net_local_server_disable_option(-1);
                tig_net_local_server_enable_option(allow_join_msg->server_options);
                tig_net_client_set_loading(allow_join_msg->client_id);

                if (tig_net_on_network_event_func != NULL) {
                    tig_net_on_network_event_func(7, -1, NULL, 0);
                }

                if (tig_net_on_join_response_func != NULL) {
                    tig_net_on_join_response_func(true);
                }
            }
            return;
        case TIG_NET_DISALLOW_JOIN:
            if (1) {
                int reason = TIG_NET_DISCONNECT_REASON_DISALLOWED;

                if (tig_net_on_join_response_func != NULL) {
                    tig_net_on_join_response_func(false);
                }

                if (tig_net_on_network_event_func != NULL) {
                    tig_net_on_network_event_func(8, -1, &reason, sizeof(reason));
                }

                tig_debug_printf("TCP-NET: Join Disallowed\n");
            }
            return;
        case TIG_NET_CLIENT_DETAILS:
            if (1) {
                TigNetClientDetailsMsg* client_details_msg = (TigNetClientDetailsMsg*)(hdr + 1);

                sub_527AC0(client_details_msg->client_id);
                tig_net_client_info_set_name(client_details_msg->client_id, client_details_msg->client_name);
                sub_52A460(client_details_msg->client_flags, client_details_msg->client_id);
            }
            return;
        case TIG_NET_SERVER_DETAILS:
            if (1) {
                TigNetServerDetailsMsg* server_details_msg = (TigNetServerDetailsMsg*)(hdr + 1);

                if ((tig_net_flags & TIG_NET_HOST) != 0
                    || (tig_net_flags & (TIG_NET_HOST | TIG_NET_FLAG_0x08)) != 0) {
                    if ((tig_net_flags & TIG_NET_HOST) != 0
                        && server_details_msg->info.addr.sin_addr.s_addr != tig_net_local_bookmark.addr.sin_addr.s_addr) {
                        sub_52B0F0(&(server_details_msg->info.addr));
                    }
                } else {
                    sub_52A630(&(server_details_msg->info));
                    if ((tig_net_flags & TIG_NET_NO_BROADCAST) == 0) {
                        struct sockaddr_in addr = server_details_msg->info.addr;
                        addr.sin_port = tig_net_ws_procs.htons(TIG_NET_SERVER_PORT);
                        sub_5289B0(&addr);
                    }

                    if (tig_net_on_server_bcast_func != NULL) {
                        tig_net_on_server_bcast_func(&(server_details_msg->info));
                    }
                }
            }
            return;
        case TIG_NET_CLIENT_DISCONNECT:
            if (1) {
                TigNetClientDisconnectMsg* client_disconnect_msg = (TigNetClientDisconnectMsg*)(hdr + 1);

                if ((tig_net_flags & TIG_NET_HOST) != 0) {
                    tig_debug_printf("TCP-NET: Client #%d (%s) is notifying they are quitting.\n",
                        client_disconnect_msg->client_id,
                        tig_net_client_info_get_name(client_disconnect_msg->client_id));
                    if (!tig_net_client_is_active(client_disconnect_msg->client_id)) {
                        return;
                    }
                } else {
                    if (tig_net_client_is_local(client_disconnect_msg->client_id)) {
                        tig_debug_printf("TCP-NET: Client was kicked from server.\n");
                        if (tig_net_on_network_event_func != NULL) {
                            tig_net_on_network_event_func(8, client_disconnect_msg->client_id, &(client_disconnect_msg->reason), sizeof(client_disconnect_msg->reason));
                        }
                        sub_5280F0();
                        return;
                    }
                }

                tig_debug_printf("TCP-NET: Client #%d (%s) Quit.\n",
                    client_disconnect_msg->client_id,
                    tig_net_client_info_get_name(client_disconnect_msg->client_id));
                if (tig_net_on_network_event_func != NULL) {
                    tig_net_on_network_event_func(8, client_disconnect_msg->client_id, &(client_disconnect_msg->reason), sizeof(client_disconnect_msg->reason));
                }

                if ((tig_net_flags & TIG_NET_HOST) != 0) {
                    tig_net_shutdown(client_disconnect_msg->client_id);
                }

                tig_net_client_info_set_name(client_disconnect_msg->client_id, "");

                if ((tig_net_flags & TIG_NET_HOST) != 0) {
                    tig_net_send_app_all(client_disconnect_msg, sizeof(*client_disconnect_msg));
                }
            }
            return;
        case 7:
            sub_52B2C0();
            return;
        case 8:
            sub_52B2D0();
            return;
        }
        return;
    case TIG_NET_APP:
        if (dword_62AC50 == 0) {
            if (tig_net_on_message_func != NULL) {
                tig_net_on_message_func(hdr + 1);
            }
        }
        return;
    case TIG_NET_BCAST_PING:
        if (1) {
            TigNetPingPacket* ping_pkt = (TigNetPingPacket*)hdr;

            if ((ping_pkt->type & 1) != 0) {
                if ((tig_net_flags & TIG_NET_HOST) != 0) {
                    if (ping_pkt->client_id != -1
                        && tig_net_client_is_active(ping_pkt->client_id)) {
                        int ping_client_id = ping_pkt->client_id;

                        ping_pkt->type = 2;
                        ping_pkt->client_id = tig_net_local_client.client_id;
                        tig_net_send_to_client_internal(&(ping_pkt->hdr), ping_pkt->hdr.size, ping_client_id);
                    }
                } else {
                    ping_pkt->type = 2;
                    ping_pkt->client_id = tig_net_local_client.client_id;
                    sub_529830(&(ping_pkt->hdr), ping_pkt->hdr.size);
                }
            } else if ((ping_pkt->type & 2) != 0) {
                if ((tig_net_flags & TIG_NET_HOST) != 0) {
                    if (ping_pkt->client_id != -1) {
                        sub_52A490(ping_pkt->client_id, tig_timer_elapsed(ping_pkt->timestamp));
                        if (tig_net_on_network_event_func != NULL) {
                            tig_net_on_network_event_func(12, ping_pkt->client_id, NULL, 0);
                        }
                    }
                } else {
                    sub_52A490(0, tig_timer_elapsed(ping_pkt->timestamp));
                    if (tig_net_on_network_event_func != NULL) {
                        tig_net_on_network_event_func(12, ping_pkt->client_id, NULL, 0);
                    }
                }
            }
        }
        return;
    case TIG_NET_ALIVE:
        if (1) {
            TigNetAlivePacket* alive_pkt = (TigNetAlivePacket*)hdr;

            if ((tig_net_flags & TIG_NET_NO_BROADCAST) != 0) {
                tig_debug_printf("TCP-NET: received bcast ping even though we have -mpnobcast... exiting (-remove for release-)\n");
                exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE.
            }

            if ((alive_pkt->type & 1) != 0) {
                struct sockaddr_in alive_packet_addr = alive_pkt->addr;

                alive_pkt->type = 2;
                alive_pkt->addr = tig_net_local_bookmark.addr;
                tig_net_ws_procs.sendto(tig_net_bcast_socket.s,
                    (char*)&(alive_pkt->hdr),
                    alive_pkt->hdr.size,
                    0,
                    (struct sockaddr*)&alive_packet_addr,
                    sizeof(alive_packet_addr));
            } else if ((alive_pkt->type & 2) != 0) {
                TigNetServer server;

                tig_idxtable_get(&tig_net_bookmarks,
                    alive_pkt->addr.sin_addr.s_addr,
                    &server);
                server.field_58 = tig_timer_elapsed(alive_pkt->time);
                tig_idxtable_set(&tig_net_bookmarks,
                    alive_pkt->addr.sin_addr.s_addr,
                    &server);

                if (tig_net_on_servers_change_func != NULL) {
                    TigNetServer* servers;
                    int num_servers;

                    tig_net_get_servers(&servers, &num_servers);
                    tig_net_on_servers_change_func(servers, num_servers);
                }
            }
        }
        break;
    case 10:
        tig_net_xfer_recv_data_pkt(hdr + 1);
        if (tig_net_on_network_event_func != NULL) {
            tig_net_on_network_event_func(14, sub_52A530(), hdr + 1, 4);
        }
        break;
    case 11:
        tig_net_xfer_recv_info_pkt(hdr + 1);
        break;
    default:
        // Invalid packet type.
        exit(EXIT_FAILURE);
    }
}

// 0x52A460
bool sub_52A460(int flags, int client_id)
{
    tig_net_clients[client_id].flags = flags;
    return true;
}

// 0x52A490
void sub_52A490(int client_id, int a2)
{
    tig_timer_now(&(tig_net_clients[client_id].field_206C));
    tig_net_clients[client_id].field_2068 = a2;
}

// 0x52A4C0
void sub_52A4C0()
{
    tig_net_flags |= TIG_NET_FLAG_0x08;
}

// 0x52A4D0
void sub_52A4D0(int client_id)
{
    tig_net_clients[client_id].flags &= ~TIG_NET_CLIENT_CONNECTED;
}

// 0x52A500
bool sub_52A500(TigNetClient* dst, TigNetClient* src)
{
    dst->client_id = src->client_id;
    strncpy(dst->name, src->name, TIG_NET_CLIENT_NAME_LENGTH - 1);
    dst->flags = src->flags;

    return true;
}

// 0x52A530
int sub_52A530()
{
    return dword_5C064C;
}

// 0x52A540
void tig_net_server_bcast_alive()
{
    TigNetServerDetailsPacket pkt;
    int err;

    if ((tig_net_flags & TIG_NET_NO_BROADCAST) != 0
        || (tig_net_flags & TIG_NET_WAITING) == 0) {
        return;
    }

    if ((tig_net_flags & TIG_NET_HOST) == 0) {
        tig_debug_printf("TCP-NET: 'Broadcast I'm Alive' failed because I am not the server!\n");
        return;
    }

    pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt.hdr.type = TIG_NET_SERVER_CMD;
    pkt.hdr.size = sizeof(pkt);
    pkt.msg.type = TIG_NET_SERVER_DETAILS;
    pkt.msg.info = tig_net_local_bookmark;
    pkt.msg.info.num_players = tig_net_num_active_clients();
    pkt.msg.info.field_54 = tig_timer_elapsed(pkt.msg.info.time) / 1000;

    if (tig_net_ws_procs.sendto(tig_net_bcast_socket.s, (char*)&(pkt.hdr), sizeof(pkt), 0, (struct sockaddr*)&(tig_net_bcast_socket.addr), sizeof(tig_net_bcast_socket.addr)) == -1) {
        err = tig_net_ws_procs.WSAGetLastError();
        tig_debug_printf("TCP-NET: Cannot send to socket %d, WSAError %s(%d)...\n",
            tig_net_bcast_socket.s,
            strerror(err),
            err);
    }
}

// 0x52A630
void sub_52A630(TigNetServer* server)
{
    TigNetServer mut_server;

    if (tig_idxtable_get(&tig_net_bookmarks, server->addr.sin_addr.s_addr, &mut_server)) {
        if ((mut_server.options & TIG_NET_SERVER_BOOKMARKED) != 0) {
            server->time = (tig_timestamp_t)-1;
            server->options |= TIG_NET_SERVER_BOOKMARKED;
        }
    }

    memcpy(&mut_server, server, sizeof(TigNetServer));

    if (mut_server.time != -1) {
        tig_timer_now(&(mut_server.time));
    }

    tig_idxtable_set(&tig_net_bookmarks, mut_server.addr.sin_addr.s_addr, &mut_server);

    if (tig_net_on_servers_change_func != NULL) {
        TigNetServer* servers;
        int num_servers;

        tig_net_get_servers(&servers, &num_servers);
        tig_net_on_servers_change_func(servers, num_servers);
    }
}

// 0x52A6E0
bool tig_net_remove_unresponsive_servers()
{
    bool found = false;
    TigNetRemoveServerListNode* node;

    tig_idxtable_enumerate(&tig_net_bookmarks, sub_52A750, &found);

    while (tig_net_remove_server_list_head != NULL) {
        node = tig_net_remove_server_list_head;
        tig_net_remove_server_list_head = node->next;
        tig_idxtable_remove(&tig_net_bookmarks, node->addr);
        FREE(node);
    }

    return found;
}

// 0x52A750
bool sub_52A750(void* entry, int index, void* context)
{
    TigNetServer* server;
    TigNetRemoveServerListNode* node;
    bool* found_ptr;

    server = (TigNetServer*)entry;
    found_ptr = (bool*)context;

    if (server->time != -1 && tig_timer_elapsed(server->time) > 10000) {
        node = (TigNetRemoveServerListNode*)MALLOC(sizeof(TigNetRemoveServerListNode));
        node->addr = index;
        node->next = tig_net_remove_server_list_head;
        tig_net_remove_server_list_head = node;
        *found_ptr = true;
    }

    return true;
}

// 0x52A7A0
void tig_net_enable_server_filter(int filter)
{
    tig_net_server_filter |= filter;
}

// 0x52A7C0
void tig_net_disable_server_filter(int filter)
{
    tig_net_server_filter &= ~filter;
}

// 0x52A7E0
int tig_net_get_server_filter()
{
    return tig_net_server_filter;
}

// 0x52A7F0
bool sub_52A7F0(void* entry, int index, void* context)
{
    TigNetServer* server;
    TigNetServer* servers;
    int v1 = 0;
    int v2 = 0;

    (void)index;

    server = (TigNetServer*)entry;
    servers = (TigNetServer*)context;

    if (tig_net_server_filter != 0) {
        switch (server->type) {
        case TIG_NET_SERVER_TYPE_FREE_FOR_ALL:
            if ((tig_net_server_filter & TIG_NET_SERVER_FILTER_FREE_FOR_ALL) != 0) {
                v2++;
            } else {
                v1++;
            }
            break;
        case TIG_NET_SERVER_TYPE_COOPERATIVE:
            if ((tig_net_server_filter & TIG_NET_SERVER_FILTER_COOPERATIVE) != 0) {
                v2++;
            } else {
                v1++;
            }
            break;
        case TIG_NET_SERVER_TYPE_ROLEPLAY:
            if ((tig_net_server_filter & TIG_NET_SERVER_FILTER_ROLEPLAY) != 0) {
                v2++;
            } else {
                v1++;
            }
            break;
        }

        if ((tig_net_server_filter & TIG_NET_SERVER_FILTER_BOOKMARKED) != 0) {
            if ((server->options & TIG_NET_SERVER_BOOKMARKED) != 0) {
                v2++;
            } else {
                v1++;
            }
        }
    }

    if ((tig_net_server_filter & TIG_NET_SERVER_FILTER_INVERSE) != 0 && v1 != 0 || v2 == 0) {
        return true;
    }

    memcpy(&(servers[tig_net_num_servers++]), server, sizeof(TigNetServer));

    return true;
}

// 0x52A890
bool tig_net_get_servers(TigNetServer** servers_ptr, int* num_servers_ptr)
{
    if (tig_net_servers != NULL) {
        FREE(tig_net_servers);
    }

    tig_net_servers = (TigNetServer*)MALLOC(sizeof(TigNetServer) * tig_idxtable_count(&tig_net_bookmarks));
    tig_net_num_servers = 0;
    tig_idxtable_enumerate(&tig_net_bookmarks, sub_52A7F0, tig_net_servers);

    *servers_ptr = tig_net_servers;
    *num_servers_ptr = tig_net_num_servers;

    return true;
}

// 0x52A900
bool sub_52A900()
{
    int index;

    if ((tig_net_flags & TIG_NET_CONNECTED) == 0) {
        return false;
    }

    if (dword_62AC88 != 0) {
        return false;
    }

    for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
        if (tig_net_client_is_active(index)
            && tig_net_client_is_loading(index)) {
            return true;
        }
    }

    return false;
}

// 0x52A940
void sub_52A940()
{
    dword_62AC88++;
}

// 0x52A950
void sub_52A950()
{
    dword_62AC88--;
}

// 0x52A960
int tig_net_num_active_clients()
{
    int count = 0;
    int index;

    for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
        if (tig_net_client_is_active(index)) {
            count++;
        }
    }

    return count;
}

// 0x52A980
void tig_net_client_set_loading(int client_id)
{
    tig_net_clients[client_id].flags |= TIG_NET_CLIENT_LOADING;
}

// 0x52A9B0
void tig_net_client_unset_loading(int client_id)
{
    tig_net_clients[client_id].flags &= ~TIG_NET_CLIENT_LOADING;
}

// 0x52A9E0
void sub_52A9E0(int client_id)
{
    TigNetClientDetailsPacket pkt;
    int index;

    if (!tig_net_client_is_waiting(client_id)) {
        return;
    }

    if (sub_52A900()) {
        tig_net_on_network_event_func(13, client_id, NULL, 0);
        return;
    }

    tig_net_client_unset_waiting(client_id);

    pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt.hdr.type = TIG_NET_SERVER_CMD;
    pkt.hdr.size = sizeof(pkt);
    pkt.msg.type = TIG_NET_CLIENT_DETAILS;
    pkt.msg.client_id = client_id;
    strncpy(pkt.msg.client_name, tig_net_client_info_get_name(client_id), TIG_NET_CLIENT_NAME_LENGTH - 1);
    pkt.msg.client_flags = tig_net_client_info_get_flags(client_id);

    for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
        if (tig_net_client_is_active(index)) {
            tig_net_send_to_client_internal(&(pkt.hdr), pkt.hdr.size, client_id);
        }
    }

    if (tig_net_on_network_event_func != NULL) {
        tig_net_on_network_event_func(6, client_id, NULL, 0);
    }

    sub_52AB10(client_id);

    if (tig_net_on_network_event_func != NULL) {
        tig_net_on_network_event_func(7, client_id, NULL, 0);
    }

    tig_debug_printf("TCP-NET: Client #%d (%s) Joined.\n", client_id, tig_net_client_info_get_name(client_id));
}

// 0x52AAF0
unsigned int tig_net_client_info_get_flags(int client_id)
{
    return tig_net_clients[client_id].flags;
}

// 0x52AB10
void sub_52AB10(int client_id)
{
    TigNetAllowJoinPacket pkt;
    int index;

    tig_net_client_set_loading(client_id);

    pkt.msg.type = TIG_NET_ALLOW_JOIN;
    pkt.msg.client_id = client_id;

    for (index = 0; index < TIG_NET_MAX_PLAYERS; index++) {
        if (tig_net_client_is_active(index)) {
            strncpy(pkt.msg.client_names[index], tig_net_clients[index].name, TIG_NET_CLIENT_NAME_LENGTH - 1);
        } else {
            pkt.msg.client_names[index][0] = '\0';
        }

        pkt.msg.client_flags[index] = tig_net_clients[index].flags;
        pkt.msg.client_ids[index] = tig_net_clients[index].client_id;
    }

    pkt.msg.max_players = tig_net_local_server_get_max_players();
    pkt.msg.server_options = tig_net_local_server_get_options();
    pkt.hdr.type = TIG_NET_SERVER_CMD;
    pkt.hdr.size = sizeof(pkt);
    tig_net_send_to_client_internal(&(pkt.hdr), sizeof(pkt), client_id);
}

// 0x52ABE0
void tig_net_client_set_waiting(int client_id)
{
    tig_net_clients[client_id].flags |= TIG_NET_CLIENT_WAITING;
}

// 0x52AC10
void tig_net_client_unset_waiting(int client_id)
{
    tig_net_clients[client_id].flags &= ~TIG_NET_CLIENT_WAITING;
}

// 0x52AC40
bool tig_net_client_is_waiting(int client_id)
{
    return (tig_net_clients[client_id].flags & TIG_NET_CLIENT_WAITING) != 0;
}

// 0x52AC60
void tig_net_shutdown(int client_id)
{
    tig_net_clients[client_id].flags |= TIG_NET_CLIENT_SHUTTING_DOWN;
    tig_net_ws_procs.shutdown(tig_net_clients[client_id].s, SD_SEND);
}

// 0x52ACA0
bool tig_net_client_is_shutting_down(int client_id)
{
    return (tig_net_clients[client_id].flags & TIG_NET_CLIENT_SHUTTING_DOWN) != 0;
}

// 0x52ACC0
bool tig_net_client_is_lagging(int client_id)
{
    return (tig_net_clients[client_id].flags & TIG_NET_CLIENT_LAGGING) != 0;
}

// 0x52ACE0
bool sub_52ACE0(int client_id, char* dst, size_t size)
{
    snprintf(dst, size, "%s", tig_net_client_is_waiting(client_id) ? "Waiting to join" : "Playing");
    return true;
}

// 0x52AD40
bool tig_net_ban(int client_id)
{
    char ip_address[16];
    tig_net_inet_string(tig_net_clients[client_id].addr.sin_addr.s_addr, ip_address);
    tig_net_blacklist_add(NULL, ip_address);
    return true;
}

// 0x52AD80
void tig_net_no_broadcast_enable()
{
    tig_net_flags |= TIG_NET_NO_BROADCAST;
}

// 0x52AD90
void tig_net_no_broadcast_disable()
{
    tig_net_flags &= ~TIG_NET_NO_BROADCAST;
}

// 0x52ADA0
void tig_net_bookmarks_load(const char* path)
{
    TigNetServer bookmark;
    TigFile* stream;
    char buffer[128];
    char* pch;

    stream = tig_file_fopen(path, "rt");
    if (stream == NULL) {
        tig_debug_printf("TCP-NET: No bookmarks.\n");
        return;
    }

    while ((pch = tig_file_fgets(buffer, sizeof(buffer), stream)) != NULL) {
        while (isspace(*pch)) {
            pch++;
        }

        if (*pch == '\0') {
            break;
        }

        if ((pch[0] == '/' && pch[1] == '/')
            || pch[0] == '#') {
            continue;
        }

        if (!tig_file_feof(stream)) {
            pch[strlen(pch) - 1] = '\0';
        }

        tig_net_bookmark_init(&bookmark);
        if (tig_net_inet_addr(pch, &(bookmark.addr.sin_addr.s_addr))) {
            strcpy(bookmark.name, pch);
            bookmark.options |= TIG_NET_SERVER_BOOKMARKED;
            bookmark.time = (tig_timestamp_t)-1;
            tig_debug_printf("TCP-NET: Found bookmark: %s (0x%08X)\n", pch, bookmark.addr.sin_addr);
            tig_idxtable_set(&tig_net_bookmarks, bookmark.addr.sin_addr.s_addr, &bookmark);
        } else {
            tig_debug_printf("TCP-NET: Could not translate bookmark: %s\n", pch);
        }
    }
}

// 0x52AF20
void tig_net_bookmarks_save(const char* path)
{
    TigFile* stream;

    if (tig_net_bookmarks_changed) {
        stream = tig_file_fopen(path, "wt");
        if (stream != NULL) {
            tig_idxtable_enumerate(&tig_net_bookmarks, tig_net_bookmarks_save_one, stream);
            tig_file_fclose(stream);
        } else {
            tig_debug_printf("TCP-NET: ERROR could not open bookmark file to save!\n");
        }
    }
}

// 0x52AF70
bool tig_net_bookmarks_save_one(void* value, int index, void* context)
{
    TigNetServer* bookmark;
    TigFile* stream;
    char buffer[128];

    (void)index;

    bookmark = (TigNetServer*)value;
    stream = (TigFile*)context;

    if ((bookmark->options & TIG_NET_SERVER_BOOKMARKED) != 0) {
        tig_net_inet_string(bookmark->addr.sin_addr.s_addr, buffer);
        tig_file_fputs(buffer, stream);
        tig_file_fputs("\n", stream);
    }

    return true;
}

// 0x52AFD0
void tig_net_bookmark_add(unsigned long addr)
{
    TigNetServer server;

    if (!tig_idxtable_get(&tig_net_bookmarks, addr, &server)) {
        tig_net_bookmark_init(&server);
        server.addr.sin_addr.s_addr = addr;
        tig_net_inet_string(addr, server.name);
    }

    server.time = (tig_timestamp_t)-1;
    server.options |= TIG_NET_SERVER_BOOKMARKED;
    tig_idxtable_set(&tig_net_bookmarks, addr, &server);

    tig_net_bookmarks_changed = true;
}

// 0x52B050
void tig_net_bookmark_remove(unsigned long addr)
{
    TigNetServer server;

    if (tig_idxtable_get(&tig_net_bookmarks, addr, &server)) {
        server.options &= ~TIG_NET_SERVER_BOOKMARKED;
        tig_timer_now(&(server.time));
        server.time -= 10000;
        tig_idxtable_set(&tig_net_bookmarks, addr, &server);

        tig_net_bookmarks_changed = true;
    }
}

// 0x52B0C0
void sub_52B0C0(struct sockaddr_in* addr)
{
    *addr = tig_net_local_bookmark.addr;
}

// 0x52B0F0
void sub_52B0F0(struct sockaddr_in* addr)
{
    TigNetServerDetailsPacket pkt;

    pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt.hdr.type = TIG_NET_SERVER_CMD;
    pkt.hdr.size = sizeof(pkt);
    pkt.msg.type = TIG_NET_SERVER_DETAILS;

    if ((tig_net_flags & TIG_NET_HOST) != 0) {
        pkt.msg.info = tig_net_local_bookmark;
        pkt.msg.info.num_players = tig_net_num_active_clients();
        pkt.msg.info.field_54 = tig_timer_elapsed(pkt.msg.info.time) / 1000;
    } else {
        memset(&(pkt.msg.info), 0, sizeof(TigNetServer));
        pkt.msg.info.addr = tig_net_local_socket.addr;
        pkt.msg.info.addr.sin_port = tig_net_bcast_socket.addr.sin_port;
    }

    addr->sin_port = tig_net_ws_procs.htons(TIG_NET_SERVER_PORT);

    tig_net_ws_procs.sendto(tig_net_bcast_socket.s,
        (char*)&(pkt.hdr),
        pkt.hdr.size,
        0,
        (struct sockaddr*)addr,
        sizeof(*addr));
}

// 0x52B1E0
void tig_net_on_server_bcast(TigNetOnServerBcast* func)
{
    tig_net_on_server_bcast_func = func;
}

// 0x52B1F0
unsigned long tig_net_ntohl(unsigned long addr)
{
    return tig_net_ws_procs.ntohl(addr);
}

// 0x52B210
void sub_52B210()
{
    tig_timer_now(&(tig_net_local_bookmark.time));
}

// 0x52B220
void sub_52B220(int client_id)
{
    struct {
        TigNetPacketHeader hdr;
        int type;
    } pkt;

    pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt.hdr.type = TIG_NET_SERVER_CMD;
    pkt.hdr.size = sizeof(pkt);
    pkt.type = 7;

    tig_net_ws_procs.send(tig_net_clients[client_id].s,
        (char*)&pkt,
        sizeof(pkt),
        0);
}

// 0x52B270
void sub_52B270(int client_id)
{
    struct {
        TigNetPacketHeader hdr;
        int type;
    } pkt;

    pkt.hdr.version = TIG_NET_PROTOCOL_VERSION;
    pkt.hdr.type = TIG_NET_SERVER_CMD;
    pkt.hdr.size = sizeof(pkt);
    pkt.type = 8;

    tig_net_ws_procs.send(tig_net_clients[client_id].s,
        (char*)&pkt,
        sizeof(pkt),
        0);
}

// 0x52B2C0
void sub_52B2C0()
{
    dword_62AC50++;
}

// 0x52B2D0
void sub_52B2D0()
{
    dword_62AC50--;
}

// 0x52B2E0
void sub_52B2E0(int a1)
{
    dword_62AC3C = a1;
}
