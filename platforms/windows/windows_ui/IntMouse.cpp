//
//	FILE			IntMouse.cpp
//	
//	DESCRIPTION		IntelliMouse support routines
//
//	CREATED			DG[31] 18.feb.1998
//
//
//	COMMENTS		Emulation exceptions is listed below: 
//					"HKEY_CURRENT_USER\Control Panel\Microsoft Input Devices\Mouse\Exceptions"


#include "core/pch.h"

#include "IntMouse.h"
#include "platforms/windows/win_handy.h"

#undef PostMessage
#define PostMessage PostMessageU

//
//	Global params.
//
BOOL	g_IntelliMouse_NativeOSSupport	= 0;
HWND	g_IntelliMouse_hwndMouseWheel	= 0;
UINT	g_IntelliMouse_MsgMouseWheel	= 0;
UINT	g_IntelliMouse_MsgScrollLines	= 0;

//
//	Static helper functions
//
static void _GetParams();
static BOOL _HasNativeSupport();



//	--------------------------------------------------------------------------
//	Main functions implementation
//	--------------------------------------------------------------------------


void IntelliMouse_Init()
{
	_GetParams();
}


//
//	Translate from the registered message MSH_MOUSEWHEEL (Win95)
//	to the corresponding WM_MOUSEWHEEL message (Win98/NT4.0)
//
#if !defined(OPTREEDLL)
void IntelliMouse_TranslateWheelMessage( MSG *msg)
{
	if(g_IntelliMouse_NativeOSSupport)
	{
		return;
	}

	extern BOOL IsPluginWnd( HWND hWnd, BOOL fTestParents);
	
	HWND focus = ::GetFocus();

	if (ISWINDOW(focus) && IsOperaWindow(focus) && !IsPluginWnd(focus, TRUE))
	{
		msg->hwnd = focus;

		BOOL leftHanded = GetSystemMetrics(SM_SWAPBUTTON);

		short zDelta = msg->wParam;
		short fwKeys = 0;

		if( GetKeyState(VK_CONTROL) & 0x8000)
			fwKeys |= MK_CONTROL;

		if( GetKeyState(VK_SHIFT) & 0x8000)
			fwKeys |= MK_SHIFT;

		if( GetAsyncKeyState(VK_LBUTTON) & 0x8000)
			fwKeys |= leftHanded ? MK_RBUTTON : MK_LBUTTON;

		if( GetAsyncKeyState(VK_MBUTTON) & 0x8000)
			fwKeys |= MK_MBUTTON;

		if( GetAsyncKeyState(VK_RBUTTON) & 0x8000)
			fwKeys |= leftHanded ? MK_LBUTTON : MK_RBUTTON;

		msg->message = WM_MOUSEWHEEL;

		msg->wParam = MAKELONG( fwKeys, zDelta);
	}
}
#endif



//The notch count is not necessarily integer
double IntelliMouse_GetNotchCount( HWND hwnd, short zDelta)
{
	static HWND hwndPrev = NULL;
	static int zDeltaRemaining = 0;

	//using an int to avoid overflow of the addition below
	int zDelta_int = zDelta;

	//	If not the same window then reset the notch counter.
	if( hwnd != hwndPrev)
	{
		hwndPrev = hwnd;
		zDeltaRemaining = 0;
	}

	//	DOIT
	zDelta_int += zDeltaRemaining;
	zDeltaRemaining = zDelta_int % WHEEL_DELTA;
	return (zDelta_int / (double)WHEEL_DELTA);
}



int IntelliMouse_GetLinesToScroll(HWND hwnd, short zDelta, BOOL* scroll_as_page)
{
	int scroll_lines;

	if(g_IntelliMouse_NativeOSSupport)
	{
		SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0 , &scroll_lines, 0);
	}
	else
	{
	   if (g_IntelliMouse_MsgScrollLines)
	   {
		  scroll_lines = (int) SendMessage(g_IntelliMouse_hwndMouseWheel, g_IntelliMouse_MsgScrollLines, 0, 0);
	   }
	   else
	   {
		  scroll_lines = 3;
	   }
	}

	if (scroll_lines == 0xffffffff)
	{
		*scroll_as_page = TRUE;
		scroll_lines = 1;
	}
	else
	{
		*scroll_as_page = FALSE;
	}

	scroll_lines *= IntelliMouse_GetNotchCount(hwnd, zDelta);
	//If the notch count is too small, scroll_lines could end up being zero.
	//We need at least to scroll of 1 or -1 since the scroll direction for 0
	//is not defined. See bug 249197
	if (scroll_lines == 0)
		if (zDelta > 0)
			scroll_lines = 1;
		else
			scroll_lines = -1;

	return scroll_lines;	
}


//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	IntelliMouse_OnMouseWheel
//
//	Default handler for WM_MOUSEWHEEL
//	Translates WM_MOUSEWHEEL message into WM_H/VSCROLL messages
//	Returns TRUE if processed
//	___________________________________________________________________________
//  
BOOL IntelliMouse_OnMouseWheel( HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	BOOL fProcessed = FALSE;

	//	WM_MOUSEWHEEL
	WORD	fwKeys	= LOWORD(wParam);			// key flags
	short	zDelta	= (short) HIWORD(wParam);   // wheel rotation

	// Don't allow mouse buttons being pressed at the same time as scrolling the wheel
	if ((fwKeys&MK_LBUTTON) || (fwKeys&MK_RBUTTON))
		return TRUE;	

	if (fwKeys==0)
	{
		BOOL scroll_as_page = FALSE;

		int nLinesToScroll = IntelliMouse_GetLinesToScroll(hWnd , zDelta, &scroll_as_page);

		int scrollCode;

		if (scroll_as_page)
		{
			scrollCode = nLinesToScroll < 0 ? SB_PAGEDOWN : SB_PAGEUP;
		}
		else
		{
			scrollCode = nLinesToScroll < 0 ? SB_LINEDOWN : SB_LINEUP;
		}

		nLinesToScroll = abs(nLinesToScroll);

		for (int i=0; i<nLinesToScroll; i++)
		{
			(void)(SendMessage)((hWnd), WM_VSCROLL, MAKEWPARAM((UINT)(int)(scrollCode), (UINT)(int)(0)), (LPARAM)(HWND)(NULL));
		}

		fProcessed = TRUE;
	}
	return fProcessed;
}


//	--------------------------------------------------------------------------
//	Static helper functions
//	--------------------------------------------------------------------------

static void _GetParams()
{
	g_IntelliMouse_NativeOSSupport = _HasNativeSupport();
//	g_IntelliMouse_NativeOSSupport = FALSE;

	if( g_IntelliMouse_NativeOSSupport)
	{
		g_IntelliMouse_MsgMouseWheel = WM_MOUSEWHEEL;
	}
	else
	{
	   g_IntelliMouse_hwndMouseWheel	= FindWindow(MSH_WHEELMODULE_CLASS, MSH_WHEELMODULE_TITLE);
	   g_IntelliMouse_MsgMouseWheel		= RegisterWindowMessage(MSH_MOUSEWHEEL);
	   g_IntelliMouse_MsgScrollLines	= RegisterWindowMessage(MSH_SCROLL_LINES);
	}
}


BOOL _HasNativeSupport()
{

    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize = sizeof(osvi);

	//	Get OS version
	if ( !GetVersionEx(&osvi) )
	{
		return FALSE;            // Got a better idea?
	}

	//	Win98 or later
	BOOL bIsWindows98orLater =  
		(osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) 
		&& ( (osvi.dwMajorVersion > 4) || ( (osvi.dwMajorVersion == 4) && (osvi.dwMinorVersion > 0) ) );
	if( bIsWindows98orLater)
		return TRUE;

    //	NT 4 and later supports WM_MOUSEWHEEL
    if ( VER_PLATFORM_WIN32_NT == osvi.dwPlatformId )
        if ( osvi.dwMajorVersion >= 4 )
            return TRUE;

    //	Future Win32 versions ( >= 5.0 ) should support WM_MOUSEWHEEL
    if ( osvi.dwMajorVersion >= 5 )
        return TRUE;

	return FALSE;
}

