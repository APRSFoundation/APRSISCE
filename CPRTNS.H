/* Prototypes generated Tue Nov 27 11:33:28 2001 */
#ifdef __cplusplus
extern "C"
{
#endif
int CpSetParameters(    COMM_PORT_S           *pCP, long  Argc, char * 	*Argv );
long CpOpen(    char *             PortName,    COMM_PORT_S           *pCP);
int CpDisableXonXoff(    COMM_PORT_S           *pCP);
int CpEnableXonXoff(    COMM_PORT_S *pCP, int In, int Out );
long CpClose(    COMM_PORT_S           *pCP);
long CpFlush( COMM_PORT_S           *pCP, long             Timeout);
long CpSetTimeout(    COMM_PORT_S           *pCP,    long             TimeoutState,    long             TimeoutValue, long TimeoutInterval);
long CpReadBytesWithTimeout(    COMM_PORT_S           *pCP,    long             Timeout,    char *            InBuffer,    long             InLength,    long             *RetBytesRead);
long CpSendBytes(    COMM_PORT_S           *pCP,    char *            OutBuffer,    long             OutLength,    long             *RetBytesSent);
long CpWaitForSentBytes(    COMM_PORT_S           *pCP);
long CpSendByte(    COMM_PORT_S           *pCP,    char *            Byte);
long CpSendAndRead(    COMM_PORT_S           *pCP,    char *            OutBuffer,    long             OutLength,    long             *RetBytesSent,    long             Timeout,    char *            InBuffer,    long             InLength,    long             *RetBytesRead);
COMM_PORT_S * CpInit(    long             Timeout,    char *             PortName, COMM_PORT_S 	*pCP);
long CpModemAbort(    void              *pCP_public);
#ifdef __cplusplus
}
#endif
