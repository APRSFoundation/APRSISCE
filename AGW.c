#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AGW.h"

#include "port.h"

#include "KISS.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !FALSE
#endif

typedef struct AGW_HEADER
{	char Port;
	char Unused0[3];
	char DataKind;
	char Unused1;
	char PID;
	char Unused2;
	unsigned char CallFrom[10];
	unsigned char CallTo[10];
	long DataLen;
	long Reserved;
} AGW_HEADER;

BOOL AGWNewConnection(void *What, BOOL (*Write)(void *What, int Len, unsigned char *Buf))
{	BOOL Result = TRUE;

	{	AGW_HEADER Head = {0};
		Head.DataKind = 'R';	/* Version info */
		Result = Write(What, sizeof(Head), (unsigned char *)&Head);
	}
	{	AGW_HEADER Head = {0};
		Head.DataKind = 'G';	/* Radio port number and description */
		Result = Write(What, sizeof(Head), (unsigned char *)&Head);
	}
	{	AGW_HEADER Head = {0};
		Head.DataKind = 'H';	/* MHeard on port 0 */
		Result = Write(What, sizeof(Head), (unsigned char *)&Head);
	}

#ifdef OLD_WAY
	{	AGW_HEADER Head = {0};
		Head.DataKind = 'm';	/* Monitor for UI packets */
		Result = Write(What, sizeof(Head), (unsigned char *)&Head);
	}
#endif

	{	AGW_HEADER Head = {0};
		Head.DataKind = 'k';	/* Monitor for Raw AX.25 packets */
		Result = Write(What, sizeof(Head), (unsigned char *)&Head);
	}
	return Result;
}

BOOL AGWGotPacket(int len, unsigned char *buf)
{	AGW_HEADER *Head;
	char Buffer[80];

	if (len < sizeof(*Head))
	{	StringCbPrintfA(Buffer, sizeof(Buffer), "AGW:Need Head, Have %ld / %ld Bytes\n", (long) len, (long) sizeof(*Head));
		PortDumpHex(NULL, Buffer, len, buf);
		return FALSE;
	}

	Head = (void *) buf;
	if (len < (int)(sizeof(*Head)+Head->DataLen))
	{	StringCbPrintfA(Buffer, sizeof(Buffer), "AGW:Need %ld Data, Have %ld / %ld Bytes\n", Head->DataLen, (long) len, sizeof(*Head)+Head->DataLen);
		PortDumpHex(NULL, Buffer, len, buf);
		return FALSE;
	}

	return TRUE;
}

char *AGWFormatReceive(int Len, unsigned char *Pkt, int *rLen)
{	AGW_HEADER *Head;
	char *Data;
	char *Result = NULL;

	if (!AGWGotPacket(Len, Pkt))
	{	*rLen = 0;
		return NULL;
	}

	Head = (void *) Pkt;
	Data = &Pkt[sizeof(*Head)];
	if (Head->DataKind == 'U' || Head->DataKind == 'T')	/* Monitor or Transmitted data */
	{	char *Fm = strstr(Data,":Fm ");
		char *To = strstr(Data," To ");
		char *Via = strstr(Data," Via ");
		char *End = strstr(Data," <");

		if (Fm && To && End)
		{	char *UI = strstr(Data," <UI");
			char *CR = strstr(Data,"]\r");
			char *CRCR = strstr(Data,"\r\r");
			if (UI && CR && CRCR)
			{	char *Ax25 = malloc(Head->DataLen);
				CR += 2;
				if (Via)
				{	Via += 5;
					sprintf(Ax25, "%.*s>%.*s,%.*s:%.*s",
						sizeof(Head->CallFrom), Head->CallFrom,
						sizeof(Head->CallTo), Head->CallTo,
						(int) (End-Via), Via,
						(int) (CRCR-CR), CR);
				} else 	sprintf(Ax25, "%.*s>%.*s:%.*s",
						sizeof(Head->CallFrom), Head->CallFrom,
						sizeof(Head->CallTo), Head->CallTo,
						(int) (CRCR-CR), CR);
				if (Head->DataKind == 'U')
				{	TraceLog(NULL, FALSE, NULL,"AGW:Posting %s\n", Ax25);
					Result = Ax25;
				} else
				{	TraceLog(NULL, FALSE, NULL,"AGW:Transmitted %s\n", Ax25);
					free(Ax25);
				}
			} else
			{	TraceLog(NULL, TRUE, NULL,"AGW:Non-UI %ld/%ld Bytes of AGW(%.*s)n",
						(long) Head->DataLen, (long) Len, (int) Head->DataLen, Data);
			}
		} else
		{	TraceLog(NULL, TRUE, NULL,"AGW:Missing From/To/End in %ld/%ld Bytes of AGW(%.*s)n",
						(long) Head->DataLen, (long) Len, (int) Head->DataLen, Data);
		}
#ifdef FUTURE
2.DATA That We transmit
The LOWORD Port field is the port which heard the frame
The LOWORD DataKind field ='T'; The ASCII value of letter T
CallFrom =The call from which we heard the frame
CallTo=is the call to whom we send the frame
DataLen is the length of the data that follow
USER is undefined.
the whole frame with the header is
[port][DataKind][CallFrom][CallTo ][DataLen][USER][Data         ]
 4bytes  4bytes     10bytes  10bytes   4bytes      4bytes  DataLen Bytes
#endif
	} else if (Head->DataKind == 'K')
	{
		if (*Data == 0)
		{	Result = KISSDeFormatAX25(Head->DataLen, Data);
			if (Result)
			{	TraceLog(NULL, TRUE, NULL, "AGW:AX.25-rPort[%ld] %s\n",
						(long) (Head->Port&0xff), Result);
			} else PortDumpHex(NULL, "AGW:AX.25-recv", Len, Pkt);
			// PortDumpHex(NULL, "Buffer(AX.25)", Len, Pkt);
		} else
		{	char Buffer[80];
			StringCbPrintfA(Buffer, sizeof(Buffer), "AGW:DataKind(%c)(0x%lX) Non-AX.25[0x%lX] in %ld/%ld Bytes\n",
							isprint(Head->DataKind&0xff)?Head->DataKind:'?',
							(long) (Head->DataKind&0xff),
							(long)(*Data&0xff), (long) Head->DataLen, (long) Len);
			//PortDumpHex(NULL, Buffer, (int) Head->DataLen, Data);
			PortDumpHex(NULL, Buffer, (int) Len, Pkt);
			PortDumpHex("Non-AX.25", Buffer, Len, Pkt);
		}
	} else if (Head->DataKind == 'R' && Head->DataLen == 8)	/* Version data */
	{	unsigned long *N = (unsigned long *)Data;
		TraceLog(NULL, TRUE, NULL,"AGW:Version %lu.%lu\n", N[0], N[1]);
	} else if (Head->DataKind == 'G')	/* Radio Port Info */
	{	PortDumpHex(NULL, "AGW:Ports", Head->DataLen, Data);
	} else
	{	char Buffer[80];
		StringCbPrintfA(Buffer, sizeof(Buffer), "AGW:DataKind(%c)(0x%lX) in %ld/%ld Bytes\n",
						isprint(Head->DataKind&0xff)?Head->DataKind:'?',
						(long) (Head->DataKind&0xff),
						(long) Head->DataLen, (long) Len);
		PortDumpHex(NULL, Buffer, (int) Len, Pkt);
	}
	*rLen = sizeof(*Head)+Head->DataLen;

	return Result;
}

static unsigned char *AGWCall(unsigned char *Call, unsigned char *Dest)
{	unsigned char *OrgCall = Call, *OrgDest = Dest;
	int i=0;

	/* Call-nn>Call-nn,Call-nn*,Path-nn: */
	while (*Call /*&& *Call != '*'*/ && *Call != '>' && *Call != ':' && *Call != ',')
	{	if (i<9 || (i==9 && *Call=='*'))	/* No more than 9 characters of callsign (10 with *) */
		{	*Dest++ = *Call;
			i++;
		} else
		{	TraceLog(NULL, TRUE, NULL, "AGW:Callsign(%s) Too Long!\n", OrgCall);
			return NULL;
		}
		Call++;	/* Skip character! */
	}
	while (i<9)	/* space-fill the callsign */
	{	*Dest++ = 0x20;
		i++;
	}
	*Dest++ = 0;	/* Null terminate the call */
	return *Call?Call+1:Call;
}

char *AGWFormatTransmit(char *String, long *rLen, void *Where)
{	AGW_HEADER *Header;
	unsigned char *Result;
#ifdef SEND_AGW_UI
	unsigned char *s, *o, *c;
	long len;

	Result = (char*)malloc(sizeof(*Header)+2+10*8+2+strlen(String));

	Header = (AGW_HEADER*) Result;
	memset(Header, 0, sizeof(*Header));
	Header->DataKind = 'V';

	s = String;
	o = (unsigned char *)&Header[1];	/* Point to data after header */

	s = AGWCall(s,&Header->CallFrom[0]);	/* Put from Call in first */
	if (!s) {*rLen = 0; free(Result); return NULL; }
	s = AGWCall(s,&Header->CallTo[0]);
	if (!s) {*rLen = 0; free(Result); return NULL; }
	c = o++;	/* c->digi count, o->digi calls */
	*c = 0;		/* Zero digis so far */
	while (s[-1] == ',')
	{	s = AGWCall(s,o);
		if (!s) {*rLen = 0; free(Result); return NULL; }
		*c += 1;	/* Count the digi */
		o += 10;	/* Skip the digi */
	}

TraceLog(NULL, FALSE, NULL, "AGW:Payload(%s)\n", s);

	if ((len=strlen(s)) > 0)
	{	memcpy(o,s,len);	/* And copy in the payload */
		o += len;
	}

	*o = 0;	/* Null terminate (probably not required) */

	len = (o-Result);
	Header->DataLen = len-sizeof(*Header);

	PortDumpHex(NULL, "AGW:Sending", len, Result);

	*rLen = len;
#else
	long AX25Len;
	char *AX25 = KISSFormatAX25(String, &AX25Len);	// includes leading null
	if (!AX25) return NULL;

	Result = (char*)malloc(sizeof(*Header)+AX25Len+1);

	Header = (AGW_HEADER*) Result;

	memset(Header, 0, sizeof(*Header)+1);
	Header->DataKind = 'K';

	Header->DataLen = AX25Len;
	memcpy((unsigned char*)&Header[1], AX25, AX25Len);

	PortDumpHex(NULL, "AGW:AX.25-xmit", sizeof(*Header)+AX25Len, Result);

	*rLen = sizeof(*Header)+AX25Len;

#endif

	return Result;
#ifdef FUTURE
Transmit DATA UNPROTO Via
.....................
Port field is the port where we want the data to tx
DataKind field ='V'; MAKELONG('V',0)The ASCII value of letter V
CallFrom is our call
CallTo is the call of the other station
DataLen is the length of the data that follow   in special format
USER is undefined
the whole frame with the header is
[ HEADER                                   ]
[port][DataKind][CallFrom][CallTo ][DataLen][USER][Data         ]
 4bytes  4bytes     10bytes  10bytes   4bytes      4bytes  DataLen Bytes
The data field contains first the via calls and then follow the actual data
the format is as follow
[   DATA  field                                 ]
[How many digis] [1stdigicall][2digicall].......
    byte            10bytes     10bytes
Be carefull each digi call must be 10 char long Null terminated.If sorter then next digi must
exactly after 10 bytes as follow
e.g char str[100];
str[0]=HowManyDigis;
str+1=1digi;//null terminated
str+1+10=2digi;//null terminated
str+1+20=3digi//null terminated
..............
..............
#endif
}

