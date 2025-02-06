#ifdef FOR_FUTURE_USE

May 22 9:52pm
Subj: FYI - Copies of verified RFPort Configs for TM-D700, TM-D710, and TH-D72

Hi Lynn,

Attached are copies of working RFPort configurations for the TM-D700, TM-D710, and TH-D72. All have been verified as working on each of my radios; however, there may need to be some tweaking to eliminate some of the “Missed Expected Response” error messages.

Note:  I have customized the D700-KISS(BDM) and D700-CONVerse(GPS) ports so that automatic frequency control is performed; based on my own Channel Memory locations and PM settings.

Billy Mason, KD5KNR

<!--RFPort[0]-->
<RFPort Name="D700-KISS(BDM)">
<Protocol>KWD700(KISS)</Protocol>
<Device>COM3:9600,N,8,1</Device>
<RfBaud>1200</RfBaud>
<OpenCmd>^M~</OpenCmd>
<OpenCmd>^M~</OpenCmd>
<OpenCmd>AI 1!AI 1</OpenCmd>
<OpenCmd>TC 1</OpenCmd>
<OpenCmd>TNC 2!TNC 2</OpenCmd>
<OpenCmd>DTB 0!DTB 0</OpenCmd>
<OpenCmd>BC 0,0!BC 0,0</OpenCmd>
<OpenCmd>FQ 00144390000,0!FQ 00144390000,0</OpenCmd>
<OpenCmd>SFT 0!SFT 0</OpenCmd>
<OpenCmd>MC 0,002!MC 0,002</OpenCmd>
<OpenCmd>BC 1,1!BC 1,1</OpenCmd>
<OpenCmd>DIM 2</OpenCmd>
<OpenCmd>TC 0!cmd:!5</OpenCmd>
<OpenCmd>ECHO OFF</OpenCmd>
<OpenCmd>BEACON EVERY 0</OpenCmd>
<OpenCmd>ECHO OFF</OpenCmd>
<OpenCmd>BEACON EVERY 0</OpenCmd>
<OpenCmd>LOC E 0</OpenCmd>
<OpenCmd>AUTOLF OFF</OpenCmd>
<OpenCmd>MCOM ON</OpenCmd>
<OpenCmd>MON ON</OpenCmd>
<OpenCmd>MRPt ON</OpenCmd>
<OpenCmd>PACLEN 250</OpenCmd>
<OpenCmd>HBAUD 1200</OpenCmd>
<OpenCmd>MSTAMP OFF</OpenCmd>
<OpenCmd>CONOK OFF</OpenCmd>
<OpenCmd>UIDIGI OFF</OpenCmd>
<OpenCmd>UIFLOOD %</OpenCmd>
<OpenCmd>UITRACE %</OpenCmd>
<OpenCmd>HEADER OFF</OpenCmd>
<OpenCmd>NEWMODE ON</OpenCmd>
<OpenCmd>NOMODE OFF</OpenCmd>
<OpenCmd>XFLOW OFF</OpenCmd>
<OpenCmd>FULLDUP OFF</OpenCmd>
<OpenCmd>KISS ON</OpenCmd>
<OpenCmd>RESTART!RESTART!2</OpenCmd>
<CloseCmd>^192^255^192~!cmd:!5</CloseCmd>
<CloseCmd>MON OFF</CloseCmd>
<CloseCmd>TC 1</CloseCmd>
<CloseCmd>TNC 0</CloseCmd>
<CloseCmd>DIM 1</CloseCmd>
<CloseCmd>BC 1,1</CloseCmd>
<CloseCmd>PM 3!PM 3</CloseCmd>
<QuietTime>0</QuietTime>
<Enabled>1</Enabled>
<XmitEnabled>1</XmitEnabled>
<ProvidesNMEA>0</ProvidesNMEA>
<RFtoISEnabled>1</RFtoISEnabled>
<IStoRFEnabled>1</IStoRFEnabled>
<MyCallNot3rd>0</MyCallNot3rd>
<BeaconingEnabled>1</BeaconingEnabled>
<BeaconPath></BeaconPath>
<BulletinObjectEnabled>1</BulletinObjectEnabled>
<MessagesEnabled>1</MessagesEnabled>
<MessagePath></MessagePath>
<TelemetryEnabled>0</TelemetryEnabled>
<TelemetryPath></TelemetryPath>
<!--DigiXform-->
</RFPort>
<!--RFPort[0]-->

<!--RFPort[1]-->
<RFPort Name="D700-CONVerse(GPS)">
<Protocol>CONVerse</Protocol>
<Device>COM3:9600,N,8,1</Device>
<RfBaud>1200</RfBaud>
<OpenCmd>^C</OpenCmd>
<OpenCmd>AI 1!AI 1</OpenCmd>
<OpenCmd>TC 1</OpenCmd>
<OpenCmd>TNC 2!TNC 2</OpenCmd>
<OpenCmd>DTB 0!DTB 0</OpenCmd>
<OpenCmd>BC 0,0!BC 0,0</OpenCmd>
<OpenCmd>FQ 00144390000,0!FQ 00144390000,0</OpenCmd>
<OpenCmd>SFT 0!SFT 0</OpenCmd>
<OpenCmd>MC 0,002!MC 0,002</OpenCmd>
<OpenCmd>BC 1,1!BC 1,1</OpenCmd>
<OpenCmd>DIM 2</OpenCmd>
<OpenCmd>TC 0!cmd:!5</OpenCmd>
<OpenCmd>BEACON EVERY 0</OpenCmd>
<OpenCmd>ECHO OFF</OpenCmd>
<OpenCmd>FLOW OFF</OpenCmd>
<OpenCmd>AUTOLF OFF</OpenCmd>
<OpenCmd>MCOM OFF</OpenCmd>
<OpenCmd>MON ON</OpenCmd>
<OpenCmd>PACLEN 128</OpenCmd>
<OpenCmd>HBAUD 1200</OpenCmd>
<OpenCmd>LOC E 0</OpenCmd>
<OpenCmd>MSTAMP OFF</OpenCmd>
<OpenCmd>HEADER OFF</OpenCmd>
<OpenCmd>NEWMODE ON</OpenCmd>
<OpenCmd>NOMODE OFF</OpenCmd>
<OpenCmd>FULLDUP OFF</OpenCmd>
<OpenCmd>XFLOW ON</OpenCmd>
<OpenCmd>^C!cmd:!1</OpenCmd>
<OpenCmd>GBAUD 4800</OpenCmd>
<OpenCmd>GPSTEXT $GPRMC</OpenCmd>
<OpenCmd>LTMH OFF</OpenCmd>
<OpenCmd>LTM 10</OpenCmd>
<CloseCmd>MON OFF</CloseCmd>
<CloseCmd>TC 1</CloseCmd>
<CloseCmd>TNC 0</CloseCmd>
<CloseCmd>DIM 1</CloseCmd>
<CloseCmd>BC 1,1</CloseCmd>
<CloseCmd>PM 3!PM 3</CloseCmd>
<QuietTime>0</QuietTime>
<Enabled>0</Enabled>
<XmitEnabled>1</XmitEnabled>
<ProvidesNMEA>1</ProvidesNMEA>
<RFtoISEnabled>1</RFtoISEnabled>
<IStoRFEnabled>1</IStoRFEnabled>
<MyCallNot3rd>0</MyCallNot3rd>
<BeaconingEnabled>1</BeaconingEnabled>
<BeaconPath></BeaconPath>
<BulletinObjectEnabled>1</BulletinObjectEnabled>
<MessagesEnabled>1</MessagesEnabled>
<MessagePath></MessagePath>
<TelemetryEnabled>0</TelemetryEnabled>
<TelemetryPath></TelemetryPath>
<!--DigiXform-->
</RFPort>
<!--RFPort[1]-->

<!--RFPort[2]-->
<RFPort Name="D710-APRS/GPS(Mobile)">
<Protocol>KWD710(APRS)</Protocol>
<Device>COM10:9600,N,8,1</Device>
<RfBaud>1200</RfBaud>
<OpenCmd>^C^C^C~!!0</OpenCmd>
<OpenCmd>TC 1!?!1.0</OpenCmd>
<OpenCmd>TC 1!TS 1!1.0</OpenCmd>
<OpenCmd>TN 1,0!TN 1,0</OpenCmd>
<OpenCmd>TC 0!!0.050</OpenCmd>
<OpenCmd>^M!cmd:!1</OpenCmd>
<OpenCmd>DIG OFF</OpenCmd>
<OpenCmd>GB 4800</OpenCmd>
<OpenCmd>HB 1200</OpenCmd>
<OpenCmd>GPST $GPRMC</OpenCmd>
<OpenCmd>LTM 0</OpenCmd>
<OpenCmd>LTMH OFF</OpenCmd>
<OpenCmd>LOC E 0</OpenCmd>
<OpenCmd>M ON</OpenCmd>
<OpenCmd>MCOM ON</OpenCmd>
<OpenCmd>MS OFF</OpenCmd>
<OpenCmd>CONO OFF</OpenCmd>
<OpenCmd>P 250</OpenCmd>
<OpenCmd>UI OFF</OpenCmd>
<OpenCmd>UIF NOID</OpenCmd>
<OpenCmd>UIT %</OpenCmd>
<OpenCmd>X OFF</OpenCmd>
<OpenCmd>HEADER OFF</OpenCmd>
<OpenCmd>NE ON</OpenCmd>
<OpenCmd>NO OFF</OpenCmd>
<OpenCmd>ECHO OFF!cmd:!1.0</OpenCmd>
<OpenCmd>BEACON EVERY 0!cmd:!1.0</OpenCmd>
<OpenCmd>ECHO OFF!cmd:!1.00</OpenCmd>
<OpenCmd>BEACON EVERY 0!cmd:!1.00</OpenCmd>
<OpenCmd>TC 1!?EH!1</OpenCmd>
<OpenCmd>TC 1!TS 1!1.00</OpenCmd>
<CloseCmd>TC 0!!0.050</CloseCmd>
<CloseCmd>^M!cmd:!1</CloseCmd>
<CloseCmd>LTM 0</CloseCmd>
<CloseCmd>TC 1!?EH!1</CloseCmd>
<CloseCmd>TC 1!TS 1</CloseCmd>
<CloseCmd>TN 0,0!TN 0,0</CloseCmd>
<QuietTime>0</QuietTime>
<Enabled>0</Enabled>
<XmitEnabled>1</XmitEnabled>
<ProvidesNMEA>1</ProvidesNMEA>
<RFtoISEnabled>0</RFtoISEnabled>
<IStoRFEnabled>0</IStoRFEnabled>
<MyCallNot3rd>0</MyCallNot3rd>
<BeaconingEnabled>1</BeaconingEnabled>
<BeaconPath></BeaconPath>
<BulletinObjectEnabled>1</BulletinObjectEnabled>
<MessagesEnabled>1</MessagesEnabled>
<MessagePath></MessagePath>
<TelemetryEnabled>0</TelemetryEnabled>
<TelemetryPath></TelemetryPath>
<!--DigiXform-->
</RFPort>
<!--RFPort[2]-->

<!--RFPort[3]-->
<RFPort Name="D72-APRS(GPS)">
<Protocol>KWD710(APRS)</Protocol>
<Device></Device>
<RfBaud>1200</RfBaud>
<OpenCmd>^C^C^C~!!0</OpenCmd>
<OpenCmd>TC 1!?!1.0</OpenCmd>
<OpenCmd>TC 1!TS 1!1.0</OpenCmd>
<OpenCmd>TN 1,0!TN 1,0</OpenCmd>
<OpenCmd>TC 0!!0.050</OpenCmd>
<OpenCmd>^M!cmd:!1</OpenCmd>
<OpenCmd>DIG OFF</OpenCmd>
<OpenCmd>GB 4800</OpenCmd>
<OpenCmd>HB 1200</OpenCmd>
<OpenCmd>GPST $GPRMC</OpenCmd>
<OpenCmd>LTM 0</OpenCmd>
<OpenCmd>LTMH OFF</OpenCmd>
<OpenCmd>LOC E 0</OpenCmd>
<OpenCmd>M ON</OpenCmd>
<OpenCmd>MCOM ON</OpenCmd>
<OpenCmd>MS OFF</OpenCmd>
<OpenCmd>CONO OFF</OpenCmd>
<OpenCmd>P 250</OpenCmd>
<OpenCmd>UI OFF</OpenCmd>
<OpenCmd>UIF NOID</OpenCmd>
<OpenCmd>UIT %</OpenCmd>
<OpenCmd>X OFF</OpenCmd>
<OpenCmd>HEADER OFF</OpenCmd>
<OpenCmd>NE ON</OpenCmd>
<OpenCmd>NO OFF</OpenCmd>
<OpenCmd>ECHO OFF!cmd:!1.0</OpenCmd>
<OpenCmd>BEACON EVERY 0!cmd:!1.0</OpenCmd>
<OpenCmd>ECHO OFF!cmd:!1.00</OpenCmd>
<OpenCmd>BEACON EVERY 0!cmd:!1.00</OpenCmd>
<OpenCmd>TC 1!?EH!1</OpenCmd>
<OpenCmd>TC 1!TS 1!1.00</OpenCmd>
<CloseCmd>TC 0!!0.050</CloseCmd>
<CloseCmd>^M!cmd:!1</CloseCmd>
<CloseCmd>LTM 0</CloseCmd>
<CloseCmd>TC 1!?EH!1</CloseCmd>
<CloseCmd>TC 1!TS 1</CloseCmd>
<CloseCmd>TN 0,0!TN 0,0</CloseCmd>
<QuietTime>0</QuietTime>
<Enabled>0</Enabled>
<XmitEnabled>1</XmitEnabled>
<ProvidesNMEA>1</ProvidesNMEA>
<RFtoISEnabled>0</RFtoISEnabled>
<IStoRFEnabled>0</IStoRFEnabled>
<MyCallNot3rd>0</MyCallNot3rd>
<BeaconingEnabled>1</BeaconingEnabled>
<BeaconPath></BeaconPath>
<BulletinObjectEnabled>1</BulletinObjectEnabled>
<MessagesEnabled>1</MessagesEnabled>
<MessagePath></MessagePath>
<TelemetryEnabled>0</TelemetryEnabled>
<TelemetryPath></TelemetryPath>
<!--DigiXform-->
</RFPort>
<!--RFPort[3]-->
#endif

#include "sysheads.h"

#ifdef OBSOLETE
#ifdef _DEBUG
#include <crtdbg.h>
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock.h>

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#endif	/* OBSOLETE */

#define sock_init() 							\
{	WORD wVersionRequested = MAKEWORD(1,1);				\
	WSADATA wsaData;						\
	int err = WSAStartup(wVersionRequested, &wsaData);		\
	if (err != 0)							\
	{	/*printf("WSAStartup Failed With %ld\n", (long) err);*/	\
		exit(-1);						\
	}								\
}
#define soclose(s) closesocket(s)
#define ioctl(s) ioctlsocket(s)
#define psock_errno(s) TraceLog(rpInfo->TraceName, TRUE, NULL, "%s errno %ld\n", s, (long) h_errno)
#define sock_errno() h_errno

#include "cpdef.h"
#include "cprtns.h"

#include "config.h"
#include "llutil.h"
#include "tcputil.h"

#include "timestamp.h"

BOOL ActiveConfigLoaded = FALSE;
CONFIG_INFO_S ActiveConfig = {0};

#include "agw.h"
#include "aprsis.h"
#include "aprs_udp.h"
#include "kiss.h"
#include "text.h"

#include "parsedef.h"
#include "parse.h"
#include "filter.h"

#include "port.h"

#include "Bluetooth.h"

#pragma warning(disable : 4995)	/* Disable deprecated warnings */

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !FALSE
#endif

// helper macros
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (UINT_PTR)(sizeof(a)/sizeof((a)[0]))
#endif

typedef struct XMIT_QUEUE_ENTRY_S
{	double msQueued;
	char *Data;			/* points to below packet, single free of packet */
} XMIT_QUEUE_ENTRY_S;

typedef struct RUNNING_PORT_INFO_S
{	PORT_CONFIG_INFO_S pcInfo;
	struct PORT_ACTION_S *Actions;
	char *TraceName;
	COMM_PORT_S *pcp;
	int mysocket;

	long (*SendRtn)(void *UserArg, char *Buffer, long Length, long *pBytesSent);
	long (*ReadRtn)(void *UserArg, long Timeout, char *Buffer, long Length, long *pBytesRead);
	void *SendReadArg;

	struct
	{	HWND hwnd;
		UINT msgStatus, msgSubStatus, msgReceived, msgXmitCount;
		WPARAM wp;
	} Notify;

#define BUFFER_SIZE 2048
	int cBytesRead, cResidualBytes, cZeroCount;
	unsigned char Buffer[BUFFER_SIZE];
	__int64 msLastRead;

	BOOL Enabled;
	BOOL Running;
	int Line;
	double lastActive;

	struct
	{	int Size;
		int Count;
		XMIT_QUEUE_ENTRY_S **Entries;
		HANDLE hmtx;
	} Transmit;

} RUNNING_PORT_INFO_S;

static DWORD DummyRunner(RUNNING_PORT_INFO_S *rpInfo);
static DWORD PortUDPListen(RUNNING_PORT_INFO_S *rpInfo);
static DWORD PortTCPListen(RUNNING_PORT_INFO_S *rpInfo);
static BOOL TCPListenMonitor(RUNNING_PORT_INFO_S *rpInfo, char *Packet);


static char *DummyFormatTransmit(char *String, long *rLen, void *Where)
{	if (rLen) *rLen = strlen(String);
	return String;
}

typedef struct PORT_ACTION_S
{	char *Protocol;
	BOOL Configurable;
	BOOL DevelopmentOnly;
	BOOL (*TweakConfig)(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo);
	BOOL (*NewConnection)(void *What, BOOL (*Write)(void *What, int Len, unsigned char *Buf));
	char *(*FormatTransmit)(char *String, long *rLen, void *Where);	/* Return value is freed unless == String */
	char *(*FormatReceive)(int Len, unsigned char *Pkt, int *rLen);
	BOOL (*GotPacket)(int len, unsigned char *buf);
	DWORD (*Runner)(RUNNING_PORT_INFO_S *rpInfo);
	BOOL (*Monitor)(RUNNING_PORT_INFO_S *rpInfo, char *Packet);
} PORT_ACTION_S;

PORT_ACTION_S Protocols[] = {
							{ "Simply(KISS)", TRUE, FALSE, SimplyKISSTweakConfig, NULL, KISSFormatTransmit, KISSFormatReceive, KISSGotPacket },
							{ "KISS", TRUE, FALSE, KISSTweakConfig, NULL, KISSFormatTransmit, KISSFormatReceive, KISSGotPacket },
							{ "AGW", TRUE, FALSE, NULL, AGWNewConnection, AGWFormatTransmit, AGWFormatReceive, AGWGotPacket },
							{ "FTM350v1.3(RO)", TRUE, FALSE, FTM350TweakConfig, NULL, FTM350FormatTransmit, FTM350FormatReceive, FTM350GotPacket },
							{ "KWD700(KISS)", TRUE, FALSE, D700KISSTweakConfig, NULL, KISSFormatTransmit, KISSFormatReceive, KISSGotPacket },
							{ "KWD700(RO+GPS)", TRUE, FALSE, D700TEXTTweakConfig, NULL, D700TEXTFormatTransmit, TEXTFormatReceive, TEXTGotPacket },
							{ "KWD710(APRS)", TRUE, FALSE, KenwoodTweakConfigAPRS, NULL, KenwoodFormatTransmitAPRS, KenwoodFormatReceive, KenwoodGotPacket },
							{ "KWD710(Pkt)", TRUE, FALSE, KenwoodTweakConfigPacket, NULL, KenwoodFormatTransmitPacket, KenwoodFormatReceive, KenwoodGotPacket },
				/* DEV */	{ "CONVerse", TRUE, TRUE, ConverseTweakConfigPacket, NULL, ConverseFormatTransmitPacket, ConverseFormatReceive, ConverseGotPacket },
							{ "TEXT", TRUE, FALSE, NULL, NULL, TEXTFormatTransmit, TEXTFormatReceive, TEXTGotPacket },
							{ "CWOP", TRUE, FALSE, CWOPTweakConfig, CWOPNewConnection, CWOPFormatTransmit, APRSISFormatReceive, APRSISGotPacket },
				/* DEV */	{ "Dummy", TRUE, TRUE, NULL, NULL, DummyFormatTransmit, NULL, NULL, DummyRunner },
				/* DEV */	{ "IS-Server", TRUE, TRUE, ServerTweakConfig, NULL, APRSISFormatTransmit, ServerFormatReceive, ServerGotPacket, PortTCPListen, TCPListenMonitor },
#define OUTPUT_SERVER "Local-Server"
				/* DEV */	{ OUTPUT_SERVER, TRUE, TRUE, ServerTweakConfig, NULL, APRSISFormatTransmit, ServerFormatReceive, ServerGotPacket, PortTCPListen, TCPListenMonitor },
		/* INT */			{ "APRSIS", FALSE, FALSE, APRSISTweakConfig, APRSISNewConnection, APRSISFormatTransmit, APRSISFormatReceive, APRSISGotPacket },
		/* INT */			{ "NMEA", FALSE, FALSE, NMEATweakConfig, NMEANewConnection, NULL, NMEAFormatReceive, TEXTGotPacket },
		/* INT *//* DEV */	{ "UDP-Listen", FALSE, TRUE, APRSUDPTweakConfig, APRSUDPNewConnection, APRSUDPFormatTransmit, APRSUDPFormatReceive, APRSUDPGotPacket, PortUDPListen },
							};
#define ProtocolCount (sizeof(Protocols)/sizeof(Protocols[0]))

void AddPortProtocols(HWND hwndList)
{	int i;
	for (i=0; i<ProtocolCount; i++)
	if (Protocols[i].Configurable)
	if (!Protocols[i].DevelopmentOnly || ActiveConfig.Update.Development)
	{	PORT_CONFIG_INFO_S *Info = NULL;
		TCHAR tText[sizeof(Info->Protocol)+1];
		StringCbPrintf(tText, sizeof(tText), TEXT("%S"), Protocols[i].Protocol);
		SendMessage(hwndList, CB_INSERTSTRING, -1, (LPARAM) tText);
	}
}


HANDLE hmtxPorts = NULL;
unsigned long PortSize;
unsigned long PortCount;
RUNNING_PORT_INFO_S **Ports;
BOOL ActiveMonitors = FALSE;

static RUNNING_PORT_INFO_S *CreatePort(PORT_CONFIG_INFO_S *pcInfo, HWND hwnd, UINT msgStatus, UINT msgSubStatus, UINT msgReceived, UINT msgXmitCount, WPARAM wp)
{	DWORD Status;
	unsigned long p;
	RUNNING_PORT_INFO_S *rpInfo = NULL;

	if (!hmtxPorts) hmtxPorts = CreateMutex(NULL, FALSE, NULL);

	for (p=0; p<ProtocolCount; p++)
	{	if (!_stricmp(Protocols[p].Protocol, pcInfo->Protocol))
			break;
	}
	if (p >= ProtocolCount)
	{	TraceLog("Ports", TRUE, hwnd, "Port(%s) Protocol(%s) Not Supported\n",
				pcInfo->Name, pcInfo->Protocol);
		return NULL;
	}

	if ((Status=WaitForSingleObject(hmtxPorts, 30000)) == WAIT_OBJECT_0)
	{
		rpInfo = (RUNNING_PORT_INFO_S *)calloc(1,sizeof(*rpInfo));

		rpInfo->pcInfo = *pcInfo;
		rpInfo->TraceName = (char*)malloc(strlen(pcInfo->Name)+16);
		sprintf(rpInfo->TraceName, "Port(%s)", pcInfo->Name);
		rpInfo->Notify.hwnd = hwnd;
		rpInfo->Notify.msgStatus = msgStatus;
		rpInfo->Notify.msgSubStatus = msgSubStatus;
		rpInfo->Notify.msgReceived = msgReceived;
		rpInfo->Notify.msgXmitCount = msgXmitCount;
		rpInfo->Notify.wp = wp;
		rpInfo->Actions = &Protocols[p];

		rpInfo->Transmit.hmtx = CreateMutex(NULL, FALSE, NULL);

		p = PortCount++;
		if (p >= PortSize)
		{	PortSize += 4;
			Ports = (RUNNING_PORT_INFO_S **)realloc(Ports, sizeof(*Ports)*PortSize);
		}
		Ports[p] = rpInfo;

		if (rpInfo->Actions->Monitor)
		{	ActiveMonitors = TRUE;
		}

		ReleaseMutex(hmtxPorts);
	}

	return rpInfo;
}

static RUNNING_PORT_INFO_S *GetPort(PORT_CONFIG_INFO_S *pcInfo, WPARAM wp)
{	DWORD Status;
	RUNNING_PORT_INFO_S *rpInfo = NULL;

	if (!hmtxPorts) return NULL;

	if ((Status=WaitForSingleObject(hmtxPorts, 30000)) == WAIT_OBJECT_0)
	{	unsigned long p;
		for (p=0; p<PortCount; p++)
		{	if (Ports[p]->Notify.wp == wp
			&& !strcmp(pcInfo->Name, Ports[p]->pcInfo.Name))
			{	rpInfo = Ports[p];
				break;
			}
		}
		ReleaseMutex(hmtxPorts);
	}
	return rpInfo;
}

static void DestroyPort(RUNNING_PORT_INFO_S *rpInfo, BOOL FreeIt)
{	DWORD Status;

	rpInfo->Enabled = FALSE;

	if (!hmtxPorts) return;

	if ((Status=WaitForSingleObject(hmtxPorts, 30000)) == WAIT_OBJECT_0)
	{	unsigned long p;
		for (p=0; p<PortCount; p++)
		{	if (Ports[p] == rpInfo)
			{	
				if (--PortCount) Ports[p] = Ports[PortCount];
				if (FreeIt)
				{	CloseHandle(rpInfo->Transmit.hmtx);
					rpInfo->Transmit.hmtx = NULL;

					if (rpInfo->Transmit.Entries)
					{	int t;
						for (t=0; t<rpInfo->Transmit.Count; t++)
							free(rpInfo->Transmit.Entries[t]);
						free(rpInfo->Transmit.Entries);
						rpInfo->Transmit.Count = rpInfo->Transmit.Size = 0;
						rpInfo->Transmit.Entries = NULL;
					}
					free(rpInfo->TraceName);
					free(rpInfo);
				}
			}
		}
		ReleaseMutex(hmtxPorts);
	}
}

static BOOL LockAndPurgeTransmitQueue(RUNNING_PORT_INFO_S *rpInfo, DWORD dwTimeout)
{	DWORD Status;

	if ((Status=WaitForSingleObject(rpInfo->Transmit.hmtx, dwTimeout)) == WAIT_OBJECT_0)
	{	double msNow = PortGetMsec();
		double msOldest = msNow - 60*1000;	/* Purge if > 60 seconds old */
		while (rpInfo->Transmit.Count
		&& rpInfo->Transmit.Entries[0]->msQueued < msOldest)
		{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Flushing Old(%ldms) Xmit(%s)\n", (long)(msNow-rpInfo->Transmit.Entries[0]->msQueued), rpInfo->Transmit.Entries[0]->Data);
			free(rpInfo->Transmit.Entries[0]);
			if (--rpInfo->Transmit.Count)
				memmove(&rpInfo->Transmit.Entries[0], &rpInfo->Transmit.Entries[1], sizeof(rpInfo->Transmit.Entries[0])*rpInfo->Transmit.Count);
		}
		if (!rpInfo->Transmit.Count && rpInfo->Transmit.Entries)
		{	free(rpInfo->Transmit.Entries);
			rpInfo->Transmit.Count = rpInfo->Transmit.Size = 0;
			rpInfo->Transmit.Entries = NULL;
		}
		return TRUE;
	} else return FALSE;
}


static void SetPortStatus(RUNNING_PORT_INFO_S *rpInfo, char *Status, int DisplayFlag)
{
	if (rpInfo->Notify.hwnd && rpInfo->Notify.msgStatus)
			PostMessage(rpInfo->Notify.hwnd, rpInfo->Notify.msgStatus,
						MAKELONG(rpInfo->Notify.wp, DisplayFlag), (LPARAM) _strdup(Status));
}

static void SetPortSubStatus(RUNNING_PORT_INFO_S *rpInfo, char *Status)
{
	if (rpInfo->Notify.hwnd && rpInfo->Notify.msgSubStatus)
			PostMessage(rpInfo->Notify.hwnd, rpInfo->Notify.msgSubStatus,
						rpInfo->Notify.wp, (LPARAM) _strdup(Status));
	else TraceLogThread(rpInfo->TraceName, TRUE, "hwnd(%p) msgSubStatus(%ld) NOT %s\n",
						rpInfo->Notify.hwnd,
						(long) rpInfo->Notify.msgSubStatus,
						Status);
}

static void SendMsgReceived(RUNNING_PORT_INFO_S *rpInfo, char *Formatted, BOOL Validated)
{
	if (ActiveMonitors)
	{	DWORD Status;
		char *Packet = Formatted;
		if (rpInfo->pcInfo.RfBaud!=-1)	/* An RF port?  May need qAR */
		{	char *p = strchr(Formatted, ':');	/* Find payload */
			if (p)	/* MUST have a payload delimiter */
			{	char *q = strstr(Formatted, ",q");	/* Any q-construct? */
				if (!q || q > p)	/* No q-construct before the payload */
				{	int Len = (p-Formatted);
					Packet = (char*)malloc(strlen(Formatted)+4+1+sizeof(CALLSIGN)+1);
					strncpy(Packet,Formatted,Len);
					Len += sprintf(Packet+Len,",qAR,%s",CALLSIGN);
					strcpy(Packet+Len,p);
				}
			}
		}
		if ((Status=WaitForSingleObject(hmtxPorts, 5000)) == WAIT_OBJECT_0)
		{	unsigned long p;
			BOOL FoundOne = FALSE;
			for (p=0; p<PortCount; p++)
			{	if (Ports[p]->Actions->Monitor)
				{	if (Ports[p] != rpInfo)	/* Not back to myself! */
						Ports[p]->Actions->Monitor(Ports[p], Packet);
				}
				FoundOne = TRUE;
			}
			if (!FoundOne)
			{	ActiveMonitors = FALSE;
			}
			ReleaseMutex(hmtxPorts);
		}
		if (Packet != Formatted) free(Packet);	/* Free the qAR'd copy */
	}

	if (IsWindow(rpInfo->Notify.hwnd))
	{	if (!PostMessage(rpInfo->Notify.hwnd, rpInfo->Notify.msgReceived,
							Validated?rpInfo->Notify.wp:-((int)rpInfo->Notify.wp),
							(LPARAM) Formatted))
		{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Error(%ld) Posting(%s)\n", (long) GetLastError(), Formatted);
			free(Formatted);
		}
	} else
	{	if (rpInfo->Notify.hwnd)
			TraceLog(rpInfo->TraceName, TRUE, NULL, "Non-Window Post(%s)\n", Formatted);
		free(Formatted);
	}
}

void PortDumpHex2(char *Log, BOOL ForceIt, char *What, int Len, unsigned char *p)
{	size_t Available;
	char *Next, *Output;
	int i, wasNonPrint = FALSE;

	if (Len == -1) Len = p?strlen(p):0;
	Available = strlen(What)+1+33+2+Len*5+1;
	Output = (char *)malloc(Available);

	StringCbPrintfExA(Output, Available, &Next, &Available, STRSAFE_IGNORE_NULLS,
					"%s[%ld]:", What?What:"Stuff", (long) Len);
	if (!p)
		StringCbPrintfExA(Next, Available, &Next, &Available, STRSAFE_IGNORE_NULLS, "*NULL*");
	else for (i=0; i<Len; i++)
	{	if (!(p[i]&0x80) && isprint(p[i]&0xff))
		{	if (wasNonPrint)
				StringCbPrintfExA(Next, Available, &Next, &Available, STRSAFE_IGNORE_NULLS,
									">");
			wasNonPrint = FALSE;
			StringCbPrintfExA(Next, Available, &Next, &Available, STRSAFE_IGNORE_NULLS,
								"%c", p[i]&0xff);
		} else
		{	StringCbPrintfExA(Next, Available, &Next, &Available, STRSAFE_IGNORE_NULLS,
							"%c%02lX",wasNonPrint?' ':'<', (long)p[i]&0xff);
			wasNonPrint = TRUE;
		}
	}
	if (wasNonPrint)
		StringCbPrintfExA(Next, Available, &Next, &Available, STRSAFE_IGNORE_NULLS,
							">");
	TraceLogThread(Log, ForceIt, "%s", Output);
	free(Output);
}

void PortDumpHex(char *Log, char *What, int Len, unsigned char *p)
{	PortDumpHex2(Log, FALSE, What, Len, p);
}




static void DummyTransmitQueue(RUNNING_PORT_INFO_S *rpInfo)
{
	if (LockAndPurgeTransmitQueue(rpInfo, 1000))
	{	XMIT_QUEUE_ENTRY_S *Entry = NULL;
		if (rpInfo->Transmit.Count)	/* Still something there? */
		{	Entry = rpInfo->Transmit.Entries[0];
			if (--rpInfo->Transmit.Count)
				memmove(&rpInfo->Transmit.Entries[0], &rpInfo->Transmit.Entries[1], sizeof(rpInfo->Transmit.Entries[0])*rpInfo->Transmit.Count);
			else
			{	free(rpInfo->Transmit.Entries);
				rpInfo->Transmit.Count = rpInfo->Transmit.Size = 0;
				rpInfo->Transmit.Entries = NULL;
			}
		}
		ReleaseMutex(rpInfo->Transmit.hmtx);
		if (Entry)										/* C0 00 <path expansion> 03 F0 <Payload> */
		{	
			PortDumpHex(rpInfo->TraceName, "Ate", (long) strlen(Entry->Data), Entry->Data);
		}
	}
}


static DWORD DummyRunner(RUNNING_PORT_INFO_S *rpInfo)
{
	TraceLog(rpInfo->TraceName, TRUE, NULL, "DummyLoad Running\n");

	while (rpInfo->Enabled)
	{	Sleep(100);
		if (rpInfo->Transmit.Count)	/* Something waiting? */
		{
			DummyTransmitQueue(rpInfo);
		}
	}

	TraceLog(rpInfo->TraceName, TRUE, NULL, "Terminating after %.0lf msec vs %ld Quiet\n",
			(double) PortGetMsec()-rpInfo->lastActive, (long) rpInfo->pcInfo.QuietTime*1000);

	return 0;
}










static int PortReadCommPort(RUNNING_PORT_INFO_S *rpInfo, COMM_PORT_S *pcp)
{	int i;

rpInfo->Line = __LINE__;
	/*for (i=0; i<50; i++) */
	for (i=0; rpInfo->Enabled && i<2; i++)
	{	long cNewBytes;
		if (rpInfo->cBytesRead >= sizeof(rpInfo->Buffer)-1)
		{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Flushing %ld/%ld Bytes In Buffer\n", (long) rpInfo->cBytesRead, (long) sizeof(rpInfo->Buffer));
			PortDumpHex(rpInfo->TraceName, "Flush", rpInfo->cBytesRead, rpInfo->Buffer);
			rpInfo->cBytesRead = 0;
		}
rpInfo->Line = __LINE__;
		if (CpReadBytesWithTimeout(pcp, 30L, &rpInfo->Buffer[rpInfo->cBytesRead], sizeof(rpInfo->Buffer)-rpInfo->cBytesRead-1, &cNewBytes))
		{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Error Reading Port, CpReadBytesWithTimeout FAILED!\n");
			return FALSE;
		}
rpInfo->Line = __LINE__;
		rpInfo->cBytesRead += cNewBytes;
		rpInfo->Buffer[rpInfo->cBytesRead] = '\0';	/* Null terminate buffer */
		if (cNewBytes)
		{	rpInfo->lastActive = PortGetMsec();
rpInfo->Line = __LINE__;
			if (rpInfo->Actions->GotPacket(rpInfo->cBytesRead, rpInfo->Buffer))
				break;
			rpInfo->cZeroCount = 0;
			rpInfo->msLastRead = llGetMsec();
		} else
		{	if (rpInfo->cZeroCount++ > 5)
			{	if (rpInfo->cBytesRead) TraceLog(rpInfo->TraceName, FALSE, NULL,"[%ld] Read Zero bytes too often with %ld in buffer\n", (long) i, (long) rpInfo->cBytesRead);
				break;
			}
			/* else if (cBytesRead) printf("[%ld] Read zero bytes with %ld in buffer\n", (long) i, (long) cBytesRead); */
		}
	}
rpInfo->Line = __LINE__;
	if (rpInfo->Enabled && rpInfo->cBytesRead)
	{	int Consumed = 0;
rpInfo->Line = __LINE__;
		do
		{	char *Formatted = rpInfo->Actions->FormatReceive(rpInfo->cBytesRead, rpInfo->Buffer, &Consumed);
#ifdef VERBOSE
if (Formatted)
	TraceLog(rpInfo->TraceName, FALSE, NULL, "Formatted %ld/%ld Bytes of (%s)\n",
		 (long) Consumed, (long) rpInfo->cBytesRead, Formatted);
else if (Consumed) TraceLog(rpInfo->TraceName, FALSE, NULL, "Consumed %ld/%ld Bytes\n",
		 (long) Consumed, (long) rpInfo->cBytesRead);
#endif
rpInfo->Line = __LINE__;
			if (Formatted)
			{	SendMsgReceived(rpInfo, Formatted, TRUE);
			}
rpInfo->Line = __LINE__;
			if (Consumed) rpInfo->cResidualBytes = rpInfo->cZeroCount = 0;
			rpInfo->cBytesRead -= Consumed;
			if (rpInfo->cBytesRead > 0)
			{	if (Consumed) memmove(&rpInfo->Buffer[0], &rpInfo->Buffer[Consumed], rpInfo->cBytesRead+1);
				else
				{	if (llMsecSince(rpInfo->msLastRead,llGetMsec()) > 5000)	/* Should be here in 5 seconds */
					{	PortDumpHex(rpInfo->TraceName, "Flushed", rpInfo->cBytesRead, rpInfo->Buffer);
						rpInfo->cBytesRead = 0;
					} else if (rpInfo->cResidualBytes
					&& rpInfo->cBytesRead != rpInfo->cResidualBytes)
					{	PortDumpHex(rpInfo->TraceName, "Residual", rpInfo->cBytesRead, rpInfo->Buffer);
					}
					rpInfo->cResidualBytes = rpInfo->cBytesRead;
				}
			} else if (rpInfo->cBytesRead < 0)
			{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Consumed %ld Results In %ld, Clearing\n",
						(long) Consumed, (long) rpInfo->cBytesRead);
				rpInfo->cBytesRead = 0;
			}

rpInfo->Line = __LINE__;
		} while (Consumed && rpInfo->cBytesRead
		&& rpInfo->Actions->GotPacket(rpInfo->cBytesRead, rpInfo->Buffer));
	}
	return TRUE;
}

static void PortTransmitQueue(RUNNING_PORT_INFO_S *rpInfo, COMM_PORT_S *pcp)
{
	if (LockAndPurgeTransmitQueue(rpInfo, 1000))
	{	XMIT_QUEUE_ENTRY_S *Entry = NULL;
		if (rpInfo->Transmit.Count)	/* Still something there? */
		{	Entry = rpInfo->Transmit.Entries[0];
			if (--rpInfo->Transmit.Count)
				memmove(&rpInfo->Transmit.Entries[0], &rpInfo->Transmit.Entries[1], sizeof(rpInfo->Transmit.Entries[0])*rpInfo->Transmit.Count);
			else
			{	free(rpInfo->Transmit.Entries);
				rpInfo->Transmit.Count = rpInfo->Transmit.Size = 0;
				rpInfo->Transmit.Entries = NULL;
			}
		}
		ReleaseMutex(rpInfo->Transmit.hmtx);
		if (Entry)										/* C0 00 <path expansion> 03 F0 <Payload> */
		{	long len, len2;
			unsigned char *Formatted = rpInfo->Actions->FormatTransmit(Entry->Data, &len, rpInfo);

			if (Formatted)
			{	int s, todo=len;
				for (s=0; s<len; s+=todo)
				{	for (todo=s; todo<len; todo++)
					{	if (Formatted[todo] == '\r')
						{	todo++;	/* Include the \r */
							break;
						}
					}
					todo -= s;	/* Make it a byte count */

					if (CpSendBytes(pcp, &Formatted[s], todo, &len2))
					{	PortDumpHex(rpInfo->TraceName, "Write FAILED\n", (long) todo, &Formatted[s]);
					} else if (todo != len2)
					{	TraceLog(rpInfo->TraceName, TRUE, NULL,"Only Wrote %ld/%ld Bytes\n", (long) len2, (long) todo);
					}
					else PortDumpHex(rpInfo->TraceName, "Sent", (long) len2, &Formatted[s]);
					if (s+todo < len) Sleep(500);
				}
				if (Formatted != Entry->Data) free(Formatted);
			}
			free(Entry);
		}
	}
}

static unsigned char *MyStrnStrn(size_t len, unsigned char *src, size_t l, unsigned char *s)
{	size_t i;
	if (len == -1) len = strlen(src);
	if (l == -1) l = strlen(s);
	if (l > len) return NULL;
	for (i=0; i<=len-l; i++)
		if (src[i] == *s	/* Match the first character? */
		&& !strncmp(&src[i],s,l))	/* Check the string */
			return &src[i];
	return NULL;
}

char *PortCommand(void * Where, unsigned char *Command, char **pResponse)
{	RUNNING_PORT_INFO_S *rpInfo = Where;
	int PCount=0, PLens[4]={0}, Len = strlen(Command);
	char *Parts[4]={0}, *p;
	char *Buffer=(char*)malloc(BUFFER_SIZE);	/* 0=command, 1=response, 2=time, 3=display */
	long Temp, BytesInBuffer;
	double msWait = 0;
	BOOL GotResponse = FALSE;
	long OrgTimeout;
	
	if (pResponse) *pResponse = NULL;	/* Default to nothing */

	if (!rpInfo->pcp && !rpInfo->mysocket)
	{	TraceLog(NULL, TRUE, NULL, "PortCommand(%s) Missing pcp and mysocket!\n", Command);
		return NULL;
	}

	if (rpInfo->pcp) OrgTimeout = rpInfo->pcp->Timeout;

	Parts[PCount++] = Command;
	for (p=Command; *p; p++)
	{	if (*p == '!')
		{	if (PCount < ARRAYSIZE(Parts))
			{	PLens[PCount-1] = p-Parts[PCount-1];
				Parts[PCount++] = p+1;	/* Character AFTER the ! */
			} else TraceLog(NULL, TRUE, NULL, "Too Many !s in Command(%s)\n", Command);
		}
	}
	PLens[PCount-1] = p-Parts[PCount-1];

	if (PCount < 2) PLens[1] = strlen(Parts[1]="cmd:");
	if (PCount < 3) PLens[2] = strlen(Parts[2]="1");
	msWait = strtod(Parts[2],&p);
	if (*p && *p != '!')
	{	if (!msWait) msWait = 1;
		TraceLog(NULL, TRUE, NULL, "Invalid Wait Time(%s) In Command(%s), Using %.0lf seconds\n",
					Parts[2], Command, (double) msWait);
	}
	msWait *= 1000;	/* Make it msec */

	if (PLens[0])	/* Any command to send? */
	{	int c, oLen = 0;
		char *Output = (char*)malloc(PLens[0]+1);	/* Room for \r */
		PortDumpHex(rpInfo->TraceName, "Command", PLens[0], Command);
		for (c=0; c<PLens[0]; c++)
		{	if (Command[c] == '^')
			{	if (Command[c+1] == '^')	/* ^^ is a ^ */
				{	Output[oLen++] = Command[c++];	/* for loop will skip the next one */
				} else if (isalpha(Command[c+1]&0xff))
				{	c++;	/* Skip the ^, for loop will skip the letter */
					Output[oLen++] = toupper(Command[c]) - 'A' + 1;	/* ^A = 0x01 */
				} else if (isdigit(Command[c+1]&0xff) && isdigit(Command[c+2]&0xff) && isdigit(Command[c+3]&0xff))
				{	int v = ((Command[c+1]-'0')*10+Command[c+2]-'0')*10+Command[c+3]-'0';
					if (v <= 255) Output[oLen++] = v;
					else TraceLog(NULL, TRUE, NULL, "Invalid %.4s in %s, Ignoring\n", &Command[c]);
					c += 3;	/* Skip nnn, for loop will skip the ^ */
				} else
				{	TraceLog(NULL, TRUE, NULL, "Invalid ^ Escape in %s\n", Command);
					Output[oLen++] = Command[c];
				}
			} else Output[oLen++] = Command[c];
		}
		if (Output[oLen-1] == '~') oLen--;	/* Don't send \r suppression ~ */
		else Output[oLen++] = '\r';
		if (oLen)
		{	PortDumpHex(rpInfo->TraceName, "Output", oLen, Output);
			rpInfo->SendRtn(rpInfo->SendReadArg, Output, oLen, &Temp);
		}
		free(Output);
	}

	if (rpInfo->pcp) rpInfo->pcp->Timeout = (long) min(msWait,1000);

	if (msWait || PLens[1])	/* Time or string expected? */
	{	msWait += PortGetMsec();	/* Adjust to end time */
		BytesInBuffer = 0;
		Buffer[BytesInBuffer] = '\0';	/* Null terminate */
		while (msWait >= PortGetMsec()
		&& !rpInfo->ReadRtn(rpInfo->SendReadArg, 10L, &Buffer[BytesInBuffer], BUFFER_SIZE-BytesInBuffer-1, &Temp))
		{	if (Temp)
			{	BytesInBuffer += Temp;
				Buffer[BytesInBuffer] = '\0';	/* Null terminate */
				PortDumpHex(NULL, "CmdResp", BytesInBuffer, Buffer);
				if (*Parts[1] && MyStrnStrn(BytesInBuffer, Buffer, PLens[1], Parts[1]))
				{	GotResponse = TRUE;
					break;
				}
			}
		}
		if (pResponse && BytesInBuffer) *pResponse = _strdup(Buffer);
	}
	if (rpInfo->pcp) rpInfo->pcp->Timeout = OrgTimeout;
	free(Buffer);

	if (PLens[1])	/* Expected response? */
	{	if (!GotResponse)
		{	TraceLog(NULL, TRUE, NULL, "Missed Expected Response(%.*s) From Command(%s)\n",
					PLens[1], Parts[1], Command);
			return Parts[3]?Parts[3]:"Missed Expected Response";
		} else return NULL;
	} else return Parts[3]?Parts[3]:NULL;	/* Message or none */
}

double PortGetMsec(void)
{	return (double) llGetMsec();
#ifdef OLD_WAY
#define C 4.294967296e9
#define Li2Double(x) ((double)((x).HighPart)*C+(double)((x).LowPart))
	double dNow;
	LARGE_INTEGER Now = {0};
static	int First = 1;
static	LARGE_INTEGER Freq;
static	double dFreq=0, dLast = 0;
	if (First)
	{	if (!QueryPerformanceFrequency(&Freq))
		{	MessageBox(NULL, TEXT("No High-Resolution Performance Counter"), TEXT("QueryPerformanceFrequency"), MB_OK | MB_ICONWARNING);
			dFreq = 1.0;
		} else
		{	dFreq = Li2Double(Freq) / 1000.0;
		}
		First = 0;
	}
	if (!QueryPerformanceCounter(&Now))
	{	return 0.0;
	}
	dNow = Li2Double(Now);
	if (dNow < dLast)
	{	dNow = ++dLast;
	} else dLast = dNow;
	return dNow / dFreq;
#endif
}

long  UdpSendRtn
(
        int *psocket,
        char *OutBuffer,
        long  OutLength,
        long  *RetBytesSent
)
{//	*RetBytesSent = send(*psocket, OutBuffer, OutLength, 0);
	TraceLogThread(NULL, TRUE, "NOT Transmitting %.*s\n", 
			  	 OutLength, OutBuffer);

	*RetBytesSent = OutLength;	/* We NEVER transmit here (yet) */
	return 0;	/* It worked */
}
long  UdpRecvRtn
(
        int *psocket,
        long Timeout,
        char *InBuffer,
        long  InLength,
        long  *RetBytesRead
)
{	int n;
	unsigned long Available = 0;

	while ((n=ioctlsocket(*psocket,FIONREAD,&Available))==0
	&& Available <= 0 && Timeout > 0)
	{	double selectStart = PortGetMsec();
		struct timeval tmo;
		fd_set fdRead;

		FD_ZERO(&fdRead);
		FD_SET(*psocket, &fdRead);
//		tmo.tv_sec = Timeout/1000; tmo.tv_usec = (Timeout%1000) * 1000;	/* uSec */
#ifdef UNDER_CE
		tmo.tv_sec = 0; tmo.tv_usec = 500*1000;	/* 500msec timeout to allow transmit */
#else
		tmo.tv_sec = 0; tmo.tv_usec = 10*1000;	/* 10msec timeout to allow transmit */
#endif
		n = select(*psocket+1, &fdRead, NULL, NULL, &tmo);
		Timeout -= (long) (PortGetMsec() - selectStart);
		if (n > 0)
		{	if (FD_ISSET(*psocket, &fdRead))
			{
				if ((n=ioctlsocket(*psocket,FIONREAD,&Available))!=0
				|| Available <= 0)
					Available = 1;	/* Readable means recv() won't block */
				n = 0;	/* make it look successful */
				break;
			}
		}
	}

	if (!n && Available)
	{	struct sockaddr_in si_other;
		int slen=sizeof(si_other);

		if ((n=recvfrom(*psocket, InBuffer, InLength, 0, (struct sockaddr *)&si_other, &slen))==-1)
		{	TraceLogThread(NULL, TRUE, "recvfrom(UDP) Failed!\n");
		} else
		{	struct in_addr xaddr;
			struct hostent *hostnm;
			char *name, *ipadd;

			ipadd = inet_ntoa(si_other.sin_addr);
			xaddr = si_other.sin_addr;
			hostnm = gethostbyaddr((void*)&xaddr,sizeof(xaddr),AF_INET);
			if (hostnm)
			{	name = hostnm->h_name;
			} else name = ipadd;

			TraceLogThread(NULL, TRUE, "Received packet from %s:%d (%s) Data: %.*s\n", 
			  	 inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), name, (int) n, InBuffer);

			*RetBytesRead = n;
			return 0;	/* It worked! */
		}
	}
	return 1;
}

static void PortUdpTransmitQueue(RUNNING_PORT_INFO_S *rpInfo, int socket)
{
	if (LockAndPurgeTransmitQueue(rpInfo, 1000))
	{	XMIT_QUEUE_ENTRY_S *Entry = NULL;
		if (rpInfo->Transmit.Count)	/* Still something there? */
		{	Entry = rpInfo->Transmit.Entries[0];
			if (--rpInfo->Transmit.Count)
				memmove(&rpInfo->Transmit.Entries[0], &rpInfo->Transmit.Entries[1], sizeof(rpInfo->Transmit.Entries[0])*rpInfo->Transmit.Count);
			else
			{	free(rpInfo->Transmit.Entries);
				rpInfo->Transmit.Count = rpInfo->Transmit.Size = 0;
				rpInfo->Transmit.Entries = NULL;
			}
		}
		ReleaseMutex(rpInfo->Transmit.hmtx);
		if (Entry)			/* C0 00 <path expansion> 03 F0 <Payload> */
		{	long len, len2;
			unsigned char *Formatted = rpInfo->Actions->FormatTransmit(Entry->Data, &len, rpInfo);

			if (Formatted)
			{	if (len)
				{	if (UdpSendRtn(&socket, Formatted, len, &len2))
					{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Write %ld Bytes FAILED\n", (long) len);
					} else if (len != len2)
					{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Only Wrote %ld/%ld Bytes\n", (long) len2, (long) len);
					}
				}
				if (Formatted != Entry->Data) free(Formatted);
			}
			free(Entry);
		}
	}
}

static DWORD PortUDPListen(RUNNING_PORT_INFO_S *rpInfo)
{	char *Arg = SpaceCompress(-1,_strdup(rpInfo->pcInfo.Device));
	int Port;
	char *Host;

	struct sockaddr_in lclserver = {0};
	struct sockaddr_in *pserver = &lclserver;
	long timeout;
	char *recvbuf;
	int n, recvlen, recvsize;
	int Success = 0;
	double maxIdle=0.0, thisIdle;
static int initialized = 0;

rpInfo->Line = __LINE__;

	if (*Arg != '@')
	{
TraceLog(rpInfo->TraceName, TRUE, NULL, "Invalid Arg(%s) Needs @\n", Arg);
		rpInfo->Running = FALSE;
		free(Arg);
		return 0;
	} else
	{	char *e, *c = strchr(Arg,':');
		if (!c)
		{
TraceLog(rpInfo->TraceName, TRUE, NULL, "Invalid Arg(%s) Needs :\n", Arg);
			rpInfo->Running = FALSE;
			free(Arg);
			return 0;
		}
		Host = Arg+1;	/* After the @ */
		*c++ = '\0';	/* Null terminate Host */
		Port = strtol(c,&e,10);
		if (*e) TraceLog(rpInfo->TraceName, TRUE, NULL,"Invalid Port %s, Using %ld\n", c, (long) Port);
	}

	TraceLog(rpInfo->TraceName, TRUE, NULL, "UDPListener Running on %s or %s:%ld (%ld OpenCmds, %ld CloseCmds)\n",
			Arg, Host, (long) Port,
			(long) rpInfo->pcInfo.OpenCmds.Count, (long) rpInfo->pcInfo.CloseCmds.Count);

	rpInfo->SendRtn = UdpSendRtn;
	rpInfo->ReadRtn = UdpRecvRtn;
	rpInfo->SendReadArg = &rpInfo->mysocket;

rpInfo->Line = __LINE__;

	if (!initialized)
	{	sock_init();
		initialized = 1;
	}

rpInfo->Line = __LINE__;
	SetPortStatus(rpInfo, "Resolve", 0);
	pserver->sin_family      = AF_INET;
	pserver->sin_port        = htons((u_short)Port);
	pserver->sin_addr.s_addr = inet_addr(Host);	/* argv[1] = hostname */
	if (pserver->sin_addr.s_addr == (u_long) -1)
	{	TraceLogThread(rpInfo->TraceName, TRUE, "inet_addr(%s) Failed, using 0.0.0.0\n", Host);
		
		pserver->sin_addr.s_addr = inet_addr("0.0.0.0");	/* argv[1] = hostname */
		if (pserver->sin_addr.s_addr == (u_long) -1)
		{	TraceLogThread(rpInfo->TraceName, TRUE, "inet_addr(0.0.0.0) Failed\n");
			goto exit;
		}
	}

	if (!rpInfo->Enabled) goto exit;
rpInfo->Line = __LINE__;

	if ((rpInfo->mysocket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))<0)
	{	psock_errno("socket");
TraceLog(rpInfo->TraceName, TRUE, NULL, "socket(%s) Failed\n", Arg);
		goto exit;
	}

	SetPortStatus(rpInfo, "Bind", 0);
rpInfo->Line = __LINE__;
	TraceLog(rpInfo->TraceName, TRUE, NULL, "Binding(%s) or %s:%ld\n",
				Host, inet_ntoa(pserver->sin_addr), (long) Port);
	if (bind(rpInfo->mysocket, (struct sockaddr *) pserver, sizeof(*pserver))==-1)
	{	psock_errno("bind");
		soclose(rpInfo->mysocket);
		rpInfo->mysocket = 0;
		TraceLog(rpInfo->TraceName, TRUE, NULL, "Bind(%s) or %s:%ld Failed\n",
					Host, inet_ntoa(pserver->sin_addr), (long) Port);
		TraceLog(rpInfo->TraceName, TRUE, NULL, "bind(%s) Failed\n", Arg);
		goto exit;
	}

	{	char *ipaddr = inet_ntoa(pserver->sin_addr);
		TraceLog(rpInfo->TraceName, TRUE, NULL, "Bound(%s)=>%s:%ld",
				Host,ipaddr?ipaddr:"NULL", (long) Port);

		if (ipaddr)
		{	size_t len = 7+strlen(ipaddr)+1+33+1;
			char *text = (char*)malloc(len);
			StringCbPrintfA(text, len, "Bound: %s:%ld", ipaddr, (long) Port);
			SetPortSubStatus(rpInfo, text);
		}
	}
	rpInfo->lastActive = PortGetMsec();

#ifdef FUTURE
	if (rpInfo->Actions->NewConnection)
	{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Firing NewConnection");
		rpInfo->Actions->NewConnection((void*)rpInfo->mysocket, PortSocketSend);
		TraceLog(rpInfo->TraceName, TRUE, NULL, "Back from NewConnection");
	}
#endif

#ifdef FUTURE
	SetPortStatus(rpInfo, "OpenCmd", 0);
	{	unsigned long c;
		for (c=0; c<rpInfo->pcInfo.OpenCmds.Count; c++)
#ifdef USE_TIMED_STRINGS
			if (rpInfo->pcInfo.OpenCmds.Entries[c].string[0] != ';'
			&& rpInfo->pcInfo.OpenCmds.Entries[c].string[0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.OpenCmds.Entries[c].string, NULL);
#else
			if (rpInfo->pcInfo.OpenCmds.Strings[c][0] != ';'
			&& rpInfo->pcInfo.OpenCmds.Strings[c][0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.OpenCmds.Strings[c], NULL);
#endif
	}
#endif

	SetPortStatus(rpInfo, "OK", 0);
	
rpInfo->Line = __LINE__;

	recvlen = 0;
	recvsize = sizeof(rpInfo->Buffer);
	recvbuf = rpInfo->Buffer;

	while (rpInfo->Enabled	/* Loop until the port is disabled */
	&& (!rpInfo->pcInfo.QuietTime
	|| rpInfo->lastActive+rpInfo->pcInfo.QuietTime*1000 >= PortGetMsec()))	/* or times out */
	{	unsigned long Available = 0;
		timeout = 1000;	/* 1 second timeout */
rpInfo->Line = __LINE__;
		while (rpInfo->Enabled && (n=ioctlsocket(rpInfo->mysocket,FIONREAD,&Available))==0 && Available <= 0 && timeout > 0)
		{	double selectStart = PortGetMsec();
			struct timeval tmo;
			fd_set fdRead;

			FD_ZERO(&fdRead);
			FD_SET(rpInfo->mysocket, &fdRead);
//			tmo.tv_sec = timeout/1000; tmo.tv_usec = (timeout%1000) * 1000;	/* uSec */
#ifdef UNDER_CE
			tmo.tv_sec = 0; tmo.tv_usec = 500*1000;	/* 500msec timeout to allow transmit */
#else
			tmo.tv_sec = 0; tmo.tv_usec = 10*1000;	/* 10msec timeout to allow transmit */
#endif
			rpInfo->Line = __LINE__;
			n = select(rpInfo->mysocket+1, &fdRead, NULL, NULL, &tmo);
rpInfo->Line = __LINE__;
			timeout -= (long) (PortGetMsec() - selectStart);
			if (n > 0)
			{	if (FD_ISSET(rpInfo->mysocket, &fdRead))
				{
rpInfo->Line = __LINE__;
					if ((n=ioctlsocket(rpInfo->mysocket,FIONREAD,&Available))!=0 || Available <= 0)
						Available = 1;	/* Readable means recv() won't block */
#ifdef VERBOSE
					else if (Available) TraceLog(rpInfo->TraceName, FALSE, NULL, "PortUDPListen:readable %ld has %ld Available\n", rpInfo->mysocket, Available);
#endif
					n = 0;	/* make it look successful */
					break;
				}
			} else if (n < 0)
				TraceLog(rpInfo->TraceName, TRUE, NULL, "PortRead:select(%ld) return %ld\n", (long) rpInfo->mysocket, (long) n);
#ifdef VERBOSE
			else TraceLog(rpInfo->TraceName, TRUE, NULL, "PortUDPListen:select(%ld) timeout %ld left\n", (long) rpInfo->mysocket, (long) timeout);
#endif
			if (rpInfo->Transmit.Count)	/* Something waiting? */
			{
rpInfo->Line = __LINE__;
				PortUdpTransmitQueue(rpInfo, rpInfo->mysocket);
rpInfo->Line = __LINE__;
			}
		}
rpInfo->Line = __LINE__;
		if (!rpInfo->Enabled || n)	/* Errors mean we're outa here */
			break;
		if (Available)
		{
#ifdef VERBOSE
TraceLog(rpInfo->TraceName, FALSE, NULL, "PortUDPListen:Got %ld Available with %ldmsec left\n", (long) Available, (long) timeout);
#endif
rpInfo->Line = __LINE__;
			if (recvlen >= recvsize-1)
			{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Flushing %ld/%ld Bytes In Buffer\n", (long) recvlen, (long) recvsize);
				PortDumpHex(rpInfo->TraceName, "Flush", recvlen, recvbuf);
				recvlen = 0;
			}

			if (!UdpRecvRtn(&rpInfo->mysocket, 0, recvbuf+recvlen, recvsize-recvlen-1, &n))
			{	int Consumed = 0;
thisIdle = PortGetMsec()-rpInfo->lastActive;
if (thisIdle > maxIdle)
{TraceLog(rpInfo->TraceName, FALSE, NULL, "Read %ld bytes after %.0lf msec (up from %.0lf) vs %ld Quiet\n",
			(long) n, (double) thisIdle, (double) maxIdle, (long) rpInfo->pcInfo.QuietTime*1000);
maxIdle = thisIdle;
}
				rpInfo->lastActive = PortGetMsec();
				recvlen += n;
				recvbuf[recvlen] = 0;	/* Null terminate for safety */

rpInfo->Line = __LINE__;

				do
				{	char *Formatted = rpInfo->Actions->FormatReceive(recvlen, recvbuf, &Consumed);
#ifdef VERBOSE
if (Formatted)
	TraceLog(rpInfo->TraceName, FALSE, NULL, "Formatted %ld/%ld Bytes of (%s)\n",
		 (long) Consumed, (long) recvlen, Formatted);
else if (Consumed) TraceLog(rpInfo->TraceName, FALSE, NULL, "Consumed %ld/%ld Bytes\n",
		 (long) Consumed, (long) recvlen);
#endif
					if (Formatted)
					{	SendMsgReceived(rpInfo, Formatted, FALSE);
					}
					if (Consumed)
					{	recvlen -= Consumed;
						if (recvlen)
						{	memmove(recvbuf, &recvbuf[Consumed], recvlen+1);
#ifdef VERBOSE
TraceLog(rpInfo->TraceName, TRUE, NULL,"Recovered %ld bytes of %s\n", recvlen, recvbuf);
#endif
rpInfo->Line = __LINE__;
//							PortDumpHex(NULL, "Recovered", recvlen, recvbuf);
						} else recvlen = 0;
					}
				} while (Consumed && recvlen
				&& rpInfo->Actions->GotPacket(recvlen, recvbuf));

				if (recvlen) PortDumpHex(NULL, "RecoveredEx", recvlen, recvbuf);

			} else	/* Errors mean we're done */
				break;
		}
		if (rpInfo->Transmit.Count)	/* Something waiting? */
		{
rpInfo->Line = __LINE__;
			PortUdpTransmitQueue(rpInfo, rpInfo->mysocket);
rpInfo->Line = __LINE__;
		}
	}

#ifdef FUTURE
	SetPortStatus(rpInfo, "CloseCmd", 0);
	{	unsigned long c;
		for (c=0; c<rpInfo->pcInfo.CloseCmds.Count; c++)
#ifdef USE_TIMED_STRINGS
			if (rpInfo->pcInfo.CloseCmds.Entries[c].string[0] != ';'
			&& rpInfo->pcInfo.CloseCmds.Entries[c].string[0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.CloseCmds.Entries[c].string, NULL);
#else
			if (rpInfo->pcInfo.CloseCmds.Strings[c][0] != ';'
			&& rpInfo->pcInfo.CloseCmds.Strings[c][0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.CloseCmds.Strings[c], NULL);
#endif
	}
#endif

	TraceLog(rpInfo->TraceName, TRUE, NULL, "Terminating after %.0lf msec vs %ld Quiet\n",
			(double) PortGetMsec()-rpInfo->lastActive, (long) rpInfo->pcInfo.QuietTime*1000);

	rpInfo->Line = __LINE__;
	soclose(rpInfo->mysocket);
	rpInfo->mysocket = 0;

exit:
rpInfo->Line = __LINE__;

	free(Arg);

rpInfo->Line = __LINE__;

	return 0;
}













long  SocketSendRtn
(
        int *psocket,
        char *OutBuffer,
        long  OutLength,
        long  *RetBytesSent
)
{	*RetBytesSent = send(*psocket, OutBuffer, OutLength, 0);
	return 0;	/* It worked */
}

long  SocketRecvRtn
(
        int *psocket,
        long Timeout,
        char *InBuffer,
        long  InLength,
        long  *RetBytesRead
)
{	int n;
	unsigned long Available = 0;

	while ((n=ioctlsocket(*psocket,FIONREAD,&Available))==0
	&& Available <= 0 && Timeout > 0)
	{	double selectStart = PortGetMsec();
		struct timeval tmo;
		fd_set fdRead;

		FD_ZERO(&fdRead);
		FD_SET(*psocket, &fdRead);
//		tmo.tv_sec = Timeout/1000; tmo.tv_usec = (Timeout%1000) * 1000;	/* uSec */
#ifdef UNDER_CE
		tmo.tv_sec = 0; tmo.tv_usec = 500*1000;	/* 500msec timeout to allow transmit */
#else
		tmo.tv_sec = 0; tmo.tv_usec = 10*1000;	/* 10msec timeout to allow transmit */
#endif
		n = select(*psocket+1, &fdRead, NULL, NULL, &tmo);
		Timeout -= (long) (PortGetMsec() - selectStart);
		if (n > 0)
		{	if (FD_ISSET(*psocket, &fdRead))
			{
				if ((n=ioctlsocket(*psocket,FIONREAD,&Available))!=0
				|| Available <= 0)
					Available = 1;	/* Readable means recv() won't block */
				n = 0;	/* make it look successful */
				break;
			}
		}
	}

	if (!n && Available)
	{
		if ((n=recv(*psocket, InBuffer, InLength, 0)) > 0)
		{	*RetBytesRead = n;
			return 0;	/* It worked! */
		}
	}
	return 1;
}

static BOOL PortSocketSend(void *What, int Len, unsigned char *Buf)
{	int socket = (int) What;
	BOOL Result = TRUE;

	if (Len && Buf)
	{	long len2 = send(socket, Buf, Len, 0);
		if (len2 < 0)
		{	TraceLog(NULL, TRUE, NULL, "Write %ld Bytes FAILED\n", (long) Len);
			Result = FALSE;
		} else if (Len != len2)
		{	TraceLog(NULL, TRUE, NULL, "Only Wrote %ld/%ld Bytes\n", (long) len2, (long) Len);
			Result = FALSE;
		} else PortDumpHex(NULL, "Wrote", Len, Buf);
				//TraceLog(NULL, TRUE, NULL, "Wrote(%.*s)\n", (long) Len, Buf);
	}
	return Result;
}












#ifdef FUTURE
static void PortUdpTransmitQueue(RUNNING_PORT_INFO_S *rpInfo, int socket)
{
	if (LockAndPurgeTransmitQueue(rpInfo, 1000))
	{	XMIT_QUEUE_ENTRY_S *Entry = NULL;
		if (rpInfo->Transmit.Count)	/* Still something there? */
		{	Entry = rpInfo->Transmit.Entries[0];
			if (--rpInfo->Transmit.Count)
				memmove(&rpInfo->Transmit.Entries[0], &rpInfo->Transmit.Entries[1], sizeof(rpInfo->Transmit.Entries[0])*rpInfo->Transmit.Count);
			else
			{	free(rpInfo->Transmit.Entries);
				rpInfo->Transmit.Count = rpInfo->Transmit.Size = 0;
				rpInfo->Transmit.Entries = NULL;
			}
		}
		ReleaseMutex(rpInfo->Transmit.hmtx);
		if (Entry)			/* C0 00 <path expansion> 03 F0 <Payload> */
		{	long len, len2;
			unsigned char *Formatted = rpInfo->Actions->FormatTransmit(Entry->Data, &len, rpInfo);

			if (Formatted)
			{	if (len)
				{	if (UdpSendRtn(&socket, Formatted, len, &len2))
					{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Write %ld Bytes FAILED\n", (long) len);
					} else if (len != len2)
					{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Only Wrote %ld/%ld Bytes\n", (long) len2, (long) len);
					}
				}
				if (Formatted != Entry->Data) free(Formatted);
			}
			free(Entry);
		}
	}
}
#endif

static BOOL TCPListenMonitor(RUNNING_PORT_INFO_S *rpInfo, char *Packet)
{
	return PortTransmit(&rpInfo->pcInfo, rpInfo->Notify.wp, Packet);
}

/*	From xastir's callpass.c */

#define kKey 0x73e2 // This is the seed for the key

static short doHash(char *theCall) {
    char rootCall[10]; // need to copy call to remove ssid from parse
    char *p1 = rootCall;
    short hash;
    short i,len;
    char *ptr = rootCall;

    while ((*theCall != '-')	/* Stop at the -SSID */
	&& (*theCall != ' ')		/* or a blank (lwd's addition) */
	&& (*theCall != '\0'))		/* or the terminating null */
		*p1++ = toupper((int)(*theCall++));
    *p1 = '\0';

    hash = kKey; // Initialize with the key value
    i = 0;
    len = (short)strlen(rootCall);

    while (i<len) {// Loop through the string two bytes at a time
        hash ^= (unsigned char)(*ptr++)<<8; // xor high byte with accumulated hash
        hash ^= (*ptr++); // xor low byte with accumulated hash
        i += 2;
    }
    return (short)(hash & 0x7fff); // mask off the high bit so number is always positive
}

static short checkHash(char *theCall, short theHash) {
    return (short)(doHash(theCall) == theHash);
}

typedef struct TCP_LISTENER_SOCKET_S
{	int	s;	/* Actual socket for client */
	char	*buf;	/* Input Buffer */
	int	n;	/* Bytes Received */
	BOOL Logon;
	BOOL Verified;
	char *Partner;
	char *Callsign;
	char *FilterText;
	double NextSend;
	FILTER_INFO_S Filter;
} TCP_LISTENER_SOCKET_S;

static void PortTCPSubStatus(RUNNING_PORT_INFO_S *rpInfo, size_t csCount, TCP_LISTENER_SOCKET_S *cs, char *Prefix)
{
	size_t c, Remain = 128+(Prefix?strlen(Prefix):0)+128*csCount+1;
	char *Buffer = (char*)malloc(Remain);
	char *Next = Buffer;
	*Next = '\0';

	if (Prefix && *Prefix)
		StringCbPrintfExA(Next, Remain, &Next, &Remain, STRSAFE_IGNORE_NULLS,
							"%s", Prefix);
	for (c=0; c<csCount; c++)
	if (cs[c].s)	/* Active only */
	{	StringCbPrintfExA(Next, Remain, &Next, &Remain, STRSAFE_IGNORE_NULLS,
							"%s%ld:%s",
							*Buffer?"\n":"",
							(long) c+1,
							cs[c].Partner?cs[c].Partner:"*Unknown*");
		if (cs[c].Logon)
			StringCbPrintfExA(Next, Remain, &Next, &Remain, STRSAFE_IGNORE_NULLS,
							" %s%s", 
							cs[c].Verified?"":"UnVerified:",
							cs[c].Callsign?cs[c].Callsign:"*NOCALL*");
		else StringCbPrintfExA(Next, Remain, &Next, &Remain, STRSAFE_IGNORE_NULLS,
							" *NoLogon*");
		if (cs[c].FilterText)
			StringCbPrintfExA(Next, Remain, &Next, &Remain, STRSAFE_IGNORE_NULLS,
							" %s:%s", 
							cs[c].Filter.HasErrors?"Errors":"Filter",
							cs[c].FilterText);
		else StringCbPrintfExA(Next, Remain, &Next, &Remain, STRSAFE_IGNORE_NULLS,
							" *NoFilter*");
	}
	SetPortSubStatus(rpInfo, Buffer);
	free(Buffer);
}

static DWORD PortTCPListen(RUNNING_PORT_INFO_S *rpInfo)
{	char *Arg = SpaceCompress(-1,_strdup(rpInfo->pcInfo.Device));
	int Port;
	char *Host;

	struct sockaddr_in lclserver = {0};
	struct sockaddr_in *pserver = &lclserver;
	long timeout;
	char *recvbuf;
	int n, recvlen, recvsize;
	int Success = 0;
	char *DefaultFilter = strcmp(rpInfo->pcInfo.Protocol,OUTPUT_SERVER)?"":"b/*";
static int initialized = 0;

#ifdef UNDER_CE
#define MAX_CLIENT 6
#else
#define MAX_CLIENT 16
#endif
	char *SubStatus = NULL;
	TCP_LISTENER_SOCKET_S cs[MAX_CLIENT] = {0};

rpInfo->Line = __LINE__;

	if (*Arg != '@')
	{
TraceLog(rpInfo->TraceName, TRUE, NULL, "Invalid Arg(%s) Needs @\n", Arg);
		rpInfo->Running = FALSE;
		free(Arg);
		return 0;
	} else
	{	char *e, *c = strchr(Arg,':');
		if (!c)
		{
TraceLog(rpInfo->TraceName, TRUE, NULL, "Invalid Arg(%s) Needs :\n", Arg);
			rpInfo->Running = FALSE;
			free(Arg);
			return 0;
		}
		Host = Arg+1;	/* After the @ */
		*c++ = '\0';	/* Null terminate Host */
		Port = strtol(c,&e,10);
		if (*e) TraceLog(rpInfo->TraceName, TRUE, NULL,"Invalid Port %s, Using %ld\n", c, (long) Port);
	}

	TraceLog(rpInfo->TraceName, TRUE, NULL, "TCPListener Running on %s or %s:%ld (%ld OpenCmds, %ld CloseCmds)\n",
			Arg, Host, (long) Port,
			(long) rpInfo->pcInfo.OpenCmds.Count, (long) rpInfo->pcInfo.CloseCmds.Count);

	rpInfo->SendRtn = SocketSendRtn;
	rpInfo->ReadRtn = SocketRecvRtn;
	rpInfo->SendReadArg = &rpInfo->mysocket;

	if (!initialized)
	{	sock_init();
		initialized = 1;
	}

	SetPortStatus(rpInfo, "Resolve", 0);
	pserver->sin_family      = AF_INET;
	pserver->sin_port        = htons((u_short)Port);
	pserver->sin_addr.s_addr = inet_addr(Host);	/* argv[1] = hostname */
	if (pserver->sin_addr.s_addr == (u_long) -1)
	{	TraceLogThread(rpInfo->TraceName, TRUE, "inet_addr(%s) Failed, using 0.0.0.0\n", Host);
		
		pserver->sin_addr.s_addr = inet_addr("0.0.0.0");	/* argv[1] = hostname */
		if (pserver->sin_addr.s_addr == (u_long) -1)
		{	TraceLogThread(rpInfo->TraceName, TRUE, "inet_addr(0.0.0.0) Failed\n");
			goto exit;
		}
	}

	if (!rpInfo->Enabled) goto exit;

	if ((rpInfo->mysocket=socket(AF_INET, SOCK_STREAM, 0))<0)
	{	psock_errno("socket");
TraceLog(rpInfo->TraceName, TRUE, NULL, "socket(%s) Failed\n", Arg);
		goto exit;
	}

	SetPortStatus(rpInfo, "Bind", 0);
rpInfo->Line = __LINE__;
	TraceLog(rpInfo->TraceName, TRUE, NULL, "Binding(%s) or %s:%ld\n",
				Host, inet_ntoa(pserver->sin_addr), (long) Port);
	if (bind(rpInfo->mysocket, (struct sockaddr *) pserver, sizeof(*pserver))==-1)
	{	psock_errno("bind");
		soclose(rpInfo->mysocket);
		rpInfo->mysocket = 0;
		TraceLog(rpInfo->TraceName, TRUE, NULL, "Bind(%s) or %s:%ld Failed\n",
					Host, inet_ntoa(pserver->sin_addr), (long) Port);
		TraceLog(rpInfo->TraceName, TRUE, NULL, "bind(%s) Failed\n", Arg);
		goto exit;
	}
	

	{	char *ipaddr = inet_ntoa(pserver->sin_addr);
		TraceLog(rpInfo->TraceName, TRUE, NULL, "Bound(%s)=>%s:%ld",
				Host,ipaddr?ipaddr:"NULL", (long) Port);

		if (ipaddr)
		{	size_t len = 7+strlen(ipaddr)+1+33+1;
			SubStatus = (char*)malloc(len);
			StringCbPrintfA(SubStatus, len, "Bound: %s:%ld", ipaddr, (long) Port);
			SetPortSubStatus(rpInfo, SubStatus);
		}
	}

	if (listen(rpInfo->mysocket, MAX_CLIENT) != 0)
	{	psock_errno("listen");
		TraceLog(rpInfo->TraceName, TRUE, NULL, "listen(%s) Failed\n", Arg);
		goto exit;
	}

	rpInfo->lastActive = PortGetMsec();

#ifdef FUTURE
	if (rpInfo->Actions->NewConnection)
	{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Firing NewConnection");
		rpInfo->Actions->NewConnection((void*)rpInfo->mysocket, PortSocketSend);
		TraceLog(rpInfo->TraceName, TRUE, NULL, "Back from NewConnection");
	}
#endif

#ifdef FUTURE
	SetPortStatus(rpInfo, "OpenCmd", 0);
	{	unsigned long c;
		for (c=0; c<rpInfo->pcInfo.OpenCmds.Count; c++)
#ifdef USE_TIMED_STRINGS
			if (rpInfo->pcInfo.OpenCmds.Entries[c].string[0] != ';'
			&& rpInfo->pcInfo.OpenCmds.Entries[c].string[0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.OpenCmds.Entries[c].string, NULL);
#else
			if (rpInfo->pcInfo.OpenCmds.Strings[c][0] != ';'
			&& rpInfo->pcInfo.OpenCmds.Strings[c][0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.OpenCmds.Strings[c], NULL);
#endif
	}
#endif

	SetPortStatus(rpInfo, "OK", 0);

	recvlen = 0;
	recvsize = sizeof(rpInfo->Buffer);
	recvbuf = rpInfo->Buffer;

	while (rpInfo->Enabled	/* Loop until the port is disabled */
	&& (!rpInfo->pcInfo.QuietTime
	|| rpInfo->lastActive+rpInfo->pcInfo.QuietTime*1000 >= PortGetMsec()))	/* or times out */
	{	int c;
		unsigned long Available = 0;
		timeout = 1000;	/* 1 second timeout */

		while (rpInfo->Enabled && timeout > 0)
		{	double selectStart = PortGetMsec();
			struct timeval tmo;
			fd_set fdRead;
			int width;

			FD_ZERO(&fdRead);
			FD_SET(rpInfo->mysocket, &fdRead);
			width = rpInfo->mysocket;

			for (c=0; c<MAX_CLIENT; c++)
			{	if (cs[c].s)
				{	FD_SET( cs[c].s, &fdRead );
					if ( cs[c].s > width ) width = cs[c].s;
				}
			}

//			tmo.tv_sec = timeout/1000; tmo.tv_usec = (timeout%1000) * 1000;	/* uSec */
#ifdef UNDER_CE
			tmo.tv_sec = 0; tmo.tv_usec = 500*1000;	/* 500msec timeout to allow transmit */
#else
			tmo.tv_sec = 0; tmo.tv_usec = 10*1000;	/* 10msec timeout to allow transmit */
#endif
			n = select(width+1, &fdRead, NULL, NULL, &tmo);
			timeout -= (long) (PortGetMsec() - selectStart);
			if (n > 0)
			{
#define KEEPALIVE_TIMER 20000
				if (FD_ISSET(rpInfo->mysocket, &fdRead))	/* Incoming */
				{	struct sockaddr_in client; /* client address information            */
					int namelen = sizeof(client);
					int tSocket = accept(rpInfo->mysocket, (struct sockaddr *)&client, &namelen);
					if (tSocket == -1)
					{	psock_errno("Accept(ss)");
					} else
					{	for (c=0; c<MAX_CLIENT; c++)
						{	if (!cs[c].s) break;
						}
						if (c>=MAX_CLIENT)
						{static char sTooMany[] = "Connection Limit Exceeded\r\n";
							TraceLogThread(NULL, TRUE, "MAX_CLIENT(%ld) Exceeded\n", (long) MAX_CLIENT);
							send(tSocket, sTooMany, sizeof(sTooMany), 0);
							soclose(tSocket);
						} else
						{	cs[c].s = tSocket;
							cs[c].n = 0;
							if (!cs[c].buf) cs[c].buf = malloc(BUFFER_SIZE);
							cs[c].Verified = cs[c].Logon = FALSE;
							if (cs[c].Partner)
							{	free(cs[c].Partner);
								cs[c].Partner = NULL;
							}
							if (cs[c].Callsign)
							{	free(cs[c].Callsign);
								cs[c].Callsign = NULL;
							}
							if (cs[c].FilterText)
							{	free(cs[c].FilterText);
								cs[c].FilterText = NULL;
								FreeFilter(&cs[c].Filter);
							}
							if (*DefaultFilter)
							{	cs[c].FilterText = strdup(DefaultFilter);
								CheckOptimizedFilter(cs[c].FilterText, &cs[c].Filter);
							}
							cs[c].Partner = (char*)malloc(3+1+3+1+3+1+3+1+33+1);
							sprintf(cs[c].Partner, "%s:%ld", inet_ntoa(((struct sockaddr_in*)&client)->sin_addr), ntohs(client.sin_port));

							TraceLogThread(NULL, TRUE, "TCP-Listener accepted connection %ld/%ld from %s:%ld Filter(%s)\n",
								(long) c, (long) MAX_CLIENT, inet_ntoa(((struct sockaddr_in*)&client)->sin_addr), ntohs(client.sin_port),
								DefaultFilter);
							send(cs[c].s, cs[c].buf, sprintf(cs[c].buf, "# %s %s\r\n", PROGNAME, Timestamp), 0);
							cs[c].NextSend = PortGetMsec() + KEEPALIVE_TIMER;

							
	{	BOOL NoDelay = 1;	/* Disable Nagle algorithm for one-way sockets */
		if (setsockopt(tSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&NoDelay, sizeof(NoDelay)))
		{	psock_errno("setsockopt");
			TraceLog(rpInfo->TraceName, TRUE, NULL, "setsockopt(%ld) Failed", (long) tSocket);
		}// else 	TraceLog(rpInfo->TraceName, TRUE, NULL, "setsockopt(%ld) Worked", (long) rpInfo->mysocket);
	}

						}
						{	char Buffer[24];
							int InUse=0;
							for (c=0; c<MAX_CLIENT; c++)
							{	if (cs[c].s) InUse++;
							}
							sprintf(Buffer,"OK %ld/%ld",InUse,MAX_CLIENT);
							SetPortStatus(rpInfo, Buffer, 0);
							PortTCPSubStatus(rpInfo, MAX_CLIENT, cs, SubStatus);
						}
					}
				}
				for (c=0; c<MAX_CLIENT; c++)
				if (FD_ISSET(cs[c].s,&fdRead))
				{	int n, c2;

					if ((n=recv(cs[c].s, cs[c].buf+cs[c].n, BUFFER_SIZE-cs[c].n, 0)) == -1)
					{	psock_errno("Recv(cs[c].s)");
						if (cs[c].s) soclose(cs[c].s);
						cs[c].s = 0;
						{	char Buffer[24];
							int InUse=0, c3;
							for (c3=0; c3<MAX_CLIENT; c3++)
							{	if (cs[c3].s) InUse++;
							}
							sprintf(Buffer,"OK %ld/%ld",InUse,MAX_CLIENT);
							SetPortStatus(rpInfo, Buffer, 0);
							PortTCPSubStatus(rpInfo, MAX_CLIENT, cs, SubStatus);
						}
					} else if (!n)
					{	TraceLogThread(NULL, TRUE, "Received 0 bytes, disconnecting %ld\n", (long) c);
						if (cs[c].s) soclose(cs[c].s);
						cs[c].s = 0;
						{	char Buffer[24];
							int InUse=0, c3;
							for (c3=0; c3<MAX_CLIENT; c3++)
							{	if (cs[c3].s) InUse++;
							}
							sprintf(Buffer,"OK %ld/%ld",InUse,MAX_CLIENT);
							SetPortStatus(rpInfo, Buffer, 0);
							PortTCPSubStatus(rpInfo, MAX_CLIENT, cs, SubStatus);
						}
					} else
					{	int Consumed;
						cs[c].n += n;
						do
						{	char *Formatted = rpInfo->Actions->FormatReceive(cs[c].n, cs[c].buf, &Consumed);
							if (Formatted)
							{	int fLen = strlen(Formatted);
								double msNow = PortGetMsec();

TraceLogThread(rpInfo->TraceName, TRUE, "Processing(%s)\n", Formatted);
/*
	See if this is a server command
*/
/*
user mycall[-ss] pass passcode[ vers softwarename softwarevers[ UDP udpport][ servercommand]]

The filter command may be set as part of the login line, as an APRS message to SERVER, or as a separate comment line (#filter r/33/-97/200). The prefered method is to set the command as part of the login which is supported by most current APRS software.
*/
								if (!_strnicmp(Formatted, "user ", 5))
								{	BOOL FilterOK = TRUE;
									char *u, *p;
									u = Formatted+5;
									while (*u && isspace(*u&0xff)) u++;
									p = strstr(u, " pass ");
									if (p)	/* Have a password, now validate it */
									{	char *e;

										if (cs[c].Callsign) free(cs[c].Callsign);
										cs[c].Callsign = (char *)malloc(p-u+1);
										strncpy(cs[c].Callsign, u, p-u);
										cs[c].Callsign[p-u] = '\0';
										RtStrnTrim(-1, cs[c].Callsign);

										p += 6;
										if (checkHash(u, (short) strtol(p,&e,10)))
										{	cs[c].Verified = TRUE;	/* Got it right! */
										}
										cs[c].Logon = TRUE;
									}
									if ((p=strstr(Formatted,"filter ")))
									{	if (cs[c].FilterText) free(cs[c].FilterText);
										cs[c].FilterText = strdup(p+7);
										FilterOK = CheckOptimizedFilter(cs[c].FilterText, &cs[c].Filter);
									}
									TraceLogThread(rpInfo->TraceName, TRUE, "[%ld] %s %sVerified %sFilter(%s) (%s)\n", c, cs[c].Callsign, cs[c].Verified?"":"NOT ", FilterOK?"":"BAD ", cs[c].FilterText?cs[c].FilterText:"*NULL*", Formatted);
/* # logresp KJ4ERJ-TS unverified, server KJ4ERJ-5 */
/* # logresp KJ4ERJ-TS verified, server KJ4ERJ-5 */
/* # logresp KJ4ERJ-TS verified, server KJ4ERJ-5, adjunct "filter m/10" OK - Filter definition updated */
									p = malloc(512);
									send(cs[c].s, p, sprintf(p,"# logresp CALLSIGN %s, server %s\r\n",
																cs[c].Verified?"verified":"unverified", CALLSIGN), 0);
									free(p);
									cs[c].NextSend = msNow + KEEPALIVE_TIMER;
									PortTCPSubStatus(rpInfo, MAX_CLIENT, cs, SubStatus);
								} else if (!_strnicmp(Formatted, "#filter ",8))
								{	char *p;
									BOOL FilterOK;
									if (cs[c].FilterText) free(cs[c].FilterText);
									cs[c].FilterText = strdup(Formatted+8);
									FilterOK = CheckOptimizedFilter(cs[c].FilterText, &cs[c].Filter);
									TraceLogThread(rpInfo->TraceName, TRUE, "[%ld] %s %sVerified %sFilter(%s) (%s)\n", c, cs[c].Callsign, cs[c].Verified?"":"NOT ", FilterOK?"":"BAD ", cs[c].FilterText?cs[c].FilterText:"*NULL*", Formatted);
									p = malloc(512);
									if (FilterOK)
										send(cs[c].s, p, sprintf(p,"# filter updated\r\n"), 0);
									else send(cs[c].s, p, sprintf(p,"# INVALID filter\r\n"), 0);
									free(p);
									cs[c].NextSend = msNow + KEEPALIVE_TIMER;
									PortTCPSubStatus(rpInfo, MAX_CLIENT, cs, SubStatus);
								} else if (strstr(Formatted,"::SERVER   :"))
/* KJ4ERJ-AP>APWW06::SERVER   :filter b/N1NAZ ... b/AVRS{0001 */
								{	char *p = strstr(Formatted,"::SERVER   :")+12;
									if (!_strnicmp(p,"filter ",7))
									{	BOOL FilterOK;
										if (cs[c].FilterText) free(cs[c].FilterText);
										cs[c].FilterText = strdup(p+7);
										FilterOK = CheckOptimizedFilter(cs[c].FilterText, &cs[c].Filter);
										TraceLogThread(rpInfo->TraceName, TRUE, "[%ld] %s %sVerified %sFilter(%s) (%s)\n", c, cs[c].Callsign, cs[c].Verified?"":"NOT ", FilterOK?"":"BAD ", cs[c].FilterText?cs[c].FilterText:"*NULL*", Formatted);
										p = malloc(512);
										if (strchr(Formatted,'>'))
										{	send(cs[c].s, p, sprintf(p,"SERVER>%s,%s*::%-9.*s:%s\r\n",DESTID,CALLSIGN,(int)(strchr(Formatted,'>')-Formatted),Formatted,FilterOK?"filter updated":"INVALID filter"), 0);
										} else send(cs[c].s, p, sprintf(p,"# %s\r\n",FilterOK?"filter updated":"INVALID filter"), 0);
										free(p);
										cs[c].NextSend = msNow + KEEPALIVE_TIMER;
										PortTCPSubStatus(rpInfo, MAX_CLIENT, cs, SubStatus);
									} else TraceLogThread(rpInfo->TraceName, TRUE, "Unrecognized SERVER command in (%s)\n", Formatted);
								} else	/* Must be a forwardable packet */
								{	double NextSend = msNow + KEEPALIVE_TIMER;
									char *Parsed = NULL, *Xmit = NULL;
									long XmitLen = 0, XmitCount = 0;
									APRS_PARSED_INFO_S APRS;

	{	char *q = strstr(Formatted, ",q");	/* Any q-construct? */
		char *a = strchr(Formatted, '>');	/* And the fromCall */
		char *p = strchr(Formatted, ':');	/* And the payload */
		if ((p && a && p > a)	/* Must have the > and : in sequence */
		&& (!q || q > p)	/* No Q-Construct yet (or inside payload) */
		&& cs[c].Verified	/* Must be verified */
		&& cs[c].Callsign	/* Must know who */
		&& !strncmp(Formatted, cs[c].Callsign, (a-Formatted)))	/* From it */
		{	char *Packet = (char*)malloc(strlen(Formatted)+4+1+sizeof(CALLSIGN)+1);
			int Len = (p-Formatted);
			strncpy(Packet,Formatted,Len);	/* Copy the header in */
			Len += sprintf(Packet+Len,",qAC,%s",CALLSIGN);	/* Add qAC */
			strcpy(Packet+Len,p);	/* Add the payload */
			free(Formatted);
			Formatted = Packet;
TraceLogThread(rpInfo->TraceName, TRUE, "qAC'd(%s)\n", Formatted);
		}
		else TraceLogThread(rpInfo->TraceName, TRUE, "NOT adding qAC to %sverified[%ld](%s) %s\n", cs[c].Verified?"":"UN", (long) c, cs[c].Callsign?cs[c].Callsign:"*NOCALL*", Formatted);
	}
									for (c2=0; c2<MAX_CLIENT; c2++)
									{	if (c2!=c && cs[c2].s)
										if (cs[c2].Logon)
										{	if (!Parsed)
											{	Parsed = strdup(Formatted);
												if (!parse_full_aprs(Parsed, &APRS))
												{	PortDumpHex(rpInfo->TraceName, "ParseFail", -1, Formatted);
													break;	/* Can't filter what we can't parse */
												}
											}
											if (FilterPacket(&cs[c2].Filter, &APRS)
											|| (cs[c2].Callsign && !strcmp(APRS.msgCall, cs[c2].Callsign)))
											{	if (!Xmit)
												{	Xmit = rpInfo->Actions->FormatTransmit(Formatted, &XmitLen, rpInfo);
												}
											TraceLogThread(rpInfo->TraceName, FALSE, "Echoing[%ld][%s](%s) Valid[0x%lX](%s)\n", c2, cs[c2].Callsign?cs[c2].Callsign:"*NONE*", Formatted, APRS.Valid, GetHitNixDetail(&cs[c2].Filter));
												if (send(cs[c2].s, Xmit, XmitLen, 0) < 0)
												{	psock_errno("Send(cs[c2].s)");
													if (cs[c2].s) soclose(cs[c2].s);
													cs[c2].s = 0;
					{	char Buffer[24];
						int InUse=0, c3;
						for (c3=0; c3<MAX_CLIENT; c3++)
						{	if (cs[c3].s) InUse++;
						}
						sprintf(Buffer,"OK %ld/%ld",InUse,MAX_CLIENT);
						SetPortStatus(rpInfo, Buffer, 0);
						PortTCPSubStatus(rpInfo, MAX_CLIENT, cs, SubStatus);
					}
												} else
												{	cs[c2].NextSend = NextSend;
													XmitCount++;
												}
											}
										}
									}
									if (Xmit && Xmit != Formatted) free(Xmit);
									if (Parsed) free(Parsed);
									PortDumpHex(rpInfo->TraceName, "Forwarding", -1, Formatted);
									SendMsgReceived(rpInfo, Formatted, cs[c].Verified);
									//else free(Formatted);
									if (XmitCount)
										PostMessage(rpInfo->Notify.hwnd, rpInfo->Notify.msgXmitCount,
													rpInfo->Notify.wp, (LPARAM) XmitCount);
								}
							}
							if (Consumed)
							{
								PortDumpHex(rpInfo->TraceName, "Consumed", Consumed, cs[c].buf);
								cs[c].n -= Consumed;
								if (cs[c].n)
								{	memmove(cs[c].buf, &cs[c].buf[Consumed], cs[c].n+1);
		//							PortDumpHex(NULL, "Recovered", recvlen, recvbuf);
								} else cs[c].n = 0;
							}
						} while (Consumed && cs[c].n
						&& rpInfo->Actions->GotPacket(cs[c].n, cs[c].buf));
					}
				}
				n = 0;	/* Make it successful */
			} else if (n < 0)
			{	psock_errno("select");
				TraceLog(rpInfo->TraceName, TRUE, NULL, "PortTcpListen:select(%ld) return %ld\n", (long) rpInfo->mysocket, (long) n);
				break;
			}
#define XMIT_LOCK_TMO 1000
			{double msEnd = PortGetMsec()+XMIT_LOCK_TMO;
			long XmitCount = 0;
			while (rpInfo->Transmit.Count
			&& PortGetMsec() < msEnd)
//			if (rpInfo->Transmit.Count)	/* Make sure it's worth locking */
			if (LockAndPurgeTransmitQueue(rpInfo, XMIT_LOCK_TMO))
			{	XMIT_QUEUE_ENTRY_S *Entry = NULL;
				if (rpInfo->Transmit.Count)	/* Still something there? */
				{	Entry = rpInfo->Transmit.Entries[0];
					if (--rpInfo->Transmit.Count)
						memmove(&rpInfo->Transmit.Entries[0], &rpInfo->Transmit.Entries[1], sizeof(rpInfo->Transmit.Entries[0])*rpInfo->Transmit.Count);
					else
					{	free(rpInfo->Transmit.Entries);
						rpInfo->Transmit.Count = rpInfo->Transmit.Size = 0;
						rpInfo->Transmit.Entries = NULL;
					}
				}
				ReleaseMutex(rpInfo->Transmit.hmtx);
				if (Entry)
				{	long len;
					unsigned char *Formatted = rpInfo->Actions->FormatTransmit(Entry->Data, &len, rpInfo);
					if (Formatted)
					{	if (len)
						{	double msNow = PortGetMsec();
							double msAge = msNow - Entry->msQueued;
							double NextSend = msNow + KEEPALIVE_TIMER;
							int c2, len2=0;
							char *Parsed = NULL;
							APRS_PARSED_INFO_S APRS;

							for (c2=0; c2<MAX_CLIENT; c2++)
							{	if (cs[c2].s)
								{	if (!Parsed)
									{	Parsed = malloc(len+1);
										strncpy(Parsed, Formatted, len);
										Parsed[len] = '\0';
										if (!parse_full_aprs(Parsed, &APRS))
										{	PortDumpHex(rpInfo->TraceName, "ParseFail", len, Formatted);
											break;	/* Can't filter what we can't parse */
										}
//else TraceLogThread(rpInfo->TraceName, FALSE, "Parse[%ld][0x%lX](%.*s)\n", c2, APRS.Valid, (int) len, Formatted);
									}
									if (!len2)
									{	len2 = len;
										while (len2
										&& (Formatted[len2-1] == '\r'
											|| Formatted[len2-1] == '\n'))
											len2--;
									}
									if (FilterPacket(&cs[c2].Filter, &APRS)
									|| (cs[c2].Callsign
										&& *APRS.msgCall
										&& IsSameBaseCallsign(APRS.msgCall, cs[c2].Callsign)))
									{
TraceLogThread(rpInfo->TraceName, msAge>=XMIT_LOCK_TMO, "Xmit(%ldms/%ld)[%ld][%s](%s-%lX)(%.*s)\n", (long) msAge, (long) rpInfo->Transmit.Count, c2, cs[c2].Callsign?cs[c2].Callsign:"*NONE*", GetHitNixDetail(&cs[c2].Filter), APRS.Valid, (int) len2, Formatted);
										if (send(cs[c2].s, Formatted, len, 0) < 0)
										{	psock_errno("Send(cs[c2].s)");
											if (cs[c2].s) soclose(cs[c2].s);
											cs[c2].s = 0;
						{	char Buffer[24];
							int InUse=0, c3;
							for (c3=0; c3<MAX_CLIENT; c3++)
							{	if (cs[c3].s) InUse++;
							}
							sprintf(Buffer,"OK %ld/%ld",InUse,MAX_CLIENT);
							SetPortStatus(rpInfo, Buffer, 0);
							PortTCPSubStatus(rpInfo, MAX_CLIENT, cs, SubStatus);
						}
										} else
										{	cs[c2].NextSend = NextSend;
											XmitCount++;
										}
									}
									else if (Port == 6360 || *APRS.msgCall) TraceLogThread(rpInfo->TraceName, msAge>=XMIT_LOCK_TMO, "NOTxmit(%ldms/%ld)[%ld][%s](%s-%lX)(%.*s)\n", (long) msAge, (long) rpInfo->Transmit.Count, c2, cs[c2].Callsign?cs[c2].Callsign:"*NONE*", GetHitNixDetail(&cs[c2].Filter), APRS.Valid, (int) len2, Formatted);
								}
							}
							if (Parsed) free(Parsed);
						}
						if (Formatted != Entry->Data) free(Formatted);
					}
					free(Entry);
				}
			}
else TraceLogThread(rpInfo->TraceName, TRUE, "LockAndPurgeTransmitQueue(%ld) Failed, %ld in queue\n",
					(long) XMIT_LOCK_TMO, (long) rpInfo->Transmit.Count);
if (rpInfo->Transmit.Count)
	TraceLogThread(rpInfo->TraceName, TRUE, "Leaving %ld in queue after %ldms\n", (long) rpInfo->Transmit.Count, (long) (PortGetMsec()-msEnd+XMIT_LOCK_TMO));
			if (XmitCount)
				PostMessage(rpInfo->Notify.hwnd, rpInfo->Notify.msgXmitCount,
							rpInfo->Notify.wp, (LPARAM) XmitCount);
			}
			{	double msNow = PortGetMsec();
				int c2;
				for (c2=0; c2<MAX_CLIENT; c2++)
				if (cs[c2].s && cs[c2].NextSend <= msNow)
				{	char *p = malloc(512);
					SYSTEMTIME stNow;
					GetSystemTime(&stNow);
/* # javAPRSSrvr 3.15b06 5 Feb 2011 06:33:40 GMT KJ4ERJ-5 192.168.10.12:14580 */
					if (send(cs[c2].s, p, sprintf(p, "# %s %s %04ld-%02ld-%02ld %02ld:%02ld:%02ld GMT %s\r\n", PROGNAME, Timestamp, stNow.wYear, stNow.wMonth, stNow.wDay, stNow.wHour, stNow.wMinute, stNow.wSecond, CALLSIGN), 0) < 0)
					{	psock_errno("Send(cs[c2].s)");
						if (cs[c2].s) soclose(cs[c2].s);
						cs[c2].s = 0;
						{	char Buffer[24];
							int InUse=0, c3;
							for (c3=0; c3<MAX_CLIENT; c3++)
							{	if (cs[c3].s) InUse++;
							}
							sprintf(Buffer,"OK %ld/%ld",InUse,MAX_CLIENT);
							SetPortStatus(rpInfo, Buffer, 0);
							PortTCPSubStatus(rpInfo, MAX_CLIENT, cs, SubStatus);
						}
					}
					free(p);
					cs[c2].NextSend = msNow + KEEPALIVE_TIMER;
				}
			}
		}
		if (!rpInfo->Enabled || n)	/* Errors mean we're outa here */
			break;
	}

#ifdef FUTURE
	SetPortStatus(rpInfo, "CloseCmd", 0);
	{	unsigned long c;
		for (c=0; c<rpInfo->pcInfo.CloseCmds.Count; c++)
#ifdef USE_TIMED_STRINGS
			if (rpInfo->pcInfo.CloseCmds.Entries[c].string[0] != ';'
			&& rpInfo->pcInfo.CloseCmds.Entries[c].string[0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.CloseCmds.Entries[c].string, NULL);
#else
			if (rpInfo->pcInfo.CloseCmds.Strings[c][0] != ';'
			&& rpInfo->pcInfo.CloseCmds.Strings[c][0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.CloseCmds.Strings[c], NULL);
#endif
	}
#endif

	TraceLog(rpInfo->TraceName, TRUE, NULL, "Terminating after %.0lf msec vs %ld Quiet\n",
			(double) PortGetMsec()-rpInfo->lastActive, (long) rpInfo->pcInfo.QuietTime*1000);

	{	int c;
		for (c=0; c<MAX_CLIENT; c++)
		{	if (cs[c].s) soclose(cs[c].s);
			if (cs[c].buf) free(cs[c].buf);
		}
	}

	soclose(rpInfo->mysocket);
	rpInfo->mysocket = 0;

exit:
	free(Arg);
	SetPortStatus(rpInfo, "Gone", 0);
	PortTCPSubStatus(rpInfo, 0, cs, NULL);
	return 0;
}

static void PortSocketTransmitQueue(RUNNING_PORT_INFO_S *rpInfo, int socket)
{
	if (LockAndPurgeTransmitQueue(rpInfo, 1000))
	{	XMIT_QUEUE_ENTRY_S *Entry = NULL;
		if (rpInfo->Transmit.Count)	/* Still something there? */
		{	Entry = rpInfo->Transmit.Entries[0];
			if (--rpInfo->Transmit.Count)
				memmove(&rpInfo->Transmit.Entries[0], &rpInfo->Transmit.Entries[1], sizeof(rpInfo->Transmit.Entries[0])*rpInfo->Transmit.Count);
			else
			{	free(rpInfo->Transmit.Entries);
				rpInfo->Transmit.Count = rpInfo->Transmit.Size = 0;
				rpInfo->Transmit.Entries = NULL;
			}
		}
		ReleaseMutex(rpInfo->Transmit.hmtx);
		if (Entry)			/* C0 00 <path expansion> 03 F0 <Payload> */
		{	long len, len2;
			unsigned char *Formatted = rpInfo->Actions->FormatTransmit(Entry->Data, &len, rpInfo);

			if (Formatted)
			{	if (len)
				{	len2 = send(socket, Formatted, len, 0);
					if (len2 < 0)
					{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Write %ld Bytes FAILED\n", (long) len);
					} else if (len != len2)
					{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Only Wrote %ld/%ld Bytes\n", (long) len2, (long) len);
					}
				}
				if (Formatted != Entry->Data) free(Formatted);
			}
			free(Entry);
		}
	}
}


static DWORD WINAPI PortSocketRead(RUNNING_PORT_INFO_S *rpInfo, BOOL LogCritical /*, int rpInfo->Socket */)
{	struct sockaddr_in lclserver = {0};
	struct sockaddr_in *pserver = &lclserver;
	long timeout;
	char *recvbuf;
	int n, recvlen, recvsize, prevextra=0;
	int Success = 0;
	double maxIdle=0.0, thisIdle;

	rpInfo->lastActive = PortGetMsec();

	if (rpInfo->Actions->NewConnection)
	{	TraceLog(rpInfo->TraceName, LogCritical, NULL, "Firing NewConnection");
		rpInfo->Actions->NewConnection((void*)rpInfo->mysocket, PortSocketSend);
		TraceLog(rpInfo->TraceName, LogCritical, NULL, "Back from NewConnection");
	}

	SetPortStatus(rpInfo, "OpenCmd", 0);
	{	unsigned long c;
		for (c=0; c<rpInfo->pcInfo.OpenCmds.Count; c++)
#ifdef USE_TIMED_STRINGS
			if (rpInfo->pcInfo.OpenCmds.Entries[c].string[0] != ';'
			&& rpInfo->pcInfo.OpenCmds.Entries[c].string[0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.OpenCmds.Entries[c].string, NULL);
#else
			if (rpInfo->pcInfo.OpenCmds.Strings[c][0] != ';'
			&& rpInfo->pcInfo.OpenCmds.Strings[c][0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.OpenCmds.Strings[c], NULL);
#endif
	}

	SetPortStatus(rpInfo, "OK", 0);
	
rpInfo->Line = __LINE__;

	recvlen = 0;
	recvsize = sizeof(rpInfo->Buffer);
	recvbuf = rpInfo->Buffer;

	while (rpInfo->Enabled	/* Loop until the port is disabled */
	&& (!rpInfo->pcInfo.QuietTime
	|| rpInfo->lastActive+rpInfo->pcInfo.QuietTime*1000 >= PortGetMsec()))	/* or times out */
	{	unsigned long Available = 0;
		timeout = 1000;	/* 1 second timeout */
rpInfo->Line = __LINE__;
		while (rpInfo->Enabled && (n=ioctlsocket(rpInfo->mysocket,FIONREAD,&Available))==0 && Available <= 0 && timeout > 0)
		{	double selectStart = PortGetMsec();
			struct timeval tmo;
			fd_set fdRead;

			FD_ZERO(&fdRead);
			FD_SET(rpInfo->mysocket, &fdRead);
//			tmo.tv_sec = timeout/1000; tmo.tv_usec = (timeout%1000) * 1000;	/* uSec */
#ifdef UNDER_CE
			tmo.tv_sec = 0; tmo.tv_usec = 500*1000;	/* 500msec timeout to allow transmit */
#else
			tmo.tv_sec = 0; tmo.tv_usec = 10*1000;	/* 10msec timeout to allow transmit */
#endif
			rpInfo->Line = __LINE__;
			n = select(rpInfo->mysocket+1, &fdRead, NULL, NULL, &tmo);
rpInfo->Line = __LINE__;
			timeout -= (long) (PortGetMsec() - selectStart);
			if (n > 0)
			{	if (FD_ISSET(rpInfo->mysocket, &fdRead))
				{
rpInfo->Line = __LINE__;
					if ((n=ioctlsocket(rpInfo->mysocket,FIONREAD,&Available))!=0 || Available <= 0)
						Available = 1;	/* Readable means recv() won't block */
#ifdef VERBOSE
					else if (Available) TraceLog(rpInfo->TraceName, FALSE, NULL, "PortSocketRead:readable %ld has %ld Available\n", rpInfo->mysocket, Available);
#endif
					n = 0;	/* make it look successful */
					break;
				}
			} else if (n < 0)
				TraceLog(rpInfo->TraceName, LogCritical, NULL, "PortSocketRead:select(%ld) return %ld\n", (long) rpInfo->mysocket, (long) n);
#ifdef VERBOSE
			else TraceLog(rpInfo->TraceName, TRUE, NULL, "PortSocketRead:select(%ld) timeout %ld left\n", (long) rpInfo->mysocket, (long) timeout);
#endif
			if (rpInfo->Transmit.Count)	/* Something waiting? */
			{
rpInfo->Line = __LINE__;
				PortSocketTransmitQueue(rpInfo, rpInfo->mysocket);
rpInfo->Line = __LINE__;
			}
		}
rpInfo->Line = __LINE__;
		if (!rpInfo->Enabled || n)	/* Errors mean we're outa here */
			break;
		if (Available)
		{
#ifdef VERBOSE
TraceLog(rpInfo->TraceName, FALSE, NULL, "PortSocketRead:Got %ld Available with %ldmsec left\n", (long) Available, (long) timeout);
#endif
rpInfo->Line = __LINE__;
			if (recvlen >= recvsize-1)
			{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Flushing %ld/%ld Bytes In Buffer\n", (long) recvlen, (long) recvsize);
				PortDumpHex(rpInfo->TraceName, "Flush", recvlen, recvbuf);
				recvlen = 0;
			}
			if ((n=recv(rpInfo->mysocket, recvbuf+recvlen, recvsize-recvlen-1, 0)) > 0)
			{	int Consumed = 0;
//TraceLog(rpInfo->TraceName, TRUE, NULL, "Avail:%ld recv(%ld+%ld)=%ld\n", (long) Available, (long) recvlen, (long) recvsize-recvlen-1, (long) n);
//PortDumpHex(rpInfo->TraceName, "Recv", recvlen+n, recvbuf);

thisIdle = PortGetMsec()-rpInfo->lastActive;
if (thisIdle > maxIdle)
{TraceLog(rpInfo->TraceName, FALSE, NULL, "Read %ld bytes after %.0lf msec (up from %.0lf) vs %ld Quiet\n",
			(long) n, (double) thisIdle, (double) maxIdle, (long) rpInfo->pcInfo.QuietTime*1000);
maxIdle = thisIdle;
}
				rpInfo->lastActive = PortGetMsec();
				recvlen += n;
				recvbuf[recvlen] = 0;	/* Null terminate for safety */

rpInfo->Line = __LINE__;

				do
				{	char *Formatted = rpInfo->Actions->FormatReceive(recvlen, recvbuf, &Consumed);
					if (Consumed < 0 || Consumed > recvlen)
						TraceLogThread(NULL, TRUE, "Consumed %ld/%ld Bytes, HOW?\n", Consumed, recvlen);
#ifdef VERBOSE
if (Formatted)
	TraceLog(rpInfo->TraceName, FALSE, NULL, "Formatted %ld/%ld Bytes of (%s)\n",
		 (long) Consumed, (long) recvlen, Formatted);
else if (Consumed) TraceLog(rpInfo->TraceName, FALSE, NULL, "Consumed %ld/%ld Bytes\n",
		 (long) Consumed, (long) recvlen);
#endif
					if (Formatted)
					{	SendMsgReceived(rpInfo, Formatted, TRUE);
					}
					if (Consumed)
					{
						recvlen -= Consumed;
						if (recvlen > 0)
						{	memmove(recvbuf, &recvbuf[Consumed], recvlen+1);
#ifdef VERBOSE
TraceLog(rpInfo->TraceName, TRUE, NULL,"Recovered %ld bytes of %s\n", recvlen, recvbuf);
#endif
rpInfo->Line = __LINE__;
//							PortDumpHex(NULL, "Recovered", recvlen, recvbuf);
						} else recvlen = 0;
					}
				} while (Consumed && recvlen
				&& rpInfo->Actions->GotPacket(recvlen, recvbuf));

				if (recvlen && recvlen == prevextra)
				{	PortDumpHex(NULL, "RecoveredEx", recvlen, recvbuf);
				} else prevextra = recvlen;

			} else	/* Errors mean we're done */
			{	TraceLog(rpInfo->TraceName, LogCritical, NULL, "PortSocketRead:recv(%ld) returned %ld\n", (long) rpInfo->mysocket, (long) n);
				break;
			}
		}
		if (rpInfo->Transmit.Count)	/* Something waiting? */
		{
rpInfo->Line = __LINE__;
			PortSocketTransmitQueue(rpInfo, rpInfo->mysocket);
rpInfo->Line = __LINE__;
		}
	}

	SetPortStatus(rpInfo, "CloseCmd", 0);
	{	unsigned long c;
		for (c=0; c<rpInfo->pcInfo.CloseCmds.Count; c++)
#ifdef USE_TIMED_STRINGS
			if (rpInfo->pcInfo.CloseCmds.Entries[c].string[0] != ';'
			&& rpInfo->pcInfo.CloseCmds.Entries[c].string[0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.CloseCmds.Entries[c].string, NULL);
#else
			if (rpInfo->pcInfo.CloseCmds.Strings[c][0] != ';'
			&& rpInfo->pcInfo.CloseCmds.Strings[c][0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.CloseCmds.Strings[c], NULL);
#endif
	}

	TraceLog(rpInfo->TraceName, TRUE, NULL, "Terminating after %.0lf msec vs %ld Quiet\n",
			(double) PortGetMsec()-rpInfo->lastActive, (long) rpInfo->pcInfo.QuietTime*1000);

	rpInfo->Line = __LINE__;
	soclose(rpInfo->mysocket);
	rpInfo->mysocket = 0;

rpInfo->Line = __LINE__;

	return 0;

}

static DWORD WINAPI PortBTRead(RUNNING_PORT_INFO_S *rpInfo, BOOL LogCritical)
{	char *Arg = _strdup(rpInfo->pcInfo.Device);

static int initialized = 0;

rpInfo->Line = __LINE__;

	if (strncmp(Arg,"@BT:",4))
	{
TraceLog(rpInfo->TraceName, TRUE, NULL, "Invalid Arg(%s) Needs @BT:\n", Arg);
		rpInfo->Running = FALSE;
		free(Arg);
		return 0;
	}

	TraceLog(rpInfo->TraceName, LogCritical, NULL, "BlueToothReader Running on %s (%ld OpenCmds, %ld CloseCmds)\n",
			Arg,
			(long) rpInfo->pcInfo.OpenCmds.Count, (long) rpInfo->pcInfo.CloseCmds.Count);

	rpInfo->SendRtn = SocketSendRtn;
	rpInfo->ReadRtn = SocketRecvRtn;
	rpInfo->SendReadArg = &rpInfo->mysocket;

rpInfo->Line = __LINE__;

	if (!initialized)
	{	sock_init();
		initialized = 1;
	}

rpInfo->Line = __LINE__;
	SetPortStatus(rpInfo, "Connect", 0);
rpInfo->Line = __LINE__;
	if ((rpInfo->mysocket = ConnectBTSocket(Arg+4)) == INVALID_SOCKET)
	{
TraceLog(rpInfo->TraceName, LogCritical, NULL, "ConnectBTSocket(%s) Failed\n", Arg);
		goto exit;
	}

	free(Arg);

	return PortSocketRead(rpInfo, LogCritical);

exit:
	free(Arg);
	return 0;
}

static DWORD WINAPI PortTcpRead(RUNNING_PORT_INFO_S *rpInfo, BOOL LogCritical)
{	char *Arg = SpaceCompress(-1,_strdup(rpInfo->pcInfo.Device));
	int Port;
	char *Host;
	struct sockaddr_in lclserver = {0};
	struct sockaddr_in *pserver = &lclserver;

static int initialized = 0;

rpInfo->Line = __LINE__;

	if (*Arg != '@')
	{
TraceLog(rpInfo->TraceName, TRUE, NULL, "Invalid Arg(%s) Needs @\n", Arg);
		rpInfo->Running = FALSE;
		free(Arg);
		return 0;
	} else
	{	char *e, *c = strchr(Arg,':');
		if (!c)
		{
TraceLog(rpInfo->TraceName, TRUE, NULL, "Invalid Arg(%s) Needs :\n", Arg);
			rpInfo->Running = FALSE;
			free(Arg);
			return 0;
		}
		Host = Arg+1;	/* After the @ */
		*c++ = '\0';	/* Null terminate Host */
		Port = strtol(c,&e,10);
		if (*e) TraceLog(rpInfo->TraceName, TRUE, NULL,"Invalid Port %s, Using %ld\n", c, (long) Port);
	}

	TraceLog(rpInfo->TraceName, LogCritical, NULL, "TcpReader Running on %s or %s:%ld (%ld OpenCmds, %ld CloseCmds)\n",
			Arg, Host, (long) Port,
			(long) rpInfo->pcInfo.OpenCmds.Count, (long) rpInfo->pcInfo.CloseCmds.Count);

	rpInfo->SendRtn = SocketSendRtn;
	rpInfo->ReadRtn = SocketRecvRtn;
	rpInfo->SendReadArg = &rpInfo->mysocket;

rpInfo->Line = __LINE__;

	if (!initialized)
	{	sock_init();
		initialized = 1;
	}

rpInfo->Line = __LINE__;
	SetPortStatus(rpInfo, "Resolve", 0);
	pserver->sin_family      = AF_INET;
	pserver->sin_port        = htons((u_short)Port);
	pserver->sin_addr.s_addr = inet_addr(Host);	/* argv[1] = hostname */
	if (pserver->sin_addr.s_addr == (u_long) -1)
	{	struct hostent *hostnm = gethostbyname(Host);
		if (!hostnm)
		{	psock_errno("gethostbyname");
TraceLog(rpInfo->TraceName, LogCritical, NULL, "gethostbyname(%s) Failed\n", Host);
			goto exit;
		}
		{	int h, c=0, z=0;
			unsigned long *Addrs;

			for (h=0; hostnm->h_addr_list[h]; h++)
			{	struct in_addr addr;
				char *ipaddr;
				addr.s_addr = *((unsigned long *)hostnm->h_addr_list[h]);
				ipaddr = inet_ntoa(addr);
				TraceLog(rpInfo->TraceName, LogCritical, NULL, "%s[%ld] = %s\n",
							Host, (long) h, ipaddr);
				c++;	/* Count the translation */
				if (!strcmp(ipaddr+strlen(ipaddr)-2,".0")) z++;
			}
			for (h=0; hostnm->h_aliases[h]; h++)
			{	TraceLog(rpInfo->TraceName, LogCritical, NULL, "Alias[%ld]:%s\n",
							(long) h, hostnm->h_aliases[h]);
			}

			if (z && c <= z)	/* Have zeros and they're all zeros? */
			{	z = 0;			/* Need to keep them */
			} else c -= z;		/* Reduce the available count */

			Addrs = (unsigned long *)malloc(sizeof(*Addrs)*c);
			c = 0;
			for (h=0; hostnm->h_addr_list[h]; h++)
			{	if (z)
				{	struct in_addr addr;
					char *ipaddr;
					addr.s_addr = *((unsigned long *)hostnm->h_addr_list[h]);
					ipaddr = inet_ntoa(addr);
					if (!strcmp(ipaddr+strlen(ipaddr)-2,".0"))
					{	TraceLog(rpInfo->TraceName, LogCritical, NULL, "Skipping[%ld]=%s\n", (long) h, ipaddr);
						continue;
					}
				}
				Addrs[c++] = *((unsigned long *)hostnm->h_addr_list[h]);
			}

			while (c > 0)
			{	int i = (int) (c * (rand() / (RAND_MAX + 1.0)));
				pserver->sin_addr.s_addr = Addrs[i];
				Addrs[i] = Addrs[--c];	/* Move the last one down here */

				if (!rpInfo->Enabled) goto exit;
rpInfo->Line = __LINE__;
				if ((rpInfo->mysocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
				{	psock_errno("socket");
TraceLog(rpInfo->TraceName, TRUE, NULL, "socket(%s) Failed\n", Arg);
					goto exit;
				}
				SetPortStatus(rpInfo, "Connect", 0);
rpInfo->Line = __LINE__;
				TraceLog(rpInfo->TraceName, LogCritical, NULL, "Connecting(%s[%ld/%ld]) or %s:%ld\n",
							Host, (long) i, (long) c, inet_ntoa(pserver->sin_addr), (long) Port);
				if (connect(rpInfo->mysocket, (struct sockaddr *)pserver, sizeof(*pserver)) < 0)
				{	psock_errno("connect");
					soclose(rpInfo->mysocket);
					rpInfo->mysocket = 0;
					TraceLog(rpInfo->TraceName, LogCritical, NULL, "Connect(%s[%ld/%ld]) or %s:%ld Failed\n",
								Host, (long) i, (long) c, inet_ntoa(pserver->sin_addr), (long) Port);
TraceLog(rpInfo->TraceName, LogCritical, NULL, "connect(%s) Failed\n", Arg);
				} else
				{	char *ipaddr = inet_ntoa(pserver->sin_addr);
					TraceLog(rpInfo->TraceName, TRUE, NULL, "Connected(%s)[%ld/%ld]=>%s:%ld\n",
							Host,(long)i,(long)c,ipaddr?ipaddr:"NULL", (long) Port);

					if (ipaddr)
					{	size_t len = strlen(ipaddr)+1+33+1;
						char *text = (char*)malloc(len);
						StringCbPrintfA(text, len, "%s:%ld", ipaddr, (long) Port);
						SetPortSubStatus(rpInfo, text);
					}

				   /* Find out what port was really assigned and print it */
					{	struct sockaddr_in lclsocket = {0};
						int namelen = sizeof(lclsocket);
						if (getsockname(rpInfo->mysocket, (struct sockaddr *) &lclsocket, &namelen) < 0)
						{	TraceLog(rpInfo->TraceName, TRUE, NULL, "getsockname Failed!\n");
							psock_errno("CiTcpGetRelativeAddress:getsockname()");
						} else
						{	char *myaddr = inet_ntoa(lclsocket.sin_addr);
							TraceLog(rpInfo->TraceName, TRUE, NULL, "Connected(%s)[%ld/%ld] via %s:%ld\n",
									Host, (long)i, (long)c, myaddr?myaddr:"NULL",
									(long) ntohs(lclsocket.sin_port));
						}
					}

					break;
				}
			}
			free(Addrs);
			if (rpInfo->mysocket == 0) goto exit;	/* None left */
		}
	} else
	{
rpInfo->Line = __LINE__;
		if ((rpInfo->mysocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{	psock_errno("socket");
TraceLog(rpInfo->TraceName, TRUE, NULL, "socket(%s) Failed\n", Arg);
			goto exit;
		}
		SetPortStatus(rpInfo, "Connect", 0);
rpInfo->Line = __LINE__;
		if (connect(rpInfo->mysocket, (struct sockaddr *)pserver, sizeof(*pserver)) < 0)
		{	psock_errno("connect");
			soclose(rpInfo->mysocket);
			rpInfo->mysocket = 0;
TraceLog(rpInfo->TraceName, LogCritical, NULL, "connect(%s) Failed\n", Arg);
			goto exit;
		} else
		{	char *ipaddr = inet_ntoa(pserver->sin_addr);
			TraceLog(rpInfo->TraceName, TRUE, NULL, "Connected %s:%ld",
					ipaddr?ipaddr:"NULL", (long) Port);

			if (ipaddr)
			{	size_t len = strlen(ipaddr)+1+33+1;
				char *text = (char*)malloc(len);
				StringCbPrintfA(text, len, "%s:%ld", ipaddr, (long) Port);
				SetPortSubStatus(rpInfo, text);
			}

		   /* Find out what port was really assigned and print it */
			{	struct sockaddr_in lclsocket = {0};
				int namelen = sizeof(lclsocket);
				if (getsockname(rpInfo->mysocket, (struct sockaddr *) &lclsocket, &namelen) < 0)
				{	TraceLog(rpInfo->TraceName, TRUE, NULL, "getsockname Failed!\n");
					psock_errno("CiTcpGetRelativeAddress:getsockname()");
				} else
				{	char *myaddr = inet_ntoa(lclsocket.sin_addr);
					TraceLog(rpInfo->TraceName, TRUE, NULL, "Connected via %s:%ld\n",
							myaddr?myaddr:"NULL", (long) ntohs(lclsocket.sin_port));
				}
			}
		}
	}

	{	BOOL NoDelay = 1;	/* Disable Nagle algorithm for one-way sockets */
		if (setsockopt(rpInfo->mysocket, IPPROTO_TCP, TCP_NODELAY, (char*)&NoDelay, sizeof(NoDelay)))
		{	psock_errno("setsockopt");
			TraceLog(rpInfo->TraceName, TRUE, NULL, "setsockopt(%ld) Failed", (long) rpInfo->mysocket);
		}// else 	TraceLog(rpInfo->TraceName, TRUE, NULL, "setsockopt(%ld) Worked", (long) rpInfo->mysocket);
	}

	free(Arg);

	return PortSocketRead(rpInfo, LogCritical);

exit:
	free(Arg);
	return 0;
}

static BOOL PortCpSend(void *What, int Len, unsigned char *Buf)
{	COMM_PORT_S *pcp = (COMM_PORT_S *)What;
	BOOL Result = TRUE;

	if (Len && Buf)
	{	long len2=0;
		if (CpSendBytes(pcp, Buf, Len, &len2))
		{	TraceLog(NULL, TRUE, NULL, "Write %ld Bytes FAILED\n", (long) Len);
			Result = FALSE;
		} else if (Len != len2)
		{	TraceLog(NULL, TRUE, NULL, "Only Wrote %ld/%ld Bytes\n", (long) len2, (long) Len);
			Result = FALSE;
		} else PortDumpHex(NULL, "Wrote", Len, Buf);
				//TraceLog(NULL, TRUE, NULL, "Wrote(%.*s)\n", (long) Len, Buf);
	}
	return Result;
}

static DWORD WINAPI PortCpRead(RUNNING_PORT_INFO_S *rpInfo, BOOL LogCritical)
{	char *Port = SpaceCompress(-1,_strdup(rpInfo->pcInfo.Device));
	COMM_PORT_S cp = {0};
	int PortOpen = FALSE;

rpInfo->Line = __LINE__;

	TraceLog(rpInfo->TraceName, LogCritical, NULL, "CpReader Running on %s (%ld OpenCmds, %ld CloseCmds)\n",
			Port, (long) rpInfo->pcInfo.OpenCmds.Count, (long) rpInfo->pcInfo.CloseCmds.Count);

	rpInfo->pcp = &cp;
	rpInfo->SendRtn = CpSendBytes;
	rpInfo->ReadRtn = CpReadBytesWithTimeout;
	rpInfo->SendReadArg = rpInfo->pcp;

	while (rpInfo->Enabled
	&& (!rpInfo->pcInfo.QuietTime
	|| rpInfo->lastActive+rpInfo->pcInfo.QuietTime*1000 >= PortGetMsec()))
	{
		if (!PortOpen)
		{
rpInfo->Line = __LINE__;
TraceLog(rpInfo->TraceName, LogCritical, NULL, "Opening %s\n", Port);

			SetPortStatus(rpInfo, "Open", 0);

			if (CpOpen(Port, &cp))
			{	int i;
TraceLog(rpInfo->TraceName, LogCritical, NULL, "Error Opening %s LastError %ld\n", Port, GetLastError());
rpInfo->Line = __LINE__;
#ifdef DO_POPUP
MessageBox(NULL, TEXT("Failed To Open Port"), TEXT("RunPort"), MB_OK | MB_ICONERROR);
#endif
				if (GetLastError() != 1167)	/* BlueTooth timeout? */
					for (i=0; rpInfo->Enabled && i<5*2; i++)
						Sleep(500);
rpInfo->Line = __LINE__;
			} else
			{	unsigned long c;
rpInfo->Line = __LINE__;
TraceLog(rpInfo->TraceName, LogCritical, NULL, "Opened %s, Flushing %ld in TransmitQueue, Sending %ld OpenCmds\n", Port, (long) rpInfo->Transmit.Count, rpInfo->pcInfo.OpenCmds.Count);
				rpInfo->Transmit.Count = 0;
				PortOpen = TRUE;
				cp.Timeout = 300;

				if (rpInfo->Actions->NewConnection)
					rpInfo->Actions->NewConnection((void*)&cp, PortCpSend);

				SetPortStatus(rpInfo, "OpenCmd", 0);
				for (c=0; c<rpInfo->pcInfo.OpenCmds.Count; c++)
#ifdef USE_TIMED_STRINGS
					if (rpInfo->pcInfo.OpenCmds.Entries[c].string[0] != ';'
					&& rpInfo->pcInfo.OpenCmds.Entries[c].string[0] != '@')
					PortCommand(rpInfo, rpInfo->pcInfo.OpenCmds.Entries[c].string, NULL);
#else
					if (rpInfo->pcInfo.OpenCmds.Strings[c][0] != ';'
					&& rpInfo->pcInfo.OpenCmds.Strings[c][0] != '@')
					PortCommand(rpInfo, rpInfo->pcInfo.OpenCmds.Strings[c], NULL);
#endif
#ifdef FUTURE
				cp.Timeout = 300;
				PortCommand(rpInfo, &cp, "^C^C^C~");
				PortCommand(rpInfo, &cp, "^003^003^003~");
				PortCommand(rpInfo, &cp, "\r~");
				PortCommand(rpInfo, &cp, "\r~");
				PortCommand(rpInfo, &cp, "XFLOW OFF");
				PortCommand(rpInfo, &cp, "FULLDUP ON");
				PortCommand(rpInfo, &cp, "KISS ON");
				PortCommand(rpInfo, &cp, "RESTART");
#endif
#ifdef OLD_COMMANDS
			{
TraceLog("KISS", FALSE, NULL,"KISS:Opened %s, Flushing %ld in TransmitQueue\n", Port, (long) KISSTransmitCount);
				KISSTransmitCount = 0;
				PortOpen = TRUE;
				cp.Timeout = 300;
				KISSCommand(&cp, "\r");
				KISSCommand(&cp, "\r");
				KISSCommand(&cp, "XFLOW OFF\r");
				KISSCommand(&cp, "KISS ON\r");
				KISSCommand(&cp, "RESTART\r");
			}
			cp.Timeout = 300;
#endif
				SetPortStatus(rpInfo, "OK", 0);
			}
			cp.Timeout = 300;
		}
rpInfo->Line = __LINE__;
		if (PortOpen)
		{
rpInfo->Line = __LINE__;
			if (!PortReadCommPort(rpInfo, &cp))
			{	TraceLog(rpInfo->TraceName, LogCritical, NULL, "Error Reading Port, PortReadCommPort FAILED!\n");
				break;
			}
rpInfo->Line = __LINE__;
			if (rpInfo->Transmit.Count)	/* Something waiting? */
			{
rpInfo->Line = __LINE__;
				PortTransmitQueue(rpInfo, &cp);
rpInfo->Line = __LINE__;
			}
rpInfo->Line = __LINE__;
		}
	}
rpInfo->Line = __LINE__;

	TraceLog(rpInfo->TraceName, TRUE, NULL, "Terminating after %.0lf msec vs %ld Quiet\n",
			(double) PortGetMsec()-rpInfo->lastActive, (long) rpInfo->pcInfo.QuietTime*1000);

	if (PortOpen)
	{	size_t c;
rpInfo->Line = __LINE__;
TraceLog(rpInfo->TraceName, LogCritical, NULL,"Closing %s\n", Port);

		SetPortStatus(rpInfo, "CloseCmd", 0);
		for (c=0; c<rpInfo->pcInfo.CloseCmds.Count; c++)
#ifdef USE_TIMED_STRINGS
			if (rpInfo->pcInfo.CloseCmds.Entries[c].string[0] != ';'
			&& rpInfo->pcInfo.CloseCmds.Entries[c].string[0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.CloseCmds.Entries[c].string, NULL);
#else
			if (rpInfo->pcInfo.CloseCmds.Strings[c][0] != ';'
			&& rpInfo->pcInfo.CloseCmds.Strings[c][0] != '@')
				PortCommand(rpInfo, rpInfo->pcInfo.CloseCmds.Strings[c], NULL);
#endif
		SetPortStatus(rpInfo, "Close", 0);
		CpClose(&cp);

	}

rpInfo->Line = __LINE__;

	return 0;
}

#ifndef UNDER_CE
void WINAPI Create_Dump(PEXCEPTION_POINTERS pException, BOOL File_Flag, BOOL Show_Flag);
#endif

static DWORD WINAPI PortRun(LPVOID pvParam)
{	RUNNING_PORT_INFO_S *rpInfo = (RUNNING_PORT_INFO_S *)pvParam;
	char *Port = rpInfo->pcInfo.Device;
	COMM_PORT_S cp = {0};
	int PortOpen = FALSE;
	double RestartDelay = 5000;	/* Default, will increase if used */

#ifndef UNDER_CE
	__try
	{
#endif

	SetTraceThreadName(strdup(rpInfo->TraceName));	/* I know it leaks */
	SetDefaultTraceName(rpInfo->TraceName);

	{	SYSTEMTIME st;
		GetSystemTime(&st);
		srand(st.wMilliseconds*1000+st.wSecond);
	}

	while (rpInfo->Enabled)
	{
		SetPortStatus(rpInfo, "Init", 0);

		rpInfo->Running = TRUE;
		rpInfo->lastActive = PortGetMsec();
	
		if (rpInfo->Actions->Runner)
			rpInfo->Actions->Runner(rpInfo);
		else if (*Port == '@')
		{	if (!strncmp(Port,"@BT:",4))
				PortBTRead(rpInfo, (RestartDelay<60000));
			else PortTcpRead(rpInfo, (RestartDelay<60000));
		} else
		{	PortCpRead(rpInfo, (RestartDelay<60000));
		}

	rpInfo->Line = __LINE__;
		if (rpInfo->Enabled)
		{	double Elapsed = PortGetMsec() - rpInfo->lastActive;
			if (Elapsed < RestartDelay)
			{	SetPortStatus(rpInfo, "Delay", 0);
				Elapsed = RestartDelay - Elapsed;
				if (Elapsed > RestartDelay) Elapsed = RestartDelay;
				TraceLog(rpInfo->TraceName, (RestartDelay<60000), NULL, "Delaying Restart for %ld/%ld msec\n", (long) Elapsed, (long) RestartDelay);
	rpInfo->Line = __LINE__;
				while (Elapsed > 0 && rpInfo->Enabled)
				{	Sleep(100);
					Elapsed -= 100;
	rpInfo->Line = __LINE__;
				}
				RestartDelay *= 2;
				if (RestartDelay > 60000) RestartDelay = 60000;
			} else
			{	RestartDelay /= 2;
				if (RestartDelay < 5000) RestartDelay = 5000;
			}
			if (rpInfo->Enabled)
			{	SetPortStatus(rpInfo, "Restart", 0);
				TraceLog(rpInfo->TraceName, (RestartDelay<60000), NULL, "Restarting Reader...\n");
			}
		}
	}

	SetPortStatus(rpInfo, "Down", 0);

rpInfo->Line = __LINE__;
	TraceLog(rpInfo->TraceName, TRUE, NULL,"Reader Exiting\n");
	rpInfo->Running = FALSE;
#ifndef UNDER_CE
	}
	__except (Create_Dump(GetExceptionInformation(), 1, 1), EXCEPTION_EXECUTE_HANDLER)
	{	exit(0);
	}
#endif
	return 0;
}

BOOL PortTransmit(PORT_CONFIG_INFO_S *pcInfo, WPARAM wp, char *String)
{	RUNNING_PORT_INFO_S *rpInfo = GetPort(pcInfo, wp);

	if (!rpInfo) return FALSE;
	if (!rpInfo->Running) return FALSE;
	if (!rpInfo->Transmit.hmtx) return FALSE;
	if (!rpInfo->Actions->FormatTransmit) return FALSE;	/* Receive-Only interface */

	if (LockAndPurgeTransmitQueue(rpInfo, 1000))
	{	XMIT_QUEUE_ENTRY_S *Entry = (XMIT_QUEUE_ENTRY_S*)malloc(sizeof(*Entry)+strlen(String)+16);	/* Temporarily larger */
		int t = rpInfo->Transmit.Count++;
		if (t >= rpInfo->Transmit.Size)
		{	t = rpInfo->Transmit.Size++;
			rpInfo->Transmit.Entries = (XMIT_QUEUE_ENTRY_S **) realloc(rpInfo->Transmit.Entries, sizeof(*rpInfo->Transmit.Entries)*rpInfo->Transmit.Size);
		}
		rpInfo->Transmit.Entries[t] = Entry;
		Entry->msQueued = PortGetMsec();
		Entry->Data = ((char*)Entry)+sizeof(*Entry);
		strcpy(Entry->Data, String);

		ReleaseMutex(rpInfo->Transmit.hmtx);
		return TRUE;
	} else return FALSE;
}

BOOL PortStart(PORT_CONFIG_INFO_S *pcInfo, HWND hwnd, UINT msgStatus, UINT msgSubStatus, UINT msgReceived, UINT msgXmitCount, WPARAM wp)
{	RUNNING_PORT_INFO_S *rpInfo;

	if (!pcInfo->IsEnabled || !pcInfo->Device[0])
	{	pcInfo->IsEnabled = FALSE;
		return FALSE;
	}

	rpInfo = CreatePort(pcInfo, hwnd, msgStatus, msgSubStatus, msgReceived, msgXmitCount, wp);
	if (!rpInfo)
	{	pcInfo->IsEnabled = FALSE;
		return FALSE;
	}

	TraceLog(rpInfo->TraceName, TRUE, NULL, "Starting %s\n", rpInfo->TraceName);

	if (rpInfo->Actions->TweakConfig)
	{	if (!rpInfo->Actions->TweakConfig(hwnd, pcInfo))
		{	TraceLog(rpInfo->TraceName, TRUE, hwnd, "Configuration Issue, Port Not Started\n");
			DestroyPort(rpInfo, TRUE);
			return FALSE;
		}
		rpInfo->pcInfo = *pcInfo;
	}

	if (!rpInfo->Running)
	{	int i;
		rpInfo->Enabled = TRUE;
		SetPortStatus(rpInfo, "Start", 0);
		CloseHandle(CreateThread(NULL, 0, PortRun, rpInfo, 0, NULL));
		for (i=0; i<100; i++)
		{	if (rpInfo->Running)
				break;
			else Sleep(100);
		}
		if (!rpInfo->Running)
		{	TraceLog(rpInfo->TraceName, TRUE, hwnd, "Failed To Start (Line %ld)\n", (long) rpInfo->Line);
			pcInfo->IsEnabled = FALSE;
		}
	}
	return pcInfo->IsEnabled;
}

BOOL PortStop(PORT_CONFIG_INFO_S *pcInfo, WPARAM wp)
{	RUNNING_PORT_INFO_S *rpInfo = GetPort(pcInfo, wp);
	BOOL Result = TRUE;
	int i;

	if (rpInfo)
	{	rpInfo->Enabled = FALSE;

		TraceLog(rpInfo->TraceName, TRUE, NULL, "Stopping %s\n", rpInfo->TraceName);

		for (i=0; rpInfo->Running && i<100; i++)
		{	TraceLog(rpInfo->TraceName, TRUE, NULL, "Waiting at line %ld\n", (long) rpInfo->Line);
			Sleep(100);
		}

		if (rpInfo->Running)
			TraceLog(rpInfo->TraceName, TRUE, NULL, "Failed To Terminate (Line %ld)\n",
					(long) rpInfo->Line);
		Result = !rpInfo->Running;
		DestroyPort(rpInfo, !rpInfo->Running);
	}
	else if (*pcInfo->Name)
		TraceLog(pcInfo->Name, TRUE, NULL, "PortStop:rpInfo(%s)(%ld) Not Found\n", pcInfo->Name, (long) wp);
	return Result;
}

