/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _VIEWERS_
#define _VIEWERS_

/**
 * Design decisions and short explanation of the viewer API:
 *
 * When redesigning the old API, the following problems in the old API were identified:
 * 1 - Inefficient querying (Viewers were stored in OpVector, and queried by string comparing each and every content-type for a match (O(n)))
 * 2 - Lots of converting from uni_char to char (or char to uni_char)
 * 3 - Extensive use of 'id' as a unique thing, which, in case of adding or deleting viewers run-time, would not be unique at all.. (id was position in array)
 * 4 - Extensive parsing (extensions were stored in one string, which was parsed every time it was queried)
 * 5 - Platform code in core module
 * 6 - UI code in core module
 * 7 - Only support for one plugin per viewer
 *
 * The problems were solved by:
 * 1 - Viewers are now stored in hashed tables (two tables, one hashed using 16-bit content type string, and one hashed using 8-bit content-type string).
 *     Querying by content-type is now reduced to O(1). The Viewers class now contains functions for querying by extension or url, but these are not
 *     hashed, and thus not as efficient.
 * 2 - Most converting was caused by mixing 16- and 8-bit content types. Viewer now contains both 16- and 8-bit version of content-type, and hashed list
 *     for both of them. For a small increase in footprint, a lot of converting is now removed.
 * 3 - id is now completely removed. Find viewer by content-type, extension or url, or use CreateIterator
 *     and GetNextViewer to iterate linear without the need for id
 * 4 - Extensions are kept in one string, but also in a parsed list of strings. It is now easy to add or remove support for a single extension run-time
 * 5 - The Quick-based BroadcastItemChanged etc, are now replaced by cross-platform functions OnViewerChanged etc
 * 6 - A lot of UI code is simply removed (watch out for regressions!). In particular, the viewer classes are no longer possible to embed directly into
 *     OpTreeViews as they don'y implement OpTreeModel anymore
 * 7 - Viewer now has a list of pointers to supported plugins. These lists are updated automatically, as long as the Viewer object is added to the
 *     one and only Viewer manager 'g_viewers'. The first enabled plugin with the lowest index is considered the default plugin (the order for other
 *     plugins is not stored, yet)
 *
 *
 * Other relevant changes: Most functions are changed to return OP_STATUS, not to Leave, take OpString parameters, and have meaningful and
 * self-explaining names. Viewer code is still split in two, one class for the Viewer itself, and one class for managing Viewer objects, which will be
 * available as one and only one instance, 'g_viewers'. Adding, deleting, querying and other viewer managment tasks is to be performed on this instance.
 */

#ifdef _PLUGIN_SUPPORT_
# include "modules/viewers/plugins.h"
#endif

#include "modules/hardcore/mh/mh.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/adt/opvector.h"

#include "modules/viewers/src/generated_viewers_enum.h"
#define defaultOperaViewerTypes_SIZE VIEWER_LAST

/** Action associated with a viewer type. Must all be positive and fit
  * in eight bits (checked by selftest). */
typedef enum {
	VIEWER_ASK_USER							= 0,
	VIEWER_SAVE								= 1,
	VIEWER_OPERA							= 2,
	VIEWER_APPLICATION						= 3,
	VIEWER_REG_APPLICATION					= 4,
	VIEWER_RUN								= 5,
	VIEWER_PLUGIN							= 6,
	VIEWER_PASS_URL							= 7,
	VIEWER_IGNORE_URL						= 8,
	VIEWER_SAVE_AND_NOTIFY					= 10,
	VIEWER_SAVE_AND_SET_READONLY			= 11,
	VIEWER_SAVE_AND_NOTIFY_AND_SET_READONLY = 12,
	VIEWER_REDIRECT                         = 13, // This is now obsolete. Please use VIEWER_WEB_APPLICATION instead
	VIEWER_WEB_APPLICATION					= 14,
	// These should be last:
	VIEWER_WAIT_FOR_DATA					= 98,
	VIEWER_NOT_DEFINED						= 99
} ViewAction;

class ViewActionFlag
{
	public:
		enum ViewActionFlagType {
			NO_FLAGS			= 0x0,
			OPEN_WHEN_COMPLETE	= 0x1,
			SAVE_DIRECTLY		= 0x2, // SAVE_DIRECTLY and SAVE_AS are mutually exclusive
			SAVE_AS				= 0x4,
			USERDEFINED_VALUE	= 0x80 // To separate between a default value (set by Opera) and the same value set by user.
		};
	private:
		ViewActionFlagType	flags;
	public:
							ViewActionFlag() : flags(NO_FLAGS) {}
		void				SetFlags(short f) { flags = static_cast<ViewActionFlagType>(f); }
		void				Set(ViewActionFlagType f) { flags = static_cast<ViewActionFlagType>(flags | f); }
		void				Unset(ViewActionFlagType f) { flags = static_cast<ViewActionFlagType>(flags & ~f); }
		void				Reset() { flags = NO_FLAGS; }
		ViewActionFlagType	Get(ViewActionFlagType f = NO_FLAGS) const { return f == NO_FLAGS ? flags : static_cast<ViewActionFlagType>(f & flags); }
		BOOL				IsSet(ViewActionFlagType f) const { return f == static_cast<ViewActionFlagType>(f & flags); }
		unsigned short		operator&(short a) { return flags & a; }
};

struct ViewerTypes
{
	const char		*type;
	const char 		*ext;
	ViewAction		action: 8;
	unsigned int	ctype: 8;
	ViewersEnum		container: 15;			/**< Type can be inside this container. */
	bool			web_handler_allowed: 1;	/**< Type can be overridden by web handler. */
	bool			allow_any_extension: 1;	/**< Type bypasses strict file extension check. */
};

class Viewer
{
friend class Viewers;
#ifdef _PLUGIN_SUPPORT_
friend class PluginViewer;
#endif // _PLUGIN_SUPPORT_
public:

	Viewer();
	virtual ~Viewer();

	/** Initializes a Viewer object. This object maps a content-type and(/or) file extension to an action and, if applicable,
	  * external application or plugin.
	  *
	  * @param action Action for this Viewer
	  * @param content_type Content-type from the URLContentType enum. At startup, this can be set to URL_UNDETERMINED_CONTENT, and
	  * will be changed to the content-type mathing content_type_string in Viewers::ImportGeneratedViewersArray
	  * @param content_type_string Content-type as a string (in case content-type isn't in the enum, or is currently unknown)
	  * @param extensions_string Comma-separated list of extensions
	  * @param flags Flags to tweak Viewer behaviour. See each and every flag for more info
	  * @param application_to_open_with Full path to external application, if action is set to use external application
	  * @param save_to_folder Full path to folder, if action is set to save to folder
	  * @param web_handler_to_open_with Url to web handler that will dealt with
	  * @param preferred_plugin_path Full path to preferred plugin. Note that the plugin might not be available yet (but will be
	  * matched to Viewer automatically when it is detected by the platform), and note that the paths are compared case sensitive
	  * if not API_PI_OPSYSTEMINFO_PATHSEQUAL is imported
	  * @param description The description of the viewer
	  * @return OpStatus::OK or ERR_NO_MEMORY.
	  */
	OP_STATUS Construct(ViewAction action, URLContentType content_type/*=URL_UNDETERMINED_CONTENT*/, const OpStringC& content_type_string,
	                    const OpStringC& extensions_string, ViewActionFlag::ViewActionFlagType flags=ViewActionFlag::NO_FLAGS,
	                    const OpStringC& application_to_open_with=UNI_L(""), const OpStringC& save_to_folder=UNI_L("")
#ifdef WEB_HANDLERS_SUPPORT
						, const OpStringC& web_handler_to_open_with=UNI_L("")
#endif // WEB_HANDLERS_SUPPORT
#ifdef _PLUGIN_SUPPORT_
	                    , const OpStringC& preferred_plugin_path=UNI_L("")
#endif // _PLUGIN_SUPPORT_
	                    , const OpStringC& description=UNI_L("")
	                    );

#ifndef PREFS_NO_WRITE
	void WriteL();
#endif // !PREFS_NO_WRITE

	/** Get action for this viewer
	  *
	  * @return Action for this viewer
	  */
	ViewAction GetAction() const {return m_action;}

	/** Get flags for this viewer
	  *
	  * @return Flags for this viewer
	  */
	ViewActionFlag GetFlags() const {return m_flags;}

	/** Get content-type for this viewer
	  *
	  * @return Content-type for this viewer
	  */
	URLContentType GetContentType() const {return GET_RANGED_ENUM(URLContentType, m_content_type); }

	/** Get content-type for this viewer, as string
	  *
	  * @return Content-type string, either as uni_char* or char*
	  */
	const uni_char* GetContentTypeString() const {return m_content_type_string.CStr();}
	const char* GetContentTypeString8() const {return m_content_type_string8.CStr();}

	/** Get one of the supported extensions
	  *
	  * @param index Zero-based index into the extension array
	  * @return The extension or NULL if none exist at 'index'.
	  */
	const uni_char* GetExtension(int index) const;

	/** Get number of supported extensions
	  *
	  * @return Number of supported extensions
	  */
	UINT GetExtensionsCount() const {return m_extension_list.GetCount();}

	/** Get the comma-separated list of extensions
	  *
	  * @return The list of extensions
	  */
	const uni_char* GetExtensions() const {return m_extensions_string.CStr();}

	/** Check if this Viewer supports the given extension
	  *
	  * @return TRUE if this Viewer supports the extension, FALSE if not. OpString8-version may Leave
	  */
	BOOL HasExtension(const OpStringC& extension) const;
	BOOL HasExtensionL(const OpStringC8& extension) const;

	/** Get the external application registered for this Viewer
	  *
	  * @return The external application
	  */
	const uni_char* GetApplicationToOpenWith() const {return m_application_to_open_with.CStr();}

#ifdef WEB_HANDLERS_SUPPORT
	/** Get the web handler registered for this Viewer
	  *
	  * @param web_handler Used to return the web handler
	  * @return OpStatus::OK or ERR_NO_MEMORY
	  */
	OP_STATUS GetWebHandlerToOpenWith(OpString& web_handler) const {return web_handler.Set(m_web_handler_to_open_with);}

	/** Get the web handler registered for this Viewer
	  *
	  * @return The external web handler
	  */
	const uni_char* GetWebHandlerToOpenWith() const {return m_web_handler_to_open_with.CStr();}
#endif // WEB_HANDLERS_SUPPORT

	/** Get the SaveTo folder registered for this Viewer
	  *
	  * @return The SaveTo folder
	  */
	const uni_char* GetSaveToFolder() const {return m_save_to_folder.CStr();}


	void SetAction(ViewAction action, BOOL set_userdefined_flag=FALSE) {m_action=action; if (set_userdefined_flag) SetFlag(ViewActionFlag::USERDEFINED_VALUE);}

	void SetFlag(ViewActionFlag::ViewActionFlagType new_flag) {m_flags.Set(new_flag);}
	void SetFlags(short f) {m_flags.SetFlags(f);}

	OP_STATUS SetContentType(URLContentType content_type, const OpStringC& content_type_string);
	OP_STATUS SetContentType(URLContentType content_type, const OpStringC8& content_type_string8);
	OP_STATUS SetContentType(const OpStringC& content_type_string); //Will look up content_type in list of known content types
	OP_STATUS SetContentType(const OpStringC8& content_type_string); //Will look up content_type in list of known content types

	OP_STATUS SetExtensions(const OpStringC& extensions);
	OP_STATUS AddExtension(const OpStringC& extension);
	OP_STATUS ResetToDefaultExtensions();

	OP_STATUS SetApplicationToOpenWith(const OpStringC& application) {return m_application_to_open_with.Set(application);}

#ifdef WEB_HANDLERS_SUPPORT
	OP_STATUS SetWebHandlerToOpenWith(const OpStringC& web_handler) {return m_web_handler_to_open_with.Set(web_handler);}
#endif

	OP_STATUS SetSaveToFolder(const OpStringC& save_to_folder) {return m_save_to_folder.Set(save_to_folder);}

#ifdef _PLUGIN_SUPPORT_
	/** Find the first plugin with the given name (case-sensitive), connected to this Viewer
	  *
	  * @param plugin_name Name of plugin to search for
	  * @param enabled_only Returns only enabled plugins
	  * @return First plugin with given name, or NULL if not found
	  */
	PluginViewer* FindPluginViewerByName(const OpStringC& plugin_name, BOOL enabled_only = FALSE) const;

	/** Find the first plugin with the given path (case-sensitive if not API_PI_OPSYSTEMINFO_PATHSEQUAL is imported), connected to this Viewer
	  *
	  * @param plugin_path Full path of plugin to search for
	  * @return First plugin with given path, or NULL if not found
	  */
	PluginViewer* FindPluginViewerByPath(const OpStringC& plugin_path) const;

	/** Find the default plugin (that is, the plugin at index 0 in the plugin array), connected to this Viewer
	  *
	  * @param skip_disabled_plugins If FALSE, plugin at index 0 is always returned. If TRUE, only enabled plugins are considered
	  * (if plugin at index 0 is disabled, plugin at index 1 will be returned (unless it, too, is disabled))
	  * @return Pointer to the plugin considered default plugin for this Viewer
	  */
	PluginViewer* GetDefaultPluginViewer(BOOL skip_disabled_plugins=TRUE) const;

	/** Find the full path of the default plugin connected to this Viewer
	  *
	  * @param plugin_path Used to return the full path of the default plugin for this Viewer (empty if not found)
	  * @param component_type Used to return the component type of the default plugin for this Viewer
	  * @param skip_disabled_plugins If FALSE, plugin at index 0 is always returned. If TRUE, only enabled plugins are considered
	  * (if plugin at index 0 is disabled, plugin at index 1 will be returned (unless it, too, is disabled))
	  * @return OpStatus::OK or ERR_NO_MEMORY
	  */
	OP_STATUS GetDefaultPluginViewerPath(OpString& plugin_path, OpComponentType& component_type, BOOL skip_disabled_plugins=TRUE) const;

	/** Find the full path of the default plugin connected to this Viewer
	  *
	  * @param skip_disabled_plugins If FALSE, plugin at index 0 is always returned. If TRUE, only enabled plugins are considered
	  * (if plugin at index 0 is disabled, plugin at index 1 will be returned (unless it, too, is disabled))
	  * @return The full path of the default plugin for this Viewer
	  */
	const uni_char* GetDefaultPluginViewerPath(BOOL skip_disabled_plugins=TRUE) const;

	UINT GetPluginViewerCount() const {OpStatus::Ignore(g_plugin_viewers->MakeSurePluginsAreDetected()); return m_plugins.GetCount();}
	PluginViewer* GetPluginViewer(unsigned int index) const {OpStatus::Ignore(g_plugin_viewers->MakeSurePluginsAreDetected()); return m_plugins.Get(index);}

	OP_STATUS SetDefaultPluginViewer(PluginViewer* plugin);
	OP_STATUS SetDefaultPluginViewer(unsigned int index);

	//PluginViewer backward compatibility functions
	const uni_char*	PluginName() const;
	const uni_char*	PluginFileOpenText() const;

	/** Connect a given plugin to this Viewer (that is, register that the given plugin can be used to view this Viewer content-type
	  * and/or extension)
	  *
	  * @param plugin_viewer Pointer to a PluginViewer object (where PluginViewer::Construct(...) is called)
	  * @return OpStatus::OK, ERR_NO_MEMORY or ERR_NULL_POINTER
	  */
	OP_STATUS ConnectToPlugin(PluginViewer* plugin_viewer);

	/** Disconnect a given plugin from this Viewer (that is, prevent the given plugin from viewing this Viewer content-type
	  * and/or extension)
	  *
	  * @param plugin_viewer Pointer to a PluginViewer object (where PluginViewer::Construct(...) is called)
	  * @return OpStatus::OK, ERR_NO_MEMORY or ERR_NULL_POINTER
	  */
	OP_STATUS DisconnectFromPlugin(PluginViewer* plugin_viewer);

#endif // _PLUGIN_SUPPORT_

	OP_STATUS CopyFrom(Viewer* source);

	OP_STATUS Empty(void);

	/** Resets the viewer to its defaults */
	void ResetAction(BOOL user_def = FALSE);

	BOOL IsUserDefined() { return m_flags.IsSet(ViewActionFlag::USERDEFINED_VALUE); }

	OP_STATUS SetDescription(const uni_char* desc) { return m_description.Set(desc, 256); }
	const uni_char* GetDescription() { return m_description; }

#ifdef WEB_HANDLERS_SUPPORT
	void SetWebHandlerAllowed(BOOL allowed) { m_web_handler_allowed = allowed != FALSE; }
	BOOL GetWebHandlerAllowed() { return m_web_handler_allowed; }
#endif // WEB_HANDLERS_SUPPORT

	void SetAllowAnyExtension(BOOL allowed) { m_allow_any_extension = allowed != FALSE; }
	BOOL GetAllowAnyExtension() { return m_allow_any_extension; }

	/** Container format allowed for this media type. */
	inline const char *AllowedContainer();
	void SetAllowedContainer(ViewersEnum container) { OP_ASSERT(container < VIEWER_LAST); m_container = static_cast<unsigned int>(container); }

private:
	OP_STATUS SetExtensions(const char* extensions);
	void SetAllowedContainer(unsigned int container) { SetAllowedContainer(static_cast<ViewersEnum>(container)); }

public:
	static OP_STATUS ParseExtensions(const OpStringC& extensions, const OpStringC& separator_chars/*=UNI_L(",. ")*/, OpVector<OpString>* result_array);

private:
	Viewers* m_owner;
	ViewActionFlag m_flags;

	OpString m_content_type_string;
	OpString8 m_content_type_string8; /* Optimization.. */
	OpString m_extensions_string;
#ifdef WEB_HANDLERS_SUPPORT
	OpString m_web_handler_to_open_with;
#endif // WEB_HANDLERS_SUPPORT
	OpString m_application_to_open_with; /** In case 'Open with other application' */
	OpString m_save_to_folder; /** In case 'Save to disk' */
#ifdef _PLUGIN_SUPPORT_
	OpString m_preferred_plugin_path; /** Used to store plugin info from prefs, before plugins are detected and available */
#endif // _PLUGIN_SUPPORT_
	OpString m_description;

	OpAutoVector<OpString> m_extension_list;
#ifdef _PLUGIN_SUPPORT_
	OpVector<PluginViewer> m_plugins;
#endif // _PLUGIN_SUPPORT_

	ViewAction m_action: 8;
	unsigned int m_content_type: 8;
	unsigned int m_container: 15;			/**< Type can be inside this container. See ViewersEnum for values. */
#ifdef WEB_HANDLERS_SUPPORT
	bool m_web_handler_allowed: 1;			/**< Type can be overridden by web handler. */
#endif // WEB_HANDLERS_SUPPORT
	bool m_allow_any_extension: 1;			/**< Type bypasses strict file extension check. */
};

/** Reply structure for GetViewAction.
  *
  * The string references are only valid as long as the relevant
  * Viewer(s) are, so any modification to the list of Viewers will
  * invalidate the contents of this structure.
  *
  * The mime_type field may also have the same lifetime as the queried
  * MIME-type string, and should thus remain in scope for at least the
  * same duration as the ViewActionReply structure.
  */
struct ViewActionReply
{
	/** The actual MIME-type of the Viewer.
	  *
	  * Will only differ from the queried MIME-type if a possible
	  * (filename) extension was used to find a Viewer.
	  */
	OpStringC mime_type;

	/** The path to the plugin viewer.
	  *
	  * Valid if the action is VIEWER_PLUGIN or if plugin information
	  * was explicitly requested.
	  */
	OpStringC app;

	/** Action to perform for the queried content type. */
	ViewAction action;

	/** The component type required to start a plugin viewer.
	  *
	  * Valid if the action is VIEWER_PLUGIN or if plugin information
	  * was explicitly requested. Defaults to COMPONENT_SINGLETON.
	  */
	OpComponentType component_type;
};

class Viewers
{
friend class Viewer;
#ifdef _PLUGIN_SUPPORT_
friend class PluginViewer;
friend class PluginViewers;
#endif // _PLUGIN_SUPPORT_
public:

	Viewers();
	virtual ~Viewers();

	OP_STATUS ConstructL();

	OP_STATUS ReadViewersL();

#if defined PREFS_HAS_PREFSFILE && defined PREFS_WRITE
	void WriteViewersL();
#endif // defined PREFS_HAS_PREFSFILE && defined PREFS_WRITE

	OP_STATUS AddViewer(ViewAction action, URLContentType content_type/*=URL_UNDETERMINED_CONTENT*/, const OpStringC& content_type_string,
	                    const OpStringC& extensions_string, ViewActionFlag::ViewActionFlagType flags=ViewActionFlag::NO_FLAGS,
	                    const OpStringC& application_to_open_with=UNI_L(""), const OpStringC& save_to_folder=UNI_L("")
#ifdef _PLUGIN_SUPPORT_
	                    , const OpStringC& preferred_plugin_path=UNI_L("")
#endif // _PLUGIN_SUPPORT_
	                    , const OpStringC& description=UNI_L("")
	                    );

	/** Add a viewer to the list of known viewers, and takes ownership of it. The viewer will also be connected to
	  * matching plugins.
	  *
	  * @param viewer Pointer to Viewer object to add to the list of known viewers
	  * @return OpStatus::OK, ERR_NO_MEMORY or ERR (in case a different viewer object with the same content-type
	  * already exists in the list. The new viewer will not be added to the list, and ownership will not be claimed)
	  */
	OP_STATUS AddViewer(Viewer* viewer);
	void DeleteViewer(Viewer* viewer);

	Viewer* FindViewerByMimeType(const OpStringC mimetype);
	Viewer* FindViewerByMimeType(const OpStringC8 mimetype);

	OP_STATUS FindViewerByExtension(const OpStringC& extension, Viewer*& ret_viewer);
	Viewer* FindViewerByExtension(const OpStringC& extension) {Viewer* v; if (FindViewerByExtension(extension, v)!=OpStatus::OK) v=NULL; return v;}
	OP_STATUS FindViewerByExtension(const OpStringC8& extension, Viewer*& ret_viewer);
	Viewer* FindViewerByExtension(const OpStringC8& extension) {Viewer* v; if (FindViewerByExtension(extension, v)!=OpStatus::OK) v=NULL; return v;}
	OP_STATUS FindAllViewersByExtension(const OpStringC& extension, OpVector<Viewer>* result_array);

	OP_STATUS FindViewerByFilename(const OpStringC& filename, Viewer*& ret_viewer);
	Viewer* FindViewerByFilename(const OpStringC& filename) {Viewer* v; if (FindViewerByFilename(filename, v)!=OpStatus::OK) v=NULL; return v;}
	OP_STATUS FindViewerByFilename(const OpStringC8& filename, Viewer*& ret_viewer);
	Viewer* FindViewerByFilename(const OpStringC8& filename) {Viewer* v; if (FindViewerByFilename(filename, v)!=OpStatus::OK) v=NULL; return v;}

	OP_STATUS FindViewerByURL(URL& url, Viewer*& ret_viewer, BOOL update_url_contenttype=TRUE);
	OP_STATUS GetAppAndAction(URL& url, ViewAction& action, const uni_char*& app, BOOL update_url_contenttype=TRUE);

	BOOL IsNativeViewerType(const OpStringC8& content_type);

	/** Create an iterator for retrieving Viewers. The created iterator is assigned to the given argument
	 *
	 * IMPORTANT: The caller is responsible for delete-ing the resulting iterator.
	 * If any error is encountered, the given argument is set to NULL.
	 * If OOM is encountered, ERR_NO_MEMORY is returned.
	 * If the viewers list is empty, ERR is returned.
	 *
	 * @param iterator The created iterator (or NULL, on error) is assigned to this argument
	 * @return OpStatus::OK, ERR_NO_MEMORY, ERR.
	 */
	OP_STATUS CreateIterator(ChainedHashIterator*& iterator);

	/** Return the next Viewer object from the given iterator (originally retrieved from CreateIterator())
	 *
	 * @param iterator The iterator from which to retrieve Viewer object.
	 * @return The next Viewer object from the given iterator, (NULL if no more Viewer objects)
	 */
	Viewer* GetNextViewer(ChainedHashIterator* iterator);

	UINT GetViewerCount() {return m_viewer_list.GetCount();}

	void OnViewerChanged(const Viewer* v) {g_main_message_handler->PostMessage(MSG_VIEWERS_CHANGED, MH_PARAM_1(v), 0);} /** Core will not notify about viewer changes. This function is here to make platform code notify other platform code about changes */

#ifdef WEB_HANDLERS_SUPPORT
	/**
	 * Finds web application viewer by url if there's one.
	 *
	 * @param url to find a web application viewer by
	 * @param ret_viewer returned viewer if found or NULL
	 *
	 * @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::OK otherwise.
	 */
	OP_STATUS FindViewerByWebApplication(const OpStringC& url, Viewer*& ret_viewer);
#endif // WEB_HANDLERS_SUPPORT

public:
	static const char* GetContentTypeString(URLContentType content_type);
	static const char* GetDefaultContentTypeStringFromExt(const OpStringC& ext);
	static const char* GetDefaultContentTypeStringFromExt(const OpStringC8& ext);

	/** Get the configured action for a URL and/or MIME-type.
	 *
	 * Queries the Viewers database for a Viewer configured to handle
	 * content with a specified MIME-type, or a Viewer configured to
	 * handle content with a specified file extension.
	 *
	 * @param url URL used to determine file extension.
	 * @param mime_type MIME-type to query. Can be empty - in which
	 *                  case a file extension will be used instead.
	 * @param reply Reply structure - @see ViewActionReply.
	 * @param want_plugin TRUE if the reply should contain plugin
	 *                    information regardless of action type.
	 * @param check_octetstream TRUE if MIME-types typically used for
	 *                          binary data should be ignored and used
	 *                          the file extension instead.
	 *
	 * @return OpStatus::ERR_NO_MEMORY in case of OOM. OpStatus::OK otherwise.
	 */
	static OP_STATUS GetViewAction(URL& url, const OpStringC mime_type, ViewActionReply& reply, BOOL want_plugin, BOOL check_octetstream=FALSE);
	static ViewAction GetDefaultAction(URLContentType content_type);
	static ViewAction GetDefaultAction(const char* type);


private:
	void DeleteAllViewers();
	OP_STATUS ReadViewers();
	OP_STATUS ImportGeneratedViewersArray();

#ifdef _PLUGIN_SUPPORT_
	void RemovePluginViewerReferences();
#endif

	void OnViewerAdded(const Viewer* v) {g_main_message_handler->PostMessage(MSG_VIEWERS_ADDED, MH_PARAM_1(v), 0);}
	void OnViewerDeleted(const Viewer* v) {g_main_message_handler->PostMessage(MSG_VIEWERS_DELETED, MH_PARAM_1(v), 0);}

public:
	class Viewer8HashFunctions : public OpHashFunctions
	{
	public:
		// Inherited interfaces --
		virtual UINT32 Hash(const void* k);
		virtual BOOL KeysAreEqual(const void *key1, const void *key2);
	};

private:
	OpAutoStringHashTable<Viewer> m_viewer_list; //Hashed by 16-bit string
	OpHashTable m_viewer_list8; //Hashed by 8-bit string
	Viewer8HashFunctions viewer8_hash_func;

#ifndef HAS_COMPLEX_GLOBALS
	void init_defaultOperaViewerTypes();
#else
	static const
#endif // HAS_COMPLEX_GLOBALS
	ViewerTypes defaultOperaViewerTypes[defaultOperaViewerTypes_SIZE];
};

inline const char *Viewer::AllowedContainer()
{ return g_viewers->defaultOperaViewerTypes[m_container].type; }

#endif // _VIEWERS_
