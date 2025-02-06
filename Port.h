#include "config.h"

#ifdef __cplusplus
extern "C"
{
#endif
void AddPortProtocols(HWND hwndList);
void cdecl TraceActivityBegin(HWND hwnd, char *Format, ...);
void cdecl TraceActivityEnd(HWND hwnd, char *Format, ...);
void cdecl TraceActivity(HWND hwnd, char *Format, ...);
void cdecl TraceError(HWND hwnd, char *Format, ...);
void cdecl TraceLog(char *Name, BOOL ForceIt, HWND hwnd, char *Format, ...);
void SetTraceThreadName(char *Name);
void SetDefaultTraceName(char *Name);

double PortGetMsec(void);
void PortDumpHex2(char *Log, BOOL ForceIt, char *What, int Len, unsigned char *p);
void PortDumpHex(char *Log, char *What, int Len, unsigned char *p);
char *PortCommand(void * Where, unsigned char *Command, char **pResponse);

BOOL PortTransmit(PORT_CONFIG_INFO_S *pInfo, WPARAM wp, char *String);
BOOL PortStart(PORT_CONFIG_INFO_S *pcInfo, HWND hwnd, UINT msgStatus, UINT msgSubStatus, UINT msgReceived, UINT msgXmitCount, WPARAM wp);
BOOL PortStop(PORT_CONFIG_INFO_S *pInfo, WPARAM wp);

#ifdef __cplusplus
}
#endif
