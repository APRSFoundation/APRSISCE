#ifdef __cplusplus
extern "C"
{
#endif

#define MONITOR_CENTER     0x0001        // center rect to monitor 
#define MONITOR_CLIP     0x0000        // clip rect to monitor 
#define MONITOR_WORKAREA 0x0002        // use monitor work area 
#define MONITOR_AREA     0x0000        // use monitor entire area 

// 
//  ClipOrCenterRectToMonitor 
// 
//  The most common problem apps have when running on a 
//  multimonitor system is that they "clip" or "pin" windows 
//  based on the SM_CXSCREEN and SM_CYSCREEN system metrics. 
//  Because of app compatibility reasons these system metrics 
//  return the size of the primary monitor. 
// 
//  This shows how you use the multi-monitor functions 
//  to do the same thing. 
// 
void GetNearestMonitor(LPRECT prc, UINT flags);	/* MONITOR_(WORK)AREA */
void ClipOrCenterRectToMonitor(LPRECT prc, UINT flags);
#ifdef __cplusplus
void CenterWindow(HWND hwnd, HWND hwndOwner=NULL);
void AlignWindowLeft(HWND hwnd, HWND hwndOwner=NULL);
#else
void CenterWindow(HWND hwnd, HWND hwndOwner);
void AlignWindowLeft(HWND hwnd, HWND hwndOwner);
#endif

#define NO_FONT ((HFONT)0)

HFONT GetMessageBoxFont(void);
HFONT GetFixedFont(void);
HFONT LoadPaintFont(TCHAR *Name, int Size, BOOL Bold);
void FreePaintFont();

int SetMessageBoxTextRect(HWND hwnd, RECT *rc, LPCWSTR lpText, int xMargin, int yMargin);
int SetFixedTextRect(HWND hwnd, RECT *rc, LPCWSTR lpText, int xMargin, int yMargin);

#define MAX_BUTTONS 8
typedef struct BUTTONS_S
{	int ButtonCount;
	struct
	{	int ID;
		HWND hwnd;
		TCHAR *Label;
	} Buttons[MAX_BUTTONS];
	int ButtonHeight;
	int ButtonWidth;
	int ButtonMargin;
	int ButtonDefault;
} BUTTONS_S;

#ifdef __cplusplus
BUTTONS_S *CreateButtons(int DefaultButton = -1);
int AddButton(BUTTONS_S *Buttons, char *Label, int ID, BOOL Default = FALSE);
#else
BUTTONS_S *CreateButtons(int DefaultButton);
int AddButton(BUTTONS_S *Buttons, char *Label, int ID, BOOL Default);
#endif
void InstantiateButtons(BUTTONS_S *Buttons, HWND hwnd);
void FixButtons(BUTTONS_S *Buttons, HWND hwnd, int cxClient, int cyClient);
void FreeButtons(BUTTONS_S *Buttons);

#ifdef __cplusplus
int LwdMessageBox2(HWND hwnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, BUTTONS_S *Buttons, HICON hIcon=NULL);
BOOL LwdMessageBox1(HWND hwnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, char *Button, BOOL Default=FALSE, HICON hIcon=NULL);
#else
int LwdMessageBox2(HWND hwnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, BUTTONS_S *Buttons, HICON hIcon);
BOOL LwdMessageBox1(HWND hwnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, char *Button, BOOL Default, HICON hIcon);
#endif
int LwdMessageBox(HWND hwnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, HICON hIcon);

char *MsgToText(UINT Msg);

#ifdef MessageBox
#undef MessageBox
#endif
#define MessageBox(hwnd,text,caption,type) LwdMessageBox(hwnd,text,caption,type,NULL)

#ifdef __cplusplus
}
#endif
