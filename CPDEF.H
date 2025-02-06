/*	uf-cpdef.h

      This file contains the required definitions for the Cp (CommPort)
	routines.  Using these routines provides platform independant access
	to RS-232 serial communications.
*/
#ifndef GOT_UF_CPDEF
#define GOT_UF_CPDEF

/*:PORT_F

	This field holds the actual platform-specific handle to the serial
	device.
*/
typedef long PORT_F;

/*:COMM_PORT_S

	This structure holds the required information for an active
	communications port.
*/
typedef struct COMM_PORT_S
{   long     Timeout;        /* Number of seconds timeout */
    long     Retries;        /* Number of times to retry a packet */
    long     UserAbort;      /* Set when user asks for abort */
    long     PacketNumber;   /* Current packet number */
    FILE        *F;             /* Current file */
    char        *FileName;      /* Current filename */

    long     FileSize;       /* Size of file being transferred, -1 if not known */
    long     Transferred;    /* Number of bytes transferred so far */

    /* Other file information may need to be kept here */
    /* See XYSendPacketZero and XYFileWriteOpen for more information */

    /* PORT_TYPE keeps information about the serial port. */
    char	Name[64];	/* Name of port */
    PORT_F      CommPort;       /* Port Handle to use */

    char        **FileNames;    /* Used by XYFileReadOpenNext */
    long     CurrentFileName;
    long     NumFileNames;
} COMM_PORT_S;

/*:XXX_TIMEOUT

	These constants define the type of timeout used by the communications
	layer.
*/
#define NORMAL_TIMEOUT          2
#define WAIT_FOR_SOMETHING      4
#define NO_WAIT_TIMEOUT         6

#endif	/* GOT_UF_CPDEF */
