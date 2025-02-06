#ifdef __cplusplus
extern "C"
{
#endif
void cdecl TraceLog(char *Name, BOOL ForceIt, HWND hwnd, char *Format, ...);

BOOL AGWNewConnection(void *What, BOOL (*Write)(void *What, int Len, unsigned char *Buf));
char *AGWFormatTransmit(char *String, long *rLen, void *Where);
char *AGWFormatReceive(int Len, unsigned char *Pkt, int *rLen);
BOOL AGWGotPacket(int len, unsigned char *buf);

#ifdef __cplusplus
}
#endif
