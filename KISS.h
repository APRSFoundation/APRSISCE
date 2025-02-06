#ifdef __cplusplus
extern "C"
{
#endif
void cdecl TraceLog(char *Name, BOOL ForceIt, HWND hwnd, char *Format, ...);

unsigned char *KISSCall(unsigned char *Call, unsigned char *Dest);
BOOL KISSTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo);
BOOL D700KISSTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo);
BOOL SimplyKISSTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo);
char *KISSFormatTransmit(char *String, long *rLen, void *Where);
char *KISSFormatAX25(char *String, long *rLen);
char *KISSDeFormatAX25(int Len, unsigned char *Pkt);
char *KISSFormatReceive(int Len, unsigned char *Pkt, int *rLen);
BOOL KISSGotPacket(int len, unsigned char *buf);

#ifdef __cplusplus
}
#endif
