/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_COMPOSITE_H
#define QUICK_COMPOSITE_H

#include "adjunct/quick_toolkit/widgets/QuickBackgroundWidget.h"
#include "modules/widgets/OpWidget.h"

class OpButton;
class QuickButton;

/**
 * A widget that groups several widgets so that they act as one.
 *
 * OpComposite can be used to build widgets like thumbnails.
 *
 * An OpComposite will update its "hover" skin state as the mouse pointer
 * moves over itself or any of its descendants.
 *
 * If you add a button to an OpComposite and pass it to SetActionButton(),
 * any mouse click on the descendants of the OpComposite will be passed to
 * the button which was passed in aforementioned method.
 *
 * @authors Wojciech Dzierzanowski (wdzierzanowski), Deepak Arora (deepaka), 
 * Karianne Ekern (karianne.ekern)
 * 
 */
class OpComposite : public OpWidget
{
public:
	OpComposite() : m_hovered(false), m_button(NULL) { }

	void SetActionButton(OpButton* button);

	// OpWidget
	virtual Type GetType() { return OpTypedObject::WIDGET_TYPE_COMPOSITE; }
	virtual void AddChild(OpWidget* child, BOOL is_internal_child = FALSE, BOOL first = FALSE);
	virtual OP_STATUS GetText(OpString& text);
	virtual void OnMouseMove(const OpPoint& point);
	virtual void OnMouseLeave();
	virtual void OnMouseUp(const OpPoint& point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseDown(const OpPoint& point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);
	virtual OpScopeWidgetInfo* CreateScopeWidgetInfo();

	// OpWidgetListener
	virtual void OnFocusChanged(OpWidget* widget, FOCUS_REASON reason) { GenerateSetSkinState(this, SKINSTATE_SELECTED, widget->IsFocused() ? true : false); }

	// OpInputContext
	virtual void SetFocus(FOCUS_REASON reason);


private:
	class CompositeWidgetInfo;

	void GenerateSetSkinState(OpWidget* widget, SkinState mask, bool state);

	bool m_hovered;
	OpButton* m_button;
};


class QuickComposite
	: public QuickBackgroundWidget<OpComposite>
{
	typedef QuickBackgroundWidget<OpComposite> Base;
	IMPLEMENT_TYPEDOBJECT(Base);

public:
	QuickComposite() : Base(true) {}
};


#endif // QUICK_COMPOSITE_H
