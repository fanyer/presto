/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#ifndef __X11_OPSYSTEMINFO_H__
#define __X11_OPSYSTEMINFO_H__

#include "platforms/unix/base/common/unix_opsysteminfo.h"
#include "platforms/unix/base/common/unix_opuiinfo.h"

class X11OpSystemInfo : public UnixOpSystemInfo
{
public:
	virtual INT32	GetShiftKeyState();

	virtual int		GetDoubleClickInterval() { return 500; }

	virtual BOOL	HasSystemTray();
};

class X11OpUiInfo : public UnixOpUiInfo
{
public:
	X11OpUiInfo() : m_vertical_dpi(0) {}
	UINT32 GetVerticalScrollbarWidth();
	UINT32 GetHorizontalScrollbarHeight();
	UINT32 GetSystemColor(OP_SYSTEM_COLOR color);
	void GetSystemFont(OP_SYSTEM_FONT font, FontAtt &outfont);
    COLORREF GetUICSSColor(int css_color_value);
    BOOL GetUICSSFont(int css_font_value, FontAtt &font);
	BOOL IsMouseRightHanded();
	BOOL IsFullKeyboardAccessActive();
	UINT32 GetCaretWidth();
	BOOL DefaultButtonOnRight();

private:
	void GetVerticalDpi();
	UINT32 GetHardcodedColor(OP_SYSTEM_COLOR color);
	inline UINT32 MakeColor(uint32_t color) { return OP_RGBA((color & 0xff0000) >> 16, (color & 0xff00) >> 8, (color & 0xff), (color & 0xff000000) >> 24); }
	inline int ConvertToPixelSize(int x) { return (x * m_vertical_dpi + 36) / 72; }

	UINT32 m_vertical_dpi;
};

#endif // __X11_OPSYSTEMINFO_H__
