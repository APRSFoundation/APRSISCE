#include "sysheads.h"

// HKEY_CURRENT_USER\Software\VB and VBA Program Settings\Peak Systems\UI-View32
// HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\UI-View32_is1

//#define VERBOSE

#include <xp/source/xmlparse.h>		/* The raw XML Parser */

#include "resource.h"

#include "parsedef.h"
#include "parse.h"	/* for displayable symbol names & xFromDec */

#include "LLUtil.h"
#include "tcputil.h"

#include "config.h"
#include "filter.h"

#include "BlueTooth.h"
#include "KISS.h"	/* For IsAX25Safe */
#include "MsgBox.h"
#include "OSMUtil.h"
#include "port.h"	/* For AddPortProtocols */
#include "TraceLog.h"

#define DEFAULT_CALLSIGN "NOCALL"

extern HINSTANCE g_hInstance;
extern HWND gModelessDialog;

#define LINE_WIDTH_MIN 1
#define LINE_WIDTH_MAX 16

#include "GPXFiles.h"

static PATH_CONFIG_INFO_S DefaultPathConfig = {
	FALSE, FALSE, TRUE, FALSE,	/* All (Network, Station, LclRF) turned off, but MyStation turned on! */
	1800, 400, 50,	/* 30 minutes, 400 mi/km, 50% */
	TRUE,	/* Show All Links */
	FALSE,	/* RGB's NOT fixed */
	/* Line width and colors follow */
	{ FALSE, 2, "orange" },	/* Packet */
	{ FALSE, 1, "salmon" },	/* Direct */
	{ FALSE, 1, "red" },	/* First */
	{ FALSE, 3, "lime" },	/* Middle */
	{ FALSE, 3, "lightgreen" },	/* Final */
	/* Time selections and durations follow */
	{ FALSE, 10 },	/* Flash */
	{ FALSE, 2*60 },	/* Short */
	{ FALSE, 10*60 },	/* Medium */
	{ TRUE, 30*60 },	/* Long */
	{ FALSE, 0 },	/* Last */
	{ FALSE, -1 }	/* All */
};

OVERLAY_CONFIG_INFO_S DefaultOverlayConfig = {
	"", TRUE, '?', /* FileName, Enabled, Type */
	{ TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE },	/* Label.* (Only ID) */
	{ TRUE, FALSE, { FALSE, FALSE, { '/', '/' } }, { 3, 50, "Crimson" }, },	/* Route */
	{ TRUE, FALSE, { FALSE, FALSE, { '/', '/' } }, { 2, 50, "Coral" }, },	/* Track */
	{ TRUE, TRUE, { TRUE, FALSE, { '\\', '.' } }, { 1, 0, "" }, },	/* Waypoint */
	FALSE };	/* RGBFixed */

// helper macros
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (UINT_PTR)(sizeof(a)/sizeof((a)[0]))
#endif

// Open the given active driver key
static LONG OpenActiveDriverKey(LPCTSTR pszDriver, HKEY *phKey)
{
	TCHAR	strFullName[] = TEXT("Drivers\\Active\\");
	LONG	lRes;
static TCHAR tBuff[1024];

	StringCbPrintf(tBuff, sizeof(tBuff), TEXT("%s%s"), strFullName, pszDriver);

	lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tBuff, 0, KEY_READ, phKey);

	return lRes;
}

static LONG GetKeyValue(HKEY hKey, LPCTSTR pszValueName, TCHAR **strValue)
{
	LONG	lRes;
	DWORD	dwSize	= 0,
			dwType;

	*strValue = NULL;

	lRes = RegQueryValueEx(hKey, pszValueName, NULL, &dwType, NULL, &dwSize);
	if(lRes == ERROR_SUCCESS)
	{
		TCHAR*	pszValue = new TCHAR[dwSize / sizeof(TCHAR)];

		if(pszValue != NULL)
		{
			lRes = RegQueryValueEx(hKey, pszValueName, NULL, &dwType, (BYTE*)pszValue, &dwSize);
			
			if(lRes == ERROR_SUCCESS)
				*strValue = pszValue;
			else
			{	delete [] pszValue;
				TraceLogThread("Ports", TRUE, "RegQueryValueEx2(%S) was %ld\n", pszValueName, (long) GetLastError());
			}
		}
		else
			lRes = ERROR_NOT_ENOUGH_MEMORY;
	}
	else TraceLogThread("Ports", TRUE, "RegQueryValueEx(%S) was %ld(%ld)\n", pszValueName, (long) lRes, (long) GetLastError());

	return lRes;
}

// Adds any Microsoft Bluetooth Serial Ports
static void AddMsBtPorts()
{
	LONG	lRes;
	HKEY	hKey;

	LPCTSTR	pszMsBtPorts = 	TEXT("Software\\Microsoft\\Bluetooth\\Serial\\Ports");

	lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pszMsBtPorts, 0, KEY_READ, &hKey);
	if(lRes == ERROR_SUCCESS)
	{
		DWORD	dwIndex;

		for(dwIndex = 0; ; ++dwIndex)
		{
			TCHAR	szKeyName[128];
			DWORD	dwNameSize	= 128;

			lRes = RegEnumKeyEx(hKey, dwIndex, szKeyName, &dwNameSize, NULL, NULL, NULL, NULL);
			if(lRes == ERROR_SUCCESS)
			{
			static TCHAR tKey[1024];
				HKEY	hKeyPort;

				StringCbPrintf(tKey, sizeof(tKey), TEXT("%s\\%s"), pszMsBtPorts, szKeyName);
		
				lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tKey, 0, KEY_READ, &hKeyPort);
				if(lRes == ERROR_SUCCESS)
				{
					TCHAR *strPortName;

					if(GetKeyValue(hKeyPort, TEXT("Port"), &strPortName) == ERROR_SUCCESS)
					{
					//	strPortName += TEXT(":");

						TraceLogThread("Ports", TRUE, "%S %S\n", strPortName, TEXT("Bluetooth"));
					}
					RegCloseKey(hKeyPort);
				}
				else TraceLogThread("Ports", TRUE, "RegOpenKeyEx(%S) was %ld\n", tKey, GetLastError());
			}
			else
				break;
		}

		RegCloseKey(hKey);
	}
}

// Enumerates all the serial ports
size_t EnumeratePorts()
{
	HKEY	hKey;
	LONG	lRes;

	TraceLogThread("Ports", TRUE, "Enumerating Ports (is this only CE?)\n");

//	{	SOCKADDR_BTH BogusAddr;
//		NameToBthAddr(TEXT("Earthmate Blue Logger GPS"), &BogusAddr);
//	}

	lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Drivers\\Active"), 0, KEY_READ, &hKey);
	if(lRes == ERROR_SUCCESS)
	{
		DWORD	dwIndex;

		for(dwIndex = 0; ; ++dwIndex)
		{
			TCHAR	szKeyName[128];
			DWORD	dwNameSize	= 128;

			lRes = RegEnumKeyEx(hKey, dwIndex, szKeyName, &dwNameSize, NULL, NULL, NULL, NULL);
			if(lRes == ERROR_SUCCESS)
			{
				HKEY	hKeyDrv;

				lRes = OpenActiveDriverKey(szKeyName, &hKeyDrv);
				if(lRes == ERROR_SUCCESS)
				{
					TCHAR *strDriverName = NULL,
							*strDriverKey = NULL,
							*strFriendlyName = NULL;

					if(GetKeyValue(hKeyDrv, TEXT("Name"), &strDriverName) == ERROR_SUCCESS)
					{
						// Check if this is a COM port
						if(_wcsnicmp(strDriverName,TEXT("COM"),3) == 0)
						{
							if(GetKeyValue(hKeyDrv, TEXT("Key"), &strDriverKey) == ERROR_SUCCESS)
							{
								HKEY	hKeyBase;

								lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strDriverKey, 0, KEY_READ, &hKeyBase);
								if(lRes == ERROR_SUCCESS)
								{
									GetKeyValue(hKeyBase, TEXT("FriendlyName"), &strFriendlyName);
									RegCloseKey(hKeyBase);
								}
							}

							TraceLogThread("Ports", TRUE, "%S %S (Key:%S)\n", strDriverName, strFriendlyName, strDriverKey);
						}// else TraceLogThread("Ports", TRUE, "%S isn't COMn\n", strDriverName);
					}

					RegCloseKey(hKeyDrv);
				}
			}
			else
				break;
		}

		RegCloseKey(hKey);
	}

	AddMsBtPorts();

	return 0;
}



char *TrimEnd(char *Buffer)
{	int Len = strlen(Buffer);
	while (Len
	&& (Buffer[Len-1] == '\r'
		|| Buffer[Len-1] == '\n'))
		Buffer[--Len] = '\0';
	return Buffer;
}

char * SpaceCompress(int l, char *s)
{	if (s)	/* protect from nulls */
	{	int i;
		if (l==-1) l = strlen(s);
		for (i=0; i<l; i++)
		{	if (isspace(s[i] & 0xff))
			{	if (l-i-1 > 0) memmove(&s[i], &s[i+1], l-i-1);
				s[l-1] = 0;	/* null out old character */
			}
		}
	}
	return s;
}

char *SpaceTrim(int Len, char *Buffer)
{	if (Len==-1) Len = strlen(Buffer);
	for (int i=0; i<Len; i++)
	{	if (isspace(Buffer[i] & 0xff))
		{	if (Len-i-1 > 0) memmove(&Buffer[i], &Buffer[i+1], Len-i-1);
			Buffer[Len-1] = 0;	/* null out old character */
		} else break;	/* Only do leading spaces */
	}
	while (Len && Buffer[Len-1] == ' ')
		Buffer[--Len] = '\0';
	return Buffer;
}

char *ZeroSSID(char *c)
{	char *d = strchr(SpaceTrim(-1,c), '-');
	if (d)
	{	char *e;
		int SSID = strtol(d+1, &e, 10);
		if (!*e && !SSID)	/* all numeric and zero? */
		{	memset(d,0,strlen(d));
		}
	}
	return c;
}

BOOL IsAX25Safe(unsigned char *Callsign)
{	unsigned char Dest[16];
	unsigned char *p = KISSCall(Callsign,Dest);
	return p!=NULL;
}

BOOL IsInternationalCall(char *Callsign)
{	BOOL Result = FALSE;
	if (strlen(Callsign) < 4) return FALSE;

	char *t = _strdup(Callsign);
	char *d = strchr(t,'-');
	if (d) *d = '\0';

#define InRange(c,s,e) ((c)>=(s) && (c)<=(e))
#define OneNine(c) InRange(c,'1','9')
#define TwoNine(c) InRange(c,'2','9')
#define Digit(c) InRange(c,'0','9')
#define Alpha(c) InRange(c,'A','Z')
/*	Per: http://www.aprs-is.net/Connecting.aspx
The regex for international callsigns is:
^(?:(?:[1-9][A-Z][A-Z]?)|(?:[A-Z][2-9A-Z]?))[0-9][A-Z]{1,4}$
*/
	if (strlen(t) < 4) Result = FALSE;
	else if (OneNine(t[0]) && Alpha(t[1]) && Digit(t[2]))	// [1-9][A-Z][0-9]
	{	if (!Alpha(t[3])) Result = FALSE;	// 1 alpha
		else if (!t[4]) Result = TRUE;
		else if (!Alpha(t[4])) Result = FALSE;	// 2 alpha
		else if (!t[5]) Result = TRUE;
		else if (!Alpha(t[5])) Result = FALSE;	// 3 alpha
		else if (!t[6]) Result = TRUE;
		else if (!Alpha(t[6])) Result = FALSE;	// 4 alpha
		else if (!t[7]) Result = TRUE;
	} else if (OneNine(t[0]) && Alpha(t[1]) && Alpha(t[2]) && Digit(t[3]))	// [1-9][A-Z][A-Z][0-9]
	{	if (!Alpha(t[4])) Result = FALSE;	// 1 alpha
		else if (!t[5]) Result = TRUE;
		else if (!Alpha(t[5])) Result = FALSE;	// 2 alpha
		else if (!t[6]) Result = TRUE;
		else if (!Alpha(t[6])) Result = FALSE;	// 3 alpha
		else if (!t[7]) Result = TRUE;
		else if (!Alpha(t[7])) Result = FALSE;	// 4 alpha
		else if (!t[8]) Result = TRUE;
	} else if (Alpha(t[0]) && (TwoNine(t[1]) || Alpha(t[1])) && Digit(t[2]))	// [A-Z][2-9A-Z][0-9]
	{	if (!Alpha(t[3])) Result = FALSE;	// 1 alpha
		else if (!t[4]) Result = TRUE;
		else if (!Alpha(t[4])) Result = FALSE;	// 2 alpha
		else if (!t[5]) Result = TRUE;
		else if (!Alpha(t[5])) Result = FALSE;	// 3 alpha
		else if (!t[6]) Result = TRUE;
		else if (!Alpha(t[6])) Result = FALSE;	// 4 alpha
		else if (!t[7]) Result = TRUE;
	} else if (Alpha(t[0]) && Digit(t[1]))	// [A-Z][0-9]
	{	if (!Alpha(t[2])) Result = FALSE;	// 1 alpha
		else if (!t[3]) Result = TRUE;
		else if (!Alpha(t[3])) Result = FALSE;	// 2 alpha
		else if (!t[4]) Result = TRUE;
		else if (!Alpha(t[4])) Result = FALSE;	// 3 alpha
		else if (!t[5]) Result = TRUE;
		else if (!Alpha(t[5])) Result = FALSE;	// 4 alpha
		else if (!t[6]) Result = TRUE;
	}
	free(t);
	return Result;
#undef Alpha
#undef Digit
#undef TwoNine
#undef OneNine
#undef InRange
}

extern LPWSTR g_lpCmdLine;

static char *GetXmlConfigFile(HWND hwnd, char *Suffix="")
{static	char CfgFile[MAX_PATH+32];

#ifdef TRAKVIEW
	StringCbPrintfA(CfgFile,sizeof(CfgFile),"TrackView%s.xml",Suffix);
#else
#ifdef UNDER_CE
#ifndef CE50
	TCHAR Path[MAX_PATH];
	if (SHGetSpecialFolderPath(hwnd, Path, CSIDL_PERSONAL, TRUE))
	{	StringCbPrintfA(CfgFile, sizeof(CfgFile), "%S/APRSISCE%s.xml", Path, Suffix);
	} else
		StringCbPrintfA(CfgFile,sizeof(CfgFile),"APRSISCE%s.xml",Suffix);
#else
#ifdef SYLVANIA
	TCHAR Path[MAX_PATH];
	if (SHGetSpecialFolderPath(hwnd, Path, CSIDL_PERSONAL, TRUE))
	{	StringCbPrintfA(CfgFile, sizeof(CfgFile), "%S/APRSISCE%s.xml", Path, Suffix);
	} else
		StringCbPrintfA(CfgFile,sizeof(CfgFile),"APRSISCE%s.xml",Suffix);
#else
		LPWSTR Cmd = GetCommandLine();
		TraceLogThread("Config", TRUE, "CommandLine: %p(%s):%ld or %p(%S)%ld",
						Cmd, Cmd, (long) (Cmd?*Cmd:0),
						g_lpCmdLine, g_lpCmdLine, (long) (g_lpCmdLine?*g_lpCmdLine:0));
		StringCbPrintfA(CfgFile,sizeof(CfgFile),"/FlashStorage2/APRSISCE%s.xml",Suffix);
#endif
#endif
#else
	StringCbPrintfA(CfgFile,sizeof(CfgFile),"APRSIS32%s.xml",Suffix);
#endif
#endif

//	TraceLog("Config", TRUE, hwnd, "GetXmlConfigFile=%s\n", CfgFile);
	return CfgFile;
}

char *GetConfigFile(HWND hwnd)
{static	char CfgFile[MAX_PATH+32];

#ifdef UNDER_CE
	TCHAR Path[MAX_PATH];
	if (SHGetSpecialFolderPath(hwnd, Path, CSIDL_PERSONAL, TRUE))
	{	StringCbPrintfA(CfgFile, sizeof(CfgFile), "%S/APRSISCE.cfg", Path);
	} else strncpy(CfgFile,"APRSISCE.cfg",sizeof(CfgFile));
#else
	strncpy(CfgFile,"APRSIS32.cfg",sizeof(CfgFile));
#endif
return CfgFile;
}

#define MONITOR_CENTER     0x0001        // center rect to monitor 
#define MONITOR_CLIP     0x0000        // clip rect to monitor 
#define MONITOR_WORKAREA 0x0002        // use monitor work area 
#define MONITOR_AREA     0x0000        // use monitor entire area 
void GetNearestMonitor(LPRECT prc, UINT flags);	/* MONITOR_(WORK)AREA */

BOOL MakeFocusControlVisible(HWND hWnd, WPARAM wParam, LPARAM lParam)
{static int xMin=0, xMax=0, yMin=0, yMax=0;

	if (xMin==0 && xMax==0 && yMin==0 && yMax==0)
	{
#ifdef FULL_SCREEN
		xMin = 0; yMin = 0;
		xMax = GetSystemMetrics(SM_CXSCREEN);
		yMax = GetSystemMetrics(SM_CYSCREEN);
#else
		{	RECT rc;

			GetWindowRect(hWnd, &rc);
			GetNearestMonitor(&rc, MONITOR_WORKAREA);
//			SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
			xMin = rc.left;
			xMax = rc.right;
			yMin = rc.top;
			yMax = rc.bottom;
		}
#endif
	}

		if (HIWORD(wParam) == EN_SETFOCUS
		|| HIWORD(wParam) == BN_SETFOCUS
		|| HIWORD(wParam) == LBN_SETFOCUS
		|| HIWORD(wParam) == CBN_SETFOCUS)
		{	int x, y;
			RECT rcDlg, rcCtl;
			GetWindowRect(hWnd, &rcDlg);
			GetWindowRect((HWND) lParam, &rcCtl);

#ifdef FULL_SCREEN
			if (!SHFullScreen(hWnd, SHFS_HIDETASKBAR))
			{	DWORD e = GetLastError();
				TCHAR Text[80];
				wsprintf(Text,TEXT("Hide Failed With Error %ld"), (long) e);
				MessageBox(NULL,Text, TEXT("SHFullScreen"), MB_OK | MB_ICONERROR);
			}
#endif
			x = rcDlg.left; y = rcDlg.top;
			if (rcCtl.left < xMin)			/* Off to the left? */
			{	x += -rcCtl.left;
				if (rcCtl.right+-rcCtl.left <= xMax)	/* Still on-screen? */
				{	x += xMax-(rcCtl.right+-rcCtl.left);/* Move it to the margin */
					if (x > xMin) x = xMin;	/* Stop at the edge */
				}
			} else if (rcCtl.right >= xMax)	/* Off to the right? */
			{	x -= rcCtl.right - xMax;
			} else if (rcDlg.left < xMin)	/* Dialog off left edge? */
			{	x += xMax-rcCtl.right;		/* Move it on over */
				if (x > xMin) x = xMin;		/* But stop at the edge */
			}
			if (rcCtl.top < yMin)			/* Off the top? */
			{	y += -rcCtl.top;
				if (rcCtl.bottom+-rcCtl.top <= yMax)	/* Still on-screen? */
				{	y += yMax-(rcCtl.bottom+-rcCtl.top);/* Move it on down */
					if (y > yMin) y = yMin;	/* Stop at the top */
				}
			} else if (rcCtl.bottom >= yMax)/* Off the bottom? */
			{	y -= rcCtl.bottom - yMax;
			} else if (rcDlg.top < yMin)	/* Dialog off the top? */
			{	y += yMax-rcCtl.bottom;		/* Move it on down */
				if (y > yMin) y = yMin;		/* But stop at the top */
			}
			if (x!=rcDlg.left || y!=rcDlg.top)
			{	
				SetWindowPos(hWnd, HWND_TOP, x, y, 0, 0,
							SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
			}
		}

#ifdef UNDER_CE
		return HIWORD(wParam)==BN_SETFOCUS || HIWORD(wParam)==BN_KILLFOCUS
			|| HIWORD(wParam)==BN_DBLCLK;
#else	/* Protect against new BN_NOTIFY style */
		return HIWORD(wParam)==BN_SETFOCUS || HIWORD(wParam)==BN_KILLFOCUS
			|| HIWORD(wParam)==BN_DBLCLK || HIWORD(wParam)==BN_DOUBLECLICKED;
#endif
}

BOOL myGetDlgItemText(HWND hDlg, int ID, char *Text, int TextLen)
{
	memset(Text, 0, TextLen);
#ifdef UNICODE
	{static	int BufLen = 0;
	static	TCHAR *Buffer = NULL;
		int GotLen;
		if (TextLen >= BufLen)
		{	if (Buffer) free(Buffer);
			BufLen = TextLen+1;
			Buffer = (TCHAR*) malloc(BufLen*sizeof(*Buffer));
		}
		GotLen = GetDlgItemText(hDlg, ID, Buffer, BufLen);
		WideCharToMultiByte(CP_ACP, 0, Buffer, GotLen,
							Text, TextLen, NULL, NULL);
	}
#else
	GetDlgItemText(hDlg, ID, Text, TextLen);
#endif
	return TRUE;
}

#ifdef FUTURE
CSIDL_APPDATA 	0x001A 	File system directory that serves as a common repository for application-specific data.
CSIDL_DESKTOP 	0x0000 	Not supported on Smartphone.
CSIDL_DESKTOPDIRECTORY 	0x0010 	File system directory used to physically store file objects on the desktop (not to be confused with the desktop folder itself).
CSIDL_FAVORITES 	0x0006 	The file system directory that serves as a common repository for the user's favorite items.
CSIDL_FONTS 	0x0014 	The virtual folder that contains fonts.
CSIDL_MYMUSIC 	0x000d 	Folder that contains music files.
CSIDL_MYPICTURES 	0x0027 	Folder that contains picture files.
CSIDL_MYVIDEO 	0x000e 	Folder that contains video files.
CSIDL_PERSONAL 	0x0005 	The file system directory that serves as a common repository for documents.
CSIDL_PROFILE 	0x0028 	Folder that contains the profile of the user.
CSIDL_PROGRAM_FILES 	0x0026 	The program files folder.
CSIDL_PROGRAMS 	0x0002 	The file system directory that contains the user's program groups, which are also file system directories.
CSIDL_RECENT 	0x0008 	File system directory that contains the user's most recently used documents.
CSIDL_STARTUP 	0x0007 	The file system directory that corresponds to the user's Startup program group. The system starts these programs when a device is powered on.
CSIDL_WINDOWS 	0x0024 	The Windows folder.
#endif

typedef enum CONFIG_TYPE_V
{	CONFIG_STRING,
	CONFIG_STRING_LIST,
	CONFIG_TIMED_STRING_LIST,
	CONFIG_CHAR,
	CONFIG_UINT,
	CONFIG_LONG,
	CONFIG_ULONG,
	CONFIG_DOUBLE,
	CONFIG_BOOL,
	CONFIG_PORT,
	CONFIG_SYSTEMTIME,
	CONFIG_STRUCT,
	CONFIG_OBSOLETE,
	CONFIG_UNKNOWN
} CONFIG_TYPE_V;

typedef struct STRUCT_LIST_S
{	unsigned long Count;
	void **Structs;
} STRUCT_LIST_S;

typedef struct CONFIG_ITEM_S
{	char *Tag;
	void *pVoid;
	CONFIG_TYPE_V Type;
	BOOL structElement;
	size_t structOffset;
	char *structTag;
	union
	{
	struct
	{	int Len;
		// char *pValue;
		char *Default;
		BOOL Compress;
	} String;
	struct
	{	BOOL NoDupes;
		// STRING_LIST_S *pList;
	} StringList;
	struct
	{	BOOL NoDupes;
		char *tTag;	/* NULL for no time attribute */
		char *vTag;	/* NULL for no value attribute */
		// STRING_LIST_S *pList;
	} TimedStringList;
	struct
	{	// char *pValue;
		char Min, Max, Default;
	} Char;
	struct
	{	// UINT *pValue;
		UINT Min, Max, Default;
	} UInt;
	struct
	{	// long *pValue;
		long Min, Max, Default;
	} Long;
	struct
	{	// unsigned long *pValue;
		unsigned long Min, Max, Default;
	} ULong;
	struct
	{	int Decimals;
		// double *pValue;
		double Min, Max, Default;
	} Double;
	struct
	{	// BOOL *pValue;
		BOOL Default;
	} Bool;
	struct
	{	int Len;
		// char *pValue;
//		PORT_CONFIG_S *pStruct;
		char *Default;
	} Port;
	struct
	{	// SYSTEMTIME *pValue;
		SYSTEMTIME Default;
	} SystemTime;
	struct
	{	// STRUCT_LIST_S *pList;
		int Size;
		BOOL Hide;
		int eCount;
		int *Elements;
		void *pDefault;
	} Struct;
	struct
	{	char *NewTag;	/* May be NULL */
		void (*Initializer)(char *Tag, const char *Value);	/* May be NULL */
	} Obsolete;
	struct
	{	// char *pValue;
	} Unknown;
	};
} CONFIG_ITEM_S;

int ItemSize = 0;
int ItemCount = 0;
CONFIG_ITEM_S *Items = NULL;

CONFIG_ITEM_S *GetConfigItem(const char *Tag)
{	int i;
	for (i=ItemCount-1; i>=0; i--)
	{	if (!_stricmp(Items[i].Tag, Tag))
			return &Items[i];
	}
	return NULL;
}

CONFIG_ITEM_S *CreateConfigItem(char *Tag, CONFIG_TYPE_V Type, void *pVoid)
{	CONFIG_ITEM_S *Result = GetConfigItem(Tag);
	if (Result) return Result;
	int i = ItemCount++;
	if (ItemCount > ItemSize)
	{	ItemSize += 8;
		Items = (CONFIG_ITEM_S *) realloc(Items, sizeof(*Items)*ItemSize);
	}
	memset(&Items[i], 0, sizeof(Items[i]));
	Items[i].Tag = Tag;
	Items[i].Type = Type;
	Items[i].pVoid = pVoid;

	if (strchr(Tag,'.'))
	{	char *newTag = _strdup(Tag);
		char *d = strchr(newTag,'.');
		*d++ = '\0';	 /* Only keep the first part, point to after . */
		CONFIG_ITEM_S *newItem = GetConfigItem(newTag);
		if (newItem && newItem->Type == CONFIG_STRUCT)
		{	int e = newItem->Struct.eCount++;
			newItem->Struct.Elements = (int*)realloc(newItem->Struct.Elements,sizeof(*newItem->Struct.Elements)*newItem->Struct.eCount);
			newItem->Struct.Elements[e] = i;
			Items[i].structElement = TRUE;	/* Skip this one in normal loop processing */
			Items[i].structOffset = (char*)pVoid - (char*)newItem->Struct.pDefault;
			Items[i].structTag = _strdup(d);	/* Remember sub-tag name */
		}
		free(newTag);
	}
	return &Items[i];
}

char GetConfigMinChar(char *Tag)
{	CONFIG_ITEM_S *Result = GetConfigItem(Tag);
	if (Result && Result->Type == CONFIG_CHAR)
		return Result->Char.Min;
	if (!Result)
		TraceLog("Config", TRUE, NULL, "GetConfigMinChar:%s is UNDEFINED!\n", Tag);
	else TraceLog("Config", TRUE, NULL, "GetConfigMinChar:%s is Type(%ld) Not(%ld)\n", Tag, (long) Result->Type, (long) CONFIG_CHAR);
	return 0;
}
char GetConfigMaxChar(char *Tag)
{	CONFIG_ITEM_S *Result = GetConfigItem(Tag);
	if (Result && Result->Type == CONFIG_CHAR)
		return Result->Char.Max?Result->Char.Max:CHAR_MAX;
	if (!Result)
		TraceLog("Config", TRUE, NULL, "GetConfigMaxChar:%s is UNDEFINED!\n", Tag);
	else TraceLog("Config", TRUE, NULL, "GetConfigMaxChar:%s is Type(%ld) Not(%ld)\n", Tag, (long) Result->Type, (long) CONFIG_CHAR);
	return 127;
}

unsigned long GetConfigMinULong(char *Tag)
{	CONFIG_ITEM_S *Result = GetConfigItem(Tag);
	if (Result && Result->Type == CONFIG_ULONG)
		return Result->ULong.Min;
	if (!Result)
		TraceLog("Config", TRUE, NULL, "GetConfigMinULong:%s is UNDEFINED!\n", Tag);
	else TraceLog("Config", TRUE, NULL, "GetConfigMinULong:%s is Type(%ld) Not(%ld)\n", Tag, (long) Result->Type, (long) CONFIG_ULONG);
	return 0;
}
unsigned long GetConfigMaxULong(char *Tag)
{	CONFIG_ITEM_S *Result = GetConfigItem(Tag);
	if (Result && Result->Type == CONFIG_ULONG)
		return Result->ULong.Max?Result->ULong.Max:LONG_MAX;
	if (!Result)
		TraceLog("Config", TRUE, NULL, "GetConfigMaxULong:%s is UNDEFINED!\n", Tag);
	else TraceLog("Config", TRUE, NULL, "GetConfigMaxULong:%s is Type(%ld) Not(%ld)\n", Tag, (long) Result->Type, (long) CONFIG_ULONG);
	return 65535;
}

unsigned long GetSpinRangeULong(char *Tag)
{	return MAKELONG(GetConfigMaxULong(Tag), GetConfigMinULong(Tag));
}

long GetConfigMinLong(char *Tag)
{	CONFIG_ITEM_S *Result = GetConfigItem(Tag);
	if (Result && Result->Type == CONFIG_LONG)
		return Result->Long.Min;
	if (!Result)
		TraceLog("Config", TRUE, NULL, "GetConfigMinLong:%s is UNDEFINED!\n", Tag);
	else TraceLog("Config", TRUE, NULL, "GetConfigMinLong:%s is Type(%ld) Not(%ld)\n", Tag, (long) Result->Type, (long) CONFIG_LONG);
	return 0;
}
long GetConfigMaxLong(char *Tag)
{	CONFIG_ITEM_S *Result = GetConfigItem(Tag);
	if (Result && Result->Type == CONFIG_LONG)
		return Result->Long.Max?Result->Long.Max:LONG_MAX;
	if (!Result)
		TraceLog("Config", TRUE, NULL, "GetConfigMaxLong:%s is UNDEFINED!\n", Tag);
	else TraceLog("Config", TRUE, NULL, "GetConfigMaxLong:%s is Type(%ld) Not(%ld)\n", Tag, (long) Result->Type, (long) CONFIG_LONG);
	return 32767;
}

unsigned long GetSpinRangeLong(char *Tag)
{	return MAKELONG(GetConfigMaxLong(Tag), GetConfigMinLong(Tag));
}

double GetConfigMinDouble(char *Tag)
{	CONFIG_ITEM_S *Result = GetConfigItem(Tag);
	if (Result && Result->Type == CONFIG_DOUBLE)
		return Result->Double.Min;
	if (!Result)
		TraceLog("Config", TRUE, NULL, "GetConfigMinDouble:%s is UNDEFINED!\n", Tag);
	else TraceLog("Config", TRUE, NULL, "GetConfigMinDouble:%s is Type(%ld) Not(%ld)\n", Tag, (long) Result->Type, (long) CONFIG_DOUBLE);
	return 0.0;
}
double GetConfigMaxDouble(char *Tag)
{	CONFIG_ITEM_S *Result = GetConfigItem(Tag);
	if (Result && Result->Type == CONFIG_DOUBLE)
		return Result->Double.Max;
	if (!Result)
		TraceLog("Config", TRUE, NULL, "GetConfigMaxDouble:%s is UNDEFINED!\n", Tag);
	else TraceLog("Config", TRUE, NULL, "GetConfigMaxDouble:%s is Type(%ld) Not(%ld)\n", Tag, (long) Result->Type, (long) CONFIG_DOUBLE);
	return 100000.0;
}

void RegisterConfigurationStruct(char *Tag, int Size, STRUCT_LIST_S *pList, BOOL Hide = FALSE)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_STRUCT, pList);
	if (!Item->Struct.pDefault)
	{	pList->Structs = (void**)calloc(1,Size);
		Item->Struct.pDefault = (void*)pList->Structs;
		Item->Struct.Size = Size;
		Item->Struct.Hide = Hide;
		Item->Struct.eCount = 0;
		Item->Struct.Elements = NULL;
	}
	//TraceLog("Config", TRUE, NULL, "RegisterConfigurationStruct(%s)[%ld] Had %ld Records, resetting\n", Tag, Size, (long) pList->Count);
	pList->Count = 0;
}

void RegisterConfigurationString(char *Tag, int Len, char *pValue, char *Default, BOOL Compress=TRUE)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_STRING, pValue);
	Item->String.Len = Len;
	Item->String.Default = Default;
	strncpy(pValue, Default, Len);
}
void RegisterConfigurationTimedStringList(char *Tag, TIMED_STRING_LIST_S *pList, BOOL NoDupes = TRUE, char *vTag = NULL, BOOL NoTime=FALSE)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_TIMED_STRING_LIST, pList);
	Item->TimedStringList.NoDupes = NoDupes;
	if (!NoTime) Item->TimedStringList.tTag = "Time";
	Item->TimedStringList.vTag = vTag;
}
void RegisterConfigurationStringList(char *Tag, STRING_LIST_S *pList, BOOL NoDupes = TRUE)
{
#ifdef USE_TIMED_STRINGS
	RegisterConfigurationTimedStringList(Tag, pList, NoDupes, NULL, TRUE);
#else
	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_STRING_LIST, pList);
	Item->StringList.NoDupes = NoDupes;
#endif
}
void RegisterConfigurationChar(char *Tag, char *pValue, char Default, char Min, char Max)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_CHAR, pValue);
	Item->Char.Default = Default;
	Item->Char.Min = Min;
	Item->Char.Max = Max;
	*pValue = Default;
}
void RegisterConfigurationUInt(char *Tag, UINT *pValue, UINT Default, UINT Min, UINT Max)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_UINT, pValue);
	Item->UInt.Default = Default;
	Item->UInt.Min = Min;
	Item->UInt.Max = Max;
	*pValue = Default;
}
void RegisterConfigurationLong(char *Tag, long *pValue, long Default, long Min, long Max)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_LONG, pValue);
	Item->Long.Default = Default;
	Item->Long.Min = Min;
	Item->Long.Max = Max;
	*pValue = Default;
}
void RegisterConfigurationULong(char *Tag, unsigned long *pValue, unsigned long Default, unsigned long Min, unsigned long Max)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_ULONG, pValue);
	Item->ULong.Default = Default;
	Item->ULong.Min = Min;
	Item->ULong.Max = Max;
	*pValue = Default;
}
void RegisterConfigurationDouble(char *Tag, int Decimals, double *pValue, double Default, double Min, double Max)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_DOUBLE, pValue);
	Item->Double.Decimals = Decimals;
	Item->Double.Default = Default;
	Item->Double.Min = Min;
	Item->Double.Max = Max;
	*pValue = Default;
}
void RegisterConfigurationBool(char *Tag, BOOL *pValue, BOOL Default)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_BOOL, pValue);
	Item->Bool.Default = Default;
	*pValue = Default;
}
void RegisterConfigurationPort(char *Tag, int Len, char *pValue, char *Default)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_PORT, pValue);
	Item->Port.Len = Len;
	Item->Port.Default = Default;
	strncpy(pValue, Default, Len);
}
void RegisterConfigurationSystemTime(char *Tag, SYSTEMTIME *pValue, SYSTEMTIME *pDefault)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_SYSTEMTIME, pValue);
	if (pDefault)
		Item->SystemTime.Default = *pDefault;
	else memset(&Item->SystemTime.Default, 0, sizeof(Item->SystemTime.Default));
}
void RegisterConfigurationObsolete(char *Tag, char *NewTag, void (*Initializer)(char *Tag, const char *Value)=NULL)
{	CONFIG_ITEM_S *Item = CreateConfigItem(Tag, CONFIG_OBSOLETE, NULL);
	Item->Obsolete.NewTag = NewTag;
	Item->Obsolete.Initializer = Initializer;
}

BOOL ConvertSystemTime(const char *Value, SYSTEMTIME *pst)
{	char *e;
	SYSTEMTIME st = {0};

	if (pst) *pst = st;
	/* 2010-08-25T23:31:00, */
	st.wYear = (WORD) strtol(Value,&e,10);
	if (*e=='-')
	{	st.wMonth = (WORD) strtol(e+1,&e,10);
		if (*e=='-')
		{	st.wDay = (WORD) strtol(e+1,&e,10);
			if (*e=='T' || *e==' ')
			{	st.wHour = (WORD) strtol(e+1,&e,10);
				if (*e==':')
				{	st.wMinute = (WORD) strtol(e+1,&e,10);
					if (*e==':')
					{	st.wSecond = (WORD) strtol(e+1,&e,10);
						if (!*e || *e==',')
						{	if (pst) *pst = st;
							return TRUE;
						}
					}
				}
			}
		}
	}
	return FALSE;
}

#ifdef USE_TIMED_STRINGS
static int CompareStringListElement(const void *One, const void *Two)
{	TIMED_STRING_S *Left = (TIMED_STRING_S*) One;
	TIMED_STRING_S *Right = (TIMED_STRING_S*) Two;
	return strcmp(Left->string, Right->string);
}

void SortStringList(STRING_LIST_S *pList)
{	if (pList->Count > 1)
		qsort(pList->Entries, pList->Count, sizeof(*pList->Entries), CompareStringListElement);
}
#else
static int CompareStringListElement(const void *One, const void *Two)
{	char **Left = (char **) One;
	char **Right = (char **) Two;
	return strcmp(*Left, *Right);
}

void SortStringList(STRING_LIST_S *pList)
{	if (pList->Count > 1)
		qsort(pList->Strings, pList->Count, sizeof(*pList->Strings), CompareStringListElement);
}
#endif

unsigned long LocateTimedStringEntry(TIMED_STRING_LIST_S *pList, const char *string, size_t len/*=-1*/)
{	unsigned long i;

	for (i=0; i<pList->Count; i++)
		if ((len==-1 && !_stricmp(pList->Entries[i].string, string))
		|| (len != -1 && !_strnicmp(pList->Entries[i].string, string, len)))
			return i;
	return -1;
}

void RemoveTimedStringEntry(TIMED_STRING_LIST_S *pList, unsigned long i)
{
	if (i != -1 && i < pList->Count)	/* Don't remove invalids */
	{	free(pList->Entries[i].string);
		if (i != --pList->Count)
			pList->Entries[i] = pList->Entries[pList->Count];
	}
}

void EmptyTimedStringList(TIMED_STRING_LIST_S *pList)
{	unsigned long i;

	for (i=0; i<pList->Count; i++)
	{	free(pList->Entries[i].string);
	}
	pList->Count = pList->Size = 0;
	free(pList->Entries);
	pList->Entries = NULL;
}

size_t PurgeOldTimedStrings(TIMED_STRING_LIST_S *pList, unsigned long Seconds)
{	unsigned long i;
	size_t Count = 0;

	for (i=0; i<pList->Count; i++)
	if (SecondsSince(&pList->Entries[i].time) > Seconds)
	{
		free(pList->Entries[i].string);
		pList->Entries[i--] = pList->Entries[--pList->Count];
		Count++;	/* Count the deleted ones */
	}
	if (!pList->Count && Count) EmptyTimedStringList(pList);
	return Count;
}

unsigned long UpdateTimedStringEntryAt(TIMED_STRING_LIST_S *pList, unsigned long At, const char *string/*=NULL*/, SYSTEMTIME *pst/*=NULL*/, long value/*=0*/)
{
	if (string && strcmp(pList->Entries[At].string, string))
	{	free(pList->Entries[At].string);
		pList->Entries[At].string = _strdup(string);
	}
	if (pst) pList->Entries[At].time = *pst;
	else GetSystemTime(&pList->Entries[At].time);
	pList->Entries[At].value = value;
	return At;
}

unsigned long AddTimedStringEntryAt(TIMED_STRING_LIST_S *pList, unsigned long At, const char *string, SYSTEMTIME *pst/*=NULL*/, long value/*=0*/)
{
	if (At >= pList->Count)
	{	At = pList->Count++;
		if (pList->Count > pList->Size)
		{	pList->Size += 32;	/* Grow 16 strings at a time */
			pList->Entries = (TIMED_STRING_S *) realloc(pList->Entries, sizeof(*pList->Entries)*pList->Size);
		}
		memset(&pList->Entries[At], 0, sizeof(pList->Entries[At]));
		pList->Entries[At].string = _strdup(string);
	} else
	{	if (At != pList->Count-1)
		{	TIMED_STRING_S Temp = pList->Entries[At];
			memmove(&pList->Entries[At], &pList->Entries[At+1], sizeof(*pList->Entries)*(pList->Count-At-1));
			pList->Entries[pList->Count-1] = Temp;
			At = pList->Count-1;
		}
	}
	return UpdateTimedStringEntryAt(pList, At, string, pst, value);
}

unsigned long AddTimedStringEntry(TIMED_STRING_LIST_S *pList, const char *string, SYSTEMTIME *pst/*=NULL*/, long value/*=0*/)
{	return AddTimedStringEntryAt(pList, pList->Count, string, pst, value);
}

unsigned long AddOrUpdateTimedStringEntry(TIMED_STRING_LIST_S *pList, const char *string, SYSTEMTIME *pst/*=NULL*/, long value/*=0*/)
{	return AddTimedStringEntryAt(pList, LocateTimedStringEntry(pList, string), string, pst, value);
}

void AddSimpleStringEntry(STRING_LIST_S *pList, char *String)
{	AddTimedStringEntry(pList, String);
}

unsigned long LocateSimpleStringEntry(STRING_LIST_S *pList, const char *string)
{	return LocateTimedStringEntry(pList, string);
}

void RemoveSimpleStringEntry(TIMED_STRING_LIST_S *pList, unsigned long i)
{	RemoveTimedStringEntry(pList, i);
}

void EmptySimpleStringList(TIMED_STRING_LIST_S *pList)
{	EmptyTimedStringList(pList);
}

int CompareColonStrings(const char *s1, const char *s2)
{	const char *p, *q;

	for (p=s1, q=s2; *p && *p!=':' && *q && *q!=':'; p++, q++)
	{	if (*p > *q)
			return -1;	/* s1 is greater */
		else if (*p < *q)
			return 1;	/* s2 is greater */
	}
	if (!*p || *p==':')
	{	if (!*q || *q==':')
			return 0;	/* End of both, they matched */
		else return 1;	/* s2 is longer */
	} else return -1;	/* s1 is longer */
}

unsigned long LocateColonStringEntry(TIMED_STRING_LIST_S *pList, const char *string)
{	unsigned long i;

	for (i=0; i<pList->Count; i++)
	{	if (!CompareColonStrings(string, pList->Entries[i].string))
			return i;	/* Found it */
	}
	return -1;
}

unsigned long AddOrUpdateColonStringEntry(TIMED_STRING_LIST_S *pList, const char *string, SYSTEMTIME *pst)
{	return AddTimedStringEntryAt(pList, LocateColonStringEntry(pList, string), string, pst);
}

unsigned long AddColonStringEntry(STRING_LIST_S *pList, char *String, SYSTEMTIME *pst)
{	return AddTimedStringEntry(pList, String);
}

void RemoveColonStringEntry(TIMED_STRING_LIST_S *pList, unsigned long i)
{	RemoveTimedStringEntry(pList, i);
}

void EmptyColonStringList(TIMED_STRING_LIST_S *pList)
{	EmptyTimedStringList(pList);
}

typedef struct PARSE_STACK_S
{	long ValueCount;
	long ValueMax;
	char *ValueB;
	char *Name;
	long ElementCount;
	long ElementMax;
	long attrCount;
	char **attrNames;
	char **attrValues;
} PARSE_STACK_S;

typedef struct PARSE_INFO_S
{	XML_Parser Parser;
	HWND hwnd;
	long Depth;
	long MaxDepth;
	char *Callsign;
	PARSE_STACK_S *Stack;
} PARSE_INFO_S;

static char *GetAttrValue(PARSE_STACK_S *Stack, char *Name)
{	if (Stack)
	{	int a;
		for (a=0; a<Stack->attrCount; a++)
			if (!_stricmp(Stack->attrNames[a], Name))
				return Stack->attrValues[a];
	}
	return NULL;
}

static void SetVerifyConfigurationValue(char *Tag, const char *Value, PARSE_STACK_S *pStack=NULL)	/* Value = NULL for verify */
{	CONFIG_ITEM_S *Item = GetConfigItem(Tag);

	if (!Item)
	{	TraceLog("Config", TRUE, NULL, "SetVerifyConfigurationValue:Creating UNKNOWN Item %s\n", Tag);
		Item = CreateConfigItem(_strdup(Tag), CONFIG_UNKNOWN, NULL);
	}

	if (Value && Item && Item->Type != CONFIG_STRUCT)
		TraceLog("Config", FALSE, NULL, "SetVerifyConfigurationValue(%s) Value %s\n", Tag, Value);

	switch (Item->Type)
	{
	case CONFIG_UNKNOWN:
		if (Value)
		{	//if (Item->pVoid) free(Item->pVoid);
			Item->pVoid = _strdup(Value);
		}
		break;
	case CONFIG_OBSOLETE:
		if (Value && Item->Obsolete.NewTag)
		{	TraceLog("Config", TRUE, NULL,"Converting %s To %s, Value %s\n", Tag, Item->Obsolete.NewTag, Value);
			if (Item->Obsolete.Initializer)
				Item->Obsolete.Initializer(Item->Obsolete.NewTag, Value);
			else SetVerifyConfigurationValue(Item->Obsolete.NewTag, Value);
		}
		break;

	case CONFIG_STRUCT:	/* For adding structs from Obsoletes */
		if (Value && Item->Struct.eCount)
		{	int k = Item->Struct.Elements[0];	/* First element is key attribute */
			STRUCT_LIST_S *pList = (STRUCT_LIST_S *)Item->pVoid;	/* Array of instances */
			void *pValue;
			int p, ei;
			p = pList->Count++;
TraceLog("Config", FALSE, NULL, "Start(%s[%ld/%ld])\n", Item->Tag, (long) p, (long) pList->Count);
			pList->Structs = (void**)realloc(p?pList->Structs:NULL,Item->Struct.Size*pList->Count);
			pValue = ((char*)pList->Structs)+p*Item->Struct.Size;
			memcpy(pValue, Item->Struct.pDefault, Item->Struct.Size);	/* Set defaults */
			for (ei=0; ei<Item->Struct.eCount; ei++)
			{	int e = Item->Struct.Elements[ei];
				Items[e].pVoid = (void*)((char*)pValue + Items[e].structOffset);
			}
			SetVerifyConfigurationValue(Items[k].Tag, Value);
		} else TraceLog("Config", Value!=NULL, NULL, "No Value(%p) or eCount(%ld) for Struct(%s)\n",
						Value, (long) Item->Struct.eCount, Item->Tag);
		break;

	case CONFIG_STRING:
		if (Value) strncpy((char*)Item->pVoid, Value, Item->String.Len);
		((char*)Item->pVoid)[Item->String.Len-1] = '\0';
		if (Item->String.Compress) SpaceCompress(-1,(char*)Item->pVoid);
		break;
#ifndef USE_TIMED_STRINGS
	case CONFIG_STRING_LIST:
	{	if (Value)
		{	STRING_LIST_S *pList = (STRING_LIST_S*)Item->pVoid;
			unsigned long i;
			if (Item->StringList.NoDupes)
			{	for (i=0; i<pList->Count; i++)
					if (!_stricmp(pList->Strings[i], Value))
						break;
			} else i = pList->Count;	/* Always insert if not NoDupes */
			if (i >= pList->Count)
			{	i = pList->Count++;
				pList->Strings = (char **) realloc(pList->Strings, sizeof(*pList->Strings)*pList->Count);
				pList->Strings[i] = _strdup(Value);
			} else if (i != pList->Count-1)
			{	char *Temp = pList->Strings[i];
				memmove(&pList->Strings[i], &pList->Strings[i+1], sizeof(*pList->Strings)*(pList->Count-i-1));
				pList->Strings[pList->Count-1] = Temp;
			}
		}
		break;
	}
#endif
	case CONFIG_TIMED_STRING_LIST:
	{	if (Value)
		{	TIMED_STRING_LIST_S *pList = (TIMED_STRING_LIST_S*)Item->pVoid;
			unsigned long i;
			SYSTEMTIME st;
			char *a;
			long v = 0;

			if (ConvertSystemTime(Value, &st))
				Value += 20;	/* 2010-08-25T23:31:00, */
			else if (Item->TimedStringList.tTag
			&& (a=GetAttrValue(pStack, Item->TimedStringList.tTag))!=NULL)
			{	if (!ConvertSystemTime(a, &st))
				{	TraceLogThread("Config", TRUE, "Element(%s) Attribute(%s) Invalid Value(%s)\n",
									Item->Tag, Item->TimedStringList.tTag, a);
					GetSystemTime(&st);
				}
			}
			else GetSystemTime(&st);
			if (Item->TimedStringList.vTag
			&& (a=GetAttrValue(pStack, Item->TimedStringList.vTag))!=NULL)
			{	char *e;
				v = strtol(a,&e,10);
				if (*e) TraceLogThread("Config", TRUE, "Element(%s) Attribute(%s) Invalid Value(%s) Using %ld\n",
									Item->Tag, Item->TimedStringList.vTag, a, v);
			}
			if (Item->TimedStringList.NoDupes)
			{	i = LocateTimedStringEntry(pList, Value);
			} else i = pList->Count;	/* Always insert if not NoDupes */
			i = AddTimedStringEntryAt(pList, i, Value, &st, v);
		}
		break;
	}
	case CONFIG_CHAR:
	{	if (Value) *(char*)Item->pVoid = *Value;
		if (*(char*)Item->pVoid < Item->Char.Min) *(char*)Item->pVoid = Item->Char.Min;
		if (Item->Char.Max && *(char*)Item->pVoid > Item->Char.Max) *(char*)Item->pVoid = Item->Char.Max;
		break;
	}
	case CONFIG_UINT:
	{	char *e;
		if (Value) *(UINT*)Item->pVoid = strtoul(Value, &e, 10);
		if (*(UINT*)Item->pVoid < Item->UInt.Min) *(UINT*)Item->pVoid = Item->UInt.Min;
		if (Item->UInt.Max && *(UINT*)Item->pVoid > Item->UInt.Max) *(UINT*)Item->pVoid = Item->UInt.Max;
		break;
	}
	case CONFIG_LONG:
	{	char *e;
		if (Value) *(long*)Item->pVoid = strtol(Value, &e, 10);
		if (*(long*)Item->pVoid < Item->Long.Min) *(long*)Item->pVoid = Item->Long.Min;
		if (Item->Long.Max && *(long*)Item->pVoid > Item->Long.Max) *(long*)Item->pVoid = Item->Long.Max;
		break;
	}
	case CONFIG_ULONG:
	{	char *e;
		if (Value) *(unsigned long*)Item->pVoid = strtoul(Value, &e, 10);
		if (*(unsigned long*)Item->pVoid < Item->ULong.Min) *(unsigned long*)Item->pVoid = Item->ULong.Min;
		if (Item->ULong.Max	/* 0 max is unlimited */
		&& *(unsigned long*)Item->pVoid > Item->ULong.Max) *(unsigned long*)Item->pVoid = Item->ULong.Max;
		break;
	}
	case CONFIG_DOUBLE:
	{	char *e;
		if (Value) *(double*)Item->pVoid = strtod(Value, &e);
		if (*(double*)Item->pVoid < Item->Double.Min) *(double*)Item->pVoid = Item->Double.Min;
		if (Item->Double.Max && *(double*)Item->pVoid > Item->Double.Max) *(double*)Item->pVoid = Item->Double.Max;
		break;
	}
	case CONFIG_BOOL:
	{	char *e;
		if (Value) *(BOOL*)Item->pVoid = strtol(Value, &e, 10);
		if (*(BOOL*)Item->pVoid) *(BOOL*)Item->pVoid = TRUE;
		else *(BOOL*)Item->pVoid = FALSE;
		break;
	}
	case CONFIG_PORT:
		if (Value) strncpy((char*)Item->pVoid, Value, Item->Port.Len);
		((char*)Item->pVoid)[Item->String.Len-1] = '\0';
		SpaceTrim(-1,(char*)Item->pVoid);
		break;
	case CONFIG_SYSTEMTIME:
		if (Value) ConvertSystemTime(Value, (SYSTEMTIME*)Item->pVoid);
		break;
	}
}

void ValidateConfig(CONFIG_INFO_S *pConfig)
{	char *Start = (char*)pConfig, *End = (char*)(pConfig+1);
	int i;

	for (i=0; i<ItemCount; i++)
	if (Items[i].Type != CONFIG_OBSOLETE
	&& Items[i].Type != CONFIG_UNKNOWN
	&& !Items[i].structElement
	&& Items[i].pVoid)
	{	if (Items[i].pVoid < Start || Items[i].pVoid >= End)
			TraceLog("Config", TRUE, NULL, "Cannot Validate %s, %p Not Between %p and %p\n", Items[i].Tag, Items[i].pVoid, Start, End);
		else SetVerifyConfigurationValue(Items[i].Tag, NULL);
	}
}

char * XmlEncodeString(int Len, char * String)
{	long ReplaceCount = 0;
	char * p, * Encoded;
	long i;
static	long ReplaceMaxSize = 6;	/* 6 character max replacement */
static	char * ReplaceChars = "<&'\">|";
static	char * ReplaceWith[] = { "&lt;", "&amp;", "&apos;", "&quot;", "&gt;", "||" };

	if (Len == -1) Len = strlen(String);
	for (i=0; i<Len && String[i]; i++)
	{	if (strchr(ReplaceChars, String[i]))
			ReplaceCount++;
		else if (!isprint(String[i]&0xff) || (String[i]&0xff) > 0x7f)
			ReplaceCount++;
	}
	if (!ReplaceCount)
	{	return NULL;
	}
	p = Encoded = (char *) malloc(Len+ReplaceCount*ReplaceMaxSize+1);
	for (i=0; i<Len && String[i]; i++)
	{	char *r;
		if ((r=strchr(ReplaceChars, String[i])) != NULL)
		{	long x = r - ReplaceChars;
//			if (x >= ACOUNT(ReplaceWith)) KILLPROC(-1,"Replacement Coding Error");
			p += strlen(strcpy(p,ReplaceWith[x]));
		} else if (!isprint(String[i]&0xff) || (String[i]&0xff) > 0x7f)	/* |XX| */
		{	p += sprintf(p,"|%02lX|",(long)(String[i]&0xff));
		} else *p++ = String[i];
	}
	*p = '\0';
	return Encoded;
}

static void SaveXmlConfigurationItem(int i, FILE *Out)
{
#ifdef VERBOSE
	TraceLog("Config", TRUE, NULL, "Saving %s", Items[i].Tag);
#endif

	switch (Items[i].Type)
	{
	case CONFIG_STRUCT:
	{	STRUCT_LIST_S *pList = (STRUCT_LIST_S *)Items[i].pVoid;	/* Array of instances */
		unsigned long p;
		fprintf(Out,"\n");
		if (!pList->Count)
		{	if (Items[i].Struct.eCount && !Items[i].Struct.Hide)
			{	int e, ei;
				e = Items[i].Struct.Elements[0];
				fprintf(Out, "<!--%s %s=\"\"-->\n", Items[i].Tag, Items[e].structTag);
				for (ei=1; ei<Items[i].Struct.eCount; ei++)
				{	int e = Items[i].Struct.Elements[ei];
					if (Items[e].structElement)
					{	fprintf(Out, "<!--%s-->\n", Items[e].structTag);
					}
				}
				fprintf(Out, "<!--/%s-->\n\n", Items[i].Tag);
			}
		} else for (p=0; p<pList->Count; p++)
		{	void *pValue = ((char*)pList->Structs)+p*Items[i].Struct.Size;
			if (Items[i].Struct.eCount)
			{	size_t l = strlen(Items[i].Tag);
				int e, ei;
				e = Items[i].Struct.Elements[0];
				Items[e].pVoid = (void*)((char*)pValue + Items[e].structOffset);
				fprintf(Out, "<!--%s[%ld]-->\n<%s %s=\"%s\">\n", Items[i].Tag, (long) p, Items[i].Tag, Items[e].structTag, (char*)Items[e].pVoid);
				for (ei=1; ei<Items[i].Struct.eCount; ei++)
				{	e = Items[i].Struct.Elements[ei];
					if (Items[e].structElement)
					if (Items[e].Type == CONFIG_UNKNOWN)
					{	fprintf(Out, "<!--Dropping Unknown(%s)-->\n", Items[e].structTag);
					} else
					{	char *t = Items[e].Tag;
						Items[e].Tag = Items[e].structTag;
						Items[e].pVoid = (void*)((char*)pValue + Items[e].structOffset);
						SaveXmlConfigurationItem(e, Out);
						Items[e].Tag = t;
					}
				}
				fprintf(Out, "</%s>\n<!--%s[%ld]-->\n\n", Items[i].Tag, Items[i].Tag, (long) p);
			}
		}
		break;
	}
	case CONFIG_STRING:
	{	char *New = XmlEncodeString(Items[i].String.Len, (char*)Items[i].pVoid);
		if (New)
		{	fprintf(Out, "<%s>%s</%s>\n", Items[i].Tag, New, Items[i].Tag);
			free(New);
		} else fprintf(Out, "<%s>%.*s</%s>\n", Items[i].Tag, Items[i].String.Len, (char*)Items[i].pVoid, Items[i].Tag);
		break;
	}
#ifndef USE_TIMED_STRINGS
	case CONFIG_STRING_LIST:
	{	STRING_LIST_S *pList = (STRING_LIST_S*)Items[i].pVoid;
		if (!pList->Count)
			fprintf(Out, "<!--%s-->\n", Items[i].Tag);
		else for (unsigned long l=0; l<pList->Count; l++)
		{	char *New = XmlEncodeString(-1, pList->Strings[l]);
			if (New)
			{	fprintf(Out, "<%s>%s</%s>\n", Items[i].Tag, New, Items[i].Tag);
				free(New);
			} else fprintf(Out, "<%s>%s</%s>\n", Items[i].Tag, pList->Strings[l], Items[i].Tag);
		}
		break;
	}
#endif
	case CONFIG_TIMED_STRING_LIST:
	{	TIMED_STRING_LIST_S *pList = (TIMED_STRING_LIST_S*)Items[i].pVoid;
		if (!pList->Count)
			fprintf(Out, "<!--%s-->\n", Items[i].Tag);
		else for (unsigned long l=0; l<pList->Count; l++)
		{	char *p, *New = XmlEncodeString(-1, pList->Entries[l].string);
			for (p=New?New:pList->Entries[l].string; *p; p++)
				if (!isprint(*p&0xff)) break;
			if (*p)
				TraceLogThread("Config", TRUE, "NonPrintable %s In (%s)\n",
								Items[i].Tag, New);
			else
			{	fprintf(Out, "<%s", Items[i].Tag);

				if (Items[i].TimedStringList.tTag)
					fprintf(Out, " %s=\"%04ld-%02ld-%02ldT%02ld:%02ld:%02ld\"",
						Items[i].TimedStringList.tTag,
						(long) pList->Entries[l].time.wYear,
						(long) pList->Entries[l].time.wMonth,
						(long) pList->Entries[l].time.wDay,
						(long) pList->Entries[l].time.wHour,
						(long) pList->Entries[l].time.wMinute,
						(long) pList->Entries[l].time.wSecond);

				if (Items[i].TimedStringList.vTag)
					fprintf(Out, " %s=\"%ld\"", Items[i].TimedStringList.vTag, pList->Entries[l].value);

#ifdef OBSOLETE
				if (Items[i].TimedStringList.tTag)
					fprintf(Out, ">%04ld-%02ld-%02ldT%02ld:%02ld:%02ld,%s</%s>\n",
						(long) pList->Entries[l].time.wYear,
						(long) pList->Entries[l].time.wMonth,
						(long) pList->Entries[l].time.wDay,
						(long) pList->Entries[l].time.wHour,
						(long) pList->Entries[l].time.wMinute,
						(long) pList->Entries[l].time.wSecond,
						New?New:pList->Entries[l].string, Items[i].Tag);
				else
#endif
					fprintf(Out, ">%s</%s>\n",
						New?New:pList->Entries[l].string, Items[i].Tag);
			}
			if (New) free(New);
		}
		break;
	}
	case CONFIG_CHAR:
	{	char *Temp = XmlEncodeString(1, (char*) Items[i].pVoid);
		if (Temp)
		{	fprintf(Out, "<%s>%s</%s>\n", Items[i].Tag, Temp, Items[i].Tag);
			free(Temp);
		} else fprintf(Out, "<%s>%c</%s>\n", Items[i].Tag, *(char*)Items[i].pVoid, Items[i].Tag);
#ifdef OLD_WAY
		if (*(char*)Items[i].pVoid == '<')
			fprintf(Out, "<%s>&lt;</%s>\n", Items[i].Tag, Items[i].Tag);
		else if (*(char*)Items[i].pVoid == '>')
			fprintf(Out, "<%s>&gt;</%s>\n", Items[i].Tag, Items[i].Tag);
		else if (*(char*)Items[i].pVoid == '&')
			fprintf(Out, "<%s>&amp;</%s>\n", Items[i].Tag, Items[i].Tag);
		else fprintf(Out, "<%s>%c</%s>\n", Items[i].Tag, *(char*)Items[i].pVoid, Items[i].Tag);
#endif
		break;
	}
	case CONFIG_UINT:
		fprintf(Out, "<%s>%u</%s>\n", Items[i].Tag, *(UINT*)Items[i].pVoid, Items[i].Tag);
		break;
	case CONFIG_LONG:
		fprintf(Out, "<%s>%ld</%s>\n", Items[i].Tag, *(long*)Items[i].pVoid, Items[i].Tag);
		break;
	case CONFIG_ULONG:
		fprintf(Out, "<%s>%lu</%s>\n", Items[i].Tag, *(unsigned long *)Items[i].pVoid, Items[i].Tag);
		break;
	case CONFIG_DOUBLE:
		fprintf(Out, "<%s>%.*lf</%s>\n", Items[i].Tag, Items[i].Double.Decimals, *(double*)Items[i].pVoid, Items[i].Tag);
		break;
	case CONFIG_BOOL:
		fprintf(Out, "<%s>%ld</%s>\n", Items[i].Tag, *(BOOL*)Items[i].pVoid, Items[i].Tag);
		break;
	case CONFIG_PORT:
	{	char *New = XmlEncodeString(Items[i].Port.Len, (char*)Items[i].pVoid);
		if (New)
		{	fprintf(Out, "<%s>%s</%s>\n", Items[i].Tag, New, Items[i].Tag);
			free(New);
		} else fprintf(Out, "<%s>%.*s</%s>\n", Items[i].Tag, Items[i].Port.Len, (char*)Items[i].pVoid, Items[i].Tag);
		break;
	}
	case CONFIG_SYSTEMTIME:
	{	SYSTEMTIME *st = (SYSTEMTIME*)Items[i].pVoid;
		fprintf(Out, "<%s>%04ld-%02ld-%02ldT%02ld:%02ld:%02ld</%s>\n",
					Items[i].Tag,
					(long) st->wYear,
					(long) st->wMonth,
					(long) st->wDay,
					(long) st->wHour,
					(long) st->wMinute,
					(long) st->wSecond,
					Items[i].Tag);
		break;
	}
	case CONFIG_OBSOLETE:
		break;	/* Nothing to emit here, that's how they disappear! */
	case CONFIG_UNKNOWN:
	{	char *New = XmlEncodeString(-1, (char*)Items[i].pVoid);
		if (New)
		{	fprintf(Out, "<!-- USUPPORTED XML Tag Follows-->\n<%s>%s</%s>\n", Items[i].Tag, New, Items[i].Tag);
			free(New);
		} else fprintf(Out, "<!-- USUPPORTED XML Tag Follows-->\n<%s>%s</%s>\n", Items[i].Tag, (char*)Items[i].pVoid, Items[i].Tag);
		break;
	}
	}
}

static BOOL SaveXmlConfiguration(HWND hwnd, CONFIG_INFO_S *pConfig, char *Why)
{	char *File = GetXmlConfigFile(hwnd);
	FILE *Out = fopen(File,"wt");
	BOOL Result = FALSE;

	TraceLog("Config", TRUE, hwnd, "SaveXmlConfiguration(%s) %s%s\n", File, Why?Why:"", Out?"":" *FAILED*");
	if (Out)
	{	int i;
#ifdef TRAKVIEW
		fprintf(Out, "<TRAKVIEW>\n");
#else
		fprintf(Out, "<APRSISCE>\n");
#endif
		for (i=0; i<ItemCount; i++)
		if (!Items[i].structElement)
		{	SaveXmlConfigurationItem(i, Out);
		}
#ifdef VERBOSE
		else TraceLog("Config", TRUE, hwnd, "Skipped structElement[%ld] %s\n", (long) i, Items[i].Tag);
#endif
#ifdef TRAKVIEW
		fprintf(Out, "</TRAKVIEW>\n");
#else
		fprintf(Out, "</APRSISCE>\n");
#endif
		Result = !ferror(Out);
		fclose(Out);
	}
	return Result;
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

	Info->Stack[Info->Depth].attrCount = 0;
	Info->Stack[Info->Depth].ValueCount = 0;
	Info->Stack[Info->Depth].ElementCount = 0;
	{	const char **a;	/* Walk the attributes looking for key */
		Info->Stack[Info->Depth].attrCount = 0;
		for (a=atts; a && *a; a+=2)
			Info->Stack[Info->Depth].attrCount++;
		if (Info->Stack[Info->Depth].attrCount)
		{	Info->Stack[Info->Depth].attrNames = (char **)malloc(sizeof(Info->Stack[Info->Depth].attrNames)*Info->Stack[Info->Depth].attrCount);
			Info->Stack[Info->Depth].attrValues = (char **)malloc(sizeof(Info->Stack[Info->Depth].attrValues)*Info->Stack[Info->Depth].attrCount);
			Info->Stack[Info->Depth].attrCount = 0;
			for (a=atts; a && *a; a+=2)
			{	Info->Stack[Info->Depth].attrNames[Info->Stack[Info->Depth].attrCount] = _strdup(a[0]);
				Info->Stack[Info->Depth].attrValues[Info->Stack[Info->Depth].attrCount] = _strdup(a[1]);
				Info->Stack[Info->Depth].attrCount++;
//TraceLog("Config", TRUE, Info->hwnd, "Saving Attribute(%s=%s) On Tag(%s)\n", a[0], a[1], name);
			}
		}
	}
	if (Info->Stack[Info->Depth].ValueB) *Info->Stack[Info->Depth].ValueB = '\0';
	if (Info->Depth > 2)
	{	int Len = strlen(Info->Stack[Info->Depth-1].Name) + 1 + strlen(name) + 1;
		Info->Stack[Info->Depth].Name = (char *) malloc(Len);
		sprintf(Info->Stack[Info->Depth].Name, "%s.%s", Info->Stack[Info->Depth-1].Name, name);
	} else
	{	Info->Stack[Info->Depth].Name = _strdup(name);
		if (Info->Depth == 2)	/* Structs & Strings occur here and here only */
		{	CONFIG_ITEM_S *newItem = GetConfigItem(name);
			if (newItem)
			{	if (newItem->Type == CONFIG_STRUCT && newItem->Struct.eCount)
				{	int k = newItem->Struct.Elements[0];	/* First element is key attribute */
					const char **a;	/* Walk the attributes looking for key */
					for (a=atts; a && *a; a+=2)
					{	if (!_stricmp(a[0], Items[k].structTag))
						{	STRUCT_LIST_S *pList = (STRUCT_LIST_S *)newItem->pVoid;	/* Array of instances */
							void *pValue;
							int p, ei;
							p = pList->Count++;
TraceLog("Config", FALSE, Info->hwnd, "Start(%s[%ld/%ld])\n", name, (long) p, (long) pList->Count);
							if (!p) pList->Structs = (void**)malloc(newItem->Struct.Size*pList->Count);
							else pList->Structs = (void**)realloc(pList->Structs,newItem->Struct.Size*pList->Count);

							pValue = ((char*)pList->Structs)+p*newItem->Struct.Size;
							memcpy(pValue, newItem->Struct.pDefault, newItem->Struct.Size);	/* Set defaults */
							for (ei=0; ei<newItem->Struct.eCount; ei++)
							{	int e = newItem->Struct.Elements[ei];
								Items[e].pVoid = (void*)((char*)pValue + Items[e].structOffset);
							}
							SetVerifyConfigurationValue(Items[k].Tag, a[1]);
						} else TraceLog("Config", TRUE, Info->hwnd, "Ingoring Extra Attribute(%s) On Tag(%s)\n", a[0], name);
					}
					fflush(stdout);
				}
			}
		}
	}

#ifdef VERBOSE
	TraceLog("Config", FALSE, Info->hwnd, "[%ld]startElement(%s)\n", (long) Info->Depth, Info->Stack[Info->Depth].Name);
#endif

#ifdef FUTURE
	long AttrCount, AttrLen;
	const char **a;
	char * p;
	long i;

	for (AttrCount=0, AttrLen=0, a=atts; *a; AttrCount++, a+=2)
	{	if (a[1])
		{	AttrLen += strlen(a[1])+1;	/* Value */
		} else
		{	//ThSprintfErrorString(Routine, HERE, "Non-Even Attribute Count In '%s', Last Attribut '%s'\n", name, a[0]);
			break;
		}
	}

	Element = HEAP_CALLOC(Info->AllocationHeap, 1, sizeof(*Element) - (Info->Depth!=1?sizeof(Element->Top):0)
				+ sizeof(*Element->Attributes)*AttrCount + AttrLen);
	Element->MagicID = XML_ELEMENT_MAGIC;
	Element->LineNumber = XML_GetCurrentLineNumber(Info->Parser);
	if (Element->LineNumber > Info->MaxLineNumber)
		Info->MaxLineNumber = Element->LineNumber;
	Element->Name = XmlSaveLabel(Info, name);
	/* strcpy(Element->Name, name); This is redundant with XmlSaveLabel */
	Element->AttributeCount = AttrCount;
	Element->Attributes = (POINTER_F)(((char *)Element) + sizeof(*Element) - (Info->Depth!=1?sizeof(Element->Top):0));
	p = (((char *)Element->Attributes) + sizeof(*Element->Attributes)*AttrCount);
	for (i=0, a=atts; i<AttrCount; i++, a+=2)
	{	if (a[0] && a[1])
		{	Element->Attributes[i].Name = XmlSaveLabel(Info, a[0]);
			Element->Attributes[i].Value = p;
			strcpy(Element->Attributes[i].Value, a[1]);
			p += strlen(a[1])+1;
		} else
		{	ThSprintfErrorString(Routine, HERE, "Error ReScanning %ld Attributes In '%s'\n", (long) AttrCount, name);
			break;
		}
	}

	Info->Stack[Info->Depth].Element = Element;
	if (Info->Depth > 1)
	{	PARSE_STACK_S *S = &Info->Stack[Info->Depth-1];
		XML_ELEMENT_S *E = Info->Stack[Info->Depth-1].Element;
		Element->ParentElement = E;	/* My parent */
		i = S->ElementCount++;
		if (S->ElementCount > S->ElementMax)
		{	S->ElementMax += RtGetGrowthFactor(S->ElementMax);
			S->Elements = THREAD_REALLOC(S->Elements, S->ElementMax*sizeof(*S->Elements));
		}
		S->Elements[i] = Element;
	} else
	{	Element->Top.MagicID = XML_TOP_ELEMENT_MAGIC;
		Element->Top.LabelHash = Info->LabelHash;
		Element->Top.AllocationHeap = Info->AllocationHeap;
	}
#endif
}

static void endElement(void *userData, const char *name)
{	PARSE_INFO_S *Info = (PARSE_INFO_S *) userData;

//	Info->Stack[Info->Depth].Name has value Info->Stack[Info->Depth].ValueB IF Info->Stack[Info->Depth].ValueCount

#ifdef VERBOSE
TraceLog("Config", FALSE, Info->hwnd, "[%ld]endElement(%s) Value %.*s\n",
					(long) Info->Depth, Info->Stack[Info->Depth].Name,
					(int) Info->Stack[Info->Depth].ValueCount, Info->Stack[Info->Depth].ValueB);
#endif

	if (!Info->Stack[Info->Depth].ElementCount
	&& Info->Stack[Info->Depth].ValueB)
	{	if (strchr(Info->Stack[Info->Depth].ValueB,'|'))	/* Need un-escaping? */
		{	char *p;
			for (p=Info->Stack[Info->Depth].ValueB; *p; p++)
			{	if (*p == '|')
				{	if (p[1] == '|')
					{	memmove(p+1,p+2,strlen(p+2)+1);
					} else if (strlen(p)>=4&& p[3] == '|')
					{	unsigned long value;
						if (FromHex(p+1,2,&value))
						{	*p = value&0xff;
							memmove(p+1,p+4,strlen(p+4)+1);
						}
					}
				}
			}
		}
		SetVerifyConfigurationValue(Info->Stack[Info->Depth].Name, Info->Stack[Info->Depth].ValueB, &Info->Stack[Info->Depth]);
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

	if (Info->Stack[Info->Depth].attrCount)
	{	int a;
		for (a=0; a<Info->Stack[Info->Depth].attrCount; a++)
		{	free(Info->Stack[Info->Depth].attrNames[a]);
			free(Info->Stack[Info->Depth].attrValues[a]);
		}
		free(Info->Stack[Info->Depth].attrNames);
		free(Info->Stack[Info->Depth].attrValues);
		Info->Stack[Info->Depth].attrNames = Info->Stack[Info->Depth].attrValues = NULL;
	}

	Info->Depth--;
}

static void startCallsign(void *userData, const char *name, const char **atts)
{	PARSE_INFO_S *Info = (PARSE_INFO_S *)userData;

	if (Info->Depth) Info->Stack[Info->Depth].ElementCount++;
	Info->Depth++;
	if (Info->Depth >= Info->MaxDepth)
	{	Info->MaxDepth = Info->Depth+1;
		Info->Stack = (PARSE_STACK_S *)realloc(Info->Stack, sizeof(*Info->Stack)*Info->MaxDepth);
		memset(&Info->Stack[Info->Depth], 0, sizeof(Info->Stack[Info->Depth])/**(Info->MaxDepth-Info->Depth+1)*/);
	}

	Info->Stack[Info->Depth].attrCount = 0;
	Info->Stack[Info->Depth].ValueCount = 0;
	Info->Stack[Info->Depth].ElementCount = 0;
	if (Info->Stack[Info->Depth].ValueB) *Info->Stack[Info->Depth].ValueB = '\0';
	if (Info->Depth > 2)
	{	int Len = strlen(Info->Stack[Info->Depth-1].Name) + 1 + strlen(name) + 1;
		Info->Stack[Info->Depth].Name = (char *) malloc(Len);
		sprintf(Info->Stack[Info->Depth].Name, "%s.%s", Info->Stack[Info->Depth-1].Name, name);
	} else
	{	Info->Stack[Info->Depth].Name = _strdup(name);
	}

#ifdef VERBOSE
	TraceLog("Config", FALSE, Info->hwnd, "[%ld]startElement(%s)\n", (long) Info->Depth, Info->Stack[Info->Depth].Name);
#endif
}

static void endCallsign(void *userData, const char *name)
{	PARSE_INFO_S *Info = (PARSE_INFO_S *) userData;

//	Info->Stack[Info->Depth].Name has value Info->Stack[Info->Depth].ValueB IF Info->Stack[Info->Depth].ValueCount

#ifdef VERBOSE
TraceLog("Config", FALSE, Info->hwnd, "[%ld]endElement(%s) Value %.*s\n",
					(long) Info->Depth, Info->Stack[Info->Depth].Name,
					(int) Info->Stack[Info->Depth].ValueCount, Info->Stack[Info->Depth].ValueB);
#endif

	if (!Info->Stack[Info->Depth].ElementCount
	&& Info->Stack[Info->Depth].ValueB)
		if (!_stricmp(Info->Stack[Info->Depth].Name, "CallSign"))
			Info->Callsign = _strdup(Info->Stack[Info->Depth].ValueB);

	free(Info->Stack[Info->Depth].Name);
	Info->Stack[Info->Depth].Name = NULL;

	if (Info->Stack[Info->Depth].attrCount)
	{	int a;
		for (a=0; a<Info->Stack[Info->Depth].attrCount; a++)
		{	free(Info->Stack[Info->Depth].attrNames[a]);
			free(Info->Stack[Info->Depth].attrValues[a]);
		}
		free(Info->Stack[Info->Depth].attrNames);
		free(Info->Stack[Info->Depth].attrValues);
		Info->Stack[Info->Depth].attrNames = Info->Stack[Info->Depth].attrValues = NULL;
	}

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
	if (i && i>=len) return;

	if (Len+len+1 > Info->Stack[Info->Depth].ValueMax)
	{	//Info->Stack[Info->Depth].ValueMax += RtGetGrowthFactor(Info->Stack[Info->Depth].ValueMax);
		if (Len+len+1 > Info->Stack[Info->Depth].ValueMax)
			Info->Stack[Info->Depth].ValueMax = Len+len+1;
		Value = (char *) realloc(Value, Info->Stack[Info->Depth].ValueMax);
	}
	for (i=0; i<len; i++)
		if (s[i] != '\n' && s[i] != '\r')
			Value[Len++] = s[i];
	Value[Len] = '\0';
	Info->Stack[Info->Depth].ValueCount = Len;
	Info->Stack[Info->Depth].ValueB = Value;
}

static void startCdata(void *userData)
{
#ifdef VERBOSE
	printf("Not Sure What To Do With CData Start!\n");
#endif
}

static void endCdata(void *userData)
{
#ifdef VERBOSE
	printf("Not Sure What To Do With CData End!\n");
#endif
}

static void ConvertKISSPort(char *Tag, const char *Value)
{	if (Value && *Value)	/* Have a port? */
	{	SetVerifyConfigurationValue("RFPort", "KISS");	/* Create Port structure for KISS */

		SetVerifyConfigurationValue("RFPort.Device", Value);	/* And define the Device */
/*	And a bunch of default values NOT to include Enabled */
		SetVerifyConfigurationValue("RFPort.Protocol", "KISS");	/* Protocol is KISS */
		SetVerifyConfigurationValue("RFPort.RfBaud", "1200");
		SetVerifyConfigurationValue("RFPort.RFtoISEnabled", "1");
		SetVerifyConfigurationValue("RFPort.IStoRFEnabled", "1");
		SetVerifyConfigurationValue("RFPort.BeaconingEnabled", "1");
		SetVerifyConfigurationValue("RFPort.BeaconPath", "WIDE1-1,WIDE2-1");
		SetVerifyConfigurationValue("RFPort.BulletinObjectEnabled", "1");
		SetVerifyConfigurationValue("RFPort.MessagesEnabled", "1");
		SetVerifyConfigurationValue("RFPort.MessagePath", "WIDE1-1,WIDE2-1");
		SetVerifyConfigurationValue("RFPort.TelemetryEnabled", "1");
		SetVerifyConfigurationValue("RFPort.TelemetryPath", "WIDE1-1,WIDE2-1");
	}
}

static void ConvertAPRSServer(char *Tag, const char *Value)
{	CONFIG_ITEM_S *Item = GetConfigItem("APRSISPort");
	char Temp[64];

	if (Item && Item->Type == CONFIG_PORT)
	{	if (!strcmp((char*)Item->pVoid, Item->String.Default))
		{	sprintf(Temp, "@%s", Value);
			SetVerifyConfigurationValue("APRSISPort", Temp);
		} else if (*(char*)Item->pVoid != '@')
		{	sprintf(Temp, "@%s%s", Value, (char*)Item->pVoid);
			SetVerifyConfigurationValue("APRSISPort", Temp);
		}
		else TraceLog("Config", TRUE, NULL, "ConvertAPRSServer(%s) Already(%s) Ignoring(%s)(%s)\n",
				Item->Tag, Item->pVoid, Tag, Value);
	}
	else TraceLog("Config", TRUE, NULL, "ConvertAPRSServer(%s)(%s): Item(%p) Type(%ld) Not(%ld)\n",
				Tag, Value, Item, Item?Item->Type:0, (long) CONFIG_STRING);
}

static void ConvertAPRSPort(char *Tag, const char *Value)
{	CONFIG_ITEM_S *Item = GetConfigItem("APRSISPort");
	char Temp[64];

	if (Item && Item->Type == CONFIG_PORT)
	{	if (!strcmp((char*)Item->pVoid, Item->String.Default))
		{	sprintf(Temp, ":%s", Value);
			SetVerifyConfigurationValue("APRSISPort", Temp);
		} else if (*(char*)Item->pVoid != ':')
		{	sprintf(Temp, "%s:%s", (char*)Item->pVoid, Value);
			SetVerifyConfigurationValue("APRSISPort", Temp);
		}
		else TraceLog("Config", TRUE, NULL, "ConvertAPRSPort(%s) Already(%s) Ignoring(%s)(%s)\n",
				Item->Tag, Item->pVoid, Tag, Value);
	}
	else TraceLog("Config", TRUE, NULL, "ConvertAPRSPort(%s)(%s): Item(%p) Type(%ld) Not(%ld)\n",
				Tag, Value, Item, Item?Item->Type:0, (long) CONFIG_STRING);
}

static void ConvertAGWPort(char *Tag, const char *Value)
{	if (Value && *Value)	/* Have a port? */
	{	SetVerifyConfigurationValue("RFPort", "AGW");	/* Create Port structure for AGW */

		SetVerifyConfigurationValue("RFPort.Device", Value);	/* And define the Device */
/*	And a bunch of default values NOT to include Enabled */
		SetVerifyConfigurationValue("RFPort.Protocol", "AGW");	/* Protocol is AGW */
		SetVerifyConfigurationValue("RFPort.RfBaud", "300");
		SetVerifyConfigurationValue("RFPort.RFtoISEnabled", "1");
		SetVerifyConfigurationValue("RFPort.IStoRFEnabled", "1");
		SetVerifyConfigurationValue("RFPort.BeaconingEnabled", "1");
		SetVerifyConfigurationValue("RFPort.BeaconPath", "WIDE1-1,WIDE2-1");
		SetVerifyConfigurationValue("RFPort.BulletinObjectEnabled", "1");
		SetVerifyConfigurationValue("RFPort.MessagesEnabled", "1");
		SetVerifyConfigurationValue("RFPort.MessagePath", "WIDE1-1,WIDE2-1");
		SetVerifyConfigurationValue("RFPort.TelemetryEnabled", "1");
		SetVerifyConfigurationValue("RFPort.TelemetryPath", "WIDE1-1,WIDE2-1");
	}
}

static void ConvertTEXTPort(char *Tag, const char *Value)
{	if (Value && *Value)	/* Have a port? */
	{	SetVerifyConfigurationValue("RFPort", "TEXT");	/* Create Port structure for KISS */

		SetVerifyConfigurationValue("RFPort.Device", Value);	/* And define the Device */
/*	And a bunch of default values NOT to include Enabled */
		SetVerifyConfigurationValue("RFPort.Protocol", "TEXT");	/* Protocol is TEXT */
		SetVerifyConfigurationValue("RFPort.RfBaud", "300");
		SetVerifyConfigurationValue("RFPort.RFtoISEnabled", "1");
		SetVerifyConfigurationValue("RFPort.IStoRFEnabled", "1");
		SetVerifyConfigurationValue("RFPort.BeaconingEnabled", "1");
		SetVerifyConfigurationValue("RFPort.BeaconPath", "WIDE1-1,WIDE2-1");
		SetVerifyConfigurationValue("RFPort.BulletinObjectEnabled", "1");
		SetVerifyConfigurationValue("RFPort.MessagesEnabled", "1");
		SetVerifyConfigurationValue("RFPort.MessagePath", "WIDE1-1,WIDE2-1");
		SetVerifyConfigurationValue("RFPort.TelemetryEnabled", "1");
		SetVerifyConfigurationValue("RFPort.TelemetryPath", "WIDE1-1,WIDE2-1");
	}
}

static void ConvertNWSOffice(char *Tag, const char *Value)
{	char *t = _strdup(Value);
	size_t Len = strlen(t);
	if (Len>1 && t[Len-1] == '*')
		t[Len-1] = '\0';	/* Wipe out the * */
	SetVerifyConfigurationValue("NWS.CWA", t);
	free(t);
}

static void ConvertAltNetChoice(char *Tag, const char *Value)
{	SetVerifyConfigurationValue("AltNet.Choice", *Value=='*'?Value+1:Value);
}

static void DefineDefaultConfig(CONFIG_INFO_S *pConfig)
{	RegisterConfigurationString("Version", sizeof(pConfig->Version), (char *) &pConfig->Version, Timestamp);
	RegisterConfigurationSystemTime("LastSaved", &pConfig->stLastSaved, NULL);
	RegisterConfigurationString("LastSaveWhy", sizeof(pConfig->LastWhy), (char *) &pConfig->LastWhy, "");
	RegisterConfigurationString("CallSign", sizeof(pConfig->CallSign), (char *) &pConfig->CallSign, DEFAULT_CALLSIGN);
	RegisterConfigurationString("Password", sizeof(pConfig->Password), (char *) &pConfig->Password, "-1");
	RegisterConfigurationBool("SuppressUnverified", &pConfig->SuppressUnverified, FALSE);

	RegisterConfigurationString("Filter", sizeof(pConfig->Filter), (char *) &pConfig->Filter, "", FALSE);
	RegisterConfigurationString("IStoRFFilter", sizeof(pConfig->IStoRFFilter), (char *) &pConfig->IStoRFFilter, "", FALSE);
	RegisterConfigurationULong("CommentInterval", &pConfig->CommentInterval, 0, 0, 60);
#ifdef TRAKVIEW
	RegisterConfigurationString("Comment", sizeof(pConfig->Comment), (char *) &pConfig->Comment, "TrakView Viewer");
#else
#ifdef UNDER_CE
	RegisterConfigurationString("Comment", sizeof(pConfig->Comment), (char *) &pConfig->Comment, "APRS-IS for CE");
#else
	RegisterConfigurationString("Comment", sizeof(pConfig->Comment), (char *) &pConfig->Comment, "APRS-IS for Win32");
#endif
#endif
	RegisterConfigurationStringList("CommentChoice", &pConfig->CommentChoices);
	RegisterConfigurationChar("Symbol.Table", &pConfig->Symbol.Table, '/', '/', '\\');
#ifdef UNDER_CE
	RegisterConfigurationChar("Symbol.Symbol", &pConfig->Symbol.Symbol, '$', '!', '~');
#else
	RegisterConfigurationChar("Symbol.Symbol", &pConfig->Symbol.Symbol, 'l', '!', '~');
#endif
	RegisterConfigurationStringList("SymbolChoice", &pConfig->SymbolChoices);
	RegisterConfigurationString("AltNet", sizeof(pConfig->AltNet), (char *) &pConfig->AltNet, "");
	RegisterConfigurationTimedStringList("AltNet.Choice", &pConfig->AltNetChoices, TRUE, "Used", TRUE);

	RegisterConfigurationDouble("MoveSpeed", 2, &pConfig->MoveSpeed, 0.5, 0.0, 10.0);
	RegisterConfigurationULong("Range", &pConfig->Range, 500, 0, 10000);
	RegisterConfigurationULong("QuietTime", &pConfig->QuietTime, 60, 60, 5*60);
	RegisterConfigurationULong("ForceMinTime", &pConfig->ForceMinTime, 5, 2, 2*60);

	RegisterConfigurationULong("Aging.TelemetryDefs.Days", &pConfig->Aging.TelemetryDefDays, 14, 0, 365);
	RegisterConfigurationULong("Aging.Bulletins.Hours", &pConfig->Aging.BulletinHours, 48, 0, 744);
	RegisterConfigurationULong("Stations.Tracks.Hours", &pConfig->Aging.TrackHours, 24, 0, 168);
	RegisterConfigurationULong("Stations.Tracks.Buddy.Hours", &pConfig->Aging.BuddyTrackHours, 0, 0, 168);
	RegisterConfigurationULong("Objects.DefaultInterval", &pConfig->Aging.DefaultObjectInterval, 10, 0, 60);
	RegisterConfigurationULong("Objects.MaxObjectKillXmits", &pConfig->Aging.MaxObjectKillXmits, 6, 0, 10);

	RegisterConfigurationULong("Stations.MinAge", &pConfig->Stations.MinAge, 80, 10, 1440);
	RegisterConfigurationULong("Stations.MaxAge", &pConfig->Stations.MaxAge, 120, 0, 1440);
	RegisterConfigurationULong("Stations.MaxAge.Buddy", &pConfig->Stations.BuddyMaxAge, 0, 0, 1440);
	RegisterConfigurationULong("Stations.AvgIntervals", &pConfig->Stations.AvgIntervals, 0, 0, 0);
	RegisterConfigurationULong("Stations.AvgCount", &pConfig->Stations.AvgCount, 0, 0, 0);
	RegisterConfigurationBool("Stations.AvgFixed", &pConfig->Stations.AvgFixed, FALSE);

	RegisterConfigurationULong("DX.MinDist", &pConfig->DX.MinDist, 5, 0, 1000);
	RegisterConfigurationULong("DX.MinTrigger", &pConfig->DX.MinTrigger, 10, 0, 1000);
	RegisterConfigurationULong("DX.MinInterval", &pConfig->DX.MinInterval, 10, 0, 1440);
	RegisterConfigurationULong("DX.Window", &pConfig->DX.Window, 60, 0, 1440);
	RegisterConfigurationString("DX.MaxEver.Station", sizeof(pConfig->DX.MaxEver.Station), (char *) &pConfig->DX.MaxEver.Station, "");
	RegisterConfigurationSystemTime("DX.MaxEver.When", &pConfig->DX.MaxEver.st, NULL);
	RegisterConfigurationSystemTime("DX.MaxEver.First", &pConfig->DX.MaxEver.stFirst, NULL);
	RegisterConfigurationDouble("DX.MaxEver.Latitude", 2, &pConfig->DX.MaxEver.lat, 0.0, -90.0, 90.0);
	RegisterConfigurationDouble("DX.MaxEver.Longitude", 2, &pConfig->DX.MaxEver.lon, 0.0, -180.0, 180.0);
	RegisterConfigurationDouble("DX.MaxEver.Distance", 2, &pConfig->DX.MaxEver.Distance, 0.0, 0.0, 0.0);
	RegisterConfigurationDouble("DX.MaxEver.Bearing", 2, &pConfig->DX.MaxEver.Bearing, 0.0, 0.0, 360.0);
	RegisterConfigurationULong("DX.MaxEver.Count", &pConfig->DX.MaxEver.Count, 0, 0, 1000);
	RegisterConfigurationTimedStringList("DX.Excluded", &pConfig->DX.Excluded);

#ifdef TRAKVIEW
	RegisterConfigurationBool("Enables.APRSIS", &pConfig->Enables.APRSIS, FALSE);
#else
	RegisterConfigurationBool("Enables.APRSIS", &pConfig->Enables.APRSIS, TRUE);
#endif
	RegisterConfigurationBool("Enables.RFPorts", &pConfig->Enables.RFPorts, TRUE);
	RegisterConfigurationBool("Enables.RFPktLog", &pConfig->Enables.RFPktLog, FALSE);
	RegisterConfigurationBool("Enables.RFReceiveOnly", &pConfig->Enables.RFReceiveOnly, FALSE);
	RegisterConfigurationBool("Enables.GPS", &pConfig->Enables.GPS, TRUE);
	RegisterConfigurationBool("Enables.Internet", &pConfig->Enables.Internet, TRUE);
	RegisterConfigurationBool("Enables.OSMFetch", &pConfig->Enables.OSMFetch, TRUE);
	RegisterConfigurationBool("Enables.Sound", &pConfig->Enables.Sound, TRUE);
	RegisterConfigurationBool("Enables.NotifyOnNewBulletin", &pConfig->Enables.NotifyOnNewBulletin, FALSE);
	RegisterConfigurationBool("Enables.CSVFile", &pConfig->Enables.CSVFile, FALSE);
	RegisterConfigurationBool("Enables.AutoSaveGPX", &pConfig->Enables.AutoSaveGPX, FALSE);
	RegisterConfigurationBool("Enables.Beacons", &pConfig->Enables.Beacons, TRUE);
#ifdef UNDER_CE
	RegisterConfigurationBool("Enables.Telemetry", &pConfig->Enables.Telemetry, TRUE);
#else
	RegisterConfigurationBool("Enables.Telemetry", &pConfig->Enables.Telemetry, FALSE);
#endif

	RegisterConfigurationBool("Enables.MicENotification", &pConfig->Enables.MicENotification, TRUE);
	RegisterConfigurationBool("Enables.MicEEmergency", &pConfig->Enables.MicEEmergency, TRUE);

	RegisterConfigurationBool("Enables.Debug", &pConfig->Enables.DebugGeneral, FALSE);
	RegisterConfigurationBool("Enables.DebugFile", &pConfig->Enables.DebugFile, FALSE);
	RegisterConfigurationBool("Enables.DebugStartup", &pConfig->Enables.DebugStartup, FALSE);

	RegisterConfigurationBool("Enables.AllMessages", &pConfig->Messaging.AllMessages, FALSE);
	RegisterConfigurationBool("Enables.RFMessages", &pConfig->Messaging.RFMessages, FALSE);
	RegisterConfigurationBool("Enables.MyMessages", &pConfig->Messaging.MyMessages, FALSE);
	RegisterConfigurationBool("Enables.HideNWS", &pConfig->Messaging.HideNWS, FALSE);
	RegisterConfigurationBool("Enables.HideQueries", &pConfig->Messaging.HideQueries, FALSE);
	RegisterConfigurationBool("Enables.NotifyOnQuery", &pConfig->Messaging.NotifyOnQuery, TRUE);
	RegisterConfigurationBool("Enables.NotifyOnNewMessage", &pConfig->Messaging.NotifyOnNewMessage, TRUE);
	RegisterConfigurationBool("Enables.MultiTrackItemInMessage", &pConfig->Messaging.MultiTrackItemInMessage, TRUE);

	RegisterConfigurationULong("AutoReply.DelayMinutes", &pConfig->Messaging.AutoAnswer.Delay, 5, 0, LONG_MAX);
	RegisterConfigurationULong("AutoReply.IntervalMinutes", &pConfig->Messaging.AutoAnswer.Interval, 60, 1, LONG_MAX);
	RegisterConfigurationString("AutoReply.Reply", sizeof(pConfig->Messaging.AutoAnswer.Reply), (char *) &pConfig->Messaging.AutoAnswer.Reply, "");
	RegisterConfigurationStringList("AutoReply.ReplyChoice", &pConfig->Messaging.AutoAnswer.ReplyChoices);
	RegisterConfigurationTimedStringList("AutoReply.Stations", &pConfig->Messaging.AutoAnswer.Stations);

	RegisterConfigurationBool("QRU.Enabled", &pConfig->QRU.Enabled, FALSE);
	RegisterConfigurationBool("QRU.RetryMessages", &pConfig->QRU.RetryMessages, FALSE);
	RegisterConfigurationULong("QRU.Interval", &pConfig->QRU.Interval, 10, 0, 1440);
	RegisterConfigurationULong("QRU.MaxObjects", &pConfig->QRU.MaxObjs, 5, 0, 30);
	RegisterConfigurationULong("QRU.Range", &pConfig->QRU.Range, 20, 0, 1440);
	RegisterConfigurationSystemTime("QRU.LastTransmit", &pConfig->QRU.LastTransmit, NULL);

	RegisterConfigurationULong("MaxGroupObjects", &pConfig->MaxGroupObjs, 5, 0, 30);

	RegisterConfigurationULong("Genius.MinTime", &pConfig->MyGenius.MinTime, 10, 10, 5*60);
	RegisterConfigurationULong("Genius.MaxTime", &pConfig->MyGenius.MaxTime, 30, 1, 60);
	RegisterConfigurationBool("Genius.TimeOnly", &pConfig->MyGenius.TimeOnly, FALSE);
	RegisterConfigurationBool("Genius.StartStop", &pConfig->MyGenius.StartStop, TRUE);
	RegisterConfigurationULong("Genius.BearingChange", &pConfig->MyGenius.BearingChange, 30, 5, 180);
	RegisterConfigurationDouble("Genius.ForecastError", 2, &pConfig->MyGenius.ForecastError, 0.1, 0.1, 2.0);
	RegisterConfigurationDouble("Genius.MaxDistance", 2, &pConfig->MyGenius.MaxDistance, 1.0, 0.1, 20.0);

	RegisterConfigurationBool("Beacon.OnlyAfterTransmit", &pConfig->Beacon.AfterTransmit, FALSE);
	RegisterConfigurationBool("Beacon.Timestamp", &pConfig->Beacon.Timestamp, TRUE);
	RegisterConfigurationBool("Beacon.HHMMSS", &pConfig->Beacon.HHMMSS, TRUE);
	RegisterConfigurationBool("Beacon.Compressed", &pConfig->Beacon.Compressed, FALSE);
	RegisterConfigurationBool("Beacon.CourseSpeed", &pConfig->Beacon.CourseSpeed, TRUE);
	RegisterConfigurationBool("Beacon.Altitude", &pConfig->Beacon.Altitude, TRUE);
	RegisterConfigurationBool("Beacon.Why", &pConfig->Beacon.Why, FALSE);
	RegisterConfigurationBool("Beacon.Comment", &pConfig->Beacon.Comment, TRUE);
	RegisterConfigurationLong("Beacon.Precision", &pConfig->Beacon.Precision, 0, -4, 2);
#ifdef UNDER_CE
	RegisterConfigurationBool("Beacon.Cellular", &pConfig->Beacon.Cellular, FALSE);
#endif
	RegisterConfigurationString("Beacon.MicETag", sizeof(pConfig->Beacon.MicETag), (char *) &pConfig->Beacon.MicETag, "");

	RegisterConfigurationBool("Status.Enabled", &pConfig->Status.Enabled, FALSE);
	RegisterConfigurationULong("Status.Interval", &pConfig->Status.Interval, 30, 0, 60);
#ifdef UNDER_CE
	RegisterConfigurationBool("Status.Cellular", &pConfig->Status.Cellular, TRUE);
#endif
	RegisterConfigurationBool("Status.Timestamp", &pConfig->Status.Timestamp, TRUE);
	RegisterConfigurationBool("Status.GridSquare", &pConfig->Status.GridSquare, FALSE);
	RegisterConfigurationBool("Status.DX", &pConfig->Status.DX, FALSE);
	RegisterConfigurationString("Status.Text", sizeof(pConfig->Status.Text), (char *) &pConfig->Status.Text, "APRSISCE/32");
	RegisterConfigurationStringList("Status.Choice", &pConfig->Status.Choices);
	RegisterConfigurationSystemTime("Status.LastSent", &pConfig->Status.LastSent, NULL);

	RegisterConfigurationDouble("Scale", 4, &pConfig->Scale, DEFAULT_SCALE, MIN_SCALE, MAX_SCALE);
	RegisterConfigurationULong("Orientation", &pConfig->Orientation, 2, 0, 2);

	RegisterConfigurationDouble("Latitude", 6, &pConfig->Latitude, 0.0, -90.0, 90.0);
	RegisterConfigurationDouble("Longitude", 6, &pConfig->Longitude, 0.0, -180.0, 180.0);
	RegisterConfigurationDouble("Altitude", 4, &pConfig->Altitude, 0.0, 0.0, 10000.0);

	RegisterConfigurationString("BeaconPath", sizeof(pConfig->BeaconPath), (char *) &pConfig->BeaconPath, "WIDE1-1,WIDE2-1");
	RegisterConfigurationString("DXPath", sizeof(pConfig->DXPath), (char *) &pConfig->DXPath, "RFONLY");
	RegisterConfigurationString("EMailServer", sizeof(pConfig->EMailServer), (char *) &pConfig->EMailServer, "EMAIL-2");
	RegisterConfigurationString("WhoIsServer", sizeof(pConfig->WhoIsServer), (char *) &pConfig->WhoIsServer, "WHO-IS");
	RegisterConfigurationBool("WhoIsFull", &pConfig->WhoIsFull, FALSE);
	RegisterConfigurationStringList("MessageGroup", &pConfig->MessageGroups);

	RegisterConfigurationBool("DefaultedURLs", &pConfig->DefaultedURLs, FALSE);
	RegisterConfigurationTimedStringList("StationURL", &pConfig->StationURLs, TRUE);
	RegisterConfigurationTimedStringList("TelemetryURL", &pConfig->TelemetryURLs, TRUE);
	RegisterConfigurationTimedStringList("WeatherURL", &pConfig->WeatherURLs, TRUE);

	RegisterConfigurationBool("NWS.Messages", &pConfig->NWS.Messages, TRUE);
	RegisterConfigurationBool("NWS.MessagesNotAll", &pConfig->NWS.MessagesNotAll, TRUE);
	RegisterConfigurationBool("NWS.Notify", &pConfig->NWS.Notify, FALSE);
	RegisterConfigurationBool("NWS.MultiTrack", &pConfig->NWS.MultiTrack, FALSE);
	RegisterConfigurationBool("NWS.MultiTrackNew", &pConfig->NWS.MultiTrackNew, FALSE);
	RegisterConfigurationBool("NWS.MultiTrackLines", &pConfig->NWS.MultiTrackLinesOnly, TRUE);
	RegisterConfigurationBool("NWS.MultiTrackMoving", &pConfig->NWS.MultiTrackMoving, FALSE);
	RegisterConfigurationBool("NWS.MultiTrackRange", &pConfig->NWS.MultiTrackRange, FALSE);
	RegisterConfigurationBool("NWS.MultiTrackMe", &pConfig->NWS.MultiTrackMe, FALSE);
	RegisterConfigurationBool("NWS.MultiTrackSpecific", &pConfig->NWS.MultiTrackPreferML, TRUE);
	RegisterConfigurationBool("NWS.MultiTrackAutoClose", &pConfig->NWS.MultiTrackCloseOnExpire, TRUE);
	RegisterConfigurationTimedStringList("NWS.CWA", &pConfig->NWS.Offices, TRUE, "Disabled", TRUE);
#ifdef SUPPORT_SHAPEFILES
	RegisterConfigurationULong("NWS.Opacity", &pConfig->NWS.Opacity, 10, 0, 100);
#ifdef UNDER_CE
	RegisterConfigurationULong("NWS.Quality", &pConfig->NWS.Quality, 50, 0, 100);
#else
	RegisterConfigurationULong("NWS.Quality", &pConfig->NWS.Quality, 100, 0, 100);
#endif
	RegisterConfigurationBool("NWS.ShapesEnabled", &pConfig->NWS.ShapesEnabled, TRUE);
	RegisterConfigurationTimedStringList("NWS.ShapeFile", &pConfig->NWS.ShapeFiles, TRUE, "Disabled", TRUE);
#endif

	RegisterConfigurationBool("MultiTrack.ViewNone", &pConfig->MultiTrack.ViewNone, FALSE);
	RegisterConfigurationLong("MultiTrack.width", &pConfig->MultiTrack.width, 0, 0x80000000, 0x7FFFFFFF);
	RegisterConfigurationLong("MultiTrack.height", &pConfig->MultiTrack.height, 0, 0x80000000, 0x7FFFFFFF);
	RegisterConfigurationULong("MultiTrack.Orientation", &pConfig->MultiTrack.Orientation, 2, 0, 2);
	RegisterConfigurationULong("MultiTrack.Percent", &pConfig->MultiTrack.Percent, 50, 0, 100);
	RegisterConfigurationLong("MultiTrack.Zoom", &pConfig->MultiTrack.Zoom, (MIN_OSM_ZOOM+MAX_OSM_ZOOM)/2, MIN_OSM_ZOOM, MAX_OSM_ZOOM);
	RegisterConfigurationDouble("MultiTrack.Scale", 4, &pConfig->MultiTrack.Scale, DEFAULT_SCALE, MIN_SCALE, MAX_SCALE);

#ifdef FOR_INFO_ONLY
OSM Mapnik 	 http://tile.openstreetmap.org/12/2047/1362.png 	 0-18
OSM Osmarender/Tiles@Home	http://tah.openstreetmap.org/Tiles/tile/12/2047/1362.png 	0-17
OSM Cycle Map 	http://andy.sandbox.cloudmade.com/tiles/cycle/12/2047/1362.png 	0-18
OSM CloudMade Web style 	http://tile.cloudmade.com/<YOUR CLOUDMADE API KEY>/1/256/12/2047/1362.png 	0-18
OSM CloudMade Fine line style 	http://tile.cloudmade.com/<YOUR CLOUDMADE API KEY>/2/256/12/2047/1362.png 	0-18
OSM CloudMade NoNames style 	http://tile.cloudmade.com/<YOUR CLOUDMADE API KEY>/3/256/12/2047/1362.png 	0-18 
#endif



	RegisterConfigurationLong("WindowPlacement.x", &pConfig->OrgWindowPlacement.x, 0, 0x80000000, 0x7FFFFFFF);
	RegisterConfigurationLong("WindowPlacement.y", &pConfig->OrgWindowPlacement.y, 0, 0x80000000, 0x7FFFFFFF);
	RegisterConfigurationLong("WindowPlacement.width", &pConfig->OrgWindowPlacement.width, 0, 0x80000000, 0x7FFFFFFF);
	RegisterConfigurationLong("WindowPlacement.height", &pConfig->OrgWindowPlacement.height, 0, 0x80000000, 0x7FFFFFFF);

#ifdef UNDER_CE
	RegisterConfigurationBool("View.ConfirmOnClose", &pConfig->View.ConfirmOnClose, TRUE);
#else
	RegisterConfigurationBool("View.ConfirmOnClose", &pConfig->View.ConfirmOnClose, FALSE);
#endif
	RegisterConfigurationBool("View.AboutRestart", &pConfig->View.AboutRestart, TRUE);
	RegisterConfigurationBool("View.Metric.Altitude", &pConfig->View.Metric.Altitude, FALSE);
	RegisterConfigurationBool("View.Metric.Distance", &pConfig->View.Metric.Distance, FALSE);
	RegisterConfigurationBool("View.Metric.Pressure", &pConfig->View.Metric.Pressure, FALSE);
	RegisterConfigurationBool("View.Metric.Rainfall", &pConfig->View.Metric.Rainfall, FALSE);
	RegisterConfigurationBool("View.Metric.Temperature", &pConfig->View.Metric.Temperature, FALSE);
	RegisterConfigurationBool("View.Metric.Windspeed", &pConfig->View.Metric.Windspeed, FALSE);

#ifdef UNDER_CE
	RegisterConfigurationULong("View.VisibleLabelsMax", &pConfig->View.VisibleLabelsMax, 10, 5, 500);
#else
	RegisterConfigurationULong("View.VisibleLabelsMax", &pConfig->View.VisibleLabelsMax, 50, 25, 5000);
#endif

	RegisterConfigurationBool("View.Altitude", &pConfig->View.Altitude, FALSE);
#ifdef UNDER_CE
	RegisterConfigurationBool("View.Ambiguity", &pConfig->View.Ambiguity, FALSE);
	RegisterConfigurationBool("View.Cellular", &pConfig->View.Cellular, TRUE);
#else
	RegisterConfigurationBool("View.Ambiguity", &pConfig->View.Ambiguity, TRUE);
#endif
	RegisterConfigurationBool("View.Callsign", &pConfig->View.Callsign, FALSE);
	RegisterConfigurationBool("View.Callsign.NotMe", &pConfig->View.CallsignNotMe, FALSE);
	RegisterConfigurationBool("View.Callsign.NotMine", &pConfig->View.CallsignNotMine, FALSE);
	RegisterConfigurationBool("View.Nicknames", &pConfig->View.Nicknames, TRUE);

	RegisterConfigurationBool("View.Footprint.Enabled", &pConfig->View.Footprint.Enabled, TRUE);
	RegisterConfigurationULong("View.Footprint.MinAltitude", &pConfig->View.Footprint.MinAltitude, 10000, 0, 300000);
	RegisterConfigurationULong("View.Footprint.MaxAltitude", &pConfig->View.Footprint.MaxAltitude, 300000, 0, 999999);

	RegisterConfigurationBool("View.GeoCache.ID", &pConfig->View.GeoCacheID, FALSE);
	RegisterConfigurationBool("View.GeoCache.Type", &pConfig->View.GeoCacheType, FALSE);
	RegisterConfigurationBool("View.GeoCache.Container", &pConfig->View.GeoCacheCont, FALSE);
	RegisterConfigurationBool("View.GeoCache.DiffTerrain", &pConfig->View.GeoCacheDT, FALSE);
#ifdef UNDER_CE
	RegisterConfigurationULong("View.GeoCache.LabelsMax", &pConfig->View.GeoCacheLabelsMax, 10, 5, 500);
#else
	RegisterConfigurationULong("View.GeoCache.LabelsMax", &pConfig->View.GeoCacheLabelsMax, 50, 25, 5000);
#endif
	RegisterConfigurationBool("View.Speed.Beaconed", &pConfig->View.Speed.Beaconed, FALSE);
	RegisterConfigurationBool("View.Speed.Calculated", &pConfig->View.Speed.Calculated, FALSE);
	RegisterConfigurationBool("View.Speed.Averaged", &pConfig->View.Speed.Averaged, FALSE);
	RegisterConfigurationBool("View.Speed.All", &pConfig->View.Speed.All, FALSE);

	RegisterConfigurationBool("View.Range.Enabled", &pConfig->View.Range.Enabled, FALSE);
	RegisterConfigurationBool("View.Range.Half", &pConfig->View.Range.Half, FALSE);
	RegisterConfigurationULong("View.Range.Opacity", &pConfig->View.Range.Opacity, 5, 0, 100);

	RegisterConfigurationBool("View.DF.Enabled", &pConfig->View.DF.Enabled, TRUE);
	RegisterConfigurationULong("View.DF.Opacity", &pConfig->View.DF.Opacity, 5, 0, 100);

	RegisterConfigurationBool("View.LabelOverlap", &pConfig->View.LabelOverlap, FALSE);
	RegisterConfigurationBool("View.NWS.Labels", &pConfig->View.LabelNWS, FALSE);
	RegisterConfigurationBool("View.Weather", &pConfig->View.LabelWeather, FALSE);
	RegisterConfigurationBool("View.ZoomReverse", &pConfig->View.ZoomReverse, TRUE);
	RegisterConfigurationLong("View.ZoomMin", &pConfig->View.ZoomMin, MIN_OSM_ZOOM, MIN_OSM_ZOOM, MAX_OSM_ZOOM);
	RegisterConfigurationLong("View.ZoomMax", &pConfig->View.ZoomMax, MAX_OSM_ZOOM, MIN_OSM_ZOOM, MAX_OSM_ZOOM);
	RegisterConfigurationULong("View.WheelDelta", &pConfig->View.WheelDelta, 4, 1, 60);
	RegisterConfigurationBool("View.ScrollInternals", &pConfig->View.ScrollInternals, TRUE);

	RegisterConfigurationBool("FreqMon.Enabled", &pConfig->FreqMon.Enabled, FALSE);
	RegisterConfigurationULong("FreqMon.FontSize", &pConfig->FreqMon.FontSize, 5, 1, 7);
#ifdef UNDER_CE
	RegisterConfigurationULong("FreqMon.Timer", &pConfig->FreqMon.Timer, 10, 10, 60);
#else
	RegisterConfigurationULong("FreqMon.Timer", &pConfig->FreqMon.Timer, 0, 0, 60);
#endif
	RegisterConfigurationString("FreqMon.Filter", sizeof(pConfig->FreqMon.Filter), (char *) &pConfig->FreqMon.Filter, "");

	RegisterConfigurationString("Screen.DateSeconds", sizeof(pConfig->Screen.DateSeconds), (char *) &pConfig->Screen.DateSeconds, "2367");
	RegisterConfigurationLong("Screen.DateTime", &pConfig->Screen.DateTime, 1, -2, 2);
	RegisterConfigurationBool("Screen.DateTimePerformance", &pConfig->Screen.DateTimePerformance, FALSE);
	RegisterConfigurationULong("Screen.SpeedSize", &pConfig->Screen.SpeedSize, 5, 0, 5);
	RegisterConfigurationLong("Screen.SymbolSizeAdjust", &pConfig->Screen.SymbolSizeAdjust, 0, -10, 10);
	RegisterConfigurationBool("Screen.Altitude", &pConfig->Screen.Show.Altitude, TRUE);
	RegisterConfigurationBool("Screen.Battery", &pConfig->Screen.Show.Battery, TRUE);
	RegisterConfigurationBool("Screen.GridSquare", &pConfig->Screen.Show.GridSquare, TRUE);
	RegisterConfigurationBool("Screen.LatLon", &pConfig->Screen.Show.LatLon, TRUE);
	RegisterConfigurationBool("Screen.Satellites", &pConfig->Screen.Show.Satellites, TRUE);
#ifdef UNDER_CE
	RegisterConfigurationBool("Screen.Reckoning", &pConfig->Screen.Show.Reckoning, FALSE);
#else
	RegisterConfigurationBool("Screen.Reckoning", &pConfig->Screen.Show.Reckoning, TRUE);
#endif
	RegisterConfigurationBool("Screen.FilterCircle", &pConfig->Screen.FilterCircle, FALSE);

	RegisterConfigurationBool("Screen.Paths.Network", &pConfig->Screen.Paths.Network, DefaultPathConfig.Network);
	RegisterConfigurationBool("Screen.Paths.Station", &pConfig->Screen.Paths.Station, DefaultPathConfig.Station);
	RegisterConfigurationBool("Screen.Paths.MyStation", &pConfig->Screen.Paths.MyStation, DefaultPathConfig.MyStation);
	RegisterConfigurationBool("Screen.Paths.RFOnly", &pConfig->Screen.Paths.LclRF, DefaultPathConfig.LclRF);
	RegisterConfigurationLong("Screen.Paths.MaxPathAge", (long *)&pConfig->Screen.Paths.MaxAge, DefaultPathConfig.MaxAge, -1, 3600);
	RegisterConfigurationULong("Screen.Paths.ReasonabilityLimit", &pConfig->Screen.Paths.ReasonabilityLimit, DefaultPathConfig.ReasonabilityLimit, 0, 16384);
	RegisterConfigurationULong("Screen.Paths.Opacity", &pConfig->Screen.Paths.Opacity, DefaultPathConfig.Opacity, 1, 100);
	RegisterConfigurationBool("Screen.Paths.AllLinks", &pConfig->Screen.Paths.ShowAllLinks, DefaultPathConfig.ShowAllLinks);
#define REGISTER_PATH_LINE(f) \
	RegisterConfigurationBool("Screen.Paths."#f".Enabled", &pConfig->Screen.Paths.f.Enabled, DefaultPathConfig.f.Enabled); \
	RegisterConfigurationULong("Screen.Paths."#f".Width", &pConfig->Screen.Paths.f.Width, DefaultPathConfig.f.Width, LINE_WIDTH_MIN, LINE_WIDTH_MAX); \
	RegisterConfigurationString("Screen.Paths."#f".Color", sizeof(pConfig->Screen.Paths.f.Color), (char *) &pConfig->Screen.Paths.f.Color, DefaultPathConfig.f.Color)
	REGISTER_PATH_LINE(Packet);
	REGISTER_PATH_LINE(Direct);
	REGISTER_PATH_LINE(First);
	REGISTER_PATH_LINE(Middle);
	REGISTER_PATH_LINE(Final);
#undef REGISTER_PATH_LINE
	RegisterConfigurationBool("Screen.Paths.Flash.Selected", &pConfig->Screen.Paths.Flash.Selected, DefaultPathConfig.Flash.Selected);
	RegisterConfigurationULong("Screen.Paths.Flash.Time", &pConfig->Screen.Paths.Flash.Time, DefaultPathConfig.Flash.Time, 1,120);
	RegisterConfigurationBool("Screen.Paths.Short.Selected", &pConfig->Screen.Paths.Short.Selected, DefaultPathConfig.Short.Selected);
	RegisterConfigurationULong("Screen.Paths.Short.Time", &pConfig->Screen.Paths.Short.Time, DefaultPathConfig.Short.Time, 60,600);
	RegisterConfigurationBool("Screen.Paths.Medium.Selected", &pConfig->Screen.Paths.Medium.Selected, DefaultPathConfig.Medium.Selected);
	RegisterConfigurationULong("Screen.Paths.Medium.Time", &pConfig->Screen.Paths.Medium.Time, DefaultPathConfig.Medium.Time, 1*60,30*60);
	RegisterConfigurationBool("Screen.Paths.Long.Selected", &pConfig->Screen.Paths.Long.Selected, DefaultPathConfig.Long.Selected);
	RegisterConfigurationULong("Screen.Paths.Long.Time", &pConfig->Screen.Paths.Long.Time, DefaultPathConfig.Long.Time, 10*60,120*60);

	RegisterConfigurationBool("Screen.Tracks", &pConfig->Screen.Show.Tracks, TRUE);
	RegisterConfigurationULong("Screen.Tracks.Follow", &pConfig->Screen.Track.Follow.Count, 32, 0, 256);
	RegisterConfigurationULong("Screen.Tracks.Follow.Width", &pConfig->Screen.Track.Follow.Width, 1, LINE_WIDTH_MIN, LINE_WIDTH_MAX);
	RegisterConfigurationString("Screen.Tracks.Follow.Color", sizeof(pConfig->Screen.Track.Follow.Color), (char *) &pConfig->Screen.Track.Follow.Color, "black");

	RegisterConfigurationULong("Screen.Tracks.Others", &pConfig->Screen.Track.Other.Count, 32, 0, 256);
	RegisterConfigurationULong("Screen.Tracks.Others.Width", &pConfig->Screen.Track.Other.Width, 1, LINE_WIDTH_MIN, LINE_WIDTH_MAX);
	RegisterConfigurationString("Screen.Tracks.Others.Color", sizeof(pConfig->Screen.Track.Other.Color), (char *) &pConfig->Screen.Track.Other.Color, "gray");

	RegisterConfigurationBool("Screen.WindBarbs.Enabled", &pConfig->Screen.WindBarbs.Enabled, TRUE);
	RegisterConfigurationBool("Screen.WindBarbs.RotateStorm", &pConfig->Screen.WindBarbs.RotateStorm, TRUE);
	RegisterConfigurationULong("Screen.WindBarbs.Width", &pConfig->Screen.WindBarbs.Width, 1, LINE_WIDTH_MIN, LINE_WIDTH_MAX);
	RegisterConfigurationString("Screen.WindBarbs.Color", sizeof(pConfig->Screen.WindBarbs.Color), (char *) &pConfig->Screen.WindBarbs.Color, "black");

#ifdef MONITOR_PHONE
	RegisterConfigurationULong("Screen.Tracks.Cellular", &pConfig->Screen.Track.Cellular.Count, 32, 0, 256);
	RegisterConfigurationULong("Screen.Tracks.Others.Width", &pConfig->Screen.Track.Cellular.Width, 1, 0, 16);
	RegisterConfigurationString("Screen.Tracks.Others.Color", sizeof(pConfig->Screen.Track.Cellular.Color), (char *) &pConfig->Screen.Track.Cellular.Color, "mediumblue");
#endif

	RegisterConfigurationBool("Screen.Circle", &pConfig->Screen.Show.Circle, TRUE);
	RegisterConfigurationBool("Screen.Dim", &pConfig->Screen.Dim, FALSE);
	RegisterConfigurationBool("Screen.RedDot", &pConfig->Screen.Show.RedDot, TRUE);
	RegisterConfigurationLong("Screen.CrossHairs", &pConfig->Screen.Show.CrossHairs, 1, -1, 1);
	RegisterConfigurationLong("Screen.CrossHairTime", &pConfig->Screen.Show.CrossHairTime, 1000, 100, 15000);

	RegisterConfigurationBool("Scroller.ShowIGateDigi", &pConfig->Scroller.ShowIGateOrDigi, FALSE);
	RegisterConfigurationBool("Scroller.FreezeOnClick", &pConfig->Scroller.FreezeOnClick, FALSE);
	RegisterConfigurationBool("Scroller.ShowAll", &pConfig->Scroller.ShowAll, TRUE);
	RegisterConfigurationBool("Scroller.NoInternals", &pConfig->Scroller.NoInternals, FALSE);
	RegisterConfigurationBool("Scroller.NotME", &pConfig->Scroller.NotME, FALSE);
	RegisterConfigurationBool("Scroller.NotMine", &pConfig->Scroller.NotMine, FALSE);
	RegisterConfigurationBool("Scroller.RFOnly", &pConfig->Scroller.RFOnly, FALSE);
	RegisterConfigurationBool("Scroller.HideNoParse", &pConfig->Scroller.HideNoParse, FALSE);
	RegisterConfigurationString("Scroller.Filter", sizeof(pConfig->Scroller.Filter), (char *) &pConfig->Scroller.Filter, "");
	RegisterConfigurationTimedStringList("PathAlias", &pConfig->PathAliases, TRUE, "Disabled");
#ifndef UNDER_CE
	RegisterConfigurationBool("AccumulateAliases", &pConfig->AccumulateAliases, FALSE);
	RegisterConfigurationTimedStringList("NewAlias", &pConfig->NewAliases, TRUE);
	RegisterConfigurationTimedStringList("ZeroAlias", &pConfig->ZeroAliases, TRUE);
#endif

/*	The definitions of Mobile and Weather symbols came from http://www.aprs.org/aprs11.html on 2010/03/03 */
	RegisterConfigurationString("View.Custom.Primary", sizeof(pConfig->View.Custom.Primary), (char *) &pConfig->View.Custom.Primary, "");
	RegisterConfigurationString("View.Custom.Alternate", sizeof(pConfig->View.Custom.Alternate), (char *) &pConfig->View.Custom.Alternate, "");
	RegisterConfigurationString("View.Flight.Primary", sizeof(pConfig->View.Flight.Primary), (char *) &pConfig->View.Flight.Primary, "'O^");
	RegisterConfigurationString("View.Flight.Alternate", sizeof(pConfig->View.Flight.Alternate), (char *) &pConfig->View.Flight.Alternate, "V^");
	RegisterConfigurationString("View.Marine.Primary", sizeof(pConfig->View.Marine.Primary), (char *) &pConfig->View.Marine.Primary, "Ys");
	RegisterConfigurationString("View.Marine.Alternate", sizeof(pConfig->View.Marine.Alternate), (char *) &pConfig->View.Marine.Alternate, "CLNs");
	RegisterConfigurationString("View.Mobile.Primary", sizeof(pConfig->View.Mobile.Primary), (char *) &pConfig->View.Mobile.Primary, "!'<=>()*0CFOPRSUXY[\\^abefgjkpsuv");
	RegisterConfigurationString("View.Mobile.Alternate", sizeof(pConfig->View.Mobile.Alternate), (char *) &pConfig->View.Mobile.Alternate, ">KOS^ksuv");
	RegisterConfigurationString("View.Weather.Primary", sizeof(pConfig->View.Weather.Primary), (char *) &pConfig->View.Weather.Primary, "_@W");
	RegisterConfigurationString("View.Weather.Alternate", sizeof(pConfig->View.Weather.Alternate), (char *) &pConfig->View.Weather.Alternate, "([*:<@BDEFGHIJTUW_efgptwy{");

	RegisterConfigurationDouble("Center.Latitude", 6, &pConfig->Center.Latitude, 0, -90.0, 90.0);
	RegisterConfigurationDouble("Center.Longitude", 6, &pConfig->Center.Longitude, 0, -180.0, 180.0);

	RegisterConfigurationDouble("Preferred.Latitude", 6, &pConfig->Preferred.Latitude, 0, -90.0, 90.0);
	RegisterConfigurationDouble("Preferred.Longitude", 6, &pConfig->Preferred.Longitude, 0, -180.0, 180.0);
	RegisterConfigurationULong("Preferred.Zoom", &pConfig->Preferred.Zoom, 0, MIN_OSM_ZOOM, MAX_OSM_ZOOM);
	RegisterConfigurationDouble("Preferred.Scale", 4, &pConfig->Preferred.Scale, DEFAULT_SCALE, MIN_SCALE, MAX_SCALE);
	RegisterConfigurationULong("Preferred.Orientation", &pConfig->Preferred.Orientation, 0, 0, 2);

	RegisterConfigurationString("Tracking.Call", sizeof(pConfig->Tracking.Call), (char *) &pConfig->Tracking.Call, "");
	RegisterConfigurationBool("Tracking.Center", &pConfig->Tracking.Center, FALSE);
	RegisterConfigurationBool("Tracking.Locked", &pConfig->Tracking.Locked, FALSE);

#ifdef TRAKVIEW
	RegisterConfigurationPort("APRSISPort", sizeof(pConfig->APRSISPort), (char *) &pConfig->APRSISPort, "");
#else
	RegisterConfigurationPort("APRSISPort", sizeof(pConfig->APRSIS.Port), (char *) &pConfig->APRSIS.Port, "@rotate.aprs2.net:14580");
#endif
	RegisterConfigurationBool("APRSIS.XmitEnabled", &pConfig->APRSIS.XmitEnabled, TRUE);
	RegisterConfigurationBool("APRSIS.RFtoISEnabled", &pConfig->APRSIS.RFtoISEnabled, TRUE);
	RegisterConfigurationBool("APRSIS.IStoRFEnabled", &pConfig->APRSIS.IStoRFEnabled, TRUE);
	RegisterConfigurationBool("APRSIS.BeaconingEnabled", &pConfig->APRSIS.BeaconingEnabled, TRUE);
	RegisterConfigurationBool("APRSIS.MessagesEnabled", &pConfig->APRSIS.MessagesEnabled, TRUE);
	RegisterConfigurationBool("APRSIS.BulletinObjectEnabled", &pConfig->APRSIS.BulletinObjectEnabled, TRUE);
	RegisterConfigurationBool("APRSIS.TelemetryEnabled", &pConfig->APRSIS.TelemetryEnabled, TRUE);

	RegisterConfigurationPort("GPSPort", sizeof(pConfig->GPSPort), (char *) &pConfig->GPSPort, "");

	RegisterConfigurationStruct("RFID", sizeof(*pConfig->RFIDs.RFID), (STRUCT_LIST_S*) &pConfig->RFIDs, TRUE);
	RegisterConfigurationString("RFID.ServerName", sizeof(pConfig->RFIDs.RFID->ServerName), (char *) &pConfig->RFIDs.RFID->ServerName, "RFID");
	RegisterConfigurationBool("RFID.AssocEnabled", &pConfig->RFIDs.RFID->AssocEnabled, FALSE);
	RegisterConfigurationBool("RFID.ServerEnabled", &pConfig->RFIDs.RFID->ServerEnabled, FALSE);
	RegisterConfigurationStringList("RFID.Readers", &pConfig->RFIDs.RFID->Readers);


	
	RegisterConfigurationLong("OSM.Zoom", &pConfig->OSMZoom, 0, MIN_OSM_ZOOM, MAX_OSM_ZOOM);
	RegisterConfigurationULong("OSM.Percent", &pConfig->OSMPercent, 50, 0, 100);
	RegisterConfigurationULong("OSM.MinMBFree", &pConfig->OSMMinMBFree, 8, 0, 1024);
	RegisterConfigurationBool("OSM.PurgeDisabled", &pConfig->OSMPurgeDisabled, FALSE);

	RegisterConfigurationString("OSM.Name", sizeof(pConfig->OSM.Name), (char*)&pConfig->OSM.Name, "");
	RegisterConfigurationString("OSM.Server", sizeof(pConfig->OSM.Server), (char *) &pConfig->OSM.Server, "tile.openstreetmap.org");
	RegisterConfigurationULong("OSM.Port", &pConfig->OSM.Port, 80, 1, 65535);
	RegisterConfigurationString("OSM.URLPrefix", sizeof(pConfig->OSM.URLPrefix), (char *) &pConfig->OSM.URLPrefix, "/");
	RegisterConfigurationBool("OSM.SupportsStatus", &pConfig->OSM.SupportsStatus, TRUE);
	RegisterConfigurationString("OSM.Path", sizeof(pConfig->OSM.Path), (char *) &pConfig->OSM.Path, "OSMTiles/", FALSE);
	RegisterConfigurationBool("OSM.PurgeEnabled", &pConfig->OSM.PurgeEnabled, TRUE);
	RegisterConfigurationULong("OSM.RetainDays", &pConfig->OSM.RetainDays, 7, 0, 365);
	RegisterConfigurationLong("OSM.RetainZoom", &pConfig->OSM.RetainZoom, 5, 0, 18);
	RegisterConfigurationLong("OSM.MinServerZoom", &pConfig->OSM.MinServerZoom, 0, 0, 20);
	RegisterConfigurationLong("OSM.MaxServerZoom", &pConfig->OSM.MaxServerZoom, 18, 0, 20);
	RegisterConfigurationLong("OSM.RevisionHours", &pConfig->OSM.RevisionHours, 0, -1, 168);

/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("TileServer", sizeof(*pConfig->TileServers.Server), (STRUCT_LIST_S*) &pConfig->TileServers);
	RegisterConfigurationString("TileServer.Name", sizeof(pConfig->TileServers.Server->Name), (char*)&pConfig->TileServers.Server->Name, "");
	RegisterConfigurationString("TileServer.Server", sizeof(pConfig->TileServers.Server->Server), (char *) &pConfig->TileServers.Server->Server, "tile.openstreetmap.org");
	RegisterConfigurationULong("TileServer.Port", &pConfig->TileServers.Server->Port, 80, 1, 65535);
	RegisterConfigurationString("TileServer.URLPrefix", sizeof(pConfig->TileServers.Server->URLPrefix), (char *) &pConfig->TileServers.Server->URLPrefix, "/");
	RegisterConfigurationBool("TileServer.SupportsStatus", &pConfig->TileServers.Server->SupportsStatus, TRUE);
	RegisterConfigurationString("TileServer.Path", sizeof(pConfig->TileServers.Server->Path), (char *) &pConfig->TileServers.Server->Path, "OSMTiles/", FALSE);
	RegisterConfigurationBool("TileServer.PurgeEnabled", &pConfig->TileServers.Server->PurgeEnabled, TRUE);
	RegisterConfigurationULong("TileServer.RetainDays", &pConfig->TileServers.Server->RetainDays, 7, 0, 365);
	RegisterConfigurationLong("TileServer.RetainZoom", &pConfig->TileServers.Server->RetainZoom, 5, 0, 18);
	RegisterConfigurationLong("TileServer.MinServerZoom", &pConfig->TileServers.Server->MinServerZoom, 0, 0, 20);
	RegisterConfigurationLong("TileServer.MaxServerZoom", &pConfig->TileServers.Server->MaxServerZoom, 18, 0, 20);
	RegisterConfigurationLong("TileServer.RevisionHours", &pConfig->TileServers.Server->RevisionHours, 0, -1, 168);

/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("RFPort", sizeof(*pConfig->RFPorts.Port), (STRUCT_LIST_S*) &pConfig->RFPorts);
	RegisterConfigurationString("RFPort.Name", sizeof(pConfig->RFPorts.Port->Name), (char*)&pConfig->RFPorts.Port->Name, "");
	RegisterConfigurationString("RFPort.Protocol", sizeof(pConfig->RFPorts.Port->Protocol), (char*)&pConfig->RFPorts.Port->Protocol, "");
	RegisterConfigurationString("RFPort.Device", sizeof(pConfig->RFPorts.Port->Device), (char*)&pConfig->RFPorts.Port->Device, "", FALSE);
	RegisterConfigurationLong("RFPort.RfBaud", &pConfig->RFPorts.Port->RfBaud, 1200, 0, 9600);
	RegisterConfigurationStringList("RFPort.OpenCmd", &pConfig->RFPorts.Port->OpenCmds, FALSE);
	RegisterConfigurationStringList("RFPort.CloseCmd", &pConfig->RFPorts.Port->CloseCmds, FALSE);
	RegisterConfigurationULong("RFPort.QuietTime", &pConfig->RFPorts.Port->QuietTime, 0, 0, 5*60);
	RegisterConfigurationBool("RFPort.Enabled", &pConfig->RFPorts.Port->IsEnabled, TRUE);
	RegisterConfigurationBool("RFPort.XmitEnabled", &pConfig->RFPorts.Port->XmitEnabled, TRUE);
	RegisterConfigurationBool("RFPort.ProvidesNMEA", &pConfig->RFPorts.Port->ProvidesNMEA, FALSE);
	RegisterConfigurationBool("RFPort.RFtoISEnabled", &pConfig->RFPorts.Port->RFtoISEnabled, TRUE);
	RegisterConfigurationBool("RFPort.IStoRFEnabled", &pConfig->RFPorts.Port->IStoRFEnabled, TRUE);
	RegisterConfigurationBool("RFPort.MyCallNot3rd", &pConfig->RFPorts.Port->MyCallNot3rd, FALSE);
	RegisterConfigurationBool("RFPort.NoGateME", &pConfig->RFPorts.Port->NoGateME, FALSE);
	RegisterConfigurationBool("RFPort.BeaconingEnabled", &pConfig->RFPorts.Port->BeaconingEnabled, TRUE);
	RegisterConfigurationString("RFPort.BeaconPath", sizeof(pConfig->RFPorts.Port->BeaconPath), (char *) &pConfig->RFPorts.Port->BeaconPath, "");
	RegisterConfigurationBool("RFPort.BulletinObjectEnabled", &pConfig->RFPorts.Port->BulletinObjectEnabled, TRUE);
	RegisterConfigurationBool("RFPort.DXEnabled", &pConfig->RFPorts.Port->DXEnabled, FALSE);
	RegisterConfigurationString("RFPort.DXPath", sizeof(pConfig->RFPorts.Port->DXPath), (char *) &pConfig->RFPorts.Port->DXPath, "RFONLY");
	RegisterConfigurationBool("RFPort.MessagesEnabled", &pConfig->RFPorts.Port->MessagesEnabled, TRUE);
	RegisterConfigurationString("RFPort.MessagePath", sizeof(pConfig->RFPorts.Port->MessagePath), (char *) &pConfig->RFPorts.Port->MessagePath, "");
	RegisterConfigurationBool("RFPort.TelemetryEnabled", &pConfig->RFPorts.Port->TelemetryEnabled, TRUE);
	RegisterConfigurationString("RFPort.TelemetryPath", sizeof(pConfig->RFPorts.Port->TelemetryPath), (char *) &pConfig->RFPorts.Port->TelemetryPath, "");

	RegisterConfigurationStringList("RFPort.DigiXform", &pConfig->RFPorts.Port->DigiXforms);

	RegisterConfigurationStringList("DigiXform", &pConfig->DigiXforms);


/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("Object", sizeof(*pConfig->Objects.Obj), (STRUCT_LIST_S*) &pConfig->Objects);
	RegisterConfigurationString("Object.Name", sizeof(pConfig->Objects.Obj->Name), (char*)&pConfig->Objects.Obj->Name, "");
	RegisterConfigurationString("Object.Group", sizeof(pConfig->Objects.Obj->Group), (char*)&pConfig->Objects.Obj->Group, "");
	RegisterConfigurationString("Object.Comment", sizeof(pConfig->Objects.Obj->Comment), (char *) &pConfig->Objects.Obj->Comment, "");
	RegisterConfigurationChar("Object.Table", &pConfig->Objects.Obj->Symbol.Table, '\\', '/', '\\');
	RegisterConfigurationChar("Object.Symbol", &pConfig->Objects.Obj->Symbol.Symbol, 'C', '!', '~');
	RegisterConfigurationDouble("Object.Latitude", 6, &pConfig->Objects.Obj->Latitude, 0, -90.0, 90.0);
	RegisterConfigurationDouble("Object.Longitude", 6, &pConfig->Objects.Obj->Longitude, 0, -180.0, 180.0);
	RegisterConfigurationBool("Object.Compressed", &pConfig->Objects.Obj->Compressed, FALSE);
	RegisterConfigurationLong("Object.Precision", &pConfig->Objects.Obj->Precision, 0, -4, 2);
	RegisterConfigurationBool("Object.Item", &pConfig->Objects.Obj->Item, FALSE);
	RegisterConfigurationBool("Object.HHMMSS", &pConfig->Objects.Obj->HHMMSS, TRUE);
	RegisterConfigurationBool("Object.Permanent", &pConfig->Objects.Obj->Permanent, FALSE);
	RegisterConfigurationULong("Object.Interval", &pConfig->Objects.Obj->Interval, 10, 0, 1440);
	RegisterConfigurationBool("Object.Kill", &pConfig->Objects.Obj->Kill, FALSE);
	RegisterConfigurationULong("Object.KillXmitCount", &pConfig->Objects.Obj->KillXmitCount, 0, 0, 60);
	RegisterConfigurationBool("Object.Enabled", &pConfig->Objects.Obj->Enabled, FALSE);
	RegisterConfigurationBool("Object.ISEnabled", &pConfig->Objects.Obj->ISEnabled, FALSE);
	RegisterConfigurationBool("Object.RFEnabled", &pConfig->Objects.Obj->RFEnabled, FALSE);
	RegisterConfigurationString("Object.RFPath", sizeof(pConfig->Objects.Obj->RFPath), (char*)&pConfig->Objects.Obj->RFPath, "");
	RegisterConfigurationSystemTime("Object.LastTransmit", &pConfig->Objects.Obj->LastTransmit, NULL);

#ifndef UNDER_CE
	RegisterConfigurationBool("Object.JT65", &pConfig->Objects.Obj->JT65, FALSE);
#endif
	RegisterConfigurationBool("Object.Weather", &pConfig->Objects.Obj->Weather, FALSE);
	RegisterConfigurationString("Object.WeatherPath", sizeof(pConfig->Objects.Obj->WeatherPath), (char*)&pConfig->Objects.Obj->WeatherPath, "");
	RegisterConfigurationSystemTime("Object.LastWeather", &pConfig->Objects.Obj->LastWeather, NULL);

	RegisterConfigurationStringList("TACTICALNever", &pConfig->TacticalNevers);
	RegisterConfigurationStringList("TACTICALSource", &pConfig->TacticalSources);
//	RegisterConfigurationTimedStringList("TACTICAL", &pConfig->Tacticals, TRUE, "Enabled");

	RegisterConfigurationTimedStringList("CompanionCall", &pConfig->CompanionCalls, TRUE, "Enabled", TRUE);
	RegisterConfigurationULong("CompanionInterval", &pConfig->CompanionInterval, 0, 0, 30);
	RegisterConfigurationBool("CompanionsEnabled", &pConfig->CompanionsEnabled, FALSE);
/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("Companion", sizeof(*pConfig->Companions.Companion), (STRUCT_LIST_S*) &pConfig->Companions);
	RegisterConfigurationString("Companion.Name", sizeof(pConfig->Companions.Companion->Name), (char*)&pConfig->Companions.Companion->Name, "");
	RegisterConfigurationString("Companion.Comment", sizeof(pConfig->Companions.Companion->Comment), (char *) &pConfig->Companions.Companion->Comment, "");
	RegisterConfigurationChar("Companion.Table", &pConfig->Companions.Companion->Symbol.Table, '\\', '/', '\\');
	RegisterConfigurationChar("Companion.Symbol", &pConfig->Companions.Companion->Symbol.Symbol, 'C', '!', '~');
	RegisterConfigurationBool("Companion.Object", &pConfig->Companions.Companion->Object, FALSE);
	RegisterConfigurationBool("Companion.Enabled", &pConfig->Companions.Companion->Enabled, FALSE);
	RegisterConfigurationBool("Companion.Beacon", &pConfig->Companions.Companion->Posit, FALSE);
	RegisterConfigurationBool("Companion.Messaging", &pConfig->Companions.Companion->Messaging, FALSE);
	RegisterConfigurationBool("Companion.ISEnabled", &pConfig->Companions.Companion->ISEnabled, FALSE);
	RegisterConfigurationBool("Companion.RFEnabled", &pConfig->Companions.Companion->RFEnabled, FALSE);
	RegisterConfigurationString("Companion.RFPath", sizeof(pConfig->Companions.Companion->RFPath), (char*)&pConfig->Companions.Companion->RFPath, "");
	RegisterConfigurationULong("Companion.Genius.MinTime", &pConfig->Companions.Companion->Genius.MinTime, 30, 10, 5*60);
	RegisterConfigurationULong("Companion.Genius.MaxTime", &pConfig->Companions.Companion->Genius.MaxTime, 30, 1, 60);
	RegisterConfigurationBool("Companion.Genius.TimeOnly", &pConfig->Companions.Companion->Genius.TimeOnly, FALSE);
	RegisterConfigurationBool("Companion.Genius.StartStop", &pConfig->Companions.Companion->Genius.StartStop, FALSE);
//	RegisterConfigurationULong("Companion.Genius.BearingChange", &pConfig->Companions.Companion->Genius.BearingChange, 30, 5, 180);
	RegisterConfigurationDouble("Companion.Genius.ForecastError", 2, &pConfig->Companions.Companion->Genius.ForecastError, 0.1, 0.1, 2.0);
	RegisterConfigurationDouble("Companion.Genius.MaxDistance", 2, &pConfig->Companions.Companion->Genius.MaxDistance, 1.0, 0.1, 20.0);
//	RegisterConfigurationSystemTime("Companion.LastTransmit", &pConfig->Companions.Companion->LastTransmit, NULL);

/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("Nickname", sizeof(*pConfig->Nicknames.Nick), (STRUCT_LIST_S*) &pConfig->Nicknames);
	RegisterConfigurationString("Nickname.Station", sizeof(pConfig->Nicknames.Nick->Station), (char*)&pConfig->Nicknames.Nick->Station, "");
	RegisterConfigurationBool("Nickname.Enabled", &pConfig->Nicknames.Nick->Enabled, FALSE);
	RegisterConfigurationBool("Nickname.AutoMultiTrack", &pConfig->Nicknames.Nick->MultiTrackNew, FALSE);
	RegisterConfigurationBool("Nickname.ActiveMultiTrack", &pConfig->Nicknames.Nick->MultiTrackActive, FALSE);
	RegisterConfigurationBool("Nickname.AlwaysMultiTrack", &pConfig->Nicknames.Nick->MultiTrackAlways, FALSE);
	RegisterConfigurationString("Nickname.Label", sizeof(pConfig->Nicknames.Nick->Label), (char*)&pConfig->Nicknames.Nick->Label, "");
	RegisterConfigurationBool("Nickname.OverrideLabel", &pConfig->Nicknames.Nick->OverrideLabel, TRUE);
	RegisterConfigurationString("Nickname.Comment", sizeof(pConfig->Nicknames.Nick->Comment), (char *) &pConfig->Nicknames.Nick->Comment, "");
	RegisterConfigurationBool("Nickname.OverrideComment", &pConfig->Nicknames.Nick->OverrideComment, FALSE);
	RegisterConfigurationChar("Nickname.Table", &pConfig->Nicknames.Nick->Symbol.Table, '\\', '/', '\\');
	RegisterConfigurationChar("Nickname.Symbol", &pConfig->Nicknames.Nick->Symbol.Symbol, 'C', '!', '~');
	RegisterConfigurationBool("Nickname.OverrideSymbol", &pConfig->Nicknames.Nick->OverrideSymbol, FALSE);
	RegisterConfigurationString("Nickname.Color", sizeof(pConfig->Nicknames.Nick->Color), (char *) &pConfig->Nicknames.Nick->Color, "");
	RegisterConfigurationBool("Nickname.OverrideColor", &pConfig->Nicknames.Nick->OverrideColor, FALSE);
	RegisterConfigurationString("Nickname.DefinedBy", sizeof(pConfig->Nicknames.Nick->DefinedBy), (char *) &pConfig->Nicknames.Nick->DefinedBy, "");
	RegisterConfigurationSystemTime("Nickname.LastUsed", &pConfig->Nicknames.Nick->LastSeen, NULL);

/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("Overlays", sizeof(*pConfig->Overlays.Overlay), (STRUCT_LIST_S*) &pConfig->Overlays);
	RegisterConfigurationString("Overlays.FileName", sizeof(pConfig->Overlays.Overlay->FileName), (char*)&pConfig->Overlays.Overlay->FileName, "");
	RegisterConfigurationChar("Overlays.Type", &pConfig->Overlays.Overlay->Type, '?', 'A', 'Z');
	RegisterConfigurationBool("Overlays.Enabled", &pConfig->Overlays.Overlay->Enabled, TRUE);
	RegisterConfigurationBool("Overlays.Label.Enabled", &pConfig->Overlays.Overlay->Label.Enabled, TRUE);
	RegisterConfigurationBool("Overlays.Label.ID", &pConfig->Overlays.Overlay->Label.ID, TRUE);
	RegisterConfigurationBool("Overlays.Label.Comment", &pConfig->Overlays.Overlay->Label.Comment, TRUE);
	RegisterConfigurationBool("Overlays.Label.Status", &pConfig->Overlays.Overlay->Label.Status, TRUE);
	RegisterConfigurationBool("Overlays.Label.Altitude", &pConfig->Overlays.Overlay->Label.Altitude, TRUE);
	RegisterConfigurationBool("Overlays.Label.Timestamp", &pConfig->Overlays.Overlay->Label.Timestamp, TRUE);
	RegisterConfigurationBool("Overlays.Label.SuppressDate", &pConfig->Overlays.Overlay->Label.SuppressDate, TRUE);
#define REGISTER_OVERLAY_SYMBOL(f) \
	RegisterConfigurationBool("Overlays."#f".Enabled", &pConfig->Overlays.Overlay->f.Enabled, TRUE); \
	RegisterConfigurationBool("Overlays."#f".Label", &pConfig->Overlays.Overlay->f.Label, TRUE); \
	RegisterConfigurationBool("Overlays."#f".Symbol.Show", &pConfig->Overlays.Overlay->f.Symbol.Show, TRUE); \
	RegisterConfigurationBool("Overlays."#f".Symbol.Force", &pConfig->Overlays.Overlay->f.Symbol.Force, FALSE); \
	RegisterConfigurationChar("Overlays."#f".Symbol.Symbol.Table", &pConfig->Overlays.Overlay->f.Symbol.Symbol.Table, '\\', '/', '\\'); \
	RegisterConfigurationChar("Overlays."#f".Symbol.Symbol.Symbol", &pConfig->Overlays.Overlay->f.Symbol.Symbol.Symbol, '.', '!', '~'); \
	RegisterConfigurationULong("Overlays."#f".Line.Width", &pConfig->Overlays.Overlay->f.Line.Width, 2, LINE_WIDTH_MIN, LINE_WIDTH_MAX); \
	RegisterConfigurationULong("Overlays."#f".Line.Opacity", &pConfig->Overlays.Overlay->f.Line.Opacity, 50, 10, 100); \
	RegisterConfigurationString("Overlays."#f".Line.Color", sizeof(pConfig->Overlays.Overlay->f.Line.Color), (char *) &pConfig->Overlays.Overlay->f.Line.Color, "Black")
	REGISTER_OVERLAY_SYMBOL(Route);
	REGISTER_OVERLAY_SYMBOL(Track);
	REGISTER_OVERLAY_SYMBOL(Waypoint);
#undef REGISTER_OVERLAY_SYMBOL

/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("MicE", sizeof(*pConfig->MicEs.MicE), (STRUCT_LIST_S*) &pConfig->MicEs);
	RegisterConfigurationString("MicE.Name", sizeof(pConfig->MicEs.MicE->Name), (char *) &pConfig->MicEs.MicE->Name, "");
	RegisterConfigurationString("MicE.XmitTag", sizeof(pConfig->MicEs.MicE->Tag), (char *) &pConfig->MicEs.MicE->Tag, "");
	RegisterConfigurationBool("MicE.Enabled", &pConfig->MicEs.MicE->Enabled, FALSE);
	RegisterConfigurationBool("MicE.InternalMessage", &pConfig->MicEs.MicE->InternalMessage, FALSE);
	RegisterConfigurationBool("MicE.MultiTrackNew", &pConfig->MicEs.MicE->MultiTrackNew, FALSE);
	RegisterConfigurationBool("MicE.MultiTrackActive", &pConfig->MicEs.MicE->MultiTrackActive, FALSE);
	RegisterConfigurationBool("MicE.FlashOnCenter", &pConfig->MicEs.MicE->FlashOnCenter, FALSE);
	RegisterConfigurationBool("MicE.Highlight", &pConfig->MicEs.MicE->Highlight, FALSE);
	RegisterConfigurationString("MicE.Color", sizeof(pConfig->MicEs.MicE->Color), (char *) &pConfig->MicEs.MicE->Color, "");
	RegisterConfigurationTimedStringList("MicE.Ignore", &pConfig->MicEs.MicE->Ignores, TRUE, "Duration");

/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("NWSServer", sizeof(*pConfig->NWSServers.Srv), (STRUCT_LIST_S*) &pConfig->NWSServers);

	RegisterConfigurationString("NWSServer.EntryCall", sizeof(pConfig->NWSServers.Srv->EntryCall), (char*)&pConfig->NWSServers.Srv->EntryCall, "");
	RegisterConfigurationString("NWSServer.Description", sizeof(pConfig->NWSServers.Srv->Desc), (char *) &pConfig->NWSServers.Srv->Desc, "");
	RegisterConfigurationBool("NWSServer.Disabled", &pConfig->NWSServers.Srv->Disabled, FALSE);
	RegisterConfigurationString("NWSServer.Finger.Server", sizeof(pConfig->NWSServers.Srv->Finger.Server), (char *) &pConfig->NWSServers.Srv->Finger.Server, "");
	RegisterConfigurationULong("NWSServer.Finger.Port", &pConfig->NWSServers.Srv->Finger.Port, 79, 1, 65535);

/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("NWSProd", sizeof(*pConfig->NWSProducts.Prod), (STRUCT_LIST_S*) &pConfig->NWSProducts);
	RegisterConfigurationString("NWSProd.Name", sizeof(pConfig->NWSProducts.Prod->PID), (char*)&pConfig->NWSProducts.Prod->PID, "");
	RegisterConfigurationString("NWSProd.Description", sizeof(pConfig->NWSProducts.Prod->Desc), (char *) &pConfig->NWSProducts.Prod->Desc, "");
	RegisterConfigurationChar("NWSProd.Symbol.Table", &pConfig->NWSProducts.Prod->Symbol.Table, '\\', '/', '\\');
	RegisterConfigurationChar("NWSProd.Symbol.Symbol", &pConfig->NWSProducts.Prod->Symbol.Symbol, 'C', '!', '~');
	RegisterConfigurationBool("NWSProd.ActionEnabled", &pConfig->NWSProducts.Prod->ActionEnabled, TRUE);
	RegisterConfigurationChar("NWSProd.LineType", &pConfig->NWSProducts.Prod->LineType, 'l', 'a', 'l');
	
/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("LineStyle", sizeof(*pConfig->LineStyles.Style), (STRUCT_LIST_S*) &pConfig->LineStyles);
	RegisterConfigurationString("LineStyle.LineType", sizeof(pConfig->LineStyles.Style->sLineType), (char *) &pConfig->LineStyles.Style->sLineType, "");
	RegisterConfigurationString("LineStyle.Description", sizeof(pConfig->LineStyles.Style->Desc), (char *) &pConfig->LineStyles.Style->Desc, "");
	RegisterConfigurationULong("LineStyle.Color", &pConfig->LineStyles.Style->Color, RGB(128,128,128), 0, ULONG_MAX);
	RegisterConfigurationLong("LineStyle.Style", &pConfig->LineStyles.Style->Style, 0, 0, 8);
	RegisterConfigurationBool("LineStyle.ActionEnabled", &pConfig->LineStyles.Style->ActionEnabled, TRUE);
	
/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("Bulletin", sizeof(*pConfig->Bulletins.Bull), (STRUCT_LIST_S*) &pConfig->Bulletins);
	RegisterConfigurationString("Bulletin.Name", sizeof(pConfig->Bulletins.Bull->Name), (char*)&pConfig->Bulletins.Bull->Name, "");
	RegisterConfigurationString("Bulletin.Comment", sizeof(pConfig->Bulletins.Bull->Comment), (char *) &pConfig->Bulletins.Bull->Comment, "");
	RegisterConfigurationULong("Bulletin.Interval", &pConfig->Bulletins.Bull->Interval, 10, 0, 60);
	RegisterConfigurationBool("Bulletin.Enabled", &pConfig->Bulletins.Bull->Enabled, FALSE);
	RegisterConfigurationBool("Bulletin.ISEnabled", &pConfig->Bulletins.Bull->ISEnabled, FALSE);
	RegisterConfigurationBool("Bulletin.RFEnabled", &pConfig->Bulletins.Bull->RFEnabled, FALSE);
	RegisterConfigurationString("Bulletin.RFPath", sizeof(pConfig->Bulletins.Bull->RFPath), (char*)&pConfig->Bulletins.Bull->RFPath, "");
	RegisterConfigurationSystemTime("Bulletin.LastTransmit", &pConfig->Bulletins.Bull->LastTransmit, NULL);

/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("CQGroup", sizeof(*pConfig->CQGroups.CQGroup), (STRUCT_LIST_S*) &pConfig->CQGroups);
	RegisterConfigurationString("CQGroup.Name", sizeof(pConfig->CQGroups.CQGroup->Name), (char*)&pConfig->CQGroups.CQGroup->Name, "");
	RegisterConfigurationString("CQGroup.Comment", sizeof(pConfig->CQGroups.CQGroup->Comment), (char *) &pConfig->CQGroups.CQGroup->Comment, "");
	RegisterConfigurationULong("CQGroup.Interval", &pConfig->CQGroups.CQGroup->Interval, 8, 0, 24);
	RegisterConfigurationBool("CQGroup.KeepAlive", &pConfig->CQGroups.CQGroup->KeepAlive, FALSE);
	RegisterConfigurationBool("CQGroup.ViaCQSRVR", &pConfig->CQGroups.CQGroup->ViaCQSRVR, FALSE);
	RegisterConfigurationBool("CQGroup.QuietMonitor", &pConfig->CQGroups.CQGroup->QuietMonitor, TRUE);
	RegisterConfigurationBool("CQGroup.ISOnly", &pConfig->CQGroups.CQGroup->ISOnly, TRUE);
	RegisterConfigurationBool("CQGroup.IfPresent", &pConfig->CQGroups.CQGroup->IfPresent, TRUE);
	RegisterConfigurationSystemTime("CQGroup.LastTransmit", &pConfig->CQGroups.CQGroup->LastTransmit, NULL);

/*	First element inside a Struct is the "Key" and must be an Attribute value */
	RegisterConfigurationStruct("ANDefinition", sizeof(*pConfig->ANDefs.ANDef), (STRUCT_LIST_S*) &pConfig->ANDefs, TRUE);	/* Hide if none */
	RegisterConfigurationString("ANDefinition.Name", sizeof(pConfig->ANDefs.ANDef->Name), (char*)&pConfig->ANDefs.ANDef->Name, "");
	RegisterConfigurationString("ANDefinition.Owner", sizeof(pConfig->ANDefs.ANDef->Owner), (char*)&pConfig->ANDefs.ANDef->Owner, "");
	RegisterConfigurationString("ANDefinition.Comment", sizeof(pConfig->ANDefs.ANDef->Comment), (char *) &pConfig->ANDefs.ANDef->Comment, "");
	RegisterConfigurationULong("ANDefinition.IdleTimeoutHours", &pConfig->ANDefs.ANDef->IdleTimeoutHours, 12, 0, 48);
	RegisterConfigurationSystemTime("ANDefinition.LastActivity", &pConfig->ANDefs.ANDef->LastActivity, NULL);
	RegisterConfigurationTimedStringList("ANDefinition.Member", &pConfig->ANDefs.ANDef->Members);

	RegisterConfigurationLong("Telemetry.Version", &pConfig->Telemetry.Version, 0, 0, 100);
	RegisterConfigurationSystemTime("Telemetry.Defined", &pConfig->Telemetry.Defined, NULL);
	RegisterConfigurationULong("Telemetry.DefHours", &pConfig->Telemetry.DefHours, 24, 0, 168);
	RegisterConfigurationULong("Telemetry.Interval", &pConfig->Telemetry.Interval, 15, 1, 60);
	RegisterConfigurationULong("Telemetry.MinTime", &pConfig->Telemetry.MinTime, 15, 10, 5*60);
	RegisterConfigurationULong("Telemetry.Index", &pConfig->Telemetry.Index, 1, 1, 999);

	RegisterConfigurationTimedStringList("ColorChoice", &pConfig->ColorChoices, TRUE, "RGB", TRUE);
	RegisterConfigurationTimedStringList("TrackColor", &pConfig->TrackColors, TRUE, "Enabled", TRUE);

	RegisterConfigurationTimedStringList("Satellite", &pConfig->Satellites, TRUE, "Enabled", TRUE);
	RegisterConfigurationTimedStringList("SatelliteURL", &pConfig->SatelliteURLs, TRUE, "Filtered");
	RegisterConfigurationTimedStringList("SatelliteFile", &pConfig->SatelliteFiles, TRUE, "Filtered");
	RegisterConfigurationULong("SatelliteCount", &pConfig->SatelliteCount, 0, 0, 999);
	RegisterConfigurationString("SatellitePos", sizeof(pConfig->SatellitePos), (char *) &pConfig->SatellitePos, "");
	RegisterConfigurationBool("SatelliteHide", &pConfig->SatelliteHide, FALSE);

	RegisterConfigurationBool("Update.AutoCheck", &pConfig->Update.AutoCheck, TRUE);
	RegisterConfigurationBool("Update.Development", &pConfig->Update.Development, FALSE);
	RegisterConfigurationULong("Update.CheckInterval", &pConfig->Update.CheckInterval, 2, 1, 30);
	RegisterConfigurationSystemTime("Update.LastCheck", &pConfig->Update.LastCheck, NULL);
	RegisterConfigurationString("Update.LastSeen", sizeof(pConfig->Update.LastSeen), (char *) &pConfig->Update.LastSeen, "");
	RegisterConfigurationULong("Update.ReminderInterval", &pConfig->Update.ReminderInterval, 7, 1, 90);
	RegisterConfigurationSystemTime("Update.LastReminder", &pConfig->Update.LastReminder, NULL);

	RegisterConfigurationTimedStringList("IgnoreEavesdrop", &pConfig->IgnoreEavesdrops, TRUE, "Duration");
	RegisterConfigurationStringList("MessageCall", &pConfig->MessageCalls);
	RegisterConfigurationStringList("EMail", &pConfig->EMails);
	RegisterConfigurationTimedStringList("Overlay", &pConfig->POSOverlays, TRUE, "Enabled", TRUE);
	RegisterConfigurationTimedStringList("TelemetryDefinition", &pConfig->TelemetryDefinitions, FALSE);
	RegisterConfigurationTimedStringList("RcvdBulletin", &pConfig->RcvdBulletins);
	RegisterConfigurationTimedStringList("RcvdWeather", &pConfig->RcvdWeather);
	RegisterConfigurationStringList("AutoTrack", &pConfig->AutoTrackers);
	RegisterConfigurationStringList("AutoTrackAlways", &pConfig->AlwaysTrackers);
	RegisterConfigurationStringList("TraceLog", &pConfig->TraceLogs);
	RegisterConfigurationStringList("TraceLogAlways", &pConfig->AlwaysTraceLogs);
	RegisterConfigurationStringList("TraceLogPos", &pConfig->TraceLogsPos);
	RegisterConfigurationStringList("WindowPosition", &pConfig->WindowPositions);

	RegisterConfigurationTimedStringList("SavedPosit", &pConfig->SavedPosits, FALSE, "MinOld");
	RegisterConfigurationString("SavedPositFilter", sizeof(pConfig->SavedPositFilter), (char *) &pConfig->SavedPositFilter, "", FALSE);
#ifdef UNDER_CE
	RegisterConfigurationBool("SavedPositPaths", &pConfig->SavedPositPaths, FALSE);
#else
	RegisterConfigurationBool("SavedPositPaths", &pConfig->SavedPositPaths, FALSE);
#endif

	RegisterConfigurationULong("PacketScrollerSize", &pConfig->PacketScrollerSize, 0, 0, 0);
	RegisterConfigurationString("MaxWidthStationID", sizeof(pConfig->MaxWidthStationID), (char 
		*) &pConfig->MaxWidthStationID, DEFAULT_CALLSIGN);

	RegisterConfigurationObsolete("TelemetryDefined", "Telemetry.Version");
	RegisterConfigurationObsolete("TelemetryInterval", "Telemetry.Interval");
	RegisterConfigurationObsolete("TelemetryIndex", "Telemetry.Index");
	RegisterConfigurationObsolete("Enables.Labels", "View.Labels");
	RegisterConfigurationObsolete("Enables.Tracking", "Enables.Beacons");
	RegisterConfigurationObsolete("Beacon.DAO", "Beacon.Precision");
	RegisterConfigurationObsolete("Object.DAO", NULL);

	RegisterConfigurationObsolete("View.Labels", "View.Callsign");
	RegisterConfigurationObsolete("View.Beacons", NULL);
	RegisterConfigurationObsolete("DateSeconds", NULL);
	RegisterConfigurationObsolete("WheelDelta", NULL);
	RegisterConfigurationObsolete("ZoomReverse", NULL);
	RegisterConfigurationObsolete("DigiTransform", NULL);
	RegisterConfigurationObsolete("AGWPort", "RFPort", ConvertAGWPort);
	RegisterConfigurationObsolete("KISSPort", "RFPort", ConvertKISSPort);
	RegisterConfigurationObsolete("TEXTPort", "RFPort", ConvertTEXTPort);
	RegisterConfigurationObsolete("Enables.AGW", NULL);
	RegisterConfigurationObsolete("Enables.KISS", NULL);
	RegisterConfigurationObsolete("Enables.TEXT", NULL);
	RegisterConfigurationObsolete("Status.TextChoice", NULL);
	RegisterConfigurationObsolete("SatelliteTracking", NULL);
	RegisterConfigurationObsolete("Enables.FrequencyMonitor", "FreqMon.Enabled");

	RegisterConfigurationObsolete("Stations.MaxCount", NULL);

	RegisterConfigurationObsolete("ANDefinition.IdleTimeout", NULL);
	
	RegisterConfigurationObsolete("Nickname.MultiTrackZoom", NULL);

	RegisterConfigurationObsolete("Screen.Paths", "Screen.Paths.Network");
	RegisterConfigurationObsolete("View.DateSeconds", "Screen.DateSeconds");
	RegisterConfigurationObsolete("View.DateTime", "Screen.DateTime");
	RegisterConfigurationObsolete("View.GridSquare", "Screen.GridSquare");
	RegisterConfigurationObsolete("View.LatLon", "Screen.LatLon");
	RegisterConfigurationObsolete("View.Satellites", "Screen.Satellites");
	RegisterConfigurationObsolete("View.Tracks", "Screen.Tracks");
	RegisterConfigurationObsolete("APRSPort", "APRSISPort", ConvertAPRSPort);
	RegisterConfigurationObsolete("APRSServer", "APRSISPort", ConvertAPRSServer);

	RegisterConfigurationObsolete("NWS.Office", "NWS.CWA", ConvertNWSOffice);
	RegisterConfigurationObsolete("AltNetChoice", "AltNet.Choice", ConvertAltNetChoice);
}

char *GetXmlCallsign(HWND hwnd)
{	PARSE_INFO_S *ParseInfo;
	int Result = TRUE;	/* Default that it worked */
	char *File = GetXmlConfigFile(hwnd, "");
	FILE *In = fopen(File,"rt");

	TraceLog("Config", FALSE, hwnd, "GetXmlCallsign(%s)\n", File);

	if (!In) return NULL;

	XML_Parser parser = XML_ParserCreate(NULL);
	ParseInfo = (PARSE_INFO_S *) calloc(1,sizeof(*ParseInfo));
	ParseInfo->Parser = parser;
	ParseInfo->hwnd = hwnd;
	XML_SetUserData(parser, ParseInfo);
	XML_SetElementHandler(parser, startCallsign, endCallsign);
	XML_SetCdataSectionHandler(parser, startCdata, endCdata);
	XML_SetCharacterDataHandler(parser, characterData);

	TraceLog("Config", TRUE, hwnd, "Parsing %s for Callsign\n", File);

#ifdef LINE_AT_A_TIME
	char InBuf[256];

	while (Result && fgets(InBuf, sizeof(InBuf), In))
		Result = XML_Parse(parser, InBuf, strlen(InBuf), FALSE);
#else
	size_t Got;
	char *InBuf = (char*)malloc(4096);
	while (Result && (Got=fread(InBuf,sizeof(*InBuf),4096,In)))
		Result = XML_Parse(parser, InBuf, Got, FALSE);
#endif
	if (Result) Result = XML_Parse(parser, NULL, 0, TRUE);
	if (!Result)
	{	TraceLog("Config", TRUE, hwnd, "XML_Parse(%s) for Callsign FAILED!  %s at line %d\n",
					File,
					XML_ErrorString(XML_GetErrorCode(parser)),
					XML_GetCurrentLineNumber(parser));
		return NULL;	/* No callsign to be had here */
	} else
	{	TraceLog("Config", TRUE, hwnd, "XML_PARSE(%s) for Callsign Succeeded!\n", File);
	}
	XML_ParserFree(parser);
	return ParseInfo->Callsign;	/* Hopefully this wasn't freed */
}


static BOOL LoadXmlConfiguration(HWND hwnd, CONFIG_INFO_S *pConfig, BOOL AllowCreate=FALSE, char *Suffix="")
{	PARSE_INFO_S *ParseInfo;
	BOOL Result = TRUE;
	char *File = GetXmlConfigFile(hwnd, Suffix);
	FILE *In = fopen(File,"rt");

	TraceLog("Config", TRUE, hwnd, "LoadXmlConfiguration(%s)%s\n", File, In?"":" *FAILED*");

	DefineDefaultConfig(pConfig);

	if (!In) return FALSE;

	XML_Parser parser = XML_ParserCreate(NULL);
	ParseInfo = (PARSE_INFO_S *) calloc(1,sizeof(*ParseInfo));
	ParseInfo->Parser = parser;
	ParseInfo->hwnd = hwnd;
	XML_SetUserData(parser, ParseInfo);
	XML_SetElementHandler(parser, startElement, endElement);
	XML_SetCdataSectionHandler(parser, startCdata, endCdata);
	XML_SetCharacterDataHandler(parser, characterData);

	TraceLog("Config", TRUE, hwnd, "Parsing %s\n", File);

	char InBuf[256];

	while (Result && fgets(InBuf, sizeof(InBuf), In))
		Result = XML_Parse(parser, InBuf, strlen(InBuf), FALSE);
	if (Result) Result = XML_Parse(parser, NULL, 0, TRUE);
	if (!Result)
	{	TraceLog("Config", TRUE, hwnd, "XML_Parse(%s) FAILED!  %s at line %d\n",
					File,
					XML_ErrorString(XML_GetErrorCode(parser)),
					XML_GetCurrentLineNumber(parser));
		if (!*Suffix)	/* Only once */
		{	if (!(Result = LoadXmlConfiguration(hwnd, pConfig, FALSE, "-Safe")))
				DefineDefaultConfig(pConfig);
			else MessageBox(hwnd, TEXT("Primary Configuration Format Error, Recovered From -Safe"), TEXT("Load Config"), MB_OK | MB_ICONWARNING);
		} else MessageBox(hwnd, TEXT("Safe Configuration Format Error, Resetting To Defaults"), TEXT("Load Config"), MB_OK | MB_ICONERROR);
	} else
	{	TraceLog("Config", TRUE, hwnd, "XML_PARSE(%s) Succeeded!  %s Saved %04ld-%02ld-%02ld %02ld:%02ld:%02ld\n",
				File, pConfig->LastWhy,
				pConfig->stLastSaved.wYear, 
				pConfig->stLastSaved.wMonth, 
				pConfig->stLastSaved.wDay, 
				pConfig->stLastSaved.wHour, 
				pConfig->stLastSaved.wMinute, 
				pConfig->stLastSaved.wSecond);
		if (!*Suffix /*&& AllowCreate*/)	/* Only the REAL primary load */
		{	char *File = GetXmlConfigFile(hwnd);
			size_t InSize = (strlen(File)+1)*sizeof(TCHAR);
			TCHAR *InFile = (TCHAR*)malloc(InSize);
			StringCbPrintf(InFile,InSize,TEXT("%S"),File);
			File = GetXmlConfigFile(hwnd,"-Safe");
			size_t OutSize = (strlen(File)+1)*sizeof(TCHAR);
			TCHAR *OutFile = (TCHAR*)malloc(OutSize);
			StringCbPrintf(OutFile,OutSize,TEXT("%S"),File);
			if (CopyFile(InFile,OutFile,FALSE))
				TraceLog("Config", TRUE, hwnd, "Copied Good Config(%S) To(%S)\n", InFile, OutFile);
			else TraceLog("Config", TRUE, hwnd, "Copy Config(%S) To(%S) Failed With %ld\n", InFile, OutFile, GetLastError());

			if (*pConfig->Version
			&& strchr(pConfig->Version,' ')
			&& strcmp(strchr(pConfig->Version,' ')+1, Timestamp))
			{	char *OldVers = _strdup(strchr(pConfig->Version,' ')+1);

				TraceLog("Config", TRUE, hwnd, "Config Version Changed From (%s) to (%s)\n", OldVers, Timestamp);

				if (strcmp(OldVers, Timestamp) > 0)
				{	TCHAR *Msg = (TCHAR*)malloc(2048);
				StringCbPrintf(Msg, 2048, TEXT("Downgrading From %S To %S is NOT recommended, Continue Anyway?\n\n(Note: Some settings may be permanently lost.)"), OldVers, Timestamp);
					if (MessageBoxW(hwnd, Msg, TEXT("Version Downgrade"), MB_YESNO | MB_ICONQUESTION) != IDYES)
					{	exit(0);
					}
					free(Msg);
				}

				pConfig->SuppressUnverified = FALSE;	/* Remind about non-verified connection each upgrade */

				for (char *p=OldVers; *p; )
				{	if (!isdigit(*p&0xff)) memmove(p, p+1, strlen(p));
					else p++;
				}
				File = GetXmlConfigFile(hwnd);
				size_t OldSize = (strlen(File)+1+strlen(OldVers)+1)*sizeof(TCHAR);
				TCHAR *OldFile = (TCHAR*)malloc(OldSize);
				StringCbPrintf(OldFile,OldSize,TEXT("%S.%S"),File,OldVers);
				if (CopyFile(InFile,OldFile,FALSE))
					TraceLog("Config", TRUE, hwnd, "Copied Old Config(%S) To(%S) Because (%s) != (%s)\n", InFile, OldFile, strchr(pConfig->Version,' ')+1, Timestamp);
				else TraceLog("Config", TRUE, hwnd, "Copy Old Config(%S) To(%S) Failed With %ld\n", InFile, OldFile, GetLastError());
				free(OldFile); free(OldVers);
			} else TraceLog("Config", TRUE, hwnd, "Config Version Matched (%s) vs (%s)\n", strchr(pConfig->Version,' ')+1, Timestamp);
			free(OutFile); free(InFile);
		}
	}
	if (Result)
		StringCbPrintfA(pConfig->Version, sizeof(pConfig->Version),
							"%s %s", PROGNAME, Timestamp);

	XML_ParserFree(parser);
	ValidateConfig(pConfig);

	if (!pConfig->DefaultedURLs)
	{	pConfig->DefaultedURLs = TRUE;
		AddTimedStringEntry(&pConfig->StationURLs, "aprs.fi=http://aprs.fi/?call=$Call");
		AddTimedStringEntry(&pConfig->TelemetryURLs, "Telem(aprs.fi)=http://aprs.fi/telemetry/$Call");
		AddTimedStringEntry(&pConfig->WeatherURLs, "WX(aprs.fi)=http://aprs.fi/weather/?call=$Call");
	}

#define ADD_IF_NEW(pList,string) if (LocateTimedStringEntry(pList, string)==-1) AddTimedStringEntry(pList,string)
	//if (!pConfig->PathAliases.Count)
	{	ADD_IF_NEW(&pConfig->PathAliases, "RELAY");
		ADD_IF_NEW(&pConfig->PathAliases, "WIDE");
		ADD_IF_NEW(&pConfig->PathAliases, "ECHO");
		ADD_IF_NEW(&pConfig->PathAliases, "TEMP");
		ADD_IF_NEW(&pConfig->PathAliases, "TRACE");
		ADD_IF_NEW(&pConfig->PathAliases, "TCPIP");
		ADD_IF_NEW(&pConfig->PathAliases, "RFONLY");
		ADD_IF_NEW(&pConfig->PathAliases, "NOGATE");
		ADD_IF_NEW(&pConfig->PathAliases, "ARISS");
		ADD_IF_NEW(&pConfig->PathAliases, "DSTAR");
	}
#ifdef NOT_NEEDED
	for (int i=0; i<ParseInfo->MaxDepth; i++)
		if (ParseInfo->Stack[i].ValueB) 
			free(ParseInfo->Stack[i].ValueB);
	if (ParseInfo->Stack) free(ParseInfo->Stack);
	free(ParseInfo);
#endif
	return Result;
}

BOOL CALLBACK GeniusDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static	GENIUS_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (GENIUS_INFO_S *) lp;

		SetDlgItemInt(hdlg, IDE_MIN_TIME, Info->MinTime, FALSE);
		SetDlgItemInt(hdlg, IDE_MAX_TIME, Info->MaxTime, FALSE);
		CheckDlgButton(hdlg, IDC_TIME_ONLY, Info->TimeOnly);
		CheckDlgButton(hdlg, IDC_START_STOP, Info->StartStop);
		SetDlgItemInt(hdlg, IDE_BEARING_CHANGE, Info->BearingChange, FALSE);
		SetDlgItemInt(hdlg, IDE_FORECAST_ERROR, (unsigned long) (Info->ForecastError*10), FALSE);
		SetDlgItemInt(hdlg, IDE_MAX_DISTANCE, (unsigned long) (Info->MaxDistance*10), FALSE);

		SendDlgItemMessage(hdlg, IDS_MIN_TIME, UDM_SETRANGE, 0, GetSpinRangeULong("Genius.MinTime"));
		SendDlgItemMessage(hdlg, IDS_MAX_TIME, UDM_SETRANGE, 0, GetSpinRangeULong("Genius.MaxTime"));
		SendDlgItemMessage(hdlg, IDS_BEARING_CHANGE, UDM_SETRANGE, 0, GetSpinRangeULong("Genius.BearingChange"));
		SendDlgItemMessage(hdlg, IDS_FORECAST_ERROR, UDM_SETRANGE, 0, MAKELONG((unsigned long) (GetConfigMaxDouble("Genius.ForecastError")*10), (unsigned long) (GetConfigMinDouble("Genius.ForecastError")*10)));
		SendDlgItemMessage(hdlg, IDS_MAX_DISTANCE, UDM_SETRANGE, 0, MAKELONG((unsigned long) (GetConfigMaxDouble("Genius.MaxDistance")*10), (unsigned long) (GetConfigMinDouble("Genius.MaxDistance")*10)));

		CenterWindow(hdlg);

		return TRUE;
	}
	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE(Genius) ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_ACCEPT:
			Info->MinTime = GetDlgItemInt(hdlg, IDE_MIN_TIME, NULL, FALSE);
			Info->MaxTime = GetDlgItemInt(hdlg, IDE_MAX_TIME, NULL, FALSE);
			Info->TimeOnly = IsDlgButtonChecked(hdlg, IDC_TIME_ONLY);
			Info->StartStop = IsDlgButtonChecked(hdlg, IDC_START_STOP);
			Info->BearingChange = GetDlgItemInt(hdlg, IDE_BEARING_CHANGE, NULL, FALSE);
			Info->ForecastError = GetDlgItemInt(hdlg, IDE_FORECAST_ERROR, NULL, FALSE) / 10.0;
			Info->MaxDistance = GetDlgItemInt(hdlg, IDE_MAX_DISTANCE, NULL, FALSE) / 10.0;
			EndDialog(hdlg, IDOK);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void BltSymbol(HDC hdc, int x, int y, int Mult, int Div, int Page, int Index, int Percentage, RECT *prc);

BOOL CALLBACK SymbolDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static	SYMBOL_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (SYMBOL_INFO_S *) lp;

		if (Info->Table == '/')
			CheckRadioButton(hdlg, IDC_PRIMARY, IDC_OVERLAY, IDC_PRIMARY);
		else if (Info->Table == '\\')
			CheckRadioButton(hdlg, IDC_PRIMARY, IDC_OVERLAY, IDC_ALTERNATE);
		else CheckRadioButton(hdlg, IDC_PRIMARY, IDC_OVERLAY, IDC_OVERLAY);

		EnableWindow(GetDlgItem(hdlg,IDE_SYM_OVERLAY),IsDlgButtonChecked(hdlg, IDC_OVERLAY));

		SetDlgItemInt(hdlg, IDE_SYM_OVERLAY, Info->Table, FALSE);
		SetDlgItemInt(hdlg, IDE_SYM_VALUE, Info->Symbol, FALSE);

		SendDlgItemMessage(hdlg, IDS_SYM_OVERLAY, UDM_SETRANGE, 0, MAKELONG(GetConfigMaxChar("Symbol.Table"), GetConfigMinChar("Symbol.Table")));
		SendDlgItemMessage(hdlg, IDS_SYM_VALUE, UDM_SETRANGE, 0, MAKELONG(GetConfigMaxChar("Symbol.Symbol"), GetConfigMinChar("Symbol.Symbol")));

		CenterWindow(hdlg);

		return TRUE;
	}
	case WM_DRAWITEM:
	if (wp == IDB_SYMBOL)
	{	RECT rc;
		POINT pt;
		DRAWITEMSTRUCT *di = (LPDRAWITEMSTRUCT) lp;
		char Table;
		if (IsDlgButtonChecked(hdlg, IDC_PRIMARY))
			Table = '/';
		else if (IsDlgButtonChecked(hdlg, IDC_ALTERNATE))
			Table = '\\';
		else
		{	Table = GetDlgItemInt(hdlg, IDE_SYM_OVERLAY, NULL, FALSE);
			if (Table < '0') Table = '0';
			if (Table > '9' && Table < 'A') Table = '9';
			if (Table > 'Z') Table = 'Z';
		}
		char Symbol = GetDlgItemInt(hdlg, IDE_SYM_VALUE, NULL, FALSE);

		pt.x = (di->rcItem.left+di->rcItem.right)/2;
		pt.y = (di->rcItem.bottom+di->rcItem.top)/2;
		Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
		BltSymbol(di->hDC, pt.x, pt.y, 2, 1, Table=='/'?0:Table=='\\'?0x1:((Table<<8)|0x1), Symbol-'!', 100, &rc);
	}
	break;

	case WM_NOTIFY:
	{	NMHDR *nmhdr = (NMHDR *) lp;
#ifdef VERBOSE
TraceLog("Config", TRUE, NULL, "WM_NOTIFY:ID:%ld vs %ld\n", (long) nmhdr->idFrom, (long) IDS_SYM_OVERLAY);
#endif
switch (nmhdr->idFrom)
		{
		case IDS_SYM_OVERLAY:
		{	NMUPDOWN *nmupdown = (NMUPDOWN *)lp;
#ifdef VERBOSE
TraceLog("Config", TRUE, NULL, "Pos:%ld Delta:%ld\n", (long) nmupdown->iPos, (long) nmupdown->iDelta);
#endif
			if (nmupdown->iPos+nmupdown->iDelta == '/'
			|| nmupdown->iPos+nmupdown->iDelta == '\\')
			{	if (nmupdown->iDelta < 0)
					nmupdown->iDelta--;
				else nmupdown->iDelta++;
			}
			break;
		}
		}
		break;
	}

	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDC_PRIMARY:
		case IDC_ALTERNATE:
		case IDC_OVERLAY:
			EnableWindow(GetDlgItem(hdlg,IDE_SYM_OVERLAY),IsDlgButtonChecked(hdlg, IDC_OVERLAY));
		case IDE_SYM_OVERLAY:
		case IDE_SYM_VALUE:
			// if (HIWORD(wp) == EN_CHANGE)
			if (GetDlgItem(hdlg, IDB_ACCEPT))
			{	char Table;
				if (IsDlgButtonChecked(hdlg, IDC_PRIMARY))
					Table = '/';
				else if (IsDlgButtonChecked(hdlg, IDC_ALTERNATE))
					Table = '\\';
				else
				{	Table = GetDlgItemInt(hdlg, IDE_SYM_OVERLAY, NULL, FALSE);
					if (Table < '0') Table = '0';
					if (Table > '9' && Table < 'A') Table = '9';
					if (Table > 'Z') Table = 'Z';
				}
				char Symbol = GetDlgItemInt(hdlg, IDE_SYM_VALUE, NULL, FALSE);
				int iSymbol = Table=='/'?Symbol:Table=='\\'?0x100|Symbol:Table<<16 | 0x100 | Symbol;
				char *Name = GetDisplayableSymbol(iSymbol);
				int uSize = (strlen(Name)+1)*sizeof(TCHAR);
				TCHAR *uName = (TCHAR *) malloc(uSize);
				InvalidateRect(GetDlgItem(hdlg,IDB_SYMBOL),NULL,FALSE);
				if (SUCCEEDED(StringCbPrintf(uName, uSize, TEXT("%S"), Name)))
					SetDlgItemText(hdlg, IDC_SYM_TEXT, uName);
				if (SUCCEEDED(StringCbPrintf(uName, uSize, TEXT("%c%c"), Table, Symbol)))
					SetDlgItemText(hdlg, IDC_SYM_SYMBOL, uName);
				free(uName);
				free(Name);
			}
			break;
		case IDB_ACCEPT:
		case IDB_SYMBOL:
			if (IsDlgButtonChecked(hdlg, IDC_PRIMARY))
				Info->Table = '/';
			else if (IsDlgButtonChecked(hdlg, IDC_ALTERNATE))
				Info->Table = '\\';
			else
			{	Info->Table = GetDlgItemInt(hdlg, IDE_SYM_OVERLAY, NULL, FALSE);
				if (Info->Table < '0') Info->Table = '0';
				if (Info->Table > '9' && Info->Table < 'A') Info->Table = '9';
				if (Info->Table > 'Z') Info->Table = 'Z';
			}
			Info->Symbol = GetDlgItemInt(hdlg, IDE_SYM_VALUE, NULL, FALSE);
			EndDialog(hdlg, IDOK);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static TCHAR *GetCommPorts(void)
{	TCHAR *Result = NULL;
	int r=0;
#ifdef UNDER_CE
			HKEY ActiveKey;
			if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Drivers\\Active"),0,KEY_READ,&ActiveKey) == ERROR_SUCCESS)
			{	LONG QResult;
				DWORD NameSize = 512;
				TCHAR *Name = (TCHAR*)malloc(NameSize*sizeof(*Name));
				FILETIME ft;
				DWORD dwIndex=0;

				while ((QResult=RegEnumKeyEx(ActiveKey,dwIndex,Name,&NameSize,NULL,NULL,NULL,&ft)) == ERROR_SUCCESS)
				{	HKEY EntryKey;
					if (RegOpenKeyEx(ActiveKey,Name,0,KEY_READ,&EntryKey) == ERROR_SUCCESS)
					{	DWORD ValueType;
						DWORD ValueSize = 512;
						BYTE *ValueValue = (BYTE*)malloc(ValueSize*sizeof(*ValueValue));
						if (RegQueryValueEx(EntryKey,TEXT("Name"),NULL,&ValueType,ValueValue,&ValueSize) == ERROR_SUCCESS)
						{	if (ValueType == REG_SZ)
							{
								if (!wcsncmp((TCHAR*)ValueValue, TEXT("COM"), 3))
								{	Result = (TCHAR*)realloc(Result,(r+ValueSize+2)*sizeof(*Result));
									r += wsprintf(&Result[r],TEXT("%s"),ValueValue)+1;/* After null */
									if (r >= 2 && Result[r-2] == *TEXT(":"))
									{	Result[r-2] = 0;
										r--;
									}
									Result[r] = 0;
								}
							} else MessageBox(NULL, TEXT("ValueType != REG_SZ"), Name,
										MB_OK | MB_ICONERROR);
						}
						free((void*)ValueValue);
						RegCloseKey(EntryKey);
					} else MessageBox(NULL, Name, TEXT("RegOpenKeyEx"),
										MB_OK | MB_ICONERROR);
					dwIndex++; NameSize = 512;
				}
				free((void*)Name);

				{		int n;
						DWORD ValueSize = 512;
						BYTE *ValueValue = (BYTE*)malloc(ValueSize*sizeof(*ValueValue));
						for (n=0; n<10; n++)
						{		ValueSize = wsprintf((TCHAR*)ValueValue, TEXT("COM%ld:"), (long) n);

								if (!wcsncmp((TCHAR*)ValueValue, TEXT("COM"), 3))
								{	Result = (TCHAR*)realloc(Result,(r+ValueSize+2)*sizeof(*Result));
									r += wsprintf(&Result[r],TEXT("%s"),ValueValue)+1;/* After null */
									if (r >= 2 && Result[r-2] == *TEXT(":"))
									{	Result[r-2] = 0;
										r--;
									}
									Result[r] = 0;
								}
						}
						free((void*)ValueValue);
				}

				if (QResult != ERROR_NO_MORE_ITEMS)
				{	TCHAR *Text = (TCHAR*)malloc(256*sizeof(*Text));
					wsprintf(Text,TEXT("Status %ld != %ld"),(long) QResult, (long) ERROR_NO_MORE_ITEMS);
					MessageBox(NULL, Text, TEXT("RegEnumKeyEx"),
								MB_OK | MB_ICONERROR);
					free(Text);
				}
				RegCloseKey(ActiveKey);
			} else MessageBox(NULL, TEXT("Failed To Open Active Drivers Key"), TEXT("RegOpenKeyEx"),
								MB_OK | MB_ICONERROR);
#else
			HKEY SerialKey;
			if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Hardware\\DeviceMap\\SERIALCOMM"),0,KEY_READ, &SerialKey) == ERROR_SUCCESS)
			{	LONG QResult;
				DWORD NameSize = 512;
				TCHAR *ValueName = (TCHAR*) malloc(NameSize*sizeof(*ValueName));
				DWORD ValueType;
				DWORD ValueSize = 512;
				BYTE *ValueValue = (BYTE*) malloc(ValueSize*sizeof(*ValueValue));
				DWORD dwIndex=0;
				while ((QResult=RegEnumValue(SerialKey,dwIndex,ValueName,&NameSize,NULL,&ValueType,ValueValue,&ValueSize)) == ERROR_SUCCESS)
				{	if (ValueType == REG_SZ)
					{	Result = (TCHAR*)realloc(Result,(r+ValueSize+2)*sizeof(*Result));
						r += wsprintf(&Result[r],TEXT("%s"),ValueValue)+1;/* After null */
						Result[r] = 0;
					} else MessageBox(NULL, TEXT("ValueType != REG_SZ"), ValueName,
								MB_OK | MB_ICONERROR);
					dwIndex++; NameSize = ValueSize = 512;
				}
				free((void*)ValueName); free((void*)ValueValue);
				if (QResult != ERROR_NO_MORE_ITEMS)
				{	TCHAR *Text = (TCHAR*)malloc(256*sizeof(*Text));
					wsprintf(Text,TEXT("Status %ld != %ld"),(long) QResult, (long) ERROR_NO_MORE_ITEMS);
					MessageBox(NULL, Text, TEXT("RegEnumValue"),
								MB_OK | MB_ICONERROR);
					free(Text);
				}
				RegCloseKey(SerialKey);
			} else MessageBox(NULL, TEXT("Failed To Open SERIALCOMM Key"), TEXT("RegOpenKeyEx"),
								MB_OK | MB_ICONERROR);
#endif
	return Result;
}


static int WideCmpMulti(TCHAR *Wide, char *Multi)
{
#ifdef UNICODE
	int Result;
	int Len = WideCharToMultiByte(CP_ACP, 0, Wide, -1,
								NULL, 0, NULL, NULL);
	char *Temp = (char*)malloc(Len*sizeof(*Temp));
	Len = WideCharToMultiByte(CP_ACP, 0, Wide, -1,
								Temp, Len, NULL, NULL);
	Result = strncmp(Temp, Multi, Len);
	free(Temp);
	return Result;
#else
	return strcmp(Wide, Multi);
#endif
}

typedef struct NUMBER_PROMPT_INFO_S
{	TCHAR *Title;
	TCHAR *Prompt;
	TCHAR *Units;
	long Value;
	long MinValue;
	long MaxValue;
	BOOL Signed;
} NUMBER_PROMPT_INFO_S;

static BOOL CALLBACK NumberPromptDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static	BOOL EntryChanged;
static NUMBER_PROMPT_INFO_S *Info = NULL;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (NUMBER_PROMPT_INFO_S *) lp;

		SetWindowText(hdlg, Info->Title);
		SetDlgItemText(hdlg, IDC_PROMPT, Info->Prompt);
		SetDlgItemText(hdlg, IDC_UNITS, Info->Units);

		SetFocus(GetDlgItem(hdlg, IDE_TEXT));

		SetDlgItemInt(hdlg, IDE_RANGE, Info->Value, Info->Signed);

		if (Info->MinValue || Info->MaxValue)
			SendDlgItemMessage(hdlg, IDS_RANGE, UDM_SETRANGE, 0, MAKELONG(Info->MaxValue, Info->MinValue));
		else DestroyWindow(GetDlgItem(hdlg, IDS_RANGE));

#ifdef USING_SIP
		SipShowIM(SIPF_ON);	/* Bring up the keyboard for message typing */
#endif
		EntryChanged = FALSE;

		CenterWindow(hdlg);

		return FALSE;	/* I already set focus */
	}
	case WM_COMMAND:

		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDCANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDOK:
		{	if (EntryChanged)
			{	Info->Value = GetDlgItemInt(hdlg, IDE_RANGE, NULL, Info->Signed);
				EndDialog(hdlg, IDOK);
			} else EndDialog(hdlg, IDCANCEL);
			return TRUE;
		}
		case IDE_RANGE:
		{	switch (HIWORD(wp))
			{
			case EN_CHANGE:
				EntryChanged = TRUE;
				break;
			}
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

unsigned long NumberPrompt(HWND hwnd, char *Title, char *Prompt, char *Units, char *ConfigParam, unsigned long DefaultValue)
{	NUMBER_PROMPT_INFO_S Info = {0};
	long Result = DefaultValue;

	Info.Title = (TCHAR *)malloc((strlen(Title)+1)*sizeof(*Info.Title));
	StringCbPrintf(Info.Title, (strlen(Title)+1)*sizeof(*Info.Title), TEXT("%S"), Title);

	Info.Prompt = (TCHAR *)malloc((strlen(Prompt)+1)*sizeof(*Info.Prompt));
	StringCbPrintf(Info.Prompt, (strlen(Prompt)+1)*sizeof(*Info.Prompt), TEXT("%S"), Prompt);

	Info.Units = (TCHAR *)malloc((strlen(Units)+1)*sizeof(*Info.Units));
	StringCbPrintf(Info.Units, (strlen(Units)+1)*sizeof(*Info.Units), TEXT("%S"), Units);

	Info.Value = DefaultValue;
	if (ConfigParam)
	{	Info.MinValue = GetConfigMinULong(ConfigParam);
		Info.MaxValue = GetConfigMaxULong(ConfigParam);
	} else Info.MinValue = Info.MaxValue = 0;
	Info.Signed = FALSE;

	switch (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SPIN_PROMPT), hwnd, NumberPromptDlgProc, (LPARAM) &Info))
	{
	case IDOK:
	{	Result = Info.Value;
		break;
	}
	case IDCANCEL:
		break;
	default:
		MessageBox(hwnd, TEXT("NumberPrompt Failed"), TEXT("DialogBoxParam"), MB_ICONERROR);
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	free(Info.Title);
	free(Info.Prompt);
	free(Info.Units);
	return Result;
}

long SignedNumberPrompt(HWND hwnd, char *Title, char *Prompt, char *Units, char *ConfigParam, long DefaultValue)
{	NUMBER_PROMPT_INFO_S Info = {0};
	long Result = DefaultValue;

	Info.Title = (TCHAR *)malloc((strlen(Title)+1)*sizeof(*Info.Title));
	StringCbPrintf(Info.Title, (strlen(Title)+1)*sizeof(*Info.Title), TEXT("%S"), Title);

	Info.Prompt = (TCHAR *)malloc((strlen(Prompt)+1)*sizeof(*Info.Prompt));
	StringCbPrintf(Info.Prompt, (strlen(Prompt)+1)*sizeof(*Info.Prompt), TEXT("%S"), Prompt);

	Info.Units = (TCHAR *)malloc((strlen(Units)+1)*sizeof(*Info.Units));
	StringCbPrintf(Info.Units, (strlen(Units)+1)*sizeof(*Info.Units), TEXT("%S"), Units);

	Info.Value = DefaultValue;
	if (ConfigParam)
	{	Info.MinValue = GetConfigMinLong(ConfigParam);
		Info.MaxValue = GetConfigMaxLong(ConfigParam);
	} else Info.MinValue = Info.MaxValue = 0;
	Info.Signed = TRUE;

	switch (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SPIN_PROMPT), hwnd, NumberPromptDlgProc, (LPARAM) &Info))
	{
	case IDOK:
	{	Result = Info.Value;
		break;
	}
	case IDCANCEL:
		break;
	default:
		MessageBox(hwnd, TEXT("NumberPrompt Failed"), TEXT("DialogBoxParam"), MB_ICONERROR);
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	free(Info.Title);
	free(Info.Prompt);
	free(Info.Units);
	return Result;
}


typedef struct STRING_PROMPT_INFO_S
{	TCHAR *Title;
	TCHAR *Prompt;
	TCHAR *String;
	int MaxLen;
	BOOL UpCase;
} STRING_PROMPT_INFO_S;

static BOOL CALLBACK StringPromptDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static	BOOL EntryChanged;
static	TCHAR EnterText[] = TEXT("Enter Name Here");
static STRING_PROMPT_INFO_S *Info = NULL;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (STRING_PROMPT_INFO_S *) lp;

		SetWindowText(hdlg, Info->Title);

		SetDlgItemText(hdlg, IDC_PROMPT, Info->Prompt);

		SendDlgItemMessage(hdlg, IDE_TEXT, EM_LIMITTEXT, Info->MaxLen, 0);
		SetDlgItemText(hdlg, IDE_TEXT, Info->String);
		SetFocus(GetDlgItem(hdlg, IDE_TEXT));
		SendDlgItemMessage(hdlg, IDE_TEXT, EM_SETSEL, 0, -1);
		if (Info->UpCase)
		{	HWND hwndEntry = GetDlgItem(hdlg, IDE_TEXT);
			LONG entryStyle = GetWindowLong(hwndEntry, GWL_STYLE);
			SetWindowLong(hwndEntry, GWL_STYLE, entryStyle | ES_UPPERCASE);
		}

#ifdef USING_SIP
		SipShowIM(SIPF_ON);	/* Bring up the keyboard for message typing */
#endif
		EntryChanged = FALSE;

		CenterWindow(hdlg);

		return FALSE;	/* I already set focus */
	}
	case WM_COMMAND:

		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDCANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDOK:
		{	if (EntryChanged)
			{	GetDlgItemText(hdlg, IDE_TEXT, Info->String, Info->MaxLen+1);
				EndDialog(hdlg, IDOK);
			} else EndDialog(hdlg, IDRETRY);
			return TRUE;
		}
		case IDE_TEXT:
		{	TCHAR *Text = (TCHAR*)malloc(sizeof(*Text)*(Info->MaxLen+1));
			switch (HIWORD(wp))
			{
			case EN_CHANGE:
				EntryChanged = TRUE;
				break;
			case EN_KILLFOCUS:
				if (GetDlgItemText(hdlg, IDE_TEXT, Text, Info->MaxLen+1))
					if (!memcmp(Text, EnterText, sizeof(EnterText)))
						SetDlgItemText(hdlg, IDE_TEXT, TEXT(""));
			}
			free(Text);
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

char *StringPromptA(HWND hwnd, char *Title, char *Prompt, int MaxLen, char *String, BOOL UpCase, BOOL NoChangeSuccess)
{	STRING_PROMPT_INFO_S Info = {0};
	char *Result = NULL;

	Info.Title = (TCHAR *)malloc((strlen(Title)+1)*sizeof(*Info.Title));
	StringCbPrintf(Info.Title, (strlen(Title)+1)*sizeof(*Info.Title), TEXT("%S"), Title);

	Info.Prompt = (TCHAR *)malloc((strlen(Prompt)+1)*sizeof(*Info.Prompt));
	StringCbPrintf(Info.Prompt, (strlen(Prompt)+1)*sizeof(*Info.Prompt), TEXT("%S"), Prompt);

	Info.String = (TCHAR *)malloc((MaxLen+1)*sizeof(*Info.String));
	StringCbPrintf(Info.String, (MaxLen+1)*sizeof(*Info.String), TEXT("%S"), String);

	Info.MaxLen = MaxLen;
	Info.UpCase = UpCase;

	switch (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_STRING_PROMPT), hwnd, StringPromptDlgProc, (LPARAM) &Info))
	{
	case IDRETRY:
		if (!NoChangeSuccess) break;
		if (!Info.String[0]) break;
	case IDOK:	/* IDRETRY falls here to set result */
	{	Result = (char*)malloc(MaxLen+1);
		StringCbPrintfA(Result, MaxLen+1, "%.*S", (int) MaxLen, Info.String);
		break;
	}
	case IDCANCEL:
		break;
	default:
		MessageBox(hwnd, TEXT("StringPrompt Failed"), TEXT("DialogBoxParam"), MB_ICONERROR);
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	free(Info.Title);
	free(Info.Prompt);
	free(Info.String);
	return Result;
}

static BOOL GetSelectedColor(HWND hdlg, int ListID, size_t ColorLen, char *pColor)
{	TCHAR Color[COLOR_SIZE+1];
	int Sel = SendDlgItemMessage(hdlg,ListID,CB_GETCURSEL,0,0);
	if (Sel != CB_ERR)
	{	if (SendDlgItemMessage(hdlg,ListID,CB_GETLBTEXT,Sel,(LPARAM)Color) != CB_ERR)
		{
			StringCbPrintfA(pColor, ColorLen, "%S", Color);
			return TRUE;
		}
	}
	return FALSE;
}

static struct
{	COLOR_SET_T Set;
	TCHAR *Description;
} ColorSetDescriptions[] = { { COLORS_AVAILABLE, TEXT("Available Track Colors") },
					{ COLORS_NO_TRACKS, TEXT("Non-Track Colors") },
					{ COLORS_TRACKS, TEXT("All Track Colors") },
					{ COLORS_ALL, TEXT("All Defined Colors") } };

static void SetupColorList(HWND hdlg, int ListID, COLOR_SET_T Set, int SetShownStaticID, char *SelectColor)
{	int i;
	TCHAR uColor[COLOR_SIZE+1];
	HWND hwndList = GetDlgItem(hdlg, ListID);

	if (!hwndList) return;	/* No list to populate */

	if (SetShownStaticID)
	{
		for (i=0; i<ARRAYSIZE(ColorSetDescriptions); i++)
		{	if (ColorSetDescriptions[i].Set == Set)
			{	SetDlgItemText(hdlg, SetShownStaticID, ColorSetDescriptions[i].Description);
			}
		}
	}
	SendMessage(hwndList, LB_RESETCONTENT, 0, 0);
	switch (Set)
	{
	case COLORS_AVAILABLE:
	{	TIMED_STRING_LIST_S *pList = &ActiveConfig.TrackColors;
		EnableWindow(GetDlgItem(hdlg,IDB_LESS),FALSE);
		EnableWindow(GetDlgItem(hdlg,IDB_MORE),TRUE);
		EnableWindow(GetDlgItem(hdlg,IDB_MORE2),TRUE);
		for (unsigned long c=0; c<pList->Count; c++)
		if (!pList->Entries[c].value
		&& strcmp(pList->Entries[c].string,
					ActiveConfig.Screen.Track.Follow.Color)
		&& strcmp(pList->Entries[c].string,
					ActiveConfig.Screen.Track.Other.Color))
		{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
			SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM) uColor);
		}
		break;
	}
	case COLORS_NO_TRACKS:
	{	TIMED_STRING_LIST_S *pList = &ActiveConfig.ColorChoices;
		DestroyWindow(GetDlgItem(hdlg,IDB_LESS));
		DestroyWindow(GetDlgItem(hdlg,IDB_MORE));
		DestroyWindow(GetDlgItem(hdlg,IDB_MORE2));
		for (unsigned long c=0; c<pList->Count; c++)
		if (LocateTimedStringEntry(&ActiveConfig.TrackColors,
									pList->Entries[c].string) == -1)
		{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
			SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM) uColor);
		}
		break;
	}
	case COLORS_TRACKS:
	{	TIMED_STRING_LIST_S *pList = &ActiveConfig.TrackColors;
		EnableWindow(GetDlgItem(hdlg,IDB_MORE),FALSE);
		EnableWindow(GetDlgItem(hdlg,IDB_LESS),TRUE);
		EnableWindow(GetDlgItem(hdlg,IDB_MORE2),TRUE);
		for (unsigned long c=0; c<pList->Count; c++)
		{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
			SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM) uColor);
		}
		break;
	}
	case COLORS_ALL:
	{	TIMED_STRING_LIST_S *pList = &ActiveConfig.ColorChoices;
		EnableWindow(GetDlgItem(hdlg,IDB_MORE2),FALSE);
		EnableWindow(GetDlgItem(hdlg,IDB_LESS),TRUE);
		EnableWindow(GetDlgItem(hdlg,IDB_MORE),TRUE);
		for (unsigned long c=0; c<pList->Count; c++)
		{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
			SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM) uColor);
		}
		break;
	}
	}	/* End of switch */

	if (SelectColor[0])
	{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),SelectColor);
		if (SendMessage(hwndList, LB_SELECTSTRING, -1, (LPARAM) uColor) == LB_ERR)
		{	SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM) uColor);
			SendMessage(hwndList, LB_SELECTSTRING, -1, (LPARAM) uColor);
		}
	}
}

static void SetupColorCombo(HWND hdlg, int ComboID, COLOR_SET_T Set, int SetShownStaticID, char *SelectColor)
{	int i;
	TCHAR uColor[COLOR_SIZE+1];
	HWND hwndCombo = GetDlgItem(hdlg, ComboID);

	if (!hwndCombo) return;	/* No list to populate */

	if (SetShownStaticID)
	{
		for (i=0; i<ARRAYSIZE(ColorSetDescriptions); i++)
		{	if (ColorSetDescriptions[i].Set == Set)
			{	SetDlgItemText(hdlg, SetShownStaticID, ColorSetDescriptions[i].Description);
			}
		}
	}
	SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0);
	switch (Set)
	{
	case COLORS_AVAILABLE:
	{	TIMED_STRING_LIST_S *pList = &ActiveConfig.TrackColors;
		EnableWindow(GetDlgItem(hdlg,IDB_LESS),FALSE);
		EnableWindow(GetDlgItem(hdlg,IDB_MORE),TRUE);
		EnableWindow(GetDlgItem(hdlg,IDB_MORE2),TRUE);
		for (unsigned long c=0; c<pList->Count; c++)
		if (!pList->Entries[c].value
		&& strcmp(pList->Entries[c].string,
					ActiveConfig.Screen.Track.Follow.Color)
		&& strcmp(pList->Entries[c].string,
					ActiveConfig.Screen.Track.Other.Color))
		{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
			SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM) uColor);
		}
		break;
	}
	case COLORS_NO_TRACKS:
	{	TIMED_STRING_LIST_S *pList = &ActiveConfig.ColorChoices;
		DestroyWindow(GetDlgItem(hdlg,IDB_LESS));
		DestroyWindow(GetDlgItem(hdlg,IDB_MORE));
		DestroyWindow(GetDlgItem(hdlg,IDB_MORE2));
		for (unsigned long c=0; c<pList->Count; c++)
		if (LocateTimedStringEntry(&ActiveConfig.TrackColors,
									pList->Entries[c].string) == -1)
		{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
			SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM) uColor);
		}
		break;
	}
	case COLORS_TRACKS:
	{	TIMED_STRING_LIST_S *pList = &ActiveConfig.TrackColors;
		EnableWindow(GetDlgItem(hdlg,IDB_MORE),FALSE);
		EnableWindow(GetDlgItem(hdlg,IDB_LESS),TRUE);
		EnableWindow(GetDlgItem(hdlg,IDB_MORE2),TRUE);
		for (unsigned long c=0; c<pList->Count; c++)
		{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
			SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM) uColor);
		}
		break;
	}
	case COLORS_ALL:
	{	TIMED_STRING_LIST_S *pList = &ActiveConfig.ColorChoices;
		EnableWindow(GetDlgItem(hdlg,IDB_MORE2),FALSE);
		EnableWindow(GetDlgItem(hdlg,IDB_LESS),TRUE);
		EnableWindow(GetDlgItem(hdlg,IDB_MORE),TRUE);
		for (unsigned long c=0; c<pList->Count; c++)
		{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
			SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM) uColor);
		}
		break;
	}
	}	/* End of switch */

	if (SelectColor[0])
	{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),SelectColor);
		if (SendMessage(hwndCombo, CB_SELECTSTRING, -1, (LPARAM) uColor) == CB_ERR)
		{	SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM) uColor);
			SendMessage(hwndCombo, CB_SELECTSTRING, -1, (LPARAM) uColor);
		}
	}
}

typedef struct COLOR_PROMPT_INFO_S
{	TCHAR *Title;
	char Color[COLOR_SIZE];	/* Current (& new) selection */
	COLOR_SET_T Set;
} COLOR_PROMPT_INFO_S;

static BOOL CALLBACK ColorPromptDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
static COLOR_PROMPT_INFO_S *Info = NULL;
#define WM_SET_COLORS WM_USER+42
	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (COLOR_PROMPT_INFO_S *) lp;

		SetWindowText(hdlg, Info->Title);

		SendMessage(hdlg, WM_SET_COLORS, 0, 0);

		CenterWindow(hdlg);

		return FALSE;	/* I already set focus */
	}
	case WM_SET_COLORS:
	{
		SetupColorList(hdlg, IDL_COLOR, Info->Set, IDC_COLORS_SHOWN, Info->Color);
#ifdef OLD_WAY
		HWND hwndList = GetDlgItem(hdlg, IDL_COLOR);

		TCHAR uColor[sizeof(Info->Color)+1];
		for (i=0; i<ARRAYSIZE(Descriptions); i++)
		{	if (Descriptions[i].Set == Info->Set)
			{	SetDlgItemText(hdlg, IDC_COLORS_SHOWN, Descriptions[i].Description);
			}
		}
		if (hwndList)
		{
			SendMessage(hwndList, LB_RESETCONTENT, 0, 0);
			switch (Info->Set)
			{
			case COLORS_AVAILABLE:
			{	TIMED_STRING_LIST_S *pList = &ActiveConfig.TrackColors;
				EnableWindow(GetDlgItem(hdlg,IDB_LESS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDB_MORE),TRUE);
				EnableWindow(GetDlgItem(hdlg,IDB_MORE2),TRUE);
				for (unsigned long c=0; c<pList->Count; c++)
				if (!pList->Entries[c].value
				&& strcmp(pList->Entries[c].string,
							ActiveConfig.Screen.Track.Follow.Color)
				&& strcmp(pList->Entries[c].string,
							ActiveConfig.Screen.Track.Other.Color))
				{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
					SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM) uColor);
				}
				break;
			}
			case COLORS_NO_TRACKS:
			{	TIMED_STRING_LIST_S *pList = &ActiveConfig.ColorChoices;
				DestroyWindow(GetDlgItem(hdlg,IDB_LESS));
				DestroyWindow(GetDlgItem(hdlg,IDB_MORE));
				DestroyWindow(GetDlgItem(hdlg,IDB_MORE2));
				for (unsigned long c=0; c<pList->Count; c++)
				if (LocateTimedStringEntry(&ActiveConfig.TrackColors,
											pList->Entries[c].string) == -1)
				{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
					SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM) uColor);
				}
				break;
			}
			case COLORS_TRACKS:
			{	TIMED_STRING_LIST_S *pList = &ActiveConfig.TrackColors;
				TCHAR uColor[sizeof(Info->Color)+1];
				EnableWindow(GetDlgItem(hdlg,IDB_MORE),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDB_LESS),TRUE);
				EnableWindow(GetDlgItem(hdlg,IDB_MORE2),TRUE);
				for (unsigned long c=0; c<pList->Count; c++)
				{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
					SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM) uColor);
				}
				break;
			}
			case COLORS_ALL:
			{	TIMED_STRING_LIST_S *pList = &ActiveConfig.ColorChoices;
				TCHAR uColor[sizeof(Info->Color)+1];
				EnableWindow(GetDlgItem(hdlg,IDB_MORE2),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDB_LESS),TRUE);
				EnableWindow(GetDlgItem(hdlg,IDB_MORE),TRUE);
				for (unsigned long c=0; c<pList->Count; c++)
				{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
					SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM) uColor);
				}
				break;
			}
			}	/* End of switch */
			if (Info->Color[0])
			{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),Info->Color);
				if (SendMessage(hwndList, LB_SELECTSTRING, -1, (LPARAM) uColor) == LB_ERR)
				{	SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM) uColor);
					SendMessage(hwndList, LB_SELECTSTRING, -1, (LPARAM) uColor);
				}
			}
		} else MessageBox(hdlg, TEXT("No Listbox"), TEXT("WM_SOMETHING"), MB_ICONERROR | MB_OK);
#endif
		break;	/* From WM_SET_COLORS */
	}
	case WM_COMMAND:

		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDCANCEL:
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_ACCEPT:
		{	//if (EntryChanged)
			{	TCHAR Color[COLOR_SIZE+1];
				int Sel = SendDlgItemMessage(hdlg,IDL_COLOR,LB_GETCURSEL,0,0);
				if (Sel != LB_ERR)
				{	if (SendDlgItemMessage(hdlg,IDL_COLOR,LB_GETTEXT,Sel,(LPARAM)Color) != LB_ERR)
					{
						StringCbPrintfA(Info->Color, sizeof(Info->Color), "%S", Color);
						EndDialog(hdlg, IDOK);
					}
				}
			}// else EndDialog(hdlg, IDCANCEL);
			return TRUE;
		}
		case IDB_MORE:
			Info->Set = COLORS_TRACKS;
			SendMessage(hdlg, WM_SET_COLORS, 0, 0);
			break;
		case IDB_MORE2:
			Info->Set = COLORS_ALL;
			SendMessage(hdlg, WM_SET_COLORS, 0, 0);
			break;
		case IDB_LESS:
			Info->Set = COLORS_AVAILABLE;
			SendMessage(hdlg, WM_SET_COLORS, 0, 0);
			break;
		case IDL_COLOR:
		{	switch (HIWORD(wp))
			{
			case LBN_DBLCLK:
				PostMessage(hdlg, WM_COMMAND, IDB_ACCEPT, 0);
				break;
			}
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
#undef WM_SET_COLORS
}

char *ColorPrompt(HWND hwnd, char *Title, char *Color, COLOR_SET_T InitialSet)
{	COLOR_PROMPT_INFO_S Info = {0};
	char *Result = NULL;

	Info.Title = (TCHAR *)malloc((strlen(Title)+1)*sizeof(*Info.Title));
	StringCbPrintf(Info.Title, (strlen(Title)+1)*sizeof(*Info.Title), TEXT("%S"), Title);

	strncpy(Info.Color, Color, sizeof(Info.Color));
	Info.Set = InitialSet;

	switch (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_COLOR), hwnd, ColorPromptDlgProc, (LPARAM) &Info))
	{
	case IDOK:
	{	Result = (char*)malloc(COLOR_SIZE+1);
		strncpy(Result, Info.Color, COLOR_SIZE+1);
		break;
	}
	case IDCANCEL:
		break;
	default:
		MessageBox(hwnd, TEXT("StringPrompt Failed"), TEXT("DialogBoxParam"), MB_ICONERROR);
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	free(Info.Title);
	return Result;
}

static BOOL CALLBACK CommPortDlgProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{static PORT_CONFIG_S *ip;
static	struct
{	long Baud;
	TCHAR *String;
} Bauds[] = { { 19200, TEXT("19200") },
				{ 9600, TEXT("9600") },
				{ 4800, TEXT("4800") },
				{ 2400, TEXT("2400") },
				{ 1200, TEXT("1200") },
				{ 38400, TEXT("38400") },
				{ 56000, TEXT("56000") },
				{ 57600, TEXT("57600") },
				{ 115200, TEXT("115200") },
				{ 128000, TEXT("128000") },
				{ 256000, TEXT("256000") } };

#define BaudCount (sizeof(Bauds)/sizeof(Bauds[0]))

	switch (wMsg)
	{
	case WM_INITDIALOG:
	{	int i;
		ip = (PORT_CONFIG_S *) lParam;

		{	TCHAR *Ports = GetCommPorts();
			if (Ports)
			{	TCHAR *p;
				LONG index = SendDlgItemMessage(hWnd, IDC_COMMPORT, CB_ADDSTRING, 0, (LPARAM) TEXT(""));
				SendDlgItemMessage(hWnd, IDC_COMMPORT, CB_SETCURSEL, index, 0);
				for (p=Ports; *p; p+=wcslen(p)+1)
				{	index = SendDlgItemMessage(hWnd, IDC_COMMPORT, CB_ADDSTRING, 0, (LPARAM) p);
					if (!WideCmpMulti(p, ip->Port))
						SendDlgItemMessage(hWnd, IDC_COMMPORT, CB_SETCURSEL, index, 0);
				}
				free(Ports);
			} else EndDialog(hWnd, 0);
		}

		for (i=0; i<BaudCount; i++)
		{	LONG index = SendDlgItemMessage(hWnd, IDC_BAUDRATE, CB_ADDSTRING, 0, (LPARAM) Bauds[i].String);
			if (Bauds[i].Baud == ip->Baud)
				SendDlgItemMessage(hWnd, IDC_BAUDRATE, CB_SETCURSEL, index, 0);
		}
		SendDlgItemMessage(hWnd, IDC_XMIT, BM_SETCHECK, ip->Transmit?BST_CHECKED:BST_UNCHECKED, 0);
		switch (ip->Bits)
		{
		case 7: CheckRadioButton(hWnd, IDR_SEVEN, IDR_EIGHT, IDR_SEVEN); break;
		default: ip->Bits = 8;
		case 8: CheckRadioButton(hWnd, IDR_SEVEN, IDR_EIGHT, IDR_EIGHT); break;
		}
		switch (ip->Parity)
		{
		case 1: CheckRadioButton(hWnd, IDR_NONE, IDR_ODD, IDR_ODD); break;
		case 2: CheckRadioButton(hWnd, IDR_NONE, IDR_ODD, IDR_EVEN); break;
		default: ip->Parity = 0;
		case 0: CheckRadioButton(hWnd, IDR_NONE, IDR_ODD, IDR_NONE); break;
		}
		switch (ip->Stop)
		{
		case 2: CheckRadioButton(hWnd, IDR_ONE, IDR_TWO, IDR_TWO); break;
		default: ip->Stop = 1;
		case 1: CheckRadioButton(hWnd, IDR_ONE, IDR_TWO, IDR_ONE); break;
		}

		CenterWindow(hWnd);

		return TRUE;
	}
	case WM_COMMAND:

		if (!MakeFocusControlVisible(hWnd, wParam, lParam))
		switch (LOWORD(wParam))
		{
		case IDOK:
		{	char Temp[32], *e;

			myGetDlgItemText(hWnd, IDC_COMMPORT, ip->Port, sizeof(ip->Port));

			myGetDlgItemText(hWnd, IDC_BAUDRATE, Temp, sizeof(Temp));
			ip->Baud = strtol(Temp,&e,10);

			ip->Transmit = IsDlgButtonChecked(hWnd, IDC_XMIT);

			if (*e) ip->Baud = 9600;
			if (IsDlgButtonChecked(hWnd, IDR_SEVEN))
				ip->Bits = 7;
			else ip->Bits = 8;	/* Default */

			if (IsDlgButtonChecked(hWnd, IDR_ODD))
				ip->Parity = 1;
			else if (IsDlgButtonChecked(hWnd, IDR_EVEN))
				ip->Parity = 2;
			else ip->Parity = 0;	/* Default */

			if (IsDlgButtonChecked(hWnd, IDR_TWO))
				ip->Stop = 2;
			else ip->Stop = 1;	/* Default */

			EndDialog(hWnd, 1);
			return TRUE;
		}
		case IDCANCEL:
			EndDialog(hWnd, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static BOOL CALLBACK TcpPortDlgProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{static PORT_CONFIG_S *ip;

	switch (wMsg)
	{
	case WM_INITDIALOG:
	{	TCHAR Buffer[255];
		ip = (PORT_CONFIG_S *) lParam;

		SendDlgItemMessage(hWnd, IDE_IP_DNS, EM_LIMITTEXT, sizeof(ip->IPorDNS)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), ip->IPorDNS);
		SetDlgItemText(hWnd, IDE_IP_DNS, Buffer);

		SendDlgItemMessage(hWnd, IDE_PORT, EM_LIMITTEXT, 5, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%ld"), ip->TcpPort);
		SetDlgItemText(hWnd, IDE_PORT, Buffer);

		SendDlgItemMessage(hWnd, IDC_XMIT, BM_SETCHECK, ip->Transmit?BST_CHECKED:BST_UNCHECKED, 0);

		CenterWindow(hWnd);

		return TRUE;
	}
	case WM_COMMAND:

		if (!MakeFocusControlVisible(hWnd, wParam, lParam))
		switch (LOWORD(wParam))
		{
		case IDOK:
		{	char Temp[32], *e;

			myGetDlgItemText(hWnd, IDE_IP_DNS, ip->IPorDNS, sizeof(ip->IPorDNS));

			myGetDlgItemText(hWnd, IDE_PORT, Temp, sizeof(Temp));
			ip->TcpPort = strtol(Temp,&e,10);

			EndDialog(hWnd, 1);
			return TRUE;
		}
		case IDCANCEL:
			EndDialog(hWnd, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static BOOL CALLBACK BTPortDlgProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{static PORT_CONFIG_S *ip;

	switch (wMsg)
	{
	case WM_INITDIALOG:
	{	ip = (PORT_CONFIG_S *) lParam;

		{	TCHAR *Ports = GetBTDevices(NULL);	/* Get them ALL */
			LONG index = SendDlgItemMessage(hWnd, IDC_BTPORT, CB_ADDSTRING, 0, (LPARAM) TEXT(""));
			BOOL Found = FALSE;
			SendDlgItemMessage(hWnd, IDC_BTPORT, CB_SETCURSEL, index, 0);
			if (Ports)
			{	TCHAR *p;
				for (p=Ports; *p; p+=wcslen(p)+1)
				{	index = SendDlgItemMessage(hWnd, IDC_BTPORT, CB_ADDSTRING, 0, (LPARAM) p);
					if (!WideCmpMulti(p, ip->IPorDNS))
					{	SendDlgItemMessage(hWnd, IDC_BTPORT, CB_SETCURSEL, index, 0);
						Found = TRUE;
					}
				}
				free(Ports);
			}
			if (!Found && *ip->IPorDNS)
			{	TCHAR Temp[sizeof(ip->IPorDNS)+1];
				StringCbPrintf(Temp, sizeof(Temp), TEXT("%S"), ip->IPorDNS);
				index = SendDlgItemMessage(hWnd, IDC_BTPORT, CB_ADDSTRING, 0, (LPARAM) Temp);
				SendDlgItemMessage(hWnd, IDC_BTPORT, CB_SETCURSEL, index, 0);
			}
		}

		CenterWindow(hWnd);

		return TRUE;
	}
	case WM_COMMAND:

		if (!MakeFocusControlVisible(hWnd, wParam, lParam))
		switch (LOWORD(wParam))
		{
		case IDOK:
		{	myGetDlgItemText(hWnd, IDC_BTPORT, ip->IPorDNS, sizeof(ip->IPorDNS));
			EndDialog(hWnd, 1);
			return TRUE;
		}
		case IDCANCEL:
			EndDialog(hWnd, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static char *ConfigIPPort(HWND hdlg, char *Current)
{	PORT_CONFIG_S ip = {0};
	char *Result = NULL;

	Current = _strdup(Current);
	if (*Current == '@')	/* It's a TCP configuration now, fill in IP */
	{	char *c = strchr(Current,':');
		if (c)
		{	char *e;
			*c++ = '\0';
			ip.TcpPort = strtol(c,&e,10);
		}
		strncpy(ip.IPorDNS, Current+1, sizeof(ip.IPorDNS));
	}
	free(Current);

	if (DialogBoxParam(g_hInstance, TEXT("TCPPORT"), hdlg, TcpPortDlgProc, (LPARAM) &ip))
	{	Result = (char*)malloc(sizeof(ip.IPorDNS)+80);
		if (*ip.IPorDNS)
			sprintf(Result, "@%.*s:%ld", sizeof(ip.IPorDNS), ip.IPorDNS, (long) ip.TcpPort);
		else *Result = '\0';	/* No configuration, clear it out */
	}

	return Result;
}

static char *ConfigBTPort(HWND hdlg, char *Current)
{	PORT_CONFIG_S BT = {0};
	char *Result = NULL;

	Current = _strdup(Current);
	if (!strncmp(Current,"@BT:",4))
	{	//char *c = strrchr(Current+4,':');
		//if (c) *c='\0';	/* Drop address */
		strncpy(BT.IPorDNS, Current+4, sizeof(BT.IPorDNS));
	}
	free(Current);

	if (DialogBoxParam(g_hInstance, TEXT("BTPORT"), hdlg, BTPortDlgProc, (LPARAM) &BT))
	{	Result = (char*)malloc(sizeof(BT.IPorDNS)+80);
		if (*BT.IPorDNS)
			sprintf(Result, "@BT:%.*s", sizeof(BT.IPorDNS), BT.IPorDNS);
		else *Result = '\0';	/* No configuration, clear it out */
	}

	return Result;
}

static char *ConfigCommPort(HWND hdlg, char *Current)
{	PORT_CONFIG_S ip = {0};
	char *Result = NULL;

	Current = _strdup(Current);
	{	char *c = strchr(Current,':');	/* Have params after :? */
		if (c)
		{	char *p;
			*c++ = '\0';	/* Null terminate comm port name */
			if (*c)	/* More after the colon, parse the port parameters */
			{	char *Commas[10];	/* Hopefully not more than 10! */
				int CommaCount=0;
				Commas[CommaCount++] = c;
				for (p=c; *p; p++)
				{	if (*p == ',')
					{	if (CommaCount < sizeof(Commas)/sizeof(Commas[0]))
							Commas[CommaCount++] = p+1;
						*p = '\0';	/* Null terminate previous string */
					}
				}
				if (CommaCount > 0) ip.Baud = atol(Commas[0]);
				if (CommaCount > 1)
				{	switch (toupper(*Commas[1]))
					{
					case 'N': ip.Parity = 0; break;
					case 'O': ip.Parity = 1; break;
					case 'E': ip.Parity = 2; break;
					default:
						printf("Invalid Parity Setting '%s'\n", Commas[1]);
					}
				}
				if (CommaCount > 2) ip.Bits = (BYTE) atol(Commas[2]);
				if (CommaCount > 3) ip.Stop = (BYTE) atol(Commas[3]);
			}
		}
		strncpy(ip.Port, Current, sizeof(ip.Port));
	}
	free(Current);

	if (DialogBoxParam(g_hInstance, TEXT("COMMPORT"), hdlg, CommPortDlgProc, (LPARAM) &ip))
	{	Result = (char*)malloc(sizeof(ip.Port)+80);
		if (*ip.Port)
		{	sprintf(Result,"%.*s:%ld,",sizeof(ip.Port), ip.Port, (long) ip.Baud);
			switch (ip.Parity)
			{
			case 1: strcat(Result,"O,"); break;
			case 2: strcat(Result,"E,"); break;
			default: strcat(Result,"N,");
			}
			sprintf(Result+strlen(Result),"%ld,%ld", (long) ip.Bits, (long) ip.Stop);
		} else *Result = '\0';
	}

	return Result;
}

char *ConfigCommOrIpPort(HWND hdlg, char *Current)
{
	if (!*Current)
	{	BUTTONS_S *Buttons = CreateButtons(-1);
		AddButton(Buttons, "TCP/IP", 1);
		AddButton(Buttons, "Bluetooth", 2);
		AddButton(Buttons, "COMn Serial", 3);
		switch (LwdMessageBox2(hdlg, TEXT("Select Port Type"), TEXT("Port Type"), MB_ICONQUESTION | MB_DEFBUTTON3, Buttons))
		{
		case 1: return ConfigIPPort(hdlg,Current);
		case 2: return ConfigBTPort(hdlg,Current);
		case 3: return ConfigCommPort(hdlg,Current);
		default:
			return NULL;
		}
	} else if (*Current == '@')
	{	if (!strncmp(Current,"@BT:",4))
			return ConfigBTPort(hdlg,Current);
		else return ConfigIPPort(hdlg,Current);
	} else return ConfigCommPort(hdlg,Current);
}

BOOL PromptGenius2(HWND hwnd, GENIUS_INFO_S *pGenius)
{	GENIUS_INFO_S Working = *pGenius;
	BOOL Result;

	if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_GENIUS_BEACON), hwnd, GeniusDlgProc, (LPARAM)&Working) == IDOK)
	{	*pGenius = Working;
		Result = TRUE;
	} else Result = FALSE;
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

BOOL PromptGenius(HWND hwnd, CONFIG_INFO_S *pConfig)
{	BOOL Result;

	if (PromptGenius2(hwnd, &pConfig->MyGenius))
	{	ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:Genius");
		Result = TRUE;
	} else Result = FALSE;
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

BOOL CALLBACK PortConfigDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static PORT_CONFIG_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (PORT_CONFIG_INFO_S *) lp;
		TCHAR Buffer[255];

		/* Temporary hack */
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S(%S)"), Info->Name, Info->Protocol);
		SetWindowText(hdlg, Buffer);

		Info->Inherit = TRUE;	/* Temporarily */
		EnableWindow(GetDlgItem(hdlg, IDC_INHERIT), FALSE);	/* Temporarily */

		CheckDlgButton(hdlg, IDC_INHERIT, Info->Inherit);
		if (Info->Inherit)
		{	EnableWindow(GetDlgItem(hdlg, IDE_CALLSIGN), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_SYMBOL), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDE_COMMENT), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_GENIUS), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_BEACON_PATH), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_TELEMETRY_PATH), FALSE);
		}

		CheckDlgButton(hdlg, IDC_RFTOIS, Info->RFtoISEnabled);
		CheckDlgButton(hdlg, IDC_ISTORF, Info->IStoRFEnabled);
		CheckDlgButton(hdlg, IDC_NOGATEME, Info->NoGateME);

		CheckDlgButton(hdlg, IDC_ENABLED, Info->IsEnabled);
		CheckDlgButton(hdlg, IDC_XMIT, Info->XmitEnabled);
		CheckDlgButton(hdlg, IDC_NMEA, Info->ProvidesNMEA);
		CheckDlgButton(hdlg, IDC_BEACON, Info->BeaconingEnabled);
		CheckDlgButton(hdlg, IDC_DX2, Info->DXEnabled);
		CheckDlgButton(hdlg, IDC_MESSAGE, Info->MessagesEnabled);
		CheckDlgButton(hdlg, IDC_BULLETIN_OBJECT, Info->BulletinObjectEnabled);
		CheckDlgButton(hdlg, IDC_TELEMETRY, Info->TelemetryEnabled);
		
		SendDlgItemMessage(hdlg, IDE_CALLSIGN, EM_LIMITTEXT, min(9,sizeof(Info->CallSign)-1), 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->CallSign);
		SetDlgItemText(hdlg, IDE_CALLSIGN, Buffer);

		SendDlgItemMessage(hdlg, IDE_COMMENT, EM_LIMITTEXT, sizeof(Info->Comment)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Comment);
		SetDlgItemText(hdlg, IDE_COMMENT, Buffer);

		if (HWND hwndList = GetDlgItem(hdlg, IDL_RFBAUD))
		{	SendMessage(hwndList, CB_LIMITTEXT, 5, 0);
			SendMessage(hwndList, CB_INSERTSTRING, 0, (LPARAM) TEXT("Other"));
			SendMessage(hwndList, CB_INSERTSTRING, 0, (LPARAM) TEXT("9600"));
			SendMessage(hwndList, CB_INSERTSTRING, 0, (LPARAM) TEXT("300"));
			SendMessage(hwndList, CB_INSERTSTRING, 0, (LPARAM) TEXT("1200"));
			SendMessage(hwndList, CB_SETCURSEL, 0, 0);	/* Select first entry in the list */
		}

		SetDlgItemInt(hdlg, IDE_QUIET_TIME, Info->QuietTime, FALSE);
		SendDlgItemMessage(hdlg, IDS_QUIET_TIME, UDM_SETRANGE, 0, GetSpinRangeULong("QuietTime"));

		if (!*Info->Device)
		{	char *Result = ConfigCommOrIpPort(hdlg, Info->Device);
			if (Result)
			{	strncpy(Info->Device, Result, sizeof(Info->Device));
				free(Result);
			}
		}

		EnableWindow(GetDlgItem(hdlg, IDC_ENABLED), *Info->Device != 0);

		EnableWindow(GetDlgItem(hdlg, IDB_DELETE), !Info->IsEnabled);

		CenterWindow(hdlg);

		return TRUE;
	}
	case WM_DRAWITEM:
	if (wp == IDB_SYMBOL)
	{	RECT rc;
		POINT pt;
		DRAWITEMSTRUCT *di = (LPDRAWITEMSTRUCT) lp;
		pt.x = (di->rcItem.left+di->rcItem.right)/2;
		pt.y = (di->rcItem.bottom+di->rcItem.top)/2;
		Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
		BltSymbol(di->hDC, pt.x, pt.y, 2, 1,
					Info->Symbol.Table=='/'?0:Info->Symbol.Table=='\\'?0x1:((Info->Symbol.Table<<8)|0x1),
					Info->Symbol.Symbol-'!', 100, &rc);
	}
	break;

	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE Port ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_DELETE:
			if (MessageBox(hdlg, TEXT("Really Delete Port?"), TEXT("Rf Port Config"), MB_ICONQUESTION | MB_YESNO) == IDYES)
				EndDialog(hdlg, IDB_DELETE);
			return TRUE;
		case IDB_ACCEPT:
		{
			Info->Inherit = IsDlgButtonChecked(hdlg, IDC_INHERIT);
			if (!Info->Inherit)
			{	myGetDlgItemText(hdlg, IDE_CALLSIGN, Info->CallSign, sizeof(Info->CallSign)-1);
				SpaceCompress(STRING(Info->CallSign));
				ZeroSSID(Info->CallSign);

				myGetDlgItemText(hdlg, IDE_COMMENT, Info->Comment, sizeof(Info->Comment));
			}

			Info->QuietTime = GetDlgItemInt(hdlg, IDE_QUIET_TIME, NULL, FALSE);

			Info->RFtoISEnabled = IsDlgButtonChecked(hdlg, IDC_RFTOIS);
			Info->IStoRFEnabled = IsDlgButtonChecked(hdlg, IDC_ISTORF);
			Info->NoGateME = IsDlgButtonChecked(hdlg, IDC_NOGATEME);

			Info->IsEnabled = IsDlgButtonChecked(hdlg, IDC_ENABLED);
			Info->XmitEnabled = IsDlgButtonChecked(hdlg, IDC_XMIT);
			Info->ProvidesNMEA = IsDlgButtonChecked(hdlg, IDC_NMEA);
			Info->BeaconingEnabled = IsDlgButtonChecked(hdlg, IDC_BEACON);
			Info->DXEnabled = IsDlgButtonChecked(hdlg, IDC_DX2);
			Info->MessagesEnabled = IsDlgButtonChecked(hdlg, IDC_MESSAGE);
			Info->BulletinObjectEnabled = IsDlgButtonChecked(hdlg, IDC_BULLETIN_OBJECT);
			Info->TelemetryEnabled = IsDlgButtonChecked(hdlg, IDC_TELEMETRY);

			EndDialog(hdlg, IDOK);
			return TRUE;
		}
		case IDB_DEVICE:
		{	char *New = ConfigCommOrIpPort(hdlg, Info->Device);
			if (New)
			{	strncpy(Info->Device, New, sizeof(Info->Device));
			}
			return TRUE;
		}
		case IDB_GENIUS:
			PromptGenius2(hdlg, &Info->Genius);
			return TRUE;
		case IDB_BEACON_PATH:
			return TRUE;
		case IDB_TELEMETRY_PATH:
			return TRUE;

		case IDB_SYMBOL:
		{	SYMBOL_INFO_S Working = Info->Symbol;
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SYMBOL), hdlg, SymbolDlgProc, (LPARAM)&Working) == IDOK)
			{	Info->Symbol = Working;
				InvalidateRect(hdlg,NULL,FALSE);
			}
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

BOOL CALLBACK NewPortDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static	BOOL EntryChanged;
static	TCHAR EnterText[] = TEXT("Enter Name Here");
static	PORT_CONFIG_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (PORT_CONFIG_INFO_S *) lp;

		if (HWND hwndList = GetDlgItem(hdlg, IDL_TYPE))
		{	SendMessage(hwndList, CB_LIMITTEXT, sizeof(Info->Protocol), 0);
			AddPortProtocols(hwndList);
			SendMessage(hwndList, CB_SETCURSEL, 0, 0);	/* Select first entry in the list */
		}
		else MessageBox(hdlg, TEXT("IDL_TYPE Not Found!"), TEXT("NewPortDlgProc"), MB_OK | MB_ICONERROR);

		SendDlgItemMessage(hdlg, IDE_TEXT, EM_LIMITTEXT, sizeof(Info->Name), 0);
		SetDlgItemText(hdlg, IDE_TEXT, EnterText);
		SetFocus(GetDlgItem(hdlg, IDE_TEXT));
		SendDlgItemMessage(hdlg, IDE_TEXT, EM_SETSEL, 0, -1);
#ifdef USING_SIP
		SipShowIM(SIPF_ON);	/* Bring up the keyboard for message typing */
#endif
		EntryChanged = FALSE;

		CenterWindow(hdlg);

		return FALSE;	/* I already set focus */
	}

	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_CREATE:
		{	TCHAR *Name = (TCHAR*)malloc(sizeof(*Name)*128), *Type = (TCHAR*)malloc(sizeof(*Type)*80);
			if (EntryChanged)
			if (GetDlgItemText(hdlg, IDE_TEXT, Name, 128))
			if (GetDlgItemText(hdlg, IDL_TYPE, Type, 80))
			{	if (SUCCEEDED(StringCbPrintfA(Info->Name, sizeof(Info->Name), "%S", Name))
				&& SUCCEEDED(StringCbPrintfA(Info->Protocol, sizeof(Info->Protocol), "%S", Type)))
				{	Info->RfBaud = 1200;
					EndDialog(hdlg, IDOK);
				} else MessageBox(hdlg, TEXT("Name/Protocol Format Failed"), TEXT("IDB_CREATE"), MB_OK | MB_ICONERROR);
			}
			free(Name); free(Type);
			return TRUE;
		}
		case IDE_TEXT:
		{	TCHAR *Text = (TCHAR*)malloc(sizeof(*Text)*80);
			switch (HIWORD(wp))
			{
			case EN_CHANGE:
				EntryChanged = GetDlgItemText(hdlg, IDE_TEXT, Text, 80) > 0;
				break;
			case EN_KILLFOCUS:
				if (GetDlgItemText(hdlg, IDE_TEXT, Text, 80))
					if (!memcmp(Text, EnterText, sizeof(EnterText)))
						SetDlgItemText(hdlg, IDE_TEXT, TEXT(""));
			}
			free(Text);
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

BOOL PromptConfigRFPort(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id)
{	PORT_CONFIG_INFO_S Working = pConfig->RFPorts.Port[id];
	BOOL Result = FALSE;
	INT_PTR idEnd;

	if ((idEnd=DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_PORT_CONFIG), hwnd, PortConfigDlgProc, (LPARAM)&Working)) == IDOK)
	{	pConfig->RFPorts.Port[id] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:PortConfig");
		Result = TRUE;
	} else if (idEnd == IDB_DELETE)
	{	if (id < --pConfig->RFPorts.Count)
			memmove(&pConfig->RFPorts.Port[id], &pConfig->RFPorts.Port[id+1],
					sizeof(pConfig->RFPorts.Port[0])*(pConfig->RFPorts.Count-id));
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

int PromptNewRFPort(HWND hwnd, CONFIG_INFO_S *pConfig)
{	PORT_CONFIG_INFO_S Working = {0};
	int Result;

	if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_NEW_RFPORT), hwnd, NewPortDlgProc, (LPARAM)&Working) == IDOK)
	{	unsigned long i = pConfig->RFPorts.Count++;
		pConfig->RFPorts.Port = (PORT_CONFIG_INFO_S *)realloc(pConfig->RFPorts.Port,sizeof(*pConfig->RFPorts.Port)*pConfig->RFPorts.Count);

		Working.IsEnabled = TRUE;
		Working.XmitEnabled = TRUE;
		Working.RFtoISEnabled = TRUE;
		Working.IStoRFEnabled = TRUE;
		Working.BeaconingEnabled = TRUE;

		Working.Inherit = TRUE;
		Working.Genius = pConfig->MyGenius;
		strncpy(Working.CallSign, pConfig->CallSign, sizeof(Working.CallSign));
		strncpy(Working.Comment, pConfig->Comment, sizeof(Working.Comment));
		Working.Symbol = pConfig->Symbol;

		pConfig->RFPorts.Port[i] = Working;
		PromptConfigRFPort(hwnd, pConfig, i);
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:NewPort");
		Result = i;
	} else Result = -1;
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

#ifdef OBSOLETE
BOOL PromptAPRSIS(HWND hwnd, CONFIG_INFO_S *pConfig)
{	char *Result = ConfigIPPort(hwnd, pConfig->APRSIS.Port);
	if (Result)
	{	strncpy(pConfig->APRSIS.Port, Result, sizeof(pConfig->APRSIS.Port));
		SaveConfiguration(hwnd, pConfig, "User:APRS-IS");
		free(Result);
		return TRUE;
	}
	return FALSE;
}
#endif

BOOL CALLBACK APRSISConfigDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static CONFIG_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (CONFIG_INFO_S *) lp;
		TCHAR Buffer[255];

		{	EnableWindow(GetDlgItem(hdlg, IDE_CALLSIGN), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_SYMBOL), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDE_COMMENT), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_GENIUS), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_BEACON_PATH), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_TELEMETRY_PATH), FALSE);
		}

		CheckDlgButton(hdlg, IDC_RFTOIS, Info->APRSIS.RFtoISEnabled);
		CheckDlgButton(hdlg, IDC_ISTORF, Info->APRSIS.IStoRFEnabled);

		CheckDlgButton(hdlg, IDC_ENABLED, Info->Enables.APRSIS);
		CheckDlgButton(hdlg, IDC_XMIT, Info->APRSIS.XmitEnabled);
		CheckDlgButton(hdlg, IDC_BEACON, Info->APRSIS.BeaconingEnabled);
		CheckDlgButton(hdlg, IDC_MESSAGE, Info->APRSIS.MessagesEnabled);
		CheckDlgButton(hdlg, IDC_BULLETIN_OBJECT, Info->APRSIS.BulletinObjectEnabled);
		CheckDlgButton(hdlg, IDC_TELEMETRY, Info->APRSIS.TelemetryEnabled);
		
		SendDlgItemMessage(hdlg, IDE_CALLSIGN, EM_LIMITTEXT, min(9,sizeof(Info->CallSign)-1), 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->CallSign);
		SetDlgItemText(hdlg, IDE_CALLSIGN, Buffer);

		SendDlgItemMessage(hdlg, IDE_COMMENT, EM_LIMITTEXT, sizeof(Info->Comment)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Comment);
		SetDlgItemText(hdlg, IDE_COMMENT, Buffer);

		SetDlgItemInt(hdlg, IDE_QUIET_TIME, Info->QuietTime, FALSE);
		SendDlgItemMessage(hdlg, IDS_QUIET_TIME, UDM_SETRANGE, 0, GetSpinRangeULong("QuietTime"));

		if (!*Info->APRSIS.Port)
		{	char *Result = ConfigIPPort(hdlg, Info->APRSIS.Port);
			if (Result)
			{	strncpy(Info->APRSIS.Port, Result, sizeof(Info->APRSIS.Port));
				free(Result);
			}
		}

		EnableWindow(GetDlgItem(hdlg, IDC_ENABLED), *Info->APRSIS.Port != 0);

		CenterWindow(hdlg);

		return TRUE;
	}
	case WM_DRAWITEM:
	if (wp == IDB_SYMBOL)
	{	RECT rc;
		POINT pt;
		DRAWITEMSTRUCT *di = (LPDRAWITEMSTRUCT) lp;
		pt.x = (di->rcItem.left+di->rcItem.right)/2;
		pt.y = (di->rcItem.bottom+di->rcItem.top)/2;
		Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
		BltSymbol(di->hDC, pt.x, pt.y, 2, 1,
					Info->Symbol.Table=='/'?0:Info->Symbol.Table=='\\'?0x1:((Info->Symbol.Table<<8)|0x1),
					Info->Symbol.Symbol-'!', 100, &rc);
	}
	break;

	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE APRSIS ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_ACCEPT:
		{
			{	myGetDlgItemText(hdlg, IDE_CALLSIGN, Info->CallSign, sizeof(Info->CallSign));
				SpaceCompress(STRING(Info->CallSign));
				ZeroSSID(Info->CallSign);

				myGetDlgItemText(hdlg, IDE_COMMENT, Info->Comment, sizeof(Info->Comment));
			}

			Info->QuietTime = GetDlgItemInt(hdlg, IDE_QUIET_TIME, NULL, FALSE);

			Info->APRSIS.RFtoISEnabled = IsDlgButtonChecked(hdlg, IDC_RFTOIS);
			Info->APRSIS.IStoRFEnabled = IsDlgButtonChecked(hdlg, IDC_ISTORF);

			Info->Enables.APRSIS = IsDlgButtonChecked(hdlg, IDC_ENABLED);
			Info->APRSIS.XmitEnabled = IsDlgButtonChecked(hdlg, IDC_XMIT);
			Info->APRSIS.BeaconingEnabled = IsDlgButtonChecked(hdlg, IDC_BEACON);
			Info->APRSIS.MessagesEnabled = IsDlgButtonChecked(hdlg, IDC_MESSAGE);
			Info->APRSIS.BulletinObjectEnabled = IsDlgButtonChecked(hdlg, IDC_BULLETIN_OBJECT);
			Info->APRSIS.TelemetryEnabled = IsDlgButtonChecked(hdlg, IDC_TELEMETRY);

			EndDialog(hdlg, IDOK);
			return TRUE;
		}
		case IDB_DEVICE:
		{	char *New = ConfigIPPort(hdlg, Info->APRSIS.Port);
			if (New)
			{	strncpy(Info->APRSIS.Port, New, sizeof(Info->APRSIS.Port));
			}
			return TRUE;
		}
		case IDB_GENIUS:
			PromptGenius2(hdlg, &Info->MyGenius);
			return TRUE;
		case IDB_BEACON_PATH:
			return TRUE;
		case IDB_TELEMETRY_PATH:
			return TRUE;

		case IDB_SYMBOL:
		{	SYMBOL_INFO_S Working = Info->Symbol;
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SYMBOL), hdlg, SymbolDlgProc, (LPARAM)&Working) == IDOK)
			{	Info->Symbol = Working;
				InvalidateRect(hdlg,NULL,FALSE);
			}
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

BOOL PromptConfigAPRSIS(HWND hwnd, CONFIG_INFO_S *pConfig)
{	CONFIG_INFO_S Working = *pConfig;
	BOOL Result = FALSE;
	INT_PTR idEnd;

	if ((idEnd=DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_APRSIS_CONFIG), hwnd, APRSISConfigDlgProc, (LPARAM)&Working)) == IDOK)
	{	pConfig->APRSIS = Working.APRSIS;

		strncpy(pConfig->CallSign, Working.CallSign, sizeof(pConfig->CallSign));
		strncpy(pConfig->Comment, Working.Comment, sizeof(pConfig->Comment));

		pConfig->QuietTime = Working.QuietTime;
		pConfig->Enables.APRSIS = Working.Enables.APRSIS;

		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:APRSISConfig");
		Result = TRUE;
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

BOOL PromptNMEA(HWND hwnd, CONFIG_INFO_S *pConfig)
{	char *Result = ConfigCommOrIpPort(hwnd, pConfig->GPSPort);
	if (Result)
	{	strncpy(pConfig->GPSPort, Result, sizeof(pConfig->GPSPort));
		RealSaveConfiguration(hwnd, pConfig, "User:NMEA");
		free(Result);
		return TRUE;
	}
	return FALSE;
}

BOOL CALLBACK BulletinDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static BULLETIN_CONFIG_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (BULLETIN_CONFIG_INFO_S *) lp;
		TCHAR Buffer[255];

		CheckDlgButton(hdlg, IDC_VIA_IS, Info->ISEnabled);
		CheckDlgButton(hdlg, IDC_VIA_RF, Info->RFEnabled);

		CheckDlgButton(hdlg, IDC_ENABLED, Info->Enabled && Info->Interval!=0);

		SendDlgItemMessage(hdlg, IDE_COMMENT, EM_LIMITTEXT, sizeof(Info->Comment)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Comment);
		SetDlgItemText(hdlg, IDE_COMMENT, Buffer);

		SendDlgItemMessage(hdlg, IDE_RF_PATH, EM_LIMITTEXT, sizeof(Info->RFPath)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->RFPath);
		SetDlgItemText(hdlg, IDE_RF_PATH, Buffer);

		if (HWND hwndList = GetDlgItem(hdlg, IDL_LINE))
		{	if (Info->New)
			{	TCHAR Line[2] = {0};
				SendMessage(hwndList, CB_LIMITTEXT, 1, 0);
				for (Line[0]=*TEXT("0"); Line[0]<=*TEXT("9"); Line[0]++)
					SendMessage(hwndList, CB_INSERTSTRING, -1, (LPARAM) Line);
				for (Line[0]=*TEXT("A"); Line[0]<=*TEXT("Z"); Line[0]++)
					SendMessage(hwndList, CB_INSERTSTRING, -1, (LPARAM) Line);
			} else
			{	TCHAR Line[2];
				StringCbPrintf(Line, sizeof(Line), TEXT("%.1S"), &Info->Name[3]);
				SendMessage(hwndList, CB_INSERTSTRING, 0, (LPARAM) Line);
				SendMessage(hwndList, CB_SETCURSEL, 0, 0);	/* Select first entry in the list */
				EnableWindow(hwndList, FALSE);	/* Disabled */
			}
			EnableWindow(GetDlgItem(hdlg, IDL_GROUP), FALSE);	/* Disabled for now */
		}
		EnableWindow(GetDlgItem(hdlg, IDL_GROUP), FALSE);	/* Disabled for now */

		SetDlgItemInt(hdlg, IDE_INTERVAL, Info->Interval, FALSE);
		SendDlgItemMessage(hdlg, IDS_INTERVAL, UDM_SETRANGE, 0, GetSpinRangeULong("Bulletin.Interval"));

		CenterWindow(hdlg);

		return TRUE;
	}
	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE Bulletin ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_DELETE:
			if (MessageBox(hdlg, TEXT("Really Delete Bulletin?"), TEXT("Bulletin Config"), MB_ICONQUESTION | MB_YESNO) == IDYES)
				EndDialog(hdlg, IDB_DELETE);
			return TRUE;
		case IDB_ACCEPT:
		{	TCHAR *Line = (TCHAR*)malloc(sizeof(*Line)*32);
			TCHAR *Group = (TCHAR*)malloc(sizeof(*Group)*32);
			TCHAR *Comment = (TCHAR*)malloc(sizeof(*Comment)*128);
			TCHAR *RFPath = (TCHAR*)malloc(sizeof(*RFPath)*128);

			Info->Interval = GetDlgItemInt(hdlg, IDE_INTERVAL, NULL, FALSE);
			Info->Enabled = IsDlgButtonChecked(hdlg, IDC_ENABLED);
			Info->ISEnabled = IsDlgButtonChecked(hdlg, IDC_VIA_IS);
			Info->RFEnabled = IsDlgButtonChecked(hdlg, IDC_VIA_RF);

			*Group = *TEXT("");
			GetDlgItemText(hdlg, IDL_GROUP, Group, 32);
			GetDlgItemText(hdlg, IDE_RF_PATH, RFPath, 128);

			if ((GetDlgItemText(hdlg, IDL_LINE, Line, 32))
			&& (GetDlgItemText(hdlg, IDE_COMMENT, Comment, 128)))
			{	if (SUCCEEDED(StringCbPrintfA(Info->Name, sizeof(Info->Name), "BLN%S%S", Line, Group))
				&& SUCCEEDED(StringCbPrintfA(Info->Comment, sizeof(Info->Comment), "%S", Comment))
				&& SUCCEEDED(StringCbPrintfA(Info->RFPath, sizeof(Info->RFPath), "%S", RFPath)))
				{	EndDialog(hdlg, IDOK);
				} else MessageBox(hdlg, TEXT("Name/Comment/RFPath Format Failed"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			} else MessageBox(hdlg, TEXT("ID and Comment Required"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			free(Line); free(Group); free(Comment); free(RFPath);
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

BOOL PromptConfigBulletin(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id)
{	BULLETIN_CONFIG_INFO_S Working = pConfig->Bulletins.Bull[id];
	BOOL Result = FALSE;
	INT_PTR idEnd;

	Working.New = FALSE;

	if ((idEnd=DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_BULLETIN), hwnd, BulletinDlgProc, (LPARAM)&Working)) == IDOK)
	{
		memset(&Working.LastTransmit, 0, sizeof(Working.LastTransmit));
		pConfig->Bulletins.Bull[id] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:Bulletin");
		Result = TRUE;
	} else if (idEnd == IDB_DELETE)
	{	if (id < --pConfig->Bulletins.Count)
			memmove(&pConfig->Bulletins.Bull[id], &pConfig->Bulletins.Bull[id+1],
					sizeof(pConfig->Bulletins.Bull[0])*(pConfig->Bulletins.Count-id));
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

BOOL PromptNewBulletin(HWND hwnd, CONFIG_INFO_S *pConfig)
{	BULLETIN_CONFIG_INFO_S Working = {0};
	BOOL Result;

	Working.New = TRUE;
	strncpy(Working.RFPath, pConfig->BeaconPath, sizeof(Working.RFPath));
	if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_BULLETIN), hwnd, BulletinDlgProc, (LPARAM)&Working) == IDOK)
	{	unsigned long i = pConfig->Bulletins.Count++;
		pConfig->Bulletins.Bull = (BULLETIN_CONFIG_INFO_S *)realloc(pConfig->Bulletins.Bull,sizeof(*pConfig->Bulletins.Bull)*pConfig->Bulletins.Count);

		pConfig->Bulletins.Bull[i] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:NewBulletin");
		Result = TRUE;
	} else Result = FALSE;
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

BOOL CALLBACK CQGroupDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static CQSRVR_GROUP_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (CQSRVR_GROUP_INFO_S *) lp;
		TCHAR Buffer[255];

		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("CQ %S"), Info->Name);
		SetWindowText(hdlg, Buffer);

		CheckDlgButton(hdlg, IDC_VIA_IS, Info->ISOnly);
		CheckDlgButton(hdlg, IDC_PRESENT, Info->IfPresent);
		CheckDlgButton(hdlg, IDC_ENABLED, Info->KeepAlive && Info->Interval!=0);
		CheckDlgButton(hdlg, IDC_MONITOR, Info->QuietMonitor);
		CheckDlgButton(hdlg, IDC_VIA_CQSRVR, Info->ViaCQSRVR);

//#ifdef UNDER_CE
		CheckDlgButton(hdlg, IDC_PRESENT, FALSE);
		EnableWindow(GetDlgItem(hdlg, IDC_PRESENT), FALSE);	/* Disabled for now */
//#endif

		SendDlgItemMessage(hdlg, IDE_COMMENT, EM_LIMITTEXT, sizeof(Info->Comment)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Comment);
		SetDlgItemText(hdlg, IDE_COMMENT, Buffer);

		SetDlgItemInt(hdlg, IDE_INTERVAL, Info->Interval, FALSE);
		SendDlgItemMessage(hdlg, IDS_INTERVAL, UDM_SETRANGE, 0, GetSpinRangeULong("CQGroup.Interval"));

		SendMessage(hdlg, WM_COMMAND, MAKELONG(IDC_VIA_CQSRVR,BN_CLICKED),
					(LPARAM) GetDlgItem(hdlg,IDC_VIA_CQSRVR));	/* Fix up enables */
		SendMessage(hdlg, WM_COMMAND, MAKELONG(IDE_COMMENT,EN_CHANGE),
					(LPARAM) GetDlgItem(hdlg,IDE_COMMENT));	/* Fix up enables */
		SendMessage(hdlg, WM_COMMAND, MAKELONG(IDC_MONITOR,BN_CLICKED),
					(LPARAM) GetDlgItem(hdlg,IDC_MONITOR));	/* Fix up enables */

		CenterWindow(hdlg);

		return TRUE;
	}
	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE CQGroup ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDC_VIA_CQSRVR:
			Info->ViaCQSRVR = IsDlgButtonChecked(hdlg, IDC_VIA_CQSRVR);
			EnableWindow(GetDlgItem(hdlg, IDC_MONITOR), !Info->ViaCQSRVR);
			if (Info->ViaCQSRVR)
			{	CheckDlgButton(hdlg, IDC_MONITOR, FALSE);
			} else CheckDlgButton(hdlg, IDC_MONITOR, Info->QuietMonitor);
		case IDE_COMMENT:	/* Fall in here to check comment length */
			EnableWindow(GetDlgItem(hdlg, IDB_SEND), SendDlgItemMessage(hdlg, IDE_COMMENT, WM_GETTEXTLENGTH, 0, 0)>0);
		case IDC_MONITOR:	/* Fall in here from COMMENT to set Accept button also */
			Info->QuietMonitor = IsDlgButtonChecked(hdlg, IDC_MONITOR);
			if (Info->QuietMonitor
			|| SendDlgItemMessage(hdlg, IDE_COMMENT, WM_GETTEXTLENGTH, 0, 0))
				EnableWindow(GetDlgItem(hdlg, IDB_ACCEPT), TRUE);
			else EnableWindow(GetDlgItem(hdlg, IDB_ACCEPT), FALSE);
			break;
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_DELETE:
			if (MessageBox(hdlg, TEXT("Really Delete AN/CQSRVR Group?"), TEXT("CQSRVR Config"), MB_ICONQUESTION | MB_YESNO) == IDYES)
				EndDialog(hdlg, IDB_DELETE);
			return TRUE;
		case IDB_SEND:
			Info->OneShot = TRUE;
		case IDB_ACCEPT:	/* Send falls in here on purpose */
		{	TCHAR *Comment = (TCHAR*)malloc(sizeof(*Comment)*128);

			Info->Interval = GetDlgItemInt(hdlg, IDE_INTERVAL, NULL, FALSE);
			Info->KeepAlive = IsDlgButtonChecked(hdlg, IDC_ENABLED);
			Info->QuietMonitor = IsDlgButtonChecked(hdlg, IDC_MONITOR);
			Info->ViaCQSRVR = IsDlgButtonChecked(hdlg, IDC_VIA_CQSRVR);
			Info->ISOnly = IsDlgButtonChecked(hdlg, IDC_VIA_IS);
			Info->IfPresent = IsDlgButtonChecked(hdlg, IDC_PRESENT);

			if (GetDlgItemText(hdlg, IDE_COMMENT, Comment, 128) || Info->QuietMonitor)
			{	if (SUCCEEDED(StringCbPrintfA(Info->Comment, sizeof(Info->Comment), "%S", Comment)))
				{	EndDialog(hdlg, IDOK);
				} else MessageBox(hdlg, TEXT("Name/Comment Format Failed"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			} else MessageBox(hdlg, TEXT("Comment Required"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			free(Comment);
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

BOOL PromptConfigCQGroup(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id)
{	CQSRVR_GROUP_INFO_S Working = pConfig->CQGroups.CQGroup[id];
	BOOL Result = FALSE;
	INT_PTR idEnd;

	if ((idEnd=DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_CQSRVR), hwnd, CQGroupDlgProc, (LPARAM)&Working)) == IDOK)
	{	if (!Working.Interval && pConfig->CQGroups.CQGroup[id].Interval)
			Working.UnJoin = TRUE;	/* Just disabled, fire an UnJoin */
		if (!Working.KeepAlive && pConfig->CQGroups.CQGroup[id].KeepAlive)
			Working.UnJoin = TRUE;	/* Just disabled, fire an UnJoin */
		pConfig->CQGroups.CQGroup[id] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:CQGroup");
		memset(&pConfig->CQGroups.CQGroup[id].LastTransmit, 0,
				sizeof(pConfig->CQGroups.CQGroup[id].LastTransmit));
		Result = TRUE;	/* Trigger a transmission poll */
	} else if (idEnd == IDB_DELETE)
	{	pConfig->CQGroups.CQGroup[id].PendingDelete = TRUE;
		memset(&pConfig->CQGroups.CQGroup[id].LastTransmit, 0,
				sizeof(pConfig->CQGroups.CQGroup[id].LastTransmit));
		Result = TRUE;	/* Trigger a transmission poll */
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

BOOL PromptNewCQGroup(HWND hwnd, CONFIG_INFO_S *pConfig)
{	CQSRVR_GROUP_INFO_S Working = {0};
	char *CQGroup;
	BOOL Result = FALSE;

	do
	{	CQGroup = StringPromptA(hwnd, "AN/CQSRVR Group", "Group:", sizeof(Working.Name)-1, Working.Name, TRUE, FALSE);
		if (!CQGroup) return NULL;
		RtStrnuprTrim(-1,CQGroup);
		if (!*CQGroup)
			MessageBox(hwnd, TEXT("Group Must Be Non-Blank"), TEXT("AN/CQSRVR Group"), MB_OK | MB_ICONWARNING);
		else if (strchr(CQGroup,' '))
			MessageBox(hwnd, TEXT("Group Cannot Contain Spaces"), TEXT("AN/CQSRVR Group"), MB_OK | MB_ICONWARNING);
		else Result = TRUE;
	} while (!Result);

	strncpy(Working.Name, CQGroup, sizeof(Working.Name));
	free(CQGroup);
	Working.ISOnly = TRUE;
	Working.Interval = 8;
	Working.IfPresent = TRUE;
	Working.QuietMonitor = TRUE;
	if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_CQSRVR), hwnd, CQGroupDlgProc, (LPARAM)&Working) == IDOK)
	{	unsigned long i = pConfig->CQGroups.Count++;
		pConfig->CQGroups.CQGroup = (CQSRVR_GROUP_INFO_S *)realloc(pConfig->CQGroups.CQGroup,sizeof(*pConfig->CQGroups.CQGroup)*pConfig->CQGroups.Count);

		pConfig->CQGroups.CQGroup[i] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:NewCQGroup");
		Result = TRUE;
	} else Result = FALSE;
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

static void PopulateListBox(HWND hwndList, TCHAR **Values)
{	TCHAR **p;
	for (p=Values; *p; p++)
	{	SendMessage(hwndList, CB_INSERTSTRING, -1, (LPARAM)*p);
	}
}

BOOL CALLBACK DFDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static char **pResult;
static TCHAR *Ranges[] = { TEXT("1"), TEXT("2"), TEXT("4"), TEXT("8"), 
							TEXT("16"), TEXT("32"), TEXT("64"), TEXT("128"), 
							TEXT("256"), TEXT("512"), NULL };
static TCHAR *Qualities[] = { TEXT("Useless"), TEXT("<240"), TEXT("<120"),
							TEXT("<64"), TEXT("<32"), TEXT("<16"),
							TEXT("<8"), TEXT("<4"), TEXT("<2"),
							TEXT("<1"), NULL };
static TCHAR *Ns[] = { TEXT("None"), TEXT("1"), TEXT("2"),
							TEXT("3"), TEXT("4"), TEXT("5"),
							TEXT("6"), TEXT("7"), TEXT("8"),
							TEXT("Man"), NULL };

	switch (msg)
	{
	case WM_INITDIALOG:
	{	pResult = (char **) lp;

		PopulateListBox(GetDlgItem(hdlg,IDL_RANGE), Ranges);
		PopulateListBox(GetDlgItem(hdlg,IDL_QUALITY), Qualities);
		PopulateListBox(GetDlgItem(hdlg,IDL_N), Ns);

		SetDlgItemInt(hdlg, IDE_BEARING, 0, FALSE);
		SendDlgItemMessage(hdlg, IDL_N, CB_SETCURSEL, 9, 0);
		SendDlgItemMessage(hdlg, IDL_RANGE, CB_SETCURSEL, 0, 0);
		SendDlgItemMessage(hdlg, IDL_QUALITY, CB_SETCURSEL, 3, 0);
		if (pResult && *pResult && strlen(*pResult)==7 && (*pResult)[3] == '/')
		{	unsigned long Bearing;
			if (!FromDec(*pResult,3,&Bearing)) Bearing = 0;
			SetDlgItemInt(hdlg, IDE_BEARING, Bearing, FALSE);
			if (isdigit((*pResult)[4]&0xff))
				SendDlgItemMessage(hdlg, IDL_N, CB_SETCURSEL, (*pResult)[4]-'0', 0);
			if (isdigit((*pResult)[5]&0xff))
				SendDlgItemMessage(hdlg, IDL_RANGE, CB_SETCURSEL, (*pResult)[5]-'0', 0);
			if (isdigit((*pResult)[6]&0xff))
				SendDlgItemMessage(hdlg, IDL_QUALITY, CB_SETCURSEL, (*pResult)[6]-'0', 0);
			if ((*pResult)[4]=='0' && (*pResult)[6]=='0')
			{	CheckDlgButton(hdlg,IDC_ENABLED,TRUE);
				EnableWindow(GetDlgItem(hdlg,IDL_QUALITY),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDL_N),FALSE);
			}
		}
		SendDlgItemMessage(hdlg, IDS_BEARING, UDM_SETRANGE, 0, MAKELONG(360,0));
		CenterWindow(hdlg);
		return TRUE;
	}

	case WM_CLOSE:
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDC_ENABLED:	/* Actually Useless */
			if (IsDlgButtonChecked(hdlg, IDC_ENABLED))
			{	SendDlgItemMessage(hdlg, IDL_QUALITY, CB_SETCURSEL, 0, 0);
				SendDlgItemMessage(hdlg, IDL_N, CB_SETCURSEL, 0, 0);
				EnableWindow(GetDlgItem(hdlg,IDL_QUALITY),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDL_N),FALSE);
			} else
			{	EnableWindow(GetDlgItem(hdlg,IDL_QUALITY),TRUE);
				EnableWindow(GetDlgItem(hdlg,IDL_N),TRUE);
			}
			return TRUE;
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_ACCEPT:
		{	if (*pResult) free(*pResult);
			*pResult = (char*)malloc(8);
			sprintf(*pResult,"%03ld/%c%c%c",
					(long) GetDlgItemInt(hdlg, IDE_BEARING, NULL, FALSE),
					SendDlgItemMessage(hdlg,IDL_N,CB_GETCURSEL,0,0)+'0',
					SendDlgItemMessage(hdlg,IDL_RANGE,CB_GETCURSEL,0,0)+'0',
					SendDlgItemMessage(hdlg,IDL_QUALITY,CB_GETCURSEL,0,0)+'0');
			EndDialog(hdlg, IDOK);
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

static TCHAR *EngHeights[] = { TEXT("10"), TEXT("20"), TEXT("40"), TEXT("80"), 
							TEXT("160"), TEXT("320"), TEXT("640"),
							TEXT("1280"), TEXT("2560"), TEXT("5120"), NULL };
static TCHAR *MetHeights[] = { TEXT("3"), TEXT("6"), TEXT("12"), TEXT("24"), 
							TEXT("49"), TEXT("98"), TEXT("195"),
							TEXT("390"), TEXT("780"), TEXT("1561"), NULL };
static TCHAR *Gains[] = { TEXT("0"), TEXT("1"), TEXT("2"), TEXT("3"), 
							TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), 
							TEXT("8"), TEXT("9"), NULL };
static TCHAR *Directs[] = { TEXT("omni"), TEXT("045 NE"), TEXT("090 E"),
							TEXT("135 SE"), TEXT("180 S"),
							TEXT("225 SW"), TEXT("270 W"),
							TEXT("315 NW"), TEXT("360 N"), NULL };

BOOL CALLBACK DFSDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static char **pResult;
static TCHAR *Strengths[] = { TEXT("0"), TEXT("1"), TEXT("2"), TEXT("3"), 
							TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), 
							TEXT("8"), TEXT("9"), NULL };

	switch (msg)
	{
	case WM_INITDIALOG:
	{	pResult = (char **) lp;

		PopulateListBox(GetDlgItem(hdlg,IDL_STRENGTH), Strengths);
		PopulateListBox(GetDlgItem(hdlg,IDL_HEIGHT), ActiveConfig.View.Metric.Altitude?MetHeights:EngHeights);
		SetDlgItemText(hdlg, IDT_UNITS, ActiveConfig.View.Metric.Altitude?TEXT("meters"):TEXT("feet"));
		PopulateListBox(GetDlgItem(hdlg,IDL_GAIN), Gains);
		PopulateListBox(GetDlgItem(hdlg,IDL_DIRECT), Directs);

		SendDlgItemMessage(hdlg, IDL_STRENGTH, CB_SETCURSEL, 3, 0);
		SendDlgItemMessage(hdlg, IDL_HEIGHT, CB_SETCURSEL, 0, 0);
		SendDlgItemMessage(hdlg, IDL_GAIN, CB_SETCURSEL, 0, 0);
		SendDlgItemMessage(hdlg, IDL_DIRECT, CB_SETCURSEL, 0, 0);
		if (pResult && *pResult && strlen(*pResult)==4)
		{	if (isdigit((*pResult)[0]&0xff))
				SendDlgItemMessage(hdlg, IDL_STRENGTH, CB_SETCURSEL, (*pResult)[0]-'0', 0);
			if (isdigit((*pResult)[1]&0xff))
				SendDlgItemMessage(hdlg, IDL_HEIGHT, CB_SETCURSEL, (*pResult)[1]-'0', 0);
			if (isdigit((*pResult)[2]&0xff))
				SendDlgItemMessage(hdlg, IDL_GAIN, CB_SETCURSEL, (*pResult)[2]-'0', 0);
			if (isdigit((*pResult)[3]&0xff))
				SendDlgItemMessage(hdlg, IDL_DIRECT, CB_SETCURSEL, (*pResult)[3]-'0', 0);
		}

		CenterWindow(hdlg);

		return TRUE;
	}

	case WM_CLOSE:
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_ACCEPT:
		{	if (*pResult) free(*pResult);
			*pResult = (char*)malloc(5);
			sprintf(*pResult,"%c%c%c%c",
					SendDlgItemMessage(hdlg,IDL_STRENGTH,CB_GETCURSEL,0,0)+'0',
					SendDlgItemMessage(hdlg,IDL_HEIGHT,CB_GETCURSEL,0,0)+'0',
					SendDlgItemMessage(hdlg,IDL_GAIN,CB_GETCURSEL,0,0)+'0',
					SendDlgItemMessage(hdlg,IDL_DIRECT,CB_GETCURSEL,0,0)+'0');
			EndDialog(hdlg, IDOK);
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

BOOL CALLBACK PHGDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static char **pResult;
static TCHAR *Powers[] = { TEXT("0"), TEXT("1"), TEXT("4"), TEXT("9"), 
							TEXT("16"), TEXT("25"), TEXT("36"), TEXT("49"), 
							TEXT("64"), TEXT("81"), NULL };

	switch (msg)
	{
	case WM_INITDIALOG:
	{	pResult = (char **) lp;

		PopulateListBox(GetDlgItem(hdlg,IDL_POWER), Powers);
		PopulateListBox(GetDlgItem(hdlg,IDL_HEIGHT), ActiveConfig.View.Metric.Altitude?MetHeights:EngHeights);
		SetDlgItemText(hdlg, IDT_UNITS, ActiveConfig.View.Metric.Altitude?TEXT("meters"):TEXT("feet"));
		PopulateListBox(GetDlgItem(hdlg,IDL_GAIN), Gains);
		PopulateListBox(GetDlgItem(hdlg,IDL_DIRECT), Directs);
		SendDlgItemMessage(hdlg, IDS_RATE, UDM_SETRANGE, 0, MAKELONG(35,0));

		SetDlgItemInt(hdlg, IDE_RATE, 6, FALSE);

		SendDlgItemMessage(hdlg, IDL_POWER, CB_SETCURSEL, 3, 0);
		SendDlgItemMessage(hdlg, IDL_HEIGHT, CB_SETCURSEL, 0, 0);
		SendDlgItemMessage(hdlg, IDL_GAIN, CB_SETCURSEL, 0, 0);
		SendDlgItemMessage(hdlg, IDL_DIRECT, CB_SETCURSEL, 0, 0);
		if (pResult && *pResult && strlen(*pResult)==4)
		{	if (isdigit((*pResult)[0]&0xff))
				SendDlgItemMessage(hdlg, IDL_POWER, CB_SETCURSEL, (*pResult)[0]-'0', 0);
			if (isdigit((*pResult)[1]&0xff))
				SendDlgItemMessage(hdlg, IDL_HEIGHT, CB_SETCURSEL, (*pResult)[1]-'0', 0);
			if (isdigit((*pResult)[2]&0xff))
				SendDlgItemMessage(hdlg, IDL_GAIN, CB_SETCURSEL, (*pResult)[2]-'0', 0);
			if (isdigit((*pResult)[3]&0xff))
				SendDlgItemMessage(hdlg, IDL_DIRECT, CB_SETCURSEL, (*pResult)[3]-'0', 0);
		}
		EnableWindow(GetDlgItem(hdlg,IDC_ENABLED),FALSE);
		EnableWindow(GetDlgItem(hdlg,IDE_RATE),FALSE);
		EnableWindow(GetDlgItem(hdlg,IDS_RATE),FALSE);

		CenterWindow(hdlg);

		return TRUE;
	}

	case WM_CLOSE:
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_ACCEPT:
		{	if (*pResult) free(*pResult);
			*pResult = (char*)malloc(5);
			sprintf(*pResult,"%c%c%c%c",
					SendDlgItemMessage(hdlg,IDL_POWER,CB_GETCURSEL,0,0)+'0',
					SendDlgItemMessage(hdlg,IDL_HEIGHT,CB_GETCURSEL,0,0)+'0',
					SendDlgItemMessage(hdlg,IDL_GAIN,CB_GETCURSEL,0,0)+'0',
					SendDlgItemMessage(hdlg,IDL_DIRECT,CB_GETCURSEL,0,0)+'0');
			EndDialog(hdlg, IDOK);
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

BOOL GetCenterPosition(double *pLat, double *pLon);
BOOL SetCenterPosition(double pLat, double pLon, int zoom, char *Station, char *Owner);
BOOL TransmitObject2(OBJECT_CONFIG_INFO_S *Obj, BOOL LocalOnly=FALSE);

static char *MyGetDlgItemTextA(HWND hdlg, int ID)
{	HWND hwnd = GetDlgItem(hdlg, ID);
	if (!hwnd) return NULL;
	int Len = GetWindowTextLength(hwnd) + 1;
	TCHAR *Text = (TCHAR*)malloc(sizeof(*Text)*Len);
	GetWindowText(hwnd, Text, Len);
	char *Ascii = (char *)malloc(Len);
	StringCbPrintfA(Ascii, Len, "%S", Text);
	free(Text);
	return Ascii;
}

static void MySetDlgItemTextA(HWND hdlg, int ID, char *Ascii)
{	HWND hwnd = GetDlgItem(hdlg, ID);
	if (!hwnd) return;
	int Len = strlen(Ascii) + 1;
	TCHAR *Text = (TCHAR*)malloc(sizeof(*Text)*Len);
	StringCbPrintf(Text, sizeof(*Text)*Len, TEXT("%S"), Ascii);
	SetWindowText(hwnd, Text);
	free(Text);
}

BOOL CALLBACK ObjectDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static OBJECT_CONFIG_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (OBJECT_CONFIG_INFO_S *) lp;
		TCHAR Buffer[255];

		CheckDlgButton(hdlg, IDC_ITEM, Info->Item);
		if (Info->Permanent)
			CheckRadioButton(hdlg, IDC_DHM, IDC_PERMANENT, IDC_PERMANENT);
		else if (Info->HHMMSS)
			CheckRadioButton(hdlg, IDC_DHM, IDC_PERMANENT, IDC_HMS);
		else CheckRadioButton(hdlg, IDC_DHM, IDC_PERMANENT, IDC_DHM);

		CheckDlgButton(hdlg, IDC_KILL, Info->Kill);
		CheckDlgButton(hdlg, IDC_COMPRESSED, Info->Compressed);

		CheckDlgButton(hdlg, IDC_VIA_IS, Info->ISEnabled);
		CheckDlgButton(hdlg, IDC_VIA_RF, Info->RFEnabled);

		CheckDlgButton(hdlg, IDC_ENABLED, Info->Enabled);

		SendDlgItemMessage(hdlg, IDE_CALLSIGN, EM_LIMITTEXT, min(9,sizeof(Info->Name)-1), 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Name);
		SetDlgItemText(hdlg, IDE_CALLSIGN, Buffer);

		SendDlgItemMessage(hdlg, IDE_COMMENT, EM_LIMITTEXT, sizeof(Info->Comment)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Comment);
		SetDlgItemText(hdlg, IDE_COMMENT, Buffer);

		SendDlgItemMessage(hdlg, IDE_RF_PATH, EM_LIMITTEXT, sizeof(Info->RFPath)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->RFPath);
		SetDlgItemText(hdlg, IDE_RF_PATH, Buffer);

		SetDlgItemInt(hdlg, IDE_INTERVAL, Info->Interval, FALSE);
		SendDlgItemMessage(hdlg, IDS_INTERVAL, UDM_SETRANGE, 0, GetSpinRangeULong("Object.Interval"));

		if (HWND hwndList = GetDlgItem(hdlg, IDL_GROUP))
		{	BOOL *Used = (BOOL*)calloc(ActiveConfig.Objects.Count+1,sizeof(*Used));
			TCHAR uGrp[sizeof(Info->Group)+1];
			SendMessage(hwndList, CB_LIMITTEXT, sizeof(Info->Group)-1, 0);
			for (unsigned long o=0; o<ActiveConfig.Objects.Count; o++)
			if (!Used[o])
			{	OBJECT_CONFIG_INFO_S *Grp = &ActiveConfig.Objects.Obj[o];
				StringCbPrintf(uGrp,sizeof(uGrp),TEXT("%S"),Grp->Group);
				SendMessage(hwndList, CB_INSERTSTRING, 0, (LPARAM) uGrp);
				for (unsigned long p=o; p<ActiveConfig.Objects.Count; p++)
				if (!strcmp(Grp->Group, ActiveConfig.Objects.Obj[p].Group))
					Used[p] = TRUE;
			}
			StringCbPrintf(uGrp,sizeof(uGrp),TEXT("%S"),Info->Group);
			if (SendMessage(hwndList, CB_SELECTSTRING, -1, (LPARAM) uGrp) == CB_ERR)
			{	SendMessage(hwndList, CB_INSERTSTRING, 0, (LPARAM) uGrp);
				SendMessage(hwndList, CB_SELECTSTRING, -1, (LPARAM) uGrp);
			}
		}

		if (Info->Weather)
		{	EnableWindow(GetDlgItem(hdlg, IDL_GROUP), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_SYMBOL), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_DF), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_DFS), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDC_KILL), FALSE);
			SetDlgItemText(hdlg, IDC_ITEM, TEXT("Station"));
			EnableWindow(GetDlgItem(hdlg, IDC_PERMANENT), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDE_COMMENT), FALSE);
			if (!*Info->Comment)
				SetDlgItemText(hdlg, IDE_COMMENT, TEXT("Raw Weather"));
		}
#ifndef UNDER_CE
		else if (Info->JT65)
		{	EnableWindow(GetDlgItem(hdlg, IDL_GROUP), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_SYMBOL), TRUE);
			EnableWindow(GetDlgItem(hdlg, IDB_DF), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDB_DFS), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDC_KILL), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDC_ITEM), FALSE);
			SetDlgItemText(hdlg, IDC_ITEM, TEXT("GridSq"));
			EnableWindow(GetDlgItem(hdlg, IDC_PERMANENT), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDE_COMMENT), FALSE);
			if (!*Info->Comment)
				SetDlgItemText(hdlg, IDE_COMMENT, TEXT("GSNNtsFFF.FFFMHz !JT65!"));
			EnableWindow(GetDlgItem(hdlg, IDC_VIA_IS), FALSE);
			EnableWindow(GetDlgItem(hdlg, IDC_VIA_RF), FALSE);
			SetDlgItemText(hdlg, IDC_COMPRESSED, TEXT("Reset"));
		}
#endif
		else if (Info->Symbol.Table == '\\'	// Area objects
		&& Info->Symbol.Symbol == 'l')			// shouldn't get mucked
		{	EnableWindow(GetDlgItem(hdlg, IDB_SYMBOL), FALSE);
			DestroyWindow(GetDlgItem(hdlg, IDB_DF));
			DestroyWindow(GetDlgItem(hdlg, IDB_DFS));
			DestroyWindow(GetDlgItem(hdlg, IDB_PHG));
		} else
		{	char *Result = Info->Comment;
			if (strlen(Result) >= 15
				&& (isdigit(Result[0]&0xff) || Result[0]==' ' || Result[0]=='.')
				&& (isdigit(Result[1]&0xff) || Result[1]==' ' || Result[1]=='.')
				&& (isdigit(Result[2]&0xff) || Result[2]==' ' || Result[2]=='.')
				&& Result[3]=='/'
				&& (isdigit(Result[4]&0xff) || Result[4]==' ' || Result[4]=='.')
				&& (isdigit(Result[5]&0xff) || Result[5]==' ' || Result[5]=='.')
				&& (isdigit(Result[6]&0xff) || Result[6]==' ' || Result[6]=='.')
				&& Result[7]=='/'
				&& isdigit(Result[8]&0xff)
				&& isdigit(Result[9]&0xff)
				&& isdigit(Result[10]&0xff)
				&& Result[11]=='/'
				&& isdigit(Result[12]&0xff)
				&& isdigit(Result[13]&0xff)
				&& isdigit(Result[14]&0xff))
			{	DestroyWindow(GetDlgItem(hdlg, IDB_DFS));
				DestroyWindow(GetDlgItem(hdlg, IDB_PHG));
			}
			char *DFS = strstr(Info->Comment,"DFS");
			if (DFS && strlen(DFS)>=7
			&& isdigit(DFS[3]&0x0ff)
			&& isdigit(DFS[4]&0x0ff)
			&& isdigit(DFS[5]&0x0ff)
			&& isdigit(DFS[6]&0x0ff))
			{	DestroyWindow(GetDlgItem(hdlg, IDB_DF));
				DestroyWindow(GetDlgItem(hdlg, IDB_PHG));
			}
			char *PHG = strstr(Info->Comment,"PHG");
			if (PHG && strlen(PHG)>=7
			&& isdigit(PHG[3]&0x0ff)
			&& isdigit(PHG[4]&0x0ff)
			&& isdigit(PHG[5]&0x0ff)
			&& isdigit(PHG[6]&0x0ff))
			{	DestroyWindow(GetDlgItem(hdlg, IDB_DF));
				DestroyWindow(GetDlgItem(hdlg, IDB_DFS));
			}
		}
		CenterWindow(hdlg);

		return TRUE;
	}
	case WM_DRAWITEM:
	if (wp == IDB_SYMBOL)
	{	RECT rc;
		POINT pt;
		DRAWITEMSTRUCT *di = (LPDRAWITEMSTRUCT) lp;
		pt.x = (di->rcItem.left+di->rcItem.right)/2;
		pt.y = (di->rcItem.bottom+di->rcItem.top)/2;
		Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
		BltSymbol(di->hDC, pt.x, pt.y, 2, 1,
					Info->Symbol.Table=='/'?0:Info->Symbol.Table=='\\'?0x1:((Info->Symbol.Table<<8)|0x1),
					Info->Symbol.Symbol-'!', 100, &rc);
	}
	break;

	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE Object ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_PHG:
		{	char *Result = MyGetDlgItemTextA(hdlg, IDE_COMMENT);
			BOOL HadOne = FALSE;
			if (Result)
			{	char *PHG = strstr(Result, "PHG");
				if (PHG && strlen(PHG)>=7
				&& isdigit(PHG[3]&0x0ff)
				&& isdigit(PHG[4]&0x0ff)
				&& isdigit(PHG[5]&0x0ff)
				&& isdigit(PHG[6]&0x0ff))
				{	memmove(Result, PHG+3, strlen(PHG+3)+1);
					Result[4] = '\0';
					HadOne = TRUE;
				} else
				{	free(Result);
					Result = NULL;
				}
			}
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_PHG), hdlg, PHGDlgProc, (LPARAM)&Result) == IDOK)
			{	char *Old = MyGetDlgItemTextA(hdlg, IDE_COMMENT);
				if (HadOne)
				{	char *PHG = strstr(Old, "PHG");
					memmove(PHG, PHG+7, strlen(PHG+7)+1);
				}
				char *New = (char*)malloc(3+strlen(Result)+strlen(Old)+1);
				sprintf(New, "PHG%s%s", Result, Old);
				MySetDlgItemTextA(hdlg, IDE_COMMENT, New);
				free(New); free(Old); free(Result);
				EnableWindow(GetDlgItem(hdlg, IDB_DF), FALSE);
				EnableWindow(GetDlgItem(hdlg, IDB_DFS), FALSE);
			}
			return TRUE;
		}

		case IDB_DF:
		{	char *Result = MyGetDlgItemTextA(hdlg, IDE_COMMENT);
			BOOL HadOne = FALSE;
			if (Result)
			{	if (strlen(Result) >= 15
				&& (isdigit(Result[0]&0xff) || Result[0]==' ' || Result[0]=='.')
				&& (isdigit(Result[1]&0xff) || Result[1]==' ' || Result[1]=='.')
				&& (isdigit(Result[2]&0xff) || Result[2]==' ' || Result[2]=='.')
				&& Result[3]=='/'
				&& (isdigit(Result[4]&0xff) || Result[4]==' ' || Result[4]=='.')
				&& (isdigit(Result[5]&0xff) || Result[5]==' ' || Result[5]=='.')
				&& (isdigit(Result[6]&0xff) || Result[6]==' ' || Result[6]=='.')
				&& Result[7]=='/'
				&& isdigit(Result[8]&0xff)
				&& isdigit(Result[9]&0xff)
				&& isdigit(Result[10]&0xff)
				&& Result[11]=='/'
				&& isdigit(Result[12]&0xff)
				&& isdigit(Result[13]&0xff)
				&& isdigit(Result[14]&0xff))
				{	memmove(Result, Result+8, strlen(Result+8)+1);
					Result[7] = '\0';
					HadOne = TRUE;
				} else
				{	free(Result);
					Result = NULL;
				}
			}
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_DF), hdlg, DFDlgProc, (LPARAM)&Result) == IDOK)
			{	char *Old = MyGetDlgItemTextA(hdlg, IDE_COMMENT);
				if (HadOne) memmove(Old, Old+15, strlen(Old+15)+1);
				char *New = (char*)malloc(8+strlen(Result)+strlen(Old)+1);
				sprintf(New, ".../.../%s%s", Result, Old);
				MySetDlgItemTextA(hdlg, IDE_COMMENT, New);
				free(New); free(Old); free(Result);
				Info->Symbol.Table = '/';
				Info->Symbol.Symbol = '\\';
				InvalidateRect(GetDlgItem(hdlg, IDB_SYMBOL),NULL,FALSE);
				EnableWindow(GetDlgItem(hdlg, IDB_DFS), FALSE);
				EnableWindow(GetDlgItem(hdlg, IDB_PHG), FALSE);
			}
			return TRUE;
		}
		case IDB_DFS:
		{	char *Result = MyGetDlgItemTextA(hdlg, IDE_COMMENT);
			BOOL HadOne = FALSE;
			if (Result)
			{	char *DFS = strstr(Result, "DFS");
				if (DFS && strlen(DFS)>=7
				&& isdigit(DFS[3]&0x0ff)
				&& isdigit(DFS[4]&0x0ff)
				&& isdigit(DFS[5]&0x0ff)
				&& isdigit(DFS[6]&0x0ff))
				{	memmove(Result, DFS+3, strlen(DFS+3)+1);
					Result[4] = '\0';
					HadOne = TRUE;
				} else
				{	free(Result);
					Result = NULL;
				}
			}
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_DFS), hdlg, DFSDlgProc, (LPARAM)&Result) == IDOK)
			{	char *Old = MyGetDlgItemTextA(hdlg, IDE_COMMENT);
				if (HadOne)
				{	char *DFS = strstr(Old, "DFS");
					memmove(DFS, DFS+7, strlen(DFS+7)+1);
				}
				char *New = (char*)malloc(3+strlen(Result)+strlen(Old)+1);
				sprintf(New, "DFS%s%s", Result, Old);
				MySetDlgItemTextA(hdlg, IDE_COMMENT, New);
				free(New); free(Old); free(Result);
				Info->Symbol.Table = '/';
				Info->Symbol.Symbol = 'r';	/* Antenna */
				InvalidateRect(GetDlgItem(hdlg, IDB_SYMBOL),NULL,FALSE);
				EnableWindow(GetDlgItem(hdlg, IDB_DF), FALSE);
				EnableWindow(GetDlgItem(hdlg, IDB_PHG), FALSE);
			}
			return TRUE;
		}
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_DELETE:
			if (MessageBox(hdlg, TEXT("Really Delete Object?"), TEXT("Object Config"), MB_ICONQUESTION | MB_YESNO) == IDYES)
				EndDialog(hdlg, IDB_DELETE);
			return TRUE;
		case IDB_MOVE:
			if (MessageBox(hdlg, TEXT("Move Object To Map Center?"), TEXT("Move Object"),
							MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
			{	GetCenterPosition(&Info->Latitude, &Info->Longitude);
			}
			return TRUE;
		case IDB_SHOW:
			EndDialog(hdlg, IDB_SHOW);
			return TRUE;
		case IDB_ACCEPT:
		{	TCHAR *Name = (TCHAR*)malloc(sizeof(*Name)*32);
			TCHAR *Group = (TCHAR*)malloc(sizeof(*Group)*32);
			TCHAR *Comment = (TCHAR*)malloc(sizeof(*Comment)*128);
			TCHAR *RFPath = (TCHAR*)malloc(sizeof(*RFPath)*128);

			Info->Interval = GetDlgItemInt(hdlg, IDE_INTERVAL, NULL, FALSE);
			Info->Enabled = IsDlgButtonChecked(hdlg, IDC_ENABLED);
			Info->ISEnabled = IsDlgButtonChecked(hdlg, IDC_VIA_IS);
			Info->RFEnabled = IsDlgButtonChecked(hdlg, IDC_VIA_RF);
			Info->Compressed = IsDlgButtonChecked(hdlg, IDC_COMPRESSED);
			Info->Kill = IsDlgButtonChecked(hdlg, IDC_KILL);
			Info->KillXmitCount = 0;
			Info->Item = IsDlgButtonChecked(hdlg, IDC_ITEM);

			Info->Permanent = Info->HHMMSS = FALSE;
			if (IsDlgButtonChecked(hdlg, IDC_PERMANENT))
				Info->Permanent = TRUE;
			else if (IsDlgButtonChecked(hdlg, IDC_HMS))
				Info->HHMMSS = TRUE;

			GetDlgItemText(hdlg, IDE_RF_PATH, RFPath, 128);

			GetDlgItemText(hdlg, IDL_GROUP, Group, 32);	/* May be blank */
			if ((GetDlgItemText(hdlg, IDE_CALLSIGN, Name, 32))
			&& (GetDlgItemText(hdlg, IDE_COMMENT, Comment, 128)))
			{	if (SUCCEEDED(StringCbPrintfA(Info->Name, sizeof(Info->Name), "%S", Name))
				&& SUCCEEDED(StringCbPrintfA(Info->Group, sizeof(Info->Group), "%S", Group))
				&& SUCCEEDED(StringCbPrintfA(Info->Comment, sizeof(Info->Comment), "%S", Comment))
				&& SUCCEEDED(StringCbPrintfA(Info->RFPath, sizeof(Info->RFPath), "%S", RFPath)))
				{	if (_stricmp(Info->Name, CALLSIGN)
					|| MessageBox(hdlg, TEXT("Duplicating your own station call-SSID is NOT recommended, Continue anyway?"), TEXT("Obj vs Station"), MB_YESNO | MB_ICONQUESTION) == IDYES)
					if (Info->JT65 || !Info->Weather || !Info->Item	/* Makes it a station */
					|| !Info->RFEnabled		/* -IS doesn't care */
					|| IsAX25Safe((unsigned char*)Info->Name)	/* RF needs AX.25 safe */
					|| MessageBox(hdlg, TEXT("Station Name is not AX.25-safe, Use it anyway?"), TEXT("Weather Station"), MB_YESNO | MB_ICONQUESTION) == IDYES)
						EndDialog(hdlg, IDOK);
				} else MessageBox(hdlg, TEXT("Name/Comment/RFPath Format Failed"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			} else MessageBox(hdlg, TEXT("ID and Comment Required"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			free(Name); free(Comment); free(RFPath);
			return TRUE;
		}
		case IDB_SYMBOL:
		{	SYMBOL_INFO_S Working = Info->Symbol;
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SYMBOL), hdlg, SymbolDlgProc, (LPARAM)&Working) == IDOK)
			{	Info->Symbol = Working;
				InvalidateRect(hdlg,NULL,FALSE);
			}
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

char *ParseWeatherFile(char *Trace, char *File, SYSTEMTIME *pst/*=NULL*/)
{	SYSTEMTIME st = {0};
	char *InBuf, *Packet = NULL;
	FILE *In = fopen(File, "rt");
static char *Months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
							"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	if (!In)
	{	TraceLogThread(Trace, TRUE, "Failed To Open(%s)\n", File);
		return NULL;
	}
	InBuf = (char*)malloc(1024);
	if (fgets(InBuf, 1024, In))
	{	TrimEnd(InBuf);
		if (strlen(InBuf) >= 25
		&& *InBuf == '_'
		&& InBuf[9] == 'c'
		&& InBuf[13] == 's')
/*  012345678901234567890123456789012345678901234 */
/*	_12241439c0-0s000g000t073r000p000P000h48xOWWD */
		{	if (!wFromDec(&InBuf[1], 2, &st.wMonth)
			|| !wFromDec(&InBuf[3], 2, &st.wDay)
			|| !wFromDec(&InBuf[5], 2, &st.wHour)
			|| !wFromDec(&InBuf[7], 2, &st.wMinute))
			{	TraceLogThread(Trace, TRUE, "Failed To Convert Date In (%s) From (%s)\n", InBuf, File);
				goto cleanup;
			}
			Packet = (char*)malloc(strlen(InBuf));
			strcpy(Packet, &InBuf[10]);	/* Pick it up after the c */
			Packet[3] = '/';	/* replace the s with / in course/speed */
		} else if (strlen(InBuf) >= 17
		&& isalpha(InBuf[0]&0xff)
		&& InBuf[3] == ' '
		&& InBuf[6] == ' '
		&& InBuf[11] == ' '
		&& InBuf[14] == ':')
		{
/*  012345678901234567890123456789012345678901234 */
/*	Jan 20 2011 15:47
	000/000g000t078r000p000P000h64b10150/fWD */
/*  012345678901234567890123456789012345678901234 */
/*	Feb 01 2009 12:34
	272/010g006t069r010p030P020h61b10150 */
/*  012345678901234567890123456789012345678901234 */
			InBuf[3] = '\0';	/* Null terminate month */
			for (st.wMonth=0; st.wMonth<ARRAYSIZE(Months); st.wMonth++)
				if (_stricmp(InBuf, Months[st.wMonth]))
					break;
			InBuf[3] = ' ';
			if (st.wMonth >= ARRAYSIZE(Months)
			|| !wFromDec(&InBuf[7], 4, &st.wYear)
			|| !wFromDec(&InBuf[4], 2, &st.wDay)
			|| !wFromDec(&InBuf[12], 2, &st.wHour)
			|| !wFromDec(&InBuf[15], 2, &st.wMinute))
			{	TraceLogThread(Trace, TRUE, "Failed To Convert Date In (%s) From (%s)\n", InBuf, File);
				goto cleanup;
			}
			if (!fgets(InBuf, 1024, In))
			{	TraceLogThread(Trace, TRUE, "Missing Second Line From (%s)\n", File);
				goto cleanup;
			}
			TrimEnd(InBuf);
			if (strlen(InBuf) < 19)
			{	TraceLogThread(Trace, TRUE, "Line(%s) Too Short From (%s)\n", InBuf, File);
				goto cleanup;
			}
			Packet = strdup(InBuf);
		} else if (strlen(InBuf) >= 16
		&& isalpha(InBuf[0]&0xff)
		&& InBuf[3] == ' '
		&& InBuf[5] == ' '
		&& InBuf[10] == ' '
		&& InBuf[13] == ':')
		{
/*  012345678901234567890123456789012345678901234 */
/*	Feb 6 2011 18:29
	107/000g000t065r000p024P004b10170h70FWX */
/*  012345678901234567890123456789012345678901234 */
			InBuf[3] = '\0';	/* Null terminate month */
			for (st.wMonth=0; st.wMonth<ARRAYSIZE(Months); st.wMonth++)
				if (_stricmp(InBuf, Months[st.wMonth]))
					break;
			InBuf[3] = ' ';
			if (st.wMonth >= ARRAYSIZE(Months)
			|| !wFromDec(&InBuf[6], 4, &st.wYear)
			|| !wFromDec(&InBuf[4], 1, &st.wDay)
			|| !wFromDec(&InBuf[11], 2, &st.wHour)
			|| !wFromDec(&InBuf[14], 2, &st.wMinute))
			{	TraceLogThread(Trace, TRUE, "Failed To Convert Date In (%s) From (%s)\n", InBuf, File);
				goto cleanup;
			}
			if (!fgets(InBuf, 1024, In))
			{	TraceLogThread(Trace, TRUE, "Missing Second Line From (%s)\n", File);
				goto cleanup;
			}
			TrimEnd(InBuf);
			if (strlen(InBuf) < 19)
			{	TraceLogThread(Trace, TRUE, "Line(%s) Too Short From (%s)\n", InBuf, File);
				goto cleanup;
			}
			Packet = strdup(InBuf);
		} else
		{	TraceLogThread(Trace, TRUE, "Unrecognized Line(%s) From (%s)\n", InBuf, File);
			goto cleanup;
		}
	}
	if (pst) *pst = st;
cleanup:
	free(InBuf);
	fclose(In);
	return Packet;
}

static char LastObjGroup[16] = {0};

OBJECT_CONFIG_INFO_S *PromptConfigObject(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id)
{	OBJECT_CONFIG_INFO_S Working = pConfig->Objects.Obj[id];
	OBJECT_CONFIG_INFO_S *Result = NULL;
	INT_PTR idEnd;

	if ((idEnd=DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_OBJECT), hwnd, ObjectDlgProc, (LPARAM)&Working)) == IDOK)
	{
		memset(&Working.LastTransmit, 0, sizeof(Working.LastTransmit));
		strncpy(LastObjGroup, Working.Group, sizeof(LastObjGroup));
		pConfig->Objects.Obj[id] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:Object");
		Result = &pConfig->Objects.Obj[id];
	} else if (idEnd == IDB_DELETE)
	{	if (Working.Enabled && !Working.Kill)
		{	Working.Kill = TRUE;
			Working.KillXmitCount = 0;
			memset(&Working.LastTransmit, 0, sizeof(Working.LastTransmit));
			TransmitObject2(&Working);
		}
		if (id < --pConfig->Objects.Count)
			memmove(&pConfig->Objects.Obj[id], &pConfig->Objects.Obj[id+1],
					sizeof(pConfig->Objects.Obj[0])*(pConfig->Objects.Count-id));
	} else if (idEnd == IDB_SHOW)
	{	TransmitObject2(&Working, TRUE);
		SetCenterPosition(Working.Latitude, Working.Longitude, MIN_SETTABLE_ZOOM,
							Working.Name, CALLSIGN);
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

OBJECT_CONFIG_INFO_S *PromptNewObject(HWND hwnd, CONFIG_INFO_S *pConfig, double Latitude, double Longitude, char Table/*='/'*/, char Symbol/*='.'*/, char *Comment/*=NULL*/, char *Group/*=NULL*/, char *Name/*=NULL*/)
{	OBJECT_CONFIG_INFO_S Working = {0};
	OBJECT_CONFIG_INFO_S *Result = NULL;

	Working.Enabled = TRUE;	/* This makes it show up */
	if (Comment) strncpy(Working.Comment, Comment, sizeof(Working.Comment));
	if (Group) strncpy(Working.Group, Group, sizeof(Working.Group));
	else strncpy(Working.Group, LastObjGroup, sizeof(Working.Group));
	if (Name) strncpy(Working.Name, Name, sizeof(Working.Name));
	strncpy(Working.RFPath, pConfig->BeaconPath, sizeof(Working.RFPath));
	Working.Symbol.Table = Table;		/* Primary table */
	Working.Symbol.Symbol = Symbol;		/* Red X */
	Working.Latitude = Latitude;
	Working.Longitude = Longitude;
	Working.Interval = ActiveConfig.Aging.DefaultObjectInterval;
	Working.Precision = ActiveConfig.Beacon.Precision;
	Working.Item = FALSE;
	Working.HHMMSS = FALSE;
	Working.Permanent = FALSE;

	INT_PTR idResult;
	
	do
	{	idResult = DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_OBJECT), hwnd, ObjectDlgProc, (LPARAM)&Working);
	} while (idResult == IDOK
	&& FindConfigObject(pConfig, Working.Name)
	&& (MessageBox(hwnd, TEXT("Object Already Exists, Choose Another Name"), TEXT("Duplicate Name"), MB_OK) || TRUE));

	if (idResult == IDOK)
	{	unsigned long i = pConfig->Objects.Count++;
		pConfig->Objects.Obj = (OBJECT_CONFIG_INFO_S *)realloc(pConfig->Objects.Obj,sizeof(*pConfig->Objects.Obj)*pConfig->Objects.Count);

		strncpy(LastObjGroup, Working.Group, sizeof(LastObjGroup));
		pConfig->Objects.Obj[i] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:NewObject");
		Result = &pConfig->Objects.Obj[i];
	} else Result = NULL;
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

BOOL PromptNewWeatherObject(HWND hwnd, CONFIG_INFO_S *pConfig, double Latitude, double Longitude, char *WeatherFile)
{	OBJECT_CONFIG_INFO_S Working = {0};
	BOOL Result;

	strncpy(Working.RFPath, pConfig->BeaconPath, sizeof(Working.RFPath));
	Working.Latitude = Latitude;
	Working.Longitude = Longitude;
	Working.Item = FALSE;
	Working.HHMMSS = FALSE;
	Working.Permanent = FALSE;

	Working.Symbol.Table = '/';		/* Primary table */
	Working.Symbol.Symbol = '_';	/* Weather Station */
	Working.Weather = TRUE;
	strncpy(Working.WeatherPath, WeatherFile, sizeof(Working.WeatherPath));

	strncpy(Working.Name, pConfig->CallSign, sizeof(Working.Name));
	if (strchr(Working.Name,'-'))
		*strchr(Working.Name,'-') = '\0';
	strcat(Working.Name, "-WX");

	if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_OBJECT), hwnd, ObjectDlgProc, (LPARAM)&Working) == IDOK)
	{	unsigned long i = pConfig->Objects.Count++;
		pConfig->Objects.Obj = (OBJECT_CONFIG_INFO_S *)realloc(pConfig->Objects.Obj,sizeof(*pConfig->Objects.Obj)*pConfig->Objects.Count);

		pConfig->Objects.Obj[i] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:NewWeather");
		Result = TRUE;
	} else Result = FALSE;
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

#ifndef UNDER_CE
BOOL PromptNewJT65Object(HWND hwnd, CONFIG_INFO_S *pConfig, double Latitude, double Longitude, char *JT65File)
{	OBJECT_CONFIG_INFO_S Working = {0};
	BOOL Result;

	strncpy(Working.RFPath, pConfig->BeaconPath, sizeof(Working.RFPath));
	Working.Latitude = Latitude;
	Working.Longitude = Longitude;
	Working.Item = FALSE;
	Working.HHMMSS = FALSE;
	Working.Permanent = FALSE;
	Working.Interval = 1;

	Working.Symbol.Table = '/';		/* Primary table */
	Working.Symbol.Symbol = '%';	/* DX Station */
	Working.JT65 = TRUE;
	strncpy(Working.WeatherPath, JT65File, sizeof(Working.WeatherPath));

	strncpy(Working.Name, pConfig->CallSign, sizeof(Working.Name));
	if (strchr(Working.Name,'-'))
		*strchr(Working.Name,'-') = '\0';
	strcat(Working.Name, "-JT");

	if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_OBJECT), hwnd, ObjectDlgProc, (LPARAM)&Working) == IDOK)
	{	unsigned long i = pConfig->Objects.Count++;
		pConfig->Objects.Obj = (OBJECT_CONFIG_INFO_S *)realloc(pConfig->Objects.Obj,sizeof(*pConfig->Objects.Obj)*pConfig->Objects.Count);

		pConfig->Objects.Obj[i] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:NewWeather");
		Result = TRUE;
	} else Result = FALSE;
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}
#endif

OBJECT_CONFIG_INFO_S *FindConfigObject(CONFIG_INFO_S *pConfig, char *Name)
{
	for (unsigned long o=0; o<pConfig->Objects.Count; o++)
	{	OBJECT_CONFIG_INFO_S *Obj = &pConfig->Objects.Obj[o];
		if (!strcmp(Obj->Name, Name)) return Obj;
	}
	return NULL;
}

static int CompareObjects(const void *One, const void *Two)
{	OBJECT_CONFIG_INFO_S *Left = (OBJECT_CONFIG_INFO_S *)One;
	OBJECT_CONFIG_INFO_S *Right = (OBJECT_CONFIG_INFO_S *)Two;
	int r = strcmp(Left->Group, Right->Group);
	if (!r) r = strcmp(Left->Name, Right->Name);
	return r;
}

void SortConfigObjects(CONFIG_INFO_S *pConfig)
{	if (pConfig->Objects.Count)
		qsort(pConfig->Objects.Obj, pConfig->Objects.Count, sizeof(*pConfig->Objects.Obj), CompareObjects);
}

BOOL IsObjectInRange(double MaxRange, BOOL Metric, OBJECT_CONFIG_INFO_S *Obj, double Lat, double Lon, double *pDistance, double *pBearing)
{	double distance, bearing;
	BOOL InRange = FALSE;

	AprsHaversineLatLon(Lat, Lon,
						Obj->Latitude, Obj->Longitude,
						&distance, &bearing);
	if (Metric)
	{	distance *= KmPerMile;
		InRange = (distance <= MaxRange);
	} else InRange = (distance <= MaxRange);
	if (pDistance) *pDistance = distance;
	if (pBearing) *pBearing = bearing;
	return InRange;
}

char *GetQRUGroups(CONFIG_INFO_S *pConfig, BOOL NoQuestion, double Lat, double Lon, double MaxRange, BOOL Metric)
{
	if (!pConfig->Objects.Count) return NULL;	/* Easy out */

	if (!MaxRange)
	{	MaxRange = pConfig->QRU.Range;
		Metric = pConfig->View.Metric.Distance;
	}

	char *Body = _strdup("");
	SortConfigObjects(pConfig);
	int *GroupCount = (BOOL*)calloc(pConfig->Objects.Count,sizeof(*GroupCount));
	for (unsigned long o=0; o<pConfig->Objects.Count; o++)
	if (pConfig->Objects.Obj[o].Group[0] == '?')	/* Query group? */
	if (pConfig->Objects.Obj[o].Enabled
	&& (pConfig->Objects.Obj[o].ISEnabled
		|| pConfig->Objects.Obj[o].RFEnabled))
	if (!GroupCount[o])
	if ((Lat==0.0 && Lon==0.0)
	|| IsObjectInRange(MaxRange, Metric, &pConfig->Objects.Obj[o], Lat, Lon))
	{	OBJECT_CONFIG_INFO_S *Obj = &pConfig->Objects.Obj[o];
		for (unsigned long p=o; p<pConfig->Objects.Count; p++)
		if (!strcmp(pConfig->Objects.Obj[p].Group, Obj->Group))
		{	GroupCount[p]++;	/* Mark for skipping, double-counts o */
			if (pConfig->Objects.Obj[p].Enabled
			&& (pConfig->Objects.Obj[p].ISEnabled
				|| pConfig->Objects.Obj[p].RFEnabled))
			if ((Lat==0.0 && Lon==0.0)
			|| IsObjectInRange(MaxRange, Metric, &pConfig->Objects.Obj[p], Lat, Lon))
				GroupCount[o]++;	/* Count in the group */
		}
		char *NewBody = (char*)malloc(strlen(Obj->Group)+1+33+1+1+strlen(Body)+1);
		sprintf(NewBody,"%s(%d)%s%s",
				Obj->Group+(NoQuestion?1:0), (int)GroupCount[o]-1,
				*Body?" ":"", Body);
		free(Body);
		Body = NewBody;
	}
	free(GroupCount);
	if (!*Body)
	{	free(Body); Body = NULL;
	}
	return Body;
}

























BOOL CALLBACK CompanionDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static COMPANION_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (COMPANION_INFO_S *) lp;
		TCHAR Buffer[255];

		CheckDlgButton(hdlg, IDC_ENABLED, Info->Enabled);

		CheckDlgButton(hdlg, IDC_ITEM, Info->Object);
		CheckDlgButton(hdlg, IDC_BEACON, Info->Posit);
		CheckDlgButton(hdlg, IDC_MESSAGE, Info->Messaging);

		CheckDlgButton(hdlg, IDC_VIA_IS, Info->ISEnabled);
		CheckDlgButton(hdlg, IDC_VIA_RF, Info->RFEnabled);

		SendDlgItemMessage(hdlg, IDE_CALLSIGN, EM_LIMITTEXT, min(9,sizeof(Info->Name)-1), 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Name);
		SetDlgItemText(hdlg, IDE_CALLSIGN, Buffer);

		SendDlgItemMessage(hdlg, IDE_COMMENT, EM_LIMITTEXT, sizeof(Info->Comment)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Comment);
		SetDlgItemText(hdlg, IDE_COMMENT, Buffer);

		SendDlgItemMessage(hdlg, IDE_RF_PATH, EM_LIMITTEXT, sizeof(Info->RFPath)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->RFPath);
		SetDlgItemText(hdlg, IDE_RF_PATH, Buffer);

		SetDlgItemInt(hdlg, IDE_MIN_TIME, Info->Genius.MinTime, FALSE);
		SetDlgItemInt(hdlg, IDE_MAX_TIME, Info->Genius.MaxTime, FALSE);
		CheckDlgButton(hdlg, IDC_TIME_ONLY, Info->Genius.TimeOnly);
		CheckDlgButton(hdlg, IDC_START_STOP, Info->Genius.StartStop);
		SetDlgItemInt(hdlg, IDE_FORECAST_ERROR, (unsigned long) (Info->Genius.ForecastError*10), FALSE);
		SetDlgItemInt(hdlg, IDE_MAX_DISTANCE, (unsigned long) (Info->Genius.MaxDistance*10), FALSE);

		SendDlgItemMessage(hdlg, IDS_MIN_TIME, UDM_SETRANGE, 0, GetSpinRangeULong("Genius.MinTime"));
		SendDlgItemMessage(hdlg, IDS_MAX_TIME, UDM_SETRANGE, 0, GetSpinRangeULong("Genius.MaxTime"));
		SendDlgItemMessage(hdlg, IDS_FORECAST_ERROR, UDM_SETRANGE, 0, MAKELONG((unsigned long) (GetConfigMaxDouble("Genius.ForecastError")*10), (unsigned long) (GetConfigMinDouble("Genius.ForecastError")*10)));
		SendDlgItemMessage(hdlg, IDS_MAX_DISTANCE, UDM_SETRANGE, 0, MAKELONG((unsigned long) (GetConfigMaxDouble("Genius.MaxDistance")*10), (unsigned long) (GetConfigMinDouble("Genius.MaxDistance")*10)));

		CenterWindow(hdlg);

		return TRUE;
	}
	case WM_DRAWITEM:
	if (wp == IDB_SYMBOL)
	{	RECT rc;
		POINT pt;
		DRAWITEMSTRUCT *di = (LPDRAWITEMSTRUCT) lp;
		pt.x = (di->rcItem.left+di->rcItem.right)/2;
		pt.y = (di->rcItem.bottom+di->rcItem.top)/2;
		Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
		BltSymbol(di->hDC, pt.x, pt.y, 2, 1,
					Info->Symbol.Table=='/'?0:Info->Symbol.Table=='\\'?0x1:((Info->Symbol.Table<<8)|0x1),
					Info->Symbol.Symbol-'!', 100, &rc);
	}
	break;

	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE Companion ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_DELETE:
			if (MessageBox(hdlg, TEXT("Really Delete Companion?"), TEXT("Companion Config"), MB_ICONQUESTION | MB_YESNO) == IDYES)
				EndDialog(hdlg, IDB_DELETE);
			return TRUE;
		case IDB_ACCEPT:
		{	TCHAR *Name = (TCHAR*)malloc(sizeof(*Name)*32);
			TCHAR *Comment = (TCHAR*)malloc(sizeof(*Comment)*128);
			TCHAR *RFPath = (TCHAR*)malloc(sizeof(*RFPath)*128);

			Info->Enabled = IsDlgButtonChecked(hdlg, IDC_ENABLED);
			Info->ISEnabled = IsDlgButtonChecked(hdlg, IDC_VIA_IS);
			Info->RFEnabled = IsDlgButtonChecked(hdlg, IDC_VIA_RF);
			Info->Object = IsDlgButtonChecked(hdlg, IDC_ITEM);
			Info->Posit = IsDlgButtonChecked(hdlg, IDC_BEACON);
			Info->Messaging = IsDlgButtonChecked(hdlg, IDC_MESSAGE);

			Info->Genius.MinTime = GetDlgItemInt(hdlg, IDE_MIN_TIME, NULL, FALSE);
			Info->Genius.MaxTime = GetDlgItemInt(hdlg, IDE_MAX_TIME, NULL, FALSE);
			Info->Genius.TimeOnly = IsDlgButtonChecked(hdlg, IDC_TIME_ONLY);
			Info->Genius.StartStop = IsDlgButtonChecked(hdlg, IDC_START_STOP);
			Info->Genius.BearingChange = 0;
			Info->Genius.ForecastError = GetDlgItemInt(hdlg, IDE_FORECAST_ERROR, NULL, FALSE) / 10.0;
			Info->Genius.MaxDistance = GetDlgItemInt(hdlg, IDE_MAX_DISTANCE, NULL, FALSE) / 10.0;

			GetDlgItemText(hdlg, IDE_RF_PATH, RFPath, 128);

			if ((GetDlgItemText(hdlg, IDE_CALLSIGN, Name, 32))
			&& (GetDlgItemText(hdlg, IDE_COMMENT, Comment, 128)))
			{	if (SUCCEEDED(StringCbPrintfA(Info->Name, sizeof(Info->Name), "%S", Name))
				&& SUCCEEDED(StringCbPrintfA(Info->Comment, sizeof(Info->Comment), "%S", Comment))
				&& SUCCEEDED(StringCbPrintfA(Info->RFPath, sizeof(Info->RFPath), "%S", RFPath)))
				{	if (_stricmp(Info->Name, CALLSIGN)
					|| MessageBox(hdlg, TEXT("Duplicating your own station call-SSID is NOT recommended, Continue anyway?"), TEXT("Companion vs Station"), MB_YESNO | MB_ICONQUESTION) == IDYES)
					if (Info->Object	/* Objects don't care */
					|| !Info->RFEnabled		/* -IS doesn't care */
					|| IsAX25Safe((unsigned char*)Info->Name)	/* RF needs AX.25 safe */
					|| MessageBox(hdlg, TEXT("Companion Name is not AX.25-safe, Use it anyway?"), TEXT("AX.25 Companion"), MB_YESNO | MB_ICONQUESTION) == IDYES)
						EndDialog(hdlg, IDOK);
				} else MessageBox(hdlg, TEXT("Name/Comment/RFPath Format Failed"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			} else MessageBox(hdlg, TEXT("ID and Comment Required"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			free(Name); free(Comment); free(RFPath);
			return TRUE;
		}
		case IDB_SYMBOL:
		{	SYMBOL_INFO_S Working = Info->Symbol;
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SYMBOL), hdlg, SymbolDlgProc, (LPARAM)&Working) == IDOK)
			{	Info->Symbol = Working;
				InvalidateRect(hdlg,NULL,FALSE);
			}
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

COMPANION_INFO_S *PromptConfigCompanion(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id)
{	COMPANION_INFO_S Working = pConfig->Companions.Companion[id];
	COMPANION_INFO_S *Result = NULL;
	INT_PTR idEnd;

	if ((idEnd=DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_COMPANION), hwnd, CompanionDlgProc, (LPARAM)&Working)) == IDOK)
	{
//		memset(&Working.LastTransmit, 0, sizeof(Working.LastTransmit));
		pConfig->Companions.Companion[id] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:Companion");
		Result = &pConfig->Companions.Companion[id];
	} else if (idEnd == IDB_DELETE)
	{	if (id < --pConfig->Companions.Count)
			memmove(&pConfig->Companions.Companion[id], &pConfig->Companions.Companion[id+1],
					sizeof(pConfig->Companions.Companion[0])*(pConfig->Companions.Count-id));
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

COMPANION_INFO_S *FindConfigCompanion(CONFIG_INFO_S *pConfig, char *Name)
{
	for (unsigned long o=0; o<pConfig->Companions.Count; o++)
	{	COMPANION_INFO_S *Companion = &pConfig->Companions.Companion[o];
		if (!strcmp(Companion->Name, Name)) return Companion;
	}
	return NULL;
}

COMPANION_INFO_S *PromptNewCompanion(HWND hwnd, CONFIG_INFO_S *pConfig, char Table/*='/'*/, char Symbol/*='.'*/, char *Comment/*=NULL*/, char *Name/*=NULL*/)
{	COMPANION_INFO_S Working = {0};
	COMPANION_INFO_S *Result = NULL;

	Working.Enabled = TRUE;	/* This makes it show up */
	if (Comment) strncpy(Working.Comment, Comment, sizeof(Working.Comment));
	if (Name) strncpy(Working.Name, Name, sizeof(Working.Name));
	strncpy(Working.RFPath, pConfig->BeaconPath, sizeof(Working.RFPath));
	Working.Symbol.Table = Table;		/* Primary table */
	Working.Symbol.Symbol = Symbol;		/* Red X */
	Working.Object = FALSE;
	Working.Genius = pConfig->MyGenius;

	INT_PTR idResult;
	
	do
	{	idResult = DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_COMPANION), hwnd, CompanionDlgProc, (LPARAM)&Working);
	} while (idResult == IDOK
	&& FindConfigCompanion(pConfig, Working.Name)
	&& (MessageBox(hwnd, TEXT("Companion Already Exists, Choose Another Name"), TEXT("Duplicate Name"), MB_OK) || TRUE));

	if (idResult == IDOK)
	{	unsigned long i = pConfig->Companions.Count++;
		pConfig->Companions.Companion = (COMPANION_INFO_S *)realloc(pConfig->Companions.Companion,sizeof(*pConfig->Companions.Companion)*pConfig->Companions.Count);

		pConfig->Companions.Companion[i] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:NewCompanion");
		Result = &pConfig->Companions.Companion[i];
	} else Result = NULL;
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

static int CompareCompanions(const void *One, const void *Two)
{	COMPANION_INFO_S *Left = (COMPANION_INFO_S *)One;
	COMPANION_INFO_S *Right = (COMPANION_INFO_S *)Two;
	int r = strcmp(Left->Name, Right->Name);
	return r;
}

void SortConfigCompanions(CONFIG_INFO_S *pConfig)
{	if (pConfig->Companions.Count)
		qsort(pConfig->Companions.Companion, pConfig->Companions.Count, sizeof(*pConfig->Companions.Companion), CompareCompanions);
}




















BOOL CALLBACK NicknameDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static NICKNAME_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (NICKNAME_INFO_S *) lp;
		TCHAR Buffer[255];

		CheckDlgButton(hdlg, IDC_ENABLED, Info->Enabled);
		CheckDlgButton(hdlg, IDC_OVERRIDE_LABEL, Info->OverrideLabel);
		CheckDlgButton(hdlg, IDC_OVERRIDE_COMMENT, Info->OverrideComment);
		CheckDlgButton(hdlg, IDC_OVERRIDE_COLOR, Info->OverrideColor);
		CheckDlgButton(hdlg, IDC_OVERRIDE, Info->OverrideSymbol);
		CheckDlgButton(hdlg, IDC_MULTI_NEW, Info->MultiTrackNew);
		CheckDlgButton(hdlg, IDC_MULTI_ACTIVE, Info->MultiTrackActive);
		CheckDlgButton(hdlg, IDC_MULTI_ALWAYS, Info->MultiTrackAlways);
		EnableWindow(GetDlgItem(hdlg,IDB_SYMBOL),IsDlgButtonChecked(hdlg, IDC_OVERRIDE));

		SendDlgItemMessage(hdlg, IDE_CALLSIGN, EM_LIMITTEXT, min(9,sizeof(Info->Station)-1), 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Station);
		SetDlgItemText(hdlg, IDE_CALLSIGN, Buffer);
		EnableWindow(GetDlgItem(hdlg, IDB_DELETE), strncmp(Info->Station, CALLSIGN, sizeof(Info->Station)));

		SendDlgItemMessage(hdlg, IDE_LABEL, EM_LIMITTEXT, sizeof(Info->Label)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Label);
		SetDlgItemText(hdlg, IDE_LABEL, Buffer);

		SendDlgItemMessage(hdlg, IDE_COMMENT, EM_LIMITTEXT, sizeof(Info->Comment)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Comment);
		SetDlgItemText(hdlg, IDE_COMMENT, Buffer);

		if (HWND hwndList = GetDlgItem(hdlg, IDL_COLOR))
		{	TIMED_STRING_LIST_S *pList = &ActiveConfig.TrackColors;
			TCHAR uColor[sizeof(Info->Color)+1];
			int Did = 0;
			SendMessage(hwndList, CB_LIMITTEXT, sizeof(Info->Color)-1, 0);
			for (unsigned long c=0; c<pList->Count; c++)
			if (!pList->Entries[c].value)	/* On non-enabled ones */
			{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
				SendMessage(hwndList, CB_ADDSTRING, 0, (LPARAM) uColor);
				Did++;
			}
			if (Did)
			{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),Info->Color);
				if (SendMessage(hwndList, CB_SELECTSTRING, -1, (LPARAM) uColor) == CB_ERR)
				{	SendMessage(hwndList, CB_ADDSTRING, 0, (LPARAM) uColor);
					SendMessage(hwndList, CB_SELECTSTRING, -1, (LPARAM) uColor);
				}
			} else SendMessage(hdlg, WM_COMMAND, IDB_MORE, 0);
		}
		{	TCHAR Buffer[80];
			if (Info->DefinedBy[0])
				StringCbPrintf(Buffer, sizeof(Buffer), TEXT("From: %.*S"), STRING(Info->DefinedBy));
			else Buffer[0] = TEXT('\0');
			SetDlgItemText(hdlg, IDC_FROM, Buffer);
		}

		CenterWindow(hdlg);

		return TRUE;
	}
	case WM_DRAWITEM:
	if (wp == IDB_SYMBOL)
	{	RECT rc;
		POINT pt;
		DRAWITEMSTRUCT *di = (LPDRAWITEMSTRUCT) lp;
		pt.x = (di->rcItem.left+di->rcItem.right)/2;
		pt.y = (di->rcItem.bottom+di->rcItem.top)/2;
		Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
		BltSymbol(di->hDC, pt.x, pt.y, 2, 1,
					Info->Symbol.Table=='/'?0:Info->Symbol.Table=='\\'?0x1:((Info->Symbol.Table<<8)|0x1),
					Info->Symbol.Symbol-'!', 100, &rc);
	}
	break;

	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE Nickname ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDC_OVERRIDE:
			EnableWindow(GetDlgItem(hdlg,IDB_SYMBOL),IsDlgButtonChecked(hdlg, IDC_OVERRIDE));
			break;

		case IDE_LABEL:		/* Check label for uniqueness on change */
		if (HIWORD(wp) == EN_CHANGE)
		{	TCHAR *Station = (TCHAR*)malloc(sizeof(*Station) * 32);
			TCHAR *Label = (TCHAR*)malloc(sizeof(*Label) * 32);
			if ((GetDlgItemText(hdlg, IDE_CALLSIGN, Station, 32)))
			{	GetDlgItemText(hdlg, IDE_LABEL, Label, 32);
			if (SUCCEEDED(StringCbPrintfA(Info->Station, sizeof(Info->Station), "%S", Station))
				&& SUCCEEDED(StringCbPrintfA(Info->Label, sizeof(Info->Label), "%S", Label)))
					EnableWindow(GetDlgItem(hdlg, IDB_ACCEPT), !IsNicknameLabelInUse(Info->pConfig, Info->Label, Info->Station, FALSE));
			}
		}
		case IDE_CALLSIGN:	/* IDE_LABEL falls through here */
		//case IDE_LABEL:
		case IDE_COMMENT:
		{	HWND hwndCtl = (HWND) lp;
			switch (HIWORD(wp))
			{
//			case EN_CHANGE:	/* This recurses if you SetWindowText inside! */
			case EN_KILLFOCUS:
			{	TCHAR *Buffer = (TCHAR*)malloc(sizeof(Info->Comment)*sizeof(TCHAR));
				if (GetWindowText(hwndCtl, Buffer, sizeof(Info->Comment)))
				{	StringCbPrintfA(Info->Comment, sizeof(Info->Comment), "%S", Buffer);
					RtStrnTrim(sizeof(Info->Comment), Info->Comment);
					StringCbPrintf(Buffer, sizeof(Info->Comment)*sizeof(TCHAR), TEXT("%S"), Info->Comment);
					SetWindowText(hwndCtl, Buffer);
				}
				free(Buffer);
				break;
			}
			}
			return TRUE;
		}

		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_DELETE:
			if (MessageBox(hdlg, strchr(Info->Station,'*')?TEXT("Really Delete Wildcard Nickname?\n\nAll non-modified clones will also be deleted."):TEXT("Really Delete Nickname?"), TEXT("Nickname Config"), MB_ICONQUESTION | MB_YESNO) == IDYES)
				EndDialog(hdlg, IDB_DELETE);
			return TRUE;
		case IDB_SHOW:
			EndDialog(hdlg, IDB_SHOW);
			return TRUE;
		case IDB_ACCEPT:
		{	TCHAR *Station = (TCHAR*)malloc(sizeof(*Station)*32);
			TCHAR *Label = (TCHAR*)malloc(sizeof(*Label)*32);
			TCHAR *Comment = (TCHAR*)malloc(sizeof(*Comment)*128);
			TCHAR *Color = (TCHAR*)malloc(sizeof(*Color)*COLOR_SIZE);

			Info->Enabled = IsDlgButtonChecked(hdlg, IDC_ENABLED);
			Info->OverrideSymbol = IsDlgButtonChecked(hdlg, IDC_OVERRIDE);
			Info->MultiTrackNew = IsDlgButtonChecked(hdlg, IDC_MULTI_NEW);
			Info->MultiTrackActive = IsDlgButtonChecked(hdlg, IDC_MULTI_ACTIVE);
			Info->MultiTrackAlways = IsDlgButtonChecked(hdlg, IDC_MULTI_ALWAYS);
			Info->OverrideLabel = IsDlgButtonChecked(hdlg, IDC_OVERRIDE_LABEL);
			Info->OverrideComment = IsDlgButtonChecked(hdlg, IDC_OVERRIDE_COMMENT);
			Info->OverrideColor = IsDlgButtonChecked(hdlg, IDC_OVERRIDE_COLOR);

			if ((GetDlgItemText(hdlg, IDE_CALLSIGN, Station, 32)))
			{	GetDlgItemText(hdlg, IDE_LABEL, Label, 32);
				GetDlgItemText(hdlg, IDE_COMMENT, Comment, 128);
				GetDlgItemText(hdlg, IDL_COLOR, Color, COLOR_SIZE);
				if (SUCCEEDED(StringCbPrintfA(Info->Station, sizeof(Info->Station), "%S", Station))
				&& SUCCEEDED(StringCbPrintfA(Info->Label, sizeof(Info->Label), "%S", Label))
				&& SUCCEEDED(StringCbPrintfA(Info->Comment, sizeof(Info->Comment), "%S", Comment))
				&& SUCCEEDED(StringCbPrintfA(Info->Color, sizeof(Info->Color), "%S", Color)))
				{
#ifdef ENSURE_SINGLE_LABEL
					if (Info->Enabled
					&& IsNicknameLabelInUse(&ActiveConfig, Info->Label, Info->Station))
						MessageBox(hdlg, TEXT("Specified Label In Use"), TEXT("Duplicate Label"), MB_OK | MB_ICONWARNING);
					else
#endif
					memset(&Info->DefinedBy, 0, sizeof(Info->DefinedBy));
					EndDialog(hdlg, IDOK);
				} else MessageBox(hdlg, TEXT("Name/Comment/Color Format Failed"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			} else MessageBox(hdlg, TEXT("ID Required"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			free(Station); free(Label); free(Comment);
			return TRUE;
		}
		case IDB_SYMBOL:
		{	SYMBOL_INFO_S Working = Info->Symbol;
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SYMBOL), hdlg, SymbolDlgProc, (LPARAM)&Working) == IDOK)
			{	Info->Symbol = Working;
				InvalidateRect(hdlg,NULL,FALSE);
			}
			return TRUE;
		}
		case IDB_SEND:
			MessageBox(hdlg, TEXT("This Space Reserved For Future Expansion"), TEXT("Post No Bills"), MB_OK | MB_ICONINFORMATION);
			break;
		case IDB_MORE:
		if (HWND hwndList = GetDlgItem(hdlg, IDL_COLOR))
		{	TIMED_STRING_LIST_S *pList = &ActiveConfig.ColorChoices;
			TCHAR uColor[sizeof(Info->Color)+1];
			SendMessage(hwndList, CB_RESETCONTENT, 0, 0);
			SendMessage(hwndList, CB_LIMITTEXT, sizeof(Info->Color)-1, 0);
			for (unsigned long c=0; c<pList->Count; c++)
			{	StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),pList->Entries[c].string);
				SendMessage(hwndList, CB_ADDSTRING, 0, (LPARAM) uColor);
			}
			StringCbPrintf(uColor,sizeof(uColor),TEXT("%S"),Info->Color);
			if (SendMessage(hwndList, CB_SELECTSTRING, -1, (LPARAM) uColor) == CB_ERR)
			{	SendMessage(hwndList, CB_ADDSTRING, 0, (LPARAM) uColor);
				SendMessage(hwndList, CB_SELECTSTRING, -1, (LPARAM) uColor);
			}
		}
		break;
		}
		break;
	}
	return FALSE;
}

char *PromptNewNickname(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station)
{	NICKNAME_INFO_S Working = {0};
	char *Result = NULL;

	if (Station) strncpy(Working.Station, Station, sizeof(Working.Station));
	Working.Symbol.Table = '/';		/* Primary table */
	Working.Symbol.Symbol = '.';	/* Red X */
	Working.Enabled = TRUE;
	Working.OverrideLabel = TRUE;
	Working.pConfig = pConfig;
	if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_NICKNAME), hwnd, NicknameDlgProc, (LPARAM)&Working) == IDOK)
	{	NICKNAME_INFO_S *New = GetOrCreateNickname(pConfig, Working.Station, Working.Label, TRUE, FALSE);
		if (New)
		{	*New = Working;
			ValidateConfig(pConfig);
			RealSaveConfiguration(hwnd, pConfig, "User:NewNickname");
			Result = New->Station;
		}
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

char *PromptConfigNickname(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station)
{	char *Result = NULL;
	NICKNAME_INFO_S *pNick = GetOrCreateNickname(pConfig, Station, NULL, FALSE, FALSE);
	if (pNick)
	{	NICKNAME_INFO_S Working = *pNick;
		INT_PTR idEnd;
		Working.pConfig = pConfig;
		if ((idEnd=DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_NICKNAME), hwnd, NicknameDlgProc, (LPARAM)&Working)) == IDOK)
		{	if (strcmp(pNick->Station, Working.Station)	/* New Station? */
			&& !strcmp(pNick->Label, Working.Label))	/* Same nickname? */
			{	pNick->Enabled = FALSE;		/* Disable it, don't delete it */
				//RemoveNickname(pConfig, pNick->Station);	/* GONE! */
			}
			pNick = GetOrCreateNickname(pConfig, Working.Station, Working.Label, TRUE, FALSE);
			if (pNick)
			{	*pNick = Working;
				ValidateConfig(pConfig);
				RealSaveConfiguration(hwnd, pConfig, "User:Nickname");
				Result = pNick->Station;
			}
		} else if (idEnd == IDB_DELETE)
		{	RemoveNickname(pConfig, Working.Station);
			Result = _strdup(Working.Station);	/* I know, we leak this */
		}
	} else return PromptNewNickname(hwnd, pConfig, Station);
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

BOOL CALLBACK TileServerDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static TILE_SERVER_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (TILE_SERVER_INFO_S *) lp;
		TCHAR Buffer[255];

		CheckDlgButton(hdlg, IDC_ENABLED, Info->PurgeEnabled);
		CheckDlgButton(hdlg, IDC_STATUS, Info->SupportsStatus);

#define ENTRY_FIELD(id,f) \
		SendDlgItemMessage(hdlg, id, EM_LIMITTEXT, sizeof(Info->f)-1, 0); \
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->f); \
		SetDlgItemText(hdlg, id, Buffer)

		ENTRY_FIELD(IDE_NAME, Name);
		ENTRY_FIELD(IDE_SERVER, Server);
		ENTRY_FIELD(IDE_URL, URLPrefix);

#undef ENTRY_FIELD

		SetDlgItemInt(hdlg, IDE_PORT, Info->Port, FALSE);
		SendDlgItemMessage(hdlg, IDS_PORT, UDM_SETRANGE, 0, GetSpinRangeULong("TileServer.Port"));

		SetDlgItemInt(hdlg, IDE_DAYS, Info->RetainDays, FALSE);
		SendDlgItemMessage(hdlg, IDS_DAYS, UDM_SETRANGE, 0, GetSpinRangeULong("TileServer.RetainDays"));

		SetDlgItemInt(hdlg, IDE_ZOOM1, Info->MinServerZoom, FALSE);
		SendDlgItemMessage(hdlg, IDS_ZOOM1, UDM_SETRANGE, 0, GetSpinRangeLong("TileServer.MinServerZoom"));

		SetDlgItemInt(hdlg, IDE_ZOOM2, Info->RetainZoom, FALSE);
		SendDlgItemMessage(hdlg, IDS_ZOOM2, UDM_SETRANGE, 0, GetSpinRangeLong("TileServer.RetainZoom"));

		SetDlgItemInt(hdlg, IDE_ZOOM3, Info->MaxServerZoom, FALSE);
		SendDlgItemMessage(hdlg, IDS_ZOOM3, UDM_SETRANGE, 0, GetSpinRangeLong("TileServer.MaxServerZoom"));

		EnableWindow(GetDlgItem(hdlg, IDB_DELETE), FALSE);
		if (!*Info->Path)
			EnableWindow(GetDlgItem(hdlg, IDB_FLUSH), FALSE);

		CenterWindow(hdlg);

		return TRUE;
	}

	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE Object ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_ABOUT:
			MessageBox(hdlg, TEXT("Default OSM Maps are\n\n(c) OpenStreetMap contributors, CC-BY-SA"), TEXT("Tile Server Config"), MB_ICONINFORMATION | MB_OK);
			return TRUE;
		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_DELETE:
			if (MessageBox(hdlg, TEXT("Really Delete Tile Server?"), TEXT("Tile Server Config"), MB_ICONQUESTION | MB_YESNO) == IDYES)
				EndDialog(hdlg, IDB_DELETE);
			return TRUE;
		case IDB_FLUSH:
			if (*Info->Path)
			{	unsigned __int64 FileSpace, DiskSpace;
				unsigned long FileCount = OSMGetTileServerCacheStats(Info, &FileSpace, &DiskSpace);
				size_t Remaining = 256;
				TCHAR *Buffer = (TCHAR*)malloc(Remaining);
				StringCbPrintf(Buffer, Remaining, TEXT("Really Flush All %lu Tiles for [%S] (%I64lu/%I64lu bytes on Disk)?\n\n(Note: This may take a while...)"), 
								FileCount, Info->Name,
								FileSpace, DiskSpace);
		
				if (MessageBox(hdlg, Buffer, TEXT("Tile Server Config"), MB_ICONQUESTION | MB_YESNO) == IDYES)
				{	HWND hwnd = GetParent(hdlg);
					if (!hwnd) hwnd = GetDesktopWindow();
					FileCount = OSMFlushTileServerCache(Info, &FileSpace, &DiskSpace, hdlg);
					StringCbPrintf(Buffer, Remaining, TEXT("%ld Files Remain for [%S] (%I64lu/%I64lu bytes on Disk)"),
									FileCount, Info->Name,
									FileSpace, DiskSpace);
					MessageBox(hdlg, Buffer, TEXT("File Server Config"), MB_ICONINFORMATION | MB_OK);
				}
				free(Buffer);
			}
			return TRUE;
		case IDB_ACCEPT:
		{	TCHAR *Name = (TCHAR*)malloc(sizeof(*Name)*32);
			TCHAR *Server = (TCHAR*)malloc(sizeof(*Server)*128);
			TCHAR *URL = (TCHAR*)malloc(sizeof(*URL)*128);

			Info->Port = GetDlgItemInt(hdlg, IDE_PORT, NULL, FALSE);
			Info->SupportsStatus = IsDlgButtonChecked(hdlg, IDC_STATUS);
			Info->PurgeEnabled = IsDlgButtonChecked(hdlg, IDC_ENABLED);
			Info->RetainDays = GetDlgItemInt(hdlg, IDE_DAYS, NULL, FALSE);
			Info->RetainZoom = GetDlgItemInt(hdlg, IDE_ZOOM2, NULL, FALSE);
			Info->MinServerZoom = GetDlgItemInt(hdlg, IDE_ZOOM1, NULL, FALSE);
			Info->MaxServerZoom = GetDlgItemInt(hdlg, IDE_ZOOM3, NULL, FALSE);

			if ((GetDlgItemText(hdlg, IDE_NAME, Name, 32))
			&& (GetDlgItemText(hdlg, IDE_SERVER, Server, 128))
			&& (GetDlgItemText(hdlg, IDE_URL, URL, 128)))
			{	if (SUCCEEDED(StringCbPrintfA(Info->Name, sizeof(Info->Name), "%S", Name))
				&& SUCCEEDED(StringCbPrintfA(Info->Server, sizeof(Info->Server), "%S", Server))
				&& SUCCEEDED(StringCbPrintfA(Info->URLPrefix, sizeof(Info->URLPrefix), "%S", URL)))
				{	if (!*Info->Path)
					{	StringCbPrintfA(Info->Path, sizeof(Info->Path), "./%s/",
										Info->Name);
					}
					if (VerifyOSMPath(hdlg, Info, FALSE, NULL))
						EndDialog(hdlg, IDOK);
				} else MessageBox(hdlg, TEXT("Name/Server/URL Format Failed"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			} else MessageBox(hdlg, TEXT("Name/Server/URL Required"), TEXT("IDB_ACCEPT"), MB_OK | MB_ICONERROR);
			free(Name); free(Server); free(URL);
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}

BOOL PromptNewTileServer(HWND hwnd, CONFIG_INFO_S *pConfig)
{	TILE_SERVER_INFO_S Working = {0};
	BOOL Result;

	Working.Port = 80;
	Working.MinServerZoom = GetConfigMinLong("TileServer.MinServerZoom");
	Working.MaxServerZoom = GetConfigMaxLong("TileServer.MaxServerZoom");

	if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_TILE_SERVER), hwnd, TileServerDlgProc, (LPARAM)&Working) == IDOK)
	{	unsigned long i = pConfig->TileServers.Count++;
		pConfig->TileServers.Server = (TILE_SERVER_INFO_S *)realloc(pConfig->TileServers.Server,sizeof(*pConfig->TileServers.Server)*pConfig->TileServers.Count);

		pConfig->TileServers.Server[i] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:NewTileServer");
		Result = TRUE;
	} else Result = FALSE;
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

BOOL PromptConfigTileServer(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long id)
{	TILE_SERVER_INFO_S Working = pConfig->TileServers.Server[id];
	BOOL Result = FALSE;
	INT_PTR idEnd;

	if ((idEnd=DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_TILE_SERVER), hwnd, TileServerDlgProc, (LPARAM)&Working)) == IDOK)
	{
		pConfig->TileServers.Server[id] = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:TileServer");
		Result = TRUE;
	} else if (idEnd == IDB_DELETE)
	{	if (id < --pConfig->TileServers.Count)
			memmove(&pConfig->TileServers.Server[id], &pConfig->TileServers.Server[id+1],
					sizeof(pConfig->TileServers.Server[0])*(pConfig->TileServers.Count-id));
	}
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

BOOL CALLBACK ConfigDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static CONFIG_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (CONFIG_INFO_S *) lp;
		TCHAR Buffer[255];

		SendDlgItemMessage(hdlg, IDE_CALLSIGN, EM_LIMITTEXT, min(9,sizeof(Info->CallSign)-1), 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->CallSign);
		SetDlgItemText(hdlg, IDE_CALLSIGN, Buffer);

		SendDlgItemMessage(hdlg, IDE_PASSWORD, EM_LIMITTEXT, sizeof(Info->Password)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Password);
		SetDlgItemText(hdlg, IDE_PASSWORD, Buffer);

		SendDlgItemMessage(hdlg, IDE_FILTER, EM_LIMITTEXT, sizeof(Info->Filter)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Filter);
		SetDlgItemText(hdlg, IDE_FILTER, Buffer);

		SendDlgItemMessage(hdlg, IDE_COMMENT, EM_LIMITTEXT, sizeof(Info->Comment)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Comment);
		SetDlgItemText(hdlg, IDE_COMMENT, Buffer);

		SetDlgItemInt(hdlg, IDE_RANGE, Info->Range, FALSE);
		SetDlgItemInt(hdlg, IDE_QUIET_TIME, Info->QuietTime, FALSE);

		SendDlgItemMessage(hdlg, IDS_RANGE, UDM_SETRANGE, 0, GetSpinRangeULong("Range"));
		SendDlgItemMessage(hdlg, IDS_QUIET_TIME, UDM_SETRANGE, 0, GetSpinRangeULong("QuietTime"));

		CheckDlgButton(hdlg, IDC_VERIFY_CLOSE, Info->View.ConfirmOnClose);
		CheckDlgButton(hdlg, IDC_ABOUT_RESTART, Info->View.AboutRestart);

		CenterWindow(hdlg);

		return TRUE;
	}
	case WM_DRAWITEM:
	if (wp == IDB_SYMBOL)
	{	RECT rc;
		POINT pt;
		DRAWITEMSTRUCT *di = (LPDRAWITEMSTRUCT) lp;
		pt.x = (di->rcItem.left+di->rcItem.right)/2;
		pt.y = (di->rcItem.bottom+di->rcItem.top)/2;
		Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
		BltSymbol(di->hDC, pt.x, pt.y, 2, 1,
					Info->Symbol.Table=='/'?0:Info->Symbol.Table=='\\'?0x1:((Info->Symbol.Table<<8)|0x1),
					Info->Symbol.Symbol-'!', 100, &rc);
	}
	break;

	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_PHG:
		{	char *Result = MyGetDlgItemTextA(hdlg, IDE_COMMENT);
			BOOL HadOne = FALSE;
			if (Result)
			{	char *PHG = strstr(Result, "PHG");
				if (PHG && strlen(PHG)>=7
				&& isdigit(PHG[3]&0x0ff)
				&& isdigit(PHG[4]&0x0ff)
				&& isdigit(PHG[5]&0x0ff)
				&& isdigit(PHG[6]&0x0ff))
				{	memmove(Result, PHG+3, strlen(PHG+3)+1);
					Result[4] = '\0';
					HadOne = TRUE;
				} else
				{	free(Result);
					Result = NULL;
				}
			}
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_PHG), hdlg, PHGDlgProc, (LPARAM)&Result) == IDOK)
			{	char *Old = MyGetDlgItemTextA(hdlg, IDE_COMMENT);
				if (HadOne)
				{	char *PHG = strstr(Old, "PHG");
					memmove(PHG, PHG+7, strlen(PHG+7)+1);
				}
				char *New = (char*)malloc(3+strlen(Result)+strlen(Old)+1);
				sprintf(New, "PHG%s%s", Result, Old);
				MySetDlgItemTextA(hdlg, IDE_COMMENT, New);
				free(New); free(Old); free(Result);
			}
			return TRUE;
		}

		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_ACCEPT:
		{	myGetDlgItemText(hdlg, IDE_CALLSIGN, Info->CallSign, sizeof(Info->CallSign));
			SpaceCompress(STRING(Info->CallSign));
			ZeroSSID(Info->CallSign);

			{	char Temp[sizeof(Info->Password)];
				myGetDlgItemText(hdlg, IDE_PASSWORD, Temp, sizeof(Temp));
				if (strncmp(Temp, Info->Password, sizeof(Temp)))
				{	strncpy(Info->Password, Temp, sizeof(Info->Password));
					SpaceCompress(STRING(Info->Password));
					Info->SuppressUnverified = FALSE;
				}
			}

			myGetDlgItemText(hdlg, IDE_FILTER, Info->Filter, sizeof(Info->Filter));
			myGetDlgItemText(hdlg, IDE_COMMENT, Info->Comment, sizeof(Info->Comment));
			Info->Range = GetDlgItemInt(hdlg, IDE_RANGE, NULL, FALSE);
			Info->QuietTime = GetDlgItemInt(hdlg, IDE_QUIET_TIME, NULL, FALSE);
			Info->View.ConfirmOnClose = IsDlgButtonChecked(hdlg, IDC_VERIFY_CLOSE);
			Info->View.AboutRestart = IsDlgButtonChecked(hdlg, IDC_ABOUT_RESTART);
			{	FILTER_INFO_S Filter = {0};
				if (!OptimizeFilter(Info->Filter,&Filter))
				{	ShowTraceLog("FilterError");
					MessageBox(hdlg, TEXT("Invalid Filter Component, See FilterError Trace"), TEXT("Filter Error"), MB_ICONERROR | MB_OK);
					FreeFilter(&Filter);
					return TRUE;	/* Don't dismiss the dialog */
				}
				FreeFilter(&Filter);
			}
			EndDialog(hdlg, IDOK);
			return TRUE;
		}
		case IDB_GENIUS:
			PromptGenius(hdlg, Info);
			return TRUE;
#ifdef OBSOLETE
		case IDB_AGW:
			PromptAGW(hdlg, Info);
			return TRUE;
		case IDB_KISS:
			PromptKISS(hdlg, Info);
			return TRUE;
		case IDB_TEXT:
			PromptTEXT(hdlg, Info);
			return TRUE;
#endif
		case IDB_NMEA:
			PromptNMEA(hdlg, Info);
			return TRUE;

		case IDB_SYMBOL:
		{	SYMBOL_INFO_S Working = Info->Symbol;
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SYMBOL), hdlg, SymbolDlgProc, (LPARAM)&Working) == IDOK)
			{	Info->Symbol = Working;
				InvalidateRect(hdlg,NULL,FALSE);
			}
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}


BOOL CALLBACK StatusReportDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{static CONFIG_INFO_S *Info;

	switch (msg)
	{
	case WM_INITDIALOG:
	{	Info = (CONFIG_INFO_S *) lp;
		TCHAR Buffer[255];

		SetDlgItemInt(hdlg, IDE_COMMENT_INTERVAL, Info->CommentInterval, FALSE);
		SendDlgItemMessage(hdlg, IDS_COMMENT_INTERVAL, UDM_SETRANGE, 0, GetSpinRangeULong("CommentInterval"));

		SendDlgItemMessage(hdlg, IDE_COMMENT, EM_LIMITTEXT, sizeof(Info->Comment)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Comment);
		SetDlgItemText(hdlg, IDE_COMMENT, Buffer);

		SetDlgItemInt(hdlg, IDE_STATUS_INTERVAL, Info->Status.Interval, FALSE);
		SendDlgItemMessage(hdlg, IDS_STATUS_INTERVAL, UDM_SETRANGE, 0, GetSpinRangeULong("Status.Interval"));

		SendDlgItemMessage(hdlg, IDE_STATUS_TEXT, EM_LIMITTEXT, sizeof(Info->Status.Text)-1, 0);
		StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), Info->Status.Text);
		SetDlgItemText(hdlg, IDE_STATUS_TEXT, Buffer);

		CheckDlgButton(hdlg, IDC_ENABLED, Info->Status.Enabled);
		CheckDlgButton(hdlg, IDC_DX, Info->Status.DX);
		CheckDlgButton(hdlg, IDC_GRIDSQUARE, Info->Status.GridSquare);
		CheckDlgButton(hdlg, IDC_TIMESTAMP, Info->Status.Timestamp);

		CenterWindow(hdlg);

		return TRUE;
	}
	case WM_CLOSE:
		TraceLog("Config", TRUE, hdlg, "WM_CLOSE ending with Cancel\n");
		EndDialog(hdlg, IDCANCEL);
		return 0;

	case WM_COMMAND:
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		switch (LOWORD(wp))
		{
		case IDB_PHG:
		{	char *Result = MyGetDlgItemTextA(hdlg, IDE_COMMENT);
			BOOL HadOne = FALSE;
			if (Result)
			{	char *PHG = strstr(Result, "PHG");
				if (PHG && strlen(PHG)>=7
				&& isdigit(PHG[3]&0x0ff)
				&& isdigit(PHG[4]&0x0ff)
				&& isdigit(PHG[5]&0x0ff)
				&& isdigit(PHG[6]&0x0ff))
				{	memmove(Result, PHG+3, strlen(PHG+3)+1);
					Result[4] = '\0';
					HadOne = TRUE;
				} else
				{	free(Result);
					Result = NULL;
				}
			}
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_PHG), hdlg, PHGDlgProc, (LPARAM)&Result) == IDOK)
			{	char *Old = MyGetDlgItemTextA(hdlg, IDE_COMMENT);
				if (HadOne)
				{	char *PHG = strstr(Old, "PHG");
					memmove(PHG, PHG+7, strlen(PHG+7)+1);
				}
				char *New = (char*)malloc(3+strlen(Result)+strlen(Old)+1);
				sprintf(New, "PHG%s%s", Result, Old);
				MySetDlgItemTextA(hdlg, IDE_COMMENT, New);
				free(New); free(Old); free(Result);
			}
			return TRUE;
		}

		case IDB_CANCEL:
			EndDialog(hdlg, IDCANCEL);
			return TRUE;
		case IDB_ACCEPT:
		{
			myGetDlgItemText(hdlg, IDE_COMMENT, Info->Comment, sizeof(Info->Comment));
			Info->CommentInterval = GetDlgItemInt(hdlg, IDE_COMMENT_INTERVAL, NULL, FALSE);

			myGetDlgItemText(hdlg, IDE_STATUS_TEXT, Info->Status.Text, sizeof(Info->Status.Text));
			Info->Status.Interval = GetDlgItemInt(hdlg, IDE_STATUS_INTERVAL, NULL, FALSE);

			Info->Status.Enabled = IsDlgButtonChecked(hdlg, IDC_ENABLED);
			Info->Status.DX = IsDlgButtonChecked(hdlg, IDC_DX);
			Info->Status.GridSquare = IsDlgButtonChecked(hdlg, IDC_GRIDSQUARE);
			Info->Status.Timestamp = IsDlgButtonChecked(hdlg, IDC_TIMESTAMP);

			EndDialog(hdlg, IDOK);
			return TRUE;
		}
		}
		break;
	}
	return FALSE;
}


static BOOL LoadConfiguration(HWND hwnd, CONFIG_INFO_S *pConfig, BOOL AllowCreate)
{	BOOL Result = FALSE;

	if (LoadXmlConfiguration(hwnd, pConfig, AllowCreate))
		return TRUE;

	char *CfgFile = GetConfigFile(hwnd);
	FILE *In = fopen(CfgFile,"rt");

	DefineDefaultConfig(pConfig);
	if (In)
	{	char InBuf[256], *e;

		if (fgets(InBuf, sizeof(InBuf), In))
			strncpy(pConfig->CallSign, TrimEnd(InBuf), sizeof(pConfig->CallSign));
		if (fgets(InBuf, sizeof(InBuf), In))
			strncpy(pConfig->Password, TrimEnd(InBuf), sizeof(pConfig->Password));
		if (fgets(InBuf, sizeof(InBuf), In))
			strncpy(pConfig->Filter, TrimEnd(InBuf), sizeof(pConfig->Filter));
		if (fgets(InBuf, sizeof(InBuf), In))
			strncpy(pConfig->Comment, TrimEnd(InBuf), sizeof(pConfig->Comment));
		if (fgets(InBuf, sizeof(InBuf), In))
			pConfig->Range = atoi(TrimEnd(InBuf));
		else MessageBox(hwnd, TEXT("Failed To Read Config File"), TEXT("Load Configuration"), MB_OK | MB_ICONERROR);
		Result = !ferror(In);					/* The preceding is all that is required */

		if (fgets(InBuf, sizeof(InBuf), In))
			pConfig->MyGenius.MinTime = atoi(TrimEnd(InBuf));
		if (fgets(InBuf, sizeof(InBuf), In))
			pConfig->MyGenius.MaxTime = atoi(TrimEnd(InBuf));
		if (fgets(InBuf, sizeof(InBuf), In))
			pConfig->QuietTime = atoi(TrimEnd(InBuf));
		if (fgets(InBuf, sizeof(InBuf), In))
			pConfig->MyGenius.BearingChange = atoi(TrimEnd(InBuf));
		if (fgets(InBuf, sizeof(InBuf), In))
			pConfig->MyGenius.ForecastError = strtod(TrimEnd(InBuf), &e);
		if (fgets(InBuf, sizeof(InBuf), In))
			pConfig->MyGenius.MaxDistance = strtod(TrimEnd(InBuf), &e);
		fclose(In);
		ValidateConfig(pConfig);
	}
	return Result;
}

BOOL RealSaveConfiguration(HWND hwnd, CONFIG_INFO_S *pConfig, char *Why)
{	BOOL Result = FALSE;
	char *CfgFile = GetConfigFile(hwnd);
#ifdef OLD_WAY
	FILE *Out = fopen(CfgFile,"wt");

	if (Out)
	{	fprintf(Out, "%.*s\n", sizeof(pConfig->CallSign), pConfig->CallSign);
		fprintf(Out, "%.*s\n", sizeof(pConfig->Password), pConfig->Password);
		fprintf(Out, "%.*s\n", sizeof(pConfig->Filter), pConfig->Filter);
		fprintf(Out, "%.*s\n", sizeof(pConfig->Comment), pConfig->Comment);
		fprintf(Out, "%ld\n", pConfig->Range);
		fprintf(Out, "%ld\n", pConfig->Genius.MinTime);
		fprintf(Out, "%ld\n", pConfig->Genius.MaxTime);
		fprintf(Out, "%ld\n", pConfig->QuietTime);
		fprintf(Out, "%ld\n", pConfig->Genius.BearingChange);
		fprintf(Out, "%.2lf\n", pConfig->Genius.ForecastError);
		fprintf(Out, "%.2lf\n", pConfig->Genius.MaxDistance);
		Result = !ferror(Out);
		fclose(Out);
	}
#else
	TCHAR Buffer[MAX_PATH];
	StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%S"), CfgFile);
	DeleteFile(Buffer);
#endif
	GetSystemTime(&pConfig->stLastSaved);
	strncpy(pConfig->LastWhy, Why?Why:"*NULL*", sizeof(pConfig->LastWhy));
	return SaveXmlConfiguration(hwnd, pConfig, Why);
}

BOOL PromptConfiguration(HWND hwnd, CONFIG_INFO_S *pConfig)
{	CONFIG_INFO_S Working = *pConfig;
	BOOL Result = FALSE;
	if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_CONFIG), hwnd, ConfigDlgProc, (LPARAM)&Working) == IDOK)
	{	*pConfig = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:General");
		Result = TRUE;
	}
	else TraceError(hwnd, "IDD_CONFIG not OK - LastError=%ld\n", GetLastError());
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

typedef struct PATH_CONFIG_DLG_INFO_S
{	HWND hwnd;
	BOOL Expert;	/* TRUE if Expert mode (all controls enabled) */
	BOOL Default;	/* TRUE if settings can slam to defaults */
	int msgRefresh;
	BOOL Initialized;
	PATH_CONFIG_INFO_S Original;
	PATH_CONFIG_INFO_S *pLive;
} PATH_CONFIG_DLG_INFO_S;	

static BOOL CALLBACK PathConfigDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
#ifdef UNDER_CE
	PATH_CONFIG_DLG_INFO_S *DlgInfo = (PATH_CONFIG_DLG_INFO_S *) GetWindowLong(hdlg, DWL_USER);
#else
	PATH_CONFIG_DLG_INFO_S *DlgInfo = (PATH_CONFIG_DLG_INFO_S *) GetWindowLongPtr(hdlg, DWLP_USER);
#endif
	PATH_CONFIG_INFO_S *Info = DlgInfo?DlgInfo->pLive:NULL;


#define WM_SET_CONTROLS WM_USER+42

	switch (msg)
	{
	case WM_INITDIALOG:
		DlgInfo = (PATH_CONFIG_DLG_INFO_S *) lp;
#ifdef UNDER_CE
		SetWindowLong(hdlg, DWL_USER, (LONG) DlgInfo);
#else
		SetWindowLongPtr(hdlg, DWLP_USER, (LONG) DlgInfo);
#endif
		Info = DlgInfo->pLive;
//		CenterWindow(hdlg);
		AlignWindowLeft(hdlg);

	case WM_SET_CONTROLS:	/* WM_INITDIALOG falls into here */
		DlgInfo->Initialized = FALSE;

		{	int TitleLen = GetWindowTextLength(DlgInfo->hwnd)+1;
			TCHAR *Title = (TCHAR*)malloc(sizeof(*Title)*TitleLen);
			GetWindowText(DlgInfo->hwnd,Title,TitleLen);
			int MyTitleLen = TitleLen + 80;
			TCHAR *MyTitle = (TCHAR*)malloc(sizeof(*MyTitle)*MyTitleLen);
			StringCbPrintf(MyTitle, MyTitleLen, TEXT("Paths(%s)"), Title);
			SetWindowText(hdlg, MyTitle);
			free(MyTitle);
			free(Title);
		}

		if (!Info->ReasonabilityLimit)
			SetDlgItemText(hdlg, IDB_MAX_LENGTH, TEXT("None"));
		else
		{	TCHAR Buffer[33];
			StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%ld%S"),
							(long) Info->ReasonabilityLimit,
							Info->ReasonabilityLimit>=1000?"":(ActiveConfig.View.Metric.Distance?"km":"mi"));
			SetDlgItemText(hdlg, IDB_MAX_LENGTH, Buffer);
		}

		CheckDlgButton(hdlg, IDC_STATION_PACKET, Info->Station);
		CheckDlgButton(hdlg, IDC_SHOW_MY_PACKET, Info->MyStation);
		CheckDlgButton(hdlg, IDC_NETWORK_LINKS, Info->Network);
		CheckDlgButton(hdlg, IDC_SHOW_RF_ONLY, Info->LclRF);
		CheckDlgButton(hdlg, IDC_SHOW_ALL, Info->ShowAllLinks);

		SetDlgItemInt(hdlg, IDE_WIDTH_PACKET, Info->Packet.Width, FALSE);
		SendDlgItemMessage(hdlg, IDS_WIDTH_PACKET, UDM_SETRANGE, 0, MAKELONG(LINE_WIDTH_MAX,LINE_WIDTH_MIN));
		SetupColorCombo(hdlg, IDL_COLOR_PACKET, COLORS_ALL, 0, Info->Packet.Color);

		SetDlgItemInt(hdlg, IDE_OPACITY, Info->Opacity, FALSE);
		SendDlgItemMessage(hdlg, IDS_OPACITY, UDM_SETRANGE, 0, GetSpinRangeULong("Screen.Paths.Opacity"));

		CheckDlgButton(hdlg, IDC_TYPE_DIRECT, Info->Direct.Enabled);
		CheckDlgButton(hdlg, IDC_TYPE_FIRST, Info->First.Enabled);
		CheckDlgButton(hdlg, IDC_TYPE_MIDDLE, Info->Middle.Enabled);
		CheckDlgButton(hdlg, IDC_TYPE_FINAL, Info->Final.Enabled);

		SetDlgItemInt(hdlg, IDE_WIDTH_DIRECT, Info->Direct.Width, FALSE);
		SetDlgItemInt(hdlg, IDE_WIDTH_FIRST, Info->First.Width, FALSE);
		SetDlgItemInt(hdlg, IDE_WIDTH_MIDDLE, Info->Middle.Width, FALSE);
		SetDlgItemInt(hdlg, IDE_WIDTH_FINAL, Info->Final.Width, FALSE);
		SendDlgItemMessage(hdlg, IDS_WIDTH_DIRECT, UDM_SETRANGE, 0, MAKELONG(LINE_WIDTH_MAX,LINE_WIDTH_MIN));
		SendDlgItemMessage(hdlg, IDS_WIDTH_FIRST, UDM_SETRANGE, 0, MAKELONG(LINE_WIDTH_MAX,LINE_WIDTH_MIN));
		SendDlgItemMessage(hdlg, IDS_WIDTH_MIDDLE, UDM_SETRANGE, 0, MAKELONG(LINE_WIDTH_MAX,LINE_WIDTH_MIN));
		SendDlgItemMessage(hdlg, IDS_WIDTH_FINAL, UDM_SETRANGE, 0, MAKELONG(LINE_WIDTH_MAX,LINE_WIDTH_MIN));
		SetupColorCombo(hdlg, IDL_COLOR_DIRECT, COLORS_ALL, 0, Info->Direct.Color);
		SetupColorCombo(hdlg, IDL_COLOR_FIRST, COLORS_ALL, 0, Info->First.Color);
		SetupColorCombo(hdlg, IDL_COLOR_MIDDLE, COLORS_ALL, 0, Info->Middle.Color);
		SetupColorCombo(hdlg, IDL_COLOR_FINAL, COLORS_ALL, 0, Info->Final.Color);

		SetDlgItemInt(hdlg, IDE_TIME_FLASH, Info->Flash.Time, FALSE);
		SetDlgItemInt(hdlg, IDE_TIME_SHORT, Info->Short.Time, FALSE);
		SetDlgItemInt(hdlg, IDE_TIME_MEDIUM, Info->Medium.Time/60, FALSE);
		SetDlgItemInt(hdlg, IDE_TIME_LONG, Info->Long.Time/60, FALSE);
		SendDlgItemMessage(hdlg, IDS_TIME_FLASH, UDM_SETRANGE, 0, MAKELONG(120, 1));
		SendDlgItemMessage(hdlg, IDS_TIME_SHORT, UDM_SETRANGE, 0, MAKELONG(600, 60));
		SendDlgItemMessage(hdlg, IDS_TIME_MEDIUM, UDM_SETRANGE, 0, MAKELONG(30, 1));
		SendDlgItemMessage(hdlg, IDS_TIME_LONG, UDM_SETRANGE, 0, MAKELONG(120, 10));

		CheckDlgButton(hdlg, IDC_TIME_ALL, FALSE);
		CheckDlgButton(hdlg, IDC_TIME_ONE, FALSE);
		CheckDlgButton(hdlg, IDC_TIME_FLASH, FALSE);
		CheckDlgButton(hdlg, IDC_TIME_SHORT, FALSE);
		CheckDlgButton(hdlg, IDC_TIME_MEDIUM, FALSE);
		CheckDlgButton(hdlg, IDC_TIME_LONG, FALSE);
		if (Info->MaxAge == -1)
		{	Info->All.Selected = TRUE;
			CheckDlgButton(hdlg, IDC_TIME_ALL, Info->All.Selected);
		} else if (Info->MaxAge == 0)
		{	Info->Last.Selected = TRUE;
			CheckDlgButton(hdlg, IDC_TIME_ONE, Info->Last.Selected);
		} else if (Info->MaxAge <= Info->Flash.Time)
		{	Info->Flash.Selected = TRUE;
			CheckDlgButton(hdlg, IDC_TIME_FLASH, Info->Flash.Selected);
		} else if (Info->MaxAge <= Info->Short.Time)
		{	Info->Short.Selected = TRUE;
			CheckDlgButton(hdlg, IDC_TIME_SHORT, Info->Short.Selected);
		} else if (Info->MaxAge <= Info->Medium.Time)
		{	Info->Medium.Selected = TRUE;
			CheckDlgButton(hdlg, IDC_TIME_MEDIUM, Info->Medium.Selected);
		} else if (Info->MaxAge <= Info->Long.Time)
		{	Info->Long.Selected = TRUE;
			CheckDlgButton(hdlg, IDC_TIME_LONG, Info->Long.Selected);
		} else CheckDlgButton(hdlg, IDC_TIME_LONG, TRUE);

		EnableWindow(GetDlgItem(hdlg,IDE_OPACITY),DlgInfo->Expert);
		EnableWindow(GetDlgItem(hdlg,IDB_MAX_LENGTH),DlgInfo->Expert);

#define DOSPIN(s) \
		EnableWindow(GetDlgItem(hdlg,IDE_##s),DlgInfo->Expert); \
		EnableWindow(GetDlgItem(hdlg,IDS_##s),DlgInfo->Expert)

		DOSPIN(TIME_FLASH);
		DOSPIN(TIME_SHORT);
		DOSPIN(TIME_MEDIUM);
		DOSPIN(TIME_LONG);

#define DOCOLOR(c) \
		DOSPIN(WIDTH_##c); \
		EnableWindow(GetDlgItem(hdlg,IDL_COLOR_##c),DlgInfo->Expert)

		DOCOLOR(PACKET);
		DOCOLOR(DIRECT);
		DOCOLOR(FIRST);
		DOCOLOR(MIDDLE);
		DOCOLOR(FINAL);
#undef DOCOLOR
#undef DOSPIN

		DlgInfo->Initialized = TRUE;

		return TRUE;

#define HANDLE_CHECK(f) if (HIWORD(wp)==BN_CLICKED) f=(SendMessage((HWND)lp,BM_GETCHECK,0,0)==BST_CHECKED); break
#define HANDLE_INT(f) if (HIWORD(wp)==EN_UPDATE) f=GetDlgItemInt(hdlg,LOWORD(wp),NULL,FALSE); break;
#define HANDLE_COLOR(f) if (HIWORD(wp)==CBN_SELCHANGE) GetSelectedColor(hdlg,LOWORD(wp),sizeof(f),f); break;
#define HANDLE_TIME_C(t) \
	if (Info->t.Selected=(SendMessage((HWND)lp,BM_GETCHECK,0,0)==BST_CHECKED)) \
		Info->MaxAge = Info->t.Time; \
	break;
#define HANDLE_TIME_E(t,m) \
	Info->t.Time = GetDlgItemInt(hdlg,LOWORD(wp),NULL,FALSE)*m; \
	if (Info->t.Selected) Info->MaxAge = Info->t.Time; \
	break;

	case WM_COMMAND:
#ifndef UNDER_CE
		if (DlgInfo && !IsWindow(DlgInfo->hwnd))
			DestroyWindow(hdlg);	/* Protect against my parent going away */
		else
#endif
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		if (DlgInfo && DlgInfo->Initialized)
		switch (LOWORD(wp))
		{
		case IDC_STATION_PACKET:	HANDLE_CHECK(Info->Station);
		case IDC_SHOW_MY_PACKET:	HANDLE_CHECK(Info->MyStation);
		case IDC_NETWORK_LINKS:		HANDLE_CHECK(Info->Network);
		case IDC_SHOW_RF_ONLY:		HANDLE_CHECK(Info->LclRF);
		case IDC_SHOW_ALL:			HANDLE_CHECK(Info->ShowAllLinks);

		case IDE_OPACITY:			HANDLE_INT(Info->Opacity);

		case IDE_WIDTH_PACKET:		HANDLE_INT(Info->Packet.Width);

		case IDL_COLOR_PACKET:		HANDLE_COLOR(Info->Packet.Color);

		case IDC_TYPE_DIRECT:		HANDLE_CHECK(Info->Direct.Enabled);
		case IDC_TYPE_FIRST:		HANDLE_CHECK(Info->First.Enabled);
		case IDC_TYPE_MIDDLE:		HANDLE_CHECK(Info->Middle.Enabled);
		case IDC_TYPE_FINAL:		HANDLE_CHECK(Info->Final.Enabled);

		case IDE_WIDTH_DIRECT:		HANDLE_INT(Info->Direct.Width);
		case IDE_WIDTH_FIRST:		HANDLE_INT(Info->First.Width);
		case IDE_WIDTH_MIDDLE:		HANDLE_INT(Info->Middle.Width);
		case IDE_WIDTH_FINAL:		HANDLE_INT(Info->Final.Width);

		case IDL_COLOR_DIRECT:		HANDLE_COLOR(Info->Direct.Color);
		case IDL_COLOR_FIRST:		HANDLE_COLOR(Info->First.Color);
		case IDL_COLOR_MIDDLE:		HANDLE_COLOR(Info->Middle.Color);
		case IDL_COLOR_FINAL:		HANDLE_COLOR(Info->Final.Color);

		case IDE_TIME_FLASH:		HANDLE_TIME_E(Flash,1);
		case IDE_TIME_SHORT:		HANDLE_TIME_E(Short,1);
		case IDE_TIME_MEDIUM:		HANDLE_TIME_E(Medium,60);
		case IDE_TIME_LONG:			HANDLE_TIME_E(Long,60);

		case IDC_TIME_ALL:			HANDLE_TIME_C(All);
		case IDC_TIME_ONE:			HANDLE_TIME_C(Last);
		case IDC_TIME_FLASH:		HANDLE_TIME_C(Flash);
		case IDC_TIME_SHORT:		HANDLE_TIME_C(Short);
		case IDC_TIME_MEDIUM:		HANDLE_TIME_C(Medium);
		case IDC_TIME_LONG:			HANDLE_TIME_C(Long);
#undef HANDLE_CHECK
#undef HANDLE_INT
#undef HANDLE_COLOR

		case IDB_MAX_LENGTH:
			Info->ReasonabilityLimit = NumberPrompt(hdlg, "Maximum Reasonable Path", "0=Unlimited", ActiveConfig.View.Metric.Distance?"km":"mi", "Screen.Paths.ReasonabilityLimit", Info->ReasonabilityLimit);
			PostMessage(hdlg, WM_SET_CONTROLS, 0, 0);
			break;
		case IDB_EXPERT:
			DlgInfo->Expert = !DlgInfo->Expert;
			PostMessage(hdlg, WM_SET_CONTROLS, 0, 0);
			break;
		case IDB_UNDO:
			*Info = DlgInfo->Original;
			PostMessage(hdlg, WM_SET_CONTROLS, 0, 0);
			break;
		case IDB_DEFAULT:
		{	*Info = DefaultPathConfig;
			PostMessage(hdlg, WM_SET_CONTROLS, 0, 0);
			break;
		}
		case IDB_CANCEL:
			*Info = DlgInfo->Original;
#ifdef UNDER_CE
			EndDialog(hdlg, IDCANCEL);
#else
			DestroyWindow(hdlg);
#endif
			return FALSE;
		case IDB_ACCEPT:
		{	if (DlgInfo->Default
			&& MessageBox(hdlg,TEXT("Save Settings across Restart?"), TEXT("Configure Paths"), MB_YESNO | MB_ICONQUESTION) == IDYES)
				ActiveConfig.Screen.Paths = *Info;
#ifdef UNDER_CE
			EndDialog(hdlg, IDOK);
#else
			DestroyWindow(hdlg);
#endif
			return FALSE;
		}
		}
		if (Info) Info->RGBFixed = FALSE;	/* Force a color lookup */
		if (DlgInfo) PostMessage(DlgInfo->hwnd, DlgInfo->msgRefresh, 0, 0);
		break;
	case WM_CLOSE:
#ifdef UNDER_CE
		EndDialog(hdlg, IDCANCEL);
#else
		DestroyWindow(hdlg);
#endif
		return 0;
#ifndef UNDER_CE
	case WM_DESTROY:
		gModelessDialog = NULL;
		if (DlgInfo) free(DlgInfo);
		break;
	case WM_ACTIVATE:
		if (!wp) gModelessDialog = NULL;
		else gModelessDialog = hdlg;
		break;
#endif
	}
	return FALSE;
#undef WM_SET_CONTROLS
}

HWND PromptPathConfig(HWND hwnd, int msgRefresh, PATH_CONFIG_INFO_S *pPaths, BOOL Default)
{	PATH_CONFIG_DLG_INFO_S *Working = (PATH_CONFIG_DLG_INFO_S *)calloc(1,sizeof(*Working));
	BOOL Result = FALSE;

	Working->hwnd = hwnd;
	Working->msgRefresh = msgRefresh;
	Working->Default = Default;
	Working->Initialized = FALSE;
	Working->Original = *pPaths;
	Working->pLive = pPaths;

#ifdef UNDER_CE
	DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_PATHS), hwnd, PathConfigDlgProc, (LPARAM)Working);
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return NULL;
#else
	HWND hwndDlg = CreateDialogParam(g_hInstance, MAKEINTRESOURCE(IDD_PATHS), hwnd, PathConfigDlgProc, (LPARAM)Working);
	ShowWindow(hwndDlg, SW_SHOW);
	return hwndDlg;
#endif
}

typedef struct OVERLAY_CONFIG_DLG_INFO_S
{	HWND hwnd;
	BOOL Default;	/* TRUE if settings can slam to defaults */
	int msgRefresh;
	BOOL Initialized;
	OVERLAY_CONFIG_INFO_S Original;
	OVERLAY_CONFIG_INFO_S *pLive;
} OVERLAY_CONFIG_DLG_INFO_S;	

static BOOL CALLBACK OverlayConfigDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
#ifdef UNDER_CE
	OVERLAY_CONFIG_DLG_INFO_S *DlgInfo = (OVERLAY_CONFIG_DLG_INFO_S *) GetWindowLong(hdlg, DWL_USER);
#else
	OVERLAY_CONFIG_DLG_INFO_S *DlgInfo = (OVERLAY_CONFIG_DLG_INFO_S *) GetWindowLongPtr(hdlg, DWLP_USER);
#endif
	OVERLAY_CONFIG_INFO_S *Info = DlgInfo?DlgInfo->pLive:NULL;

#ifndef WM_SET_CONTROLS
#define WM_SET_CONTROLS WM_USER+42
#endif

	switch (msg)
	{
	case WM_INITDIALOG:
		DlgInfo = (OVERLAY_CONFIG_DLG_INFO_S *) lp;
#ifdef UNDER_CE
		SetWindowLong(hdlg, DWL_USER, (LONG) DlgInfo);
#else
		SetWindowLongPtr(hdlg, DWLP_USER, (LONG) DlgInfo);
#endif
		Info = DlgInfo->pLive;
//		CenterWindow(hdlg);
		AlignWindowLeft(hdlg);

	case WM_SET_CONTROLS:	/* WM_INITDIALOG falls into here */
		DlgInfo->Initialized = FALSE;

		{	char *Type;
			int MyTitleLen = sizeof(Info->FileName)+80;
			TCHAR *MyTitle = (TCHAR*)malloc(sizeof(*MyTitle)*MyTitleLen);
			switch (Info->Type)
			{
			case 'P':	Type = "POS"; break;
			case 'G':	Type = "GPX"; break;
			default: Type = "???";
			}
//			char *Name1 = strrchr(Info->FileName,'\\');
//			char *Name2 = strrchr(Info->FileName,'/');
//			if (Name1) Name1++; if (Name2) Name2++;	/* Skip over slashes */
//			if (!Name1 || Name2 > Name1) Name1 = Name2;
//			if (!Name1) Name1 = Info->FileName;
			StringCbPrintf(MyTitle, MyTitleLen, TEXT("%S:%S"), Type, LocateFilename(Info->FileName));
			SetWindowText(hdlg, MyTitle);

			int Points=-1, Routes=-1, Tracks=-1;
			GetOverlayCounts(Info, &Points, &Routes, &Tracks);
			StringCbPrintf(MyTitle, MyTitleLen, TEXT("Points (%ld)"), Points);
			SetDlgItemText(hdlg, IDC_BOX_POINTS, MyTitle);
			StringCbPrintf(MyTitle, MyTitleLen, TEXT("Routes (%ld)"), Routes);
			SetDlgItemText(hdlg, IDC_BOX_ROUTES, MyTitle);
			StringCbPrintf(MyTitle, MyTitleLen, TEXT("Tracks (%ld)"), Tracks);
			SetDlgItemText(hdlg, IDC_BOX_TRACKS, MyTitle);

			if (!Points)
			{	CheckDlgButton(hdlg, IDC_SHOW_POINTS, FALSE);
				EnableWindow(GetDlgItem(hdlg,IDC_SHOW_POINTS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDC_LABEL_POINTS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDC_SHOW_SYMBOL_POINTS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDC_FORCE_SYMBOL_POINTS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDB_SYMBOL_POINTS),FALSE);
			}

			if (!Routes)
			{	CheckDlgButton(hdlg, IDC_SHOW_ROUTES, FALSE);
				EnableWindow(GetDlgItem(hdlg,IDC_SHOW_ROUTES),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDC_LABEL_ROUTES),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDC_SHOW_SYMBOL_ROUTES),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDE_WIDTH_ROUTES),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDS_WIDTH_ROUTES),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDE_OPACITY_ROUTES),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDS_OPACITY_ROUTES),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDL_COLOR_ROUTES),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDB_SYMBOL_ROUTES),FALSE);
			}

			if (!Tracks)
			{	CheckDlgButton(hdlg, IDC_SHOW_TRACKS, FALSE);
				EnableWindow(GetDlgItem(hdlg,IDC_SHOW_TRACKS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDC_LABEL_TRACKS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDC_SHOW_SYMBOL_TRACKS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDE_WIDTH_TRACKS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDS_WIDTH_TRACKS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDE_OPACITY_TRACKS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDS_OPACITY_TRACKS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDL_COLOR_TRACKS),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDB_SYMBOL_TRACKS),FALSE);
			}

			free(MyTitle);
		}

		CheckDlgButton(hdlg, IDC_VISIBLE, Info->Enabled);

		CheckDlgButton(hdlg, IDC_LABEL_NONE, !Info->Label.Enabled);
		CheckDlgButton(hdlg, IDC_LABEL_ID, Info->Label.ID);
		CheckDlgButton(hdlg, IDC_LABEL_COMMENT, Info->Label.Comment);
		CheckDlgButton(hdlg, IDC_LABEL_STATUS, Info->Label.Status);
		CheckDlgButton(hdlg, IDC_LABEL_ALTITUDE, Info->Label.Altitude);
		CheckDlgButton(hdlg, IDC_LABEL_TIME, Info->Label.Timestamp);
		CheckDlgButton(hdlg, IDC_LABEL_NODATE, Info->Label.SuppressDate);

		CheckDlgButton(hdlg, IDC_SHOW_POINTS, Info->Waypoint.Enabled);
		CheckDlgButton(hdlg, IDC_LABEL_POINTS, Info->Waypoint.Label);
		CheckDlgButton(hdlg, IDC_SHOW_SYMBOL_POINTS, Info->Waypoint.Symbol.Show);
		CheckDlgButton(hdlg, IDC_FORCE_SYMBOL_POINTS, Info->Waypoint.Symbol.Force);
		/* IDB_SYMBOL_POINTS - Info->Waypoint.Symbol.Symbol */

		CheckDlgButton(hdlg, IDC_SHOW_ROUTES, Info->Route.Enabled);
		CheckDlgButton(hdlg, IDC_LABEL_ROUTES, Info->Route.Label);
		CheckDlgButton(hdlg, IDC_SHOW_SYMBOL_ROUTES, Info->Route.Symbol.Show);
		/* IDB_SYMBOL_ROUTES - Info->Route.Symbol.Symbol */
		SetDlgItemInt(hdlg, IDE_OPACITY_ROUTES, Info->Route.Line.Opacity, FALSE);
		SendDlgItemMessage(hdlg, IDS_OPACITY_ROUTES, UDM_SETRANGE, 0, GetSpinRangeULong("Overlays.Route.Line.Opacity"));
		SetDlgItemInt(hdlg, IDE_WIDTH_ROUTES, Info->Route.Line.Width, FALSE);
		SendDlgItemMessage(hdlg, IDS_WIDTH_ROUTES, UDM_SETRANGE, 0, MAKELONG(LINE_WIDTH_MAX,LINE_WIDTH_MIN));
		SetupColorCombo(hdlg, IDL_COLOR_ROUTES, COLORS_ALL, 0, Info->Route.Line.Color);

		CheckDlgButton(hdlg, IDC_SHOW_TRACKS, Info->Track.Enabled);
		CheckDlgButton(hdlg, IDC_LABEL_TRACKS, Info->Track.Label);
		CheckDlgButton(hdlg, IDC_SHOW_SYMBOL_TRACKS, Info->Track.Symbol.Show);
		/* IDB_SYMBOL_TRACKS - Info->Track.Symbol.Symbol */
		SetDlgItemInt(hdlg, IDE_OPACITY_TRACKS, Info->Track.Line.Opacity, FALSE);
		SendDlgItemMessage(hdlg, IDS_OPACITY_TRACKS, UDM_SETRANGE, 0, GetSpinRangeULong("Overlays.Track.Line.Opacity"));
		SetDlgItemInt(hdlg, IDE_WIDTH_TRACKS, Info->Track.Line.Width, FALSE);
		SendDlgItemMessage(hdlg, IDS_WIDTH_TRACKS, UDM_SETRANGE, 0, MAKELONG(LINE_WIDTH_MAX,LINE_WIDTH_MIN));
		SetupColorCombo(hdlg, IDL_COLOR_TRACKS, COLORS_ALL, 0, Info->Track.Line.Color);

		EnableWindow(GetDlgItem(hdlg,IDB_SHOW),FALSE);

		DlgInfo->Initialized = TRUE;

		return TRUE;

	case WM_DRAWITEM:
	{	SYMBOL_INFO_S *pSym = NULL;
		switch (wp)
		{
		case IDB_SYMBOL_POINTS:	pSym = &Info->Waypoint.Symbol.Symbol; break;
		case IDB_SYMBOL_ROUTES:	pSym = &Info->Route.Symbol.Symbol; break;
		case IDB_SYMBOL_TRACKS:	pSym = &Info->Track.Symbol.Symbol; break;
		}
		if (pSym)
		{	RECT rc;
			POINT pt;
			DRAWITEMSTRUCT *di = (LPDRAWITEMSTRUCT) lp;
			pt.x = (di->rcItem.left+di->rcItem.right)/2;
			pt.y = (di->rcItem.bottom+di->rcItem.top)/2;
			Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
			BltSymbol(di->hDC, pt.x, pt.y, 2, 1,
						pSym->Table=='/'?0:pSym->Table=='\\'?0x1:((pSym->Table<<8)|0x1),
						pSym->Symbol-'!', 100, &rc);
		}
	}
	break;

#define HANDLE_CHECK(f) if (HIWORD(wp)==BN_CLICKED) f=(SendMessage((HWND)lp,BM_GETCHECK,0,0)==BST_CHECKED); break
#define HANDLE_INVCHECK(f) if (HIWORD(wp)==BN_CLICKED) f=(SendMessage((HWND)lp,BM_GETCHECK,0,0)!=BST_CHECKED); break
#define HANDLE_INT(f) if (HIWORD(wp)==EN_UPDATE) f=GetDlgItemInt(hdlg,LOWORD(wp),NULL,FALSE); break;
#define HANDLE_COLOR(f) if (HIWORD(wp)==CBN_SELCHANGE) GetSelectedColor(hdlg,LOWORD(wp),sizeof(f),f); break;

	case WM_COMMAND:
#ifndef UNDER_CE
		if (DlgInfo && !IsWindow(DlgInfo->hwnd))
			DestroyWindow(hdlg);	/* Protect against my parent going away */
		else
#endif
		if (!MakeFocusControlVisible(hdlg, wp, lp))
		if (DlgInfo && DlgInfo->Initialized)
		switch (LOWORD(wp))
		{
		case IDC_VISIBLE:	HANDLE_CHECK(Info->Enabled);

		case IDC_LABEL_NONE:	HANDLE_INVCHECK(Info->Label.Enabled);
		case IDC_LABEL_ID:	HANDLE_CHECK(Info->Label.ID);
		case IDC_LABEL_COMMENT:	HANDLE_CHECK(Info->Label.Comment);
		case IDC_LABEL_STATUS:	HANDLE_CHECK(Info->Label.Status);
		case IDC_LABEL_ALTITUDE:	HANDLE_CHECK(Info->Label.Altitude);
		case IDC_LABEL_TIME:	HANDLE_CHECK(Info->Label.Timestamp);
		case IDC_LABEL_NODATE:	HANDLE_CHECK(Info->Label.SuppressDate);

		case IDC_SHOW_POINTS:	HANDLE_CHECK(Info->Waypoint.Enabled);
		case IDC_LABEL_POINTS:	HANDLE_CHECK(Info->Waypoint.Label);
		case IDC_SHOW_SYMBOL_POINTS:	HANDLE_CHECK(Info->Waypoint.Symbol.Show);
		case IDC_FORCE_SYMBOL_POINTS:	HANDLE_CHECK(Info->Waypoint.Symbol.Force);

		case IDC_SHOW_ROUTES:	HANDLE_CHECK(Info->Route.Enabled);
		case IDC_LABEL_ROUTES:	HANDLE_CHECK(Info->Route.Label);
		case IDC_SHOW_SYMBOL_ROUTES:	HANDLE_CHECK(Info->Route.Symbol.Show);
		case IDE_OPACITY_ROUTES:			HANDLE_INT(Info->Route.Line.Opacity);
		case IDE_WIDTH_ROUTES:			HANDLE_INT(Info->Route.Line.Width);
		case IDL_COLOR_ROUTES:		HANDLE_COLOR(Info->Route.Line.Color);

		case IDC_SHOW_TRACKS:	HANDLE_CHECK(Info->Track.Enabled);
		case IDC_LABEL_TRACKS:	HANDLE_CHECK(Info->Track.Label);
		case IDC_SHOW_SYMBOL_TRACKS:	HANDLE_CHECK(Info->Track.Symbol.Show);
		case IDE_OPACITY_TRACKS:			HANDLE_INT(Info->Track.Line.Opacity);
		case IDE_WIDTH_TRACKS:			HANDLE_INT(Info->Track.Line.Width);
		case IDL_COLOR_TRACKS:		HANDLE_COLOR(Info->Track.Line.Color);

#undef HANDLE_CHECK
#undef HANDLE_INT
#undef HANDLE_COLOR

		case IDB_SYMBOL_POINTS:
		{	SYMBOL_INFO_S Working = Info->Waypoint.Symbol.Symbol;
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SYMBOL), hdlg, SymbolDlgProc, (LPARAM)&Working) == IDOK)
			{	Info->Waypoint.Symbol.Symbol = Working;
				InvalidateRect(hdlg,NULL,FALSE);
			}
			return TRUE;
		}
		case IDB_SYMBOL_ROUTES:
		{	SYMBOL_INFO_S Working = Info->Route.Symbol.Symbol;
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SYMBOL), hdlg, SymbolDlgProc, (LPARAM)&Working) == IDOK)
			{	Info->Route.Symbol.Symbol = Working;
				InvalidateRect(hdlg,NULL,FALSE);
			}
			return TRUE;
		}
		case IDB_SYMBOL_TRACKS:
		{	SYMBOL_INFO_S Working = Info->Track.Symbol.Symbol;
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SYMBOL), hdlg, SymbolDlgProc, (LPARAM)&Working) == IDOK)
			{	Info->Track.Symbol.Symbol = Working;
				InvalidateRect(hdlg,NULL,FALSE);
			}
			return TRUE;
		}

		case IDB_UNDO:
			*Info = DlgInfo->Original;
			PostMessage(hdlg, WM_SET_CONTROLS, 0, 0);
			break;
		case IDB_DEFAULT:
		{	*Info = DefaultOverlayConfig;
			strncpy(Info->FileName, DlgInfo->Original.FileName, sizeof(Info->FileName));
			Info->Type = DlgInfo->Original.Type;
			Info->Runtime = DlgInfo->Original.Runtime;
			PostMessage(hdlg, WM_SET_CONTROLS, 0, 0);
			break;
		}
		case IDB_CANCEL:
			*Info = DlgInfo->Original;
#ifdef UNDER_CE
			EndDialog(hdlg, IDCANCEL);
#else
			DestroyWindow(hdlg);
#endif
			return FALSE;
		case IDB_DELETE:
			if (MessageBox(hdlg, TEXT("Really Delete Overlay?"), TEXT("Configure Overlay"), MB_ICONQUESTION | MB_YESNO) == IDYES)
			{	MessageBox(hdlg, TEXT("Note: This will leak memory, so if you do it frequently, please restart."), TEXT("Delete Overlay"), MB_ICONINFORMATION | MB_OK);
				RemoveOverlay(&ActiveConfig, Info->FileName);
#ifdef UNDER_CE
				EndDialog(hdlg, IDB_DELETE);
#else
				DestroyWindow(hdlg);
#endif
			}
			return FALSE;
		case IDB_ACCEPT:
		{
//			if (DlgInfo->Default
//			&& MessageBox(hdlg,TEXT("Save Settings across Restart?"), TEXT("Configure Overlay"), MB_YESNO | MB_ICONQUESTION) == IDYES)
//				ActiveConfig.Screen.Paths = *Info;
#ifdef UNDER_CE
			EndDialog(hdlg, IDOK);
#else
			DestroyWindow(hdlg);
#endif
			return FALSE;
		}
		}
		if (Info)
		{	Info->RGBFixed = FALSE;	/* Force a color lookup */
			if (LOWORD(wp)==IDC_VISIBLE)	/* Visibility changed? */
			if (HIWORD(wp)==BN_CLICKED)	/* I mean REALLY changed? */
			if (Info->Enabled)				/* If now enabled */
			switch (Info->Type)				/* Reload based on type */
			{
			case 'P':	LoadPOSOverlay(DlgInfo->hwnd, Info, TRUE); break;
			case 'G':	LoadGPXFile(DlgInfo->hwnd, Info/*, WM_PORT_RECEIVED, RFPORT_INTERNAL*/); break;
			}

		}
		if (DlgInfo) PostMessage(DlgInfo->hwnd, DlgInfo->msgRefresh, TRUE, 0);
		break;
	case WM_CLOSE:
#ifdef UNDER_CE
		EndDialog(hdlg, IDCANCEL);
#else
		DestroyWindow(hdlg);
#endif
		return 0;
#ifndef UNDER_CE
	case WM_DESTROY:
		gModelessDialog = NULL;
		if (DlgInfo) free(DlgInfo);
		break;
	case WM_ACTIVATE:
		if (!wp) gModelessDialog = NULL;
		else gModelessDialog = hdlg;
		break;
#endif
	}
	return FALSE;
#undef WM_SET_CONTROLS
}

HWND PromptOverlayConfig(HWND hwnd, int msgRefresh, OVERLAY_CONFIG_INFO_S *pOver, BOOL Default)
{	OVERLAY_CONFIG_DLG_INFO_S *Working = (OVERLAY_CONFIG_DLG_INFO_S *)calloc(1,sizeof(*Working));
	BOOL Result = FALSE;

	Working->hwnd = hwnd;
	Working->msgRefresh = msgRefresh;
	Working->Default = Default;
	Working->Initialized = FALSE;
	Working->Original = *pOver;
	Working->pLive = pOver;

#ifdef UNDER_CE
	DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_OVERLAY), hwnd, OverlayConfigDlgProc, (LPARAM)Working);
#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return NULL;
#else
	HWND hwndDlg = CreateDialogParam(g_hInstance, MAKEINTRESOURCE(IDD_OVERLAY), hwnd, OverlayConfigDlgProc, (LPARAM)Working);
	ShowWindow(hwndDlg, SW_SHOW);
	return hwndDlg;
#endif
}

BOOL PromptStatusReport(HWND hwnd, CONFIG_INFO_S *pConfig)
{	CONFIG_INFO_S Working = *pConfig;
	BOOL Result = FALSE;
	if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_STATUS_REPORT), hwnd, StatusReportDlgProc, (LPARAM)&Working) == IDOK)
	{	*pConfig = Working;
		ValidateConfig(pConfig);
		RealSaveConfiguration(hwnd, pConfig, "User:StatusReport");
		Result = TRUE;
	}

#ifdef USING_SIP
	SipShowIM(SIPF_OFF);	/* Shut down the SIP */
#endif
	return Result;
}

BOOL VerifyOSMPath(HWND hwnd, TILE_SERVER_INFO_S *tInfo, BOOL DefaultOSM, BOOL *pConfigChanged)
{	BOOL Changed = FALSE, First;
	size_t ProbeLen = 80 * sizeof(TCHAR);
	TCHAR *ProbeFile = (TCHAR*)malloc(ProbeLen);

	StringCbPrintf(ProbeFile, ProbeLen, TEXT("%S.osm"), PROGNAME);

	First = !IsDirectory(tInfo->Path);

	TraceLogThread("Config", TRUE, "VerifyOSMPath[%s](%s)/%S\n", tInfo->Name, tInfo->Path, ProbeFile);

	for (;;)	/* infinite loop! */
	{	OPENFILENAME ofn = {0};
		BOOL Exists = FALSE;

		if (*tInfo->Path)
		{	char *Path=(char*)malloc(MAX_PATH+sizeof(tInfo->Path));
			HWND hwndProgress = NULL;
			FILE *Test = NULL;
			sprintf(Path,"%.*s%S", sizeof(tInfo->Path), tInfo->Path, ProbeFile);
			int retry = 0;
			// for (retry=0; !Test && retry<1; retry++)
			{	Test = fopen(Path,"rb");
				if (!Test)
				{	TraceLog("Config", TRUE, hwnd, "Try[%ld] Failed To Probe OSM(%s) for Read\n", (long) retry, Path);
					if (!First) Test = fopen(Path,"wb");
					if (Test)
					{	fclose(Test);
						TraceLog("Config", TRUE, hwnd, "Try[%ld] Create Probe OSM(%s) Worked!\n", (long) retry, Path);
						Test = fopen(Path,"rb");
						if (!Test) TraceLog("Config", TRUE, hwnd, "Re-Open Probe OSM(%s) FAILED!\n", Path);
					} else
					{
	if (hwnd && !retry)	/* First time only */
	{	RECT rc;
		GetWindowRect(hwnd, &rc);
		hwndProgress = CreateWindow(
						PROGRESS_CLASS,		/*The name of the progress class*/
						NULL, 			/*Caption Text*/
						CCS_TOP | WS_POPUP /*| WS_CHILD */| WS_VISIBLE
						| WS_BORDER /*| WS_CLIPSIBLINGS*/
						| WS_EX_STATICEDGE
						| PBS_SMOOTH,	/*Styles*/
						rc.left, 			/*X co-ordinates*/
						(rc.bottom+rc.top)/2-15,	/*Y co-ordinates*/
						rc.right-rc.left, 			/*Width*/
						30, 			/*Height*/
						hwnd, 			/*Parent HWND*/
						(HMENU) NULL/*1*/, 	/*The Progress Bar's ID*/
						g_hInstance,		/*The HINSTANCE of your program*/ 
						NULL);			/*Parameters for main window*/
		if (hwndProgress)
		{	SendMessage(hwndProgress, PBM_SETRANGE32, 0, 30);
		}
	}
	if (hwndProgress)
		SendMessage(hwndProgress, PBM_SETPOS, retry+1, 0);
						Sleep(1000);	/* Wait for the SD to wake up! */
					}
				} else TraceLog("Config", TRUE, hwnd, "Try[%ld] Probe OSM(%s) Worked!\n", (long) retry, Path);
			}	/* End-for */
	if (hwndProgress) DestroyWindow(hwndProgress);
			free(Path);
			if (Test)	/* all is good */
			{	fclose(Test);
				{	BOOL SomeOSM;
					if (!isOSMDataOnly(tInfo->Path, ProbeFile, &SomeOSM))
					{	TraceLog("Config", TRUE, hwnd, "OSMPath(%s) Contains %s Data\n", tInfo->Path, SomeOSM?"Mixed":"NO");
					} else TraceLog("Config", TRUE, hwnd, "OSMPath(%s) Contains ONLY OSM Data\n", tInfo->Path);
				}

				free(ProbeFile);

				return TRUE;
			}
		} else TraceLog("Config", TRUE, hwnd, "VerifyOSMPath[%s](%s) is BLANK!\n", tInfo->Name, tInfo->Path);
		First = FALSE;	/* Allow write probing next time... */

		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = hwnd;
		ofn.lpstrFilter = TEXT("OSM Files\0*.osm\0\0");
		ofn.nFilterIndex = 1;	/* Default to all */
		ofn.nMaxFile = sizeof(tInfo->Path)-2;
		ofn.lpstrFile = (LPWSTR) calloc(ofn.nMaxFile,sizeof(*ofn.lpstrFile));
//		ofn.nMaxFileTitle = sizeof(tInfo->Path)-2;
//		ofn.lpstrTitle = (LPWSTR) calloc(ofn.nMaxFile,sizeof(*ofn.lpstrTitle));
		ofn.lpstrTitle = TEXT("OSM Tile Cache");
		ofn.Flags = /* OFN_PATHMUSTEXIST | */ OFN_NOCHANGEDIR;
//		ofn.Flags = OFN_PROJECT;
		StringCbPrintf(ofn.lpstrFile, ofn.nMaxFile*sizeof(*ofn.lpstrFile), TEXT("%S.osm"), PROGNAME);
		for (;;)
		{	if (DefaultOSM)
			{	StringCbPrintf(ofn.lpstrFile, sizeof(tInfo->Path)*sizeof(*ofn.lpstrFile), TEXT("./"));
				break;
			} else
			{	MessageBox(hwnd, TEXT("Please Locate or Create\nOSMTiles Folder for\nOpenStreetMap Tile Cache\n"), TEXT("Configuration"), MB_OK | MB_ICONWARNING);
				if (GetSaveFileName(&ofn))
				{	TraceLog("Config", TRUE, hwnd, "GetSaveFileName File(%S) OR %ld(%.*S) Title(%S)\n", ofn.lpstrFile, (long) ofn.nFileOffset, (int) ofn.nFileOffset, ofn.lpstrFile, ofn.lpstrTitle);
					break;	/* Got one to try */
				}
				TraceLog("Config", TRUE, hwnd, "GetSaveFileName(%S) Failed (or Cancelled), GetLastError=%ld\n", ofn.lpstrFile, GetLastError());
				if (MessageBox(hwnd, TEXT("If you Cancel now, the OSM Tile Fetcher will be disabled."), TEXT("Configuration"), MB_RETRYCANCEL | MB_ICONQUESTION) == IDCANCEL)
				{	free(ProbeFile);
					return FALSE;
				}
			}
		}

		if (pConfigChanged) *pConfigChanged = TRUE;
		memset(tInfo->Path, 0, sizeof(tInfo->Path));
		StringCbPrintfA(tInfo->Path, sizeof(tInfo->Path), "%S", ofn.lpstrFile);

#ifdef OLD_WAY
		if (tInfo->Path[strlen(tInfo->Path)-1] != '/'
		&& tInfo->Path[strlen(tInfo->Path)-1] != '\\')
			tInfo->Path[strlen(tInfo->Path)] = '\\';
#endif

		/*	Make them all / for easier coding */
		for (char *c=tInfo->Path; *c; c++)
			if (*c == '\\') *c = '/';

		if (strrchr(tInfo->Path,'/'))
			strrchr(tInfo->Path,'/')[1] = '\0';

		TraceLog("Config", TRUE, hwnd, "New OSMPath(%s)\n", tInfo->Path);

		{	BOOL SomeOSM;
			if (!isOSMDataOnly(tInfo->Path, ProbeFile, &SomeOSM))
			{	if (!SomeOSM)
			{	if (DefaultOSM
				|| MessageBox(hwnd, TEXT("Warning: OSMPath Contains Non-OSM Data\n\nCreate Dedicated OSMTiles SubDirectory?"), TEXT("VerifyOSMPath"), MB_ICONINFORMATION | MB_YESNO) == IDYES)
					{	strcat(tInfo->Path, "OSMTiles/");
						MakeDirectory(tInfo->Path);
					}
				} else TraceLog("Config", TRUE, hwnd, "OSMPath(%s) Contains Mixed Data\n", tInfo->Path);
			}
		}

#ifdef REMOVE_REDUNDANT
/*	Check for a redundant final path component */
		{	char *e = &tInfo->Path[strlen(tInfo->Path)-1];
			char *s1, *s2;
			for (s2=e-1; s2>tInfo->Path; s2--)
			{	if (*s2 == '/')
					break;
			}
			for (s1=s2-1; s1>tInfo->Path; s1--)
			{	if (*s1 == '/')
					break;
			}
			if (s2-s1 == e-s2
			&& !strncmp(s1,s2,s2-s1))
			{	TraceLog("Config", TRUE, hwnd, "Removing Redundant %.*s from %s\n",
							(int) (s2-s1), tInfo->Path);
				memset(s2+1,0,s2-s1);	/* null out final piece */
				TraceLog("Config", TRUE, hwnd, "Non-Redundant Is %s\n",
							tInfo->Path);
			}
		}
#endif
	}
	free(ProbeFile);
}

BOOL LoadOrDefaultConfiguration(HWND hwnd, CONFIG_INFO_S *pConfig, BOOL AllowCreate, BOOL DefaultOSMIfNew)
{	BOOL Result = TRUE;
	BOOL ConfigChanged = FALSE;

	if (!LoadConfiguration(hwnd, pConfig, AllowCreate))
	{
TraceLog("Config", TRUE, hwnd, "LoadConfiguration FAILED, Defining Default\n");
		DefineDefaultConfig(pConfig);
		Result = FALSE;
		if (!AllowCreate) return Result;
	} else TraceLog("Config", FALSE, hwnd,"LoadConfiguration WORKED!\n");

	if (!_strnicmp(pConfig->CallSign,DEFAULT_CALLSIGN,min(strlen(DEFAULT_CALLSIGN),sizeof(pConfig->CallSign))))
	{	TraceLog("Config", TRUE, hwnd, "Prompting for Configuration\n");
		while (!PromptConfiguration(hwnd, pConfig)
		|| !_strnicmp(pConfig->CallSign,DEFAULT_CALLSIGN,sizeof(pConfig->CallSign)))
		{	if (MessageBox(hwnd, TEXT("A Valid Amateur Radio Callsign is Required to Run.\n\nDo you want to try again?\n"), TEXT("Configuration"), MB_YESNO | MB_ICONQUESTION) != IDYES)
				exit(1);
		}
		ConfigChanged = TRUE;
	}

	//if (AllowCreate)	/* Only do this on the REAL load */
	{	if (!pConfig->OSM.Name[0])
		{	strncpy(pConfig->OSM.Name, "Original", sizeof(pConfig->OSM.Name));
			ConfigChanged = TRUE;
		}
		if (!pConfig->TileServers.Count)
		{	unsigned long i = pConfig->TileServers.Count++;
			pConfig->TileServers.Server = (TILE_SERVER_INFO_S *)realloc(pConfig->TileServers.Server,sizeof(*pConfig->TileServers.Server)*pConfig->TileServers.Count);
			pConfig->TileServers.Server[i] = pConfig->OSM;
			ConfigChanged = TRUE;
		}
		if (!VerifyOSMPath(hwnd, &pConfig->OSM, DefaultOSMIfNew&&!Result, &ConfigChanged))
		{	pConfig->Enables.OSMFetch = FALSE;
			ConfigChanged = TRUE;
		}

		if (DefineColorChoices(pConfig, TRUE))
			ConfigChanged = TRUE;	/* Get it saved */
		if (DefineTrackColors(pConfig))
			ConfigChanged = TRUE;	/* Get it saved */

		pConfig->Screen.WindBarbs.RGB = GetColorRGB(pConfig, pConfig->Screen.WindBarbs.Color, "WindBarbs");

		pConfig->Screen.Track.Follow.RGB = GetColorRGB(pConfig, pConfig->Screen.Track.Follow.Color, "Track.Follow");
		pConfig->Screen.Track.Other.RGB = GetColorRGB(pConfig, pConfig->Screen.Track.Other.Color, "Track.Other");
#ifdef MONITOR_PHONE
		pConfig->Screen.Track.Cellular.RGB = GetColorRGB(pConfig, pConfig->Screen.Track.Cellular.Color, "Track.Cellular");
#endif
		pConfig->Screen.Paths.All.Time = -1;
		pConfig->Screen.Paths.Last.Time = 0;
	}

	if (ConfigChanged && (Result || AllowCreate))
	{
		RealSaveConfiguration(hwnd, pConfig, "Loaded:Changed");
	}

	SpaceCompress(STRING(pConfig->AltNet));
	SpaceCompress(STRING(pConfig->CallSign));
	SpaceCompress(STRING(pConfig->Password));
	ZeroSSID(pConfig->AltNet);
	ZeroSSID(pConfig->CallSign);
	return Result;
}

BOOL CheckMessageCall(HWND hwnd, CONFIG_INFO_S *pConfig, char *Call)
{
#ifdef USE_TIMED_STRINGS
	return LocateTimedStringEntry(&pConfig->MessageCalls, Call) != -1;
#else
	unsigned int i;
	for (i=0; i<pConfig->MessageCalls.Count; i++)
		if (!strcmp(pConfig->MessageCalls.Strings[i], Call))
			return TRUE;
	return FALSE;
#endif
}

void RememberMessageCall(HWND hwnd, CONFIG_INFO_S *pConfig, char *Call)
{	SetVerifyConfigurationValue("MessageCall",Call);
}

void RememberEMail(HWND hwnd, CONFIG_INFO_S *pConfig, char *EMail)
{	SetVerifyConfigurationValue("EMail",EMail);
}

void RememberCommentChoice(HWND hwnd, CONFIG_INFO_S *pConfig, char *Choice)
{	SetVerifyConfigurationValue("CommentChoice",Choice);
}

BOOL RememberSymbolChoice(HWND hwnd, CONFIG_INFO_S *pConfig, SYMBOL_INFO_S *Symbol)
{	STRING_LIST_S *pList = &pConfig->SymbolChoices;
	unsigned long WasCount = pList->Count;
	char *New = (char*)calloc(3,sizeof(*New));
	New[0] = Symbol->Table; New[1] = Symbol->Symbol;
#ifdef USE_TIMED_STRINGS
	AddOrUpdateTimedStringEntry(pList, New);
#else
	unsigned int i;
	for (i=0; i<pConfig->SymbolChoices.Count; i++)
		if (!strcmp(pConfig->SymbolChoices.Strings[i], New))
			break;
	if (i>=pConfig->SymbolChoices.Count)
	{	SetVerifyConfigurationValue("SymbolChoice",New);
		//RealSaveConfiguration(hwnd, pConfig, "SymbolChoice");
	}
#endif
	free(New);
	return pList->Count != WasCount;
}

BOOL RememberAltNetChoice(HWND hwnd, CONFIG_INFO_S *pConfig, char *AltNet, BOOL Used)
{	STRING_LIST_S *pList = &pConfig->AltNetChoices;
#ifdef OLD_WAY
	unsigned int len = strlen(AltNet);
	unsigned int i;

	for (i=0; i<pList->Count; i++)
	{
#ifdef USE_TIMED_STRINGS
		char *String = pList->Entries[i].string;
#else
		char *String = pList->Strings[i];
#endif
		if ((*String=='*' && !strcmp(String+1, AltNet))
		|| !strcmp(String, AltNet))
		{	if (Used && *String!='*')	/* Needs to be used */
				RemoveSimpleStringEntry(pList, i--);
			else break;
		}
	}
	if (i>=pList->Count)
	{	char *New = (char*)malloc(1+len+1);
		if (Used) strcpy(New,"*");
		else *New = '\0';
		strcat(New,AltNet);
		SetVerifyConfigurationValue("AltNetChoice",New);
		//RealSaveConfiguration(hwnd, pConfig, "AltNetChoice");
		free(New);
		return TRUE;
	}
	return FALSE;
#else
	unsigned long WasCount = pList->Count;
	AddOrUpdateTimedStringEntry(pList, AltNet, NULL, Used);
	return WasCount != pList->Count;
#endif
}

void RememberNWSOffice(HWND hwnd, CONFIG_INFO_S *pConfig, char *Choice, BOOL Active)
{	STRING_LIST_S *pList = &pConfig->NWS.Offices;
#ifdef OLD_WAY
	unsigned int len = strlen(Choice);
	unsigned int i;
	BOOL Found = FALSE;

	if (len > 1 && Choice[len-1] == '*') Choice[len-1] = '\0';

	for (i=0; i<pList->Count; i++)
	{
#ifdef USE_TIMED_STRINGS
		char *String = pList->Entries[i].string;
#else
		char *String = pList->Strings[i];
#endif
		if (!strncmp(String, Choice, len))
		{	RemoveSimpleStringEntry(pList, i--);
			Found = TRUE;
		}
	}
	if (i>=pList->Count)
	{	char *New = (char*)malloc(len+1+1);
		strcpy(New,Choice);
		if (!Active) strcat(New,"*");
		SetVerifyConfigurationValue("NWS.Office",New);
		free(New);
	}
	return Found;
#else
	AddOrUpdateTimedStringEntry(pList, Choice, NULL, Active);
#endif
}

#ifdef SUPPORT_SHAPEFILES
void RememberNWSShapeFile(HWND hwnd, CONFIG_INFO_S *pConfig, char *File, long Enabled)
{	AddOrUpdateTimedStringEntry(&pConfig->NWS.ShapeFiles, File, NULL, Enabled);
}
#endif

void RememberPOSOverlay(HWND hwnd, CONFIG_INFO_S *pConfig, char *File, long Enabled)
{	AddOrUpdateTimedStringEntry(&pConfig->POSOverlays, File, NULL, Enabled);
}

void RememberStatusChoice(HWND hwnd, CONFIG_INFO_S *pConfig, char *Choice)
{	SetVerifyConfigurationValue("Status.Choice",Choice);
}

void RememberAutoAnswerChoice(HWND hwnd, CONFIG_INFO_S *pConfig, char *Choice)
{	SetVerifyConfigurationValue("AutoReply.ReplyChoice",Choice);
}

void RememberAutoAnswerStation(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station)
{	AddOrUpdateTimedStringEntry(&pConfig->Messaging.AutoAnswer.Stations,
								Station);
}

void PurgeAutoAnswerStations(HWND hwnd, CONFIG_INFO_S *pConfig)
{	EmptyTimedStringList(&pConfig->Messaging.AutoAnswer.Stations);
}

BOOL RemoveOverlay(CONFIG_INFO_S *pConfig, char *FileName)
{	unsigned int p;

	if (!*FileName) return FALSE;		/* Have to keep ME */

	for (p=0; p<pConfig->Overlays.Count; p++)
	{	if (!_stricmp(pConfig->Overlays.Overlay[p].FileName, FileName))
			break;
	}
	if (p >= pConfig->Overlays.Count) return FALSE;	/* Not found */

	pConfig->Overlays.Overlay[p] = pConfig->Overlays.Overlay[--pConfig->Overlays.Count];
	return TRUE;
}

OVERLAY_CONFIG_INFO_S *GetOrCreateOverlay(CONFIG_INFO_S *pConfig, char *FileName, char Type, BOOL AllowCreate)
{	unsigned int p;

	for (p=0; p<pConfig->Overlays.Count; p++)
	{	if (!_stricmp(pConfig->Overlays.Overlay[p].FileName, FileName))
			break;
	}

	if (p >= pConfig->Overlays.Count)
	{	if (!AllowCreate) return NULL;
		p = pConfig->Overlays.Count++;
		pConfig->Overlays.Overlay = (OVERLAY_CONFIG_INFO_S *)realloc(p?pConfig->Overlays.Overlay:NULL, sizeof(*pConfig->Overlays.Overlay)*pConfig->Overlays.Count);
		pConfig->Overlays.Overlay[p] = DefaultOverlayConfig;
		strncpy(pConfig->Overlays.Overlay[p].FileName, FileName, sizeof(pConfig->Overlays.Overlay[p].FileName)-1);
		pConfig->Overlays.Overlay[p].Type = Type;
		return GetOrCreateOverlay(pConfig, FileName, Type, FALSE);
	}
	return &pConfig->Overlays.Overlay[p];
}

void RememberRFIDReader(HWND hwnd, CONFIG_INFO_S *pConfig, char *Reader)
{
	if (pConfig->RFIDs.Count)
	{
		STRING_LIST_S *pList = &pConfig->RFIDs.RFID[0].Readers;
		if (strchr(Reader,'>'))
		{	char *Temp = _strdup(Reader);
			unsigned long i;
			char *eol;

			eol = Temp+strlen(Temp)-1;
			while (eol >= Temp && (*eol=='\n' || *eol=='\r'))
				*eol-- = '\0';

			eol = strchr(Temp,'>');	/* Up to the end of the callsign */

			i = LocateTimedStringEntry(pList, Temp, eol-Temp);
			if (i == -1)
			{	AddTimedStringEntry(pList, Temp);
			} else	/* Actually have this type for this station, replace it */
			{	UpdateTimedStringEntryAt(pList, i, Temp);
			}
			free(Temp);
			if (pConfig->RFIDs.RFID[0].AssocEnabled) RealSaveConfiguration(hwnd, pConfig, "RFID-Reader");
		}
	}
}

size_t PurgeTelemetryDefinitions(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long Seconds)
{
	size_t Count = PurgeOldTimedStrings(&pConfig->TelemetryDefinitions, Seconds);
	if (Count) RealSaveConfiguration(hwnd, pConfig, "Telemetry:Definition:Purge");
	return Count;
}

void RememberTelemetryDefinition(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station, char *Definition)
{	TIMED_STRING_LIST_S *pList = &pConfig->TelemetryDefinitions;
	unsigned long i;
	size_t Size = 1+9+1+strlen(Definition)+1;
	char *eol, *Temp = (char*)malloc(Size);
	StringCbPrintfA(Temp, Size, ":%-9s:%s", Station, Definition);

	eol = Temp+strlen(Temp)-1;
	while (eol >= Temp && (*eol=='\n' || *eol=='\r'))
		*eol-- = '\0';

	i = LocateTimedStringEntry(pList, Temp, 1+9+1+5);	/* :XXXXXX-SS:WHAT. */
	AddTimedStringEntryAt(pList, i, Temp);
	free(Temp);

	// RealSaveConfiguration(hwnd, pConfig, "Telemetry:Definition");
}

char *RecallTelemetryDefinition(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station, char *What)	/* What should be 5 characters! */
{	TIMED_STRING_LIST_S *pList = &pConfig->TelemetryDefinitions;
	unsigned long i;
	size_t Size = 1+9+1+strlen(What)+1;
	char *Temp = (char*)malloc(Size);
	StringCbPrintfA(Temp, Size, ":%-9s:%s", Station, What);
	Size = strlen(Temp);

	i = LocateTimedStringEntry(pList, Temp, Size);
	if (i < pList->Count)
	{	GetSystemTime(&pList->Entries[i].time);
		return _strdup(pList->Entries[i].string+1+9+1);	/* Caller doesn't want the Station */
	} else	return NULL;
}

void RefreshTelemetryDefinition(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station)
{	TIMED_STRING_LIST_S *pList = &pConfig->TelemetryDefinitions;
	SYSTEMTIME stNow;
	unsigned long i;
	size_t Size = 1+9+1+1;
	char *Temp = (char*)malloc(Size);
	StringCbPrintfA(Temp, Size, ":%-9s:", Station);
	Size = strlen(Temp);
	GetSystemTime(&stNow);

	for (i=0; i<pList->Count; i++)
		if (!strncmp(pList->Entries[i].string, Temp, Size))	/* :XXXXXX-SS: */
			pList->Entries[i].time = stNow;
	free(Temp);
}

size_t ForgetRcvdBulletin(HWND hwnd, CONFIG_INFO_S *pConfig, char *From, char ID, char *Group)
{	TIMED_STRING_LIST_S *pList = &pConfig->RcvdBulletins;
	unsigned long i;
	size_t Count = 0;
	size_t Size = 1+9+1+1+1+5+1+1;
	char *eol, *Temp = (char*)malloc(Size);
	size_t tLen;
	StringCbPrintfA(Temp, Size, ":%.9s:%.1s:%.5s:", From, &ID, Group);
	tLen = strlen(Temp);

	eol = Temp+strlen(Temp)-1;
	while (eol >= Temp && (*eol=='\n' || *eol=='\r'))
		*eol-- = '\0';

	while ((i=LocateTimedStringEntry(pList, Temp, tLen)) < pList->Count)
	{	RemoveTimedStringEntry(pList, i);
		Count++;
	}
	if (!Count) TraceLogThread("Bulletins", TRUE, "Failed To Find %s\n", Temp);
	free(Temp);
	return Count;
}

void RememberRcvdBulletin(HWND hwnd, CONFIG_INFO_S *pConfig, char *From, char ID, char *Group, char *Text)
{	TIMED_STRING_LIST_S *pList = &pConfig->RcvdBulletins;
	unsigned long i;
	size_t Size = 1+9+1+1+1+5+1+strlen(Text)+1;
	char *eol, *Temp = (char*)malloc(Size), *Key = (char*)malloc(Size);
	int kLen;
	StringCbPrintfA(Key, Size, ":%.9s:%.1s:%.5s:", From, &ID, Group);
	StringCbPrintfA(Temp, Size, ":%.9s:%.1s:%.5s:%s", From, &ID, Group, Text);
	kLen = strlen(Key);

	eol = Temp+strlen(Temp)-1;
	while (eol >= Temp && (*eol=='\n' || *eol=='\r'))
		*eol-- = '\0';

	i = LocateTimedStringEntry(pList, Key, kLen);	/* :XXXXXX-SS:X:GROUP: */
	AddTimedStringEntryAt(pList, i, Temp);

	// RealSaveConfiguration(hwnd, pConfig, "Telemetry:Definition");
	free(Temp);
	free(Key);
}

void RememberRcvdWeather(HWND hwnd, CONFIG_INFO_S *pConfig, char *From, char *To, char *Comment, char *Packet)
{	TIMED_STRING_LIST_S *pList = &pConfig->RcvdWeather;
	char *Date = strdup(Comment);
	char *Type = strchr(Date,',');
	if (Type)
	{	char *Zones = strchr(Type+1,',');
		if (Zones)
		{	char *Seq = strchr(Zones+1,'{');
			if (Seq && strlen(Seq) == 6)
			{	SYSTEMTIME stWhen, stExpires;
				*Type++ = *Zones++ = *Seq++ = '\0';

				GetSystemTime(&stWhen);	/* Need to know what time it is now */
				stExpires = stWhen;
				stExpires.wSecond = stExpires.wMilliseconds = 0;
				if (cFromDec(&Date[0], 2, (unsigned char*)&stExpires.wDay)
				&& cFromDec(&Date[2], 2, (unsigned char*)&stExpires.wHour)
				&& cFromDec(&Date[4], 2, (unsigned char*)&stExpires.wMinute))
				{	size_t Len = strlen(From)+1+strlen(Seq)+1+strlen(Packet)+1;
					char *Buffer = (char*)malloc(Len);

					StringCbPrintfA(Buffer, Len, "%s{%s:%s", From, Seq, Packet);

					if (stWhen.wDay+14 < stExpires.wDay)	/* Last month? */
					{	if (!--stExpires.wMonth)
						{	stExpires.wMonth = 12;
							stExpires.wYear--;
						}
					} else if (stWhen.wDay-14 > stExpires.wDay)	/* Next month? */
					{	if (++stExpires.wMonth > 12)
						{	stExpires.wMonth = 1;
							stExpires.wYear++;
						}
					}
					AddOrUpdateColonStringEntry(pList, Buffer, &stExpires);
					TraceLogThread("Weather", TRUE, "Age:%ld seconds %s\n", (long) SecondsSince(&stExpires), Buffer);
					free(Buffer);
				}
			}
		}
	}
	free(Date);
}

#ifdef OBSOLETE
void RememberLatLon(HWND hwnd, CONFIG_INFO_S *pConfig, double Latitude, double Longitude)
{	char Temp[33];
	sprintf(Temp,"%lf",(double)Latitude);
	SetVerifyConfigurationValue("Latitude", Temp);
	sprintf(Temp,"%lf",(double)Longitude);
	SetVerifyConfigurationValue("Longitude", Temp);
	//RealSaveConfiguration(hwnd, pConfig);
}

void RememberScale(HWND hwnd, CONFIG_INFO_S *pConfig, double Scale)
{	char Temp[33];
	sprintf(Temp,"%lf",(double)Scale);
	SetVerifyConfigurationValue("Scale", Temp);
	//RealSaveConfiguration(hwnd, pConfig);
}

void RememberOrientation(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long Orientation)
{	char Temp[33];
	sprintf(Temp,"%lu",(double)Orientation);
	SetVerifyConfigurationValue("Orientation", Temp);
	//RealSaveConfiguration(hwnd, pConfig);
}
#endif

void RememberOSMZoom(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long zoom)
{	char Temp[33];
	sprintf(Temp,"%lu",(unsigned long)zoom);
	SetVerifyConfigurationValue("OSM.Zoom", Temp);
	//RealSaveConfiguration(hwnd, pConfig, "OSM:Zoom");
}

#ifdef OBSOLETE
int AddNewRFPort(CONFIG_INFO_S *pConfig, char *Name)
{	CONFIG_ITEM_S *newItem = GetConfigItem("RFPort");
	int p = pConfig->RFPorts.Count++;

	pConfig->RFPorts.Port = (PORT_CONFIG_INFO_S *)realloc(p?pConfig->RFPorts.Port:NULL, sizeof(*pConfig->RFPorts.Port)*pConfig->RFPorts.Count);

	if (newItem && newItem->Type == CONFIG_STRUCT && newItem->Struct.pDefault)
	{	void *pValue = (void*)&pConfig->RFPorts.Port[p];
		memcpy(pValue, newItem->Struct.pDefault, newItem->Struct.Size);
	}

	strncpy(pConfig->RFPorts.Port[p].Name, Name, sizeof(pConfig->RFPorts.Port[p].Name));
	return p;
}
#endif

static int CompareNickname(const void *One, const void *Two)
{	NICKNAME_INFO_S *Left = (NICKNAME_INFO_S *)One;
	NICKNAME_INFO_S *Right = (NICKNAME_INFO_S *)Two;
	int r;
	if (!_stricmp(Left->Station, CALLSIGN)) return -1;
	if (!_stricmp(Right->Station, CALLSIGN)) return 1;
	r = strcmp(Left->Label, Right->Label);
	if (!r) r = _stricmp(Left->Station, Right->Station);
	if (!r) r = strcmp(Left->Comment, Right->Comment);
	return r;
}
void SortNicknames(CONFIG_INFO_S *pConfig)
{
	if (pConfig->Nicknames.Count > 1)
		qsort(pConfig->Nicknames.Nick, pConfig->Nicknames.Count, sizeof(*pConfig->Nicknames.Nick), CompareNickname);
}

BOOL RemoveNickname(CONFIG_INFO_S *pConfig, char *Station)
{	unsigned int p;

	if (!*Station) return FALSE;		/* Have to keep ME */
	if (!strncmp(pConfig->CallSign, Station, sizeof(pConfig->CallSign))) return FALSE;	/* Cannot delete ME */

	for (p=0; p<pConfig->Nicknames.Count; p++)
	{	if (!_stricmp(pConfig->Nicknames.Nick[p].Station, Station))
			break;
	}
	if (p >= pConfig->Nicknames.Count) return FALSE;	/* Not found */

	pConfig->Nicknames.Nick[p] = pConfig->Nicknames.Nick[--pConfig->Nicknames.Count];
	SortNicknames(pConfig);
	return TRUE;
}

BOOL IsNicknameLabelInUse(CONFIG_INFO_S *pConfig, char *Label, char *Ignore, BOOL DisableDupes)
{
	if (!*Label) return FALSE;
	for (unsigned int p=0; p<pConfig->Nicknames.Count; p++)
	{	if (pConfig->Nicknames.Nick[p].Enabled
		&& (*Label && !_stricmp(pConfig->Nicknames.Nick[p].Label, Label))
		&& _stricmp(pConfig->Nicknames.Nick[p].Station, Ignore))
		{	TraceLogThread("Config", TRUE, "Nickname(%s) In Use By Station(%s) Ignoring(%s)%s\n",
						Label, pConfig->Nicknames.Nick[p].Station, Ignore,
						DisableDupes?"Disabling":"");
			if (DisableDupes)
				pConfig->Nicknames.Nick[p].Enabled = FALSE;
			else return TRUE;	/* Can't use this label */
		}
	}
	return FALSE;
}

NICKNAME_INFO_S *GetOrCreateNickname(CONFIG_INFO_S *pConfig, char *Station, char *Label, BOOL AllowCreate, BOOL DisableDupes)
{	unsigned int p;

	for (p=0; p<pConfig->Nicknames.Count; p++)
	{	if (!_stricmp(pConfig->Nicknames.Nick[p].Station, Station))
			break;
	}

	if (p >= pConfig->Nicknames.Count)
	{	if (!AllowCreate || !Label) return NULL;
		if (IsNicknameLabelInUse(pConfig, Label, Station, DisableDupes)) return NULL;	/* Can't create */
		p = pConfig->Nicknames.Count++;
		pConfig->Nicknames.Nick = (NICKNAME_INFO_S *)realloc(p?pConfig->Nicknames.Nick:NULL, sizeof(*pConfig->Nicknames.Nick)*pConfig->Nicknames.Count);
		memset(&pConfig->Nicknames.Nick[p], 0, sizeof(pConfig->Nicknames.Nick[p]));
		strncpy(pConfig->Nicknames.Nick[p].Station, Station, sizeof(pConfig->Nicknames.Nick[p].Station)-1);
		strncpy(pConfig->Nicknames.Nick[p].Label, Label, sizeof(pConfig->Nicknames.Nick[p].Label)-1);
		pConfig->Nicknames.Nick[p].Symbol.Table = '\\';
		pConfig->Nicknames.Nick[p].Symbol.Symbol = '?';
		_strupr(pConfig->Nicknames.Nick[p].Station);
		SortNicknames(pConfig);
		return GetOrCreateNickname(pConfig, Station, Label, FALSE, FALSE);
	}
	if (!pConfig->Nicknames.Nick[p].OverrideLabel
	&& !pConfig->Nicknames.Nick[p].OverrideSymbol
	&& !pConfig->Nicknames.Nick[p].OverrideComment
	&& !pConfig->Nicknames.Nick[p].OverrideColor)
		pConfig->Nicknames.Nick[p].Enabled = FALSE;
	return &pConfig->Nicknames.Nick[p];
}

void SetNicknameSymbol(CONFIG_INFO_S *pConfig, char *Station, SYMBOL_INFO_S *pSymbol)
{	NICKNAME_INFO_S *pNick = GetOrCreateNickname(pConfig, Station, NULL, FALSE, FALSE);
	if (pNick) pNick->Symbol = *pSymbol;
}

void RememberTacticalNickname(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station, char *Definition, char *From)
{	BOOL AllowCreate = Definition && *Definition;
	NICKNAME_INFO_S *pNick = GetOrCreateNickname(pConfig, Station, Definition, AllowCreate, TRUE);

	if (pNick)
	{	Definition = _strdup(Definition);	/* Modifiable copy */
		char *s = strchr(Definition,'(');	/* Possible symbol? */
		char *e = s?strchr(s+1,')'):NULL;	/* Maybe so */
		SYMBOL_INFO_S Sym = {0};
		if (s && e && e==s+3)				/* Definitely (ts) */
		if (s[1] >= '!' && s[1] <= '~'
		&& s[2] >= '!' && s[2] <= '~')		/* Must be valid */
		{	Sym.Table = s[1];
			Sym.Symbol = s[2];
			strcpy(s,e+1);					/* Eliminte (ts) */
		}
		IsNicknameLabelInUse(pConfig, Definition, Station, TRUE);
		pNick->Enabled = AllowCreate;	/* Turn off if empty definition */
		if (Sym.Table)	/* Got Symbol? */
		{	pNick->OverrideSymbol = TRUE;
			pNick->Symbol = Sym;
		}
		if (*Definition)	/* More than just symbol? */
		{	pNick->OverrideLabel = TRUE;
			strncpy(pNick->Label, Definition, sizeof(pNick->Label)-1);
			pNick->Label[sizeof(pNick->Label)-1] = '\0';	/* Ensure null termination */
			if (strlen(Definition) >= sizeof(pNick->Label))
			{	pNick->OverrideComment = TRUE;
				strncpy(pNick->Comment, Definition, sizeof(pNick->Comment)-1);
			}
		}
		strncpy(pNick->DefinedBy, From, sizeof(pNick->DefinedBy)-1);
		free(Definition);
	}

	// RealSaveConfiguration(hwnd, pConfig, "Tactical:Definition");
}

int ClearClonedNicknames(HWND hwnd, CONFIG_INFO_S *pConfig, char *DefinedBy)
{	unsigned int p;
	int count=0;

	for (p=0; p<pConfig->Nicknames.Count; p++)
	{	if (*pConfig->Nicknames.Nick[p].DefinedBy
		&& (!DefinedBy || !_stricmp(DefinedBy, pConfig->Nicknames.Nick[p].DefinedBy)))
		{	pConfig->Nicknames.Nick[p--] = pConfig->Nicknames.Nick[--pConfig->Nicknames.Count];
			count++;
		}
	}
	SortNicknames(pConfig);
	return count;
}

int ClearTacticalNicknames(HWND hwnd, CONFIG_INFO_S *pConfig, char *DefinedBy)
{	unsigned int p;
	int count = 0;

	if (DefinedBy) count = ClearClonedNicknames(hwnd, pConfig, DefinedBy);
	else
	{	for (p=0; p<pConfig->Nicknames.Count; p++)
		{	if (*pConfig->Nicknames.Nick[p].DefinedBy
			&& !strchr(pConfig->Nicknames.Nick[p].DefinedBy,'*'))	/* Protect the clones */
			{	pConfig->Nicknames.Nick[p--] = pConfig->Nicknames.Nick[--pConfig->Nicknames.Count];
				count++;
			}
		}
		SortNicknames(pConfig);
	}
	return count;
}

#ifdef OBSOLETE
static int CompareTactical(const void *One, const void *Two)
{	TIMED_STRING_S *Left = (TIMED_STRING_S*) One;
	TIMED_STRING_S *Right = (TIMED_STRING_S*) Two;
	return strcmp(Left->string+21, Right->string+21);
}

void SortTacticals(CONFIG_INFO_S *pConfig)
{	STRING_LIST_S *pList = &ActiveConfig.Tacticals;
	if (pList->Count > 1)
		qsort(pList->Entries, pList->Count, sizeof(*pList->Entries), CompareTactical);
}

size_t PurgeTacticals(HWND hwnd, CONFIG_INFO_S *pConfig, unsigned long Seconds)
{
	size_t Count = PurgeOldTimedStrings(&pConfig->Tacticals, Seconds);
	if (Count) RealSaveConfiguration(hwnd, pConfig, "Tactical:Purge");
	return Count;
}

int RemoveTactical(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station)
{	TIMED_STRING_LIST_S *pList = &pConfig->Tacticals;
	unsigned long i;
	size_t Size = 9+1;
	char *Temp = (char*)malloc(Size);
	StringCbPrintfA(Temp, Size, "%-9s", Station);
	Size = strlen(Temp);

	i = LocateTimedStringEntry(pList, Temp, Size);
	if (i != -1) RemoveTimedStringEntry(pList, i);
	return i != -1;
}

void RememberTactical(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station, char *Definition, char *From)
{	if (!Definition || !*Definition)
	{	RemoveTactical(hwnd, pConfig, Station);
		return;
	}
	TIMED_STRING_LIST_S *pList = &pConfig->Tacticals;
	unsigned long i;
	size_t Size = 9+1+9+1+1+strlen(Definition)+1;	/* Station(From)=Def */
	char *eol, *Temp = (char*)malloc(Size);
	StringCbPrintfA(Temp, Size, "%-9s(%-9s)=%s", Station, From, Definition);

	eol = Temp+strlen(Temp)-1;
	while (eol >= Temp && (*eol=='\n' || *eol=='\r'))
		*eol-- = '\0';

	i = LocateTimedStringEntry(pList, Temp, 9);	/* Station only */
	AddTimedStringEntryAt(pList, i, Temp, NULL,
							i==-1?1:pList->Entries[i].value);
	free(Temp);

	// RealSaveConfiguration(hwnd, pConfig, "Tactical:Definition");
}

char *RecallTactical(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station, long *pEnabled)
{	TIMED_STRING_LIST_S *pList = &pConfig->Tacticals;
	unsigned long i;
	size_t Size = 9+1;
	char *Temp = (char*)malloc(Size);
	StringCbPrintfA(Temp, Size, "%-9s", Station);
	Size = strlen(Temp);

	i = LocateTimedStringEntry(pList, Temp, Size);
	if (i < pList->Count)
	{	if (pEnabled) *pEnabled = pList->Entries[i].value;
		return _strdup(pList->Entries[i].string+9+1+9+1+1);	/* Caller doesn't want the Station(From)= */
	} else	return NULL;
}

int ToggleTactical(HWND hwnd, CONFIG_INFO_S *pConfig, char *Station)
{	TIMED_STRING_LIST_S *pList = &pConfig->Tacticals;
	unsigned long i;
	size_t Size = 9+1;
	char *Temp = (char*)malloc(Size);
	StringCbPrintfA(Temp, Size, "%-9s", Station);
	Size = strlen(Temp);

	i = LocateTimedStringEntry(pList, Temp, Size);
	if (i != -1)
	{	pList->Entries[i].value = !pList->Entries[i].value;
		return pList->Entries[i].value;
	}
	return FALSE;
}
#endif

static int CompareANDef(const void *One, const void *Two)
{	ANSRVR_GROUP_DEFINITION_S *Left = (ANSRVR_GROUP_DEFINITION_S *)One;
	ANSRVR_GROUP_DEFINITION_S *Right = (ANSRVR_GROUP_DEFINITION_S *)Two;

	return strcmp(Left->Name, Right->Name);
}
static void SortANDefinitions(CONFIG_INFO_S *pConfig)
{
	if (pConfig->ANDefs.Count > 1)
		qsort(pConfig->ANDefs.ANDef, pConfig->ANDefs.Count, sizeof(*pConfig->ANDefs.ANDef), CompareANDef);
}

BOOL RemoveANDefinition(CONFIG_INFO_S *pConfig, char *Name)
{	unsigned int p;

	if (pConfig->ANDefs.Count <= 1) return FALSE;	/* Have to keep one */

	for (p=0; p<pConfig->ANDefs.Count; p++)
	{	if (!_stricmp(pConfig->ANDefs.ANDef[p].Name, Name))
			break;
	}
	if (p >= pConfig->ANDefs.Count) return FALSE;	/* Not found */

	if (pConfig->ANDefs.ANDef[p].Members.Count) return FALSE;	/* has members */
	EmptyTimedStringList(&pConfig->ANDefs.ANDef[p].Members);
	pConfig->ANDefs.ANDef[p] = pConfig->ANDefs.ANDef[--pConfig->ANDefs.Count];
	SortANDefinitions(pConfig);
	return TRUE;
}

BOOL IsValidANGroupName(char *Name)
{
	if (!*Name) return FALSE;
	if (strlen(Name) > sizeof(ActiveConfig.ANDefs.ANDef[0].Name)-1) return FALSE;
	for (char *p=Name; *p; p++)
	{	if (!isalnum(*p&0xff)) return FALSE;
		if (isspace(*p&0xff)) return FALSE;
	}
	return TRUE;
}

ANSRVR_GROUP_DEFINITION_S *GetOrCreateANDefinition(CONFIG_INFO_S *pConfig, char *Name, BOOL AllowCreate)
{	unsigned int p;

	if (!IsValidANGroupName(Name)) return NULL;

	for (p=0; p<pConfig->ANDefs.Count; p++)
	{	if (!_strnicmp(pConfig->ANDefs.ANDef[p].Name, Name, sizeof(pConfig->ANDefs.ANDef[p].Name)-1))
			break;
	}

	if (p >= pConfig->ANDefs.Count)
	{	if (!AllowCreate) return NULL;
		p = pConfig->ANDefs.Count++;
		pConfig->ANDefs.ANDef = (ANSRVR_GROUP_DEFINITION_S *)realloc(p?pConfig->ANDefs.ANDef:NULL, sizeof(*pConfig->ANDefs.ANDef)*pConfig->ANDefs.Count);
		memset(&pConfig->ANDefs.ANDef[p], 0, sizeof(pConfig->ANDefs.ANDef[p]));
		strncpy(pConfig->ANDefs.ANDef[p].Name, Name, sizeof(pConfig->ANDefs.ANDef[p].Name)-1);
		_strupr(pConfig->ANDefs.ANDef[p].Name);
		SortANDefinitions(pConfig);
		return GetOrCreateANDefinition(pConfig, Name, FALSE);
	}
	GetSystemTime(&pConfig->ANDefs.ANDef[p].LastActivity);
	return &pConfig->ANDefs.ANDef[p];
}

int FindANMemberIndex(ANSRVR_GROUP_DEFINITION_S *pAN, char *Member)
{
	if (pAN)
	{	TIMED_STRING_LIST_S *pList = &pAN->Members;
		return LocateTimedStringEntry(&pAN->Members, Member);
	}
	return -1;
}

BOOL AddANDefinitionMember(CONFIG_INFO_S *pConfig, char *Name, char *Member, ANSRVR_GROUP_DEFINITION_S **ppAN)
{	ANSRVR_GROUP_DEFINITION_S *pAN = GetOrCreateANDefinition(pConfig, Name, TRUE);
	if (!pAN) return FALSE;	/* Should never happen, but just in case */

	TIMED_STRING_LIST_S *pList = &pAN->Members;
	BOOL Result;
	unsigned long m;

	if (ppAN) *ppAN = NULL;

	m = FindANMemberIndex(pAN, Member);
	if (m == -1)	/* Not found, add it */
	{	m = AddTimedStringEntry(pList, Member);
		Result = TRUE;	/* New member */
	} else
	{	Result = FALSE;
		UpdateTimedStringEntryAt(pList, m);
	}
	if (ppAN) *ppAN = pAN;
	return Result;
}

BOOL RemoveANDefinitionMember(CONFIG_INFO_S *pConfig, char *Name, char *Member)
{	ANSRVR_GROUP_DEFINITION_S *pAN = GetOrCreateANDefinition(pConfig, Name, FALSE);
	TIMED_STRING_LIST_S *pList = &pAN->Members;
	BOOL Result;
	unsigned long m;

	if (!pAN) return FALSE;	/* No group? Not a member. */

	m = FindANMemberIndex(pAN, Member);
	if (m != -1)	/* Found, remove it */
	{	RemoveTimedStringEntry(pList, m);
		if (!pList->Count)	/* Empty list? */
		{	RemoveANDefinition(pConfig, Name);
		}
		Result = TRUE;
	} else Result = FALSE;
	return Result;
}

MICE_ACTION_S MicEActions[] = {
	/* Name       IntMsg MT-New MTAct  Flash  Color */
	{ "ZERO", "", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Must be first to avoid zero offsets, see qsort() below */
	{ "EMERGENCY!", "!EMERGENCY!", TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, "Red" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Off Duty", "!OFF-DUTY!", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "En Route", "!ENROUTE!", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "In Service", "!INSERVICE!", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Returning", "!RETURNING!", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Committed", "!COMMITTED!", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Special", "!SPECIAL!", TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, "Yellow" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Priority", "!PRIORITY!", TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, "Orange" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Custom-0", "", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Custom-1", "", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Custom-2", "", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Custom-3", "", TRUE ,FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Custom-4", "", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Custom-5", "", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Custom-6", "", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "" },	/* Names must match parse.c MicMessage/MicCustom */
	{ "Alarm", "!ALARM!", TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, "Orange" },
	{ "Alert", "!ALERT!", TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, "Yellow" },
	{ "Warning", "!WARNING!", TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, "Orange" },
	{ "Wx Alarm", "!WXALARM!", TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, "Orange" },
	{ "TEST Alarm", "!TESTALARM!", TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, "Crimson" },
	{ "Sym(Emergency)", "", TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, "Red" },	/* Note: Name is hard-coded in aprsis32.cpp */
	{ "unknown", "", TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, "DarkKhaki" } };	/* Should be last, see qsort() below */
	
#ifdef MICE_NOT_SORTED
static int CompareMultiLineStyle(const void *One, const void *Two)
{	MULTILINE_STYLE_S *Left = (MULTILINE_STYLE_S*) One;
	MULTILINE_STYLE_S*Right = (MULTILINE_STYLE_S*) Two;
	return strcmp(Left->sLineType, Right->sLineType);
}
#endif

unsigned char GetMicEActionIndex(CONFIG_INFO_S *pConfig, char *Name, char *Why)
{	unsigned long p;

	if (!Name || !*Name) return NULL;

	if (pConfig->MicEs.Count != ARRAYSIZE(MicEActions))
	{	for (p=0; p<ARRAYSIZE(MicEActions); p++)
		{	unsigned long n;
			for (n=0; n<pConfig->MicEs.Count; n++)
			{	if (!strcmp(pConfig->MicEs.MicE[n].Name, MicEActions[p].Name))
					break;	/* Already defined */
			}
			if (n >= pConfig->MicEs.Count)
			{	n = pConfig->MicEs.Count++;
				pConfig->MicEs.MicE = (MICE_ACTION_S *)realloc(pConfig->MicEs.MicE, pConfig->MicEs.Count*sizeof(*pConfig->MicEs.MicE));
				pConfig->MicEs.MicE[n] = MicEActions[p];
				TraceLogThread("Config", TRUE, "Defining New Mic-E:%s\n", MicEActions[p].Name);
			}
		}
		if (pConfig->MicEs.Count != ARRAYSIZE(MicEActions))
			TraceLogThread("Config", TRUE, "Warning: %ld Known Mic-Es vs %ld Static Definintions\n",
							(long) pConfig->MicEs.Count, (long) ARRAYSIZE(MicEActions));
#ifdef MICE_NOT_SORTED
		qsort(pConfig->LineStyles.Style, pConfig->LineStyles.Count,
				sizeof(*pConfig->LineStyles.Style), CompareMultiLineStyle);
#endif
	}

	for (p=0; p<pConfig->MicEs.Count; p++)
	{	if (!_strnicmp(pConfig->MicEs.MicE[p].Name, Name, sizeof(pConfig->MicEs.MicE[p].Name)))
			return (unsigned char) p;
	}

	p = pConfig->MicEs.Count-1;

	TraceLogThread("Config", TRUE, "Unknown MiceE(%s) from %s, using %s\n", Name, Why, pConfig->MicEs.MicE[p].Name);

static char Buffer[256];
	sprintf(Buffer,"Unknown MiceE(%s) from %s\n",
			Name, Why);
	QueueDebugMessage(llGetMsec(), Buffer);

	return (unsigned char)p;
}

#ifdef FUTURE
size_t PurgeIgnoreStrings(HWND hwnd, TIMED_STRING_LIST_S *pList)
{
	size_t Count = PurgeOldTimedStrings(&pConfig->TelemetryDefinitions, Seconds);
	if (Count) RealSaveConfiguration(hwnd, pConfig, "Telemetry:Definition:Purge");
	return Count;
}
#endif

void DefineIgnoreString(HWND hwnd, TIMED_STRING_LIST_S *pList, char *Station, long Duration)
{
	AddOrUpdateTimedStringEntry(pList, Station, NULL, Duration);
}

char * FormatIgnoreStringDelta(TIMED_STRING_LIST_S *pList, char *Station, char *Next, size_t Remaining, char **pNext, size_t *pRemaining)
{	unsigned long i = LocateTimedStringEntry(pList, Station);
	if (i == -1)
	{	StringCbPrintfExA(Next, Remaining, pNext, pRemaining,
							STRSAFE_IGNORE_NULLS, 
							"Never");
		return Next;
	}
	return FormatDeltaTime(SecondsSince(&pList->Entries[i].time), Next, Remaining, pNext, pRemaining);
}

BOOL CheckIgnoreString(HWND hwnd, TIMED_STRING_LIST_S *pList, char *Station, BOOL Update)
{	unsigned long i;

	i = LocateTimedStringEntry(pList, Station);
	if (i != -1 && Update)
	{	GetSystemTime(&pList->Entries[i].time);
	}
	return i != -1;
}

void DeleteIgnoreString(HWND hwnd, TIMED_STRING_LIST_S *pList, char *Station)
{	RemoveTimedStringEntry(pList, LocateTimedStringEntry(pList, Station));
}

#ifndef PS_DASHDOT
#define PS_DASHDOT PS_DASH
#endif
#ifndef PS_DOT
#define PS_DOT PS_DASH
#endif

static struct
{	char LineType;
	char *Desc;
	COLORREF Color;
	int Style;
} Default_LineStyles[] = {
/* from http://www.aprs-is.net/WX/MultilineProtocol.aspx */
{ '?',  "Undefined LineType", RGB(64,64,64), PS_DOT },
{ 'a', 	"Red solid Tornado Warning", RGB(192,0,0), PS_SOLID },
{ 'b', 	"Red dashed Tornado Watch", RGB(192,0,0), PS_DASH },
{ 'c', 	"Red double-dashed", RGB(192,0,0), PS_DASHDOT },
{ 'd', 	"Yellow solid Severe Thunderstorm Warning", RGB(192,192,0), PS_SOLID },
{ 'e', 	"Yellow dashed Severe Thunderstorm Watch", RGB(192,192,0), PS_DASH },
{ 'f', 	"Yellow double-dashed", RGB(192,192,0), PS_DASHDOT },
{ 'g', 	"Blue solid Test Warning", RGB(0,0,192), PS_SOLID },
{ 'h', 	"Blue dashed Test Watch", RGB(0,0,192), PS_DASH },
{ 'i', 	"Blue double-dashed", RGB(0,0,192), PS_DASHDOT },
{ 'j', 	"green solid ", RGB(0,192,0), PS_SOLID },
{ 'k', 	"green dashed Mesoscale Discussion Areas", RGB(0,192,0), PS_DASH },
{ 'l', 	"green double-dashed", RGB(0,192,0), PS_DASHDOT },
};
#ifdef FOR_INFO_ONLY
typedef struct MULTILINE_STYLE_S
{	char LineType;
	char *Desc;
	unsigned long Color;
	long Style;
	BOOL ActionEnabled;
} MULTILINE_STYLE_S;
	struct
	{	unsigned long Count;
		MULTILINE_STYLE_S *Style;
	} LineStyles;

	struct
	{	unsigned long Count;
		NWS_PRODUCT_S *Prod;
	} NWSProducts;
#endif

static int CompareMultiLineStyle(const void *One, const void *Two)
{	MULTILINE_STYLE_S *Left = (MULTILINE_STYLE_S*) One;
	MULTILINE_STYLE_S*Right = (MULTILINE_STYLE_S*) Two;
	return strcmp(Left->sLineType, Right->sLineType);
}

MULTILINE_STYLE_S *GetMultiLineStyle(CONFIG_INFO_S *pConfig, char LineType, char *Why)
{	unsigned long p;
static	MULTILINE_STYLE_S DefStyle = {0};

	if (!isprint(LineType & 0xff)) return NULL;

	if (!pConfig->LineStyles.Count)
	{	pConfig->LineStyles.Style = (MULTILINE_STYLE_S *)calloc(ARRAYSIZE(Default_LineStyles), sizeof(*pConfig->LineStyles.Style));
		for (p=0; p<ARRAYSIZE(Default_LineStyles); p++)
		{	unsigned long n;
			for (n=0; n<pConfig->LineStyles.Count; n++)
				if (*pConfig->LineStyles.Style[n].sLineType == Default_LineStyles[p].LineType)
					break;
			if (n < pConfig->LineStyles.Count)		/* Duplicate! */
				TraceLogThread("Config", TRUE, "Skipping Duplicate MultiLineStyle[%ld](%c) (%s) vs (%s)\n",
								(long) p, Default_LineStyles[p].LineType,
								Default_LineStyles[p].Desc, 
								pConfig->LineStyles.Style[n].Desc);
			else
			{	n = pConfig->LineStyles.Count++;
				MULTILINE_STYLE_S *Style = &pConfig->LineStyles.Style[n];
				*Style->sLineType = Default_LineStyles[p].LineType;
				strncpy(Style->Desc, Default_LineStyles[p].Desc, sizeof(Style->Desc));
				Style->Color = Default_LineStyles[p].Color;
				Style->Style = Default_LineStyles[p].Style;
				Style->ActionEnabled = TRUE;
			}
		}
		qsort(pConfig->LineStyles.Style, pConfig->LineStyles.Count,
				sizeof(*pConfig->LineStyles.Style), CompareMultiLineStyle);
	}

	for (p=0; p<pConfig->LineStyles.Count; p++)
	{	if (*pConfig->LineStyles.Style[p].sLineType == LineType)
			return &pConfig->LineStyles.Style[p];
	}

	TraceLogThread("Config", TRUE, "Unsupported MultiLineStyle(%c) from %s, Learning from Default\n", LineType, Why);

static char Buffer[256];
	sprintf(Buffer,"Defaulted MultiLineStyle(%c) for %s",
			LineType, Why);
	QueueDebugMessage(llGetMsec(), Buffer);

	for (p=0; p<pConfig->LineStyles.Count; p++)
	{	if (*pConfig->LineStyles.Style[p].sLineType, Default_LineStyles[0].LineType)
		{	DefStyle = pConfig->LineStyles.Style[p];
			break;
		}
	}
	if (p >= pConfig->LineStyles.Count)
	{	*DefStyle.sLineType = Default_LineStyles[0].LineType;
		strncpy(DefStyle.Desc, Default_LineStyles[0].Desc, sizeof(DefStyle.Desc));
		DefStyle.Style = Default_LineStyles[0].Color;
		DefStyle.Color = Default_LineStyles[0].Style;
		DefStyle.ActionEnabled = TRUE;
	}

	p = pConfig->LineStyles.Count++;
	pConfig->LineStyles.Style = (MULTILINE_STYLE_S *)realloc(pConfig->LineStyles.Style, pConfig->LineStyles.Count*sizeof(*pConfig->LineStyles.Style));

	pConfig->LineStyles.Style[p] = DefStyle;
	*pConfig->LineStyles.Style[p].sLineType = LineType;

	qsort(pConfig->LineStyles.Style, pConfig->LineStyles.Count,
			sizeof(*pConfig->LineStyles.Style), CompareMultiLineStyle);

	return GetMultiLineStyle(pConfig, LineType, Why);
}

static struct
{	char LineType;
	char *PID;	/* Product from http://www.aprs-is.net/WX/ProdCodes.aspx */
	char *Desc;
	char *TabSym;
} Default_NWS_Products[] = {
{'l', "???","Undefined NWS Product ID", "/." },	/* Default is [0] */
{'l', "ADR","Administrative Message", "\\<" },	/* Advisory */
{'b', "AVA","Avalanche Watch","\\*" },	/* Snow */
{'a', "AVW","Avalanche Warning","\\*" },	/* Snow */
{'a', "BZW","Blizzard Warning","\\B" },	/* Snow Blowing */
{'i', "CAE","Child Abduction Emergency","/!" },	/* Police */
{'c', "CDW","Civil Danger Warning", "\\c" },	/* Civil Defense */
{'i', "CEM","Civil Emergency Message", "\\c" },	/* Civil Defense */
{'e', "CFA","Coastal Flood Watch", "\\w" },	/* Flooding */
{'d', "CFW","Coastal Flood Warning", "\\w" },	/* Flooding */
{'g', "DMO","Practice/Demo Warning", "\\<" },	/* Advisory */
{'d', "DSW","Dust Storm Warning", "\\b" },	/* Dust blwng */
{'c', "EAN","Emergency Action Notification", "\\!" },	/* Emergency */
{'i', "EAT","Emergency Action Termination", "\\!" },	/* Emergency */
{'a', "EQW","Earthquake Warning", "\\Q" },	/* Quake */
{'c', "EVI","Evacuation Immediate", "\\<" },	/* Advisory */
{'b', "FFA","Flash Flood Watch", "\\w" },	/* Flooding */
{'c', "FFS","Flash Flood Statement", "\\w" },	/* Flooding */
{'a', "FFW","Flash Flood Warning", "\\w" },	/* Flooding */
{'e', "FLA","Flood Watch", "\\w" },	/* Flooding */
{'f', "FLS","Flood Statement", "\\w" },	/* Flooding */
{'a', "FLW","Flood Warning", "\\w" },	/* Flooding */
{'a', "FRW","Fire Warning", "/:" },	/* Fire */
{'c', "HLS","Hurricane Statement", "\\@" },	/* Hurricane */
{'d', "HMW","Hazardous Materials Warning", "WH" },	/* Hazardous Waste */
{'b', "HUA","Hurricane Watch", "\\@" },	/* Hurricane */
{'a', "HUW","Hurricane Warning", "\\@" },	/* Hurricane */
{'e', "HWA","High Wind Watch", "\\g" },	/* Gale */
{'d', "HWW","High Wind Warning", "\\g" },	/* Gale */
{'c', "LAE","Local Area Emergency", "\\!" },	/* Emergency */
{'d', "LEW","Law Enforcement Warning","/!" },	/* Police */
{'i', "NIC","National Information Center", "/o" },	/* EOC */
{'i', "NMN","Network Message Notification", "\\<" },	/* Advisory */
{'h', "NPT","National Periodic Test", "\\<" },	/* Advisory */
{'a', "NUW","Nuclear Power Plant Warning", "N%" },	/* Nuclear Power */
{'a', "RHW","Radiological Hazard Warning", "N%" },	/* Nuclear Power */
{'h', "RMT","Required Monthly Test", "\\<" },	/* Advisory */
{'h', "RWT","Required Weekly Test", "\\<" },	/* Advisory */
{'d', "SMW","Special Marine Warning", "\\C" },	/* Coast G'rd */
{'f', "SPS","Special Weather Statement", "\\<" },	/* Advisory */
{'f', "SPW","Shelter in Place Warning", "/z" },	/* Shelter */
{'e', "SVA","Severe Thunderstorm Watch", "\\T" },	/* T'storm */
{'d', "SVR","Severe Thunderstorm Warning", "\\T" },	/* T'storm */
{'f', "SVS","Severe Weather Statement", "\\<" },	/* Advisory */
{'b', "TOA","Tornado Watch", "\\t" },	/* Tornado */
{'i', "TOE","911 Telephone Outage Emergency", "/$" },	/* Phone */
{'a', "TOR","Tornado Warning", "\\t" },	/* Tornado */
{'e', "TRA","Tropical Storm Watch", "/@" },	/* HC Future */
{'d', "TRW","Tropical Storm Warning", "/@" },	/* HC Future */
{'e', "TSA","Tsunami Watch", "\\N" },	/* Nav Buoy */
{'d', "TSW","Tsunami Warning", "\\N" },	/* Nav Buoy */
{'a', "VOW","Volcano Warning", "\\E" },	/* Smoke? */
{'e', "WSA","Winter Storm Watch","\\G" },	/* Snow Shwr */
{'d', "WSW","Winter Storm Warning","\\G" },	/* Snow Shwr */

// BFW, BUF, UNK - Currently unknown and learning!

/* WXSVR-AU Product Codes from http://wxsvr.aprs.net.au/products.php */
{'c', "BFA", "Bushfire Alert", "/:" },	/* Fire */
{'i', "BWA", "Bushwalking Advice", "/[" },	/* Jogger */
{'i', "FBN", "Fire Ban", "/:" },	/* Fire */
{'a', "FLW", "Flood Warning", "\\w" },	/* Flooding */
{'f', "FOG", "Fog Advisory", "\\{" },	/* Fog */
{'f', "FST", "Frost Advisory", "\\<" },	/* Advisory */
{'f', "FWW", "Fire Weather", "/:" },	/* Fire */
{'d', "GLE", "Gale Warning", "\\g" },	/* Gale */
{'d', "ICE", "Ice Warning", "\\F" },	/* Fr'ze Rain */
{'f', "RWA", "Road Weather Alert", "/!" },	/* Police */
{'i', "SGW", "Sheep Weather / Graziers", "/p" },	/* Rover (Dog */
{'d', "SNW", "Snow Warning", "\\G" },	/* Snow Shwr */
{'d', "SQL", "Squall Warning", "\\`" },	/* Rain */
{'d', "STS", "Severe Thunderstorm", "\\T" },	/* T'storm */
{'f', "SVR", "Severe Weather", "\\<" },	/* Advisory */
{'e', "SWW", "Strong Wind", "\\g" },	/* Gale */
{'a', "TCY", "Cyclone Warning", "\\@" },	/* Hurricane */

{'l', "ZZZ","Internal APRSISCE/32 Test", "/." },	/* Generated Test objects */

};

static int CompareNWSProduct(const void *One, const void *Two)
{	NWS_PRODUCT_S *Left = (NWS_PRODUCT_S*) One;
	NWS_PRODUCT_S*Right = (NWS_PRODUCT_S*) Two;
	if (Left->LineType < Right->LineType) return -1;
	else if (Left->LineType > Right->LineType) return 1;
	else return strcmp(Left->PID, Right->PID);
}

void SortNWSProducts(CONFIG_INFO_S *pConfig)
{	if (pConfig->NWSProducts.Count)
		qsort(pConfig->NWSProducts.Prod, pConfig->NWSProducts.Count,
				sizeof(*pConfig->NWSProducts.Prod), CompareNWSProduct);
}

NWS_PRODUCT_S *GetNWSProduct(CONFIG_INFO_S *pConfig, char *PID, char *From, char *Via)
{	unsigned long p;
static	NWS_PRODUCT_S DefProd = {0};

	if (!pConfig->NWSProducts.Count)
	{	pConfig->NWSProducts.Prod = (NWS_PRODUCT_S *)calloc(ARRAYSIZE(Default_NWS_Products), sizeof(*pConfig->NWSProducts.Prod));
		for (p=0; p<ARRAYSIZE(Default_NWS_Products); p++)
		{	unsigned long n;
			for (n=0; n<pConfig->NWSProducts.Count; n++)
				if (!_stricmp(pConfig->NWSProducts.Prod[n].PID, Default_NWS_Products[p].PID))
					break;
			if (n < pConfig->NWSProducts.Count)		/* Duplicate! */
				TraceLogThread("Config", TRUE, "Skipping Duplicate NWS Product[%ld](%s) (%s) vs (%s)\n",
								(long) p, Default_NWS_Products[p].PID,
								Default_NWS_Products[p].Desc, 
								pConfig->NWSProducts.Prod[n].Desc);
			else
			{	n = pConfig->NWSProducts.Count++;
				NWS_PRODUCT_S *Prod = &pConfig->NWSProducts.Prod[n];
				strncpy(Prod->PID, Default_NWS_Products[p].PID, sizeof(Prod->PID));
				strncpy(Prod->Desc, Default_NWS_Products[p].Desc, sizeof(Prod->Desc));
				Prod->LineType = Default_NWS_Products[p].LineType;
				Prod->ActionEnabled = TRUE;
				switch (strlen(Default_NWS_Products[p].TabSym))
				{
				case 0:
					Prod->Symbol.Table = '/';
					Prod->Symbol.Symbol = '.';
					break;
				case 1:
					Prod->Symbol.Table = '/';
					Prod->Symbol.Symbol = *Default_NWS_Products[p].TabSym;
					break;
				default:
					Prod->Symbol.Table = *Default_NWS_Products[p].TabSym;
					Prod->Symbol.Symbol = Default_NWS_Products[p].TabSym[1];
					break;
				}
			}
		}
		SortNWSProducts(pConfig);
	}

	for (p=0; p<pConfig->NWSProducts.Count; p++)
	{	if (!_stricmp(pConfig->NWSProducts.Prod[p].PID, PID))
			return &pConfig->NWSProducts.Prod[p];
	}

	TraceLogThread("Config", TRUE, "Undefined NWS ProductID(%s) from %s via %s, Learning from Default\n", PID, From, Via);

static char Buffer[256];
	sprintf(Buffer,"Defaulted NWS Product(%s) for %s via %s",
			PID, From, Via);
	QueueDebugMessage(llGetMsec(), Buffer);

	if (Via && *Via)
		sprintf(Buffer,"New NWS Product(%s) for %s from %s", PID, From, Via);
	else sprintf(Buffer,"New NWS Product(%s) for %s", PID, From);
	QueueInternalMessage(Buffer, FALSE);

	for (p=0; p<pConfig->NWSProducts.Count; p++)
	{	if (!_stricmp(pConfig->NWSProducts.Prod[p].PID, Default_NWS_Products[0].PID))
		{	DefProd = pConfig->NWSProducts.Prod[p];
			break;
		}
	}
	if (p >= pConfig->NWSProducts.Count)
	{	strncpy(DefProd.PID, Default_NWS_Products[0].PID, sizeof(DefProd.PID));
		strncpy(DefProd.Desc, Default_NWS_Products[0].Desc, sizeof(DefProd.Desc));
		DefProd.LineType = Default_NWS_Products[0].LineType;
		DefProd.ActionEnabled = TRUE;
		DefProd.Symbol.Table = '/';
		DefProd.Symbol.Symbol = '.';
	}

	p = pConfig->NWSProducts.Count++;
	pConfig->NWSProducts.Prod = (NWS_PRODUCT_S *)realloc(pConfig->NWSProducts.Prod, pConfig->NWSProducts.Count*sizeof(*pConfig->NWSProducts.Prod));

	pConfig->NWSProducts.Prod[p] = DefProd;
	strncpy(pConfig->NWSProducts.Prod[p].PID, PID, sizeof(pConfig->NWSProducts.Prod[p].PID));
	if (Via && *Via)
		StringCbPrintfA(pConfig->NWSProducts.Prod[p].Desc, sizeof(pConfig->NWSProducts.Prod[p].Desc), "New NWS Product ID for %s via %s", From, Via);
	else StringCbPrintfA(pConfig->NWSProducts.Prod[p].Desc, sizeof(pConfig->NWSProducts.Prod[p].Desc), "New NWS Product ID for %s", From);

	SortNWSProducts(pConfig);

	return GetNWSProduct(pConfig, PID, From, Via);
}

static int CompareNWSServer(const void *One, const void *Two)
{	NWS_ENTRY_SERVER_S *Left = (NWS_ENTRY_SERVER_S*) One;
	NWS_ENTRY_SERVER_S*Right = (NWS_ENTRY_SERVER_S*) Two;
	return strcmp(Left->EntryCall, Right->EntryCall);
}

NWS_ENTRY_SERVER_S *GetNWSServer(CONFIG_INFO_S *pConfig, char *EntryCall, char *Why, char *srcCall, BOOL Remember)
{	unsigned long p;
static	NWS_ENTRY_SERVER_S DefSrv = {0};

	if (!pConfig->NWSServers.Count)
	{	pConfig->NWSServers.Srv = (NWS_ENTRY_SERVER_S *)calloc(3, sizeof(*pConfig->NWSServers.Srv));

		strcpy(pConfig->NWSServers.Srv[0].EntryCall, "AE5PL-WX");
		strcpy(pConfig->NWSServers.Srv[0].Desc, "AE5PL's US NWS Server");
		strcpy(pConfig->NWSServers.Srv[0].Finger.Server, "wxsvr.ae5pl.net");
		pConfig->NWSServers.Srv[0].Finger.Port = 79;

		strcpy(pConfig->NWSServers.Srv[1].EntryCall, "WE7U-WX");
		strcpy(pConfig->NWSServers.Srv[1].Desc, "WE7U's Firenet NWS Server");

		strcpy(pConfig->NWSServers.Srv[2].EntryCall, "WXSVR-AU");
		strcpy(pConfig->NWSServers.Srv[2].Desc, "VK2XJG's Australian NWS Server");

		pConfig->NWSServers.Count = 3;

		qsort(pConfig->NWSServers.Srv, pConfig->NWSServers.Count,
				sizeof(*pConfig->NWSServers.Srv), CompareNWSServer);
	}
	if (!EntryCall || !*EntryCall) return NULL;

	for (p=0; p<pConfig->NWSServers.Count; p++)
	{	if (!_stricmp(pConfig->NWSServers.Srv[p].EntryCall, EntryCall))
			return &pConfig->NWSServers.Srv[p];
	}

	if (!Remember) return NULL;

static char Buffer[256];
	if (*Why != 'q'		/* Entry must be from q-Construct */
	|| !_stricmp(Why,"qAR"))	/* And NOT RF-Received */
	{
		TraceLogThread("Config", TRUE, "NOT Defaulting NWS EntryCall(%s) for %s from %s",
						EntryCall, Why, srcCall);
//		sprintf(Buffer,"%s>DEBUG::%-9s:NOT Defaulting NWS EntryCall(%s) for %s from %s",
//			CALLSIGN, "KJ4ERJ-DB", EntryCall, Why, srcCall);
//		QueueDebugMessage(llGetMsec(), Buffer);
		return NULL;
	}
	TraceLogThread("Config", TRUE, "New NWS EntryCall(%s) for %s from %s, Learning from Default\n", EntryCall, Why, srcCall);

	sprintf(Buffer,"Defaulted NWS EntryCall(%s) for %s from %s",
			EntryCall, Why, srcCall);
	QueueDebugMessage(llGetMsec(), Buffer);

	sprintf(Buffer,"New NWS EntryCall(%s) for %s from %s",
			EntryCall, Why, srcCall);
	QueueInternalMessage(Buffer, FALSE);

	p = pConfig->NWSServers.Count++;
	pConfig->NWSServers.Srv = (NWS_ENTRY_SERVER_S *)realloc(pConfig->NWSServers.Srv, pConfig->NWSServers.Count*sizeof(*pConfig->NWSServers.Srv));

	memset(&pConfig->NWSServers.Srv[p], 0, sizeof(pConfig->NWSServers.Srv[p]));

	strcpy(pConfig->NWSServers.Srv[p].EntryCall, EntryCall);
	strcpy(pConfig->NWSServers.Srv[p].Desc, "Newly Discovered NWS EntryCall");
	pConfig->NWSServers.Srv[p].Disabled = TRUE;

	qsort(pConfig->NWSServers.Srv, pConfig->NWSServers.Count,
			sizeof(*pConfig->NWSServers.Srv), CompareNWSServer);

	return GetNWSServer(pConfig, EntryCall, Why, srcCall, Remember);
}

#ifndef RGB
#define RGB(r,g,b)          ((long)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned long)(unsigned char)(b))<<16)))
#endif

/* See: http://en.wikipedia.org/wiki/Web_colors */

/* From: http://en.wikipedia.org/wiki/X11_color_names */
/*
Color variations

For 78 colors as listed above, rgb.txt offers four variants ‘color 1’, ‘color 2’, ‘color 3’, and ‘color 4’, with ‘color 1’ corresponding to ‘color’, so e.g. ‘Snow 1’ is the same as ‘Snow’. These variations are neither supported by popular browsers nor adopted by W3C standards.

The formulae used to determine the RGB values for these variations appear to be somewhere near

    color 2 := color × 93.2%
    color 3 := color × 80.4%
    color 4 := color × 54.8%

Examples:

    205.2 = 255 × 80.4/100 and 192.96 = 240 × 80.4/100 explain ‘Ivory 3’ (205, 205, 196) based on ‘Ivory’ (255, 255, 240).
    139.74 = 255 × 54.8/100 and 131.52 = 240 × 54.8/100 are close to ‘Azure 4’ (131, 139, 139) based on ‘Azure’ (240, 255, 255).
    237.66 = 255 × 93.2/100 yields ‘Yellow 2’ (238, 238, 0) based on ‘Yellow’ (255, 255 ,0).
*/

static struct
{	char Name[COLOR_SIZE];
	long Color;
	BOOL Basic16;
} DefaultColors[] = {
{ "aliceblue", RGB(0xF0,0xF8,0xFF) },
{ "antiquewhite", RGB(0xFA,0xEB,0xD7) },
{ "aqua", RGB(0x00,0xFF,0xFF), TRUE },
{ "aquamarine", RGB(0x7F,0xFF,0xD4) },
{ "azure", RGB(0xF0,0xFF,0xFF) },
{ "beige", RGB(0xF5,0xF5,0xDC) },
{ "bisque", RGB(0xFF,0xE4,0xC4) },
{ "black", RGB(0x00,0x00,0x00), TRUE },
{ "blanchedalmond", RGB(0xFF,0xEB,0xCD) },
{ "blue", RGB(0x00,0x00,0xFF), TRUE },
{ "blueviolet", RGB(0x8A,0x2B,0xE2) },
{ "brown", RGB(0xA5,0x2A,0x2A) },
{ "burlywood", RGB(0xDE,0xB8,0x87) },
{ "cadetblue", RGB(0x5F,0x9E,0xA0) },
{ "chartreuse", RGB(0x7F,0xFF,0x00) },
{ "chocolate", RGB(0xD2,0x69,0x1E) },
{ "coral", RGB(0xFF,0x7F,0x50) },
{ "cornflowerblue", RGB(0x64,0x95,0xED) },
{ "cornsilk", RGB(0xFF,0xF8,0xDC) },
{ "crimson", RGB(0xDC,0x14,0x3C) },
{ "cyan", RGB(0x00,0xFF,0xFF) },
{ "darkblue", RGB(0x00,0x00,0x8B) },
{ "darkcyan", RGB(0x00,0x8B,0x8B) },
{ "darkgoldenrod", RGB(0xB8,0x86,0x0B) },
{ "darkgray", RGB(0xA9,0xA9,0xA9) },
{ "darkgreen", RGB(0x00,0x64,0x00) },
{ "darkkhaki", RGB(0xBD,0xB7,0x6B) },
{ "darkmagenta", RGB(0x8B,0x00,0x8B) },
{ "darkolivegreen", RGB(0x55,0x6B,0x2F) },
{ "darkorange", RGB(0xFF,0x8C,0x00) },
{ "darkorchid", RGB(0x99,0x32,0xCC) },
{ "darkred", RGB(0x8B,0x00,0x00) },
{ "darksalmon", RGB(0xE9,0x96,0x7A) },
{ "darkseagreen", RGB(0x8F,0xBC,0x8F) },
{ "darkslateblue", RGB(0x48,0x3D,0x8B) },
{ "darkslategray", RGB(0x2F,0x4F,0x4F) },
{ "darkturquoise", RGB(0x00,0xCE,0xD1) },
{ "darkviolet", RGB(0x94,0x00,0xD3) },
{ "deeppink", RGB(0xFF,0x14,0x93) },
{ "deepskyblue", RGB(0x00,0xBF,0xBF) },
{ "dimgray", RGB(0x69,0x69,0x69) },
{ "dodgerblue", RGB(0x1E,0x90,0xFF) },
{ "firebrick", RGB(0xB2,0x22,0x22) },
{ "floralwhite", RGB(0xFF,0xFA,0xF0) },
{ "forestgreen", RGB(0x22,0x8B,0x22) },
{ "fuchsia", RGB(0xFF,0x00,0xFF), TRUE },
{ "ghostwhite", RGB(0xF8,0xF8,0xFF) },
{ "gainsboro", RGB(0xDC,0xDC,0xDC) },
{ "gold", RGB(0xFF,0xD7,0x00) },
{ "goldenrod", RGB(0xDA,0xA5,0x20) },
{ "gray", RGB(0x80,0x80,0x80), TRUE },
{ "green", RGB(0x00,0x80,0x00), TRUE },
{ "greenyellow", RGB(0xAD,0xFF,0x2F) },
{ "honeydew", RGB(0xF0,0xFF,0xF0) },
{ "hotpink", RGB(0xFF,0x69,0xB4) },
{ "indianred", RGB(0xCD,0x5C,0x5C) },
{ "indigo", RGB(0x4B,0x00,0x82) },
{ "ivory", RGB(0xFF,0xFF,0xF0) },
{ "khaki", RGB(0xF0,0xE6,0x8C) },
{ "lavender", RGB(0xE6,0xE6,0xFA) },
{ "lavenderblush", RGB(0xFF,0xF0,0xF5) },
{ "lawngreen", RGB(0x7C,0xFC,0x00) },
{ "lemonchiffon", RGB(0xFF,0xFA,0xCD) },
{ "lightblue", RGB(0xAD,0xD8,0xE6) },
{ "lightcoral", RGB(0xF0,0x80,0x80) },
{ "lightcyan", RGB(0xE0,0xFF,0xFF) },
{ "lightgoldenrodyellow", RGB(0xFA,0xFA,0xD2) },
{ "lightgreen", RGB(0x90,0xEE,0x90) },
{ "lightgrey", RGB(0xD3,0xD3,0xD3) },
{ "lightpink", RGB(0xFF,0xB6,0xC1) },
{ "lightsalmon", RGB(0xFF,0xA0,0x7A) },
{ "lightseagreen", RGB(0x20,0xB2,0xAA) },
{ "lightskyblue", RGB(0x87,0xCE,0xFA) },
{ "lightslategray", RGB(0x77,0x88,0x99) },
{ "lightsteelblue", RGB(0xB0,0xC4,0xDE) },
{ "lightyellow", RGB(0xFF,0xFF,0xE0) },
{ "lime", RGB(0x00,0xFF,0x00), TRUE },
{ "limegreen", RGB(0x32,0xCD,0x32) },
{ "linen", RGB(0xFA,0xF0,0xE6) },
{ "magenta", RGB(0xFF,0x00,0xFF) },
{ "maroon", RGB(0x80,0x00,0x00), TRUE },
{ "mediumaquamarine", RGB(0x66,0xCD,0xAA) },
{ "mediumblue", RGB(0x00,0x00,0xCD) },
{ "mediumorchid", RGB(0xBA,0x55,0xD3) },
{ "mediumpurple", RGB(0x93,0x70,0xDB) },
{ "mediumseagreen", RGB(0x3C,0xB3,0x71) },
{ "mediumslateblue", RGB(0x7B,0x68,0xEE) },
{ "mediumspringgreen", RGB(0x00,0xFA,0x9A) },
{ "mediumturquoise", RGB(0x48,0xD1,0xCC) },
{ "mediumvioletred", RGB(0xC7,0x15,0x85) },
{ "midnightblue", RGB(0x19,0x19,0x70) },
{ "mintcream", RGB(0xF5,0xFF,0xFA) },
{ "mistyrose", RGB(0xFF,0xE4,0xE1) },
{ "moccasin", RGB(0xFF,0xE4,0xB5) },
{ "navajowhite", RGB(0xFF,0xDE,0xAD) },
{ "navy", RGB(0x00,0x00,0x80), TRUE },
{ "oldlace", RGB(0xFD,0xF5,0xE6) },
{ "olive", RGB(0x80,0x80,0x00), TRUE },
{ "olivedrab", RGB(0x6B,0x8E,0x23) },
{ "orange", RGB(0xFF,0xA5,0x00) },
{ "orangered", RGB(0xFF,0x45,0x00) },
{ "orchid", RGB(0xDA,0x70,0xD6) },
{ "palegoldenrod", RGB(0xEE,0xE8,0xAA) },
{ "palegreen", RGB(0x98,0xFB,0x98) },
{ "paleturquoise", RGB(0xAF,0xEE,0xEE) },
{ "palevioletred", RGB(0xDB,0x70,0x93) },
{ "papayawhip", RGB(0xFF,0xEF,0xD5) },
{ "peachpuff", RGB(0xFF,0xDA,0xB9) },
{ "peru", RGB(0xCD,0x85,0x3F) },
{ "pink", RGB(0xFF,0xC0,0xCB) },
{ "plum", RGB(0xDD,0xA0,0xDD) },
{ "powderblue", RGB(0xB0,0xE0,0xE6) },
{ "purple", RGB(0x80,0x00,0x80), TRUE },
{ "red", RGB(0xFF,0x00,0x00), TRUE },
{ "rosybrown", RGB(0xBC,0x8F,0x8F) },
{ "royalblue", RGB(0x41,0x69,0xE1) },
{ "saddlebrown", RGB(0x8B,0x45,0x13) },
{ "salmon", RGB(0xFA,0x80,0x72) },
{ "sandybrown", RGB(0xF4,0xA4,0x60) },
{ "seagreen", RGB(0x2E,0x8B,0x57) },
{ "seashell", RGB(0xFF,0xF5,0xEE) },
{ "sienna", RGB(0xA0,0x52,0x2D) },
{ "silver", RGB(0xC0,0xC0,0xC0), TRUE },
{ "skyblue", RGB(0x87,0xCE,0xEB) },
{ "slateblue", RGB(0x6A,0x5A,0xCD) },
{ "slategray", RGB(0x70,0x80,0x90) },
{ "snow", RGB(0xFF,0xFA,0xFA) },
{ "springgreen", RGB(0x00,0xFF,0x7F) },
{ "steelblue", RGB(0x46,0x82,0xB4) },
{ "tan", RGB(0xD2,0xB4,0x8C) },
{ "teal", RGB(0x00,0x80,0x80), TRUE },
{ "thistle", RGB(0xD8,0xBF,0xD8) },
{ "tomato", RGB(0xFF,0x63,0x47) },
{ "turquoise", RGB(0x40,0xE0,0xD0) },
{ "violet", RGB(0xEE,0x82,0xEE) },
{ "wheat", RGB(0xF5,0xDE,0xB3) },
{ "white", RGB(0xFF,0xFF,0xFF), TRUE },
{ "whitesmoke", RGB(0xF5,0xF5,0xF5) },
{ "yellow", RGB(0xFF,0xFF,0x00), TRUE },
{ "yellowgreen", RGB(0x9A,0xCD,0x32) } };

BOOL DefineColorChoices(CONFIG_INFO_S *pConfig, BOOL All)
{	BOOL Result = FALSE;
	if (All || pConfig->ColorChoices.Count < ARRAYSIZE(DefaultColors))
	{	int i;
		for (i=0; i<ARRAYSIZE(DefaultColors); i++)
		if (All || DefaultColors[i].Basic16)
		{	if (LocateTimedStringEntry(&pConfig->ColorChoices, DefaultColors[i].Name) == -1)
			{	AddTimedStringEntry(&pConfig->ColorChoices, DefaultColors[i].Name, NULL, DefaultColors[i].Color);
				Result = TRUE;
			}
		}
	}
	return Result;
}

COLORREF GetColorRGB(CONFIG_INFO_S *pConfig, char *Name, char *Why)
{	unsigned long i;
//	TraceLogThread("Colors", FALSE, "%s:GetColorRGB(%s)\n", Why, Name);
	for (i=0; i<pConfig->ColorChoices.Count; i++)
	if (!_stricmp(pConfig->ColorChoices.Entries[i].string, Name))
	{	return pConfig->ColorChoices.Entries[i].value;
	}
	for (i=0; i<ARRAYSIZE(DefaultColors); i++)
	if (!_stricmp(DefaultColors[i].Name, Name))
	{	return DefaultColors[i].Color;
	}
	TraceError(NULL, "%s:Undefine Color(%s), returning BLACK\n", Why, Name);
	return 0;
}

char *GetRGBColorName(CONFIG_INFO_S *pConfig, COLORREF Color)
{	unsigned long i;

	for (i=0; i<pConfig->ColorChoices.Count; i++)
	if (pConfig->ColorChoices.Entries[i].value == Color)
	{	return pConfig->ColorChoices.Entries[i].string;
	}
	for (i=0; i<ARRAYSIZE(DefaultColors); i++)
	if (DefaultColors[i].Color == Color)
	{	return DefaultColors[i].Name;
	}
	TraceError(NULL, "Undefined RGBColor(0x%lX), returning *unknown*\n", (long) Color);
	return "*unknown*";
}

BOOL DefineTrackColors(CONFIG_INFO_S *pConfig)
{	BOOL Result = FALSE;
	if (!pConfig->TrackColors.Count)
	{	int i;
		for (i=0; i<ARRAYSIZE(DefaultColors); i++)
		if (DefaultColors[i].Basic16)
		{	if (LocateTimedStringEntry(&pConfig->TrackColors, DefaultColors[i].Name) == -1)
			{
				AddTimedStringEntry(&pConfig->TrackColors, DefaultColors[i].Name, NULL, !_stricmp(DefaultColors[i].Name,"black"));
				Result = TRUE;
			}
		}
	}
	return Result;
}



#ifdef TRACKER2_SECRETS
1184 	// SECRET - Create one-time password generator key
1185 	unsigned char cmd_secret(unsigned char *cmdline, unsigned char id, unsigned char index)
1186 	{
1187 	        unsigned char *p, len, c, *q;
1188 	        unsigned long newkey[4];
1189 	       
1190 	        // Set initialization vector
1191 	        newkey[0] = 0x25B58745;
1192 	        newkey[1] = 0x97119BC5;
1193 	        newkey[2] = 0xB556AE25;
1194 	        newkey[3] = 0xCAA24730;
1195 	       
1196 	        if (*cmdline)
1197 	        {
1198 	                len = strlen(cmdline);
1199 	                // Minimum secret length is 8 characters
1200 	                if (len < 8) return 1;
1201 	                // Zero-pad remainder of command buffer
1202 	                p = cmdbuf + sizeof(cmdbuf)-1;
1203 	                q = strend(cmdline);
1204 	                while (p > q) *p-- = 0;
1205 	                // Hash command line to get new key
1206 	                for (c = 0; c < len; c+=16)
1207 	                {
1208 	                        // Key is each 8-byte block of pass phrase in succession
1209 	                        reset_watchdog;
1210 	                        enctea(newkey, (unsigned long *)(cmdline + c));
1211 	                        enctea(newkey+2, (unsigned long *)(cmdline + c + 8));
1212 	                }
1213 	                // Save key
1214 	                memcpy(dev_config->secret, newkey, sizeof(newkey));
1215 	                dev_config->otp_sequence = 0;
1216 	                otp_sequence = 0;
1217 	                strcpy(cmdbuf, "Set.");
1218 	        }
1219 	        else
1220 	        {
1221 	                // Report next OTP sequence
1222 	                q = get_command_name(id, index, cmdbuf);
1223 	                itoa(otp_sequence, q);
1224 	        }
1225 	        return 0;
1226 	}

// Passlist - Generate list of one-time passwords
1247 	unsigned char cmd_passlist(unsigned char *cmdline, unsigned char id, unsigned char index)
1248 	{
1249 	        unsigned int i, cnt;
1250 	        unsigned char pw[5];
1251 	        unsigned char c;
1252 	        i = otp_sequence;
1253 	        if (*cmdline)
1254 	        {
1255 	                cnt = atoui(cmdline);
1256 	        }
1257 	        else cnt = 144;
1258 	        for (c = 0; c < cnt; c++, i++)
1259 	        {
1260 	                // Output four 5-bit encoded characters from resulting hash
1261 	                get_password(i, pw);
1262 	                printf("%4u:%s ", i, pw);
1263 	                if (((c & 7) == 7) || (c==cnt-1)) sci_print("\r\n");
1264 	        }
1265 	        *cmdbuf = 0;
1266 	        return 0;
1267 	}

// Return the 4-character password for the given sequence number
1229 	void get_password(unsigned int i, unsigned char *c)
1230 	{
1231 	        unsigned long nonce[2];
1232 	        unsigned char *p;
1233 	        nonce[0] = 0x77A25667;
1234 	        nonce[1] = 0x69436027 ^ i;
1235 	        p = (unsigned char *)(&nonce[1]);
1236 	        // XXTEA-encrypt nonce+counter with secret key to get password
1237 	        reset_watchdog;
1238 	        enctea(nonce, globalcfg.secret);
1239 	        c[0] = otpcharset[p[0] & 0x1f];
1240 	        c[1] = otpcharset[p[1] & 0x1f];
1241 	        c[2] = otpcharset[p[2] & 0x1f];
1242 	        c[3] = otpcharset[p[3] & 0x1f];
1243 	        c[4] = 0;
1244 	}

// Encrypt 64 bits using XXTEA
315 	void enctea(unsigned long* v, unsigned long* k)
316 	{
317 	        unsigned long z=v[1], y=v[0], sum=0;
318 	        unsigned char q, e;
319 	
320 	        for (q = 32; q; q--)
321 	        {
322 	                sum += 0x9e3779b9;
323 	                e = (unsigned char)(sum >> 2) & 3;
324 	                y = v[1];
325 	                z = v[0] += (z>>5^y<<2) + (y>>3^z<<4)^(sum^y) + (k[e]^z);
326 	                y = v[0];
327 	                z =     v[1] += (z>>5^y<<2) + (y>>3^z<<4)^(sum^y) + (k[1^e]^z);
328 	        }
329 	        return;
330 	}
331 	

1 	/* TEA, a Tiny Encryption Algorithm.
2 	    David Wheeler
3 	    Roger Needham
4 	    Computer Laboratory
5 	    Cambridge University
6 	    England
7 	
8 	    Routine, written in the C language, for encoding
9 	    with key k[0] - k[3]. Data in v[0] and v[1].
10 	*/
11 	
12 	void tea_encode(long* v, long* k)
13 	{
14 	        unsigned long y=v[0],z=v[1], sum=0,
15 	                      delta=0x9e3779b9, n=32;
16 	        while (n-- > 0)
17 	        {
18 	                sum += delta;
19 	                y += (z<<4)+k[0] ^ z+sum ^ (z>>5)+k[1];
20 	                z += (y<<4)+k[2] ^ y+sum ^ (y>>5)+k[3];
21 	        }
22 	        v[0]=y;
23 	        v[1]=z;
24 	}
25 	
26 	
27 	void tea_decode(long* v,long* k)
28 	{
29 	        unsigned long n=32, sum, y=v[0], z=v[1],
30 	                      delta=0x9e3779b9;
31 	        sum=delta<<5;
32 	
33 	        while (n-- > 0)
34 	        {
35 	                z-= (y<<4)+k[2] ^ y+sum ^ (y>>5)+k[3];
36 	                y-= (z<<4)+k[0] ^ z+sum ^ (z>>5)+k[1];
37 	                sum-=delta;
38 	        }
39 	        v[0]=y;
40 	        v[1]=z;
41 	}
42 	
43 	
44 	/* A simple improvement is to copy k[0-3] into a,b,c,d before the
45 	   iteration so that the indexing is taken out of the loop. */


#endif