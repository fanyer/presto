/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WIDGET_WINDOW_H
#define WIDGET_WINDOW_H

class WidgetContainer;
class WidgetWindowHandler;
class OpWindowListener;

#include "modules/pi/OpWindow.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
# include "modules/accessibility/opaccessibleitem.h"
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/hardcore/mh/messobj.h"
#include "modules/inputmanager/inputcontext.h"

class OpWidget;

/**
	WidgetWindow
	Minimal class to open a OpWindow and add a WidgetContainer.
*/

class WidgetWindow : public MessageObject, public OpWindowListener, public OpInputContext
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
				, public OpAccessibleItem
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
{
public:

	WidgetWindow(WidgetWindowHandler *handler = NULL);
	virtual ~WidgetWindow();

	OP_STATUS Init(OpWindow::Style style, OpWindow* parent_window = NULL, OpView* parent_view = NULL, UINT32 effects = 0, void* native_handle = NULL);

	class Listener
	{
	public:
		virtual ~Listener() {}

		virtual void OnShow(WidgetWindow *window, BOOL show) = 0;
		virtual void OnClose(WidgetWindow *window)           = 0;
	};

	void SetListener(Listener *listener);

	/** Close and delete this window. A WidgetWindow should rarely be closed
	 * immediately since it's not always certain it's not on the stack
	 * itself. @param immediately If immediately is FALSE, the window will
	 * dissapear from screen but delete itself later (posted message). @param
	 * user_initiated TRUE if the window was closed by the user, FALSE if
	 * closed by Opera itself.
	 */
	void Close(BOOL immediately = FALSE, BOOL user_initiated = FALSE);

	/** Show or hide the window.
	 * @param show TRUE for show, FALSE for hide.
	 * @param preferred_rect The window will set its position and size to this rect if it is being showed (and the rect is not NULL).
	 */
	void Show(BOOL show, const OpRect* preferred_rect = NULL);

	virtual WidgetContainer*	GetWidgetContainer()	{ return m_widget_container; }
	OpWindow*					GetWindow()				{ return m_window; }

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

		// accessibility methods
		virtual BOOL				AccessibilityIsReady() const {return TRUE; }
		virtual OP_STATUS			AccessibilityClicked();
		virtual OP_STATUS			AccessibilitySetValue(int value);
		virtual OP_STATUS			AccessibilitySetText(const uni_char* text);
		virtual OP_STATUS			AccessibilityGetAbsolutePosition(OpRect &rect);
		virtual OP_STATUS			AccessibilityGetValue(int &value);
		virtual OP_STATUS			AccessibilityGetText(OpString& str);
		virtual OP_STATUS			AccessibilityGetDescription(OpString& str);
		virtual OP_STATUS			AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut);
		virtual Accessibility::State AccessibilityGetState();
		virtual Accessibility::ElementKind AccessibilityGetRole() const;
		virtual OpWindow*			AccessibilityGetWindow() const {return m_window;}
		virtual int					GetAccessibleChildrenCount();
		virtual OpAccessibleItem*	GetAccessibleParent() {return NULL;};
		virtual OpAccessibleItem*	GetAccessibleChild(int);
		virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
		virtual OpAccessibleItem*	GetAccessibleChildOrSelfAt(int x, int y);
		virtual OpAccessibleItem*	GetNextAccessibleSibling();
		virtual OpAccessibleItem*	GetPreviousAccessibleSibling();
		virtual OpAccessibleItem*	GetAccessibleFocusedChildOrSelf();

		virtual OpAccessibleItem*	GetLeftAccessibleObject();
		virtual OpAccessibleItem*	GetRightAccessibleObject();
		virtual OpAccessibleItem*	GetDownAccessibleObject();
		virtual OpAccessibleItem*	GetUpAccessibleObject();

#endif //ACCESSIBILITY_EXTENSION_SUPPORT


	// OpInputContext

	virtual BOOL			IsInputContextAvailable(FOCUS_REASON reason) {return FALSE;}

	// OpWindowListener

	virtual void			OnResize(UINT32 width, UINT32 height);
	void					OnClose(BOOL user_initiated);
	virtual void			OnVisibilityChanged(BOOL vis);

	/**
	 * Helper function so that all dropdown like children won't have to implement the
	 * same calculations.
	 *
	 * @param widget The widget that will be the base of the 
	 * dropdown (be it a calendar or or a listbox or
	 * something else.
	 * @param width The width of the window.
	 * @param height The height of the window.
	 * @param limits If set, this rectangle sets a limit on the available area. It
	 *               will intersect with the workspace
	 */
	static OpRect GetBestDropdownPosition(OpWidget* widget, INT32 width, INT32 height, BOOL force_over_if_needed = FALSE, const OpRect* limits = 0);

protected:
	OpWindow*			m_window;
	WidgetContainer*	m_widget_container;
	WidgetWindowHandler *m_handler;

	Listener *m_listener;

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
};

#endif // WIDGET_WINDOW_H
