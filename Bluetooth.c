#define INITGUID
#include "sysheads.h"

//#include <initguid.h>
#ifdef UNDER_CE
#include <ws2bth.h>
#include <bt_sdp.h>
#include <bthapi.h>
#else
#include <Bthsdpdef.h>
#endif

#include "BlueTooth.h"

#include "tcputil.h"	/* For TraceLog stuff */

extern HINSTANCE g_hInstance;

// helper macros
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (UINT_PTR)(sizeof(a)/sizeof((a)[0]))
#endif

#define CXN_TRANSFER_DATA_LENGTH 2048

void DiscoverServices(SOCKADDR_BTH *psa)
{	INT				iResult = 0;
	LPWSAQUERYSET	pwsaResults;
	DWORD			dwSize = 0;
	HANDLE hLookup;
	
	SOCKADDR_BTH sa = {0};
	CSADDR_INFO csai = {0};
	BTHNS_RESTRICTIONBLOB RBlob = {0};
	BLOB blob;
//	GUID protocol = L2CAP_PROTOCOL_UUID;
	WSAQUERYSET wsaq = {0};
	union {
		CHAR buf[5000];				// returned struct can be quite large 
		SOCKADDR_BTH	__unused;	// properly align buffer to BT_ADDR requirements
	} ubu;

	sa.btAddr = psa->btAddr;
	sa.addressFamily = AF_BTH;

	csai.RemoteAddr.lpSockaddr = (struct sockaddr *) &sa;
	csai.RemoteAddr.iSockaddrLength = sizeof(sa);
	
	RBlob.type = SDP_SERVICE_SEARCH_REQUEST;
	RBlob.uuids[0].uuidType = SDP_ST_UUID16;
	RBlob.uuids[0].u.uuid16 = SerialPortServiceClassID_UUID16;
//	RBlob.uuids[1].uuidType = SDP_ST_UUID16;
//	RBlob.uuids[1].u.uuid16 = DialupNetworkingServiceClassID_UUID16;

	blob.cbSize = sizeof(RBlob);
	blob.pBlobData = (BYTE *)&RBlob;

	wsaq.dwSize      = sizeof(wsaq);
#ifdef UNDER_CE
	wsaq.dwNameSpace = NS_BTH;
#else
	wsaq.dwNameSpace = NS_ALL;	/* Mabye NS_ALL? *//* Was NS_BTH */
	//	wsaq.lpServiceClassId = &protocol;
#endif
	wsaq.lpBlob      = &blob;
	wsaq.lpcsaBuffer = &csai;

	// initialize searching procedure
	iResult = WSALookupServiceBegin(&wsaq, 0, &hLookup);
	
	if (iResult != 0)
	{
		iResult = WSAGetLastError();		
		TraceLogThread("BlueTooth", TRUE, "%d:Socket Error: %d\n", __LINE__, iResult);
	}
	
	for (; ;)
	{
		pwsaResults = (LPWSAQUERYSET) ubu.buf;
		
		dwSize  = sizeof(ubu.buf);
		
		memset(pwsaResults,0,sizeof(WSAQUERYSET));
		pwsaResults->dwSize      = sizeof(WSAQUERYSET);
		// namespace MUST be NS_BTH for bluetooth queries
		pwsaResults->dwNameSpace = NS_BTH;
		pwsaResults->lpBlob      = NULL;
		
		// iterate through all found devices, returning name and address
		// (this sample only uses the name, but address could be used for
		// further queries)
		iResult = WSALookupServiceNext (hLookup, 
			0 | /*LUP_FLUSHCACHE |*/LUP_RETURN_NAME | /*LUP_RETURN_TYPE | LUP_RETURN_ADDR | */LUP_RETURN_BLOB /*| LUP_RETURN_COMMENT*/, 
			&dwSize, 
			pwsaResults);
		
		
		if (iResult != 0)
		{
			iResult = WSAGetLastError();
			if (iResult != WSA_E_NO_MORE)
			{
				iResult = WSAGetLastError();
				TraceLogThread("BlueTooth", TRUE, "%d:Socket Error: %d\n", __LINE__, iResult);
			}
			// we're finished

			break;
		}
		
		if (pwsaResults->lpszServiceInstanceName)
			TraceLogThread("BlueTooth", TRUE, "    %S\n", pwsaResults->lpszServiceInstanceName);

		// add the name to the listbox
		if (pwsaResults->lpBlob)
		{	TCHAR Buff[1024];
			TCHAR *Next = Buff;
			UINT Remain = sizeof(Buff);
			unsigned int i;
			TraceLogThread("BlueTooth", TRUE, "    pBlob:%p nBlob:%ld\n",
							pwsaResults->lpBlob,
							(long) pwsaResults->lpBlob->cbSize);
			for (i=0; i<pwsaResults->lpBlob->cbSize; i++)
			{	StringCbPrintfEx(Next, Remain, &Next, &Remain, STRSAFE_IGNORE_NULLS,
								TEXT("%02lX "), (long) pwsaResults->lpBlob->pBlobData[i]&0xff);
			}
			TraceLogThread("BlueTooth", TRUE, "      %S", Buff);
#ifdef FUTURE

STDMETHODIMP
ServiceAndAttributeSearchParse(
  UCHAR *szResponse,   // in - pointer to buffer representing 
                       // the SDP record, in binary format, 
                       // returned by the Bthnscreate tool.
  DWORD cbResponse,   // in - size of szResponse 
  ISdpRecord ***pppSdpRecords, // out - array of pSdpRecords
  ULONG *pNumRecords   // out - number of elements in pSdpRecords array
)
{

  HRESULT hres = E_FAIL;
  *pppSdpRecords = NULL;
  *pNumRecords = 0;
  ISdpStream *pIStream = NULL;
// Create a stream object.
if (FAILED(CoCreateInstance(__uuidof(SdpStream),NULL,CLSCTX_INPROC_SERVER,
                        __uuidof(ISdpStream),(LPVOID *) &pIStream))) 
{
  return E_FAIL;
}
// Ensure that the stream is valid and is well formed.
  hres = pIStream->Validate(szResponse,cbResponse,NULL);

  if (SUCCEEDED(hres)) 
  {
  // Ensure that the sequence of the stream is valid and is well formed.
    hres = pIStream->VerifySequenceOf(szResponse,cbResponse,
                       SDP_TYPE_SEQUENCE,NULL,pNumRecords);
    if (SUCCEEDED(hres) && *pNumRecords > 0) 
    {
      *pppSdpRecords = (ISdpRecord **) 
      //Allocate memory for the SDP record buffer.
      CoTaskMemAlloc(sizeof(ISdpRecord*) * (*pNumRecords));
      if (pppSdpRecords != NULL) 
      {
         // Retrieve the SDP records from the stream.
         hres = pIStream->RetrieveRecords ( szResponse, 
                    cbResponse,*pppSdpRecords,pNumRecords);
        //If retrieval of records from the stream failed,
        // free memory allocated to the SDP record array and set the 
        // SDP record count to zero (0).
        if (!SUCCEEDED(hres)) 
        {
            CoTaskMemFree(*pppSdpRecords);
            *pppSdpRecords = NULL;
            *pNumRecords = 0;
         }
       }
       else 
       {
          hres = E_OUTOFMEMORY;
       }
     }
  }
  // Release the stream.
  if (pIStream != NULL) 
  {
    pIStream->Release();
    pIStream = NULL;
  }
  return hres;
}


#endif
		}
	}
	
	WSALookupServiceEnd(hLookup);

}

TCHAR *GetBTDevices(__in const LPWSTR pszRemoteName)
{
	TCHAR *Result = NULL;
	int r=0;

	INT             iResult = FALSE;

	BOOL            bRemoteDeviceFound = FALSE;
	LPWSAQUERYSET	pwsaResults;
	DWORD			dwSize = 0;
	WSAQUERYSET		wsaq;
//	HCURSOR			hCurs;
	HANDLE			hLookup = 0;
	
	union {
		CHAR buf[5000];				// returned struct can be quite large 
		SOCKADDR_BTH	__unused;	// properly align buffer to BT_ADDR requirements
	} ubu;
	
	if (pszRemoteName)
		TraceLogThread("BlueTooth", TRUE, "Resolving %S\n", pszRemoteName);
	else TraceLogThread("BlueTooth", TRUE, "Enumerating BlueTeeth\n");

	memset (&wsaq, 0, sizeof(wsaq));
	wsaq.dwSize      = sizeof(wsaq);
	wsaq.dwNameSpace = NS_BTH;
	wsaq.lpcsaBuffer = NULL;
	
	// initialize searching procedure
	iResult = WSALookupServiceBegin(&wsaq, 
		LUP_CONTAINERS, 
		&hLookup);
	
	if (iResult != 0)
	{
//		TCHAR tszErr[32];
		iResult = WSAGetLastError();		
		TraceLogThread("BlueTooth", TRUE, "%d:Socket Error: %d\n", __LINE__, iResult);
	}
	
	for (; ;)
	{
		pwsaResults = (LPWSAQUERYSET) ubu.buf;
		
		dwSize  = sizeof(ubu.buf);
		
		memset(pwsaResults,0,sizeof(WSAQUERYSET));
		pwsaResults->dwSize      = sizeof(WSAQUERYSET);
		// namespace MUST be NS_BTH for bluetooth queries
		pwsaResults->dwNameSpace = NS_BTH;
		pwsaResults->lpBlob      = NULL;
		
		// iterate through all found devices, returning name and address
		// (this sample only uses the name, but address could be used for
		// further queries)
		iResult = WSALookupServiceNext (hLookup, 
			LUP_RETURN_NAME | LUP_RETURN_ADDR, 
			&dwSize, 
			pwsaResults);
		
		
		if (iResult != 0)
		{
			iResult = WSAGetLastError();
			if (iResult != WSA_E_NO_MORE)
			{
//				TCHAR tszErr[32];
				iResult = WSAGetLastError();
				TraceLogThread("BlueTooth", TRUE, "%d:Socket Error: %d\n", __LINE__, iResult);
			}
			// we're finished

			break;
		}
		
		// add the name to the listbox
		if (pwsaResults->lpszServiceInstanceName)
		{	TraceLogThread("BlueTooth", TRUE, "%S nProt:%ld nCsA:%ld\n",
							pwsaResults->lpszServiceInstanceName,
							(long) pwsaResults->dwNumberOfProtocols,
							(long) pwsaResults->dwNumberOfCsAddrs);
            if (!pszRemoteName
			|| ( !_wcsicmp(pwsaResults->lpszServiceInstanceName, pszRemoteName) ) )
			{	DWORD c;
				for (c=0; c<pwsaResults->dwNumberOfCsAddrs; c++)
				{	switch (pwsaResults->lpcsaBuffer[c].RemoteAddr.lpSockaddr->sa_family)
					{
					case AF_BTH:
					{	int nLen = wcslen(pwsaResults->lpszServiceInstanceName);
						SOCKADDR_BTH *sa = (SOCKADDR_BTH *) pwsaResults->lpcsaBuffer[c].RemoteAddr.lpSockaddr;
						TraceLogThread("BlueTooth", TRUE, "[%ld] AF_BT:%ld NAP:%04X SAP:%08X Chan:%ld\n",
										(long) c, (long) sa->addressFamily,
										(long) GET_NAP(sa->btAddr), (long) GET_SAP(sa->btAddr),
										(long) sa->port);
						DiscoverServices(sa);
						Result = (TCHAR*)realloc(Result,(r+nLen+2)*sizeof(*Result));
						r += wsprintf(&Result[r],TEXT("%s"),pwsaResults->lpszServiceInstanceName)+1;/* After null */
						Result[r] = 0;	/* Trailing double-null */
					}
					default:
					TraceLogThread("BlueTooth", TRUE, "[%ld] AF:%ld != BTH(%ld)\n",
									(long) c, (long) pwsaResults->lpcsaBuffer[c].RemoteAddr.lpSockaddr->sa_family, (long) AF_BTH);
					}
				}
			} else if (pszRemoteName)
				TraceLogThread("BlueTooth", TRUE, "GetBTDevices:Query found (%S) Need (%S)\n", pwsaResults->lpszServiceInstanceName, pszRemoteName);
		}
	}
	
	WSALookupServiceEnd(hLookup);
	
    return Result;
}

//
// NameToBthAddr converts a bluetooth device name to a bluetooth address,
// if required by performing inquiry with remote name requests.
// This function demonstrates device inquiry, with optional LUP flags.
//
ULONG NameToBthAddr(__in const LPWSTR pszRemoteName, __out PSOCKADDR_BTH pRemoteBtAddr)
{
    INT             iResult = FALSE;

	BOOL            bRemoteDeviceFound = FALSE;
	LPWSAQUERYSET	pwsaResults;
	DWORD			dwSize = 0;
	WSAQUERYSET		wsaq;
//	HCURSOR			hCurs;
	HANDLE			hLookup = 0;
	
	union {
		CHAR buf[5000];				// returned struct can be quite large 
		SOCKADDR_BTH	__unused;	// properly align buffer to BT_ADDR requirements
	} ubu;
	
	TraceLogThread("BlueTooth", TRUE, "Resolving %S\n", pszRemoteName);

	memset (&wsaq, 0, sizeof(wsaq));
	wsaq.dwSize      = sizeof(wsaq);
	wsaq.dwNameSpace = NS_BTH;
	wsaq.lpcsaBuffer = NULL;
	
	// initialize searching procedure
	iResult = WSALookupServiceBegin(&wsaq, 
		LUP_CONTAINERS, 
		&hLookup);
	
	if (iResult != 0)
	{
//		TCHAR tszErr[32];
		iResult = WSAGetLastError();		
		TraceLogThread("BlueTooth", TRUE, "%d:Socket Error: %d\n", __LINE__, iResult);
	}
	
	for (; ;)
	{
		pwsaResults = (LPWSAQUERYSET) ubu.buf;
		
		dwSize  = sizeof(ubu.buf);
		
		memset(pwsaResults,0,sizeof(WSAQUERYSET));
		pwsaResults->dwSize      = sizeof(WSAQUERYSET);
		// namespace MUST be NS_BTH for bluetooth queries
		pwsaResults->dwNameSpace = NS_BTH;
		pwsaResults->lpBlob      = NULL;
		
		// iterate through all found devices, returning name and address
		// (this sample only uses the name, but address could be used for
		// further queries)
		iResult = WSALookupServiceNext (hLookup, 
			LUP_RETURN_NAME | LUP_RETURN_ADDR, 
			&dwSize, 
			pwsaResults);
		
		
		if (iResult != 0)
		{
			iResult = WSAGetLastError();
			if (iResult != WSA_E_NO_MORE)
			{
//				TCHAR tszErr[32];
				iResult = WSAGetLastError();
				TraceLogThread("BlueTooth", TRUE, "%d:Socket Error: %d\n", __LINE__, iResult);
			}
			// we're finished

			break;
		}
		
		// add the name to the listbox
		if (pwsaResults->lpszServiceInstanceName)
		{	TraceLogThread("BlueTooth", TRUE, "%S nProt:%ld nCsA:%ld\n",
							pwsaResults->lpszServiceInstanceName,
							(long) pwsaResults->dwNumberOfProtocols,
							(long) pwsaResults->dwNumberOfCsAddrs);
            if ( ( !_wcsicmp(pwsaResults->lpszServiceInstanceName, pszRemoteName) ) ) {
                //
                // Found a remote bluetooth device with matching name.
                // Get the address of the device and exit the lookup.
                //
                CopyMemory(pRemoteBtAddr,
                           (PSOCKADDR_BTH) pwsaResults->lpcsaBuffer->RemoteAddr.lpSockaddr,
                           sizeof(*pRemoteBtAddr));
                bRemoteDeviceFound = TRUE;
			} else TraceLogThread("BlueTooth", TRUE, "NameToBthAddr:Query found (%S) Need (%S)\n", pwsaResults->lpszServiceInstanceName, pszRemoteName);
#ifdef FOR_INFO_ONLY
typedef struct _WSAQuerySetW
{
    DWORD           dwSize;
    LPWSTR          lpszServiceInstanceName;
    LPGUID          lpServiceClassId;
    LPWSAVERSION    lpVersion;
    LPWSTR          lpszComment;
    DWORD           dwNameSpace;
    LPGUID          lpNSProviderId;
    LPWSTR          lpszContext;
    DWORD           dwNumberOfProtocols;
    LPAFPROTOCOLS   lpafpProtocols;
    LPWSTR          lpszQueryString;
    DWORD           dwNumberOfCsAddrs;
    LPCSADDR_INFO   lpcsaBuffer;
    DWORD           dwOutputFlags;
    LPBLOB          lpBlob;
} WSAQUERYSETW, *PWSAQUERYSETW, *LPWSAQUERYSETW;
typedef struct _CSADDR_INFO {
    SOCKET_ADDRESS LocalAddr ;
    SOCKET_ADDRESS RemoteAddr ;
    INT iSocketType ;
    INT iProtocol ;
} CSADDR_INFO, *PCSADDR_INFO, FAR * LPCSADDR_INFO ;#endif
typedef struct _SOCKET_ADDRESS {
    LPSOCKADDR lpSockaddr ;
    INT iSockaddrLength ;
} SOCKET_ADDRESS, *PSOCKET_ADDRESS, FAR * LPSOCKET_ADDRESS ;
typedef struct sockaddr FAR *LPSOCKADDR;
struct sockaddr {
        u_short sa_family;              /* address family */
        char    sa_data[14];            /* up to 14 bytes of direct address */
};
typedef struct _SOCKADDR_BTH {
  USHORT addressFamily;
  bt_addr btAddr;
  GUID serviceClassId;
  ULONG port;
} SOCKADDR_BTH, *PSOCKADDR_BTH;
#endif
			{	DWORD c;
			for (c=0; c<pwsaResults->dwNumberOfCsAddrs; c++)
			{	switch (pwsaResults->lpcsaBuffer[c].RemoteAddr.lpSockaddr->sa_family)
				{
				case AF_BTH:
				{	SOCKADDR_BTH *sa = (SOCKADDR_BTH *) pwsaResults->lpcsaBuffer[c].RemoteAddr.lpSockaddr;
					TraceLogThread("BlueTooth", TRUE, "[%ld] AF_BT:%ld NAP:%04X SAP:%08X Chan:%ld\n",
									(long) c, (long) sa->addressFamily,
									(long) GET_NAP(sa->btAddr), (long) GET_SAP(sa->btAddr),
									(long) sa->port);
					DiscoverServices(sa);
					break;
				}
				default:
				TraceLogThread("BlueTooth", TRUE, "[%ld] AF:%ld\n",
								(long) c, (long) pwsaResults->lpcsaBuffer[c].RemoteAddr.lpSockaddr->sa_family);
				}
			}
			}
		}
	}
	
	WSALookupServiceEnd(hLookup);
	
    if ( bRemoteDeviceFound ) {
        iResult = TRUE;
		{	SOCKADDR_BTH *RemoteAddr = (SOCKADDR_BTH*)calloc(1,sizeof(*RemoteAddr));
			*RemoteAddr = *pRemoteBtAddr;
#ifdef FUTURE
			TraceLogThread("BlueTooth", TRUE, "RunClientMode(%S)\n", pszRemoteName);
			CloseHandle(CreateThread(NULL, 0, RunClientMode, RemoteAddr, 0, NULL));
#endif
		}

    } else {
        iResult = FALSE;
    }

    return iResult;
}


int ConnectBTSocket(char *Name)
{
    int             iCxnCount = 0;
    SOCKET          LocalSocket = INVALID_SOCKET;
    SOCKADDR_BTH    SockAddrBthServer;
//    HRESULT         res;
	size_t wSize = (strlen(Name)+1)*sizeof(TCHAR);
	TCHAR *wName = (TCHAR*)malloc(wSize);

	if (strrchr(Name,':')) *strrchr(Name,':') = 0;	/* no port */
	StringCbPrintf(wName, wSize, TEXT("%S"), Name);

	if (!NameToBthAddr(wName, (SOCKADDR_BTH*) &SockAddrBthServer))
	{	TraceLogThread("BlueTooth", TRUE, "NameToBthAddr(%S) Failed\n", wName);
		free(wName);
        return INVALID_SOCKET;
	}
	free(wName);	
	
//	SetTraceThreadName(_strdup("RunClientMode"));	/* I know it leaks */
//	SetDefaultTraceName("BlueTooth");

        //
        // Setting address family to AF_BTH indicates winsock2 to use Bluetooth sockets
        // Port should be set to 0 if ServiceClassId is spesified.
        //
        SockAddrBthServer.addressFamily = AF_BTH;
//        SockAddrBthServer.serviceClassId = g_guidServiceClass;
//		SockAddrBthServer.serviceClassId = SerialPortServiceClassID_UUID16;
		SockAddrBthServer.serviceClassId = SerialPortServiceClass_UUID;
        SockAddrBthServer.port = 0;

            TraceLogThread("BlueTooth", TRUE, "socket()\n");

            //
            // Open a bluetooth socket using RFCOMM protocol
            //
            LocalSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
            if ( INVALID_SOCKET == LocalSocket ) {
                TraceLogThread("BlueTooth", TRUE, "=CRITICAL= | socket() call failed. WSAGetLastError = [%d]\n", WSAGetLastError());
				return INVALID_SOCKET;
            }

//#define NEED_PIN
#ifdef NEED_PIN
			BTH_SOCKOPT_SECURITY sockopt;
			sockopt.iLength = 4;
			sockopt.btAddr = btAddr;
			sockopt.caData[0] = '0';
			sockopt.caData[1] = '0';
			sockopt.caData[2] = '0';
			sockopt.caData[3] = '0';
			int iOptLen = sizeof(sockopt);

			if (setsockopt(LocalSocket, SOL_RFCOMM, SO_BTH_SET_PIN , (char*)&sockopt, iOptLen) == SOCKET_ERROR) {
                TraceLogThread("BlueTooth", TRUE, "=CRITICAL= | setsockopt() call failed. WSAGetLastError=[%d]\n", WSAGetLastError());
				closesocket(LocalSocket);
				return INVALID_SOCKET;
			}
#endif

//#define NEED_AUTHENTICATE
#ifdef NEED_AUTHENTICATE
			ULONG ulTrue = TRUE;
			int iOptLen = sizeof(ulTrue);

			TraceLogThread("BlueTooth", TRUE, "setsockopt(BTH_AUTHENTICATE)\n");

			if (setsockopt(LocalSocket, SOL_RFCOMM, SO_BTH_AUTHENTICATE, (char*)&ulTrue, iOptLen) == SOCKET_ERROR) {
                TraceLogThread("BlueTooth", TRUE, "=CRITICAL= | setsockopt(SO_BTH_AUTHENTICATE) call failed. WSAGetLastError=[%d]\n", WSAGetLastError());
				closesocket(LocalSocket);
				return INVALID_SOCKET;
			}
#endif
			//
            // Connect the socket (pSocket) to a given remote socket represented by address (pServerAddr)
            //
            TraceLogThread("BlueTooth", TRUE, "connect()\n");

            if ( SOCKET_ERROR == connect(LocalSocket,
                                         (struct sockaddr *) &SockAddrBthServer,
                                         sizeof(SOCKADDR_BTH)) ) {
                TraceLogThread("BlueTooth", TRUE, "=CRITICAL= | connect() call failed. WSAGetLastError=[%d]\n", WSAGetLastError());
				closesocket(LocalSocket);
				return INVALID_SOCKET;
            }


	return LocalSocket;
}

