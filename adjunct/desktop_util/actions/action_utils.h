/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
* 
* George Refseth, rfz@opera.com
*/

#ifndef ACTION_UTILS_H_
#define ACTION_UTILS_H_

#include "modules/dochand/winman_constants.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"

class OpWidget;
class OpDocumentContext;
class URLInformation;


/**
 * @brief Class used to break ResolveContextMenu up a little, and to be able to override the 
 * spellchecker parts in selftests. 
 */
class ContextMenuResolver
{
public:
	virtual ~ContextMenuResolver() {}
	void ResolveContextMenu(const OpPoint &point, DesktopMenuContext * menu_context, const uni_char*& menu_name, OpRect &rect, Window_Type window_type/*=WIN_TYPE_DOWNLOAD*/);
 
protected:
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	virtual BOOL HasSpellCheckWord(int spell_session_id);
	virtual BOOL CanDisableSpellcheck(int spell_session_id);
#endif // INTERNAL_SPELLCHECK_SUPPORT

private:
	void ResolveOpDocumentContextMenu(const OpPoint &point, OpDocumentContext * context, const uni_char*& menu_name, OpRect &rect, Window_Type window_type/*=WIN_TYPE_DOWNLOAD*/);
	void ResolveFileURLContextMenu(const OpPoint &point, const URLInformation * url_info, const uni_char*& menu_name, OpRect &rect, Window_Type window_type/*=WIN_TYPE_DOWNLOAD*/);
	void ResolveWidgetContextMenu(const OpPoint &point, const DesktopMenuContext::WidgetContext & widget_context, const uni_char*& menu_name, OpRect &rect, Window_Type window_type/*=WIN_TYPE_DOWNLOAD*/);
	
	BOOL IsEditableWidget(INT32 widget_type);
};


/**
 * Takes a DesktopMenuContext and tries to figure out which context menu (name) is the appropriate
 * one for this context. The retrieved menu name will be returned through menu_name. Also, in some 
 * cases, rect's topleft point will be set to point (not sure why). InvokeContextMenuAction is 
 * typically called right after calling this function.
 */
void ResolveContextMenu(	 const OpPoint &				point,
							 DesktopMenuContext*			context,
							 const uni_char*&				menu_name,
							 OpRect &						rect,
							 Window_Type					window_type = WIN_TYPE_DOWNLOAD);

#ifdef WIDGET_RUNTIME_SUPPORT
/**
 * Resolves the menu name according to the context.  Assumes the menu is
 * triggerred from a gadget.
 *
 * @param menu_context carries the information required to resolve the menu name
 * @param menu_name receives the resolved menu name
 */
void ResolveGadgetContextMenu(DesktopMenuContext& menu_context,
		const uni_char*& menu_name);
#endif // WIDGET_RUNTIME_SUPPORT


/**
 * Typically called after ResolveContextMenu. Will take a position/rect, context and menu name, and 
 * then trigger ACTION_SHOW_POPUP_MENU_WITH_MENU_CONTEXT.
 */
BOOL InvokeContextMenuAction(const OpPoint & 				point,
							 DesktopMenuContext*			context,
							 const uni_char*				menu_name,
							 OpRect &						rect);


/**
 * Creates a DesktopMenuContext holding an OpWidgetContext (also created in this function)
 * and tries to open a context menu with this newly created context.
 * @param	widget The widget that triggered the context menu request.
 * @param	pos		The (relative mouse) position from where the context menu request was triggered.
 */
BOOL HandleWidgetContextMenu(OpWidget* widget, const OpPoint & pos);


#endif /*ACTION_UTILS_H_*/
