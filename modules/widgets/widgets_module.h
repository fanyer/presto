/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_WIDGETS_WIDGETS_MODULE_H
#define MODULES_WIDGETS_WIDGETS_MODULE_H

#include "modules/hardcore/opera/module.h"
#include "modules/util/adt/opvector.h"

struct WIDGET_GLOBALS;
struct OP_TCINFO;
class OpWidgetPainterManager;
class WidgetInputMethodListener;
class OpWidgetExternalListener;


/**
   lock for deleting widgets - no widgets will be deleted while the lock lives
*/
class LockDeletedWidgetsCleanup
{
public:
	LockDeletedWidgetsCleanup();
	~LockDeletedWidgetsCleanup();
};

class OpDropDown;
class LockedDropDownCloser
{
private:
	OpVector<OpDropDown> locked_dropdowns;

public:
	~LockedDropDownCloser() { locked_dropdowns.Clear(); }
	OP_STATUS OnDropdownLocked(OpDropDown *drop_down)
	{
		return locked_dropdowns.Add(drop_down);
	}

	OP_STATUS OnDropdownUnLocked(OpDropDown *drop_down)
	{
		return locked_dropdowns.RemoveByItem(drop_down);
	}

	void CloseLockedDropDowns();

	BOOL Contains(OpDropDown *drop_down)
	{
		return locked_dropdowns.Find(drop_down) != -1;
	}
};

/**
 * Class for global data in the widgets module.
 */
class WidgetsModule : public OperaModule, public MessageObject
{
	friend class LockDeletedWidgetsCleanup;

public:
	WidgetsModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	/**
	   puts widget in deletion queue - requests callback if necessary
	 */
	void PostDeleteWidget(class OpWidget* widget);
	// MessageObject - for MSG_DELETE_WIDGETS
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** DEPRECATED */
	OpRect content_cliprect;

	OP_TCINFO* tcinfo;
	OpWidgetPainterManager* widgetpaintermanager;

#ifdef WIDGETS_IME_SUPPORT
	/** Global listener for IME events. */
	WidgetInputMethodListener* im_listener;

	/** TRUE only just when the IME is being created with the OpView::SetInputMethodMode call. */
	BOOL im_spawning;
#endif // WIDGETS_IME_SUPPORT

	/** More global data for widgets */
	WIDGET_GLOBALS* widget_globals;

	/** Add a listener for all widgets. */
	void AddExternalListener(OpWidgetExternalListener *listener);

	/** Remove a listener for all widgets. */
	void RemoveExternalListener(OpWidgetExternalListener *listener);

	Head external_listeners;

	LockedDropDownCloser locked_dd_closer;

private:
	// utility functions for handling MSG_DELETE_WIDGETS messages
	void PostDeleteWidgetsMessage();
	void ClearDeleteWidgetsMessage();
	UINT32 m_delete_lock_count; ///< number of consecutive locks created
	BOOL m_has_failed_to_post_delete_message; ///< TRUE if SetCallBack or PostMessage fails - new attempts will be made
	Head m_deleted_widgets; ///< a list of widgets yet to be deleted.
};

#define g_widget_globals g_opera->widgets_module.widget_globals

/** DEPRECATED */
#define g_op_content_cliprect (&(g_opera->widgets_module.content_cliprect))

#define g_widgetpaintermanager g_opera->widgets_module.widgetpaintermanager

#define g_locked_dd_closer g_opera->widgets_module.locked_dd_closer

#ifdef WIDGETS_IME_SUPPORT
#define g_im_listener g_opera->widgets_module.im_listener
#endif // WIDGETS_IME_SUPPORT

#define WIDGETS_MODULE_REQUIRED

#endif // !MODULES_WIDGETS_WIDGETS_MODULE_H
