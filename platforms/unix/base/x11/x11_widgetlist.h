/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_WIDGETLIST_H__
#define __X11_WIDGETLIST_H__

#include "modules/util/simset.h"
#include "platforms/utilix/x11types.h"

class X11Widget;

class X11WidgetList
{
public:
	struct Element : public ListElement<Element>
	{
		Element(X11Widget& widget) : widget(widget) {}
		X11Widget& widget;
	};

	X11WidgetList()
		:captured_widget(NULL)
		,popup_parent_widget(NULL)
		,grabbed_widget(NULL)
		,dragged_widget(NULL)
		,drag_source(NULL)
		,drop_target(NULL)
		,modal_toolkit_parent(NULL)
		,input_widget(NULL)
	{
	}

	~X11WidgetList();

	X11Widget *FindWidget(X11Types::Window handle);
	void AddWidget(Element& widget);
	void RemoveWidget(Element& widget);

	/** Set this widget as the modal widget.
	 * Doing this will make the event handling system ignore all input events
	 * not sent to this modal widget, or one of its descendants.
	 * The modal widgets mechanism is actually a stack, making it possible
	 * to have multiple modal widgets. May be useful if one modal dialog box
	 * opens another modal dialogbox.
	 * @param widget Widget to set as modal widget
	 */
	void AddModalWidget(Element& widget);

	/** Make this widget non-modal again.
	 * Remove it from the modal widgets stack - no matter whether it's the
	 * current active modal widget (top of the stack) or not.
	 * @param widget Widget to make non-modal
	 */
	void RemoveModalWidget(Element& widget);

	X11Widget* GetModalWidget();

	void UpdateCursor();
	// Called whenever the entire application is shown or hidden
	void OnApplicationShow(bool show);
	// Called when the application is about to shut down. After the user or seession
	// management can intervene and abort.
	void OnApplicationExit();

	void SetCursor(unsigned int cursor);
	void RestoreCursor();

	// A captured widget is a widget where the mde-implementation below holds a capture
    // It is captured as long as the mouse button is pressed
    void SetCapturedWidget(X11Widget *widget) { captured_widget = widget; }
    X11Widget* GetCapturedWidget() { return captured_widget; }

	// A popup widget does not have a real parent (widgetwize), but
	// we save the widget that was active when the popup was opened
	void SetPopupParentWidget(X11Widget *widget) { popup_parent_widget = widget; }
    X11Widget* GetPopupParentWidget() { return popup_parent_widget; }

	// A grabbed widget is a widget that has grabbed mouse (and possible) keyboard input
	X11Widget* GetGrabbedWidget() { return grabbed_widget; }

	// Add widget to grab stack. The active grabbed widget as returned by @ref
	// GetGrabbedWidget() is set regardless of return state
	// @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY
	OP_STATUS AddGrabbedWidget(X11Widget* widget);

	// Remove widget from grab stack. If the active grabbed widget is the same as the
	// widget then it is reset so that @ref GetGrabbedWidget() returns 0 until @ref
	// AddGrabbedWidget is called again.
	// @return OpStatus::OK on success, OpStatus::ERR if widget was no in list
	OP_STATUS RemoveGrabbedWidget(X11Widget* widget);

	// A dragged widget is a widget that dragged by the window manager on behalf of Opera
	void SetDraggedWidget(X11Widget *widget) { dragged_widget = widget; }
	X11Widget* GetDraggedWidget() { return dragged_widget; }

	// The drag source is the widget where the last drag started. It
	// shall only be used in code that deals with drag and drop.
	void SetDragSource(X11Widget* widget) { drag_source = widget; }
	X11Widget* GetDragSource() const { return drag_source; }

	// The drop target is the widget that is currently accepting a drop
	void SetDropTarget(X11Widget* widget) { drop_target = widget; }
	X11Widget* GetDropTarget() const { return drop_target; }

	// The parent of a toolkit window (dialog) that needs to be modal
	void SetModalToolkitParent(X11Widget* widget) { modal_toolkit_parent = widget; }
	X11Widget* GetModalToolkitParent() const { return modal_toolkit_parent; }

	// The last widget that has received an input event
	void SetInputWidget(X11Widget* widget) { input_widget = widget; }
	X11Widget* GetInputWidget() const { return input_widget; }

private:
	List<Element> widgets;
	X11Widget* captured_widget;
	X11Widget* popup_parent_widget;
	X11Widget* grabbed_widget;
	X11Widget* dragged_widget;
	X11Widget* drag_source;
	X11Widget* drop_target;
	X11Widget* modal_toolkit_parent;
	X11Widget* input_widget;
	OpVector<X11Widget> m_grab_stack;
};

#endif // __X11_WIDGETLIST_H__
