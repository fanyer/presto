//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?
//	FILE		docwin_mousewheel.cpp
//	CREATED		
//	
//	DESCRIPTION	
//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?
//  


#include "core/pch.h"

#include <math.h>

#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"

#include "modules/inputmanager/inputaction.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/windowcommander/OpViewportController.h"
#include "modules/windowcommander/OpWindowCommander.h"

#include "platforms/windows/CustomWindowMessages.h"
#include "platforms/windows/pi/WindowsOpView.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/windows_ui/IntMouse.h"
#include "platforms/windows/windows_ui/docwin_mouse.h"
#include "platforms/windows/user_fun.h"

#define PANNINGRESOLUTION 1000/75
static UINT wTimerRes = 0;
static OpView* viewWheelScrollActive = NULL;
static WindowsOpView* scrolling_view = NULL;
static OpWindowCommander* winCommander = NULL;
static LONG bTimerPending = FALSE;
static MMRESULT mmtimerPanning = 0;
static HANDLE timerPanning = NULL;
static BOOL panning_activation = FALSE;

// in WindowsOpMessageLoop.cpp
extern HINSTANCE hInst;

BOOL IsPanning()
{
	return viewWheelScrollActive != NULL;
}

void CALLBACK PostPanningMessage(UINT wTimerID, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
    if (!bTimerPending)
	{
		PostMessage((HWND)dwUser,WM_OPERA_TIMERDOCPANNING, 0,0);
		bTimerPending = TRUE;
	}
}

// Use InterlockedCompareExchange as this callback is in a different thread
void CALLBACK PostPanningMessage2(PVOID dwUser, BOOLEAN TimerOrWaitFired)
{
    if (!InterlockedCompareExchange(&bTimerPending,TRUE,FALSE))
	{
		PostMessage((HWND)dwUser,WM_OPERA_TIMERDOCPANNING, 0,0);
	}
}

void EndPanning()
{
	if (mmtimerPanning || timerPanning )
	{
		DeleteTimerQueueTimer(NULL,timerPanning,INVALID_HANDLE_VALUE);
		timerPanning = NULL;
		bTimerPending = FALSE;
	}

	if (viewWheelScrollActive)
	{
		ReleaseCapture();
		SetCursor( LoadCursor( NULL, IDC_ARROW));
		winCommander = NULL;
	}

	viewWheelScrollActive = NULL;
}

VOID OnWMOperaTimerDocPanning(HWND hWnd)
{
	static POINT	ptRef;							  //	Ref. point
	static HCURSOR	hCursorPanning[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };  //	START - UP - DOWN
	static float    pixelsToScrollX = 0.0f;
	static float    pixelsToScrollY = 0.0f;
	static LONG		x_block = 0;
	static LONG		y_block = 0;

	MSG				msg;						//	We are peeking messages

	bTimerPending = FALSE;

	//	Terminate panning ?  (have to peek messages -- user input has
	//	low priority)
    if (!PeekMessage(&msg, hWnd, WM_LBUTTONDOWN, WM_MOUSELAST, PM_NOREMOVE) &&
		!PeekMessage(&msg, hWnd, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE) &&
		!PeekMessage(&msg, hWnd, WM_MOUSEWHEEL, WM_MOUSEWHEEL, PM_NOREMOVE))
	{
		if (!viewWheelScrollActive)
		{
			//	INIT (this is the first call after starting wheel scroll)
			SetActiveWindow(hWnd);

			viewWheelScrollActive = scrolling_view;

			//winView->ReleaseMouseCapture();

			//New behavior: Start panning from wherever the cursor is
			GetCursorPos(&ptRef);

			//Old behavior: Center the mouse pointer before starting panning
			/*OpPoint view_pos = view->ConvertToScreen(OpPoint(0, 0));
			ptRef.x = view_pos.x + view_rect.width/2;
			ptRef.y = view_pos.y + view_rect.height/2;

			SetCursorPos(ptRef.x, ptRef.y);*/
		}
		else
		{

			UINT32 width, height;
			viewWheelScrollActive->GetSize(&width, &height);
			OpRect view_rect = OpRect(0, 0, width, height);
			OpPoint view_pos = OpPoint(0, 0);
			view_pos = viewWheelScrollActive->ConvertToScreen(OpPoint(0, 0));
			OpRect visual_viewport = OpRect(0, 0, 0, 0);
			unsigned int doc_width = 0, doc_height = 0;
			OpViewportController* viewportController = winCommander ? winCommander->GetViewportController() : NULL;
			if (viewportController)
			{
				visual_viewport = viewportController->GetRenderingViewport();
				viewportController->GetDocumentSize(&doc_width, &doc_height);
			}
			INT32 xpos = visual_viewport.x;
			INT32 ypos = visual_viewport.y;
			INT32 right = visual_viewport.Right();
			INT32 bottom = visual_viewport.Bottom();

			POINT ptCursor;
			GetCursorPos(&ptCursor);

			//	Calc cursor offset

			int cx = max(1, abs(view_rect.width));
			int cy = max(1, abs(view_rect.height));

			float dx = float(ptCursor.x - ptRef.x);
			float dy = float(ptCursor.y - ptRef.y);

			if (fabs(dx) <= 5 || (dx < -5 && xpos == 0) || (dx > 5 && right > 0 && (UINT)right >= doc_width))
			{
				dx = 0;
			}

			if (fabs(dy) <= 5 || (dy < -5 && ypos == 0) || (dy > 5 && bottom > 0 && (UINT)bottom >= doc_height))
			{
				dy = 0;
			}

			POINT pt;

			//(julienp) The following is used to test wether the mousecursor
			//			is blocked by the border of the screen. If that is the case,
			//			it should behave like the mousecursor is moving further away
			//			in that direction (continue to accelerate)
			//			It should also come back to a normal speed when the cursor is
			//			moved away from the edge again
			pt.y = ptCursor.y;
			if (dx < 0)
			{
				pt.x = ptCursor.x - 1;
				if (!MonitorFromPoint(pt, MONITOR_DEFAULTTONULL))
					x_block --;
				else
					x_block = 0;
			}
			else if (dx > 0)
			{
				pt.x = ptCursor.x + 1;
				if (!MonitorFromPoint(pt, MONITOR_DEFAULTTONULL))
					x_block ++;
				else
					x_block = 0;
			}
			else
				x_block = 0;

			pt.x = ptCursor.x;
			if (dy < 0)
			{
				pt.y = ptCursor.y - 1;
				if (!MonitorFromPoint(pt, MONITOR_DEFAULTTONULL))
					y_block --;
				else
					y_block = 0;
			}
			else if (dy > 0)
			{
				pt.y = ptCursor.y + 1;
				if (!MonitorFromPoint(pt, MONITOR_DEFAULTTONULL))
					y_block ++;
				else
					y_block = 0;
			}
			else
				y_block = 0;

			if (!hCursorPanning[0])
			{
				hCursorPanning[0] = LoadCursorA(hInst, "PAN_SUD");
				hCursorPanning[1] = LoadCursorA(hInst, "PAN_UP");
				hCursorPanning[2] = LoadCursorA(hInst, "PAN_DOWN");
				hCursorPanning[3] = LoadCursorA(hInst, "PAN_LEFT");
				hCursorPanning[4] = LoadCursorA(hInst, "PAN_RIGHT");
				hCursorPanning[5] = LoadCursorA(hInst, "PAN_UPLEFT");
				hCursorPanning[6] = LoadCursorA(hInst, "PAN_UPRIGHT");
				hCursorPanning[7] = LoadCursorA(hInst, "PAN_DOWNLEFT");
				hCursorPanning[8] = LoadCursorA(hInst, "PAN_DOWNRIGHT");
			}

			//	Set cursor

			INT32 cursor = 0;

			if (dy < -5)
			{
				if (dx < -5)
				{
					cursor = 5;
				}
				else if (dx > 5)
				{
					cursor = 6;
				}
				else
				{
					cursor = 1;
				}
			}
			else if (dy > 5)
			{
				if (dx < -5)
				{
					cursor = 7;
				}
				else if (dx > 5)
				{
					cursor = 8;
				}
				else
				{
					cursor = 2;
				}
			}
			else
			{
				if (dx < -5)
				{
					cursor = 3;
				}
				else if (dx > 5)
				{
					cursor = 4;
				}
			}

			SetCursor(hCursorPanning[cursor]);

			if (cursor)
			{
				//	The response curve

				dx = float(pow(float(ptCursor.x - ptRef.x + x_block) / cx, 3) * float(cx));
				dy = float(pow(float(ptCursor.y - ptRef.y + y_block) / cy, 3) * float(cy));

				pixelsToScrollX += dx;
				pixelsToScrollY += dy;

				//	Scroll

				if (fabs(pixelsToScrollX) >= 1.0f || fabs(pixelsToScrollY) >= 1.0f)
				{
					INT32 new_x = xpos + (INT32) pixelsToScrollX;
					INT32 new_y = ypos + (INT32) pixelsToScrollY;
					new_x = MIN((INT32)doc_width - visual_viewport.width, new_x);
					new_y = MIN((INT32)doc_height - visual_viewport.height, new_y);
					new_x = MAX(0, new_x);
					new_y = MAX(0, new_y);
					if (viewportController)
					{
						viewportController->SetBufferedMode(TRUE);
						viewportController->SetVisualViewportPos(OpPoint(new_x, new_y));
						viewportController->SetRenderingViewportPos(OpPoint(new_x, new_y));
						viewportController->SetBufferedMode(FALSE);

						// We must invalidate because SetBufferedMode doesn't work 2 times in a row without getting paint in between.
						WindowsOpWindow *window = WindowsOpWindow::GetWindowFromHWND(hWnd);
						if (window)
							window->Validate();
					}

					pixelsToScrollX = (float)fmod(pixelsToScrollX, 1);
					pixelsToScrollY = (float)fmod(pixelsToScrollY, 1);
				}
			}
		}
	}
}

BOOL DocWin_HandleIntelliMouseMessages(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (g_application && g_application->GetMenuHandler() && 
			g_application->GetMenuHandler()->IsShowingMenu())
	{
		return FALSE;
	}

	if (viewWheelScrollActive)
	{
		//	Terminate AutoScroll on all user input except WM_MOUSEMOVE
		if (message != WM_MBUTTONUP && (((message >= WM_LBUTTONDOWN) && (message < WM_MOUSELAST))
			|| ((message > WM_KEYFIRST) && (message < WM_KEYLAST)
			||  (message == WM_MOUSEWHEEL))))
		{
			EndPanning();
			return TRUE;
		}
		
		switch (message)
		{
		case 0:
		case WM_KILLFOCUS:
		case WM_CAPTURECHANGED:
			EndPanning();
			return TRUE;
		}
	}

	switch (message)
	{
	case WM_OPERA_TIMERDOCPANNING:
		if (mmtimerPanning || timerPanning)
			OnWMOperaTimerDocPanning(hWnd);
		return FALSE;
	
	case WM_KILLFOCUS:
		if (viewWheelScrollActive)
		{
			EndPanning();
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void DocWin_HandleIntelliMouseWheelDown(OpWindowCommander* commander, OpView* view)
{
	winCommander = commander;

	WindowsOpView* winView = (WindowsOpView*)view;
	scrolling_view = winView;


	HWND hWnd = winView->GetNativeWindow() ? winView->GetNativeWindow()->m_hwnd : NULL;

	if (viewWheelScrollActive != view)
	{
		EndPanning();

		if (!wTimerRes)
		{
			TIMECAPS tc;

			if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
				return;
			
			wTimerRes = min(max(tc.wPeriodMin, PANNINGRESOLUTION), tc.wPeriodMax);
		}

		//	Start panning
		SetActiveWindow( hWnd);
		//SetCapture(hWnd);

		CreateTimerQueueTimer(&timerPanning, NULL, PostPanningMessage2, (PVOID)hWnd, wTimerRes, wTimerRes, NULL);
		bTimerPending = FALSE;
	}
	else
	{
		EndPanning();
	}
}

