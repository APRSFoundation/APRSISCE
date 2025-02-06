#ifdef KISS_INFO
Dec 24 17:50:02 (236) 200:
<C0 01 14 C0 
C0 02 FF C0 
C0 03 00 C0 
C0 05 00 C0 
C0 04 0A C0 
C0 00 82 A0 94 92>df`<96 94>h<8A A4 94>d<AE 92 88 8A>d@c<03 F0>!2759.80NS08039.54W#W1 PHG2120 Palm Bay FL<C0 
C0 00 82 A0 94 92>df`<96 94>h<8A A4 94>d<AE 92 88 8A>d@c<03 F0><IGATE,MSG_CNT=0,LOC_CNT=0<C0 
C0 00 82 A0 94 92>df`<96 94>h<8A A4 94>d<AE 92 88 8A>d@c<03 F0>>http://APRSISCE.homeside.to:12501<C0>
#endif

#ifdef KENWOOD_KISS_INFO
http://www.mentby.com/Group/linux-hams-discuss/linux-ax25-kiss-driver-for-kenwood-d700.html
The 710 ?bug? is not a violation of the KISS protocol?it is the result
of trying to use the same serial port for both a TNC and a Front panel
control. The problem is when in KISS mode (which sends binary packets
?encapsulated in the KISS protocol?  it is possible to have the
character sequence ?TC 0<Cr>?  as binary or  character data within the
KISS encapsulation. This sequence will be interpreted by the serial
port controller in the Control head of the D710 as ?escape to control
mode? and which will switch the serial interface from TNC mode to
front panel control mode.  This can have disastrous affects when
trying to transfer binary files using KISS.

One solution would be to entice Kenwood to modify the firmware in the
control head to having entered the KISS protocol to ignore all mode
control commands including ?TC 0<Cr>?. This would require using the
standardized escape KISS sequence {0xC0, 0xFF, 0xC0} prior to any
control panel commands and then re enabling KISS to continue TNC
operation?This gets tricky and is very likely to impact existing
programs that now work.

The solution we used and which appears to work well and is solid is to
escape the ?C? character.  So when any data is sent to the TNC in KISS
mode if there is an ASCII ?C? it is replaced by the sequence <0xDB>?C?
which essentially escapes the ?TC 0<Cr>? sequence. The KISS protocol
(in the D710?s KISS TNC implementation) removes the <0xDB> which is
the frame escape so no modification of the transmitted data stream is
actually made. Thanks to Peter Woods for figuring this out.
#endif


#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port.h"
#include "text.h"

#include "kiss.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !FALSE
#endif

static char *TranslateCall(unsigned char *Call, char *Dest, int FlagUsed)
{	int i;

	for (i=0; i<6; i++)
		if (Call[i] != 0x20<<1)
			*Dest++ = (Call[i]>>1)&0x7F;
	if (Call[6]>>1 & 0xF) Dest += sprintf(Dest, "-%ld", Call[6]>>1 & 0xF);
	if (FlagUsed && Call[6] & 0x80) *Dest++ = '*';
	return Dest;
}

unsigned char *KISSCall(unsigned char *Call, unsigned char *Dest)
{	unsigned char *OrgCall = Call, *OrgDest = Dest;
	int i=0;

	/* Call-nn>Call-nn,Path-nn: */
	while (*Call && *Call != '*' && *Call != '>' && *Call != ':' && *Call != ',' && *Call != '-')
	{	if (i<6)	/* No more than 6 characters of callsign */
		{	*Dest++ = *Call<<1;
			i++;
		} else
		{	TraceLog(NULL, TRUE, NULL, "KISS:Callsign(%s) Too Long!\n", OrgCall);
			return NULL;
		}
		Call++;	/* Skip character! */
	}
	while (i<6)	/* space-fill the callsign */
	{	*Dest++ = 0x20<<1;
		i++;
	}
	if (*Call == '-')	/* we have an SSID */
	{	int SSID = strtol(++Call, &Call, 10);
		if (*Call && *Call != '*' && *Call != '>' && *Call != ':' && *Call != ',')
		{	TraceLog(NULL, TRUE, NULL, "KISS:Non-Numeric SSID(%s)\n", OrgCall);
			return NULL;
		}
		*Dest = SSID<<1;
	} else *Dest = 0;	/* No - is SSID==0 */
	if (*Call == '*')
	{	*Dest |= 0x80;	/* Set the Used bit */
		Call++;			/* Skip the * */
	}
	*Dest++ |= 0x60;	/* Set the reserved bits */
//printf("Formatted %ld bytes from %ld (%.*s)\n", (long) (Dest-OrgDest),  (long) (Call-OrgCall), (int) (Call-OrgCall), OrgCall);
	return *Call?Call+1:Call;
}

static unsigned char *KissStrnChr(int len, unsigned char *src, unsigned char c)
{	int i;
	for (i=0; i<len; i++)
		if (src[i] == c)
			return &src[i];
	return NULL;
}

BOOL KISSGotPacket(int len, unsigned char *buf)
{
	if (len > 1 && buf[len-1] == 0xC0)	/* End of Kiss Packet? */
	{
//		printf("[%ld] Got %ld Bytes ending in 0xC0\n", (long) i, (long) cBytesRead);
		return TRUE;
	}
	// else TraceLog(NULL, FALSE, NULL, "KISS:Got %ld Bytes ending in 0x%02lX\n", (long) len, buf[len-1]);
	return FALSE;
}

char *KISSDeFormatAX25(int Len, unsigned char *Pkt)
{	char *Result = NULL;
	int OrgLen = Len;
	unsigned char *p = Pkt;

	if (!Len) return NULL;

	if (*p & 0x0F)	/* Need a 0 at the packet start */
	{	PortDumpHex(NULL, "KISS:Missing 0", OrgLen, Pkt);
		return NULL;
	}
	
	if (*p & 0x80)	/* SMACK packet? */
	{	PortDumpHex(NULL, "KISS:Ignoring SMACK Checksum", Len, Pkt);
		Len -= 2;	/* And don't process them */
	}

	if (*p & 0x70)	/* Multi-Port KISS? */
	{	char Buffer[80];
		sprintf(Buffer,"KISS:Processing MultiPort[%ld] as 0", (*p&0x70)>>4);
		PortDumpHex(NULL, Buffer, Len, Pkt);
	}

	if (Len < 18)
	{	PortDumpHex(NULL, "KISS:Not long enough (Need 18)", OrgLen, Pkt);
		return NULL;
	}

	if (Len > 400)
	{	PortDumpHex(NULL, "KISS:Too LONG! (Max 400)", OrgLen, Pkt);
		return NULL;
	}

	{	char Packet[512];
		char *Out = Packet;

		p += 1; Len -= 1;
		Out = TranslateCall(p+7,Out,FALSE);
		*Out++ = '>';
		Out = TranslateCall(p,Out,FALSE);
		p += 14; Len -= 14;	/* Skip the header */

		while (Len>7 && (p[-1]&0x01)==0)	/* While there's more packet and not the last call */
		{	*Out++ = ',';	/* Add the path comma */
			Out = TranslateCall(p,Out,TRUE);
			p += 7; Len -= 7;	/* Skip the call */
		}
		if (Len > 2 && p[0] == 0x03 && p[1] == 0xF0)
		{	*Out++ = ':';
			p += 2; Len -= 2;

/*	Get rid of leading \r\n in the payload */
			while (Len && (*p=='\r' || *p=='\n'))
			{	p++; Len--;
			}
/*	Copy the payload if we have any */
			if (Len)
			{	memcpy(Out, p, Len);
				Out += Len;
			}
/*	We don't like trailing \r\n either */
			while (Out>Packet && (!Out[-1] || Out[-1]=='\r' || Out[-1]=='\n'))
				*--Out = '\0';
			*Out = '\0';	/* Null terminate the string */

{	BOOL HasNULL = KissStrnChr(Out-Packet, Packet, '\0') != NULL;
	if (HasNULL)
		PortDumpHex("Packets(NULL)", "DeAX25:NULL-Truncated", Out-Packet, Packet);
}
			{	int len = (int)(Out-Packet);
				Result = malloc(len+1);
				memcpy(Result, Packet, len);
				Result[len] = '\0';
			}

		}
		else
		{	PortDumpHex(NULL, "KISS:Missing 03 F0", OrgLen, Pkt);
		}
	}
	return Result;
}

#ifdef BUSTED_C0s
char *KISSFormatReceive(int Len, unsigned char *Pkt, int *rLen)
{	char *Result = NULL;
	int OrgLen = Len;
	unsigned char *p = Pkt, *e;

	if (!Len) {	*rLen = 0; return NULL; }
	if (*p != 0xC0)
	{	p = KissStrnChr(Len, Pkt, 0xC0);
		if (!p)
		{	PortDumpHex(NULL, "KISS:Missing Leading C0", OrgLen, Pkt);
			*rLen = Len;	/* No 0xC0 */
			return NULL;
		}
		Len -= (p - Pkt);	/* What's left */
	}
	if (Len < 1)
	{
//printf("Can't check for 0, only %ld bytes\n", (long) Len);
		*rLen = (p-Pkt);	/* Eat the bytes up to the C0 for next time */
		return NULL;
	}
	if (p[1] == 0xC0)
	{	*rLen = 1;	/* Eat the leading (residual) C0 */
		return NULL;	/* But no packet */
	}

	if ((p[1]&0x0F) != 0)	/* Need a command 0 following the 0xC0 */
	{	p = KissStrnChr(Len-1, p+1, 0xC0);
		if (!p)
		{	PortDumpHex(NULL, "KISS:Missing 0, no C0", OrgLen, Pkt);
			*rLen = OrgLen;	/* Eat the whole packet if no other C0 */
			return NULL;
		} else
		{	PortDumpHex(NULL, "KISS:Missing 0, found C0", OrgLen, Pkt);
			*rLen = (p-Pkt);	/* Eat the bytes up to the C0 for next time */
			return NULL;
		}
	}
	e = KissStrnChr(Len-1, p+1, 0xC0);	/* Do we have the ending C0? */
	if (!e)
	{
//printf("Missing ending C0\n");
		*rLen = (p-Pkt);	/* Nope, eat the skipped leading chars */
		return NULL;
	}
	Len = (e-p)+1;

	if (p[1]&0x80)	/* SMACK packet? */
	{	PortDumpHex(NULL, "KISS:Ignoring SMACK Checksum", OrgLen, Pkt);
		e[-1] = e[-2] = 0xC0;	/* Stomp checksum with C0s */
		Len -= 2;	/* And don't process them */
	}

	if (p[1]&0x70)	/* Multi-Port KISS? */
	{	char Buffer[80];
		sprintf(Buffer,"KISS:Processing MultiPort[%ld] as 0", (p[1]&0x70)>>4);
		PortDumpHex(NULL, Buffer, OrgLen, Pkt);
	}
	if (Len>=7 && Pkt[2]=='$' 	/* $....*cc (Optionally <CR> or <CR><LF>) */
	&& (Pkt[Len-4]=='*' || Pkt[Len-5]=='*' || Pkt[Len-6] == '*'))
	{	TraceLog("NMEA", FALSE, NULL, "Parsing KISS[%ld] NMEA(%.*s)\n", (long) (p[1]&0x70)>>4, (int)Len-3, Pkt+2);
		NMEAFormatReceive(Len-3, Pkt+2, rLen);
		*rLen += 3;	/* Account for C0 and Cmd/Port and C0*/
		return NULL;	/* got nothing from this one as far as APRS */
	}
	if (Len < 18)
	{	PortDumpHex(NULL, "KISS:Not long enough (Need 18)", OrgLen, Pkt);
		*rLen = Len;	/* Eat the packet, not long enough */
		return NULL;
	}
	if (Len > 400)
	{	PortDumpHex(NULL, "KISS:Too LONG! (Max 400)", OrgLen, Pkt);
		*rLen = OrgLen;	/* Eat the whole packet in this case */
		return NULL;
	}

	{	char Packet[512];
		char *Out = Packet;

		p += 2; Len -= 2;
		Out = TranslateCall(p+7,Out,FALSE);
		*Out++ = '>';
		Out = TranslateCall(p,Out,FALSE);
		p += 14; Len -= 14;	/* Skip the header */

		while (Len>7 && (p[-1]&0x01)==0)	/* While there's more packet and not the last call */
		{	*Out++ = ',';	/* Add the path comma */
			Out = TranslateCall(p,Out,TRUE);
			p += 7; Len -= 7;	/* Skip the call */
		}
		if (Len > 2 && p[0] == 0x03 && p[1] == 0xF0)
		{	*Out++ = ':';
			p += 2; Len -= 2;

/*	Get rid of leading \r\n in the payload */
			while (Len && (*p=='\r' || *p=='\n'))
			{	p++; Len--;
			}
/*	Copy the payload if we have any */
			if (Len > 1)
			{	memcpy(Out, p, Len-1);
				Out += Len-1;
			}
/*	We don't like trailing \r\n either */
			while (Out>Packet && (!Out[-1] || Out[-1]=='\r' || Out[-1]=='\n'))
				*--Out = '\0';
			*Out = '\0';	/* Null terminate the string */

{	BOOL HasNULL = KissStrnChr(Out-Packet, Packet, '\0') != NULL;
	if (HasNULL)
		PortDumpHex("Packets(NULL)", "KISS:NULL-Truncated", (int)(Out-Packet), Packet);
	TraceLog(NULL, FALSE, NULL, "KISS:%.*s  (%ld)\n",(int)(Out-Packet), Packet, (long) Out[-2]);
}
			{	int len = (int)(Out-Packet);
				Result = malloc(len+1);
				memcpy(Result, Packet, len);
				Result[len] = '\0';
			}
		} else
		{	PortDumpHex(NULL, "KISS:Missing 03 F0", OrgLen, Pkt);
		}
	}
	*rLen = (e-Pkt)+1;	/* Eat the whole packet */
	return Result;
}
#else
char *KISSFormatReceive(int Len, unsigned char *Pkt, int *rLen)
{	char *Result = NULL;
	int OrgLen = Len;
	unsigned char *p = Pkt, *e;

	if (!Len) {	*rLen = 0; return NULL; }
	if (Len < 1)
	{
//printf("Can't check for 0, only %ld bytes\n", (long) Len);
		*rLen = (p-Pkt);	/* Eat the bytes up to the C0 for next time */
		return NULL;
	}
	if (p[0] == 0xC0)
	{	*rLen = 1;	/* Eat the leading (residual) C0 */
		return NULL;	/* But no packet */
	}

	if ((p[0]&0x0F) != 0)	/* Need a command 0 at start of packet */
	{	p = KissStrnChr(Len-1, p+1, 0xC0);
		if (!p)
		{	PortDumpHex(NULL, "KISS:Missing 0, no C0", OrgLen, Pkt);
			*rLen = OrgLen;	/* Eat the whole packet if no other C0 */
			return NULL;
		} else
		{	PortDumpHex(NULL, "KISS:Missing 0, found C0", OrgLen, Pkt);
			*rLen = (p-Pkt);	/* Eat the bytes up to the C0 for next time */
			return NULL;
		}
	}
	e = KissStrnChr(Len-1, p+1, 0xC0);	/* Do we have the ending C0? */
	if (!e)
	{
//printf("Missing ending C0\n");
		*rLen = (p-Pkt);	/* Nope, eat the skipped leading chars */
		return NULL;
	}
	Len = (e-p)+1;

	if (p[0]&0x80)	/* SMACK packet? */
	{	PortDumpHex(NULL, "KISS:Ignoring SMACK Checksum", OrgLen, Pkt);
		e[-1] = e[-2] = 0xC0;	/* Stomp checksum with C0s */
		Len -= 2;	/* And don't process them */
	}

	if (p[0]&0x70)	/* Multi-Port KISS? */
	{	char Buffer[80];
		sprintf(Buffer,"KISS:Processing MultiPort[%ld] as 0", (p[0]&0x70)>>4);
		PortDumpHex(NULL, Buffer, OrgLen, Pkt);
	}
	if (Len>=7 && Pkt[1]=='$' 	/* $....*cc (Optionally <CR> or <CR><LF>) */
	&& (Pkt[Len-4]=='*' || Pkt[Len-5]=='*' || Pkt[Len-6] == '*'))
	{	TraceLog("NMEA", FALSE, NULL, "Parsing KISS[%ld] NMEA(%.*s)\n", (long) (p[0]&0x70)>>4, (int)Len-2, Pkt+1);
		NMEAFormatReceive(Len-2, Pkt+1, rLen);
		*rLen += 2;	/* Account for Cmd/Port and C0*/
		return NULL;	/* got nothing from this one as far as APRS */
	}
	if (Len < 18)
	{	PortDumpHex(NULL, "KISS:Not long enough (Need 18)", OrgLen, Pkt);
		*rLen = Len;	/* Eat the packet, not long enough */
		return NULL;
	}
	if (Len > 400)
	{	PortDumpHex(NULL, "KISS:Too LONG! (Max 400)", OrgLen, Pkt);
		*rLen = OrgLen;	/* Eat the whole packet in this case */
		return NULL;
	}

	{	char Packet[512];
		char *Out = Packet;

		p += 1; Len -= 1;
		Out = TranslateCall(p+7,Out,FALSE);
		*Out++ = '>';
		Out = TranslateCall(p,Out,FALSE);
		p += 14; Len -= 14;	/* Skip the header */

		while (Len>7 && (p[-1]&0x01)==0)	/* While there's more packet and not the last call */
		{	*Out++ = ',';	/* Add the path comma */
			Out = TranslateCall(p,Out,TRUE);
			p += 7; Len -= 7;	/* Skip the call */
		}
		if (Len > 2 && p[0] == 0x03 && p[1] == 0xF0)
		{	*Out++ = ':';
			p += 2; Len -= 2;

/*	Get rid of leading \r\n in the payload */
			while (Len && (*p=='\r' || *p=='\n'))
			{	p++; Len--;
			}
/*	Copy the payload if we have any */
			if (Len > 1)
			{	memcpy(Out, p, Len-1);
				Out += Len-1;
			}
/*	We don't like trailing \r\n either */
			while (Out>Packet && (!Out[-1] || Out[-1]=='\r' || Out[-1]=='\n'))
				*--Out = '\0';
			*Out = '\0';	/* Null terminate the string */

{	BOOL HasNULL = KissStrnChr(Out-Packet, Packet, '\0') != NULL;
	if (HasNULL)
		PortDumpHex("Packets(NULL)", "KISS:NULL-Truncated", (int)(Out-Packet), Packet);
	TraceLog(NULL, FALSE, NULL, "KISS:%.*s  (%ld)\n",(int)(Out-Packet), Packet, (long) Out[-2]);
}
			{	int len = (int)(Out-Packet);
				Result = malloc(len+1);
				memcpy(Result, Packet, len);
				Result[len] = '\0';
			}
		} else
		{	PortDumpHex(NULL, "KISS:Missing 03 F0", OrgLen, Pkt);
		}
	}
	*rLen = (e-Pkt)+1;	/* Eat the whole packet */
	return Result;
}
#endif

char *KISSFormatAX25(char *String, long *rLen)
{	unsigned char *s, *o, *Kissified = (char*)malloc(2+10*6+2+strlen(String));
	long len;

	s = String;
	o = Kissified;

	*o++ = 0x00;	/* Something about channel 0 data? */

	s = KISSCall(s,&o[7]);	/* Put from Call in first */
	if (!s) {*rLen = 0; free(Kissified); return NULL; }
	s = KISSCall(s,&o[0]);
	if (!s) {*rLen = 0; free(Kissified); return NULL; }
	o[6] |= 0x80;	/* Set the H bit on Dest, not Source (1 0 = Command) */
	o += 14;	/* Skip over the two required calls */
	while (s[-1] == ',')
	{	s = KISSCall(s,o);
		if (!s) {*rLen = 0; free(Kissified); return NULL; }
		o += 7;
	}
	o[-1] |= 0x01;	/* Set the last address flag */

	*o++ = 0x03;	/* UI packet */
	*o++ = 0xF0;	/* No Level 3 Protocol */

	if ((len=strlen(s)) > 0)
	{	memcpy(o,s,len);	/* And copy in the payload */
		o += len;
	}

	len = (o-Kissified);

	*rLen = len;
	return Kissified;
}

char *KISSFormatTransmit(char *String, long *rLen, void *Where)
{	unsigned char *s, *o, *Kissified = (char*)malloc(2+10*6+2+strlen(String));
	long len;

	s = String;
	o = Kissified;

	*o++ = 0xC0;	/* KISS packet */
	*o++ = 0x00;	/* Something about channel 0 data? */

	s = KISSCall(s,&o[7]);	/* Put from Call in first */
	if (!s) {*rLen = 0; free(Kissified); return NULL; }
	s = KISSCall(s,&o[0]);
	if (!s) {*rLen = 0; free(Kissified); return NULL; }
	o[6] |= 0x80;	/* Set the H bit on Dest, not Source (1 0 = Command) */
	o += 14;	/* Skip over the two required calls */
	while (s[-1] == ',')
	{	s = KISSCall(s,o);
		if (!s) {*rLen = 0; free(Kissified); return NULL; }
		o += 7;
	}
	o[-1] |= 0x01;	/* Set the last address flag */

	*o++ = 0x03;	/* UI packet */
	*o++ = 0xF0;	/* No Level 3 Protocol */

//TraceLog(NULL, FALSE, NULL, "KISS:Payload(%s)\n", s);
	if ((len=strlen(s)) > 0)
	{	memcpy(o,s,len);	/* And copy in the payload */
		o += len;
	}

	*o++ = 0xC0;	/* Trailing KISS delimiter */

	len = (o-Kissified);

//	PortDumpHex(NULL, "KISS:Sending", len, Kissified);

	*rLen = len;
	return Kissified;
}

BOOL KISSTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo)
{
	if (!pcInfo->OpenCmds.Count)
	{	STRING_LIST_S *pList = &pcInfo->OpenCmds;
#ifdef USE_TIMED_STRINGS
		AddSimpleStringEntry(pList, "^M~");
		AddSimpleStringEntry(pList, "^M~");
		AddSimpleStringEntry(pList, "XFLOW OFF");
		AddSimpleStringEntry(pList, "FULLDUP OFF");	/* Was on, but that disables carrier detect */
		AddSimpleStringEntry(pList, "KISS ON");
		AddSimpleStringEntry(pList, "RESTART!!0");
#else
		pcInfo->OpenCmds.Count = 6;
		pcInfo->OpenCmds.Strings = (char **) malloc(sizeof(*pcInfo->OpenCmds.Strings)*pcInfo->OpenCmds.Count);
		pcInfo->OpenCmds.Strings[0] = _strdup("^M~");
		pcInfo->OpenCmds.Strings[1] = _strdup("^M~");
		pcInfo->OpenCmds.Strings[2] = _strdup("XFLOW OFF");
		pcInfo->OpenCmds.Strings[3] = _strdup("FULLDUP OFF");	/* Was on, but that disables carrier detect */
		pcInfo->OpenCmds.Strings[4] = _strdup("KISS ON");
		pcInfo->OpenCmds.Strings[5] = _strdup("RESTART!!0");
#endif
	}

	if (!pcInfo->CloseCmds.Count)
	{	STRING_LIST_S *pList = &pcInfo->CloseCmds;
#ifdef USE_TIMED_STRINGS
		AddSimpleStringEntry(pList, "^192^255^192~!!0");
		AddSimpleStringEntry(pList, "^C^C^C~!!0");
		AddSimpleStringEntry(pList, "TC 1!TS 1");
		AddSimpleStringEntry(pList, "TN 2,0!TN 2,0");
#else
		pcInfo->CloseCmds.Count = 4;
		pcInfo->CloseCmds.Strings = (char **) malloc(sizeof(*pcInfo->CloseCmds.Strings)*pcInfo->CloseCmds.Count);
		pcInfo->CloseCmds.Strings[0] = _strdup("^192^255^192~!!0");
		pcInfo->CloseCmds.Strings[1] = _strdup("^C^C^C~!!0");
		pcInfo->CloseCmds.Strings[2] = _strdup("TC 1!TS 1");
		pcInfo->CloseCmds.Strings[3] = _strdup("TN 2,0!TN 2,0");
#endif
	}

	return TRUE;
}

BOOL D700KISSTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo)
{
	if (!pcInfo->OpenCmds.Count)
	{	STRING_LIST_S *pList = &pcInfo->OpenCmds;
		AddSimpleStringEntry(pList, "^M~");
		AddSimpleStringEntry(pList, "^M~");
//		AddSimpleStringEntry(pList, "LOC E 0");
//		AddSimpleStringEntry(pList, "LTMON 5");
//		AddSimpleStringEntry(pList, "LTMHEAD OFF");
//		AddSimpleStringEntry(pList, "GPSTEXT $GPRMC");
		AddSimpleStringEntry(pList, "XFLOW OFF");
		AddSimpleStringEntry(pList, "FULLDUP OFF");	/* Was on, but that disables carrier detect */
		AddSimpleStringEntry(pList, "KISS ON");
		AddSimpleStringEntry(pList, "RESTART!!0");
	}

	if (!pcInfo->CloseCmds.Count)
	{	STRING_LIST_S *pList = &pcInfo->CloseCmds;
		AddSimpleStringEntry(pList, "^192^255^192~!!0");
	}

	return TRUE;
}

BOOL SimplyKISSTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo)
{
	if (!pcInfo->OpenCmds.Count)
	{	STRING_LIST_S *pList = &pcInfo->OpenCmds;
		AddSimpleStringEntry(pList, "~!!0");
	}

	if (!pcInfo->CloseCmds.Count)
	{	STRING_LIST_S *pList = &pcInfo->CloseCmds;
		AddSimpleStringEntry(pList, "~!!0");
	}

	return TRUE;
}

