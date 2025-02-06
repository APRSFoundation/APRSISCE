#ifdef _DEBUG
#include <crtdbg.h>
#endif

#pragma warning(disable : 4995)

#define INITGUID
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <strsafe.h>
#ifdef UNDER_CE
#ifndef NO_SHELL
#include <aygshell.h>
#endif
#endif

#define TRACE_CRITICAL FALSE


#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "MsgBox.h"

#include "config.h"		/* For ActiveConfig */
#include "llutil.h"	/* For UTF8 conversion */
#include "resource.h"	/* for IDI_MY_ICON */
#include "tcputil.h"	/* For TraceLog* */

#define FONT_COUNT 16

LOGFONT   g_lfPaint[FONT_COUNT] = {0};
HFONT     g_hFontPaint[FONT_COUNT] = {0};
extern DWORD     g_dwFontSize;
extern HWND gModelessDialog;

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
void GetNearestMonitor(LPRECT prc, UINT flags)	/* MONITOR_(WORK)AREA */
{
    HMONITOR hMonitor;
    MONITORINFO mi;
    // 
    // get the nearest monitor to the passed rect. 
    // 
    hMonitor = MonitorFromRect(prc, MONITOR_DEFAULTTONEAREST);

    // 
    // get the work area or entire monitor rect. 
    // 
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);

    if (flags & MONITOR_WORKAREA)
        *prc = mi.rcWork;
    else
        *prc = mi.rcMonitor;
//	TraceLogThread("MessageBox",TRACE_CRITICAL,"Monitor:%s:%ld %ld -> %ld %ld\n",
//				flags&MONITOR_WORKAREA?"WorkArea":"Area",
//				prc->left, prc->top, prc->right, prc->bottom);
}

void ClipOrCenterRectToMonitor(LPRECT prc, UINT flags)
{
    HMONITOR hMonitor;
    MONITORINFO mi;
    RECT        rc;
    int         w = prc->right  - prc->left;
    int         h = prc->bottom - prc->top;
	int			dx = 0, dy = 0;
    // 
    // get the nearest monitor to the passed rect. 
    // 
    hMonitor = MonitorFromRect(prc, MONITOR_DEFAULTTONEAREST);

    // 
    // get the work area or entire monitor rect. 
    // 
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);

	TraceLogThread("Activity",TRUE,"Monitor:%ld %ld -> %ld %ld WorkArea:%ld %ld -> %ld %ld\n",
				mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right, mi.rcMonitor.bottom,
				mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom);

	if (flags & MONITOR_WORKAREA)
	{	rc = mi.rcWork;
		dx = mi.rcWork.left - mi.rcMonitor.left;
		dy = mi.rcWork.top - mi.rcMonitor.top;
	} else rc = mi.rcMonitor;
    // 
    // center or clip the passed rect to the monitor rect 
    // 
    if (flags & MONITOR_CENTER)
    {
        prc->left   = rc.left + (rc.right  - rc.left - w) / 2;
        prc->top    = rc.top  + (rc.bottom - rc.top  - h) / 2;
        prc->right  = prc->left + w;
        prc->bottom = prc->top  + h;
    }
    else
    {
		prc->left   = max(rc.left, min(rc.right-w,  prc->left)+dx);
        prc->top    = max(rc.top,  min(rc.bottom-h, prc->top)+dy);
        prc->right  = prc->left + w;
        prc->bottom = prc->top  + h;
	}
}

#ifdef FUTURE
void ClipOrCenterWindowToMonitor(HWND hwnd, UINT flags)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    ClipOrCenterRectToMonitor(&rc, flags);
    SetWindowPos(hwnd, NULL, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
#endif

void CenterWindow(HWND hwnd, HWND hwndOwner)
{	RECT rc, rcDlg, rcOwner; 

	if (hwndOwner == NULL)
	if ((hwndOwner = GetParent(hwnd)) == NULL) 
    {
//TraceLogThread("MessageBox", TRACE_CRITICAL, "GetParent() Returned NULL, Centering on Desktop\n");
		hwndOwner = GetDesktopWindow(); 
    }

    GetWindowRect(hwndOwner, &rcOwner); 
    GetWindowRect(hwnd, &rcDlg); 
    CopyRect(&rc, &rcOwner); 

    // Offset the owner and dialog box rectangles so that right and bottom 
    // values represent the width and height, and then offset the owner again 
    // to discard space taken up by the dialog box. 

    OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top); 
    OffsetRect(&rc, -rc.left, -rc.top);
    OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom); 

	RECT rcMonitor;
	rcMonitor.left = rcOwner.left + (rc.right / 2);
	rcMonitor.top = rcOwner.top + (rc.bottom / 2);
	rcMonitor.right = rcMonitor.left + rcDlg.right;
	rcMonitor.bottom = rcMonitor.top + rcDlg.bottom;

	// Make sure it is on screen
	ClipOrCenterRectToMonitor(&rcMonitor, MONITOR_CLIP | MONITOR_WORKAREA);
	
	// The new position is the sum of half the remaining space and the owner's 
    // original position. 

//	TraceLogThread("MessageBox", TRACE_CRITICAL, "Center:@%ld %ld\n",
//					rcMonitor.left, rcMonitor.top);
    SetWindowPos(hwnd, 
                 HWND_TOP, rcMonitor.left, rcMonitor.top,
                 0, 0,          // Ignores size arguments. 
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE); 
}

void AlignWindowLeft(HWND hwnd, HWND hwndOwner)
{	RECT rc, rcDlg, rcOwner; 

	if (hwndOwner == NULL)
	if ((hwndOwner = GetParent(hwnd)) == NULL) 
    {
//TraceLogThread("MessageBox", TRACE_CRITICAL, "GetParent() Returned NULL, Centering on Desktop\n");
		hwndOwner = GetDesktopWindow(); 
    }

    GetWindowRect(hwndOwner, &rcOwner); 
    GetWindowRect(hwnd, &rcDlg); 
    CopyRect(&rc, &rcOwner); 

    // Offset the owner and dialog box rectangles so that right and bottom 
    // values represent the width and height, and then offset the owner again 
    // to discard space taken up by the dialog box. 

    OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top); 
    OffsetRect(&rc, -rc.left, -rc.top);
    OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom); 

	RECT rcMonitor;
	rcMonitor.left = rcOwner.left;
	rcMonitor.top = rcOwner.top + (rc.bottom / 2);
	rcMonitor.right = rcMonitor.left + rcDlg.right;
	rcMonitor.bottom = rcMonitor.top + rcDlg.bottom;

	// Make sure it is on screen
	ClipOrCenterRectToMonitor(&rcMonitor, MONITOR_CLIP | MONITOR_WORKAREA);
	
	// The new position is the sum of half the remaining space and the owner's 
    // original position. 

//	TraceLogThread("MessageBox", TRACE_CRITICAL, "Center:@%ld %ld\n",
//					rcMonitor.left, rcMonitor.top);
    SetWindowPos(hwnd, 
                 HWND_TOP, rcMonitor.left, rcMonitor.top,
                 0, 0,          // Ignores size arguments. 
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE); 
}

//#define LWD_DT (DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK)
#define LWD_DT (DT_LEFT | DT_TOP | DT_NOPREFIX | DT_EXPANDTABS | DT_WORDBREAK)

#ifdef FOR_INFO_ONLY
typedef struct tagNONCLIENTMETRICS {
  UINT    cbSize;
  int     iBorderWidth;
  int     iScrollWidth;
  int     iScrollHeight;
  int     iCaptionWidth;
  int     iCaptionHeight;
  LOGFONT lfCaptionFont;
  int     iSmCaptionWidth;
  int     iSmCaptionHeight;
  LOGFONT lfSmCaptionFont;
  int     iMenuWidth;
  int     iMenuHeight;
  LOGFONT lfMenuFont;
  LOGFONT lfStatusFont;
  LOGFONT lfMessageFont;
#if (WINVER >= 0x0600)
  int     iPaddedBorderWidth;
#endif 
} NONCLIENTMETRICS, *PNONCLIENTMETRICS, *LPNONCLIENTMETRICS;
#endif

#define NO_FONT ((HFONT)0)

HFONT GetMessageBoxFont(void)
{	HFONT hResult = NO_FONT;
#ifndef UNDER_CE
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(ncm);
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
	{	hResult = CreateFontIndirect(&ncm.lfMessageFont);
	}
#endif
	return hResult;
}

HFONT LoadPaintFont(TCHAR *Name, int Size, BOOL Bold)
{	int f, a=-1;
	LOGFONT lf = {0};

	if (!Size) Size = g_dwFontSize;

	wcsncpy(lf.lfFaceName, Name, ARRAYSIZE(lf.lfFaceName));
	lf.lfHeight = -(LONG)Size;
	lf.lfWeight = Bold?FW_HEAVY:FW_NORMAL;
#ifdef FOR_INFO_ONLY
FW_BOLD 	700
FW_EXTRABOLD 	800
FW_ULTRABOLD 	800
FW_HEAVY 	900
FW_BLACK 	900
#endif

	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;

	for (f=0; f<FONT_COUNT; f++)
	{	if (!g_hFontPaint[f]) a = f;
		else if (!wcsncmp(g_lfPaint[f].lfFaceName, lf.lfFaceName, ARRAYSIZE(lf.lfFaceName))
		&& g_lfPaint[f].lfHeight == lf.lfHeight
		&& g_lfPaint[f].lfWeight == lf.lfWeight)
			break;
	}
	if (f >= FONT_COUNT)
	{	if (a != -1)
		{	f = a;
			g_lfPaint[f] = lf;
			g_hFontPaint[f] = CreateFontIndirect(&lf);
			TraceActivity(NULL, "New Font %S[%ld] at %ld/%ld is %ld%s\n",
				Name, (long) Size, (long) FONT_COUNT-(f+1), (long) FONT_COUNT, (long) g_hFontPaint[f], Bold?" BOLD":"");
		} else
		{	TraceError(NULL, "No Room For Font (max %ld)!\n", (long) FONT_COUNT);
			return NULL;
		}
	}

	// Return the cached font handle
	return g_hFontPaint[f];
}

void FreePaintFont()
{	int f;
	for (f=0; f<FONT_COUNT; f++)
	if (g_hFontPaint[f] != NULL)
	{
		DeleteObject(g_hFontPaint[f]);
		g_hFontPaint[f] = NULL;
	}
}

HFONT GetFixedFont(void)
{	HFONT hResult = NO_FONT;
#ifdef UNDER_CE
		return LoadPaintFont(TEXT("Courier New"), 0, FALSE);
#else
		return LoadPaintFont(TEXT("FixedSys"), 0, FALSE);
#endif
}

int SetMessageBoxTextRect(HWND hwnd, RECT *rc, LPCWSTR lpText, int xMargin, int yMargin)
{	int Result;

	HDC hdc = GetDC(hwnd);
	HFONT hOld = NO_FONT, hFont = GetMessageBoxFont();
	if (hFont != NO_FONT) hOld = (HFONT) SelectObject(hdc, hFont);

	rc->right -= xMargin * 2;	/* Save the space */
	Result = DrawText(hdc, lpText, -1, rc, LWD_DT | DT_CALCRECT);
//	TraceLogThread("MessageBox", TRACE_CRITICAL, "Size:%ld %ld -> %ld %ld (%ld High)\n",
//					rc->left, rc->top, rc->right, rc->bottom, Result);
	if (hFont != NO_FONT) DeleteObject(SelectObject(hdc, hOld));
	ReleaseDC(hwnd, hdc);
	rc->right += xMargin*2;	/* Put the margin back in */
	Result += yMargin*2;
	return Result;
}

int SetFixedTextRect(HWND hwnd, RECT *rc, LPCWSTR lpText, int xMargin, int yMargin)
{	int Result;

	HDC hdc = GetDC(hwnd);
	HFONT hOld = NO_FONT, hFont = GetFixedFont();
	if (hFont != NO_FONT) hOld = (HFONT) SelectObject(hdc, hFont);

	rc->right -= xMargin * 2;	/* Save the space */
	Result = DrawText(hdc, lpText, -1, rc, LWD_DT | DT_CALCRECT);
	TraceLogThread("Chat", FALSE, "Size(%S):%ld %ld -> %ld %ld (%ld High) hwnd(0x%lX) Error(%ld)\n",
					lpText, rc->left, rc->top, rc->right, rc->bottom, Result, hwnd, GetLastError());
	if (hFont != NO_FONT) SelectObject(hdc, hOld);
	ReleaseDC(hwnd, hdc);
	rc->right += xMargin*2;	/* Put the margin back in */
	Result += yMargin*2;
	return Result;
}

BUTTONS_S *CreateButtons(int DefaultButton)
{	BUTTONS_S *Buttons = (BUTTONS_S *)calloc(1,sizeof(BUTTONS_S));
	Buttons->ButtonDefault = DefaultButton;
	return Buttons;
}

int AddButton(BUTTONS_S *Buttons, char *Label, int ID, BOOL Default)
{	if (Buttons->ButtonCount < MAX_BUTTONS)
	{	int b = Buttons->ButtonCount++;
		size_t len = (strlen(Label)+8)*sizeof(TCHAR);
		Buttons->Buttons[b].ID = ID;
		Buttons->Buttons[b].Label = (TCHAR *)malloc(len);
//		StringCbPrintf(Buttons->Buttons[b].Label, len, TEXT("%S"), Label);
		StringCbPrintExUTF8(Buttons->Buttons[b].Label, len, NULL, NULL,
							strlen(Label), Label, NULL);
		if (Default) Buttons->ButtonDefault = Buttons->ButtonCount;
		return b;
	}
	return MAX_BUTTONS;
}

void InstantiateButtons(BUTTONS_S *Buttons, HWND hwnd)
{
	Buttons->ButtonWidth = 0;
	Buttons->ButtonHeight = 0;
	if (Buttons->ButtonCount)
	{	int b;
		if (Buttons->ButtonCount == 1)
			Buttons->ButtonDefault = 0;
		for (b=0; b<Buttons->ButtonCount; b++)
		{	RECT rc;
			SetRect(&rc, 0, 0, 32767, 32767);
			SetMessageBoxTextRect(hwnd, &rc, Buttons->Buttons[b].Label, 0, 0);
			OffsetRect(&rc, -rc.left, -rc.top);
			if (rc.right > Buttons->ButtonWidth)
				Buttons->ButtonWidth = rc.right;
			if (rc.bottom > Buttons->ButtonHeight)
				Buttons->ButtonHeight = rc.bottom;
		}

		Buttons->ButtonWidth = (int)(Buttons->ButtonWidth*2);
		Buttons->ButtonMargin = (int)(Buttons->ButtonHeight*0.6)/2;
		Buttons->ButtonHeight = (int)(Buttons->ButtonHeight*1.6);
		for (b=0; b<Buttons->ButtonCount; b++)
		{	Buttons->Buttons[b].hwnd = CreateWindow(TEXT("button"),
					Buttons->Buttons[b].Label,
					WS_CHILD | WS_VISIBLE
					| (b==Buttons->ButtonDefault&&FALSE?BS_DEFPUSHBUTTON:BS_PUSHBUTTON),
					0, 0, Buttons->ButtonWidth, Buttons->ButtonHeight,
					hwnd, (HMENU) Buttons->Buttons[b].ID,
					g_hInstance, NULL);
//			if (b == Buttons->ButtonDefault)
//				SetFocus(Buttons->Buttons[b].hwnd);
		}
	}
}

void FixButtons(BUTTONS_S *Buttons, HWND hwnd, int cxClient, int cyClient)
{
	if (Buttons && Buttons->ButtonCount)
	{	int b, lm, rm, but, gap;
		RECT rc;

		GetWindowRect(Buttons->Buttons[0].hwnd,&rc);
		but = rc.right - rc.left;	/* Width of a button */

TraceLogThread("MessageBox", TRACE_CRITICAL, "hwnd: %p FixButtons(%ld)\n", hwnd, Buttons->ButtonCount);

		gap = (cxClient-but*Buttons->ButtonCount) / (Buttons->ButtonCount+1);
		lm = rm = gap;

		for (b=0; b<Buttons->ButtonCount; b++)
		{	HWND hwndButton = Buttons->Buttons[b].hwnd;
TraceLogThread("MessageBox", TRACE_CRITICAL, "hwnd: %p hwndButton[%ld]%p to %ld %ld\n", hwnd, b, hwndButton, (b%Buttons->ButtonCount)*(gap+but)+lm, (cyClient-(b/Buttons->ButtonCount+1)*(rc.bottom-rc.top))-Buttons->ButtonMargin);
			SetWindowPos(hwndButton, HWND_TOP,
					(b%Buttons->ButtonCount)*(gap+but)+lm,
					(cyClient-(b/Buttons->ButtonCount+1)*(rc.bottom-rc.top))-Buttons->ButtonMargin,
					0, 0, SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
TraceLogThread("MessageBox", TRACE_CRITICAL, "hwnd: %p hwndButton[%ld]%p at %ld %ld\n", hwnd, b, hwndButton, (b%Buttons->ButtonCount)*(gap+but)+lm, (cyClient-(b/Buttons->ButtonCount+1)*(rc.bottom-rc.top))-Buttons->ButtonMargin);
		}
	}
}

void FreeButtons(BUTTONS_S *Buttons)
{	for (int b=0; b<Buttons->ButtonCount; b++)
		free(Buttons->Buttons[b].Label);
	free(Buttons);
}

typedef struct LWD_MSG_INFO_S
{	LPCWSTR lpText;
	LPCWSTR lpCaption;
	UINT uType;
	HWND hwndOwner;	/* NULL if non-modal */
	int Result;		/* Result for modal boxes */
	BUTTONS_S *Buttons;
	RECT rcText;
	int cxClient, cyClient;
	int iVscrollPos, iHscrollPos;
	int iVscrollMax, iHscrollMax;
	int cxChar, cxCaps, cyChar, cyText;
} LWD_MSG_INFO_S;

LRESULT CALLBACK LwdMsgWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	LWD_MSG_INFO_S *Info;
static int usSysLineHeight, usSysDescender, usSysCharWidth;

//if (TraceMessages) TraceLogThread("WinMsg", TRUE, "hwndMsg: %lX msg: %s(%lX) wp: %lX lp: %lX\n", (long) hwnd, MsgToText(iMsg), (long) iMsg, (long) wParam, (long) lParam);
TraceLogThread("MessageBox", TRACE_CRITICAL, "%p: hwndMsg: %lX msg: %s(%lX) wp: %lX lp: %lX\n", &hwnd, (long) hwnd, MsgToText(iMsg), (long) iMsg, (long) wParam, (long) lParam);
	switch (iMsg)
	{
	case WM_CREATE:
	{	CREATESTRUCT *cs = (CREATESTRUCT *) lParam;
		Info = (LWD_MSG_INFO_S *)cs->lpCreateParams;

		HDC hdc = GetDC(hwnd);
		TEXTMETRIC tm;

		GetTextMetrics(hdc, &tm);
		usSysLineHeight = tm.tmHeight + tm.tmExternalLeading;
		usSysDescender = tm.tmDescent;
		usSysCharWidth = tm.tmAveCharWidth;

		HFONT hOld = NO_FONT, hFont = GetMessageBoxFont();
		if (hFont != NO_FONT) hOld = (HFONT) SelectObject(hdc, hFont);

		GetTextMetrics(hdc, &tm);
		Info->cxChar = tm.tmAveCharWidth;
		Info->cxCaps = (tm.tmPitchAndFamily & 1 ? 3 : 2) * Info->cxChar / 2;
		Info->cyChar = tm.tmHeight + tm.tmExternalLeading;
		if (hFont != NO_FONT) DeleteObject(SelectObject(hdc, hOld));
		ReleaseDC(hwnd, hdc);

		SetWindowLong(hwnd, GWL_USERDATA, (LONG) Info);

		SystemParametersInfo(SPI_GETWORKAREA, sizeof(Info->rcText), &Info->rcText, 0);
		TraceLogThread("MessageBox", TRACE_CRITICAL, "Work:%ld %ld -> %ld %ld\n",
						Info->rcText.left, Info->rcText.top,
						Info->rcText.right, Info->rcText.bottom);
		RECT rcFrame;
		CopyRect(&rcFrame, &Info->rcText);
		AdjustWindowRectEx(&rcFrame, GetWindowLong(hwnd,GWL_STYLE), FALSE, GetWindowLong(hwnd,GWL_EXSTYLE));
		TraceLogThread("MessageBox", TRACE_CRITICAL, "Frame:%ld %ld -> %ld %ld\n",
						rcFrame.left, rcFrame.top, rcFrame.right, rcFrame.bottom);
		OffsetRect(&rcFrame, -rcFrame.left, -rcFrame.top);
		OffsetRect(&Info->rcText, -Info->rcText.left, -Info->rcText.top);
		if (rcFrame.right > Info->rcText.right)
			Info->rcText.right -= (rcFrame.right-Info->rcText.right);

		SetRect(&Info->rcText, 0, 0, Info->rcText.right, 65536);	/* Arbitrary max height */

		RECT rcTitle = Info->rcText;
		int d3Width = GetSystemMetrics(SM_CXEDGE);
#ifdef UNDER_CE
		SetMessageBoxTextRect(hwnd, &rcTitle, Info->lpCaption, d3Width*5, 0);
		TraceLogThread("MessageBox", TRACE_CRITICAL, "Title:%ld %ld -> %ld %ld\n",
						rcTitle.left, rcTitle.top, rcTitle.right, rcTitle.bottom);
#else
		int tbButtonWidth = GetSystemMetrics(SM_CXSIZE);
		int tbButtonHeight = GetSystemMetrics(SM_CYSIZE);
		SetMessageBoxTextRect(hwnd, &rcTitle, Info->lpCaption, tbButtonWidth+d3Width*5, 0);
		TraceLogThread("MessageBox", TRACE_CRITICAL, "Title:%ld %ld -> %ld %ld (Icon:%ld x %ld)\n",
						rcTitle.left, rcTitle.top,
						rcTitle.right, rcTitle.bottom,
						tbButtonWidth, tbButtonHeight);
#endif
		OffsetRect(&rcTitle, -rcTitle.left, -rcTitle.top);

		TraceLogThread("MessageBox", TRACE_CRITICAL, "PreText1:%ld %ld -> %ld %ld\n",
						Info->rcText.left, Info->rcText.top,
						Info->rcText.right, Info->rcText.bottom);
		if (GetWindowLong(hwnd,GWL_STYLE) & WS_VSCROLL)	/* Reserve scroll bar */
			Info->rcText.right -= GetSystemMetrics(SM_CXVSCROLL);

		TraceLogThread("MessageBox", TRACE_CRITICAL, "PreText2:%ld %ld -> %ld %ld\n",
						Info->rcText.left, Info->rcText.top,
						Info->rcText.right, Info->rcText.bottom);

		Info->cyText = SetMessageBoxTextRect(hwnd, &Info->rcText, Info->lpText, Info->cxCaps/2, Info->cyChar/4);
		TraceLogThread("MessageBox", TRACE_CRITICAL, "Text:%ld %ld -> %ld %ld\n",
						Info->rcText.left, Info->rcText.top,
						Info->rcText.right, Info->rcText.bottom);

		InstantiateButtons(Info->Buttons, hwnd);

		Info->rcText.bottom += Info->Buttons->ButtonHeight+Info->Buttons->ButtonMargin*2;
		int bWidth = Info->Buttons->ButtonWidth;
		bWidth += Info->Buttons->ButtonMargin;
		bWidth *= Info->Buttons->ButtonCount;
		bWidth += Info->Buttons->ButtonMargin;
		if (Info->rcText.right-Info->rcText.left < bWidth)
			Info->rcText.right = Info->rcText.left + bWidth;
		TraceLogThread("MessageBox", TRACE_CRITICAL, "bAdj:%ld %ld -> %ld %ld\n",
						Info->rcText.left, Info->rcText.top,
						Info->rcText.right, Info->rcText.bottom);

		AdjustWindowRectEx(&Info->rcText, GetWindowLong(hwnd,GWL_STYLE), FALSE, GetWindowLong(hwnd,GWL_EXSTYLE));
		TraceLogThread("MessageBox", TRACE_CRITICAL, "Frame:%ld %ld -> %ld %ld\n",
						Info->rcText.left, Info->rcText.top,
						Info->rcText.right, Info->rcText.bottom);

		if (GetWindowLong(hwnd,GWL_STYLE) & WS_HSCROLL)
			Info->rcText.bottom += GetSystemMetrics(SM_CYHSCROLL);
		if (GetWindowLong(hwnd,GWL_STYLE) & WS_VSCROLL)
			Info->rcText.right += GetSystemMetrics(SM_CXVSCROLL);

		TraceLogThread("MessageBox", TRACE_CRITICAL, "Scroll:%ld %ld -> %ld %ld\n",
						Info->rcText.left, Info->rcText.top,
						Info->rcText.right, Info->rcText.bottom);

		int Width = Info->rcText.right - Info->rcText.left;
		int Height = Info->rcText.bottom - Info->rcText.top;
		TraceLogThread("MessageBox", TRACE_CRITICAL, "WH1 %ld x %ld\n", Width, Height);
		if (Width < rcTitle.right) Width = rcTitle.right;
		TraceLogThread("MessageBox", TRACE_CRITICAL, "WH2 %ld x %ld\n", Width, Height);

		RECT rcWork;
		SystemParametersInfo(SPI_GETWORKAREA, sizeof(rcWork), &rcWork, 0);
		OffsetRect(&rcWork, -rcWork.left, -rcWork.top);	/* Make zero relative */

#ifdef UNDER_CE
#ifndef NO_SHELL
		if (Info->hwndOwner)
		{	HWND hMenuBar = SHFindMenuBar( Info->hwndOwner);
			if (hMenuBar)
			{	RECT rcMenu;
				GetWindowRect(hMenuBar, &rcMenu);
				TraceLogThread("MessageBox", TRACE_CRITICAL, "Menu:%ld %ld -> %ld %ld\n",
								rcMenu.left, rcMenu.top, rcMenu.right, rcMenu.bottom);
				OffsetRect(&rcMenu,-rcMenu.left, -rcMenu.top);	/* Make right/top be size */
				rcWork.bottom -= rcMenu.bottom;
			}
		}
#endif
#endif

		if (Width > rcWork.right) Width = rcWork.right;
		if (Height > rcWork.bottom) Height = rcWork.bottom;
		TraceLogThread("MessageBox", TRACE_CRITICAL, "WH3 %ld x %ld\n", Width, Height);
		
		TraceLogThread("MessageBox", TRACE_CRITICAL, "CreateSize:%ld x %ld\n",
						Width, Height);
		SetWindowPos(hwnd, 
                 HWND_TOP, 
                 0, 0, Width, Height,
                 SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER); 
		TraceLogThread("MessageBox", TRACE_CRITICAL, "Sized:%ld x %ld\n",
						Width, Height);

		if (Info->hwndOwner)
		{	TraceLogThread("MessageBox",TRACE_CRITICAL,"WM_CREATE:Disable Parent Window\n");
			EnableWindow(Info->hwndOwner, FALSE);
		}

		return 0;
	}
#ifdef UNDER_CE
#ifndef CE50
#ifndef NO_SHELL
	case WM_SETFOCUS:
		SHFullScreen(hwnd,SHFS_HIDESIPBUTTON);
		return 0;
	case WM_KILLFOCUS:
		SHFullScreen(hwnd,SHFS_SHOWSIPBUTTON);
		return 0;
#endif
#endif
#endif

	case WM_SIZE:
	Info = (LWD_MSG_INFO_S *) GetWindowLong(hwnd, GWL_USERDATA);
	if (Info)
	{	int newX = LOWORD(lParam);
		int newY = HIWORD(lParam);
static int Nesting=0;
		Nesting++;
		TraceLogThread("MessageBox",TRACE_CRITICAL,"WM_SIZE[%ld]:%ld x %ld\n", Nesting, newX, newY);
		Info->cxClient = LOWORD(lParam);
		Info->cyClient = HIWORD(lParam);

#ifdef CE50	/* Hack for useless recursion */
		if (Nesting == 1)	/* Only the first time */
#endif
		{
		SetRect(&Info->rcText, 0, 0, Info->cxClient, 65536);	/* Arbitrary max height */
		Info->cyText = SetMessageBoxTextRect(hwnd, &Info->rcText, Info->lpText, Info->cxCaps/2, Info->cyChar/4);

		Info->iVscrollMax = max(0, Info->cyText - (Info->cyClient-Info->Buttons->ButtonHeight-Info->Buttons->ButtonMargin*2));
		Info->iVscrollPos = min(Info->iVscrollPos, Info->iVscrollMax);

#ifdef OLD_WAY
TraceLogThread("MessageBox",TRACE_CRITICAL,"WM_SIZE[%ld]0:%ld x %ld vMax:%ld\n", Nesting, newX, newY, Info->iVscrollMax);
		SetScrollRange(hwnd, SB_VERT, 0, Info->iVscrollMax, FALSE);
TraceLogThread("MessageBox",TRACE_CRITICAL,"WM_SIZE[%ld]1:%ld x %ld vPos:%ld/%ld\n", Nesting, newX, newY, Info->iVscrollPos, Info->iVscrollMax);
		SetScrollPos(hwnd, SB_VERT, Info->iVscrollPos, TRUE);
TraceLogThread("MessageBox",TRACE_CRITICAL,"WM_SIZE[%ld]2:%ld x %ld vDone %ld/%ld\n", Nesting, newX, newY, Info->iVscrollPos, Info->iVscrollMax);
#else
		{	SCROLLINFO sc;
			sc.cbSize = sizeof(sc);
			sc.fMask = SIF_ALL;
			if (GetScrollInfo(hwnd, SB_VERT, &sc))
			{	sc.fMask = 0;
				if (sc.nMin != 0
				|| sc.nMax != Info->iVscrollMax)
				{	sc.fMask |= SIF_RANGE;
					sc.nMin = 0;
					sc.nMax = Info->iVscrollMax;
				}
				if (sc.nPos != Info->iVscrollPos)
				{	sc.fMask |= SIF_POS;
					sc.nPos = Info->iVscrollPos;
				}
				if (sc.fMask)
				{	TraceLogThread("MessageBox", TRACE_CRITICAL, "WM_SIZE[%ld] SetVert to %ld/%ld\n", Nesting, sc.nPos, sc.nMax);
					if (SetScrollInfo(hwnd, SB_VERT, &sc, TRUE) != sc.nPos)
					{	TraceLogThread("MessageBox", TRUE, "SetScrollInfo(VERT) Failed!\n");
					}
					else TraceLogThread("MessageBox", TRACE_CRITICAL, "WM_SIZE[%ld] DoneVert to %ld/%ld\n", Nesting, sc.nPos, sc.nMax);
				}
			}// else TraceLogThread("MessageBox", TRUE, "GetScrollInfo(VERT) Failed!\n");
		}
#endif

#ifdef FOR_INFO
		{	SCROLLINFO sc = {0};
			sc.cbSize = sizeof(sc);
			sc.fMask = SIF_ALL;
			if (GetScrollInfo(hwnd, SB_VERT, &sc))
			{	TraceLog("ScrollBar", TRUE, hwnd, "Min:%ld Max:%ld Page:%ld Pos:%ld Track:%ld\n", sc.nMin, sc.nMax, sc.nPage, sc.nPos, sc.nTrackPos);
			}
			if (Info->iVscrollMax > 0)
			{	sc.fMask = SIF_PAGE;
				sc.nPage = Info->cyClient / Info->iVscrollMax;
				SetScrollInfo(hwnd, SB_VERT, &sc, TRUE);
			}
		}
The nPage member must specify a value from 0 to nMax - nMin +1. The nPos member must specify a value between nMin and nMax - max( nPage– 1, 0). If either value is beyond its range, the function sets it to a value that is just within the range. 
 
        // Set the vertical scrolling range and page size
        si.cbSize = sizeof(si); 
        si.fMask  = SIF_RANGE | SIF_PAGE; 
        si.nMin   = 0; 
        si.nMax   = LINES - 1; 
        si.nPage  = yClient / yChar; 
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE); 
#endif

		Info->iHscrollMax = max(0, (Info->rcText.right-Info->rcText.left)-Info->cxClient);
		Info->iHscrollPos = min(Info->iHscrollPos, Info->iHscrollMax);

#ifdef OLD_WAY
		TraceLogThread("MessageBox",TRACE_CRITICAL,"WM_SIZE[%ld]3:%ld x %ld hMax:%ld\n", Nesting, newX, newY, Info->iHscrollMax);
		SetScrollRange(hwnd, SB_HORZ, 0, Info->iHscrollMax, FALSE);
TraceLogThread("MessageBox",TRACE_CRITICAL,"WM_SIZE[%ld]4:%ld x %ld hPos:%ld/%ld\n", Nesting, newX, newY, Info->iHscrollPos, Info->iHscrollMax);
		SetScrollPos(hwnd, SB_HORZ, Info->iHscrollPos, TRUE);
TraceLogThread("MessageBox",TRACE_CRITICAL,"WM_SIZE[%ld]5:%ld x %ld hDone:%ld/%ld\n", Nesting, newX, newY, Info->iHscrollPos, Info->iHscrollMax);
#else
		{	SCROLLINFO sc;
			sc.cbSize = sizeof(sc);
			sc.fMask = SIF_ALL;
			if (GetScrollInfo(hwnd, SB_HORZ, &sc))
			{	sc.fMask = 0;
				if (sc.nMin != 0
				|| sc.nMax != Info->iHscrollMax)
				{	sc.fMask |= SIF_RANGE;
					sc.nMin = 0;
					sc.nMax = Info->iHscrollMax;
				}
				if (sc.nPos != Info->iHscrollPos)
				{	sc.fMask |= SIF_POS;
					sc.nPos = Info->iHscrollPos;
				}
				if (sc.fMask)
				{	TraceLogThread("MessageBox", TRACE_CRITICAL, "WM_SIZE[%ld] SetHorz to %ld/%ld\n", Nesting, sc.nPos, sc.nMax);
					if (SetScrollInfo(hwnd, SB_HORZ, &sc, TRUE) != sc.nPos)
					{	TraceLogThread("MessageBox", TRUE, "SetScrollInfo(HORZ) Failed!\n");
					}
					else TraceLogThread("MessageBox", TRACE_CRITICAL, "WM_SIZE[%ld] DoneHorz to %ld/%ld\n", Nesting, sc.nPos, sc.nMax);
				}
			}// else TraceLogThread("MessageBox", TRUE, "GetScrollInfo(HORZ) Failed!\n");
		}
#endif
		}
#ifdef OBSOLETE
		TraceLogThread("MessageBox", TRACE_CRITICAL, "Horiz: %ld -> %ld Client:%ld Text:%ld->%ld=%ld\n",
						Info->iHscrollMax, Info->iHscrollPos,
						Info->cxClient,
						Info->rcText.left, Info->rcText.right,
						Info->rcText.right-Info->rcText.left);
#endif
		FixButtons(Info->Buttons, hwnd, Info->cxClient, Info->cyClient);
		TraceLogThread("MessageBox",TRACE_CRITICAL,"WM_SIZED[%ld]:%ld x %ld\n", Nesting, newX, newY);
		Nesting--;
	}
	break;
	case WM_VSCROLL:
	Info = (LWD_MSG_INFO_S *) GetWindowLong(hwnd, GWL_USERDATA);
	if (Info)
	{	int iVscrollInc;
		switch(LOWORD(wParam))
		{
		case SB_TOP:		iVscrollInc = -Info->iVscrollPos; break;
		case SB_BOTTOM:		iVscrollInc = Info->iVscrollMax - Info->iVscrollPos; break;
		case SB_LINEUP:		iVscrollInc = -Info->cyChar; break;
		case SB_LINEDOWN:	iVscrollInc =  Info->cyChar; break;
		case SB_PAGEUP:		iVscrollInc = -Info->cyClient; break;
		case SB_PAGEDOWN:	iVscrollInc =  Info->cyClient; break;
		case SB_THUMBTRACK:	iVscrollInc = HIWORD(wParam) - Info->iVscrollPos; break;
//		case SB_THUMBPOSITION:	iVscrollInc = HIWORD(wParam) - Info->iVscrollPos; break;
		default: iVscrollInc = 0; break;
		}
		iVscrollInc = max(-Info->iVscrollPos, min(iVscrollInc, Info->iVscrollMax-Info->iVscrollPos));
		if (iVscrollInc != 0)
		{	Info->iVscrollPos += iVscrollInc;
			SetScrollPos(hwnd, SB_VERT, Info->iVscrollPos, TRUE);
#ifdef UNDER_CE
			InvalidateRect(hwnd, NULL, TRUE);
#else
			ScrollWindow(hwnd, 0, -iVscrollInc, NULL, NULL);
			FixButtons(Info->Buttons, hwnd, Info->cxClient, Info->cyClient);
			UpdateWindow(hwnd);
#endif
		}
#ifdef OBSOLETE
		TraceLogThread("MessageBox", TRACE_CRITICAL, "VScroll:%ld (Moved %ld)\n", Info->iVscrollPos, iVscrollInc);
#endif
	}
	break;
	case WM_HSCROLL:
	Info = (LWD_MSG_INFO_S *) GetWindowLong(hwnd, GWL_USERDATA);
	if (Info)
	{	int iHscrollInc;
		switch(LOWORD(wParam))
		{
		case SB_LINEUP:	iHscrollInc = -Info->cxCaps; break;
		case SB_LINEDOWN:	iHscrollInc = Info->cxCaps; break;
		case SB_PAGEUP:	iHscrollInc = -Info->cxCaps*8; break;
		case SB_PAGEDOWN:	iHscrollInc = Info->cxCaps*8; break;
		case SB_THUMBTRACK: iHscrollInc = HIWORD(wParam)-Info->iHscrollPos; break;
//		case SB_THUMBPOSITION: iHscrollInc = HIWORD(wParam)-Info->iHscrollPos; break;
		default: iHscrollInc = 0; break;
		}
		iHscrollInc = max(-Info->iHscrollPos,min(iHscrollInc, Info->iHscrollMax-Info->iHscrollPos));
		if (iHscrollInc != 0)
		{	Info->iHscrollPos += iHscrollInc;
			SetScrollPos(hwnd, SB_HORZ, Info->iHscrollPos, TRUE);
#ifdef UNDER_CE
			InvalidateRect(hwnd, NULL, TRUE);
#else
			ScrollWindow(hwnd, -iHscrollInc, 0, NULL, NULL);
			FixButtons(Info->Buttons, hwnd, Info->cxClient, Info->cyClient);
			UpdateWindow(hwnd);
#endif
		}
	}
	break;
	case WM_KEYDOWN:
//TraceLogThread("MessageBox", TRACE_CRITICAL, "hwndMsg: %lX msg: %s(%lX) wp: %lX lp: %lX\n", (long) hwnd, MsgToText(iMsg), (long) iMsg, (long) wParam, (long) lParam);
		switch(wParam)
		{
		case VK_HOME:	SendMessage(hwnd, WM_VSCROLL, SB_TOP, 0); break;
		case VK_END:	SendMessage(hwnd, WM_VSCROLL, SB_BOTTOM, 0); break;
		case VK_PRIOR:	SendMessage(hwnd, WM_VSCROLL, SB_PAGEUP, 0); break;
		case VK_NEXT:	SendMessage(hwnd, WM_VSCROLL, SB_PAGEDOWN, 0); break;
		case VK_UP:		SendMessage(hwnd, WM_VSCROLL, SB_LINEUP, 0); break;
		case VK_DOWN:	SendMessage(hwnd, WM_VSCROLL, SB_LINEDOWN, 0); break;
		case VK_LEFT:	SendMessage(hwnd, WM_HSCROLL, SB_PAGEUP, 0); break;
		case VK_RIGHT:	SendMessage(hwnd, WM_HSCROLL, SB_PAGEDOWN, 0); break;
		case VK_ESCAPE:	PostMessage(hwnd, WM_CLOSE, 0, 0); break;
		case VK_INSERT:
		if (GetKeyState(VK_CONTROL))
			return SendMessage(hwnd, WM_CHAR, 3, 0);	/* Ctrl-C Copy */
		break;
		case VK_DELETE:
		if (GetKeyState(VK_SHIFT))
			return SendMessage(hwnd, WM_CHAR, 24, 0);	/* Ctrl-X Cut */
		break;
		}
	return 0;

	case WM_CONTEXTMENU:
	{	POINT pt;
		pt.x = LOWORD(lParam); pt.y = HIWORD(lParam);
//		ClientToScreen(hwnd, &pt);
		HMENU hPopup = CreatePopupMenu();

		AppendMenu(hPopup, MF_STRING, ID_LOG_COPY, TEXT("Copy"));

		switch (TrackPopupMenu(hPopup,
						TPM_CENTERALIGN | TPM_VCENTERALIGN
						| TPM_NONOTIFY | TPM_RETURNCMD,
						pt.x, pt.y, 0, hwnd, NULL))
		{
		case ID_LOG_COPY:
		{	SendMessage(hwnd, WM_CHAR, 3, 0);	/* Ctrl-C Copy */
			break;
		}
		}
		return 0;
	}


	case WM_CHAR:
//TraceLogThread("MessageBox", TRACE_CRITICAL, "hwndMsg: %lX msg: %s(%lX) wp: %lX lp: %lX\n", (long) hwnd, MsgToText(iMsg), (long) iMsg, (long) wParam, (long) lParam);
		Info = (LWD_MSG_INFO_S *) GetWindowLong(hwnd, GWL_USERDATA);
		switch (wParam)
		{
		case 3:	/* Control-C = Copy */
		case 24:	/* Control-X = Cut */
		{	size_t Len = wcslen(Info->lpCaption) + 2 + 1 + wcslen(Info->lpText) + 1;
			HANDLE hMem = GlobalAlloc(GHND, Len*sizeof(*Info->lpText));
			if (hMem)
			{	LPWSTR pMem = (LPWSTR) GlobalLock(hMem);
				wcscpy(pMem, Info->lpCaption);
				wcscat(pMem, TEXT("\r\n"));
				wcscat(pMem, Info->lpText);
				GlobalUnlock(hMem);
				if (OpenClipboard(hwnd))
				{	EmptyClipboard();
					if (!SetClipboardData(/*CF_TEXT*/CF_UNICODETEXT, hMem))
						GlobalFree(hMem);
					CloseClipboard();
				} else GlobalFree(hMem);
			}
			break;
		}
		case '\n':
		case '\r':
			if (Info)
			{	if (!Info->Buttons->ButtonCount)
					PostMessage(hwnd, WM_CLOSE, 0, 0);
				else if (Info->Buttons->ButtonDefault >= 0
				&& Info->Buttons->ButtonDefault < Info->Buttons->ButtonCount)
				SendMessage(hwnd, WM_COMMAND, Info->Buttons->Buttons[Info->Buttons->ButtonDefault].ID, 0);
			}
			break;
		case 27:	PostMessage(hwnd, WM_CLOSE, 0, 0); break;	/* Escape */
		default:
		if (Info)
		{	for (int b=0; b<Info->Buttons->ButtonCount; b++)
			{	if (toupper(wParam) == *Info->Buttons->Buttons[b].Label)
				{	SendMessage(hwnd, WM_COMMAND, Info->Buttons->Buttons[b].ID, 0);
					break;
				}
			}
		}
		}
	return 0;

	case WM_COMMAND:
//TraceLogThread("MessageBox", TRACE_CRITICAL, "hwndMsg: %lX msg: %s(%lX) wp: %lX lp: %lX\n", (long) hwnd, MsgToText(iMsg), (long) iMsg, (long) wParam, (long) lParam);
	Info = (LWD_MSG_INFO_S *) GetWindowLong(hwnd, GWL_USERDATA);
	if (Info)
	{	Info->Result = LOWORD(wParam);
		PostMessage(hwnd, WM_CLOSE, 0, 0);
	}
	return 0;

	case WM_PAINT:
	Info = (LWD_MSG_INFO_S *) GetWindowLong(hwnd, GWL_USERDATA);
	if (Info)
	{
		PAINTSTRUCT ps;
// Start the paint operation
		if (BeginPaint(hwnd, &ps) == NULL)
		{	return 0;
		}
		SetBkColor(ps.hdc, GetSysColor(COLOR_BTNFACE));	/* Gray background */
		SetTextColor(ps.hdc, GetSysColor(COLOR_BTNTEXT));
		FillRect(ps.hdc, &ps.rcPaint, GetSysColorBrush(COLOR_BTNFACE)); 
		HFONT hOld = NO_FONT, hFont = GetMessageBoxFont();
		if (hFont != NO_FONT) hOld = (HFONT) SelectObject(ps.hdc, hFont);

		RECT rcWin;
		int cyText;
		GetClientRect(hwnd, &rcWin);

//		TraceLogThread("MessageBox", TRACE_CRITICAL, "\nClient:%ld %ld -> %ld %ld\n",
//						rcWin.left, rcWin.top, rcWin.right, rcWin.bottom);
		if (Info->iVscrollPos)
			rcWin.top -= Info->iVscrollPos;
		else if (Info->cyClient-Info->Buttons->ButtonHeight-Info->Buttons->ButtonMargin*2 > Info->rcText.bottom-Info->rcText.top)
			rcWin.top += (Info->cyClient-Info->Buttons->ButtonHeight-Info->Buttons->ButtonMargin*2-(Info->rcText.bottom-Info->rcText.top))/2;
		if (Info->iHscrollPos)
			rcWin.left -= Info->iHscrollPos;
		else if (Info->cxClient > Info->rcText.right-Info->rcText.left)
			rcWin.left += (Info->cxClient-(Info->rcText.right-Info->rcText.left))/2;

//#define IT_WAS_BLUE
#ifdef IT_WAS_BLUE
		if (Info->ButtonCount)
		{	RECT rcBottom = rcWin;
			rcBottom.top = rcWin.top + Info->cyText;
			rcBottom.left = 0; rcBottom.right = rcWin.right-rcWin.left;
			HBRUSH hbrBack = GetSysColorBrush(COLOR_BTNFACE);
			HBRUSH hbrOrg = (HBRUSH) SelectObject(ps.hdc, hbrBack);
			Rectangle(ps.hdc, rcBottom.left, rcBottom.top, rcBottom.right, rcBottom.bottom);
			SelectObject(ps.hdc, hbrOrg);
		}
#endif

//		rcWin.top += Info->cyChar/4;	/* Not sure why this isn't needed */
		rcWin.left += Info->cxCaps/2;	/* Not sure why this IS needed */
		cyText = DrawText(ps.hdc, Info->lpText, -1, &rcWin, LWD_DT);
//		TraceLogThread("MessageBox", TRACE_CRITICAL, "  Draw:%ld %ld -> %ld %ld (%ld High)\n",
//						rcWin.left, rcWin.top, rcWin.right, rcWin.bottom, cyText);

		if (hFont != NO_FONT) DeleteObject(SelectObject(ps.hdc, hOld));

		EndPaint(hwnd, &ps);
	}
	break;
	case WM_CLOSE:
		Info = (LWD_MSG_INFO_S *) GetWindowLong(hwnd, GWL_USERDATA);
		if (Info->hwndOwner)
		{	TraceLogThread("MessageBox",TRACE_CRITICAL,"WM_CLOSE:Enable Parent Window\n");
			EnableWindow(Info->hwndOwner, TRUE);
		}
		DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		/* Deinitialize stuff here */
		Info = (LWD_MSG_INFO_S *) GetWindowLong(hwnd, GWL_USERDATA);
		SetWindowLong(hwnd, GWL_USERDATA, 0);
		if (Info)
		{	if (Info->hwndOwner)
			{//	TraceLogThread("MessageBox",TRACE_CRITICAL,"Posting WM_QUIT\n");
				PostQuitMessage(Info->Result);
			} else
			{	free((void*)Info->lpText);
				free((void*)Info->lpCaption);
				FreeButtons(Info->Buttons);
				free(Info);
//				TraceLogThread("MessageBox",TRACE_CRITICAL,"Destroy Non-Modal Done\n");
			}
		}
		return 0;
	default:
		/* Handle registered window messages like Find/Replace */
		;
	}
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

int LwdMessageBox3(HWND hwnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, BUTTONS_S *Buttons, HICON hIcon, BOOL Stock)
{	int Result = 0;
static	BOOL First = TRUE;
static const TCHAR g_szMsgName[] = TEXT("LwdMessageBox");

TraceLogThread("MessageBox", TRACE_CRITICAL, "MessageBox%s(%p):%S %S\n", Stock?"":"3", hwnd, lpCaption, lpText);

//	if (Stock) return MessageBoxW(hwnd, lpText, lpCaption, uType);

	LWD_MSG_INFO_S *Info = (LWD_MSG_INFO_S *) calloc(1,sizeof(*Info));
	Info->lpText = _wcsdup(lpText);
	Info->lpCaption = _wcsdup(lpCaption);
	Info->hwndOwner = hwnd;
	Info->Buttons = Buttons?Buttons:CreateButtons();
	Info->uType = uType;

//	TraceLogThread("WinMsg", TRUE, "LwdMessageBox:Starting...\n");
//	TraceMessages = TRUE;

#ifdef UNDER_CE
	HWND hwndMsg;
	if (First)
	{	WNDCLASS msgClass = {0};
		msgClass.style = CS_HREDRAW | CS_VREDRAW;
		msgClass.lpfnWndProc = LwdMsgWndProc;
		msgClass.hInstance = g_hInstance;
//		msgClass.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
//		msgClass.hbrBackground = (HBRUSH) CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
		msgClass.hbrBackground = (HBRUSH) COLOR_BTNFACE+1;
		msgClass.lpszClassName = g_szMsgName;
		RegisterClass(&msgClass);
		First = FALSE;
	}
	hwndMsg = CreateWindow(g_szMsgName, lpCaption,
//							/* WS_EX_CAPTIONOKBTN*/ /*WS_OVERLAPPED |*/ WS_SYSMENU | WS_VSCROLL | WS_HSCROLL,
							WS_CAPTION | WS_CLIPCHILDREN | WS_POPUP | WS_SYSMENU | WS_VSCROLL | WS_HSCROLL,
							CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
							hwnd, NULL, g_hInstance, (LPVOID) Info);
#else
	HWND hwndMsg;
	if (First)
	{	WNDCLASSEX msgClass = {0};
		msgClass.cbSize = sizeof(msgClass);
		msgClass.style = CS_HREDRAW | CS_VREDRAW;
		msgClass.lpfnWndProc = LwdMsgWndProc;
		msgClass.hInstance = g_hInstance;
//		if (LoadIconMetric(g_hInstance, MAKEINTRESOURCE(IDI_MY_ICON), LIM_SMALL, &msgClass.hIcon) != S_OK)
		if (!msgClass.hIcon)
			msgClass.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_MY_ICON));
		if (!msgClass.hIcon)
			msgClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		msgClass.hCursor = LoadCursor(NULL, IDC_ARROW);
//		msgClass.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
		msgClass.hbrBackground = (HBRUSH) COLOR_BTNFACE+1;
//		msgClass.lpszMenuName = MAKEINTRESOURCE(IDM_LOG_W32);
		msgClass.lpszClassName = g_szMsgName;
		RegisterClassEx(&msgClass);
		First = FALSE;
	}

	hwndMsg = CreateWindow(g_szMsgName, lpCaption,
								WS_POPUP | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VSCROLL | WS_HSCROLL,
								CW_USEDEFAULT, CW_USEDEFAULT,
								CW_USEDEFAULT, CW_USEDEFAULT,
								hwnd, NULL, g_hInstance, (LPVOID) Info);
	if (hIcon)
	{	SendMessage(hwndMsg, WM_SETICON, ICON_BIG, (LPARAM) hIcon);
		SendMessage(hwndMsg, WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
	}
#endif
	if (hwndMsg)
	{
	TraceLogThread("MessageBox", TRACE_CRITICAL, "CenterWindow(%p)\n", hwndMsg);
	CenterWindow(hwndMsg);
//	TraceLogThread("MessageBox", TRACE_CRITICAL, "TopActivate(%p)\n", hwndMsg);
//    SetWindowPos(hwndMsg, HWND_TOP, 0, 0, 0, 0,
//                 SWP_NOSIZE | SWP_NOMOVE); 
	TraceLogThread("MessageBox", TRACE_CRITICAL, "ShowWindow(%p)\n", hwndMsg);
	ShowWindow(hwndMsg, SW_SHOW);
//	SetForegroundWindow(hwndMsg);
	TraceLogThread("MessageBox", TRACE_CRITICAL, "UpdateWindow(%p)\n", hwndMsg);
	UpdateWindow(hwndMsg);
	TraceLogThread("MessageBox", TRACE_CRITICAL, "Sound(%p)\n", hwndMsg);
	if (ActiveConfig.Enables.Sound)
	{	UINT Icons = MB_ICONHAND | MB_ICONQUESTION
					| MB_ICONEXCLAMATION | MB_ICONASTERISK;
//	TraceLogThread("MessageBox", TRACE_CRITICAL, "uType=0x%lX Icons=0x%lX Icon=0x%lX\n", uType, Icons, uType&Icons);
		MessageBeep(uType & Icons);
	}
	if (Info->hwndOwner)
	{	MSG msg;

//		TraceLogThread("MessageBox",TRACE_CRITICAL,"Disable Parent Window\n");
//		EnableWindow(Info->hwndOwner, FALSE);

//		TraceLogThread("MessageBox",TRACE_CRITICAL3,"Spinning Modal Message Loop\n");
		while (GetMessage(&msg, NULL, 0, 0) > 0)
		{//	double Start = OSMGetMsec();
		//static double msMax = 0, msLast = 0;
//TraceActivity(hwndMain, "hwndMsg: %lX msg: %lX wp: %lX lp: %lX\n", (long) msg.hwnd, (long) msg.message, (long) msg.wParam, (long) msg.lParam);
			if (!gModelessDialog || !IsDialogMessage(gModelessDialog, &msg))
			{	TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
//			MessageTimer("hwndMsg:DispatchMessage", Start, &msMax, &msLast,
//						msg.hwnd, msg.message, msg.wParam, msg.lParam);
		}
//		TraceLogThread("MessageBox",TRACE_CRITICAL,"Modal Loop Complete\n");
		if (!IsWindow(hwndMsg))
		{//	TraceLogThread("MessageBox",TRACE_CRITICAL,"hwndMsg:WM_QUIT!\n");
//			EnableWindow(Info->hwndOwner, TRUE);
//			ShowWindow(Info->hwndOwner, SW_SHOW);
			SetForegroundWindow(Info->hwndOwner);
//			SetActiveWindow(Info->hwndOwner);
			SetWindowPos(Info->hwndOwner, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
			Result = Info->Result;
			free((void*)Info->lpText);
			free((void*)Info->lpCaption);
			FreeButtons(Info->Buttons);
			free(Info);
		}// else TraceLogThread("MessageBox",TRACE_CRITICAL,"hwndMsg:Ignoring WM_QUIT, Still A Window!\n");
	}
//	StopTraceMessages = TRUE;
//	TraceLogThread("WinMsg", TRUE, "LwdMessageBox:Stopping...\n");
	} else TraceLogThread("MessageBox", TRACE_CRITICAL, "MessageBox2(%p->%S) Failed with %ld\n", hwnd, lpCaption, (long) GetLastError());
	return Result;
}

int LwdMessageBox2(HWND hwnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, BUTTONS_S *Buttons, HICON hIcon)
{	return LwdMessageBox3(hwnd, lpText, lpCaption, uType, Buttons, hIcon, FALSE);
}

BOOL LwdMessageBox1(HWND hwnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, char *Button, BOOL Default, HICON hIcon)
{
	BUTTONS_S *Buttons = CreateButtons();
	AddButton(Buttons, Button, 1, Default);
	return LwdMessageBox2(hwnd, lpText, lpCaption, uType, Buttons, hIcon)==1;
}

int LwdMessageBox(HWND hwnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, HICON hIcon)
{
	BUTTONS_S *Buttons = CreateButtons();
	if ((uType & MB_DEFBUTTON4) == MB_DEFBUTTON4)
		Buttons->ButtonDefault = 3;
	else if ((uType & MB_DEFBUTTON3) == MB_DEFBUTTON3)
		Buttons->ButtonDefault = 2;
	else if ((uType & MB_DEFBUTTON2) == MB_DEFBUTTON2)
		Buttons->ButtonDefault = 1;
	else if ((uType & MB_DEFBUTTON1) == MB_DEFBUTTON1)
		Buttons->ButtonDefault = 0;
	else
		Buttons->ButtonDefault = 0;
	switch (uType & 0x7)
	{
	case MB_OK:
		//AddButton(Buttons, "OK", IDOK);
		break;
	case MB_OKCANCEL:
		AddButton(Buttons, "OK", IDOK);
		AddButton(Buttons, "Cancel", IDCANCEL);
		break;
	case MB_ABORTRETRYIGNORE:
		AddButton(Buttons, "Abort", IDABORT);
		AddButton(Buttons, "Retry", IDRETRY);
		AddButton(Buttons, "Ignore", IDIGNORE);
		break;
	case MB_YESNOCANCEL:
		AddButton(Buttons, "Yes", IDYES);
		AddButton(Buttons, "No", IDNO);
		AddButton(Buttons, "Cancel", IDCANCEL);
		break;
	case MB_YESNO:
		AddButton(Buttons, "Yes", IDYES);
		AddButton(Buttons, "No", IDNO);
		break;
	case MB_RETRYCANCEL:
		AddButton(Buttons, "Retry", IDRETRY);
		AddButton(Buttons, "Cancel", IDCANCEL);
		break;
	default: TraceLogThread("MessageBox", TRUE, "Unrecognized buttons %ld\n", uType&7);
	}
	return LwdMessageBox3(hwnd, lpText, lpCaption, uType, Buttons, hIcon, TRUE);
}

