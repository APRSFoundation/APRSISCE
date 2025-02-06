/* Prototypes generated Sat Mar 19 01:40:55 2011 */
#ifdef __cplusplus
extern "C"
{
#endif
extern HWND hwndSatellites;
void SaveSatellites(void);
BOOL IsSatelliteName(char *Name);
char *GetPassString(char *Name, char *For, double lat, double lon, long alt);
void Satellite(HWND hwnd, UINT msgReceived, WPARAM wp, char *Name, double lat, double lon, long alt);
#ifdef __cplusplus
}
#endif
