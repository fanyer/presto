/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _PLUGINLIBHANDLER_H_INC_
#define _PLUGINLIBHANDLER_H_INC_

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/plugincommon.h"

#include "modules/util/simset.h"

#ifdef NS4P_UNLOAD_PLUGIN_TIMER
#include "modules/hardcore/timer/optimer.h"
#endif // NS4P_UNLOAD_PLUGIN_TIMER

class PluginLib;
class OpDLL;

#ifdef  STATIC_NS4PLUGINS
#include "modules/ns4plugins/src/staticplugin.h"
#endif // STATIC_NS4PLUGINS

#ifdef NS4P_UNLOAD_PLUGIN_TIMER
class PluginLibHandler : private OpTimerListener
#else
class PluginLibHandler
#endif // NS4P_UNLOAD_PLUGIN_TIMER
{
private:

    Head				pluginlib_list;
#ifdef NS4P_NDATA_REALPLAYER_WORKAROUND
	char				ndata_dummy[90];	// ARRAY OK 2011-11-15 markuso
#endif // NS4P_NDATA_REALPLAYER_WORKAROUND

#ifdef NS4P_UNLOAD_PLUGIN_TIMER
	OpTimer*			m_unload_timer;		// Used to wake up and unload unused library, if any
	virtual void		OnTimeOut(OpTimer* timer);
#endif // NS4P_UNLOAD_PLUGIN_TIMER

public:
    					PluginLibHandler();
    					~PluginLibHandler();

	/**
	 * Initialize the library handler.
	 *
	 * @return OpStatus::OK on success.
	 */
	OP_STATUS           Construct();

	/**
	 * Look up the named plug-in library.
	 *
	 * @return A plug-in library if loaded, else NULL.
	 */
    PluginLib*			FindPluginLib(const uni_char *lib);

	/**
	 * Load a plug-in library from a DLL.
	 *
	 * Increments the existing library's reference count if it's already loaded.
	 *
	 * @param dll A handle to the DLL.
	 * @param lib_name The name of the library.
	 * @param owns_dll Let the created PluginLib assume ownership of the DLL handle.
	 *
	 * @return OpStatus::OK on success.
	 */
	OP_STATUS			CreateLib(OpDLL *dll, const uni_char *lib_name, BOOL owns_dll = TRUE);

	/**
	 * Load a plug-in library from a path.
	 *
	 * If the library is already loaded, a pointer to the existing library will be returned.
	 *
	 * @param lib Library path.
	 * @param component_type Component type to use to handle the library
	 * @param[out] plug_funcs Plug-in library entry points.
	 *
	 * @return OpStatus::OK on success.
	 */
	OP_STATUS			CreateLib(const uni_char *lib, OpComponentType component_type, NPPluginFuncs** plug_funcs);

#ifdef STATIC_NS4PLUGINS
	OP_STATUS			AddStaticLib(const uni_char *lib, NP_InitializeP init_function, NP_ShutdownP shutdown_function);
#endif // STATIC_NS4PLUGINS

	/**
	 * Unload the named plug-in library.
	 *
	 * Decrements its reference count, and, if the reference count hits zero and
	 * NS4P_TWEAK_UNLOAD_PLUGIN_TIMER is in use, destroys the library after the
	 * specified delay.
	 *
	 * To release the library with immediate effect, see DestroyLib().
	 *
	 * @param lib Library path.
	 */
	void				DeleteLib(const uni_char *lib);

	/**
	 * Destroy a named plug-in library.
	 *
	 * This call will instantly OP_DELETE() the PluginLib if found.
	 *
	 * @param lib Library path.
	 * @param fatal_error TRUE if the process hosting the library experienced a fatal error, such
	 *                    as a crash, otherwise FALSE. If TRUE, all Plugin objects associated with
	 *                    the named library will be informed of its destruction.
	 * @param address If fatal_error is set, the address of the process that experienced the fatal error
	 */
	void				DestroyLib(const uni_char *lib, bool fatal_error, const OpMessageAddress& address);

	/**
	 * Retrieve the saved data pointer for a named library.
	 *
	 * @param lib Library path.
	 *
	 * @return Saved data pointer. @See NPAPI.
	 */
    NPSavedData*		GetSavedDataPointer(const uni_char *lib);

	/**
	 * Clear site data for all plugins. Currently visits all plugins (and
	 * loads them, if necessary) and executes ClearSiteData. This function
	 * also makes sure to clean up after itself, ie. unload plugins not in use
	 * by the user at the time it is invoked.
	 * @return
	 * 	OpStatus::ERR when one or more plugins fail to clear site data.
	 * 	Some data may or may not have been cleared. Otherwise, OpStatus::OK.
	 */
	OP_STATUS			ClearAllSiteData();

#ifdef NS4P_NDATA_REALPLAYER_WORKAROUND
	char*				GetNdatadummy() { return ndata_dummy; }
#endif // NS4P_NDATA_REALPLAYER_WORKAROUND
};

#endif // _PLUGIN_SUPPORT_
#endif // !_PLUGINLIBHANDLER_H_INC_
