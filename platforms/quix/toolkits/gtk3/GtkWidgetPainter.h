/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#ifndef GTK3_WIDGET_PAINTER_H
#define GTK3_WIDGET_PAINTER_H

#include "platforms/quix/toolkits/ToolkitWidgetPainter.h"

class GtkWidgetPainter : public ToolkitWidgetPainter
{
public:
	GtkWidgetPainter() {}

	virtual bool SupportScrollbar() const { return false; }
	virtual bool SupportSlider() const { return true; }

	virtual ToolkitSlider* CreateSlider();

	virtual ToolkitScrollbar* CreateScrollbar() { return 0; }
	virtual int GetVerticalScrollbarWidth()     { return 0; }
	virtual int GetHorizontalScrollbarHeight()  { return 0; }
	virtual int GetScrollbarFirstButtonSize()   { return 0; }
	virtual int GetScrollbarSecondButtonSize()  { return 0; }

private:
	class Slider;
};

#endif // GTK3_WIDGET_PAINTER_H
