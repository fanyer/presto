/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_NATIVE_DIALOG_H__
#define __X11_NATIVE_DIALOG_H__

#include "platforms/utilix/x11_all.h"

/**
 * This is a small collection of X11 based window widgets and an Application class
 * that can (and should only) be used in case we have to show a dialog before 
 * core and quick has been set up.
 *
 * There is no skinning on these window widgets.
 * 
 * One want to use @ref X11NativeDialog::ShowTwoButtonDialog() or similar
 */


class X11NativeButtonListener
{
public:
	/**
	 * Called when a button has been activated
	 *
	 * @param is Button identifier
	 */
	virtual void OnButtonActivated(int id) = 0;
};

/**
 * General layout manager that can be implemented for all sorts of layouts. The 
 * layout is owned by a widget which uses it position sub widgets. The layout 
 * knows about the content, the widget does not.
 */
class X11NativeLayout
{
public:
	/**
	 * Sets the geometry of widgets maintained by the layout
	 *
	 * @param width Available width
	 * @param height Available height
	 */
	virtual void Resize(UINT32 width, UINT32 height) = 0;

	/**
	 * Returns the minimun size the layout needs to properly show the widgets
	 * The owner widget must then try to resize itself to allow for the 
	 * recomended size
	 *
	 * @param width On return the minimum width
	 * @param height On return the minimum height
	 */
	virtual void GetMinimumSize(UINT32& width, UINT32& height) = 0;
};


/**
 * Base class for all window widgets
 */
class X11NativeWindow 
{
public:
	enum WindowFlags
	{
		NORMAL    = 0, // Normal window
		UNMANAGED = 1, // No window manager decorations
		MODAL     = 2  // Dialogs
	};

public:
	/**
	 * Initializes the widget
	 *
	 * @param parent Widget parent. Can be 0
	 * @param flags Widget flags
	 * @param rect The widget size and position. If 0 a default size of 100x100 and position of 0,0 is used
	 *
	 */
	X11NativeWindow(X11NativeWindow* parent, int flags = NORMAL, OpRect* rect=0);

	/**
	 * Removes all allocated resources
	 */
	~X11NativeWindow();

	/**
	 * Makes the widget visible
	 */
	virtual	void Show();

	/**
	 * Hides th widget
	 */
	virtual void Hide();

	/**
	 * Called just before and just after widget is mapped (made visible)
	 *
	 * @param before Just before if true, just after if false
	 */
	virtual void OnMapped(bool before) {}

	/**
	 * Examines an event sent to this widget.
	 *
	 * @param event The event
	 *
	 * @return true is further processing should stop, otherwise false
	 */
	virtual bool HandleEvent(XEvent* event);

	/**
	 * Alias for @ref Hide(). Should be reimplmented to allow a widget
	 * to know why is was hidden as is was hidden
	 */
	virtual	void Accept() { Hide(); }

	/**
	 * Alias for @ref Hide(). Should be reimplmented to allow a widget
	 * to know why is was hidden as is was hidden
	 */
	virtual	void Cancel() { Hide(); }

	/**
	 * Draw the widget content. 
	 *
	 * @param rect Area that at least must be updated
	 */
	virtual void Repaint(const OpRect& rect);
	virtual bool AcceptFocus() const { return false; }
	virtual bool HasFocus() const { return m_has_focus; }

	/**
	 * Returns the preferred size of the widget
	 *
	 * @param width On return the preferred width
	 * @param heigth On return the preferred height
	 */
	virtual void GetPreferredSize(UINT& width, UINT& height) { width = height = 1; }
	
	/**
	 * Sets widget position
	 * 
	 * @param x X coordinate 
	 * @param y Y coordinate 
	 */
	void SetPosition(int x, int y);

	/**
	 * Sets widget size
	 * 
	 * @param width Widget width 
	 * @param height Widget height
	 */
	void SetSize(UINT32 width, UINT32 height);

	/**
	 * Updates widget geometry without actually modifying the actual geometry.
	 * Typically called when the geometry has already been changed and the internal
	 * geometry must be updated to match it.
	 * 
	 * @param rect The new geometry
	 *
	 */
	void SetRect(const OpRect& rect) { m_rect.Set(rect); }

	/**
	 * Sets minimum widget size
	 * 
	 * @param width Widget width 
	 * @param height Widget height
	 */
	void SetMinimumSize(UINT32 width, UINT32 height);

	/**
	 * Sets the widget title. Affects only toplevel widgets
	 *
	 * @param title The new title
	 */
	void SetTitle(const OpStringC8& title);

	/**
	 * Sets the widget text. Not all widgets use this data
	 * so result depends on the reimplemented widget
	 * 
	 * @param width Widget width 
	 * @param height Widget height
	 */
	void SetText(const OpStringC8& text);

	/**
	 * Specifies the widget to stay above all other widgets
	 *
	 * @param state Enable stay on top if true, turn off if false.
	 */
	void SetAbove(bool state);

	/**
	 * Assignes a layout manager to the widget. Only the 
	 * layout manager knows how to position the child widgets
	 *
	 * The widget takes ownership of the manager
	 *
	 * @param layout The layout manager
	 */
	void SetLayout(X11NativeLayout* layout) { if (layout != m_layout) {OP_DELETE(m_layout); m_layout = layout;} }

	/**
	 * Returns parent widget
	 */
	X11NativeWindow* GetParent() const { return m_parent; }
	
	/**
	 * Returns X11 window handle
	 */
	X11Types::Window GetHandle() const { return m_handle; }

	/**
	 * Returns active layout manager as set by @ref SetLayout()
	 */
	X11NativeLayout* GetLayout() const { return m_layout; }

	/**
	 * Returns the window depth in pixels
	 */
	int GetDepth() const { return m_depth; }

	/**
	 * Returns the widget flags
	 */
	int GetFlags() const { return m_flags; }

	/**
	 * Returns active text as set by @ref SetText()
	 */
	const OpStringC8& GetText() const { return m_text; }

	/**
	 * Returns the current geometry 
	 */
	const OpRect& GetRect() const { return m_rect; }

private:
	X11NativeWindow* m_parent;
	X11Types::Window m_handle;
	X11NativeLayout* m_layout;
	XSizeHints* m_size_hints;
	int  m_depth;
	int  m_flags;
	bool m_has_focus;
	OpString8 m_text;
	OpRect m_rect;
};


class X11NativeButton : public X11NativeWindow 
{
public:
	/**
	 * Initializes the button widget
	 *
	 * @param parent Widget parent. Can be 0
	 * @param flags Widget flags
	 * @param rect The widget size and position. If 0 a default size of 100x100 and position of 0,0 is used
	 *
	 */
	X11NativeButton(X11NativeWindow* parent, int flags = NORMAL, OpRect* rect=0);

	/**
	 * Examines an event sent to this widget.
	 *
	 * @param event The event
	 *
	 * @return true is further processing should stop, otherwise false
	 */
	bool HandleEvent(XEvent* event);

	/**
	 * Draw the widget content. 
	 *
	 * @param rect Area that at least must be updated
	 */
	void Repaint(const OpRect& rect);
	bool AcceptFocus() const { return true; }

	/**
	 * Returns the preferred size of the widget
	 *
	 * @param width On return the preferred width
	 * @param heigth On return the preferred height
	 */
	void GetPreferredSize(UINT& width, UINT& height);
	
	/**
	 * Assigns a listener to this button. Only one listener is supported
	 *
	 * @param listener The button listener
	 */
	void SetX11NativeButtonListener(X11NativeButtonListener* listener) { m_listener = listener; }

	/**
	 * Sets button identifier
	 *
	 * @param id The button identifier
	 */
	void SetId(int id) { m_id = id; }

private:
	X11NativeButtonListener* m_listener;
	int m_id;
	bool m_down;
	bool m_inside;
	bool m_pressed;
};


class X11NativeLabel : public X11NativeWindow 
{
	class TextCollection
	{
	public:
		void Reset() { list.DeleteAll(); width = line_height = ascent = descent = 0; }

	public:
		OpAutoVector<OpString8> list;
		INT32 width;
		INT32 line_height;
		INT32 ascent;
		INT32 descent;
	};

public:
	/**
	 * Initializes the label widget
	 *
	 * @param parent Widget parent. Can be 0
	 * @param flags Widget flags
	 * @param rect The widget size and position. If 0 a default size of 100x100 and position of 0,0 is used
	 *
	 */
	X11NativeLabel(X11NativeWindow* parent, int flags = NORMAL, OpRect* rect=0);

	/**
	 * Draw the widget content. 
	 *
	 * @param rect Area that at least must be updated
	 */
	void Repaint(const OpRect& rect);

	/**
	 * Returns the preferred size of the widget
	 *
	 * @param width On return the preferred width
	 * @param heigth On return the preferred height
	 */
	void GetPreferredSize(UINT& width, UINT& height);

	/**
	 * Processes the text set by @ref SetText() into multiple lines 
	 * if there is not enough room on one line.
	 *
	 * @param width Maximum width in pixels of a line of text using 
	 *        the current font
	 */
	void FormatText(INT32 width);

private:
	TextCollection m_text_collection;
};



class X11NativeDialog : public X11NativeWindow, public X11NativeButtonListener
{
public:
	enum DialogResult
	{
		REP_YES    = 0,
		REP_OK     = 0,
		REP_NO     = 1,
		REP_CANCEL = 1
	};

public:
	/**
	 * Initializes the dialog widget
	 *
	 * @param parent Widget parent. Can be 0
	 * @param flags Widget flags
	 * @param rect The widget size and position. If 0 a default size of 100x100 and position of 0,0 is used
	 *
	 */
	X11NativeDialog(X11NativeWindow* parent, int flags = NORMAL, OpRect* rect=0);

	/**
	 * Opens dialog. Will not return until @ref Accept() or @ref Cancel has been called 
	 *
	 * @return REP_OK if @ref Accept() is called, otherwise REP_CANCEL
	 */
	DialogResult Exec();

	/**
	 * Called just before and just after widget is mapped (made visible)
	 *
	 * @param before Just before if true, just after if false
	 */
	void OnMapped(bool before);

	/**
	 * Examines an event sent to this widget.
	 *
	 * @param event The event
	 *
	 * @return true is further processing should stop, otherwise false
	 */
	bool HandleEvent(XEvent* event);

	/**
	 * Sets return code to REP_OK and closes the dialog
	 */
	void Accept() { m_result = REP_OK; Hide(); }

	/**
	 * Sets return code to REP_CANCEL and closes the dialog
	 */
	void Cancel() { m_result = REP_CANCEL; Hide(); }

	/**
	 * Calls either @ref Accept() or @ref Cancel() depending identifier
	 * 
	 * @param id Should be one of enum DialogResult
	 */
	void OnButtonActivated(int id) 
	{ 
		if (id == REP_OK)
			Accept();
		else if (id == REP_CANCEL)
			Cancel();
	}

public:
	/**
	 * Shows a stack modal dialog with two action buttons and a message
	 * 
	 * @param title Dialog title
	 * @param message Dialog message
	 * @param ok Wording for accept button     
	 * @param cancel Wording for cancel button
	 *
	 * @param return A value from DialogResult
	 */
	static DialogResult ShowTwoButtonDialog(const char* title, const char* message, const char* ok, const char* cancel);

private:
	DialogResult m_result;
};


/** 
 * A small application class that can only be use of widgets of this file 
 */
class X11NativeApplication
{
public:
	/**
	 * Returns the one and only instance of the X11NativeApplication object
	 */
	static X11NativeApplication* Self();

	/**
	 * Initializes the application state
	 */
	X11NativeApplication();
	
	/**
	 * Adds a window to the list of maintained widget windows
	 *
	 * @param window Window to add
	 */
	void AddWindow(X11NativeWindow* window);

	/**
	 * Removes a window from the list of maintained widget windows
	 *
	 * @param window Window to remove
	 */
	void RemoveWindow(X11NativeWindow* window);

	/**
	 * Sets the window that should be treated as the active focused window.
	 * A visual update will only happen after next FocusIn event from system
	 *
	 * @param window Window to be set as focused window
	 */
	void SetFocus(X11NativeWindow* window) { m_focused_window = window; }

	/**
	 * Moves keyboard focus to the next or previous widget window that 
	 * accepts keyboard focus 
	 * 
	 * @param next Go to next if true, otherwise previous
	 * @param time The time when focus changes. Can be CurrentTime
	 */
	void StepFocus(bool next, X11Types::Time time);
	
	/**
	 * Assigns initial focus to the first widget window that accepcts 
	 * focus
	 */ 
	void InitFocus();
	
	/**
	 * Returns the window widget that uses the specified X11 window
	 * handle
	 *
	 * @param handle The window handle
	 *
	 * @return The window widget, or NULL on no match
	 */
	X11NativeWindow* GetWindow(X11Types::Window handle);

	/**
	 * Enters the main event loop. The loop is not terminated until
	 * @ref LeaveLoop() is called. One can call EnterLoop() recursively
	 */
	void EnterLoop();

	/**
	 * Signals the application to leave the current event loop as soon 
	 * as possible
	 */
	void LeaveLoop();

#if defined(SELFTEST)
	/**
	 * Assigns a listener. Only one listener is supported
	 *
	 * @param listener The button listener
	 */
	void SetX11NativeButtonListener(X11NativeButtonListener* listener) { m_listener = listener; }

	/**
	 * Sets identifier
	 *
	 * @param id The button identifier
	 */
	void SetId(int id) { m_id = id; }
#endif

private:
	X11NativeWindow* m_window_list[10];
	X11NativeWindow* m_focused_window;
	UINT32 m_num_window;
	UINT32 m_nest_count;
	UINT32 m_stop_count;
#if defined(SELFTEST)
	X11NativeButtonListener* m_listener;
	int m_id;
#endif
};




#endif

