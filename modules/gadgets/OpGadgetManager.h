/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OP_GADGET_MANAGER_H
#define OP_GADGET_MANAGER_H
#ifdef GADGET_SUPPORT

#include "modules/url/url_man.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/gadget_utils.h"
#include "modules/device_api/OpListenable.h"

#define GADGET_PERSISTENT_DATA_FORMAT_VERSION	1
#define GADGET_PERSISTENT_DATA_FILENAME			UNI_L("widgets.dat")

#define OPEN_GADGET_SUPPORT ///< Core 2.6+ OpGadgetManager::OpenGadget() API available

class OpGadget;
class OpGadgetClass;
class PrefsFile;
class OpString_list;
class OpZipFolder;
class URL_Rep;
class OpGadgetDownloadObject;
class OpGadgetDownloadCallbackToken;
class OpGadgetUpdateObject;

#ifdef GADGETS_INSTALLER_SUPPORT
class OpGadgetInstallObject;
#endif // GADGETS_INSTALLER_SUPPORT

// Callback interface used towards DOM_WidgetManager
class OpGadgetInstallListener
{
public:
	typedef enum {
		GADGET_INSTALL_USER_CONFIRMED,				//< User confirmation dialog returned OK. Typically shown after the user clicks download.
		GADGET_INSTALL_USER_CANCELLED,				//< User confirmation dialog returned CANCEL.
		GADGET_INSTALL_DOWNLOADED,					//< Widget has been downloaded, installation process begins.
		GADGET_INSTALL_DOWNLOAD_FAILED,				//< Download failed (network error).
		GADGET_INSTALL_NOT_ENOUGH_SPACE,			//< Not enough space to install or download widget.
		GADGET_INSTALL_INSTALLED,					//< Downloaded widget installed successfully.
		GADGET_INSTALL_INSTALLATION_FAILED,			//< Installation failed for some reason.
		GADGET_INSTALL_PROCESS_FINISHED				//< Semi-internal; sent when download process has ended (sucessfully or not)
	} Event;

	typedef struct {
		Event event;			//< Type of event (See Event typedef)
		void* userdata;			//< Value passed to OpGadgetManager::DownloadAndInstall
		OpGadgetDownloadObject* download_object;	//< The download object if present or NULL
	} GadgetInstallEvent;

	virtual void HandleGadgetInstallEvent(GadgetInstallEvent& evt) = 0;
	virtual void OnGadgetInstallEvent(GadgetInstallEvent& evt) { HandleGadgetInstallEvent(evt); }
};

/** @short Enumeration for finding a widget. Used in OpGadgetManager::Find */
enum GADGETFindType
{
	/** @short Find widget by name. */
	GADGET_FIND_BY_NAME			= 0,
	/** @short Find widget by path. The path is in the local filesystem. */
	GADGET_FIND_BY_PATH			= 1,
	/** @short Find widget by URL. The URL is based off the widget:// protocol. */
	GADGET_FIND_BY_URL			= 2,
	/** @short Find widget by unique ID. */
	GADGET_FIND_BY_ID			= 3,
	/** @short Find widget by download URL. The URL is the URL the widget was downloaded from. */
	GADGET_FIND_BY_DOWNLOAD_URL = 4,
	/** @short Find widget by filename. */
	GADGET_FIND_BY_FILENAME     = 5,
	/** @short Find widget by ID specified in the widget's config.xml file. */
	GADGET_FIND_BY_GADGET_ID	= 6
#ifdef WEBSERVER_SUPPORT
	/** @short Find widget by service name. */
,	GADGET_FIND_BY_SERVICE_NAME	= 7
#endif
	/** @short Find widget by URL as above but without the index file. */
,	GADGET_FIND_BY_URL_WITHOUT_INDEX	= 8
};

/** @short Enumeration for selecting among wich widgets to search */
enum GADGETFindState
{
	/** @short Search among both active and inactive widgets */
	GADGET_FIND_ALL       = -1,
	/** @short Search among only inactive widgets */
	GADGET_FIND_INACTIVE  = 1 << 0,
	/** @short Search among only active widgets */
	GADGET_FIND_ACTIVE    = 1 << 1
};

#ifdef EXTENSION_SUPPORT
/** Definition of an enabled extension User JavaScript. */
class OpGadgetExtensionUserJS
	: public Link
{
public:
	OpString userjs_filename;	///< Absolute path to the file representing this userjs.
	OpGadget* owner;			///< The owner of this userjs.
	OpGadgetExtensionUserJS *Suc() const { return reinterpret_cast<OpGadgetExtensionUserJS *>(Link::Suc()); }
};
#endif

class OpGadgetManager
	: public MessageObject
	, public OpListenable<OpGadgetListener>
	, public OpListenable<OpGadgetInstallListener>
{
public:
	/** Create a OpGadgetManager instance.
	  * This will create new instance of OpGadgetManager and
	  * start the delayed initialization process.
	  */
	static OP_STATUS Make(OpGadgetManager*& new_obj);

	/** Constructor */
	OpGadgetManager();

	/** Destructor */
	virtual ~OpGadgetManager();

	/** Open a gadget.
	  *
	  * Schedules a gadget to be opened. Depending on the installation status
	  * the gadget may be opened right away or the operation may be postponed.
	  *
	  * If the gadget signature check is still in progress,
	  * OK_SIGNATURE_VERIFICATION_IN_PROGRESS will be returned and the gadget
	  * will be automatically opened when the verification finishes (unless the
	  * signature check fails).
	  *
	  * @param gadget_path Path to folder or zip-archive containing widget.
	  * @param type Content-Type for this gadget.
	  * @return
	  *  OK if successful,
	  *  OK_SIGNATURE_VERIFICATION_IN_PROGRESS if postponed due to sig check being in progress,
	  *  ERR_NO_MEMORY on OOM,
	  *  other errors (see OP_GADGET_STATUS).
	  */
	OP_GADGET_STATUS OpenGadget(const OpStringC& gadget_path, enum URLContentType type) { return OpenGadgetInternal(gadget_path, FALSE, type); }

	/** Open a gadget.
	  * @param gadget Gadget to open.
	  * @return OK if successful, OK_SIGNATURE_VERIFICATION_IN_PROGRESS, ERR_NO_MEMORY on OOM, other errors.
	  */
	OP_GADGET_STATUS OpenGadget(OpGadget* gadget) { return OpenGadgetInternal(gadget, FALSE); }

	/** Close an open gadget.
	 * If the window is to be closed asynchronously then it requires an
	 * ES_Scheduler in the window for it to work. Otherwise if the window is
	 * closed immediately the caller must make sure to not use the window
	 * object after this method returns.
	 *
	 * @param gadget The gadget object that should be closed.
	 * @param async If TRUE then it close the gadget asynchronously, otherwise it closes it immediately.
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	 */
	OP_STATUS CloseGadget(OpGadget *gadget, BOOL async=TRUE);

#ifdef WEBSERVER_SUPPORT
	/** Opens the webserver root service
	  * @param gadget_path Path to folder or zip-archive containing widget.
	  * @return OK if successful, OK_SIGNATURE_VERIFICATION_IN_PROGRESS, ERR_NO_MEMORY on OOM, other errors.
	  */
	OP_GADGET_STATUS OpenRootService(const OpStringC& gadget_path) { return OpenGadgetInternal(gadget_path, TRUE, URL_UNITESERVICE_INSTALL_CONTENT); }

	/** Opens the webserver root service
	  * @param gadget Gadget to open.
	  * @return OK if successful, OK_SIGNATURE_VERIFICATION_IN_PROGRESS, ERR_NO_MEMORY on OOM, other errors.
	  */
	OP_GADGET_STATUS OpenRootService(OpGadget* gadget)  { return OpenGadgetInternal(gadget, TRUE); }

	/** Opens all installed Unite services, except the root service, which
	  * needs to have been started manually by using OpenRootService() first.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_GADGET_STATUS OpenUniteServices();

	/** Close all active Unite services. If no services can be found,
	  * nothing happens, and the function returns OK.
	  * @return OK if sucessful, or OOM.
	  */
	OP_GADGET_STATUS CloseUniteServices();
#endif // WEBSERVER_SUPPORT

	/** Create a widget.
	  * @param gadget Where the widget is returned. Only valid if return value is OK.
	  * @param klass On which OpGadgetClass this widget should be based.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_GADGET_STATUS CreateGadget(OpGadget** gadget, OpGadgetClass *klass);

	/** Create a widget.
	  * NB! This will most likely create a new OpGadgetClass.
	  * @param gadget Where the widget is returned. Only valid if return value is OK.
	  * @param gadget_path Path to folder or zip-archive containing widget.
	  * @param type Content-Type for this gadget.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_GADGET_STATUS CreateGadget(OpGadget** gadget, const OpStringC& gadget_path, enum URLContentType type);

	/** Upgrade a widget.
	  * Upgrade a widget. Retain persistent settings and uid. The old gadget
	  * file will be deleted, if it was located inside the gadgets folder.
	  * @param gadget Widget to upgrade.
	  * @param gadget_path Path to folder or zip-archive containing new widget.
	  * @param type Content-Type for the new gadget.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_GADGET_STATUS UpgradeGadget(OpGadget* gadget, const OpStringC& gadget_path, enum URLContentType type);

	/** Upgrade a widget.
	  * Upgrade a widget. Retain persistent settings and uid. The old gadget
	  * file will be deleted, if it was located inside the gadgets folder.
	  * @param id the id of the widget to upgrade (not the widget UID!).
	  * @param gadget_path Path to folder or zip-archive containing new widget.
	  * @param type Content-Type for the new gadget.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_GADGET_STATUS UpgradeGadget(const uni_char* id, const OpStringC& gadget_path, enum URLContentType type);

	/** Destroys a widget instance.
	  * The widget is removed from the list of widgets to load.
	  * Persistent data is wiped.
	  * Cache is wiped.
	  * @param gadget Widget to destroy.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS DestroyGadget(OpGadget* gadget);

	/** Remove a class of widgets.
	  * All instances of this widget is destoyed.
	  * Persistent data is wiped.
	  * Cache is wiped.
	  * The widget-data itself is wiped.
	  * @param gadget_path Path to widget that is being destroyed.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS RemoveGadget(const OpStringC& gadget_path);

	/** Create a widget class.
	  * Create a widget class without instantiating a widget.
	  * If the class already exists, an upgrade will be attempted.
	  * @param gadget_path Path to widget that is being added.
	  * @param type Content-Type for this gadget.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_GADGET_STATUS CreateGadgetClass(const OpStringC& gadget_path, enum URLContentType type);

	/** Create a widget class.
	  * Create a widget class without instantiating a widget.
	  * If the class already exists, an upgrade will be attempted.
	  * @param gadget_class Pointer to the new widget class is returned here.
	  * @param gadget_path Path to widget that is being added.
	  * @param type Content-Type for this gadget.
	  * @param download_url The download url of gadget.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_GADGET_STATUS CreateGadgetClass(OpGadgetClass** gadget_class, const OpStringC& gadget_path, enum URLContentType type, const OpStringC* download_url = NULL);

	/** Create a widget class.
	  * Create a widget class with data from gadget_path. This function is usually called
	  * to inspect the settings of a widget.
	  * The widget is not added to the system.
	  * The caller owns the result and thus have to deal with cleanup.
	  * @param gadget_path Path to widget.
	  * @param type Content-Type for this gadget.
	  * @param error_reason Set to reason of failure in case of an error(not modified otherwise). 
	  * @return Pointer to the new widget class or NULL.
	  */
	OpGadgetClass* CreateClassWithPath(const OpStringC& gadget_path, enum URLContentType type, const uni_char* download_url, OpString& error_reason);

#ifndef GADGET_LOAD_SUPPORT
	private:
#endif // !GADGET_LOAD_SUPPORT
	/** Load gadgets data
	  * Read and parse widgets.dat; this is used when importing the API_GADGETS_LOAD api
	  * which requries the platform to manually initate loading of gadget data.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_GADGET_STATUS LoadGadgets();
#ifndef GADGET_LOAD_SUPPORT
	public:
#endif // !GADGET_LOAD_SUPPORT

	/** Flushes all widget persistent data to disk.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_GADGET_STATUS SaveGadgets();

	/** @deprecated Use SaveGadgets() instead. */
	DEPRECATED(OP_GADGET_STATUS SaveAllGadgets());

	/** Searches for a gadget given its uid.
	  * @param unique_identifier The uid of the widget to search for.
	  * @return The pointer to the resulting widget or NULL.
	  */
	OpGadget* FindGadget(const OpStringC& unique_identifier);

	/** @deprecated Use FindGadget(const OpStringC& unique_identifier) instead. */
	DEPRECATED(OpGadget* FindGadget(const OpStringC8& unique_identifier));

	/** Searches for a gadget given a specific criterion.
	  * @param find_type The attribute this search will try to match on.
	  * @param find_param The string to match with.
	  * @param state Restricts search according to active status of the widget
	  * @return The pointer to the resulting widget or NULL.
	  */
	OpGadget* FindGadget(const GADGETFindType find_type, const OpStringC& find_param, const GADGETFindState state = GADGET_FIND_ALL);

	/** Searches for a gadget given a specific criterion.
	  * @param find_type The attribute this search will try to match on.
	  * @param find_param The string to match with.
	  * @param is_active Restricts search according to active status of the widget
	  * @return The pointer to the resulting widget or NULL.
	  */
	OpGadget* FindGadget(const GADGETFindType find_type, const OpStringC& find_param, BOOL is_active)
	{ return FindGadget(find_type, find_param, is_active ? GADGET_FIND_ACTIVE : GADGET_FIND_ALL); };

	/** Searches for a gadget given a specific path.
	  * @param path The path to match.
	  * @param is_active Restricts search according to active status of the widget
	  * @return The pointer to the resulting widget or NULL.
	  */
	OpGadget* FindGadget(const OpFile &find_param, BOOL is_active = FALSE)
	{ return FindGadget(GADGET_FIND_BY_PATH, find_param.GetFullPath(), is_active ? GADGET_FIND_ACTIVE : GADGET_FIND_ALL); };

	/** Checks whether a path points to a valid widget.
	  * @param gadget_path Path to folder or zip-archive containing widget.
	  * @return TRUE if the path points to a valid widget, else FALSE.
	  */
	BOOL IsThisAGadgetPath(const OpStringC& gadget_path);

	/** Checks whether a url points to a valid widget.
	  * @param gadget_url Widget url (widet:// protocol)
	  * @return TRUE if the url points to a valid widget, else FALSE.
	  */
	BOOL IsThisAValidWidgetURL(const OpStringC& gadget_url);

	/** @deprecated Use IsThisAGadgetPath(OpStringC& gadget_path) instead. */
	DEPRECATED(BOOL IsThisAGadgetFolder(const OpStringC& gadget_path));

#ifdef WEBSERVER_SUPPORT
	/** Checks whether a path points to a valid Alien widget.
	  * @param gadget_path Path to folder or zip-archive containing widget.
	  * @param allow_noext Allow widget archives without file name extension.
	  * @return TRUE if the path points to a valid widget, else FALSE.
	  */
	BOOL IsThisAnAlienGadgetPath(const OpStringC& gadget_path, BOOL allow_noext = TRUE)
	{ return IsThisA(gadget_path, URL_UNITESERVICE_INSTALL_CONTENT, allow_noext); }

	/** @deprecated Use IsThisAnAlienGadgetPath(OpStringC& gadget_path) instead. */
	DEPRECATED(BOOL IsThisAlienGadget(const OpStringC& gadget_path));

	/** Generates a unique service path for a given Alien widget.
	  * This function will generate a unique service name based off
	  * the service_path parameter appended with f.ex. _1
	  * @param gadget Gadget that needs a unique service path.
	  * @param service_path The base service path (requested path).
	  * @param unique_service_path The resulting unique service path.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS GetUniqueSubServerPath(OpGadget* gadget, const OpStringC &service_path, OpString &unique_service_path);
#endif // WEBSERVER_SUPPORT

#ifdef EXTENSION_SUPPORT
	/** Checks whether a path points to a valid extension widget.
	  * @param gadget_path Path to folder or zip-archive containing widget.
	  * @param allow_noext Allow widget archives without file name extension.
	  * @return TRUE if the path points to a valid widget, else FALSE.
	  */
	BOOL IsThisAnExtensionGadgetPath(const OpStringC& gadget_path, BOOL allow_noext = TRUE)
	{ return IsThisA(gadget_path, URL_EXTENSION_INSTALL_CONTENT, allow_noext); }

	unsigned int NextExtensionUnique() { return m_extension_unique_supply++; }

	/** The gadget manager tracks if extensions add or remove UserJS
	  * files, keeping an updated-since-last-checked flag.
	  * As part of processing the list of UserJS files, the UserJS
	  * manager clears the flag using ResetUserJSUpdated().
	  */
	void ResetUserJSUpdated() { m_extension_userjs_updated = FALSE; }

	/** @return TRUE if UserJS has been added/removed since last
	  * time ResetUserJSUpdated() was called.
	  */
	BOOL HasUserJSUpdates() { return m_extension_userjs_updated; }
#endif // EXTENSION_SUPPORT

	/** @deprecated */
	DEPRECATED(BOOL IsThisAGadgetResource(const uni_char* resource_path));

	/** Convert widget protocol URL to absolute path. Checks if the URL points
	  * to a valid widget and translates the file name into an absolute
	  * file system path inside the widget (possibly inside a localized folder).
	  * @param[in]  msg_handler FIXME: Describe me.
	  * @param[in]  url URL for this widget.
	  *             The URL object's attributes may be modified by this function.
	  *             In particular, the MIME type and charset might be set according
	  *             to meta data set in the gadget's configuration document.
	  * @param[out] full_path Absolute path for the requested file.
	  * @retval OK valid widget, and the file was found in it.
	  * @retval ERR_NO_MEMORY oom
	  * @retval ERR_FILE_NOT_FOUND valid widget but file wasn't found.
	  * @retval ERR not a valid widget or no access.
	  */
	OP_STATUS FindGadgetResource(MessageHandler* msg_handler, URL* url, OpString& full_path);

	/** Retrieve UIDs of all the widgets.
	  * The caller owns the returned vector and thus should call
	  * DeleteAll on it when it's no longer needed.
	  * @param ids The resulting vector.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS GetIDs(OpVector<OpString>* ids);

	/** @deprecated Use GetIDs(OpVector<OpString> *ids) instead. */
	DEPRECATED(OP_STATUS GetIDs(OpVector<OpString8>* ids));

	/** Sets the max limit of how much persistent storage data one can have across all widgets.
	  * Currently a no-op, returning ERR.
	  * @param storage_limit Maximum number of bytes the persistent storage can occupy.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	DEPRECATED (OP_STATUS EnableStorageLimit(unsigned int storage_limit));

	/** Request a widget window to be closed.
	  * This will signal a window to be closed via proper ES events.
	  * @param window The window to close.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS CloseWindowPlease(Window* window);

	/** Returns the number of widget instances.
	  * @return The number of widget instances.
	  */
	UINT32 NumGadgets() { return m_gadgets.Cardinal(); }

	/** Returns the widget instance for given index.
	  * @param index The index of widget to get. Must non-negative and be less than NumGadgets().
	  * @return The widget instance.
	  */
	OpGadget* GetGadget(UINT index) const;

	/** Returns the first widget from (widget) Head
	  * @return The widget instance.
	  */
	OpGadget* GetFirstGadget() const { return static_cast<OpGadget *>(m_gadgets.First()); }

	/** Returns the widget which profile has the given context id.
	  * @return The widget instance or NULL if not found.
	  */
	OpGadget* GetGadgetByContextId(URL_CONTEXT_ID context_id) const;

	/** Returns the number of widget classes.
	  * @return The number of widget classes.
	  */
	UINT32 NumGadgetClasses() { return m_classes.Cardinal(); }

	/** Returns the widget class for given index.
	  * @param index The index of widget class to get. Must non-negative and be less than NumGadgetClasses().
	  * @return The widget class.
	  */
	OpGadgetClass* GetGadgetClass(UINT index) const;

	/** Returns the number of widget instances of a given class.
	  * @param klass The class to count instances of.
	  * @return The number of widget instances of this class.
	  */
	UINT32 NumGadgetInstancesOf(OpGadgetClass* klass);

	/** Deletes all unknown files and directories from the users widget directory.
	  * I would like to discourage the use of this function.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS CleanWidgetsDirectory();

	/** Gets the URL associated with the specific namespace.
	  * @param gadgetNamespace the namespace to get the url for
	  * @param url The string to place the url in.
	  * @return OK if successful, ERR_NO_MEMORY on OOM or ERR on unknown namespace.
	  */
	OP_STATUS GetNamespaceUrl(GadgetNamespace gadgetNamespace, OpString &url);

	/** Sets a key in the global key-value store.
	  * @param key Key to set.
	  * @param data Value to store.
	  * @result OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS SetGlobalData(const uni_char *key, const uni_char *data);

	/** Retrieves information from the global key-value store.
	  * @param key Key to retrieve.
	  * @result Resulting string or NULL.
	  */
	const uni_char*	GetGlobalData(const uni_char *key);

	/** Used to set an identifier that is used to separate different instances of the users widget data.
	  * Could be used on a mobile device to separate widget data between different SIM cards.
	  * Example: Calling this with a key equal to "testkey" will create a folder inside the widget instance
	  * folder called testkey where prefs and cache is store, like so:
	  * <global widgets folder>/<widget id>/testkey
	  *
	  * This is what you'll end up with:
	  * <global widgets folder>/<widget id>/<key>/cache
	  * <global widgets folder>/<widget id>/<key>/cookies4.dat
	  * <global widgets folder>/<widget id>/<key>/prefs.dat
	  *
	  * @param key The key to use when separating this instance from the others.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS SetSeparateStorageKey(const uni_char* key) { return m_separate_storage_key.Set(key); }
	const uni_char* GetSeparateStorageKey() { return m_separate_storage_key.CStr(); }

	PrefsFile* CreatePersistentDataFile();

	/** Add another extension to the list of allowed extensions for widgets.
	  * F.ex: AddAllowedWidgetExtension(UNI_L(".auw"))
	  *
	  * By default the extensions ".wgt" and ".zip" are supported
	  *
	  * @param ext Extension to add to the allowed list
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS AddAllowedWidgetExtension(const uni_char *ext);

	/** Checks if filename contains allowed extension.
	  *
	  * @param filename Filename of widget
	  * @return TRUE if filename ends with one of the allowed extensions or FALSE if not.
	  */
	BOOL IsAllowedExtension(const uni_char *filename);

#ifdef GADGET_DUMP_TO_FILE
	/** Dumps all parsed information about a given widget to a file.
	  *
	  * @param filename Filename of widget
	  * @param target File to write output to
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS Dump(const OpStringC &filename, OpFileDescriptor *target);
#endif // GADGET_DUMP_TO_FILE

	/**
	 * @deprecated Use AttachListener() instead.
	 */
	DEPRECATED(inline OP_STATUS AddListener(OpGadgetInstallListener *listener));
	/**
	 * @deprecated Use DetachListener() instead.
	 */
	DEPRECATED(inline void RemoveListener(OpGadgetInstallListener *listener));

	/**
	 * Attach a new listener for installation events. The listener will be
	 * notified when the installation process changes. For instance when
	 * the widget was downloaded or installed.
	 * @param listener The listener object to receive callbacks on installer events.
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	 */
	OP_STATUS AttachListener(OpGadgetInstallListener *listener) { return OpListenable<OpGadgetInstallListener>::AttachListener(listener); }

	/**
	 * @copydoc OpListenable::DetachListener
	 */
	OP_STATUS DetachListener(OpGadgetInstallListener *listener) { return OpListenable<OpGadgetInstallListener>::DetachListener(listener); }

	/**
	 * Attach a new listener for widget events. The listener will be notified
	 * on important events regarding widgets. For instance when a widget
	 * is installed, removed, started or stopped.
	 * @param listener The listener object to receive callbacks on widget events.
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	 */
	OP_STATUS AttachListener(OpGadgetListener *listener) { return OpListenable<OpGadgetListener>::AttachListener(listener); }

	/**
	 * @copydoc OpListenable::DetachListener
	 */
	OP_STATUS DetachListener(OpGadgetListener *listener) { return OpListenable<OpGadgetListener>::DetachListener(listener); }

	OP_GADGET_STATUS DownloadAndInstall(const uni_char *url, const uni_char *caption, void* userdata);
	OP_GADGET_STATUS Install(OpGadgetDownloadObject *obj);
	OP_STATUS DownloadSuccessful(OpGadgetDownloadObject *obj);
	OP_STATUS DownloadFailed(OpGadgetDownloadObject *obj);

#ifdef GADGETS_INSTALLER_SUPPORT
	/** Install a temporary widget file to the system.
	  * The newly created gadget class will be available in the @a obj.
	  */
	OP_STATUS Install(OpGadgetInstallObject *obj);

	/** Upgrade an existing gadget to use a new widget file as defined in
	  * @a obj.
	  * The newly created gadget class will be available in the @a obj.
	  */
	OP_STATUS Upgrade(OpGadgetInstallObject *obj);
#endif // GADGETS_INSTALLER_SUPPORT

#ifdef SELFTEST
	void SetAllowBundledGlobalPolicy(BOOL allow) { m_allow_bundled_global_policy  = allow; }
	BOOL GetAllowBundledGlobalPolicy() const { return m_allow_bundled_global_policy; }
#endif // SELFTEST

#ifdef EXTENSION_SUPPORT
	/** Get the first active user JavaScript.
	  * @return The first active user JavaScript in the list,
	  *         NULL if there are none. */
	OpGadgetExtensionUserJS *GetFirstActiveExtensionUserJS() { return reinterpret_cast<OpGadgetExtensionUserJS *>(m_enabled_userjs.First()); }
	/** Enable a user JavaScript. For use from OpGadget only. */
	void AddUserJavaScript(OpGadgetExtensionUserJS *p) { m_extension_userjs_updated = TRUE; p->Into(&m_enabled_userjs); }
	/** Disable a user JavaScript. For use from OpGadget only. */
	void DeleteUserJavaScripts(OpGadget *owner);

	BOOL IsExtensionBackgroundWindow(OpGadget *gadget, Window *window) { return gadget->IsExtension() && window == gadget->GetWindow(); }

	/**
	 * Notification method called by PS_Manager to tell the gadget with the given context id
	 * that its widget.preferences data has been cleared, and therefore the widget should
	 * reload the <preference> elements' values from the config.xml file.
	 */
	OP_STATUS OnExtensionStorageDataClear(URL_CONTEXT_ID context_id, OpGadget::InitPrefsReason reason);
#endif

	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, GadgetUpdateReady, OpGadgetListener::OpGadgetUpdateData&);

	/** Check if an update is available for an installed gadget.
	 *
	 * If an update is available for the widget @a klass, notifies the listeners
	 * (OnGadgetUpdateReady).
	 *
	 * @param klass The gadget to update.
	 * @param additional_params Additonal params passed to OpGadgetUpdateObject::Make.
	 */
	OP_GADGET_STATUS Update(OpGadgetClass* klass, const uni_char* additional_params = NULL);

	/** Check if an update is available for a not-yet-installed gadget.
	 *
	 * If @a update_description_document_url describes a gadget that can be downloaded
	 * and installed, notifies the listeners (OnGadgetUpdateReady).
	 *
	 * @param update_description_document_url URL of the Update Description Document
	 * for the gadget.
	 * @param user_data Pointer to arbitrary user data that will be attached to the notification.
	 * @param additional_params Additonal params passed to OpGadgetUpdateObject::Make.
	 */
	OP_GADGET_STATUS Update(const uni_char* update_description_document_url,
							const void* user_data,
							const uni_char* additional_params = NULL);

protected:
	friend class OpGadget;
	friend class OpGadgetClass;
	friend class OpGadgetParsers;
	friend class OpGadgetUpdateObject;
	friend class OpGadgetDownloadCallbackToken;
#ifdef SELFTEST
	friend class ST_gadgetsgadgetmanager;
	friend class ST_gadgetsgadgetclass;
	friend class ST_gadgetslocalized;
	friend class ST_gadgetsgadgetparsers;
#endif
#ifdef SCOPE_WIDGET_MANAGER_SUPPORT
	// Needs access to locale methods
	friend class OpScopeWidgetManager;
#endif

	//////////////////////////////////////////////////////////////////////////
	// Notify calls for OpGadgetInstallListener

	/**
	 * Notifies all install-listeners about a change in the installation
	 * process. For instance that the widget was downloaded or installed.
	 * @param arg1 Information about the event.
	 */
	DECLARE_LISTENABLE_EVENT1(OpGadgetInstallListener, GadgetInstallEvent, OpGadgetInstallListener::GadgetInstallEvent&);

	//////////////////////////////////////////////////////////////////////////
	// Notify calls for OpGadgetListener

	// Notify calls shared with OpGadget
	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, GadgetUpgraded, OpGadgetListener::OpGadgetStartStopUpgradeData&);
	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, GadgetStarted, OpGadgetListener::OpGadgetStartStopUpgradeData&);
	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, GadgetStopped, OpGadgetListener::OpGadgetStartStopUpgradeData&);

	// Internal Notify calls
	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, GadgetDownloaded, OpGadgetListener::OpGadgetDownloadData&);
	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, RequestRunGadget, OpGadgetListener::OpGadgetDownloadData&);
	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, GadgetInstanceCreated, OpGadgetListener::OpGadgetInstanceData&);
	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, GadgetInstanceRemoved, OpGadgetListener::OpGadgetInstanceData&);
	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, GadgetRemoved, OpGadgetListener::OpGadgetInstallRemoveData&);
	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, GadgetInstalled, OpGadgetListener::OpGadgetInstallRemoveData&);
	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, GadgetStartFailed, OpGadgetListener::OpGadgetStartFailedData&);
	DECLARE_LISTENABLE_EVENT1(OpGadgetListener, GadgetSignatureVerified, OpGadgetListener::OpGadgetSignatureVerifiedData&);

	//////////////////////////////////////////////////////////////////////////
	// More or less internal functions

	OP_STATUS GetIDForURL(OpString& id, const OpStringC& url);
	void CancelUpdate(OpGadgetClass* klass);
	OP_GADGET_STATUS Disable(OpGadgetClass *klass);

	OP_GADGET_STATUS DownloadAndInstall_stage2(BOOL allow, const uni_char *url, const uni_char *caption, void *userdata);

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

#ifdef SIGNED_GADGET_SUPPORT
	void SignatureVerified(OpGadgetClass *klass);
#endif // SIGNED_GADGET_SUPPORT

private:
	OP_STATUS DeleteGadgetClass(OpGadgetClass* klass);

	OP_STATUS FindGadgetClass(OpGadgetClass*& gadget_class, const OpStringC& gadget_path);
	OP_STATUS FindInactiveGadget(OpGadget** found_gadget, OpGadgetClass* gadget_class);

	/** Sets the global security policy for gadgets.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS SetupGlobalGadgetsSecurity();
	OP_STATUS ParseGlobalGadgetsSecurity(const char *ini_name, BOOL signed_security, BOOL &security_failure);

	OP_STATUS NormalizeGadgetPath(OpString& gadget_path, const OpStringC& gadget_url, OpGadget** gadget = NULL);

	/** Finds the path to the widget resource. If traverse is TRUE, we
	  * get the localized version of a widget resource. If the file is
	  * not localized, the unlocalized default version is returned.
	  *
	  * If traverse is FALSE, we only check if the file exists or not.
	  *
	  * All paths passed and returned must use the local path separator
	  * convention, so if your source is a URL or path attribute, you
	  * need to call OpGadgetUtils::ChangeToLocalPathSeparators() first.
	  *
	  * @param[in]  klass        class of the gadget we are searching in. MUST NOT be NULL.
	  * @param[in]  path         The file to be located.
	  * @param[in]  traverse     Whether to traverse the "locales" folder.
	  * @param[out] target       the full path to the file to be used.
	  * @param[out] found        TRUE if file was found anywhere, FALSE if not.
	  * @param[out] locale       the locale used (eg en-gb, nb-no)
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS Findi18nPath(OpGadgetClass* klass, const OpStringC& path, BOOL traverse, OpString& target, BOOL &found, OpString *locale = NULL);

	/** Finds the root path of an incorrectly packed legacy widget.
	  * If the config.xml is in a folder named after the base name of the
	  * widget, sets the gadget_root_path to the full path to this
	  * directory.
	  * @param[in]  gadget_path
	  *   Path to the widget archive.
	  * @param[out] gadget_root_path
	  *   Path to the legacy widget root, if different from gadget_path.
	  */
	OP_STATUS FindLegacyRootPath(const OpStringC &gadget_path, OpString &gadget_root_path);

public:
	/** Returns the priority number of the language code, as found in an
	  * xml:lang attribute for element-based localization. 0 means
	  * language was not found. 1 is the highest priority, 2 the
	  * next-highest, etc.
	  * @param xmllang The value of the xml:lang attribute.
	  * @param default_locale fall-back locale for the gadget, added at
	  *        the end of the browser locale list when getting priority.
	  * @return language priority
	  */
	int GetLanguagePriority(const uni_char *xmllang, const uni_char* default_locale = NULL);
private:
	/** Finds gadget class, if previously loaded or if another instance of same class gadget
	  * was previously loaded.
	  * @param gadget_class The pointer that contains the gadget class.
	  * @param gadget_path The path of gadget.
	  * @param download_url The download url of gadget.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_GADGET_STATUS FindOrCreateClassWithPath(OpGadgetClass** gadget_class, const OpStringC& gadget_path, URLContentType type, const OpStringC* download_url = NULL);

	/** Loads all gadgets from the widgets.dat file.
	 *
	 * @param gadget_class_flags Bit flags of kinds of gadgets to load,
	 * see GadgetClass enum for allowed values. Defaults to all gadget types.
	 */
	OP_GADGET_STATUS LoadAllGadgets(unsigned int gadget_class_flags = GADGET_CLASS_ANY);
	OP_GADGET_STATUS LoadGadget(PrefsFile* datafile, OpString& identifier, const OpStringC& gadget_path, const OpStringC& download_url, URLContentType type);

	/** Check if a gadget content type is of a given gadget class.
	 *
	 * Checks whether the gadget install content type matches one of the gadget
	 * classes in gadget_class_flags.
	 *
	 * @param type Gadget install content type.
	 * @param gadget_class_flags Bitwise ORed flags of class types to match against.
	 * See the GadgetClass enum.
	 */
	BOOL IsContentTypeOfGadgetClass(URLContentType type, unsigned int gadget_class_flags);

	/** @deprecated Use IsThisAGadgetPath instead. */
	DEPRECATED(BOOL IsThisAGadgetFile(const OpStringC& gadget_path));

	OP_GADGET_STATUS HelpLoadAllGadgets(PrefsFile* datafile, OpString_list* sections, unsigned int gadget_class_flags);
	OP_GADGET_STATUS HelpSaveAllGadgets(PrefsFile* datafile);
	OP_STATUS CommitPrefs(void);

	/** Second-phase constructor. */
	OP_STATUS Construct();

	/** Initializes the widget subsystem and initiates loading of widgets.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS Initialize();

	/** Sends out notifications that widget opening has failed.
	  *
	  * @param klass Class of the widget.
	  * @param gadget The gadget instance if available (may be NULL).
	  */
	void OpenGadgetFailed(OpGadgetClass* klass, OpGadget* gadget = NULL);
	void OpenGadgetFailed(OpGadget* gadget) { OP_ASSERT(gadget); OpenGadgetFailed(gadget->GetClass(), gadget); }

	/** Configure the browser locale, as seen by widgets. */
	OP_STATUS SetupBrowserLocale();
	/** Helper method for SetupBrowserLocale(). */
	OP_STATUS SetupLanguagesFromLocaleList(OpString *);
#ifdef SELFTEST
	/** For use by module-internal testsuite code only. */
	OP_STATUS OverrideBrowserLocale(const char *);
#endif
	/* Helper class for iterating list of locales. */
	class GadgetLocaleIterator
	{
		friend class OpGadgetManager;
	public:
		const uni_char* GetNextLocale()
		{
			const uni_char* ret_val = *m_cur_ptr ? m_cur_ptr : NULL;
			m_cur_ptr = m_cur_ptr + uni_strlen(m_cur_ptr) + 1;
			return ret_val;
		};
	private:
		GadgetLocaleIterator(const uni_char* locale_store) : m_cur_ptr(locale_store) {}
		const uni_char* m_cur_ptr;
		
	};

	/** Iterator for browser locales. */
	GadgetLocaleIterator GetBrowserLocaleIterator() { return GadgetLocaleIterator(m_browser_locale.CStr()); }

	OP_GADGET_STATUS OpenGadgetInternal(const OpStringC& gadget_path, BOOL is_root_service, enum URLContentType type);
	OP_GADGET_STATUS OpenGadgetInternal(OpGadget* gadget, BOOL is_root_service);

private:
#if defined WEBSERVER_SUPPORT || defined EXTENSION_SUPPORT
	/** Helper for IsThisAnAlienGadgetPath() and IsThisAnExtensionGadgetPath() */
	BOOL IsThisA(const OpStringC& gadget_path, URLContentType wanted_type, BOOL allow_noext);
#endif

	/** Removes unwanted folder/files from Widget cache dir.
	  * @return OK if successful, ERR for other errors.
	  */
	OP_STATUS CleanCache();

	Head m_gadgets;
	Head m_classes;
	OpVector<OpString> m_delete_on_exit;	// cache directories to be deleted on exit
	OpVector<OpString> m_gadget_extensions;

	OpString m_gadget_path;
	OpString m_separate_storage_key;
	OpString m_browser_locale;              // Do not access this directly - use GetBrowserLocaleIterator

	PrefsFile *m_prefsfile;

	Head m_update_objects;
	Head m_download_objects;
	Head m_tokens;

#ifdef EXTENSION_SUPPORT
	/** List of userjs scripts enabled by extensions */
	Head m_enabled_userjs;

	/** TRUE if a gadget has added/removed extension UserJS;
	  * cleared by ResetUserJSUpdated(). */
	BOOL m_extension_userjs_updated;

	/** Across-extension unique ID source; required for IDs on global UI reps. of these.
	  * This counter could overflow, but not very likely (an extension uses maybe one of these),
	  * so no escape mechanism provided (or overflow caught.) */
	unsigned int m_extension_unique_supply;
#endif

	BOOL m_write_msg_posted;
#ifdef SELFTEST
	BOOL m_allow_bundled_global_policy;
#endif // SELFTEST
};

inline BOOL OpGadgetManager::IsThisAGadgetFile(const OpStringC& gadget_path) {  return IsThisAGadgetPath(gadget_path); }
inline BOOL OpGadgetManager::IsThisAGadgetFolder(const OpStringC& gadget_path) { return IsThisAGadgetPath(gadget_path); }
inline OP_GADGET_STATUS OpGadgetManager::SaveAllGadgets() { return SaveGadgets(); }
#ifdef WEBSERVER_SUPPORT
inline BOOL OpGadgetManager::IsThisAlienGadget(const OpStringC& gadget_path) { return IsThisAnAlienGadgetPath(gadget_path); }
#endif // WEBSERVER_SUPPORT
inline OP_STATUS OpGadgetManager::EnableStorageLimit(unsigned int storage_limit) { return OpStatus::ERR; }

// Deprecated functions

OP_STATUS OpGadgetManager::AddListener(OpGadgetInstallListener *listener) { return OpListenable<OpGadgetInstallListener>::AttachListener(listener); }
void OpGadgetManager::RemoveListener(OpGadgetInstallListener *listener) { OpStatus::Ignore(OpListenable<OpGadgetInstallListener>::DetachListener(listener)); }

/** Singleton global instance of OpGadgetManager */
#define g_gadget_manager (g_opera->gadgets_module.m_gadget_manager)

#endif // GADGET_SUPPORT
#endif // !OP_GADGET_MANAGER_H
