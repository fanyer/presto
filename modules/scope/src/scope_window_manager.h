/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Author: Johannes Hoff
** Documentation: ../documentation/window-manager.html
*/

#ifndef SCOPE_WINDOW_MANAGER_H
#define SCOPE_WINDOW_MANAGER_H

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT

#include "modules/scope/src/scope_service.h"
#include "modules/scope/scope_readystate_listener.h"
#include "modules/util/adt/opvector.h"
#include "modules/ecmascript_utils/esdebugger.h"
#include "modules/doc/frm_doc.h"
#include "modules/scope/src/generated/g_scope_window_manager_interface.h"

class OpScopeWindowManager
	: public OpScopeWindowManager_SI
#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	, public ES_DebugWindowAccepter
#endif // SCOPE_ECMASCRIPT_DEBUGGER
{
public:

	/** Window activations are recorded. This is the max number of items
	    in that history. */
	static const unsigned MAX_ACTIVATION_HISTORY = 4;

	OpScopeWindowManager();
	virtual ~OpScopeWindowManager();

	OP_STATUS Construct();

	/* Callbacks from core */
	void NewWindow(Window *win);
	void WindowRemoved(Window *win);
	void WindowTitleChanged(Window *win);
	void ActiveWindowChanged(Window *win);
	void ReadyStateChanged(FramesDocument *doc, OpScopeReadyStateListener::ReadyState state);

	/** Does a window_id pass the filter?
		If you have access to the Window object, AcceptWindow(Window* window) will be quicker.
	    @returns TRUE if the window_id is in the include filter and not in the exclude filter, FALSE otherwise **/
	BOOL AcceptWindow(unsigned window_id);

	/** Does a window type pass the filter?
	    @return TRUE if the window type is not excluded, FALSE if it is. **/
	BOOL IsAcceptedType(Window_Type type);

	/** Does a window pass the filter?
	    Virtual function from ES_DebugWindowAccepter
	    @returns TRUE if the window_id is in the include filter and not in the exclude filter, FALSE otherwise **/
	virtual BOOL AcceptWindow(Window* window);

private:
	/// Like AcceptWindow, accept it checks the opener of the window
	/// @param window_id The window to get the opener from
	BOOL AcceptOpener(Window* window);

	/**
	 * Find the Window with the specified ID.
	 *
	 * @param id The ID of the Window to find.
	 * @return The Window with the specified ID, or NULL if not found.
	 */
	Window *FindWindow(unsigned id) const;

	/**
	 * Like the function above, but fails with a suitable SetCommandError
	 * if the Window isn't found.
	 *
	 * @param id The ID of the Window to find.
	 * @param window The pointer to the Window is stored here (NULL on fail).
	 * @return OpStatus::ERR if the Window is not found. Can also OOM.
	 */
	OP_STATUS FindWindow(unsigned id, Window *&window);

	/**
	 * Convert from string to Window_Type.
	 *
	 * @param name [in] The string representation of the type, e.g. 'normal'.
	 * @param type [out] The Window_Type corresponding to 'name', or undefined on ERR.
	 * @return OpStatus::OK on success, OpStatus::ERR on invalid type, or OOM.
	 */
	OP_STATUS GetWindowType(const uni_char *name, Window_Type &type);

	/**
	 * Reset the window type filter to the default.
	 * @return OpStatus::OK or OOM.
	 */
	OP_STATUS SetDefaultTypeFilter();

	virtual OP_STATUS OnServiceEnabled();

	void ClearWindowIdFilters();

	OpINT32Vector included_windows;
	OpINT32Vector excluded_windows;
	BOOL include_all;
	OpVector<Window> window_activations;
	OpINT32Set excluded_types;


public:
	// Request/Response functions
	virtual OP_STATUS DoGetActiveWindow(WindowID &out);
	virtual OP_STATUS DoListWindows(WindowList &out);
	virtual OP_STATUS DoModifyFilter(const WindowFilter &in);
	virtual OP_STATUS DoCreateWindow(const CreateWindowArg &in, WindowID &out);
	virtual OP_STATUS DoCloseWindow(const CloseWindowArg &in);
	virtual OP_STATUS DoOpenURL(const OpenURLArg &in);
	virtual OP_STATUS DoModifyTypeFilter(const ModifyTypeFilterArg &in);

private:
	OP_STATUS SetWindowInfo(WindowInfo &info, Window *win);


};

#endif // SCOPE_WINDOW_MANAGER_SUPPORT

#endif // SCOPE_WINDOW_MANAGER_H
