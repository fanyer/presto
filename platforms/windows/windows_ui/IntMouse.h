//
//	FILE			IntMouse.h
//	
//	DESCRIPTION		IntelliMouse support routines
//
//	CREATED			DG[31] 18.feb.1998
//


#ifndef _INTMOUSE_H_INCLUDED_
#define _INTMOUSE_H_INCLUDED_

#include <zmouse.h>

#ifndef SPI_GETWHEELSCROLLLINES
	#define SPI_GETWHEELSCROLLLINES   104
#endif


//	IntelliMouse params.
extern HWND	g_IntelliMouse_hwndMouseWheel;
extern UINT	g_IntelliMouse_MsgMouseWheel;
extern UINT	g_IntelliMouse_Msg3DSupport;
extern UINT	g_IntelliMouse_MsgScrollLines;
extern BOOL	g_IntelliMouse_3DSupport;
extern INT	g_IntelliMouse_ScrollLines;
extern BOOL	g_IntelliMouse_NativeOSSupport;

//	Main functions
void	IntelliMouse_Init();
double	IntelliMouse_GetNotchCount( HWND hwnd, short zDelta);
int		IntelliMouse_GetLinesToScroll( HWND hwnd, short zDelta);

//	Default handler for WM_MOUSEWHEEL
//	Translates WM_MOUSEWHEEL message into WM_H/VSCROLL messages
//	Returns TRUE if processed
BOOL	IntelliMouse_OnMouseWheel( HWND hWnd, WPARAM wParam, LPARAM lParam);


#if !defined(OPTREEDLL)
void	IntelliMouse_TranslateWheelMessage( MSG *msg);
#endif

#endif	//	#ifndef _INTMOUSE_H_INCLUDED

