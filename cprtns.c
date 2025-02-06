/*      cp-cprtns.c

        This file contains the serial comm port structure
        support routines.

	When	Who		What
	011011	L.Deffenbaugh	Added CpEnableXonXoff
	011127	L.Deffenbaugh	Added CpSetParameters
	011127	L.Deffenbaugh	Re-Adopted from CI
	011129	L.Deffenbaugh	Fixed CpFlush to handle steady stream of incoming data
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpdef.h"

#include "cprtns.h"   /* Local  prototypes */

void cdecl TraceLogThread(char *Name, BOOL ForceIt, char *Format, ...);

/*
        Debugging hooks.  Replace StsWarn with
                #define StsWarn(s)  (s)
        to disable debugging messages.
*/
#define StatusWarn(r,s)  CpPrintError(r,GetLastError(), __FILE__, __LINE__)

#ifndef STRING
#define STRING(s) sizeof(s),s
#endif

static long  CpPrintError
(
		const int						r,
        const int                       s,
        const char                      *file,
        const int                       line
)
{
	TraceLogThread(NULL, TRUE, "Status %ld Error %ld At %ld in %s\n", (long) r, (long) s, line, file);
        /* fprintf(stderr, "!?:%s:%d:", file, line); */
        return -1;
}

/*
        Encapsulates a common idiom:
        Test a return code and return it if there was an error.
*/
#define StatusReturn(e)	{ int rc; if ((rc=(e)) != 0) return StatusWarn(rc, GetLastError()); }

/*:Serial Interface

        This layer consists of routines to interface with the underlying
        operating system and/or application.  These are necessarily
        OS-specific.
*/

/*:CpSetParameters

	This routine sets the port parameters as specified by the
	array of string values specified as Argc and Argv.  These
	strings are:
<pre>
		Argv[0] = Baud Rate
		Argv[1] = Parity (N, O, E, or M)
		Argv[2] = Bits (7 or 8)
		Argv[3] = Stop Bits (1, 1.5, or 2)
</pre>
*/
int  CpSetParameters
(
        COMM_PORT_S                     *pCP,
	long				Argc,	/* 1, 2, 3, or 4 */
	char *			*Argv	/* See comments above */
)
{
	if (Argc)
	{
	static	char Parity[] = "NOEMS";
	static char *StopBits[] = { "1", "1.5", "2" };
	static char *RtsControl[] = { "DISABLE","ENABLE","HANDSHAKE","TOGGLE" };
	static char *DtrControl[] = { "DISABLE","ENABLE","HANDSHAKE" };
		DCB dcb;
		COMMPROP commprop = {0};
		if (GetCommProperties((HANDLE)pCP->CommPort, &commprop))
		{
			TraceLogThread(NULL, FALSE, "%s:Packet Length %ld Version %ld\n", pCP->Name,
				(long) commprop.wPacketLength, (long) commprop.wPacketVersion);
			TraceLogThread(NULL, FALSE, "%s:ServiceMask 0x%lX Reserved1 0x%lX\n", pCP->Name,
				(long) commprop.dwServiceMask, (long) commprop.dwReserved1);
			TraceLogThread(NULL, FALSE, "%s:MaxQueue Tx %ld Rx %ld Baud %ld\n", pCP->Name,
				(long) commprop.dwMaxTxQueue, (long) commprop.dwMaxRxQueue, (long) commprop.dwMaxBaud);
			TraceLogThread(NULL, FALSE, "%s:Provider SubType %ld Capabilities 0x%lX\n", pCP->Name,
				(long) commprop.dwProvSubType, (long) commprop.dwProvCapabilities);
			TraceLogThread(NULL, FALSE, "%s:Settable Params %ld Baud 0x%lX Data 0x%lX StopParity 0x%lX\n", pCP->Name,
				(long) commprop.dwSettableParams, (long) commprop.dwSettableBaud,
				(long) commprop.wSettableData, (long) commprop.wSettableStopParity);
			TraceLogThread(NULL, FALSE, "%s:Current Queue Tx %ld Rx %ld\n", pCP->Name,
				(long) commprop.dwCurrentTxQueue, (long) commprop.dwCurrentRxQueue);
			TraceLogThread(NULL, FALSE, "%s:Provider Specifics 0x%lX 0x%lX\n", pCP->Name,
				(long) commprop.dwProvSpec2, (long) commprop.dwProvSpec2);
		} else TraceLogThread(NULL, TRUE, "GetCommProperties(%s) Failed\n", pCP->Name);

		dcb.DCBlength = sizeof(dcb);
		if (!GetCommState((HANDLE)pCP->CommPort,&dcb))
		{	return FALSE;
		}
		if (Argc > 0) dcb.BaudRate = atol(Argv[0]);
		dcb.fOutxCtsFlow = dcb.fOutxDsrFlow = FALSE;
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
		dcb.fDsrSensitivity = FALSE;

#ifdef SUPPORT_TOGGLE_RTS
		if (!commprop.dwProvCapabilities
		|| (commprop.dwProvCapabilities & PCF_DTRDSR) != 0)
			dcb.fRtsControl = RTS_CONTROL_TOGGLE;
		else dcb.fRtsControl = RTS_CONTROL_ENABLE;

#endif
//		dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
		dcb.fRtsControl = RTS_CONTROL_ENABLE;

		if (Argc > 1)
		{	switch (toupper(*Argv[1]))
			{
			case 'N': dcb.Parity = NOPARITY; break;
			case 'O': dcb.Parity = ODDPARITY; break;
			case 'E': dcb.Parity = EVENPARITY; break;
			case 'M': dcb.Parity = MARKPARITY; break;
			case 'S': dcb.Parity = SPACEPARITY; break;
			default:
				TraceLogThread(NULL, TRUE, "Invalid Parity Setting '%s'\n", Argv[1]);
			}
		}
		if (Argc > 2) dcb.ByteSize = (BYTE) atol(Argv[2]);
		if (Argc > 3)
		{	if (!strcmp(Argv[3],"1"))
				dcb.StopBits = ONESTOPBIT;
			else if (!strcmp(Argv[3],"1.5"))
				dcb.StopBits = ONE5STOPBITS;
			else if (!strcmp(Argv[3],"2"))
				dcb.StopBits = TWOSTOPBITS;
			else TraceLogThread(NULL, TRUE, "Invalid StopBits Setting '%s'\n", Argv[3]);
		}
		if (!SetCommState((HANDLE)pCP->CommPort,&dcb))
		{	TraceLogThread(NULL, TRUE, "CpOpen:SetCommState failed!\n");
			return FALSE;
		}
 else TraceLogThread(NULL, FALSE, "CpOpen:Port %.*s %ld,%c,%ld,%s %s %s %s(%ld) %s(%ld) DTR_%s %s RTS_%s\n",
			STRING(pCP->Name),
			(long) dcb.BaudRate,
			Parity[dcb.Parity],
			(long) dcb.ByteSize,
			StopBits[dcb.StopBits],
			dcb.fOutxCtsFlow?"CTSFlow":"!CTSFlow",
			dcb.fOutxDsrFlow?"DSRFlow":"!DSRFlow",
			dcb.fOutX?"OutXon":"!OutXon",
			(long) dcb.XonLim,
			dcb.fInX?"InXoff":"!InXoff",
			(long) dcb.XoffLim,
			DtrControl[dcb.fDtrControl],
			dcb.fDsrSensitivity?"DSRSense":"!DSRSense",
			RtsControl[dcb.fRtsControl]);
	}
	return TRUE;
}

/*:CpOpen

        Open the serial port.
*/
long  CpOpen
(
        char *                          PortName,
        COMM_PORT_S                     *pCP
)
{	char *Commas[10];	/* Hopefully not more than 10! */
	int CommaCount=0;
	char *colon;
	long Result = 0;

	if (!PortName || !*PortName) return -1;

	PortName = _strdup(PortName);

	colon = strchr(PortName,':');
	if (colon)
	{	char *p;
		*colon++ = '\0';	/* Null terminate comm port name */
		if (*colon)	/* More after the colon, parse the port parameters */
		{	Commas[CommaCount++] = colon;
			for (p=colon; *p; p++)
			{	if (*p == ',')
				{	if (CommaCount < sizeof(Commas)/sizeof(Commas[0]))
						Commas[CommaCount++] = p+1;
					*p = '\0';	/* Null terminate previous string */
				}
			}
		}
	}

	TraceLogThread(NULL, TRUE, "Opening %s with %ld Args\n", PortName, (long) CommaCount);

	strncpy(pCP->Name, PortName, sizeof(pCP->Name));
/*
        Open the Port.
*/
	{	TCHAR *NewPort = malloc((4+strlen(PortName)+1)*sizeof(*NewPort));
#ifdef UNDER_CE
		wsprintf(NewPort,TEXT("%hS:"),PortName);	/* CE doesn't like this! */
#else
		wsprintf(NewPort,TEXT("\\\\.\\%hS"),PortName);	/* For NT's >com9 */
#endif
		pCP->CommPort = (PORT_F) CreateFile(NewPort,
						GENERIC_READ|GENERIC_WRITE,
						0, NULL, OPEN_EXISTING, 0, NULL);
		free(NewPort);
	}

	if (!pCP->CommPort || ((void *) pCP->CommPort == (void *) INVALID_HANDLE_VALUE))
	{	int e = GetLastError();
		TraceLogThread(NULL, TRUE, "Opening %s Got %ld Error %ld\n", PortName, pCP->CommPort, (long) e);
		Result = StatusWarn(0,e);
	} else
	{	if (CommaCount)
		{	if (!CpSetParameters(pCP, CommaCount, Commas))
				TraceLogThread(NULL, TRUE, "SetParameters(%ld pieces) Failed\n", (long) CommaCount);
		}
#ifdef WAIT_FOR_SEND
{	DWORD EvtMask;
	      GetCommMask((HANDLE)pCP->CommPort, &EvtMask);
			EvtMask |= EV_TXEMPTY;
	      SetCommMask((HANDLE)pCP->CommPort, EvtMask);
printf("SetCommMask to include EV_TXEMPTY\n");
}
#endif
	}
		free(PortName);
        return Result;
}

/*:CpDisableXonXoff

        Disable XonXoff on port.
*/
int  CpDisableXonXoff
(
        COMM_PORT_S                     *pCP
)
{
	{
#ifdef DEBUG
	static	char Parity[] = "NOEMS";
	static char *StopBits[] = { "1", "1.5", "2" };
	static char *RtsControl[] = { "DISABLE","ENABLE","HANDSHAKE","TOGGLE" };
	static char *DtrControl[] = { "DISABLE","ENABLE","HANDSHAKE" };
#endif
		DCB dcb;

		dcb.DCBlength = sizeof(dcb);
		if (!GetCommState((HANDLE)pCP->CommPort,&dcb))
		{	return FALSE;
		}
		dcb.fInX = dcb.fOutX = FALSE;
		if (!SetCommState((HANDLE)pCP->CommPort,&dcb))
		{
TraceLogThread(NULL, TRUE, "CpDisableXonXoff:SetCommState(XonXoff failed!\n");
			return FALSE;
		}
#ifdef DEBUG
else TraceLogThread(NULL, FALSE, "CpDisableXonXoff:Port %.*s %ld,%c,%ld,%s %s %s %s(%ld) %s(%ld) DTR_%s %s RTS_%s\n",
				STRING(pCP->Name),
				(long) dcb.BaudRate,
				Parity[dcb.Parity],
				(long) dcb.ByteSize,
				StopBits[dcb.StopBits],
				dcb.fOutxCtsFlow?"CTSFlow":"!CTSFlow",
				dcb.fOutxDsrFlow?"DSRFlow":"!DSRFlow",
				dcb.fOutX?"OutXon":"!OutXon",
				(long) dcb.XonLim,
				dcb.fInX?"InXoff":"!InXoff",
				(long) dcb.XoffLim,
				DtrControl[dcb.fDtrControl],
				dcb.fDsrSensitivity?"DSRSense":"!DSRSense",
				RtsControl[dcb.fRtsControl]);
#endif
		return TRUE;
	}
}

/*:CpEnableXonXoff

        Disable XonXoff on port.
*/
int  CpEnableXonXoff
(
        COMM_PORT_S 	*pCP,
	int	In,	/* TRUE to enable */
	int	Out	/* TRUE to enable */
)
{
	{
#ifdef DEBUG
	static	char Parity[] = "NOEMS";
	static char *StopBits[] = { "1", "1.5", "2" };
	static char *RtsControl[] = { "DISABLE","ENABLE","HANDSHAKE","TOGGLE" };
	static char *DtrControl[] = { "DISABLE","ENABLE","HANDSHAKE" };
#endif
		DCB dcb;

		dcb.DCBlength = sizeof(dcb);
		if (!GetCommState((HANDLE)pCP->CommPort,&dcb))
		{	return FALSE;
		}
		dcb.fInX = In;
		dcb.fOutX = Out;
		if (!SetCommState((HANDLE)pCP->CommPort,&dcb))
		{
TraceLogThread(NULL, TRUE, "CpEnableXonXoff:SetCommState(XonXoff failed!\n");
			return FALSE;
		}
#ifdef DEBUG
else TraceLogThread(NULL, FALSE, "CpEnableXonXoff:Port %.*s %ld,%c,%ld,%s %s %s %s(%ld) %s(%ld) DTR_%s %s RTS_%s\n",
				STRING(pCP->Name),
				(long) dcb.BaudRate,
				Parity[dcb.Parity],
				(long) dcb.ByteSize,
				StopBits[dcb.StopBits],
				dcb.fOutxCtsFlow?"CTSFlow":"!CTSFlow",
				dcb.fOutxDsrFlow?"DSRFlow":"!DSRFlow",
				dcb.fOutX?"OutXon":"!OutXon",
				(long) dcb.XonLim,
				dcb.fInX?"InXoff":"!InXoff",
				(long) dcb.XoffLim,
				DtrControl[dcb.fDtrControl],
				dcb.fDsrSensitivity?"DSRSense":"!DSRSense",
				RtsControl[dcb.fRtsControl]);
#endif
		return TRUE;
	}
}

/*:CpClose

        Close the serial port.
*/
long  CpClose
(
        COMM_PORT_S                     *pCP
)
{
	if (!CloseHandle((HANDLE)pCP->CommPort))
		StatusReturn(-1);
        return 0;
}

/*:CpFlush

        Read characters from serial port until indicated timeout occurs.
        CpFlush(0) simply flushes the receive queue.
*/
long  CpFlush
(	COMM_PORT_S                     *pCP,
	long                         Timeout
)
{
        char                            Junk[256];
        long                         BytesRead;

        do
        {	BytesRead = 0;
		StatusReturn(CpReadBytesWithTimeout(pCP, Timeout,
							Junk, sizeof(Junk),
							&BytesRead));
	} while (BytesRead >= sizeof(Junk));

        return 0;
}

/*
  Note 1

  Write Timeout.  Bit 0 in Flags3 controls the characteristics of Write Timeout processing.  If the
  bit is 0, Write Timeout processing uses the value in the Write Timeout WORD in the device
  control block.  If the bit is 1, Write Timeout processing is infinite timeout.

  The value in the Write Timeout WORD is in .01 second units, based on 0 (where 0 = .01
  seconds).  The physical device driver is considered to be doing Normal Write Timeout processing
  when the Write Timeout WORD is used.

  Note 2

  Read Timeout.  Bits 2, 1 of Flags3 control the Read Timeout processing characteristics of the
  physical device driver.  The three possible types of Read Timeout processing are:

  Normal               Bits 2, 1 = 0, 1
  Wait-For-Something   Bits 2, 1 = 1, 0
  No-Wait              Bits 2, 1 = 1, 1

  The value in the Read Timeout WORD is in .01 second units, based on 0 (where 0 = .01
  seconds).  The physical device driver uses the value in the Read Timeout WORD for Normal and
  Wait-For-Something Read Timeout processing.  The accuracy of the time interval can be
  determined by the request, which is blocked in the physical device driver, or by the device
  driver timer ticks.

  If the physical device driver is doing Normal Read Timeout processing, the device driver waits
  for the amount of time specified in the Read Timeout WORD. The request is completed after
  that interval of time elapses if no more data has been received for the request.  If any data is
  received by the physical device driver from the receive hardware for the request (including
  XON/XOFF characters), it waits the specified period of time is for more data to arrive.  However,
  in the following two cases, the current interval of time will continue to be waited on without
  starting to wait from the beginning of the interval again:

    o  If input sensitivity using DSR is enabled and the value of the DSR modem control signal
       causes input data to be thrown away.  See Note 4.

    o  If null stripping is enabled and a null character is stripped. See Note 6.

  If the physical device driver is doing No-Wait Read Timeout processing, it does not wait for any
  data to be available in the receive queue. When the physical device driver begins to try to
  move data from the receive queue to the request, the request is completed.  Whatever data is
  available in the receive queue at that time is moved to the request.

  If the physical device driver is doing Wait-For-Something Read Timeout processing, the physical
  device driver processes the request initially as if it had No-Wait Timeout processing.  If no data
  was available at the time the request would have completed due to No-Wait processing, the
  request is not completed.  Instead, it waits for some data to be available before completing the
  request.  However, the physical device driver does enter Normal Read Timeout processing for
  this request.  Therefore, if no data is available after the Normal Timeout processing interval,
  then the request is completed anyway.  The request never waits longer than it would have due
  to Normal Read Timeout processing.

  The Read Timeout processing characteristics that apply to a given Read request are not
  determined until the physical device driver begins processing that request.  At that time, a
  change to the Read Timeout processing characteristics of the physical device driver between
  Wait-For-Something and Normal Timeout processing might take effect for the current Read
  request being processed.  If the timeout period is changed by this IOCtl, the new timeout period
  might take effect immediately or it might take effect after the next character is received from
  the receive hardware.  When the physical device driver is initialized, Normal Read Timeout
  processing is in effect.

  When the physical device driver receives an OPEN request packet for the port and the port is
  not already open, the value in the Read Timeout WORD is set to one minute and Normal Read
  Timeout processing characteristics are put into effect.

*/

/*:CpSetTimeout

        Set the timeout value for the serial port.
        The valid timeout states are:
                NORMAL_TIMEOUT       Bits 2, 1 = 0, 1
                WAIT_FOR_SOMETHING   Bits 2, 1 = 1, 0
                NO_WAIT_TIMEOUT      Bits 2, 1 = 1, 1

        Error Handling:
                * returns -1 for Dos return code error
                * returns  0 otherwise
*/
long  CpSetTimeout
(
        COMM_PORT_S                     *pCP,
        long                         TimeoutState,
        long                         TimeoutValue,
		long							TimeoutInterval	/* Only used for WAIT_FOR_SOMETHING */
)
{
{	COMMTIMEOUTS ct = {0};	/* Initialize it all to zero */
	switch (TimeoutState)
	{
	case NORMAL_TIMEOUT:
		ct.ReadTotalTimeoutConstant = TimeoutValue;
		break;
	case WAIT_FOR_SOMETHING:
		ct.ReadTotalTimeoutConstant = TimeoutValue;
		ct.ReadIntervalTimeout = TimeoutInterval;	/* This is an attempt */
		break;
	case NO_WAIT_TIMEOUT:
		ct.ReadIntervalTimeout = MAXDWORD;
		break;
	default:
		TraceLogThread(NULL, TRUE, "CpSetTimeout:Unsupported TimeoutState %ld\n",
			(long) TimeoutState);
	}
	if (!SetCommTimeouts((HANDLE) pCP->CommPort, &ct))
		StatusReturn(-1);
}

	return 0;
}

/*:CpReadBytesWithTimeout

        Read bytes from serial port into buffer, ending when the buffer is
        filled or there is a delay longer than `timeout' between bytes.

        We accept a timeout parameter rather than using the timeout value
        in the COMM_PORT_S structure because at certain points in the protocol
        different timeouts apply.  These other timeouts are computed from the
        timeout value in the COMM_PORT_S structure.

        Error Handling:
                * returns -1 for Dos return code error
                * returns  0 otherwise
*/
long  CpReadBytesWithTimeout
(
        COMM_PORT_S                     *pCP,
        long                         Timeout,
        char *                        InBuffer,
        long                         InLength,
        long                         *RetBytesRead
)
{
        unsigned long                   BytesRead = 0;  /* Number of bytes read by DosRead */

	*RetBytesRead = 0;		/* So it is initialized */

        if (pCP->UserAbort)
                return StatusWarn(0, cpFail);
/*
        Get bytes from serial port
*/
       StatusReturn(CpSetTimeout(pCP, WAIT_FOR_SOMETHING, pCP->Timeout, Timeout));

	if (!ReadFile((HANDLE)pCP->CommPort, InBuffer, InLength,
				(LPDWORD) &BytesRead, NULL))
		StatusReturn(-1);
        *RetBytesRead = BytesRead;

#ifdef UNNECESSARY
		if ((long) BytesRead < InLength)
        {       unsigned long BytesAlreadyRead = BytesRead;

                StatusReturn(CpSetTimeout(pCP, NORMAL_TIMEOUT, Timeout, 1));

		if (!ReadFile((HANDLE)pCP->CommPort, InBuffer+BytesAlreadyRead, InLength-BytesAlreadyRead,
				(LPDWORD) &BytesRead, NULL))
			StatusReturn(-1);
                *RetBytesRead += BytesRead;
        }
#endif
		return 0;
}

/*:CpSendBytes

        Send the bytes through the serial port.  If possible, return
        as soon as the bytes are queued, and implement CpWaitForSentBytes()
        to wait until the bytes are actually sent.

        Error Handling:
                * returns -1 for Dos return code error
                * returns  0 otherwise
*/

long  CpSendBytes
(
        COMM_PORT_S                     *pCP,
        char *                        OutBuffer,
        long                         OutLength,
        long                         *RetBytesSent
)
{
        unsigned long                   BytesSent = 0;

	*RetBytesSent = 0;		/* So it is initialized */

        if (pCP->UserAbort)
                return StatusWarn(0, cpFail);
/*
        Send bytes
*/
	if (!WriteFile((HANDLE)pCP->CommPort, OutBuffer, OutLength,
			&BytesSent, NULL))
		StatusReturn(-1);
#ifdef WAIT_FOR_SEND
{  DWORD EvtMask = 0;
   MILLISECS_F Start = RtGetMsec();
   do
   {  	if (!WaitCommEvent((HANDLE)pCP->CommPort, &EvtMask, NULL))
   	{	break;
        }
   } while ((EvtMask & EV_TXEMPTY) == 0);
   Start = RtGetMsec() - Start;
   if ((long)Start > 1) printf("Wait Took %ld msec\n", (long) Start);
}
#endif
        *RetBytesSent = BytesSent;

        return 0;
}

/*:CpWaitForSentBytes

        Delay until the send queue is empty.  This allows the protocol to
        exploit the time during which the outgoing serial queue is being emptied
        while still providing accurate timing.

        This ality is not available on all systems.  It can be
        left as a stub with no ill effect except at very slow speeds, where
        the timeout interval should probably be enlarged.

        Error Handling:
                * returns -1 for Dos return code error
                * returns  0 otherwise
*/
long  CpWaitForSentBytes
(
        COMM_PORT_S                     *pCP
)
{
        if (pCP->UserAbort)
                return StatusWarn(0, cpFail);
/*
        Wait for send buffer to clear
*/
        return 0;
}

/*:CpSendByte

        Send a single byte.
*/
long  CpSendByte
(
        COMM_PORT_S                     *pCP,
        char *                        Byte
)
{
        long                         BytesSent;
        StatusReturn(CpSendBytes(pCP, Byte, sizeof(char), &BytesSent));
        return 0;
}

/*:CpSendAndRead

        Send an optional output sting to the comm port with an optional
        input string.
*/
long  CpSendAndRead
(
        COMM_PORT_S                     *pCP,
        char *                        OutBuffer,
        long                         OutLength,
        long                         *RetBytesSent,
        long                         Timeout,
        char *                        InBuffer,
        long                         InLength,
        long                         *RetBytesRead
)
{
        long                         BytesSent = 0;
        long                         BytesRead = 0;

	*RetBytesRead = *RetBytesSent = 0;	/* So it is initialized */

        if (OutLength > 0)
        {
                StatusReturn(CpFlush(pCP, 0));
                StatusReturn(CpSendBytes(pCP, OutBuffer, OutLength, &BytesSent));
        }

        *RetBytesSent = BytesSent;
        if (InLength > 0)
                StatusReturn(CpReadBytesWithTimeout(pCP, Timeout, InBuffer, InLength, &BytesRead));

        *RetBytesRead = BytesRead;
        return 0;
}

/*:CpInit

        Currently, this simply returns a pointer to a static structure.
        If multiple simultaneous transfers are needed, then this can
        be changed to dynamically allocate the structure.
*/
COMM_PORT_S * CpInit
(
        long                         Timeout,
        char *                          PortName,
	COMM_PORT_S			*pCP
)
{
static	COMM_PORT_S              cp;

	if (!pCP) pCP = &cp;

        memset(pCP, 0, sizeof(*pCP));   /* Initialize structure */

        pCP->Timeout  = Timeout;        /* This is the Initial Timeout value */
        pCP->Retries  = 10;

        if (CpOpen(PortName, pCP) == 0) /* Open the comm port */
		return pCP;
	else return NULL;		/* Failed to open! */
}
/*:CpModemAbort

        Set the abort flag and return.
*/
long  CpModemAbort
(
        void                            *pCP_public
)
{
        COMM_PORT_S                     *pCP = pCP_public;
        pCP->UserAbort = TRUE;

        return 0;
}

