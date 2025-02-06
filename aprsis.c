#ifdef _DEBUG
#include <crtdbg.h>
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "tcputil.h"
#include "tracelog.h"

#include "aprsis.h"
#include "port.h"

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

BOOL APRSISGotPacket(int len, unsigned char *buf)
{	if (len > 1 && buf[len-1] == '\n')	/* End of Packet? */
	{
		return TRUE;
	} else TraceLog(NULL, FALSE, NULL,"Got %ld Bytes ending in 0x%02lX\n", (long) len, buf[len-1]);
	return FALSE;
}

char *APRSISFormatReceive(int Len, unsigned char *Pkt, int *rLen)
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

	{	BOOL HasNULL = MyStrnChr(Len, Pkt, '\0') != NULL;
		if (HasNULL)
			PortDumpHex("Packets(NULL)", "APRS-IS:NULL-Truncated", (int)Len, Pkt);
		TraceLog(NULL, FALSE, NULL, "%.*s\n", (int)Len, Pkt);
	}

	*rLen = (e-Pkt)+1;
	if (*Pkt == '#')	/* Skip Comments from the server */
	{	Result = NULL;
		if (!strncmp(Pkt,"# logresp", 9))
		{	char *Safe = (char*)malloc(*rLen+1);
			strncpy(Safe, Pkt, *rLen);
			//strcpy(Safe, Pkt);
			Safe[*rLen] = '\0';
			TraceLog(NULL, TRUE, NULL, "%s%.*s\n",
					*rLen<(int)strlen(Pkt)?"TOO LONG:":"", *rLen, Pkt);

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
			if (strstr(Safe, "unverified") && !ActiveConfig.SuppressUnverified)
			{	QueueInternalMessage("Warning: APRS-IS Server Rejected Password (unverified)", FALSE);
				ActiveConfig.SuppressUnverified = TRUE;
			}
			free(Safe);
		}
	} else
	{	Result = (char*)malloc(Len+1);
		memcpy(Result, Pkt, Len);
		Result[Len] = '\0';

		{	char *p = strchr(Result,':');	/* Get to payload */
			if (p && Len-(p-Result)>2)	/* Only those with payloads and long enough to matter */
			{	char *a = strchr(p+2,'>');	/* Packet delimiter out there? */
				while (a)
				{	char *c = strchr(a+1,':');	/* And a new payload? */
					if (c && c>a+1)
					{	char *t;
						for (t=a+1; t<c; t++)
							if (!isalnum(*t&0xff)
							&& *t!=','
							&& *t!='-'
							&& *t!='*')
								break;
						if (t>=c)
						{	char *q = strstr(a+1,",q");
							if (q && q < c)
							{
#ifndef UNDER_CE
								if (IsTraceLogEnabled("Concat"))
								{	char *s;
									TraceLogThread("Concat", FALSE, "BAD: %s\n", Result);
									for (s=a-1; s>a-9; s--)
										if (!isalnum(s[-1]&0xff)
										&& s[-1] != ','
										&& s[-1] != '-'
										&& s[-1] != '*')
											break;
									TraceLogThread("Concat", FALSE, "\tMaybe: %s\n", s);
								}
#endif
								free(Result);
								Result = NULL;
								break;	/* Quit looking */
							}
#ifndef UNDER_CE
							else if (isdigit(a[-1]&0xff)
							|| (isalpha(a[-1]&0xff)&&isupper(a[-1]&0xff)))
								TraceLogThread("Concat", FALSE, "NOQ(%.*s) in %s\n", (int)(c-a)+1, a, Result);
#endif
						}
						//else TraceLogThread("Concat", FALSE, "NOT(%.*s) in %s\n", (int)(c-a)+1, a, Result);
					}
					a = strchr(a+1,'>');	/* Check the next > */
				}
			}
		}
	}
	return Result;
}

char *APRSISFormatTransmit(char *String, long *rLen, void *Where)
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

BOOL APRSISTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo)
{	pcInfo->RfBaud = 0;	/* Not really RF */
	pcInfo->NotRF = TRUE;
	pcInfo->RequiresInternet = TRUE;
	pcInfo->RequiresFilter = TRUE;
	return TRUE;
}

char * cdecl FormatFilter(void);

BOOL APRSISNewConnection(void *What, BOOL (*Write)(void *What, int Len, unsigned char *Buf))
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

BOOL CWOPTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo)
{	pcInfo->RfBaud = 0;	/* Not really RF */
	pcInfo->NotRF = TRUE;
	pcInfo->RequiresInternet = TRUE;
	pcInfo->RequiresFilter = TRUE;
	pcInfo->XmitEnabled = FALSE;
	pcInfo->RFtoISEnabled = FALSE;
	pcInfo->IStoRFEnabled = FALSE;
	pcInfo->BeaconingEnabled = FALSE;
	pcInfo->TelemetryEnabled = FALSE;
	return TRUE;
}

BOOL CWOPNewConnection(void *What, BOOL (*Write)(void *What, int Len, unsigned char *Buf))
{	return APRSISNewConnection(What, Write);
}

char *CWOPFormatTransmit(char *String, long *rLen, void *Where)
{	if (!_strnicmp(String,"#filter",7))
	{	TraceLogThread(NULL, TRUE, "CWOP:Transmitting %s\n", String);
		return APRSISFormatTransmit(String, rLen, Where);
	}
	TraceLogThread(NULL, TRUE, "CWOP:NOT Transmitting non-filter:%s\n", String);
	return NULL;
}

BOOL ServerTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo)
{	pcInfo->RfBaud = -1;	/* Not really RF, but IGate it! */
	pcInfo->NotRF = TRUE;
	return TRUE;
}

BOOL ServerGotPacket(int len, unsigned char *buf)
{	if (len > 1 && buf[len-1] == '\n')	/* End of Packet? */
	{	return TRUE;
	} else TraceLog(NULL, FALSE, NULL,"Server:[%ld] Got %ld Bytes ending in 0x%02lX\n", (long) len, buf[len-1]);
	return FALSE;
}

char *ServerFormatReceive(int Len, unsigned char *Pkt, int *rLen)
{	char *Result;
	int OrgLen = Len;
	unsigned char *p = Pkt, *e;

	if (!Len)
	{	*rLen = 0;
		return NULL;
	}
	if (*Pkt == '\n' || *Pkt == '\r')	/* Eat leading line breaks */
	{	*rLen = 1;						/* One character at a time */
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

{	BOOL HasNULL = MyStrnChr(Len, Pkt, '\0') != NULL;
	if (HasNULL)
		PortDumpHex("Packets(NULL)", "Server:NULL-Truncated", Len, Pkt);
	TraceLog(NULL, FALSE, NULL, "Server:%.*s\n",(int)Len, Pkt);
}

	*rLen = (e-Pkt)+1;
	Result = (char*)malloc(Len+1);
	memcpy(Result, Pkt, Len);
	Result[Len] = '\0';

	return Result;
}

