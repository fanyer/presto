/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_WINDOW_H
#define QUICK_WINDOW_H


#include "adjunct/quick_toolkit/windows/DesktopWindowListener.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContent.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/pi/OpWindow.h"
#include "modules/util/OpTypedObject.h"

class OpScreenProperties;
class OpWidget;
class TypedObjectCollection;
class UiContext;

/** @brief A top-level window in the user interface
  */
class QuickWindow
  : public QuickWidgetContainer
  , private DesktopWindowListener
  , private MessageObject
{
public:
	/** Constructor
	  * @param style Style of window to create
	  * @param type Type of window to create
	  */
	QuickWindow(OpWindow::Style style = OpWindow::STYLE_DESKTOP,
			OpTypedObject::Type type = OpTypedObject::WINDOW_TYPE_TOPLEVEL);

	virtual ~QuickWindow();

	/** Initialize window, run before using other functions
	  */
	virtual OP_STATUS Init();

	/** Set the title of this window
	  * @param title Title of this window
	  */
	virtual OP_STATUS SetTitle(const OpStringC& title);
	const uni_char* GetTitle() { return m_title.CStr(); }

	void SetEffects(UINT32 effects) { m_effects = effects; }

	/** Set the contents of this window
	  * @param widget Contents for this window (will take ownership)
	  */
	void SetContent(QuickWidget* widget);

	/** Set the map that can be used to get widgets within this window by name.
	  * @param widgets the map (will take ownership)
	  */
	void SetWidgetCollection(TypedObjectCollection* widgets);

	/** @return a map that can be used to get widgets within this window by name
	  */
	const TypedObjectCollection* GetWidgetCollection() const { return m_widgets; }

	/** Make the window always adapt its size to content size or not (default
	  * is @c TRUE).
	  *
	  * Is set to @c FALSE, the window will calculate its size from content
	  * initially, and then it will keep it.
	  *
	  * @param resize whether to resize on content change
	  */
	void SetResizeOnContentChange(BOOL resize) { m_resize_on_content_change = resize; }

	/** Set a UI context for the window to operate in
	  * @param context Context for window
	  */
	void SetContext(UiContext* context) { m_context = context; }

	/** Show the window
	  */
	OP_STATUS Show();

	/** Get the desktop window associated with this window
	  */
	DesktopWindow* GetDesktopWindow() { return m_desktop_window; }

	/** Affects the return value of the OnDesktopWindowIsCloseAllowed method.
	  *
	  * Will disallow closing of the dialog using methods native to the
	  * operating system - for example clicking on the window's controls close
	  * button or using keyboard shortcut.
	  * Will not prevent close action triggered internally (using action
	  * handling for example).
	  *
	  * @param block Whether dialog closing should be blocked.
	  */
	void BlockClosing(bool block) { m_block_closing = block; }

	/** Set the name used to identify this window
	  * @param window_name Name for the window
	  */
	virtual OP_STATUS SetName(const OpStringC8 & window_name) { return m_window_name.Set(window_name); }

	/** @return Name used to identify this window
	  */
	const char* GetName() const { return m_window_name.CStr(); }

	// Overriding QuickWidgetContainer
	virtual void OnContentsChanged();

protected:
	/** @return Name of the default skin to use for this window */
	virtual const char* GetDefaultSkin() const { return "Window Skin"; }

	/** @return Whether skin in DefaultSkin should only be painted as background to other widgets in the window */
	virtual BOOL SkinIsBackground() const { return TRUE; }

	QuickWidget* GetContent() { return m_content.GetContent(); }
	void SetParentWindow(DesktopWindow* parent) { m_parent = parent; }
	void SetStyle(OpWindow::Style style) { m_style = style; }

	/** Hook called when showing the window.
	  * The DesktopWindow is available by that time.
	  *
	  * @return status.  An error code stops the window from showing.
	  */
	virtual OP_STATUS OnShowing() { return OpStatus::OK; }

	unsigned GetNominalWidth();
	unsigned GetNominalHeight(unsigned width);

	// Override MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// Override DesktopWindowListener
	virtual void OnDesktopWindowResized(DesktopWindow* desktop_window, INT32 width, INT32 height);
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);
	virtual BOOL OnDesktopWindowIsCloseAllowed(DesktopWindow* desktop_window) { return !m_block_closing; }

private:
	class DesktopWindowHelper;
	friend class DesktopWindowHelper;

	OP_STATUS InitDesktopWindow();

	/** Given some initial window size, update the minimum, maximum and current
	  * window sizes so that content fits best.
	  *
	  * @param width some initial window width that the content must fit into
	  * @param height some initial window height that the content must fit into
	  * @return @c true iff this function changes the current window size
	  */
	bool UpdateSizes(unsigned width, unsigned height);

	void UpdateDecorationSizes();
	void FitToScreen();
	void LayoutContent();

	/** size details (derived from contents) */
	inline OpWidget* GetRoot();
	inline unsigned GetHorizontalPadding();
	inline unsigned GetVerticalPadding();
	inline unsigned GetContentWidth(unsigned window_width);
	inline unsigned GetPreferredWidth();
	inline unsigned GetPreferredHeight(unsigned width);
	inline unsigned GetMaximumInnerWidth();
	inline unsigned GetMaximumInnerHeight(unsigned width);

	QuickWidgetContent m_content;
	TypedObjectCollection* m_widgets;
	DesktopWindow* m_desktop_window;

	DesktopWindow* m_parent;
	const OpTypedObject::Type m_type;
	OpWindow::Style m_style;
	UINT32 m_effects;
	OpString m_title;
	UiContext* m_context;
	BOOL m_scheduled_relayout;
	BOOL m_resize_on_content_change;
	OpString8 m_window_name;
	OpScreenProperties* m_screen_properties;
	unsigned m_decoration_width;
	unsigned m_decoration_height;
	bool m_block_closing;
};

#endif // QUICK_WINDOW_H
