/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
* @author mzdun
*/

#ifndef OPWIDGETNOTIFIER_H
#define OPWIDGETNOTIFIER_H

#include "adjunct/desktop_pi/desktop_notifier.h"
#include "modules/hardcore/timer/optimer.h"
#include "adjunct/quick/widgets/OpOverlayWindow.h"

class BrowserDesktopWindow;

class OpWidgetNotifier
	: public OpOverlayWindow
	, public OpTimerListener
	, public DesktopNotifier
{
public:
	OpWidgetNotifier(OpWidget* widget);
	virtual				~OpWidgetNotifier();

	void				OnAnimationComplete();

	const char*			GetWindowName() {return "Widget Notification";}

	// need input context to respond to esc
	const char*			GetInputContextName() {return "Widget Notification";}
	void				OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	void				OnMouseMove(OpWidget *widget, const OpPoint &point);
					
	// OpTimerListener
	void				OnTimeOut(OpTimer* timer);

	// DesktopNotifier
	OP_STATUS			Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& title, Image& image, const OpStringC& text, OpInputAction* action, OpInputAction* cancel_action, BOOL managed = FALSE, BOOL wrapping = FALSE);
	OP_STATUS Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& text, const OpStringC8& image, OpInputAction* action, BOOL managed = FALSE, BOOL wrapping = FALSE) { return OpStatus::ERR_NOT_SUPPORTED; }
	OP_STATUS			AddButton(const OpStringC& text, OpInputAction* action) { return OpStatus::ERR_NOT_SUPPORTED; }
	void				StartShow();
	BOOL				IsManaged() const { return TRUE; }

	void				AddListener(DesktopNotifierListener* listener) { if (!m_listener) m_listener = listener; }
	void				RemoveListener(DesktopNotifierListener* listener) { if(m_listener == listener) m_listener = NULL; }
	void				SetReceiveTime(time_t time) {}

protected:
	OP_STATUS			InitContent(const OpStringC& title, const OpStringC& text);
	void				Layout();
	virtual	bool 		UsingAnimation() {return m_animate;}

private:
	OpWidget*			m_anchor;
	OpMultilineEdit*	m_edit;
	OpButton*			m_close_button;
	OpMultilineEdit*	m_longtext;
	OpButton*			m_image_button;
	OpTimer*			m_timer;
	OpInputAction*		m_action;
	OpInputAction*      m_cancel_action;
	DesktopNotifierListener* m_listener;
	BOOL				m_wrapping;
	Image				m_image;
	UINT				m_contents_width;
	UINT				m_contents_height;
	bool				m_animate;
};

#endif // OPWIDGETNOTIFIER_H
