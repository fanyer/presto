/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPNOTIFIER_H
#define OPNOTIFIER_H

#include "adjunct/desktop_pi/desktop_notifier.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/util/adt/opvector.h"
#include "adjunct/quick_toolkit/widgets/DesktopWidgetWindow.h"
#include "modules/widgets/OpWidget.h"

class OpMultilineEdit;
class OpHoverButton;
class OpNotifier;

class OpNotifier : public DesktopWidgetWindow, public OpTimerListener, public OpWidgetListener, public DesktopNotifier
{
public:
	OpNotifier();
	virtual ~OpNotifier();

	OP_STATUS Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& text, const OpStringC8& image, OpInputAction* action, BOOL managed = FALSE, BOOL wrapping = FALSE);
	OP_STATUS Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& title, Image& image, const OpStringC& text, OpInputAction* action, OpInputAction* cancel_action, BOOL managed = FALSE, BOOL wrapping = FALSE);
	
	void SetText(const OpStringC& text);
	void SetInputOffset(INT32 offset) {m_input_offset = offset;}

	/** Adds a button to the notifier below the title
	  * @param text Text on button
	  * @param action Action to execute when this button is clicked
	  */
	OP_STATUS AddButton(const OpStringC& text, OpInputAction* action);

	void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

	void OnMouseMove(OpWidget *widget, const OpPoint &point);

	void OnTimeOut(OpTimer* timer) { Animate(); }

	void StartShow();

	BOOL IsManaged() const { return m_managed; }

	// enable/disable wrapping of text
	void SetWrapping(BOOL wrapping) { m_wrapping = wrapping; }

	void AddListener(DesktopNotifierListener* listener) { if (!m_listener) m_listener = listener; }
	void RemoveListener(DesktopNotifierListener* listener) { if(m_listener == listener) m_listener = NULL; }
	void SetReceiveTime(time_t time) {}
	
#ifdef WIDGETS_IME_SUPPORT
//	IM_WIDGETINFO OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose);
//	IM_WIDGETINFO OnCompose();
//	IM_WIDGETINFO GetWidgetInfo();
//	void OnCommitResult();
//	void OnStopComposing(BOOL cancel);
#endif // WIDGETS_IME_SUPPORT

private:
	static const int DISAPPEAR_DELAY = 300;
	static const int WIDTH = 300;

	inline static bool IsAnimated();

	void Layout();
	void Animate();

	int						m_counter;
	INT32					m_input_offset;
	OpMultilineEdit*		m_edit;
	OpMultilineEdit*		m_longtext;
	OpVector<OpHoverButton>	m_buttons;
	OpButton*				m_image_button;
	OpButton*				m_close_button;
	OpTimer*				m_timer;
	OpInputAction*			m_action;
	OpInputAction*          m_cancel_action;
	BOOL					m_auto_close;
	OpRect					m_full_rect;
	BOOL					m_managed;
	DesktopNotifierListener*     m_listener;
	BOOL					m_wrapping;
	Image					m_image;
};




#endif // OPNOTIFIER_H
