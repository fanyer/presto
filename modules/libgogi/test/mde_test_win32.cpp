#include "core/pch.h"
#include <windows.h>
#include <windowsx.h>
#include <zmouse.h>
#include "mde.h"

#define MDE_SCREEN_W 800
#define MDE_SCREEN_H 600

void InitTest(MDE_Screen *screen);
void ShutdownTest();
void PulseTest(MDE_Screen *screen);

class TestWindow : public MDE_Screen
{
public:
	TestWindow();
	~TestWindow();

	// == Inherit MDE_View ========================================
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);

	// == Inherit MDE_Screen ======================================
	virtual void OutOfMemory();
	virtual MDE_FORMAT GetFormat();
	virtual MDE_BUFFER *LockBuffer();
	virtual void UnlockBuffer(MDE_Region *update_region);

public:
	bool quit;
	unsigned int *data;
	MDE_BUFFER screen;

	HWND hwnd;
	WNDCLASS wc;

	BITMAPINFO bi;
	HDC bitmaphdc;
	HBITMAP hBitmap;
};

VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	TestWindow *win = (TestWindow *) GetWindowLong(hwnd, GWL_USERDATA);
	win->Validate(true);
	PulseTest(win);
}

static ShiftKeyState WParamToKeyMod(WPARAM wParam)
{
	int keymod = SHIFTKEY_NONE;
	if ((GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0)
		keymod |= SHIFTKEY_CTRL;
	if ((GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) != 0)
		keymod |= SHIFTKEY_SHIFT;
	return static_cast<ShiftKeyState>(keymod);
}

long FAR PASCAL WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	RECT r;
	TestWindow *win = (TestWindow *) GetWindowLong(hwnd, GWL_USERDATA);
	switch(message)
	{
		case WM_ERASEBKGND:
			break;
		case WM_KEYDOWN:
			if ((int) wParam != VK_ESCAPE)
				break;
		case WM_CLOSE:
			win->quit = true;
			break;
		case WM_PAINT:
			if (GetUpdateRect(hwnd, &r, false))
			{
				win->Invalidate(MDE_MakeRect(r.left, r.top, r.right - r.left, r.bottom - r.top), true);
				ValidateRect(hwnd, &r);
			}
			break;

		case WM_LBUTTONDOWN:
			win->TrigMouseDown((short)LOWORD(lParam), (short)HIWORD(lParam), 1, 1, WParamToKeyMod(wparam)); SetCapture(hwnd); break;
		case WM_RBUTTONDOWN:
			win->TrigMouseDown((short)LOWORD(lParam), (short)HIWORD(lParam), 2, 1, WParamToKeyMod(wparam)); SetCapture(hwnd); break;
		case WM_MBUTTONDOWN:
			win->TrigMouseDown((short)LOWORD(lParam), (short)HIWORD(lParam), 3, 1, WParamToKeyMod(wparam)); SetCapture(hwnd); break;

		case WM_LBUTTONDBLCLK:
			win->TrigMouseDown((short)LOWORD(lParam), (short)HIWORD(lParam), 1, 2, WParamToKeyMod(wparam)); break;
		case WM_RBUTTONDBLCLK:
			win->TrigMouseDown((short)LOWORD(lParam), (short)HIWORD(lParam), 2, 2, WParamToKeyMod(wparam)); break;
		case WM_MBUTTONDBLCLK:
			win->TrigMouseDown((short)LOWORD(lParam), (short)HIWORD(lParam), 3, 2, WParamToKeyMod(wparam)); break;

		case WM_LBUTTONUP:
			win->TrigMouseUp((short)LOWORD(lParam), (short)HIWORD(lParam), 1, WParamToKeyMod(wparam)); ReleaseCapture(); break;
		case WM_RBUTTONUP:
			win->TrigMouseUp((short)LOWORD(lParam), (short)HIWORD(lParam), 2, WParamToKeyMod(wparam)); ReleaseCapture(); break;
		case WM_MBUTTONUP:
			win->TrigMouseUp((short)LOWORD(lParam), (short)HIWORD(lParam), 3, WParamToKeyMod(wparam)); ReleaseCapture(); break;

		case WM_MOUSEMOVE:
			win->TrigMouseMove((short)LOWORD(lParam), (short)HIWORD(lParam), WParamToKeyMod(wparam)); break;

		case WM_MOUSEWHEEL:
			win->TrigMouseWheel(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), ((short)HIWORD(wParam)), true, WParamToKeyMod(wparam)); break;

		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return FALSE;
}

TestWindow::TestWindow()
{
	quit = false;
	Init(MDE_SCREEN_W, MDE_SCREEN_H);

	// ~~~~ Create bitmap ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	bitmaphdc = CreateCompatibleDC(NULL);

	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = MDE_SCREEN_W;
	bi.bmiHeader.biHeight = -MDE_SCREEN_H;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biSizeImage = MDE_SCREEN_W * MDE_SCREEN_H * 4;
	bi.bmiHeader.biXPelsPerMeter = 0;
	bi.bmiHeader.biYPelsPerMeter = 0;
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	hBitmap = CreateDIBSection( bitmaphdc, &bi, DIB_RGB_COLORS, (void **)&data, NULL, 0 );
	SelectObject(bitmaphdc, hBitmap);

	// ~~~~ Create window ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	int x = 150, y = 100;
	RECT r = { x, y, x + MDE_SCREEN_W, y + MDE_SCREEN_H };
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, false);

	wc.style = CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = NULL;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszMenuName = "MDE Test";
	wc.lpszClassName = "MDE Test";
	RegisterClass(&wc);

	hwnd = CreateWindowEx(0, wc.lpszClassName, "MDE Test", WS_OVERLAPPEDWINDOW,
							r.left, r.top, r.right - r.left, r.bottom - r.top,
							NULL, NULL, NULL, NULL);
	SetWindowLong(hwnd, GWL_USERDATA, (long)this);
	ShowWindow(hwnd, SW_SHOWNORMAL);
}

TestWindow::~TestWindow()
{
	DestroyWindow(hwnd);
	// Leak some stuff. (Let windows do its best to free it)
}

void TestWindow::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_SetColor(MDE_RGB(69, 81, 120), screen);
	MDE_DrawRectFill(rect, screen);
}

MDE_FORMAT TestWindow::GetFormat()
{
	return MDE_FORMAT_BGRA32;
}

void TestWindow::OutOfMemory()
{
}

MDE_BUFFER *TestWindow::LockBuffer()
{
	MDE_InitializeBuffer(MDE_SCREEN_W, MDE_SCREEN_H, MDE_SCREEN_W * 4, MDE_FORMAT_BGRA32, data, NULL, &screen);
	return &screen;
}

void TestWindow::UnlockBuffer(MDE_Region *update_region)
{
	HDC hdc = GetDC(hwnd);
	for(int i = 0; i < update_region->num_rects; i++)
	{
		MDE_ASSERT(	update_region->rects[i].x >= 0 &&
					update_region->rects[i].y >= 0 &&
					update_region->rects[i].x + update_region->rects[i].w <= m_rect.w &&
					update_region->rects[i].y + update_region->rects[i].h <= m_rect.h);
		BitBlt(hdc, update_region->rects[i].x, update_region->rects[i].y, update_region->rects[i].w, update_region->rects[i].h,
					bitmaphdc, update_region->rects[i].x, update_region->rects[i].y, SRCCOPY);
	}
	ReleaseDC(hwnd, hdc);
}

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	TestWindow *window = new TestWindow();

	InitTest(window);
	
	SetTimer(window->hwnd, (long)0, 10, TimerProc);

	MSG msg;
	while(GetMessage(&msg, 0, 0, 0) && !window->quit)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	ShutdownTest();
	delete window;

	return 0;
}
