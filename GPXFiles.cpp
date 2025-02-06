#include "sysheads.h"

#include <xp/source/xmlparse.h>		/* The raw XML Parser */

#include "config.h"
#include "LLUtil.h"
#include "tcputil.h"
//#include "llutil.h"	/* For APRSSymbolInt */
#include "parsedef.h"	/* required for parse.h */
#include "parse.h"	/* For GetSymbolByName */

#include "GPXFiles.h"

// helper macros
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (UINT_PTR)(sizeof(a)/sizeof((a)[0]))
#endif

extern "C"
{
void *CreateOverlayObject(OVERLAY_CONFIG_INFO_S *pOver, char *ID, char Type, double Lat, double Lon, long Alt, int isymbol, char *Comment);
void SetOverlayObjectStatus(OVERLAY_CONFIG_INFO_S *pOver, char *ID, char *Status);
void MoveOverlayObject(void *ObjHandle, double Lat, double Lon, long Alt, BOOL ClearFirst);
}

static long GPXFileCount = 0;
static long GPXObjCount = 0;
static long GPXSegCount = 0;

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
//	UINT msg;
//	WPARAM wp;
	BOOL ProgressFailed;
	OVERLAY_CONFIG_INFO_S *pOver;
	long FileSize;
	long Depth;
	long MaxDepth;

	char *name;
	char *desc;
	double lat, lon, alt;
	int symbol;
	BOOL GotLat, GotLon, GotName, GotDesc;

	BOOL GotPtName, GotPtCmt, GotPtDesc;
	char *PtName, *PtCmt, *PtDesc;

	int TrkSize;
	int TrkCount;
	TRACK_INFO_S *TrkPts;

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

#ifdef DEBUGGING
	{	FILE *Log = fopen("GPXLoad.log", "a");
		if (Log)
		{	fprintf(Log,"Progress(%s) Line %ld Char %ld Byte %lu/%lu\n",
					Info->Stack[Info->Depth].Name,
					XML_GetCurrentLineNumber(Info->Parser),
					XML_GetCurrentColumnNumber(Info->Parser),
					(unsigned long) XML_GetCurrentByteIndex(Info->Parser),
					(unsigned long) ByteCount);
			fclose(Log);
		}
	}
#endif

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
//			TraceLog("GPXLoad", TRUE, Info->hwnd, "CreateWindow(Progress) Failed with %ld\n", GetLastError());
		} else
		{
//			TraceLog("GPXLoad", TRUE, Info->hwnd, "Progress window created %p @ %ld %ld %ld %ld\n", Info->hwndProgress, (long) rc.left, (long) rc.top, (long) rc.right, (long) rc.bottom);
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

//TraceLog("GPXLoad", TRUE, Info->hwnd, "[%ld]beginElement(%s)\n", (long) Info->Depth, Info->Stack[Info->Depth].Name);

	if (!strcmp(Info->Stack[Info->Depth].Name, "wpt"))
	{	const char **a;
		Info->alt = 0;
		Info->symbol = 0; //0x100 | '/';	/* Black Dot */
		Info->GotLat = Info->GotLon = Info->GotName = Info->GotDesc = FALSE;
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
//		TraceLog("GPXLoad", FALSE, Info->hwnd, "Did WPT %sLat=%.4lf %sLon=%.4lf\n", Info->GotLat?"":"MISSING ", Info->lat, Info->GotLon?"":"MISSING ", Info->lon);
//		TraceLog("GPXLoad", FALSE, Info->hwnd, "Did WPT %sLat=%.4lf %sLon=%.4lf\n", Info->GotLat?"":"MISSING ", Info->lat, Info->GotLon?"":"MISSING ", Info->lon);
	} else if (!strcmp(Info->Stack[Info->Depth].Name, "trk.trkseg.trkpt")
	|| !strcmp(Info->Stack[Info->Depth].Name, "rte.rtept"))
	{	const char **a;
		BOOL GotLat=FALSE, GotLon=FALSE;
		double lat=0.0, lon=0.0;

		for (a=atts; *a; a+=2)
		{	if (!strcmp(a[0],"lat"))
			{	char *e;
				lat = strtod(a[1],&e);
				GotLat = !*e;
			} else if (!strcmp(a[0],"lon"))
			{	char *e;
				lon = strtod(a[1],&e);
				GotLon = !*e;
			}
		}
		if (GotLat && GotLon)
		{	int t = Info->TrkCount++;
			if (Info->TrkCount >= Info->TrkSize)
			{	Info->TrkSize += 32;
				Info->TrkPts = (TRACK_INFO_S *)realloc(Info->TrkPts,sizeof(*Info->TrkPts)*Info->TrkSize);
			}
			Info->TrkPts[t].pCoord = (COORDINATE_S *)calloc(1,sizeof(COORDINATE_S));
			Info->TrkPts[t].pCoord->lat = lat;
			Info->TrkPts[t].pCoord->lon = lon;
//			TraceLog("GPXLoad", TRUE, Info->hwnd, "Track[%ld] %.4lf %.4lf\n", t, lat, lon);
		}
//		else TraceLog("GPXLoad", TRUE, Info->hwnd, "Track missing%s%s in %ld Attrs\n", GotLat?"":" lat", GotLon?"":" lon", (long) (a-atts)/2);
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

//TraceLog("GPXLoad", TRUE, Info->hwnd, "[%ld]endElement(%s) Count %ld Value %.*s\n",
//					(long) Info->Depth, Info->Stack[Info->Depth].Name, (long) Info->Stack[Info->Depth].ElementCount,
//					(int) Info->Stack[Info->Depth].ValueCount, Info->Stack[Info->Depth].ValueB);

	if (!Info->Stack[Info->Depth].ElementCount
	&& Info->Stack[Info->Depth].ValueB)
	{	if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.name")
		|| !strcmp(Info->Stack[Info->Depth].Name,"rte.name"))
		{	Info->name = _strdup(Info->Stack[Info->Depth].ValueB);
			Info->GotName = TRUE;
//TraceLogThread("GPXLoad", TRUE, "%s is %s\n", Info->Stack[Info->Depth].Name, Info->name);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.desc")
		|| !strcmp(Info->Stack[Info->Depth].Name,"rte.desc"))
		{	Info->desc = _strdup(Info->Stack[Info->Depth].ValueB);
			Info->GotDesc = TRUE;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.ele"))
		{	char *e;
			Info->alt = (long)strtod(Info->Stack[Info->Depth].ValueB,&e);
			if (*e) TraceLog("GPXLoad", TRUE, Info->hwnd, "Invalid Alt(%s) Using %.3lf\n",
								Info->Stack[Info->Depth].ValueB,
								(double) Info->alt);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"rte.rtept.name")
		|| !strcmp(Info->Stack[Info->Depth].Name,"trk.trkseg.trkpt.name"))
		{	Info->PtName = _strdup(Info->Stack[Info->Depth].ValueB);
			Info->GotPtName = TRUE;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"rte.rtept.cmt")
		|| !strcmp(Info->Stack[Info->Depth].Name,"trk.trkseg.trkpt.cmt"))
		{	Info->PtCmt = _strdup(Info->Stack[Info->Depth].ValueB);
			Info->GotPtCmt = TRUE;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"rte.rtept.desc")
		|| !strcmp(Info->Stack[Info->Depth].Name,"trk.trkseg.trkpt.desc"))
		{	Info->PtDesc = _strdup(Info->Stack[Info->Depth].ValueB);
			Info->GotPtDesc = TRUE;
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"rte.rtept.ele")
		|| !strcmp(Info->Stack[Info->Depth].Name,"trk.trkseg.trkpt.ele"))
		{	int t = Info->TrkCount-1;
			if (t >= 0)
			{	char *e;
				Info->TrkPts[t].alt = (long)strtod(Info->Stack[Info->Depth].ValueB,&e);
				if (*e) TraceLog("GPXLoad", TRUE, Info->hwnd, "Invalid Alt(%s) Using %.3lf\n",
								Info->Stack[Info->Depth].ValueB,
								(double) Info->TrkPts[t].alt);
			}
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt.sym"))
		{
#ifdef FUTURE_GARMIN_SYMBOLS
			int s;
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

http://garminoregon.wikispaces.com/message/view/home/43318316
http://www.garmindeveloper.com/apps/GarminCustomSymbols/CustomSymbolList.txt
http://www.garmindeveloper.com/apps/GarminCustomSymbols/CustomGeocacheList.txt

http://www.topofusion.com/forum/index.php?action=printpage;topic=3452.0
#endif

//				Geocaches[g].symbol = 0x100 | 'C' | ('G'<<16);	/* G overlayed bullseye circle */
//				Geocaches[g].symbol = 0x100 | 'C';	/* No-overlay bullseye circle */
//				Geocaches[g].symbol = '/';	/* Red Dot */
//				Geocaches[g].symbol = 0x100 | '/';	/* Black Dot */
			if (strlen(Info->Stack[Info->Depth].ValueB) == 2)	/* Direct symbol? */
			{	Info->symbol = SymbolInt(Info->Stack[Info->Depth].ValueB[0],
											Info->Stack[Info->Depth].ValueB[1]);
			} else
			{	int sym = GetSymbolByName(Info->Stack[Info->Depth].ValueB);
				if (sym) Info->symbol = sym;
			}
			if (!Info->symbol /*== (0x100 | '.')*/)	/* Still default? */
				TraceLog("GPXLoad", TRUE, Info->hwnd, "Unrecognized wpt.sym(%s)\n", Info->Stack[Info->Depth].ValueB);
		}
//		else TraceLog("GPXLoad", FALSE, Info->hwnd, "Unsupported element %s(%s)\n",
//					Info->Stack[Info->Depth].Name,
//					Info->Stack[Info->Depth].ValueB);
	} else if (!strcmp(Info->Stack[Info->Depth].Name,"wpt"))
	{	UpdateProgress(Info);

		if (!Info->GotName)
		{	Info->name = (char*)malloc(3+33+1+33+1);
			sprintf(Info->name, "GPX%03ld-%02ld", GPXFileCount, GPXObjCount++);
			Info->GotName = TRUE;
		}

		if (Info->GotName && Info->GotLat && Info->GotLon)
		{	char *Desc = Info->GotDesc?Info->desc:"";
//			TraceLog("GPXLoad", TRUE, Info->hwnd, "Name:%s Desc:%s Lat:%.4lf Lon:%.4lf Sym:%ld\n",
//					Info->name, Desc,
//					(double) Info->lat, (double) Info->lon,
//					Info->symbol);
#ifdef OLD_WAY
	size_t Remaining = 256 + strlen(Info->name)+strlen(Desc);
	char *DAO=NULL, *Buffer = (char*)malloc(Remaining);
	TCHAR *LatLon = APRSLatLon(Info->lat, Info->lon,
										'/', '/',
										0, 2, &DAO);
	char *Next = Buffer;

	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
			"%s>%s:;%-9.9s%c111111z",	/* Object format */
			CALLSIGN, DESTID,
			Info->name, FALSE?'_':'*');

	/* And finally the payload */
	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
			"%S%s%s", LatLon, Desc,
			DAO?DAO:"");

	TraceLogThread("GPXLoad", TRUE, "%s", Buffer);

	free(LatLon);
	if (DAO) free(DAO);

	SendMessage(Info->hwnd, Info->msg, Info->wp, (LPARAM) Buffer);
#else
	CreateOverlayObject(Info->pOver, Info->name, 'P', Info->lat, Info->lon, (long) Info->alt, Info->symbol, Desc);
#endif
			//	Info->GotDesc = FALSE;	/* So it doesn't get freed */
		}
//else TraceLog("GPXLoad", FALSE, Info->hwnd, "%s %s %s\n", Info->GotName?"":"NO NAME", Info->GotLat?"":"NO LAT", Info->GotLon?"":"NO LON");
		if (Info->GotName && Info->name) free(Info->name);
		if (Info->GotDesc && Info->desc) free(Info->desc);
		Info->name = NULL; Info->desc = NULL;
		Info->GotName = Info->GotDesc = FALSE;


	} else if (!strcmp(Info->Stack[Info->Depth].Name,"rte.rtept")
	|| !strcmp(Info->Stack[Info->Depth].Name,"trk.trkseg.trkpt"))
	{	UpdateProgress(Info);

		if (Info->GotPtName || Info->GotPtCmt || Info->GotPtDesc)
		{	int t = Info->TrkCount-1;
			if (t >= 0)
			{	if (!Info->GotPtName)
				{	Info->PtName = (char*)malloc(3+33+1+33+1);
					sprintf(Info->PtName, "GPX%03ld-%02ld", GPXFileCount, GPXObjCount++);
					Info->GotPtName = TRUE;
				}
				char *Desc = Info->GotPtDesc?Info->PtDesc:Info->GotPtCmt?Info->PtCmt:"";
//				TraceLog("GPXLoad", TRUE, Info->hwnd, "RtePt:Name:%s Desc:%s Lat:%.4lf Lon:%.4lf\n",
//							Info->PtName, Desc,
//							(double) Info->TrkPts[t].pCoord->lat, (double) Info->TrkPts[t].pCoord->lon);
#ifdef OLD_WAY
				size_t Remaining = 256 + strlen(Info->PtName)+strlen(Desc);
				char *DAO=NULL, *Buffer = (char*)malloc(Remaining);
				TCHAR *LatLon = APRSLatLon(Info->TrkPts[t].pCoord->lat, Info->TrkPts[t].pCoord->lon,
													'/', '/',
													0, 2, &DAO);
				char *Next = Buffer;

				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
						"%s>%s:;%-9.9s%c111111z",	/* Object format */
						CALLSIGN, DESTID,
						Info->PtName, FALSE?'_':'*');

				/* And finally the payload */
				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
						"%S%s%s", LatLon, Desc,
						DAO?DAO:"");

				TraceLogThread("GPXLoad", TRUE, "%s", Buffer);

				free(LatLon);
				if (DAO) free(DAO);

				SendMessage(Info->hwnd, Info->msg, Info->wp, (LPARAM) Buffer);
#else
	CreateOverlayObject(Info->pOver, Info->PtName,
						!strcmp(Info->Stack[Info->Depth].Name,"rte.rtept")?'R':'T',
						Info->TrkPts[t].pCoord->lat,
						Info->TrkPts[t].pCoord->lon,
						Info->TrkPts[t].alt, 0, Desc);
	if (Info->GotPtDesc && Info->GotPtCmt
	&& strcmp(Info->PtDesc, Info->PtCmt))
		SetOverlayObjectStatus(Info->pOver, Info->PtName, Info->PtCmt);
#endif
			}
			if (Info->GotPtName) free(Info->PtName);
			if (Info->GotPtCmt) free(Info->PtCmt);
			if (Info->GotPtDesc) free(Info->PtDesc);
			Info->PtName = Info->PtCmt = Info->PtDesc = NULL;
			Info->GotPtName = Info->GotPtCmt = Info->GotPtDesc = FALSE;
		}
	} else if (!strcmp(Info->Stack[Info->Depth].Name,"trk.trkseg")
	|| !strcmp(Info->Stack[Info->Depth].Name,"rte"))
	{	UpdateProgress(Info);

#ifdef OLD_WAY
		if (Info->TrkCount)
		{	int t, c, TieLen;
			char *TieName;

			if (Info->GotName && *Info->name && strlen(Info->name) < 16)
			{	TieName = _strdup(Info->name);
			} else
			{	TieName = (char*)malloc(3+33+1+33+1);
				sprintf(TieName, "GPX%03ldS%ld",
					GPXFileCount,
					GPXSegCount);
				GPXSegCount++;
			}
			TieLen = strlen(TieName);
		
//#define MAX_POINTS 15
			for (t=0; t<Info->TrkCount; t+=c)	/* Move in 15 increment chunks */
			{	double lat = Info->TrkPts[t].pCoord->lat;
				double lon = Info->TrkPts[t].pCoord->lon;
				char *ML=NULL;
				
				c = min(35,Info->TrkCount-t);
				do
				{	ML = CoordTrackToVariableMultiLine("trkseg",
												lat, lon,
												c, &Info->TrkPts[t],
												128-TieLen, FALSE);
					if (!ML) c--;	/* Try one fewer */
				} while (!ML && c >= 15);
				if (ML)
				{	if (t+c < Info->TrkCount) c--;	/* Repeat last point for continuity */

					if (!Info->GotName)
					{	Info->name = (char*)malloc(3+33+1+33+1);
						sprintf(Info->name, "GPX%03ld%c%02ld",
							GPXFileCount,
							(char) (GPXObjCount<100?'-':
										GPXObjCount<1000?'0'+GPXObjCount/100:
											'A'+(GPXObjCount-1000)/100),
							GPXObjCount%100);
						GPXObjCount++;
						Info->GotName = TRUE;
					}

					TraceLog("GPXLoad", TRUE, Info->hwnd, "Name:%s trkseg of %ld/%ld Points\n",
							name, c, Info->TrkCount-t);

					size_t Remaining = 256 + strlen(name)+strlen(ML);
					char *Buffer = (char*)malloc(Remaining);
					char *Next = Buffer;
					TCHAR *LatLon = APRSCompressLatLon(lat, lon,
										'/', '/',
										FALSE, 0, 0, FALSE, 0);

					StringCbPrintfExA(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
							"%s>%s:;%-9.9s%c111111z",	/* Object format */
							CALLSIGN, DESTID,
							Info->name, FALSE?'_':'*');

					StringCbPrintfExA(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
										"%S%s }j1%s", LatLon, TieName, ML);

					TraceLogThread("GPXLoad", TRUE, "%s", Buffer);

					free(LatLon);
					if (Info->GotName && Info->name)
					{	free(Info->name);
						Info->GotName = FALSE;
					}

					SendMessage(Info->hwnd, Info->msg, Info->wp, (LPARAM) Buffer);

				} else
				{	TraceLogThread("GPXLoad", TRUE, "CoordTrackToVariableMultiLine(%ld/%ld) c=%ld Failed!\n", (long) t, (long) Info->TrkCount, c);
					MessageBox(Info->hwnd, TEXT("MultiLine Format Failed, Track Too Long?"), TEXT("GPX Track"), MB_ICONERROR | MB_OK);
				}

				free(ML);
			}
			for (t=0; t<Info->TrkCount; t++)
				free(Info->TrkPts[t].pCoord);
			free(Info->TrkPts);
			Info->TrkSize = Info->TrkCount = 0;
			Info->TrkPts = NULL;
			free(TieName);
#else
		if (Info->TrkCount)
		{	int t;

			if (!Info->GotName)
			{	Info->name = (char*)malloc(3+33+1+33+1);
				sprintf(Info->name, "GPX%03ld%c%02ld",
					GPXFileCount,
					(char) (GPXObjCount<100?'-':
								GPXObjCount<1000?'0'+GPXObjCount/100:
									'A'+(GPXObjCount-1000)/100),
					GPXObjCount%100);
				GPXObjCount++;
				Info->GotName = TRUE;
			}
			void *Obj = CreateOverlayObject(Info->pOver, Info->name,
											!strcmp(Info->Stack[Info->Depth].Name,"rte")?'R':'T',
											Info->TrkPts[0].pCoord->lat, 
											Info->TrkPts[0].pCoord->lon, 
											Info->TrkPts[0].alt, 
											0, "");

//#define MAX_POINTS 15
			for (t=0; t<Info->TrkCount; t++)
			{	MoveOverlayObject(Obj,
									Info->TrkPts[t].pCoord->lat,
									Info->TrkPts[t].pCoord->lon,
									Info->TrkPts[t].alt, t==0);
			}
			for (t=0; t<Info->TrkCount; t++)
				free(Info->TrkPts[t].pCoord);
			free(Info->TrkPts);
			Info->TrkSize = Info->TrkCount = 0;
			Info->TrkPts = NULL;
			if (Info->GotName && Info->name)
			{	free(Info->name);
				Info->name = NULL;
				Info->GotName = FALSE;
			}
#endif
		}
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
	Info->Stack[Info->Depth].Name = NULL;

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

void FreeGPXFile(HWND hwnd)
{
}

unsigned long LoadGPXFile(HWND hwnd, OVERLAY_CONFIG_INFO_S *pOver/*, UINT msg, WPARAM wp*/)
{	BOOL Result = FALSE;
	if (pOver && pOver->FileName[0])
	{	FILE *In;
		In = fopen(pOver->FileName,"rt");
		if (!In) return FALSE;
		PARSE_INFO_S *Info;
		XML_Parser parser = XML_ParserCreate(NULL);
		Info = (PARSE_INFO_S *) calloc(1,sizeof(*Info));
		Info->Parser = parser;
		Info->hwnd = hwnd;
//		Info->msg = msg;
//		Info->wp = wp;
		Info->pOver = pOver;
		fseek(In, 0, SEEK_END);
		Info->FileSize = ftell(In);
		fseek(In, 0, SEEK_SET);
		XML_SetUserData(parser, Info);
		XML_SetElementHandler(parser, startElement, endElement);
		XML_SetCdataSectionHandler(parser, startCdata, endCdata);
		XML_SetCharacterDataHandler(parser, characterData);

		size_t ReadBytes;
		BOOL First = TRUE;
	static	char InBuf[4096];

	GPXObjCount = 0;
	GPXSegCount = 0;
	GPXFileCount++;

	Result = TRUE;
	while (Result
	&& !ferror(In)
	&& (ReadBytes = fread(InBuf, sizeof(*InBuf), sizeof(InBuf), In)))
	{	if (First)	/* Check for Unicode headers */
		{	if ((InBuf[0]&0xff) == 0xef && (InBuf[1]&0xff) == 0xbb && (InBuf[2]&0xff) == 0xbf)	/* UTF-8 */
			{	ReadBytes -= 3;
				memmove(&InBuf[0], &InBuf[3], ReadBytes);	/* Move the data down */
			} else if (!isprint(InBuf[0]&0xff))
			{	Result = FALSE;
				break;	/* Invalid GPX file, probably other Unicode */
			}
			First = FALSE;
		}
		Result = XML_Parse(parser, InBuf, ReadBytes, FALSE);
	}
	if (Result) Result = XML_Parse(parser, NULL, 0, TRUE);
	if (!Result)
	{static TCHAR Buffer[256];
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("XML_Parse(%S) FAILED!  %S at line %d\n"),
					pOver->FileName,
					XML_ErrorString(XML_GetErrorCode(parser)),
					XML_GetCurrentLineNumber(parser));
		MessageBox(hwnd, Buffer, TEXT("GPXLoad"), MB_OK);
		TraceActivityEnd(hwnd, "XML_Parse(%s) FAILED!  %s at line %d\n",
					pOver->FileName,
					XML_ErrorString(XML_GetErrorCode(parser)),
					XML_GetCurrentLineNumber(parser));
	} else
	{	TraceActivityEnd(hwnd, "Loaded GPX File\n");
	}
		if (Info->hwndProgress)
		{	UpdateProgress(Info);
			DestroyWindow(Info->hwndProgress);
		}
		XML_ParserFree(parser);
#ifdef UNNECESSARY
		for (int i=0; i<Info->MaxDepth; i++)
			if (Info->Stack[i].ValueB) 
				free(Info->Stack[i].ValueB);
		if (Info->Stack) free(Info->Stack);
		free(Info);
#endif
		if (In) fclose(In);
	}
	return Result;
}

int LoadPOSOverlay(HWND hwnd, OVERLAY_CONFIG_INFO_S *pOver, BOOL Active)
{	int Count = 0;
	FILE *In = fopen(pOver->FileName,"rt");
	if (!In) return Count;
	char *InBuf = (char*)malloc(1024);
	if (fgets(InBuf, 1024, In))	/* Ignore the first line */
	while (fgets(InBuf, 1024, In))
	if (*InBuf != '*')	/* Ignore comments */
	{	char *p, *q;
		for (p=InBuf+strlen(InBuf)-1; p>=InBuf; p--)
		if (*p == '\n' || *p == '\r') *p=0;
		else break;
		p = strchr(InBuf,'!');
		q = strchr(InBuf,'>');
		if (q && (!p || q < p)) p = q;
		if (p && p>InBuf && (p-InBuf) <= 9)
		{	if (*p == '!')	/* We only handle definitions for now */
			{	size_t Remaining = 80+strlen(InBuf);
				char *Buffer = (char*)malloc(Remaining);
				char *Next = Buffer;
				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
//					"%s>%s:)%.*s%c%s",	/* Item format */
					"%s>%s:;%-9.*s%c111111z%s",	/* Object format (allows _) */
					CALLSIGN, DESTID,
					(int) (p-InBuf), InBuf,
//					!Active?'_':'!',	/* Kill the inactive ones */
					!Active?'_':'*',	/* Kill the inactive ones */
					p+1);
#ifdef OLD_WAY
				AprsLogInternalPacket("Overlay", NULL, Buffer, TRUE);
#else
				APRS_PARSED_INFO_S APRS;
				if (parse_full_aprs(Buffer, &APRS))
				{	CreateOverlayObject(pOver, APRS.objCall, 'P',
										APRS.lat, APRS.lon, (long) APRS.alt,
										APRS.symbol, APRS.Comment);
				} else TraceLogThread("Overlays", TRUE, "Error Parsing %s\n", Buffer);
				free(Buffer);
#endif
				Count++;
			} else if (*p == '>')
			{	*p++ = '\0';	/* Null terminate the station ID */
#ifdef OLD_WAY
				STATION_INFO_S *Station = FindStationCall(InBuf, CALLSIGN);
				if (Station)
				{	UTF8Save(&Station->sStatusReport, &Station->pStatusReport, p HERE);
				} else TraceLogThread("Overlays", TRUE, "Failed to Find Object(%s)\n", InBuf);
#else
				SetOverlayObjectStatus(pOver, InBuf, p);
#endif
			} else TraceLogThread("Overlays", *InBuf, "Not ! or > in (%s)\n", InBuf);
		} else TraceLogThread("Overlays", *InBuf, "No ! or > in (%s)\n", InBuf);
	} else TraceLogThread("Overlays", FALSE, "Comment(%s)\n", InBuf);
	fclose(In);
	return Count;
}

