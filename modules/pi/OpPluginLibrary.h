/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2011-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPPLUGINLIBRARY_H__
#define OPPLUGINLIBRARY_H__

#ifdef _PLUGIN_SUPPORT_

#include "modules/opdata/UniString.h"
#include "modules/ns4plugins/src/plug-inc/npfunctions.h"

/**
 * A platform interface for loading, probing and initializing libraries
 * conforming to the Netscape Plug-in API (NPAPI).
 *
 * If it manages to successfully load a plug-in library, the object should keep
 * the library loaded as long as it exists.  On destruction, the object should
 * unload the library and release its resources.
 *
 * In a particular process, at most one instance of OpPluginLibrary will
 * exist at any time.
 */
class OpPluginLibrary
{
public:
	typedef NPNetscapeFuncs* BrowserFunctions;
	typedef NPPluginFuncs* PluginFunctions;

	/**
	 * Unload the library and release any resources.
	 */
	virtual ~OpPluginLibrary() {}

	struct LibraryPath
	{
		UniString path;       /**< Path to a plugin library */
		OpComponentType type; /**< Component type to start for running this
		                           plugin. Can be used by the platform to specify
		                           e.g. different architectures */
	};

	/**
	 * Enumerate the plug-in libraries available on the system. Filtering of
	 * undesired libraries (e.g. applying security blacklists and
	 * plugin_ignore.ini) may be done by this method or deferred until probing,
	 * leaving them for OpPluginLibrary::Create() to reject by returning
	 * OpStatus::ERR.
	 *
	 * @param[out] library_paths  A list of library identifiers returned
	 * to the caller. The identifiers must be valid arguments to Create().
	 *
	 * @param suggested_paths A list of directories in which plugin libraries
	 * may be found. The string is the return value of OpSystemInfo::GetPluginPathL()
	 * invoked with the preference [User Prefs] "Plugin Path" as argument. The
	 * preference setters are responsible for using a platform-appropriate separator
	 * (e.g. colon on Unix-like platforms, semicolon on MS-Windows) between
	 * directories in the list. This list is intended to augment platform
	 * knowledge, not replace it.
	 *
	 * @return See OpStatus; OK on success.
	 */
	static OP_STATUS EnumerateLibraries(OtlList<LibraryPath>* library_paths,
	                                    const UniString& suggested_paths);

	/**
	 * Create a new OpPluginLibrary.
	 *
	 * The identifying argument is the path member of an OpPluginLibrary returned
	 * by EnumerateLibraries(). This method will be called by a component of the
	 * type specified by the corresponding type parameter.
	 *
	 * Before calling this method to instantiate a new OpPluginLibrary object, the
	 * owning plug-in component will destroy any previously instantiated library,
	 * such that the number of concurrent library instances in a process is less
	 * than or equal to the number of plug-in components the platform is running
	 * in said process.
	 *
	 * @param[out] library An OpPluginLibrary object on success, otherwise undefined.
	 * @param[in] path The name of the library file to open, including path to the file.
	 *
	 * @return See OpStatus.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR if the library is unsuitable, such as when it is blacklisted
	 *         for compatibility or security reasons, or simply if it turns out not to be
	 *         a valid NPAPI plug-in library.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS Create(OpPluginLibrary** library, const UniString& path);

	/**
	 * Retrieve the plug-in library name.
	 *
	 * @param[out] name  The name the plug-in represents itself as, or more
	 *                   specifically, the string retrieved by
	 *                   NPPVpluginNameString (e.g. "Shockwave Flash").
	 *
	 * @return See OpStatus; OK on success.
	 */
	virtual OP_STATUS GetName(UniString* name) = 0;

	/**
	 * Retrieve the plug-in library description as given by the plug-in library.
	 *
	 * @param[out] description  The plug-in library description string,
	 *                          retrieved by NPPVpluginDescriptionString.
	 *
	 * @return See OpStatus; OK on success.
	 */
	virtual OP_STATUS GetDescription(UniString* description) = 0;

	/**
	 * Retrieve the plug-in library version.
	 *
	 * @param[out] version  The plug-in library version string.
	 *
	 * @return See OpStatus; OK on success.
	 */
	virtual OP_STATUS GetVersion(UniString* version) = 0;

	/**
	 * Check if the plug-in library is enabled.
	 *
	 * Browser users may go to about:plugins to see a detailed list of plug-in libraries
	 * discovered. This page also allows libraries to be enabled or disabled, the latter
	 * state preventing the browser from loading and using the library. If a specific
	 * library should be listed but not be used unless explicitly enabled by the user,
	 * an implementation may return false here.
	 *
	 * @return true if enabled, false otherwise.
	 */
	virtual bool GetEnabled() { return true; }

	/**
	 * Return the list of content types handled by this plug-in library.
	 *
	 * The list was previously populated by a call to NP_GetMIMEDescription
	 * during initialisation and contains all the MIME types the plug-in claims
	 * to support.
	 *
	 * @param[out] content_types  The list of content types supported by the plug-in library.
	 *
	 * @return See OpStatus.
	 */
	virtual OP_STATUS GetContentTypes(OtlList<UniString>* content_types) = 0;

	/**
	 * Retrieve the file extensions associated with a library-defined content type.
	 *
	 * @param[out] extensions    The list of extensions associated with this content type.
	 * @param      content_type  The content type to retrieve file extensions for.
	 *
	 * @return See OpStatus; OK on success.
	 */
	virtual OP_STATUS GetExtensions(OtlList<UniString>* extensions, const UniString& content_type) = 0;

	/**
	 * Retrieve a description of a content type.
	 *
	 * @param[out] description   A content type description string.
	 * @param      content_type  The content type to describe.
	 *
	 * @return See OpStatus; OK on success.
	 */
	virtual OP_STATUS GetContentTypeDescription(UniString* description, const UniString& content_type) = 0;

	/**
	 * Load the library, if it's not already loaded.
	 *
	 * Platforms are encouraged to use the RAII idiom and perform load/unload
	 * of the library in the initialiser/destructor. Whenever Load() is called,
	 * simply return OpStatus::OK to indicate that the library is loaded.
	 *
	 * @return See OpStatus; OK if the library is loaded on return from this method.
	 */
	virtual OP_STATUS Load() = 0;

	/**
	 * Call the NPAPI initializer function or functions, and retrieve the
	 * plug-in library's entry points. The exact approach varies between platforms.
	 *
	 * Load() will be called before this method.
	 *
	 * @param plugin_funcs The table of plug-in library entry points to be filled by
	 *        the plug-in.
	 * @param browser_funcs The browser entry points to be given to the plug-in library.
	 *
	 * @return The return value is the NPAPI return value of the first NPAPI function
	 *         that failed, or the NPError translation of the first significant error,
	 *         or NPERR_NO_ERROR on success. Before returning an error, all resources
	 *         allocated by this method must be released. Shutdown() will be called
	 *         precisely if Initialize() succeeds.
	 */
	virtual NPError Initialize(PluginFunctions plugin_funcs, const BrowserFunctions browser_funcs) = 0;

	/**
	 * Call the NPAPI shutdown function (NP_Shutdown).
	 */
	virtual void Shutdown() = 0;
};

#endif // defined(_PLUGIN_SUPPORT_)

#endif // OPPLUGINLIBRARY_H__
