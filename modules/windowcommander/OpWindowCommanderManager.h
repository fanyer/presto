/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OP_WINDOW_COMMANDER_MANAGER_H
#define OP_WINDOW_COMMANDER_MANAGER_H

#include "modules/hardcore/base/opstatus.h"

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
//for PersistentStorageCommander class
# include "modules/database/ps_commander.h"
#endif //DATABASE_MODULE_MANAGER_SUPPORT

#ifdef WIC_TAB_API_SUPPORT
#include "modules/windowcommander/OpTabAPIListener.h"
#endif //WIC_TAB_API_SUPPORT

class OpWindowCommander;
class OpTransferManager;
class OpAuthenticationCallback;
class OpSensor;

#ifdef GADGET_SUPPORT
class OpGadgetClass;
class OpGadget;
#endif // GADGET_SUPPORT

class OpUiWindowListener
{
public:
	virtual ~OpUiWindowListener() {}

	enum WindowType
	{
		WINDOWTYPE_REGULAR,				///< Regular window
		WINDOWTYPE_WIDGET,				///< Widget window
		WINDOWTYPE_EXTENSION			///< Extension
	};

	enum ExtensionType
	{
		EXTENSIONTYPE_HIDDEN,			///< Hidden extension (do we need to expose this to the UI?)
		EXTENSIONTYPE_WIDGET,			///< The extensions main script is a widget.
		EXTENSIONTYPE_TOOLBAR,			///< Extension toolbar
		EXTENSIONTYPE_POPUP,			///< The window for the extension's popup
		EXTENSIONTYPE_OPTIONS			///< The extension options page
	};

#ifdef WIC_ADDITIONAL_WINDOW_CREATION_ARGS
	struct CreateUiWindowArgs
	{
		CreateUiWindowArgs() { op_memset(this, 0, sizeof(CreateUiWindowArgs)); }

		OpWindowCommander* opener;
		WindowType type;

		/** If a window is locked, then the window shall not be closed unless
		 *	its associated top-level window closes or the entire application
		 *	exits. This means that
		 *	- the UI's implementations of OpUiWindowListener::CloseUiWindow()
		 *	  and OpDocumentListener::OnClose() need to ignore the request,
		 *	- the UI shall not close the window (especially it shall not call
		 *	  OpWindowCommander::OnUiWindowClosing() or
		 *	  OpWindowCommanderManager::ReleaseWindowCommander()) before the
		 *	  associated top-level window is closed or the entire application
		 *	  exits. E.g. the UI could disable the close-button or the close
		 *	  action from the window's popup-menu.
		 *  @note If UI supports window locking then it should also provide
		 *        user the possiblility of unlocking a window.
		 */
		BOOL locked;

#ifdef WIC_TAB_API_SUPPORT
		/** @param insert_target optional. Specifies the position in window/ tab structure
		 *         where the window should be open. Only valid for WINDOW_REGULAR windows.
		 *         If it is NULL then the UI shall place the window at its discretion.
		 */
		const OpTabAPIListener::WindowTabInsertTarget* insert_target;
#endif // WIC_TAB_API_SUPPORT
		union
		{
			struct
			{
				UINT32 width;			///< the recommended width (check! with or without decoration?)
				UINT32 height;			///< the recommended height (check! with or without decoration?)
				bool scrollbars;		///< FALSE if javascript requested no toolbars, TRUE if normal rules should apply.
				bool toolbars;			///< FALSE if javascript requested no toolbars, TRUE if normal rules should apply.
				bool focus_document;	///< If TRUE, the new window will probably receive input focus or something.
				bool open_background;	///< TRUE if the window is to be opened in the background, stacking-order wise.
				bool transparent;		///< TRUE if the window should have per-pixel-transparency, FALSE otherwise.
				bool opened_by_script;	///< TRUE if opened by script.
				bool user_initiated;	///< TRUE if initiated by user.
			} regular;

			struct
			{
				OpGadget* widget;		///< the gadget for which the window is to be created
				UINT32 width;			///< the requested width (without decoration)
				UINT32 height;			///< the requested height (without decoration)
			} widget;

			struct
			{
				void *data;					///< Additional data that might be needed to open an extension window. Defined and owned by the UI, core doesn't touch this.
				ExtensionType type;			///< Extension type (see enum ExtensionType)
			} extension;
		};
	};
#endif // WIC_ADDITIONAL_WINDOW_CREATION_ARGS

	/** These flags should be combined with | and given as 'flags' parameter in CreateUiWindow */
	enum CreateUiWindowFlags {
		CREATEFLAG_NO_FLAGS				= 0x0,
		CREATEFLAG_SCROLLBARS			= 0x1,	// The window should have scrollbars enabled.
		CREATEFLAG_TOOLBARS				= 0x2,	// The window should have toolbar enabled.
		CREATEFLAG_FOCUS_DOCUMENT		= 0x4,	// The window will probably receive focus.
		CREATEFLAG_OPEN_BACKGROUND		= 0x8,	// The window should be opened in the background, stacking-order wise.
		CREATEFLAG_TRANSPARENT			= 0x10,	// The window should have per pixel transparency.
		CREATEFLAG_OPENED_BY_SCRIPT		= 0x20,	// The window is opened by a script.
		CREATEFLAG_USER_INITIATED		= 0x40	// The window is initiated by an user.
	};

	/** Called when Opera needs the UI to embed a core window in a ui.
	 *
	 * The specified OpWindowCommander is created before calling this method and
	 * the implementation of this method shall associate the UI window with this
	 * OpWindowCommander instance (and not request a new OpWindowCommader
	 * instance (by calling the method
	 * OpWindowCommanderManager::GetWindowCommander()). Apart from that, the
	 * exact same procedure should be carried out as when the UI itself
	 * initiates window creation.
	 *
	 * @param windowCommander a OpWindowCommander instance which was created by
	 *  the caller. The pointer shall never be 0. If this method returns with
	 *  status OpStatus::OK, the implementation of this method takes over the
	 *  ownership of this instance.
	 * @param opener The window commander that triggered this window creation
	 *  (or 0 if there is no opener window).
	 * @param width the recommended width (check! with or without decoration?)
	 * @param height the recommended height (check! with or without decoration?)
	 * @param flags is an OR-combination of the enum CreateUiWindowFlags.
	 * @retval OpStatus::OK if the UI window was created and the specified
	 *  OpWindowCommander instance is now owned by the UI window representation.
	 * @retval OpStatus::ERR or another error-code if the UI window was not
	 *  created or there was an error on trying to do so. In this case the
	 *  specified OpWindowCommander instance will be deleted by the caller.
	 */
	virtual OP_STATUS CreateUiWindow(OpWindowCommander* windowCommander, OpWindowCommander* opener, UINT32 width, UINT32 height, UINT32 flags = CREATEFLAG_SCROLLBARS | CREATEFLAG_TOOLBARS | CREATEFLAG_USER_INITIATED) = 0;
#ifdef WIC_ADDITIONAL_WINDOW_CREATION_ARGS
	virtual OP_STATUS CreateUiWindow(OpWindowCommander* windowCommander, const CreateUiWindowArgs &args) = 0;
#endif // WIC_ADDITIONAL_WINDOW_CREATION_ARGS

#ifdef WIC_CREATEDIALOGWINDOW
	/** @short Called when core needs the UI to embed a core window in a dialog-style window.
	 *
	 * In this case, the WindowCommander will be pre-created, and no new should
	 * be requested from core. Apart from that, the exact same procedure should
	 * be carried out as when the UI itself initiates window creation.
	 *
	 * The dialog-style window could be centered in its opener window. Window
	 * stacking-wise, it should always be on top its opener window.
	 *
	 * @param wic A pre-created WindowCommander.
	 * @param opener The opener window; the window commander that triggered
	 * this window creation
	 * @param width Suggested window width, not including window decorations
	 * @param height Suggested window height, not including window decorations
	 * @param modal TRUE if the window dialog-style window should be modal -
	 * i.e. not allow input to or focusing of any other windows while it is
	 * open.
	 *
	 * @return OK if the window creation was wanted, ERR if it was
	 * rejected. The returning of an error code will cause the pre-created
	 * windowcommander to be destroyed elsewhere, i.e. it does not need to be
	 * taken care of in the UI layer.
	 */
	virtual OP_STATUS CreateDialogWindow(OpWindowCommander* wic, OpWindowCommander* opener, unsigned int width, unsigned int height, BOOL modal) = 0;
#endif // WIC_CREATEDIALOGWINDOW

	/** Called when Opera needs the UI to close one of its
		windows. The exact same procedure should be carried out as when the UI
		itself initiates window removal.

		Note that OpWindowCommander::OpDocumentListener::OnClose will also be called.

		@param windowCommander the WindowCommander whose window Opera wants to destroy.
	*/

	virtual void CloseUiWindow(OpWindowCommander* windowCommander) = 0;

};

/** Listener used to report when opera switches to and from the idle state. */
class OpIdleStateChangedListener
{
public:
	virtual ~OpIdleStateChangedListener() {}

	/** Report the new idle state. */
	virtual void OnIdle() = 0;
	virtual void OnNotIdle() = 0;

};

/** Listener used to report "out of memory" and other similar error conditions. */
class OpOomListener
{
public:
	virtual ~OpOomListener() {}

	/** Went out of memory while attempting to perform an operation. The operation failed. */
	virtual void OnOomError() = 0;

	/** Went out of memory, but no operation failed because of it. */
	virtual void OnSoftOomError() = 0;

	/** Went out of disk space. */
	virtual void OnOodError() = 0;
};

/**
 * This abstract class is used to handle authentication requests for loading a
 * url which has no associated window. In that case
 * OpAuthenticationListener::OnAuthenticationRequired() is called.
 *
 * The UI can implement this listener class to open an authentication dialog and
 * ask the user for user-name and password. The user's data needs to be returned
 * to the caller by calling OpAuthenticationCallback::Authenticate() or
 * OpAuthenticationCallback::CancelAuthentication().
 *
 * The UI can register an instance of the implementation of this class on the
 * OpWindowCommanderManager by calling
 * OpWindowCommanderManager::SetAuthenticationListener().
 */
class OpAuthenticationListener
{
public:
	virtual ~OpAuthenticationListener() {}

	/** This method is called when a url that doesn't have a window needs
	 * authentication information. The implementation of this interface needs to
	 * call OpAuthenticationCallback::Authenticate() or
	 * OpAuthenticationCallback::CancelAuthentication() to continue processing
	 * the url.
	 *
	 * @param callback is the OpAuthenticationCallback for the authentication
	 *  request. The callback instance will be alive until the implementation of
	 *  this class called one of the callback methods or until
	 *  OnAuthenticationCancelled() is called.
	 *
	 * @see OnAuthenticationCancelled()
	 * @see OpLoadingListener::OnAuthenticationRequired()
	 * @see OpLoadingListener::OnAuthenticationCancelled()
	 */
	virtual void OnAuthenticationRequired(OpAuthenticationCallback* callback) = 0;

	/** This method is called when a url authentication request is cancelled.
	 *
	 * A possible use-case is that an authentication is requested multiple times
	 * for the same url in different contexts. E.g. in addition to the
	 * OnAuthenticationRequired() the same url might be loaded in a window (see
	 * OpWindowCommander's OpLoadingListener::OnAuthenticationRequired()) or in
	 * another windowless context. If the user entered or cancelled the other
	 * authentication request before handling this authentication, then this
	 * method is called to cancel this authentication request.
	 *
	 * The implementation of this method should e.g. close the associated
	 * authentication dialog.
	 *
	 * @param callback is the authentication callback that was cancelled. This
	 *  callback instance shall no longer be used.
	 *
	 * @see OnAuthenticationRequired()
	 * @see OpLoadingListener::OnAuthenticationRequired()
	 * @see OpLoadingListener::OnAuthenticationCancelled()
	 */
	virtual void OnAuthenticationCancelled(const OpAuthenticationCallback* callback) = 0;
};


#ifdef GADGET_SUPPORT
/** @short Listener for gadget-management related events. */
class OpGadgetListener
{
public:
	typedef enum
	{
		/** Unknown update message. */
		GADGET_UNKNOWN,

		/** Update is available for this gadget. */
		GADGET_UPDATE,

		/** This gadget is to be disabled. */
		GADGET_DISABLE,

		/** This gadget is to be enabled. */
		GADGET_ACTIVATE,

		/** This gadget is to be deleted. */
		GADGET_DELETE,

		/** No update is available. */
		GADGET_NO_UPDATE
	} GadgetUpdateType;

	typedef enum
	{
		/** Unspecified or unsupported hash type. */
		GADGET_HASH_UNKNOWN,

		/** Hash is of type SHA1. */
		GADGET_HASH_SHA1,

		/** Has is of type MD5. */
		GADGET_HASH_MD5
	} GadgetHashType;

	typedef enum
	{
		/** Error while retrieving update XML document via HTTP. */
		GADGET_ERROR_LOADING_FAILED,

		/** Error while parsing retrieved XML document. */
		GADGET_ERROR_PARSING_FAILED,

		/** Gadgets' XML signature is invalid. */
		GADGET_ERROR_XML_SIGNATURE_INVALID
	} GadgetErrorType;

	struct OpGadgetUpdateData
	{
		GadgetUpdateType type;				///< What type of update this is (update, disable, activate, delete)
		const uni_char* id;					///< Widget id (not the same as widget uid, which is called id...), specified in config.xml
		const uni_char* src;				///< URI pointing to the updated widget.
		const uni_char* version;			///< The new version string.
		UINT32 bytes;						///< Size of the widget archive (or 0).
		BOOL mandatory;						///< This is a mandatory update.

		const uni_char* details;			///< Detail text as returned by the update server.
		const uni_char* href;				///< URI that could point to an additional information document.

		GadgetHashType hash_type;			///< Hash type (sha1 or md5)
		const uni_char* hash;				///< The hash

		OpGadgetClass* gadget_class;		///< Gadget class affected by an update operation. NULL if not yet installed.

#ifdef GOGI_GADGET_INSTALL_FROM_UDD
		/// URL of the Update Description Document with the source URI for the widget. Set if @a gadget_class is NULL.
		const uni_char* update_description_document_url;

		/// Value passed as @a user_data argument in OpGadgetManager::Update(). Set if @a gadget_class is NULL.
		const void* user_data;
#endif // GOGI_GADGET_INSTALL_FROM_UDD
	};

	struct OpGadgetErrorData
	{
		GadgetErrorType error;				///< Error type
		OpGadgetClass* gadget_class;		///< Gadget class affected by an update operation. NULL if not yet installed.

#ifdef GOGI_GADGET_INSTALL_FROM_UDD
		/// URL of the Update Description Document with the source URI for the widget. Set if @a gadget_class is NULL.
		const uni_char* update_description_document_url;
		const void* user_data;				///< Value passed as @a user_data argument in OpGadgetManager::Update().
#endif // GOGI_GADGET_INSTALL_FROM_UDD
	};

	struct OpGadgetDownloadData
	{
		const uni_char* caption;			///< Name or caption supplied by script.
		const uni_char* url;				///< Download url or NULL if not applicable.
		const uni_char* path;				///< Full path to the widget being installed or NULL if not applicable.
		const uni_char* identifier;			///< Widget UID (used by GADGET_DOWNLOAD_RUN_WIDGET)
	};

	struct OpGadgetInstallRemoveData
	{
		OpGadgetClass* gadget_class;        ///< Gadget class affected by operation.
	};

	struct OpGadgetInstanceData
	{
		OpGadget* gadget;					///< Gadget affected by operation.
	};

	struct OpGadgetStartStopUpgradeData
	{
		OpGadget* gadget;                   ///< Gadget instance affected by operation, contains the new gadget class.
		OpGadgetClass* gadget_class;
		/**<
		 * When a gadget is upgraded this contains the gadget class before the
		 * upgrade, for other operations it is @c NULL.
		 */
	};

	struct OpGadgetStartFailedData
	{
		OpGadgetClass* gadget_class;        ///< Class of the gadget being started.
		OpGadget* gadget;                   ///< Gadget instance that couldn't be started, NULL if not available.
	};

	struct OpGadgetSignatureVerifiedData
	{
		OpGadgetClass* gadget_class;             ///< Gadget class affected by the operation.
	};

	/** @short Gadget download permission notifier callback. */
	class GadgetDownloadPermissionCallback
	{
	public:
		virtual ~GadgetDownloadPermissionCallback() { }

		/** @short Tell core whether the previously permission to download a gadget was granted.
		 *
		 * @param allow TRUE to permit download, FALSE to deny.
		 */
		virtual void OnGadgetDownloadPermission(BOOL allow) = 0;
	};

	virtual ~OpGadgetListener() { }

	/** @short Called when new gadget update information is available. */
	virtual void OnGadgetUpdateReady(const OpGadgetUpdateData& data) = 0;

	/** @short Called if something went wrong while retrieving or parsing gadget update information. */
	virtual void OnGadgetUpdateError(const OpGadgetErrorData& data) = 0;

	/** @short Called when user permission is needed to download a gadget.
	 *
	 * @param data Information about the gadget
	 * @param callback Pointer to callback object. The method
	 * callback->GadgetDownloadPermissionCallback() must be called as soon as a
	 * decision to download or not has been made.
	 */
	virtual void OnGadgetDownloadPermissionNeeded(const OpGadgetDownloadData& data, GadgetDownloadPermissionCallback *callback) = 0;

	/** @short Gadget has been downloaded and is ready to be instantiated. */
	virtual void OnGadgetDownloaded(const OpGadgetDownloadData& data) = 0;

	/** @short Gadget installation has been initiated.
	  *
	  * Gadgets of this class can be opened but the operation will be
	  * asynchronous and gadgets will be opened after the installation has
	  * finished.
	  */
	virtual void OnGadgetInstalled(const OpGadgetInstallRemoveData& data) = 0;
	virtual void OnGadgetRemoved(const OpGadgetInstallRemoveData& data) = 0;
	virtual void OnGadgetUpgraded(const OpGadgetStartStopUpgradeData& data) = 0;

	/** @short Gadget signature has been verified.
	  *
	  * This callback is invoked regardless of the outcome of the verification.
	  * data.gadget_class may be used to retrieve information about the gadget
	  * class' sign state.
	  */
	virtual void OnGadgetSignatureVerified(const OpGadgetSignatureVerifiedData& data) = 0;

	/** @short Instantiate gadget. */
	virtual void OnRequestRunGadget(const OpGadgetDownloadData& data) = 0;

	/** @short Gadget instance has been started. */
	virtual void OnGadgetStarted(const OpGadgetStartStopUpgradeData& data) = 0;
	/** @short An error occured when starting a gadget instance. */
	virtual void OnGadgetStartFailed(const OpGadgetStartFailedData& data) = 0;
	virtual void OnGadgetStopped(const OpGadgetStartStopUpgradeData& data) = 0;

	/** @short A new gadget instance was created */
	virtual void OnGadgetInstanceCreated(const OpGadgetInstanceData& data) = 0;
	/** @short A gadget instance was removed */
	virtual void OnGadgetInstanceRemoved(const OpGadgetInstanceData& data) = 0;

	/** @short Called if something went wrong while downloading the gadget. */
	virtual void OnGadgetDownloadError(const OpGadgetErrorData& data) = 0;
};
#endif // GADGET_SUPPORT

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
/**
 * This class implements an interface that allows core to
 * query the platform for information regarding search engines.
 */
class OpSearchProviderListener
{
public:
	/**
	 * This enumeration represents the motives why OnRequestSearchProviderInfo
	 * might be called
	 */
	enum RequestReason
	{
		/**
		 * Request search engine info for OpErrorPage.
		 */
		REQUEST_ERROR_PAGE = 1
	};

	/**
	 * This class represents neatly organized information about a
	 * search engine that will be used by core.
	 * Both m_query and m_url might contains the %s token, which will
	 * should be recognized by all the places in core that use this information
	 */
	class SearchProviderInfo
	{
	public:

		SearchProviderInfo(){}

		/**
		 * Human readable name that identifies the search engine.
		 * Can be NULL
		 * Example: Google Search, Ask.com
		 */
		virtual const uni_char* GetName() = 0;

		/**
		 * Url for the search. Might contain query arguments.
		 * Must NOT be NULL
		 */
		virtual const uni_char* GetUrl() = 0;

		/**
		 * Data that should be posted if the request is a POST request
		 * Can be NULL
		 * Note: the name of field that holds the search terms must not be here
		 */
		virtual const uni_char* GetQuery() = 0;

		/**
		 * Url to file with icon used for the search engine.
		 * Can be NULL. Can point be a file or http url, or
		 * can be the name of a skin icon. All should be handled
		 */
		virtual const uni_char* GetIconUrl() = 0;

		/**
		 * Encoding to be used when making request
		 * Can be NULL. The default encoding will be used.
		 */
		virtual const uni_char* GetEncoding() = 0;

		/**
		 * Tells if the search should do a post request, else do a GET
		 */
		virtual BOOL IsPost() = 0;

		/**
		 * Returns number of search hits. This value will replace
		 * the %i token in the url of the search engine.
		 * Return 0 for it to be ignored.
		 */
		virtual int GetNumberOfSearchHits() = 0;

		/**
		 * After platforms having returned a OpSearchProviderListener
		 * to core, core disposes of the object calling this method.
		 * If platforms create instances for each call to OnRequestSearchProviderInfo
		 * then this should clean up, else if platforms return a singleton
		 * object, then this method can be a no-op.
		 * Platforms are encouraged to use singleton objects, considering
		 * this information rarely mutates and it's fairly static
		 */
		virtual void Dispose() = 0;

		/**
		 * Returns the global identifier of the search entry
		 */
		virtual const uni_char* GetGUID() = 0;

		/**
		 * Returns TRUE if the search provider supports collecting
		 * search suggestions
		 */
		virtual BOOL ProvidesSuggestions() = 0;
	};

	/**
	 * Utility class to auto-Dispose a SearchProviderInfo object
	 */
	class SearchProviderInfoAnchor
	{
		SearchProviderInfo*& m_ptr;
	public:
		SearchProviderInfoAnchor(SearchProviderInfo*& ptr) : m_ptr(ptr){}
		~SearchProviderInfoAnchor(){ if (m_ptr != NULL){ m_ptr->Dispose(); m_ptr = NULL; } }
	};

	OpSearchProviderListener(){}
	virtual ~OpSearchProviderListener(){}

	/**
	 * This function will be called when core requests information about search
	 * providers from platforms. Any call to this function must gracefully handle
	 * NULL when returned. After being done with the SearchProviderInfo object,
	 * core will call SearchProviderInfo::Dispose().
	 * Naive implementations can just return NULL.
	 *
	 * @param reason         motive why this function is being called. Check
	 *                       OpSearchProviderListener::RequestReason for the
	 *                       possible values
	 *
	 * @return object with search engine info
	 */
	virtual SearchProviderInfo* OnRequestSearchProviderInfo(RequestReason reason) = 0;
};
#endif //SEARCH_PROVIDER_QUERY_SUPPORT

#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
class OpSSLListener;
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER

#if defined SUPPORT_DATA_SYNC && defined CORE_BOOKMARKS_SUPPORT
/** @short Listener for data sync events.
 *
 * This listener is called when synchronizing the client's data (bookmarks,
 * speeddial, ...) with the Opera Link server requires some user-feedback:
 * - OnSyncLogin() is called, when the client needs user credentials to log in
 *   to the Opera Link server.
 * - OnSyncError() is called, when a sync related error has occurred.
 *
 * The UI which wants to support sync, needs to implement this class and
 * register an instance of this class by calling
 * @code
 *  g_windowCommanderManager->SetDataSyncListener();
 * @endcode
 */
class OpDataSyncListener
{
public:
	virtual ~OpDataSyncListener() {}

	/** @short Sync user credentials context.
	 *
	 * An instance of this class is passed as argument to
	 * OpDataSyncListener::OnSyncLogin(). The UI implementation of
	 * OpDataSyncListener shall call this callback (i.e. either
	 * OnSyncLoginDone() or OnSyncLoginCancel()) when the user has provided the
	 * login information or cancelled the login dialog.
	 */
	class SyncCallback
	{
	public:
		virtual ~SyncCallback() {}

		/** @short User credentials were provided.
		 *
		 * Called by the implementation of OpDataSyncListener when  the user
		 * provided login information.
		 * @param username is the username of the sync login.
		 * @param password is the password of the sync login.
		 */
		virtual void OnSyncLoginDone(const uni_char* username, const uni_char* password) = 0;

		/** @short User credentials were not provided.
		 *
		 * Called by the implementation of OpDataSyncListener when the user
		 * cancelled the login.
		 */
		virtual void OnSyncLoginCancel() = 0;
	};

	/** @short Ask the user about login information for the sync login.
	 *
	 * This method is called by core, when it is about to log in to the Opera
	 * Link server. The implementation of this class can e.g. display a login
	 * dialog to ask the user for username and password.
	 *
	 * When the user has entered the information
	 * SyncCallback::OnSyncLoginDone() shall be called.
	 *
	 * When the user cancelled the login dialog,
	 * SyncCallback::OnSyncLoginCancel() shall be called.
	 *
	 * @param callback is the callback class that shall be called by the
	 *  implementation of this class when the user has entered the login
	 *  information or cancelled the login.
	 */
	virtual void OnSyncLogin(SyncCallback* callback) = 0;

	/** @short A sync-related error occurred.
	 *
	 * An implementation of this class may e.g. display an error message dialog
	 * with the specified error message.
	 *
	 * @param error_msg The error message describing what went wrong.
	 */
	virtual void OnSyncError(const uni_char* error_msg) = 0;
};
#endif // SUPPORT_DATA_SYNC && CORE_BOOKMARKS_SUPPORT

#ifdef WEB_TURBO_MODE
/** Listener used to report usage of Opera Turbo. */
class OpWebTurboUsageListener
{
public:
	typedef enum {
		NORMAL,
		TEMPORARILY_INTERRUPTED,
		INTERRUPTED
	} TurboUsage;

	virtual ~OpWebTurboUsageListener() {}

	/** Usage has changed. */
	virtual void OnUsageChanged(TurboUsage status) = 0;
};
#endif // WEB_TURBO_MODE

#ifdef PI_SENSOR
/**
 * A listener for handling a sensor calibration request.
 *
 * If a UI wants to supports sensor calibration, it shall implement this listener
 * class and call OpWindowCommanderManager::SetSensorCalibrationListener().
 *
 * The listener's OnSensorCalibrationRequest() method is called by core when the system
 * notifies Opera that a compass-calibration may be needed to increase
 * the accuracy of the device-orientation data and the document does not
 * provide its own calibration UI.
 *
 * The UI can implement the calibration request, e.g., by loading the system's
 * native calibration application.
 * Core does not need any notification about the calibration-result.
 *
 * @see http://dev.w3.org/geo/api/spec-source-orientation.html#compassneedscalibration
 */
class OpSensorCalibrationListener
{
public:
	virtual ~OpSensorCalibrationListener() {}
	/** Requests calibration for a OpSensor.
	 *
	 * @param sensor the OpSensor that requires calibration
	 */
	virtual void OnSensorCalibrationRequest(OpSensor *sensor) = 0;
};
#endif // PI_SENSOR

class OpWindowCommanderManager
{
public:
	virtual ~OpWindowCommanderManager() {}

	static OpWindowCommanderManager* CreateL();

	/**
	   Creates a windowcommander.
	   @param windowCommander return parameter that contains the windowcommander on success
	   @return OpStatus::OK on success, appropriate error message otherwise. out of memory conditions are handled internally, return value is for use in the caller only.
	*/
	virtual OP_STATUS GetWindowCommander(OpWindowCommander** windowCommander) = 0;
	/** Releases the windowcommander passed in as a parameter. This method may only be called once on each windowcommander.
		After this call has returned the windowCommander to be considered destroyed.
	*/
	virtual void ReleaseWindowCommander(OpWindowCommander* windowCommander) = 0;

	// listener dpt:
	virtual void SetUiWindowListener(OpUiWindowListener* listener) = 0;
	virtual OpUiWindowListener* GetUiWindowListener() = 0;

#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	/**
		Set the listener for certificate dialogs that does not explicitly belong to a windowcommander
		this is the same abstract listener that is declared in OpWindowCommander, but when used in this
		context, the windowcommander argument will always be NULL
	*/
	virtual void SetSSLListener(OpSSLListener* listener) = 0;
	virtual OpSSLListener* GetSSLListener() = 0;
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER

#ifdef WINDOW_COMMANDER_TRANSFER
	/**
	   @return the singleton transfermanager object. creates it if it doesn't exist. returns NULL on failure.
	*/
	virtual OpTransferManager* GetTransferManager() = 0;  // should be moved to the "core object" later.
#endif // WINDOW_COMMANDER_TRANSFER

#ifdef APPLICATION_CACHE_SUPPORT
	/**
	 * Class used for describing an installed application cache.  Will be returned
	 * when requesting vector of all application caches, or sent as a parameter when
	 * deleteting an application cache.
	 */
	class OpApplicationCacheEntry {
	public:
		virtual ~OpApplicationCacheEntry() {}

		/**
		 * @return  Returns the Domain of the domain from which this application cache was installed
		 *          There may be several ApplicationCaches for the same domain, but their manifest
		 *          URL will always be different.
		 */
		virtual const uni_char* GetApplicationCacheDomain() const = 0;

		/**
		 * @return  Returns the URL of the manifest which describes this application cache.
		 *          This information is unlikely to be interesting to anyone but web developers.
		 */
		virtual const uni_char* GetApplicationCacheManifestURL() const = 0;

		/**
		 * @return the current disk quota in bytes for the cache this entry uses.  Note that
		 * 		   other applications may share the cache (if they have the same
		 * 		   manifest URL).  0 means no limit.
		 */
		virtual OpFileLength GetApplicationCacheDiskQuota() const = 0;

		/**
		 * @return the current disk use in bytes for the cache this entry uses.  Note that
		 * 		   other applications may share the cache (if they have the same
		 * 		   manifest URL), and hence all disk space isn't necessaritly used
		 * 		   by this application.
		 */
		virtual OpFileLength GetApplicationCacheCurrentDiskUse() const = 0;
	};

	/**
	 * Fills a vector with an OpApplicationCacheEntry for each application cache
	 * which is currently installed. Only the latest version for each cache is used.
	 * @all_app_caches  A vector which will be filled.  It's the callers responsibility to delete the
	 * 					elements of the vector (OpAutoVector may be used if you don't want to care about it).
	 * 					Even when the return value is OUT_OF_MEMORY some values may have been filled
	 * 					and need to be deleted.
	 * 					Order of entries is arbitrary.
	 * @return OK or OUT_OF_MEMORY
	 */
	virtual OP_STATUS GetAllApplicationCacheEntries(OpVector<OpApplicationCacheEntry>& all_app_caches) = 0;

	/**
	 * Deletes a specific application cache
	 * @param manifest_url reference to the cache to be deleted. get url from from OpApplicationCacheEntry::GetApplicationCacheManifestURL()
	 * @return OK or OUT_OF_MEMORY
	 */
	virtual OP_STATUS DeleteApplicationCache(const uni_char *manifest_url) = 0;

	/**
	 * Deletes all installed application caches.
	 * @return OK or OUT_OF_MEMORY
	 */
	virtual OP_STATUS DeleteAllApplicationCaches() = 0;

#endif // APPLICATION_CACHE_SUPPORT

	/** sets the authentication listener for urls that don't load in a window. this code will be subjected to redesign but is needed now. */
	virtual void SetAuthenticationListener(OpAuthenticationListener* listener) = 0;  // should be moved to the "core object" later.

	virtual void SetIdleStateChangedListener(OpIdleStateChangedListener* listener) = 0;
	virtual OpIdleStateChangedListener* GetIdleStateChangedListener() = 0;

	virtual void SetOomListener(OpOomListener* listener) = 0;
	virtual OpOomListener* GetOomListener() = 0;

#if defined SUPPORT_DATA_SYNC && defined CORE_BOOKMARKS_SUPPORT
	virtual void SetDataSyncListener(OpDataSyncListener* listener) = 0;
	virtual OpDataSyncListener* GetDataSyncListener() = 0;
#endif // SUPPORT_DATA_SYNC && CORE_BOOKMARKS_SUPPORT

#ifdef WEB_TURBO_MODE
	virtual void SetWebTurboUsageListener(OpWebTurboUsageListener* listener) = 0;
	virtual OpWebTurboUsageListener* GetWebTurboUsageListener() = 0;
#endif // WEB_TURBO_MODE

#ifdef PI_SENSOR
	virtual void SetSensorCalibrationListener(OpSensorCalibrationListener* listener) = 0;
	virtual OpSensorCalibrationListener* GetSensorCalibrationListener() = 0;
#endif //PI_SENSOR

#ifdef GADGET_SUPPORT
	virtual void SetGadgetListener(OpGadgetListener* listener) = 0;
	virtual OpGadgetListener* GetGadgetListener() = 0;
#endif // GADGET_SUPPORT

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	virtual void SetSearchProviderListener(OpSearchProviderListener* listener) = 0;
	virtual OpSearchProviderListener* GetSearchProviderListener() = 0;
#endif //SEARCH_PROVIDER_QUERY_SUPPORT

#ifdef WIC_TAB_API_SUPPORT
	/** Set listener for extension Tab API events */
	virtual void SetTabAPIListener(OpTabAPIListener* listener) = 0;
	virtual OpTabAPIListener* GetTabAPIListener() = 0;
#endif // WIC_TAB_API_SUPPORT

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	virtual PersistentStorageCommander* GetPersistentStorageCommander() = 0;
#endif //DATABASE_MODULE_MANAGER_SUPPORT

	typedef enum
	{
		WIC_OPERA_CLEAR_ALL              = -1
		,WIC_OPERA_CLEAR_VISITED_LINKS   = 1 << 0
		,WIC_OPERA_CLEAR_DISK_CACHE      = 1 << 1
		,WIC_OPERA_CLEAR_IMAGE_CACHE     = 1 << 2
		,WIC_OPERA_CLEAR_MEMORY_CACHE    = 1 << 3
		,WIC_OPERA_CLEAR_SENSITIVE_DATA  = 1 << 4
		,WIC_OPERA_CLEAR_SESSION_COOKIES = 1 << 5
		,WIC_OPERA_CLEAR_ALL_COOKIES     = 1 << 6
		,WIC_OPERA_CLEAR_GLOBAL_HISTORY  = 1 << 7
		,WIC_OPERA_CLEAR_CONSOLE         = 1 << 8
		,WIC_OPERA_CLEAR_THUMBNAILS      = 1 << 9
		,WIC_OPERA_CLEAR_WEBDATABASES    = 1 << 10
		,WIC_OPERA_CLEAR_WEBSTORAGE      = 1 << 11
		,WIC_OPERA_CLEAR_APPCACHE        = 1 << 12
		,WIC_OPERA_CLEAR_GEOLOCATION_PERMISSIONS = 1 << 13
		,WIC_OPERA_CLEAR_SITE_PREFS      = 1 << 14
		,WIC_OPERA_CLEAR_PLUGIN_DATA     = 1 << 15
		,WIC_OPERA_CLEAR_CORS_PREFLIGHT  = 1 << 16
		,WIC_OPERA_CLEAR_CAMERA_PERMISSIONS = 1 << 17
		,WIC_OPERA_CLEAR_EXTENSION_STORAGE = 1 << 18

	} ClearPrivateDataFlags;

	/**
	 * Clears the specified parts of private data.
	 *
	 * @param options Specifies which kind of private data to
	 *  clear. The flags can be combined with bitwise or of
	 *  the values from the enum ClearPrivateDataFlags.
	 */
	virtual void ClearPrivateData(int option_flags) = 0;

#ifdef MEDIA_PLAYER_SUPPORT
	/** Checks if the media content of the specified handle is associated with a video context.
	 *
	 *  @param[in] handle Handle to the core media resource.
	 *  @return TRUE if the handle is associated with a video context (e.g. HTML/SVG <video>), FALSE otherwise.
	 */
	virtual BOOL IsAssociatedWithVideo(OpMediaHandle handle) = 0;
#endif //MEDIA_PLAYER_SUPPORT
};

#endif // OP_WINDOW_COMMANDER_MANAGER_H
