/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __ICON_SOURCE_H__
#define __ICON_SOURCE_H__


namespace IconSource
{
	/**
	 * Return an icon in raw ico format. It is always quadratic (same width and height).
	 *
	 * @param width Should be 16, 32 or 48 on input. The actual width of the matched
	 *        quadratic icon is retuned. It can be different from the input width
	 * @param data On return, icon data
	 * @param size_in_bytes On return, size of icon data in bytes
	 *
	 * @return TRUE on success, otherwise FALSE
	 */
	BOOL GetApplicationIcon(UINT32& width, UINT8*& data, UINT32& size_in_bytes);
	BOOL GetSystemTrayUniteIcon(UINT32& width, UINT8*& data, UINT32& size_in_bytes);
	BOOL GetSystemTrayOperaIcon(UINT32& width, UINT8*& data, UINT32& size_in_bytes);
	BOOL GetSystemTrayMailIcon(UINT32& width, UINT8*& data, UINT32& size_in_bytes);
	BOOL GetSystemTrayChatIcon(UINT32& width, UINT8*& data, UINT32& size_in_bytes);

	BOOL GetDragAndDropIcon(UINT8*& data, UINT32& size_in_bytes);
	BOOL GetUpArrow(UINT8*& data, UINT32& size_in_bytes);
	BOOL GetLeftArrow(UINT8*& data, UINT32& size_in_bytes);
	BOOL GetCross(UINT8*& data, UINT32& size_in_bytes);
	BOOL GetMenuMinimize(UINT8*& data, UINT32& size_in_bytes);
	BOOL GetMenuRestore(UINT8*& data, UINT32& size_in_bytes);
	BOOL GetMenuClose(UINT8*& data, UINT32& size_in_bytes);
};



#endif
