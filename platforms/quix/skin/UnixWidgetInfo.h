/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef UNIX_WIDGET_INFO_H
#define UNIX_WIDGET_INFO_H

#include "modules/skin/IndpWidgetInfo.h"

class UnixWidgetPainter;

class UnixWidgetInfo : public IndpWidgetInfo
{
public:
	UnixWidgetInfo(UnixWidgetPainter* widget_painter) : m_widget_painter(widget_painter) {}

	virtual BOOL GetScrollbarPartHitBy(OpWidget *widget, const OpPoint &pt, SCROLLBAR_PART_CODE &hitPart);
	virtual UINT32 GetVerticalScrollbarWidth();
	virtual UINT32 GetHorizontalScrollbarHeight();
	virtual INT32 GetScrollbarFirstButtonSize();
	virtual INT32 GetScrollbarSecondButtonSize();


private:
	UnixWidgetPainter* m_widget_painter;
};

#endif // UNIX_WIDGET_INFO_H
