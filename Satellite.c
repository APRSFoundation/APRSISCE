#define TRACE_CRITICAL FALSE

#include "sysheads.h"

#ifdef OBSOLETE
#ifdef _DEBUG
#include <crtdbg.h>
#endif

#include <windows.h>
#include <strsafe.h>
#endif	/* OBSOLETE */

#include "resource.h"

#include "config.h"
#include "llutil.h"
#include "msgbox.h"
#include "osmutil.h"
#include "parsedef.h"	/* For parse.h */
#include "parse.h"	/* For SymbolInt */
#include "tracelog.h"

#include "../gpredict-1.3/src/mypredict.h"
#include "../gpredict-1.3/src/time-tools.h"
#include "../gpredict-1.3/src/predict-tools.h"
#include "../gpredict-1.3/src/sgpsdp/sgp4sdp4.h"

extern HINSTANCE g_hInstance;
HICON MakeSymbolIcon(HWND hwnd, int Symbol);
void CreateNotifyIcon(HWND hwnd, TCHAR *Tip, UINT id, UINT Msg, HICON hIcon);
void PopupNotifyIcon(HWND hwnd, char *Title, char *Text, UINT id);
void DestroyNotifyIcon(HWND hwnd, UINT id);
BOOL TransmitString(char *Buffer);

//	From gtk-sat-map.c
/** \brief Arccosine implementation. 
 *
 * Returns a value between zero and two pi.
 * Borrowed from gsat 0.9 by Xavier Crehueras, EB3CZS.
 * Optimized by Alexandru Csete.
 */
static gdouble
arccos (gdouble x, gdouble y)
{
    if (x && y) {
        if (y > 0.0)
            return acos (x/y);
        else if (y < 0.0)
            return pi + acos (x/y);
    }

    return 0.0;
}
//	From gtk-sat-map.c
/** \brief Mirror the footprint longitude. */
static gboolean
mirror_lon (sat_t *sat, gdouble rangelon, gdouble *mlon)
{
    gdouble diff;
    gboolean warped = FALSE;

    if (sat->ssplon < 0.0) {
        /* western longitude */
        if (rangelon < 0.0) {
            /* rangelon has not been warped over */
            *mlon = sat->ssplon + fabs (rangelon - sat->ssplon);
        }
        else {
            /* rangelon has been warped over */
            diff = 360.0 + sat->ssplon - rangelon;
            *mlon = sat->ssplon + diff;
            warped = TRUE;
        }
    }
    else {
        /* eastern longitude */
        *mlon = sat->ssplon + fabs (rangelon - sat->ssplon);
        
        if (*mlon > 180.0) {
            *mlon -= 360;
            warped = TRUE;
        }
    }

    return warped;
}

//	From gtk-sat-map.c
/** \brief Calculate satellite footprint and coverage area.
 *  \param satmap TheGtkSatMap widget.
 *  \param sat The satellite.
 *  \param points1 Initialised GooCanvasPoints structure with 360 points.
 *  \param points2 Initialised GooCanvasPoints structure with 360 points.
 *  \return The number of range circle parts.
 *
 * This function calculates the "left" side of the range circle and mirrors
 * the points in longitude to create the "right side of the range circle, too.
 * In order to be able to use the footprint points to create a set of subsequent
 * lines conencted to each other (poly-lines) the function may have to perform
 * one of the following three actions:
 *
 * 1. If the footprint covers the North or South pole, we need to sort the points
 *    and add two extra points: One to begin the range circle (e.g. -180,90) and
 *    one to end the range circle (e.g. 180,90). This is necessary to create a
 *    complete and consistent set of points suitable for a polyline. The addition
 *    of the extra points is done by the sort_points function.
 *
 * 2. Else if parts of the range circle is on one side of the map, while parts of
 *    it is on the right side of the map, i.e. the range circle runs off the border
 *    of the map, it calls the split_points function to split the points into two
 *    complete and consistent sets of points that are suitable to create two 
 *    poly-lines.
 *
 * 3. Else nothing needs to be done since the points are already suitable for
 *    a polyline.
 *
 * The function will re-initialise points1 and points2 according to its needs. The
 * total number of points will always be 360, even with the addition of the two
 * extra points. 
 */
static char *
calculate_footprint (char *Name, char *What, sat_t *sat, size_t *pCount, TRACK_INFO_S **pTracks, int Steps, COORDINATE_S **pCoords)
{	char *Result = NULL;
	if (sat->footprint != sat->footprint)
	{	*pCount = 0;
		*pTracks = NULL;
		*pCoords = NULL;
		TraceLogThread("Footprint", TRUE, "%s(%s) NAN!  foot:%lf\n", Name, What, (double) sat->footprint);
		Result = _strdup("");
	} else
	{
	char *f, *p;
    guint azi;
//    gfloat sx, sy, msx, msy, ssx, ssy;
//	gdouble mlon;
    gdouble ssplat, ssplon, beta, azimuth, num, dem;
    gdouble rangelon, rangelat;
    gboolean warped = FALSE;
//    guint numrc = 1;

	p = Result = (char*)malloc(5+(Steps+1)*8+2+1);

	*pCount = 0;
	*pTracks = (TRACK_INFO_S*)calloc(Steps+1,sizeof(**pTracks));
	*pCoords = (COORDINATE_S*)calloc(Steps+1,sizeof(**pCoords));

    /* Range circle calculations.
     * Borrowed from gsat 0.9.0 by Xavier Crehueras, EB3CZS
     * who borrowed from John Magliacane, KD2BD.
     * Optimized by Alexandru Csete and William J Beksi.
     */
    ssplat = sat->ssplat * de2ra;
    ssplon = sat->ssplon * de2ra;
    beta = (0.5 * sat->footprint) / xkmper;

	TraceLogThread("Footprint", TRUE, "%s(%s) foot:%lf beta:%lf\n", Name, What, (double) sat->footprint, (double) beta);

	strcpy(p," }h1");	/* Start of multiline object */
						/* e=yellowdash, h=bluedash j=greensolid */
	p += strlen(p);		/* Advance to coordinates */
	f = p;	/* First point */

//    for (azi = 0; azi < 180; azi++)    {
    for (azi = 0; azi < 360; azi+=360/Steps)    {
        azimuth = de2ra * (double)azi;
        rangelat = asin (sin (ssplat) * cos (beta) + cos (azimuth) *
                         sin (beta) * cos (ssplat));
        num = cos (beta) - (sin (ssplat) * sin (rangelat));
        dem = cos (ssplat) * cos (rangelat);
            
        if (azi == 0 && (beta > pio2 - ssplat))
            rangelon = ssplon + pi;
            
        else if (azi == 180 && (beta > pio2 + ssplat))
            rangelon = ssplon + pi;
                
        else if (fabs (num / dem) > 1.0)
            rangelon = ssplon;
                
        else {
            if ((180 - azi) >= 0)
                rangelon = ssplon - arccos (num, dem);
            else
                rangelon = ssplon + arccos (num, dem);
        }
                
        while (rangelon < -pi)
            rangelon += twopi;
        
        while (rangelon > (pi))
            rangelon -= twopi;
                
        rangelat = rangelat / de2ra;
        rangelon = rangelon / de2ra;

//TraceLogThread(Name, FALSE, "%s[%ld] is %S\n", What, (long) azi,
//			   APRSLatLon(rangelat, rangelon, ' ', ' ', 2, 0, NULL));

		MakeBase91((long)(380926*(90-rangelat)), 4,p);
		(*pTracks)[*pCount].pCoord = &(*pCoords)[*pCount];
		(*pTracks)[*pCount].pCoord->lat = rangelat;
		if (azi < 180)
		{	MakeBase91((long)(190463*(180+rangelon)), 4,p+4);
			(*pTracks)[*pCount].pCoord->lon = rangelon;
		} else
		{	gdouble mlon;
			mirror_lon (sat, rangelon, &mlon);
			MakeBase91((long)(190463*(180+mlon)), 4,p+4);
			(*pTracks)[*pCount].pCoord->lon = mlon;
		}
		p += 8;
		*pCount += 1;

#ifdef NOT_ME
		/* mirror longitude */
        if (mirror_lon (sat, rangelon, &mlon))
            warped = TRUE;

		lonlat_to_xy (satmap, rangelon, rangelat, &sx, &sy);
        lonlat_to_xy (satmap, mlon, rangelat, &msx, &msy);

        points1->coords[2*azi] = sx;
        points1->coords[2*azi+1] = sy;
    
        /* Add mirrored point */
        points1->coords[718-2*azi] = msx;
        points1->coords[719-2*azi] = msy;
#endif
    }

//	if (Close)	// Always close it!
	{	memcpy(p, f, 8);	/* Copy starting point to end */
		p += 8;
		if (pCount && pTracks)
		{	(*pTracks)[*pCount] = (*pTracks)[0];
			*pCount += 1;
		}
	}
	*p++ = '{';
	*p = Steps+1;
	if (*p > 9) *p++ += 'A'-10;	/* Base 36 */
	else *p++ += '0';	/* Digit */
	*p++ = '\0';

    /* points1 ow contains 360 pairs of map-based XY coordinates.
       Check whether actions 1, 2 or 3 have to be performed.
    */
#ifdef MAYBE_FUTURE
    /* pole is covered => sort points1 and add additional points */
    if (pole_is_covered (sat)) {

        sort_points_x (satmap, sat, points1, 360);
        numrc = 1;

    }

    /* pole not covered but range circle has been warped
       => split points */
    else if (warped == TRUE) {

        lonlat_to_xy (satmap, sat->ssplon, sat->ssplat, &ssx, &ssy);
        split_points (satmap, sat, ssx);
        numrc = 2;

    }

    /* the nominal condition => points1 is adequate */
    else {

        numrc = 1;

    }
#endif
	}

//    return numrc;
	return Result;
}

static char *MakeSatCleanName(char *name)
{	size_t i;
	char *Result = _strdup(name);
	for (i=0; i<strlen(Result); i++)
		if (!isalnum(Result[i]&0xff))
			memmove(&Result[i], &Result[i+1], strlen(&Result[i]));
	return Result;
}

static char *MakeSatShortName(char *sat_name)
{	char *b, *e, *Result = _strdup(sat_name);

	if (!strncmp(Result,"ISS ",4))
	{	Result[3] = '\0';
		return Result;
	}

	if (strlen(Result)<=9) return Result;
	b = strchr(sat_name,'(');
	if (b)
	{	e = strchr(b,')');
		if (e && e > b+1)
		{	Result = strncpy(Result, b+1, e-b-1);
			Result[e-b-1] = '\0';
		}
	}
	if (strlen(Result)<=9) return Result;
	Result = RtStrnTrim(-1,Result);
	if (strlen(Result)<=9) return Result;
	for (b=Result; *b; b++)
	{	if (isspace(*b&0xff))
		{	memmove(b,b+1,strlen(b));
			b--;
		}
	}
	if (strlen(Result)<=9) return Result;
	TraceLogThread("Satellite", TRUE, "SatName(%s) Still Too long At(%s)\n",
					sat_name, Result);
	return Result;
}

static char *MakeSatObjName(long catnr, char *sat_name)
{
	if (catnr == 25544)	/* ISS (ZARYA) */
	{	return _strdup("ISS");
	}
	if (catnr == 40654)	/* PSAT (NO-84) per n8hm@arrl.net on amsat-bb on 4/12/2016 */
	{	return _strdup("PSAT");
	}

	if (catnr <= 999999)
	{	char *Result = (char*)malloc(10);
		if (catnr > 99999)
			StringCbPrintfA(Result, 10, "SAT%ld", (long) catnr);
		else StringCbPrintfA(Result, 10, "SAT-%ld", (long) catnr);
		return Result;
	}
	return MakeSatShortName(sat_name);
}

static char *MakeSatObjName2(long catnr, char *sat_name)
{
	if (catnr <= 999999)
	{	char *Result = (char*)malloc(10);
		if (catnr > 99999)
			StringCbPrintfA(Result, 10, "SAT%ld", (long) catnr);
		else StringCbPrintfA(Result, 10, "SAT_%ld", (long) catnr);
		return Result;
	}
	return MakeSatShortName(sat_name);
}

int cdecl IsStationFollowed(char *StationID);

/* From Steve at http://www.amsat.org/amsat-new/satellites/status.php */
static long ActiveSats[] = { 
/*SRM-A*/		37838,
/*SRM-B-SRMVU*/	37839,
/*SRM-C*/		37840,
/*SRM-D*/		37841,
/*SRM-E-JUGNU*/	37842,
/*ARRISSat-1*/	37772,
/*FASTRAC 2*/	37380,
/*FASTRAC 1*/	37227,
/*O/OREOS*/		37224,
/*HO-68*/		36122,
/*ITUpSAT1*/	35935,
/*BEESAT*/		35933,
/*SwissCube*/	35932,
/*SO-67*/		35870,
/*KKS-1*/		33499,
/*STARS*/		33498,
/*PRISM*/		33493,
/*Rs-30*/		32953,
/*CO-66*/		32791,
/*DO-64*/		32789,
/*Compass-1*/	32787,
/*Co-65*/		32785,
/*GENESAT-1*/	29655,
/*CO-58*/		28895,
/*VO-52*/		28650,
/*AO-51*/		28375,
/*RS-22*/		27939,
/*CO-57*/		27848,
/*CO-55*/		27844,
/*SAUDISAT 1C*/	27607,
/*NO-44*/		26931,
/*SAUDISAT 1B*/	26549,
/*SAUDISAT 1A*/	26545,
/*ARISS*/		25544,
/*SO-33*/		25509,
/*GO-32*/		25397,
/*FO-29*/		24278,
/*RS-15*/		23439,
/*IO-26*/		22826,
/*AO-27*/		22825,
/*AO-16*/		20439,
/*UO-11*/		14781,
/*AO-7*/		7530
};
static int ActiveSatCount = sizeof(ActiveSats)/sizeof(ActiveSats[0]);

struct
{	char *Name;
	double lat;
	double lon;
	long alt;
	HWND hwnd;
	UINT msg;
	WPARAM wp;
} QTHInfo;

typedef enum ILLUMINATION_T
{	ILL_UNKNOWN = ' ',
	ILL_VISIBLE = 'V',
	ILL_DAYLIGHT = 'L',
	ILL_DARK = 'N'
} ILLUMINATION_T;

typedef struct PASS_INFO_S
{	BOOL Valid;
	BOOL Visible;
	char Delta[16];
	struct
	{	gdouble t;
		gdouble az;
		gdouble el;
		ILLUMINATION_T Lit;
	} AOS, MaxEl, LOS, Now;
} PASS_INFO_S;

typedef struct SAT_INFO_S
{	sat_t sat;
	char *ShortName;
	char *ObjName;
	char *ObjName2;
	char *CleanName;	/* AlphaNumeric name */
	BOOL SatChanged;	/* True if TLE was updated */

	PASS_INFO_S Pass;

	BOOL WasVisible;	/* Tracks last Visibility state */
	struct tm tmOS;
	gdouble OS;
	gdouble MaxEl;
	gdouble MaxT;
	char PassTime[16];	/* Duration of next pass if !WasVisible */
	gdouble NextXmit;
	HWND hwndCheck;
	int CheckState;	/* BST_INDETERMINATE, BST_CHECKED, BST_UNCHECKED */
	int CheckIndex;
	int LastState;	/* For change detection */
	struct
	{	int Count;	/* Count of PassInfo queries */
		char LastFor[16];	/* Who last */
		SYSTEMTIME stLast;	/* And When */
//		BOOL NeedPosition;	/* TRUE if a position should be sent */
	} Query;
} SAT_INFO_S;

static qth_t qth = {0};	/* Observer position */

static	int InfoCount = 0;
static	SAT_INFO_S *Info = NULL;
HWND hwndSatellites = NULL;
static RECT rcSize = {0};
static BOOL WakeupSleeper = FALSE;

static HANDLE hmtxInfo = NULL;
static BOOL LockInfo(DWORD msTimeout)
{
	if (!hmtxInfo)
	{	hmtxInfo = CreateMutex(NULL, FALSE, NULL);
	}
	return WaitForSingleObject(hmtxInfo, msTimeout) == WAIT_OBJECT_0;
}

static void UnlockInfo(void)
{
	ReleaseMutex(hmtxInfo);
}

int GetInfoIndex(char *Name)
{	int s;
	int n = 0;

	if (!_strnicmp(Name,"SAT",3))
	{	char *e;
		n = strtol(Name+3,&e,10);
		if (*e) n = 0;
		else if (n < 0) n = -n;
	}

	for (s=0; s<InfoCount; s++)
	{	if ((n && Info[s].sat.tle.catnr == n)
		|| !_stricmp(Info[s].CleanName, Name)
		|| !_stricmp(Info[s].ObjName, Name)
		|| !_stricmp(Info[s].ObjName2, Name)
		|| !_stricmp(Info[s].ShortName, Name))
		{	return s;
		}
	}
	return -1;
}

BOOL IsSatelliteName(char *Name)
{	int s;
	if (!LockInfo(1000)) return FALSE;
	s = GetInfoIndex(Name);
	UnlockInfo();
	return s != -1;
}

static BOOL GetPassInfo(sat_t *sat, qth_t *qth, gdouble t, PASS_INFO_S *Info)
{
	predict_calc (sat, qth, t);
	memset(Info, 0, sizeof(*Info));
	Info->Now.t = t;
	Info->Now.az = sat->az;
	Info->Now.el = sat->el;
//	Info->Now.Lit = ????;
	Info->Visible = sat->el > 0.0;
	if (!Info->Visible)
	{	Info->AOS.t = find_aos(sat, qth, t, 2.0);	/* Look 2 days out */
	} else
	{	Info->AOS.t = find_prev_aos(sat, qth, t);	/* Find start of current pass */
	}
	if (Info->AOS.t)	/* Do we have a pass? */
	{	Info->AOS.az = sat->az;
		Info->AOS.el = sat->el;
//		Info->AOS.Lit = ????;
		Info->LOS.t = find_los(sat, qth, Info->AOS.t, 2.0);	/* up to 2 day passes */
		if (Info->LOS.t)
		{	__int64 PassTime = (__int64)((Info->LOS.t-Info->AOS.t)*24*60*60);
			if (PassTime > 0)
			{	FormatDeltaTime((__int64)PassTime,
								Info->Delta,
								sizeof(Info->Delta), NULL, NULL);
			}
			Info->LOS.az = sat->az;
			Info->LOS.el = sat->el;
//			Info->LOS.Lit = ????;
			Info->MaxEl.el = find_max_el(sat, qth, Info->AOS.t, Info->LOS.t, 0, &Info->MaxEl.t);
			if (Info->MaxEl.el)
			{	Info->MaxEl.az = sat->az;
//				Info->MaxEl.Lit = ????;
			}
		}
	}
	Info->Valid = (Info->AOS.t != 0.0 && Info->LOS.t != 0.0 && Info->MaxEl.t != 0.0);
	return Info->Valid;
}

char *FormatPassInfoString(PASS_INFO_S *Info)
{	double now = get_current_daynum();
	size_t Remaining = 80;
	char *Buffer = (char*)malloc(Remaining);
	char *Next = Buffer;

	if (!Info->Visible)	/* Not Visible */
	{	if (Info->AOS.t)
		{	char Temp[80];
			StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"AOS in %s",
					FormatDeltaTime((__int64)((Info->AOS.t-now)*24*60*60),
									Temp, sizeof(Temp), NULL, NULL));
			if (Info->Delta[0])	/* Got a pass duration? */
			{	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS,
						" for %s", Info->Delta);
				if (Info->MaxEl.el)
					StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
							STRSAFE_IGNORE_NULLS,
							" MaxEl %ld", (long) (Info->MaxEl.el+0.5));
			}
		} else StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"No AOS within 2 days");
	} else	/* It's visible! */
	{	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
			STRSAFE_IGNORE_NULLS,
			"Az %ld El %ld",
			(long) (Info->Now.az+0.5), (long) (Info->Now.el+0.5));
		if (Info->LOS.t)
		{	char Temp[80];
			if (Info->MaxEl.el)
				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS,
						"%c of %ld",
						Info->MaxEl.t < now ? '-' : '+',
						(long) (Info->MaxEl.el+0.5));
			StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				" LOS in %s",
				FormatDeltaTime((__int64)((Info->LOS.t-now)*24*60*60),
								Temp, sizeof(Temp), NULL, NULL));
		} else StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				" NO LOS!");
	}
	return Buffer;
}

char *FormatPassInfoString2(PASS_INFO_S *Info)
{	double now = get_current_daynum();
	size_t Remaining = 80;
	char *Buffer = (char*)malloc(Remaining);
	char *Next = Buffer;

	if (!Info->Visible)	/* Not Visible */
	{	if (Info->AOS.t)
		{	char Temp[80];
			__int64 DeltaT = (__int64)((Info->AOS.t-now)*24*60*60);

			StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"AOS %s",
					FormatDeltaTime(DeltaT, Temp, sizeof(Temp), NULL, NULL));
			if (DeltaT > 60*60)
			{	struct tm tmAOS;
				time_t tNow = time(NULL);
				struct tm tmNow = *gmtime(&tNow);
				Date_Time(Info->AOS.t, &tmAOS);
				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS, " (");
				if (tmNow.tm_mday != tmAOS.tm_mday)
					StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
							STRSAFE_IGNORE_NULLS,
							"%02ld ", (long) tmAOS.tm_mday);
				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS,
						"%02ld%02ldz)",
						(long) tmAOS.tm_hour,
						(long) tmAOS.tm_min);
				if (Info->MaxEl.el)
					StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
								STRSAFE_IGNORE_NULLS,
								" %s^%ld",
								GetCompassPoint8((long)Info->MaxEl.az),
								(long) (Info->MaxEl.el+0.5));
			} else
			{	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS,
						" %s", GetCompassPoint8((long)Info->AOS.az));
				if (Info->MaxEl.t)
					StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
								STRSAFE_IGNORE_NULLS,
								" %s^%ld",
								GetCompassPoint8((long)Info->MaxEl.az),
								(long) (Info->MaxEl.el+0.5));
				if (Info->LOS.t)	/* Got a pass duration? */
				{	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
							STRSAFE_IGNORE_NULLS,
							" %s",
							GetCompassPoint8((long)Info->LOS.az));
					DeltaT = (__int64)((Info->LOS.t-Info->AOS.t)*24*60*60);
					if (DeltaT > 0)
						StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
								STRSAFE_IGNORE_NULLS,
								" %ldm", (long) (DeltaT+30)/60);
				}
			}
		} else StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"No AOS within 2 days");
	} else	/* It's visible! */
	{	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
			STRSAFE_IGNORE_NULLS,
			"%S^%ld",
			GetCompassPoint((long)Info->Now.az),
			(long) (Info->Now.el+0.5));
		if (Info->LOS.t)
		{	char Temp[80];
			if (Info->MaxEl.el && Info->MaxEl.t > now)
				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS,
						" %S^%ld",
						GetCompassPoint((long)Info->MaxEl.az),
						(long) (Info->MaxEl.el+0.5));
			StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				" %S LOS %s",
				GetCompassPoint((long)Info->LOS.az),
				FormatDeltaTime((__int64)((Info->LOS.t-now)*24*60*60),
								Temp, sizeof(Temp), NULL, NULL));
		} else StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				" NO LOS!");
	}
	return Buffer;
}

char *GetPassString(char *Name, char *For, double lat, double lon, long alt)
{	int s;
	qth_t Myqth = {0};	/* Observer position */
	sat_t Mysat;
	double now = get_current_daynum();
	char *sat_name;
	PASS_INFO_S Pass;
	char *Result = NULL;

	if (!LockInfo(1000))
	{	return _strdup("Internal Error");
	}
	s = GetInfoIndex(Name);
	if (s == -1)
	{	UnlockInfo();
		return _strdup("Unknown Satellite");
	}

	Myqth.name = For;
	Myqth.loc = For;
	Myqth.desc = For;
	Myqth.lat = lat;
	Myqth.lon = lon;
	Myqth.alt = alt;

	Info[s].Query.Count++;	/* Count the reference */
	strncpy(Info[s].Query.LastFor, For, sizeof(Info[s].Query.LastFor));
	Info[s].Query.LastFor[sizeof(Info[s].Query.LastFor)-1] = '\0';
	GetSystemTime(&Info[s].Query.stLast);

	/* Bring this one into the dialog from now on */
	if (Info[s].CheckState == BST_UNCHECKED) Info[s].CheckState = BST_INDETERMINATE;

	Mysat = Info[s].sat;
	sat_name = Mysat.name?Mysat.name:Mysat.nickname?Mysat.nickname:Mysat.tle.sat_name;

	predict_calc (&Mysat, &Myqth, 0);	/* Initialize the sat */
	GetPassInfo(&Mysat, &Myqth, now, &Pass);
	Result = FormatPassInfoString2(&Pass);
	UnlockInfo();
	return Result;

#ifdef OBSOLETE
	size_t Remaining = 80;
	char *Buffer = (char*)malloc(Remaining);
	char *Next = Buffer;
	struct tm cnow;

	predict_calc (&Mysat, &Myqth, now);	/* Fly the bird to current time */
	Date_Time(now, &cnow);

	TraceLogThread(sat_name, TRUE, "%s %s %s%04ld-%02ld-%02ld %02ld:%02ld:%02ld Az:%.2lf El:%.2lf Range:%.2lf Lat:%.4lf Lon:%.4lf Alt:%.1lf Foot:%.1lf Vel:%.4lf\n",
			sat_name, For, Mysat.el>=0.0?"VISIBLE ":"", 
			cnow.tm_year+1900, cnow.tm_mon+1, cnow.tm_mday,
			cnow.tm_hour, cnow.tm_min, cnow.tm_sec,
				Mysat.az, Mysat.el, Mysat.range,
				Mysat.ssplat, Mysat.ssplon, Mysat.alt,
				Mysat.footprint, Mysat.velo);

	if (Mysat.el <= 0.0)	/* Not Visible */
	{	gdouble AOS = find_aos(&Mysat, &Myqth, now, 2.0);	/* See 2 days response below */
		char cPassTime[16] = {0};
		gdouble maxEl = 0.0;
		gdouble maxT = 0.0;
		if (AOS)
		{	gdouble LOS = find_los(&Mysat, &Myqth, AOS, 2.0);
			if (LOS)
			{	__int64 PassTime = (__int64)((LOS-AOS)*24*60*60);
				if (PassTime > 0)
				{	FormatDeltaTime((__int64)PassTime,
									cPassTime,
									sizeof(cPassTime), NULL, NULL);
					maxEl = find_max_el (&Mysat, &Myqth, AOS, LOS, 0, &maxT);
				}
			}
		} else TraceLogThread(sat_name, TRUE, "find_aos: FAILED!\n");

		if (AOS)
		{	char Temp[80];
			StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"AOS in %s",
					FormatDeltaTime((__int64)((AOS-now)*24*60*60), Temp, sizeof(Temp), NULL, NULL));
			if (cPassTime[0])	/* Got a pass duration? */
			{	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS,
						" for %s", cPassTime);
				if (maxEl)
					StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
							STRSAFE_IGNORE_NULLS,
							" MaxEl %ld", (long) (maxEl+0.5));
			}
		} else StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"No AOS within 2 days");

	} else	/* It's visible! */
	{	gdouble LOS = find_los(&Mysat, &Myqth, now, 2.0);
		gdouble maxEl = 0.0;
		gdouble maxT = 0.0;

		if (LOS)
		{	maxEl = find_max_el (&Mysat, &Myqth, now, LOS, 0, &maxT);
		} else TraceLogThread(sat_name, TRUE, "find_los: FAILED!\n");

		predict_calc (&Mysat, &Myqth, now);	/* Put it back to now */

		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
			STRSAFE_IGNORE_NULLS,
			"Az %ld El %ld",
			(long) (Mysat.az+0.5), (long) (Mysat.el+0.5));
		if (LOS)
		{	char Temp[80];
			if (maxEl)
				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS,
						"%c of %ld",
						maxT < now ? '-' : '+',
						(long) (maxEl+0.5));
			StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				" LOS in %s",
				FormatDeltaTime((__int64)((LOS-now)*24*60*60), Temp, sizeof(Temp), NULL, NULL));
		}
		else StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				" NO LOS!");
	}
	UnlockInfo();
	return Buffer;
#endif
}

BOOL TLERunning = FALSE;
BOOL TLEExit = FALSE;
static	BOOL HideUnchecked = FALSE;

void SaveSatellites(void)
{	int i;
	for (i=0; i<InfoCount; i++)
	{	AddOrUpdateTimedStringEntry(&ActiveConfig.Satellites, Info[i].ObjName, NULL, Info[i].CheckState);
	}
	ActiveConfig.SatelliteCount = TLERunning?InfoCount:0;
	ActiveConfig.SatelliteHide = HideUnchecked;
	if (hwndSatellites) DestroyWindow(hwndSatellites);

	if (!IsRectEmpty(&rcSize))
	{	StringCbPrintfA(ActiveConfig.SatellitePos, sizeof(ActiveConfig.SatellitePos),
							"@%ld,%ld[%ld,%ld]",
							rcSize.left, rcSize.top,
							rcSize.right-rcSize.left,
							rcSize.bottom-rcSize.top);
	}
}

static void PopulateSatelliteWindowPos(RECT *prc)
{	char *e = ActiveConfig.SatellitePos;
	SetRect(prc, CW_USEDEFAULT, CW_USEDEFAULT,
					CW_USEDEFAULT, CW_USEDEFAULT);
	if (e && *e=='@')
	{	RECT rc;
		rc.left = strtol(e+1,&e,10);
		if (e && *e==',')
		{	rc.top = strtol(e+1,&e,10);
			if (e && *e=='[')
			{	rc.right = rc.left + strtol(e+1,&e,10);
				if (e && *e==',')
				{	rc.bottom = rc.top + strtol(e+1,&e,10);
					if (e && *e==']')
					{	ClipOrCenterRectToMonitor(&rc,
									MONITOR_CLIP | MONITOR_WORKAREA);
						SetRect(prc, rc.left, rc.top,
									rc.right-rc.left,
									rc.bottom-rc.top);
					}
				}
			}
		}
	}
}

static int CompareSatInfo(const void *One, const void *Two)
{const SAT_INFO_S *Left = One;
const SAT_INFO_S *Right = Two;

	if (Left->WasVisible && !Right->WasVisible)
		return -1;
	else if (!Left->WasVisible && Right->WasVisible)
		return 1;
	else if (Left->OS < Right->OS)
		return -1;
	else if (Left->OS > Right->OS)
		return 1;
	else return strcmp(Left->ShortName, Right->ShortName);
}

static void SortSatInfo(void)
{	if (InfoCount > 1) qsort(Info, InfoCount, sizeof(*Info), CompareSatInfo);
}

static BOOL RunTLESatellite(char *Name, double lat, double lon, long alt)
{
	int First = TRUE;

	qth.name = Name;
	qth.loc = Name;
	qth.desc = Name;
	qth.lat = lat;
	qth.lon = lon;
	qth.alt = alt;

	while (!TLEExit)	/* Infinite loop? */
	if (LockInfo(1000))
	{	double now = get_current_daynum();
		struct tm cnow;
		int s;
		DWORD msSleep;
		BOOL EventVis = FALSE;
		char *EventObj = "NONE";
		char *EventBird = "NONE";
		ULONG32 SecTillXmit = MAXULONG32;
		ULONG32 SecTillEvent = MAXULONG32;

		for (s=0; s<InfoCount; s++)
		if (!HideUnchecked || Info[s].CheckState || Info[s].Query.Count)
		{	char *sat_name = Info[s].sat.name?Info[s].sat.name:Info[s].sat.nickname?Info[s].sat.nickname:Info[s].sat.tle.sat_name;

			predict_calc (&Info[s].sat, &qth, now);
			Date_Time(now, &cnow);

#ifdef NOT_ANY_MORE
			TraceLogThread("Satellite", First || Info[s].sat.el>=0.0, "%s %s%04ld-%02ld-%02ld %02ld:%02ld:%02ld Az:%.2lf El:%.2lf Range:%.2lf Lat:%.4lf Lon:%.4lf Alt:%.1lf Foot:%.1lf Vel:%.4lf\n",
			sat_name, Info[s].sat.el>=0.0?"VISIBLE ":"", 
			cnow.tm_year+1900, cnow.tm_mon+1, cnow.tm_mday,
			cnow.tm_hour, cnow.tm_min, cnow.tm_sec,
				Info[s].sat.az, Info[s].sat.el, Info[s].sat.range,
				Info[s].sat.ssplat, Info[s].sat.ssplon, Info[s].sat.alt,
				Info[s].sat.footprint, Info[s].sat.velo);
#endif

			TraceLogThread(sat_name, First || Info[s].sat.el>=0.0, "%s %s%04ld-%02ld-%02ld %02ld:%02ld:%02ld Az:%.2lf El:%.2lf Range:%.2lf Lat:%.4lf Lon:%.4lf Alt:%.1lf Foot:%.1lf Vel:%.4lf\n",
			sat_name, Info[s].sat.el>=0.0?"VISIBLE ":"", 
			cnow.tm_year+1900, cnow.tm_mon+1, cnow.tm_mday,
			cnow.tm_hour, cnow.tm_min, cnow.tm_sec,
				Info[s].sat.az, Info[s].sat.el, Info[s].sat.range,
				Info[s].sat.ssplat, Info[s].sat.ssplon, Info[s].sat.alt,
				Info[s].sat.footprint, Info[s].sat.velo);

#ifdef VERBOSE
	if (Info[s].NextXmit && Info[s].OS)
	if ((Info[s].NextXmit <= now
	|| Info[s].OS <= Info[s].NextXmit))
	{	struct tm tmNext, tmNow, tmOS;
		Date_Time(Info[s].NextXmit, &tmNext);
		Date_Time(now, &tmNow);
		Date_Time(Info[s].OS, &tmOS);

		TraceLogThread("Satellite", TRUE, "%s %s OS:%02ld-%02ld %02ld:%02ld:%02ld Now:%02ld:%02ld:%02ld Xmit:%02ld:%02ld:%02ld\n",
						Info[s].Name,
						((Info[s].NextXmit <= now
						|| Info[s].OS <= Info[s].NextXmit))?"XMIT":"no",
						tmOS.tm_mon+1, tmOS.tm_mday,
						tmOS.tm_hour, tmOS.tm_min, tmOS.tm_sec,
						tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec,
						tmNext.tm_hour, tmNext.tm_min, tmNext.tm_sec);
	}
#endif

{	SYMBOL_INFO_S Symbol = { '\\', 'S' };	/* Satellite */
	BOOL Compressed = TRUE;
	long Precision = 2;
	char *RFPath = "";

	SYSTEMTIME stObj;
	int XmitSecs = 10*60;	/* Default to every 10 minutes */
	BOOL Changed = Info[s].SatChanged;
	BOOL Visible = (Info[s].sat.el>0.0);

	if (Info[s].SatChanged	/* Define the Satellite TACTICAL */
	&& strcmp(Info[s].ObjName, Info[s].ShortName))
	{	char *Message = (char*)malloc(256);
		StringCbPrintfA(Message, 256,
				"%s>%s%s%s::%-9.9s:%s=%s",	/* TACTICAL Message format */
				Name, "APZTLE",
				*RFPath?",":"", RFPath,
				"TACTICAL",
				Info[s].ObjName, Info[s].ShortName);
		TraceLogThread("Satellite", FALSE, "%s", Message);

		if (!PostMessage(QTHInfo.hwnd, QTHInfo.msg, QTHInfo.wp, (LPARAM) Message))
		{
		}
	}

	if (Info[s].SatChanged)
		Info[s].WasVisible = !Visible;	/* Force update */
	else if (Info[s].OS < now)	/* Missed it? */
	{	char Temp[16];
		TraceLogThread("Satellite", TRUE, "%s %s Negative[%s]",
				sat_name, Info[s].WasVisible?"LOS":"AOS",
				FormatDeltaTime((__int64)((Info[s].OS-now)*24*60*60), Temp, sizeof(Temp), NULL, NULL));
		Info[s].WasVisible = !Visible;	/* Force an update */
	}
	if (Info[s].WasVisible != Visible)
	{	GetPassInfo(&Info[s].sat, &qth, now, &Info[s].Pass);
	} else
	{	Info[s].Pass.Now.t = now;
		Info[s].Pass.Now.az = Info[s].sat.az;
		Info[s].Pass.Now.el = Info[s].sat.el;
//		Info[s].Pass.Now.Lit = ????;
	}

	if (Info[s].WasVisible && !Visible)
	{	Changed = TRUE;
		Info[s].OS = find_aos(&Info[s].sat, &qth, now, 2.0);
		if (Info[s].OS)
		{	gdouble LOS;
			char Temp[80];
			Date_Time(Info[s].OS, &Info[s].tmOS);
			TraceLogThread(sat_name, TRUE, "AOS:%04ld-%02ld-%02ld %02ld:%02ld:%02ldz\n",
				(long) Info[s].tmOS.tm_year+1900,
				(long) Info[s].tmOS.tm_mon+1,
				(long) Info[s].tmOS.tm_mday,
				(long) Info[s].tmOS.tm_hour,
				(long) Info[s].tmOS.tm_min,
				(long) Info[s].tmOS.tm_sec,
				FormatDeltaTime((__int64)((Info[s].OS-now)*24*60*60), Temp, sizeof(Temp), NULL, NULL));
			//calculate_footprint (sat_name, "AOS", &Info[s].sat);

			LOS = find_los(&Info[s].sat, &qth, Info[s].OS, 2.0);
			if (LOS)
			{	__int64 PassTime = (__int64)((LOS-Info[s].OS)*24*60*60);
				if (PassTime > 0)
				{	FormatDeltaTime((__int64)PassTime,
									Info[s].PassTime,
									sizeof(Info[s].PassTime), NULL, NULL);
					Info[s].MaxEl = find_max_el (&Info[s].sat, &qth, Info[s].OS, LOS, 0, &Info[s].MaxT);
				} else
				{	char *Message = malloc(128);
					memset(&Info[s].PassTime, 0, sizeof(Info[s].PassTime));
TraceLogThread(sat_name, TRUE, "%s Passtime[0](%ld) FAILED\n", sat_name, (long) PassTime);
					StringCbPrintfA(Message, 128, "%s Passtime[0](%ld) FAILED\n", sat_name, (long) PassTime);
					TraceLogThread(sat_name, TRUE, "%s", Message);
//					QueueInternalMessage(Message, TRUE);
					Info[s].MaxEl = 0.0;	/* No pass duration, no MaxEl */
				}
				//calculate_footprint (sat_name, "ALS", &Info[s].sat);
			} else
			{	char *Message = malloc(128);
				memset(&Info[s].PassTime, 0, sizeof(Info[s].PassTime));
TraceLogThread(sat_name, TRUE, "%s LOS[0](%lf) FAILED\n", sat_name, (double) LOS);
				StringCbPrintfA(Message, 128, "%s LOS[0](%lf) FAILED\n", sat_name, (double) LOS);
				TraceLogThread(sat_name, TRUE, "%s", Message);
//				QueueInternalMessage(Message, TRUE);
				Info[s].MaxEl = 0.0;	/* No LOS, no MaxEl */
			}
		} else TraceLogThread(sat_name, TRUE, "find_aos: FAILED!\n");
		Info[s].WasVisible = Visible;
		predict_calc (&Info[s].sat, &qth, now);	/* Need to go back to now */
	} else if (!Info[s].WasVisible && Visible)
	{	Changed = TRUE;
		Info[s].OS = find_los(&Info[s].sat, &qth, now, 2.0);
		if (Info[s].OS)
		{	char Temp[80], Temp2[80];
			SYSTEMTIME st;

			Info[s].MaxEl = find_max_el (&Info[s].sat, &qth, now, Info[s].OS, 0, &Info[s].MaxT);

			Date_Time(Info[s].OS, &Info[s].tmOS);
			TraceLogThread(sat_name, TRUE, "LOS:%04ld-%02ld-%02ld %02ld:%02ld:%02ldz (+%s)\n",
				(long) Info[s].tmOS.tm_year+1900,
				(long) Info[s].tmOS.tm_mon+1,
				(long) Info[s].tmOS.tm_mday,
				(long) Info[s].tmOS.tm_hour,
				(long) Info[s].tmOS.tm_min,
				(long) Info[s].tmOS.tm_sec,
				FormatDeltaTime((__int64)((Info[s].OS-now)*24*60*60), Temp, sizeof(Temp), NULL, NULL));
				//calculate_footprint (sat_name, "LOS", &Info[s].sat);
			GetLocalTime(&st);
			StringCbPrintfA(Temp2, sizeof(Temp2), "AOS:%02ld-%02ld %02ld:%02ld:%02ld LOS in %s MaxEl %ld\n",
				(long) st.wMonth, (long) st.wDay,
				(long) st.wHour, (long) st.wMinute, (long) st.wSecond,
				FormatDeltaTime((__int64)((Info[s].OS-now)*24*60*60), Temp, sizeof(Temp), NULL, NULL),
				(long) Info[s].MaxEl);
			PopupNotifyIcon(hwndSatellites, Info[s].ShortName, Temp2, 3);
		} else
		{	TraceLogThread(sat_name, TRUE, "find_los: FAILED!\n");
			Info[s].MaxEl = 0.0;	/* No LOS, no MaxEl */
		}
		Info[s].WasVisible = Visible;
		predict_calc (&Info[s].sat, &qth, now);	/* Need to go back to now */
	}

	else if (!Info[s].WasVisible && !Info[s].PassTime[0])	/* Need PassTime? */
	{	// Changed = TRUE;	/* We may or may not want this */
		gdouble AOS = find_aos(&Info[s].sat, &qth, now, 2.0);
		if (AOS)
		{	gdouble LOS;
			char Temp[80];
			struct tm tmOS;
			Date_Time(AOS, &tmOS);
			TraceLogThread(sat_name, TRUE, "AOS[*]:%04ld-%02ld-%02ld %02ld:%02ld:%02ldz\n",
				(long) tmOS.tm_year+1900,
				(long) tmOS.tm_mon+1,
				(long) tmOS.tm_mday,
				(long) tmOS.tm_hour,
				(long) tmOS.tm_min,
				(long) tmOS.tm_sec,
				FormatDeltaTime((__int64)((AOS-now)*24*60*60), Temp, sizeof(Temp), NULL, NULL));
	
			LOS = find_los(&Info[s].sat, &qth, AOS, 2.0);
			if (LOS)
			{	__int64 PassTime = (__int64)((LOS-AOS)*24*60*60);
				if (PassTime > 0)
				{	char *Message = malloc(128);
					FormatDeltaTime((__int64)PassTime,
									Info[s].PassTime,
									sizeof(Info[s].PassTime), NULL, NULL);
					Info[s].MaxEl = find_max_el (&Info[s].sat, &qth, Info[s].OS, LOS, 0, &Info[s].MaxT);
	TraceLogThread(sat_name, TRUE, "%s Recovered PassTime(%s)\n", sat_name, Info[s].PassTime);
					StringCbPrintfA(Message, 128, "%s Recovered PassTime(%s)", sat_name, Info[s].PassTime);
					QueueInternalMessage(Message, TRUE);
				} else
				{	memset(&Info[s].PassTime, 0, sizeof(Info[s].PassTime));
	TraceLogThread(sat_name, TRUE, "%s Passtime[*](%ld) FAILED\n", sat_name, (long) PassTime);
					Info[s].MaxEl = 0.0;	/* No pass duration, no MaxEl */
				}
				//calculate_footprint (sat_name, "ALS", &Info[s].sat);
			} else
			{	memset(&Info[s].PassTime, 0, sizeof(Info[s].PassTime));
	TraceLogThread(sat_name, TRUE, "%s LOS[*](%lf) FAILED\n", sat_name, (double) LOS);
				Info[s].MaxEl = 0.0;	/* No LOS, no MaxEl */
			}
		}
		GetPassInfo(&Info[s].sat, &qth, now, &Info[s].Pass);
		predict_calc (&Info[s].sat, &qth, now);	/* Need to go back to now */
	}

	if (Info[s].OS && Info[s].OS > now)
	{	ULONG32 SecondsLeft = (ULONG32)((Info[s].OS-now)*24*60*60);
		XmitSecs = SecondsLeft / 4;	/* Default to this if we actually transmit */
		if (SecondsLeft < SecTillEvent)
		{	SecTillEvent = SecondsLeft;
			EventVis = Visible;
			EventObj = Info[s].ObjName;
			EventBird = Info[s].ShortName;
		}
	}

	if (Info[s].CheckState != Info[s].LastState)	/* Changed also */
		Changed = TRUE;

	if (Info[s].CheckState != BST_UNCHECKED	/* Not if disabled */
	|| Info[s].CheckState != Info[s].LastState)	/* But always if changed */
	if (Changed	/* These always do it */
	|| Info[s].CheckState == BST_CHECKED	/* always these */
	|| Visible	/* Visibles always do it */
//	|| Info[s].sat.tle.catnr == 25544	/* ISS (ZARYA) */
	|| Info[s].OS <= now+10.0/60.0/24.0	/* Or AOS within 10 minutes */
	|| IsStationFollowed(Info[s].ObjName))
	{
	if (Changed
//	|| Info[s].CheckState == BST_CHECKED	/* always these */
	|| Info[s].NextXmit <= now+1.0/60.0/60.0/24.0	/* within 1 second? */
	|| Info[s].OS <= Info[s].NextXmit)
	{
		size_t TrackCount=0;
		TRACK_INFO_S *Tracks = NULL;
		COORDINATE_S *Coords = NULL;
		char *MultiLine = calculate_footprint (sat_name, "Orbit", &Info[s].sat, &TrackCount, &Tracks, 18, &Coords);
		size_t BuffSize = 256 + 4+strlen(sat_name)+strlen(MultiLine)+1;
		char *DAO=NULL, *Buffer = (char*)malloc(BuffSize);
		TCHAR *LatLon;
		size_t Remaining = BuffSize;
		char *Next = Buffer;
		
		Info[s].LastState = Info[s].CheckState;	/* Remember current state */

		if (Changed && !Info[s].SatChanged)
			XmitSecs = 1;	/* Force one extra transmission */

		DAO = NULL;
		if (Visible) Symbol.Table = 'V';	/* Visible */
		LatLon = Compressed /*&& !isdigit(Symbol.Table)*/
						?	APRSCompressLatLon(Info[s].sat.ssplat, Info[s].sat.ssplon,
											Symbol.Table, Symbol.Symbol,
											FALSE, 0, 0, FALSE, 0)
						:	APRSLatLon(Info[s].sat.ssplat, Info[s].sat.ssplon,
											Symbol.Table, Symbol.Symbol,
											0, Precision, &DAO);

		GetSystemTime(&stObj);

		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				"%s>%s%s%s:;%-9.9s%c",	/* Object format */
				Name, "APZTLE",
				*RFPath?",":"", RFPath,
				Info[s].ObjName,
				!Visible&&Info[s].CheckState!=BST_CHECKED?'_':'*');	/* Kill invisible */

		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				"%02ld%02ld%02ldh%S",
				(long) stObj.wHour, 
				(long) stObj.wMinute, 
				(long) stObj.wSecond,
				LatLon);

		/* And finally the payload */
		if (Visible)	/* Visible gets Azimuth & Elevation */
		{	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				"Az %ld El %ld",
				(long) (Info[s].sat.az+0.5), (long) (Info[s].sat.el+0.5));
			if (Info[s].OS)
			{	char Temp[80];
				if (Info[s].MaxEl)
					StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
							STRSAFE_IGNORE_NULLS,
							"%c / %ld",
							Info[s].MaxT < now ? '-' : '+',
							(long) (Info[s].MaxEl+0.5));
				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					" LOS %s",
					FormatDeltaTime((__int64)((Info[s].OS-now)*24*60*60), Temp, sizeof(Temp), NULL, NULL));
			}
			else StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					" NO LOS!");
		} else
		{	if (Info[s].OS)
			{	char Temp[80];
				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS,
						"AOS %s",
						FormatDeltaTime((__int64)((Info[s].OS-now)*24*60*60), Temp, sizeof(Temp), NULL, NULL));
				if (Info[s].PassTime[0])	/* Got a pass duration? */
				{
					StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
							STRSAFE_IGNORE_NULLS,
							"+%s", Info[s].PassTime);
					if (Info[s].MaxEl)
						StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
								STRSAFE_IGNORE_NULLS,
								" MaxEl %ld", (long) (Info[s].MaxEl+0.5));
				}
			} else StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS,
						"No AOS within 2 Days");
		}

		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"%s", MultiLine);

		if (!Compressed && DAO && *DAO)
			StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"%s", DAO);

		free(LatLon);
		if (DAO) free(DAO);
		free(MultiLine);

		TraceLogThread("Satellite", FALSE, "%s", Buffer);

		if (!PostMessage(QTHInfo.hwnd, QTHInfo.msg, QTHInfo.wp, (LPARAM) Buffer))
		{
		}
/*
	Send another copy in with a "traditional" MultiLine
*/
		Remaining = BuffSize;
		Next = Buffer = (char*)malloc(BuffSize);

		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				"%s>%s%s%s:;%-9.9s%c",	/* Object format */
				Name, "APZTLE",
				*RFPath?",":"", RFPath,
				Info[s].ObjName2,
				!Visible&&Info[s].CheckState!=BST_CHECKED?'_':'*');	/* Kill invisible */

		LatLon = APRSLatLon(Info[s].sat.ssplat, Info[s].sat.ssplon,
											Symbol.Table, Symbol.Symbol,
											0, 2, &DAO);

		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				"%02ld%02ld%02ldh%S",
				(long) stObj.wHour, 
				(long) stObj.wMinute, 
				(long) stObj.wSecond,
				LatLon);

		/* And finally the payload */
		if (Visible)	/* Visible gets Azimuth & Elevation */
		{	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				"Az %ld El %ld",
				(long) (Info[s].sat.az+0.5), (long) (Info[s].sat.el+0.5));
			if (Info[s].OS)
			{	char Temp[80];
				if (Info[s].MaxEl)
					StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
							STRSAFE_IGNORE_NULLS,
							"%c / %ld",
							Info[s].MaxT < now ? '-' : '+',
							(long) (Info[s].MaxEl+0.5));
				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					" LOS %s",
					FormatDeltaTime((__int64)((Info[s].OS-now)*24*60*60), Temp, sizeof(Temp), NULL, NULL));
			}
			else StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					" NO LOS!");
		} else
		{	if (Info[s].OS)
			{	char Temp[80];
				StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS,
						"AOS %s",
						FormatDeltaTime((__int64)((Info[s].OS-now)*24*60*60), Temp, sizeof(Temp), NULL, NULL));
				if (Info[s].PassTime[0])	/* Got a pass duration? */
				{
					StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
							STRSAFE_IGNORE_NULLS,
							"+%s", Info[s].PassTime);
					if (Info[s].MaxEl)
						StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
								STRSAFE_IGNORE_NULLS,
								" MaxEl %ld", (long) (Info[s].MaxEl+0.5));
				}
			} else StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
						STRSAFE_IGNORE_NULLS,
						"No AOS within 2 Days");
		}

		MultiLine = CoordTrackToMultiLine(Info[s].ObjName2,
										Info[s].sat.ssplat,
										Info[s].sat.ssplon,
										TrackCount, Tracks,
										TrackCount*2+8, TRUE);
		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					" }k1%s", MultiLine);
		free(MultiLine);

		if (DAO && *DAO)
			StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"%s", DAO);
		else
		{	TraceLogThread("Satellite", TRUE, "DAO is %p\n", DAO);
			StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"!W00!");	/* Need something here! */
		}
		free(LatLon);
		if (DAO) free(DAO);

		TraceLogThread("Satellite", FALSE, "%s", Buffer);

		if (!PostMessage(QTHInfo.hwnd, QTHInfo.msg, QTHInfo.wp, (LPARAM) Buffer))
		{
		}
		free(Tracks);
		free(Coords);
/*
	And now, KJ4ERJ-15 inject ISS into the APRS-IS
*/
		if ((Info[s].sat.tle.catnr == 25544	/* ISS only */
		 || Info[s].sat.tle.catnr == 40654)	/* PSAT aka NO-84 */
		&& !strcmp(CALLSIGN, "KJ4ERJ-15"))
		{
		free(calculate_footprint (sat_name, "Orbit", &Info[s].sat, &TrackCount, &Tracks, 10, &Coords));	/* 8, 10, 12, 15 works as does 18 but too long */
		Remaining = BuffSize;
		Next = Buffer = (char*)malloc(BuffSize);

		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				"%s>%s,TCPIP*:;%-9.9s*",	/* Object format */
				Name, "APZTLE",
				Info[s].ObjName);	/* Should be ISS */

		LatLon = APRSLatLon(Info[s].sat.ssplat, Info[s].sat.ssplon,
											'\\', 'S',
											0, 2, &DAO);

		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				"%02ld%02ld%02ldh%S",
				(long) stObj.wHour, 
				(long) stObj.wMinute, 
				(long) stObj.wSecond,
				LatLon);

		/* And finally the payload */
		{	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				"Msg4Pass");
		}

		MultiLine = CoordTrackToMultiLine(Info[s].ShortName,
										Info[s].sat.ssplat,
										Info[s].sat.ssplon,
										TrackCount, Tracks,
										TrackCount*2+8, TRUE);
		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					" }k1%s", MultiLine);
		free(MultiLine);

#define USE_DAO
#ifdef USE_DAO
		if (DAO && *DAO)
			StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"%s", DAO);
		else
		{	TraceLogThread("Satellite", TRUE, "DAO is %p\n", DAO);
			StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
					STRSAFE_IGNORE_NULLS,
					"!W00!");	/* Need something here! */
		}
#else
		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, 
				STRSAFE_IGNORE_NULLS,
				"#####");	/* Non-AlphaNumerics */
#endif
#undef USE_DAO
		free(LatLon);
		if (DAO) free(DAO);
		free(Tracks);
		free(Coords);

		TraceLogThread("ISS2-IS", TRUE, "%s", Buffer);
		TransmitString(Buffer);
		XmitSecs = 2*60;	/* Only every 2 minutes to APRS-IS */
		}

		if (Visible && XmitSecs > 60)
			XmitSecs = 60;	/* Default to 1 minute while visible */
		else if (XmitSecs > 2*60)	/* No event? */
		{	if (First)			/* Space out the initial transmissions */
				XmitSecs = min(s*2+2,2*60);
			else XmitSecs = 2*60;	/* Maximum transmission interval */
		}
		Info[s].NextXmit = now + XmitSecs/60.0/60.0/24.0;
	}
		{	ULONG32 SecondsLeft = (ULONG32)((Info[s].NextXmit-now)*24*60*60);
			if (SecondsLeft < SecTillXmit)
				SecTillXmit = SecondsLeft;
		}
	}
}
			Info[s].SatChanged = FALSE;	/* Got it taken care of */
		}

		{	char Temp[16];
		static char *LastBird = NULL;
			FormatDeltaTime(SecTillEvent, Temp, sizeof(Temp), NULL, NULL),
			SecTillEvent /= 4;	/* Speed up at 4 minute (4*60/4=60) away */
								/* At 4+ minutes, every minute */
								/* At 3 minutes, 45 seconds */
								/* At 2 minutes, 30 seconds */
								/* At 1 minute, 15 seconds */
								/* At 32 seconds, every 8 seconds */
								/* At 16 seconds, every 4 seconds */
								/* At 4 and closer, every second */
			//if (SecTillXmit < SecTillEvent)	/* Xmit needed sooner? */
			//	SecTillEvent = SecTillXmit;	/* Wake up sooner */
			//msSleep = max(1,min(SecTillEvent,60)) *1000;
			msSleep = max(1,min(SecTillXmit,60)) *1000;
			TraceLogThread("Satellite", First || msSleep<15000 || LastBird!=EventBird,
							"%s (%s) %s Till %s, Sleeping %ldsec (Ev:%ld vs Xm:%ld)\n",
							EventObj, EventBird, Temp,
							EventVis?"LOS":"AOS",
							(long) msSleep/1000,
							(long) SecTillEvent, (long) SecTillXmit);
			LastBird = EventBird;
		}
		SortSatInfo();
		UnlockInfo();

		if (hwndSatellites) InvalidateRect(hwndSatellites, NULL, FALSE);
		while (msSleep > 0 && !WakeupSleeper)
		{	Sleep(500);
			msSleep -= 500;
		}
		WakeupSleeper = FALSE;
		First = FALSE;
	} else TraceLogThread("Satellite", TRUE, "LockInfo(!TLEExit) FAILED!");

	return TRUE;
}

#ifdef UNDER_CE
DWORD RunTLEThread(LPVOID pvParam)
#else
DWORD WINAPI RunTLEThread(LPVOID pvParam)
#endif
{
	SetDefaultTraceName("Satellite");
	SetTraceThreadName("RunTLEs");
	TLEExit = FALSE;
	TraceLogThread("Satellite", TRUE, "Calling RunTLESatellite\n");
	RunTLESatellite(QTHInfo.Name, QTHInfo.lat, QTHInfo.lon, QTHInfo.alt);
	TraceLogThread("Satellite", TRUE, "RunTLESatellite Exited!\n");
	TLERunning = FALSE;
	return 0;
}

static int ParseTLEs(char *TLEs, BOOL FilterActives)
{	int c, l, t, TLen;
	int ParsedCount = 0;
	char   tle_str[3][80] = {0};

	if (!TLEs || !*TLEs) return FALSE;
	TLen = strlen(TLEs);

	TraceLogThread("Satellite", TRUE, "Parsing %ld Bytes of TLE data\n", (long) TLen);

	if (!LockInfo(1000))
	{	TraceLogThread("Satellite", TRUE, "LockInfo(ParseTLEs) FAILED!");
		return FALSE;
	}

#ifdef OBSOLETE
	if (ClearFirst)
	{	for (c=0; c<InfoCount; c++)
			if (Info[c].hwndCheck)
				DestroyWindow(Info[c].hwndCheck);
		InfoCount = 0;	/* Clear out the old ones */
	}
#endif
	for (t=0; t<TLen; )
	{	memset(tle_str, 0, sizeof(tle_str));
		for (l=0; t<TLen && l<3; l++)
		{	for (c=0; t<TLen && c<80; c++, t++)
			{	tle_str[l][c] = TLEs[t];
				if (TLEs[t] == '\n' || TLEs[t] == '\r')
				{	tle_str[l][++c] = '\0';
					break;
				}
			}
			while (t<TLen && (TLEs[t] == '\n' || TLEs[t] == '\r')) t++;
		}
		for (l=0; l<3; l++)
			TraceLogThread("Satellite", TRUE, "[%d] %s\n", l, tle_str[l]);

		{	int s = InfoCount++;
			Info = (SAT_INFO_S *)realloc(Info,sizeof(*Info)*InfoCount);
			memset(&Info[s], 0, sizeof(Info[s]));
			
			if (Get_Next_Tle_Set (tle_str, &Info[s].sat.tle) == 1)
			{	char *sat_name = Info[s].sat.name?Info[s].sat.name:Info[s].sat.nickname?Info[s].sat.nickname:Info[s].sat.tle.sat_name;
				int a;

				ParsedCount++;	/* Count the good TLE */
				for (a=0; a<ActiveSatCount; a++)
				{	if (ActiveSats[a] == Info[s].sat.tle.catnr)
						break;
				}
				if (FilterActives && a >= ActiveSatCount)
				{
				TraceLogThread("Satellite", TRUE, "INACTIVE Sats[%ld] cat:%ld is %s\n",
								(long) s, (long) Info[s].sat.tle.catnr,
								sat_name);
				InfoCount--;	/* Drop the last one */
				} else
				{
for (a=0; a<s; a++)
{	if (Info[a].sat.tle.catnr == Info[s].sat.tle.catnr)
	{	
		{	TraceLogThread("Satellite", TRUE, "Updating Sats[%ld] cat:%ld as %s epoch %.4lf with %.4lf\n",
							(long) a, (long) Info[a].sat.tle.catnr, sat_name,
							(double) Info[a].sat.tle.epoch,
							(double) Info[s].sat.tle.epoch);
		if (Info[a].sat.tle.epoch != Info[s].sat.tle.epoch)
		TraceLogThread("Satellite-Load", TRUE, "Updating Sats[%ld] cat:%ld as %s epoch %.4lf with %.4lf\n",
							(long) a, (long) Info[a].sat.tle.catnr, sat_name,
							(double) Info[a].sat.tle.epoch,
							(double) Info[s].sat.tle.epoch);
			Info[a].sat.tle = Info[s].sat.tle;
#ifdef BUSTED
		} else if (Info[a].sat.tle.epoch == Info[s].sat.tle.epoch)
		{	TraceLogThread("Satellite-Load", TRUE, "Sats[%ld] cat:%ld as %s epoch UNCHANGED %.4lf is %.4lf\n",
							(long) a, (long) Info[a].sat.tle.catnr, sat_name,
							(double) Info[a].sat.tle.epoch,
							(double) Info[s].sat.tle.epoch);
		} else 
		{	TraceLogThread("Satellite-Load", TRUE, "Sats[%ld] cat:%ld as %s epoch OLDER %.4lf vs %.4lf\n",
							(long) a, (long) Info[a].sat.tle.catnr, sat_name,
							(double) Info[a].sat.tle.epoch,
							(double) Info[s].sat.tle.epoch);
#endif
		}
		
		InfoCount--;	/* Don't count this one */
		s = a;	/* Reinitialize the original entry */
		sat_name = Info[s].sat.name?Info[s].sat.name:Info[s].sat.nickname?Info[s].sat.nickname:Info[s].sat.tle.sat_name;
	}
}

				Info[s].SatChanged = TRUE;	/* TLE updated */
				Info[s].ShortName = MakeSatShortName(sat_name);
				Info[s].CleanName = MakeSatCleanName(Info[s].ShortName);
				Info[s].ObjName = MakeSatObjName(Info[s].sat.tle.catnr, sat_name);
				Info[s].ObjName2 = MakeSatObjName2(Info[s].sat.tle.catnr, sat_name);
				Info[s].sat.flags = 0;	/* Per gtk_sat_data_read_sat */
				select_ephemeris (&Info[s].sat);

				predict_calc (&Info[s].sat, &qth, 0);	/* Initialize the sat */

				{	unsigned long e = LocateTimedStringEntry(&ActiveConfig.Satellites, Info[s].ObjName, -1);
					if (e != -1)
						Info[s].CheckState = ActiveConfig.Satellites.Entries[e].value;
					else Info[s].CheckState = Info[s].sat.tle.catnr == 25544 ? BST_CHECKED : BST_INDETERMINATE;	/* ISS(ZARYA) */
				}

				TraceLogThread("Satellite", TRUE, "Sats[%ld] cat:%ld is %s Short:%s Obj:%s\n",
								(long) s, (long) Info[s].sat.tle.catnr,
								sat_name, Info[s].ShortName, Info[s].ObjName);
				}
			} else
			{	TraceLogThread("Satellite", TRUE, "Get_NextTle_Set Failed!\n");
				InfoCount--;	/* Drop the last one */
			}
		}
	}
	UnlockInfo();
	if (!TLERunning)
	{	TLERunning = TRUE;
		TraceLogThread("Satellite", TRUE, "Starting RunTLEThread\n");
		CloseHandle(CreateThread(NULL, 0, RunTLEThread, NULL, 0, NULL));
	} else WakeupSleeper = TRUE;	/* Force an update pass */
	return ParsedCount;
}

static int LoadParseURL(HWND hwnd, char *URL, BOOL FilterActives)
{	int ResultCount = 0;
static	char *TLE = "\
ISS                     \n\
1 25544U 98067A   11276.93861111  .00074774  00000-0  88929-3 0  1762\n\
2 25544  51.6431 333.2674 0016393 284.2468  73.2794 15.60299781737889\n\
";
	if (strncmp(URL,"http://",7))
	{	TraceLogThread("Satellite", TRUE, "URLs Must Start With http://");
	} else
	{	char *TLEServer = URL+7;
		char *TLEFile = strchr(TLEServer,'/');
		if (!TLEFile)
		{	TraceLogThread("Satellite", TRUE, "Cannot Locate File In %s\n",
							URL);
		} else if (TLEFile == TLEServer)
		{	TraceLogThread("Satellite", TRUE, "No Server In %s\n", URL);
		} else
		{	int TLen = 0, ServerLen = (TLEFile-TLEServer);
			char *TLEs, *Error;
			TLEServer = _strdup(TLEServer);
			TLEServer[ServerLen] = '\0';	/* Null end server name */
			TLEs = httpGetBuffer(hwnd, TLEServer, 80, TLEFile, &TLen, "Satellite", TRUE, &Error);

			if (TLEs)
			{	TraceLogThread("Satellite", TRUE, "Using TLEs from http://%s%s\n", TLEServer, TLEFile);
				ResultCount = ParseTLEs(TLEs, FilterActives);
				free(TLEs);
			} else TraceLogThread("Satellite", TRUE, "httpGetBuffer(%s) Failed with %s\n", URL, Error?Error:"*NULL*");
			free(TLEServer);
		}
	}
	if (ResultCount)	/* Worked, update the entry */
	{	TIMED_STRING_LIST_S *pList = &ActiveConfig.SatelliteURLs;
		AddOrUpdateTimedStringEntry(pList, URL, NULL, FilterActives);
	} else if (hwnd)	/* Failed, tell them */
	{	size_t MsgLen = sizeof(TCHAR)*(strlen(URL)+80);
		TCHAR *Message = malloc(MsgLen);
		StringCbPrintf(Message, MsgLen, TEXT("No TLEs Loaded From %S"), URL);
		MessageBox(hwnd, Message, TEXT("TLE URL"), MB_OK | MB_ICONERROR);
		free(Message);
	}
	return ResultCount;
}

static char *GetTLEFile(HWND hwnd)
{	char *Return = NULL;
static	OPENFILENAME ofn = {0};
	
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = TEXT("TLE Files\0*.tle\0TXT Files\0*.txt\0All Files\0*.*\0\0");
	ofn.nFilterIndex = 2;	/* Default to TXT */
	ofn.nMaxFile = 256;
	ofn.lpstrFile = (LPWSTR) calloc(ofn.nMaxFile,sizeof(*ofn.lpstrFile));
	ofn.lpstrTitle = TEXT("TLE File");
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	ofn.lpstrDefExt = TEXT("txt");
	
	if (GetOpenFileName(&ofn))
	{	Return = (char*)malloc(ofn.nMaxFile);
		StringCbPrintfA(Return, ofn.nMaxFile, "%.*S", ofn.nMaxFile, ofn.lpstrFile);
	}
	return Return;
}

static int LoadParseFile(HWND hwnd, char *File, BOOL FilterActives)
{	int ResultCount = 0;
	FILE *In = fopen(File,"rt");
static	char InBuf[4096];

	if (!In) return FALSE;
	while (fgets(InBuf, sizeof(InBuf), In))
	{	int InLen = strlen(InBuf);
		if (fgets(InBuf+InLen, sizeof(InBuf)-InLen, In))
		{	InLen = strlen(InBuf);
			if (fgets(InBuf+InLen, sizeof(InBuf)-InLen, In))
			{	ResultCount += ParseTLEs(InBuf, FilterActives);
			}
		}
	}
	fclose(In);
	if (ResultCount)	/* Worked, update the entry */
	{	TIMED_STRING_LIST_S *pList = &ActiveConfig.SatelliteFiles;
		AddOrUpdateTimedStringEntry(pList, File, NULL, FilterActives);
	} else if (hwnd)	/* Failed, tell them */
	{	size_t MsgLen = sizeof(TCHAR)*(strlen(File)+80);
		TCHAR *Message = malloc(MsgLen);
		StringCbPrintf(Message, MsgLen, TEXT("No TLEs Loaded From %S"), File);
		MessageBox(hwnd, Message, TEXT("TLE File"), MB_OK | MB_ICONERROR);
		free(Message);
	}
	return ResultCount;
}

#ifdef OBSOLETE
		&& ActiveConfig.SatelliteCount <= 1
		&& (ActiveConfig.SatelliteCount == 1
		|| MessageBox(hwnd, TEXT("Yes to see LOTS of Satellites\n(Monitor Events In Satellite Trace Log)\n\nNo to see Just the ISS"), TEXT("Satellite Orbits"), MB_ICONQUESTION | MB_YESNO) == IDNO))
		{	TLEs = strstr(TLEs, "ISS (ZARYA)");
			if (TLEs)
			{	char *e = strchr(TLEs,'\n');	/* End of satellite */
				if (e) e = strchr(e+1,'\n');	/* End of 1 line */
				if (e) e = strchr(e+1,'\n');	/* End of 2 line */
				if (e)
				{	e[1] = '\0';
					TraceLogThread("Satellite", TRUE, "Using ISS TLE from http://%s%s\n", TLEServer, TLEFile);
				} else
				{	TLEs = NULL;	/* Use internal TLE */
					TraceLogThread("Satellite", TRUE, "Missing ISS line ends from http://%s%s\n", TLEServer, TLEFile);
				}
			} else TraceLogThread("Satellite", TRUE, "No ISS (ZARYA) from http://%s%s\n", TLEServer, TLEFile);
		} else TraceLogThread("Satellite", TRUE, "Using TLEs from http://%s%s\n", TLEServer, TLEFile);
#endif

static LRESULT CALLBACK SatelliteWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
static int usSysLineHeight, usSysDescender, usSysCharWidth, usSquare;
static int NextCheckID = 32767;	/* Backwards from here */
static	int Widths[8] = {0};
static	int LastCount = 0, LastDid = 0;

	switch (iMsg)
	{
	case WM_CREATE:
	{	CREATESTRUCT *cs = (CREATESTRUCT *) lParam;

		HDC hdc = GetDC(hwnd);
		TEXTMETRIC tm;

		GetTextMetrics(hdc, &tm);
		usSysLineHeight = tm.tmHeight + tm.tmExternalLeading;
		usSysDescender = tm.tmDescent;
		usSysCharWidth = tm.tmAveCharWidth;
		usSquare = max(usSysLineHeight, usSysCharWidth);

		memset(Widths,0,sizeof(Widths));
		LastCount = 0;	/* For Restart */
		LastDid = 0;	/* For Restart */

#ifdef FUTURE
		HFONT hOld = NO_FONT, hFont = GetMessageBoxFont();
		if (hFont != NO_FONT) hOld = (HFONT) SelectObject(hdc, hFont);

		GetTextMetrics(hdc, &tm);
		Info->cxChar = tm.tmAveCharWidth;
		Info->cxCaps = (tm.tmPitchAndFamily & 1 ? 3 : 2) * Info->cxChar / 2;
		Info->cyChar = tm.tmHeight + tm.tmExternalLeading;
		if (hFont != NO_FONT) DeleteObject(SelectObject(hdc, hOld));
		ReleaseDC(hwnd, hdc);
#endif
		SetTimer(hwnd, 11, 5000, NULL);

		{	int SatSym = SymbolInt('\\', 'S');
			HICON hIcon = MakeSymbolIcon(hwnd, SatSym);
			if (hIcon)
			{	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM) hIcon);
				SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
				CreateNotifyIcon(hwnd, TEXT("Satellite Details"), 3, WM_USER+1, hIcon);
			}
		}
		HideUnchecked = ActiveConfig.SatelliteHide;
		PostMessage(hwnd, WM_USER, 0, 0);	/* Defer setting menu text */
		return 0;
	}
	case WM_USER+1:	/* Notification bar notification */
		switch (lParam)
		{
		case NIN_BALLOONUSERCLICK:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
		{	
#ifdef SUPPORT_MINIMIZE
			if (IsIconic(hwnd))
			{	WINDOWPLACEMENT wd;
				wd.length = sizeof(wd);
				GetWindowPlacement(hwnd, &wd);
				switch (cInfo->prevSize)
				{
				case SIZE_MAXIMIZED:	wd.showCmd = SW_MAXIMIZE; break;
				case SIZE_RESTORED:		wd.showCmd = SW_RESTORE; break;
				default:				wd.showCmd = SW_RESTORE; break;
				}
				SetWindowPlacement(hwnd, &wd);
			}
#endif
			ShowWindow(hwnd, SW_RESTORE);
			SetForegroundWindow(hwnd);
			break;
		}
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MOUSEMOVE:
			break;
		}
		break;
	case WM_USER:
	{	HMENU hMenu = GetMenu(hwnd);
		if (hMenu)
		{	MENUITEMINFO mi = {0};
			mi.cbSize = sizeof(mi);
			mi.fMask = MIIM_TYPE;
			mi.fType = MFT_STRING;
			mi.dwTypeData = HideUnchecked?TEXT("Show All"):TEXT("Hide UnChecked");
			mi.cch = 7;
			if (!SetMenuItemInfo(hMenu, IDM_TLE_HIDE, FALSE, &mi))
			{	TraceLog("Menu", TRUE, hwnd, "SetMenuItemInfo(IDM_TLE_HIDE) Failed, Error %ld\n", GetLastError());
			}
			DrawMenuBar(hwnd);
		}
		break;
	}

	case WM_TIMER:
		if (wParam == 10)
			InvalidateRect(hwnd, NULL, FALSE);
		else if (wParam == 11 || wParam == 12)
		{static char *URL1 = "http://www.celestrak.com/NORAD/elements/amateur.txt";
//		static char *URL2 = "http://dinesh.cyanam.net/dl/SRMSAT_TLEs.txt";
		static char *URL2 = "http://dinesh.cyanam.net/dl/SRMSat_Updated_Keps.txt";
			KillTimer(hwnd, 11);	/* Only once! */
			SetTimer(hwnd, 12, 24*60*60*1000L, NULL);	/* Reload daily */
			if (ActiveConfig.SatelliteURLs.Count)
			{	unsigned long u;
				TIMED_STRING_LIST_S *pList = &ActiveConfig.SatelliteURLs;
				unsigned long Count = pList->Count;
				char **URLs = calloc(sizeof(*URLs), Count);
				BOOL *Filters = calloc(sizeof(*Filters), Count);
				for (u=0; u<Count; u++)
				{	URLs[u] = _strdup(pList->Entries[u].string);
					Filters[u] = pList->Entries[u].value;
				}
				for (u=0; u<Count; u++)
				{	int c = LoadParseURL(hwnd, URLs[u], Filters[u]);
					TraceLogThread("Satellite-Load", TRUE, "Loaded %ld From %s\n", (long) c, URLs[u]);
					free(URLs[u]);
				}
				free(Filters);
				free(URLs);
			} else
			{	int c = LoadParseURL(hwnd, URL1, TRUE);
				TraceLogThread("Satellite-Load", TRUE, "Loaded %ld From %s\n", (long) c, URL1);
				c = LoadParseURL(hwnd, URL2, FALSE);
				TraceLogThread("Satellite-Load", TRUE, "Loaded %ld From %s\n", (long) c, URL2);
			}
			if (ActiveConfig.SatelliteFiles.Count)
			{	unsigned long u;
				TIMED_STRING_LIST_S *pList = &ActiveConfig.SatelliteFiles;
				unsigned long Count = pList->Count;
				char **Files = calloc(sizeof(*Files), Count);
				BOOL *Filters = calloc(sizeof(*Filters), Count);
				for (u=0; u<Count; u++)
				{	Files[u] = _strdup(pList->Entries[u].string);
					Filters[u] = pList->Entries[u].value;
				}
				for (u=0; u<Count; u++)
				{	int c = LoadParseFile(hwnd, Files[u], Filters[u]);
					TraceLogThread("Satellite-Load", TRUE, "Loaded %ld From %s\n", (long) c, Files[u]);
					free(Files[u]);
				}
				free(Filters);
				free(Files);
			}
			ShowWindow(hwnd, SW_SHOW);
			SetTimer(hwnd, 10, 1000, NULL);
		}
		break;
	case WM_MOVE:
		GetWindowRect(hwnd, &rcSize);
		break;

	case WM_SIZE:
		GetWindowRect(hwnd, &rcSize);
		break;

#ifdef NOT_NEEDED_YET
	case WM_SIZE:
	{	int newX = LOWORD(lParam);
		int newY = HIWORD(lParam);
static int Nesting=0;
		Nesting++;
		TraceLogThread("Satellite",TRUE,"WM_SIZE[%ld]:%ld x %ld\n", Nesting, newX, newY);
		TraceLogThread("Satellite",TRUE,"WM_SIZED[%ld]:%ld x %ld\n", Nesting, newX, newY);
		Nesting--;
	}
	break;
#endif

	case WM_INITMENUPOPUP:
	{	HMENU hmenu = (HMENU) wParam;
	    int  cMenuItems = GetMenuItemCount(hmenu); 
		UINT id = GetMenuItemID(hmenu, 0);

TraceLog("Menu", FALSE, hwnd, "INITMENUPOPUP(%ld)\n", (long) id);

		if (id == IDM_TLE_FETCH)
		{	unsigned long p;
			TIMED_STRING_LIST_S *pList = &ActiveConfig.SatelliteURLs;
			for (p=cMenuItems-1; p>=2; p--)	/* 2 original elements */
				DeleteMenu(hmenu, p, MF_BYPOSITION);
			for (p=0; p<pList->Count; p++)
			{	size_t NameLen = sizeof(TCHAR)*(strlen(pList->Entries[p].string)+80);
				TCHAR *Name = (TCHAR*)malloc(NameLen);
				StringCbPrintf(Name, NameLen, TEXT("%S"), pList->Entries[p].string);
				AppendMenu(hmenu, MF_STRING, ID_DYNAMIC_BASE+(p+1), Name);
//				EnableMenuItem(hmenu, ID_RFPORTS_ENABLE+(p+1), (ActiveConfig.Enables.RFPorts&&*Port->Device)?MF_ENABLED:MF_GRAYED);
//				CheckMenuItem(hmenu, ID_RFPORTS_ENABLE+(p+1), (Port->IsEnabled?MF_CHECKED:MF_UNCHECKED));
				free(Name);
			}
		}
		if (id == IDM_TLE_FILE)
		{	unsigned long p;
			TIMED_STRING_LIST_S *pList = &ActiveConfig.SatelliteFiles;
			for (p=cMenuItems-1; p>=2; p--)	/* 2 original elements */
				DeleteMenu(hmenu, p, MF_BYPOSITION);
			for (p=0; p<pList->Count; p++)
			{	size_t NameLen = sizeof(TCHAR)*(strlen(pList->Entries[p].string)+80);
				TCHAR *Name = (TCHAR*)malloc(NameLen);
				StringCbPrintf(Name, NameLen, TEXT("%S"), pList->Entries[p].string);
				AppendMenu(hmenu, MF_STRING, ID_DYNAMIC_BASE+200+(p+1), Name);
//				EnableMenuItem(hmenu, ID_RFPORTS_ENABLE+(p+1), (ActiveConfig.Enables.RFPorts&&*Port->Device)?MF_ENABLED:MF_GRAYED);
//				CheckMenuItem(hmenu, ID_RFPORTS_ENABLE+(p+1), (Port->IsEnabled?MF_CHECKED:MF_UNCHECKED));
				free(Name);
			}
		}
		break;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_TLE_PASTE:
			if (IsClipboardFormatAvailable(CF_TEXT))
			{	if (OpenClipboard(hwnd))
				{	char *String = NULL;
					HANDLE hClip = GetClipboardData(CF_TEXT);
					if (hClip)
					{	size_t StringSize = sizeof(*String)*(GlobalSize(hClip)+1);
						String = calloc(sizeof(*String),GlobalSize(hClip)+1);
						if (String)
						{	TCHAR *pClip = GlobalLock(hClip);
							if (pClip)
							{	StringCbPrintfA(String, StringSize, "%s", pClip);
							}
							GlobalUnlock(hClip);
						}
					}
					CloseClipboard();
					if (String && *String)
					{	if (!ParseTLEs(String, FALSE))
							MessageBox(hwnd, TEXT("No TLEs Parsed"), TEXT("Paste TLE"), MB_OK | MB_ICONERROR);
					} else MessageBox(hwnd, TEXT("No TEXT Received From Clipboard"), TEXT("Paste TLE"), MB_OK | MB_ICONERROR);
					if (String) free(String);
				}
			} else MessageBox(hwnd, TEXT("No TEXT In Clipboard"), TEXT("Paste TLE"), MB_OK | MB_ICONERROR);
			break;
		case IDM_TLE_FETCH:
		{	char URL[256] = {0};
			char *New = StringPromptA(hwnd, "Satellite TLE URL", "Enter TLE URL (CaSe SeNsItIvE)", sizeof(URL)-1, URL, FALSE, FALSE);
			if (New)
			{	if (!LoadParseURL(hwnd, New, FALSE))
					MessageBox(hwnd, TEXT("No TLEs Loaded From URL"), TEXT("Fetch TLEs"), MB_OK | MB_ICONERROR);
				free(New);
			}
			break;
		}
		case IDM_TLE_FILE:
		{	char *File = GetTLEFile(hwnd);
			if (File)
			{	if (!LoadParseFile(hwnd, File, FALSE))
					MessageBox(hwnd, TEXT("No TLEs Loaded From File"), TEXT("File TLEs"), MB_OK | MB_ICONERROR);
				free(File);
			}
			break;
		}
		case IDM_TLE_HIDE:
		{	int i;
			HideUnchecked = !HideUnchecked;
			SendMessage(hwnd, WM_USER, 0, 0);
			for (i=0; i<InfoCount; i++)
			{	Info[i].CheckIndex = -1;	/* Force checkbox move */
#ifdef FUTURE
				if (Info[i].hwndCheck)
					EnableWindow(Info[i].hwndCheck, !HideUnchecked);
#endif
			}
			if (!HideUnchecked) WakeupSleeper = TRUE;	/* Get them running */
			InvalidateRect(hwnd, NULL, FALSE);
//			UpdateWindow(hwnd);
			break;
		}
		default:
			if (LOWORD(wParam) > ID_DYNAMIC_BASE
			&& LOWORD(wParam) <= ID_DYNAMIC_BASE+ActiveConfig.SatelliteURLs.Count)
			{	unsigned long p = LOWORD(wParam)-ID_DYNAMIC_BASE-1;
				TIMED_STRING_LIST_S *pList = &ActiveConfig.SatelliteURLs;
				BUTTONS_S *Buttons = CreateButtons(-1);
				size_t NameLen = sizeof(TCHAR)*(strlen(pList->Entries[p].string)+80);
				TCHAR *Name = (TCHAR*)malloc(NameLen);
				StringCbPrintf(Name, NameLen, TEXT("%S\n\nLoaded: %04ld-%02ld-%02ld %02ld:%02ld:%02ldz"),
						pList->Entries[p].string,
						(long) pList->Entries[p].time.wYear,
						(long) pList->Entries[p].time.wMonth,
						(long) pList->Entries[p].time.wDay,
						(long) pList->Entries[p].time.wHour,
						(long) pList->Entries[p].time.wMinute,
						(long) pList->Entries[p].time.wSecond);
				AddButton(Buttons, "Reload", IDYES, FALSE);
				AddButton(Buttons, "Delete", IDRETRY, FALSE);
				AddButton(Buttons, "Cancel", IDABORT, TRUE);
				switch (LwdMessageBox2(hwnd, Name, TEXT("Fetch TLE"), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON3, Buttons, NULL))
				{
				case IDYES:
					if (!LoadParseURL(hwnd, pList->Entries[p].string, pList->Entries[p].value))
						MessageBox(hwnd, TEXT("No TLEs Loaded From URL"), TEXT("Fetch TLE"), MB_OK | MB_ICONERROR);
					break;
				case IDRETRY:
					StringCbPrintf(Name, NameLen, TEXT("Really Delete\n\n%S"), pList->Entries[p].string);
					if (MessageBox(hwnd, Name, TEXT("Delete TLE URL"),
									MB_YESNO | MB_ICONQUESTION) == IDYES)
					{	RemoveTimedStringEntry(pList, p);
					}
					break;
				}
				free(Name);
			} else if (LOWORD(wParam) > ID_DYNAMIC_BASE+200
			&& LOWORD(wParam) <= ID_DYNAMIC_BASE+200+ActiveConfig.SatelliteFiles.Count)
			{	unsigned long p = LOWORD(wParam)-ID_DYNAMIC_BASE-200-1;
				TIMED_STRING_LIST_S *pList = &ActiveConfig.SatelliteFiles;
				BUTTONS_S *Buttons = CreateButtons(-1);
				size_t NameLen = sizeof(TCHAR)*(strlen(pList->Entries[p].string)+80);
				TCHAR *Name = (TCHAR*)malloc(NameLen);
				StringCbPrintf(Name, NameLen, TEXT("%S\n\nLoaded: %04ld-%02ld-%02ld %02ld:%02ld:%02ldz"),
						pList->Entries[p].string,
						(long) pList->Entries[p].time.wYear,
						(long) pList->Entries[p].time.wMonth,
						(long) pList->Entries[p].time.wDay,
						(long) pList->Entries[p].time.wHour,
						(long) pList->Entries[p].time.wMinute,
						(long) pList->Entries[p].time.wSecond);
				AddButton(Buttons, "Reload", IDYES, FALSE);
				AddButton(Buttons, "Delete", IDRETRY, FALSE);
				AddButton(Buttons, "Cancel", IDABORT, TRUE);
				switch (LwdMessageBox2(hwnd, Name, TEXT("File TLE"), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON3, Buttons, NULL))
				{
				case IDYES:
					if (!LoadParseFile(hwnd, pList->Entries[p].string, pList->Entries[p].value))
						MessageBox(hwnd, TEXT("No TLEs Loaded From File"), TEXT("File TLE"), MB_OK | MB_ICONERROR);
					break;
				case IDRETRY:
					StringCbPrintf(Name, NameLen, TEXT("Really Delete\n\n%S"), pList->Entries[p].string);
					if (MessageBox(hwnd, Name, TEXT("Delete TLE File"),
									MB_YESNO | MB_ICONQUESTION) == IDYES)
					{	RemoveTimedStringEntry(pList, p);
					}
					break;
				}
				free(Name);
			} else if (HIWORD(wParam) == BN_CLICKED)
			{	int i;
				for (i=0; i<InfoCount; i++)
				{	if (Info[i].hwndCheck == (HWND) lParam)
					{	Info[i].CheckState = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
						if (HideUnchecked
						&& Info[i].CheckState==BST_UNCHECKED)
						{	Info[i].CheckState = BST_CHECKED;
							SendMessage((HWND)lParam, BM_SETCHECK,
										Info[i].CheckState, 0);
						}

TraceLogThread("CheckErr", TRUE, "[%ld vs %ld]%s Toggle State %ld\n",
				(long) Info[i].CheckIndex, (long) i,
				Info[i].ObjName, Info[i].CheckState);

						Info[i].NextXmit = 0;	/* Force a transmission */
						WakeupSleeper = TRUE;
						break;
					}
				}
			}
		}
	return 0;

	case WM_PAINT:
	{	int i, j;
		RECT rcWin;
		PAINTSTRUCT ps;
		BOOL ReSize = FALSE;
		int MaxWidth = 0;
		int DidCount = 0;

		if (LockInfo(1000))
		{

// Start the paint operation
		if (BeginPaint(hwnd, &ps) == NULL)
		{	return 0;
		}
		SetBkColor(ps.hdc, GetSysColor(COLOR_BTNFACE));	/* Gray background */
		SetTextColor(ps.hdc, GetSysColor(COLOR_BTNTEXT));
//		FillRect(ps.hdc, &ps.rcPaint, GetSysColorBrush(COLOR_BTNFACE));
		GetClientRect(hwnd, &rcWin);

		for (i=0; i<InfoCount /*&& (InfoCount != LastCount || rcWin.bottom > rcWin.top)*/; i++)
		if (!HideUnchecked || Info[i].CheckState || Info[i].Query.Count)
		{	TCHAR Text[80];
			RECT rcText = rcWin, rcTemp;
			int Height = 0;
			int Align;	/* -1=left, 0=center, 1=right */

			DidCount++;
			if (!Info[i].hwndCheck || !IsWindow(Info[i].hwndCheck))
			{	StringCbPrintf(Text, sizeof(Text), TEXT("%S"),
									Info[i].ObjName);
				Info[i].hwndCheck = CreateWindow(TEXT("button"),
										Text, /*TEXT(""),*/
										WS_CHILD | WS_VISIBLE
										| BS_AUTO3STATE,
										0, rcWin.top,
										usSquare*3+usSysCharWidth*9/*strlen(Info[i].ObjName)*/, usSquare,
										hwnd, (HMENU) NextCheckID--,
										g_hInstance, NULL);
				SendMessage(Info[i].hwndCheck, BM_SETCHECK, Info[i].CheckState, 0);
				Info[i].CheckIndex = i;
			} else if (Info[i].CheckIndex != i)
			{	POINT pt;
				RECT rcCheck;
				GetWindowRect(Info[i].hwndCheck,&rcCheck);
				pt.x = rcCheck.left; pt.y = rcCheck.top;
				ScreenToClient(hwnd, &pt);
				{	TraceLogThread("CheckErr", TRUE, "[%ld vs %ld]%s: Moving from %ld %ld (%ldx%ld) to %ld %ld (%ldx%ld)\n",
								(long) Info[i].CheckIndex, (long) i,
								Info[i].ObjName,
								pt.x, pt.y,
								(rcCheck.right-rcCheck.left),
								(rcCheck.bottom-rcCheck.top),
								0, rcWin.top,
								(rcWin.right-rcWin.left),
								(rcWin.bottom-rcWin.top));
				}
		
				SetWindowPos(Info[i].hwndCheck, NULL, 0, rcWin.top, 0, 0,
						SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER);
				InvalidateRect(Info[i].hwndCheck, NULL, TRUE);
				Info[i].CheckIndex = i;
			}
			if (IsWindow(Info[i].hwndCheck))
			{	POINT pt;
				RECT rcCheck;
				GetWindowRect(Info[i].hwndCheck,&rcCheck);
				pt.x = rcCheck.left; pt.y = rcCheck.top;
				ScreenToClient(hwnd, &pt);
				if (rcWin.top != pt.y || rcWin.left != pt.x)
				{	TraceLogThread("CheckErr", TRUE, "[%ld vs %ld]%s:%ld %ld (%ldx%ld) NOT %ld %ld (%ldx%ld)\n",
								(long) Info[i].CheckIndex, (long) i,
								Info[i].ObjName,
								pt.x, pt.y,
								(rcCheck.right-rcCheck.left),
								(rcCheck.bottom-rcCheck.top),
								rcWin.left, rcWin.top,
								(rcWin.right-rcWin.left),
								(rcWin.bottom-rcWin.top));
				}
			}
			if (!IsWindowVisible(Info[i].hwndCheck))
				ShowWindow(Info[i].hwndCheck, SW_SHOWNA);
			rcText.left += usSquare *3+usSysCharWidth*9/*strlen(Info[i].ObjName)*/;
			for (j=0; j<sizeof(Widths)/sizeof(Widths[0]); j++)
			{	*Text = TEXT('\0');	/* Empty string by default */
				switch (j)
				{
				case 0:	/* Obj */
					StringCbPrintf(Text, sizeof(Text), TEXT("%S"),
									Info[i].ObjName);
					Align = DT_CENTER; break;
				case 1:	/* Short */
					StringCbPrintf(Text, sizeof(Text), TEXT("%S"),
									Info[i].ShortName);
					Align = DT_CENTER; break;
				case 2:	/* Query Format */
				{	//if (Info[i].Pass.Valid)
					{	char *Temp = FormatPassInfoString2(&Info[i].Pass);
						StringCbPrintf(Text, sizeof(Text), TEXT("%S"), Temp);
						free(Temp);
					}
					Align = DT_LEFT; break;
				}
				case 3:	/* Query Count */
				{	char Temp[16];
					if (Info[i].Query.Count)
						StringCbPrintf(Text, sizeof(Text), TEXT("Q:%ld (%.*S -%S)"),
										(long) Info[i].Query.Count,
										STRING(Info[i].Query.LastFor),
										FormatDeltaTime(SecondsSince(&Info[i].Query.stLast),Temp,sizeof(Temp), NULL, NULL));
					Align = DT_LEFT; break;
				}
#ifdef OBSOLETE
				case 2:	/* What */
					StringCbPrintf(Text, sizeof(Text), TEXT("%S"),
							Info[i].WasVisible?"LOS":"AOS");
					Align = DT_CENTER; break;
				case 3:	/* When */
					if (Info[i].OS)
					{	double now = get_current_daynum();
						char Temp[16];
						ULONG32 SecondsLeft = (ULONG32)((Info[i].OS-now)*24*60*60);
						FormatDeltaTime(SecondsLeft, Temp, sizeof(Temp), NULL, NULL);
						StringCbPrintf(Text, sizeof(Text), TEXT("%S"),
										Temp);
						Align = DT_RIGHT;
					} else
					{	StringCbPrintf(Text, sizeof(Text), TEXT("NONE"));
						Align = DT_CENTER;
					}
					break;
				case 4:	/* Az */
					if (Info[i].WasVisible)
					{	StringCbPrintf(Text, sizeof(Text), TEXT("Az %ld"),
										(long) (Info[i].sat.az+0.5));
						Align = DT_RIGHT;
					} else if (Info[i].PassTime[0])	/* Got a pass duration? */
					{	StringCbPrintf(Text, sizeof(Text), TEXT("+%S"),
										(long) Info[i].PassTime);
						Align = DT_LEFT;
					} else *Text = TEXT('\0');
					break;
				case 5:	/* El */
					if (Info[i].WasVisible)
					{	double now = get_current_daynum();
						StringCbPrintf(Text, sizeof(Text), TEXT("El %2ld%c / %2ld"),
										(long) (Info[i].sat.el+0.5),
										Info[i].MaxT < now ? '-' : '+',
										(long) (Info[i].MaxEl+0.5));
					} else
						StringCbPrintf(Text, sizeof(Text), TEXT("MaxEl %2ld"),
										(long) (Info[i].MaxEl+0.5));
					Align = DT_LEFT; break;
#ifdef OBSOLETE
				case 6:	/* Altitude */
					StringCbPrintf(Text, sizeof(Text), TEXT("Alt:%ld FP:%ld"),
										(long) Info[i].sat.alt, (long) Info[i].sat.footprint);
					Align = DT_RIGHT; break;
#endif
#endif
				}

				if (*Text)
				{	RECT rcDraw;
					rcTemp = rcText;
					Height = DrawText(ps.hdc, Text, -1, &rcTemp, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_CALCRECT);
					if (rcTemp.right-rcTemp.left > Widths[j])
					{	Widths[j] = rcTemp.right-rcTemp.left;
						ReSize = TRUE;
					}
					if (!j)
					{	rcDraw = rcWin;
						rcDraw.bottom = rcDraw.top + Height;
						FillRect(ps.hdc, &rcDraw,
								GetSysColorBrush(COLOR_BTNFACE));
					}
					rcDraw = rcText;
					rcDraw.bottom = rcDraw.top + Height;
					rcDraw.right = rcDraw.left + Widths[j];
					Height = DrawText(ps.hdc, Text, -1, &rcDraw, Align | DT_TOP | DT_NOPREFIX);
				}
				rcText.left += Widths[j] + usSysCharWidth;
				if (rcText.left > MaxWidth) MaxWidth = rcText.left;
			}
			rcWin.top += Height;
		} else if (Info[i].hwndCheck
		&& IsWindowVisible(Info[i].hwndCheck))
			ShowWindow(Info[i].hwndCheck, SW_HIDE);

		FillRect(ps.hdc, &rcWin, GetSysColorBrush(COLOR_BTNFACE));

#ifdef FUTURE
		HFONT hOld = NO_FONT, hFont = GetMessageBoxFont();
		if (hFont != NO_FONT) hOld = (HFONT) SelectObject(ps.hdc, hFont);

		RECT rcWin;
		int cyText;
		GetClientRect(hwnd, &rcWin);

//		TraceLogThread("MessageBox", TRACE_CRITICAL, "\nClient:%ld %ld -> %ld %ld\n",
//						rcWin.left, rcWin.top, rcWin.right, rcWin.bottom);
		if (Info->iVscrollPos)
			rcWin.top -= Info->iVscrollPos;
		else if (Info->cyClient-Info->Buttons->ButtonHeight-Info->Buttons->ButtonMargin*2 > Info->rcText.bottom-Info->rcText.top)
			rcWin.top += (Info->cyClient-Info->Buttons->ButtonHeight-Info->Buttons->ButtonMargin*2-(Info->rcText.bottom-Info->rcText.top))/2;
		if (Info->iHscrollPos)
			rcWin.left -= Info->iHscrollPos;
		else if (Info->cxClient > Info->rcText.right-Info->rcText.left)
			rcWin.left += (Info->cxClient-(Info->rcText.right-Info->rcText.left))/2;

//#define IT_WAS_BLUE
#ifdef IT_WAS_BLUE
		if (Info->ButtonCount)
		{	RECT rcBottom = rcWin;
			rcBottom.top = rcWin.top + Info->cyText;
			rcBottom.left = 0; rcBottom.right = rcWin.right-rcWin.left;
			HBRUSH hbrBack = GetSysColorBrush(COLOR_BTNFACE);
			HBRUSH hbrOrg = (HBRUSH) SelectObject(ps.hdc, hbrBack);
			Rectangle(ps.hdc, rcBottom.left, rcBottom.top, rcBottom.right, rcBottom.bottom);
			SelectObject(ps.hdc, hbrOrg);
		}
#endif

//		rcWin.top += Info->cyChar/4;	/* Not sure why this isn't needed */
		rcWin.left += Info->cxCaps/2;	/* Not sure why this IS needed */
		cyText = DrawText(ps.hdc, Info->lpText, -1, &rcWin, LWD_DT);
//		TraceLogThread("MessageBox", TRACE_CRITICAL, "  Draw:%ld %ld -> %ld %ld (%ld High)\n",
//						rcWin.left, rcWin.top, rcWin.right, rcWin.bottom, cyText);

		if (hFont != NO_FONT) DeleteObject(SelectObject(ps.hdc, hOld));
#endif
		EndPaint(hwnd, &ps);
		if (ReSize || DidCount != LastDid || LastCount != InfoCount)
		{	RECT rcFrame;
			SetRect(&rcFrame, 0, 0, MaxWidth+usSysCharWidth, rcWin.top);
			AdjustWindowRectEx(&rcFrame, GetWindowLong(hwnd,GWL_STYLE), TRUE, GetWindowLong(hwnd,GWL_EXSTYLE));
			SetWindowPos(hwnd, NULL, 0, 0,
						rcFrame.right-rcFrame.left,
						rcFrame.bottom-rcFrame.top,
						SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
//			if (!IsWindowVisible(hwnd))
//			{	ShowWindow(hwnd, SW_SHOW);
//				UpdateWindow(hwnd);
//			}
//			if (DidCount > LastDid)
//			{	InvalidateRect(hwnd, NULL, FALSE);
//				UpdateWindow(hwnd);
//			}
			PostMessage(hwnd, WM_TIMER, 10, 0);	/* Defer an invalidation */
			LastDid = DidCount;
			LastCount = InfoCount;
		}
		UnlockInfo();
		} else TraceLogThread("Satellite", TRUE, "LockInfo(WM_PAINT) FAILED!");
	}
	break;
	case WM_CLOSE:
		if (MessageBox(hwnd, TEXT("Closing this window terminates the satellite object generation, Really Close?\n\n(Note: This may leave orphan satellite objects on your display)"), TEXT("Close Satellites"), MB_YESNO | MB_ICONQUESTION) == IDYES)
		{	ActiveConfig.SatelliteCount = 0;	/* No restart */
			DestroyWindow(hwnd);
		}
		return 0;
	case WM_DESTROY:
		DestroyNotifyIcon(hwnd, 3);
		hwndSatellites = NULL;
		TLEExit = TRUE;
		WakeupSleeper = TRUE;
		return 0;
	default:
		/* Handle registered window messages like Find/Replace */
		;
	}
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

int CreateSatelliteWindow(HWND hwnd)
{
	int Result = 0;
static	BOOL First = TRUE;
static const TCHAR g_szMsgName[] = TEXT("Satellites");

	if (First)
	{	WNDCLASSEX msgClass = {0};
		msgClass.cbSize = sizeof(msgClass);
		msgClass.style = CS_HREDRAW | CS_VREDRAW;
		msgClass.lpfnWndProc = SatelliteWndProc;
		msgClass.hInstance = g_hInstance;
//		if (LoadIconMetric(g_hInstance, MAKEINTRESOURCE(IDI_MY_ICON), LIM_SMALL, &msgClass.hIcon) != S_OK)
//		if (!msgClass.hIcon)
//			msgClass.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_MY_ICON));
//		if (!msgClass.hIcon)
//			msgClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		msgClass.hCursor = LoadCursor(NULL, IDC_ARROW);
//		msgClass.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
//		msgClass.hbrBackground = (HBRUSH) COLOR_BTNFACE+1;
//		msgClass.lpszMenuName = MAKEINTRESOURCE(IDM_LOG_W32);
		msgClass.lpszClassName = g_szMsgName;
		msgClass.lpszMenuName = MAKEINTRESOURCE(IDM_SATELLITE);
		RegisterClassEx(&msgClass);
		First = FALSE;
	}

	PopulateSatelliteWindowPos(&rcSize);
	hwndSatellites = CreateWindow(g_szMsgName, TEXT("Satellite Details"),
								/* WS_POPUP | */ WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN /*| WS_VSCROLL | WS_HSCROLL*/,
								rcSize.left, rcSize.top,
								rcSize.right, rcSize.bottom,
								NULL, NULL, g_hInstance, (LPVOID) Info);
	if (hwndSatellites)
	{
//	CenterWindow(hwndMsg);
//	ShowWindow(hwndSatellites, SW_SHOW);
//	UpdateWindow(hwndSatellites);
	}
	return Result;
}

void Satellite(HWND hwnd, UINT msg, WPARAM wp, char *Name, double lat, double lon, long alt)
{
	QTHInfo.Name = _strdup(Name);
	QTHInfo.lat = lat;
	QTHInfo.lon = lon;
	QTHInfo.alt = alt;
	QTHInfo.hwnd = hwnd;
	QTHInfo.msg = msg;
	QTHInfo.wp = wp;
	if (!hwndSatellites)
		CreateSatelliteWindow(hwnd);

//	ShowTraceLog("Satellite",FALSE);
}

