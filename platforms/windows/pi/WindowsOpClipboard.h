/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPCLIPBOARD_H
#define WINDOWSOPCLIPBOARD_H

#include "modules/pi/OpClipboard.h"

class WindowsOpClipboard : public OpClipboard
{
public:
	WindowsOpClipboard();
	~WindowsOpClipboard();

	BOOL HasText();

	OP_STATUS PlaceText(const uni_char* text, UINT32 token);

	OP_STATUS GetText(OpString& text);

#ifdef DOCUMENT_EDIT_SUPPORT
	BOOL SupportHTML() { return TRUE; }

	BOOL HasTextHTML();

	OP_STATUS PlaceTextHTML(const uni_char* htmltext, const uni_char* text, UINT32 token);

	OP_STATUS GetTextHTML(OpString& text);
#endif // DOCUMENT_EDIT_SUPPORT

	OP_STATUS PlaceBitmap(const class OpBitmap* bitmap, UINT32 token);

	OP_STATUS EmptyClipboard(UINT32 token);

private:
	BOOL PlaceTextOnClipboard(const uni_char* text, UINT uFormat, int bufsize);
	BOOL GetTextFromClipboard(OpString& string, UINT uFormat);
	UINT32 ConvertLineBreaks(const uni_char* src, UINT32 src_size, uni_char* dst, UINT32 dst_size);
	UINT html_cf;
	UINT32 m_owner_token;
};

#endif // !WINDOWSOPCLIPBOARD_H
