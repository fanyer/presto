/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef KDE4_WIDGET_PAINTER_H
#define KDE4_WIDGET_PAINTER_H

#include "platforms/quix/toolkits/ToolkitWidgetPainter.h"

class QRect;
class KApplication;

class Kde4WidgetPainter : public ToolkitWidgetPainter
{
public:
	Kde4WidgetPainter(KApplication* application) : m_app(application) {}

	virtual bool SupportScrollbar() const { return true; }
	virtual bool SupportSlider() const { return true; }

	virtual ToolkitSlider* CreateSlider();

	virtual ToolkitScrollbar* CreateScrollbar();
	virtual int GetVerticalScrollbarWidth();
	virtual int GetHorizontalScrollbarHeight();
	virtual int GetScrollbarFirstButtonSize();
	virtual int GetScrollbarSecondButtonSize();

private:
	QRect GetVerticalScrollbarGroove(int scrollbar_size);

	class Slider;
	class Scrollbar;

	KApplication* m_app;
};

#endif // KDE4_WIDGET_PAINTER_H
