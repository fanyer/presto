/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_SPINNER_H
#define OP_SPINNER_H

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpButton.h"

class OpButton;

/** OpSpinner is containing two buttons. One up, and one down button. */

class OpSpinner : public OpWidget
#ifndef QUICK
			 , public OpWidgetListener
#endif // QUICK
{
protected:
	OpSpinner();
	//	virtual	~OpSpinnerberEdit();

public:
	static OP_STATUS Construct(OpSpinner** obj);

	void SetUpEnabled(BOOL enabled_up) { m_up_button->SetEnabled(enabled_up); }
	void SetDownEnabled(BOOL enabled_down) { m_down_button->SetEnabled(enabled_down); }

	void GetPreferedSize(INT32* w, INT32* h);

	// == Hooks ======================
	void OnResize(INT32* new_w, INT32* new_h);
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	void OnTimer();

	// Implementing the OpTreeModelItem interface
	//virtual Type		GetType() {return WIDGET_TYPE_SPINNER;}
	//virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_SPINNER || OpWidget::IsOfType(type); }

	// OpWidgetListener
	virtual void OnClick(OpWidget *object, UINT32 id);
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual void OnMouseLeave(OpWidget *widget);

#ifndef QUICK
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from OpWidget. */
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows) {}
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect) {}
	virtual void OnMouseLeave() {}
#endif // QUICK

private:
	void InvokeCurrentlyPressedButton();
	OpButton*	 m_up_button;
	OpButton*	 m_down_button;

	void UpdateButtonState();
};

#endif // OP_SPINNER_H
