/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _USER_FUN_H_
#define _USER_FUN_H_

#define ST_VCENTER			1
#define ST_FILLFONT			2
#define ST_HCENTER			4
#define ST_DOTTRUNCATED		8	//	NOTE: Implies NO wordbreak. (Win api bug (?) - DT_END_ELLIPSIS does not work with DT_WORDBREAK)

/** returns an unique filename based on the passed name, if catalog is specified in name this is used */
OP_STATUS GetUniqueFileName(OpString& inName, OpString& result);

OP_STATUS ParseMenuItemName(const OpStringC& name, OpString& parsed_name, int& amp_pos);
OP_STATUS DrawMenuItemText(HDC hdc, const OpStringC& text, LPCRECT rect, int underline_pos = -1);

#define BUTTON_DOWN 					0x000001
#define BUTTON_SELECTED					0x000002
#define BUTTON_DISABLED 				0x000004
#define BUTTON_HIGHLIGHT				0x000008
#define BUTTON_POPUP					0x000010
#define BUTTON_POPUP_SEPERATE			0x000020
#define BUTTON_INLINE_PUSHBUTTON		0x000040
#define BUTTON_NO_TEXT_ON_RIGHT			0x000080
#define BUTTON_ELLIPSIS					0x000100
#define BUTTON_FOCUS					0x000200
#define BUTTON_DEAD						0x000400
#define BUTTON_WRAP						0x000800
#define BUTTON_PUSHBUTTON				0x001000
#define BUTTON_BOLD						0x002000
#define BUTTON_TEXT_COLOR				0x004000
#define BUTTON_UP						0x008000
#define BUTTON_TEXT_ONLY				0x010000
#define BUTTON_TEXT_PREFIX				0x020000
#define BUTTON_TEXT_NO_PREFIX			0x040000
#define BUTTON_ATTENTION				0x080000
#define BUTTON_FLAT						0x100000

#define POPUP_ARROW_BUTTON_WIDTH  13
#define MAX_EXE_FILENAME 512

OP_STATUS GetExePath(OpString &target);
void TerminateCurrentHandles();

/**
 * Returns 2-letter ISO3166 country code for location set in OS
 *
 * @param result gets country code if available
 *
 * @result TRUE if country code for location was successfuly retrieved
 */
BOOL GetLocation(uni_char result[3]);

#endif	//	this file

