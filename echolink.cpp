#include "sysheads.h"

#include <xp/source/xmlparse.h>		/* The raw XML Parser */

#include "filter.h"
#include "llutil.h"
#include "OSMutil.h"
#include "parsedef.h"
#include "tcputil.h"

#include "EchoLink.h"

//unsigned long EchoLinkCount = 0;
//unsigned long EchoLinkSize = 0;
//unsigned long EchoLinkRAM = 0;
//ECHOLINK_INFO_S *EchoLinks = NULL;

unsigned long OffLine = 0, UnKnown = 0, OnLine = 0;

// helper macros
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (UINT_PTR)(sizeof(a)/sizeof((a)[0]))
#endif

typedef struct PARSE_STACK_S
{	long ValueCount;
	long ValueMax;
	char *ValueB;
	char *Name;
	long ElementCount;
	long ElementMax;
} PARSE_STACK_S;

typedef struct PARSE_INFO_S
{	XML_Parser Parser;
	HWND hwnd;
	HWND hwndProgress;

	UINT msgReceived;
	WPARAM wp;

	BOOL ProgressFailed;
	long FileSize;
	long Depth;
	long MaxDepth;

	int DidCount;
	ECHOLINK_INFO_S Link;
	BOOL GotLat, GotLon, GotNode, GotCall;

	PARSE_STACK_S *Stack;
} PARSE_INFO_S;

static void SpinMessages(HWND hwnd)
{	MSG msg;
	__int64 msNow = llGetMsec();
static	__int64 msNext = 0;

	if (msNow >= msNext)
	{	msNext = msNow + 500;

	while (PeekMessage(&msg, hwnd,  0, 0,
#define PROCESS_ALL
#ifndef PROCESS_ALL
#ifndef UNDER_CE
						//PM_QS_INPUT |
						PM_QS_PAINT |
						PM_QS_POSTMESSAGE |
						PM_QS_SENDMESSAGE |
#endif
#endif
						PM_REMOVE)) 
	{	TranslateMessage(&msg); 
		DispatchMessage(&msg); 
    } 
	}
}

COLORREF GetScaledRGColor(double Current, double RedValue, double GreenValue);
extern HINSTANCE g_hInstance;

static void UpdateProgress(PARSE_INFO_S *Info)
{	long CurIndex = XML_GetCurrentByteIndex(Info->Parser);
	long ByteCount = Info->FileSize;

	if (!Info->hwndProgress && !Info->ProgressFailed && ByteCount)
	{	RECT rc;
		GetWindowRect(Info->hwnd, &rc);
		Info->hwndProgress = CreateWindow(
						PROGRESS_CLASS,		/*The name of the progress class*/
						NULL, 			/*Caption Text*/
						CCS_TOP | WS_CHILD | WS_VISIBLE
						| WS_BORDER | WS_CLIPSIBLINGS
						| WS_EX_STATICEDGE
						| PBS_SMOOTH,	/*Styles*/
						0, 			/*X co-ordinates*/
						(rc.bottom-rc.top)/2-15, 			/*Y co-ordinates*/
						rc.right-rc.left, 			/*Width*/
						30, 			/*Height*/
						Info->hwnd, 			/*Parent HWND*/
						(HMENU) 1, 	/*The Progress Bar's ID*/
						g_hInstance,		/*The HINSTANCE of your program*/ 
						NULL);			/*Parameters for main window*/
		if (!Info->hwndProgress)
		{	Info->ProgressFailed = TRUE;
			TraceLog("EchoLinks", TRUE, Info->hwnd, "CreateWindow(Progress) Failed with %ld\n", GetLastError());
		} else
		{	TraceLog("EchoLinks", TRUE, Info->hwnd, "Progress window created %p @ %ld %ld %ld %ld\n", Info->hwndProgress, (long) rc.left, (long) rc.top, (long) rc.right, (long) rc.bottom);
			SendMessage(Info->hwndProgress, PBM_SETRANGE32, 0, ByteCount);
		}
	}
	if (Info->hwndProgress)
	{	SpinMessages(Info->hwnd);
#ifndef UNDER_CE
//		COLORREF bar = GetScaledRGColor(CurIndex, 0, ByteCount);
//		SendMessage(Info->hwndProgress, PBM_SETBARCOLOR, 0, bar);
#endif
		SendMessage(Info->hwndProgress, PBM_SETPOS, CurIndex, 0);
	}
}

static void startElement(void *userData, const char *name, const char **atts)
{	PARSE_INFO_S *Info = (PARSE_INFO_S *)userData;

	if (Info->Depth) Info->Stack[Info->Depth].ElementCount++;
	Info->Depth++;
	if (Info->Depth >= Info->MaxDepth)
	{	Info->MaxDepth = Info->Depth+1;
		Info->Stack = (PARSE_STACK_S *)realloc(Info->Stack, sizeof(*Info->Stack)*Info->MaxDepth);
		memset(&Info->Stack[Info->Depth], 0, sizeof(Info->Stack[Info->Depth])/**(Info->MaxDepth-Info->Depth+1)*/);
	}

	Info->Stack[Info->Depth].ValueCount = 0;
	Info->Stack[Info->Depth].ElementCount = 0;
	if (Info->Stack[Info->Depth].ValueB) *Info->Stack[Info->Depth].ValueB = '\0';
	if (Info->Depth > 2)
	{	int Len = strlen(Info->Stack[Info->Depth-1].Name) + 1 + strlen(name) + 1;
		Info->Stack[Info->Depth].Name = (char *) malloc(Len);
		sprintf(Info->Stack[Info->Depth].Name, "%s.%s", Info->Stack[Info->Depth-1].Name, name);
		//if (Info->Depth == 2) UpdateProgress(Info);
	} else Info->Stack[Info->Depth].Name = _strdup(name);

//	TraceLog("EchoLinks", FALSE, Info->hwnd, "[%ld]startElement(%s)\n", (long) Info->Depth, Info->Stack[Info->Depth].Name);

//	const char **a;
//	for (a=atts; *a; a+=2)
//	{	TraceLog("EchoLinks", FALSE, Info->hwnd, "[%ld]     (%s) %s=%s\n", (long) Info->Depth, Info->Stack[Info->Depth].Name, a[0], a[1]);
//	}

	if (!strcmp(Info->Stack[Info->Depth].Name, "station"))
	{	Info->GotLat = Info->GotLon = Info->GotNode = Info->GotCall = FALSE;
		if (Info->Link.location) free(Info->Link.location);
		if (Info->Link.dblocation) free(Info->Link.dblocation);
		if (Info->Link.status_comment) free(Info->Link.status_comment);
		memset(&Info->Link, 0, sizeof(Info->Link));
//		TraceLog("EchoLinks", FALSE, Info->hwnd, "Did WPT %sLat=%.4lf %sLon=%.4lf\n", Info->GotLat?"":"MISSING ", Info->lat, Info->GotLon?"":"MISSING ", Info->lon);
	}
}

static void CrunchHtml(char *string)
{	char *c, *s;

	for (c=string; s=strchr(c,'<'); c=s)
	{	char *e = strchr(s,'>');
		if (!e) break;
		if (_stricmp(s,"<br")
		&& _stricmp(s,"<li"))
			*s++ = ' ';
		else *s++ = '\n';
		strcpy(s,e+1);
	}
}

static void CrunchSpaces(char *string)
{	int i, len = strlen(string);

	for (i=0; i<len; i++)
		if (!isspace(string[i]&0xff))
			break;
	if (i)
	{	strcpy(string,&string[i]);
		len -= i;
	}

	for (; i<len-3; i++)
	{	if (string[i]==string[i+1]
		&& string[i]==string[i+2])
		{	int j;
			for (j=i+3; j<len; j++)
				if (string[i] != string[j])
					break;
			len -= (j-(i+2));
			strcpy(&string[i+2], &string[j]);
		}
	}
}

static BOOL MakeEchoLinkObject(HWND hwnd, UINT msgReceived, WPARAM wp, ECHOLINK_INFO_S *Link);

static void endElement(void *userData, const char *name)
{	PARSE_INFO_S *Info = (PARSE_INFO_S *) userData;

//	Info->Stack[Info->Depth].Name has value Info->Stack[Info->Depth].ValueB IF Info->Stack[Info->Depth].ValueCount

//TraceLog("EchoLinks", TRUE, Info->hwnd, "[%ld]endElement(%s) Count %ld Value %.*s\n",
//					(long) Info->Depth, Info->Stack[Info->Depth].Name, (long) Info->Stack[Info->Depth].ElementCount,
//					(int) Info->Stack[Info->Depth].ValueCount, Info->Stack[Info->Depth].ValueB);

	if (!Info->Stack[Info->Depth].ElementCount
	&& Info->Stack[Info->Depth].ValueB)
	{	if (!strcmp(Info->Stack[Info->Depth].Name,"station.call"))
		{	strncpy(Info->Link.call,Info->Stack[Info->Depth].ValueB,sizeof(Info->Link.call));
			Info->GotCall = TRUE;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.location"))
		{	Info->Link.location = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.dblocation"))
		{	Info->Link.dblocation = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.node"))
		{	char *e;
			Info->Link.node = strtol(Info->Stack[Info->Depth].ValueB, &e, 10);
			Info->GotNode = !*e;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.lat"))
		{	char *e;
			Info->Link.lat = strtod(Info->Stack[Info->Depth].ValueB, &e);
			Info->GotLat = !*e;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.lon"))
		{	char *e;
			Info->Link.lon = strtod(Info->Stack[Info->Depth].ValueB, &e);
			Info->GotLon = !*e;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.freq"))
		{	char *e;
			Info->Link.freq = strtod(Info->Stack[Info->Depth].ValueB, &e);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.pl"))
		{	char *e;
			Info->Link.pl = strtod(Info->Stack[Info->Depth].ValueB, &e);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.power"))
		{	char *e;
			Info->Link.power = strtol(Info->Stack[Info->Depth].ValueB, &e, 10);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.haat"))
		{	char *e;
			Info->Link.haat = strtol(Info->Stack[Info->Depth].ValueB, &e, 10);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.gain"))
		{	char *e;
			Info->Link.gain = strtol(Info->Stack[Info->Depth].ValueB, &e, 10);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.directivity"))
		{	char *e;
			Info->Link.directivity = strtol(Info->Stack[Info->Depth].ValueB, &e, 10);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.status"))
		{	Info->Link.status = *Info->Stack[Info->Depth].ValueB;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.status_comment"))
		{	Info->Link.status_comment = _strdup(Info->Stack[Info->Depth].ValueB);	/* 0123456789012345 */
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"station.last_update"))	/* MM/DD/YYYY HH:MM */
		{	if (strlen(Info->Stack[Info->Depth].ValueB) == 16
			&& Info->Stack[Info->Depth].ValueB[2] == '/'
			&& Info->Stack[Info->Depth].ValueB[5] == '/'
			&& Info->Stack[Info->Depth].ValueB[10] == ' '
			&& Info->Stack[Info->Depth].ValueB[13] == ':')
			{	char *e;
				Info->Link.st.wYear = (WORD) strtol(Info->Stack[Info->Depth].ValueB+6, &e, 10);
				Info->Link.st.wMonth = (WORD) strtol(Info->Stack[Info->Depth].ValueB+0, &e, 10);
				Info->Link.st.wDay = (WORD) strtol(Info->Stack[Info->Depth].ValueB+3, &e, 10);
				Info->Link.st.wHour = (WORD) strtol(Info->Stack[Info->Depth].ValueB+11, &e, 10);
				Info->Link.st.wMinute = (WORD) strtol(Info->Stack[Info->Depth].ValueB+14, &e, 10);
			}
		}
	} else if (!strcmp(Info->Stack[Info->Depth].Name,"station"))
	{	UpdateProgress(Info);
		if (Info->GotCall && Info->GotNode && Info->GotLat && Info->GotLon)
		{	if (Info->Link.node <= 999999)
				sprintf(Info->Link.ID, "EL-%06ld", (long) Info->Link.node);
			else if (Info->Link.node <= 9999999) 
				sprintf(Info->Link.ID, "EL%ld", (long) Info->Link.node);
			else if (Info->Link.node <= 99999999)
				sprintf(Info->Link.ID, "E%ld", (long) Info->Link.node); 
			else sprintf(Info->Link.ID, "%ld", (long) Info->Link.node); 
//			EchoLinkRAM += strlen(Info->Link.location)+1;
//			EchoLinkRAM += strlen(Info->Link.dblocation)+1;
//			EchoLinkRAM += strlen(Info->Link.status_comment)+1;

#ifdef OLD_WAY
			unsigned long g;
			for (g=0; g<EchoLinkCount; g++)
				if (!strncmp(EchoLinks[g].call, Info->Link.call, sizeof(EchoLinks[g].call)))
					break;
			if (g >= EchoLinkCount)	/* It's a new one */
			{	g = EchoLinkCount++;
				if (g >= EchoLinkSize)
				{	EchoLinkSize += 32;
					EchoLinkRAM += sizeof(*EchoLinks)*32;
					EchoLinks = (ECHOLINK_INFO_S *)realloc(EchoLinks,sizeof(*EchoLinks)*EchoLinkSize);
				}
			} else
			{	if (EchoLinks[g].location) free(EchoLinks[g].location);
				if (EchoLinks[g].dblocation) free(EchoLinks[g].dblocation);
				if (EchoLinks[g].status_comment) free(EchoLinks[g].status_comment);
			}
			EchoLinks[g] = Info->Link;
			Info->DidCount++;
#else
			if (MakeEchoLinkObject(Info->hwnd, Info->msgReceived, Info->wp, &Info->Link))
				Info->DidCount++;
			if (Info->Link.location) free(Info->Link.location);
			if (Info->Link.dblocation) free(Info->Link.dblocation);
			if (Info->Link.status_comment) free(Info->Link.status_comment);
#endif
			memset(&Info->Link, 0, sizeof(Info->Link));

#ifdef VERBOSE
			TraceLog("EchoLinks", FALSE, Info->hwnd, "EchoLink[%ld] is %.*s (%.*s) at %.4lf %.4lf\n", (long) g, sizeof(EchoLinks[g].ID), EchoLinks[g].ID, sizeof(EchoLinks[g].call), EchoLinks[g].call, EchoLinks[g].lat, EchoLinks[g].lon);
#endif
		}
else TraceLog("EchoLinks", FALSE, Info->hwnd, "%s %s %s %s\n", Info->GotCall?"":"NO CALL", Info->GotNode?"":"NO NODE", Info->GotLat?"":"NO LAT", Info->GotLon?"":"NO LON");
	}
	free(Info->Stack[Info->Depth].Name);

	Info->Depth--;
}

static void characterData(void *userData, const char *s, int len)
{	PARSE_INFO_S *Info = (PARSE_INFO_S *) userData;
	char * Value = Info->Stack[Info->Depth].ValueB;
	long Len = Info->Stack[Info->Depth].ValueCount;
	long i;

	for (i=0; i<len; i++)
		if (s[i] != '\n' && s[i] != '\r')
			break;
//	if (i && i>=len) return;

	if (Len+len+1 > Info->Stack[Info->Depth].ValueMax)
	{	//Info->Stack[Info->Depth].ValueMax += RtGetGrowthFactor(Info->Stack[Info->Depth].ValueMax);
		if (Len+len+1 > Info->Stack[Info->Depth].ValueMax)
			Info->Stack[Info->Depth].ValueMax = Len+len+1;
		Value = (char *) realloc(Value, Info->Stack[Info->Depth].ValueMax);
	}
	for (i=0; i<len; i++)
	{	if (s[i] != '\n' && s[i] != '\r')
		{	Value[Len++] = s[i];
		} else
		{	Value[Len++] = ' ';	/* Linebreak == whitespace */
		}
	}
	Value[Len] = '\0';
	Info->Stack[Info->Depth].ValueCount = Len;
	Info->Stack[Info->Depth].ValueB = Value;
}

static void startCdata(void *userData)
{
#ifdef VERBOSE
	DgPrintf("Not Sure What To Do With CData Start!\n");
#endif
}

static void endCdata(void *userData)
{
#ifdef VERBOSE
	DgPrintf("Not Sure What To Do With CData End!\n");
#endif
}

static char CharPower(int Power)
{	long value = Power;

	if ( value < 1 )
    	return '0';

	if ( value >= 1 && value < 4 )
    	return '1';

	if ( value >= 4 && value < 9 )
    	return '2';

	if ( value >= 9 && value < 16 )
	    return '3';

	if ( value >= 16 && value < 25 )
	    return '4';

	if ( value >= 25 && value < 36 )
	    return '5';

	if ( value >= 36 && value < 49 )
	    return '6';

	if ( value >= 49 && value < 64 )
	    return '7';

	if ( value >= 64 && value < 81 )
	    return '8';

	if ( value >= 81 )
	    return '9';

	return '?';
}

static char CharHeight(int Height)
{	long value = Height;

	return (char)('0'+(long)(log(((double)value)/10.0)/log((double)2)));
}

static char CharGain(int Gain)
{	long value = Gain;

    if (value > 9)
        return '9';

    if (value < 0)
        return '0';

    return (char)('0'+value);
}



static char CharDirection(int Direction)
{
	if (Direction < 0 || Direction > 8) return '?';
	return Direction+'0';
}

static char *PHG(int Power, int Height, int Gain, int Direction)
{	char *Output = (char*)malloc(8);
	sprintf(Output, "PHG%c%c%c%c",
		CharPower(Power), CharHeight(Height),
		CharGain(Gain), CharDirection(Direction));
	return Output;
}

static char *tRANGE(int Power, int Height, int Gain, int Direction)
{	double p = Power;
	double h = Height;
	double g = pow((double)10,(double)Gain/10);
	double Range = sqrt(2*h*sqrt((p/10)*(g/2)));
	long r = (long) (Range+0.5);
	char *Output = (char*)malloc(5);
static	char *Prefix[] = { "R", "NE", "E", "SE", "S", "SW", "W", "NW", "N" };

	if (r>99) r = 99;

	if (Direction < 0 || Direction > 8)
		sprintf(Output,"R%02ldm", (long) r);
	else
	{	char *Pre = Prefix[Direction];
		sprintf(Output,"%s%02ld%s", Pre, (long) r, strlen(Pre)==1?"m":"");
	}
	return Output;
}

static char *FREQ(double Freq, double Tone, char *sPHG, char *sRange)
{	double f = Freq;
	long t = (long) Tone;
	char *Output = (char*)malloc(max(21,33+33+strlen(sPHG)+3));
static	struct
{	double Base;
	char Prefix;
} Freqs[] = {	{ 1200, 'A' }, { 2300, 'B' }, { 2400, 'C' },
		{ 3400, 'D' }, { 5600, 'E' }, { 5700, 'F' },
		{ 5800, 'G' }, { 10100, 'H' }, { 10200, 'I' },
		{ 10300, 'J' }, { 10400, 'K' }, { 10500, 'L' },
		{ 24000, 'M' }, { 24100, 'N' }, { 24200, 'O' } };

	if (f < 1000.0 && t < 1000)
	{	sprintf(Output,"%7.3lfMHz T%03ld %4.4s", (double) f, (long) t, sRange);
		if (Output[0] == ' ') Output[0] = '0';
		if (Output[1] == ' ') Output[1] = '0';
	} else
	{	int i;	
		for (i=0; i<ARRAYSIZE(Freqs); i++)
		{	if (f >= Freqs[i].Base && f < Freqs[i].Base+100)
				break;
		}
		if (i<ARRAYSIZE(Freqs) && t < 1000)
		{	sprintf(Output,"%c%6.3lfMHz T%03ld %4.4s", Freqs[i].Prefix, (double) f-Freqs[i].Base, (long) t, sRange);
			if (Output[1] == ' ') Output[1] = '0';
		} else sprintf(Output,"%s %lf %lf", sPHG, Freq, Tone);
	}

	{	char *t = strstr(Output,"T000");
		if (t) strncpy(t,"Toff",4);
	}

	return Output;
}

FILTER_INFO_S Filter = {0};

static BOOL MakeEchoLinkObject(HWND hwnd, UINT msgReceived, WPARAM wp, ECHOLINK_INFO_S *Link)
{	BOOL Result = FALSE;
	{	APRS_PARSED_INFO_S *Packet = (APRS_PARSED_INFO_S *)calloc(1,sizeof(*Packet));

		Packet->Valid = APRS_LATLON_VALID | APRS_SYMBOL_VALID | APRS_OBJECT_VALID;
		Packet->lat = Link->lat;
		Packet->lon = Link->lon;
		Packet->symbol = ('E'<<16) | 0x100 | '0';	/* Circle E */
		strncpy(Packet->srcCall, "ECHOLINK", sizeof(Packet->srcCall));
		strncpy(Packet->dstCall, "APELNK", sizeof(Packet->dstCall));
		sprintf(Packet->objCall, "EL-%06ld", Link->node);

		BOOL Hit = FilterPacket(&Filter, Packet);
		if (!Hit) return FALSE;
		free(Packet);
	}

if (Link->status == 'N' || Link->status == 'C' || Link->status == 'B')
{	TCHAR *tLatLon = APRSLatLon(Link->lat, Link->lon, 'E', '0', 0);	/* E0=CircleE, /r=antenna */
	char * sPHG = PHG(Link->power,Link->haat,Link->gain,Link->directivity);
	char * sRange = tRANGE(Link->power,Link->haat,Link->gain,Link->directivity);
	char * sFreq = FREQ(Link->freq, Link->pl, sPHG, sRange);
	char * srcCall = _strdup(Link->call);
	char * dashCall = strchr(srcCall,'-');
	double Gain = pow((double)10,(double)Link->gain/10);
	double Range = sqrt(2*Link->haat*sqrt((Link->power/10)*(Gain/2)));
	char * tLocation = _strnicmp(Link->location,"In Conference ",14)?Link->location:Link->location+14;
	char * status;
	char * Packet;

	if (dashCall) *dashCall++ = '\0';	/* Remove -L/-R */
	else dashCall = "?";			/* No dash letter */

	switch (Link->status)
	{
	case 'N': status = "On"; break;
	case 'B': status = "Busy"; break;
	case 'C': status = "Conf"; break;
	case 'F': status = "Off"; break;
	default:
		TraceLog("EchoLinks", TRUE, hwnd, "*** Node %s Unknown Status Len(%c), Location(%s) Comment(%s)\n", Link->call, Link->status, Link->location, Link->status_comment);
		status = "?";
	}

	OnLine++;
#ifdef VERBOSE
	TraceLog("EchoLinks", FALSE, hwnd, "Station:%s Loc:%s Node:%ld lat/lon:%.4lf %.4lf Freq:%.3lf(%.1lf) phgd(%s):%ld %ld %ld %ld Range:%.1lfm Status:%s(%s) Updated:%04ld-%02ld-%02ld %02ld:%02ld\n",
		Link->call, Link->location, Link->node,
		Link->lat, Link->lon, Link->freq, Link->pl,
		PHG(Link->power, Link->haat, Link->gain, Link->directivity),
		Link->power, Link->haat, Link->gain, Link->directivity, (double) Range,
		status, Link->status_comment,
		Link->st.wYear, Link->st.wMonth, Link->st.wDay,
		Link->st.wHour, Link->st.wMinute);
#endif

#ifdef FORGERY
	TraceLog("EchoLinks", FALSE, hwnd, "%s>APELNK,KJ4ERJ*:;EL-%06ld*%02ld%02ld%02ldz%S%s %s%s %.*s\n",
		srcCall, Link->node, Link->st.wDay, Link->st.wHour, Link->st.wMinute,
		tLatLon, sFreq, status, Link->call, (int) max(0,(int)(43-(strlen(sFreq)+1+strlen(status)+strlen(Link->call)+1))), tLocation);
#endif

	Packet = (char*)malloc(512);
	sprintf(Packet,"ECHOLINK>APELNK:;EL-%06ld*%02ld%02ld%02ldz%S%s %s %s %.*s",
		Link->node, Link->st.wDay, Link->st.wHour, Link->st.wMinute,
		tLatLon, sFreq, status, Link->call, (int) max(0,(int)(43-(strlen(sFreq)+1+strlen(status)+strlen(Link->call)+1))), tLocation);
	TraceLog("EchoLinks", FALSE, hwnd, "%s\n",Packet);

	SendMessage(hwnd, msgReceived, wp, (LPARAM) Packet);	/* wp = Ports Index, lp = Received Packet */
	Result = TRUE;

#ifdef FUTURE
	if (strstr(location,"Palm Bay"))
		DsAddToPointerArray(Pkts, Packet);
#endif

#ifdef TRUNCATE
	if (strlen(tLocation) > max(0,(int)(43-(strlen(sFreq)+1+strlen(status)+strlen(Link->call)+1))))
		TraceLog("EchoLinks", FALSE, hwnd, "*** Node %s Location (%s) Truncated to (%.*s)\n",
			Link->call, Link->location,
			(int) max(0,(int)(43-(strlen(sFreq)+1+strlen(status)+strlen(Link->call)+1))), tLocation);
#endif

	free(srcCall);
	free(tLatLon);
	free(sRange);
	free(sFreq);
	free(sPHG);
} else if (Link->status == 'F')
{	OffLine++;
	TraceLog("EchoLinks", FALSE, hwnd, "[%ld]*** %s Offline Status %c (%s)\n", OffLine, Link->call, Link->status, Link->status_comment);
} else
{	UnKnown++;
	TraceLog("EchoLinks", FALSE, hwnd, "[%ld]*** %s Unknown Status %c (%s)\n", UnKnown, Link->call, Link->status, Link->status_comment);
}
	return Result;
}

void FreeEchoLinks(HWND hwnd)
{
//	for (unsigned int g=0; g<EchoLinkCount; g++)
//	{	if (EchoLinks[g].location) free(EchoLinks[g].location);
//		if (EchoLinks[g].dblocation) free(EchoLinks[g].dblocation);
//		if (EchoLinks[g].status_comment) free(EchoLinks[g].status_comment);
//	}
//	if (EchoLinks) free(EchoLinks);
//	EchoLinks = NULL;
//	EchoLinkSize = EchoLinkCount = EchoLinkRAM = 0;
}

extern "C"
{
char * cdecl FormatFilter(void);
}

unsigned long LoadEchoLinks(HWND hwnd, UINT msgReceived, WPARAM wp)
{
	int BufLen;
	char *Buffer;
	TCHAR *Address = NULL;
	int DoneCount = 0;

	Buffer = httpGetBuffer(hwnd, "www.echolink.org", 80, "/node_location.xml", &BufLen, "APRSISCE/32", TRUE);
	if (!Buffer) return 0;

	{	char *CurrentFilter = FormatFilter();
		if (!CheckOptimizedFilter(CurrentFilter, &Filter))
			TraceError(hwnd, "Invalid Current Filter(%s)\n", CurrentFilter);
		free(CurrentFilter);
	}

	{	PARSE_INFO_S *Info;
		XML_Parser parser = XML_ParserCreate(NULL);
		Info = (PARSE_INFO_S *) calloc(1,sizeof(*Info));
		Info->Parser = parser;
		Info->hwnd = hwnd;
		Info->msgReceived = msgReceived;
		Info->wp = wp;
		Info->FileSize = BufLen;
		XML_SetUserData(parser, Info);
		XML_SetElementHandler(parser, startElement, endElement);
		XML_SetCdataSectionHandler(parser, startCdata, endCdata);
		XML_SetCharacterDataHandler(parser, characterData);

		BOOL Result = TRUE;
		Result = XML_Parse(parser, Buffer, BufLen, FALSE);
		if (Result) Result = XML_Parse(parser, NULL, 0, TRUE);
		if (!Result)
		{static TCHAR Buffer[256];
			StringCbPrintf(Buffer, sizeof(Buffer), TEXT("XML_Parse FAILED!  %S at line %d\n"),
						XML_ErrorString(XML_GetErrorCode(parser)),
						XML_GetCurrentLineNumber(parser));
			MessageBox(hwnd, Buffer, TEXT("LoadEchoLinks"), MB_OK);
			TraceActivityEnd(hwnd, "XML_Parse FAILED!  %s at line %d\n",
						XML_ErrorString(XML_GetErrorCode(parser)),
						XML_GetCurrentLineNumber(parser));
		} else
		{	DoneCount = Info->DidCount;
			TraceActivityEnd(hwnd, "Loaded %ld EchoLinks\n", (long) DoneCount);
		}
		if (Info->hwndProgress)
		{	UpdateProgress(Info);
			DestroyWindow(Info->hwndProgress);
		}
		XML_ParserFree(parser);

		if (Info->Link.location) free(Info->Link.location);
		if (Info->Link.dblocation) free(Info->Link.dblocation);
		if (Info->Link.status_comment) free(Info->Link.status_comment);
#ifdef UNNECESSARY
		for (int i=0; i<Info->MaxDepth; i++)
			if (Info->Stack[i].ValueB) 
				free(Info->Stack[i].ValueB);
		if (Info->Stack) free(Info->Stack);
		free(Info);
#endif
		memset(&Info->Link, 0, sizeof(Info->Link));
	}
//	for (unsigned int e=0; e<EchoLinkCount; e++)
//		MakeEchoLinkObject(hwnd, &EchoLinks[e]);

	TraceLog("EchoLinks", TRUE, hwnd, "Found %ld OnLine, %ld OffLine, %ld Unknown in %ld Bytes of XML\n", OnLine, OffLine, UnKnown, BufLen);

	free(Buffer);

	return DoneCount;
}

