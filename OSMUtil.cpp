#include "sysheads.h"

#pragma warning(disable : 4995)	/* Don't care about deprecated functions */

//#include <math.h>
#include <float.h>
#include <ctype.h>
//#include <stdio.h>
//#include <string.h>
//#include <stdlib.h>
#ifndef UNDER_CE
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

//#include <windows.h>
//#include <winsock.h>
#include <commctrl.h>
//#include <strsafe.h>

#ifdef UNDER_CE
int FrameRect(HDC hdc, CONST RECT *prc, HBRUSH hbr);
#endif

#include "config.h"
#include "tcputil.h"
#include "tracelog.h"

COLORREF GetScaledRGColor(double Current, double RedValue, double GreenValue);
 
void SpinMessages(HWND hwnd)
{	MSG msg;

	while (PeekMessage(&msg, hwnd,  0, 0,
#define PROCESS_ALL
#ifndef PROCESS_ALL
#ifndef UNDER_CE
						PM_QS_INPUT |
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

#ifdef UNDER_CE
#ifdef CE50
#define USE_PNG_IMAGE
#else
#define USE_SHELL_IMAGE
#endif
#else
#define USE_PNG_IMAGE
#define USE_JPEG_IMAGE
#endif

#ifdef USE_SHELL_IMAGE
#include <Aygshell.h>
#endif
#ifdef USE_PNG_IMAGE
#include "pngUtil.h"
#endif
#ifdef USE_JPEG_IMAGE
#include "jpgUtil.h"
#endif

#include "LLUtil.h"
#include "OSMUtil.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static long TilesAttempted=0, TilesFetched=0, TileBytesSent=0, TileBytesRecv=0;

static int CacheCount = 0;
static int CacheSize = 0;
static int CacheMaxInUse = 0;
static int CacheInUse = 0;
#define CACHE_INUSE_FACTOR ((18-0+1)/2)
					/* Cache size relative to MaxInUse (zoom layers cached) */

typedef struct OSM_CACHE_TILE_S
{	OSM_TILE_INFO_S *Tile;
	__int64 msUse;	/* Last use time */
	int UseCount;
} OSM_CACHE_TILE_S;
OSM_CACHE_TILE_S *CacheBitmaps = NULL;

#ifdef UNDER_CE
#define MAX_BIGSx 8
/*	NOTE: Product of X*Y must be <= 32! */
#define BIG_X 4
#define BIG_Y 4
#else
#define MAX_BIGSx 16
/*	NOTE: Product of X*Y must be <= 32! */
#define BIG_X 8
#define BIG_Y 4
#endif

/* Default values layout must match struct definition */
TILE_SERVER_INFO_S OSM = { "Default", "tile.openstreetmap.org", 80, "/", TRUE, "./OSMTiles/", TRUE, 0, 0, 0, 18, 0 };

static BOOL OSMPurgeEnabled = TRUE;

#define OSMRetainDays OSM.RetainDays
#define OSMRetainZoom OSM.RetainZoom

static int ServerCount = 0;
static TILE_SERVER_INFO_S **TileServers = NULL;

unsigned int iBig = 0;
unsigned int nBig = 0;
unsigned int bigCount = 0;
unsigned long *bBigs = NULL;	/* In use bits */
unsigned int *cBigs = NULL;	/* In use count */
HBITMAP *hbmBigs = NULL;	/* Bitmap Handles */

#define sock_init() 							\
{	WORD wVersionRequested = MAKEWORD(1,1);				\
	WSADATA wsaData;						\
	int err = WSAStartup(wVersionRequested, &wsaData);		\
	if (err != 0)							\
	{	/*printf("WSAStartup Failed With %ld\n", (long) err);*/	\
		exit(-1);						\
	}								\
}
#define soclose(s) closesocket(s)
#define ioctl(s) ioctlsocket(s)
#define psock_errno(s) printf("%s errno %ld\n", s, (long) h_errno)
#define sock_errno() h_errno

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int IsDirectory(char *Dir)
{	int Result = FALSE;
	int Size = (strlen(Dir)+1)*sizeof(TCHAR);
	TCHAR *Path = (TCHAR *)malloc(Size);
	WIN32_FIND_DATA fd = {0};
	HANDLE hFile;

	StringCbPrintf(Path,Size,TEXT("%S"),Dir);
	if (strlen(Dir) && (Path[strlen(Dir)-1] == '\\' || Path[strlen(Dir)-1] == '/'))
		Path[strlen(Dir)-1] = *TEXT("");
	hFile = FindFirstFile(Path, &fd);
	if (hFile != INVALID_HANDLE_VALUE)
	{	if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			Result = TRUE;	/* Already a directory */
		FindClose(hFile);
	}
	free(Path);
	return Result;
}

int MakeDirectory(char *Dir)
{	int Result;
#ifdef OLD_WAY
#ifdef UNDER_CE
	int Size = (strlen(Dir)+1)*sizeof(TCHAR);
	TCHAR *Path = (TCHAR *)malloc(Size);
	StringCbPrintf(Path,Size,TEXT("%S"),Dir);
	Result = CreateDirectory(Path,NULL);
	free(Path);
#else
	struct stat st;
	if (stat(Dir,&st) == 0)
		Result = (st.st_mode & S_IFDIR) != 0;
	else Result = !_mkdir(Dir);
#endif
#else
	if (!IsDirectory(Dir))
	{	int Size = (strlen(Dir)+1)*sizeof(TCHAR);
		TCHAR *Path = (TCHAR *)malloc(Size);
		StringCbPrintf(Path,Size,TEXT("%S"),Dir);
		Result = CreateDirectory(Path,NULL);
		free(Path);
	} else Result = TRUE;
#endif

#ifndef VERBOSE
	if (!Result)
#endif
		TraceLogThread("OSM", TRUE, "MakeDirectory(%s) returning %ld\n", Dir, (long) Result);
	return Result;
}

static int DeleteDirectory(char *Dir)
{	BOOL Result;
	int Size = (strlen(Dir)+1)*sizeof(TCHAR);
	TCHAR *Path = (TCHAR *)malloc(Size);
	StringCbPrintf(Path,Size,TEXT("%S"),Dir);
	Result = RemoveDirectory(Path);
	free(Path);
	return Result;
}

static int RemoveFile(char *path)
{
#ifdef UNDER_CE
	BOOL Result;
	int Size = (strlen(path)+1)*sizeof(TCHAR);
	TCHAR *Path = (TCHAR *)malloc(Size);
	StringCbPrintf(Path,Size,TEXT("%S"),path);
	Result = DeleteFile(Path);
	free(Path);
	return Result;
#else
	return !remove(path);
#endif
}

static BOOL OSMUseSingleDigitDirectories(char *Path)
{	BOOL Result = FALSE;
	char Last = strlen(Path)?Path[strlen(Path)-1]:'\0';
	BOOL HaveSlash = (Last=='\\' || Last=='/');
	char *fPath = (char*)malloc(MAX_PATH+strlen(Path));

	for (int zoom=0; zoom<9; zoom++)
	{	sprintf(fPath,"%s%s%ld", Path, HaveSlash?"":"/", (long) zoom);
		if (IsDirectory(fPath))
		{	Result = TRUE;
			break;
		}
	}
	free(fPath);
	TraceLogThread("OSM", TRUE, "OSMUseSingleDigitDirectories[%s]:%s SingleDigit Directories\n", 
					Path, Result?"Has":"NOT");
	return Result;
}

static BOOL OSMSameTileServer(TILE_SERVER_INFO_S *sOne, TILE_SERVER_INFO_S *sTwo)
{	if (!sOne || !sTwo) return FALSE;
	if (sOne->Port != sTwo->Port) return FALSE;
	if (*(unsigned long*)&sOne->Name != *(unsigned long*)&sTwo->Name) return FALSE;
	if (*(unsigned long*)&sOne->Server != *(unsigned long*)&sTwo->Server) return FALSE;
	if (*(unsigned long*)&sOne->URLPrefix != *(unsigned long*)&sTwo->URLPrefix) return FALSE;
	return !strncmp(sOne->Name,sTwo->Name,sizeof(sOne->Name))
		&& !strncmp(sOne->Server,sTwo->Server,sizeof(sOne->Server))
		&& !strncmp(sOne->URLPrefix,sTwo->URLPrefix,sizeof(sOne->URLPrefix));
}

static TILE_SERVER_INFO_S *OSMGetActualTileServer(TILE_SERVER_INFO_S *sInfo)
{	int s;

	for (s=0; s<ServerCount; s++)
	{	if (TileServers[s] == sInfo)
			return TileServers[s];
	}
	for (s=0; s<ServerCount; s++)
	{	if (OSMSameTileServer(TileServers[s], sInfo))
			return TileServers[s];
	}
	return NULL;
}

TILE_SERVER_INFO_S *OSMRegisterTileServer(TILE_SERVER_INFO_S *sInfo, char *From)
{	TILE_SERVER_INFO_S *Result = OSMGetActualTileServer(sInfo);

	if (!Result)
	{	int s = ServerCount++;
		TileServers = (TILE_SERVER_INFO_S **)realloc(TileServers,sizeof(*TileServers)*ServerCount);
		Result = TileServers[s] = (TILE_SERVER_INFO_S *)calloc(1,sizeof(*TileServers[s]));
		TraceLogThread("OSM", TRUE, "OSMRegisterTileServer:NEW[%ld][%s] http://%s:%ld%s/0/0/0.png into %s\n", s, sInfo->Name, sInfo->Server, sInfo->Port, sInfo->URLPrefix, sInfo->Path);
	}
	if (strncmp(Result->Path, sInfo->Path, sizeof(Result->Path)))
	{	sInfo->SingleDigitDirectories = OSMUseSingleDigitDirectories(sInfo->Path);
		TraceLogThread("OSM", TRUE, "%s:OSMRegisterTileServer[%s]@%p:%s SingleDigit from %s\n", 
						From, sInfo->Name, Result,
						sInfo->SingleDigitDirectories?"Using":"NOT Using",
						sInfo->Path);
	} else if (sInfo->SingleDigitDirectories != Result->SingleDigitDirectories)
	{	TraceLogThread("OSM", TRUE, "%s:OSMRegisterTileServer[%s]@%p:%s SingleDigit FIXING CALLER's %s\n", 
						From, sInfo->Name, Result,
						sInfo->SingleDigitDirectories?"Using":"NOT Using",
						sInfo->Path);
		sInfo->SingleDigitDirectories = OSMUseSingleDigitDirectories(sInfo->Path);
	}
#ifdef VERBOSE
	else TraceLogThread("OSM", TRUE, "%s:OSMRegisterTileServer[%s]@%p:%s SingleDigit from CALLER's %s\n", 
						From, sInfo->Name, Result,
						sInfo->SingleDigitDirectories?"Using":"NOT Using",
						sInfo->Path);
#endif
	TILE_SERVER_INFO_S Temp = *Result;
	*Result = *sInfo;
	Result->Private = Temp.Private;
	return Result;
}

//			if (httpGet("tah.openstreetmap.org", 80, "/Tiles/tile/17/36168/54911.png", "test2.png"))
//			if (httpGet("tile.openstreetmap.org", 80, "/Tiles/tile/17/36168/54911.png", "test2.png"))
//			if (httpGet("192.168.10.254", 800, "http://tile.openstreetmap.org/17/36168/54911.png", "17/36168/54911.png"))

//		URL = (char *) malloc(strlen(FileName)+strlen(OSMURLPrefix)+80);
//		sprintf(URL, "http://tile.openstreetmap.org/%s", FileName);
//		if (!httpGet("192.168.10.254", 800, URL, FileName))
//
//	http://tile.openstreetmap.org/12/2047/1362.png/status
//	Tile is clean. Last rendered at Mon Apr 26 10:33:17 2010
//
//	http://andy.sandbox.cloudmade.com/tiles/cycle/12/2047/1362.png/status
//	Tile is clean. Last rendered at Wed Apr 21 16:21:19 2010
//
//	http://tile.cloudmade.com/8bafab36916b5ce6b4395ede3cb9ddea/1/256/12/2047/1362.png/status
//	Tile (2047, 1362, 12, 256, u'1') should be located in metafile /mnt/var/www/direct/1/256/12/0/0/117/245/226.meta. Size of metafile is: 91313.
//
//	http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html
//	14.25 If-Modified-Since
#ifdef FOR_INFO_ONLY

The If-Modified-Since request-header field is used with a method to make it conditional: if the requested variant has not been modified since the time specified in this field, an entity will not be returned from the server; instead, a 304 (not modified) response will be returned without any message-body.

       If-Modified-Since = "If-Modified-Since" ":" HTTP-date

An example of the field is:

       If-Modified-Since: Sat, 29 Oct 1994 19:43:31 GMT

A GET method with an If-Modified-Since header and no Range header requests that the identified entity be transferred only if it has been modified since the date given by the If-Modified-Since header. The algorithm for determining this includes the following cases:

      a) If the request would normally result in anything other than a
         200 (OK) status, or if the passed If-Modified-Since date is
         invalid, the response is exactly the same as for a normal GET.
         A date which is later than the server's current time is
         invalid.

      b) If the variant has been modified since the If-Modified-Since
         date, the response is exactly the same as for a normal GET.

      c) If the variant has not been modified since a valid If-
         Modified-Since date, the server SHOULD return a 304 (Not
         Modified) response.

The purpose of this feature is to allow efficient updates of cached information with a minimum amount of transaction overhead.

      Note: The Range request-header field modifies the meaning of If-
      Modified-Since; see section 14.35 for full details.

      Note: If-Modified-Since times are interpreted by the server, whose
      clock might not be synchronized with the client.

      Note: When handling an If-Modified-Since header field, some
      servers will use an exact date comparison function, rather than a
      less-than function, for deciding whether to send a 304 (Not
      Modified) response. To get best results when sending an If-
      Modified-Since header field for cache validation, clients are
      advised to use the exact date string received in a previous Last-
      Modified header field whenever possible.

      Note: If a client uses an arbitrary date in the If-Modified-Since
      header instead of a date taken from the Last-Modified header for
      the same request, the client should be aware of the fact that this
      date is interpreted in the server's understanding of time. The
      client should consider unsynchronized clocks and rounding problems
      due to the different encodings of time between the client and
      server. This includes the possibility of race conditions if the
      document has changed between the time it was first requested and
      the If-Modified-Since date of a subsequent request, and the

      possibility of clock-skew-related problems if the If-Modified-
      Since date is derived from the client's clock without correction
      to the server's clock. Corrections for different time bases
      between client and server are at best approximate due to network
      latency.

#endif

static BOOL OSMFetchEnabled = TRUE;
static unsigned long OSMMinMBFree = 0;
static UINT OSMNotifyMsg = 0;
static unsigned long OSMFileCount = 0;
static unsigned __int64 OSMFileSpace = 0;
static unsigned __int64 OSMDiskSpace = 0;
static struct
{	TCHAR *Label;
	unsigned long Age;
	unsigned long Count;
	unsigned __int64 Space, DiskSpace;
} OSMFileAges[10] = {0};	/* hour, day, week, month, year, longer */
static long OSMFileAgeCount = 0;	/* Last one is always longer */

static struct
{	double name;
	double conn;
	double send;
	double recv;
	double get;	/* Total network time */
	double gets;
	double write;	/* File writing */
	double writes;
	double read;	/* File reading / Rasterization */
	double reads;
} FetchTimes = {0};

BOOL OSMGetFreeSpace(TILE_SERVER_INFO_S *sInfo, unsigned __int64 *pFree)
{	ULARGE_INTEGER FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes;
	TCHAR uPath[MAX_PATH];

	*pFree = 0;
	StringCbPrintf(uPath, sizeof(uPath), TEXT("%S"), sInfo->Path);
	if (!GetDiskFreeSpaceEx(uPath, &FreeBytesAvailable,
							&TotalNumberOfBytes, &TotalNumberOfFreeBytes))
		return FALSE;

	*pFree = FreeBytesAvailable.QuadPart;
	return TRUE;
}

#ifdef UNDER_CE
#include <storemgr.h>
#endif

unsigned __int64 OSMGetClusterSize(TILE_SERVER_INFO_S *sInfo)
{
#ifdef UNDER_CE
#ifdef FOR_INFO_ONLY
typedef struct _CE_VOLUME_INFO{
  DWORD cbSize;
  DWORD dwAttributes;
  DWORD dwFlags;
  DWORD dwBlockSize;
  TCHAR szStoreName[STORENAMESIZE];
  TCHAR szPartitionName[PARTITIONNAMESIZE];
} CE_VOLUME_INFO, *PCE_VOLUME_INFO, *LPCE_VOLUME_INFO;
WINBASEAPI BOOL CeGetVolumeInfo(
  LPCWSTR pszRootPath,
  CE_VOLUME_INFO_LEVEL InfoLevel,
  LPCE_VOLUME_INFO lpVolumeInfo
);
#endif
#ifndef CE50
	CE_VOLUME_INFO CEInfo;
	CEInfo.cbSize = sizeof(CEInfo);
	TCHAR uPath[MAX_PATH];

	StringCbPrintf(uPath, sizeof(uPath), TEXT("%S"), sInfo->Path);
	if (!CeGetVolumeInfo(uPath, CeVolumeInfoLevelStandard, &CEInfo))
	{	TraceLogThread("OSM",TRUE,"CeGetVolumeInfo FAILED!\n");
		return 0;
	}
	TraceLogThread("OSM",FALSE,"CeGetVolumeInfo(%s) Gave %ld\n", sInfo->Path, (long) CEInfo.dwBlockSize);
	return CEInfo.dwBlockSize;
#else
	unsigned __int64 Free = 0;
	char *Path = (char *) malloc(strlen(sInfo->Path)+80);
	sprintf(Path,"%s/temp.tmp", sInfo->Path);
	if (OSMGetFreeSpace(sInfo, &Free))
	{	FILE *Temp = fopen(Path,"w");
		if (Temp)
		{	unsigned __int64 NowFree = 0;
			fprintf(Temp,"This is a short file\n");
			fclose(Temp);
			if (OSMGetFreeSpace(sInfo, &NowFree))
			{	Free -= NowFree;
			}
			RemoveFile(Path);
		}
	}
	free(Path);
	return Free;
#endif
#else
	DWORD lSectorsPerCluster, lBytesPerSector, lNumberOfFreeClusters, lTotalNumberOfClusters;
	TCHAR uPath[MAX_PATH];

	StringCbPrintf(uPath, sizeof(uPath), TEXT("%S"), sInfo->Path);
	if (!GetDiskFreeSpace(uPath, &lSectorsPerCluster, &lBytesPerSector,
							&lNumberOfFreeClusters, &lTotalNumberOfClusters))
	{	TraceLogThread("OSM",TRUE,"GetDiskFreeSpace(%s) FAILED!\n", sInfo->Path);
		return 0;
	}
	TraceLogThread("OSM",TRUE,"GetDiskFreeSpace(%s) Gave %ld*%ld=%ld\n", sInfo->Path, (long) lSectorsPerCluster, (long) lBytesPerSector, (long) lSectorsPerCluster * (long) lBytesPerSector);
	return lSectorsPerCluster * lBytesPerSector;
#endif
}

static BOOL isOSMPath(TCHAR *Name)
{	TCHAR *p;

	for (p=Name; *p; p++)
		if (*p < *TEXT("0") || *p > *TEXT("9"))
			return FALSE;
	return TRUE;
}

static BOOL isOSMFile(TCHAR *Name)
{	TCHAR *p;

	for (p=Name; *p; p++)
	{	if (*p < *TEXT("0") || *p > *TEXT("9"))
		{	if (*p != *TEXT(".")) return FALSE;
			if (p[1] != *TEXT("p") && p[1] != *TEXT("P")) return FALSE;
			if (p[2] != *TEXT("n") && p[2] != *TEXT("N")) return FALSE;
			if (p[3] != *TEXT("g") && p[3] != *TEXT("G")) return FALSE;
			if (p[4] != *TEXT("")) return FALSE;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL isOSMDataOnly(char *path, TCHAR *ProbeFile, BOOL *SomeOSM)
{	size_t len = strlen(path);
	TCHAR *myPath = (TCHAR*)malloc(sizeof(TCHAR)*(len+3));
	WIN32_FIND_DATA *pData = (WIN32_FIND_DATA *)malloc(sizeof(*pData));
	HANDLE hFind;
	BOOL Result = FALSE;

	if (SomeOSM) *SomeOSM = FALSE;
	StringCbPrintf(myPath, sizeof(TCHAR)*(len+3), TEXT("%.*S%S*"),
					len, path, 
					(path[len-1] != '/' && path[len-1] != '\\')?"/":"");

	hFind = FindFirstFile(myPath,pData);
	if (hFind != INVALID_HANDLE_VALUE)
	{	Result = TRUE;	/* Until proven otherwise */
		do
		{
#ifdef FOR_INFORMATION_ONLY
typedef struct _WIN32_FIND_DATA { 
  DWORD dwFileAttributes; 
  FILETIME ftCreationTime; 
  FILETIME ftLastAccessTime; 
  FILETIME ftLastWriteTime; 
  DWORD nFileSizeHigh; 
  DWORD nFileSizeLow; 
  DWORD dwOID; 
  TCHAR cFileName[MAX_PATH]; 
} WIN32_FIND_DATA; 
#endif
			if (pData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{	if (wcscmp(pData->cFileName,TEXT("."))
				&& wcscmp(pData->cFileName,TEXT("..")))
				{	int newLen = len+1+wcslen(pData->cFileName)+1;
					char *newPath = (char*)malloc(newLen);

					StringCbPrintfA(newPath, newLen, "%.*s%s%S",
									len, path, 
									(path[len-1] != '/' && path[len-1] != '\\')?"/":"",
									pData->cFileName);

					if (!isOSMPath(pData->cFileName))
					{	Result = FALSE;
						TraceLogThread("OSM", FALSE, "Non-OSM Path %s\n", newPath);
					} else if (SomeOSM) *SomeOSM = TRUE;
					free(newPath);
				}
			} else
			{	int newLen = sizeof(TCHAR)*(len+1+wcslen(pData->cFileName)+1);
				TCHAR *newPath = (TCHAR*)malloc(newLen);
				StringCbPrintf(newPath, newLen, TEXT("%.*S%S%s"),
								len, path, 
								(path[len-1] != '/' && path[len-1] != '\\')?"/":"",
								pData->cFileName);
				if (!isOSMFile(pData->cFileName)
				&& wcscmp(pData->cFileName,ProbeFile))
				{	Result = FALSE;
					TraceLogThread("OSM", FALSE, "Non-OSM File %S\n", newPath);
				} else if (SomeOSM) *SomeOSM = TRUE;
				free(newPath);
			}
		} while (FindNextFile(hFind,pData));
		FindClose(hFind);
	}

	free(myPath);
	free(pData);

	return Result;
}

char *OSMCheckTileState(TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y)
{	BOOL Exists;
	size_t Remaining = 1024;
	char *Output = (char*)malloc(Remaining);
	char *Next = Output;

	char *File = OSMPrepTileFile(sInfo, zoom, x, y, &Exists HERE);

	if (!File || !Exists) TraceLogThread("OSM", TRUE, "z:%ld x:%ld y:%ld %s %s\n", zoom, x, y, File?File:"*NULL*", Exists?"Exists":"NOT FOUND");

	if (!File) return NULL;	/* Didn't even format! */
	if (!Exists) return NULL;	/* Don't even HAVE that one! */

	size_t tLen = sizeof(TCHAR)*(strlen(File)+1);
	TCHAR *tPath=(TCHAR *)malloc(tLen);
	WIN32_FILE_ATTRIBUTE_DATA Attrs;
	SYSTEMTIME st;

	StringCbPrintfExA(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
					"%s\n", File);

	StringCbPrintf(tPath,tLen,TEXT("%S"), File);
	if (sInfo->SingleDigitDirectories)
	{	StringCbPrintfExA(Next, Remaining, &Next, &Remaining,
					STRSAFE_IGNORE_NULLS, "Single Digit Directories!\n");
	}

	if (!GetFileAttributesEx(tPath, GetFileExInfoStandard, &Attrs))
	{	TraceLogThread("OSM", TRUE, "GetFileAttributesEx(%s) Failed with %ld (%S)\n", File, GetLastError(), tPath);
		free(tPath);
		free(File);
		return NULL;
	}
	free(tPath);

	if (Attrs.ftCreationTime.dwLowDateTime || Attrs.ftCreationTime.dwHighDateTime)
	{	FileTimeToSystemTime(&Attrs.ftCreationTime, &st);
		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
					"Create:%4ld-%02ld-%02ldT%02ld:%02ld:%02ld.%03ld\n",
					st.wYear, st.wMonth, st.wDay,
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
		TraceLogThread("OSM", TRUE, "%s Create:%4ld-%02ld-%02ldT%02ld:%02ld:%02ld.%03ld\n", File,
					st.wYear, st.wMonth, st.wDay,
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	}
	if (Attrs.ftLastWriteTime.dwLowDateTime || Attrs.ftLastWriteTime.dwHighDateTime)
	{	FileTimeToSystemTime(&Attrs.ftLastWriteTime, &st);
		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
					" Write:%4ld-%02ld-%02ldT%02ld:%02ld:%02ld.%03ld\n",
					st.wYear, st.wMonth, st.wDay,
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
		TraceLogThread("OSM", TRUE, "%s Write:%4ld-%02ld-%02ldT%02ld:%02ld:%02ld.%03ld\n", File,
					st.wYear, st.wMonth, st.wDay,
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	}
	if (Attrs.ftLastAccessTime.dwLowDateTime || Attrs.ftLastAccessTime.dwHighDateTime)
	{	FileTimeToSystemTime(&Attrs.ftLastAccessTime, &st);
		StringCbPrintfExA(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
					"Access:%4ld-%02ld-%02ldT%02ld:%02ld:%02ld.%03ld\n",
					st.wYear, st.wMonth, st.wDay,
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
		TraceLogThread("OSM", TRUE, "%s Access:%4ld-%02ld-%02ldT%02ld:%02ld:%02ld.%03ld\n", File,
					st.wYear, st.wMonth, st.wDay,
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	}
	free(File);
	return Output;
}

static unsigned long OSMCalcTileStats(TILE_SERVER_INFO_S *sInfo, int len, char *path, int depth, BOOL nopurge, unsigned __int64 ClusterBytes=0, unsigned __int64 *pFileSpace=NULL, unsigned __int64 *pDiskSpace=NULL, BOOL ForcePurge=FALSE, HWND hwnd=NULL)
{	TCHAR *myPath = (TCHAR*)malloc(sizeof(TCHAR)*(len+3));
	WIN32_FIND_DATA *pData = (WIN32_FIND_DATA *)malloc(sizeof(*pData));
	HANDLE hFind;
	FILETIME ftNow;
	unsigned __int64 Now;
	unsigned long FileCount = 0;
	HWND hwndProgress = NULL;
	unsigned long ProgressCount=0, ProgressTotal=0;

	if (pFileSpace) *pFileSpace = 0;
	if (pDiskSpace) *pDiskSpace = 0;

#ifdef UNDER_CE
	GetCurrentFT(&ftNow);
#else
	GetSystemTimeAsFileTime(&ftNow);
#endif
	Now = ((unsigned __int64) ftNow.dwHighDateTime)<<32 | ftNow.dwLowDateTime;

	StringCbPrintf(myPath, sizeof(TCHAR)*(len+3), TEXT("%.*S%S*"),
					len, path, 
					(path[len-1] != '/' && path[len-1] != '\\')?"/":"");

	if (depth <= 1) TraceLogThread("OSM", FALSE, "[%s]OSMPrintfTileStats[%ld](%S)\n", sInfo->Name, (long) depth, myPath);

	if (hwnd)
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
						(rc.bottom+rc.top)/2-15+depth*30, 			/*Y co-ordinates*/
						rc.right-rc.left, 			/*Width*/
						30, 			/*Height*/
						hwnd, 			/*Parent HWND*/
						(HMENU) NULL/*1*/, 	/*The Progress Bar's ID*/
						g_hInstance,		/*The HINSTANCE of your program*/ 
						NULL);			/*Parameters for main window*/
		if (!hwndProgress) TraceLogThread("OSM", TRUE, "CreateWindow(Progress) Failed with %ld\n", GetLastError());
		else
		{	SendMessage(hwndProgress, PBM_SETRANGE32, 0, 1);
			hFind = FindFirstFile(myPath,pData);
			if (hFind != INVALID_HANDLE_VALUE)
			{	do
				{	ProgressTotal++;
				} while (FindNextFile(hFind,pData));
				FindClose(hFind);
			}
			GetWindowRect(hwndProgress,&rc);
			TraceLogThread("OSM", TRUE, "Progress(%s)[%ld] window created %p @ %ld %ld %ld %ld\n", myPath, ProgressTotal, hwndProgress, (long) rc.left, (long) rc.top, (long) rc.right, (long) rc.bottom);
			SendMessage(hwndProgress, PBM_SETRANGE32, 0, ProgressTotal*2);
			SendMessage(hwndProgress, PBM_SETPOS, ProgressCount*2+1, 0);
		}
	}

	hFind = FindFirstFile(myPath,pData);
	if (hFind != INVALID_HANDLE_VALUE)
	{	do
		{	unsigned __int64 Accessed;
			FILETIME ftCreate, ftAccess, ftWrite;
			SYSTEMTIME stCreate={0}, stAccess={0}, stWrite={0};

			if (pData->ftCreationTime.dwLowDateTime || pData->ftCreationTime.dwHighDateTime)
			{	FileTimeToLocalFileTime(&pData->ftCreationTime, &ftCreate);
				FileTimeToSystemTime(&ftCreate, &stCreate);
			}
			if (pData->ftLastAccessTime.dwLowDateTime || pData->ftLastAccessTime.dwHighDateTime)
			{	FileTimeToLocalFileTime(&pData->ftLastAccessTime, &ftAccess);
				FileTimeToSystemTime(&ftAccess, &stAccess);
			}
			if (pData->ftLastWriteTime.dwLowDateTime || pData->ftLastWriteTime.dwHighDateTime)
			{	FileTimeToLocalFileTime(&pData->ftLastWriteTime, &ftWrite);
				FileTimeToSystemTime(&ftWrite, &stWrite);
			}

			unsigned __int64 Space = ((unsigned __int64)pData->nFileSizeHigh) * (((unsigned __int64)MAXDWORD)+1) + ((unsigned __int64)pData->nFileSizeLow);
			unsigned __int64 DiskSpace = ClusterBytes?(ClusterBytes*((Space+ClusterBytes-1)/ClusterBytes)):0;

			if (pFileSpace) *pFileSpace += Space;
			if (pDiskSpace) *pDiskSpace += DiskSpace;

			Accessed = ((unsigned __int64) pData->ftLastAccessTime.dwHighDateTime)<<32 | pData->ftLastAccessTime.dwLowDateTime;
			if (!Accessed) Accessed = ((unsigned __int64) pData->ftLastWriteTime.dwHighDateTime)<<32 | pData->ftLastWriteTime.dwLowDateTime;
			if (!Accessed) Accessed = ((unsigned __int64) pData->ftCreationTime.dwHighDateTime)<<32 | pData->ftCreationTime.dwLowDateTime;

			Accessed = Now - Accessed;	/* Delta time before now */
			Accessed /= 10;	/* 1000 nanosec = microsec */
			Accessed /= 1000;	/* 1000 microsec = millisec */
			Accessed /= 1000;	/* 1000 millisec = seconds */

#ifdef VERBOSE
			TraceLogThread("OSM", FALSE, "%S %ld (%.2lf days ago)Cre: %4ld-%02ld-%02ld %02ld:%02ld:%02ld Acc: %4ld-%02ld-%02ld %02ld:%02ld:%02ld Wri: %4ld-%02ld-%02ld %02ld:%02ld:%02ld\n",
						pData->cFileName, (long) pData->nFileSizeLow,
						(double) ((double)Accessed / (60.0*60.0*24.0)),
						(long) stCreate.wYear, (long) stCreate.wMonth, (long) stCreate.wDay,
						(long) stCreate.wHour, (long) stCreate.wMinute, (long) stCreate.wSecond,
						(long) stAccess.wYear, (long) stAccess.wMonth, (long) stAccess.wDay,
						(long) stAccess.wHour, (long) stAccess.wMinute, (long) stAccess.wSecond,
						(long) stWrite.wYear, (long) stWrite.wMonth, (long) stWrite.wDay,
						(long) stWrite.wHour, (long) stWrite.wMinute, (long) stWrite.wSecond);
#endif
#ifdef FOR_INFORMATION_ONLY
typedef struct _WIN32_FIND_DATA { 
  DWORD dwFileAttributes; 
  FILETIME ftCreationTime; 
  FILETIME ftLastAccessTime; 
  FILETIME ftLastWriteTime; 
  DWORD nFileSizeHigh; 
  DWORD nFileSizeLow; 
  DWORD dwOID; 
  TCHAR cFileName[MAX_PATH]; 
} WIN32_FIND_DATA; 
#endif
			if (pData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{	if (wcscmp(pData->cFileName,TEXT("."))
				&& wcscmp(pData->cFileName,TEXT("..")))
				{	int newLen = len+1+wcslen(pData->cFileName)+1;
					char *newPath = (char*)malloc(newLen);
					unsigned long newCount;
					unsigned __int64 newSpace, newDSpace;
					BOOL newpurge = nopurge;	/* Keep the caller's value */

					if (!depth)
					{	TCHAR Zoom[16];
						if (sInfo->SingleDigitDirectories)
							StringCbPrintf(Zoom, sizeof(Zoom), TEXT("%ld"), (long) sInfo->RetainZoom);
						else StringCbPrintf(Zoom, sizeof(Zoom), TEXT("%02ld"), (long) sInfo->RetainZoom);
						newpurge = wcscmp(pData->cFileName,Zoom)<=0;	/* Override caller */
					}

					StringCbPrintfA(newPath, newLen, "%.*s%s%S",
									len, path, 
									(path[len-1] != '/' && path[len-1] != '\\')?"/":"",
									pData->cFileName);

					if (isOSMPath(pData->cFileName))
					{	newCount = OSMCalcTileStats(sInfo, strlen(newPath),newPath,depth+1,newpurge,ClusterBytes,&newSpace,&newDSpace,ForcePurge,hwnd);

						if (!depth && (newCount || newSpace))
						{	char *What = !ForcePurge&&newpurge?"Retained":"Purged to";
							if (newSpace >= 1024*1024 || newDSpace > 1024*1024)
								TraceLogThread("OSM", FALSE, "[%s]OSMCalcTileStats:Level[%S] %s %ld in %.2lf/%.2lfMB\n",
											sInfo->Name,
											pData->cFileName, What, (long) newCount,
											((double)(__int64)newSpace)/1024.0/1024.0, ((double)(__int64)newDSpace)/1024.0/1024.0);
							else TraceLogThread("OSM", FALSE, "[%s] OSMCalcTileStats:Level[%S] %s %ld in %.2lf/%.2lfKB\n",
											sInfo->Name,
											pData->cFileName, What, (long) newCount,
											((double)(__int64)newSpace)/1024.0, ((double)(__int64)newDSpace)/1024.0);
						}

						if (!newCount)
						{	if (!DeleteDirectory(newPath))
							{	TraceLogThread("OSM", TRUE, "Failed To Delete EMPTY DIRECTORY %s Error %ld\n", newPath, (long) GetLastError());
							} else
							{	TraceLogThread("OSM", FALSE, "Deleted EMPTY DIRECTORY %s\n", newPath);
								if (pFileSpace) *pFileSpace -= Space;
								if (pDiskSpace) *pDiskSpace -= DiskSpace;
							}
						}
						FileCount += newCount;
						if (pFileSpace) *pFileSpace += newSpace;
						if (pDiskSpace) *pDiskSpace += newDSpace;
					} else TraceLogThread("OSM", TRUE, "Not Purging Non-OSM Path %s\n", newPath);
					free(newPath);
				}
			} else
			{	FileCount++;
				if (ForcePurge
				|| (OSMFetchEnabled	/* If we're allowed to fetch */
				&& OSMPurgeEnabled	/* Globally enabled */
				&& sInfo->PurgeEnabled	/* And we're allowed to purge this set */
				&& sInfo->RetainDays	/* And we have a retention limit */
				&& !nopurge			/* And we're allowed to purge */
				&& (Accessed / (60*60*24) > sInfo->RetainDays)))	/* days since access, delete it */
				{	int newLen = sizeof(TCHAR)*(len+1+wcslen(pData->cFileName)+1);
					TCHAR *newPath = (TCHAR*)malloc(newLen);
					StringCbPrintf(newPath, newLen, TEXT("%.*S%S%s"),
									len, path, 
									(path[len-1] != '/' && path[len-1] != '\\')?"/":"",
									pData->cFileName);
					if (isOSMFile(pData->cFileName))
					{	if (DeleteFile(newPath))
						{	FileCount--;
							if (pFileSpace) *pFileSpace -= Space;
							if (pDiskSpace) *pDiskSpace -= DiskSpace;
						} else
						{	TraceLogThread("OSM", TRUE, "Failed To Delete %S Error %ld\n", newPath, (long) GetLastError());
						}
					} else TraceLogThread("OSM", TRUE, "Not Purging Non-OSM File %S\n", newPath);
					free(newPath);
				} else
				{	int i;
					for (i=0; i<OSMFileAgeCount-1; i++)
					{	if (Accessed < OSMFileAges[i].Age)
							break;
					}
					OSMFileAges[i].Count++;
					OSMFileAges[i].Space += Space;
					OSMFileAges[i].DiskSpace += DiskSpace;
				}
			}

			if (hwndProgress)
			{	ProgressCount++;
#ifndef UNDER_CE
				COLORREF bar = GetScaledRGColor(ProgressCount, 0, ProgressTotal);
				SendMessage(hwndProgress, PBM_SETBARCOLOR, 0, bar);
#endif
				SendMessage(hwndProgress, PBM_SETPOS, ProgressCount*2, 0);
			}

		} while (FindNextFile(hFind,pData));
		FindClose(hFind);
	}
	if (depth <= 1) TraceLogThread("OSM", FALSE, "[%s]OSMPrintfTileStats[%ld](%S) Totals %lu\n", sInfo->Name, depth, myPath, (unsigned long) FileCount);
	free(myPath);
	free(pData);

	if (hwndProgress) DestroyWindow(hwndProgress);

	return FileCount;
}

unsigned long OSMGetTileServerCacheStats(TILE_SERVER_INFO_S *sInfo, unsigned __int64 *pFileSpace/*=NULL*/, unsigned __int64 *pDiskSpace/*=NULL*/)
{	if (pFileSpace) *pFileSpace = 0;
	if (pDiskSpace) *pDiskSpace = 0;
	return OSMCalcTileStats(sInfo, strlen(sInfo->Path), sInfo->Path, 0, TRUE, OSMGetClusterSize(sInfo), pFileSpace, pDiskSpace);
}

unsigned long OSMFlushTileServerCache(TILE_SERVER_INFO_S *sInfo, unsigned __int64 *pFileSpace/*=NULL*/, unsigned __int64 *pDiskSpace/*=NULL*/, HWND hwnd/*=NULL*/)
{	if (pFileSpace) *pFileSpace = 0;
	if (pDiskSpace) *pDiskSpace = 0;
	unsigned long FilesLeft = OSMCalcTileStats(sInfo, strlen(sInfo->Path), sInfo->Path, 0, FALSE, OSMGetClusterSize(sInfo), pFileSpace, pDiskSpace, TRUE, hwnd);
	TraceLogThread("OSM", TRUE, "[%s] Flushed (%s) Leaving %lu Files\n",
					sInfo->Name, sInfo->Path, FilesLeft);
	return FilesLeft;
}

#ifdef UNDER_CE
static DWORD OSMFileMonitor(LPVOID pvParam)
#else
static DWORD WINAPI OSMFileMonitor(LPVOID pvParam)
#endif
{	SetTraceThreadName("OSMMonitor");
	Sleep(1*60*1000L);	/* Wait for the process to stabilize */
	for (;;)	/* And forever while we're running */
	{	unsigned __int64 TotalSpace = 0, TotalDSpace = 0;
		int i;
		FILETIME ftNow;
		unsigned __int64 Now;

#ifdef UNDER_CE
	GetCurrentFT(&ftNow);
#else
	GetSystemTimeAsFileTime(&ftNow);
#endif
		Now = ((unsigned __int64) ftNow.dwHighDateTime)<<32 | ftNow.dwLowDateTime;
		memset(&OSMFileAges, 0, sizeof(OSMFileAges));
		i = 0;
		OSMFileAges[i].Label = TEXT("Hour");
		OSMFileAges[i++].Age = (unsigned long) (60*60);	/* Recent hour */
		OSMFileAges[i].Label = TEXT("Day");
		OSMFileAges[i++].Age = (unsigned long) (24*60*60L);	/* Recent day */
		OSMFileAges[i].Label = TEXT("Week");
		OSMFileAges[i++].Age = (unsigned long) (7*24*60*60L);	/* Recent week */
		OSMFileAges[i].Label = TEXT("Month");
		OSMFileAges[i++].Age = (unsigned long) (30*24*60*60L);	/* Recent "month" */
		OSMFileAges[i].Label = TEXT("Year");
		OSMFileAges[i++].Age = (unsigned long) (365*24*60*60L);	/* Recent "year" */
		OSMFileAges[i].Label = TEXT("Longer");
		OSMFileAges[i++].Age = 0;						/* Remainder */
		OSMFileAgeCount = i;
		unsigned long TotalCount = 0;
	
		for (int s=0; s<ServerCount; s++)
		{	unsigned __int64 NewSpace = 0, NewDSpace = 0;
			unsigned long NewCount;

			NewCount = OSMCalcTileStats(TileServers[s], strlen(TileServers[s]->Path),TileServers[s]->Path,0,TRUE,OSMGetClusterSize(TileServers[s]),&NewSpace,&NewDSpace);

			TraceLogThread("OSM", TRUE, "[%s]@%p %lu Files in %I64u/%I64u Bytes on %s\n",
						TileServers[s]->Name, TileServers[s],
						(unsigned long) NewCount,
						(unsigned __int64) NewSpace,
						(unsigned __int64) NewDSpace,
						TileServers[s]->Path);

			TileServers[s]->Private.CountOnDisk = NewCount;
			TileServers[s]->Private.SpaceUsed = NewSpace;
			TileServers[s]->Private.DSpaceUsed = NewDSpace;

			TotalCount += NewCount;
			TotalSpace += NewSpace;
			TotalDSpace += NewDSpace;
		}

		for (i=0; i<OSMFileAgeCount; i++)
		{	if (OSMFileAges[i].Count)
			{	TraceLogThread("OSM", TRUE, "%6S: %5lu %6.2lf/%6.2lfMB\n",
							OSMFileAges[i].Label,
							OSMFileAges[i].Count,
							((double)(__int64)OSMFileAges[i].Space)/1024.0/1024.0,
							((double)(__int64)OSMFileAges[i].DiskSpace)/1024.0/1024.0);
			}
		}
		TraceLogThread("OSM", TRUE, "%6S: %5lu %6.2lf/%6.2lfMB\n",
					TEXT("Total"),
					TotalCount,
					((double)(__int64)TotalSpace)/1024.0/1024.0,
					((double)(__int64)TotalDSpace)/1024.0/1024.0);

		OSMFileCount = TotalCount;
		OSMFileSpace = TotalSpace;
		OSMDiskSpace = TotalDSpace;
		Sleep(4*60*60*1000L);	/* Scan once every 4 hours */
	}
}

void OSMGetTileServerTotals(unsigned long *pTotalTiles, unsigned __int64 *pTotalSpace, unsigned __int64 *pTotalDSpace, long *pTilesAttempted, long *pTilesFetched, long *bSent, long *bRecv, double *msGetTime)
{	if (pTilesAttempted) *pTilesAttempted = TilesAttempted;
	if (pTilesFetched) *pTilesFetched = TilesFetched;
	if (bSent) *bSent = TileBytesSent;
	if (bRecv) *bRecv = TileBytesRecv;
	if (pTotalTiles) *pTotalTiles = OSMFileCount;
	if (pTotalSpace) *pTotalSpace = OSMFileSpace;
	if (pTotalDSpace) *pTotalDSpace = OSMDiskSpace;
	if (msGetTime) *msGetTime = FetchTimes.get;
}

void OSMGetTileServerStats(TILE_SERVER_INFO_S *sInfo, unsigned long *pTotalTiles, unsigned __int64 *pTotalSpace, unsigned __int64 *pTotalDSpace)
{	sInfo = OSMGetActualTileServer(sInfo);
	if (sInfo)
	{
		TraceLogThread("OSM", TRUE, "[%s]@%p %lu Files in %I64u/%I64u Bytes on %s\n",
					sInfo->Name, sInfo,
					(unsigned long) sInfo->Private.CountOnDisk,
					(unsigned __int64) sInfo->Private.SpaceUsed,
					(unsigned __int64) sInfo->Private.DSpaceUsed,
					sInfo->Path);

		if (pTotalTiles) *pTotalTiles = sInfo->Private.CountOnDisk;
		if (pTotalSpace) *pTotalSpace = sInfo->Private.SpaceUsed;
		if (pTotalDSpace) *pTotalDSpace = sInfo->Private.DSpaceUsed;
	} else
	{	if (pTotalTiles) *pTotalTiles = 0;
		if (pTotalSpace) *pTotalSpace = 0;
		if (pTotalDSpace) *pTotalDSpace = 0;
	}
}

TCHAR *OSMGetTileAgeStats(void)
{	size_t Remaining = sizeof(TCHAR)*1024;
	TCHAR *Buffer=(TCHAR*)malloc(Remaining);
	TCHAR *Next = Buffer;
	unsigned long TotalCount=0;
	unsigned __int64 TotalSpace=0;
	unsigned __int64 TotalDSpace=0;
	int i;

	for (i=0; i<OSMFileAgeCount; i++)
	{	if (OSMFileAges[i].Count)
		{	TotalCount += OSMFileAges[i].Count;
			TotalSpace += OSMFileAges[i].Space;
			TotalDSpace += OSMFileAges[i].DiskSpace;
			StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
						TEXT("%s%s:\t%5lu\t%6.2lfMB"),
						Next!=Buffer?TEXT("\n"):TEXT(""),
						OSMFileAges[i].Label, 
						OSMFileAges[i].Count,
						((double)(__int64)OSMFileAges[i].Space)/1024.0/1024.0);
		}
	}
	if (TotalCount)
		StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
					TEXT("%s%s:\t%5lu\t%6.2lf/%6.2lfMB"),
					Next!=Buffer?TEXT("\n"):TEXT(""),
					TEXT("Total"), 
					TotalCount,
					((double)(__int64)TotalSpace)/1024.0/1024.0,
					((double)(__int64)TotalDSpace)/1024.0/1024.0);

	if (Next!=Buffer)
			StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
						TEXT("\n"));

	if (FetchTimes.reads || FetchTimes.writes || FetchTimes.gets)
	{	if (FetchTimes.reads)
		{	StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
							TEXT("%sRds: %ld/%ld=%ldms"),
							Next!=Buffer?TEXT("\n"):TEXT(""),
							(long) FetchTimes.read/1000, (long) FetchTimes.reads,
							(long) (FetchTimes.read/FetchTimes.reads));
		}
		if (FetchTimes.writes)
		{	StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
							TEXT("%sWrt: %ld/%ld=%ldms"),
							Next!=Buffer?TEXT("\n"):TEXT(""),
							(long) FetchTimes.write/1000, (long) FetchTimes.writes,
							(long) (FetchTimes.write/FetchTimes.writes));
		}
		if (FetchTimes.gets)
		{	StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
							TEXT("%sGet: %ld %ld %ld %ld=%ld/%ld=%ldms"),
							Next!=Buffer?TEXT("\n"):TEXT(""),
							(long) FetchTimes.name/1000, (long) FetchTimes.conn/1000,
							(long) FetchTimes.send/1000, (long) FetchTimes.recv/1000,
							(long) FetchTimes.get/1000, (long) FetchTimes.gets,
							(long) (FetchTimes.get/FetchTimes.gets));
		}
	}

	{	StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
						TEXT("%sCache: U:%ld/%ld S:%ld/%ld/%ld"),
						Next!=Buffer?TEXT("\n"):TEXT(""),
						(long) CacheInUse, (long) CacheMaxInUse,
						(long) CacheCount, (long) CacheSize,
						(long) (CacheMaxInUse*CACHE_INUSE_FACTOR));
	}

	{	int bigTotal = 0;
		StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
						TEXT("\nBigs[%ld]"), bigCount);
		for (unsigned int b=0; b<bigCount; b++)
		{	StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
						TEXT("%S%ld%S%S"),
						b?" ":"(",
						cBigs[b], b==iBig?"*":"",
						!cBigs[b]&&hbmBigs[b]?"?":"");
			bigTotal += cBigs[b];
TraceLogThread("OSM",cBigs[b]<0||cBigs[b]>BIG_X*BIG_Y,"b/cBigs[%ld]=0x%lX %ld\n", b, bBigs[b], cBigs[b]);
		}
		StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS, TEXT(")=%ld"), bigTotal);
	}

	if (Next == Buffer)
	{	free(Buffer); Buffer = NULL;
	}

	return Buffer;
}

void OSMSetFetchEnable(BOOL FetchEnabled)
{	OSMFetchEnabled = FetchEnabled;
	if (!OSMFetchEnabled) OSMFlushTileQueue(TRUE);
}

void OSMSetPurgeEnable(BOOL PurgeEnabled)
{	OSMPurgeEnabled = PurgeEnabled;
}

void OSMSetTileServerInfo(HWND hWnd, UINT msgNotify, TILE_SERVER_INFO_S *tInfo, unsigned long MinMBFree)
{	OSM = *tInfo;
//	strncpy(OSMServer, Server, sizeof(OSMServer));
//	OSMPort = Port;
//	strncpy(OSMURLPrefix, URLPrefix, sizeof(OSMURLPrefix));
//	strncpy(OSMPath, Path, sizeof(OSMPath));
//	OSMRetainDays = RetainDays;
//	OSMRetainZoom = RetainZoom;
//	OSMRevisionHours = RevisionHours;
	OSMNotifyMsg = msgNotify;
	OSMMinMBFree = MinMBFree;

	OSM.SingleDigitDirectories = tInfo->SingleDigitDirectories = OSMUseSingleDigitDirectories(OSM.Path);
	TraceLogThread("OSM", TRUE, "OSMSetTileServerInfo[%s]@%p:%s SingleDigit in %s\n", 
						tInfo->Name, tInfo,
						tInfo->SingleDigitDirectories?"Using":"NOT Using",
						tInfo->Path);
}

static BOOL OSMTileInRange(TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y)
{	if (zoom < MIN_OSM_ZOOM || zoom > MAX_OSM_ZOOM) return FALSE;	/* Compile-time limits */
	if (zoom < sInfo->MinServerZoom || zoom > sInfo->MaxServerZoom) return FALSE;	/* This should look at which OSM server is used */
	if (x < 0 || x >= 1<<zoom) return FALSE;
	if (y < 0 || y >= 1<<zoom) return FALSE;
	return TRUE;
}

static double pow2(int z)
{static	double pows[32] = {0};
	if (z >= sizeof(pows)/sizeof(pows[0])) return pow(2.0,z);
	if (!pows[z]) pows[z] = pow(2.0,z);
	return pows[z];
}

double long2tilexd(double lon, int z)
{	return (lon + 180.0) / 360.0 * pow2(z)/*pow(2.0, z)*/;
}

double lat2tileyd(double lat, int z)
{register double t = lat * M_PI/180.0;
	return (1.0 - log( tan(t) + 1.0 / cos(t)) / M_PI) / 2.0 * pow2(z)/*pow(2.0, z)*/; 
}

int long2tilex(double lon, int z)
{	return (int)(floor(long2tilexd(lon,z)));
}

int lat2tiley(double lat, int z)
{	return (int)(floor(lat2tileyd(lat,z)));
}

//#define INEFFICIENT
//#ifndef INEFFICIENT
#define TILE(t) ((int)(floor(t)))
#define PIXEL(t) ((int)(256.0*(t-floor(t))))
//#endif

static int long2tilexPixel(double lon, int z)
{
#ifdef INEFFICIENT
	double x = (lon + 180.0) / 360.0 * pow(2.0, z);
	return (int)(256.0*(x-floor(x))); 
#else
	return PIXEL(long2tilexd(lon,z));
#endif
}

static int lat2tileyPixel(double lat, int z)
{
#ifdef INEFFICIENT
	double y = (1.0 - log( tan(lat * M_PI/180.0) + 1.0 / cos(lat * M_PI/180.0)) / M_PI) / 2.0 * pow(2.0, z);
	return (int)(256.0*(y-floor(y)));
#else
	return PIXEL(lat2tileyd(lat,z));
#endif
}

double tiledx2long(double x, int z) 
{	return x / pow(2.0, z) * 360.0 - 180;
}

double tiledy2lat(double y, int z) 
{	double n = M_PI - 2.0 * M_PI * y / pow(2.0, z);
	return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
}

static double tilex2long(int x, int z) 
{	return tiledx2long(x,z);
	//	return x / pow(2.0, z) * 360.0 - 180;
}

static double tiley2lat(int y, int z) 
{	return tiledy2lat(y,z);
//	double n = M_PI - 2.0 * M_PI * y / pow(2.0, z);
//	return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
}

double GetLongPixelDelta(double lon, int z)
{	int xt = long2tilex(lon, z);
	double slon = tilex2long(xt, z);
	double elon = tilex2long(xt+1, z);
	return (elon-slon)/256.0;
}

double GetLatPixelDelta(double lat, int z)
{	int yt = lat2tiley(lat, z);
	double slat = tiley2lat(yt, z);
	double elat = tiley2lat(yt+1, z);
	return (elat-slat)/256.0;
}

void LatLonToTileCoord(double lat, double lon, OSM_TILE_COORD_S *tCoord)
{	if (lon <= -180) lon = -179.99999999999;
	if (lon >= 180) lon = 179.99999999999;
	if (lat <= -90) lat = -89.99999;
	if (lat >= 90) lat = 89.99999;
	double xd = long2tilexd(lon, 24);
	double yd = lat2tileyd(lat, 24);
	tCoord->x = TILE(xd)<<8 | PIXEL(xd);
	tCoord->y = TILE(yd)<<8 | PIXEL(yd);
}

static double DegToRad(double deg) { return deg / DegreesPerRadian; }
static double Rad2Deg(double rad) { return rad * DegreesPerRadian; }

/*	From: http://www.movable-type.co.uk/scripts/latlong.html */

static void HaversineLatLon(double lat1, double lon1, double lat2, double lon2, double *Dist, double *Bearing)
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

#ifdef OBSOLETE
double OSMGetMsec(void)
{
#define C 4.294967296e9
#define Li2Double(x) ((double)((x).HighPart)*C+(double)((x).LowPart))
	double dNow;
	LARGE_INTEGER Now = {0};
static	int First = 1;
static	LARGE_INTEGER Freq;
static	double dFreq=0, dLast = 0;
	if (First)
	{	if (!QueryPerformanceFrequency(&Freq))
		{	MessageBox(NULL, TEXT("No High-Resolution Performance Counter"), TEXT("QueryPerformanceFrequency"), MB_OK | MB_ICONWARNING);
			dFreq = 1.0;
		} else
		{	dFreq = Li2Double(Freq) / 1000.0;
		}
		First = 0;
	}
	if (!QueryPerformanceCounter(&Now))
	{	return 0.0;
	}
	dNow = Li2Double(Now);
	if (dNow < dLast)
	{	dNow = ++dLast;
	} else dLast = dNow;
	return dNow / dFreq;
}

double OSMMSecSince(double msStart, double msNow)
{	double Result = msNow - msStart;
	if (Result < 0) Result = 0;
	return Result;
}
#endif

double OSMCalculateScale(OSM_TILE_SET_S *ts, RECT *rc)	/* returns miles for 1/2 rectangle */
{	int xt = long2tilex(ts->lon, ts->zoom);
	int yt = lat2tiley(ts->lat, ts->zoom);
	double slat = tiley2lat(yt, ts->zoom);
	double elat = tiley2lat(yt+1, ts->zoom);
	double slon = tilex2long(xt, ts->zoom);
	double elon = tilex2long(xt+1, ts->zoom);
	double hDist, hBearing, vDist, vBearing;
	int width = rc->right - rc->left, height = rc->bottom - rc->top;
	double hScale, vScale, Scale;

	HaversineLatLon(slat, ts->lon, elat, ts->lon, &vDist, &vBearing);
	HaversineLatLon(ts->lat, slon, ts->lat, elon, &hDist, &hBearing);

	hScale = hDist * width / 256.0;
	vScale = vDist * height / 256.0;
	Scale = (hScale + vScale) / 2.0 / 2.0;

#ifdef VERBOSE
#undef VERBOSE
	TraceLogThread("OSM", TRUE, "OSMCalculateScale:%ld Tiles %ld %ld = %.8lf @ %ld x %.8lf @ %ld over %ld x %ld gives %.8lf+%.8lf = %.8lf scale (%.8lf %.8lf)\n",
			(long) ts->Count,
			(long) xt, (long) yt,
			(double) hDist, (long) hBearing,
			(double) vDist, (long) vBearing,
			(long) width, (long) height,
			(double) hScale, (double) vScale, (double) Scale,
			(double) ts->lat, (double) ts->lon);
#endif

#ifdef VERBOSE
	TraceLogThread("OSM", FALSE, "Zoom 0 @ %.4lf %.4lf\n", (double) tiley2lat(0,0), (double) tilex2long(0,0));
	TraceLogThread("OSM", FALSE, "Zoom 1-0 @ %.4lf %.4lf\n", (double) tiley2lat(0,1), (double) tilex2long(0,1));
	TraceLogThread("OSM", FALSE, "Zoom 1-1 @ %.4lf %.4lf\n", (double) tiley2lat(0,1), (double) tilex2long(1,1));
	TraceLogThread("OSM", FALSE, "Zoom 1-2 @ %.4lf %.4lf\n", (double) tiley2lat(1,1), (double) tilex2long(0,1));
	TraceLogThread("OSM", FALSE, "Zoom 1-3 @ %.4lf %.4lf\n", (double) tiley2lat(1,1), (double) tilex2long(1,1));
#endif

	return Scale;
}

BOOL OSMPointToLatLon(OSM_TILE_SET_S *ts, POINT *pt, double *pLat, double *pLon)
{	int t;

	for (t=0; t<ts->Count; t++)
	if (ts->Tiles[t].rc.left <= pt->x && ts->Tiles[t].rc.right > pt->x
	&& ts->Tiles[t].rc.top <= pt->y && ts->Tiles[t].rc.bottom > pt->y)
	{	double x = (double) ts->Tiles[t].x + ((pt->x-ts->Tiles[t].rc.left)/256.0);
		double y = (double) ts->Tiles[t].y + ((pt->y-ts->Tiles[t].rc.top)/256.0);
		*pLon = tiledx2long(x, ts->zoom);
		*pLat = tiledy2lat(y, ts->zoom);

#ifdef DEBUGGING_POINTS
		POINT pt2 = {0};
		OSMGetXYPos(ts, *pLat, *pLon, &pt2, TRUE);
		TCHAR *LatLon = APRSLatLon(*pLat, *pLon, ' ', ' ', 3);
		TraceLogThread("Point2LatLon", TRUE, "[%ld](%ld %ld) %ld %ld -> %ld %ld has %ld %ld -> %S -> %ld %ld %s\n",
						(long) t,
						(long) ts->Tiles[t].x, (long) ts->Tiles[t].y, 
						(long) ts->Tiles[t].rc.left,
						(long) ts->Tiles[t].rc.top,
						(long) ts->Tiles[t].rc.right,
						(long) ts->Tiles[t].rc.bottom,
						(long) pt->x, (long) pt->y,
						LatLon,
						(long) pt2.x, (long) pt2.y,
						pt2.x==pt->x&&pt2.y==pt->y?"OK":"BAD!");
		free(LatLon);
#endif

		return TRUE;
	}
	return FALSE;
}

BOOL OSMTileCoordToPoint(OSM_TILE_SET_S *ts, OSM_TILE_COORD_S *tCoord, POINT *pt, BOOL Visibility/*=TRUE*/)
{	int x = tCoord->x >> (24-ts->zoom);
	int y = tCoord->y >> (24-ts->zoom);
	int xt = x >> 8;
	int yt = y >> 8;
	int xo = x & 0xff;
	int yo = y & 0xff;

	if (Visibility)	/* Longer to do it this way */
	{
		int t;
		double MinDistance = DBL_MAX;
		POINT ptC, ptM;	/* Center point, Min point */
		ptC.x = ts->rcWin.left + (ts->rcWin.right-ts->rcWin.left)/2;
		ptC.y = ts->rcWin.top + (ts->rcWin.bottom-ts->rcWin.top)/2;

		for (t=0; t<ts->Count; t++)
		{	if (ts->Tiles[t].Tile
			&& ts->Tiles[t].x == xt
			&& ts->Tiles[t].y == yt)
			{
#ifdef LINEAR_SCALE
				pt->x = ts->Tiles[t].rc.left + (int)((lon-ts->Tiles[t].Tile->min.lon)*256.0/(ts->Tiles[t].Tile->max.lon-ts->Tiles[t].Tile->min.lon));
				pt->y = ts->Tiles[t].rc.top + (int)((lat-ts->Tiles[t].Tile->min.lat)*256.0/(ts->Tiles[t].Tile->max.lat-ts->Tiles[t].Tile->min.lat));
#else
				POINT ptT;
				ptT.x = ts->Tiles[t].rc.left + xo;
				ptT.y = ts->Tiles[t].rc.top + yo;
				int xd = ptT.x-ptC.x;
				int yd = ptT.y-ptC.y;
				double Distance = ((double)xd*(double)xd+(double)yd*(double)yd);
				if (Distance < MinDistance)
				{	MinDistance = Distance;
					ptM = ptT;
				}
#endif
#ifdef VERBOSE
TraceLogThread("OSM", FALSE, "OSMGetXYPos:%.4lf (%.4lf %.4lf) %.4lf (%.4lf %.4lf) Tile[%ld] z:%ld %ld %ld At %ld %ld Left/Top: %ld %ld\n",
			(double) lat,
			(double) ts->Tiles[t].Tile->min.lat, (double) ts->Tiles[t].Tile->max.lat, 
			(double) lon,
			(double) ts->Tiles[t].Tile->min.lon, (double) ts->Tiles[t].Tile->max.lon, 
			(long) t, (long) ts->zoom, (long) xt, (long) yt,
			(long) pt->x, (long) pt->y,
			(long) ts->Tiles[t].rc.left, (long) ts->Tiles[t].rc.top);
#endif
//return TRUE;
			}
		}

		if (MinDistance != DBL_MAX)	/* Found it? */
		{	*pt = ptM;
			return TRUE;
		}
	}

//TraceLogThread("OSM", FALSE, "OSMGetXYPos(Default):%.4lf %.4lf (Tile %ld %ld) Not In Tile Set\n",
//	   (double) lat, (double) lon, (long) xt, (long) yt);

	{	int width = ts->rcWin.right-ts->rcWin.left, height = ts->rcWin.bottom-ts->rcWin.top;
		int cx = width/2, cy = height/2;
		int left = ts->rcWin.left+(xt-ts->xt)*256+cx-128+ts->xo;
		int top = ts->rcWin.top+(yt-ts->yt)*256+cy-128+ts->yo;

		pt->x = left + xo;
		pt->y = top + yo;
	}

	return FALSE;
}



BOOL OSMGetXYPos(OSM_TILE_SET_S *ts, double lat, double lon, POINT *pt, BOOL Visibility/*=TRUE*/)
{	BOOL Visible = FALSE;

	if (lon <= -180) lon = -179.99999999999;
	if (lon >= 180) lon = 179.99999999999;
	if (lat <= -90) lat = -89.99999;
	if (lat >= 90) lat = 89.99999;

#ifdef INEFFICIENT
	int xt = long2tilex(lon, ts->zoom);
	int yt = lat2tiley(lat, ts->zoom);
	int xo = long2tilexPixel(lon, ts->zoom);
	int yo = lat2tileyPixel(lat, ts->zoom);
#else
	double xd = long2tilexd(lon, ts->zoom);
	double yd = lat2tileyd(lat, ts->zoom);

	int xt = TILE(xd);
	int yt = TILE(yd);
	int xo = PIXEL(xd);
	int yo = PIXEL(yd);
#endif
	if (Visibility)	/* Longer to do it this way */
	{
		int t;
		double MinDistance = DBL_MAX;
		POINT ptC, ptM;	/* Center point, Min point */
		ptC.x = ts->rcWin.left + (ts->rcWin.right-ts->rcWin.left)/2;
		ptC.y = ts->rcWin.top + (ts->rcWin.bottom-ts->rcWin.top)/2;

		for (t=0; t<ts->Count; t++)
		{	if (ts->Tiles[t].Tile
			&& ts->Tiles[t].x == xt
			&& ts->Tiles[t].y == yt)
			{
#ifdef LINEAR_SCALE
				pt->x = ts->Tiles[t].rc.left + (int)((lon-ts->Tiles[t].Tile->min.lon)*256.0/(ts->Tiles[t].Tile->max.lon-ts->Tiles[t].Tile->min.lon));
				pt->y = ts->Tiles[t].rc.top + (int)((lat-ts->Tiles[t].Tile->min.lat)*256.0/(ts->Tiles[t].Tile->max.lat-ts->Tiles[t].Tile->min.lat));
#else
				POINT ptT;
				ptT.x = ts->Tiles[t].rc.left + xo;
				ptT.y = ts->Tiles[t].rc.top + yo;
				int xd = ptT.x-ptC.x;
				int yd = ptT.y-ptC.y;
				double Distance = ((double)xd*(double)xd+(double)yd*(double)yd);
				if (Distance < MinDistance)
				{	MinDistance = Distance;
					ptM = ptT;
				}
#endif
#ifdef VERBOSE
TraceLogThread("OSM", FALSE, "OSMGetXYPos:%.4lf (%.4lf %.4lf) %.4lf (%.4lf %.4lf) Tile[%ld] z:%ld %ld %ld At %ld %ld Left/Top: %ld %ld\n",
			(double) lat,
			(double) ts->Tiles[t].Tile->min.lat, (double) ts->Tiles[t].Tile->max.lat, 
			(double) lon,
			(double) ts->Tiles[t].Tile->min.lon, (double) ts->Tiles[t].Tile->max.lon, 
			(long) t, (long) ts->zoom, (long) xt, (long) yt,
			(long) pt->x, (long) pt->y,
			(long) ts->Tiles[t].rc.left, (long) ts->Tiles[t].rc.top);
#endif
//return TRUE;
			}
		}

		if (MinDistance != DBL_MAX)	/* Found it? */
		{	*pt = ptM;
			Visible = TRUE;
		}
	}

//TraceLogThread("OSM", FALSE, "OSMGetXYPos(Default):%.4lf %.4lf (Tile %ld %ld) Not In Tile Set\n",
//	   (double) lat, (double) lon, (long) xt, (long) yt);

	if (!Visible)	/* Still need to calculate? */
	{	int width = ts->rcWin.right-ts->rcWin.left, height = ts->rcWin.bottom-ts->rcWin.top;
		int cx = width/2, cy = height/2;
		int left = ts->rcWin.left+(xt-ts->xt)*256+cx-128+ts->xo;
		int top = ts->rcWin.top+(yt-ts->yt)*256+cy-128+ts->yo;

		pt->x = left + xo;
		pt->y = top + yo;
	}

	return Visible;
}

#ifdef OBSOLETE
static void DrawGridSquareLines(HDC hdc, OSM_TILE_SET_S *ts)
{
	double latStep, lonStep;

	if (ts->zoom <= 4)
	{	latStep = 10; lonStep = 20;
	} else if (ts->zoom <= 8)
	{	latStep = 1; lonStep = 2;
	} else
	{	latStep = 2.5/60.0; lonStep = 5.0/60.0;	/* as fine as it gets */
	}

	int pMax = 180.0/latStep + 360.0/lonStep + 4;
	POINT *Points = (POINT *) malloc(sizeof(*Points)*pMax);
	int p;
	double lat, lon;

	double latStart = ts->lat-latStep*10, latEnd = ts->lat+latStep*10;	/* -80, 80 */
	double lonStart = ts->lon-lonStep*10, lonEnd = ts->lon+lonStep*10;	/* -180, 180 */

	latStart -= fmod(latStart, latStep);
	latEnd += latStep - fmod(latEnd, latStep);
	lonStart -= fmod(lonStart, lonStep);
	lonEnd += lonStep - fmod(lonEnd, lonStep);

	for (lat = latStart; lat <= latEnd; lat += latStep)
	{	double tlat = lat;
		while (tlat <= -90) tlat += 180;
		while (tlat >= 90) tlat -= 180;
		if (tlat >= -85 && tlat <= 85)
		for (lon=lonStart, p=0; lon<=lonEnd && p<pMax; lon+=lonStep, p++)
		{	double tlon = lon;
			while (tlon < -180) tlon += 360;
			while (tlon > 180) tlon -= 360;
			if (!OSMGetXYPos(ts, tlat, tlon, &Points[p]))
			{	p--;	/* Ignore Bad conversion */
			}
			if (p>=0)
			if (Points[p].x < -32767 || Points[p].x > 32767
			|| Points[p].y < -32767 || Points[p].y > 32767)
			{	if (p > 1) Polyline(hdc, Points, p);
				p = -1;	/* Restart the list, auto increment above */
			}
		} else p = 0;
		if (p > 1) Polyline(hdc, Points, p);
	}

	for (lon=lonStart; lon<=lonEnd; lon+=lonStep)
	{	double tlon = lon;
		while (tlon < -180) tlon += 360;
		while (tlon > 180) tlon -= 360;
		for (lat=latStart, p=0; lat<=latEnd && p<pMax; lat+=latStep, p++)
		{	double tlat = lat;
			while (tlat <= -90) tlat += 180;
			while (tlat >= 90) tlat -= 180;
			if (tlat >= -85 && tlat <= 85)
			{	if (!OSMGetXYPos(ts, tlat, tlon, &Points[p]))
				{	p--;	/* Ignore Bad conversion */
				}
			}
			else Points[p].x = Points[p].y = 65535;	/* Bad conversion */

			if (p>=0)
			if (Points[p].x < -32767 || Points[p].x > 32767
			|| Points[p].y < -32767 || Points[p].y > 32767)
			{	if (p > 1) Polyline(hdc, Points, p);
				p = -1;	/* Restart the list, auto increment above */
			}
		}
		if (p > 1) Polyline(hdc, Points, p);
	}
	free(Points);
}
#endif

static void DrawGridSquareLines2(HDC hdc, OSM_TILE_SET_S *ts)
{
	double latStep, lonStep;

	if (ts->zoom <= 4)
	{	latStep = 10; lonStep = 20;
	} else if (ts->zoom <= 9)
	{	latStep = 1; lonStep = 2;
	} else
	{	latStep = 2.5/60.0; lonStep = 5.0/60.0;	/* as fine as it gets */
	}

	double lat, lon;

	double latStart = ts->lat-latStep*30, latEnd = ts->lat+latStep*30;	/* -80, 80 */
	double lonStart = ts->lon-lonStep*30, lonEnd = ts->lon+lonStep*30;	/* -180, 180 */

//	latStart = tiley2lat(ts->ys, ts->zoom);
//	latEnd = tiley2lat(ts->ye+1, ts->zoom);
//	lonStart = tilex2long(ts->xs, ts->zoom);
//	lonEnd = tilex2long(ts->xe+1, ts->zoom);

	latStart -= fmod(latStart, latStep);
	latEnd += latStep - fmod(latEnd, latStep);
	lonStart -= fmod(lonStart, lonStep);
	lonEnd += lonStep - fmod(lonEnd, lonStep);

	for (lat = latStart; lat <= latEnd; lat += latStep)
	{	double tlat = lat;
		while (tlat <= -90) tlat += 180;
		while (tlat >= 90) tlat -= 180;
		if (tlat >= -85 && tlat <= 85)
		{

#ifdef INEFFICIENT
	int yt = lat2tiley(tlat, ts->zoom);
	int yo = lat2tileyPixel(tlat, ts->zoom);
#else
	double yd = lat2tileyd(tlat, ts->zoom);
	int yt = TILE(yd);
	int yo = PIXEL(yd);
#endif

	int t;
	for (t=0; t<ts->Count; t++)
	{	if (ts->Tiles[t].Tile
		&& ts->Tiles[t].x == ts->xt
		&& ts->Tiles[t].y == yt)
		{	int y = (ts->Tiles[t].yw-ts->ys)*256 + yo;

			POINT pts[2];
			pts[0].x = 0;
			pts[0].y = y;
			pts[1].x = (ts->xe-ts->xs+1)*256;
			pts[1].y = y;
			Polyline(hdc, pts, 2);
		}
	}
		}
	}

	for (lon=lonStart; lon<=lonEnd; lon+=lonStep)
	{	double tlon = lon;
		while (tlon < -180) tlon += 360;
		while (tlon > 180) tlon -= 360;

//	if (tlon <= -180) tlon = -179.99999999999;
//	if (tlon >= 180) tlon = -180;

#ifdef INEFFICIENT
	int xt = long2tilex(tlon, ts->zoom);
	int xo = long2tilexPixel(tlon, ts->zoom);
#else
	double xd = long2tilexd(tlon, ts->zoom);
	int xt = TILE(xd);
	int xo = PIXEL(xd);
#endif

	int t;
	for (t=0; t<ts->Count; t++)
	{	if (ts->Tiles[t].Tile
		&& ts->Tiles[t].x == xt
		&& ts->Tiles[t].y == ts->yt)
		{	long x = (ts->Tiles[t].xw-ts->xs)*256 + xo;

			POINT pts[2];
			pts[0].x = x;
			pts[0].y = 0;
			pts[1].x = x;
			pts[1].y = (ts->ye-ts->ys+1)*256;
			Polyline(hdc, pts, 2);

		}
	}

	}
}

void OSMPaintTileSet(HWND hWnd, HDC hdc, RECT *rc, int Percent, OSM_TILE_SET_S *ts)
{	BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, 0 };
	HDC memDC = CreateCompatibleDC(hdc);
	__int64 Start = llGetMsec();
	int t, Painted=0;

	if (!ts) return;

	bf.SourceConstantAlpha = (Percent * 255) / 100;

#ifdef VERBOSE
	if (ts->Percent != Percent)
		TraceLogThread("OSM", FALSE, "OsmPaintTileSet:Percent Mismatch! %ld != %ld\n", (long) ts->Percent, (long) Percent);
#endif

	if (ts->hbm && ts->Percent == Percent)
	{	POINT pt, ht, tg;
		int Width = ts->rcWin.right - ts->rcWin.left;
		int Height = ts->rcWin.bottom - ts->rcWin.top;
		int cx = Width/2, cy=Height/2;
		HGDIOBJ old = SelectObject(memDC,ts->hbm);

		if (!old) TraceLogThread("OSM", TRUE, "OSMPaintTileSet:SelectObject for ts->hbm 0x%lX into memDC 0x%lX Failed, Error %ld\n", (long) ts->hbm, (long) memDC,(long) GetLastError());

		pt.x = ((ts->xe-ts->xs+1)*256-Width)/2-ts->xo;
		pt.y = ((ts->ye-ts->ys+1)*256-Height)/2-ts->yo;
#ifdef FOR_REFERENCE
rcWin->left+(ts->Tiles[t].x-xt)*256+cx-128+xo,
rcWin->top+(ts->Tiles[t].y-yt)*256+cy-128+yo,

WinMain:2010-09-13T11:58:42.054 OSMPaintTileSet: Screen 987 x 584 World 1280 x 1280 - Draw from 272 391 for offset -125 -43 (x 251->253->255 y 157->159->161)
WinMain:2010-09-13T11:58:42.125 OSMPaintTileSet: Screen 987 x 584 World 1280 x 1280 - Draw from 273 390 for offset -126 -42 (x 251->253->255 y 157->159->161)
WinMain:2010-09-13T11:58:42.283 OSMPaintTileSet: Screen 987 x 584 World 1280 x 1280 - Draw from 274 390 for offset -127 -42 (x 251->253->255 y 157->159->161)

WinMain:2010-09-13T11:58:42.301 OSMPaintTileSet: Screen 987 x 584 World 1280 x 1280 - Draw from 19 390 for offset 128 -42 (x 251->254->255 y 157->159->161)
WinMain:2010-09-13T11:58:42.340 OSMPaintTileSet: Screen 987 x 584 World 1280 x 1280 - Draw from 19 389 for offset 128 -41 (x 251->254->255 y 157->159->161)
WinMain:2010-09-13T11:58:42.407 OSMPaintTileSet: Screen 987 x 584 World 1280 x 1280 - Draw from 19 388 for offset 128 -40 (x 251->254->255 y 157->159->161)


(255-251+1)/2 = 512
(253-251) = 512
(254-251) = 

#endif

		pt.x = ((ts->xe-ts->xs+1)*256)/2-cx-ts->xo;
		pt.y = ((ts->ye-ts->ys+1)*256)/2-cy-ts->yo;

		pt.x = (ts->xt-ts->xs)*256+128-ts->xo - cx;
		pt.y = (ts->yt-ts->ys)*256+128-ts->yo - cy;

		tg.x = ts->rcWin.left;
		tg.y = ts->rcWin.top;

		if (pt.x < 0)
		{	tg.x += -pt.x;
			pt.x = 0;
		}
		if (pt.y < 0)
		{	tg.y += -pt.y;
			pt.y = 0;
		}

		ht.x = ts->rcWin.right - ts->rcWin.left;
		ht.y = ts->rcWin.bottom - ts->rcWin.top;

		if (pt.x+ht.x >= (ts->xe-ts->xs+1)*256)
		{	ht.x = (ts->xe-ts->xs+1)*256 - pt.x;
		}
		if (pt.y+ht.y >= (ts->ye-ts->ys+1)*256)
		{	ht.y = (ts->ye-ts->ys+1)*256 - pt.y;
		}

#ifdef VERBOSE
TraceLogThread("OSM", TRUE, "OSMPaintTileSet: Screen %ld x %ld World %ld x %ld - Draw from %ld %ld for offset %ld %ld (x %ld->%ld->%ld y %ld->%ld->%ld)\n",
			Width, Height,
			(ts->xe-ts->xs+1)*256, 
			(ts->ye-ts->ys+1)*256, 
			pt.x, pt.y, ts->xo, ts->yo,
			ts->xs, ts->xt, ts->xe,
			ts->ys, ts->yt, ts->ye);
#endif

		if (!BitBlt(hdc, tg.x, tg.y,
			ht.x, ht.y,
			memDC, pt.x, pt.y, SRCCOPY))
			TraceLogThread("OSM", TRUE, "OSMPaintTileSet:BitBlt Failed on hbm 0x%lX error %ld\n", (long) ts->hbm, GetLastError());

//		if (GridSquareLines) DrawGridSquareLines(hdc, ts);

		Start = llMsecSince(Start,llGetMsec());
	if (Start > 50)
		TraceLogThread("OSM", Start>=1000 || (ts->msWorld&&Start>ts->msWorld), "Painted Whole World (%ld x %ld @ %ld%%) in %ld msec (Original %ld)\n",
			(long) ht.x, (long) ht.y, (long) ts->Percent, (long) Start, (long) ts->msWorld);
		Start = llGetMsec();
	}
/*
	Don't have a whole world Bitmap, do it the old way - Tile by Tile
*/
	else
	{

#ifdef VERBOSE
	TraceLogThread("OSM", FALSE, "Painting %ld Tiles in rc %ld %ld -> %ld %ld (%ld x %ld)\n",
				   (long) ts->Count, (long) rc->left, (long) rc->top, (long) rc->right, (long) rc->bottom,
				   (long) rc->right-rc->left, (long) rc->bottom-rc->top);
#endif

	for (t=0; t<ts->Count; t++)
	{	if (ts->Tiles[t].Tile)
		{	RECT rcTemp;
			if (!rc || IntersectRect(&rcTemp, rc, &ts->Tiles[t].rc))
			{	HGDIOBJ old = SelectObject(memDC,ts->Tiles[t].Tile->hbmBig);
				if (!old) TraceLogThread("OSM", TRUE, "OSMPaintTileSet:SelectObject for Tile[%ld/%ld] hbm 0x%lX into memDC 0x%lX Failed, Error %ld\n",
									(long) t, (long) ts->Count, (long) ts->Tiles[t].Tile->hbmBig, (long) memDC, (long) GetLastError());
#ifdef TRACK_PAINTING
				else TraceLogThread("OSM", FALSE, "OSMPaintTileSet:Tile[%ld/%ld] Tile %p hbm 0x%lX\n",
									(long) t, (long) ts->Count, ts->Tiles[t].Tile, (long) ts->Tiles[t].Tile->hbm);
#endif
				Painted++;
				if (Percent > 90)
				{	if (ts->Tiles[t].stretch)
					{
					if (!StretchBlt(hdc, ts->Tiles[t].rc.left, ts->Tiles[t].rc.top,
						ts->Tiles[t].rc.right - ts->Tiles[t].rc.left,
						ts->Tiles[t].rc.bottom - ts->Tiles[t].rc.top,
						memDC, ts->Tiles[t].rcSource.left, ts->Tiles[t].rcSource.top,
						ts->Tiles[t].rcSource.right - ts->Tiles[t].rcSource.left,
						ts->Tiles[t].rcSource.bottom - ts->Tiles[t].rcSource.top, SRCCOPY))
						TraceLogThread("OSM", TRUE, "OSMPaintTileSet:StretchBlt Failed on hbm 0x%lX\n", (long) ts->Tiles[t].Tile->hbmBig);
//					FrameRect(hdc, &ts->Tiles[t].rc, (HBRUSH) GetStockObject(BLACK_BRUSH));
#ifdef HASH_STRETCHED
					{	POINT Points[8];
						Points[0].x = ts->Tiles[t].rc.left;
						Points[0].y = ts->Tiles[t].rc.top;
						Points[1].x = ts->Tiles[t].rc.right;
						Points[1].y = ts->Tiles[t].rc.bottom;
						Points[2].x = ts->Tiles[t].rc.right;
						Points[2].y = ts->Tiles[t].rc.top;
						Points[3].x = ts->Tiles[t].rc.left;
						Points[3].y = ts->Tiles[t].rc.bottom;
						Points[4].x = ts->Tiles[t].rc.left;
						Points[4].y = ts->Tiles[t].rc.top;
						Points[5].x = ts->Tiles[t].rc.right;
						Points[5].y = ts->Tiles[t].rc.top;
						Points[6].x = ts->Tiles[t].rc.right;
						Points[6].y = ts->Tiles[t].rc.bottom;
						Points[7].x = ts->Tiles[t].rc.left;
						Points[7].y = ts->Tiles[t].rc.bottom;
						Polyline(hdc, Points, 8);
					}
#endif
//					TCHAR Buffer[33];
//					StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%ld"), (long) ts->Tiles[t].stretch);
//					DrawText(hdc, Buffer, -1, &ts->Tiles[t].rc, DT_CENTER | DT_VCENTER | DT_NOPREFIX | DT_SINGLELINE);
 					} else
					if (!BitBlt(hdc, ts->Tiles[t].rc.left, ts->Tiles[t].rc.top,
						ts->Tiles[t].rc.right - ts->Tiles[t].rc.left,
						ts->Tiles[t].rc.bottom - ts->Tiles[t].rc.top,
						memDC, ts->Tiles[t].Tile->rcBig.left, ts->Tiles[t].Tile->rcBig.top, SRCCOPY))
						TraceLogThread("OSM", TRUE, "OSMPaintTileSet:BitBlt Failed on hbm 0x%lX error %ld\n", (long) ts->Tiles[t].Tile->hbmBig, GetLastError());
				} else
				{	FillRect(hdc, &ts->Tiles[t].rc, (HBRUSH) GetStockObject(ts->Dim?BLACK_BRUSH:WHITE_BRUSH));
					if (ts->Tiles[t].stretch)
					{
//					FrameRect(hdc, &ts->Tiles[t].rc, (HBRUSH) GetStockObject(BLACK_BRUSH));
#ifdef NO_ALPHABLEND
					if (!StretchBlt(hdc, ts->Tiles[t].rc.left, ts->Tiles[t].rc.top,
						ts->Tiles[t].rc.right - ts->Tiles[t].rc.left,
						ts->Tiles[t].rc.bottom - ts->Tiles[t].rc.top,
						memDC, ts->Tiles[t].rcSource.left, ts->Tiles[t].rcSource.top,
						ts->Tiles[t].rcSource.right - ts->Tiles[t].rcSource.left,
						ts->Tiles[t].rcSource.bottom - ts->Tiles[t].rcSource.top, SRCCOPY))
						TraceLogThread("OSM", TRUE, "OSMPaintTileSet:StretchBlt Failed on hbm 0x%lX\n", (long) ts->Tiles[t].Tile->hbmBig);
#else
					if (!AlphaBlend(hdc, ts->Tiles[t].rc.left, ts->Tiles[t].rc.top,
						ts->Tiles[t].rc.right - ts->Tiles[t].rc.left,
						ts->Tiles[t].rc.bottom - ts->Tiles[t].rc.top,
						memDC, ts->Tiles[t].rcSource.left, ts->Tiles[t].rcSource.top,
						ts->Tiles[t].rcSource.right - ts->Tiles[t].rcSource.left,
						ts->Tiles[t].rcSource.bottom - ts->Tiles[t].rcSource.top,
						bf))
						TraceLogThread("OSM", TRUE, "OSMPaintTileSet:AlphaBlend(stretch) Failed on hbm 0x%lX\n", (long) ts->Tiles[t].Tile->hbmBig);
#endif
#ifdef HASH_STRETCHED
					{	POINT Points[8];
						Points[0].x = ts->Tiles[t].rc.left;
						Points[0].y = ts->Tiles[t].rc.top;
						Points[1].x = ts->Tiles[t].rc.right;
						Points[1].y = ts->Tiles[t].rc.bottom;
						Points[2].x = ts->Tiles[t].rc.right;
						Points[2].y = ts->Tiles[t].rc.top;
						Points[3].x = ts->Tiles[t].rc.left;
						Points[3].y = ts->Tiles[t].rc.bottom;
						Points[4].x = ts->Tiles[t].rc.left;
						Points[4].y = ts->Tiles[t].rc.top;
						Points[5].x = ts->Tiles[t].rc.right;
						Points[5].y = ts->Tiles[t].rc.top;
						Points[6].x = ts->Tiles[t].rc.right;
						Points[6].y = ts->Tiles[t].rc.bottom;
						Points[7].x = ts->Tiles[t].rc.left;
						Points[7].y = ts->Tiles[t].rc.bottom;
						Polyline(hdc, Points, 8);
					}
#endif
//					TCHAR Buffer[33];
//					StringCbPrintf(Buffer, sizeof(Buffer), TEXT("%ld"), (long) ts->Tiles[t].stretch);
//					DrawText(hdc, Buffer, -1, &ts->Tiles[t].rc, DT_CENTER | DT_VCENTER | DT_NOPREFIX | DT_SINGLELINE);
					} else 
#ifdef NO_ALPHABLEND
					if (!BitBlt(hdc, ts->Tiles[t].rc.left, ts->Tiles[t].rc.top,
						ts->Tiles[t].rc.right - ts->Tiles[t].rc.left,
						ts->Tiles[t].rc.bottom - ts->Tiles[t].rc.top,
						memDC, ts->Tiles[t].Tile->rcBig.left, ts->Tiles[t].Tile->rcBig.top, SRCCOPY))
						TraceLogThread("OSM", TRUE, "OSMPaintTileSet:BitBlt Failed on hbm 0x%lX error %ld\n", (long) ts->Tiles[t].Tile->hbmBig, GetLastError());
#else
					if (!AlphaBlend(hdc, ts->Tiles[t].rc.left, ts->Tiles[t].rc.top,
						ts->Tiles[t].rc.right - ts->Tiles[t].rc.left,
						ts->Tiles[t].rc.bottom - ts->Tiles[t].rc.top,
						memDC, ts->Tiles[t].Tile->rcBig.left, ts->Tiles[t].Tile->rcBig.top,
						ts->Tiles[t].rc.right - ts->Tiles[t].rc.left,
						ts->Tiles[t].rc.bottom - ts->Tiles[t].rc.top,
						bf))
						TraceLogThread("OSM", TRUE, "OSMPaintTileSet:AlphaBlend(straight) Failed on hbm 0x%lX\n", (long) ts->Tiles[t].Tile->hbmBig);
#endif
				}
				if (old) SelectObject(memDC, old);
				if (!rc)
				{	ValidateRect(hWnd, &ts->Tiles[t].rc);
				}
//FrameRect(hdc, &ts->Tiles[t].rc, (HBRUSH) GetStockObject(BLACK_BRUSH));
			}
#ifdef TRACK_PAINTING
			else TraceLogThread("OSM", FALSE, "OSMPaintTileSet:Tile[%ld/%ld] Tile %p %ld %ld %ld %ld Not Intersect %ld %ld %ld %ld\n",
						(long) t, (long) ts->Count, ts->Tiles[t].Tile,
						ts->Tiles[t].rc.left, ts->Tiles[t].rc.top,
						ts->Tiles[t].rc.right, ts->Tiles[t].rc.bottom,
						rc->left, rc->top,
						rc->right, rc->bottom);
#endif
		}
		else TraceLogThread("OSM", FALSE, "OSMPaintTileSet:Tile[%ld/%ld] NULL Tile %ld %ld\n",
							(long) t, (long) ts->Count, ts->Tiles[t].x, ts->Tiles[t].y);
	}
	Start = llMsecSince(Start,llGetMsec());
	if (Start > 50)
		TraceLogThread("OSM", Start>=1000, "Painted %ld/%ld Tiles in %.2lf msec\n",
				   (long) Painted, (long) ts->Count, Start);
	if (Start >= 1000)
		TraceLogThread("Activity", TRUE, "Painted %ld/%ld Tiles in %.2lf msec\n",
					   (long) Painted, (long) ts->Count, Start);
	}
	if (!DeleteObject(memDC))
		TraceLogThread("OSM",TRUE,"OSMPaintTileSet:Failed to Delete memDC %p error %ld\n", memDC, GetLastError());
}

void OSMFreeTileSet(OSM_TILE_SET_S *ts)
{	int t;

	for (t=0; t<ts->Count; t++)
	if (ts->Tiles[t].Tile)
		OSMFreeTile(ts->Tiles[t].Tile);
	if (ts->Tiles) free(ts->Tiles);
	if (ts->hbm) if (!DeleteObject(ts->hbm))
				TraceLogThread("OSM",TRUE,"OSMFreeTileSet:Failed to Delete ts->hbm %p error %ld\n", ts->hbm, GetLastError());

	free(ts);
}

static void OSMCalcTileRanges(RECT *rcWin, int zoom, double lat, double lon,
								int *pxt, int *pyt, int *pxo, int *pyo,
								int *pxs, int *pxe, int *pys, int *pye, BOOL Limit=TRUE)
{	int width = rcWin->right-rcWin->left, height = rcWin->bottom-rcWin->top;

#ifdef INEFFICIENT
	int xt = long2tilex(lon, zoom);
	int yt = lat2tiley(lat, zoom);
	int xo = 128 - long2tilexPixel(lon, zoom);
	int yo = 128 - lat2tileyPixel(lat, zoom);
#else
	double xd = long2tilexd(lon, zoom);
	double yd = lat2tileyd(lat, zoom);

	int xt = TILE(xd);
	int yt = TILE(yd);
	int xo = 128-PIXEL(xd);
	int yo = 128-PIXEL(yd);
#endif

	int xs = xt - width/256/2 - 1;
	int xe = xt + width/256/2 + 1;
	int ys = yt - height/256/2 - 1;
	int ye = yt + height/256/2 + 1;

	if (Limit)
	{	if (xs < 0) xs = 0;
		if (xe >= 1<<zoom) xe = (1<<zoom)-1;
		if (ys < 0) ys = 0;
		if (ye >= 1<<zoom) ye = (1<<zoom)-1;
		if (xs > xe) xs = xe;
		if (ys > ye) ys = ye;
	}

#ifdef OBSOLETE
#ifndef UNDER_CE
static	double lastLat=0, lastLon = 0;
if (lat != lastLat || lon != lastLon)
{	lastLat = lat; lastLon = lon;
	TraceLogThread("OSM", FALSE, "OSM3:X/lon %.4lf vs %.4lf-%.4lf = %ld (%ld) into %ld\n",
			(double) lon, (double) tilex2long(xt,zoom), (double) tilex2long(xt+1,zoom),
			(long) 128-xo, (long) xo, (long) xt);
	TraceLogThread("OSM", FALSE, "OSMGetTileSet:Y/lat %.4lf vs %.4lf-%.4lf = %ld (%ld) into %ld\n",
			(double) lat, (double) tiley2lat(yt,zoom), (double) tiley2lat(yt+1,zoom),
			(long) 128-yo, (long) yo, (long) yt);
	TraceLogThread("OSM", FALSE, "OSMGetTileSet:Final: %.4lf %.4lf -> %ld-%ld %ld-%ld @ %ld\n",
			(double) lat, (double) lon, (long) xs, (long) xe, (long) ys, (long) ye, (long) zoom);
	TraceLogThread("OSM", FALSE, "OSMGetTileSet:Calculated %ld tiles for %ld x %ld (%ld x %ld)\n",
	   (long) ts->Size, (long) width, (long) height, (long)(width+255)/256, (long)(height+255)/256);
}
#endif
#endif

	*pxt = xt; *pyt = yt; *pxo = xo; *pyo = yo;
	*pxs = xs; *pxe = xe; *pys = ys; *pye = ye;
}

#ifdef UNNECESSARY
static BOOL AreAllTilesPresent(OSM_TILE_SET_S *ts, int xs, int xe, int ys, int ye)
{
	for (int x=xs; x<=xe; x++)
	{	for (int y=ys; y<=ye; y++)
		{	int t;
			for (t=0; t<ts->Count; t++)
			{	if (ts->Tiles[t].x == x && ts->Tiles[t].y == y)
					break;
			}
			if (t >= ts->Count)
			{
TraceLogThread("OSM", FALSE, "Missing Tile %ld %ld from %ld->%ld %ld->%ld over %ld Tiles\n", x, y, xs, xe, ys, ye, ts->Count);
				return FALSE;
			}
		}
	}
TraceLogThread("OSM", TRUE, "All Tiles Found From %ld->%ld %ld->%ld over %ld Tiles\n", xs, xe, ys, ye, ts->Count);
	return TRUE;
}
#endif

static void PaintTileToWorld(OSM_TILE_SET_S *ts, OSM_TILE_POS_S *tp, BOOL Dim, HDC dcDst, HDC dcSrc, int Percent)
{	RECT rcTarget;

	rcTarget.left = (tp->xw-ts->xs)*256;
	rcTarget.top = (tp->yw-ts->ys)*256;
	rcTarget.right = rcTarget.left + 256;
	rcTarget.bottom = rcTarget.top + 256;

	if (tp->Tile)
	{	HGDIOBJ old = SelectObject(dcSrc,tp->Tile->hbmBig);
		if (!old) TraceLogThread("OSM", TRUE, "PaintTileToWorld:SelectObject for hbm 0x%lX into dcSrc 0x%lX Failed, Error %ld\n",
								(long) tp->Tile->hbmBig, (long) dcSrc, (long)GetLastError());

TraceLogThread("OSM", FALSE, "PaintTileToWorld:Tile %ld %ld w(%ld %ld) targets %ld %ld -> %ld %ld\n",
		   tp->x, tp->y,
		   tp->xw, tp->yw,
		   rcTarget.left, rcTarget.top, rcTarget.right, rcTarget.bottom);

		if (Percent > 90)
		{	if (tp->stretch)
			{	if (!StretchBlt(dcDst,
						rcTarget.left, rcTarget.top,
						(rcTarget.right-rcTarget.left),
						(rcTarget.bottom-rcTarget.top),
						dcSrc,
						tp->rcSource.left, tp->rcSource.top,
						tp->rcSource.right - tp->rcSource.left,
						tp->rcSource.bottom - tp->rcSource.top,
						SRCCOPY))
					TraceLogThread("OSM", TRUE, "PaintTileToWorld:StretchBlt Failed on hbm 0x%lX\n", (long) tp->Tile->hbmBig);

			} else
			{	if (!BitBlt(dcDst,
						rcTarget.left, rcTarget.top,
						(rcTarget.right-rcTarget.left),
						(rcTarget.bottom-rcTarget.top),
						dcSrc, tp->Tile->rcBig.left, tp->Tile->rcBig.top, SRCCOPY))
					TraceLogThread("OSM", TRUE, "PaintTileToWorld:BitBlt Failed on hbm 0x%lX error %ld\n", (long) tp->Tile->hbmBig, GetLastError());
			}
		} else
		{	BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, 0 };
			bf.SourceConstantAlpha = (Percent * 255) / 100;
			FillRect(dcDst, &rcTarget, (HBRUSH) GetStockObject(Dim?BLACK_BRUSH:WHITE_BRUSH));
#ifdef NO_ALPHABLEND
			if (tp->stretch)
			{	if (!StretchBlt(dcDst,
						rcTarget.left, rcTarget.top,
						(rcTarget.right-rcTarget.left),
						(rcTarget.bottom-rcTarget.top),
						dcSrc,
						tp->rcSource.left, tp->rcSource.top,
						tp->rcSource.right - tp->rcSource.left,
						tp->rcSource.bottom - tp->rcSource.top,
						SRCCOPY))
					TraceLogThread("OSM", TRUE, "PaintTileToWorld:StretchBlt Failed on hbm 0x%lX\n", (long) tp->Tile->hbmBig);

			} else
			{	if (!BitBlt(dcDst,
						rcTarget.left, rcTarget.top,
						(rcTarget.right-rcTarget.left),
						(rcTarget.bottom-rcTarget.top),
						dcSrc, tp->Tile->rcBig.left, tp->Tile->rcBig.top, SRCCOPY))
					TraceLogThread("OSM", TRUE, "PaintTileToWorld:BitBlt Failed on hbm 0x%lX error %ld\n", (long) tp->Tile->hbmBig, GetLastError());
			}
#else
			if (tp->stretch)
			{
				if (!AlphaBlend(dcDst,
							rcTarget.left, rcTarget.top,
							(rcTarget.right-rcTarget.left),
							(rcTarget.bottom-rcTarget.top),
					dcSrc, tp->rcSource.left, tp->rcSource.top,
					tp->rcSource.right - tp->rcSource.left,
					tp->rcSource.bottom - tp->rcSource.top,
					bf))
						TraceLogThread("OSM", TRUE, "PaintTileToWorld:AlphaBlend(stretch) Failed on hbm 0x%lX\n", (long) tp->Tile->hbmBig);
			} else 
			{
				if (!AlphaBlend(dcDst,
								rcTarget.left, rcTarget.top,
								(rcTarget.right-rcTarget.left),
								(rcTarget.bottom-rcTarget.top),
					dcSrc, tp->Tile->rcBig.left, tp->Tile->rcBig.top,
					tp->rc.right - tp->rc.left,
					tp->rc.bottom - tp->rc.top,
					bf))
						TraceLogThread("OSM", TRUE, "PaintTileToWorld:AlphaBlend(straight) Failed on hbm 0x%lX\n", (long) tp->Tile->hbmBig);
			}
#endif
		}
//#define FRAME_TILES
#ifdef FRAME_TILES
		FrameRect(dcDst, &rcTarget, (HBRUSH) GetStockObject(BLACK_BRUSH));
#endif
//#define LABEL_TILES
#ifdef LABEL_TILES
		FrameRect(dcDst, &rcTarget, (HBRUSH) GetStockObject(BLACK_BRUSH));
		{	TCHAR Text[80];
			StringCbPrintf(Text, sizeof(Text), TEXT("%ld"), rcTarget.top);
			DrawText(dcDst, Text, -1, &rcTarget, DT_TOP | DT_CENTER | DT_SINGLELINE);
			StringCbPrintf(Text, sizeof(Text), TEXT("%ld"), rcTarget.bottom);
			DrawText(dcDst, Text, -1, &rcTarget, DT_BOTTOM | DT_CENTER | DT_SINGLELINE);
			StringCbPrintf(Text, sizeof(Text), TEXT(" %ld"), rcTarget.left);
			DrawText(dcDst, Text, -1, &rcTarget, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
			StringCbPrintf(Text, sizeof(Text), TEXT("%ld "), rcTarget.right);
			DrawText(dcDst, Text, -1, &rcTarget, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
			StringCbPrintf(Text, sizeof(Text), TEXT("%ld %ld"), (long) tp->x, (long) tp->y);
			DrawText(dcDst, Text, -1, &rcTarget, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}
#endif
		if (old) if (SelectObject(dcSrc, old) != tp->Tile->hbmBig)
					TraceLogThread("OSM", TRUE, "PaintTileToWorld:SelectObject(dcSrc,old) Failed, Error %ld\n",
								GetLastError());
	}
	else
	{	TraceLogThread("OSM", TRUE, "PaintTileToWorld:TileNULL Tile %ld %ld w(%ld %ld)\n",
						tp->x, tp->y, tp->xw, tp->yw);
		FillRect(dcDst, &rcTarget, (HBRUSH) GetStockObject(GRAY_BRUSH));
	}
}

BOOL OSMIsTileSetCompatible(HWND hwnd, OSM_TILE_SET_S *ts, RECT *rcWin, TILE_SERVER_INFO_S *sInfo, BOOL Dim, int zoom, double lat, double lon, int Percent, BOOL GridSquareLines, BOOL *pUpdated, char **pWhy)
{
static char Why[512];

	sInfo = OSMRegisterTileServer(sInfo, "Compat");

	if (pUpdated) *pUpdated = FALSE;
	if (pWhy) *pWhy = Why;
	if (ts)
	{
	if (!ts->BuiltTooMany
	&& ts->Dim == Dim
	&& ts->zoom == zoom
	&& ts->Percent == Percent
	&& ts->GridSquares == GridSquareLines
	&& ts->sInfo == sInfo)
	{	int xt, yt, xo, yo;
		int xs, xe, ys, ye;
		OSMCalcTileRanges(rcWin, zoom, lat, lon, &xt, &yt, &xo, &yo, &xs, &xe, &ys, &ye, FALSE);
		if ((xs >= ts->xs && xe <= ts->xe
		&& ys >= ts->ys && ye <= ts->ye)
#ifdef UNNECESSARY
		|| AreAllTilesPresent(ts, xs, xe, ys, ye)
#endif
		)
		{
/*	If we're zoomed in to stretching range, palm it off to someone else */
			if (ts->StillLoading && ts->TilesFetched != TilesFetched && ts->zoom > sInfo->MaxServerZoom)
			{	StringCbPrintfA(Why, sizeof(Why), "MaxZoom [%ld] %s %s BUT %ld > %ld",
								(long) ts->Count,
								ts->StillLoading?"Loading":"Done",
								ts->TilesFetched==TilesFetched?"NoNew":"New",
								(long) ts->zoom, (long) sInfo->MaxServerZoom);
				return FALSE;	/* Loading and stretching is not compatible */
			}
/*	Fix up for anything that moved or re-sized */			
			if (ts->lat != lat || ts->lon != lon
			|| !EqualRect(&ts->rcWin, rcWin))
			{	int width = rcWin->right-rcWin->left, height = rcWin->bottom-rcWin->top;
				int cx = width/2, cy = height/2;
				int t;
				for (t=0; t<ts->Count; t++)
				{	OSM_TILE_POS_S *tp = &ts->Tiles[t];
					SetRect(&tp->rc, rcWin->left+(tp->xw-xt)*256+cx-128+xo,
									rcWin->top+(tp->yw-yt)*256+cy-128+yo,
									rcWin->left+(tp->xw-xt)*256+256+cx-128+xo,
									rcWin->top+(tp->yw-yt)*256+256+cy-128+yo);
				}
				ts->xt=xt; ts->yt=yt; ts->xo=xo; ts->yo=yo;
				ts->lat = lat; ts->lon = lon;
				CopyRect(&ts->rcWin, rcWin);
				if (pUpdated) *pUpdated = TRUE;
			}
/*	If we were loading, see if there's anything new to copy to world */			
			if (ts->StillLoading && ts->TilesFetched != TilesFetched)	/* Maybe some to copy? */
			{	HDC dcCompat, dcWhole, memDC;
				HGDIOBJ oldWhole;
				int ReadyCount = 0, FailCount = 0;

				StringCbPrintfA(Why, sizeof(Why), "FIX [%ld] %s %s",
								(long) ts->Count,
								ts->StillLoading?"Loading":"Done",
								ts->TilesFetched==TilesFetched?"NoNew":"New");
//#ifdef SHOW_INCOMPATIBLE
				TraceLogThread("OSM", FALSE, "%s\n", Why);
//#endif
				ts->StillLoading = FALSE;
				ts->TilesFetched = TilesFetched;
				for (int t=0; t<ts->Count; t++)
				if (!ts->Tiles[t].Tile || ts->Tiles[t].stretch)
				{	OSM_TILE_POS_S *tp = &ts->Tiles[t];
					BOOL Exists = FALSE;
					char *Path = OSMPrepTileFile(sInfo, ts->zoom, tp->x, tp->y, &Exists HERE, FALSE, FALSE);

TraceLogThread("OSMOptimize",FALSE,"Optimize:Tile[%ld]->%s %s %s (%ld %ld)\n",									t, ts->Tiles[t].Tile?"Loaded":"Missing", ts->Tiles[t].stretch?"Stretched":"Normal", Path&&Exists?"READY":"Queued?", ts->Tiles[t].x, ts->Tiles[t].y);

					if (Path)
					{	if (Exists)
						{	OSM_TILE_INFO_S *newTile = OSMGetTileInfo(hwnd, sInfo, ts->zoom, tp->x, tp->y,
																		1, FALSE, FALSE, NULL, Path);
							if (newTile)
							{	if (tp->Tile) OSMFreeTile(tp->Tile);
								tp->Tile = newTile;
								tp->stretch = 0;
								if (!ReadyCount)	/* first one? */
								{	dcCompat = GetDC(hwnd);
									dcWhole = CreateCompatibleDC(dcCompat);
									memDC = CreateCompatibleDC(dcWhole);
									oldWhole = SelectObject(dcWhole,ts->hbm);
if (!oldWhole) TraceLogThread("OSM", TRUE, "OSMIsTileSetCompatible:SelectObject for ts->hbm 0x%lX into dcWhole 0x%lX Failed, Error %ld\n",
										(long) ts->hbm, (long) dcWhole, (long)GetLastError());
								}

								PaintTileToWorld(ts, tp, Dim, dcWhole, memDC, ts->Percent);

								ReadyCount++;
							}
							else
							{	ts->StillLoading++;
								TraceLogThread("OSM", TRUE, "OSMGetTileInfo(%ld %ld %ld) Failed!\n", ts->zoom, tp->x, tp->y);
							}
						} else
						{	ts->StillLoading++;
							free(Path);
						}
					} else ts->StillLoading++;
				}

				if (ReadyCount)
				{	if (pUpdated) *pUpdated = TRUE;

					if (ts->GridSquares) DrawGridSquareLines2(dcWhole, ts);	/* Put the lines back */

					if (!DeleteObject(memDC))
							TraceLogThread("OSM",TRUE,"OSMGetTileSet:Failed to Delete memDC %p error %ld\n", memDC, GetLastError());
					if (oldWhole) SelectObject(dcWhole, oldWhole);
					if (!DeleteObject(dcWhole))
						TraceLogThread("OSM",TRUE,"OSMGetTileSet:Failed to Delete dcWhole %p error %ld\n", dcWhole, GetLastError());
					ReleaseDC(hwnd, dcCompat);
					TraceLogThread("OSMOptimize",FALSE,"ts had %ld READY! %s\n", ReadyCount, ts->StillLoading?"STILL LOADING!":"");
				} else TraceLogThread("OSMOptimize",FALSE,"ts Incompatible with %ld ready? %s\n", ReadyCount, ts->StillLoading?"STILL LOADING!":"");
			} else
				StringCbPrintfA(Why, sizeof(Why), "OK ts[%ld] @ %.6lf %.6lf vs %.6lf %.6lf %s z:%ld=%ld ts(%ld %ld %ld %ld) new(%ld %ld %ld %ld) %s %ld->%ld",
								(long) ts->Count,
								(double) ts->lat, (double) ts->lon,
								(double) lat, (double) lon,
								ts->BuiltTooMany?"BuiltTooMany":"NOT BuiltTooMany",
								(long) ts->zoom, (long) zoom, 
								(long) ts->xs, (long) ts->xe, (long) ts->ys, (long) ts->ye,
								(long) xs, (long) xe, (long) ys, (long) ye,
								ts->StillLoading?"StillLoading":"NOT StillLoading",
								(long) ts->TilesFetched, (long) TilesFetched);
#ifdef SHOW_INCOMPATIBLE
				TraceLogThread("OSM", TRUE, "OSMIsTileSetCompatible:%s\n", Why);
#endif
			return TRUE;
		}
		else StringCbPrintfA(Why, sizeof(Why), "X/Y [%ld] %s %s",
							(long) ts->Count,
							(xs < ts->xs || xe > ts->xe)?"NeedX":"Xok",
							(ys < ts->ys || ye > ts->ye)?"NeedY":"Yok");
	}
	else if (ts->BuiltTooMany)
		StringCbPrintfA(Why, sizeof(Why), "BuiltTooMany [%ld]",
						(long) ts->Count);
	else if (ts->sInfo != sInfo)
		StringCbPrintfA(Why, sizeof(Why), "TS %.*s->%.*s",
						sizeof(ts->sInfo->Name),
						ts->sInfo?ts->sInfo->Name:"*NULL*",
						sizeof(sInfo->Name),
						sInfo?sInfo->Name:"*NULL*");
	else StringCbPrintfA(Why, sizeof(Why), "DZPG [%ld] %s(%s->%s) %s(%ld->%ld) %s(%ld%%->%ld%%) %s",
						(long) ts->Count,
						ts->Dim==Dim?"":"DIM",
						ts->Dim?"Dim":"Bright",
						Dim?"Dim":"Bright",
						ts->zoom==zoom?"":"ZOOM",
						(long) ts->zoom, (long) zoom,
						ts->Percent==Percent?"":"PERCENT",
						(long) ts->Percent, (long) Percent,
						ts->GridSquares==GridSquareLines?"":"GridSQ");
	} else StringCbPrintfA(Why, sizeof(Why), "NULL ts");

	if (hwnd && (!ts || !ts->StillLoading)) OSMFlushTileQueue(FALSE, hwnd);	/* Bump priority of pending requests */

//#ifdef SHOW_INCOMPATIBLE
	TraceLogThread("OSM", TRUE, "OSMIsTileSetCompatible:%s\n", Why);
//#endif
	return FALSE;
}

int OSMPrefetchTiles(HWND hWnd, RECT *rcWin, TILE_SERVER_INFO_S *sInfo, int minzoom, int maxzoom, double lat, double lon, BOOL OnlyCount)
{//	int width = rcWin->right-rcWin->left, height = rcWin->bottom-rcWin->top;
//	int cx = width/2, cy = height/2;
	int xt, yt, xo, yo;
	int xs, xe, ys, ye;
	int Count = 0;
	int zoom;

	OSMCalcTileRanges(rcWin, minzoom, lat, lon, &xt, &yt, &xo, &yo, &xs, &xe, &ys, &ye);

	if (maxzoom > sInfo->MaxServerZoom) maxzoom = sInfo->MaxServerZoom;
	if (maxzoom > MAX_REASONABLE_ZOOM) maxzoom = MAX_REASONABLE_ZOOM;
	if (maxzoom < minzoom) maxzoom = minzoom;
	if (OSMLockQueue(1000))
	{
	for (zoom=minzoom; zoom<=maxzoom; zoom++)
	{	if (OnlyCount)
			Count += (xe-xs+1)*(ye-ys+1);
		else
		{	int x, y;
			for (x=xs; x<=xe; x++)
			{	for (y=ys; y<=ye; y++)
				{	BOOL Queued;
					char *osmTileFile = OSMGetOrQueueTileFile(hWnd, sInfo, zoom, x, y, -(zoom-minzoom)*100-1, FALSE, &Queued);
					if (osmTileFile) free(osmTileFile);	/* Just priming the pump */
					if (Queued) Count++;
				}
			}
		}
		xs = xs*2; xe = xe*2 + 1;
		ys = ys*2; ye = ye*2 + 1;
	}
	OSMUnlockQueue();
	}
	return Count;
}

#ifdef NEVER_DID_WORK_OUT
static void MergeTilesCovered(OSM_TILE_SET_S *ts, int x, int y, int z)
{
/* Low hanging fruit first */
	for (int i=0; i<ARRAYSIZE(ts->TilesCovered); i++)
	{
		if (ts->TilesCovered[i].tileZ == z
		&& ts->TilesCovered[i].tileX == x
		&& ts->TilesCovered[i].tileY == y)
				return;
		if (ts->TilesCovered[i].tileX != -1)	/* In use? */
		{	int dz = z-ts->TilesCovered[i].tileZ;
			int cx=x>>dz, cy=y>>dz;

			if (ts->TilesCovered[i].tileX == cx
			&& ts->TilesCovered[i].tileY == cy)
				return;

			for (int j=1; j<3; j++)	/* 3 is arbitrary */
			if ((ts->TilesCovered[i].tileX>>j) == (cx>>j)
			&& (ts->TilesCovered[i].tileY>>j) == (cy>>j))
			{	ts->TilesCovered[i].tileX >>= j;
				ts->TilesCovered[i].tileY >>= j;
				ts->TilesCovered[i].tileZ -= j;
				return;
			} else TraceLogThread("Tiles", TRUE, "[%ld] >>%ld+%ld %ld %ld != %ld %ld for %ld %ld %ld\n",
								(long) i, (long) dz, (long) j,
								(ts->TilesCovered[i].tileX>>j),
								(ts->TilesCovered[i].tileY>>j),
								(cx>>j), (cy>>j),
								x, y, z);
		}
	}

	for (int i=0; i<ARRAYSIZE(ts->TilesCovered); i++)
	{	if (ts->TilesCovered[i].tileX == -1)
		{	ts->TilesCovered[i].tileX = x;
			ts->TilesCovered[i].tileY = y;
			ts->TilesCovered[i].tileZ = z;
				return;
		}
	}

	TraceLogThread("Tiles", TRUE, "Failed to Cover %ld %ld %ld with %ld %ld %ld and %ld %ld %ld\n",
					x, y, z,
					ts-> TilesCovered[0].tileX, 
					ts-> TilesCovered[0].tileY, 
					ts-> TilesCovered[0].tileZ, 
					ts-> TilesCovered[1].tileX, 
					ts-> TilesCovered[1].tileY, 
					ts-> TilesCovered[1].tileZ);
}
#endif	/* NEVER_DID_WORK_OUT */

BOOL OSMIsTileInSet(OSM_TILE_SET_S *ts, int xt, int yt, int zt)
{	int dz = zt-ts->zoom;
	int cx=xt>>dz, cy=yt>>dz;

	for (int t=0; t<ts->Count; t++)
	{	if (ts->Tiles[t].x == cx
		&& ts->Tiles[t].y == cy)
			return TRUE;
	}
	return FALSE;
}

OSM_TILE_SET_S *OSMGetTileSet(HWND hWnd, RECT *rcWin,
							  TILE_SERVER_INFO_S *sInfo, 
							  BOOL Dim, int zoom,
							  double lat, double lon, int Percent,
							  BOOL GridSquareLines)
{	int width = rcWin->right-rcWin->left, height = rcWin->bottom-rcWin->top;
	int cx = width/2, cy = height/2;
	int x, xs, xe, xt, xo;
	int y, ys, ye, yt, yo;
	OSM_TILE_SET_S *ts = (OSM_TILE_SET_S *) calloc(1,sizeof(*ts));
static	BOOL First = TRUE;
	int BuiltCount = 0;
	int tWidth = (width/256+2)*10+1, tHeight = (height/256+2)*10+1;
#ifdef UNDER_CE
#define BORDER 0
#else
#define BORDER 0	/* This seems to be fine */
#endif
//	int MaxDistance = (int) sqrt((double)(tWidth*tWidth+tHeight*tHeight));
	int Distance = 0, xd=(width/2/256)+1+BORDER, yd=(height/2/256)+1+BORDER;
	int MaxDistance = max(xd,yd);
	__int64 Start = llGetMsec();
static __int64 MaxElapsed = 0;

	if (First)
	{	First = FALSE;
		CloseHandle(CreateThread(NULL, 0, OSMFileMonitor, NULL, 0, NULL));
	}

	sInfo = OSMRegisterTileServer(sInfo, "GetTileSet");

	ts->GridSquares = GridSquareLines;
	ts->Percent = Percent;
	ts->rcWin = *rcWin;
	ts->sInfo = sInfo;
	ts->Dim = Dim;
	ts->zoom = zoom;
	ts->lat = lat;
	ts->lon = lon;
	ts->TilesFetched = TilesFetched;

//	ts->TilesCovered[0].tileX = ts->TilesCovered[0].tileX = ts->TilesCovered[0].tileZ = -1;
//	ts->TilesCovered[1] = ts->TilesCovered[0];

	OSMCalcTileRanges(rcWin, zoom, lat, lon, &xt, &yt, &xo, &yo, &xs, &xe, &ys, &ye, FALSE);

	xs -= BORDER;
	ys -= BORDER;
	xe += BORDER;
	ye += BORDER;

	ts->Size = (xe-xs+1) * (ye-ys+1);
	//if (ts->Size < width/256+2) ts->Size = width/256+2;
	ts->Tiles = (OSM_TILE_POS_S *) calloc(ts->Size+1, sizeof(*ts->Tiles));

/*	Prime the pump by making sure the center tile is local (DON'T queue adjacents) */
#ifdef PRIME_CENTER_TILE
	{	__int64 PrimeStart = llGetMsec();
	static	__int64 PrimeMax = 0;
		char *osmTileFile = OSMGetOrQueueTileFile(hWnd, sInfo, zoom, xt, yt, 1, FALSE);
		if (osmTileFile) free(osmTileFile);	/* Just priming the pump */
		PrimeStart = llMsecSince(PrimeStart,llGetMsec());
		if (PrimeStart > PrimeMax)
		{	TraceLogThread("OSM", TRUE, "OSMGetTileSet: New Prime Max %.0lfmsec for %ld %ld zoom %ld (was %.0lfmsec)\n",
							(double) PrimeStart, (long) xt, (long) yt, (long) zoom, (double) PrimeMax);
			PrimeMax = PrimeStart;
		}
		ts->msPrime = PrimeStart;
	}
#endif

	ts->xt=xt; ts->yt=yt; ts->xo=xo; ts->yo=yo;
	ts->xs=xs; ts->ys=ys; ts->xe=xe; ts->ye=ye;

	ts->tileMinMax.left = xs<<(MAX_OSM_ZOOM-ts->zoom);
	ts->tileMinMax.right = ((xe+1)<<(MAX_OSM_ZOOM-ts->zoom));
	ts->tileMinMax.top = ys<<(MAX_OSM_ZOOM-ts->zoom);
	ts->tileMinMax.bottom = ((ye+1)<<(MAX_OSM_ZOOM-ts->zoom));

	TraceLogThread("OSM", FALSE, "OSMGetTileSet: Zoom %ld Lat %.4lf Lon %.4lf is x %ld %ld %ld y %ld %ld %ld (MaxDistance %ld (%ld->%ld %ld->%ld)\n",
					(long) zoom, (double) lat, (double) lon,
					(long) xs, (long) xt, (long) xe,
					(long) ys, (long) yt, (long) ye,
					(long) MaxDistance,
					(long) width, (long) tWidth, (long) height, (long) tHeight);

	while (/*!ts->BuiltTooMany &&*/ ts->Count < ts->Size && Distance <= MaxDistance)
	{
int tBuilt = 0;
//	for (x=xs; x<=xe; x++)
//	{	for (y=ys; y<=ye; y++)
TraceLogThread("OSM", FALSE, "OSMGetTileSet:Distance[%ld/%ld]\n", Distance, MaxDistance);
	for (int xw=xt-min(xd,Distance); xw<=xt+min(xd,Distance); xw++)
	{	for (int yw=max(ys,max(0,yt-min(yd,Distance))); yw<=min(ye,min(yt+min(yd,Distance),(1<<zoom)-1)); yw++)
		{
			x = xw; y = yw;
			while (x < 0) x += 1<<zoom;
			while (x >= 1<<zoom) x -= (1<<zoom);
			while (y < 0) y += 1<<zoom;
			while (y >= 1<<zoom) y -= (1<<zoom);

			int dx=(x-xt)*10, dy=(y-yt)*10;
			/* Note: 10 tiles diagnonally is 14 */
			int Priority = (int) sqrt((double)(dx*dx+dy*dy));

			int tUsed;
			for (tUsed=0; tUsed < ts->Count; tUsed++)
				if (ts->Tiles[tUsed].xw == xw
				&& ts->Tiles[tUsed].yw == yw)
					break;

			if (tUsed >= ts->Count /*&& Priority == Distance*/)
			{
tBuilt++;
			int t = ts->Count++;
			if (ts->Count <= ts->Size)
			{	//RECT rcTemp;
				SetRect(&ts->Tiles[t].rc, rcWin->left+(xw-xt)*256+cx-128+xo,
											rcWin->top+(yw-yt)*256+cy-128+yo,
											rcWin->left+(xw-xt)*256+256+cx-128+xo,
											rcWin->top+(yw-yt)*256+256+cy-128+yo);
				//MergeTilesCovered(ts, x, y, zoom);
				//if (IntersectRect(&rcTemp, rcWin, &ts->Tiles[t].rc))
				{	BOOL Built = FALSE;
					ts->Tiles[t].xw = xw;
					ts->Tiles[t].yw = yw;
					ts->Tiles[t].x = x;
					ts->Tiles[t].y = y;
					ts->Tiles[t].zoom = zoom;
/* Don't queue adjacent's here as the user is waiting! */
					if ((ts->Tiles[t].Tile = OSMGetTileInfo(hWnd, sInfo, zoom, x, y, Priority, ts->BuiltTooMany, FALSE, &Built)) == NULL)
					{	int szoom=zoom, sx=x, sy=y;
						__int64 sStart = llGetMsec();
						ts->StillLoading++;
						if (szoom)	/* if we've got room to go out! */
						{	for (ts->Tiles[t].stretch=1; --szoom; ts->Tiles[t].stretch++)
							{	sx /= 2; sy /= 2;
								if ((ts->Tiles[t].Tile = OSMGetTileInfo(hWnd, sInfo, szoom, sx, sy,
																	Priority+200*ts->Tiles[t].stretch,
																	ts->BuiltTooMany && szoom>2, FALSE, &Built)) != NULL)
								{	int pow2 = 1<<ts->Tiles[t].stretch;
									int size = 256 / pow2;
									int xoffset = (x%pow2)*size;
									int yoffset = (y%pow2)*size;
									if (szoom >= sInfo->MaxServerZoom)
										ts->StillLoading--; /* It doesn't get any better */
									SetRect(&ts->Tiles[t].rcSource, ts->Tiles[t].Tile->rcBig.left+xoffset, ts->Tiles[t].Tile->rcBig.top+yoffset, ts->Tiles[t].Tile->rcBig.left+xoffset+size, ts->Tiles[t].Tile->rcBig.top+yoffset+size);
#ifdef VERBOSE
									TraceLogThread("OSM", TRUE, "Stetching zoom %ld by %ld for %ld %ld (using %ld at %ld %ld) (%ld %ld to %ld %ld)\n",
												(long) ts->Tiles[t].zoom, (long) ts->Tiles[t].stretch,
												(long) ts->Tiles[t].x, (long) ts->Tiles[t].y,
												(long) szoom, (long) sx, (long) sy,
												(long) ts->Tiles[t].rcSource.left, (long) ts->Tiles[t].rcSource.top,
												(long) ts->Tiles[t].rcSource.right, (long) ts->Tiles[t].rcSource.bottom);
#endif
									break;
								}
							}
						}
						if (ts->Tiles[t].Tile)
							ts->StretchCount++;
						else ts->MissingCount++;
//						if (!szoom)
//							TraceLogThread("OSM", TRUE, "Failed to stretch zoom %ld at %ld %ld\n",
//											(long) ts->Tiles[t].zoom,
//											(long) ts->Tiles[t].x, (long) ts->Tiles[t].y);
						ts->msStretch += llMsecSince(sStart,llGetMsec());
					}
					if (Built)
					{	BuiltCount++;
						if (llMsecSince(Start,llGetMsec()) > 500)
						{	TraceLogThread("OSM", FALSE, "OSMGetTileSet:Built %ld out of %ld (%ldmsec)\n", (long) BuiltCount, (long) ts->Size, (long) llMsecSince(Start,llGetMsec()));
							ts->BuiltTooMany = TRUE;
							// break;
						}
					}
//				} else
//				{	ts->Count--;	/* Didn't need this one! */
				}
			} else
			{	TraceLogThread("OSM", TRUE, "Oops, TileSize[%ld] Overflow at %.4lf %.4lf!\n", (long) ts->Size, (double) lat, (double) lon);
				ts->Count--;
			}
			}
		}
		// if (ts->BuiltTooMany) break;
	}
	TraceLogThread("OSM", tBuilt==0, "OSMGetTileSet:Built %ld at Distance %ld/%ld\n", tBuilt, Distance, MaxDistance);
	Distance++;	/* Go on to the next increment outwards */
	}

	TraceLogThread("OSM", FALSE, "OSMGetTileSet: Zoom %ld Lat %.4lf Lon %.4lf Gave %ld/%ld Tiles Distance %ld/%ld (%ld x %ld for %ld x %ld)\n",
					(long) zoom, (double) lat, (double) lon,
					(long) ts->Count, (long) ts->Size,
					(long) Distance, (long) MaxDistance,
					(long) xd, (long) yd,
					(long) width, (long) height);

//	TraceLogThread("Tiles", TRUE, "Covered %ld %ld %ld +/- %ld with %ld %ld %ld and %ld %ld %ld\n",
//					x, y, zoom, MaxDistance,
//					ts-> TilesCovered[0].tileX, 
//					ts-> TilesCovered[0].tileY, 
//					ts-> TilesCovered[0].tileZ, 
//					ts-> TilesCovered[1].tileX, 
//					ts-> TilesCovered[1].tileY, 
//					ts-> TilesCovered[1].tileZ);

//	TraceLogThread("OSM", TRUE, "Built Zoom %ld Tile Set %p, %ld Loading\n", ts?ts->zoom:0, ts, ts->StillLoading);

	Start = llMsecSince(Start,llGetMsec());
	if (Start > 1000)
		TraceLogThread("Activity", TRUE, "OSMGetTileSet Took %.2lfmsec for %ld/%ld Tiles (%ld Built)%s%s (Max %.2lfmsec)\n",
					(double) Start, (long) ts->Count, (long) ts->Size, (long) BuiltCount,
					ts->BuiltTooMany?" BuiltTooMany":"",
					ts->StillLoading?" STILL LOADING!":"",
					(double) MaxElapsed);
	TraceLogThread("OSM", Start>MaxElapsed, "OSMGetTileSet Took %.2lfmsec for %ld/%ld Tiles (%ld Built)%s%s (Max %.2lfmsec)\n",
					(double) Start, (long) ts->Count, (long) ts->Size, (long) BuiltCount,
					ts->BuiltTooMany?" BuiltTooMany":"",
					ts->StillLoading?" STILL LOADING!":"",
					(double) MaxElapsed);
	if (Start > MaxElapsed) MaxElapsed = Start;
	ts->BuiltCount = BuiltCount;
	ts->msBuilt = Start;

	if (ts->BuiltTooMany && OSMNotifyMsg)
	{	PostMessage(hWnd, OSMNotifyMsg, 0, 0);
	}
#define USE_WORLD_BITMAP
#ifdef USE_WORLD_BITMAP
/*
	Now build a whole-world bitmap for faster painting
*/
	HDC dcCompat = GetDC(hWnd);
	HBITMAP hbmWhole = CreateCompatibleBitmap(dcCompat, (ts->xe-ts->xs+1)*256, (ts->ye-ts->ys+1)*256);
	if (hbmWhole)
	{	HDC dcWhole = CreateCompatibleDC(dcCompat);
		HGDIOBJ oldWhole = SelectObject(dcWhole,hbmWhole);
		if (!oldWhole) TraceLogThread("OSM", TRUE, "OSMGetTileSet:SelectObject for hbmWhole 0x%lX into dcWhole 0x%lX Failed, Error %ld\n",
									(long) hbmWhole, (long) dcWhole, (long) GetLastError());

{	HDC memDC = CreateCompatibleDC(dcWhole);
	__int64 Start = llGetMsec();
	int Painted=0;

	for (int t=0; t<ts->Count; t++)
	{	PaintTileToWorld(ts, &ts->Tiles[t], Dim, dcWhole, memDC, ts->Percent);
		Painted++;
#ifdef OBSOLETE
		RECT rcTarget;
		rcTarget.left = (ts->Tiles[t].xw-ts->xs)*256;
		rcTarget.top = (ts->Tiles[t].yw-ts->ys)*256;
		rcTarget.right = rcTarget.left + 256;
		rcTarget.bottom = rcTarget.top + 256;

		if (ts->Tiles[t].Tile)
		{	HGDIOBJ old = SelectObject(memDC,ts->Tiles[t].Tile->hbmBig);
			if (!old) TraceLogThread("OSM", TRUE, "OSMGetTileSet:SelectObject for Tile[%ld/%ld] hbm 0x%lX Failed, Error %ld\n",
									(long) t, (long) ts->Count, (long) ts->Tiles[t].Tile->hbmBig, GetLastError());

	TraceLogThread("OSM", FALSE, "OSMGetTileSet:Tiles[%ld/%ld] %ld %ld w(%ld %ld) targets %ld %ld -> %ld %ld\n",
			   t, ts->Count,
			   ts->Tiles[t].x, ts->Tiles[t].y,
			   ts->Tiles[t].xw, ts->Tiles[t].yw,
			   rcTarget.left, rcTarget.top, rcTarget.right, rcTarget.bottom);

			Painted++;

			if (Percent > 90)
			{	if (ts->Tiles[t].stretch)
				{	if (!StretchBlt(dcWhole,
							rcTarget.left, rcTarget.top,
							(rcTarget.right-rcTarget.left),
							(rcTarget.bottom-rcTarget.top),
							memDC,
							ts->Tiles[t].rcSource.left, ts->Tiles[t].rcSource.top,
							ts->Tiles[t].rcSource.right - ts->Tiles[t].rcSource.left,
							ts->Tiles[t].rcSource.bottom - ts->Tiles[t].rcSource.top,
							SRCCOPY))
						TraceLogThread("OSM", TRUE, "OSMGetTileSet:StretchBlt Failed on hbm 0x%lX\n", (long) ts->Tiles[t].Tile->hbmBig);

				} else
				{	if (!BitBlt(dcWhole,
							rcTarget.left, rcTarget.top,
							(rcTarget.right-rcTarget.left),
							(rcTarget.bottom-rcTarget.top),
							memDC, ts->Tiles[t].Tile->rcBig.left, ts->Tiles[t].Tile->rcBig.top, SRCCOPY))
						TraceLogThread("OSM", TRUE, "OSMGetTileSet:BitBlt Failed on hbm 0x%lX error %ld\n", (long) ts->Tiles[t].Tile->hbmBig, GetLastError());
				}
			} else
			{	FillRect(dcWhole, &rcTarget, (HBRUSH) GetStockObject(WHITE_BRUSH));
				if (ts->Tiles[t].stretch)
				{
					if (!AlphaBlend(dcWhole,
								rcTarget.left, rcTarget.top,
								(rcTarget.right-rcTarget.left),
								(rcTarget.bottom-rcTarget.top),
						memDC, ts->Tiles[t].rcSource.left, ts->Tiles[t].rcSource.top,
						ts->Tiles[t].rcSource.right - ts->Tiles[t].rcSource.left,
						ts->Tiles[t].rcSource.bottom - ts->Tiles[t].rcSource.top,
						bf))
							TraceLogThread("OSM", TRUE, "OSMGetTileSet:AlphaBlend(stretch) Failed on hbm 0x%lX\n", (long) ts->Tiles[t].Tile->hbmBig);
				} else 
				{
					if (!AlphaBlend(dcWhole,
									rcTarget.left, rcTarget.top,
									(rcTarget.right-rcTarget.left),
									(rcTarget.bottom-rcTarget.top),
						memDC, ts->Tiles[t].Tile->rcBig.left, ts->Tiles[t].Tile->rcBig.top,
						ts->Tiles[t].rc.right - ts->Tiles[t].rc.left,
						ts->Tiles[t].rc.bottom - ts->Tiles[t].rc.top,
						bf))
							TraceLogThread("OSM", TRUE, "OSMGetTileSet:AlphaBlend(straight) Failed on hbm 0x%lX\n", (long) ts->Tiles[t].Tile->hbmBig);
				}
			}
			if (old) SelectObject(memDC, old);
//FrameRect(dcWhole, &rcTarget, (HBRUSH) GetStockObject(BLACK_BRUSH));
		}
		else
		{	TraceLogThread("OSM", TRUE, "OSMGetTileSet:Tile[%ld/%ld] NULL Tile %ld %ld w(%ld %ld)\n",
							(long) t, (long) ts->Count, ts->Tiles[t].x, ts->Tiles[t].y, ts->Tiles[t].xw, ts->Tiles[t].yw);
			FillRect(dcWhole, &rcTarget, (HBRUSH) GetStockObject(WHITE_BRUSH));
		}
#endif
	}
	Start = llMsecSince(Start,llGetMsec());
	TraceLogThread("OSM", Start>=1000, "Prepared %ld/%ld WORLD Tiles (%ld%%) in %.2lf msec\n",
				   (long) Painted, (long) ts->Count, (long) Percent, Start);
	if (!DeleteObject(memDC))
		TraceLogThread("OSM",TRUE,"OSMGetTileSet:Failed to Delete memDC %p error %ld\n", memDC, GetLastError());
	ts->msWorld = Start;
}

		if (GridSquareLines) DrawGridSquareLines2(dcWhole, ts);

		ts->hbm = hbmWhole;
		if (oldWhole) SelectObject(dcWhole, oldWhole);
		if (!DeleteObject(dcWhole))
			TraceLogThread("OSM",TRUE,"OSMGetTileSet:Failed to Delete dcWhole %p error %ld\n", dcWhole, GetLastError());
	} else TraceLogThread("OSM", TRUE, "OSMGetTileSet:CreateCompatibleBitmap failed Error %ld\n", GetLastError());
	ReleaseDC(hWnd, dcCompat);
#endif	/* USE_WORLD_BITMAP */
	return ts;
}

void OSMFlushUnreferencedBigs(HBITMAP hbmProtect=NULL)
{
	for (unsigned int i=0; i<bigCount; i++)
	if (hbmBigs[i] && hbmBigs[i] != hbmProtect)
	{	int t;

		for (t=0; t<CacheCount; t++)
		if (CacheBitmaps[t].Tile)
		if (CacheBitmaps[t].Tile->hbmBig == hbmBigs[i])
			break;

		if (t>=CacheCount)
		{
if (bBigs[i] || cBigs[i])
	TraceLogThread("OSM",TRUE,"Unreferenced hbmBig[%ld] %p has b/c 0x%lX %ld\n", i, hbmBigs[i], bBigs[i], cBigs[i]);
else
{
			if (!DeleteObject(hbmBigs[i]))
				TraceLogThread("OSM",TRUE,"Failed to Delete Bigs[%ld] %p error %ld\n", i, hbmBigs[i], GetLastError());
			hbmBigs[i] = 0;
}
		}
	}
}

void OSMFlushTileCache(int RetainSeconds)
{	int t;
	int Flushed = 0;
	int InUseCount = 0;
	__int64 msNow = llGetMsec();
static __int64 msLastPurge=0;

	if (RetainSeconds && llMsecSince(msLastPurge,msNow)/1000 < RetainSeconds/2)
	{	return;
	} else if (RetainSeconds) TraceLogThread("OSM", FALSE, "OSMFlushTileCache:Running %ld second purge, been %ld seconds since last\n", RetainSeconds, (long) (llMsecSince(msLastPurge,msNow)/1000));

	TraceLogThread("OSM", FALSE, "OSMFlushTileCache:Flushing Cache: Use:%ld/%ld Size:%ld/%ld Age:%ld Seconds",
						(long) CacheInUse, (long) CacheMaxInUse,
						(long) CacheCount, (long) CacheSize,
						(long) RetainSeconds);

	for (t=0; t<CacheCount; t++)
	if (CacheBitmaps[t].Tile)
	{
		if (!CacheBitmaps[t].UseCount
		&& (!RetainSeconds
		|| llMsecSince(CacheBitmaps[t].msUse,msNow)/1000 > RetainSeconds))
		{
#ifdef VERBOSE
			TraceLogThread("OSM", FALSE, "OSMFlushTileCache:hbm[%ld/%ld] hbm 0x%lX Tile %p %ld seconds old\n", (long) t, (long) CacheCount, (long) CacheBitmaps[t].Tile->hbmBig, CacheBitmaps[t].Tile, (long)(llMsecSince(CacheBitmaps[t].msUse,msNow)/1000));
#endif
			if (CacheBitmaps[t].Tile->hbmBig)
			{	if (CacheBitmaps[t].Tile->inBig == -1)
				{	if (!DeleteObject(CacheBitmaps[t].Tile->hbmBig))
						TraceLogThread("OSM",TRUE,"OSMFlushTileCache:Failed to Delete Cache[%ld] hbmBig %p error %ld\n", t, CacheBitmaps[t].Tile->hbmBig, GetLastError());
				} else
				{	unsigned int iBig = HIWORD(CacheBitmaps[t].Tile->inBig);
					unsigned int nBig = LOWORD(CacheBitmaps[t].Tile->inBig);
					if (iBig < bigCount)
					if (nBig < BIG_X*BIG_Y)
					{	int nMask = 1<<nBig;
						if (bBigs[iBig]&nMask)
						{	bBigs[iBig] &= ~nMask;
							cBigs[iBig]--;
						} else TraceLogThread("OSMFlush",TRUE,"OsmFlushTileCache:cBigs[%ld]=%ld NOT decremented, bBigs[%ld]=0x%lX [%ld]0x%lX NOT Set\n",
								iBig, cBigs[iBig], iBig, bBigs[iBig], nBig, nMask);
					} else TraceLogThread("OSMFlush",TRUE,"OsmFlushTileCache:cBigs[%ld]=%ld NOT decremented, nBig[%ld] vs %ld->%ld\n",
								iBig, cBigs[iBig], nBig, 0, BIG_X*BIG_Y);
					else TraceLogThread("OSMFlush",TRUE,"OsmFlushTileCache:cBigs NOT decremented, iBig[%ld] vs %ld->%ld\n",
								iBig, 0, bigCount);
				}
			}

			free(CacheBitmaps[t].Tile);
			CacheBitmaps[t].Tile = NULL;
			Flushed++;
		} else	/* Retain because In Use or not old enough */
		{	CacheBitmaps[InUseCount++] = CacheBitmaps[t];
#ifdef VERBOSE
			TraceLogThread("OSM", FALSE, "OSMFlushTileCache:CacheBitmaps[%ld/%ld] hbm 0x%lX Tile %p UseCount=%ld Age %ld/%ld, Keeping %ld\n", (long) t, (long) CacheCount, (long) CacheBitmaps[t].Tile->hbmBig, CacheBitmaps[t].Tile, (long) CacheBitmaps[t].UseCount, (long)(llMsecSince(CacheBitmaps[t].msUse,msNow)/1000), (long) RetainSeconds, (long) InUseCount);
#endif
		}
	} else TraceLogThread("OSM", TRUE, "OSMFlushTileCache:CacheBitmaps[%ld/%ld] has NULL Tile?\n", (long) t, (long) CacheCount);

	if (InUseCount)
	{	CacheCount = InUseCount;
	} else
	{	CacheCount = CacheSize = 0;
		free(CacheBitmaps);
		CacheBitmaps = NULL;
		TraceLogThread("OSM", TRUE, "OSMFlushTileCache: Freed CacheBitmaps Memory!\n");
	}
//	if (InUseCount != CacheInUse)
//	{	TraceLogThread("OSM", TRUE, "CacheBitmaps %ld Remaining InUse, %ld Calculated, Fixing Calculation\n", (long) InUseCount, (long) CacheInUse);
//		CacheInUse = InUseCount;
//	}

	TraceLogThread("OSM", FALSE, "Flushed %ld from Cache: Use:%ld/%ld Size:%ld/%ld",
						(long) Flushed,
						(long) CacheInUse, (long) CacheMaxInUse,
						(long) CacheCount, (long) CacheSize);

	OSMFlushUnreferencedBigs();

	msLastPurge = msNow;	/* Remember for purge throttling */
}

int OSMGetCacheSize(void)
{	return CacheSize;
}

void OSMFreeTile(OSM_TILE_INFO_S *Tile)
{	int t, f=0;
	for (t=0; t<CacheCount; t++)
	{	if (CacheBitmaps[t].Tile == Tile)
		{	if (!--CacheBitmaps[t].UseCount)
				CacheInUse--;
//			break;
			f++;
		}
	}
	if (f>1) TraceLogThread("OSM", TRUE, "Tile %p In Set %p %ld Times!\n", f);
}

typedef struct OSM_RECENT_TILE_S
{	BOOL Failed;	/* TRUE if download or load failed, FALSE for simple checks */
	__int64 msWhen;
	TILE_SERVER_INFO_S *sInfo;
	int zoom, x, y;
} OSM_RECENT_TILE_S;

static HANDLE hmtxRecent = NULL;
static size_t RecentCount = 0;
static size_t RecentSize = 0;
static OSM_RECENT_TILE_S *RecentTiles = NULL;

/*	Returns TRUE if already there, otherwise inserts it and returns FALSE */
static BOOL OSMCheckOrInsertRecentTile(TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y, BOOL Failed, BOOL Remember)
{	BOOL Result = FALSE;

	if (!hmtxRecent) hmtxRecent = CreateMutex(NULL, FALSE, NULL);
	if (WaitForSingleObject(hmtxRecent, 5000) == WAIT_OBJECT_0)
	{	size_t i;
		__int64 msNow = llGetMsec();
		__int64 msExpired = msNow - 10000;	/* Hold off for 10 seconds */

		for (i=0; i<RecentCount; i++)
		{	if (RecentTiles[i].msWhen < msExpired)
				RecentTiles[i--] = RecentTiles[--RecentCount];
			else if (RecentTiles[i].y == y
			&& RecentTiles[i].x == x
			&& RecentTiles[i].zoom == zoom
			&& RecentTiles[i].sInfo == sInfo
			&& RecentTiles[i].Failed == Failed)
				break;
		}
		if (i >= RecentCount)
		{
//			TraceLogThread("OSM", TRUE, "OSMInsertRecentTile:%s %ld %ld %ld %s\n",
//							Remember?"Insert":"Check",
//							(long) zoom, (long) x, (long) y, Failed?(Remember?"FAILED":"Failure"):"Checked");

			if (Remember && (x || y || zoom))
			{	i = RecentCount++;

				if (RecentCount > RecentSize)
				{	RecentSize += 16;
					RecentTiles = (OSM_RECENT_TILE_S *) realloc(RecentTiles,sizeof(*RecentTiles)*RecentSize);
				}

				RecentTiles[i].x = x;
				RecentTiles[i].y = y;
				RecentTiles[i].zoom = zoom;
				RecentTiles[i].sInfo = sInfo;
				RecentTiles[i].Failed = Failed;
				RecentTiles[i].msWhen = msNow;	/* Reference the current entry */
			}
		} else
		{	Result = TRUE;
			if (Remember) RecentTiles[i].msWhen = msNow;	/* Reference the current entry */
		}
		ReleaseMutex(hmtxRecent);
	} else TraceLogThread("OSM", TRUE, "OSMInsertRecentTile:WaitForSingleObject(hmtxRecent)(0x%lX) FAILED!\n", hmtxRecent);

	return Result;
}

static void OSMFlushRecentTiles(TILE_SERVER_INFO_S *sInfo)
{	OSMCheckOrInsertRecentTile(sInfo,0,0,0,FALSE,FALSE);	/* Spin the list expiring old ones */
}

static BOOL OSMTileRecentlyChecked(TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y)
{	return OSMCheckOrInsertRecentTile(sInfo, zoom, x, y, FALSE, TRUE);
}

static BOOL OSMTileRecentlyFailed(TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y, BOOL ReallyFailed)
{	return OSMCheckOrInsertRecentTile(sInfo, zoom, x, y, TRUE, ReallyFailed);
}

OSM_TILE_INFO_S *OSMGetTileInfo(HWND hWnd, TILE_SERVER_INFO_S *sInfo, int zoom, int xin, int yin, int Priority, BOOL CacheOnly, BOOL Adjacent, BOOL *pBuilt, char *osmTileFile)
{	OSM_TILE_INFO_S *Tile = NULL;
	HBITMAP hbm = 0;
	__int64 oldestTime = 0;
	int t, oldestT = 0;

	int x = xin, y = yin;

	if (x < 0) x += 1<<zoom;
	if (x >= 1<<zoom) x -= (1<<zoom);
	if (y < 0 || y >= 1<<zoom)
	{	if (osmTileFile) free(osmTileFile);
		return NULL;
	}

	if (!OSMTileInRange(sInfo, zoom, x, y))
	{	TraceLogThread("OSM", FALSE, "OSMGetTileInfo: Zoom %ld x %ld y %ld OUT OF RANGE!\n",
					(long) zoom, (long) x, (long) y);
		if (osmTileFile) free(osmTileFile);
		return NULL;
	}

	if (pBuilt) *pBuilt = FALSE;

	for (t=0; t<CacheCount; t++)
	{	if (CacheBitmaps[t].Tile->hWnd == hWnd
		&& CacheBitmaps[t].Tile->tzoom == zoom
		&& CacheBitmaps[t].Tile->tx == xin
		&& CacheBitmaps[t].Tile->ty == yin
		&& CacheBitmaps[t].Tile->sInfo == sInfo)
		{	if (!CacheBitmaps[t].UseCount++)
			{	CacheInUse++;
				if (CacheInUse > CacheMaxInUse)
				{	CacheMaxInUse = CacheInUse;
					TraceLogThread("OSM", FALSE, "OSMGetTileInfo:New CacheMaxInUse=%ld\n", CacheMaxInUse);
				}
			}
			CacheBitmaps[t].msUse = llGetMsec();
			if (osmTileFile) free(osmTileFile);
			return CacheBitmaps[t].Tile;
		}
		if (CacheBitmaps[t].UseCount == 0
		&& (!oldestTime || CacheBitmaps[t].msUse < oldestTime))
		{	oldestTime = CacheBitmaps[t].msUse;
			oldestT = t;
		}
	}

	if (CacheOnly)
	{	if (osmTileFile) free(osmTileFile);
		return Tile;	/* Don't bother trying to build one */
	}

	if (!osmTileFile) osmTileFile = OSMGetOrQueueTileFile(hWnd, sInfo, zoom, x, y, Priority, Adjacent);
	if (osmTileFile)
	{	RECT rcBig; int inBig;
		TraceLogThread("OSM", FALSE, "OSMGetTileInfo:Building Bitmap for %ld %ld %ld from %s\n",
					(long) zoom, (long) x, (long) y, osmTileFile);

		if ((hbm = OSMMakeTileBitmap(hWnd, osmTileFile, &rcBig, &inBig)) != 0)
		{	if (pBuilt) *pBuilt = TRUE;
			if (oldestTime
			&& (CacheCount >= CacheMaxInUse*CACHE_INUSE_FACTOR
				|| llMsecSince(oldestTime,llGetMsec())/1000 > 5*60))
			{	t = oldestT;
				if (CacheBitmaps[t].Tile->hbmBig)
				{	if (CacheBitmaps[t].Tile->inBig == -1)
					{	if (!DeleteObject(CacheBitmaps[t].Tile->hbmBig))
								TraceLogThread("OSM",TRUE,"OSMGetTileInfo:Failed to Delete Cache[%ld] hbmBig %p error %ld\n", t, CacheBitmaps[t].Tile->hbmBig, GetLastError());
					} else 
					{	unsigned int iBig = HIWORD(CacheBitmaps[t].Tile->inBig);
						unsigned int nBig = LOWORD(CacheBitmaps[t].Tile->inBig);
						if (iBig < bigCount)
						if (nBig < BIG_X*BIG_Y)
						{	int nMask = 1<<nBig;
							if (bBigs[iBig]&nMask)
							{	bBigs[iBig] &= ~nMask;
								cBigs[iBig]--;
//TraceLogThread("OSM",TRUE,"OSMGetTileInfo:Free[%ld][%ld]0x%lX gives cBigs[%ld]=%ld, bBigs[%ld]=0x%lX\n", iBig, nBig, nMask, iBig, cBigs[iBig], iBig, bBigs[iBig]);
							} else TraceLogThread("OSM",TRUE,"OSMGetTileInfo:cBigs[%ld]=%ld NOT decremented, bBigs[%ld]=0x%lX [%ld]0x%lX NOT Set\n",
									iBig, cBigs[iBig], iBig, bBigs[iBig], nBig, nMask);
						} else TraceLogThread("OSM",TRUE,"OSMGetTileInfo:cBigs[%ld]=%ld NOT decremented, nBig[%ld] vs %ld->%ld\n",
									iBig, cBigs[iBig], nBig, 0, BIG_X*BIG_Y);
						else TraceLogThread("OSM",TRUE,"OSMGetTileInfo:cBigs NOT decremented, iBig[%ld] vs %ld->%ld\n",
									iBig, 0, bigCount);
					}
				}
				TraceLogThread("OSM", FALSE, "OSMGetTileInfo:Free CacheBitmaps[%ld/%ld/%ld] %ld %ld %ld hbm 0x%lX Tile %p\n",
								(long) t, (long) CacheCount, (long) CacheSize,
								(long) CacheBitmaps[t].Tile->tzoom,
								(long) CacheBitmaps[t].Tile->tx,
								(long) CacheBitmaps[t].Tile->ty,
								(long) CacheBitmaps[t].Tile->hbmBig,
								CacheBitmaps[t].Tile);
				free(CacheBitmaps[t].Tile);
				CacheBitmaps[t].Tile = NULL;
				OSMFlushUnreferencedBigs(hbm);
			} else 
			{	t = CacheCount++;
				if (CacheCount > CacheSize)
				{	CacheSize += 16;
					TraceLogThread("OSM", TRUE, "OSMGetTileInfo:Growing Cache To %ld (Max %ld)\n", (long) CacheSize, (long) CacheMaxInUse*CACHE_INUSE_FACTOR);
					CacheBitmaps = (OSM_CACHE_TILE_S *) realloc(CacheBitmaps, CacheSize*sizeof(*CacheBitmaps));
				}
			}
			CacheBitmaps[t].Tile = Tile = (OSM_TILE_INFO_S *) calloc(1,sizeof(*Tile));
			CacheBitmaps[t].UseCount = 1;
			CacheBitmaps[t].msUse = llGetMsec();
			Tile->hWnd = hWnd;
			Tile->sInfo = sInfo;
			Tile->tzoom = zoom;
			Tile->tx = xin;
			Tile->ty = yin;
			Tile->hbmBig = hbm;
			Tile->rcBig = rcBig;
			Tile->inBig = inBig;
		TraceLogThread("OSM", FALSE, "OSMGetTileInfo:Built Bitmap for %ld %ld %ld from %s to %p %ld %ld\n",
					(long) zoom, (long) x, (long) y, osmTileFile,
					Tile->hbmBig, Tile->rcBig.left, Tile->rcBig.top);
//			Tile->min.lat = tiley2lat(y,zoom);
//			Tile->max.lat = tiley2lat(y+1,zoom);
//			Tile->min.lon = tilex2long(x,zoom);
//			Tile->max.lon = tilex2long(x+1,zoom);
			CacheInUse++;
			if (CacheInUse > CacheMaxInUse)
			{	CacheMaxInUse = CacheInUse;
				TraceLogThread("OSM", FALSE, "OSMGetTileInfo:New CacheMaxInUse=%ld\n", CacheMaxInUse);
			}
			TraceLogThread("OSM", FALSE, "OSMGetTileInfo:New CacheBitmaps[%ld/%ld/%ld] %ld %ld %ld hbm 0x%lX Tile %p\n",
							(long) t, (long) CacheCount, (long) CacheSize,
							(long) CacheBitmaps[t].Tile->tzoom,
							(long) CacheBitmaps[t].Tile->tx,
							(long) CacheBitmaps[t].Tile->ty,
							(long) CacheBitmaps[t].Tile->hbmBig,
							CacheBitmaps[t].Tile);
		}
		else
		{	TraceLogThread("OSM", FALSE, "OSMGetTileInfo:Failed to Build Bitmap %ld %ld %ld from %s\n",
						(long) zoom, (long) x, (long) y, osmTileFile);
			OSMTileRecentlyFailed(sInfo, zoom, x, y, TRUE);
		}
		free(osmTileFile);
	}
//	else TraceLogThread("OSM", FALSE, "OSMGetTileInfo:osmTileFile NULL for %ld %ld %ld\n", (long) zoom, (long) x, (long) y);

	return Tile;
}

static HBITMAP OSMLoadPngOrJpeg(HWND hWnd, char *osmTileFile)
{	HBITMAP hbm = NULL;

#if defined(USE_PNG_IMAGE)
	if (!hbm)
	{	PNG_INFO_S *Info = pngLoadFromFile(osmTileFile);
		if (Info)
		{	int Size = Info->m_width * Info->m_height * 4;
			HDC hdcWin = GetDC(hWnd);

			if (Info->m_width != 256 || Info->m_height != 256)
				TraceLogThread("OSM", TRUE, "OSMTile(%s) Size %ld x %ld != 256x256\n",
							osmTileFile, Info->m_width, Info->m_height);

#ifdef UNDER_CE
			hbm = CreateCompatibleBitmap(hdcWin, Info->m_width, Info->m_height);
			if (!hbm)
			{	TraceLogThread("OSM", TRUE, "CreateCompatibleBitmap(%ld,%ld) Failed For %s (GetLastError=%ld)\n",
							(long) Info->m_width, (long) Info->m_height,
							osmTileFile, (long) GetLastError());
				OSMFlushTileCache(0);
				hbm = CreateCompatibleBitmap(hdcWin, Info->m_width, Info->m_height);
				if (!hbm)
					TraceLogThread("OSM", TRUE, "CreateCompatibleBitmap(%ld,%ld) RETRY Failed For %s (GetLastError=%ld)\n",
							(long) Info->m_width, (long) Info->m_height,
							osmTileFile, (long) GetLastError());
			}
#else
			// setup bitmap info  
			BITMAPINFO bmi;        // bitmap header 
			// zero the memory for the bitmap info 
			ZeroMemory(&bmi, sizeof(BITMAPINFO));

			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = Info->m_width;
			bmi.bmiHeader.biHeight = Info->m_height;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;         // four 8-bit components 
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biSizeImage = Info->m_width *Info->m_height * 4;

			// create our DIB section and select the bitmap into the dc 
			VOID *pvBitsAlpha;          // pointer to DIB section 
			hbm = CreateDIBSection(hdcWin, &bmi, DIB_RGB_COLORS, &pvBitsAlpha, NULL, 0x0);
#endif
			if (hbm)
			{
#ifdef VERBOSE
				BITMAP bm = {0};
				GetObject(hbm, sizeof(bm), &bm);
				TraceLogThread("OSM-bpp", FALSE, "OSMMakeTileBitmap:New bmp(0x%lX) %ld x %ld (t:%ld %ldx%ld w:%ld pl:%ld bpp:%ld)\n",
							(long) hbm,
							(long) Info->m_width,
							(long) Info->m_height,
							bm.bmType, bm.bmWidth, bm.bmHeight,
							bm.bmWidthBytes, bm.bmPlanes, bm.bmBitsPixel);
#endif
				SetBitmapBits(hbm, Size, Info->m_bgra);
			}
			ReleaseDC(hWnd, hdcWin);
			pngDestroy(Info);
		}
	}
#endif
#if defined(USE_JPEG_IMAGE)
	if (!hbm)
	{	hbm = jpgLoadFromFile(hWnd, osmTileFile);
	}
#endif
	return hbm;
}

HBITMAP OSMMakeTileBitmap(HWND hWnd, char *osmTileFile, RECT *prc, int *piBig)
{	HBITMAP hbm = 0;

	*piBig = -1;
	SetRect(prc, 0, 0, 256, 256);

	if (osmTileFile)
	{	__int64 Start = llGetMsec();
#ifdef USE_SHELL_IMAGE
		int Size = (strlen(osmTileFile)+1)*sizeof(TCHAR);
		TCHAR *uFile = (TCHAR *) malloc(Size);
		StringCbPrintf(uFile, Size, TEXT("%S"), osmTileFile);
		hbm = SHLoadImageFile(uFile);
		free(uFile);

		if (!hbm)
		{	HBITMAP hbmBad = hbm;
			HDC hdc = GetDC(hWnd);
			TraceLogThread("OSM", TRUE, "ShLoadImageFile(%s) (GetLastError=%ld)\n",
							osmTileFile, (long) GetLastError());
			hbm = CreateCompatibleBitmap(hdc, 256, 256);
			if (!hbm)
			{	TraceLogThread("OSM", TRUE, "CreateCompatibleBitmap(%ld,%ld) Failed For %s (GetLastError=%ld) %sFlushing Cache!\n",
							(long) 256, (long) 256,
							osmTileFile, (long) GetLastError(),
							CacheCount>CacheMaxInUse?"":"NOT ");
				if (CacheCount > CacheMaxInUse) OSMFlushTileCache(0);
				hbm = CreateCompatibleBitmap(hdc, 256, 256);
				if (!hbm)
				{	TraceLogThread("OSM", TRUE, "CreateCompatibleBitmap(%ld,%ld) RETRY Failed For %s (GetLastError=%ld)\n",
							(long) 256, 256,
							osmTileFile, (long) GetLastError());
				} else 
				{	DeleteObject(hbm);
					TraceLogThread("OSM", TRUE, "CreateCompatibleBitmap(%ld,%ld) RETRY SUCCEEDED!\n",
							(long) 256, 256);
				}
			} else
			{	DeleteObject(hbm);
				TraceLogThread("OSM", TRUE, "CreateCompatibleBitmap(%ld,%ld) Succeeded after ShLoadImage Failed On %s (GetLastError=%ld), %sFlushing Cache!\n",
							(long) 256, (long) 256,
							osmTileFile, (long) GetLastError(),
							CacheCount>CacheMaxInUse?"":"NOT ");
				if (CacheCount > CacheMaxInUse) OSMFlushTileCache(0);
			}
			ReleaseDC(hWnd, hdc);
			hbm = hbmBad;
		}
		if (hbm) { }	/* Success doesn't do anything */
#elif defined(USE_PNG_IMAGE) || defined(USE_JPEG_IMAGE)
		hbm = OSMLoadPngOrJpeg(hWnd, osmTileFile);
		if (hbm) { }	/* Success doesn't do anything */
#else
#error No Image Support!
#endif
		else
		{	OSMFileCount--;
			RemoveFile(osmTileFile);
			TraceLogThread("OSM", TRUE, "OSMMakeTileBitmap:Removed Corrupt OSM File %s\n", osmTileFile);
		}

		if (hbm)	/* If we got one, try caching it in a "big" bitmap */
		{	HDC hdcWin = GetDC(hWnd);

			if (iBig >=0 && iBig < bigCount)
			if (nBig < 0 || nBig >= BIG_X*BIG_Y
			|| bBigs[iBig]&(1<<nBig))
			{	for (nBig=0; nBig<BIG_X*BIG_Y; nBig++)
				{	if (!(bBigs[iBig]&(1<<nBig)))
					{	break;
					}
				}
//				if (nBig >= BIG_X*BIG_Y) TraceLogThread("OSM", TRUE, "OSMMakeTileBitmap:cBig[%ld]=%ld No BIT in 0x%lX\n", iBig, cBigs[iBig], bBigs[iBig]);
//				else if (nBig) TraceLogThread("OSM", TRUE, "OSMMakeTileBitmap:Recycling [%ld][%ld] b/cBig=%lX %ld\n", iBig, nBig, bBigs[iBig], cBigs[iBig]);
			}

			if (nBig < 0 || nBig >= BIG_X*BIG_Y
			|| iBig < 0 || iBig >= bigCount)
			{	for (iBig=0; iBig<bigCount; iBig++)
				{	if (!hbmBigs[iBig])	/* No bitmap? */
					{	break;
					} else if (cBigs[iBig] < BIG_X*BIG_Y)	/* Empty hole? */
					{	break;
					}
				}
				if (iBig >= bigCount)
				{	iBig = bigCount++;
					bBigs = (unsigned long *) realloc(bBigs,sizeof(*bBigs)*bigCount);
					cBigs = (unsigned int *) realloc(cBigs,sizeof(*cBigs)*bigCount);
					hbmBigs = (HBITMAP*) realloc(hbmBigs,sizeof(*hbmBigs)*bigCount);
					nBig = 0;
					bBigs[iBig] = 0;
					cBigs[iBig] = 0;
					hbmBigs[iBig] = NULL;
TraceLogThread("OSM", TRUE, "OSMMakeTileBitmap:Expanded bigCount to %ld\n", bigCount);
				}
/* Moved iBig, find a new nBig */
				for (nBig=0; nBig<BIG_X*BIG_Y; nBig++)
				{	if (!(bBigs[iBig]&(1<<nBig)))
					{	break;
					}
				}
				if (nBig >= BIG_X*BIG_Y) TraceLogThread("OSM", TRUE, "OSMMakeTileBitmap:cBig[%ld]=%ld No BIT in 0x%lX\n", iBig, cBigs[iBig], bBigs[iBig]);
				else if (nBig) TraceLogThread("OSM", TRUE, "OSMMakeTileBitmap:Recycling [%ld][%ld] b/cBig=%lX %ld\n", iBig, nBig, bBigs[iBig], cBigs[iBig]);
			}

			if (iBig >= 0 && iBig < bigCount && !hbmBigs[iBig])
			{	hbmBigs[iBig] = CreateCompatibleBitmap(hdcWin, 256*BIG_X, 256*BIG_Y);
				if (!hbmBigs[iBig])
				{	TraceLogThread("OSM", TRUE, "OSMMakeTileBitmap:Failed to create hbmBig[%ld] error %ld\n", iBig, GetLastError());
				}
				if (nBig) TraceLogThread("OSM", TRUE, "OSMMakeTileBitmap:New hbmBigs[%ld] but nBig=%ld (b/cBig=%lX %ld)\n", iBig, nBig, bBigs[iBig], cBigs[iBig]);
			}

			if (iBig >= 0 && iBig < bigCount)
			if (nBig >= 0 && nBig < BIG_X*BIG_Y)
			if (hbmBigs[iBig])
			{	HDC hdcSrc = CreateCompatibleDC(hdcWin);
				HDC hdcDest = CreateCompatibleDC(hdcWin);
				HGDIOBJ hOrgDestMap=0, hOrgSrcMap=0;

				hOrgSrcMap = SelectObject(hdcSrc, hbm);

				hOrgDestMap = SelectObject(hdcDest, hbmBigs[iBig]);

				prc->left = (nBig%BIG_X)*256;
				prc->top = (nBig/BIG_X)*256;
				prc->right = prc->left + 256;
				prc->bottom = prc->top + 256;

				if (!BitBlt(hdcDest, prc->left, prc->top, 256, 256,
						hdcSrc, 0, 0, SRCCOPY))
					TraceLogThread("OSM", TRUE, "OSMMakeTileBitmap:BitBlt Failed on hbm %p to [%ld]%p %ld %ld Error %ld\n", hbm, iBig, hbmBigs[iBig], prc->left, prc->top, GetLastError());

				SelectObject(hdcDest, hOrgDestMap);
				SelectObject(hdcSrc, hOrgSrcMap);
				DeleteDC(hdcDest);
				DeleteDC(hdcSrc);
				if (!DeleteObject(hbm))
					TraceLogThread("OSM",TRUE,"OSMMakeTileBitmap:Failed to Delete hbm %p error %ld\n", hbm, GetLastError());

				hbm = hbmBigs[iBig];
				bBigs[iBig] |= 1<<nBig;
				cBigs[iBig]++;
				*piBig = (iBig<<16) | nBig;
//TraceLogThread("OSM", TRUE, "hbmBig[%ld][%ld] %p b/cBig 0x%lX %ld returning 0x%lX\n", iBig, nBig, hbm, bBigs[iBig], cBigs[iBig], *piBig);
				while (++nBig < BIG_X*BIG_Y)
					if (!(bBigs[iBig] & (1<<nBig)))	/* free slot? */
						break;
				if (nBig >= BIG_X*BIG_Y)	/* Find an available one */
				for (iBig=0; iBig<bigCount; iBig++)
				{	if (hbmBigs[iBig]		/* Has a bitmap */
					&& cBigs[iBig] < BIG_X*BIG_Y)	/* With empty hole? */
					{	break;
					}
				}
			}
else TraceLogThread("OSM", TRUE, "hbmBig[%ld] NULL, Caching individual bitmap %p\n", iBig, hbm);
else TraceLogThread("OSM", TRUE, "Invalid nBig[%ld] %ldx%ld=%ld, Caching individual Tile bitmap %p\n", iBig, BIG_X, BIG_Y, BIG_X*BIG_Y, hbm);
else TraceLogThread("OSM", TRUE, "Invalid iBig[%ld] bigCount[%ld], Caching individual Tile bitmap %p\n", iBig, bigCount, hbm);
			ReleaseDC(hWnd, hdcWin);
		}

		TraceLogThread("OSM", FALSE, "OSMMakeTileBitmap(%s) Gives [%lX] %p %ld %ld\n",
					osmTileFile, *piBig, hbm, prc->left, prc->top);

		FetchTimes.reads++;
		FetchTimes.read += llGetMsec()-Start;
	}

	return hbm;
}

char *OSMPrepTileFile(TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y, BOOL *Exists CALLER, char *CheckPath, BOOL CreateDirs)
{	int Len = strlen(sInfo->Path)+33*3+4+1;
	char *Path;
	TCHAR *tPath;
	HANDLE Handle;

	if (!OSMFetchEnabled) CreateDirs = FALSE;

	if (!OSMTileInRange(sInfo, zoom, x, y)) return NULL;

	if (!CheckPath) CheckPath = sInfo->Path;

	Path=(char *)_malloc_dbg(sizeof(*Path)*Len, _NORMAL_BLOCK, file, line);
	tPath=(TCHAR *)_malloc_dbg(sizeof(*tPath)*Len, _NORMAL_BLOCK, file, line);
	if (sInfo->SingleDigitDirectories)
	{	sprintf(Path,"%.*s%ld/%ld/%ld.png", sizeof(sInfo->Path), CheckPath, (long) zoom, (long) x, (long) y);
		StringCbPrintf(tPath,sizeof(*tPath)*Len,TEXT("%.*S%ld/%ld/%ld.png"), sizeof(sInfo->Path), CheckPath, (long) zoom, (long) x, (long) y);
	} else
	{	sprintf(Path,"%.*s%02ld/%ld/%ld.png", sizeof(sInfo->Path), CheckPath, (long) zoom, (long) x, (long) y);
		StringCbPrintf(tPath,sizeof(*tPath)*Len,TEXT("%.*S%02ld/%ld/%ld.png"), sizeof(sInfo->Path), CheckPath, (long) zoom, (long) x, (long) y);
	}
#ifdef VERBOSE
	TraceLogThread("OSM", TRUE, "OSMPrepTileFile[%s]@%p:%s SingleDigit gives %S\n",
					sInfo->Name, sInfo, sInfo->SingleDigitDirectories?"Using":"NOT Using", tPath);
#endif

#ifdef UNDER_CE
	Handle = CreateFile(tPath, GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#else
	Handle = CreateFile(tPath, GENERIC_READ | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#endif

	if (Handle != INVALID_HANDLE_VALUE)
	{
#ifdef FUTURE
		FILETIME AccessTime;
#ifdef UNDER_CE
		GetCurrentFT(&AccessTime);
#else
		GetSystemTimeAsFileTime(&AccessTime);
		if (!SetFileTime(Handle, NULL, &AccessTime, NULL))
			TraceLogThread("OSM", TRUE, "SetFileTime(%S) Failed with %ld\n", tPath, GetLastError());
#endif
#endif
		CloseHandle(Handle);
		*Exists = TRUE;
	} else
	{static char *First = NULL;
	static HANDLE hmtxFirst = NULL;
		DWORD Error = GetLastError();
//		if (Error != ERROR_FILE_NOT_FOUND
//		&& Error != ERROR_PATH_NOT_FOUND)
#ifdef VERBOSE
TraceLogThread("OSM", TRUE, "CreateFile(%S) Failed with %ld\n", tPath, Error);
#endif

		if (!hmtxFirst) hmtxFirst = CreateMutex(NULL, FALSE, NULL);

		sprintf(Path, "%.*s", sizeof(sInfo->Path), CheckPath);

		if (WaitForSingleObject(hmtxFirst, 5000) == WAIT_OBJECT_0)
		{	if (*Path && (!First || strcmp(Path,First)))
			{	if (First) free(First);
				First = _strdup(Path);
				if (CreateDirs) MakeDirectory(Path);
			}
			ReleaseMutex(hmtxFirst);
		} else TraceLogThread("OSM", TRUE, "OSMPrepTileFile:WaitForSingleObject(hmtxFirst)(0x%lX) FAILED!\n", hmtxFirst);

		if (sInfo->SingleDigitDirectories)
			sprintf(Path+strlen(Path),"%ld",(long) zoom);
		else sprintf(Path+strlen(Path),"%02ld",(long) zoom);
		if (CreateDirs) MakeDirectory(Path);
		sprintf(Path+strlen(Path),"/%ld",(long) x);
		if (CreateDirs) MakeDirectory(Path);
		sprintf(Path+strlen(Path),"/%ld.png",(long) y);
		*Exists = FALSE;
	}
	free(tPath);
	return Path;
}

char *OSMGetOrFetchTileFile(int thread, HWND hWnd, TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y, BOOL *pFetched=NULL)
{	BOOL Exists;
	char *Path;
	
	if (pFetched) *pFetched = FALSE;

	if (!OSMTileInRange(sInfo, zoom, x, y)) return NULL;

	if (OSMTileRecentlyFailed(sInfo, zoom, x, y, FALSE))
	{	TraceLogThread("OSM", FALSE, "OSMGetOrFetchTileFile:%ld %ld %ld Recently Failed\n",
						(long) zoom, (long) x, (long) y);
		return NULL;
	}

	Path=OSMPrepTileFile(sInfo, zoom, x, y, &Exists HERE);

	if (!Path)
		TraceLogThread("OSM", FALSE, "OSMGetOrFetchTileFile:Problems with %ld %ld %ld\n",
						(long) zoom, (long) x, (long) y);
	else if (!Exists)
	{	if (!OSMFetchEnabled)
		{	free(Path);
			Path = NULL;
		} else
		{	char *URL = (char *) malloc(strlen(sInfo->URLPrefix)+33*3+80);
			BOOL DidOne = FALSE;
			char *f, *u = URL;

			for (f=sInfo->URLPrefix; *f; )
			if (*f == '%')
			{	BOOL Inverse = FALSE;
				if (f[1] == '!')	/* %!Y inverts Y via (2^Z-Y-1) */
				{	Inverse = TRUE;
					f++;	/* Skip the ! */
				}
				switch (f[1])
				{	case '%': *u++ = f[1]; break;	/* Copy a % */
					case 'z': case 'Z':
						u += sprintf(u,"%ld",(long) zoom);
						DidOne = TRUE;
						break;
					case 'x': case 'X':
						u += sprintf(u,"%ld",(long) Inverse?((1<<zoom)-x-1):x);
						DidOne = TRUE;
						break;
					case 'y': case 'Y':
						u += sprintf(u,"%ld",(long) Inverse?((1<<zoom)-y-1):y);
						DidOne = TRUE;
						break;
					default:		/* Copy unrecognized %A through intact */
						if (Inverse) *u++ = '%';	/* f->! */
						*u++ = *f;
						*u++ = f[1];
				}
				f += 2;	/* Skip the %x */
			} else *u++ = *f++;	/* Copy this character over */
			if (!DidOne)	/* Old format without %'s */
			{	if (u > URL && u[-1] != '\\' && u[-1] != '/') *u++ = '/';
				sprintf(u, "%ld/%ld/%ld.png", (long) zoom, (long) x, (long) y);
			} else *u++ = '\0';	 /* Null terminate the result */

			//Sleep(1000);

			if (!httpGet(thread, hWnd, sInfo->Server, sInfo->Port, URL, Path))
			{	TraceLogThread("OSM", TRUE, "OSMGetOrFetchTileFile:Failed To Fetch zoom %ld @ %ld %ld into %s\n",
							(long) zoom, (long) x, (long) y, Path);
				free(Path);
				Path = NULL;
				OSMTileRecentlyFailed(sInfo, zoom, x, y, TRUE);	/* Mark it failed */
			} else
			{	TraceLogThread("OSM", FALSE, "OSMGetOrFetchTileFile:Fetched zoom %ld @ %ld %ld into %s\n",
							(long) zoom, (long) x, (long) y, Path);
				if (pFetched) *pFetched = TRUE;
			}
			free(URL);
		}
	}
#ifdef VERBOSE
	else TraceLogThread("OSM", FALSE, "OSMGetOrFetchTileFile:Got %ld %ld %ld in %s\n",
						(long) zoom, (long) x, (long) y, Path);
#endif
	return Path;
}

static HANDLE hmtxQueue = NULL;
static HANDLE hevQueue = NULL;
typedef struct TILE_QUEUE_S
{	TILE_SERVER_INFO_S *sInfo;
	int zoom;
	int x;
	int y;
	int Priority;
	HWND hWnd;
} TILE_QUEUE_S;
int TQCount = 0;
int TQNext = 0;
int TQSize = 0;
TILE_QUEUE_S *TQs;
#ifdef UNDER_CE
#define THREAD_COUNT 3
#else
#define THREAD_COUNT 3
#endif
char *TQStatus[THREAD_COUNT+1] = {0};

int OSMGetQueueStats(int *Count, int *Servers)
{	if (Count) *Count = TQCount;
	if (Servers) *Servers = THREAD_COUNT;
	return TQCount - TQNext;
}

char *OSMGetQueueStatus(void)
{static	char Combined[THREAD_COUNT*32];
	int i;
	*Combined = '\0';
	for (i=0; i<THREAD_COUNT+1; i++)
	if (TQStatus[i])
	{	sprintf(Combined+strlen(Combined),"%s%ld-%s",i?((i&1)?"\t":"\n"):"",(long)i,TQStatus[i]);
	}
	return Combined;
}

#define PRIORITY_ADJUST 10000
#define FLUSH_PRIORITY (PRIORITY_ADJUST*5)

void OSMFlushTileQueue(BOOL ClearAll, HWND hwnd, BOOL WindowGone)
{	if (TQCount)	/* If there's even a queue to deal with */
	{	DWORD Status;
		int PriAdjust = WindowGone?FLUSH_PRIORITY:PRIORITY_ADJUST;

		if ((Status=WaitForSingleObject(hmtxQueue, 1000)) == WAIT_OBJECT_0)
		{	if (ClearAll)
				TQNext = TQCount;
			else
			{	int t;
				for (t=TQNext; t<TQCount; t++)
					if (TQs[t].Priority >= 0
					&& (!hwnd || TQs[t].hWnd == hwnd))
						TQs[t].Priority += PriAdjust;
			}
			ReleaseMutex(hmtxQueue);
		} else TraceLogThread("OSM", TRUE, "WaitForSingleObject(Flush)(0x%lX) Failed With %ld (%ld)\n", hmtxQueue, (long) Status, (long) GetLastError());
	}
}

/* Thread 0 gets priority only */
/* all but Thread (THREAD_COUNT-1) ignore PreFetch */
static TILE_QUEUE_S *OSMDequeueTile(int thread)
{	DWORD Status;
	TILE_QUEUE_S *Tile = NULL;

	TQStatus[thread] = "Deque";
	if ((Status=WaitForSingleObject(hmtxQueue, 1000)) == WAIT_OBJECT_0)
	{	unsigned long FlushCount = 0;
		unsigned long h, hwndCount = 0;
		HWND *hwnds = 0;

		ResetEvent(hevQueue);
		while (TQNext < TQCount
		&& TQs[TQNext].Priority > FLUSH_PRIORITY)	/* Bumped it 5 times, forget about it! */
		{	FlushCount++;
#ifdef VERBOSE
TraceLogThread("OSM", TRUE, "Flushing Priority %ld (%ld %ld %ld)\n",
			  (long) TQs[TQNext].Priority,
			  (long) TQs[TQNext].zoom,
			  (long) TQs[TQNext].x,
			  (long) TQs[TQNext].y);
#endif
			if (OSMNotifyMsg && TQs[TQNext].hWnd)
			{	for (h=0; h<hwndCount; h++)
				{	if (hwnds[h] == TQs[TQNext].hWnd)
						break;
				}
				if (h >= hwndCount)
				{	h = hwndCount++;
					hwnds = (HWND *) realloc(hwnds, sizeof(*hwnds)*hwndCount);
					hwnds[h] = TQs[TQNext].hWnd;
				}
			}
			TQNext++;
		}
		if (FlushCount)
		{	TraceLogThread("OSM", TRUE, "Flushed %lu Queue Entries (%lu hwnds Notified)\n", (unsigned long) FlushCount, (unsigned long) hwndCount);
			if (hwndCount)
			{	for (h=0; h<hwndCount; h++)
					PostMessage(TQs[TQNext].hWnd, OSMNotifyMsg, 0, 0);
				if (hwnds) free(hwnds);
			}
		}
		if (TQNext < TQCount)
		{
			if (OSMMinMBFree)
			{	unsigned __int64 Free;
				if (OSMGetFreeSpace(TQs[TQNext].sInfo, &Free))
				{	if (Free/1024/1024 < OSMMinMBFree)
					{	TraceLogThread("OSM", FALSE, "OSMDequeueTile:[%s] Free %ldMB < %ld, Suspending Fetch\n",
									TQs[TQNext].sInfo->Path,
									(long) Free, (long) OSMMinMBFree);
						ReleaseMutex(hmtxQueue);
						return NULL;
					}
				}
			}

			if (thread != (THREAD_COUNT-1)
			&& TQs[TQNext].Priority < -1)
			{	TraceLogThread("OSM", FALSE, "OSMDequeueTile:thread[%ld/%ld] NOT pre-fetching priority %ld\n",
								(long) thread, (long) THREAD_COUNT,
								(long) TQs[TQNext].Priority);
				ReleaseMutex(hmtxQueue);
				return NULL;
			}

			if (thread == 0
			&& (TQs[TQNext].Priority < 0 || TQs[TQNext].Priority >= PRIORITY_ADJUST))
			{	TraceLogThread("OSM", FALSE, "OSMDequeueTile:thread[%ld/%ld] NOT Fetching non-priority %ld\n",
								(long) thread, (long) THREAD_COUNT,
								(long) TQs[TQNext].Priority);
				ReleaseMutex(hmtxQueue);
				return NULL;
			}

			Tile = (TILE_QUEUE_S *) malloc(sizeof(*Tile));
			*Tile = TQs[TQNext++];
			TraceLogThread("OSM", FALSE, "DeQueue[%ld/%ld/%ld] zoom %ld @ %ld %ld Priority %ld\n",
						 (long) TQNext, (long) TQCount, (long) TQSize,
						 (long) Tile->zoom, (long) Tile->x, (long) Tile->y, (long) Tile->Priority);
		} else
		{	if (TQs) free(TQs);
			TQs = NULL;
//			TraceLogThread("OSM", FALSE, "DeQueue[%ld/%ld/%ld] is EMPTY!\n", (long) TQNext, (long) TQCount, (long) TQSize);
			TQNext = TQCount = TQSize = 0;
		}
		ReleaseMutex(hmtxQueue);
	} else TraceLogThread("OSM", TRUE, "WaitForSingleObject(DeQueue)(0x%lX) Failed With %ld (%ld)\n", hmtxQueue, (long) Status, (long) GetLastError());
	return Tile;
}

static BOOL OSMQueuePrefetch(HWND hWnd, TILE_SERVER_INFO_S *sInfo, int nx, int ny, int nz, int np)
{	BOOL Result = FALSE;
	if (OSMTileInRange(sInfo, nz, nx, ny))
	{	if (!OSMTileRecentlyChecked(sInfo, nz, nx, ny))
		{	char *Temp = OSMGetOrQueueTileFile(hWnd, sInfo, nz, nx, ny, np, FALSE, &Result);
			if (Temp) free(Temp);
			else TraceLogThread("OSM", FALSE, "%s Adjacent Prefetch for %ld %ld %ld Priority %ld\n",
							Temp?"Unnecessary":(Result?"Queued":"Skipped"),
							(long) (nz), (long) (nx), (long) (ny), (long) np);
		}
//		else TraceLogThread("OSM", FALSE, "Redundant Adjacent Prefetch for %ld %ld %ld Priority %ld\n",
//							(long) (nz), (long) (nx), (long) (ny), (long) np);
	}
//	else TraceLogThread("OSM", FALSE, "OutOfRange Adjacent Prefetch for %ld %ld %ld Priority %ld\n",
//						(long) (nz), (long) (nx), (long) (ny), (long) np);
	return Result;
}

static int OSMQueueAdjacentTiles(HWND hWnd, TILE_SERVER_INFO_S *sInfo, int x, int y, int zoom, int Priority)
{	int Queued = 0;

#define OFF1 10	/* sqrt(10^2+0^2) */
#define OFFD 14	/* sqrt(10^2+10^2) */
	if (zoom <= MAX_REASONABLE_ZOOM)	/* No Prefetches deeper than reasonable */
	{	if (OSMQueuePrefetch(hWnd,sInfo,x-1,y,zoom,Priority+100+OFF1)) Queued++;
		if (OSMQueuePrefetch(hWnd,sInfo,x+1,y,zoom,Priority+100+OFF1)) Queued++;
		if (OSMQueuePrefetch(hWnd,sInfo,x,y-1,zoom,Priority+100+OFF1)) Queued++;
		if (OSMQueuePrefetch(hWnd,sInfo,x,y+1,zoom,Priority+100+OFF1)) Queued++;
		if (zoom > 1) if (OSMQueuePrefetch(hWnd,sInfo,x/2,y/2,zoom-1,Priority+200)) Queued++;
		if (zoom > 2) if (OSMQueuePrefetch(hWnd,sInfo,x/4,y/4,zoom-2,Priority+300)) Queued++;
		if (Priority < 2)	/* Only drill down for immediate adjacency */
		{	if (OSMQueuePrefetch(hWnd,sInfo,2*x,2*y,zoom+1,Priority+400)) Queued++;
			if (OSMQueuePrefetch(hWnd,sInfo,2*x+1,2*y,zoom+1,Priority+400)) Queued++;
			if (OSMQueuePrefetch(hWnd,sInfo,2*x,2*y+1,zoom+1,Priority+400)) Queued++;
			if (OSMQueuePrefetch(hWnd,sInfo,2*x+1,2*y+1,zoom+1,Priority+400)) Queued++;
		}
	}
	return Queued;
}

#ifdef UNDER_CE
DWORD OSMQueueServer(LPVOID pvParam)
#else
DWORD WINAPI OSMQueueServer(LPVOID pvParam)
#endif
{	int thread = (int) pvParam;
	char *MyName = (char*)malloc(44);

	StringCbPrintfA(MyName, 44, "OSMQueue(%ld)", (long) thread);
	SetTraceThreadName(MyName);	/* Yep, it leaks */

	for (;;)
	{	TILE_QUEUE_S *Tile;
		TQStatus[thread] = "WaitEvt";
		WaitForSingleObject(hevQueue, 5*60*1000);	/* Just to clean up sometimes */
//		TraceLogThread("OSM", FALSE, "QueueServer Running!  hmtxQueue=%lu hevQueue=%lu\n", (unsigned long) hmtxQueue, (unsigned long) hevQueue);
		ResetEvent(hevQueue);
		while ((Tile=OSMDequeueTile(thread)) != NULL)
		{	BOOL Fetched = FALSE;
			char *Path = OSMGetOrFetchTileFile(thread, Tile->hWnd, Tile->sInfo, Tile->zoom, Tile->x, Tile->y, &Fetched);
			if (Tile->hWnd && OSMNotifyMsg)
			{	PostMessage(Tile->hWnd, OSMNotifyMsg, Tile->zoom, 0);
				TraceLogThread("OSM", FALSE, "Notified zoom %ld @ %ld %ld Priority %ld\n",
						(long) Tile->zoom, (long) Tile->x, (long) Tile->y, (long) Tile->Priority);
			}
			if (Path) free(Path);
			if (Path && !Fetched) TraceLogThread("OSM", TRUE, "Redundant (Prefetch?) zoom %ld @ %ld %ld Priority %ld\n",
						(long) Tile->zoom, (long) Tile->x, (long) Tile->y, (long) Tile->Priority)
;
/*
	When we get a tile, queue up the next outer zoom
*/
#ifdef OLD_WAY
			if (Tile->zoom > MIN_OSM_ZOOM)
			{	BOOL Queued;
				char *Temp = OSMGetOrQueueTileFile(Tile->hWnd, Tile->sInfo, Tile->zoom-1, Tile->x/2, Tile->y/2, -1, FALSE, &Queued);
				if (Temp) free(Temp);
				if (Queued) TraceLogThread("OSM", FALSE, "%s zoom out Prefetch for %ld %ld %ld\n",
									Temp?"Unnecessary":(Queued?"Queued":"Skipped"),
									(long) Tile->zoom-1, (long) Tile->x/2, (long) Tile->y/2);
			}
#else
			if (Tile->Priority < 100	/* Fetched for screen? */
			&& Tile->Priority > 0)		/* And NOT a prefetch! */
			{	int Queued = OSMQueueAdjacentTiles(Tile->hWnd, Tile->sInfo, Tile->x, Tile->y, Tile->zoom, Tile->Priority);
				if (Queued) TraceLogThread("OSM", FALSE, "Queued %ld Prefetch(s) for %ld %ld %ld\n",
									(long) Queued,
									(long) Tile->zoom, (long) Tile->x, (long) Tile->y);
			}
#endif
			free(Tile);
		}
	}
}

static int OSMCompareQueuePriority(const void *One, const void *Two)
{	TILE_QUEUE_S *Left = (TILE_QUEUE_S *) One;
	TILE_QUEUE_S *Right = (TILE_QUEUE_S *) Two;
	int lp = Left->Priority;
	int rp = Right->Priority;

	if (lp == rp) return 0;

	if (lp < 0 && rp > 0)	/* Negatives are last */
		return 1;
	if (lp > 0 && rp < 0)	/* Positives are first */
		return -1;

	if (lp < 0) lp = -lp;	/* Abosolute values now */
	if (rp < 0) rp = -rp;

	if (lp < rp)
		return -1;
	if (lp > rp)
		return 1;

	return 0;
}

BOOL OSMLockQueue(DWORD msWaitTime)
{	DWORD Status;
	if (hmtxQueue == NULL)
	{	int i;
		hmtxQueue = CreateMutex(NULL, FALSE, NULL);
		hevQueue = CreateEvent(NULL, TRUE, FALSE, NULL);
		for (i=0; i<THREAD_COUNT; i++)
		{	CloseHandle(CreateThread(NULL, 0, OSMQueueServer, (LPVOID) i, 0, NULL));
		}
	}
	Status = WaitForSingleObject(hmtxQueue, msWaitTime);
	return Status == WAIT_OBJECT_0;
}

void OSMUnlockQueue(void)
{	ReleaseMutex(hmtxQueue);
}

static BOOL OSMQueueFetchTile(HWND hWnd, TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y, int Priority)
{	DWORD Status;
	BOOL Queued = FALSE;
	BOOL NeedSort = FALSE;

	if (!OSMFetchEnabled) return FALSE;

	if (hmtxQueue == NULL)
	{	if (OSMLockQueue(100))	/* Prime the pump */
			OSMUnlockQueue();
	}
	if ((Status=WaitForSingleObject(hmtxQueue, 1000)) == WAIT_OBJECT_0)
	{	int t;
		for (t=0; t<TQCount; t++)
		{	if (TQs[t].zoom == zoom
			&& TQs[t].x == x
			&& TQs[t].y == y
			&& TQs[t].sInfo == sInfo)
			{	if (Priority > 0	/* Negatives don't bother shuffling or replacing */
				&& (TQs[t].Priority > Priority
					|| TQs[t].Priority < 0))	/* Positives replace negatives */
				{	TQs[t].Priority = Priority;
					NeedSort = TRUE;
				}
				if (hWnd) TQs[t].hWnd = hWnd;
				break;
			}
		}
		if (t >= TQCount)
		{	t = TQCount++;
			if (TQCount > TQSize)
			{	TQSize += 8;
				TQs = (TILE_QUEUE_S *) realloc(TQs, TQSize*sizeof(*TQs));
			}
			TQs[t].sInfo = sInfo;
			TQs[t].zoom = zoom;
			TQs[t].x = x;
			TQs[t].y = y;
			TQs[t].Priority = Priority;
			TQs[t].hWnd = hWnd;
			NeedSort = (Priority>=0);
			Queued = TRUE;
			TraceLogThread("OSM", FALSE, "Queue[%ld/%ld/%ld] %ld zoom %ld @ %ld %ld Priority %ld%s\n",
							 (long) TQNext, (long) TQCount, (long) TQSize, (long) t,
							 (long) TQs[t].zoom, (long) TQs[t].x, (long) TQs[t].y, (long) TQs[t].Priority,
							 NeedSort&&TQCount-TQNext>1?" QSORT":"");
		}
#ifdef VERBOSE
		else TraceLogThread("OSM", FALSE, "Redundant[%ld] zoom %ld @ %ld %ld\n",
							(long) t, (long) zoom, (long) x, (long) y);
#endif
		if (NeedSort && TQCount-TQNext > 1)
		{	qsort(&TQs[TQNext], TQCount-TQNext, sizeof(TQs[0]), 
OSMCompareQueuePriority);

#ifdef PRIORITY_PRINT
{	int q, c=0, p;
	for (q=TQNext; q<TQCount; q++)
	{	if (!c) p = TQs[q].Priority;
		if (p != TQs[q].Priority)
		{	TraceLogThread("OSM", FALSE, "Queue[%ld] %ld @ %ld\n", (long) TQCount, (long) c, (long) p);
			p = TQs[q].Priority;
			c = 1;
		} else c++;
	}
	if (c) TraceLogThread("OSM", FALSE, "Queue[%ld] %ld @ %ld\n", (long) TQCount, (long) c, (long) p);
}
#endif

		}

		ReleaseMutex(hmtxQueue);
		SetEvent(hevQueue);	/* Notify thread */
	} else TraceLogThread("OSM", TRUE, "WaitForSingleObject(Queue)(0x%lX) Failed with %ld (%ld)\n", hmtxQueue, Status, (long) GetLastError());
	return Queued;
}

char *OSMGetOrQueueTileFile(HWND hWnd, TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y, int Priority, BOOL Adjacent, BOOL *pQueued)
{	BOOL Exists;
	char *Path;

	if (pQueued) *pQueued = FALSE;

	if (!OSMTileInRange(sInfo, zoom, x, y)) return NULL;

	Path=OSMPrepTileFile(sInfo, zoom, x, y, &Exists HERE);
	if (!Path)
		TraceLogThread("OSM", TRUE, "OSMGetOrQueueTileFile:Problems with zoom %ld @ %ld %ld\n",
						(long) zoom, (long) x, (long) y);
	else if (!Exists)
	{	BOOL Queued = OSMQueueFetchTile(hWnd, sInfo, zoom, x, y, Priority);
		free(Path);
		Path = NULL;
		if (pQueued) *pQueued = Queued;
	}
#ifdef VERBOSE
	else TraceLogThread("OSM", FALSE, "OSMGetOrQueueTileFile:Got zoom %ld @ %ld %ld in %s\n",
						(long) zoom, (long) x, (long) y, Path);
#endif

	if (Adjacent)
	{	OSMQueueAdjacentTiles(hWnd, sInfo, x, y, zoom, Priority);
	}

	return Path;
}

#ifdef FOR_REFERENCE_ONLY
int PNGAPI
png_sig_cmp(png_bytep sig, png_size_t start, png_size_t num_to_check)
{
   png_byte png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
   if (num_to_check > 8)
      num_to_check = 8;
   else if (num_to_check < 1)
      return (-1);

   if (start > 7)
      return (-1);

   if (start + num_to_check > 8)
      num_to_check = 8 - start;

   return ((int)(png_memcmp(&sig[start], &png_signature[start], num_to_check)));
}
#endif

char *httpGetBuffer(HWND hWnd, char *Host, long Port, char *URL, int *pLen, char *Agent, BOOL ForceProgress, char **pError)
{
	struct sockaddr_in lclserver = {0};
	struct sockaddr_in *pserver = &lclserver;
	int mysocket;
	long timeout;
	char *sendbuf, *recvbuf;
	int n, recvlen, recvsize, length, ContentLength=0, TotalLength=0;
	int Success = 0;
	HWND hwndProgress = NULL;
	__int64 ProgressTime = ForceProgress?0:(llGetMsec()+100);	/* Fire up the progress window at 2 seconds */
static int initialized = 0;

TraceLogThread("OSM", FALSE, "httpGetBuffer(%s) From %s:%ld\n", URL, Host, (long) Port);

	if (!initialized)
	{	sock_init();
		initialized = 1;
	}

	pserver->sin_family      = AF_INET;
	pserver->sin_port        = htons((u_short)Port);
	pserver->sin_addr.s_addr = inet_addr(Host);	/* argv[1] = hostname */
	if (pserver->sin_addr.s_addr == (u_long) -1)
	{	struct hostent *hostnm = gethostbyname(Host);
		if (!hostnm)
		{	TraceLogThread("OSM", TRUE, "httpGetBuffer:gethostbyname(%s) Failed\n", Host);
			if (pError) *pError = "gethostbyname() Failed";
			return NULL;
		}
		pserver->sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);
	}
	{	char *ipaddr = inet_ntoa(pserver->sin_addr);
		TraceLogThread("OSM", FALSE, "Resolved(%s)=>%s\n",Host, ipaddr?ipaddr:"NULL");
	}

	if ((mysocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{	TraceLogThread("OSM", TRUE, "httpGetBuffer:socket() Failed\n");
		if (pError) *pError = "socket() Failed";
		return NULL;
	}

	if (connect(mysocket, (struct sockaddr *)pserver, sizeof(*pserver)) < 0)
	{	soclose(mysocket);
		TraceLogThread("OSM", TRUE, "httpGetBuffer:connect(%s:%ld) Failed\n", Host, (long) Port);
		if (pError) *pError = "connect() Failed";
		return NULL;
	}

	sendbuf = (char *) malloc(strlen(URL)+strlen(Host)+strlen(Agent)+256);
	length = sprintf(sendbuf, "GET %s HTTP/1.0\r\nAccept: */*\r\nHost: %s\r\nConnection: close\r\nContent-Length: 0\r\nUser-Agent: %s\r\n\r\n", URL, Host, Agent);

	n=send(mysocket, sendbuf, length, 0);
	free(sendbuf);

	if (n != (int)length)
	{	soclose(mysocket);
		TraceLogThread("OSM", TRUE, "httpGetBuffer:send(%ld) Failed\n", (long) length);
		if (pError) *pError = "send() Failed";
		return NULL;
	}

	recvlen = 0;
	recvsize = 8192;
	recvbuf = (char *) malloc(recvsize+1);

	do
	{	unsigned long Available = 0;
		timeout = 15000;	/* 15 second timeout */
		if (hWnd && !hwndProgress && TotalLength && llGetMsec() >= ProgressTime)
		{	RECT rc;
			GetWindowRect(hWnd, &rc);
			hwndProgress = CreateWindow(
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
							hWnd, 			/*Parent HWND*/
							(HMENU) 1, 	/*The Progress Bar's ID*/
							g_hInstance,		/*The HINSTANCE of your program*/ 
							NULL);			/*Parameters for main window*/
			if (!hwndProgress) TraceLogThread("OSM", TRUE, "CreateWindow(Progress) Failed with %ld\n", GetLastError());
			else
			{	TraceLogThread("OSM", TRUE, "Progress window created %p @ %ld %ld %ld %ld\n", hwndProgress, (long) rc.left, (long) rc.top, (long) rc.right, (long) rc.bottom);
				SendMessage(hwndProgress, PBM_SETRANGE32, 0, TotalLength);
#ifdef FUTURE
/*Create a Static Label control*/

hwndLabel = CreateWindow(
                        TEXT("STATIC"),                   /*The name of the static control's class*/
                        TEXT("Label 1"),                  /*Label's Text*/
                        WS_CHILD | WS_VISIBLE | SS_LEFT,  /*Styles (continued)*/
                        0,                                /*X co-ordinates*/
                        0,                                /*Y co-ordinates*/
                        50,                               /*Width*/
                        25,                               /*Height*/
                        hwnd,                             /*Parent HWND*/
                        (HMENU) ID_MYPROGRESS,            /*The Progress Bar's ID*/
                        hInstance,                        /*The HINSTANCE of your program*/ 
                        NULL);                            /*Parameters for main window*/

Using the Label

    Setting Label's Text

To set the text of your Label (static control), use the SendMessage() function with the WM_SETTEXT message.

/*Setting the Label's text
/*You may need to cast the text as (LPARAM)*/

SendMessage(    hwndLabel , 	/*HWND*/        /*Label*/
                WM_SETTEXT,  	/*UINT*/        /*Message*/
                NULL,           /*WPARAM*/      /*Unused*/
       (LPARAM) TEXT("Hello"));  /*LPARAM*/      /*Text*/
#endif



			}
			ProgressTime = llGetMsec() + 99999999;	/* Don't try again */
		}
		if (hwndProgress) SpinMessages(hWnd);
		while ((n=ioctlsocket(mysocket,FIONREAD,&Available))==0 && Available <= 0 && timeout > 0)
		{	__int64 selectStart = llGetMsec();
			struct timeval tmo;
			fd_set fdRead;

			FD_ZERO(&fdRead);
			FD_SET(mysocket, &fdRead);
			tmo.tv_sec = timeout/1000; tmo.tv_usec = (timeout%1000) * 1000;	/* uSec */
			if (hwndProgress) SpinMessages(hWnd);
			n = select(mysocket+1, &fdRead, NULL, NULL, &tmo);
			if (hwndProgress) SpinMessages(hWnd);
			timeout -= (long) (llGetMsec() - selectStart);
			if (n > 0)
			{	if (FD_ISSET(mysocket, &fdRead))
				{	if ((n=ioctlsocket(mysocket,FIONREAD,&Available))!=0 || Available <= 0)
					{	TraceLogThread("OSM", TRUE, "httpGetBuffer:ioctl(%ld)=%ld (%ld) or Available=%ld timeout %ld left\n",
									(long) mysocket, (long) n, (long) sock_errno(), (long) Available, (long) timeout);
						Available = 1;	/* Readable means recv() won't block */
					} else if (Available) TraceLogThread("OSM", FALSE, "httpGetBuffer:readable %ld has %ld Available timeout %ld left\n", mysocket, Available, (long) timeout);
					n = 0;	/* make it look successful */
					break;
				}
			} else if (n < 0)
				TraceLogThread("OSM", TRUE, "httpGetBuffer:select(%ld) return %ld timeout:%ld\n", (long) mysocket, (long) n, (long) timeout);
			else TraceLogThread("OSM", TRUE, "httpGetBuffer:select(%ld) timeout %ld left\n", (long) mysocket, (long) timeout);
		}
		if (n)	/* Errors mean we're outa here */
		{	TraceLogThread("OSM", TRUE, "httpGetBuffer:n=%ld, timeout=%ld GONE!\n", (long) n, (long) timeout);
			break;
		} else if (Available)
		{
TraceLogThread("OSM", FALSE, "httpGetBuffer:Got %ld Available with %ldmsec left\n", (long) Available, (long) timeout);
			if (hwndProgress) SpinMessages(hWnd);
			if ((n=recv(mysocket, recvbuf+recvlen, recvsize-recvlen, 0)) > 0)
			{	char *c, *e;
				if (!recvlen)	/* first buffer? */
				{	int p;
					for (p=0; p<n; p++)
					{	if (recvbuf[p]=='\r')
						{	if (recvbuf[p+1]=='\n'
							&& recvbuf[p+2]=='\r' && recvbuf[p+3]=='\n')
							{	p += 4;
								break;
							}
						} else if (!isprint(recvbuf[p]&0xff) && recvbuf[p] != '\n')
							break;
					}
					TraceLogThread("OSM", FALSE, "httpGetBuffer:Read %ld (%ld printable) Bytes of %.*s\n", (long) n, (long) p, (int) p, recvbuf);
				}

				recvlen += n;
				recvbuf[recvlen] = 0;	/* Null terminate for safety */

				if (hwndProgress)
				{
#ifndef UNDER_CE
					COLORREF bar = GetScaledRGColor(recvlen, 0, TotalLength);
					SendMessage(hwndProgress, PBM_SETBARCOLOR, 0, bar);
#endif
					SendMessage(hwndProgress, PBM_SETPOS, recvlen, 0);
				}

				if (recvlen >= recvsize)
				{	recvsize += 1024;
					recvbuf = (char *) realloc(recvbuf, recvsize+1);
				}
				if (!ContentLength && (c = strstr(recvbuf,"Content-Length:")) != NULL)
				{	c += 15;	/* strlen("Content-Length:") */
					ContentLength = strtol(c,&e,10);
TraceLogThread("OSM", FALSE, "httpGetBuffer:Content-Length: %ld Have %ld/%ld\n", (long) ContentLength, (long) recvlen, (long) recvsize);
				}
				if (!ContentLength && (c = strstr(recvbuf,"content-length:")) != NULL)
				{	c += 15;	/* strlen("content-length:") */
					ContentLength = strtol(c,&e,10);
TraceLogThread("OSM", FALSE, "httpGetBuffer:content-length: %ld Have %ld/%ld\n", (long) ContentLength, (long) recvlen, (long) recvsize);
				}
				if (ContentLength && !TotalLength && (c=strstr(recvbuf,"\r\n\r\n")) != NULL)
				{	long HeaderLength = (c+4)-recvbuf;
					TotalLength = ContentLength + HeaderLength;
TraceLogThread("OSM", FALSE, "httpGetBuffer:Header %ld Content %ld Total %ld Got %ld/%ld\n", (long) HeaderLength, (long) ContentLength, (long) TotalLength, (long) recvlen, (long) recvsize);
					if (TotalLength+1 >= recvsize)
					{	recvsize = TotalLength + 1;
						recvbuf = (char *) realloc(recvbuf, recvsize+1);
					}
				}
			} else	/* Errors mean we're done */
			{	TraceLogThread("OSM", TRUE, "httpGetBuffer:recv(%ld) Error %ld (%ld), timeout %ld\n", (long) mysocket, (long) n, (long) sock_errno(), (long) timeout);
				break;
			}
		}
	} while (timeout>0 && (!TotalLength || recvlen < TotalLength));
	TraceLogThread("OSM", FALSE, "httpGetBuffer:All done, timeout:%ld recvlen %ld / %ld Total\n", (long) timeout, (long) recvlen, (long) TotalLength);
	soclose(mysocket);

	if (hwndProgress)
	{
#ifndef UNDER_CE
		COLORREF bar = GetScaledRGColor(recvlen, 0, TotalLength);
		SendMessage(hwndProgress, PBM_SETBARCOLOR, 0, bar);
#endif
		SendMessage(hwndProgress, PBM_SETPOS, recvlen, 0);
	}

	if (recvlen && (!TotalLength || recvlen >=TotalLength))
	{
//#define KEEP_HEADER
#ifndef KEEP_HEADER
		for (n=0; n<=recvlen-4; n++)
		{	if (recvbuf[n] == '\r' && recvbuf[n+1] == '\n'
			&& recvbuf[n+2] == '\r' && recvbuf[n+3] == '\n')
				break;
		}
		if (n<=recvlen-4)
		{	n += 4;	/* Skip over \r\n\r\n */
			recvlen -= n;
			memmove(recvbuf,&recvbuf[n],recvlen);
			recvbuf[recvlen] = '\0';
		}
#else
		n = 0;
#endif
	} else if (recvbuf)
	{	free(recvbuf);
		recvbuf = NULL;
		recvlen = 0;	/* It's like it never happened */
	}
	if (pLen) *pLen = recvlen;

	if (hwndProgress) SpinMessages(hWnd);
	if (hwndProgress) DestroyWindow(hwndProgress);
	if (!recvbuf && pError) *pError = "recv() Failed";
	return recvbuf;
}

int httpGet(int thread, HWND hWnd, char *Host, long Port, char *URL, char *Path)
{
static char *gblServer[THREAD_COUNT] = {0};
static int gblResolved[THREAD_COUNT] = {0};
static struct sockaddr_in gblserver[THREAD_COUNT] = {0};
	int lclResolved = FALSE;
	int *pResolved = &lclResolved;
	struct sockaddr_in lclserver = {0};
	struct sockaddr_in *pserver = &lclserver;
	int mysocket;
	long timeout;
	unsigned long Available = 0;
	char *sendbuf, *recvbuf;
	int n, recvlen, recvsize, length, ContentLength=0, TotalLength=0;
	int Success = 0;
	__int64 startTime, nameTime, connTime, sendTime, recvTime, fileTime, totalTime;
static int initialized = 0;

TraceLogThread("OSM", FALSE, "Thread[%ld] httpGet(%s) From %s:%ld to %s\n", (long) thread, URL, Host, (long) Port, Path);

	if (thread >= 0 && thread < THREAD_COUNT)
	{	pResolved = &gblResolved[thread];
		pserver = &gblserver[thread];
		if (!gblServer[thread] || strcmp(Host,gblServer[thread]))
		{	if (gblServer[thread]) free(gblServer[thread]);
			gblServer[thread] = _strdup(Host);
			*pResolved = FALSE;
		}
	} else thread = THREAD_COUNT;

	startTime = llGetMsec();

	if (!initialized)
	{	TQStatus[thread] = "Init";
		sock_init();
		initialized = 1;
	}

	TilesAttempted++;

	if (!*pResolved)
	{	TQStatus[thread] = "DNS";
		pserver->sin_family      = AF_INET;
		pserver->sin_port        = htons((u_short)Port);
		pserver->sin_addr.s_addr = inet_addr(Host);	/* argv[1] = hostname */
		if (pserver->sin_addr.s_addr == (u_long) -1)
		{	struct hostent *hostnm = gethostbyname(Host);
			if (!hostnm)
			{	TraceLogThread("OSM", TRUE, "httpGet:gethostbyname(%s) Failed\n", Host);
				return 0;
			}
			pserver->sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);
		}
		{	char *ipaddr = inet_ntoa(pserver->sin_addr);
			TraceLogThread("OSM", FALSE, "%ld:OSM Resolved(%s)=>%s\n",(long) thread, Host, ipaddr?ipaddr:"NULL");
		}
		*pResolved = TRUE;	/* Buffer this IP */
	}

	nameTime = llGetMsec();

	if ((mysocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{	TraceLogThread("OSM", TRUE, "httpGet:socket() Failed\n");
		*pResolved = FALSE;	/* Kill optimizations */
		return 0;
	}

	TQStatus[thread] = "Conn";
	if (connect(mysocket, (struct sockaddr *)pserver, sizeof(*pserver)) < 0)
	{	soclose(mysocket);
		TraceLogThread("OSM", TRUE, "httpGet:connect(%s:%ld) Failed\n", Host, (long) Port);
		*pResolved = FALSE;	/* Kill optimizations */
		return 0;
	}

	connTime = llGetMsec();

	sendbuf = (char *) malloc(strlen(URL)+strlen(Host)+256);
	length = sprintf(sendbuf, "GET %s HTTP/1.0\r\nAccept: */*\r\nHost: %s\r\nConnection: close\r\nContent-Length: 0\r\nUser-Agent: %s(%s)\r\n\r\n", URL, Host, PROGNAME, CALLSIGN);

	TileBytesSent += length;

//	TraceLogThread("OSM", FALSE, "%.*s\n", (int) length, sendbuf);

	TQStatus[thread] = "Send";
	n = send(mysocket, sendbuf, length, 0);
	free(sendbuf);
	if (n != (int)length)
	{	soclose(mysocket);
		TraceLogThread("OSM", TRUE, "httpGet:send(%ld) Failed\n", (long) length);
		*pResolved = FALSE;	/* Kill optimizations */
		return 0;
	}

	sendTime = llGetMsec();

	recvlen = 0;
	recvsize = 8192;
	recvbuf = (char *) malloc(recvsize+1);

	do
	{	int dSleep = 100;	/* with select() it doesn't matter */
		Available = 0;
		timeout = 15000;	/* 15 second timeout */
		TQStatus[thread] = "Ioctl";
		while ((n=ioctlsocket(mysocket,FIONREAD,&Available))==0 && Available <= 0 && timeout > 0)
		{static char Status[THREAD_COUNT+1][80];
#ifdef OLD_WAY
			sprintf(&Status[thread][0],"Sleep(%ld/%ld)", (long) dSleep, (long) timeout);
			TQStatus[thread] = &Status[thread][0];
			Sleep(dSleep);
#else
			dSleep = timeout;
			sprintf(&Status[thread][0],"Wait(%ld)", (long) dSleep/1000);
			TQStatus[thread] = &Status[thread][0];

			__int64 selectStart = llGetMsec();
			fd_set fdRead, fdError;
			struct timeval tmo;
			FD_ZERO(&fdRead); FD_ZERO(&fdError);
			FD_SET(mysocket, &fdRead); FD_SET(mysocket, &fdError);
			tmo.tv_sec = dSleep/1000; tmo.tv_usec = (dSleep%1000) * 1000;	/* uSec */
			n = select(mysocket+1, &fdRead, NULL, &fdError, &tmo);
			if (n > 0)
			{	if (FD_ISSET(mysocket, &fdRead))
				{	dSleep = (long) (llGetMsec() - selectStart);
//TraceLogThread("OSM", FALSE, "httpGet:select(%ld) readable after %ld/%ld with %ld left\n", (long) mysocket, (long) dSleep, (long) tmo.tv_sec*1000+tmo.tv_usec/1000, (long)  timeout);
					if ((n=ioctlsocket(mysocket,FIONREAD,&Available))!=0 || Available <= 0)
						Available = 1;	/* Readable means recv() won't block */
//else if (Available) TraceLogThread("OSM", FALSE, "httpGet:readable %ld has %ld Available\n", mysocket, Available);
					n = 0;	/* make it look successful */
					break;
				}
				if (FD_ISSET(mysocket, &fdError))
					TraceLogThread("OSM", TRUE, "httpGet:selelect(%ld) errored with %ld left\n", (long) mysocket, (long) timeout);
			} else if (n < 0)
				TraceLogThread("OSM", TRUE, "httpGet:select(%ld) return %ld\n", (long) mysocket, (long) n);
			else TraceLogThread("OSM", TRUE, "httpGet:select(%ld) timeout %ld/%ld left\n", (long) mysocket, (long) dSleep, (long) timeout);
#endif
			timeout -= dSleep;
			dSleep *= 2;
			if (dSleep > 100) dSleep = 100;
			TQStatus[thread] = "Ioctl";
		}
		if (n)	/* Errors mean we're outa here */
			break;
		else if (Available)
		{
//TraceLogThread("OSM", FALSE, "httpGet:Got %ld Available with %ldmsec left\n", (long) Available, (long) timeout);
			TQStatus[thread] = "Recv";
			if ((n=recv(mysocket, recvbuf+recvlen, recvsize-recvlen, 0)) > 0)
			{	char *c, *e;
				recvlen += n;
				recvbuf[recvlen] = 0;	/* Null terminate for safety */
				if (recvlen >= recvsize)
				{	recvsize += 1024;
					recvbuf = (char *) realloc(recvbuf, recvsize+1);
				}
				if (!ContentLength && (c = strstr(recvbuf,"Content-Length:")) != NULL)
				{	c += 15;	/* strlen("Content-Length:") */
					ContentLength = strtol(c,&e,10);
//TraceLogThread("OSM", FALSE, "httpGet:Content-Length: %ld Have %ld/%ld for %s\n", (long) ContentLength, (long) recvlen, (long) recvsize, Path);
				}
				if (ContentLength && !TotalLength && (c=strstr(recvbuf,"\r\n\r\n")) != NULL)
				{	long HeaderLength = (c+4)-recvbuf;
					TotalLength = ContentLength + HeaderLength;
TraceLogThread("OSM", FALSE, "httpGet:Header %ld Content %ld Total %ld Got %ld/%ld for %s\n", (long) HeaderLength, (long) ContentLength, (long) TotalLength, (long) recvlen, (long) recvsize, Path);
					if (TotalLength+1 >= recvsize)
					{	recvsize = TotalLength + 1;
						recvbuf = (char *) realloc(recvbuf, recvsize+1);
					}
				}
			} else	/* Errors mean we're done */
				break;
		}
	} while (timeout>0 && (!TotalLength || recvlen < TotalLength));

	recvTime = llGetMsec();

//#define KEEP_HEADER
#ifndef KEEP_HEADER
	for (n=0; n<=recvlen-4; n++)
	{	if (recvbuf[n] == '\r' && recvbuf[n+1] == '\n'
		&& recvbuf[n+2] == '\r' && recvbuf[n+3] == '\n')
			break;
	}
	n += 4;	/* Skip over \r\n\r\n */
//	TraceLogThread("OSM", FALSE, "%.*s", (int) n, recvbuf);
#else
	n = 0;
#endif
	if (recvlen > n)
	{	FILE *Out = fopen(Path, "wb");
		TQStatus[thread] = "File";

		if (Out)
		{	if (ContentLength && ContentLength != recvlen-n)
			TraceLogThread("OSM", TRUE, "httpGet:ContentLength=%ld Got %ld for %s\n",
						(long) ContentLength, (long) recvlen-n, Path);
			if ((int)fwrite(recvbuf+n, sizeof(*recvbuf), recvlen-n, Out) < recvlen-n)
			{	fclose(Out);
				RemoveFile(Path);
				TraceLogThread("OSM", TRUE, "httpGet:Removed Incompletely Written File %s\n", Path);
		} else
			{	fclose(Out);
				Success = 1;
				OSMFileCount++;
				OSMFileSpace += recvlen-n;
			}
		}
		TileBytesRecv += recvlen;
		TilesFetched++;
	}

	fileTime = llGetMsec();

	free(recvbuf);

	soclose(mysocket);

	TQStatus[thread] = "Done";

	totalTime = recvTime - startTime;	/* Only the network time */
	fileTime -= recvTime;
	recvTime -= sendTime;
	sendTime -= connTime;
	connTime -= nameTime;
	nameTime -= startTime;
	TraceLogThread("OSM", FALSE, "httpGet(%s) Name:%.0lf Conn:%.0lf Send:%.0lf Recv:%.0lf File:%.0lf Total:%.0lf (%ld Bytes)\n",
				Path, nameTime, connTime, sendTime, recvTime, fileTime, totalTime, (long) recvlen);

	FetchTimes.name += nameTime;
	FetchTimes.conn += connTime;
	FetchTimes.send += sendTime;
	FetchTimes.recv += recvTime;
	FetchTimes.write += fileTime;
	FetchTimes.writes++;
	FetchTimes.get += totalTime;
	FetchTimes.gets++;

	if (!Success) *pResolved = FALSE;	/* Kill optimizations */
	return Success;
}

