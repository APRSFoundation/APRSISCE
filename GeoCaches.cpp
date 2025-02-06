#include "sysheads.h"

#include <xp/source/xmlparse.h>		/* The raw XML Parser */

#include "config.h"
#include "tcputil.h"
//#include "llutil.h"	/* For APRSSymbolInt */
#include "parsedef.h"	/* required for parse.h */
#include "parse.h"	/* For GetSymbolByName */

#include "Geocaches.h"

unsigned long GeocacheCount = 0;
unsigned long GeocacheSize = 0;
unsigned long GeocacheRAM = 0;
GEOCACHE_INFO_S *Geocaches = NULL;

// helper macros
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (UINT_PTR)(sizeof(a)/sizeof((a)[0]))
#endif

static char *GetGPXFile(HWND hwnd)
{	char *Return = NULL;
static	OPENFILENAME ofn = {0};
	
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = TEXT("GPX Files\0*.gpx\0LOC Files\0*.loc\0All Files\0*.*\0\0");
	ofn.nFilterIndex = 1;	/* Default to GPX */
	ofn.nMaxFile = 256;
	ofn.lpstrFile = (LPWSTR) calloc(ofn.nMaxFile,sizeof(*ofn.lpstrFile));
	ofn.lpstrTitle = TEXT("Geocache File");
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	ofn.lpstrDefExt = TEXT("gpx");
	
	if (GetOpenFileName(&ofn))
	{	Return = (char*)malloc(ofn.nMaxFile);
		StringCbPrintfA(Return, ofn.nMaxFile, "%.*S", ofn.nMaxFile, ofn.lpstrFile);
	}
	return Return;
}

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
	BOOL ProgressFailed;
	long FileSize;
	long Depth;
	long MaxDepth;

	char *name;
	char *desc, *hint;
	char *type, *container;
	char *difficulty, *terrain;
	char *sdesc, *ldesc;
	double lat, lon;
	int symbol;
	BOOL GotLat, GotLon, GotName, GotDesc, GotShort, GotLong, htmlShort, htmlLong;

	PARSE_STACK_S *Stack;
} PARSE_INFO_S;

static void SpinMessages(HWND hwnd)
{	MSG msg;

	while (PeekMessage(&msg, hwnd,  0, 0,
#ifndef UNDER_CE
						//PM_QS_INPUT |
						PM_QS_PAINT |
						PM_QS_POSTMESSAGE |
						PM_QS_SENDMESSAGE |
#endif
						PM_REMOVE)) 
	{	TranslateMessage(&msg); 
		DispatchMessage(&msg); 
    } 
}

COLORREF GetScaledRGColor(double Current, double RedValue, double GreenValue);

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
			TraceLog("GeoCaches", TRUE, Info->hwnd, "CreateWindow(Progress) Failed with %ld\n", GetLastError());
		} else
		{	TraceLog("GeoCaches", TRUE, Info->hwnd, "Progress window created %p @ %ld %ld %ld %ld\n", Info->hwndProgress, (long) rc.left, (long) rc.top, (long) rc.right, (long) rc.bottom);
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

//	TraceLog("GeoCaches", FALSE, Info->hwnd, "[%ld]startElement(%s)\n", (long) Info->Depth, Info->Stack[Info->Depth].Name);

//	const char **a;
//	for (a=atts; *a; a+=2)
//	{	TraceLog("GeoCaches", FALSE, Info->hwnd, "[%ld]     (%s) %s=%s\n", (long) Info->Depth, Info->Stack[Info->Depth].Name, a[0], a[1]);
//	}

	if (!strcmp(Info->Stack[Info->Depth].Name, "wpt"))
	{	const char **a;
//				Geocaches[g].symbol = 0x100 | 'C' | ('G'<<16);	/* G overlayed bullseye circle */
//				Geocaches[g].symbol = 0x100 | 'C';	/* No-overlay bullseye circle */
//				Geocaches[g].symbol = '/';	/* Red Dot */
//				Geocaches[g].symbol = 0x100 | '/';	/* Black Dot */
		Info->symbol = 0x100 | '/';	/* Black Dot */
		Info->GotLat = Info->GotLon = Info->GotName = Info->GotDesc = Info->GotShort = Info->GotLong = Info->htmlShort = Info->htmlLong = FALSE;
		if (Info->sdesc) free(Info->sdesc);
		if (Info->ldesc) free(Info->ldesc);
		if (Info->type) free(Info->type);
		if (Info->hint) free(Info->hint);
		if (Info->container) free(Info->container);
		if (Info->difficulty) free(Info->difficulty);
		if (Info->terrain) free(Info->terrain);
		Info->sdesc = Info->ldesc = NULL;
		Info->type = Info->hint = Info->container = NULL;
		Info->difficulty = Info->terrain = NULL;
		for (a=atts; *a; a+=2)
		{	if (!strcmp(a[0],"lat"))
			{	char *e;
				Info->lat = strtod(a[1],&e);
				Info->GotLat = !*e;
			} else if (!strcmp(a[0],"lon"))
			{	char *e;
				Info->lon = strtod(a[1],&e);
				Info->GotLon = !*e;
			}
		}
//		TraceLog("GeoCaches", FALSE, Info->hwnd, "Did WPT %sLat=%.4lf %sLon=%.4lf\n", Info->GotLat?"":"MISSING ", Info->lat, Info->GotLon?"":"MISSING ", Info->lon);
//		TraceLog("GeoCaches", FALSE, Info->hwnd, "Did WPT %sLat=%.4lf %sLon=%.4lf\n", Info->GotLat?"":"MISSING ", Info->lat, Info->GotLon?"":"MISSING ", Info->lon);
	} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.groundspeak:cache.groundspeak:short_description"))
	{	const char **a;
		for (a=atts; *a; a+=2)
		{	if (!strcmp(a[0],"html"))
			{	Info->htmlShort = !_stricmp(a[1],"True");
			}
		}
	} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.groundspeak:cache.groundspeak:long_description"))
	{	const char **a;
		for (a=atts; *a; a+=2)
		{	if (!strcmp(a[0],"html"))
			{	Info->htmlLong = !_stricmp(a[1],"True");
			}
		}
/*
	Now the .LOC file tags...
*/
	} else if (!strcmp(Info->Stack[Info->Depth].Name, "waypoint"))
	{	Info->symbol = 0x100 | '/';	/* Black Dot */
		Info->GotLat = Info->GotLon = Info->GotName = Info->GotDesc = Info->GotShort = Info->GotLong = Info->htmlShort = Info->htmlLong = FALSE;
		if (Info->sdesc) free(Info->sdesc);
		if (Info->ldesc) free(Info->ldesc);
		if (Info->type) free(Info->type);
		if (Info->hint) free(Info->hint);
		if (Info->container) free(Info->container);
		if (Info->difficulty) free(Info->difficulty);
		if (Info->terrain) free(Info->terrain);
		Info->sdesc = Info->ldesc = NULL;
		Info->type = Info->hint = Info->container = NULL;
		Info->difficulty = Info->terrain = NULL;
//		TraceLog("GeoCaches", FALSE, Info->hwnd, "Did waypoint %sLat=%.4lf %sLon=%.4lf\n", Info->GotLat?"":"MISSING ", Info->lat, Info->GotLon?"":"MISSING ", Info->lon);
	} else if (!strcmp(Info->Stack[Info->Depth].Name, "waypoint.coord"))
	{	const char **a;
		for (a=atts; *a; a+=2)
		{	if (!strcmp(a[0],"lat"))
			{	char *e;
				Info->lat = strtod(a[1],&e);
				Info->GotLat = !*e;
			} else if (!strcmp(a[0],"lon"))
			{	char *e;
				Info->lon = strtod(a[1],&e);
				Info->GotLon = !*e;
			}
		}
//		TraceLog("GeoCaches", FALSE, Info->hwnd, "Did WPT %sLat=%.4lf %sLon=%.4lf\n", Info->GotLat?"":"MISSING ", Info->lat, Info->GotLon?"":"MISSING ", Info->lon);
	} else if (!strcmp(Info->Stack[Info->Depth].Name, "waypoint.name"))
	{	const char **a;
		for (a=atts; *a; a+=2)
		{	if (!strcmp(a[0],"id"))
			{	Info->name = _strdup(a[1]);
				Info->GotName = TRUE;
			}
		}
//		TraceLog("GeoCaches", FALSE, Info->hwnd, "Did WPT %sLat=%.4lf %sLon=%.4lf\n", Info->GotLat?"":"MISSING ", Info->lat, Info->GotLon?"":"MISSING ", Info->lon);
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

static void endElement(void *userData, const char *name)
{	PARSE_INFO_S *Info = (PARSE_INFO_S *) userData;

//	Info->Stack[Info->Depth].Name has value Info->Stack[Info->Depth].ValueB IF Info->Stack[Info->Depth].ValueCount

#ifdef Encounters
2009-11-07T05:06:55 ***** [2]startElement(wpt)
2009-11-07T05:06:55 ***** [3]startElement(wpt.time)
2009-11-07T05:06:55 ***** [3]endElement(wpt.time) Value 2007-09-28T17:24:48.11
2009-11-07T05:06:55 ***** [3]startElement(wpt.name)
2009-11-07T05:06:55 ***** [3]endElement(wpt.name) Value L111A6A
2009-11-07T05:06:55 ***** [3]startElement(wpt.cmt)
2009-11-07T05:06:55 ***** [3]endElement(wpt.cmt) Value Goode Park. Concrete boat ramp. Restrooms, pavillion and parking.
2009-11-07T05:06:55 ***** [3]startElement(wpt.desc)
2009-11-07T05:06:55 ***** [3]endElement(wpt.desc) Value Goode Park
2009-11-07T05:06:55 ***** [3]startElement(wpt.url)
2009-11-07T05:06:55 ***** [3]endElement(wpt.url) Value http://www.geocaching.com/seek/wpt.aspx?WID=6760013f-a437-42a7-a949-bf3a039efc3b
2009-11-07T05:06:55 ***** [3]startElement(wpt.urlname)
2009-11-07T05:06:55 ***** [3]endElement(wpt.urlname) Value Goode Park
2009-11-07T05:06:55 ***** [3]startElement(wpt.sym)
2009-11-07T05:06:55 ***** [3]endElement(wpt.sym) Value Reference Point
2009-11-07T05:06:55 ***** [3]startElement(wpt.type)
2009-11-07T05:06:55 ***** [3]endElement(wpt.type) Value Waypoint|Reference Point
2009-11-07T05:06:55 ***** [2]endElement(wpt) Value                                   
#endif

//TraceLog("GeoCaches", TRUE, Info->hwnd, "[%ld]endElement(%s) Count %ld Value %.*s\n",
//					(long) Info->Depth, Info->Stack[Info->Depth].Name, (long) Info->Stack[Info->Depth].ElementCount,
//					(int) Info->Stack[Info->Depth].ValueCount, Info->Stack[Info->Depth].ValueB);

	if (!Info->Stack[Info->Depth].ElementCount
	&& Info->Stack[Info->Depth].ValueB)
	{	if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.name"))
		{	Info->name = _strdup(Info->Stack[Info->Depth].ValueB);
			Info->GotName = TRUE;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.desc")
		|| !strcmp(Info->Stack[Info->Depth].Name,"waypoint.name"))
		{	Info->desc = _strdup(Info->Stack[Info->Depth].ValueB);
			Info->GotDesc = TRUE;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.sym")
		|| !strcmp(Info->Stack[Info->Depth].Name,"waypoint.type"))
		{	int s;
static struct
{	char *Sym;
	int symbol;
} Symbols[] = { { "Geocache", '/' },	/* Red Dot for not found */
				{ "Geocache Found", 0x100 | '/' },	/* Black Dot for Found */
				{ "Parking Area", 0x100 | 'P' },	/* Parking symbol */
				{ "Reference Point", '.' },	/* X marks the spot */
				{ "Trailhead", '[' },	/* Jogger is trail head */
				{ "Stages of a Multicache", 0x100 | 'L' }	/* Lighthouse */
};
//				Geocaches[g].symbol = 0x100 | 'C' | ('G'<<16);	/* G overlayed bullseye circle */
//				Geocaches[g].symbol = 0x100 | 'C';	/* No-overlay bullseye circle */
//				Geocaches[g].symbol = '/';	/* Red Dot */
//				Geocaches[g].symbol = 0x100 | '/';	/* Black Dot */
			if (strlen(Info->Stack[Info->Depth].ValueB) == 2)	/* Direct symbol? */
			{	Info->symbol = SymbolInt(Info->Stack[Info->Depth].ValueB[0],
											Info->Stack[Info->Depth].ValueB[1]);
			} else
			{	Info->symbol = 0x100 | '.';		/* Default to circle ? (Unknown) */
				for (s=0; s<sizeof(Symbols)/sizeof(Symbols[0]); s++)
				{	if (!_stricmp(Info->Stack[Info->Depth].ValueB,Symbols[s].Sym))
					{	Info->symbol = Symbols[s].symbol;
						break;
					}
				}
				if (s >= sizeof(Symbols)/sizeof(Symbols[0]))
				{	int sym = GetSymbolByName(Info->Stack[Info->Depth].ValueB);
					if (sym) Info->symbol = sym;
				}
			}
			if (Info->symbol == (0x100 | '.'))	/* Still default? */
				TraceLog("GeoCaches", TRUE, Info->hwnd, "Unrecognized wpt.sym(%s)\n", Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.groundspeak:cache.groundspeak:type"))
		{	if (Info->type) free(Info->type);
			Info->type = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.groundspeak:cache.groundspeak:encoded_hints"))
		{	if (Info->hint) free(Info->hint);
			Info->hint = _strdup(Info->Stack[Info->Depth].ValueB);
			CrunchSpaces(Info->hint);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.groundspeak:cache.groundspeak:container"))
		{	if (Info->container) free(Info->container);
			Info->container = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"waypoint.container"))
		{	char *e;
			long Container = strtol(Info->Stack[Info->Depth].ValueB,&e,10);
			static char *Containers[] = { "Cont(0)", "Not Chosen", "Micro", "Regular",
											"Large", "Virtual", "Other", "Cont(7)? - Tell KJ4ERJ!", "Small" };
			if (Info->container) free(Info->container);
			if (!*e && Container >= 0 && Container < sizeof(Containers)/sizeof(Containers[0]))
			{	Info->container = _strdup(Containers[Container]);
			} else Info->container = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.groundspeak:cache.groundspeak:difficulty")
		|| !strcmp(Info->Stack[Info->Depth].Name,"waypoint.difficulty"))
		{	if (Info->difficulty) free(Info->difficulty);
			Info->difficulty = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.groundspeak:cache.groundspeak:terrain")
		|| !strcmp(Info->Stack[Info->Depth].Name,"waypoint.terrain"))
		{	if (Info->terrain) free(Info->terrain);
			Info->terrain = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.groundspeak:cache.groundspeak:short_description"))
		{	if (Info->sdesc) free(Info->sdesc);
			Info->sdesc = _strdup(Info->Stack[Info->Depth].ValueB);
			if (Info->htmlShort) CrunchHtml(Info->sdesc);
			CrunchSpaces(Info->sdesc);
			Info->GotShort = TRUE;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.groundspeak:cache.groundspeak:long_description"))
		{	if (Info->ldesc) free(Info->ldesc);
			Info->ldesc = _strdup(Info->Stack[Info->Depth].ValueB);
			if (Info->htmlLong) CrunchHtml(Info->ldesc);
			CrunchSpaces(Info->ldesc);
			Info->GotLong = TRUE;
		}
	} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt")
	|| !strcmp(Info->Stack[Info->Depth].Name,"waypoint"))
	{	UpdateProgress(Info);
		if (Info->GotName && Info->GotLat && Info->GotLon)
		{	unsigned long g;
			for (g=0; g<GeocacheCount; g++)
				if (!strncmp(Geocaches[g].ID, Info->name, sizeof(Geocaches[g].ID)))
					break;
			if (g >= GeocacheCount)	/* It's a new one */
			{	g = GeocacheCount++;
				if (g >= GeocacheSize)
				{	GeocacheSize += 32;
					GeocacheRAM += sizeof(*Geocaches)*32;
					Geocaches = (GEOCACHE_INFO_S *)realloc(Geocaches,sizeof(*Geocaches)*GeocacheSize);
				}
				strncpy(Geocaches[g].ID, Info->name, sizeof(Geocaches[g].ID));
				Geocaches[g].Desc = Info->GotDesc?Info->desc:_strdup("");
				GeocacheRAM += strlen(Geocaches[g].Desc)+1;
				Info->GotDesc = FALSE;	/* So it doesn't get freed */
				Geocaches[g].ShortDesc = Info->GotShort?Info->sdesc:_strdup("");
				Info->sdesc = NULL;
				GeocacheRAM += strlen(Geocaches[g].ShortDesc)+1;
				Info->GotShort = FALSE;	/* So it doesn't get freed */
//#ifdef UNDER_CE
//				Geocaches[g].LongDesc = _strdup("");
//#else
				Geocaches[g].LongDesc = Info->GotLong?Info->ldesc:_strdup("");
				Info->GotLong = FALSE;	/* So it doesn't get freed */
				Info->ldesc = NULL;
//#endif
				GeocacheRAM += strlen(Geocaches[g].LongDesc)+1;

				Geocaches[g].lat = Info->lat;
				Geocaches[g].lon = Info->lon;
				Geocaches[g].symbol = Info->symbol;

				Geocaches[g].Container = Info->container?Info->container:_strdup("");
				Info->container = NULL;
				GeocacheRAM += strlen(Geocaches[g].Container)+1;

				if (Info->difficulty)
				{	char *e;
					Geocaches[g].difficulty = (char) (strtod(Info->difficulty,&e)*2);
				} else Geocaches[g].difficulty = 0;
				if (Info->terrain)
				{	char *e;
					Geocaches[g].terrain = (char) (strtod(Info->terrain,&e)*2);
				} else Geocaches[g].terrain = 0;

				if (Info->type)
				{	char *s = strstr(Info->type," Cache");
					if (s) *s = '\0';	/* Shorten the (redundant) text */
				}
				Geocaches[g].Type = Info->type?Info->type:_strdup("");
				Info->type = NULL;
				GeocacheRAM += strlen(Geocaches[g].Type)+1;

				Geocaches[g].Hint = Info->hint?Info->hint:_strdup("");
				Info->hint = NULL;
				GeocacheRAM += strlen(Geocaches[g].Hint)+1;

TraceLog("GeoCaches", FALSE, Info->hwnd, "GeoCache[%ld] is %.*s at %.4lf %.4lf\n", (long) g, sizeof(Geocaches[g].ID), Geocaches[g].ID, Geocaches[g].lat, Geocaches[g].lon);
			}
		}
else TraceLog("GeoCaches", FALSE, Info->hwnd, "%s %s %s\n", Info->GotName?"":"NO NAME", Info->GotLat?"":"NO LAT", Info->GotLon?"":"NO LON");
		if (Info->GotName && Info->name) free(Info->name);
	}

#ifdef FUTURE
	XML_ELEMENT_S *Element = Info->Stack[Info->Depth].Element;

	if (Info->Stack[Info->Depth].ValueCount)
	{	Element->Value = HEAP_STRDUP(Info->AllocationHeap, Info->Stack[Info->Depth].ValueB);
	}
	if (Info->Stack[Info->Depth].ElementCount)
	{	Element->SubElementCount = Info->Stack[Info->Depth].ElementCount;
		Element->SubElements = HEAP_MALLOC(Info->AllocationHeap, Element->SubElementCount*sizeof(*Element->SubElements));
		memcpy(Element->SubElements, Info->Stack[Info->Depth].Elements,
			Element->SubElementCount*sizeof(*Element->SubElements));
	}
#endif

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

void FreeGeocaches(HWND hwnd)
{	if (Geocaches) free(Geocaches);
	Geocaches = NULL;
	GeocacheSize = GeocacheCount = GeocacheRAM = 0;
}

unsigned long LoadGeocaches(HWND hwnd)
{	unsigned long OrgCount = GeocacheCount;
	char *File = GetGPXFile(hwnd);
	if (File)
	{	FILE *In;
		In = fopen(File,"rt");
		if (!In) return FALSE;
		PARSE_INFO_S *Info;
		XML_Parser parser = XML_ParserCreate(NULL);
		Info = (PARSE_INFO_S *) calloc(1,sizeof(*Info));
		Info->Parser = parser;
		Info->hwnd = hwnd;
		fseek(In, 0, SEEK_END);
		Info->FileSize = ftell(In);
		fseek(In, 0, SEEK_SET);
		XML_SetUserData(parser, Info);
		XML_SetElementHandler(parser, startElement, endElement);
		XML_SetCdataSectionHandler(parser, startCdata, endCdata);
		XML_SetCharacterDataHandler(parser, characterData);

		BOOL Result = TRUE;
		size_t ReadBytes;
	static	char InBuf[4096];

	while (Result
	&& !ferror(In)
	&& (ReadBytes = fread(InBuf, sizeof(*InBuf), sizeof(InBuf), In)))
		{	Result = XML_Parse(parser, InBuf, ReadBytes, FALSE);
		}
		if (Result) Result = XML_Parse(parser, NULL, 0, TRUE);
		if (!Result)
		{static TCHAR Buffer[256];
			StringCbPrintf(Buffer, sizeof(Buffer), TEXT("XML_Parse(%S) FAILED!  %S at line %d\n"),
						File,
						XML_ErrorString(XML_GetErrorCode(parser)),
						XML_GetCurrentLineNumber(parser));
			MessageBox(hwnd, Buffer, TEXT("LoadGeocaches"), MB_OK);
			TraceActivityEnd(hwnd, "XML_Parse(%s) FAILED!  %s at line %d\n",
						File,
						XML_ErrorString(XML_GetErrorCode(parser)),
						XML_GetCurrentLineNumber(parser));
		} else
		{	TraceActivityEnd(hwnd, "Loaded %ld Geocaches in %ld bytes\n", (long) GeocacheCount, (long) GeocacheRAM);
		}
		if (Info->hwndProgress)
		{	UpdateProgress(Info);
			DestroyWindow(Info->hwndProgress);
		}
		XML_ParserFree(parser);
		if (Info->sdesc) free(Info->sdesc);
		if (Info->ldesc) free(Info->ldesc);
		if (Info->type) free(Info->type);
		if (Info->hint) free(Info->hint);
		if (Info->container) free(Info->container);
#ifdef UNNECESSARY
		for (int i=0; i<Info->MaxDepth; i++)
			if (Info->Stack[i].ValueB) 
				free(Info->Stack[i].ValueB);
		if (Info->Stack) free(Info->Stack);
		free(Info);
#endif
		Info->sdesc = Info->ldesc = Info->type = Info->container = NULL;
		if (In) fclose(In);
	}
	return GeocacheCount-OrgCount;
}

