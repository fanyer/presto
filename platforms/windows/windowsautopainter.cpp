#include "core/pch.h"

#include "windowsautopainter.h"

static HWND old_hwnd = NULL;
static PAINTSTRUCT old_ps;
static HDC old_hdc = NULL;
static BOOL has_old_autopainter = FALSE;

WindowsAutoPainter::WindowsAutoPainter(HWND hwnd)
{
	OP_ASSERT(hwnd != NULL);
	OP_ASSERT(!has_old_autopainter);

	this->hwnd = hwnd;
	dc = ::BeginPaint(hwnd, &ps);

	OP_ASSERT(dc != NULL);

#ifdef _DEBUG
	has_old_autopainter = TRUE;
	old_hdc = dc;
	old_hwnd = hwnd;
	old_ps = ps;
#endif // _DEBUG
}

WindowsAutoPainter::~WindowsAutoPainter()
{
	OP_ASSERT(has_old_autopainter);

	OP_ASSERT(hwnd == old_hwnd);
	OP_ASSERT(dc == old_hdc);
	OP_ASSERT(ps.hdc == old_ps.hdc);
	OP_ASSERT(dc == ps.hdc);

	::EndPaint(hwnd, &ps);
#ifdef _DEBUG
	has_old_autopainter = FALSE;
#endif // _DEBUG
}

void WindowsAutoPainter::GetPaintStruct(PAINTSTRUCT* ps)
{
	*ps = this->ps;
}

BOOL WindowsAutoPainter::IsEmpty()
{
	return IsRectEmpty(&ps.rcPaint);
}
