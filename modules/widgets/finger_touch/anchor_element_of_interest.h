/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ANCHOR_ELEMENT_OF_INTEREST
#define ANCHOR_ELEMENT_OF_INTEREST

#include "modules/widgets/finger_touch/element_of_interest.h"
#include "modules/hardcore/timer/optimer.h"

class AnchorElementOfInterest : public ElementOfInterest, public OpTimerListener
{
private:
	OpTimer popupMenuTimer;
	BOOL popupMenuOpen;
	void OnTimeOut(OpTimer* timer);
	virtual BOOL DoIsSuitable(unsigned int max_elm_size);
	virtual void DoActivate();

protected:
	// Implementing OpWidgetListener:
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual void OnClick(OpWidget *widget, UINT32 id);
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect);

public:
	AnchorElementOfInterest(HTML_Element* html_element) :
		ElementOfInterest(html_element), popupMenuOpen(FALSE) { }

	virtual void GenerateInitialDestRects(float scale, unsigned int max_elm_size, const OpPoint& click_origin);
	virtual void TranslateDestination(int offset_x, int offset_y);

	void LayoutMultilineElements();
	OP_STATUS InitContent();

	virtual AnchorElementOfInterest *GetAnchorElementOfInterest() { return this; }
};

#endif // ANCHOR_ELEMENT_OF_INTEREST
