/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef FORM_ELEMENT_OF_INTEREST_H
#define FORM_ELEMENT_OF_INTEREST_H

#include "modules/widgets/finger_touch/element_of_interest.h"
#include "modules/hardcore/mh/messobj.h"

class WidgetAnchorFragment;
class OpWidget;

class FormElementOfInterest : public ElementOfInterest, public MessageObject
{
private:
	virtual void DoActivate();
	virtual OpWidget* DoGetWidget() const { return clone; }
	virtual OpWidget* GetWidgetFromHtmlElement();

protected:
	WidgetAnchorFragment *widget_a_frag;
	OpWidget *clone;
	OpWidget *source;

	virtual OP_STATUS MakeClone(BOOL expanded);
	virtual int GetPreferredNumberOfRows(BOOL expanded) const;

	// Implementing OpWidgetListener:
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual void OnClick(OpWidget *widget, UINT32 id);
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse);
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect);

	// Implementing MessageObject:
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	virtual OpPoint UpdateFragmentPositions(int preferred_w, int preferred_h);

	/**
	   generates MSG_FINGERTOUCH_DELAYED_CLICK callback, passing arg as param2
	 */
	void DelayOnClick(void* arg = 0);

public:
	FormElementOfInterest(HTML_Element* html_element) :
		ElementOfInterest(html_element),
		widget_a_frag(0),
		clone(0),
		source(0) { }
	~FormElementOfInterest();

	/**
	 * OOM safe creation of a FormElementOfInterest object.
	 *
	 * @param html_element The html element of the element of interest.
	 * @return FormElementOfInterest* The created object if successful and NULL otherwise.
	 */
	static FormElementOfInterest* Create(HTML_Element* html_element);

	OP_STATUS InitContent();

	virtual OpWidget* GetSourceWidget() const { return source; }
	virtual OpWidget* GetCloneWidget() const { return clone; }

	virtual void OnEnd();
	virtual FormElementOfInterest *GetFormElementOfInterest() { return this; }
};

#endif // FORM_ELEMENT_OF_INTEREST_H
