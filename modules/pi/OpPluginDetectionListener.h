/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPPLUGINDETECTIONLISTENER_H
#define OPPLUGINDETECTIONLISTENER_H

/** @short Plug-in detection listener used to notify core that one plug-in has been detected.
 *
 * Methods in this class are called from an OpSystemInfo::DetectPlugins()
 * implementation. If a method runs out of memory, it returns
 * OpStatus::ERR_NO_MEMORY. If this happens, the implementation of
 * OpSystemInfo::DetectPlugins() should stop detecting plug-ins and propagate
 * the OpStatus::ERR_NO_MEMORY return value to its caller.
 */
class OpPluginDetectionListener
{
public:
	virtual ~OpPluginDetectionListener() {}

	/** Register a new plug-in in a list of potential new plug-ins.
	 *
	 * The plug-in will not be added to the list of known plug-ins before
	 * OnCommitPreparedPlugin() is is called. The newly prepared plug-in can be
	 * referenced by using the returned token.
	 *
	 * @param full_path Full path to the plug-in .so/.DLL file
	 * @param plugin_name The name the plug-in presents itself as
	 * @param plugin_description The description for the plug-in
	 * @param plugin_version The version for the plug-in
	 * @param component_type Component type to use to start this plugin
	 * @param enabled The initial state of the plug-in
	 * @param token Returns an unique identifier for this prepared plug-in
	 *
	 * @return OK if successfully registered, ERR if the plug-in path already
	 * exists for another plug-in already registered. See class documentation
	 * for OOM details.
	 */
	virtual OP_STATUS OnPrepareNewPlugin(const OpStringC& full_path, const OpStringC& plugin_name, const OpStringC& plugin_description, const OpStringC& plugin_version, OpComponentType component_type, BOOL enabled, void*& token) = 0;

	/** Add content-type and description to an already prepared plug-in.
	 *
	 * You can call this function multiple times to add multiple content-types
	 * to the plug-in.
	 *
	 * @param token The unique identifier returned when preparing this plug-in
	 * @param content_type A supported content-type. If already added, the rest
	 * of the parameters will be added to the existing entry
	 * @param content_type_description Description for content-type
	 *
	 * @return OK if successfully registered, ERR if the token or content-type
	 * is invalid or the plug-in is in the plug-in-ignore list. See class
	 * documentation for OOM details.
	 */
	virtual OP_STATUS OnAddContentType(void* token, const OpStringC& content_type, const OpStringC& content_type_description=UNI_L("")) = 0;

	/** Add extension and description of extension to an already prepared
	 * plug-in and content-type.
	 *
	 * You can call this function multiple times for the same plug-in and
	 * content-type, to add multiple extensions and -descriptions to it.
	 *
	 * @param token The unique identifier returned when preparing this plug-in
	 * @param content_type A supported content-type. If already added, the rest
	 * of the parameters will be added to the existing entry
	 * @param extensions A comma-separated list of extensions connected to the
	 * given content-type
	 * @param extension_description The description for the extensions (used in
	 * FileOpen/Save-dialogs)
	 *
	 * @return OK if successfully registered, ERR if the token is invalid. See
	 * class documentation for OOM details.
	 */
	virtual OP_STATUS OnAddExtensions(void* token, const OpStringC& content_type, const OpStringC& extensions=UNI_L(""), const OpStringC& extension_description=UNI_L("")) = 0;

	/** Commit a prepared plug-in.
	 *
	 * This will add the plug-in to the list of known plug-ins, connect the
	 * plug-in to viewers for the added content-types and extensions, and
	 * remove it from the list of prepared-only plug-ins. The token will no
	 * longer be valid after calling this function
	 *
	 * @param token The unique identifier returned when preparing this plug-in
	 *
	 * @return OK if successfully registered, ERR if the plug-in path already
	 * exists for another plug-in already registered ot the token is
	 * invalid. See class documentation for OOM details.
	 */
	virtual OP_STATUS OnCommitPreparedPlugin(void* token) = 0;

	/** Cancel a prepared plug-in.
	 *
	 * This will remove the plug-in from the list of prepared-only plug-ins,
	 * without adding it to the list of known plug-ins. The token will no
	 * longer be valid after calling this function
	 *
	 * @param token The unique identifier returned when preparing this plug-in
	 *
	 * @return OK if successfully registered. See class documentation for OOM
	 * details.
	 */
	virtual OP_STATUS OnCancelPreparedPlugin(void* token) = 0;
};

#endif // OPPLUGINDETECTIONLISTENER_H
