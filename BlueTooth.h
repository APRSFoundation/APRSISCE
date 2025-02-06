#ifndef GOT_BLUETOOTH_H
#define GOT_BLUETOOTH_H

#ifdef __cplusplus
extern "C"
{
#endif

//extern char Timestamp[];
//extern CONFIG_INFO_S ActiveConfig;

typedef struct BTPORT_CONFIG_S
{	char	Port[32];
	long	Baud;
	int		Bits;
	int		Parity;
	int		Stop;
	int		Transmit;	/* Wants to see rasterized forms */
	char	IPorDNS[64];
	long	TcpPort;
} BTPORT_CONFIG_S;

#include <ws2bth.h>

TCHAR * GetBTDevices(__in const LPWSTR pszRemoteName);

ULONG NameToBthAddr(__in const LPWSTR pszRemoteName, __out PSOCKADDR_BTH pRemoteBtAddr);

int ConnectBTSocket(char *Name);

#ifdef __cplusplus
}
#endif

#endif	/* GOT_BLUETOOTH_H */
