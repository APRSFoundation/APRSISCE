#ifdef __cplusplus
extern "C"
{
#endif
void cdecl TraceLog(char *Name, BOOL ForceIt, HWND hwnd, char *Format, ...);
void SetTraceThreadName(char *Name);

char *APRSUDPFormatTransmit(char *String, long *rLen, void *Where);
char *APRSUDPFormatReceive(int Len, unsigned char *Pkt, int *rLen);
BOOL APRSUDPGotPacket(int len, unsigned char *buf);

BOOL APRSUDPTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo);
BOOL APRSUDPNewConnection(void *What, BOOL (*Write)(void *What, int Len, unsigned char *Buf));

#ifdef __cplusplus
}
#endif
