/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef COCOA_OPCLIPBOARD_H
#define COCOA_OPCLIPBOARD_H

#include "core/pch.h"
#include "modules/pi/OpClipboard.h"

class OpBitmap;
class MacOpBitmap;

class CocoaOpClipboard : public OpClipboard
{
private:
	UINT32			m_owner_token;
	NSPasteboard*	m_service_pboard;
	NSArray*		m_service_types;
public:
	CocoaOpClipboard();
	virtual ~CocoaOpClipboard();

	static CocoaOpClipboard* s_cocoa_op_clipboard;

	/** Returns TRUE if there is text on the clipboard */
	virtual BOOL HasText();

	/** Places the text on the clipboard
		@param text a null-terminated string to place on the clipboard
		@return OpStatus::OK on success, error otherwise.
	 */
	virtual OP_STATUS PlaceText(const uni_char* text, UINT32 token);

	/**
	 * Allocates a copy of the clipboard-text and sets the pointer text
	 * to it. The ownership of the allocated memory is assigned to the
	 * caller ans must be deleted when no longer needed.
	 *
	 * @param text On return a pointer to the allocated memeory is returned
	 *        here.
	 */
	virtual OP_STATUS GetText(OpString &text);

#ifdef DOCUMENT_EDIT_SUPPORT
	virtual BOOL SupportHTML();
	virtual BOOL HasTextHTML();
	virtual OP_STATUS PlaceTextHTML(const uni_char* htmltext, const uni_char* text, UINT32 token);
	virtual OP_STATUS GetTextHTML(OpString &text);
#endif // DOCUMENT_EDIT_SUPPORT

	/** Places the bitmap on the clipboard
		@param bitmap a bitmap to place on the clipboard
		@return OpStatus::Ok on success, error otherwise.
	*/
	virtual OP_STATUS PlaceBitmap(const OpBitmap* bitmap, UINT32 token);

	virtual OP_STATUS EmptyClipboard(UINT32 token);
	
	void SetServicePasteboard(NSPasteboard *pboard, NSArray *types);
};

#endif // !COCOA_OPCLIPBOARD_H
