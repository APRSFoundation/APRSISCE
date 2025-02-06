#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "llutil.h"

#include "port.h"	/* For PortDumpHex */
#include "tracelog.h"

#include "filter.h"

#pragma warning(disable : 4995)

static void FreeSplitPieces(char **Array)
{
	if (Array) free(Array[0]);
}

static int SplitFilterPieces(char *Filter, int aSize, char **Array)
{	char *p;
	int aCount=0;

	if (!*Filter)	/* Empty string? */
	{	Array[0] = NULL;
		return 0;
	}
	Array[aCount++] = _strdup(Filter);
	for (p=Array[0]; *p; p++)
	{	if (*p == '/')	/* Filter delimiter */
		{	*p = '\0';	/* Null terminate previous piece */
			if (aCount < aSize)
			{	Array[aCount++] = p+1;
			} else
			{	TraceLog("FilterError", TRUE, NULL, "SplitFilterPieces:Filter(%s) Has > %ld Pieces!\n", 
							Array[0], (long) aSize);
				break;
			}
		}
	}
	if (!*Array[aCount-1]) aCount--;
	return aCount;
}

static void AddFilterPiece(FILTER_INFO_S *Filter, FILTER_TYPE_V Type, BOOL Negative, BOOL Positive, double f1, double f2, double f3, double f4, int ElementCount, char **Elements, BOOL WildCard=FALSE)
{	int f = Filter->Count++;
	if (Negative) Filter->HasNegative = Negative;
	if (Positive) Filter->HasPositive = Positive;
	Filter->Pieces = (FILTER_COMPONENT_S *)realloc(Filter->Pieces, sizeof(*Filter->Pieces)*Filter->Count);
	memset(&Filter->Pieces[f], 0, sizeof(Filter->Pieces[f]));
	Filter->Pieces[f].Type = Type;
	Filter->Pieces[f].Negative = Negative;
	Filter->Pieces[f].Positive = Positive;
	Filter->Pieces[f].f1 = f1;
	Filter->Pieces[f].f2 = f2;
	Filter->Pieces[f].f3 = f3;
	Filter->Pieces[f].f4 = f4;
	Filter->Pieces[f].ElementCount = ElementCount;
	if (ElementCount)
	{	Filter->Pieces[f].Elements = (char **) malloc(sizeof(*Filter->Pieces[f].Elements)*ElementCount);
		Filter->Pieces[f].EWild = (char *) malloc(sizeof(*Filter->Pieces[f].Elements)*ElementCount);
		Filter->Pieces[f].ELen = (char *) malloc(sizeof(*Filter->Pieces[f].Elements)*ElementCount);
		for (int e=0; e<ElementCount; e++)
		{	Filter->Pieces[f].Elements[e] = _strdup(Elements[e]);
			Filter->Pieces[f].ELen[e] = strlen(Elements[e]);
			if (WildCard && *Elements[e])
			{	if (Elements[e][Filter->Pieces[f].ELen[e]-1] == '*')
				{	Filter->Pieces[f].EWild[e] = TRUE;
					Filter->Pieces[f].ELen[e]--;
				} else Filter->Pieces[f].EWild[e] = FALSE;
			} else Filter->Pieces[f].EWild[e] = FALSE;
		}
	} else Filter->Pieces[f].Elements = NULL;
}

static BOOL ProcessFilterPiece(char *Piece, FILTER_INFO_S *Filter)
{	int o=0, pCount;
	BOOL Neg=FALSE, Pos=FALSE;
	char *Pieces[32];
	BOOL Error = FALSE;

	if (!*Piece) return TRUE;	/* Empty pieces don't matter */
	if (*Piece == '-') Neg = TRUE;
	else if (*Piece == '+') Pos = TRUE;
	if (Neg || Pos) o = 1;	/* o=1 for negative or positive */

	if (Piece[o+1] != '/')
	{	TraceLog("FilterError", TRUE, NULL, "Filter Error:Piece(%s) missing / as second character\n", Piece);	
		return FALSE;	/* Not a b/ or f/, probably not a good filter piece! */
	}

	pCount = SplitFilterPieces(Piece+o+2, sizeof(Pieces)/sizeof(Pieces[0]), Pieces);
	switch (Piece[o])
	{
	case 'b':	/* Budlist: b/call1/call2... (support *) */
		if (pCount < 1)
		{	TraceLog("FilterError", TRUE, NULL, "Buddy(%s) Has %ld Pieces, should be at least 1\n", Piece, (long) pCount);
			Error = TRUE;
		} else AddFilterPiece(Filter, FILTER_BUDLIST, Neg, Pos, 0, 0, 0, 0, pCount, Pieces, TRUE);
		break;
	case 'g':	/* Group message: g/call1/call2... (support *) */
		if (pCount < 1)
		{	TraceLog("FilterError", TRUE, NULL, "Buddy(%s) Has %ld Pieces, should be at least 1\n", Piece, (long) pCount);
			Error = TRUE;
		} else AddFilterPiece(Filter, FILTER_MSG_GROUP, Neg, Pos, 0, 0, 0, 0, pCount, Pieces, TRUE);
		break;
	case 'f':	/* Friend: f/call/dist (up to 9) */
		if (pCount != 2)
		{	TraceLog("FilterError", TRUE, NULL, "Friend(%s) Has %ld Pieces, should be 2\n", Piece, (long) pCount);
			Error = TRUE;
		} else if (!strlen(Pieces[0]))
		{	TraceLog("FilterError", TRUE, NULL, "Friend(%s) Has Empty Call\n", Piece);
			Error = TRUE;
		} else
		{	char *e;
			double dist = strtod(Pieces[1],&e) / KmPerMile;
			if (*e)
			{	TraceLog("FilterError", TRUE, NULL, "Friend(%s) Has Invalid Distance(%s)\n", Piece, Pieces[1]);
				Error = TRUE;
			} else AddFilterPiece(Filter, FILTER_FRIEND, Neg, Pos, dist, 0, 0, 0, pCount, Pieces);
		}
		break;
	case 'r':	/* Range: r/lat/lon/dist (up to 9) */
		if (pCount != 3)
		{	TraceLog("FilterError", TRUE, NULL, "Range(%s) Has %ld Pieces, should be 3\n", Piece, (long) pCount);
			Error = TRUE;
		} else
		{	char *e1, *e2, *e3;
			double lat = strtod(Pieces[0],&e1);
			double lon = strtod(Pieces[1],&e2);
			double dist = strtod(Pieces[2],&e3) / KmPerMile;
			if (*e1 || *e2 || *e3)
			{	TraceLog("FilterError", TRUE, NULL, "Range(%s) Has Invalid Lat(%s) Lon(%s) or Distance(%s)\n",
						Piece, Pieces[0], Pieces[1], Pieces[2]);
				Error = TRUE;
			} else AddFilterPiece(Filter, FILTER_RANGE, Neg, Pos, lat, lon, dist, 0, 0, NULL);
		}
		break;
	case 'p':	/* Prefix: p/aa/bb/cc... (implied *?) */
		if (pCount < 1)
		{	TraceLog("FilterError", TRUE, NULL, "Prefix(%s) Has %ld Pieces, should be at least 1\n", Piece, (long) pCount);
			Error = TRUE;
		} else
		{	AddFilterPiece(Filter, FILTER_PREFIX, Neg, Pos, 0, 0, 0, 0, pCount, Pieces);
			{	FILTER_COMPONENT_S *F = &Filter->Pieces[Filter->Count-1];
				for (unsigned int e=0; e<F->ElementCount; e++)
					F->EWild[e] = TRUE;	/* Implied wildcard to do substring match */
			}
		}
		break;
	case 'o':	/* Object: o/obj1/obj2... (supports *) */
		if (pCount < 1)
		{	TraceLog("FilterError", TRUE, NULL, "Object(%s) Has %ld Pieces, should be at least 1\n", Piece, (long) pCount);
			Error = TRUE;
		} else AddFilterPiece(Filter, FILTER_OBJECT, Neg, Pos, 0, 0, 0, 0, pCount, Pieces, TRUE);
		break;
	case 't':	/* Type: t/poimqstunw or t/poimqstu/call/km (Might work for t/m/CALLSIGN/Range?) */
		if (pCount != 1 && pCount != 3)
		{	TraceLog("FilterError", TRUE, NULL, "Type(%s) Has %ld Pieces, should be 1 or 3\n", Piece, (long) pCount);
			Error = TRUE;
		} else if (pCount == 1)
			AddFilterPiece(Filter, FILTER_TYPE, Neg, Pos, 0, 0, 0, 0, pCount, Pieces);
		else if (pCount == 3)
		{	if (!strlen(Pieces[1]))
			{	TraceLog("FilterError", TRUE, NULL, "Friend(%s) Has Empty Call\n", Piece);
				Error = TRUE;
			} else
			{	char *e;
				double dist = strtod(Pieces[2],&e) / KmPerMile;
				if (*e)
				{	TraceLog("FilterError", TRUE, NULL, "Friend(%s) Has Invalid Distance(%s)\n", Piece, Pieces[2]);
					Error = TRUE;
				} else AddFilterPiece(Filter, FILTER_TYPE_FRIEND, Neg, Pos, dist, 0, 0, 0, pCount, Pieces);
			}
		}
		break;
	case 's':	/* Symbol: s/pri/alt/over */
		if (pCount != 1 && pCount != 2 && pCount != 3)
		{	TraceLog("FilterError", TRUE, NULL, "Symbol(%s) Has %ld Pieces, should be 1, 2, or 3\n", Piece, (long) pCount);
			Error = TRUE;
		} else AddFilterPiece(Filter, FILTER_SYMBOL, Neg, Pos, 0, 0, 0, 0, pCount, Pieces);
		break;
	case 'd':	/* Digi: d/digi1/digi2... (supports *) */
		if (pCount < 1)
		{	TraceLog("FilterError", TRUE, NULL, "Digi(%s) Has %ld Pieces, should be at least 1\n", Piece, (long) pCount);
			Error = TRUE;
		} else AddFilterPiece(Filter, FILTER_DIGI, Neg, Pos, 0, 0, 0, 0, pCount, Pieces, TRUE);
		break;
	case 'D':	/* Digi: d/digi1/digi2... (supports *) */
		if (pCount < 1)
		{	TraceLog("FilterError", TRUE, NULL, "Digi(%s) Has %ld Pieces, should be at least 1\n", Piece, (long) pCount);
			Error = TRUE;
		} else AddFilterPiece(Filter, FILTER_DIGI_ANY, Neg, Pos, 0, 0, 0, 0, pCount, Pieces, TRUE);
		break;
	case 'a':	/* Area: a/latN/lonW/latS/lonE (up to 9) */
		if (pCount != 4)
		{	TraceLog("FilterError", TRUE, NULL, "Area(%s) Has %ld Pieces, should be 4\n", Piece, (long) pCount);
			Error = TRUE;
		} else
		{	char *e1, *e2, *e3, *e4;
			double latN = strtod(Pieces[0],&e1);
			double lonW = strtod(Pieces[1],&e2);
			double latS = strtod(Pieces[2],&e3);
			double lonE = strtod(Pieces[3],&e4);
			if (*e1 || *e2 || *e3 || *e4)
			{	TraceLog("FilterError", TRUE, NULL, "Area(%s) Has Invalid LatN(%s) LonW(%s) LatS(%s) or LonE(%s) \n",
						Piece, Pieces[0], Pieces[1], Pieces[2], Pieces[3]);
				Error = TRUE;
			} else if (latN <= latS)
			{	TraceLog("FilterError", TRUE, NULL, "Area(%s) Has Empty Range LatN(%s) -> LatS(%s)\n",
						Piece, Pieces[0], Pieces[2]);
				Error = TRUE;
			} else if (lonE <= lonW)
			{	TraceLog("FilterError", TRUE, NULL, "Area(%s) Has Empty Range LonW(%s) -> LonE(%s) \n",
						Piece, Pieces[1], Pieces[3]);
				Error = TRUE;
			} else AddFilterPiece(Filter, FILTER_AREA, Neg, Pos, latN, lonW, latS, lonE, 0, NULL);
		}
		break;
	case 'e':	/* Entry: e/call1/call2/... (supports *) */
		if (pCount < 1)
		{	TraceLog("FilterError", TRUE, NULL, "Entry(%s) Has %ld Pieces, should be at least 1\n", Piece, (long) pCount);
			Error = TRUE;
		} else AddFilterPiece(Filter, FILTER_ENTRY, Neg, Pos, 0, 0, 0, 0, pCount, Pieces, TRUE);
		break;
	case 'u':	/* Unproto: u/unproto1/unproto2/... (supports *) */
		if (pCount < 1)
		{	TraceLog("FilterError", TRUE, NULL, "Unproto(%s) Has %ld Pieces, should be at least 1\n", Piece, (long) pCount);
			Error = TRUE;
		} else AddFilterPiece(Filter, FILTER_UNPROTO, Neg, Pos, 0, 0, 0, 0, pCount, Pieces, TRUE);
		break;
	case 'q':	/* qConstruct: q/con/ana */
		if (pCount != 1 && pCount != 2)
		{	TraceLog("FilterError", TRUE, NULL, "qConstruct(%s) Has %ld Pieces, should be 1 or 2 (second ignored anyway)\n", Piece, (long) pCount);
			Error = TRUE;
		} else AddFilterPiece(Filter, FILTER_Q, Neg, Pos, 0, 0, 0, 0, pCount, Pieces);
		break;
	case 'm':	/* My Range: m/dist */
		if (pCount != 1)
		{	TraceLog("FilterError", TRUE, NULL, "MyRange(%s) Has %ld Pieces, should be 1\n", Piece, (long) pCount);
			Error = TRUE;
		} else
		{	char *e;
			double dist = strtod(Pieces[0],&e) / KmPerMile;
			if (*e)
			{	TraceLog("FilterError", TRUE, NULL, "MyRange(%s) Has Invalid Distance(%s)\n", Piece, Pieces[0]);
				Error = TRUE;
			} else AddFilterPiece(Filter, FILTER_MY, Neg, Pos, dist, 0, 0, 0, 0, NULL);
		}
		break;
	default:
		TraceLog("FilterError", TRUE, NULL, "Filter Error:Unrecognized Piece(%s), invalid first character\n", Piece);
		Error = TRUE;
	}
	FreeSplitPieces(Pieces);
	return !Error;
}

void FreeFilter(FILTER_INFO_S *Filter)
{	unsigned long f;

	if (!Filter) return;

	for (f=0; f<Filter->Count; f++)
	{	if (Filter->Pieces[f].ElementCount)
		{	for (unsigned int e=0; e<Filter->Pieces[f].ElementCount; e++)
			{	free(Filter->Pieces[f].Elements[e]);
			}
			free(Filter->Pieces[f].Elements);
			if (Filter->Pieces[f].EWild) free(Filter->Pieces[f].EWild);
			if (Filter->Pieces[f].ELen) free(Filter->Pieces[f].ELen);
		}
	}
	if (Filter->Pieces) free(Filter->Pieces);
	if (Filter->FilterText) free(Filter->FilterText);
	memset(Filter, 0, sizeof(*Filter));
}

static char *SkipWhite(char *p)
{	while (*p && isspace(*p & 0xff)) p++;
	return p;
}

BOOL OptimizeFilter(char *FilterText, FILTER_INFO_S *Filter)
{	BOOL Error = FALSE;

	FreeFilter(Filter);
	Filter->FilterText = _strdup(FilterText);

	//ClearTraceLog("FilterError");
	FilterText = SkipWhite(FilterText);	/* Don't care about leading whitespace */
	TraceLogThread("FilterError", TRUE, "Parsing %s\n", FilterText);
	PortDumpHex("FilterError", "Parsing(Hex):", strlen(FilterText), (unsigned char *) FilterText);
	{	char *p, *fp, *FreeMe = _strdup(FilterText);

		fp = FreeMe;	/* Start at the first piece */
		for (p=FreeMe; *p; p++)
		{	if (isspace(*p & 0xff))
			{	*p = '\0';	/* Null terminate previous piece */
				if (!ProcessFilterPiece(fp, Filter))
					Error = TRUE;
				fp = SkipWhite(p+1);	/* Skip to next piece */
				p = fp-1;	/* loop will increment */
			}
		}
		if (!ProcessFilterPiece(fp, Filter))	/* And the last piece */
			Error = TRUE;
		free(FreeMe);
	}
	Filter->HasErrors = Error;
	return !Error;
}

BOOL CheckOptimizedFilter(char *FilterText, FILTER_INFO_S *Filter)
{	if (!Filter->FilterText || strcmp(Filter->FilterText, FilterText))
	{	return OptimizeFilter(FilterText, Filter);
	}
	return Filter?!Filter->HasErrors:TRUE;
}

BOOL GetMyCoordinates(double *pLat, double *pLon);
BOOL GetFriendCoordinates(char *Station, double *pLat, double *pLon);

static int CheckElementHit(FILTER_COMPONENT_S *F, char *What)
{
	for (unsigned int p=0; p<F->ElementCount; p++)
	{	if (F->EWild[p])
		{	if (!strncmp(What, F->Elements[p], F->ELen[p]))
			{	F->LastHit = p+1;
				return p+1;	/* Where the Hit took place, but BOOL-ized */
			}
		} else if (!strncmp(What, F->Elements[p], F->ELen[p])
		&& (!What[F->ELen[p]] || What[F->ELen[p]]=='*'))	/* source* isn't part of the match */
		{	F->LastHit = p+1;
			return p+1;	/* Where the Hit took place, but BOOL-ized */
		}
	}
	F->LastHit = 0;
	return 0;
}

int FilterPacket(FILTER_INFO_S *Filter, APRS_PARSED_INFO_S *Packet)
{	unsigned long f;
	int Result = FALSE;

	Filter->LastHit = Filter->LastNix = 0;
	for (f=0; f<Filter->Count; f++)
	if (!Result	/* Don't need to check if we've already hit, but... */
	|| Filter->Pieces[f].Positive	/* Must check ALL of these */
	|| Filter->Pieces[f].Negative)	/* And ALL of these */
	{	BOOL Hit = FALSE;
		FILTER_COMPONENT_S *F = &Filter->Pieces[f];
		switch (F->Type)
		{
		case FILTER_PREFIX:		/* p/aa/bb/cc... */
		{	Hit = CheckElementHit(F, Packet->srcCall) != 0;
			break;
		}
		case FILTER_BUDLIST:	/* b/call1/call2... *able */
		{	Hit = CheckElementHit(F, Packet->srcCall) != 0;
			break;
		}
		case FILTER_MSG_GROUP:	/* g/call1/call2... *able */
		{	if (Packet->Valid & APRS_MESSAGE_VALID)
				Hit = CheckElementHit(F, Packet->msgCall) != 0;
			break;
		}
		case FILTER_OBJECT:		/* o/obj1/obj2... *able */
		{	if (Packet->Valid & (APRS_OBJECT_VALID|APRS_ITEM_VALID))
				Hit = CheckElementHit(F, Packet->objCall) != 0;
			break;
		}
		case FILTER_SYMBOL:		/* s/pri/alt/over */
		{	if (Packet->Valid & APRS_SYMBOL_VALID)
			{	char Sym = Packet->symbol & 0xff;
				char Tab = (Packet->symbol>>8) & 0xff;
				char Ovr = (Packet->symbol>>16) & 0xff;
				if (!Tab) Hit = strchr(F->Elements[0],Sym) != NULL;	/* Primary table */
				else if (F->ElementCount > 1)
				{	if ((Hit = (strchr(F->Elements[1],Sym) != NULL))
					&& F->ElementCount > 2)
						Hit = Ovr && strchr(F->Elements[2],Ovr) != NULL;
				}
				if (Hit) F->LastHit = (Ovr<<16) | (Tab<<8) | Sym;
#ifdef DEBUG_FILTER
		TraceLogThread("Filter", TRUE, "%sSymbol(0x%lX)(%ld %ld %ld) vs %s %s %s\n",
			   Hit?"HIT ":"",
			   (long) Packet->symbol,
			   Sym, Tab, Ovr,
			   F->Elements[0],
			   F->ElementCount>1?F->Elements[1]:"*NULL*",
			   F->ElementCount>2?F->Elements[2]:"*NULL*");
#endif
		}
#ifdef DEBUG_FILTER
else TraceLogThread("Filter", TRUE, "%sSymbol not valid\n", Hit?"HIT ":"");
#endif
break;
		}
		case FILTER_DIGI:		/* d/digi1/digi2... *able */
		{	int h;
			for (h=2; h<Packet->Path.hopUnused && !Hit; h++)
			{	Hit = CheckElementHit(F, Packet->Path.Hops[h]) != 0;
			}
			break;
		}
		case FILTER_DIGI_ANY:		/* D/digi1/digi2... *able */
		{	int h;
			for (h=2; h<Packet->Path.hopCount && !Hit; h++)
			{	Hit = CheckElementHit(F, Packet->Path.Hops[h]) != 0;
			}
			break;
		}
		case FILTER_AREA:		/* a/latN/lonW/latS/lonE (max:9) */
			if (Packet->Valid & APRS_LATLON_VALID)
			{	Hit = F->f1 >= Packet->lat	/* South of North */
					&& F->f3 <= Packet->lat	/* North of South */
					&& Packet->lon >= F->f2	/* East of West */
					&& Packet->lon <= F->f4;	/* West of East */
			}
			break;
		case FILTER_UNPROTO:	/* u/unproto1/unproto2/... *able */
		{	Hit = CheckElementHit(F, Packet->dstCall) != 0;
			break;
		}
		case FILTER_ENTRY:		/* e/call1/call2/... *able */
		{	for (int h=2; h<Packet->Path.hopCount-1; h++)	/* -1 keeps one after the q to check */
			if (strlen(Packet->Path.Hops[h]) == 3
			&& Packet->Path.Hops[h][0] == 'q'
			&& Packet->Path.Hops[h][1] == 'A')
			{	Hit = CheckElementHit(F, Packet->Path.Hops[h+1]) != 0;
				break;
			}
			break;
		}
		case FILTER_Q:			/* q/con/ana */
		{	if (Packet->Path.hopCount > 1
			&& strlen(Packet->Path.Hops[Packet->Path.hopCount-2]) == 3
			&& Packet->Path.Hops[Packet->Path.hopCount-2][0] == 'q'
			&& Packet->Path.Hops[Packet->Path.hopCount-2][1] == 'A')
			{	char *w = strchr(F->Elements[0], Packet->Path.Hops[Packet->Path.hopCount-2][2]);
				if (Hit = (w != NULL))
				{	F->LastHit = w-F->Elements[0]+1;
				}
			}
			break;
		}
		case FILTER_RANGE:		/* r/lat/lon/dist (max:9) */
		{double distance, bearing;
			if (Packet->Valid & APRS_LATLON_VALID)
			{	AprsHaversineLatLon(Packet->lat, Packet->lon,
								F->f1, F->f2, &distance, &bearing);
				Hit = distance < F->f3;
			}
			break;
		}
		case FILTER_MY:			/* m/dist */
		{	double lat, lon;
			if (Packet->Valid & APRS_LATLON_VALID)
			if (GetMyCoordinates(&lat, &lon))
			{double distance, bearing;
				AprsHaversineLatLon(Packet->lat, Packet->lon,
									lat, lon, &distance, &bearing);
				Hit = distance < F->f1;
			}
			break;
		}
		case FILTER_FRIEND:		/* f/call/dist (max:9)*/
		{	double lat, lon;
			if (Packet->Valid & APRS_LATLON_VALID)
			if (GetFriendCoordinates(F->Elements[0], &lat, &lon))
			{double distance, bearing;
				AprsHaversineLatLon(Packet->lat, Packet->lon,
									lat, lon, &distance, &bearing);
				Hit = distance < F->f1;
			}
			break;
		}
		case FILTER_TYPE_FRIEND:	/* t/poimqstu/call/km */
		{	double lat, lon;
			if (Packet->Valid & APRS_LATLON_VALID)
			if (GetFriendCoordinates(F->Elements[0], &lat, &lon))
			{double distance, bearing;
				AprsHaversineLatLon(Packet->lat, Packet->lon,
									lat, lon, &distance, &bearing);
				Hit = distance < F->f1;
			}
			if (!Hit) break;	/* Fall through if Friend was a hit */
		}
		case FILTER_TYPE:		/* t/poimqstunw */
		{	char *c;
			for (c=F->Elements[0]; *c && !Hit; c++)
			switch (*c)
			{
/* Position */		case 'p':	Hit = Packet->Valid & APRS_LATLON_VALID; break;
/* Objects */		case 'o':	Hit = Packet->Valid & APRS_OBJECT_VALID; break;
/* Items */			case 'i':	Hit = Packet->Valid & APRS_ITEM_VALID; break;
/* Message */		case 'm':	Hit = Packet->Valid & APRS_MESSAGE_VALID; break;
/* Query */			case 'q':	Hit = Packet->datatype == '?'; break;
/* Status */		case 's':	Hit = Packet->datatype == '>'; break;
/* Telemetry */		case 't':	Hit = (Packet->Valid & APRS_TELEMETRY_VALID)
									|| (Packet->Valid & APRS_TELEMETRYDEF_VALID); break;
/* User-defined */	case 'u':	Hit = Packet->datatype == '{'; break;
/* NWS Weather & Weather Objects */
					case 'n':	/* See parse.c for definition */
								Hit = Packet->Valid & APRS_NWS_VALID; break;
/* Weather */		case 'w':	Hit = Packet->Valid & APRS_WEATHER_VALID; break;
			}
			if (Hit) F->LastHit = c-F->Elements[0];	/* c++ already happened */
			break;
		}
		default:
			;
		}
		if (F->Positive)
		{	if (!Hit)	/* Missed positives knock out Result */
			{	Filter->LastNix = f+1;
				Result = FALSE;
				break;
			}	/* Hitting positives don't change Result */
		} else if (Hit)
		{	if (Result = !F->Negative)
			{	Filter->LastHit = f+1;
			} else Filter->LastNix = f+1;
			if (!Filter->HasPositive
			&& (!Filter->HasNegative || F->Negative))
				break;	/* Short circuit out if no + or - */
		}
	}

	return Result;
}

static char *FormatFilterElementHit(FILTER_COMPONENT_S *F)
{
/* Set up a default */
	sprintf(F->HitNixDetail,"%c/*UNKNOWN*", F->Type);

	switch (F->Type)
	{
	case FILTER_PREFIX:		/* p/aa/bb/cc... */
	case FILTER_BUDLIST:	/* b/call1/call2... *able */
	case FILTER_MSG_GROUP:	/* g/call1/call2... *able */
	case FILTER_OBJECT:		/* o/obj1/obj2... *able */
	case FILTER_DIGI:		/* d/digi1/digi2... *able */
	case FILTER_DIGI_ANY:		/* D/digi1/digi2... *able */
	case FILTER_ENTRY:		/* e/call1/call1/... *able */
	case FILTER_UNPROTO:	/* u/unproto1/unproto2/... *able */
		if (F->LastHit && F->LastHit <= F->ElementCount)
		{	sprintf(F->HitNixDetail, "%c/%s", F->Type, F->Elements[F->LastHit-1]);
		}
		break;

	case FILTER_SYMBOL:		/* s/pri/alt/over */
	{	char Sym = F->LastHit & 0xff;
		char Tab = (F->LastHit>>8) & 0xff;
		char Ovr = (F->LastHit>>16) & 0xff;
		if (!Tab) sprintf(F->HitNixDetail,"%c/%c", F->Type, Sym);
		else if (F->ElementCount > 2)
			sprintf(F->HitNixDetail,"%c//%c/%c (0x%lX)", F->Type, Sym, Ovr, (long) F->LastHit);
		else if (F->ElementCount > 1)
			sprintf(F->HitNixDetail,"%c//%c", F->Type, Sym);
		break;
	}
	case FILTER_AREA:		/* a/latN/lonW/latS/lonE (max:9) */
		sprintf(F->HitNixDetail, "%c/%lf/%lf/%lf/%lf",
				F->Type, F->f1, F->f2, F->f3, F->f4);
		break;
	case FILTER_Q:			/* q/con/ana */
		if (F->LastHit && F->LastHit <= strlen(F->Elements[0]))
			sprintf(F->HitNixDetail, "%c/%c",
					F->Type, F->Elements[0][F->LastHit-1]);
		break;
	case FILTER_RANGE:		/* r/lat/lon/dist (max:9) */
		sprintf(F->HitNixDetail, "%c/%lf/%lf/%.1lfmi",
				F->Type, F->f1, F->f2, F->f3);
		break;
	case FILTER_MY:			/* m/dist */
		sprintf(F->HitNixDetail, "%c/%.1lfmi",
				F->Type, F->f1);
		break;
	case FILTER_FRIEND:		/* f/call/dist (max:9)*/
		sprintf(F->HitNixDetail, "%c/%s/%.1lfmi",
				F->Type, F->Elements[0], F->f1);
		break;
	case FILTER_TYPE_FRIEND:	/* t/poimqstu/call/km */
		if (F->LastHit && F->LastHit <= strlen(F->Elements[0]))
			sprintf(F->HitNixDetail, "%c/%c/%s/%.1lfmi",
					F->Type, F->Elements[0][F->LastHit-1],
					F->Elements[0], F->f1);
		break;
	case FILTER_TYPE:		/* t/poimqstunw */
		if (F->LastHit && F->LastHit <= strlen(F->Elements[0]))
			sprintf(F->HitNixDetail, "%c/%c",
					F->Type, F->Elements[0][F->LastHit-1]);
		break;
	default:
		;	/* Default was already established */
	}
	return F->HitNixDetail;
}

char *GetHitNixDetail(FILTER_INFO_S *Filter)
{	
	if (Filter->LastHit)
	{	char *HitText = FormatFilterElementHit(&Filter->Pieces[Filter->LastHit-1]);
		if (Filter->LastNix)
		{	char *NixText = FormatFilterElementHit(&Filter->Pieces[Filter->LastNix-1]);
			sprintf(Filter->HitNixDetail, "Nix(%s) Hit(%s)", NixText, HitText);
		} else sprintf(Filter->HitNixDetail, "Hit(%s)", HitText);
	} else if (Filter->LastNix)
	{	char *NixText = FormatFilterElementHit(&Filter->Pieces[Filter->LastNix-1]);
		sprintf(Filter->HitNixDetail, "Nix(%s)", NixText);
	} else sprintf(Filter->HitNixDetail, "NoHit");
	return Filter->HitNixDetail;
}

