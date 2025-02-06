#define ECHOLINK_ID_SIZE 10

typedef struct ECHOLINK_INFO_S
{	char ID[ECHOLINK_ID_SIZE];	/* EL-nnnnnn */
	char call[ECHOLINK_ID_SIZE];	/* <call> */
	char *location;		/* <location> */
	char *dblocation;	/* <dblocation> */
	long node;			/* <node> */
	double lat, lon;	/* <lat> <lon> */
	double freq, pl;		/* <freq> <pl> */
	int power, haat, gain, directivity;	/* <power, haat, gain, directivity> */
	char status;		/* <status> */
	char *status_comment;	/* <status_comment> */
	SYSTEMTIME st;		/* From <last_update>MM/DD/YYYY HH:MM</last_update> */

} ECHOLINK_INFO_S;

//extern unsigned long EchoLinkCount;
//extern unsigned long EchoLinkSize;
//extern unsigned long EchoLinkRAM;
//extern ECHOLINK_INFO_S *EchoLinks;

void FreeEchoLinks(HWND hwnd);
unsigned long LoadEchoLinks(HWND hwnd, UINT msgReceived, WPARAM wp);
