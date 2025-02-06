#define GEOCACHE_ID_SIZE 10

typedef struct GEOCACHE_INFO_S
{	char ID[GEOCACHE_ID_SIZE];
	char *Desc;
	char *Owner;
	char *ShortDesc;
	char *LongDesc;
	char *Container;
	char *Type;
	char *Hint;
	char difficulty, terrain;	/* both*2 */
	double lat, lon;	/* Last reported lat/lon */
	int symbol;				/* Symbol and page */
	BOOL visible;			/* TRUE if station is visible on-screen */
	RECT rc;				/* Target icon rectangle */
	BOOL labelled;
	RECT rcLabel;
} GEOCACHE_INFO_S;

extern unsigned long GeocacheCount;
extern unsigned long GeocacheSize;
extern unsigned long GeocacheRAM;
extern GEOCACHE_INFO_S *Geocaches;

void FreeGeocaches(HWND hwnd);
unsigned long LoadGeocaches(HWND hwnd);
