#include "core/pch.h"

#include <WinUser.h>
#include "platforms/windows/windows_ui/oui.h"


/***********************************************************************************
**
**	OUIWindow
**
***********************************************************************************/

HINSTANCE	OUIWindow::s_Instance = NULL;
UINT		OUIWindow::s_WM_OUI_GETWINDOWOBJECT = 0;

/**********************************************************************************/

OUIWindow::OUIWindow()
{
	m_NotifyTarget	= NULL;
	m_HWnd			= NULL;
}

/**********************************************************************************/

OUIWindow::~OUIWindow()
{
	// assert below should not happen, it must have been destroyed before,
	// otherwise messages will arrive to objects where
	// derived classes has already been destructed. bad!

	OP_ASSERT(!m_HWnd);
}

/**********************************************************************************/

void OUIWindow::StaticRegister
(
	HINSTANCE in_Instance
)
{
	s_Instance					= in_Instance;
	s_WM_OUI_GETWINDOWOBJECT	= RegisterWindowMessage(UNI_L("WM_OUI_GETWINDOWOBJECT"));

	WNDCLASS wndClass;

	wndClass.style			= CS_DBLCLKS;
	wndClass.lpfnWndProc	= StaticWindowProc;
	wndClass.cbClsExtra		= 0;
	wndClass.cbWndExtra		= sizeof(OUIWindow *);
	wndClass.hInstance		= s_Instance;
	wndClass.hIcon			= LoadIcon(s_Instance, UNI_L("OPERA"));
	wndClass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground	= NULL;
	wndClass.lpszMenuName	= NULL;
	wndClass.lpszClassName	= UNI_L("OUIWINDOW");

	RegisterClass(&wndClass);
}

/**********************************************************************************/

HWND OUIWindow::Create(uni_char* name, int style, int exStyle, int x, int y, int width, int height, HWND parent, HMENU menu)
{
	return CreateWindowEx(exStyle, UNI_L("OUIWINDOW"), name, style, x, y, width, height, parent, menu, s_Instance, this);
}

/**********************************************************************************/
 
void OUIWindow::Destroy()
{
	if(m_HWnd)
	{
		::DestroyWindow(m_HWnd);
	}
}

/**********************************************************************************/

OUIWindow* OUIWindow::GetParent()
{
	HWND parent = GetParentHWnd();

	if(parent)
	{
		return StaticGetWindowObject(parent);
	}

	return NULL;
}

/**********************************************************************************/

LRESULT	OUIWindow::Notify
(
	int	in_NotifyCode
)
{
	if (m_NotifyTarget)
	{
		return ::PostMessage(m_NotifyTarget, WM_COMMAND, MAKELONG(0, in_NotifyCode), (LPARAM)m_HWnd);
	}
	else
	{
		return ::PostMessage(GetParentHWnd(), WM_COMMAND, MAKELONG(0, in_NotifyCode), (LPARAM)m_HWnd);
	}
}

/**********************************************************************************/
 
int OUIWindow::GetWindowWidth()
{
	RECT rect;

	GetWindowRect(m_HWnd, &rect);

	return rect.right - rect.left;
}

/**********************************************************************************/
 
int OUIWindow::GetWindowHeight()
{
	RECT rect;

	GetWindowRect(m_HWnd, &rect);

	return rect.bottom - rect.top;
}

/**********************************************************************************/
 
BOOL OUIWindow::IsPointInWindow
(
	int	in_X,
	int	in_Y
)
{
	POINT point;
	point.x = in_X;
	point.y = in_Y;

	RECT rect;
	GetClientRect(&rect);

	return ::PtInRect(&rect, point);
}

/**********************************************************************************/

LRESULT OUIWindow::OnNCDestroy(WPARAM wparam, LPARAM lparam)
{
	LRESULT result = DefaultWindowProc(WM_NCDESTROY, wparam, lparam);
	m_HWnd = NULL;
	return result;
}

/**********************************************************************************/

LRESULT OUIWindow::OnEraseBKGnd(WPARAM wparam, LPARAM lparam)
{
	return TRUE;
}

/**********************************************************************************/

LRESULT CALLBACK OUIWindow::StaticWindowProc
(
	HWND	in_HWnd, 
	UINT	in_Message,
	WPARAM	in_WParam,
	LPARAM	in_LParam
)
{
	OUIWindow* window = (OUIWindow*) GetWindowLongPtr(in_HWnd, 0);
	
	if (in_Message == WM_CREATE)
	{
		LPCREATESTRUCT lpcs = (LPCREATESTRUCT) in_LParam;

		window = (OUIWindow*) lpcs->lpCreateParams;

		if (window)
		{
			window->m_HWnd = in_HWnd;
		
			SetWindowLongPtr(in_HWnd, 0, (LONG_PTR)window);
		}
	}

	if(window)
	{
		return window->WindowProc(in_Message, in_WParam, in_LParam);
	}
	else
	{
		return DefWindowProc(in_HWnd, in_Message, in_WParam, in_LParam);
	}
}

