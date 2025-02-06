#include "sysheads.h"

#include "llutil.h"

double DegToRad(double deg) { return deg / DegreesPerRadian; }
double Rad2Deg(double rad) { return rad * DegreesPerRadian; }

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (UINT_PTR)(sizeof(a)/sizeof((a)[0]))
#endif

TCHAR *GetPrettyDistance(double distance)	/* In Miles */
{	size_t BufSize = 80;
	TCHAR *Buffer = (TCHAR*) malloc(BufSize*sizeof(*Buffer));
	char *Units = "mi";

	if (ActiveConfig.View.Metric.Distance)
	{	distance *= KmPerMile;
		Units = "km";
	}
	if (distance >= 100.0)
		StringCbPrintf(Buffer, BufSize, TEXT("%ld%S"), (long) distance, Units);
	else if (distance >= 10.0)
		StringCbPrintf(Buffer, BufSize, TEXT("%.1lf%S"), (double) distance, Units);
	else if (ActiveConfig.View.Metric.Distance)
	{	if (distance > (999.0/1000.0))
			StringCbPrintf(Buffer, BufSize, TEXT("%.2lf%S"), (double) distance, Units);
		else StringCbPrintf(Buffer, BufSize, TEXT("%ldm"), (long) (distance*1000.0));
	} else
	{	if (distance > (999.0/5280.0))
			StringCbPrintf(Buffer, BufSize, TEXT("%.2lf%S"), (double) distance, Units);
		else StringCbPrintf(Buffer, BufSize, TEXT("%ldft"), (long) (distance*5280.0));
	}
	return Buffer;
}

TCHAR *GetCompassPoint(int Degrees)
{
static TCHAR *Headings[] = { TEXT("N"), TEXT("NNE"), TEXT("NE"), TEXT("ENE"),
							TEXT("E"), TEXT("ESE"), TEXT("SE"), TEXT("SSE"),
							TEXT("S"), TEXT("SSW"), TEXT("SW"), TEXT("WSW"),
							TEXT("W"), TEXT("WNW"), TEXT("NW"), TEXT("NNW") };

	Degrees = Degrees + 360/ARRAYSIZE(Headings)/2;
	if (Degrees >= 360) Degrees -= 360;
	Degrees = Degrees * ARRAYSIZE(Headings) / 360;
	if (Degrees >= 0 && Degrees < ARRAYSIZE(Headings))
		return Headings[Degrees];
	return TEXT("***");
}

char *GetCompassPoint8(int Degrees)
{
static char *Headings[] = { "N", "NE",
							"E", "SE",
							"S", "SW",
							"W", "NW" };

	Degrees = Degrees + 360/ARRAYSIZE(Headings)/2;
	if (Degrees >= 360) Degrees -= 360;
	Degrees = Degrees * ARRAYSIZE(Headings) / 360;
	if (Degrees >= 0 && Degrees < ARRAYSIZE(Headings))
		return Headings[Degrees];
	return "**";
}

int MakeBase91(long v, int digits, char *buff)
{	int orgdigits = digits;
	long orgv = v;
	char *OrgBuff = buff;
	int used = 1;	/* Even 0 uses 1 digit */

	if (v < 0)
	{
		TraceLogThread("Track2ML", TRUE, "MakeBase91(%ld) Invalid Negative!\n", (long) v);
		return FALSE;
	}
	while (digits--)
	{	long n = v/91;
		unsigned char d = (unsigned char) ((v-(n*91)) + 33);
		buff[digits] = d;
		v = n;
		if (v) used++;
	}
	if (v)
	{	TraceLogThread("Track2ML", TRUE, "MakeBase91(%ld) Remainder %ld after %ld Digits!\n", (long) orgv, v, orgdigits);
		return FALSE;	/* Failed */
	}
	return used;
}

/*	From: http://www.movable-type.co.uk/scripts/latlong.html */
void AprsHaversinePos(GPS_POSITION *From, GPS_POSITION *To, double *Dist, double *Bearing)
{	AprsHaversineLatLon(From->dblLatitude, From->dblLongitude, To->dblLatitude, To->dblLongitude, Dist, Bearing);
}

void AprsHaversineLatLon(double lat1, double lon1, double lat2, double lon2, double *Dist, double *Bearing)
{
	lat1 = DegToRad(lat1);
	lon1 = DegToRad(lon1);
	lat2 = DegToRad(lat2);
	lon2 = DegToRad(lon2);

/*	var R = 6371; // km
	var dLat = (lat2-lat1).toRad();
	var dLon = (lon2-lon1).toRad(); 
	var a = Math.sin(dLat/2) * Math.sin(dLat/2) +
	        Math.cos(lat1.toRad()) * Math.cos(lat2.toRad()) * 
	        Math.sin(dLon/2) * Math.sin(dLon/2); 
	var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a)); 
	var d = R * c;
*/
	double dLat = lat2 - lat1;
	double dLon = lon2 - lon1;
	double sindLat2 = sin(dLat/2);
	double sindLon2 = sin(dLon/2);
	double a = sindLat2 * sindLat2 + cos(lat1)*cos(lat2) * sindLon2*sindLon2;
	double c = 2 * atan2(sqrt(a), sqrt(1.0-a));
/*
	var y = Math.sin(dLon) * Math.cos(lat2);
	var x = Math.cos(lat1)*Math.sin(lat2) -
	        Math.sin(lat1)*Math.cos(lat2)*Math.cos(dLon);
	var brng = Math.atan2(y, x).toBrng();
*/
	double y = sin(dLon) * cos(lat2);
	double x = cos(lat1)*sin(lat2) - sin(lat1)*cos(lat2)*cos(dLon);

	*Dist = EarthRadius * c;
	*Bearing = fmod(Rad2Deg(atan2(y,x)) + 360.0,360.0);
}

/*

#define EarthRadius 3959.0

double Rad2Deg(double rad) { return rad * DegreesPerRadian; }

New position given current lat/lon, distance, and bearing

d/R is the angular distance (in radians), where d is the distance travelled and R is the earth’s radius 
JavaScript: var lat2 = Math.asin( Math.sin(lat1)*Math.cos(d/R) + 
                      Math.cos(lat1)*Math.sin(d/R)*Math.cos(brng) );
var lon2 = lon1 + Math.atan2(Math.sin(brng)*Math.sin(d/R)*Math.cos(lat1), 
                             Math.cos(d/R)-Math.sin(lat1)*Math.sin(lat2)); 

lat2: =ASIN(SIN(lat1)*COS(d/ER) + COS(lat1)*SIN(d/ER)*COS(brng))
lon2: =lon1 + ATAN2(COS(d/ER)-SIN(lat1)*SIN(lat2), SIN(brng)*SIN(d/ER)*COS(lat1))

*/
void AprsProjectLatLon(double fLat, double fLon, double Dist, double Bearing, double *ptLat, double *ptLon)
{	
	double lat1 = DegToRad(fLat);
	double lon1 = DegToRad(fLon);
	double brng = DegToRad(Bearing);
	double dR = Dist / EarthRadius;
	double lat2 = asin(sin(lat1)*cos(dR) + cos(lat1)*sin(dR)*cos(brng));
	double lon2 = lon1 + atan2(sin(brng)*sin(dR)*cos(lat2), cos(dR)-sin(lat1)*sin(lat2));
	*ptLat = Rad2Deg(lat2);
	*ptLon = Rad2Deg(lon2);
}

void AprsProjectWaypoint(GPS_POSITION *From, double Dist, double Bearing, GPS_POSITION *To)
{	AprsProjectLatLon(From->dblLatitude, From->dblLongitude,
					  Dist, Bearing,
					  &To->dblLatitude, &To->dblLongitude);
}

#ifdef OBSOLETE
TCHAR *FormatLatLon(double LatLon, const TCHAR *PNstring)
{static	TCHAR OutBuf[30];
	OutBuf[0] = PNstring[LatLon<0?1:0];

	if (LatLon < 0) LatLon = - LatLon;
	StringCbPrintf(&OutBuf[1], sizeof(OutBuf)-sizeof(TCHAR), TEXT("%ld %7.4lf"),
					(long) floor(LatLon),
					(double) (LatLon - floor(LatLon))*60.0);
	return OutBuf;
}
#endif

#ifdef __cplusplus
extern "C"
{
#endif
void cdecl TraceError(HWND hwnd, char *Format, ...);
void cdecl TraceLog(char *Name, BOOL ForceIt, HWND hwnd, char *Format, ...);
#ifdef __cplusplus
}
#endif

#include "parsedef.h"
#include "parse.h"

static size_t FormatCoordinate(double Coord, int degDigits, int addDigits, int daoDigits, size_t bufSize, char *Buffer)
{	double fCoord;

	if (Coord < 0) Coord = -Coord;
	fCoord = floor(Coord);

	StringCbPrintfA(Buffer, bufSize, "%0*ld%*.*lf",
				(int) degDigits, (long) fCoord,
				5+addDigits+daoDigits, 2+addDigits+daoDigits,
				(double) (Coord - fCoord)*60.0);
	return strlen(Buffer);
}

static void FormatLatLon(double Lat, double Lon, int addDigits, int daoDigits, size_t latSize, char *latBuf, size_t lonSize, char *lonBuf)
{	FormatCoordinate(Lat, 2, addDigits, daoDigits, latSize, latBuf);
	FormatCoordinate(Lon, 3, addDigits, daoDigits, lonSize, lonBuf);
}

BOOL AreCoordinatesEquivalent(double Lat1, double Lon1, double Lat2, double Lon2, int daoDigits)
{	char lat1[33], lon1[33], lat2[33], lon2[33];

	if (Lat1 == Lat2 && Lon1 == Lon2) return TRUE;
	FormatLatLon(Lat1, Lon1, 0, daoDigits, sizeof(lat1), lat1, sizeof(lon1), lon1);
	FormatLatLon(Lat2, Lon2, 0, daoDigits, sizeof(lat2), lat2, sizeof(lon2), lon2);
#ifdef VERBOSSE
	TraceLog("Transmit", TRUE, NULL, "DAO(%ld) %s vs %s and %s vs %s is %s\n",
			(long) daoDigits, lat1, lat2, lon1, lon2,
			!strcmp(lat1, lat2) && !strcmp(lon1, lon2) ? "Equivalent" : "Different");
#endif
	return !strcmp(lat1, lat2) && !strcmp(lon1, lon2);
}

static void DoPair(double Lat, double Lon, char *Target, int Digits, char Base)
{	double fLat = floor(Lat), fLon = floor(Lon);
	Target[1] = ((int)floor(Lat))+Base;
	Target[0] = ((int)floor(Lon))+Base;
	Target[2] = '\0';

	if (Digits < 2) return;

	Lat -= fLat; Lon -= fLon;
	if (Base == 'A' || Base == 'a')
	{	Lat *= 10; Lon *= 10;
		DoPair(Lat, Lon, Target+2, Digits-2, '0');
	} else if (Base == '0')
	{	Lat *= 24; Lon *= 24;
		DoPair(Lat, Lon, Target+2, Digits-2, 'a');
	}
	return;
}

char *GridSquare(double Lat, double Lon, int Pairs)
{static char OutBuf[48];

	Lat = Lat + 90;	/* Make it 0-180 south to north */
	Lon = Lon + 180;	/* Make it 0-360 west to east */

	if (Lat < 0 || Lat >= 180.0
	|| Lon < 0 || Lon >= 360.0)
	{	strncpy(OutBuf,"Bogus",sizeof(OutBuf));
	} else
	{	DoPair(Lat/10.0, Lon/20.0, OutBuf, Pairs*2-2, 'A');
	}
	return OutBuf;
}

static void FromPair(char *Pair, char Base, double Factor, double *pLat, double *pLon)
{
	if (!Pair || strlen(Pair) < 2)
	{	*pLat = *pLon = Factor/2;
	} else
	{	double Lat = Pair[1]-Base, Lon = Pair[0]-Base;
		if (Base == 'A')
		{	double dLat, dLon;
			FromPair(Pair+2, '0', 10, &dLat, &dLon);
			Lat += dLat/10; Lon += dLon/10;
		} else if (Base == '0')
		{	double dLat, dLon;
			FromPair(Pair+2, 'A', 24, &dLat, &dLon);
			Lat += dLat/24; Lon += dLon/24;
		}
		*pLat = Lat; *pLon = Lon;
	}
}

void GridSquare2LatLon(char *GridSquare, double *pLat, double *pLon)
{	double Lat, Lon;
	GridSquare = _strupr(_strdup(GridSquare));
	FromPair(GridSquare, 'A', 18, &Lat, &Lon);
	*pLat = Lat*10 - 90; *pLon = Lon*20 - 180;
	free(GridSquare);
}

char *APRSCompressed(GPS_POSITION *Pos, BOOL NewPos, char Table, char Code, BOOL CourseSpeed, BOOL Altitude)
{	char *OutBuf = (char*)malloc(sizeof(*OutBuf)*14);
	char *p = OutBuf;

//	if (isdigit(Table & 0xff)) return NULL;	/* Can't be numeric */
	if (isdigit(Table & 0xff)) Table += 'a' - '0';	/* Offset to a-j */
	if (CourseSpeed)	/* Make sure we can */
	{	if ((Pos->dwValidFields & (GPS_VALID_HEADING | GPS_VALID_SPEED)) != (GPS_VALID_HEADING | GPS_VALID_SPEED)) CourseSpeed = FALSE;
		else if (Pos->flHeading < 0 || Pos->flHeading > 360) CourseSpeed = FALSE;
		else if (Pos->flSpeed < 0 || Pos->flSpeed >= 1100) CourseSpeed = FALSE;
	}
	if  (Altitude)	/* Make sure we can */
	{	if (!(Pos->dwValidFields & GPS_VALID_ALTITUDE_WRT_SEA_LEVEL)) Altitude = FALSE;
		else if (Pos->flAltitudeWRTSeaLevel <= 0) Altitude = FALSE;
		else if (Pos->flAltitudeWRTSeaLevel*3.2808399 >= 15332112) Altitude = FALSE;
	}
	if (CourseSpeed && Altitude) return NULL;	/* Can't do both */

	*p++ = Table;	/* non-numeric table starts it out */

	if (!MakeBase91((long)(380926*(90-Pos->dblLatitude)),4,p)) return NULL;
	if (!MakeBase91((long)(190463*(180+Pos->dblLongitude)),4,p+4)) return NULL;
	p += 8;
	*p++ = Code;

	char T=2;	/* Software source to Base91 */
	if (NewPos) T |= 0x20;	/* Current fix, not stored */
	if (CourseSpeed)
	{	*p++ = (char)((Pos->flHeading+2)/4+33);			/* Degrees */
		*p++ = (char)(log(Pos->flSpeed)/log(1.08)+33);	/* Knots */
		*p++ = T+33;
	} else if (Altitude)
	{	long ra = (long)(log(Pos->flAltitudeWRTSeaLevel*3.2808399)/log(1.002));
		if (!MakeBase91(ra,2,p)) return NULL;	/* Feet */
		p += 2;
		T |= 0x10;		/* GGA source indicates altitude */
		*p++ = T+33;
	} else
	{	*p++ = ' ';		/* no csT, blank the first */
		*p++ = 's';		/* but don't leave */
		*p++ = 'T';		/* trailing blanks */
	}
	*p++ = '\0';		/* And null terminate the whole thing */
	return OutBuf;
}

TCHAR *APRSCompressLatLon(double Lat, double Lon, char Table, char Code, BOOL CourseSpeed, double Course, double Speed, BOOL Altitude, double Alt)
{	TCHAR *OutBuf = (TCHAR*)malloc(sizeof(*OutBuf)*14);
	TCHAR *p = OutBuf;
	char tbuf[4];
	int i;

	if (isdigit(Table & 0xff)) Table += 'a' - '0';	/* Offset to a-j */
	if (CourseSpeed)	/* Make sure we can */
	{	if (Course < 0 || Course > 360) CourseSpeed = FALSE;
		else if (Speed < 0 || Speed >= 1100) CourseSpeed = FALSE;
	}
	if  (Altitude)	/* Make sure we can */
	{	if (Alt <= 0) Altitude = FALSE;
		else if (Alt*3.2808399 >= 15332112) Altitude = FALSE;
	}
	if (CourseSpeed && Altitude) return NULL;	/* Can't do both */

	*p++ = Table;	/* non-numeric table starts it out */

	if (!MakeBase91((long)(380926*(90-Lat)),4,tbuf)) return NULL;
	else for (i=0; i<4; i++) *p++ = tbuf[i];
	if (!MakeBase91((long)(190463*(180+Lon)),4,tbuf)) return NULL;
	else for (i=0; i<4; i++) *p++ = tbuf[i];
	*p++ = Code;

	char T=2;	/* Software source to Base91 */
	if (CourseSpeed)
	{	*p++ = (char)((Course+2)/4+33);			/* Degrees */
		*p++ = (char)(log(Speed)/log(1.08)+33);	/* Knots */
		*p++ = T+33;
	} else if (Altitude)
	{	long ra = (long)(log(Alt*3.2808399)/log(1.002));
		if (!MakeBase91(ra,2,tbuf)) return NULL;	/* Feet */
		*p++ = tbuf[0];
		*p++ = tbuf[1];
		T |= 0x10;		/* GGA source indicates altitude */
		*p++ = T+33;
	} else
	{	*p++ = ' ';		/* no csT, blank the first */
		*p++ = 's';		/* but don't leave */
		*p++ = 'T';		/* trailing blanks */
	}
	*p++ = '\0';		/* And null terminate the whole thing */
	return OutBuf;
}

TCHAR *APRSLatLon(double Lat, double Lon, char Table, char Code, int addDigits, int daoDigits, char **pDAO)
{	size_t BufSize = sizeof(TCHAR)*48;
	TCHAR *OutBuf = (TCHAR*)malloc(BufSize);
	size_t DAOSize = sizeof(char)*16;
	char *DAO = (char*)malloc(DAOSize);
	TCHAR *p;
	char NS = (Lat<0)?'S':'N';
	char EW = (Lon<0)?'W':'E';
	char lat[33], lon[33];
	int latLen, lonLen;

	if (Lat < 0) Lat = -Lat;
	if (Lon < 0) Lon = -Lon;
	double fLat = floor(Lat);
	double fLon = floor(Lon);

	if (addDigits < 0 && daoDigits == 0)
	{	daoDigits = addDigits;
		addDigits = 0;
	}

	DAO[0] = '\0';
	if (daoDigits <= 0)
	{	StringCbPrintf(OutBuf, BufSize, TEXT("%02ld%*.*lf%c%c%03ld%*.*lf%c%c"),
						(long) fLat, 5+addDigits, 2+addDigits,
						(double) (Lat - fLat)*60.0, NS, (char) Table,
						(long) fLon, 5+addDigits, 2+addDigits,
						(double) (Lon - fLon)*60.0, EW, (char) Code);
		if ((int) wcslen(OutBuf) != 19+addDigits*2)
			TraceLog("LatLon",TRUE,NULL,"%s from %.6lf %.6lf f(%.6lf %.6lf) d(%.6lf %.6lf)\n",
					OutBuf, (double) Lat, (double) Lon, (double) fLat, (double) fLon,
					(double) (Lat-fLat), (double) (Lon-fLon));
	} else
	{	if (daoDigits > 2) daoDigits = 2;
		FormatLatLon(Lat, Lon, addDigits, daoDigits, sizeof(lat), lat, sizeof(lon), lon);
#ifdef OLD_WAY
		StringCbPrintfA(lat, sizeof(lat), "%02ld%*.*lf",
						(long) fLat, 5+addDigits+daoDigits, 2+addDigits+daoDigits,
						(double) (Lat - fLat)*60.0);
		StringCbPrintfA(lon, sizeof(lon), "%03ld%*.*lf",
						(long) fLon, 5+addDigits+daoDigits, 2+addDigits+daoDigits,
						(double) (Lon - fLon)*60.0);
#endif
		latLen = strlen(lat); lonLen = strlen(lon);
#ifdef VERBOSE
		TraceError(NULL, "RFID:daoDigits=%ld  latLen=%ld or %.*s of %s  lonLen=%ld or %.*s of %s\n",
					(long) daoDigits,
					(long) latLen, latLen-daoDigits, lat, lat,
					(long) lonLen, lonLen-daoDigits, lon, lon);
#endif
		StringCbPrintf(OutBuf, BufSize, TEXT("%.*S%c%c%.*S%c%c"),
						latLen-daoDigits, lat, NS, (char) Table,
						lonLen-daoDigits, lon, EW, (char) Code);
		if (daoDigits == 1)
			StringCbPrintfA(DAO, DAOSize, "!W%c%c!", lat[2+5+addDigits], lon[3+5+addDigits]);
		else if (daoDigits == 2)
		{	StringCbPrintfA(DAO, DAOSize, "!w%c%c!", (char)(atoi(&lat[2+5+addDigits])/1.10)+'!',(char)(atoi(&lon[3+5+addDigits])/1.10)+'!'); 
#ifdef VERBOSE
			TraceError(NULL, "RFID:Lat: %s -> %ld or %ld (%c) Lon: %s -> %ld or %ld (%c)  (%S)\n",
						lat, (long) atoi(&lat[2+5+addDigits]), (long) (atoi(&lat[2+5+addDigits])/1.10), (char) (atoi(&lat[2+5+addDigits])/1.10+'!'),
						lon, (long) atoi(&lon[3+5+addDigits]), (long) (atoi(&lon[3+5+addDigits])/1.10), (char) (atoi(&lon[3+5+addDigits])/1.10+'!'),
						OutBuf);
#endif
		}
	}
	for (p=OutBuf; *p; p++)
		if (*p == *TEXT(" ") && (p-OutBuf)!=8+addDigits && (p-OutBuf)!=18+addDigits*2)
			*p = *TEXT("0");

	if (daoDigits < 0)
	switch(daoDigits)
	{
	case -4: OutBuf[2] = ' ';	/* falls through on purpose */
	case -3: OutBuf[3] = ' ';	/* falls through on purpose */
	case -2: OutBuf[5] = ' ';	/* falls through on purpose */
	case -1: OutBuf[6] = ' ';	/* falls through on purpose */
		break;
	}

	if (pDAO) *pDAO = DAO;
	else free(DAO);
	return OutBuf;
}

TCHAR *APRSAltitude(BOOL Valid, double Alt, char Next)
{static TCHAR OutBuf[30];

	if (!Valid || Alt <= 0.0) return TEXT("");

	StringCbPrintf(OutBuf, sizeof(OutBuf), TEXT("/A=%06ld%S"), (long) (Alt * 3.2808399), isdigit(Next&0xff)?" ":"");
	return OutBuf;
}

TCHAR *APRSHeadSpeed(BOOL Valid, double Hdg, double Spd)
{static TCHAR OutBuf[30];

	if (!Valid) return TEXT("");
	if (Hdg < 0.5) Hdg = 360.0;	/* 0 is special for fixed position objects, 360 = North */
	StringCbPrintf(OutBuf, sizeof(OutBuf), TEXT("%03ld/%03ld"), (long) (Hdg+0.5), (long) (Spd+0.5));
	return OutBuf;
}

double MultiLineScale(char s)
{	return pow(10,(s-33)/20.0)*0.0001;
}

char *CoordTrackToMultiLine(char *What, double lat, double lon, int TrackCount, TRACK_INFO_S *Tracks, int MaxLen, BOOL Close)
{	int t;
	double nowLat=lat, nowLon=lon;
	double MaxOff=0, MinOff=0;
	double Scale1=0, Scale2=0, Scale=0;
	long IntScale, reqLat=1, reqLon=1;
	char *Result = (char*)malloc(1+(TrackCount+1)*2+1+1);
	char *p = Result;

	if (TrackCount+(Close?1:0) > 23	/* Spec max count */
	|| 1+(TrackCount+(Close?1:0))*2+2 > MaxLen)	/* or too long */
		return NULL;

	for (t=0; t<TrackCount; t++)
	{	double latOff = -(lat - Tracks[t].pCoord->lat);
		double lonOff = (lon - Tracks[t].pCoord->lon);
		if (latOff > 90) latOff -= 180;
		if (latOff < -90) latOff += 180;
		if (lonOff > 180) lonOff -= 360;
		if (lonOff < -180) lonOff += 360;
		double latDlt = -(nowLat - Tracks[t].pCoord->lat);
		double lonDlt = (nowLon - Tracks[t].pCoord->lon);
		char b91Lat[4], b91Lon[4];
		int latDigs = MakeBase91((long)fabs(latDlt*380926)*2,sizeof(b91Lat),b91Lat);
		int lonDigs = MakeBase91((long)fabs(lonDlt*190463)*2,sizeof(b91Lon),b91Lon);
		if (latDigs > reqLat) reqLat = latDigs;
		if (lonDigs > reqLon) reqLon = lonDigs;
		TraceLogThread("Track2ML", FALSE, "%s:Track[%ld] Offset %.8lf %.8lf Comp(%ld %ld) B91(%.*s %.*s) or %ld %ld\n",
						What, (long) t, latDlt, lonDlt,
						(long)(latDlt*380926), (long)(lonDlt*190463),
						4, b91Lat, 4, b91Lon,
						(long) latDigs, (long) lonDigs);
		if (latOff > MaxOff) MaxOff = latOff;
		if (lonOff > MaxOff) MaxOff = lonOff;
		if (latOff < MinOff) MinOff = latOff;
		if (lonOff < MinOff) MinOff = lonOff;
		nowLat = Tracks[t].pCoord->lat;
		nowLon = Tracks[t].pCoord->lon;
	}
	TraceLogThread("Track2ML", FALSE, "%s:MaxOff %.8lf (%ld) MinOff %.8lf (%ld) Requires %ld and %ld Digits\n",
					What, MaxOff, (long) (MaxOff/0.0001),
					MinOff, (long) (MinOff/0.0001),
					(long) reqLat, (long) reqLon);

	Scale1 = (MaxOff / 44.0); Scale2 = (-MinOff / 45.0);
	Scale = Scale1>Scale2?Scale1:Scale2;

	TraceLogThread("Track2ML", FALSE, "%s:+Scale %.8lf -Scale %.8lf =Scale %.8lf\n",
					What, Scale1, Scale2, Scale);

	if (Scale<0.0001)
	{	TraceLogThread("Track2ML", FALSE, "%s:Scale(%.8lf) < 0.0001, not doing log\n",
						What, (double) Scale);
		return NULL;	/* Can't take log(0) */
	}

	IntScale = (long) (log10(Scale/0.0001)*20);
	if (IntScale > 91)
	{	TraceLogThread("Track2ML", TRUE, "%s:Scale(%.8lf) Gives Int(%ld), Truncating to 91\n",
						What, (double) Scale, (long) IntScale);
		IntScale = 91;
	}
	*p++ = (char) IntScale+33;

	Scale1 = MultiLineScale(*Result);
	TraceLogThread("Track2ML", FALSE, "%s:Scale %.8lf or %.8lf is %ld giving %.8lf\n",
					What, Scale, Scale/0.0001, IntScale, Scale1);

	for (t=0; t<TrackCount; t++)
	{	double latOff = -(lat - Tracks[t].pCoord->lat);
		double lonOff = (lon - Tracks[t].pCoord->lon);
		if (latOff > 90) latOff -= 180;
		if (latOff < -90) latOff += 180;
		if (lonOff > 180) lonOff -= 360;
		if (lonOff < -180) lonOff += 360;
		double latNew = ((long)(latOff/Scale)) * Scale1;
		double lonNew = ((long)(lonOff/Scale)) * Scale1;
		TraceLogThread("Track2ML", FALSE, "%s:Track[%ld] Offset %.8lf %.8lf or %ld %ld giving %.8lf %.8lf or %.8lf %.8lf Error\n",
						What, (long) t, latOff, lonOff,
						(long) (latOff/Scale), (long) (lonOff/Scale),
						latNew, lonNew,
						latOff-latNew, lonOff-lonNew);
		*p++ = ((char)(latOff/Scale))+78;
		*p++ = ((char)(lonOff/Scale))+78;
	}
	if (Close	/* If not already closed, close it */
	&& (p[-2] != Result[1] || p[-1] != Result[2]))
	{	*p++ = Result[1];
		*p++ = Result[2];
	}
	*p++ = '{';	/* !DAO! will end up after this */
	*p++ = '\0';

	return Result;
}

char *CoordTrackToCoordMultiLine(char *What, double lat, double lon, int TrackCount, TRACK_INFO_S *Tracks, int MaxLen, BOOL Close)
{	int t;
	char *Result = (char*)malloc(1+(TrackCount+1)*8+2+1);
	char *p = Result;

	TraceLogThread("Track2ML", TRUE, "CoordTrackToCoordMultiLine:%s at %.4lf %.4lf Doing %ld points into %ld bytes%s\n",
						What, lat, lon,
						TrackCount, MaxLen, Close?" CLOSED":"");

	if (TrackCount+(Close?1:0) > 35	/* Max base 36 */
	|| (TrackCount+(Close?1:0))*8+3 > MaxLen)	/* Or too long */
	{	TraceLogThread("Track2ML", TRUE, "CoordTrackToVariableMultiLine:Too Many Tracks %ld%s > 35 or Length %ld > %ld\n",
						TrackCount, Close?"+1":"",
						(TrackCount+(Close?1:0))*8+3, MaxLen);
		return NULL;
	}

	for (t=0; t<TrackCount; t++)
	{	MakeBase91((long)(380926*(90-Tracks[t].pCoord->lat)),
					4,p);
		MakeBase91((long)(190463*(180+Tracks[t].pCoord->lon)),
					4,p+4);
		p += 8;
	}
	if (Close)
	{	memcpy(p, Result, 8);	/* Copy starting point to end */
		p += 8;
	}
	*p++ = '{';
	*p = TrackCount+(Close?1:0);
	if (*p > 9) *p++ += 'A'-10;	/* Base 36 */
	else *p++ += '0';	/* Digit */
	*p++ = '\0';

	return Result;
}

char *CoordTrackToVariableMultiLine(char *What, double lat, double lon, int TrackCount, TRACK_INFO_S *Tracks, int MaxLen, BOOL Close)
{	int t;
	double nowLat=lat, nowLon=lon;
	long LatLen=1, LonLen=1, LatMid, LonMid;
	char *Result = (char*)malloc((TrackCount+1)*8+3+1);
	char *p = Result;

	TraceLogThread("Track2ML", TRUE, "CoordTrackToVariableMultiLine:%s at %.4lf %.4lf Doing %ld points into %ld bytes%s\n",
						What, lat, lon,
						TrackCount, MaxLen, Close?" CLOSED":"");

	if (TrackCount+(Close?1:0) > 35)	/* Max base 36 */
	{	TraceLogThread("Track2ML", TRUE, "CoordTrackToVariableMultiLine:Too Many Tracks %ld%s > 35\n",
						TrackCount, Close?"+1":"");
		return NULL;
	}

	double log91 = log(91.0);
	char b91Lat[5], b91Lon[5];
	char b92Lat[4], b92Lon[4];
	LatMid = (long) pow(91.0,(int)sizeof(b92Lat))/2;
	LonMid = (long) pow(91.0,(int)sizeof(b92Lon))/2;

	for (t=0; t<TrackCount; t++)
	{	double latDlt = -(nowLat - Tracks[t].pCoord->lat);
		double lonDlt = (nowLon - Tracks[t].pCoord->lon);
		int latDigs = ((long)(latDlt*380926))==0?1:(int) ceil(log(fabs(latDlt*380926)*2)/log91);
		int lonDigs = ((long)(lonDlt*190463))==0?1:(int) ceil(log(fabs(lonDlt*190463)*2)/log91);
		int wasLat = MakeBase91((long)fabs(latDlt*380926)*2,sizeof(b91Lat),b91Lat);
		int wasLon = MakeBase91((long)fabs(lonDlt*190463)*2,sizeof(b91Lon),b91Lon);

		int tstLat = MakeBase91((long)(latDlt*380926)+LatMid,sizeof(b92Lat),b92Lat);
		int tstLon = MakeBase91((long)(lonDlt*190463)+LonMid,sizeof(b92Lon),b92Lon);

		if (latDigs > LatLen) LatLen = latDigs;
		if (lonDigs > LonLen) LonLen = lonDigs;
		TraceLogThread("Track2ML", FALSE, "Track[%ld] @%p->%p %.4lf %.4lf Offset %.8lf %.8lf Comp(%ld %ld) Mid(%ld %ld) B91(%.*s %.*s) or %ld %ld Was:%ld %ld B92(%.*s %.*s) Dig:%ld %ld\n",
						(long) t,
						&Tracks[t], Tracks[t].pCoord,
						Tracks[t].pCoord->lat, Tracks[t].pCoord->lon,
						latDlt, lonDlt,
						(long)(latDlt*380926), (long)(lonDlt*190463),
						(long) (latDlt*380926)+LatMid, (long) (lonDlt*380926)+LonMid,
						sizeof(b91Lat), b91Lat, sizeof(b91Lon), b91Lon,
						(long) latDigs, (long) lonDigs,
						(long) wasLat, (long) wasLon,
						sizeof(b92Lat), b92Lat, sizeof(b92Lon), b92Lon,
						(long) tstLat, (long) tstLon);

		signed long sslat, sslon;

		if (!newbase91decode(b92Lat, sizeof(b92Lat), &sslat)
		|| !newbase91decode(b92Lon, sizeof(b92Lon), &sslon))
		{
TraceLogThread("Track2ML", TRUE, "Compressed(%.*s) Or(%.*s) INVALID\n", sizeof(b92Lat),b92Lat,sizeof(b92Lon),b92Lon);
		} else
		{	double offLat = (sslat-LatMid) / 380926.0;
			double offLon = (sslon-LonMid) / 190463.0;
TraceLogThread("Track2ML", FALSE, "Track[%ld] Offset %.8lf %.8lf Actual %.8lf %.8lf Error %.8lf %.8lf\n",
				(long) t,
				latDlt, lonDlt,
				offLat, offLon,
				Tracks[t].pCoord->lat-(nowLat+offLat),
				Tracks[t].pCoord->lon-(nowLon-offLon));
				nowLat += offLat;
				nowLon -= offLon;
		}
//		nowLat = Tracks[t].pCoord->lat;
//		nowLon = Tracks[t].pCoord->lon;
	}

	LatMid = (long) pow(91.0,LatLen)/2;
	LonMid = (long) pow(91.0,LonLen)/2;
	TraceLogThread("Track2ML", FALSE, "LatMid:%ld(%ld) LonMid:%ld(%ld)\n",
					(long) LatMid, (long) LatLen,
					(long) LonMid, (long) LonLen);

	if (LatLen + LonLen >= 8
	|| (TrackCount+(Close?1:0))*(LatLen+LonLen)+3 > MaxLen)	/* Or too long */
		return CoordTrackToCoordMultiLine(What, lat, lon, TrackCount, Tracks, MaxLen, Close);

	nowLat=lat;
	nowLon=lon;
	for (t=0; t<TrackCount; t++)
	{	double latDlt = -(nowLat - Tracks[t].pCoord->lat);
		double lonDlt = (nowLon - Tracks[t].pCoord->lon);

TraceLogThread("Track2ML", FALSE, "Dlt[%ld] %.8lf %.8lf => %ld/%ld %ld/%ld\n", (long) t,
		latDlt, lonDlt,
		(long)(latDlt*380926)+LatMid, LatMid*2,
		(long)(lonDlt*190463)+LonMid, LonMid*2);

		MakeBase91((long)(latDlt*380926)+LatMid,LatLen,p);
		MakeBase91((long)(lonDlt*190463)+LonMid,LonLen,p+LatLen);

		signed long sslat=0, sslon=0;

		newbase91decode(p, LatLen, &sslat);
		newbase91decode(p+LatLen, LonLen, &sslon);
		double offLat = (sslat-LatMid) / 380926.0;
		double offLon = (sslon-LonMid) / 190463.0;

		p += LatLen + LonLen;
		nowLat += offLat;
		nowLon -= offLon;
TraceLogThread("Track2ML", FALSE, "Track[%ld] Error %.8lf %.8lf %s\n",
				(long) t,
				Tracks[t].pCoord->lat-nowLat,
				Tracks[t].pCoord->lon-nowLon,
				AreCoordinatesEquivalent(Tracks[t].pCoord->lat, Tracks[t].pCoord->lon, nowLat, nowLon, 0)?"Equiv":"DIFFERENT!");

	}
	if (Close)
	{	memcpy(p, Result, LatLen+LonLen);	/* Copy starting point to end */
		p += LatLen+LonLen;
	}
	*p++ = '{';
	*p = TrackCount+(Close?1:0);
	if (*p > 9) *p++ += 'A'-10;	/* Base 36 */
	else *p++ += '0';	/* Digit */
	*p++ = (char) LatLen + '0';
	*p++ = (char) LonLen + '0';
	*p++ = '\0';

	return Result;
}

#ifdef UNDER_CE
int strnlen(char *p, int l)
{	int r;
	for (r=0; r<l; r++)
		if (!*p) break;;
	return r;
}
#endif

HRESULT StringCbPrintExUTF8
(	LPTSTR pszDest,
    size_t cbDest,
    LPTSTR *ppszDestEnd,
    size_t *pcbRemaining,
	size_t UTF8Len,
	char *UTF8Buf,
	char *Trailer	/* May be NULL */
)
{	size_t Result;

	memset(pszDest, 0, cbDest);	/* null fill target */

	if (!ppszDestEnd) ppszDestEnd = &pszDest;
	if (!pcbRemaining) pcbRemaining = &cbDest;

	if (!UTF8Buf) UTF8Buf = "";
	if (UTF8Len == -1) UTF8Len = strlen(UTF8Buf);
	else UTF8Len = strnlen(UTF8Buf, UTF8Len);

	Result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
										UTF8Buf, UTF8Len,
										pszDest, cbDest/sizeof(TCHAR));
	if (Result)
	{	*ppszDestEnd = pszDest + Result;		/* Add number of wide characters */
		*pcbRemaining -= Result*sizeof(TCHAR);	/* Subtract off consumed bytes */
		Result = S_OK;
	} else	/* MultiToWide failed, do it the old way */
	{	Result = StringCbPrintfEx(pszDest, cbDest, ppszDestEnd, pcbRemaining, 
								STRSAFE_IGNORE_NULLS, TEXT("%.*S"), UTF8Len, UTF8Buf);
	}
	if (Result == S_OK && Trailer && *Trailer)
		Result = StringCbPrintfEx(*ppszDestEnd, *pcbRemaining, ppszDestEnd, pcbRemaining,
								STRSAFE_IGNORE_NULLS, TEXT("%S"), Trailer);
	return Result;
}

int RtStrnlen
(	int Size,
	char * String
)
{
register int i;

	if (!String) return 0;

	if (Size == -1) return strlen(String);

	for (i=0; i<Size; i++)
	{	if (!String[i])
			break;
	}
	return i;
}

char * RtStrnupr
(	int Size,	/* -1 for null terminated */
	char * String	/* String to convert to uppercase */
)
{
register int i;

	if (!String) return String;
	if (Size == -1) return _strupr(String);

	for (i=0; i<Size && String[i]; i++)
	{	if (islower(String[i] & 0xff))
			String[i] = toupper(String[i]);
	}
	return String;
}

char * RtStrnTrim
(	int Len,	/* -1 for null terminated */
	char * String
)
{	int i;
	if (!String) return String;
	Len = RtStrnlen(Len,String);
	for (i=0; i<Len; i++)
		if (!isspace(String[i] & 0xff))
			break;
	if (i) memmove(String,String+i,Len-i);
	for (i=Len-i-1; i>=0; i--)
		if (String[i] && !isspace(String[i] & 0xff))
			break;
	i++;
	memset(String+i,0,Len-i);
	return String;
}

char * RtStrnuprTrim
(	int Len,	/* -1 for null terminated */
	char * String
)
{	if (!String) return String;
	RtStrnTrim(Len,String);
	return RtStrnupr(Len,String);
}

char *LocateFilename(char *Path)
{
	char *Name1 = strrchr(Path,'\\');
	char *Name2 = strrchr(Path,'/');
	if (Name1) Name1++; if (Name2) Name2++;	/* Skip over slashes */
	if (!Name1 || Name2 > Name1) Name1 = Name2;
	if (!Name1) Name1 = Path;
	return Name1;
}

__int64 DeltaSeconds(SYSTEMTIME *stStart, SYSTEMTIME *stEnd)
{
	FILETIME ftEnd, ftCheck;
	__int64 ullEnd, Elapsed;

	SystemTimeToFileTime(stEnd, &ftEnd);

	ullEnd = ((__int64) ftEnd.dwHighDateTime)<<32 | ftEnd.dwLowDateTime;

	SystemTimeToFileTime(stStart, &ftCheck);

	Elapsed = ((__int64) ftCheck.dwHighDateTime)<<32 | ftCheck.dwLowDateTime;
	Elapsed = ullEnd - Elapsed;	/* Delta time before now */
	Elapsed /= 10;	/* 1000 nanosec = microsec */
	Elapsed /= 1000;	/* 1000 microsec = millisec */
	Elapsed /= 1000;	/* 1000 millisec = seconds */

	return Elapsed;
}

__int64 SecondsSince(SYSTEMTIME *st)
{
	SYSTEMTIME stNow;
	GetSystemTime(&stNow);
	return DeltaSeconds(st, &stNow);
}

__int64 LocalSecondsSince(SYSTEMTIME *st)
{
	SYSTEMTIME stNow;
	GetLocalTime(&stNow);
	return DeltaSeconds(st, &stNow);
}

BOOL IsSystemTimeout(SYSTEMTIME *stLast, unsigned long Seconds)
{	return SecondsSince(stLast) >= Seconds;
}

void OffsetSystemTime(SYSTEMTIME *st, unsigned long Seconds)
{
	FILETIME ftEnd;
	__int64 ullEnd;

	SystemTimeToFileTime(st, &ftEnd);

	ullEnd = ((__int64) ftEnd.dwHighDateTime)<<32 | ftEnd.dwLowDateTime;

	ullEnd += ((__int64)Seconds)*10*1000*1000;	/* 10*100nsec = microsec */
												/* 1000 microsec = millisec */
												/* 1000 millisec = second */

	ftEnd.dwHighDateTime = (ullEnd >> 32) & 0xffffffff;
	ftEnd.dwLowDateTime = ullEnd & 0xffffffff;

	FileTimeToSystemTime(&ftEnd, st);
}

char * FormatDeltaTime(__int64 Age, char *Next, size_t Remaining, char **pNext, size_t *pRemaining)
{	char *Return = Next;
	long Days = (long) (Age / (24*60*60));	/* Days */
	Age = Age % (24*60*60);
	long Hours = (long) (Age / (60*60));	/* Hours */
	Age = Age % (60*60);
	long Minutes = (long) (Age / 60);	/* Minutes */
	long Seconds = (long) (Age % 60);

	if (!pNext) pNext = &Next;
	if (!pRemaining) pRemaining = &Remaining;

	if (Days)	/* Got Days? */
	{	StringCbPrintfExA(Next, Remaining, pNext, pRemaining,
						STRSAFE_IGNORE_NULLS, 
						"%ldd %ldh%02ldm",
						(long) Days,
						(long) Hours, (long) Minutes);
	} else if (Hours)
	{	StringCbPrintfExA(Next, Remaining, pNext, pRemaining,
						STRSAFE_IGNORE_NULLS, 
						"%ldh",
						(long) Hours);
		if (Minutes)
			StringCbPrintfExA(*pNext, *pRemaining, pNext, pRemaining,
						STRSAFE_IGNORE_NULLS, 
						"%02ldm",
						(long) Minutes);
	} else if (Minutes)
	{	StringCbPrintfExA(Next, Remaining, pNext, pRemaining,
						STRSAFE_IGNORE_NULLS, 
						"%ldm",
						(long) Minutes);
		if (Seconds)
			StringCbPrintfExA(*pNext, *pRemaining, pNext, pRemaining,
						STRSAFE_IGNORE_NULLS, 
						"%lds",
						(long) Seconds);
	} else if (Seconds)
	{	StringCbPrintfExA(Next, Remaining, pNext, pRemaining,
						STRSAFE_IGNORE_NULLS, 
						"%lds",
						(long) Seconds);
	} else StringCbPrintfExA(Next, Remaining, pNext, pRemaining,
						STRSAFE_IGNORE_NULLS, 
						"NOW!");
	return Return;
}
