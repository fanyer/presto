/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file OpGadget.h
 *
 * (todo: short description)
 *
 * @author lasse@opera.com
*/

#ifndef OP_GADGET_H
#define OP_GADGET_H
#ifdef GADGET_SUPPORT

#include "modules/dochand/win.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/gadgets/gadget_utils.h"


class Window;
class OpWindow;
class PrefsFile;
class PrefsSection;
class URL;

#ifdef EXTENSION_SUPPORT
/** OpGadgetExtensionListener
  * A gadget represents the background page of an extension, having
  * zero or or more extension environments that communicate with it.
  * These have independent lifetimes, so uses of a gadget needs to
  * register to reliably know when it is terminated/shut down.
  */
class OpGadgetExtensionListener
	: public ListElement<OpGadgetExtensionListener>
{
public:
	virtual ~OpGadgetExtensionListener() {}

	/** TRUE if the extension gadget has notified the listener that
	  * it is now running.
	  */
	virtual BOOL HasGadgetConnected() = 0;

	/** Notify the listener that the extension gadget is now running.
	  */
	virtual void OnGadgetRunning() = 0;

	/** On receiving, the listener can no longer assume the gadget is
	  * alive and available; drop all uses and references.
	  */
	virtual void OnGadgetTerminated() = 0;
};
#endif // EXTENSION_SUPPORT

class OpGadget
	: public Link
	, public OpPersistentStorageListener
	, public OpDocumentListener::GadgetShowNotificationCallback
	, public MessageObject
{
public:
	virtual ~OpGadget();

	/** Opens URL.
	  * Typically this is called by ES.
	  * @param url The URL to open.
	  */
	void OpenURL(const URL& url);

	/** Opens URL.
	  * Typically this is called when the user clicks a link in the widget.
	  * @param url The URL to open.
	  * @param is_dom_call Specify whether this was user initiated or script generated.
	  */
	void OpenURL(const uni_char* url, BOOL is_dom_call = FALSE);

	/** Creates a new window and loads the supplied URL in that window.
	  * Typically this is called by ES or when the user clicks a link in the widget.
	  * @param url The URL to open
	  * @param opener_window The window to use as opener for the new window (optional, will use
	  * the window associated with this gadget otherwise).
	  */
	void OpenURLInNewWindow(const URL& url, Window* opener_window = NULL);

	/** Creates a new window and loads the supplied URL in that window.
	  * Typically this is called by ES or when the user clicks a link in the widget.
	  * @param url The URL to open
	  * @param opener_window The window to use as opener for the new window (optional, will use
	  * the window associated with this gadget otherwise).
	  */
	void OpenURLInNewWindow(const uni_char* url, Window* opener_window = NULL);

#ifdef EXTENSION_SUPPORT
	/** Open the extension's options page in a new window. */
	OP_STATUS OpenExtensionOptionsPage();

	/** Return TRUE if this gadget has an options page. */
	BOOL HasOptionsPage();

	/** Create a new UI window that has this gadget associated with it (so it's able to open
	 *  urls internal to this gadget, for example).
	 *  The window is created using the OpUiWindowListener::CreateUiWindow() interface.
	 *
	 *  @param args The arguments to use when creating the window.
	 *  @param url  Url to open in new window. If NULL then no url shall be open.
	 *  @param[out] new_wc If non-null the windowcommander created for the new window will be returned here.
	 */
	OP_STATUS CreateNewUiExtensionWindow(OpUiWindowListener::CreateUiWindowArgs args, const uni_char *url, OpWindowCommander **new_wc = NULL);

	/** Return TRUE if the provided window is this extension's background process window. */
	BOOL IsExtensionBackgroundWindow(OpWindow *window) { return m_window && m_window->GetOpWindow() == window; };
#endif // EXTENSION_SUPPORT

	/** Reload widget meta data.
	  * Calling this function will reload and reparse config.xml, the index.html
	  * file displayed will not be reloaded.
	  * This function is usually called in response to a reload of index.html
	  */
	void Reload();

	/** Request user attention (by f.ex. flashing the application button in the taskbar).
	  * Usually called by the widget DOM object (script generated).
	  */
	void GetAttention();

	/** Show information popup (ala Growl).
	  * Usually called by the widget DOM object (script generated).
	  * When the platform has displayed the popup, OnShowNotificationFinished() should
	  * be called which will trigger an event in the widget DOM object.
	  * @param message The string to display in the popup.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS ShowNotification(const uni_char* message);

	/** Retrieve the widget url (Eg: widget://[uid]/index.html).
	  * @param url The resulting URL.
	  * @param include_index_file Whether you want to include the index file in the url.
	  * @param include_protocol Whether you want to include the protocol in the url.
	  * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	  */
	OP_STATUS GetGadgetUrl(OpString& url, BOOL include_index_file = TRUE, BOOL include_protocol = TRUE);

	/** Check whether this widget needs to be updated and update if necessary.
	  * This function will attempt to access content on the update server as
	  * specified in config.xml (See http://dev.w3.org/2006/waf/widgets-updates/Overview.src.html
	  * for more information.
	  *
	  * @param additional_params OPTIONAL Additional parameters to be appended to the exisiting ones.
	  *                          Can be used to gather usage statistics, as they allow to personalize
	  *                          the update URL. As such, they raise privacy issues and the caller should
	  *                          be careful, where he sends the additional parameters to.
	  *
	  *                          The parameters are simply appeneded to the update URL, which means that:
	  *                          a) they should be already concatenated with ampersands, b) they should
	  *                          not be prepended by neither & nor ? and c) they already should be properly
	  *                          encoded.
	  */
	OP_GADGET_STATUS Update(const uni_char* additional_params = NULL);

#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
	/** Override the global security policy for this gadget instance.
	  *
	  * If successful, this gadget instance will take over the ownership
	  * of the GadgetSecurityPolicy. This function will only succeed if the
	  * gadget instance is allowed to do this.
	  *
	  * @see SetAllowGlobalPolicy
	  *
	  * @param policy The global policy to use, or @c NULL to disable an existing
	  *               override.
	  * @return OpStatus::OK if set successfully, or OpStatus::ERR if permission
	  *         was denied. (Setting the policy to @c NULL will always be allowed).
	  */
	OP_STATUS SetGlobalSecurityPolicy(GadgetSecurityPolicy *policy);

	/** Get the global security policy override for this gadget instance.
	  *
	  * @return The global policy override, or @c NULL if there is no override.
	  */
	const GadgetSecurityPolicy* GetGlobalSecurityPolicy() const;

	/** Set whether this instance should be allowed to override global policy.
	  *
	  * WARNING: Setting this to @c TRUE means the gadget runtime will have access
	  * to the widget.setGlobalPolicy function, and with it, replace the normal
	  * global policy completely.
	  *
	  * This is not allowed by default, and should only be used under special
	  * testing circumstances. Gadgets deployed over Scope with a special flag
	  * set will be allowed to do this, but it should in general be disallowed
	  * in all other cases.
	  *
	  * @param allow @c TRUE to allow global policy override, @c FALSE to disallow.
	  */
	void SetAllowGlobalPolicy(BOOL allow);

	/** Get whether this instance is allowed to override global policy.
	  *
	  * @see SetAllowGlobalPolicy
	  *
	  * @return @c TRUE if allowed, @c FALSE if not.
	  */
	BOOL GetAllowGlobalPolicy() const;
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

	OP_STATUS OnUiWindowCreated(OpWindow *opWindow, Window *window);
	void OnUiWindowClosing(Window *window);

	// setters getters
	void			SetWindow(Window* window) { m_window = window; }
	Window*			GetWindow() { return m_window; }

	void			SetIsClosing(BOOL closing) { m_is_closing = closing; }
	BOOL			GetIsClosing() { return m_is_closing; }

	void			SetIsOpening(BOOL opening) { m_is_opening = opening; }
	BOOL			GetIsOpening() { return m_is_opening; }

	/** Write or delete a persistent setting.
	  * @param key A key to identify the setting with.
	  * @param data A value to write, or NULL to remove the value.
	  * @return OK if everything went well. */
	OP_STATUS		SetPersistentData(const uni_char *key, const uni_char *data);
	/** Write a persistent setting as a number.
	  * @param key A key to identify the setting with.
	  * @param data A value to write.
	  * @return OK if everything went well. */
	OP_STATUS		SetPersistentData(const uni_char *key, int data);
	/** Get a persistent setting as a string.
	  * @param key A key identifying the setting.
	  * @return The setting string, or NULL if not found or error. */
	const uni_char*	GetPersistentData(const uni_char *key) const;

	OP_STATUS		SetUIData(const uni_char *key, const uni_char *data);
	const uni_char*	GetUIData(const uni_char *key) const;

	OP_STATUS		SetGlobalData(const uni_char *key, const uni_char *data);
	const uni_char*	GetGlobalData(const uni_char *key);

	OP_STATUS       SetSessionData   (const uni_char *key, const uni_char *data);
	const uni_char*	GetSessionData   (const uni_char *key);
	void            ClearSessionData() { m_session_hash.DeleteAll(); }

	const uni_char*	GetIdentifier() { return m_identifier.CStr(); }

	OP_STATUS		SetGadgetName(const uni_char *name);
	OP_STATUS		GetGadgetName(OpString& name);

	OpGadgetClass*	GetClass() { return m_class; }

	void			SetWidth(int w) { m_width = w; }
	int				GetWidth() { return m_width; }

	void			SetHeight(int h) { m_height = h; }
	int				GetHeight() { return m_height; }

	void			SetIsActive(BOOL is_active) { m_is_active = is_active; }
	BOOL			GetIsActive() { return m_is_active; }

	BOOL			GadgetUrlLoaded() { return m_gadget_url_loaded; }

	BOOL			CanSupportMode(WindowViewMode mode);
	OP_STATUS		SetMode(WindowViewMode mode);
	OP_STATUS		SetMode(const OpStringC& mode);

	WindowViewMode	GetMode() { return m_mode; }
	OP_STATUS		GetMode(OpString& mode);

	void			SetIcon(const uni_char *icon);

	URL_CONTEXT_ID	UrlContextId() { return m_context_id; }	// WARNING: UrlContextId != this

#ifdef WEBSERVER_SUPPORT
	void			SetIsRootService(BOOL is_root_service) { m_is_root_service = is_root_service; }
	BOOL			IsRootService() { return m_is_root_service; }
	OP_STATUS		SetupWebSubServer();
	OP_STATUS		GetGadgetSubserverUri(OpString& subserver_uri);

	BOOL				IsVisibleASD() const;
	BOOL				IsVisibleUPNP() const;
	BOOL				IsVisibleRobots() const;

	void				SetVisibleASD(BOOL asd_visibility);
	void				SetVisibleUPNP(BOOL upnp_visibility);
	void				SetVisibleRobots(BOOL robots_visibility);
#endif //WEBSERVER_SUPPORT

	void			SetInstanceSecurity(GadgetSecurityPolicy *policy);
	GadgetSecurityPolicy* GetInstanceSecurity() { return m_security_policy; }

	OP_STATUS		SetSharedFolder(const uni_char *folder);
	const uni_char *GetSharedFolder();

	// OpGadgetClass helpers
	int				InitialWidth() { return m_class->InitialWidth(); }												///< @short @see OpGadgetClass::InitialWidth()
	int				InitialHeight() { return m_class->InitialHeight(); }											///< @short @see OpGadgetClass::InitialHeight()
	GadgetType		GetGadgetType() { return m_class->GetGadgetType(); }											///< @short @see OpGadgetClass::GetGadgetType()
	UINT32			GetGadgetIconCount() { return m_class->GetGadgetIconCount(); }									///< @short @see OpGadgetClass::GetGadgetIconCount()
	OP_STATUS		GetGadgetIcon(UINT32 idx, OpString& path, INT32& w, INT32& h, BOOL full_path = TRUE) { return m_class->GetGadgetIcon(idx, path, w, h, full_path); } ///< @short @see OpGadgetClass::GetGadgetIcon()
	OP_STATUS		GetGadgetIcon(OpBitmap **icon, UINT32 index) { return m_class->GetGadgetIcon(icon, index); }	///< @short @see OpGadgetClass::GetGadgetIcon()
	OP_STATUS		GetGadgetPath(OpString& gadget_path) { return  m_class->GetGadgetPath(gadget_path); }			///< @short @see OpGadgetClass::GetGadgetPath()
	const uni_char* GetGadgetPath() { return m_class->GetGadgetPath(); }											///< @short @see OpGadgetClass::GetGadgetPath()
	const uni_char* GetGadgetRootPath() { return m_class->GetGadgetRootPath(); }									///< @short @see OpGadgetClass::GetGadgetRootPath()
#ifdef EXTENSION_SUPPORT
	OP_STATUS		GetGadgetIncludesPath(OpString& path) { return m_class->GetGadgetIncludesPath(path); }			///< @short @see OpGadgetClass::GetGadgetIncludesPath()
#endif // EXTENSION_SUPPORT
	OP_STATUS		GetGadgetFileName(OpString& name);																///< @short @see OpGadgetClass::GetGadgetFilename()
	OP_STATUS		GetGadgetDownloadUrl(OpString& url) { return m_class->GetGadgetDownloadUrl(url); } 				///< @short @see OpGadgetClass::GetGadgetDownloadUrl()
	OP_STATUS		SetGadgetDownloadUrl(OpString& url) { return m_class->SetGadgetDownloadUrl(url); } 				///< @short @see OpGadgetClass::SetGadgetDownloadUrl()
	WindowViewMode	GetDefaultMode() { return m_class->GetDefaultMode(); }											///< @short @see OpGadgetClass::GetDefaultMode()
	BOOL			HasFileAccess() { return m_class->HasFileAccess(); }											///< @short @see OpGadgetClass::HasFileAccess()
	BOOL			HasGeolocationAccess() { return m_class->HasGeolocationAccess(); }								///< @short @see OpGadgetClass::HasGeolocationAccess()
	BOOL			HasGetUserMediaAccess() { return m_class->HasGetUserMediaAccess(); }							///< @short @see OpGadgetClass::HasGetUserMediaAccess()
	BOOL			IsDockable() { return m_class->IsDockable(); }													///< @short @see OpGadgetClass::IsDockable()
#ifdef WEBSERVER_SUPPORT
	BOOL			IsSubserver() { return m_class->IsSubserver(); }												///< @short @see OpGadgetClass::IsSubserver()
	GADGET_WEBSERVER_TYPE GetWebserverType() { return m_class->GetWebserverType(); }								///< @short @see OpGadgetClass::GetWebserverType()
#else
	BOOL			IsSubserver() { return FALSE; }
#endif //WEBSERVER_SUPPORT
#ifdef EXTENSION_SUPPORT
	BOOL			IsExtension() { return m_class->IsExtension(); }												///< @short @see OpGadgetClass::IsExtension()
#else
	BOOL			IsExtension() { return FALSE; }
#endif // EXTENSION_SUPPORT

#ifdef EXTENSION_SUPPORT
	void			AddExtensionListener(OpGadgetExtensionListener *l) { l->Into(&m_extensions_listeners); }
	/** Inform gadget listeners of change in running state. */
	void			ConnectGadgetListeners();

	BOOL			IsRunning() { return m_is_running; }
	void			SetIsRunning(BOOL r);
	void 			StopExtension();
	BOOL			HasURLFilterAccess() { return m_class->HasURLFilterAccess(); }

	/** Extension configuration option flags; controlling the
	  * operational behaviour of an installed (and/or running)
	  * extension gadget.
	  */
	enum ExtensionFlag
	{
		/** If an extension contains UserJS that would activate
		  * on HTTPS hosted content, this flag controls if those
		  * scripts should be allowed to activate.
		  */
		AllowUserJSHTTPS = 0x1,
		/** If in a private mode setting, this flag controls if an
		  * an extension's UserJS should be allowed to activate
		  * (provided it would otherwise apply.) Not orthogonal to
		  * the above option; HTTPS may or may not be supported within
		  * privacy mode windows.
		  */
		AllowUserJSPrivate = 0x2
	};

	void			SetExtensionFlag(ExtensionFlag flag, BOOL value);
	BOOL			GetExtensionFlag(ExtensionFlag flag);

	/** If url is a relative url, resolve it relative to this gadget's url. */
	URL				ResolveExtensionURL(const uni_char *url);
#endif //EXTENSION_SUPPORT
	BOOL			HasTransparentFeatures() { return m_class->HasTransparentFeatures(); }							///< @short @see OpGadgetClass::HasTransparentFeatures()
	BOOL			HasFeature(const uni_char *feature) { return m_class->HasFeature(feature); }					///< @short @see OpGadgetClass::HasFeature()
	UINT			FeatureCount() { return m_class->FeatureCount(); }												///< @short @see OpGadgetClass::FeatureCount()
	OpGadgetFeature *GetFeature(UINT index) { return m_class->GetFeature(index); }									///< @short @see OpGadgetClass::GetFeature()
	Head*			GetFeatures() { return m_class->GetFeatures(); }												///< @short @see OpGadgetClass::GetFeatures()
	GadgetSignatureState SignatureState() { return m_class->SignatureState(); }										///< @short @see OpGadgetClass::SignatureState()
	const uni_char*	SignatureId() { return m_class->SignatureId(); }												///< @short @see OpGadgetClass::SignatureId()
	OP_STATUS		GetGadgetId(OpString& id) { return id.Set(m_class->GetGadgetId()); }							///< @short @see OpGadgetClass::GetGadgetId(OpString)
	const uni_char* GetGadgetId() { return m_class->GetGadgetId(); }												///< @short @see OpGadgetClass::GetGadgetId()
	BOOL			HasJSPluginsAccess() { return m_class->HasJSPluginsAccess(); }                                  ///< @short @see OpGadgetClass::HasJSPluginsAccess()
	const uni_char* GetLocale() { return m_class->GetLocale(); }													///< @short @see OpGadgetClass::GetLocale()
	BOOL IsFeatureRequired(const uni_char *feature) { return m_class->IsFeatureRequired(feature); }					///< @short @see OpGadgetClass::IsFeatureRequired(const uni_char *)
	BOOL IsFeatureRequired(const char *feature)		{ return m_class->IsFeatureRequired(feature); }					///< @short @see OpGadgetClass::IsFeatureRequired(const char *)
	BOOL IsFeatureRequested(const uni_char *feature) { return m_class->IsFeatureRequested(feature); }				///< @short @see OpGadgetClass::IsFeatureRequested(const uni_char *)
	BOOL IsFeatureRequested(const char *feature)	{ return m_class->IsFeatureRequested(feature); }				///< @short @see OpGadgetClass::IsFeatureRequested(const char *)
	BOOL HasFeatureTagSupport() const { return m_class->HasFeatureTagSupport(); }									///< @short @see OpGadgetClass::HasFeatureTagSupport()
	BOOL SupportsOperaExtendedAPI() const { return m_class->SupportsOperaExtendedAPI(); }							///< @short @see OpGadgetClass::SupportsOperaExtendedAPI()

	INT32			GetAttribute(GadgetIntegerAttribute attr) { return m_class->GetAttribute(attr); }
	void			SetAttribute(GadgetIntegerAttribute attr, INT32 value, BOOL override = FALSE) { m_class->SetAttribute(attr, value, override); }
	const uni_char* GetAttribute(GadgetStringAttribute attr) { return m_class->GetAttribute(attr); }
	OP_STATUS		SetAttribute(GadgetStringAttribute attr, const uni_char* value, BOOL override = FALSE) { return m_class->SetAttribute(attr, value, override); }

	// event handlers
	OP_STATUS	OnDragStart();
	OP_STATUS	OnDragStop();
#ifdef DOM_JIL_API_SUPPORT
	OP_STATUS	OnFocus();
#endif // DOM_JIL_API_SUPPORT
	OP_STATUS	OnShow();
	OP_STATUS	OnHide();
	void		OnShowNotificationFinished(Reply reply);

	enum InitPrefsReason {
		/**
		 * The widget prefs are being accessed for the first time this browsing
		 * session, so the prefs are applied if they have not been applied some
		 * previous time.
		 */
		PREFS_APPLY_FIRST_ACCESS,
		/**
		 * The previously saved widget prefs have been lost (due to data
		 * corruption, disk problem, etc) and therefore they have to be
		 * reapplied regardless.
		 */
		PREFS_APPLY_DATA_LOST,
		/**
		 * The widget prefs have been cleared by delete private data, so the
		 * default prefs are set only if they have been set already at least
		 * once. This is a small optimization during delete private data which
		 * prevents prefs from being reset if they have never been set before.
		 */
		PREFS_APPLY_DATA_CLEARED
	};

	/**
	 * Prefills the widget.preferences object with the values
	 * found in the <preference> elements in config.xml
	 *
	 * @param reason Reason why these prefs are being set. See InitPrefsReason.
	 */
	OP_STATUS	InitializePreferences(InitPrefsReason reason);

public:
	// deprecated functions
	DEPRECATED(OP_STATUS	GetIdentifier(OpString8& identifier));
	DEPRECATED(OP_STATUS	GetIdentifier(OpString& identifier));
	DEPRECATED(void			SetClass(OpGadgetClass* gadget_class));
	DEPRECATED(OP_STATUS	GetGadgetIcon(OpString& icon_path));
	DEPRECATED(OP_STATUS	GetGadgetIcon(OpBitmap **icon));
	DEPRECATED(OP_STATUS	GetDefaultMode(WindowViewMode *mode));
	DEPRECATED(BOOL3		HasIntranetAccess());
	DEPRECATED(void			HasIntranetAccess(BOOL3 intranet));

protected:
	// from OpPersistentStorageListener
	OP_STATUS		SetPersistentData(const uni_char* section, const uni_char* key, const uni_char* value);
	const uni_char*	GetPersistentData(const uni_char* section, const uni_char* key);
	OP_STATUS		DeletePersistentData(const uni_char* section, const uni_char* key);
	OP_STATUS		GetPersistentDataItem(UINT32 idx, OpString& key, OpString& data);
	OP_STATUS		GetStoragePath(OpString& path);
	OP_STATUS		GetApplicationPath(OpString& path);
	// end OpPersistentStorageListener

	// from MessageObject
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	friend class OpGadgetManager;

	/** Constructor
	  * @param gadget_class Mandatory, must point to valid OpGadgetClass object.
	  */
	OpGadget(OpGadgetClass* gadget_class);

	/** Initialization function.
	  * @param gadget_id If creating a NEW gadget leave this empty. If attempting to
	  * reload a previously instantiated gadget, supply the old id here or a new one
	  * will be created and persistent data will be lost.
	  * @return OK if successful, ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS Construct(OpString& gadget_id);

	OP_GADGET_STATUS Upgrade(OpGadgetClass* gadget_class);

	OP_STATUS CreateCacheContext();
	void DeleteCacheContext();

	void SaveToFileL(PrefsFile* prefsfile, const OpStringC& section);
	void LoadFromFileL(PrefsFile* prefsfile, const OpStringC& section);
	OP_STATUS CommitPrefs(void);
	void LoadOldPrefsL(PrefsFile* prefsfile, const OpStringC& section);
	void MigrateHashL(PrefsFile* from_prefsfile, const uni_char* from_section, const uni_char* from_hashname, const uni_char *to_section);

	void ApplyNewStyles();
	OP_STATUS ApplyModeChanged();

	PrefsFile *GetPrefsFile() { return m_prefsfile; }

private:
	OpString m_identifier;
	OpString m_shared_folder;

	OpFileFolder	m_gadget_root;
	OpGadgetClass*	m_class;
	Window*			m_window;
	PrefsFile*		m_prefsfile;
	PrefsSection*	m_mp_section;

	WindowViewMode	m_mode;

	int				m_width;
	int				m_height;

	struct SessionDataItem
	{
		uni_char * key;
		uni_char * data;
		SessionDataItem() : key(NULL), data(NULL){}
		~SessionDataItem()
		{
			op_free(key);
			op_free(data);
		}
	};

	OpAutoStringHashTable<SessionDataItem> m_session_hash;

#ifdef WEBSERVER_SUPPORT
	BOOL			m_is_root_service;
#endif
	BOOL			m_write_msg_posted;
	BOOL			m_is_closing;
	BOOL			m_is_active;
	BOOL			m_gadget_url_loaded;
	BOOL			m_default_prefs_applied;
	BOOL			m_is_opening;

	Head			m_aso_objects;
	GadgetSecurityPolicy* m_security_policy;
#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
	GadgetSecurityPolicy* m_global_security_policy; //< @see SetGlobalSecurityPolicy
	BOOL m_allow_global_security_policy; //< @see SetAllowGlobalPolicy
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

	URL_CONTEXT_ID	m_context_id;

#ifdef EXTENSION_SUPPORT
	List<OpGadgetExtensionListener> m_extensions_listeners;
	BOOL			m_is_running;

	unsigned int	m_extension_flags;
#endif // EXTENSION_SUPPORT
};

//////////////////////////////////////////////////////////////////////////

class OpGadgetAsyncObject : public MessageObject, public Link
{
public:
	OpGadgetAsyncObject(OpGadget* gadget);
	virtual ~OpGadgetAsyncObject();

	// from MessageObject
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OpGadget*		m_gadget;
	URL				m_url;
	BOOL			m_open_in_new_window;
	OpSecurityState m_secstate;

private:
	OpGadgetAsyncObject() {};
};

//////////////////////////////////////////////////////////////////////////

inline OP_STATUS	OpGadget::GetIdentifier(OpString8& identifier) { return identifier.Set(m_identifier.CStr()); }
inline OP_STATUS	OpGadget::GetIdentifier(OpString& identifier) { return identifier.Set(m_identifier.CStr()); }
inline void			OpGadget::SetClass(OpGadgetClass* gadget_class) { m_class = gadget_class; }
inline OP_STATUS	OpGadget::GetGadgetIcon(OpString& icon_path) { INT32 dummy1, dummy2; return m_class->GetGadgetIcon(0, icon_path, dummy1, dummy2); }
inline OP_STATUS	OpGadget::GetGadgetIcon(OpBitmap **icon) { return m_class->GetGadgetIcon(icon, 0); }
inline OP_STATUS	OpGadget::GetGadgetFileName(OpString& name) { return m_class->GetGadgetFileName(name); }
inline OP_STATUS	OpGadget::GetDefaultMode(WindowViewMode *mode) { *mode = m_class->GetDefaultMode(); return OpStatus::OK; }
inline BOOL3		OpGadget::HasIntranetAccess() { return MAYBE; }
inline void			OpGadget::HasIntranetAccess(BOOL3 intranet) {}

#endif // GADGET_SUPPORT
#endif // !OP_GADGET_H
