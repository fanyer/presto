/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _PLUGINS_
#define _PLUGINS_

#if defined(_PLUGIN_SUPPORT_)

/**
 * Design decisions and short explanation of the plugin API:
 *
 * When redesigning the old API, the following problems in the old API were identified:
 * 1 - Slow startup (plugin detection on some systems opens each plugin to query for information, which adds up to a lot of time)
 * 2 - Memory footprint (plugin information was duplicated into each and every connected Viewer)
 * 3 - No runtime enable/disable of individual plugins
 * 4 - Platform code in core module
 * 5 - UI code in core module
 *
 * The problems were solved by:
 * 1 - Delaying plugin detection by posting a request to the message loop. Unless something really need the plugins detected during
 *     module initializations, detection will thus not happen until after initialization. The total time spent will be the same, but
 *     Opera will appear sooner on screen, and by this hopefully feel faster to start up.
 * 2 - The list of supported content-types and extensions is kept for the plugin, and only pointers to the plugin object(s) are stored in Viewer
 *     (to have a quick lookup of which plugins support the content-type or extension of a given Viewer)
 * 3 - Plugins now have functions to enable/disable them at runtime, and Viewer will, unless told otherwise, use the first *enabled* plugin in its list
 * 4 - Platform code (for detecting plugins) is now moved to porting interface, and implemented by each platform
 * 5 - A lot of UI code is simply removed (watch out for regressions!). In particular, the plugin classes are no longer possible to embed directly into
 *     OpTreeViews as they don't implement OpTreeModel anymore
 * 6 - Extensions are kept in one string, but also in a parsed list of strings. It is now easy to add or remove support for a single extension run-time
 *
 *
 * Other relevant changes: Most functions are changed to return OP_STATUS, not to Leave, take OpString parameters, and have meaningful and
 * self-explaining names. Plugin code have also been split in two, one class for the Plugin itself (called PluginViewer, as there already exists a
 * class Plugin in another module), and one class for managing Plugin objects, which will be available as one and only one instance, 'g_plugin_viewers'.
 * Adding, deleting, querying and other plugin managment tasks is to be performed on this instance.
 */

#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/pi/OpPluginDetectionListener.h"

class JS_Plugin;
class ES_Runtime;

struct PluginExtensionDetails
{
	OpString m_extension;
	OpString m_extension_description;
};

class PluginViewer;
class PluginContentTypeDetails
{
friend class PluginViewer;
friend class PluginViewers;
public:
	PluginContentTypeDetails() : m_plugin(NULL) {} //Don't use this directly, use OpPluginDetectionListener or PluginViewer API!
	~PluginContentTypeDetails() {}

private:
	/** Set the content-type
      *
      * @param content_type Content-type this object contains details about
      * @param description Description for the content type
      * @return OpStatus::OK, ERR or ERR_NO_MEMORY.
      */
	OP_STATUS SetContentType(const OpStringC& content_type, const OpStringC& description=UNI_L(""));

	OP_STATUS GetContentType(OpString& content_type, OpString& description);

	int CompareContentType(const OpStringC& content_type) const {return m_content_type.CompareI(content_type);}

	/** Add an extension to the list of supported extensions
      *
      * @param extension Extension to add to the list of supported extensions
      * @param extension_description Description for the extension (most commonly used in the File|Open drop-down list of extensions)
      * @return OpStatus::OK, ERR or ERR_NO_MEMORY.
      */
	OP_STATUS AddExtension(const OpStringC& extension, const OpStringC& extension_description);

	/** Add a list (separated by separator_chars) of extensions to the list of supported extensions
      *
      * @param extensions Extensions to add to the list of supported extensions
      * @param extensions_separator_chars Character used to separate extensions in the extensions parameter
      * @param extensions_descriptions Description of the extensions. Should be in the same order and the same count as extensions
      * @param extensions_descriptions_separator_chars Character used to separate extensions descriptions in the extensions descriptions parameter
      * @return OpStatus::OK, ERR or ERR_NO_MEMORY.
      */
	OP_STATUS AddExtensions(const OpStringC& extensions, const OpStringC& extensions_separator_chars,
	                        const OpStringC& extensions_descriptions, const OpStringC& extensions_descriptions_separator_chars);

	/** Remove an extension from the list of supported extensions.
      *
      * @param extension Extension to remove from the list of supported extensions
      * @return OpStatus::OK
      */
	OP_STATUS RemoveExtension(const OpStringC& extension);

	/** Check if extension is present in m_supported_extension_list
      *
      * @param extension Extension to check if is in list
      * @return TRUE if extension is in list, FALSE if not
      */
	BOOL SupportsExtension(const OpStringC& extension) const;

	OP_STATUS ConnectToViewers();
	OP_STATUS DisconnectFromViewers();

	PluginExtensionDetails* GetExtensionDetails(const OpStringC& extension) const;
	PluginExtensionDetails* GetFirstExtensionDetails() const { return m_supported_extension_list.Get(0); };

private:
	PluginViewer* m_plugin;
	OpString m_content_type;
	OpString m_description;
	OpAutoVector<PluginExtensionDetails> m_supported_extension_list;
};

class PluginViewer
{ /** This class is a wrapper for a browser plugin, but there already exists a class 'Plugin', so PluginViewer it is.. */
friend class PluginViewers;
friend class PluginContentTypeDetails;
public:

	PluginViewer();
	virtual ~PluginViewer();

	/** Initializes a PluginViewer object. Most likely called from platform code, where plugin path is known. Note that path is considered case sensitive in all the PluginViewer code, if not API_PI_OPSYSTEMINFO_PATHSEQUAL is imported!
      *
      * @param path Full path to the plugin (that is, including the filename of the .DLL/.so)
      * @param product_name Whatever the plugin exports as its product name
      * @param description Whatever the plugin exports as its description
      * @param version Whatever the plugin exports as its version
      * @param component_type Component type to use to start this plugin
      * @return OpStatus::OK or ERR_NO_MEMORY.
      */
	OP_STATUS Construct(const OpStringC& path, const OpStringC& product_name, const OpStringC& description, const OpStringC& version, OpComponentType component_type);

	OP_STATUS AddContentType(PluginContentTypeDetails* content_type);

	/** Remove a content-type (and its related extensions) from the list of this plugins supported content-types.
      *
      * @param content_type Content-type to remove from the list of supported content-types
      * @return OpStatus::OK
      */
	OP_STATUS RemoveContentType(const OpStringC& content_type);

	/** Check if plugin supports a given content-type.
      *
      * @param content_type Content-type to check if is supported by plugin
      * @return TRUE if content-type is supported by plugin, FALSE if not
      */
	BOOL SupportsContentType(const OpStringC& content_type) const;

	PluginContentTypeDetails* GetContentTypeDetails(const OpStringC& content_type) const;

	/** Check if plugin supports a given extension.
      *
      * @param extension Extension to check if is supported by plugin
      * @return TRUE if extension is supported by plugin, FALSE if not
      */
	BOOL SupportsExtension(const OpStringC& extension) const;

	/** Get full path to plugin (.DLL/.so)
      *
      * @param path Used to return full path to plugin
      * @return OpStatus::OK or ERR_NO_MEMORY
      */
	OP_STATUS GetPath(OpString& path) const {return path.Set(m_path);}

	/** Get full path to plugin (.DLL/.so)
      *
      * @return Full path to plugin
      */
	const uni_char* GetPath() const {return m_path.CStr();}

	/** Get plugin product name
      *
      * @param product_name Used to return plugin product name
      * @return OpStatus::OK or ERR_NO_MEMORY
      */
	OP_STATUS GetProductName(OpString& product_name) const {return product_name.Set(m_product_name);}

	/** Get plugin product name
      *
      * @return Plugin product name
      */
	const uni_char* GetProductName() const {return m_product_name.CStr();}

	/** Get plugin description
      *
      * @param description Used to return plugin description
      * @return OpStatus::OK or ERR_NO_MEMORY
      */
	OP_STATUS GetDescription(OpString& description) const {return description.Set(m_description);}

	/** Get plugin description
      *
      * @return Plugin description
      */
	const uni_char* GetDescription() const {return m_description.CStr();}


	/** Get plugin version
      *
      * @return Plugin version
      */
	const uni_char* GetVersion() const {return m_version.CStr();}

	/** Get description of MIME type from plugin
	  *
	  * @param[in]  content_type MIME type to look up
	  * @param[out] description  Used to return type description
	  * @return OpStatus::OK or ERR_NO_MEMORY
	  */
	OP_STATUS GetTypeDescription(const OpStringC& content_type, OpString& description) const;

	/** Get the component type used to start this plugin
	  *
	  * @return Component type
	  */
	OpComponentType GetComponentType() const { return m_component_type; }

	// To set default plugin, call Viewer::SetDefaultPluginViewer(PluginViewer* plugin)

	void EnablePluginViewer() {m_enabled = TRUE;}
	void DisablePluginViewer() {m_enabled = FALSE;}
	void SetEnabledPluginViewer(BOOL enabled) {m_enabled = enabled;}

	/** Check if plugin is enabled
      *
      * @return TRUE if plugin is enabled, FALSE if it is disabled
      */
	BOOL IsEnabled() const {return m_enabled;}

private:
	OP_STATUS ConnectToViewers();
	OP_STATUS DisconnectFromViewers();

private:
	OpString m_path;
	OpString m_product_name;
	OpString m_description;
	OpString m_version;
	OpComponentType m_component_type;
	BOOL	m_enabled;
	BOOL	m_in_plugin_viewer_list;
	OpAutoVector<PluginContentTypeDetails> m_supported_content_type_list;
};

class PluginDetector;

class PluginViewers
	: public OpPluginDetectionListener
#ifndef NS4P_COMPONENT_PLUGINS
	, private MessageObject
#endif // NS4P_COMPONENT_PLUGINS
{
public:
#ifdef NS4P_COMPONENT_PLUGINS
	class OpDetectionDoneListener
	{
	public:
		/** Fires when asynchronous plug-in detection is completed. */
		virtual void DetectionDone() = 0;
	};
#endif // NS4P_COMPONENT_PLUGINS

	PluginViewers();
	virtual ~PluginViewers();

	/** Initializes the one and only PluginViewers object (which will be available as g_plugin_viewers).
      *
      * @return OpStatus::OK. May Leave
      */
	OP_STATUS ConstructL();

	/** Detect plugins. The detection itself is done in platform code, and platforms are expected to create PluginViewer* objects,
	  * call PluginViewer::Construct(...), add supported content-types and extensions, and finally call PluginViewers::AddPluginViewer(...)
	  * to hand the detected plugin over to the plugin viewer manager
      *
      * @return OpStatus::OK or ERR_NO_MEMORY
      */
	OP_STATUS DetectPluginViewers(const OpStringC& plugin_path=UNI_L(""), bool async = false);

/** Start of implementing OpPluginDetectionListener (modules/pi/OpPluginDetectionListener.h) */
	OP_STATUS OnPrepareNewPlugin(const OpStringC& full_path, const OpStringC& plugin_name, const OpStringC& plugin_description, const OpStringC& plugin_version, OpComponentType component_type, BOOL enabled, void*& token);

	OP_STATUS OnAddContentType(void* token, const OpStringC& content_type, const OpStringC& content_type_description=UNI_L(""));

	OP_STATUS OnAddExtensions(void* token, const OpStringC& content_type, const OpStringC& extensions=UNI_L(""), const OpStringC& extension_description=UNI_L(""));

	OP_STATUS OnCommitPreparedPlugin(void* token);

	OP_STATUS OnCancelPreparedPlugin(void* token);
/** End of implementing OpPluginDetectionListener */

	/** Returns 'true' if asynchronous plug-in detection is currently in progress. */
	bool IsDetecting() const;

#ifdef NS4P_COMPONENT_PLUGINS
	/** Add/Remove listener which will receive a callback when asynchronous plug-in detection is done. */
	OP_STATUS AddDetectionDoneListener(OpDetectionDoneListener* listener);
	OP_STATUS RemoveDetectionDoneListener(OpDetectionDoneListener* listener);
#endif // NS4P_COMPONENT_PLUGINS


	/** Removes a plugin object from the list, and deletes the plugin object itself.
      *
      * @param plugin Plugin to delete
      * @return OpStatus::OK or ERR_NULL_POINTER
      */
	OP_STATUS DeletePluginViewer(PluginViewer* plugin);

	/** Find a plugin, given plugin path. Note that paths are compared case sensitive if not API_PI_OPSYSTEMINFO_PATHSEQUAL is imported!
      *
      * @param path Path to search for
      * @return Plugin with matching path, or NULL
      */
	PluginViewer* FindPluginViewerByPath(const OpStringC& path);

	PluginViewer* GetPluginViewer(int pos) {return m_plugin_list.Get(pos);}

	/** Returns numbers of found plugins.
	  *
	  * @param enabled_only If TRUE, returns count of enabled plugins only
	  * @return Number of enabled or all plugins
	  */
	UINT GetPluginViewerCount(BOOL enabled_only = FALSE) const;

	/** Refreshes the list of registered plugins (refresh, as in clear the list of registered plugins, and have platform detect them
	  * one more time)
	  *
	  * @param delayed If FALSE, plugins will be refreshed immediately. If TRUE, they will be refreshed after one trip through the message loop
	  * @param path Used to override the default plugin path
	  * @return OpStatus::OK. May Leave
	  */
	OP_STATUS RefreshPluginViewers(BOOL delayed, const OpStringC& path=UNI_L(""));

	/** Reads pref that contains paths that user manually disabled and stores them in a vector. Plugins found in list
	  * will be matched to existing plugins and existing plugins will get disabled while reading pref.
	  *
	  * @return OpStatus::OK or ERR_NO_MEMORY
	  */
	OP_STATUS ReadDisabledPluginsPref();

	/** Concatenates paths of plugins disabled in current session and ones that were disabled in pref, eliminating duplicates
	  * and making sure that paths which are not currently accessible are kept as disabled.
	  *
	  * @return OpStatus::OK. May Leave
	  */
	OP_STATUS SaveDisabledPluginsPref();

	/** Clear the list of registered plugins, without re-detecting them. (To re-detect, you will have to call RefreshPluginViewers yourself)
      */
	void DeleteAllPlugins();

	/** Plugin viewers are not detected at Opera startup, but immediately after startup. If something wants plugins during startup,
	  * this function should be called. Normally, plugins will be requested through calls using g_viewers or g_plugin_viewers, which
	  * will automatically call this function when needed
	  *
      * @param path Used to override the default plugin path
      * @return OpStatus::OK
	  */
	OP_STATUS MakeSurePluginsAreDetected(const OpStringC& path=UNI_L(""));

private:
	OP_STATUS AddPluginViewer(PluginViewer* plugin);

	void OnPluginViewerAdded() {g_main_message_handler->PostMessage(MSG_VIEWERS_PLUGIN_ADDED, 0, 0);}
	void OnPluginViewerChanged() {g_main_message_handler->PostMessage(MSG_VIEWERS_PLUGIN_CHANGED, 0, 0);} /** Core will not notify about plugin changes. This function is here to make platform code notify other platform code about changes */
	void OnPluginViewerDeleted() {g_main_message_handler->PostMessage(MSG_VIEWERS_PLUGIN_DELETED, 0, 0);}

#ifndef NS4P_COMPONENT_PLUGINS
	// Inherited interfaces from MessageObject:
	virtual void HandleCallback(OpMessage, MH_PARAM_1, MH_PARAM_2);
#endif // NS4P_COMPONENT_PLUGINS

private:
	OpAutoVector<PluginViewer> m_plugin_list;
	OpAutoVector<PluginViewer> m_prepared_plugin_list;
	OpVector<uni_char> m_manually_disabled_path_list;
	BOOL m_has_detected_plugins;
#ifdef NS4P_COMPONENT_PLUGINS
	PluginDetector *m_detector;
#else // !NS4P_COMPONENT_PLUGINS
	BOOL m_has_set_callback;
#endif // !NS4P_COMPONENT_PLUGINS
};

#endif // _PLUGIN_SUPPORT_

#endif // _PLUGINS_
