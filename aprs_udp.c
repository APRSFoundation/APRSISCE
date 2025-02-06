#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "tcputil.h"

#include "aprs_udp.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !FALSE
#endif

extern CONFIG_INFO_S ActiveConfig;

static unsigned char *MyStrnChr(int len, unsigned char *src, unsigned char c)
{	int i;
	for (i=0; i<len; i++)
		if (src[i] == c)
			return &src[i];
	return NULL;
}

BOOL APRSUDPGotPacket(int len, unsigned char *buf)
{	if (len > 1 && buf[len-1] == '\n')	/* End of Packet? */
	{
		return TRUE;
	} else TraceLog(NULL, FALSE, NULL,"APRS-IS:[%ld] Got %ld Bytes ending in 0x%02lX\n", (long) len, buf[len-1]);
	return FALSE;
}

char *APRSUDPFormatReceive(int Len, unsigned char *Pkt, int *rLen)
{	char *Result;
	int OrgLen = Len;
	unsigned char *p = Pkt, *e;

	if (!Len)
	{	*rLen = 0;
		return NULL;
	}
	e = MyStrnChr(Len, Pkt, '\n');	/* Do we have the ending <LF>? */
	if (!e)
	{
//printf("Missing ending C0\n");
		*rLen = 0;	/* Nope, keep them all and wait for <LF> */
		return NULL;
	}
	Len = (e-p);
	while (Len && Pkt[Len-1] == '\r')
		Len--;
	if (!Len)
	{	*rLen = (e-Pkt)+1;	/* only \r and \n?  eat them all! */
		return NULL;
	}

	if (Pkt[0] == '[')	/* Smells like a javAPRSSrvr monitor port */
	{	char *e = MyStrnChr(Len, Pkt, ']');
		if (e++)	/* Skip over ] if found */
		{	Len -= (e-Pkt);	/* Actual packet length */
			if (Len<=0)
			{	*rLen = (e-Pkt)+1;	/* only []?  eat them all! */
				return NULL;
			}
			memmove(Pkt, e, Len);	/* Move the data down */
		}
	}

	TraceLog(NULL, FALSE, NULL, "APRS-IS:%.*s\n",(int)Len, Pkt);

	*rLen = (e-Pkt)+1;
	if (*Pkt == '#')	/* Skip Comments from the server */
	{	Result = NULL;
		if (!strncmp(Pkt,"# logresp", 9))
		{	char *Safe = (char*)malloc(*rLen+1);
			strcpy(Safe, Pkt);
			Safe[*rLen] = '\0';
			TraceLog(NULL, TRUE, NULL, "%.*s\n", *rLen, Pkt);

			if (ActiveConfig.Enables.Internet
			/* && strstr(Safe, "unverified") *//*&& !ActiveConfig.SuppressUnverified*/)
			{	char *Buffer = (char*)malloc(*rLen+128);
				SYSTEMTIME stUTCTime;
				int Len;
				GetSystemTime(&stUTCTime);

				if (strstr(Safe, ", adjunct")) *strstr(Safe, ", adjunct") = '\0';

				Len = sprintf(Buffer,"%04ld-%02ld-%02ldT%02ld:%02ld:%02ld %s (%s %s) %s",
									(long) stUTCTime.wYear, 
									(long) stUTCTime.wMonth, 
									(long) stUTCTime.wDay, 
									(long) stUTCTime.wHour, 
									(long) stUTCTime.wMinute, 
									(long) stUTCTime.wSecond,
									CALLSIGN,
									PROGNAME, VERSION, Safe);
				tcp_send_udp("aprsisce.dnsalias.net", 6369, Len+1, Buffer, 1);
				free(Buffer);
			}
			free(Safe);
		}
	} else
	{	Result = (char*)malloc(Len+1);
		memcpy(Result, Pkt, Len);
		Result[Len] = '\0';
	}
	return Result;
}

char *APRSUDPFormatTransmit(char *String, long *rLen, void *Where)
{	size_t len = strlen(String);
	if (len <= 2 || String[len-2] != '\r' || String[len-1] != '\n')
	{	char *New = (char*)malloc(len+3);
		strcpy(New, String);
		New[len++] = '\r';
		New[len++] = '\n';
		String = New;
	}
	*rLen = len;
	return String;
}

BOOL APRSUDPTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo)
{	pcInfo->RfBaud = 0;	/* Not really RF */
	pcInfo->NotRF = TRUE;
	pcInfo->RequiresInternet = TRUE;
	pcInfo->XmitEnabled = FALSE;
	pcInfo->RFtoISEnabled = FALSE;
	pcInfo->IStoRFEnabled = FALSE;
	pcInfo->BeaconingEnabled = FALSE;
	pcInfo->MessagesEnabled = FALSE;
	pcInfo->BulletinObjectEnabled = FALSE;
	pcInfo->TelemetryEnabled = FALSE;
	EmptySimpleStringList(&pcInfo->OpenCmds);	/* Not supported */
	EmptySimpleStringList(&pcInfo->CloseCmds);	/* Not supported */
	return TRUE;
}

char * cdecl FormatFilter(void);

BOOL APRSUDPNewConnection(void *What, BOOL (*Write)(void *What, int Len, unsigned char *Buf))
{	BOOL Result;
	char *Filter = FormatFilter();
	char *Logon = (char*)malloc(512+strlen(Filter));
	sprintf(Logon, "user %s pass %s vers %s %s filter %s\r\n",
			CALLSIGN, PASSWORD, PROGNAME, VERSION, Filter);
	free(Filter);
	{	char *p=strstr(Logon,"vers ");
		if (p) p = strchr(p+5,' ');	/* After APRSISxx */
		if (p) p = strchr(p+1,' ');	/* Inside VERSION */
		if (p) *p = '-';
	}
	Result = Write(What, strlen(Logon), Logon);
	free(Logon);
	return Result;
}

