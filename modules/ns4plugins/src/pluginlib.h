/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * This file contains three implementations of PluginLib, only one of which is
 * compiled, depending on the state of mutually exclusive tweaks. They should
 * all use the same interface, but for historical reasons they do not. Fortunately,
 * all but the first are expected to be retired as part of CORE-40424.
 */
#ifndef _PLUGINLIB_H_INC_
#define _PLUGINLIB_H_INC_

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/pluginglobals.h"

#include "modules/util/simset.h"

#ifdef USE_OPDLL
#include "modules/pi/OpDLL.h"
#endif // USE_OPDLL

#ifdef NS4P_COMPONENT_PLUGINS
#include "modules/ns4plugins/src/generated/g_message_ns4plugins_messages.h"

/**
 * Plug-in library representation.
 *
 * Connects to and maintains a link to a remote PluginComponent.
 */
class PluginLib
	: public Link
	, public OpMessageListener
{
public:
	PluginLib();
	~PluginLib();

	/**
	 * Load library from OpDLL object.
	 *
	 * Not supported with NS4P_COMPONENT_PLUGINS, but kept for API compatibility.
	 *
	 * @returns OpStatus::ERR.
	 */
	OP_STATUS InitLibFromDll(OpDLL* dll, const uni_char* path, BOOL assume_ownership = TRUE);

	/**
	 * Load named library in a plug-in component.
	 *
	 * @param path Path to library.
	 * @param component_type The type of plug-in component to use for loading thelibrary.
	 *
	 * @return OpStatus::OK on success, and OpStatus::ERR_NO_MEMORY on OOM. Any other
	 *         error reflects an internal communication error and should be considered
	 *         as general failure.
	 */
	OP_STATUS LoadLib(const uni_char* path, OpComponentType component_type);

	/**
	 * Initialize the plug-in library through suitable NPAPI functions.
	 *
	 * @return NPERR_NO_ERROR on success, NPERR_GENERIC_ERROR on failure.
	 */
	NPError Init();

	/**
	 * Convenience methods for iterating over the list of plug-in libraries.
	 */
	PluginLib* Suc() { return static_cast<PluginLib*>(Link::Suc()); }
	PluginLib* Pred() { return static_cast<PluginLib*>(Link::Pred()); }

	/**
	 * Maintain reference counts for this library.
	 */
	void Inc() { m_reference_count++; }
	void Dec() { m_reference_count--; }
	BYTE Value() const { return m_reference_count; }

	/**
	 * Maintain access time for this library.
	 */
	time_t LastAccessed() const { return m_last_access; }
	void SetLastAccessed(time_t t) { m_last_access = t; }

	/**
	 * Return the channel to the plug-in component hosting the library.
	 * May be NULL.
	 */
	OpChannel* GetComponent() { return m_component; }

	/**
	 * Maintain saved data, as set by NPP_Destroy and received by NPP_New.
	 */
	NPSavedData* SavedData() { return m_saved_data; }
	void SetSavedData(NPSavedData* data) { m_saved_data = data; }

	/**
	 * Request that a URL be Unload()-ed when this library is destroyed.
	 *
	 * @param url URL to unload.
	 */
	OP_STATUS AddURLIntoCleanupTaskList(URL *url);

	/**
	 * Check if this library has been initialized and is ready for use.
	 */
	BOOL Initialized() { return m_initialized; }

	/**
	 * Check if this library has the given name.
	 *
	 * @param path Library path to compare this library's path against.
	 */
	BOOL isThisLibrary(const uni_char* path) { return m_path == path; }

	/**
	 * Check if this library has an active connection to a plug-in component.
	 * This does not imply that the library is initialized, but it does
	 * imply that it has been loaded.
	 */
	BOOL ok() { return !!m_component; }

	/**
	 * Retrieve the plug-in functions defined by this library.
	 *
	 * Only valid if the library has been initialized.
	 */
	NPPluginFuncs* PluginFuncs() { return &m_plugin_functions; }

	/**
	 * Retrieve an object class used for objects created by this library.
	 *
	 * This class is allowed to be constant since it is merely a proxy
	 * routing the calls to the right methods in the plug-in component.
	 */
	NPClass* GetPluginObjectClass() { return &m_plugin_object_class; }

	/**
	 * Message handlers. See component/messages.proto.
	 */
	OP_STATUS ProcessMessage(const OpTypedMessage* message);
	OP_STATUS OnGetStringIdentifiersMessage(OpPluginGetStringIdentifiersMessage* message);
	OP_STATUS OnGetIntIdentifierMessage(OpPluginGetIntIdentifierMessage* message);
	OP_STATUS OnLookupIdentifierMessage(OpPluginLookupIdentifierMessage* message);
	OP_STATUS OnPluginRetainObjectMessage(OpPluginRetainObjectMessage* message);
	OP_STATUS OnPluginReleaseObjectMessage(OpPluginReleaseObjectMessage* message);
	OP_STATUS OnPluginGetValueMessage(OpPluginGetValueMessage* message);
	OP_STATUS OnPluginUserAgentMessage(OpPluginUserAgentMessage* message);
	OP_STATUS OnPluginReloadMessage(OpPluginReloadMessage* message);
	OP_STATUS OnPluginExceptionMessage(OpPluginExceptionMessage* message);

	/**
	 * Clear site data for this plug-in library.
	 *
	 * @param site The site for which to clear data. If NULL, then all sites and more
	 * 		  generic data as well. String must be lowercase for ASCII domains,
	 * 		  and NKFC-encoded UTF8 for international domains. IP address
	 * 		  literals must be enclosed in square brackets '[]' (see RFC-3987).
	 * @param flags The types of data to clear.
	 * @param max_age How old the data should be (in seconds). If max_age is the maximum
	 * 		  unsigned 64-bit integer, all data is cleared.
	 *
	 * @return See NPAPI:ClearSiteData: https://wiki.mozilla.org/NPAPI:ClearSiteData#Clearing_Data
	 * @retval NPERR_NO_ERROR on success.
	 * @retval NPERR_TIME_RANGE_NOT_SUPPORTED on invalid time range.
	 * @retval NPERR_MALFORMED_SITE on malformed \p site string.
	 * @retval NPERR_GENERIC_ERROR for any other type of error.
	 */
	NPError ClearSiteData(const char *site, uint64_t flags, uint64_t max_age);

	/**
	 * Retrieve a list of sites with data.
	 *
	 * @return NULL-terminated array of site strings, or NULL on failure. See
	 * ClearSiteData() for format. Both site string and array must be manually
	 * freed with NPN_MemFree when no longer in use.
	 */
	char** GetSitesWithData();

protected:
	/** Load library named by m_path using component of type m_component_type. */
	OP_STATUS Load();

	/** Reference count of this library. I.e. count of Plugin objects associated with this library. */
	int m_reference_count;

	/** Time of last access. */
	time_t m_last_access;

	/** Plug-in function table. */
	NPPluginFuncs m_plugin_functions;

	/** Plug-in object function table. */
	NPClass m_plugin_object_class;

	/** Saved data. See NPAPI's NPP_New() and NPP_Destroy(). */
	NPSavedData* m_saved_data;

	/** Path to plug-in library represented by this object. May be empty unless library is initialized. */
	UniString m_path;

	/** Channel to plug-in component hosting plug-in library. May be NULL unless library is initialized. */
	OpChannel* m_component;

	/** Type of plug-in component used to host the plug-in library. */
	OpComponentType m_component_type;

	/** True if library has been initialized and is ready for use. */
	bool m_initialized;

	/** List of URL's whose Unload() method should be called when this library is destroyed. */
	OpAutoVector<URL_InUse> m_urls_to_unload;
};

#elif (defined(USE_OPDLL) || defined(STATIC_NS4PLUGINS))

#include "modules/ns4plugins/src/staticplugin.h"

class PluginLib : public Link
{
public:
	PluginLib();
	~PluginLib();

	OP_STATUS		InitLibFromDll(OpDLL* dll, const uni_char *lib_name, BOOL owns_plugin_dll = TRUE);
	OP_STATUS		LoadLib(const uni_char* plugin_dll);
	OP_STATUS		LoadStaticLib(const uni_char* plugin_dll,	NP_InitializeP init_function, NP_ShutdownP shutdown);

	PluginLib*		Suc() { return (PluginLib*) Link::Suc(); }
	PluginLib*		Pred() { return (PluginLib*) Link::Pred(); }

	void			Inc() { count++; }
	void			Dec() { count--; }
	BYTE			Value() const { return count; }

	time_t			LastAccessed() const { return last_accessed; }
    void			SetLastAccessed(time_t acc) { last_accessed = acc; }

	NPSavedData*	SavedData() { return saved; }

	/**
     * Checks if this object uses the library which is
     * specified by the lib parameter.
     * @param lib Library name to check.
     * @return TRUE if this object uses the library lib.
     */
	BOOL			isThisLibrary(const uni_char* lib);
	NPPluginFuncs*	PluginFuncs() { return &pluginfuncs; }
	BOOL			ok() const;


	/**
	 * Call plugin's dll_init function with parameters
	 */
	NPError			Init();

	/**
	 * Check if plugin library has been initialized
     * @return TRUE if plugin library was initialized.
	 */
	BOOL			Initialized() { return initialized; }

	/**
	 * Clear site data for this plugin.
	 * @param site
	 * 		The site for which to clear data. If NULL, then all sites and more
	 * 		generic data as well. String must be lowercase for ASCII domains,
	 * 		and NKFC-encoded UTF8 for international domains. IP address
	 * 		literals must be enclosed in square brackets '[]' (see RFC-3987).
	 * @param flags
	 * 		The types of data to clear.
	 * @param max_age
	 * 		How old the data should be (in seconds). If max_age is the maximum
	 * 		unsigned 64-bit integer, all data is cleared.
	 */
	NPError			ClearSiteData(const char *site, uint64_t flags, uint64_t max_age);

	/**
	 * Return a NULL-terminated array of sites with data. Both site string and
	 * array must be manually freed with NPN_MemFree when no longer in use.
	 */
	char**			GetSitesWithData();

#ifdef NS4P_OPDLL_SELF_INIT
	/**
	 * Return pointer to the plugin library's OpDLL
	 */
	OpDLL*				GetOpDLL() { return dll; }
#endif // NS4P_OPDLL_SELF_INIT

	OP_STATUS		AddURLIntoCleanupTaskList(URL *url);

private:
#ifdef DYNAMIC_NS4PLUGINS
	OpDLL*				dll;
	BOOL				owns_dll;
#endif //DYNAMIC_NS4PLUGINS
	BOOL                initialized;
	uni_char*			plugin;
	NPPluginFuncs		pluginfuncs;
	NPSavedData*		saved;
	BYTE				count;
	time_t				last_accessed;

	// Plugin DLL functions
	NP_InitializeP		dll_init;
	NP_ShutdownP		dll_shutdown;


#if defined(MSWIN) || defined(_WINDOWS) || defined(WIN_CE) || defined(WINGOGI) || defined(_MACINTOSH_)
	NP_GetEntryPointsP	dll_get_entry_points;
#endif // MSWIN

	OpAutoVector<URL_InUse> url_cleanup_tasklist;
};

#else // USE_OPDLL || STATIC_NS4PLUGINS

class PluginLib
  : public Link
{
private:

	uni_char*			plugin;

#if defined(MSWIN) || defined(WINGOGI)
	HINSTANCE			PluginDLL;
#elif defined(EPOC)
	RLibrary			PluginDLL;
#endif

	NPPluginFuncs		pluginfuncs;
	NPSavedData*		saved;
	BYTE				count;
	time_t				last_accessed;
	
#if defined(MSWIN) || defined(WINGOGI)
    NPError (WINAPI*	dll_init)(NPNetscapeFuncs* funcs);
    NPError (WINAPI*	get_entries)(NPPluginFuncs* funcs);
    int	    (WINAPI*	dll_shutdown)();
#elif defined(EPOC)
    NPError (*			dll_init)(NPNetscapeFuncs*, NPPluginFuncs*);
    void	(*			dll_shutdown)(void);
#endif


public:

						PluginLib();
    					~PluginLib();

	OP_STATUS			LoadLib(const uni_char* plugin_dll);

    PluginLib*			Suc() { return (PluginLib*) Link::Suc(); }
    PluginLib*			Pred() { return (PluginLib*) Link::Pred(); }

    void				Inc() { count++; }
    void				Dec() { count--; }

    BYTE				Value() const { return count; }

    time_t				LastAccessed() const { return last_accessed; }
    void				SetLastAccessed(time_t acc) { last_accessed = acc; }
    NPSavedData*		SavedData() { return saved; }
    BOOL				isThisLibrary(const uni_char* lib);
    NPPluginFuncs*		PluginFuncs() { return &pluginfuncs; }
#if defined(MSWIN) || defined(WINGOGI)
    BOOL				ok() const { return (BOOL) PluginDLL; }
#elif defined(EPOC)
    BOOL				ok() const { return (BOOL) PluginDLL.Handle(); }
#elif defined(_OPL_)
	BOOL				ok() const { return FALSE; /* OPL: IMPLEMENTME */ }
#else
	BOOL                ok() const { OP_ASSERT(FALSE); return FALSE; /* implement on your platform */ }
#endif
	/**
	 * Call plugin's dll_init function with parameters
	 */
	NPError			Init() { return NPERR_NO_ERROR; }

	/**
	 * Check if plugin library has been initialized
     * @return TRUE if plugin library was initialized.
	 */
	BOOL			Initialized() { return TRUE; }


};

#endif // USE_OPDLL

#endif // _PLUGIN_SUPPORT_
#endif // !_PLUGINLIB_H_INC_
