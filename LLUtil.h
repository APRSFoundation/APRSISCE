#ifndef GOT_LLUTIL_H
#define GOT_LLUTIL_H
#ifdef __cplusplus
extern "C"
{
#endif

#define EarthRadius 3958.76	/* Miles */
#define DegreesPerRadian 57.2957795
#define InHgPerMb 0.02952998
#define MmPerInch 25.4
#define FeetPerMeter 3.2808399
#define KmPerMile 1.609344
#define KphPerKnot 1.852
#define KmPerNM 1.85200
#define MilePerNM 1.15077945
	 	
#include "text.h"

/* Prototypes generated Fri Aug  8 08:50:30 2008 */
double DegToRad(double deg) ;
double Rad2Deg(double rad) ;
char *GetCompassPoint8(int Degrees);
TCHAR *GetCompassPoint(int Degrees);
TCHAR *GetPrettyDistance(double miles);
int MakeBase91(long v, int digits, char *buff) ;
void AprsHaversinePos(GPS_POSITION *From, GPS_POSITION *To, double *Dist, double *Bearing);
void AprsHaversineLatLon(double lat1, double lon1, double lat2, double lon2, double *Dist, double *Bearing);
void AprsProjectLatLon(double fLat, double fLon, double Dist, double Bearing, double *ptLat, double *ptLon);
void AprsProjectWaypoint(GPS_POSITION *From, double Dist, double Bearing, GPS_POSITION *To);
//TCHAR *FormatLatLon(double LatLon, const TCHAR *PNstring);
BOOL AreCoordinatesEquivalent(double Lat1, double Lon1, double Lat2, double Lon2, int daoDigits);
char *GridSquare(double Lat, double Lon, int Pairs);
void GridSquare2LatLon(char *GridSquare, double *pLat, double *pLon);
char *APRSCompressed(GPS_POSITION *Pos, BOOL NewPos, char Table, char Code, BOOL CourseSpeed, BOOL Altitude);
TCHAR *APRSCompressLatLon(double Lat, double Lon, char Table, char Code, BOOL CourseSpeed, double Course, double Speed, BOOL Altitude, double Alt);
#ifdef __cplusplus
TCHAR *APRSLatLon(double Lat, double Lon, char Table, char Code, int addDigits, int daoDigits=0, char **pDAO=NULL);
char * FormatDeltaTime(__int64 Age, char *Next, size_t Remaining, char **pNext=NULL, size_t *pRemaining=NULL);
#else
TCHAR *APRSLatLon(double Lat, double Lon, char Table, char Code, int addDigits, int daoDigits, char **pDAO);
char * FormatDeltaTime(__int64 Age, char *Next, size_t Remaining, char **pNext, size_t *pRemaining);
#endif
TCHAR *APRSAltitude(BOOL Valid, double Alt, char Next);
TCHAR *APRSHeadSpeed(BOOL Valid, double Hdg, double Spd);
__int64 DeltaSeconds(SYSTEMTIME *stStart, SYSTEMTIME *stEnd);
__int64 SecondsSince(SYSTEMTIME *st);
__int64 LocalSecondsSince(SYSTEMTIME *st);
BOOL IsSystemTimeout(SYSTEMTIME *stLast, unsigned long Seconds);
void OffsetSystemTime(SYSTEMTIME *st, unsigned long Seconds);

HRESULT StringCbPrintExUTF8
(	LPTSTR pszDest,
    size_t cbDest,
    LPTSTR *ppszDestEnd,
    size_t *pcbRemaining,
	size_t UTF8Len,
	char *UTF8Buf,
	char *Trailer	/* May be NULL */
);

int RtStrnlen
(	int Size,
	char * String
);
char * RtStrnupr
(	int Size,	/* -1 for null terminated */
	char * String	/* String to convert to uppercase */
);
char * RtStrnTrim
(	int Len,	/* -1 for null terminated */
	char * String
);
char * RtStrnuprTrim
(	int Len,	/* -1 for null terminated */
	char * String
);
char *LocateFilename(char *Path);

#ifndef STRING
#define STRING(s) sizeof(s),(s)
#endif

#define DEG2LONG_MULT 10000000.0
#define DEG2LONG(d) ((long)((d)*DEG2LONG_MULT))
#define LONG2DEG(l) (((double)l)/DEG2LONG_MULT)
//#define DEG2LONG(d) d
//#define LONG2DEG(l) l

double MultiLineScale(char s);

char *CoordTrackToMultiLine(char *What, double lat, double lon, int TrackCount, TRACK_INFO_S *Tracks, int MaxLen, BOOL Close);
char *CoordTrackToCoordMultiLine(char *What, double lat, double lon, int TrackCount, TRACK_INFO_S *Tracks, int MaxLen, BOOL Close);
char *CoordTrackToVariableMultiLine(char *What, double lat, double lon, int TrackCount, TRACK_INFO_S *Tracks, int MaxLen, BOOL Close);

#ifdef __cplusplus
}
#endif

#endif
