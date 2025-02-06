typedef struct GPX_WAYPOINT_INFO_S
{	char *Name;
	char *Desc;
	double lat, lon;	/* Last reported lat/lon */
	int symbol;				/* Symbol and page */
} GPX_WAYPOINT_INFO_S;

void FreeGPXFile(HWND hwnd);
unsigned long LoadGPXFile(HWND hwnd, OVERLAY_CONFIG_INFO_S *pOver/*, UINT msg, WPARAM wp*/);
int LoadPOSOverlay(HWND hwnd, OVERLAY_CONFIG_INFO_S *pOver, BOOL Active);

