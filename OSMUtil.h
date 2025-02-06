#ifdef __cplusplus

#ifdef UNDER_CE
#define MIN_OSM_ZOOM 0	/* 4 is good */
#define MAX_OSM_ZOOM 22	/* Never larger than 24! */
#else
#define MIN_OSM_ZOOM 0	/* 4 is good */
#define MAX_OSM_ZOOM 23	/* Never larger than 24! */
#endif
#define MAX_REASONABLE_ZOOM 16	/* No auto-prefetch closer than this */

typedef struct OSM_TILE_INFO_S
{
//	struct
//	{	double lat, lon;
//	} min, max;
	HWND hWnd;
	TILE_SERVER_INFO_S *sInfo;
	int tzoom, tx, ty;
	int inBig;	/* HIWORD=i, LOWORD=n */
	RECT rcBig;
	HBITMAP hbmBig;
} OSM_TILE_INFO_S;

typedef struct OSM_TILE_POS_S
{	RECT rc;
	int stretch;
	int zoom, x, y, xw, yw;
	RECT rcSource;
	OSM_TILE_INFO_S *Tile;
} OSM_TILE_POS_S;

typedef struct OSM_TILE_SET_S
{	TILE_SERVER_INFO_S *sInfo;
	RECT rcWin, tileMinMax;
	int zoom, xs, ys, xe, ye;
	int xt, yt, xo, yo;
	double lat, lon;
	BOOL Dim;
	int Percent;
	long TilesFetched;
	BOOL StillLoading;
	BOOL BuiltTooMany;
	int BuiltCount;
	int StretchCount;
	int MissingCount;
	__int64 msBuilt;
	__int64 msStretch;
#ifdef PRIME_CENTER_TILE
	__int64 msPrime;
#endif
	__int64 msWorld;
	HBITMAP hbm;
	BOOL GridSquares;
//	struct
//	{	int tileX, tileY, tileZ;	/* -1, -1, -1 is not used */
//	} TilesCovered[2];
	int Count;
	int Size;
	OSM_TILE_POS_S *Tiles;
} OSM_TILE_SET_S;

TILE_SERVER_INFO_S *OSMRegisterTileServer(TILE_SERVER_INFO_S *sInfo, char *From);

char *OSMGetQueueStatus(void);
int OSMGetQueueStats(int *Count=NULL, int *Servers=NULL);
void OSMSetFetchEnable(BOOL FetchEnabled);
void OSMSetPurgeEnable(BOOL PurgeEnabled);
void OSMSetTileServerInfo(HWND hWnd, UINT msgNotify, TILE_SERVER_INFO_S *tInfo, unsigned long MinMBFree);
BOOL OSMGetFreeSpace(TILE_SERVER_INFO_S *sInfo, unsigned __int64 *pFree);
unsigned __int64 OSMGetClusterSize(TILE_SERVER_INFO_S *sInfo);
void OSMGetTileServerTotals(unsigned long *pTotalTiles, unsigned __int64 *pTotalSpace, unsigned __int64 *pTotalDSpace, long *pTilesAttempted, long *pTilesFetched, long *bSent, long *bRecv, double *msGetTime);
void OSMGetTileServerStats(TILE_SERVER_INFO_S *sInfo, unsigned long *pTotalTiles, unsigned __int64 *pTotalSpace, unsigned __int64 *pTotalDSpace);
unsigned long OSMGetTileServerCacheStats(TILE_SERVER_INFO_S *sInfo, unsigned __int64 *pFileSpace=NULL, unsigned __int64 *pDiskSpace=NULL);
unsigned long OSMFlushTileServerCache(TILE_SERVER_INFO_S *sInfo, unsigned __int64 *pFileSpace=NULL, unsigned __int64 *pDiskSpace=NULL, HWND hwnd=NULL);
BOOL isOSMDataOnly(char *path, TCHAR *ProbeFile, BOOL *SomeOSM);
TCHAR *OSMGetTileAgeStats(void);

int long2tilex(double lon, int z);
int lat2tiley(double lat, int z);
double tiledx2long(double x, int z);
double tiledy2lat(double y, int z);
double GetLongPixelDelta(double lon, int z);
double GetLatPixelDelta(double lat, int z);
void LatLonToTileCoord(double lat, double lon, OSM_TILE_COORD_S *tCoord);
BOOL OSMTileCoordToPoint(OSM_TILE_SET_S *ts, OSM_TILE_COORD_S *tCoord, POINT *pt, BOOL Visibility/*=TRUE*/);
BOOL OSMGetXYPos(OSM_TILE_SET_S *ts, double lat, double lon, POINT *pt, BOOL Visibility=TRUE);
BOOL OSMPointToLatLon(OSM_TILE_SET_S *ts, POINT *pt, double *pLat, double *pLon);
double OSMCalculateScale(OSM_TILE_SET_S *ts, RECT *rc);

void OSMPaintTileSet(HWND hWnd, HDC hdc, RECT *rc, int Percent, OSM_TILE_SET_S *ts);
BOOL OSMIsTileSetCompatible(HWND hwnd, OSM_TILE_SET_S *ts, RECT *rcWin, TILE_SERVER_INFO_S *sInfo, BOOL Dim, int zoom, double lat, double lon, int Percent, BOOL GridSquareLines, BOOL *pUpdated=NULL, char **pWhy=NULL);
void OSMFreeTileSet(OSM_TILE_SET_S *ts);
BOOL OSMIsTileInSet(OSM_TILE_SET_S *ts, int xt, int yt, int zt);
OSM_TILE_SET_S *OSMGetTileSet(HWND hWnd, RECT *rcWin, TILE_SERVER_INFO_S *sInfo, BOOL Dim, int zoom, double lat, double lon, int Percent, BOOL GridSquareLines);
int OSMPrefetchTiles(HWND hWnd, RECT *rcWin, TILE_SERVER_INFO_S *sInfo, int minzoom, int maxzoom, double lat, double lon, BOOL OnlyCount);

int OSMGetCacheSize(void);
void OSMFlushTileCache(int RetainSeconds);
void OSMFreeTile(OSM_TILE_INFO_S *Tile);
OSM_TILE_INFO_S *OSMGetTileInfo(HWND hWnd, TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y, int Priority, BOOL CacheOnly=FALSE, BOOL Adjacent=FALSE, BOOL *pBuilt=NULL, char *osmTileFile=NULL);
HBITMAP OSMMakeTileBitmap(HWND hWnd, char *osmTileFile, RECT *prc, int *piBig);

char *OSMCheckTileState(TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y);

char *OSMPrepTileFile(TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y, BOOL *Exists CALLER, char *CheckPath=NULL, BOOL CreateDirs=TRUE);
char *OSMGetOrFetchTileFile(int thread, HWND hWnd, TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y);
char *OSMGetOrQueueTileFile(HWND hWnd, TILE_SERVER_INFO_S *sInfo, int zoom, int x, int y, int Priority, BOOL Adjacent, BOOL *pQueued=NULL);
void OSMFlushTileQueue(BOOL ClearAll, HWND hwnd=NULL, BOOL WindowGone=FALSE);
int httpGet(int thread, HWND hWnd, char *Host, long Port, char *URL, char *Path);
BOOL OSMLockQueue(DWORD msWaitTime);
void OSMUnlockQueue(void);

int IsDirectory(char *Dir);
int MakeDirectory(char *Dir);

void SpinMessages(HWND hwnd);

extern "C"
{
char *httpGetBuffer(HWND hWnd, char *Host, long Port, char *URL, int *pLen, char *Agent = "httpGetBuffer", BOOL ForceProgress=FALSE, char **pError=NULL);
}

#ifdef OBSOLETE
double OSMGetMsec(void);
double OSMMSecSince(double msStart, double msNow);
#endif

#else	/* __cplusplus */

char *httpGetBuffer(HWND hWnd, char *Host, long Port, char *URL, int *pLen, char *Agent, BOOL ForceProgress, char **pError);

#endif	/* __cplusplus */
