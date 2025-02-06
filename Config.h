#ifndef GOT_CONFIG_H
#define GOT_CONFIG_H

#include "sysheads.h"

//#include "llutil.h"	/* For several structure definitions */

#ifdef EXTREME_FUTURE
#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif
#endif

#define MAX_RANGERS 6

#ifdef UNDER_CE
#ifdef CE50
#define MAX_TRACKERS 2
#else
#define MAX_TRACKERS 1
#endif
#else
#define MAX_TRACKERS 16
#endif

#ifdef UNDER_CE
#ifndef CE50
#ifndef NO_SHELL
#define USING_COMM_MGR
#define USING_GPSAPI
#define USING_POWER
#define USING_SHELL
#define USING_SIP
#define MONITOR_PHONE
#endif
#endif

#define USING_CHAT

#else
//#define USING_SIP
#define USING_CHAT
#endif

//#ifndef USING_SIP
//#define SipShowIM(SIPF_ON)
//#endif

#define SUPPORT_SHAPEFILES
#define SUPPORT_POLYGON_FILL
//#ifndef UNDER_CE
//#define SUPPORT_AREA_FILL
//#else
//#define SUPPORT_RECTANGLE_FILL
//#endif

#ifdef UNDER_CE
#define strdup(s) strcpy((char*)malloc(strlen(s)+1),s)
#endif
#ifdef USING_GPSAPI
#include <gpsapi.h>
#else
//#include <strsafe.h>

#define GPS_MAX_SATELLITES      24
#define GPS_MAX_PREFIX_NAME     16
#define GPS_MAX_FRIENDLY_NAME   64

#define GPS_VERSION_1           1
#define GPS_VERSION_CURRENT     GPS_VERSION_1

typedef enum {
	GPS_FIX_UNKNOWN = 0,
	GPS_FIX_2D,
	GPS_FIX_3D
}
GPS_FIX_TYPE;

typedef enum {
	GPS_FIX_SELECTION_UNKNOWN = 0,
	GPS_FIX_SELECTION_AUTO,
	GPS_FIX_SELECTION_MANUAL
}
GPS_FIX_SELECTION;

typedef enum {
	GPS_FIX_QUALITY_UNKNOWN = 0,
	GPS_FIX_QUALITY_GPS,
	GPS_FIX_QUALITY_DGPS
}
GPS_FIX_QUALITY;

//
// GPS_VALID_XXX bit flags in GPS_POSITION structure are valid.
//
#define GPS_VALID_UTC_TIME                                 0x00000001
#define GPS_VALID_LATITUDE                                 0x00000002
#define GPS_VALID_LONGITUDE                                0x00000004
#define GPS_VALID_SPEED                                    0x00000008
#define GPS_VALID_HEADING                                  0x00000010
#define GPS_VALID_MAGNETIC_VARIATION                       0x00000020
#define GPS_VALID_ALTITUDE_WRT_SEA_LEVEL                   0x00000040
#define GPS_VALID_ALTITUDE_WRT_ELLIPSOID                   0x00000080
#define GPS_VALID_POSITION_DILUTION_OF_PRECISION           0x00000100
#define GPS_VALID_HORIZONTAL_DILUTION_OF_PRECISION         0x00000200
#define GPS_VALID_VERTICAL_DILUTION_OF_PRECISION           0x00000400
#define GPS_VALID_SATELLITE_COUNT                          0x00000800
#define GPS_VALID_SATELLITES_USED_PRNS                     0x00001000
#define GPS_VALID_SATELLITES_IN_VIEW                       0x00002000
#define GPS_VALID_SATELLITES_IN_VIEW_PRNS                  0x00004000
#define GPS_VALID_SATELLITES_IN_VIEW_ELEVATION             0x00008000
#define GPS_VALID_SATELLITES_IN_VIEW_AZIMUTH               0x00010000
#define GPS_VALID_SATELLITES_IN_VIEW_SIGNAL_TO_NOISE_RATIO 0x00020000


//
// GPS_DATA_FLAGS_XXX bit flags set in GPS_POSITION dwFlags field
// provide additional information about the state of the query.
// 

// Set when GPS hardware is not connected to GPSID and we 
// are returning cached data.
#define GPS_DATA_FLAGS_HARDWARE_OFF                        0x00000001

//
// GPS_POSITION contains our latest physical coordinates, the time, 
// and satellites used in determining these coordinates.
// 
typedef struct _GPS_POSITION {
	DWORD dwVersion;             // Current version of GPSID client is using.
	DWORD dwSize;                // sizeof(_GPS_POSITION)

	// Not all fields in the structure below are guaranteed to be valid.  
	// Which fields are valid depend on GPS device being used, how stale the API allows
	// the data to be, and current signal.
	// Valid fields are specified in dwValidFields, based on GPS_VALID_XXX flags.
	DWORD dwValidFields;

	// Additional information about this location structure (GPS_DATA_FLAGS_XXX)
	DWORD dwFlags;
	
	//** Time related
	SYSTEMTIME stUTCTime; 	//  UTC according to GPS clock.
	
	//** Position + heading related
	double dblLatitude;            // Degrees latitude.  North is positive
	double dblLongitude;           // Degrees longitude.  East is positive
	float  flSpeed;                // Speed in knots
	float  flHeading;              // Degrees heading (course made good).  True North=0
	double dblMagneticVariation;   // Magnetic variation.  East is positive
	float  flAltitudeWRTSeaLevel;  // Altitute with regards to sea level, in meters
	float  flAltitudeWRTEllipsoid; // Altitude with regards to ellipsoid, in meters

	//** Quality of this fix
	GPS_FIX_QUALITY     FixQuality;        // Where did we get fix from?
	GPS_FIX_TYPE        FixType;           // Is this 2d or 3d fix?
	GPS_FIX_SELECTION   SelectionType;     // Auto or manual selection between 2d or 3d mode
	float flPositionDilutionOfPrecision;   // Position Dilution Of Precision
	float flHorizontalDilutionOfPrecision; // Horizontal Dilution Of Precision
	float flVerticalDilutionOfPrecision;   // Vertical Dilution Of Precision

	//** Satellite information
	DWORD dwSatelliteCount;                                            // Number of satellites used in solution
	DWORD rgdwSatellitesUsedPRNs[GPS_MAX_SATELLITES];                  // PRN numbers of satellites used in the solution

	DWORD dwSatellitesInView;                      	                   // Number of satellites in view.  From 0-GPS_MAX_SATELLITES
	DWORD rgdwSatellitesInViewPRNs[GPS_MAX_SATELLITES];                // PRN numbers of satellites in view
	DWORD rgdwSatellitesInViewElevation[GPS_MAX_SATELLITES];           // Elevation of each satellite in view
	DWORD rgdwSatellitesInViewAzimuth[GPS_MAX_SATELLITES];             // Azimuth of each satellite in view
	DWORD rgdwSatellitesInViewSignalToNoiseRatio[GPS_MAX_SATELLITES];  // Signal to noise ratio of each satellite in view

	DWORD dwBadBuffers;
	DWORD dwBadChecksum;
	DWORD dwBadCommas;
	DWORD dwUnsupported;


} GPS_POSITION, *PGPS_POSITION;

#endif

#define VERSION Timestamp

#define LOCAL_MAX_HOPS 2	/* Count of used hops to consider "local" (0=direct only) */
#define RECENTLY_HEARD_MINUTES 30	/* Minutes to retain HeardOnRF status */
#define DUPLICATE_DETECT_SECONDS 30	/* Seconds to check for packet duplicates */

#define MINIMUM_APRS_DELTA (Genius->MinTime*1000)		/* at least 10 seconds */
#define MAXIMUM_APRS_DELTA (Genius->MaxTime*60.0*1000.0)		/* at most 30 minutes */
//#define MINIMUM_APRS_BEARING_CHANGE (Genius->BearingChange)		/* Degrees of turn before transmit */
//#define MAXIMUM_APRS_FORECAST_ERROR (Genius->ForecastError)		/* Transmit if forecast is 0.1 miles (528 feet) off */
//#define MAXIMUM_APRS_DISTANCE (Genius->MaxDistance)		/* at least once per mile as the crow flies! */
#define MAXIMUM_APRS_QUIET_TIME (ActiveConfig.QuietTime*1000.0)		/* Disconnect in 60 seconds of silence */
#define MOVING_SPEED (ActiveConfig.MoveSpeed)			/* Speed to be considered moving (filters GPS jitters) (in KNOTS) */
#define RANGE (ActiveConfig.Range*1.609344/10.0)		/* How far around we want to see in KM */
#define CALLSIGN (ActiveConfig.CallSign)
#define PASSWORD (ActiveConfig.Password)
#define PATH (ActiveConfig.BeaconPath)
#define FILTER (ActiveConfig.Filter)
#define COMMENT (ActiveConfig.Comment)
#define SYMBOL ActiveConfig.Symbol.Table,ActiveConfig.Symbol.Symbol
#define MAX_STATION_LABELS (ActiveConfig.View.VisibleLabelsMax)

#define COLOR_SIZE 32

#define MIN_SETTABLE_ZOOM 14	/* Minimum zoom to set a current location */

//#define DESTID "APZA4C"	/* Experimental Aprs 4 Ce */

/* 00 - Pre 2010/03/16 */
/* 01 - 2010/03/16 */
/* 02 - 2010/03/25 */
/* 03 - 2010/06/16 */
/* 04 - 2010/08/20 */
/* 05 - 2010/10/03 */
/* 06 - 2011/01/23 */
/* 07 - 2011/02/10 */
/* 08 - 2011/05/02 */
/* 09 - 2012/02/29 */
/* 10 - 2012/05/24 */
#define APVER "10"
#define DESTWM "APWM"APVER	/* APRSISCE for Windows Mobile */
#define DEST32 "APWW"APVER	/* APRSISCE for Win32 */

#ifdef TRAKVIEW
#define DESTID "APZTKV"
#define PROGNAME "TRAKVIEW32"
#define PROGCALL "TRAKVW-32"
#else

#ifdef UNDER_CE
#define DESTID (ActiveConfig.AltNet[0]?ActiveConfig.AltNet:DESTWM)	/* APRSISCE for Windows Mobile */
#ifdef CE50
#ifdef SYLVANIA
#define PROGNAME "APRSISCE6Syl"
#else
#define PROGNAME "APRSISCE5x86"
#endif
#else
#define PROGNAME "APRSISCE6P"
#endif
#define PROGCALL "APRSIS-CE"
#else
#define DESTID (ActiveConfig.AltNet[0]?ActiveConfig.AltNet:DEST32)	/* APRSISCE for Win32 */
#define PROGNAME "APRSIS32"
#define PROGCALL "APRSIS-32"
#endif

#endif

/*
#
# JUST MOBILE SYMBOLS: Use these symbols to select only mobile/portable stations for display.
. . . Primary: '<=>()*0COPRSUXY[^abefgjkpsuv . Alternate: /0>AKOS^knsuv
# JUST WEATHER SYMBOLS: Primary: _ and W and Alternate: ([*:<@BDEFGHIJTUW_efgptwy{
*/
#define SYM_PHONE	'/','$'
#define SYM_HOUSE	'/','-'
#define SYM_CAR		'/','>'
#define SYM_BUS		'/','U'
#define SYM_JEEP	'/','j'
#define SYM_TRUCK	'/','k'
#define SYM_VAN		'/','v'
#define SYM_LOCOMOTIVE '/','='
#define SYM_CANOE	'/','C'
#define SYM_BALLOON	'/','O'
#define SYM_BICYCLE	'/','b'
#define SYM_FILESVR	'/','?'
#define SYM_LAPTOP	'/','l'

#ifdef UNDER_CE
#define MY_SYMBOL	SYM_PHONE
#else
#define MY_SYMBOL	SYM_LAPTOP
//#define MY_SYMBOL 'S','v'
#endif

typedef struct TIMED_STRING_S
{	SYSTEMTIME time;
	char *string;
	long value;
} TIMED_STRING_S;

typedef struct TIMED_STRING_LIST_S
{	unsigned long Count;
	unsigned long Size;	/* For chunking allocations */
	TIMED_STRING_S *Entries;
} TIMED_STRING_LIST_S;

#define USE_TIMED_STRINGS
#ifdef USE_TIMED_STRINGS
typedef TIMED_STRING_LIST_S STRING_LIST_S;
#else
typedef struct STRING_LIST_S
{	unsigned long Count;
	char **Strings;
} STRING_LIST_S;
#endif

typedef struct SYMBOL_INFO_S
{	char Table;
	char Symbol;
} SYMBOL_INFO_S;

typedef struct GENIUS_INFO_S
{	unsigned long MinTime;
	unsigned long MaxTime;
	BOOL TimeOnly;
	BOOL StartStop;
	unsigned long BearingChange;
	double ForecastError;
	double MaxDistance;
/*	The following is used for the delta calculations */
	SYSTEMTIME stLastAPRS;
	__int64 LastAPRSUpdate;
	__int64 msLastComment;
	GPS_POSITION LastAPRSPosition;
	GPS_POSITION ProjectedPosition;
	BOOL coordinatesMoved;	/* TRUE if coordinates move by enough precision digits */
	double forecastDistance, projDistance, projBearing;
	double deltaDistance, deltaBearing, deltaHeading;
} GENIUS_INFO_S;

typedef struct COMPANION_INFO_S
{	char Name[16];
	char Comment[128];
	GENIUS_INFO_S Genius;
	SYMBOL_INFO_S Symbol;
	BOOL Enabled;
	BOOL Object;
	BOOL Posit;
	BOOL Messaging;
	BOOL RFEnabled;	/* TRUE to transmit via RF */
	BOOL ISEnabled;	/* TRUE to send via -IS */
	char RFPath[64];	/* Path to use via RF */
} COMPANION_INFO_S;

typedef struct RFID_CONFIG_INFO_S
{	BOOL AssocEnabled;
	BOOL ServerEnabled;
	char ServerName[16];
	STRING_LIST_S Readers;
} RFID_CONFIG_INFO_S;

typedef struct PORT_CONFIG_S
{	char	Port[32];
	long	Baud;
	int		Bits;
	int		Parity;
	int		Stop;
	int		Transmit;	/* Wants to see rasterized forms */
	char	IPorDNS[64];/* Also BT */
	long	TcpPort;
} PORT_CONFIG_S;

typedef struct PORT_CONFIG_INFO_S
{	char Name[64];
	char Protocol[16];	/* AGW, KISS, TEXT */
	char Device[64];	/* Com Port, or TCP spec, or whatever */
	STRING_LIST_S OpenCmds;	/* Set of UI-View-compatible port initialization strings */
	STRING_LIST_S CloseCmds;	/* Similar set of commands for closing */
	long RfBaud;	/* CrossPort-gating looks at this */
	unsigned long QuietTime;	/* Time in seconds between expected data */
	BOOL IsEnabled;		/* TRUE if enabled */
	BOOL NotRF;			/* TRUE if interface is NOT RF, Ignore IGate functions */
	BOOL RequiresInternet;	/* TRUE if the port requires Internet Enabled */
	BOOL RequiresFilter;	/* TRUE if filters changes needed */
	BOOL NoUsedPath;	/* TRUE if can't transmit a "Used" Path */
	BOOL XmitEnabled;	/* TRUE if transmit is enabled */
	BOOL ProvidesNMEA;	/* TRUE if NMEA data comes from here */
	BOOL RFtoISEnabled;	/* TRUE to gate to -IS */
	BOOL IStoRFEnabled;	/* TRUE to gate messages to RF */
	BOOL MyCallNot3rd;	/* TRUE to gate MyCall base as non-3rd party packet */
	BOOL NoGateME;		/* TRUE to not gate ME's packets from RF to -IS */
	BOOL DXEnabled;		/* TRUE to send DX cluster packets here */
	char DXPath[64];	/* Path for DX packets */
	BOOL BeaconingEnabled;	/* TRUE to send beacons here */
	char BeaconPath[64];
	BOOL MessagesEnabled;	/* TRUE to send messages here */
	char MessagePath[64];
	BOOL BulletinObjectEnabled;	/* TRUE to send bulletins & objects here */
	BOOL TelemetryEnabled;	/* TRUE to send telementry here */
	char TelemetryPath[64];
	STRING_LIST_S DigiXforms;	/* List of callsign-SSID=callsign-SSID, empty for no Digi */


	BOOL Inherit;	/* TRUE to inherit the following setting from general config */
	char CallSign[16];
	char Comment[128];
	GENIUS_INFO_S Genius;
	SYMBOL_INFO_S Symbol;
#ifdef FUTURE
	struct
	{	BOOL Timestamp;
		BOOL HHMMSS;
		BOOL Compressed;	/* TRUE if using base-91 Compressed */
		BOOL CourseSpeed;	/* Only if known */
		BOOL Altitude;	/* Only if positive */
		BOOL Why;		/* (Transmit Pressure String) */
		BOOL Comment;	/* I know, they can just blank it! */
		long Precision;	/* Number of digits */
	} Beacon;
#endif
} PORT_CONFIG_INFO_S;

/* From: http://tinyurl.com/433evuz
Tactical Callsigns

Assign tactical calls to any station on the map! The local display will show the tactical call instead of the callsign or object name until the tactical call is erased. You may assign "Team 1", "Air 2", etc, up to 57 characters per tactical call. Send an APRS message to "TACTICAL" with a tac-call assignment and every Xastir and APRS+SA station that decodes the message will display the same tactical callsign. The format for an assignment is in the body of the message:

"callsign-ssid=tactical call that is really long"

To de-assign a tactical call send this: "callsign-ssid="

To send multiple tactical calls to others, send this: "CALL1=TAC1;CALL2=TAC2;CALL3=TAC3"

Note that '=' and ';' characters cannot be in the TAC callsign. 
*/
/* From Xastir's db.c tactical_data_add
//
// Assign tactical callsigns based on messages sent to "TACTICAL"
//
//  *) To set your own tactical callsign and send it to others,
//     send an APRS message to "TACTICAL" with your callsign in
//     the message text.
//
//  *) To send multiple tactical calls to others, send an APRS
//     message to "TACTICAL" and enter:
//     "CALL1=TAC1;CALL2=TAC2;CALL3=TAC3" in the message text.
//
//  '=' or ';' characters can not be in the TAC callsign.
// 
*/
typedef struct NICKNAME_INFO_S
{	BOOL Enabled;
	char Station[16];	/* Actually limited to 9 (XXXXXX-NN) */
	BOOL OverrideLabel;
	char Label[16];
	BOOL OverrideComment;
	char Comment[128];	/* Blank to keep station's beacon comment */
	BOOL OverrideSymbol;
	SYMBOL_INFO_S Symbol;
	BOOL OverrideColor;
	char Color[COLOR_SIZE];
	COLORREF RGB;		/* Not saved, populated as needed */
	char DefinedBy[16];
	SYSTEMTIME LastSeen;
	BOOL MultiTrackNew;
	BOOL MultiTrackActive;
	BOOL MultiTrackAlways;
	struct CONFIG_INFO_S *pConfig;		/* Only for use by the editing dialog procedure */
} NICKNAME_INFO_S;

#ifndef STATION_SIZE
#define STATION_SIZE 10
#endif

typedef struct OSM_TILE_COORD_S	/* All at zoom 24! */
{	unsigned long x;	/* lower 8 bits is offset in tile, upper 24 is tile */
	unsigned long y;	/* lower 8 bits is offset in tile, upper 24 is tile */
} OSM_TILE_COORD_S;

typedef struct LAT_LON_S
{	double lat, lon;
} LAT_LON_S;

typedef struct COORDINATE_S
{	double lat, lon;	/* raw doubles */
	OSM_TILE_COORD_S TileCoord;	/* Integers */
	POINT pt[MAX_TRACKERS];
	unsigned long References;
} COORDINATE_S;

typedef enum TRACK_VALIDITY_V	/* See also GetInvalidColor */
{	TRACK_OK = 0,	/* This will be FALSE in Invalid */
	TRACK_DUP,
	TRACK_QUICK,
	TRACK_FAST,
	TRACK_RESTART
} TRACK_VALIDITY_V;

typedef struct TRACK_INFO_S
{	TRACK_VALIDITY_V Invalid;
#ifndef UNDER_CE
	char IGate[STATION_SIZE];		/* Most recent gate */
#endif
	SYSTEMTIME st;		/* UTC of data point */
	__int64 msec;		/* For speed and aging calculations */
	long alt;
//	long lat, lon, alt;
	COORDINATE_S *pCoord;
#ifdef TRACK_SPEED
	short speed;		/* for putting speed along historical track */
#endif
#ifdef TRACK_BEARING
	short bearing;		/* For putting altitude away from line */
#endif
} TRACK_INFO_S;

typedef struct OVERLAY_OBJECT_INFO_S
{	char *ID;		/* _strdup()d */
	char Type;		/* Point, Route, or Track */
	unsigned char sComment;			/* String buffer sizes (# TCHARs) */
	unsigned char sStatusReport;	/* String buffer sizes (# TCHARs) */
	TCHAR *pComment;		/* Most recent (non-blank) comment */
	TCHAR *pStatusReport;		/* Most recent (non-blank) status */
	COLORREF TrackColor;	/* RGB of user-specified color */
	int isymbol;			/* Symbol and page & overlay */
	long alt;
//	MULTILINE_INFO_S *MultiLine;
	COORDINATE_S *pCoord;	/* Points to unique coord matching lat/lon */
	TRACK_INFO_S *Tracks;
	int TrackCount;
	int TrackSize;

	struct
	{	BOOL rcsymvalid:1;		/* TRUE if rc has been validated */
		BOOL rclblvalid:1;		/* TRUE if rcLabel has been validated */
		BOOL ptVisible:1;		/* TRUE if ptObj is visible */
		POINT ptObj;			/* Actual point on screen */
		RECT rcSym;				/* Target icon rectangle */
		RECT rcLbl;				/* Target label rectangle */ 
	} TInfo[MAX_TRACKERS];

} OVERLAY_OBJECT_INFO_S;

typedef struct OVERLAY_INFO_S
{	int PointCount, RouteCount, TrackCount;
	int ObjectCount;
	int ObjectSize;
	OVERLAY_OBJECT_INFO_S *Objects;
} OVERLAY_INFO_S;

typedef struct OVERLAY_CONFIG_POINT_S
{	BOOL Enabled;
	BOOL Label;
	struct
	{	BOOL Show;
		BOOL Force;	/* Only for Waypoints */
		SYMBOL_INFO_S Symbol;
	} Symbol;
	struct
	{	unsigned long Width;
		unsigned long Opacity;
		char Color[COLOR_SIZE];
		COLORREF RGB;		/* Not saved, populated as needed */
	} Line;	/* Only for Route & Track */
} OVERLAY_CONFIG_POINT_S;

typedef struct OVERLAY_CONFIG_INFO_S
{	char FileName[MAX_PATH];	/* Path to overlay file */
	BOOL Enabled;
	char Type;	/* G-PX or P-os */
	struct
	{	BOOL Enabled;
		BOOL ID;
		BOOL Comment;
		BOOL Status;
		BOOL Altitude;
		BOOL Timestamp;
		BOOL SuppressDate;
	} Label;
	OVERLAY_CONFIG_POINT_S Route, Track, Waypoint;
	BOOL RGBFixed;	/* FALSE if RGB not yet converted */
	OVERLAY_INFO_S Runtime;	/* Holds the overlays objects */
} OVERLAY_CONFIG_INFO_S;

typedef struct OBJECT_CONFIG_INFO_S
{	char Name[16];	/* Actually limited to 9 */
	char Group[16];	/* For organizational purposes */
	char Comment[128];
	SYMBOL_INFO_S Symbol;
	double Latitude;
	double Longitude;
	BOOL Compressed;	/* TRUE to use Compressed coordinates */
	long Precision;	/* Number of digits (-4 .. 2) */
	BOOL JT65;	/* TRUE to be a JT65-log-file driven object */
	BOOL Weather;	/* TRUE to be a weather-file driven object */
	BOOL Item;		/* FALSE to be Object, TRUE is Item (no timestamp) */
	BOOL Permanent;	/* TRUE if permanent (111111z) Object */
	BOOL HHMMSS;	/* TRUE to transmit local hhmmss vs FALSE for ddhhmm zulu */
	BOOL Enabled;
	unsigned long Interval;	/* In minutes (0 = disabled) */
	BOOL RFEnabled;	/* TRUE to transmit via RF */
	BOOL ISEnabled;	/* TRUE to send via -IS */
	BOOL Kill;		/* TRUE to flag object as Killed */
	unsigned long KillXmitCount;	/* Count of Kills transmitted */
	char RFPath[64];	/* Path to use via RF */
	int WeatherRetries;	/* Not persistent, count of weather format failures */
	char WeatherPath[MAX_PATH];	/* Path to JT65/weather file */
	SYSTEMTIME LastWeather;	/* Also JT65 */
	SYSTEMTIME LastTransmit;
} OBJECT_CONFIG_INFO_S;

typedef struct BULLETIN_CONFIG_INFO_S
{	char Name[16];	/* Actually limited to 9 */
	char Comment[128];
	BOOL Enabled;
	unsigned long Interval;	/* In minutes (0 = disabled) */
	BOOL RFEnabled;	/* TRUE to transmit via RF */
	BOOL ISEnabled;	/* TRUE to send via -IS */
	char RFPath[64];	/* Path to use via RF */
	SYSTEMTIME LastTransmit;
	BOOL New;	/* Internal only, not saved to config */
} BULLETIN_CONFIG_INFO_S;

typedef struct CQSRVR_GROUP_INFO_S
{	char Name[16];
	char Comment[128];
	BOOL ISOnly;	/* TRUE to send only via -IS */
	BOOL KeepAlive;
	BOOL ViaCQSRVR;
	BOOL QuietMonitor;
	BOOL IfPresent;
	unsigned long Interval;	/* In hours (0 = disabled) */
	SYSTEMTIME LastTransmit;
	BOOL OneShot;	/* TRUE if I need to send regardless of other conditions */
/*	The following fields are runtime only, NOT stored in the config file */
	BOOL UnJoin;	/* TRUE if group should be Unjoined (recently disabled) */
	BOOL PendingDelete;	/* TRUE if group should be Unjoined and deleted */
} CQSRVR_GROUP_INFO_S;

typedef struct ANSRVR_GROUP_DEFINITION_S
{	char Name[16];
	char Owner[10];
	char Comment[128];
	unsigned long IdleTimeoutHours;	/* In hours (0 = default) */
	SYSTEMTIME LastActivity;
	TIMED_STRING_LIST_S Members;
} ANSRVR_GROUP_DEFINITION_S;

typedef struct MULTILINE_STYLE_S
{	char sLineType[2];	/* Only uses 1st character, workaround for struct key */
	char Desc[128];
	unsigned long Color;
	long Style;
	BOOL ActionEnabled;
} MULTILINE_STYLE_S;

typedef struct MICE_ACTION_S
{	char Name[16];
	char Tag[16];
	BOOL Enabled;
	BOOL InternalMessage;
	BOOL MultiTrackNew;
	BOOL MultiTrackActive;
	BOOL FlashOnCenter;
	BOOL Highlight;
	char Color[COLOR_SIZE];	/* Color to highlight station */
	TIMED_STRING_LIST_S Ignores;	/* List of stations to ignore for notifications */
	BOOL ColorFixed;	/* RGB Color->RGB needed if FALSE */
	COLORREF RGB;		/* Not saved, populated as needed */
} MICE_ACTION_S;

typedef struct NWS_PRODUCT_S
{	char PID[16];	/* Actually only 3 */
	char Desc[128];	/* Description */
	SYMBOL_INFO_S Symbol;	/* Symbol to display */
	char LineType;	/* Temporary (I think) */
	BOOL ActionEnabled;	/* FALSE to not notify or do MultiTrack */
} NWS_PRODUCT_S;

typedef struct NWS_ENTRY_SERVER_S
{	char EntryCall[16];	/* Actually a station ID (e/ filter) */
	char Desc[128];	/* Description */
	BOOL Disabled;	/* TRUE if disabled for NWS actions */
	struct
	{	char Server[64];	/* Blank if no finger server supported */
		unsigned long Port;	/* Zero if no finger server supported */
	} Finger;
} NWS_ENTRY_SERVER_S;

/* Default values in OSMUtil.cpp must match struct definition */
typedef struct TILE_SERVER_INFO_S
{	char Name[16];	/* Name of tile set - User specified */
	char Server[64];	/* DNS or IP of tile server */
	unsigned long Port;	/* Port (typically 80) */
	char URLPrefix[128];	/* Prefix in front of zoom/x/y.png, may include %z/%x/%y.png */
	BOOL SupportsStatus;	/* Whether this server supports /status checks */
	char Path[MAX_PATH];	/* Path (relative or absolute) to Tile cache */
	BOOL PurgeEnabled;	/* Whether this tile set should be purged */
	unsigned long RetainDays;	/* Retention days for purger (0 disables) */
	long RetainZoom;	/* How many levels to skip purging */
	long MinServerZoom;	/* Min zoom to ask of server (0) */
	long MaxServerZoom;	/* Max zoom to ask of server (18) */
	long RevisionHours;	/* Hours between revision checks */
	BOOL SingleDigitDirectories;

	struct
	{	unsigned __int64 SpaceUsed;
		unsigned __int64 DSpaceUsed;
		unsigned long CountOnDisk;
	} Private;

} TILE_SERVER_INFO_S;

/*	NOTE: ANY changes to this structure may break DefaultPathConfig */
typedef struct PATH_CONFIG_INFO_S
{	BOOL Network;	/* TRUE to show accumulated paths on screen */
	BOOL Station;	/* TRUE to show station's last packet on screen */
	BOOL MyStation;	/* TRUE to show MyStation's last packet on screen */
	BOOL LclRF;		/* TRUE to only show RF-received paths */
	unsigned long MaxAge;	/* -1 through a long time (seconds) */
	unsigned long ReasonabilityLimit;	/* Length (in miles or km) of max "reasonable" hop */
	unsigned long Opacity;	/* For Network, not station */
	BOOL ShowAllLinks;
	BOOL RGBFixed;	/* FALSE if RGB not yet converted */
	struct
	{	BOOL Enabled;
		unsigned long Width;
		char Color[COLOR_SIZE];
		COLORREF RGB;	/* NOT Saved */
	} Packet, Direct, First, Middle, Final;
	struct
	{	BOOL Selected;
		unsigned long Time;
	} Flash, Short, Medium, Long, Last, All;
} PATH_CONFIG_INFO_S;

typedef struct CONFIG_INFO_S
{	char Version[80];
	char LastWhy[80];
	SYSTEMTIME stLastSaved;
	char CallSign[16];
	char Password[16];
	BOOL SuppressUnverified;	/* TRUE if user has acknowledged unverified password */
	char Filter[256];
	char IStoRFFilter[256];

	TIMED_STRING_LIST_S CompanionCalls;
	unsigned long CompanionInterval;	/* In minutes, timed only */

	char Comment[128];
	unsigned long CommentInterval;	/* In minutes (0 = every position) */
	STRING_LIST_S CommentChoices;	/* List of selectable comments */

	SYMBOL_INFO_S Symbol;
	STRING_LIST_S SymbolChoices;	/* List of selectable symbols */

	char AltNet[6+1];				/* "Alternate" (ToCall) Network */
	TIMED_STRING_LIST_S AltNetChoices;	/* List of selectable AltNets */

	struct
	{	char Text[128];
		BOOL Enabled;				/* TRUE to send status */
#ifdef UNDER_CE
		BOOL Cellular;
#endif
		BOOL Timestamp;				/* TRUE to include timestamp */
		BOOL GridSquare;			/* TRUE to include GridSquare (overrides time) */
		BOOL DX;
		unsigned long Interval;		/* In minutes (0 = disabled) */
		SYSTEMTIME LastSent;		/* Time status last sent */
		STRING_LIST_S Choices;	/* List of selectable Texts */
	} Status;

	double MoveSpeed;	/* Minimum speed for move vs stop & trust heading, in Knots! */
	unsigned long Range;
	unsigned long QuietTime;
	unsigned long ForceMinTime;
	struct
	{	unsigned long MinAge;
		unsigned long MaxAge;
		unsigned long BuddyMaxAge;
		unsigned long AvgIntervals;
		unsigned long AvgCount;
		BOOL AvgFixed;
	} Stations;
	struct
	{	unsigned long TelemetryDefDays;	/* Days */
		unsigned long BulletinHours;	/* Hours */
		unsigned long TrackHours;		/* Hours */
		unsigned long BuddyTrackHours;	/* Hours */
		unsigned long DefaultObjectInterval;	/* Minutes */
		unsigned long MaxObjectKillXmits;	/* Transmissions */
	} Aging;
	struct
	{	unsigned long MinDist;
		unsigned long MinTrigger;
		unsigned long MinInterval;
		unsigned long Window;
		struct
		{	SYSTEMTIME st;	/* When Distance was set */
			SYSTEMTIME stFirst;	/* Original reception */
			double lat, lon;
			unsigned long Count;
			double Distance, Bearing;
			char Station[16];
		} MaxEver;
		TIMED_STRING_LIST_S Excluded;	/* List of excluded stations */
	} DX;
	struct
	{	BOOL APRSIS;
		BOOL RFPorts;
		BOOL RFPktLog;
		BOOL RFReceiveOnly;
		BOOL GPS;
		BOOL Internet;
		BOOL OSMFetch;
		BOOL Sound;
		BOOL NotifyOnNewBulletin;
		BOOL CSVFile;
		BOOL AutoSaveGPX;
		BOOL Beacons;
		BOOL Telemetry;
		BOOL DebugFile;
		BOOL DebugGeneral;
		BOOL DebugStartup;
		BOOL MicENotification;
		BOOL MicEEmergency;
	} Enables;
	GENIUS_INFO_S MyGenius;

	struct
	{	BOOL AllMessages;
		BOOL RFMessages;
		BOOL MyMessages;
		BOOL HideNWS;
		BOOL HideQueries;
		BOOL NotifyOnQuery;
		BOOL NotifyOnNewMessage;
		BOOL MultiTrackItemInMessage;
		struct
		{	unsigned long Delay;	/* Delay after interaction (minutes) */
			unsigned long Interval;	/* Delay between auto-answers (minutes)  */
			char Reply[67-3+1];		/* Current auto-answer if non-blank */
			STRING_LIST_S ReplyChoices;	/* List of selectable Auto-Answers */
			TIMED_STRING_LIST_S Stations;	/* List of stations which have been auto-answered */
		} AutoAnswer;
	} Messaging;

	struct
	{	BOOL Enabled;	/* One stop enable */
		BOOL RetryMessages;	/* TRUE to retry QRU-initiated messages */
		unsigned long Interval;	/* 0 to disable */
		unsigned long MaxObjs;	/* 0 for unlimited */
		unsigned long Range;	/* 0 to disable (Native) */
		SYSTEMTIME LastTransmit;	/* For tracking transmission */
	} QRU;
	unsigned long MaxGroupObjs;	/* 0 for unlimited */

	struct
	{	BOOL AfterTransmit;
		BOOL Timestamp;
		BOOL HHMMSS;
#ifdef UNDER_CE
		BOOL Cellular;
#endif
		BOOL Compressed;	/* TRUE if using base-91 Compressed */
		BOOL CourseSpeed;	/* Only if known */
		BOOL Altitude;	/* Only if positive */
		BOOL Why;		/* (Transmit Pressure String) */
		BOOL Comment;	/* I know, they can just blank it! */
		long Precision;	/* Number of digits (-4..2) */
		char MicETag[16];
	} Beacon;

#define DEFAULT_SCALE 8.0						/* Miles */
#define MIN_SCALE (1/64.0)
#define MAX_SCALE 256.0
	double Scale;
	unsigned long Orientation;	/* 0=Wide, 1=Tall, 2=Auto */

	double Latitude;
	double Longitude;
	double Altitude;

	char BeaconPath[64];
	char DXPath[64];
	char EMailServer[16];
	char WhoIsServer[16];
	BOOL WhoIsFull;
	STRING_LIST_S MessageGroups;

	long OSMZoom;
	BOOL OSMPurgeDisabled;	/* Global purge disable */
	unsigned long OSMPercent;
	unsigned long OSMMinMBFree;
#ifdef OBSOLETE
	struct
	{	long Zoom;
		unsigned long Percent;
//		char Server[64];
//		unsigned long Port;
//		char URLPrefix[64];
//		char Path[MAX_PATH];
//		BOOL PurgeEnabled;
		unsigned long MinMBFree;
//		unsigned long RetainDays;
//		long RetainZoom;
//		long RevisionHours;
	} OSM;
#endif
	TILE_SERVER_INFO_S OSM;

	struct
	{	unsigned long Count;
		TILE_SERVER_INFO_S *Server;	/* Actually an array of Tile Server */
	} TileServers;

	struct
	{	BOOL Messages;
		BOOL MessagesNotAll;
		BOOL Notify;
		BOOL MultiTrack;
		BOOL MultiTrackNew;
		BOOL MultiTrackPreferML;
		BOOL MultiTrackLinesOnly;
		BOOL MultiTrackMe;
		BOOL MultiTrackRange;
		BOOL MultiTrackMoving;
		BOOL MultiTrackCloseOnExpire;
		STRING_LIST_S Offices;
#ifdef SUPPORT_SHAPEFILES
		unsigned long Opacity;
		unsigned long Quality;
		BOOL ShapesEnabled;
		TIMED_STRING_LIST_S ShapeFiles;
#endif
	} NWS;

	struct
	{	BOOL ViewNone;
		long Zoom;
		double Scale;
		long width, height;
		unsigned long Percent;
		unsigned long Orientation;	/* 0=Wide, 1=Tall, 2=Auto */
	} MultiTrack;

/*	Everything below this line should NOT need min/default/max initializers or validators */

	struct
	{	long x, y;
		long width, height;
	} OrgWindowPlacement;

	struct
	{	BOOL ConfirmOnClose;	/* TRUE to confirm on close, FALSE won't bother asking */
		BOOL AboutRestart;		/* TRUE to always offer Restart on About */
		struct
		{	BOOL Altitude;	/* True is m, FALSE, is ft */
			BOOL Distance;	/* True is km, FALSE is mi */
			BOOL Temperature;	/* True is C, FALSE is F */
			BOOL Pressure;	/* True is mb, FALSE is inHg */
			BOOL Rainfall;	/* True is mm, FALSE is in */
			BOOL Windspeed;	/* True is knots (kn), FALSE is Distance */
		} Metric;
		unsigned long WheelDelta;	/* Click = WHEEL_DELTA*WheelDelta/4 */
		BOOL ZoomReverse;			/* TRUE to reverse zoom +/- */
		long ZoomMin;		/* Min allowed zoom */
		long ZoomMax;		/* Max allowed zoom */
		unsigned long VisibleLabelsMax;	/* Max number of labels visible */
		BOOL Altitude;		/* TRUE to show altitude with label */
		BOOL Ambiguity;		/* TRUE to show ambiguity circles/rectangles */
		BOOL Callsign;		/* TRUE to show Callsign */
		BOOL CallsignNotMe;	/* TRUE to suppress Me's callsign */
		BOOL CallsignNotMine;	/* TRUE to suppress my base callsigns */
		BOOL Nicknames;		/* FALSE to disable Nickname use */
#ifdef UNDER_CE
		BOOL Cellular;
#endif

		struct
		{	BOOL Enabled;
			unsigned long MinAltitude;
			unsigned long MaxAltitude;
		} Footprint;

		BOOL GeoCacheID;	/* Label components on Geocaches */
		BOOL GeoCacheType;
		BOOL GeoCacheCont;
		BOOL GeoCacheDT;
		unsigned long GeoCacheLabelsMax;	/* Max number of labels visible */

		BOOL LabelNWS;		/* TRUE to label NWS */
		BOOL LabelWeather;	/* TRUE to label weather */
		BOOL LabelOverlap;	/* TRUE to allow labels to overlap */
		struct
		{	BOOL Beaconed;	/* TRUE to show Beaconed speed */
			BOOL Calculated;	/* TRUE to show Calculated speed */
			BOOL Averaged;	/* TRUE to show Average calculated speed */
			BOOL All;		/* TRUE to show All available speeds */
		} Speed;
		BOOL ScrollInternals;	/* FALSE to hide internally-generated packets */
		struct
		{	char Primary[128];
			char Alternate[128];
		} Mobile, Weather, Flight, Marine, Custom;

		struct
		{	BOOL Enabled;
			BOOL Half;
			unsigned long Opacity;
		} Range;

		struct
		{	BOOL Enabled;
			unsigned long Opacity;
		} DF;

	} View;

	struct
	{	char DateSeconds[16];
		long DateTime;	/* 0=none, 1=Local (default), 2=GMT, Negative is GPS-Only */
		BOOL DateTimePerformance;	/* TRUE to display performance instead */
		unsigned long SpeedSize;		/* Speed font multiplier */
		long SymbolSizeAdjust;	/* +/- Symbol Size Adjust */
		BOOL Dim;				/* True to use black background on maps */
		PATH_CONFIG_INFO_S Paths;

		struct
		{	struct
			{	unsigned long Count;
				unsigned long Width;
				char Color[COLOR_SIZE];	/* NULL to rotate */
				COLORREF RGB;	/* NOT Saved */
			}
#ifdef MONITOR_PHONE
				Cellular,
#endif
				Follow, Other;
		} Track;

		struct
		{	BOOL Enabled;
			BOOL RotateStorm;
			unsigned long Width;
			char Color[COLOR_SIZE];	/* NULL to rotate */
			COLORREF RGB;	/* NOT Saved */
		} WindBarbs;
#define STORM_SPIN_MSEC 400	/* Max time per step */
#define STORM_SPIN_ROT_SECONDS 10	/* One Complete Rotation */

		BOOL FilterCircle;		/* TRUE to add r/ filter to main window */
		struct
		{	BOOL Altitude;		/* TRUE to show Altitude on screen */
			BOOL Battery;		/* TRUE to show Battery power bar */
			BOOL Satellites;	/* TRUE to show Satellites on screen */
			BOOL LatLon;		/* TRUE to show LatLon on screen */
			BOOL GridSquare;	/* TRUE to show GridSquare on screen */

			BOOL Reckoning;		/* TRUE to show dead reckoning forecast */
			BOOL Tracks;		/* TRUE to show accumulated tracks on screen */
			BOOL Circle;		/* TRUE to show scale circle */
			BOOL RedDot;		/* TRUE to show Red Dot forecast error */
			long CrossHairs;	/* 0=Never, -1=Always, 1=Timed */
			long CrossHairTime;	/* N=msec after pan/zoom */
		} Show;
	} Screen;

	struct
	{	BOOL Enabled;
		unsigned long Timer;
		unsigned long FontSize;
		char Filter[256];
	} FreqMon;

	struct
	{	BOOL ShowIGateOrDigi;
		BOOL FreezeOnClick;
		BOOL ShowAll;
		BOOL HideNoParse;
		BOOL NoInternals;
		BOOL NotME;
		BOOL NotMine;
		BOOL RFOnly;
		char Filter[256];
	} Scroller;
	STRING_LIST_S PathAliases;
#ifndef UNDER_CE
	BOOL AccumulateAliases;
	STRING_LIST_S NewAliases;
	STRING_LIST_S ZeroAliases;
#endif

	struct
	{	double Latitude;
		double Longitude;
	} Center;

	struct
	{	double Latitude;
		double Longitude;
		unsigned long Zoom;
		double Scale;
		unsigned long Orientation;	/* 0=Wide, 1=Tall, 2=Auto */
	} Preferred;

	struct
	{	char Call[16];
		BOOL Center;
		BOOL Locked;
	} Tracking;

	struct
	{	BOOL AutoCheck;
		BOOL Development;
		unsigned long CheckInterval;	/* In days (hours if development) */
		SYSTEMTIME LastCheck;
		char LastSeen[64];
		unsigned long ReminderInterval;	/* In days (regardless) */
		SYSTEMTIME LastReminder;
	} Update;

	struct
	{	BOOL XmitEnabled;	/* TRUE if transmit is enabled */
		BOOL RFtoISEnabled;	/* TRUE to gate to -IS */
		BOOL IStoRFEnabled;	/* TRUE to gate messages to RF */
		BOOL BeaconingEnabled;	/* TRUE to send beacons here */
		BOOL MessagesEnabled;	/* TRUE to send messages here */
		BOOL BulletinObjectEnabled;	/* TRUE to send bulletins/objects here */
		BOOL TelemetryEnabled;	/* TRUE to send telementry here */
		char Port[64];
	} APRSIS;

	char GPSPort[64];
	STRING_LIST_S DigiXforms;	/* List of callsign-SSID=callsign-SSID */

	BOOL DefaultedURLs;			/* TRUE if default URLs were populated */
	STRING_LIST_S StationURLs;
	STRING_LIST_S WeatherURLs;
	STRING_LIST_S TelemetryURLs;

	struct
	{	unsigned long Count;
		RFID_CONFIG_INFO_S *RFID;	/* Actually an array of RFID structures */
	} RFIDs;

	struct
	{	unsigned long Count;
		PORT_CONFIG_INFO_S *Port;	/* Actually an array of Port structures */
	} RFPorts;

	struct
	{	unsigned long Count;
		OBJECT_CONFIG_INFO_S *Obj;
	} Objects;

	struct
	{	unsigned long Count;
		COMPANION_INFO_S *Companion;
	} Companions;
	BOOL CompanionsEnabled;

	struct
	{	unsigned long Count;
		OVERLAY_CONFIG_INFO_S *Overlay;
	} Overlays;

	struct
	{	unsigned long Count;
		NICKNAME_INFO_S *Nick;
	} Nicknames;
	STRING_LIST_S TacticalSources;	/* List of calls from whom TACTICAL is trusted */
	STRING_LIST_S TacticalNevers;	/* List of calls from whom TACTICAL is ignored */
//	TIMED_STRING_LIST_S Tacticals;	/* Station(From)=Nickname Timed & Enabled */

	struct
	{	unsigned long Count;
		MICE_ACTION_S *MicE;
	} MicEs;

	struct
	{	unsigned long Count;
		MULTILINE_STYLE_S *Style;
	} LineStyles;

	struct
	{	unsigned long Count;
		NWS_PRODUCT_S *Prod;
	} NWSProducts;

	struct
	{	unsigned long Count;
		NWS_ENTRY_SERVER_S *Srv;
	} NWSServers;

	struct
	{	unsigned long Count;
		BULLETIN_CONFIG_INFO_S *Bull;
	} Bulletins;

	struct
	{	unsigned long Count;
		CQSRVR_GROUP_INFO_S *CQGroup;
	} CQGroups;

	struct
	{	unsigned long Count;
		ANSRVR_GROUP_DEFINITION_S *ANDef;
	} ANDefs;

	struct
	{	long Version;			/* Version last defined */
		SYSTEMTIME Defined;		/* Time definitions last sent */
		unsigned long DefHours;	/* In hours */
		unsigned long Interval;	/* In minutes */
		unsigned long MinTime;	/* In seconds */
		unsigned long Index;	/* Count from 1, wraps at 999 */
	} Telemetry;

	TIMED_STRING_LIST_S ColorChoices;	/* Total color choices + Enabled */
	TIMED_STRING_LIST_S TrackColors;	/* Track color choices + Enabled */
	TIMED_STRING_LIST_S Satellites;		/* Satellite choices + TriState */
	TIMED_STRING_LIST_S SatelliteURLs;	/* TLE URLs + ActiveFilter */
	TIMED_STRING_LIST_S SatelliteFiles;	/* TLE Files + ActiveFilter */
	BOOL SatelliteHide;					/* TRUE to hide unchecked satellites */
	char SatellitePos[64];				/* Position of Satellite Details window */
	unsigned long SatelliteCount;		/* 0, 1, or >1 if actively tracking */

/*
	The IgnoreXxxxx string lists are timed and colon-based:timeout-valued
*/
	TIMED_STRING_LIST_S IgnoreEavesdrops;
/*
	These values are just remembered for various dynamic program elements
*/
	STRING_LIST_S MessageCalls;
	STRING_LIST_S EMails;
	TIMED_STRING_LIST_S POSOverlays;
	TIMED_STRING_LIST_S TelemetryDefinitions;
	TIMED_STRING_LIST_S RcvdBulletins;
	TIMED_STRING_LIST_S RcvdWeather;
	STRING_LIST_S AutoTrackers;
	STRING_LIST_S AlwaysTrackers;
	STRING_LIST_S TraceLogs;
	STRING_LIST_S AlwaysTraceLogs;
	STRING_LIST_S TraceLogsPos;
	STRING_LIST_S WindowPositions;
	TIMED_STRING_LIST_S SavedPosits;
	char SavedPositFilter[256];
	BOOL SavedPositPaths;

	char MaxWidthStationID[16];
	unsigned long PacketScrollerSize;

} CONFIG_INFO_S;

#ifdef __cplusplus
extern "C"
{
#endif

extern HINSTANCE g_hInstance;

char *ZeroSSID(char *c);
char *TrimEnd(char *Buffer);
char *SpaceCompress(int l, char *s);
char *SpaceTrim(int Len, char *Buffer);
BOOL IsAX25Safe(unsigned char *Callsign);
BOOL IsInternationalCall(char *Callsign);

BOOL MakeFocusControlVisible(HWND hWnd, WPARAM wParam, LPARAM lParam);

void SortStringList(STRING_LIST_S *pList);

void AddSimpleStringEntry(TIMED_STRING_LIST_S *pList, char *String);
unsigned long LocateSimpleStringEntry(STRING_LIST_S *pList, const char *string);
void RemoveSimpleStringEntry(TIMED_STRING_LIST_S *pList, unsigned long i);
void EmptySimpleStringList(TIMED_STRING_LIST_S *pList);

int CompareColonStrings(const char *s1, const char *s2);
unsigned long LocateColonStringEntry(TIMED_STRING_LIST_S *pList, const char *string);
unsigned long AddOrUpdateColonStringEntry(TIMED_STRING_LIST_S *pList, const char *string, SYSTEMTIME *pst);
unsigned long AddColonStringEntry(STRING_LIST_S *pList, char *String, SYSTEMTIME *pst);
void RemoveColonStringEntry(TIMED_STRING_LIST_S *pList, unsigned long i);
void EmptyColonStringList(TIMED_STRING_LIST_S *pList);
#ifdef __cplusplus
}
#endif

void RemoveTimedStringEntry(TIMED_STRING_LIST_S *pList, unsigned long i);
void EmptyTimedStringList(TIMED_STRING_LIST_S *pList);
size_t PurgeOldTimedStrings(TIMED_STRING_LIST_S *pList, unsigned long Seconds);
#ifdef __cplusplus
unsigned long LocateTimedStringEntry(TIMED_STRING_LIST_S *pList, const char *string, size_t len=-1);
unsigned long UpdateTimedStringEntryAt(TIMED_STRING_LIST_S *pList, unsigned long At, const char *string=NULL, SYSTEMTIME *pst=NULL, long value=0);
unsigned long AddTimedStringEntryAt(TIMED_STRING_LIST_S *pList, unsigned long At, const char *string, SYSTEMTIME *pst=NULL, long value=0);
unsigned long AddTimedStringEntry(TIMED_STRING_LIST_S *pList, const char *string, SYSTEMTIME *pst=NULL, long value=0);
unsigned long AddOrUpdateTimedStringEntry(TIMED_STRING_LIST_S *pList, const char *string, SYSTEMTIME *pst=NULL, long value=0);
#else
unsigned long AddOrUpdateColonStringEntry(TIMED_STRING_LIST_S *pList, const char *string, SYSTEMTIME *pst);
unsigned long AddColonStringEntry(STRING_LIST_S *pList, char *String, SYSTEMTIME *pst);

unsigned long LocateTimedStringEntry(TIMED_STRING_LIST_S *pList, const char *string, size_t len);
unsigned long AddOrUpdateTimedStringEntry(TIMED_STRING_LIST_S *pList, const char *string, SYSTEMTIME *pst, long value);
#endif

BOOL RealSaveConfiguration(HWND hwnd, CONFIG_INFO_S *pConfig, char *Why);
BOOL CALLBACK SymbolDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL PromptGenius(HWND hwnd, CONFIG_INFO_S *pConfig);
char *ConfigCommOrIpPort(HWND hdlg, char *Current);
#ifdef OBSOLETE
BOOL PromptAGW(HWND hwnd, CONFIG_INFO_S *pConfig);
BOOL PromptKISS(HWND hwnd, CONFIG_INFO_S *pConfig);
BOOL PromptTEXT(HWND hwnd, CONFIG_INFO_S *pConfig);
#endif

int PromptNewRFPort(HWND hwnd, CONFIG_INFO_S *pConfig);
BOOL PromptConfigRFPort(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id);

BOOL PromptNewBulletin(HWND hwnd, CONFIG_INFO_S *pConfig);
BOOL PromptConfigBulletin(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id);

BOOL PromptNewCQGroup(HWND hwnd, CONFIG_INFO_S *pConfig);
BOOL PromptConfigCQGroup(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id);

char *ParseWeatherFile(char *Trace, char *File, SYSTEMTIME *pst);
#ifdef __cplusplus
COMPANION_INFO_S *PromptNewCompanion(HWND hwnd, CONFIG_INFO_S *pConfig, char Table='/', char Symbol='.', char *Comment=NULL, char *Name=NULL);
COMPANION_INFO_S *PromptConfigCompanion(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id);
COMPANION_INFO_S *FindConfigCompanion(CONFIG_INFO_S *pConfig, char *Name);
void SortConfigCompanions(CONFIG_INFO_S *pConfig);
OBJECT_CONFIG_INFO_S *PromptNewObject(HWND hwnd, CONFIG_INFO_S *pConfig, double Latitude, double Longitude, char Table='/', char Symbol='.', char *Comment=NULL, char *Group=NULL, char *Name=NULL);
BOOL PromptNewWeatherObject(HWND hwnd, CONFIG_INFO_S *pConfig, double Latitude, double Longitude, char *WeatherFile);
BOOL PromptNewJT65Object(HWND hwnd, CONFIG_INFO_S *pConfig, double Latitude, double Longitude, char *JT65File);
OBJECT_CONFIG_INFO_S *PromptConfigObject(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id);
OBJECT_CONFIG_INFO_S *FindConfigObject(CONFIG_INFO_S *pConfig, char *Name);
void SortConfigObjects(CONFIG_INFO_S *pConfig);
BOOL IsObjectInRange(double MaxRange, BOOL Metric, OBJECT_CONFIG_INFO_S *Obj, double Lat, double Lon, double *pDistance=NULL, double *pBearing=NULL);
char *GetQRUGroups(CONFIG_INFO_S *pConfig, BOOL NoQuestion=FALSE, double Lat=0, double Lon=0, double MaxRange=0, BOOL Metric=FALSE);
#endif

BOOL VerifyOSMPath(HWND hwnd, TILE_SERVER_INFO_S *tInfo, BOOL DefaultOSM, BOOL *pConfigChanged);
BOOL PromptNewTileServer(HWND hwnd, CONFIG_INFO_S *pConfig);
BOOL PromptConfigTileServer(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id);

//BOOL PromptAPRSIS(HWND hwnd, CONFIG_INFO_S *pConfig);
BOOL PromptConfigAPRSIS(HWND hwnd, CONFIG_INFO_S *pConfig);
BOOL PromptNMEA(HWND hwnd, CONFIG_INFO_S *pConfig);

typedef enum COLOR_SET_T
{	COLORS_AVAILABLE,
	COLORS_NO_TRACKS,
	COLORS_TRACKS,
	COLORS_ALL
} COLOR_SET_T;
char *ColorPrompt(HWND hwnd, char *Title, char *Color, COLOR_SET_T InitialSet);

BOOL PromptConfiguration(HWND hwnd, CONFIG_INFO_S *pConfig);
HWND PromptPathConfig(HWND hwnd, int msgRefresh, PATH_CONFIG_INFO_S *pPaths, BOOL Default);
BOOL PromptStatusReport(HWND hwnd, CONFIG_INFO_S *pConfig);
BOOL LoadOrDefaultConfiguration(HWND hwnd, CONFIG_INFO_S *pConfig, BOOL AllowCreate, BOOL DefaultOSMIfNew);
char *GetXmlCallsign(HWND hwnd);

BOOL CheckMessageCall(HWND hwnd, CONFIG_INFO_S *pConfig, char *Call);

void RememberMessageCall(HWND hwnd, CONFIG_INFO_S *pConfig, char *Call);
void RememberEMail(HWND hwnd, CONFIG_INFO_S *pConfig, char *EMail);
void RememberCommentChoice(HWND hwnd, CONFIG_INFO_S *pConfig, char *Choice);
BOOL RememberSymbolChoice(HWND hwnd, CONFIG_INFO_S *pConfig, SYMBOL_INFO_S *Symbol);
BOOL RememberAltNetChoice(HWND hwnd, CONFIG_INFO_S *pConfig, char *AltNet, BOOL Used);
void RememberNWSOffice(HWND hwnd, CONFIG_INFO_S *pConfig, char *Choice, BOOL Active);
void RememberNWSShapeFile(HWND hwnd, CONFIG_INFO_S *pConfig, char *File, long Enabled);
void RememberPOSOverlay(HWND hwnd, CONFIG_INFO_S *pConfig, char *File, long Enabled);
void RememberStatusChoice(HWND hwnd, CONFIG_INFO_S *pConfig, char *Choice);
void RememberAutoAnswerChoice(HWND hwnd, CONFIG_INFO_S *pConfig, char *Choice);
void RememberAutoAnswerStation(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station);
void PurgeAutoAnswerStations(HWND hwnd, CONFIG_INFO_S *pConfig);

void RememberRFIDReader(HWND hwnd, CONFIG_INFO_S *pConfig, char *Reader);
size_t PurgeTelemetryDefinitions(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long Seconds);
void RememberTelemetryDefinition(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station, char *Definition);
char *RecallTelemetryDefinition(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station, char *What);	/* What should be 5 characters! */
void RefreshTelemetryDefinition(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station);
size_t ForgetRcvdBulletin(HWND hwnd, CONFIG_INFO_S *pConfig, char *From, char ID, char *Group);
void RememberRcvdBulletin(HWND hwnd, CONFIG_INFO_S *pConfig, char *From, char ID, char *Group, char *Text);
void RememberRcvdWeather(HWND hwnd, CONFIG_INFO_S *pConfig, char *From, char *To, char *Comment, char *Packet);
#ifdef OBSOLETE
void RememberScale(HWND hwnd, CONFIG_INFO_S *pConfig, double Scale);
void RememberOrientation(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long Orientation);
void RememberLatLon(HWND hwnd, CONFIG_INFO_S *pConfig, double Latitude, double Longitude);
void RememberOSMZoom(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long zoom);
#endif

HWND PromptOverlayConfig(HWND hwnd, int msgRefresh, OVERLAY_CONFIG_INFO_S *pOver, BOOL Default);
BOOL RemoveOverlay(CONFIG_INFO_S *pConfig, char *FileName);
OVERLAY_CONFIG_INFO_S *GetOrCreateOverlay(CONFIG_INFO_S *pConfig, char *FileName, char Type, BOOL AllowCreate);

char * PromptConfigNickname(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station);
char * PromptNewNickname(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station);
void SortNicknames(CONFIG_INFO_S *pConfig);
BOOL RemoveNickname(CONFIG_INFO_S *pConfig, char *Station);
BOOL IsNicknameLabelInUse(CONFIG_INFO_S *pConfig, char *Label, char *Ignore, BOOL DisableDupes);
NICKNAME_INFO_S *GetOrCreateNickname(CONFIG_INFO_S *pConfig, char *Station, char *Label, BOOL AllowCreate, BOOL DisableDupes);
void SetNicknameSymbol(CONFIG_INFO_S *pConfig, char *Station, SYMBOL_INFO_S *pSymbol);

#ifdef OBSOLETE
void SortTacticals(CONFIG_INFO_S *pConfig);
size_t PurgeTacticals(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long Seconds);
int RemoveTactical(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station);
void RememberTactical(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station, char *Definition, char *From);
char *RecallTactical(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station, long *pEnabled);
int ToggleTactical(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station);
#endif

BOOL IsValidANGroupName(char *Name);
ANSRVR_GROUP_DEFINITION_S *GetOrCreateANDefinition(CONFIG_INFO_S *pConfig, char *Name, BOOL AllowCreate);
BOOL RemoveANDefinition(CONFIG_INFO_S *pConfig, char *Name);
int FindANMemberIndex(ANSRVR_GROUP_DEFINITION_S *pAN, char *Member);
BOOL AddANDefinitionMember(CONFIG_INFO_S *pConfig, char *Name, char *Member, ANSRVR_GROUP_DEFINITION_S **ppAN);
BOOL RemoveANDefinitionMember(CONFIG_INFO_S *pConfig, char *Name, char *Member);

BOOL QueueDebugMessage(_int64 Now, char *Buffer);	/* In APRSISCE.cpp */
BOOL QueueInternalMessage(char *Buffer, BOOL FreeIt);	/* In APRSISCE.cpp */

unsigned char GetMicEActionIndex(CONFIG_INFO_S *pConfig, char *Name, char *Why);

MULTILINE_STYLE_S *GetMultiLineStyle(CONFIG_INFO_S *pConfig, char LineType, char *Why);
void SortNWSProducts(CONFIG_INFO_S *pConfig);
NWS_PRODUCT_S *GetNWSProduct(CONFIG_INFO_S *pConfig, char *PID, char *From, char *Via);
NWS_ENTRY_SERVER_S *GetNWSServer(CONFIG_INFO_S *pConfig, char *EntryCall, char *Why, char *srcCall, BOOL Remember);

BOOL DefineColorChoices(CONFIG_INFO_S *pConfig, BOOL All);
BOOL DefineTrackColors(CONFIG_INFO_S *pConfig);
COLORREF GetColorRGB(CONFIG_INFO_S *pConfig, char *Name, char *Why);
char *GetRGBColorName(CONFIG_INFO_S *pConfig, COLORREF Color);

size_t EnumeratePorts();

char *StringPromptA(HWND hwnd, char *Title, char *Prompt, int MaxLen, char *String, BOOL UpCase, BOOL NoChangeSuccess);
unsigned long NumberPrompt(HWND hwnd, char *Title, char *Prompt, char *Units, char *ConfigParam, unsigned long DefaultValue);
long SignedNumberPrompt(HWND hwnd, char *Title, char *Prompt, char *Units, char *ConfigParam, long DefaultValue);

#ifdef __cplusplus

void RememberTacticalNickname(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station, char *Definition, char *From);
int ClearClonedNicknames(HWND hwnd, CONFIG_INFO_S *pConfig, char *DefinedBy=NULL);
int ClearTacticalNicknames(HWND hwnd, CONFIG_INFO_S *pConfig, char *DefinedBy=NULL);

char * FormatIgnoreStringDelta(TIMED_STRING_LIST_S *pList, char *Station, char *Next, size_t Remaining, char **pNext=NULL, size_t *pRemaining=NULL);
size_t PurgeIgnoreStrings(HWND hwnd, TIMED_STRING_LIST_S *pList);
void DefineIgnoreString(HWND hwnd, TIMED_STRING_LIST_S *pList, char *Station, long Duration=0);
BOOL CheckIgnoreString(HWND hwnd, TIMED_STRING_LIST_S *pList, char *Station, BOOL Update=FALSE);
void DeleteIgnoreString(HWND hwnd, TIMED_STRING_LIST_S *pList, char *Station);

extern "C"
{
#endif
extern char Timestamp[];
extern BOOL ActiveConfigLoaded;
extern CONFIG_INFO_S ActiveConfig;
void GetOverlayCounts(OVERLAY_CONFIG_INFO_S *pOver, int *pPoints, int *pRoutes, int *pTracks);

#ifdef __cplusplus
}
#endif

#endif	/* GOT_CONFIG_H */
