/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WIDGET_CONTAINER_H
#define WIDGET_CONTAINER_H

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpNamedWidgetsCollection.h"
#include "modules/pi/OpView.h"
#include "modules/pi/OpInputMethod.h"
#include "modules/display/coreview/coreview.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"

class OpWindow;
class DesktopWindow;
class VisualDevice;

/**
	A container for OpWidget's. For use in UI.
	Will set up a CoreView in the OpWidow or parent CoreView, create listeners to handle painting and input interaction
	with the widgets. It also takes care of some tab-focus navigation if the WidgetContainer is used in UI (has no document).
	Add new widgets under the root-widget (GetRoot()).
*/

class WidgetContainer : public OpInputContext
#ifdef DRAG_SUPPORT
, public CoreViewDragListener
#endif // DRAG_SUPPORT
, public OpPrefsListener
{
public:
	WidgetContainer(DesktopWindow* parent_desktop_window = NULL);
	~WidgetContainer();

	OP_STATUS Init(const OpRect &rect, OpWindow* window, CoreView* parentview = NULL);

	/** Set position of the CoreView in this WidgetContainer. */
	void SetPos(const AffinePos& pos);

	/** Set the size of the CoreView and the root widget in this WidgetContainer. */
	void SetSize(INT32 width, INT32 height);

	VisualDevice* GetVisualDevice();

	/** Get the root OpWidget. There is always a root widget in WidgetContainer, which other widgets should be added under. */
	OpWidget* GetRoot() { return root; }

	/** Get the CoreView of this WidgetContainer */
	CoreView* GetView();

	/** Get the OpView of this WidgetContainer. Remember that this WidgetContainer might not be positioned at 0,0 in the OpView.
		Use CoreView's convert-functions to translate positions to the OpView. */
	OpView* GetOpView();

	OpWindow* GetWindow() { return window; }

	/** Sets if the background should be cleared before painting childwidgets */
	void SetEraseBg(BOOL erase_bg) { this->erase_bg = erase_bg; }
	BOOL GetEraseBg() { return erase_bg; }

	void SetFocusable(BOOL focusable) { is_focusable = focusable; }
	BOOL IsFocusable() { return is_focusable; }

	void SetWidgetListener(OpWidgetListener* widget_listener) {root->SetListener(widget_listener);}

	OpWidget* GetNextFocusableWidget(OpWidget* widget, BOOL forward);

	void SetParentDesktopWindow(DesktopWindow* parent_desktop_window) {m_parent_desktop_window = parent_desktop_window;}
	DesktopWindow* GetParentDesktopWindow() {return m_parent_desktop_window;}

	void UpdateDefaultPushButton();

	// Implementing OpInputContext interface

	virtual BOOL			OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() {return "Widget Container";}

#ifdef DRAG_SUPPORT
	// Implementing CoreViewDragListener interface
	virtual void OnDragStart(CoreView* view, const OpPoint& start_point, ShiftKeyState modifiers, const OpPoint& current_point);
	virtual void OnDragEnter(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers);
	virtual void OnDragCancel(CoreView* view, OpDragObject* drag_object, ShiftKeyState modifiers);
	virtual void OnDragLeave(CoreView* view, OpDragObject* drag_object, ShiftKeyState modifiers);
	virtual void OnDragMove(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers);
	virtual void OnDragEnd(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers);
	virtual void OnDragDrop(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers);
#endif // DRAG_SUPPORT	

	/**
	 * Disables (if val==FALSE) default button which will make the container
	 * skip all changing of button styles when UpdateDefaultButton is called.
	 *
	 * The default is to have a default button.
	 */
	void SetHasDefaultButton(BOOL val) { m_has_default_button = val; }

	// Implementing OpPrefsListener interface

	virtual void PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue);

private:

	OpWidget* root;
	CoreView* view;
	OpWindow* window;
	BOOL erase_bg;
	BOOL is_focusable;
	DesktopWindow* m_parent_desktop_window;

	class ContainerPaintListener* paint_listener;
	class ContainerMouseListener* mouse_listener;
#ifdef TOUCH_EVENTS_SUPPORT
	class ContainerTouchListener* touch_listener;
#endif // TOUCH_EVENTS_SUPPORT

	BOOL m_has_default_button;
};

#ifdef WIDGETS_IME_SUPPORT

/** Inputmethod-listener for widgets */

class WidgetInputMethodListener : public OpInputMethodListener
{
public:
	WidgetInputMethodListener();

	IM_WIDGETINFO OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose);
	IM_WIDGETINFO GetWidgetInfo();
	IM_WIDGETINFO OnCompose();
	void OnCommitResult();
	void OnStopComposing(BOOL canceled);

#ifdef IME_RECONVERT_SUPPORT
	virtual void OnPrepareReconvert(const uni_char*& str, int& sel_start, int& sel_stop);
	virtual void OnReconvertRange(int sel_start, int sel_stop);
#endif // IME_RECONVERT_SUPPORT

	void OnCandidateShow(BOOL visible);

	BOOL IsIMEActive() { return inputcontext ? TRUE : FALSE; }
private:
	OpInputContext* inputcontext;
};

#endif // WIDGETS_IME_SUPPORT

#endif
