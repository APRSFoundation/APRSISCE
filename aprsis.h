#ifdef __cplusplus
extern "C"
{
#endif
void SetTraceThreadName(char *Name);

char *APRSISFormatTransmit(char *String, long *rLen, void *Where);
char *APRSISFormatReceive(int Len, unsigned char *Pkt, int *rLen);
BOOL APRSISGotPacket(int len, unsigned char *buf);

BOOL APRSISTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo);
BOOL APRSISNewConnection(void *What, BOOL (*Write)(void *What, int Len, unsigned char *Buf));

BOOL CWOPTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo);
BOOL CWOPNewConnection(void *What, BOOL (*Write)(void *What, int Len, unsigned char *Buf));
char *CWOPFormatTransmit(char *String, long *rLen, void *Where);

BOOL ServerTweakConfig(HWND hwnd, PORT_CONFIG_INFO_S *pcInfo);
BOOL ServerGotPacket(int len, unsigned char *buf);
char *ServerFormatReceive(int Len, unsigned char *Pkt, int *rLen);

#ifdef __cplusplus
}
#endif
