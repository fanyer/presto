/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file OpWindowCommander.h
 *
 * Interface to core that can be used by user interfaces.
 *
 * @author rikard@opera.com
*/

/**
 * @mainpage
   @section core ON THE CORE SIDE

   WindowCommander consists of the following parts/roles on the core side:

   OpWindowCommanderManager -- exported by core so that an "outsider"
   (= ui implementation) can get hold of an OpWindowCommander.

   OpWindowCommander -- a representation of a core window towards the
   outside. Allows manipulation ("commanding") of the core
   representation of a window (not to be confused with a Window as
   known in core, there is no 1-1 mapping)

   There is also a set of listeners that allow the ui/ui window to get
   notifications from core. These are interfaces that are to be
   implemented on the ui side if the ui needs the particular type of
   notifications.

   OpWindow -- not strictly a part of the WindowCommander kit, but it
   has a role to play: It is the glue between the platform widget
   Opera's hierarchy of display classes that will live within it. Note
   that an OpWindow doesn't need to be a decorated window, or even a
   window at all. One can easily think of scenarios where the
   OpWindows are panels, or even something else.

   @section ui ON THE UI SIDE

   There are also a role that is needed on the ui side, namely:

   OpUiWindowListener -- this is an interface that needs to be
   implemented so that core can tell someone that a new window is
   asked for from the inside, for example from a script. The ui can
   then determine if and how to instantiate the window. This listener
   is set on OpWindowCommanderManager. More about this in the HOWTO
   section.

   @section diagram Class diagram
   @dotfile windowcommander.dot "WindowCommander overview."

   @section howtos HOWTOs

   HOWTO start using:
     - Implement OpUiWindowListener and set the listener on g_windowCommanderManager (created in initcore.cpp)
	 - Implement OpWindow
	 - Implement your ui window. WindowCommander puts no restriction on this.
	 - Implement the listeners that you need from this file, and set
	   them. You don't have to implement those that you don't need.

   HOWTO create a new window that is triggered from the ui:
     - Call g_windowCommanderManager->GetWindowCommander(OpWindowCommander**) to get an OpWindowCommander.
	 - Create an OpWindow.
	 - Couple the OpWindowCommander and the OpWindow by calling yourWindowCommander->OnUiWindowCreated(yourOpWindow);
	 - Optional: Set the listeners that you want.

   HOWTO handle popup windows, triggered from core (typically javascript):
     - g_windowCommanderManager->GetUiWindowListener()->CreateUiWindow(windowCommander) gets called from core.
	 - The implementation of OpUiWindowListener decides whether it wants to create the window or not, and how.
	 - It then creates an OpWindow, couples it with the OpWindowCommander that was passed and sets the listeners.
	 - Note: the procedure is identical to the case where the ui
	   itself triggers the window creation, except for the first step
	   where the OpWindowCommander is created. In the ui triggered case
	   it is asked for by the ui, in the core triggered case it is
	   pre-created by core.

   HOWTO handle closing of windows:
     - Call yourWindowCommander->OnUiWindowClosing();
	 - Call g_windowCommanderManager->ReleaseWindowCommander(yourWindowCommander);
	 - Destroy your OpWindow and do whatever other cleanup that is
	   needed on the ui side, like removing toolbars, closing container
	   windows etc.

	   NOTE: A window can not safely be closed by the UI when the UI
		     is called by core, unless the call is to a message
		     handler registered by the UI.  In other words, to safely
		     close a window, the UI should wait until the next time
		     Opera::Run() returns or post a message to itself and
		     close the window from the message handler.

   HOWTO handle popup menus:
     - Implement OpMenuListener and set it on the OpWindowCommander.
	 - OnPopupMenu will be called on certain core events (see the OpMenuListener for documentation).
	 - OnMenuDispose will be called when core recommends the ui to dispose of the menu.

	 - Note that a menu doesn't need to be a menu in the widget sense
	   -- one can think of a menu that is being displayed in a separate
	   panel on the side of the browser component.

   HOWTO handle status information and tooltips:
     - Implement OpDocumentListener.
	 - OnHover will be called when the mouse hovers over a link.
	 - OnNoHover will be called when the mouse doesn't hover anymore.
	 - OnLinkNavigated is called when a link is navigated to.
	 - OnNoNavigation is called when the navigation is reset.

   HOWTO handle resizing:
     - When triggered from the ui: just call yourOpWindow->SetSize(...);
	 - When triggered from core (javascript),
	   OpDocumentListener::OnResize(...) will be called. The ui then
	   decides whether it wants to/can resize the window to meet the
	   wish. It should then resize the OpWindow accordingly, and
	   everything will be handled perfectly well. */

#ifndef OP_WINDOW_COMMANDER_H
#define OP_WINDOW_COMMANDER_H

#include "modules/windowcommander/wic_capabilities.h"
#include "modules/windowcommander/OpTransferManager.h"
#include "modules/windowcommander/OpCookieManager.h"
#include "modules/windowcommander/WritingSystem.h"

#ifdef SCOPE_URL_PLAYER
/** Import listener for urlplayer. */
#include "modules/windowcommander/src/WindowCommanderUrlPlayerListener.h"
#endif // SCOPE_URL_PLAYER

#include "modules/hardcore/base/op_types.h"
#include "modules/hardcore/base/opstatus.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/hardcore/keys/opkeys.h"
#include "modules/pi/network/OpCertificate.h"
#include "modules/pi/OpWindow.h"

#include "modules/url/url2.h"

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
#include "modules/display/vis_dev_transform.h"
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

#include "modules/windowcommander/src/generated/g_message_windowcommander_messages.h"

class Window;
class OpView;
class OpWindowCommander;
class OpBitmap;
class VisualDevice;
class OpViewportController;
class OpGadget;
class OpDocumentContext;
#ifdef EXTENSION_SUPPORT
class OpExtensionUIListener;
#endif // EXTENSION_SUPPORT
#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
class OpSelftestListener;
#endif // GOGI_SELFTEST_FINISHED_NOTIFICATION
#ifdef XML_AIT_SUPPORT
class AITData;
#endif // XML_AIT_SUPPORT
#ifdef SECURITY_INFORMATION_PARSER
class OpSecurityInformationParser;
#endif // SECURITY_INFORMATION_PARSER
#ifdef TRUST_INFO_RESOLVER
class OpTrustInformationResolver;
class OpTrustInfoResolverListener;
#endif // TRUST_INFO_RESOLVER
class OpMailClientListener;

/** @short Bitmask used for Widgets 1.0: View Modes.
 * http://www.w3.org/TR/view-mode/
 *
 * Some of the modes are marked as LEGACY and they only apply to
 * old Opera widgets modes. They duplicate the same functionality as
 * their W3C counterparts, but are still needed to keep compatibility
 * with the client code still using them. Still expect them to disappear
 * sooner or later and don't use them unless dealing with with legacy
 * opera widgets.
 *
 * This is not ifdef-ed GADGET_SUPPORT as we apply this to
 * normal windows and not only to gadgets.
 */
enum WindowViewMode
{
	WINDOW_VIEW_MODE_UNKNOWN			= 0x001,	///< Unknown
	WINDOW_VIEW_MODE_WIDGET				= 0x002,	///< Same as floating. LEGACY
	WINDOW_VIEW_MODE_FLOATING			= 0x004,	///< No chrome.
	WINDOW_VIEW_MODE_DOCKED				= 0x008,	///< Same as minimized. LEGACY
	WINDOW_VIEW_MODE_APPLICATION		= 0x010,	///< Same as windowed. LEGACY
	WINDOW_VIEW_MODE_FULLSCREEN			= 0x020,	///< Fullscreen (no chrome, occupies the entire screen).
	WINDOW_VIEW_MODE_WINDOWED			= 0x040,	///< With chrome and not occupying the entire screen.
	WINDOW_VIEW_MODE_MAXIMIZED			= 0x080,	///< With chrome and occupying the entire screen.
	WINDOW_VIEW_MODE_MINIMIZED			= 0x100		///< Docked or minimized, but with a dynamic grapical representation.
};

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
/**
 * OpSearchSuggestionsCallback allows setting up search suggestions
 * as a list. The list must be sets of {url},{name},{url},{name},...
 *
 * The caller creates an OpSearchSuggestionsCallback object and calls
 * OpLoadingListener::OnSearchSuggestionsRequested() with the object
 * as parameter. A reply (if any) must be retuned in @ref OnDataReceived
 * by the generator object.
 *
 * The generator object should install itself as a listener to the callback
 * so that it will be notified via @ref OnDestroyed() should the callback
 * be made invalid.
 */
class OpSearchSuggestionsCallback
{
public:
	class Listener
	{
	public:
		/**
		 * Sent to a listener of the callback when the callback is destroyed
		 *
		 * @param callback The callback that is destroyed
		 */
		virtual void OnDestroyed(OpSearchSuggestionsCallback* callback) = 0;
	};

public:
	/**
	 * Creates and initializes the callback
	 */
	OpSearchSuggestionsCallback(): m_listener(0) {}

	/**
	 * Destroys the callback. A listener will be notified via @ref OnDestroyed
	 */
	~OpSearchSuggestionsCallback() { if(m_listener) { m_listener->OnDestroyed(this); } }

	/**
	 * The generator object sets itself as this listener to be notified if the
	 * callback is invalidated by the caller.
	 *
	 * @param listener The listener object
	 */
	void SetListener(Listener* listener) { m_listener = listener; }

	/**
	 * Set the search result outcome represented as a list of {url},{name} entries
	 *
	 * @param list Search suggestions. The list count must be an even number. 2 or more.
	 *
	 * @return OpStatus::OK on success, otherwise an error code describing the problem
	 */
	virtual OP_STATUS OnDataReceived(OpVector<OpString>& list) = 0;

protected:
	Listener* m_listener;
};
#endif

class LoadingInformation
{
public:
	LoadingInformation():loadedImageCount(0),totalImageCount(0),loadedBytes(0),totalBytes(0),loadedBytesRootDocument(0),totalBytesRootDocument(0),loadingMessage(NULL), isUploading(FALSE)
#ifdef WEB_TURBO_MODE
		,turboTransferredBytes(0), turboOriginalTransferredBytes(0)
#endif
		,hasRequestedDocument(FALSE)
	{}
	virtual ~LoadingInformation() {}
	virtual void Reset()
	{
		loadedImageCount = 0; totalImageCount = 0; loadedBytes = 0; totalBytes = 0; loadedBytesRootDocument = 0; totalBytesRootDocument = 0; loadingMessage = NULL; isUploading = FALSE;
#ifdef WEB_TURBO_MODE
		turboTransferredBytes = 0; turboOriginalTransferredBytes = 0;
#endif // WEB_TURBO_MODE
		hasRequestedDocument = FALSE;
	}
	/** Downloaded inlines so far. If isUploading is TRUE, then this is the total number of files uploaded so far. */
	INT32 loadedImageCount;
	/** Total number of inlines. If isUploading is TRUE, then this is the total number of files to upload. */
	INT32 totalImageCount;
	OpFileLength loadedBytes;
	OpFileLength totalBytes;
	OpFileLength loadedBytesRootDocument;
	OpFileLength totalBytesRootDocument;
	const uni_char* loadingMessage;
	BOOL isUploading;
#ifdef WEB_TURBO_MODE
	OpFileLength turboTransferredBytes;
	OpFileLength turboOriginalTransferredBytes;
#endif // WEB_TURBO_MODE
	/** Flag which indicates if data for a new root document is
	 * being or has been loaded, i.e. if the current loading
	 * session aims to, or has, replace the root document. */
	BOOL hasRequestedDocument;
	// possibly more here
};

enum DocumentURLType {
	DocumentURLType_Unknown,
	DocumentURLType_FTP,
	DocumentURLType_HTTP,
	DocumentURLType_HTTPS,
	DocumentURLType_Other
};

/**
 * The abstract interface OpAuthenticationCallback is used to store information
 * about an authentication request and provide a possibility for the UI to
 * answer that request. The callback instance is passed to
 * OpAuthenticationListener::OnAuthenticationRequired() or
 * OpLoadingListener::OnAuthenticationRequired().
 */
class OpAuthenticationCallback
{
public:
	enum AuthenticationType {
		/**
		 * This type is used for authenticating users to a server.
		 */
		AUTHENTICATION_NEEDED = 0,

		/**
		 * This type is used for authenticating users to a server when
		 * a previous authentication to the server has failed. This
		 * may be the case when the user has provided wrong
		 * credentials.
		 */
		AUTHENTICATION_WRONG,

		/**
		 * This type is used for authenticating users to a proxy
		 * server.
		 *
		 * @note In principle a client could be asked to authenticate
		 *  itself to both a proxy and an end-server, but never in the
		 *  same request.
		 */
		PROXY_AUTHENTICATION_NEEDED,

		/**
		 * This type is used for authenticating users to a proxy
		 * server when a previous authentication to the proxy  server
		 * has failed. This may be the case when the user has provided
		 * wrong credentials.
		 */
		PROXY_AUTHENTICATION_WRONG
	};

	enum AuthenticationSecurityLevel {
		/**
		 * Plain text (even when encypted).
		 */
		AUTH_SEC_PLAIN_TEXT = 0,

		/**
		 * Some security, Cryptographical authentication, but low
		 * connection security.
		 */
		AUTH_SEC_LOW = 1,

		/**
		 * Better security, Cryptographical authentication, with good
		 * connection security.
		 */
		AUTH_SEC_GOOD = 2
	};

	virtual ~OpAuthenticationCallback() {}

	/**
	 * Returns the type of the authentication-request.
	 */
	virtual AuthenticationType GetType() const = 0;

	/**
	 * Returns the 8-bit version of the server's name. This is the
	 * name of the server that requests authentication.
	 */
	virtual const char*	ServerNameC() const = 0;

	/**
	 * Returns the unicode version of the server's name. This is the
	 * name of the server that requests authentication.
	 */
	virtual const uni_char* ServerUniName() const = 0;
	
	virtual int GetSecurityMode() const = 0; 				///< returns the security mode
	virtual UINT32 SecurityLowStatusReason() const = 0;

#ifdef TRUST_RATING
	virtual int GetTrustMode() const = 0; 					///< returns the trust mode
#endif
	virtual int GetWicURLType() const = 0;

#if defined SECURITY_INFORMATION_PARSER || defined TRUST_INFO_RESOLVER
	/** @short Create URLInformation object for auth info url.
	 *
	 * Returns a heavy weight object for more advanced interaction with the
	 * address.
	 *
	 * @param[out] url_info If the method returns OpStatus::OK, then this is
	 * the created url_info object. Is owned by the caller and should be freed
	 * with OP_DELETE.
	 *
	 * @return ERR if there was no address or ERR_NO_MEMORY if an out of memory
	 * situation occurred. If the object was created correctly, OK will be
	 * returned.
	 */
	virtual OP_STATUS CreateURLInformation(URLInformation** url_info) const = 0;
#endif // SECURITY_INFORMATION_PARSER || TRUST_INFO_RESOLVER


	/**
	 * Returns the last used user name that was used for
	 * authentication.
	 */
	virtual const uni_char*	LastUserName() const = 0;

	/**
	 * Returns the username found in the url: username@www...
	 */
	virtual const char* GetUserName() const = 0;

	/**
	 * Returns the url without user name and password, escaped
	 * sequences are preserved. On loading this url, the server
	 * requested authentication.
	 */
	virtual const uni_char*	Url() const = 0;

	/**
	 * Returns the actual port the url will access.
	 */
	virtual UINT16 Port() const = 0;

	/**
	 * Returns the URL_ID of the associated url.
	 */
	virtual URL_ID UrlId() const = 0;

	/**
	 * Returns the url type of the url.
	 */
	virtual DocumentURLType GetURLType() const = 0;

	/**
	 * Returns the security level of the connection. This can be used
	 * to determine if the password will be sent as plain text
	 * (AUTH_SEC_PLAIN_TEXT) or if there is a low or good connection
	 * security.
	 */
	virtual enum AuthenticationSecurityLevel SecurityLevel() const = 0;

	/**
	 * Returns the current authentication description (like: "Basic
	 * Authentication: Clear text").
	 */
	virtual const uni_char* AuthenticationName() const = 0;

	/**
	 * Returns the utf-8 encoded authentication message that is
	 * specified on the server.
	 *
	 * E.g. the apache server can specify in its configuration file
	 * (e.g. \c .htaccess) a message by the directive
	 * @code
	 * AuthName "message"
	 * @endcode
	 */
	virtual const char* Message() const = 0;

#ifdef WAND_SUPPORT
	/**
	 * Returns the suggested wand-id for the authentication request. This
	 * wand-id can be used to find and store user-credentials for this url in
	 * wand.
	 *
	 * The format of the wand-id depends on the #type of authentication:
	 * - For a proxy authentication (PROXY_AUTHENTICATION_NEEDED or
	 *   PROXY_AUTHENTICATION_WRONG) the wand-id is "<ServerUniName>:<Port>".
	 * - For normal authentication (AUTHENTICATION_NEEDED or
	 *   AUTHENTICATION_WRONG) the wand-id is "*<url>" where "<url>" is the url
	 *   without the query parameters.
	 *
	 * @note The "*" in the wand-id means that on using WandManager::FindLogin()
	 *  with this wand-id the wand-manager will find all user-credentials in
	 *  wand that have the same protocol+server-name as the specified url. And
	 *  storing the user-credentials in wand with a wand-id that starts with "*"
	 *  means that the stored credentials will be found if you call
	 *  WandManager::FindLogin() with a wand-id which does not start with a "*",
	 *  but has the same protocol+server-name as the stored wand-id.
	 *
	 * @see WandManager::FindLogin()
	 * @see WandManager::StoreLogin()
	 */
	virtual const uni_char* WandID() const = 0;

	/**
	 * This enum defines possible actions for Authenticate(). The default action
	 * is WAND_NO_CHANGES, i.e. Authenticate() will neither store nor delete the
	 * specified user-name,password pair in wand. If the application wants to
	 * store or remove a user-name,password pair, it needs to call
	 * SetWandAction() with one of the action values.
	 */
	enum WandAction {
		/** Don't change any wand-entry on handling the Authenticate() call. */
		WAND_NO_CHANGES = 0,

		/** Store the user-name,password pair that is specified in the
		 * Authenticate() call in wand, using either the default wand-id (as
		 * returned by WandID() or the wand-id specified in SetWandOption().
		 * If there is already a wand-entry for that wand-id,user-name pair,
		 * the corresponding password will be overwritten. */
		WAND_STORE_LOGIN = 1,

		/** If wand has a login for the specified wand-id,user-name pair, then
		 * Authenticate() will remove that entry from wand. */
		WAND_DELETE_LOGIN = 2
	};

	/**
	 * Specify if Authenticate() shall store or delete the specified
	 * user-name,password pair in wand or if Authenticate() shall not change
	 * anything.
	 *
	 * @param action Is the WandAction for Authenticate().
	 * @param wand_id Specifies the wand-id which shall be used to store or
	 *  delete the user-name,password pair. If the value is 0, then the default
	 *  wand-id (as returned by WandID()) is used. For the format of the string
	 *  see WandId(). E.g. in case of a normal authentication
	 *  (AUTHENTICATION_NEEDED or AUTHENTICATION_WRONG) the UI may decide to not
	 *  use a wand-id starts with an "*" and set this argument to WandId()+1.
	 */
	virtual void SetWandAction(enum WandAction action, const uni_char* wand_id) = 0;
#endif // WAND_SUPPORT

	/**
	 * An implementation of the class OpLoadingListener calls this method to
	 * provide the specified authentication request with the user's
	 * authentication data.
	 *
	 * @param user_name, password are the user's credentials.
	 *
	 * @see OpLoadingListener::OnAuthenticationRequired()
	 * @see CancelAuthentication()
	 */
	virtual void Authenticate(const uni_char* user_name, const uni_char* password) = 0;

	/**
	 * Cancels the authentication of the specified authentication request.
	 *
	 * @see OpLoadingListener::OnAuthenticationRequired()
	 * @see Authenticate()
	 */
	virtual void CancelAuthentication() = 0;

	/**
	 * Check for the same authorization, i.e. that the function parameter
	 * callback requests authentication to the same authentication realm
	 * of the same server. Usually it means that the following are the same:
	 * protocol, hostname, port and realm.
	 *
	 * @param callback, the OpAuthenticationCallback object to compare against.
	 * @return TRUE if both callbacks relate to the same authentication realm,
	 * FALSE otherwise.
	 */
	virtual BOOL IsSameAuth(const OpAuthenticationCallback* callback) const = 0;
};

class HistoryInformation
{
public:
	int number;
	int id;
	const uni_char* url;
	const uni_char* title;
	const uni_char* server_name;
};

/** @short Class to keep platform-specific data for a history
 * position.
 *
 * Inherit this class in the platform code and pass an instance of it
 * to OpWindowCommander::SetHistoryUserData(). Core will own every
 * object passed in to this method and core may delete the instance
 * when the associated history entry is no longer kept in memory. You
 * can override the destructor if you need to be informed about the
 * deletion.
 */
class OpHistoryUserData
{
public:
	OpHistoryUserData() {}
	virtual ~OpHistoryUserData() {}
};

#ifdef WIC_USE_DOWNLOAD_CALLBACK
/** DownloadContext defines a class to be inherited and used as a context holder for
 *  the OpDocumentListener for the duration of the download in the context of the DownloadCallback
 *  it shall be possible to extract from the TransferManager/Item.
 */
class DownloadContext
{
public:
	virtual ~DownloadContext() {}

	virtual OpWindowCommander* GetWindowCommander() = 0;
	virtual OpTransferListener*	GetTransferListener() const	{ return NULL; }
	virtual BOOL				IsShownDownload() const	{ return TRUE; }
};

class URLInformation
{
public:
	/** Since this is to be a persistent object we need to know when
	 * we think it has served its purpose, and then allow or disallow
	 * its destruction
	 */
	class DoneListener
	{
	public:
		virtual ~DoneListener(){}
		/** return TRUE if URLInformation is allowed to be deleted
		 * FALSE if not, if the URLInformation object has no DoneListener
		 * it will behave as if the return value was TRUE.
		 * URLInformation are expected to be ready for another sequence of
		 * UI interaction
		 */
		virtual BOOL OnDone(URLInformation* url_info) = 0;
	};

	enum GadgetContentType {
		URLINFO_GADGET_INSTALL_CONTENT,
		URLINFO_MULTI_CONFIGURATION_CONTENT,
		URLINFO_OTHER_CONTENT
	};

	virtual ~URLInformation(){}
	/** Infrastructure for reuse of URLInformation object */
	/** SetDoneListener, returns the previous DoneListener */
	virtual DoneListener *						SetDoneListener(DoneListener * listener) = 0;
	/** URLName, SuggestedFilename, MIMEType, Size and SuggestedSavePath are
	 * methods that can give initial values to the user interaction */
	virtual const uni_char*						URLName() = 0;
	virtual const uni_char*						SuggestedFilename() = 0;
	virtual const char *						MIMEType() = 0;
	virtual OP_STATUS							SetMIMETypeOverride(const char *mime_type) = 0;
	virtual long								Size() = 0;
	virtual const uni_char *					SuggestedSavePath() = 0;
	virtual const uni_char *					HostName() = 0;
	/** Maps gadget related URLContent type from url,
	 * Currently required by Quick */
	virtual GadgetContentType               GetGadgetContentType() = 0;
	/** This call exposes passwords and are not for user presentation
	 * Currently required by Quick */
	virtual const uni_char *                URLUniFullName() = 0;
	/** DownloadDefault will download and save to default DOWNLOAD - folder
	 * DownloadTo will download, ask user with SAVE folder as default for save.
	 * If download path is defined, that will be used as target folder.
	 * If DownloadDefault can not be done with the suggested filename and default folder
	 * ERR_FILE_NOT_FOUND shall be returned and the caller can choose to use DownloadTo.
	 * DownloadDefault and DownloadTo are operations that can be done on the URLInformation
	 * Object
	 */
	virtual OP_STATUS							DownloadDefault(DownloadContext * context, const uni_char * downloadpath = NULL) = 0;
	virtual OP_STATUS							DownloadTo(DownloadContext * context, const uni_char * downloadpath = NULL) = 0;

	/** Get a URL_ID */
	virtual URL_ID								GetURL_Id() = 0;
	/** Prepares the URLInformation object for a continued life outside the originating (web-)document */
	virtual void								ReleaseDocument() = 0;
	/** The URLInformation will no longer be accessed after a Done call. */
	virtual void								URL_Information_Done() = 0;

#ifdef SECURITY_INFORMATION_PARSER
	virtual OP_STATUS							CreateSecurityInformationParser(OpSecurityInformationParser** parser) = 0;
	/** Get the security mode */
	virtual int									GetSecurityMode() = 0;
	/** Get the trust mode */
	virtual int									GetTrustMode() = 0;
	virtual UINT32								SecurityLowStatusReason() = 0;
	virtual const uni_char*						ServerUniName() const = 0;
#endif // SECURITY_INFORMATION_PARSER
#ifdef TRUST_INFO_RESOLVER
	virtual OP_STATUS							CreateTrustInformationResolver(OpTrustInformationResolver** resolver, OpTrustInfoResolverListener * listener) = 0;
#endif // TRUST_INFO_RESOLVER

};

#endif // WIC_USE_DOWNLOAD_CALLBACK

#ifdef PLUGIN_AUTO_INSTALL
/**
 * Class that describes a plugin, used to pass plugin information between core and platform. Easies maintenance of the multiple
 * function calls between layout and platform, and also separates the core internal types from platform.
 */
class PluginInstallInformation
{
public:
	virtual ~PluginInstallInformation() {};

	/**
	 * Get the mime-type stored in this plugin information struct.
	 *
	 * @param mime_type - OpString reference that will be Set() with the mim-type stored with this object.
	 *
	 * @return - OpStatus::OK if the mime-type was retrieved successfuly, error code otherwise.
	 */
	virtual OP_STATUS GetMimeType(OpString& mime_type) const = 0;

	/**
	 * Get the string representation of the plugin content URL stored in this plugin information struct. The caller chooses whether the returned string
	 * should contain the username and password, in which case the string must not be displayed in the platform UI, or should the login data be hidden,
	 * in which case using the string in the UI is fine.
	 *
	 * @param url_string - OpString reference that will be Set() to contain the desired URL string
	 * @param for_ui - TRUE to have the login data stripped from the resulting URL string, FALSE otherwise
	 *
	 * @return - OpStatus::OK if the URL string was fetched successfully, error code otherwise.
	 */
	virtual OP_STATUS GetURLString(OpString& url_string, BOOL for_ui = TRUE) const = 0;

	/**
	 * Check whether the URL is valid in terms of using it for accessing the plugin content. It is possible that an OBJECT has a broken
	 * URL assigned (i.e. with data="whatever"), and an EMBED in the OBJECT has the URL valid. In case there is no viewer for the specific
	 * plugin, layout will pass the OBJECT's URL and the Plugin Install Dialog will try to open it causing havoc.
	 * The check relies on the URL type, should any more conditions be needed, those should be added here.
	 *
	 * @return - TRUE if the URL can be safely opened by the platform, FALSE otherwise.
	 */
	virtual BOOL IsURLValidForDialog() const = 0;
};

/** @short Missing plugin manager notifications
 *
 */
class OpPluginInstallListener
{
public:
	/** @short Notify core that a plugin became available for installation
	 *
	 * This is triggered by the platform missing plugin manager when a plugin becomes available, i.e. we know its name and
	 * an installer is available for it. Part of seamless plugin installation solution - the plugin install wizard.
	 *
	 * @param mime_type - The mimetype for which a plugin has been found to be available.
	 */
	virtual void OnSignalPluginAvailable(const char* mime_type) = 0;
};
#endif // PLUGIN_AUTO_INSTALL

/** @short Listener for document loading related events. */
class OpLoadingListener
{
public:
	enum LoadingFinishStatus {
		LOADING_SUCCESS,
		LOADING_REDIRECT,
		LOADING_COULDNT_CONNECT,
		LOADING_UNKNOWN
	};

	virtual ~OpLoadingListener() {}

	/** @short Core changed the window's current URL.
	 *
	 * This happens when the address initially typed by the user is resolved to
	 * a complete URL ("opera" -> "http://www.opera.com/"), for HTTP redirects,
	 * when navigating to a new URL in history, by clicking on a link, and so
	 * on.
	 *
	 * @param commander the windowcommander
	 * @param url the new URL
	 */
	virtual void OnUrlChanged(OpWindowCommander* commander, const uni_char* url) = 0;

	/** @short Loading has started.
	 *
	 * There will be a call to OnLoadingFinished() later on, when loading has
	 * finished (or been aborted). Before that, there will typically be one or
	 * many calls to OnLoadingProgress(). Note that some pages, because of
	 * their design, never finish loading by themselves (and have to be aborted
	 * by calling OpWindowCommander::Stop(), clicking on a link, or something
	 * along those lines).
	 */
	virtual void OnStartLoading(OpWindowCommander* commander) = 0;

	/** @short Loading has progressed.
	 *
	 * Called when more data has been sent or received, when connections are
	 * made, when attempting to resolve host names, when host names were
	 * resolved, and so on.
	 */
	virtual void OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info) = 0;

	/** @short Loading has finished.
	 *
	 * @see OnStartLoading()
	 */
	virtual void OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status) = 0;

#ifdef URL_MOVED_EVENT
	/**
	 * Called when a redirect occurs.
	 * @param commander the windowcommander.
	 * @param url the URL redirected to.
	 */
	virtual void OnUrlMoved(OpWindowCommander* commander, const uni_char* url) = 0;
#endif // URL_MOVED_EVENT

	/** @short Authentication is needed.
	 *
	 * The platform can implement this method by opening an authentication
	 * dialog and ask the user for user-name and password. The user's data needs
	 * to be returned to the caller by calling
	 * OpAuthenticationCallback::Authenticate() or
	 * OpAuthenticationCallback::CancelAuthentication() on the specified
	 * callback instance.
	 *
	 * @param commander is the OpWindowCommander instance that requests the
	 *  authentication.
	 * @param callback contains the OpAuthenticationCallback instance for the
	 *  authentication request. This callback needs to be notified about the
	 *  user's input.
	 *
	 * @see OpAuthenticationListener::OnAuthenticationRequired()
	 */
	virtual void OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback) = 0;

	/** @short A file is being uploaded.
	 *
	 * There will be a call to OnUploadingFinished() later on, when uploading
	 * has finised.
	 */
	virtual void OnStartUploading(OpWindowCommander* commander) = 0;

	/** @short File upload has finished. */
	virtual void OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status) = 0;

	/** @short Loading failed.
	 *
	 * This method is called when the OpLoadingListener implementation needs to
	 * handle specific loading errors without getting the standard core error
	 * dialogs.
	 *
	 * @param commander the windowcommander
	 * @param msg_id the error id, can be mapped to the error string expected
	 * in the regular error dialog
	 * @param url the failed url
	 * @return TRUE if the error is handled by the listener, if FALSE core will
	 * provide further actions, like initiate a standard error dialog
	 */
	virtual BOOL OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url) = 0;

#ifdef EMBROWSER_SUPPORT
	/**
		Called when the previous document is undisplayed because a new document is loaded.
		@param commander the windowcommander the request is for.
	*/
	virtual void OnUndisplay(OpWindowCommander* commander) = 0;
	/**
		Called when a new document is created and ready for display.
		@param commander what windowcommander the request is for.
	*/
	virtual void OnLoadingCreated(OpWindowCommander* commander) = 0;
#endif // EMBROWSER_SUPPORT

	/**
	 * Called before starting to load a URL.
	 * @param url the url to be loaded
	 * @param inline_document if TRUE, the url is an inline part of the document, and not the main document itself.
	 * @return TRUE if the URL should be loaded normally, or FALSE if loading should be aborted.
	 */
	virtual BOOL OnAboutToLoadUrl(OpWindowCommander* commander, const char* url, BOOL inline_document){return TRUE;};

	/** @short Parsing the document as XML has failed
	 *
	 * Depending on the LOGDOC_FALLBACK_TO_REPARSE_XML_AS_HTML tweak, this
	 * may cause either an error page to be displayed instead, or the page
	 * to be automatically reparsed as HTML.
	 */
	virtual void OnXmlParseFailure(OpWindowCommander* commander) = 0;

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	/**
	 * Called when search suggestions for a specified url is requested. This
	 * typically occurs when a page has failed loading and a set of suggestions
	 * is needed. It is up to the receiver to decide what part of the url to use
	 * as basis for suggestions. No suggestions is a valid outcome.
	 *
	 * The suggestions, which can be text and/or links are typically inserted into
	 * a web page that displays the url that failed (an error page).
	 *
	 * @param url The key to use for determining suggestions. The implemenation
	 *        is free to use the whole url or only part(s) of it to provide usable
	 *        suggestions
	 * @param callback Suggestions are returned via via OpSearchSuggestionsCallback::OnDataReceived()
	 *        in the callback. The reply can be asynchronous
	 */
	virtual void OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback) = 0;
#endif

#ifdef XML_AIT_SUPPORT
	/**
	 * Called when an AIT (Application Information Table) document has
	 * been loaded from an XML_AIT_URL. This callback contains the
	 * parsed data from the document. It is meta data on Hybrid
	 * Broadcast Broadband applications and it is up to the receiver
	 * to decide how to act on the received data. The callback might
	 * result in a new application (URL) being loaded once the
	 * callback is returned.
	 *
	 * @param commander the windowcommander
	 * @param ait_data the application meta data parsed from the XML AIT document.
	 * @return OpStatus::OK on success, otherwise an error code describing the problem.
	 */
	virtual OP_STATUS OnAITDocument(OpWindowCommander* commander, AITData* ait_data) = 0;
#endif // XML_AIT_SUPPORT
};

class OpDocumentListener
{
public:
	enum ImageDisplayMode { NO_IMAGES = 0, LOADED_IMAGES, ALL_IMAGES };
	enum SecurityMode
	{
		NO_SECURITY = 0,          /**< No security, non-"https" url. */
		LOW_SECURITY,             /**< Low security, lowered due to: certificate warning, weak key, or old SSL version. */
		MEDIUM_SECURITY,          /**< Medium security, lowered due to: unable to crl validate, ocsp failed, unable to ocsp validate, slightly weak key, renegotiation vulnerability. */
		HIGH_SECURITY,            /**< High security. */
		SOME_EXTENDED_SECURITY,   /**< Main document has EXTENDED_SECURITY, inline element has only HIGH_SECURITY. */
		EXTENDED_SECURITY,        /**< HIGH_SECURITY and server has certificate satisfying Extended Validation (EV) specifications. */
		UNKNOWN_SECURITY,         /**< Security state has not been determined yet or is temporarily unknown, while the document is being (re-)loaded.
		                           *   If reloading, don't conclude that security has been lowered, show previous state until the final state is determined. */
		FLAWED_SECURITY           /**< Mixed security, main document has HIGH_SECURITY or higher, inline element has MEDIUM_SECURITY or lower. */
	};
#ifdef TRUST_RATING
	enum TrustMode
	{
		TRUST_NOT_SET = 0,
		TRUSTED_DOMAIN,
		UNKNOWN_DOMAIN,
		PHISHING_DOMAIN,
		MALWARE_DOMAIN
	};
#endif // TRUST_RATING
	enum CssDisplayMode   { CSS_AUTHOR_MODE = 0, CSS_USER_MODE };
#ifdef _PRINT_SUPPORT_
	enum PrintPreviewMode { PRINT_PREVIEW_OFF = 0, PRINT_PREVIEW_ON };
#endif // _PRINT_SUPPORT_
	enum DownloadRequestMode { UNKNOWN, SAVE, ASK_USER };

#ifdef XHR_SECURITY_OVERRIDE
	enum XHRPermission { ALLOW, DENY, AUTO };
#endif // XHR_SECURITY_OVERRIDE
#ifdef WIC_ORIENTATION_CHANGE
	enum Orientation { PORTRAIT, LANDSCAPE };
#endif //WIC_ORIENTATION_CHANGE

	virtual ~OpDocumentListener() {}

	/** The state of show images was changed.
		@param commander the window commander for which this happened
		@param mode the new mode */
	virtual void OnImageModeChanged(OpWindowCommander* commander, ImageDisplayMode mode) = 0;

	/** The security state of the document or one of its inlines changed.
		@param commander the window commander for which this happened
		@param mode the new mode
		@param inline_elt TRUE if the change happened to an inline element, and not the actual document */
	virtual void OnSecurityModeChanged(OpWindowCommander* commander, SecurityMode mode, BOOL inline_elt) = 0;

#ifdef TRUST_RATING
	/** The trust rating (anti-phishing) status of the document has changed
		@param commander the window commander for which this happened
	    @param new_rating the new trust rating */
	virtual void OnTrustRatingChanged(OpWindowCommander* commander, TrustRating new_rating) = 0;
#endif // TRUST_RATING

	/** The CSS mode changed (document, css mode)
		@param commander the window commander for which this happened
		@param mode the new mode */
	virtual void OnCssModeChanged(OpWindowCommander* commander, CssDisplayMode mode) = 0;

	/** The handheld state has changed for this document
		@param commander the window commander for which this happened
		@param handheld the handheld state */
	virtual void OnHandheldChanged(OpWindowCommander* commander, BOOL handheld) = 0;
#ifdef _PRINT_SUPPORT_
	/** The print preview mode changed.
		@param commander the window commander for which this happened
		@param mode the new mode */
	virtual void OnPrintPreviewModeChanged(OpWindowCommander* commander, PrintPreviewMode mode) = 0;
#endif // _PRINT_SUPPORT_

	/** The document title was changed. */
	virtual void OnTitleChanged(OpWindowCommander* commander, const uni_char* title) = 0;

	//rg may not need this one
	virtual void OnLinkClicked(OpWindowCommander* commander) = 0;

	/** A link was navigated to. */
	virtual void OnLinkNavigated(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image, const OpRect* link_rect = NULL) = 0;

#ifdef SUPPORT_VISUAL_ADBLOCK
	/** Content needs to be drawn as being removed (used in content block editing mode for now) */
	virtual void OnContentBlocked(OpWindowCommander* commander, const uni_char *image_url) = 0;
	virtual void OnContentUnblocked(OpWindowCommander* commander, const uni_char *image_url) = 0;
#endif // SUPPORT_VISUAL_ADBLOCK

#ifdef ACCESS_KEYS_SUPPORT
	/** An access key has been used. */
	virtual void OnAccessKeyUsed(OpWindowCommander* commander) = 0;
#endif // ACCESS_KEYS_SUPPORT

	/** Navigation is reset. */
	virtual void OnNoNavigation(OpWindowCommander* commander) = 0;

	/** Something is hovered. */
	virtual void OnHover(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image) = 0;

	/** No hover anymore. */
	virtual void OnNoHover(OpWindowCommander* commander) = 0;

	/** The focus has been moved to the next element in the tab chain.
		The context is only valid throughout the call. Any content of the
		context that is required to outlive the call shall be copied by the
		implementation of this method.  The element that received the focus is
		described by the OpDocumentContext.
		@param commander the window commander for which this happened
		@param context the context describing the object that received focus. */
	virtual void OnTabFocusChanged(OpWindowCommander* commander, OpDocumentContext& context) = 0;

#ifndef HAS_NO_SEARCHTEXT
	/** Called when the search result marking is cleared. */
	virtual void OnSearchReset(OpWindowCommander* commander) = 0;

#ifdef WIC_SEARCH_MATCHES
	/** Called when a search hit is about to be painted. */
	virtual void OnSearchHit(OpWindowCommander* commander) = 0;
#endif // WIC_SEARCH_MATCHES
#endif // HAS_NO_SEARCHTEXT
#ifdef SHORTCUT_ICON_SUPPORT
	virtual void OnDocumentIconAdded(OpWindowCommander* commander) = 0;
	virtual void OnDocumentIconRemoved(OpWindowCommander* commander) = 0;
#endif // SHORTCUT_ICON_SUPPORT

	/** Called when the scale of the document has changed.
		@param commander the window commander for which this happened
		@param newScale the new scale
	*/
	virtual void OnScaleChanged(OpWindowCommander* commander, UINT32 newScale) = 0;

#if LAYOUT_MAX_REFLOWS > 0
	/**
	 * Called from the layouting engine when the number of reflows in a
	 * document exceeds LAYOUT_MAX_REFLOWS (if set). Reflow is halted at
	 * this point, and the document might be in an inderterminate state.
	 *
	 * At such a point, the platform is probably limited to reporting
	 * the error and closing the window. Reloading the document may
	 * help.
	 *
	 * @param commander The WindowCommander object associated with the window
	 */
	virtual void OnReflowStuck(OpWindowCommander* commander)  = 0;
#endif

#ifdef DRAG_SUPPORT
# ifdef GADGET_SUPPORT
	/** Called when user drags in a draggable region */
	virtual BOOL HasGadgetReceivedDragRequest(OpWindowCommander* commander) = 0;
	virtual void OnGadgetDragRequest(OpWindowCommander* commander) = 0;
	virtual void CancelGadgetDragRequest(OpWindowCommander* commander) = 0;
	virtual void OnGadgetDragStart(OpWindowCommander* commander, INT32 x, INT32 y) = 0;
	virtual void OnGadgetDragMove(OpWindowCommander* commander, INT32 x, INT32 y) = 0;
	virtual void OnGadgetDragStop(OpWindowCommander* commander) = 0;
	virtual void OnMouseGadgetEnter(OpWindowCommander* commander) = 0;
	virtual void OnMouseGadgetLeave(OpWindowCommander* commander) = 0;
# endif // GADGET_SUPPORT
#endif // DRAG_SUPPORT

#ifdef GADGET_SUPPORT
	virtual BOOL OnGadgetClick(OpWindowCommander* commander, const OpPoint &point, MouseButton button, UINT8 nclicks) = 0;
	virtual void OnGadgetPaint(OpWindowCommander* commander, VisualDevice* vis_dev) = 0;

	/** @short The gadget wants attention.
	 *
	 * The implementation of this method should do whatever is the common way
	 * for an application to raise user attention in the operating system (such
	 * as e.g. making the application button in the task bar flash).
	 *
	 * @param commander the window commander (gadget) for which this happened
	 */
	virtual void OnGadgetGetAttention(OpWindowCommander* commander) = 0;

	/** @short Gadget notification callback-to-core context.
	 *
	 * Passed via OnGadgetShowNotification().
	 */
	class GadgetShowNotificationCallback
	{
	public:
		/** @short User response to a notification. */
		enum Reply
		{
			/** @short The notification was ignored (or at least not read) by the user. */
			REPLY_IGNORED,

			/** @short The notification was read and acknowledged by the user. */
			REPLY_ACKNOWLEDGED
		};

		virtual ~GadgetShowNotificationCallback() {}

		/** @short The user has responded to the notification.
		 *
		 * After this call, this GadgetShowNotificationCallback object is no
		 * longer to be used, because it may be deleted by core at any time
		 * after this.
		 *
		 * @param reply How the user responded to the notification.
		 */
		virtual void OnShowNotificationFinished(Reply reply) = 0;
	};

	/** @short A gadget wants to show a notification.
	 *
	 * How to do this is operating system / desktop environment specific. It
	 * could be to display a small window in the bottom right corner of the
	 * screen for a short time (a typical way for Windows), or by integrating
	 * with Growl (Mac OS).
	 *
	 * callback->OnNotificationFinished() must be called when the notification
	 * disappears. It may be called synchronously (i.e. before
	 * OnGadgetShowNotification() has returned), or asynchronously. Only one
	 * operation of this type at a time is allowed. In some cases, a
	 * notification may be retracted / cancelled from the core side before the
	 * user has responded, by calling OnGadgetShowNotificationCancel().
	 *
	 * @param commander the window commander (gadget) for which this happened
	 * @param message The message to display in the notification.
	 * @param callback Context object for this operation. The product/UI code
	 * uses this object to tell core how the user responded to the
	 * notification. This object is made invalid by
	 * OnGadgetShowNotificationCancel().
	 */
	virtual void OnGadgetShowNotification(OpWindowCommander* commander, const uni_char* message, GadgetShowNotificationCallback* callback) = 0;

	/** @short A gadget wants to retract a previously issued but unhandled notification, if any.
	 *
	 * This typically happens if the gadget is closed before
	 * OnNotificationFinished() is called. The product/UI side must refrain
	 * from calling the pending OnNotificationFinished() when this method is
	 * called, because the callback object may be deleted at any time after
	 * this.
	 *
	 * Note that it is not an error to call this method even if there is no
	 * unhandled notification.
	 *
	 * @param commander the window commander (gadget) for which this happened
	 */
	virtual void OnGadgetShowNotificationCancel(OpWindowCommander* commander) = 0;

# ifdef WIC_USE_DOWNLOAD_CALLBACK
	/** @short Require user interaction when getting a gadget download
	 *
	 * @param commander the window commander for the document/window that
	 * initiatied the download
	 * @param url_info the object that can be used to get information about the
	 * gadget (http headers are loaded, referrer are known) for UI interaction
	 * and subsequent cancelation (call URL_Information_Done()) or download it
	 * using the convenience functions.
	 */
	virtual void OnGadgetInstall(OpWindowCommander* commander, URLInformation* url_info) = 0;
# endif // WIC_USE_DOWNLOAD_CALLBACK
#endif // GADGET_SUPPORT

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	/**
	 * Called whenever a search has been submitted or a link been clicked on the error page.
	 *
	 * Before this function is called the error page must be populated with links and a search
	 * field. On construction, the error page sets up the search field and also request search
	 * suggestions. The flow of commands and replies is as follows:
	 * @msc
	 * Core,UI,User;
	 * Core => UI [label="OpLoadingListener::OnSearchSuggestionsRequested()", URL="\ref OpLoadingListener::OnSearchSuggestionsRequested()"];
	 * UI => Core [label="OpSearchSuggestionsCallback::OnDataReceived()", URL="\ref OpSearchSuggestionsCallback::OnDataReceived()"];
	 * Core -> User [label="Show document with search suggestions"];
	 * User -> Core [label="submit search or click on a link"];
	 * Core -> UI [label="OpDocumentListener::OnSearchSuggestionsUsed()", URL="\ref OpDocumentListener::OnSearchSuggestionsUsed()"];
	 * @endmsc
	 *
	 * @param commander The WindowCommander object associated with the window
	 * @param suggestions TRUE if a suggestion link was clicked, FALSE of the search
	 *        form was activated
	 */
	virtual void OnSearchSuggestionsUsed(OpWindowCommander* commander, BOOL suggestions) = 0;
#endif

	virtual void OnInnerSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height) = 0;
	virtual void OnGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height) = 0;
	virtual void OnOuterSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height) = 0;
	virtual void OnGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height) = 0;
	virtual void OnMoveRequest(OpWindowCommander* commander, INT32 x, INT32 y) = 0;
	virtual void OnGetPosition(OpWindowCommander* commander, INT32* x, INT32* y) = 0;

#ifdef WIC_USE_WINDOW_ATTACHMENT
	/** @short Core wants the UI to attach/detach the window to/from the browser chrome.
	 *
	 * Currently, this is only used to integrate the developer tools window
	 * into the desktop browser. In the future, this is going to be used for
	 * toolbars, panels etc created by extensions, and the API will be extended
	 * with methods to request and query specific types of UI attachment.
	 *
	 * @param commander the windowcommander in question
	 * @param attached TRUE if the window should be attached to the browser
	 * chrome, FALSE if it should be or detached from it.
	 */
	virtual void OnWindowAttachRequest(OpWindowCommander* commander, BOOL attached) = 0;

	/** @short Core wants to know whether the window is currently attached to the browser chrome.
	 *
	 * @param commander the windowcommander in question
	 * @param attached The query result should be stored here. TRUE if the
	 * window is attached, FALSE if it is not.
	 */
	virtual void OnGetWindowAttached(OpWindowCommander* commander, BOOL* attached) = 0;
#endif // WIC_USE_WINDOW_ATTACHMENT

	/** Will be called when opera wants the ui to close the window associated with the windowcommander.
		Note that OpWindowCommanderManager's OpUiWindowListener::OnCloseUiWindow will also be called.
	*/
	virtual void OnClose(OpWindowCommander* commander) = 0;

	/** Called when opera wants the ui to raise the window connected to the windowcommander. Typically occurs as the result of a javascript call (focus).
		@param commander the windowcommander in question
	*/
	virtual void OnRaiseRequest(OpWindowCommander* commander) = 0;

	/** Called when opera wants the ui to lower the window connected to the windowcommander. Typically occurs as the result of a javascript call (blur).
		@param commander the windowcommander in question
	*/
	virtual void OnLowerRequest(OpWindowCommander* commander) = 0;

	/** Called when a script would like to change statusbar text	*/

	virtual void OnStatusText(OpWindowCommander* commander, const uni_char* text) = 0;

	/** Called when a "mailto" link is followed.
	 *
	 * @param raw_url The mailto URL string being followed.
	 * @param called_externally TRUE if the call is initiated from outside Opera.
	 * @param mailto_from_form TRUE if the mailto is the target of a form element.
	 * @param servername Server name for the containing document.
	 */
	virtual void OnMailTo(OpWindowCommander* commander, const OpStringC8& raw_url, BOOL called_externally, BOOL mailto_from_form, const OpStringC8& servername) = 0;
	virtual void OnTel(OpWindowCommander* commander, const uni_char* url) = 0;
	virtual BOOL OnUnknownProtocol(OpWindowCommander* commander, const uni_char* url) = 0;
	virtual void OnUnhandledFiletype(OpWindowCommander* commander, const uni_char* url) = 0;

#ifdef _WML_SUPPORT_
	/** Will be called to notify the user when the origin check for a transition to a WML
		card with an ACCESS element fails. */
	virtual void OnWmlDenyAccess(OpWindowCommander* commander) = 0;
#endif // _WML_SUPPORT_

#ifdef WIC_USE_DOWNLOAD_CALLBACK
	class DownloadCallback : public URLInformation
	{
	public:
		virtual ~DownloadCallback() {}

		/** Mode and the URLInfoAPI methods are
		 * methods that can give initial values to the user interaction */
		virtual DownloadRequestMode				Mode() = 0;
		/** If the listener has the need, information that needs to be persistent
		 *  can be set and got by use of DownloadContext */
		virtual void							SetDownloadContext(DownloadContext * dc) = 0;
		virtual DownloadContext *				GetDownloadContext() = 0;
		/** Save, Run, ReturnToSender or Abort must be the last interaction with the DownloadCallback instance.
		 * One must be called.
		 * Save or Run indicates that the download in question shall continue under
		 * supervision of the TransferManager, the target file shall be that indicated by filename
		 * ReturnToSender means it is given back to DocumentManager..
		 * A TRUE return value means that the object can no longer be accessed and pointer
		 * to it should be set to NULL */
		virtual BOOL							Save(uni_char* filename) = 0;
		virtual BOOL							Run(uni_char* filename) = 0;
		virtual BOOL							ReturnToSender(BOOL usePlugin) = 0;
		virtual BOOL							Abort() = 0;
	};

	virtual void OnDownloadRequest(OpWindowCommander* commander, DownloadCallback* callback) = 0;
#else // !WIC_USE_DOWNLOAD_CALLBACK
	/** @short Download requested.
	  *
	  * This method is called when core requests a download. InitiateTransfer()
	  * or CancelTransfer() should be called as a reply to this.
	  *
	  * @param commander The windowcommander in question
	  * @param url The URL to download
	  * @param suggested_filename Suggested filename for the download content
	  * @param mimetype Mime-type of the download content
	  * @param size Size, in bytes, of the download content
	  * @param mode Mode of the download
	  * @param suggested_path Suggested path for the download content
	  */
	virtual void OnDownloadRequest(OpWindowCommander* commander, const uni_char* url,  const uni_char* suggested_filename, const char* mimetype, OpFileLength size, DownloadRequestMode mode, const uni_char* suggested_path = NULL) = 0;
#endif // !WIC_USE_DOWNLOAD_CALLBACK

#ifdef XHR_SECURITY_OVERRIDE
    /** This method is called when the security manager is about to evaluate
	 * whether an XHR should be allowed or denied, based on single origin
	 * and related policies. Depending on the return value of this method,
	 * the security manager may be run, or it may be bypassed and the decision
	 * is made by this method.
	 *
	 * @param commander The windowcommander in question
	 * @param source_url The origin page URL
	 * @param target The URL that the page is trying to access via XHR
	 */
	virtual XHRPermission OnXHR( OpWindowCommander* commander, const uni_char* source_url, const uni_char* target_url) = 0;
#endif // XHR_SECURITY_OVERRIDE

	/** Called when a refresh is about to be done on a url
		@return TRUE if a refresh of the URL is allowed, FALES otherwise
		*/
	virtual BOOL OnRefreshUrl(OpWindowCommander* commander, const uni_char* url) = 0;

#ifdef RSS_SUPPORT
	virtual void OnRss(OpWindowCommander* commander, const uni_char* url) = 0;

# ifdef WIC_SUBSCRIBE_FEED
	/** @short Newsfeed subscription requested.
	 *
	 * This method is called when core is set up to use an external feed
	 * reader, such as M2 or something completely different.
	 *
	 * @param commander The windowcommander in question
	 * @param url The URL identifying the newsfeed.
	 */
	virtual void OnSubscribeFeed(OpWindowCommander* commander, const uni_char* url) = 0;
# endif // WIC_SUBSCRIBE_FEED
#endif // RSS_SUPPORT

	/** Callback for script dialogs (alert, confirm, prompt.)  The UI MUST call the callback, regardless of how much it
	    fails to actually handle the dialog.  The callback can be called immediately when the dialog is signalled (if
	    the UI wants to ignore the dialog.)  If it is not called immediately, script execution in the document that
	    generated the dialog will be blocked until the callback is called.

	    If the UI provides a "stop this script" choice in the dialog, the dialog can be dismissed by calling the
	    function AbortScript instead of one of the other dismissal functions.  This will abort the script that generated
	    the dialog, but other scripts in the window will continue to be executed.  Recommended use is combined with
	    OpWindowCommander::SetScriptingDisabled(TRUE). */
	class JSDialogCallback
	{
	public:
		virtual ~JSDialogCallback() {}
		virtual void OnAlertDismissed() = 0;
		/**< Called by the UI when a dialog signalled through a call to OnJSAlert has been dismissed. */
		virtual void OnConfirmDismissed(BOOL ok) = 0;
		/**< Called by the UI when a dialog signalled through a call to OnJSConfirm has been dismissed.
		     @param ok TRUE if the user selected the positive choice. */
		virtual void OnPromptDismissed(BOOL ok, const uni_char *value) = 0;
		/**< Called by the UI when a dialog signalled through a call to OnJSPrompt has been dismissed.
		     @param ok FALSE if the user did not provide an input (e.g. selected a "cancel" choice.)
		     @param value The value entered by the user, if 'ok' is TRUE.  Will be copied by the callback. */

		virtual void AbortScript() {}
		/**< Can be called instead of OnAlertDismissed, OnConfirmDismissed or OnPromptDismissed, if the
		     user selected a "stop this script" choice. */
	};

	/** Called when opera wants the ui to display a JS alert (OK) dialog.
		@param commander the windowcommander in question
		@param hostname hostname of script that requested the dialog.  Should be clearly displayed in the UI, for anti-spoofing purposes.
		@param message message to display in the dialog
		@param callback callback object to notify when the user dismisses the dialog */
	virtual void OnJSAlert(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, JSDialogCallback *callback) = 0;

	/** Called when opera wants the ui to display a JS confirm (OK + Cancel) dialog.
		@param commander the windowcommander in question
		@param hostname hostname of script that requested the dialog.  Should be clearly displayed in the UI, for anti-spoofing purposes.
		@param message message to display in the dialog
		@param callback callback object to notify when the user dismisses the dialog */
	virtual void OnJSConfirm(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, JSDialogCallback *callback) = 0;

	/** Called when opera wants the ui to display a JS prompt (textedit + OK + Cancel) dialog.
		@param commander the windowcommander in question
		@param hostname hostname of script that requested the dialog.  Should be clearly displayed in the UI, for anti-spoofing purposes.
		@param message message to display in the dialog
		@param default_value default text
		@param callback callback object to notify when the user dismisses the dialog */
	virtual void OnJSPrompt(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, const uni_char *default_value, JSDialogCallback *callback) = 0;

	/** Delayed popup opener.  Used by the ui to open a blocked popup later<tm>.  This object
	    needs to be freed by the ui by calling either Open or Release.  The ui should not
	    delete it. */
	class JSPopupOpener
	{
	public:
		virtual ~JSPopupOpener() {}
		/** Called by the ui to open the popup.  Once called, the JSPopupOpener reference is
		    invalid (the JSPopupOpener object may have been deleted).  The popup cannot be
		    opened twice.*/
		virtual void Open() = 0;

		/** Called by the ui to discard the popup. */
		virtual void Release() = 0;
	};

	/** Popup callback.  Used by the ui to signal its wishes regarding the blocking of a popup.
	    The object is freed by calling Continue.  The ui should not delete it. */
	class JSPopupCallback
	{
	public:
		virtual ~JSPopupCallback() {}
		enum Action
		{
			POPUP_ACTION_DEFAULT, ///< Let core decide.
			POPUP_ACTION_ACCEPT,  ///< Open the popup.
			POPUP_ACTION_REFUSE   ///< Don't open the popup (window.open will return null).
		};

		/** Called by the ui to let the script that wanted to open the popup continue.  Should
		    be called reasonably soon after the call to OnJSPopup, since all script activity
		    and most other activity in the document is blocked until it is called (though if
		    the ui displays a modal dialog, having script activity blocked while the dialog is
		    displayed is of course the right thing).

		    If 'opener' is not NULL and the popup is blocked, '*opener' is set to point to a
		    JSPopupOpener object that can be used to open the popup later.  In this case, a
		    handle to a fake window object will be returned to the script.

		    If 'opener' is not NULL and the popup is not blocked, '*opener' is set to NULL.
		    This way the ui can determine whether the popup was blocked or not.

		    If 'opener' is NULL and the popup is blocked, null is returned to the script.

		    @param action what core should do (default, override to open or override to block)
		    @param opener location to store pointer to JSPopupOpener object (or NULL) */
		virtual void Continue(Action action = POPUP_ACTION_DEFAULT, JSPopupOpener **opener = NULL) = 0;
	};

	/** Called when opera is about to open or block a popup.  Can be used to display a dialog
	    letting the user decide whether the popup is to be blocked or not.  The ui must call the
	    supplied callback to let core know what to do.

	    @param commander the windowcommander in question
	    @param url the url to be opened in the popup.  May be empty.  If not empty, any password
	               information in the url will be hidden (replaced by '*').
	    @param name the name of the popup window
	    @param left the popup window's position (-1 if the script said nothing)
	    @param top the popup window's position (-1 if the script said nothing)
	    @param width the popup window's dimensions (0 if the script said nothing)
	    @param height the popup window's dimensions (0 if the script said nothing)
	    @param scrollbars display scrollbars in the popup window (MAYBE if the script said nothing)
	    @param location display address field in the popup window (MAYBE if the script said nothing)
	    @param refuse TRUE if the popup will be refused (unless overridden by the ui)
	    @param unrequested TRUE if the popup is classified as "unrequested"
	    @param callback callback object to notify opera to continue */
	virtual void OnJSPopup(OpWindowCommander* commander, const uni_char *url, const uni_char *name, int left, int top, int width, int height, BOOL3 scrollbars, BOOL3 location, BOOL refuse, BOOL unrequested, JSPopupCallback *callback) = 0;

#if defined(USER_JAVASCRIPT) && defined(REVIEW_USER_SCRIPTS)
	/**
	 * Called when the userjs manager is deciding what userjs files to use.
	 *
	 * @param wc The windowcommander representing the window for which userjs
	 *           files are needed.
	 * @param hostname The hostname from the URL being loaded.
	 * @param preference_value The default userjs files from preferences.
	 * @param reviewed_value contains new userjs filenames if any change is requested.
	 * reviewed_value with empty string "", and TRUE as return value means that no userjs should be used.
	 * @return TRUE if a change to userjs is requested, FALSE if prfeerence_value is accepted.
	 */
	virtual BOOL OnReviewUserScripts(OpWindowCommander *wc, const uni_char *hostname, const uni_char *preference_value, OpString &reviewed_value) = 0;
#endif // USER_JAVASCRIPT && REVIEW_USER_SCRIPTS

	/** @short Form submission request context.
	 *
	 * An object of this type is passed via a call to OnFormSubmit().
	 */
	class FormSubmitCallback
	{
	public:
		virtual ~FormSubmitCallback() {}

		/** @short Get the name of the destination server.
		 *
		 * This is the server to which the form is to be submitted.
		 */
		virtual const uni_char* FormServerName() = 0;

		/** @short Get the name of the referrer server.
		 *
		 * This is the server from which the form is to be submitted (i.e. the
		 * server part of the current URL).
		 */
		virtual const uni_char* ReferrerServerName() = 0;

		/** @short Is the server to which the form is to be submitted an HTTP server?
		 *
		 * @return TRUE if it is an HTTP server, or FALSE if it is HTTPS or
		 * whatever else.
		 */
		virtual BOOL FormServerIsHTTP() = 0;

		/** @short Get the security level of the referrer.
		 *
		 * This will return the security level of the URL from which the form
		 * is to be submitted (i.e. the current URL).
		 */
		virtual SecurityMode ReferrerSecurityMode() = 0;

		/** @short Get target windowcommander.
		 *
		 * This is the windowcommander of the page to which the form is to be
		 * submitted. In many cases it is the same as the originating
		 * windowcommander that was passed to OnFormSubmit(), but if the form
		 * has a different target window, it will be different.
		 */
		virtual OpWindowCommander* GetTargetWindowCommander() = 0;

		/** @short Generate a document containing form submission details.
		 *
		 * Generate a document based on the servers involved and the form url
		 * to better describe the submit about to happen.
		 *
		 * @param details_commander pointer to object where to render the form details
		 */
		virtual OP_STATUS GenerateFormDetails(OpWindowCommander* details_commander) = 0;

		/** @short A desicion has been made by the user. Continue.
		 *
		 * @param submit If TRUE, the form is submitted unless the document had
		 * already discarded the request for whatever reason. If FALSE, the
		 * form will not be submitted.
		 */
		virtual void Continue(BOOL submit) = 0;
	};

	/** @short Permission to submit a form was requested.
	 *
	 * Use preferences, FormServerIsHTTP() and ReferrerSecurityMode() to
	 * determine the appropriate action. callback->Continue() MUST be called,
	 * either directly during the call to this function, or asynchronously.
	 *
	 * FIXME: demanding, or even suggesting, that the preference system be used
	 * to determine the appropriate action probably indicates a design flaw.
	 *
	 * @param commander The windowcommander in question. This is the
	 * originating windowcommander, the one from which the form is to be
	 * submitted.
	 * @param callback Form submission request context. May be used to obtain
	 * additional information about the form submission request, and to notify
	 * core about the decision made (submit / don't submit).
	 */
	virtual void OnFormSubmit(OpWindowCommander* commander, FormSubmitCallback *callback) = 0;

#ifdef _PLUGIN_SUPPORT_
    class PluginPostCallback
    {
    public:
		virtual ~PluginPostCallback() {} // has virtual methods, so needs virtual destructor.

        /** PluginServerName can be used to extract server string
         *  used for the user interaction. */
        virtual const uni_char * PluginServerName() = 0;

        /** PluginServerIsHTTP and ReferrerSecurityMode can be used to extract server security
         *  that is used to determinate appropriate user interaction. */
        virtual BOOL PluginServerIsHTTP() = 0;
        virtual SecurityMode ReferrerSecurityMode() = 0;

        /** Continue.  If 'post' is TRUE, the data is submitted unless Opera
         *  discards the request for whatever reason. */
        virtual void Continue(BOOL post) = 0;
    };

    /** Called when plugin data is posted. Use PluginServerIsHTTP and ReferrerSecurityMode
     *  to determine the appropriate action. The supplied callback MUST be called.
     *  It can be called directly during the call to this function. */
    virtual void OnPluginPost(OpWindowCommander* commander, PluginPostCallback *callback) = 0;
#endif // _PLUGIN_SUPPORT_


	/**
		generic callback class used for dialog-like callbacks from core code to platform
		DialogCallback::OnDialogReply must always be called as a response on these callbacks
	*/
	class DialogCallback
	{
	public:
		virtual ~DialogCallback() {}
		enum Reply
		{
			REPLY_CANCEL,
			REPLY_YES,
			REPLY_NO
		};

		virtual void OnDialogReply(Reply reply) = 0;
	};

	/** @short Called if core no longer wants the result of a previously requested dialog.
	 *
	 * OnCancelDialog() is called when core needs to cancel a dialog. The only
	 * thing the UI needs to make sure is not to use the DialogCallback object
	 * after this method has been called, as it is about to be destroyed by
	 * core.
	 *
	 * This is a general cancelation of any dialog initiated by a method that
	 * includes a DialogCallback object; i.e. OnConfirm(),
	 * OnAskAboutUrlWithUserName() and OnAskAboutFormRedirect().
	 */
	virtual void OnCancelDialog(OpWindowCommander* commander, DialogCallback* callback) = 0;

	/** Called when opera wants the ui to display a confirm (OK + Cancel) dialog.
		@param commander the windowcommander in question
		@param message message to display in the dialog
		@param callback callback object to notify when the user dismisses the dialog */
	virtual void OnConfirm(OpWindowCommander* commander, const uni_char *message, DialogCallback *callback) = 0;

	/** Called when opera wants the ui to display an error message for which there
		is no respone.
		@param commander the windowcommander in question
		@param title title of the mesasge to display
		@param message message to display
		@param additional additional text to display */
	virtual void OnGenericError(OpWindowCommander* commander, const uni_char *title, const uni_char *message, const uni_char *additional = NULL) = 0;

	/** Called when opera wants the ui to display a question about going into
		online mode from offline mode. Ok + No buttons are included.
		@param commander the windowcommander in question
		@param message message to display in the question dialog
		If query for example is about an url, message should contain the url.
		@param callback callback object to notify when the user dismisses the dialog
		DialogCallback::Reply::REPLY_YES if user agrees to go online, and anything else if not.
		*/
	virtual void OnQueryGoOnline(OpWindowCommander* commander, const uni_char *message, DialogCallback *callback) = 0;

#ifdef DRAG_SUPPORT
	/** Called when opera wants the ui to display a confirmation dialog asking whether a dropped file's data may be
		passed to a page (it's used only for the cases the file dropping will cause passing it to the page).
		Yes + No buttons are included.
		@param commander the windowcommander in question
		@param server_name the name of the server the dropped files will be uploaded to
		@param callback callback object to notify when the user dismisses the dialog
		DialogCallback::Reply::REPLY_YES if user agrees to upload the file, and anything else if not.
		*/
	virtual void OnFileDropConfirm(OpWindowCommander* commander, const uni_char *server_name, DialogCallback *callback) = 0;
#endif // DRAG_SUPPORT

	/**
		called when a URL that contains a username and password is
		about to be loaded, reply YES to continue loading or CANCEL/NO
		to abort.

		The url 'url' has specified a username 'username' for the host 'hostname'.
		@param commander the windowcommander in question
		@param url the url that is about to be loaded
		@param hostname the host that the username is specified for
		@param username the username specified in the url
		@param callback Call-back object. DialogCallback::OnDialogReply MUST be called.
	*/
	virtual void OnAskAboutUrlWithUserName(OpWindowCommander* commander, const uni_char* url, const uni_char* hostname, const uni_char* username, DialogCallback* callback) = 0;

	/**
		called when a server wants to redirect a form
		reply YES to re-POST the form data to the new destination
		reply NO to GET the new destination without resending the form data
		reply CANCEL to abort
		@param commander the windowcommander in question
		@param source_url the original form url
		@param target_url the url that the server want to redirect the POST request to
		@param callback Call-back object. DialogCallback::OnDialogReply MUST be called.
	*/
	virtual void OnAskAboutFormRedirect(OpWindowCommander* commander, const uni_char* source_url, const uni_char* target_url, DialogCallback* callback) = 0;

	/**
		Called when the user has submitted a HTML form to a server, which accepts the request,
		but never sends an HTTP_CONTINUE (HTTP response code 100) to tell Opera to send the
		forms data. Opera waits for 40 seconds before calling this callback.
	*/
	virtual void OnFormRequestTimeout(OpWindowCommander* commander, const uni_char* url) = 0;

#ifdef WEB_HANDLERS_SUPPORT
	/** Callback class used for the dialog asking a user if the registered web protocol/content handler
	 * is allowed to be used. OnOK() or OnCancel() must always be called as a response on these callbacks.
	 */
	class WebHandlerCallback
	{
	public:
		/** Should be called when a user accepts the dialog.
		 *
		 * @param dont_ask_again TRUE if the user ticked the 'never ask me again' checkbox.
		 * If it is TRUE, chosen option will be remembered and the user will not be asked when
		 * the protocol or the mime type in question is handled again.
		 * If it is FALSE and the web-handler was selected the user will be asked before the handler
		 * is used again. In case dont_ask_again parameter is FALSE but the web-handler was not chosen
		 * selected appliaction is remembered and used for a session time.
		 * @param app application which should be used to handle the protocol/content in question.
		 * This has to fulfil the following requirements:
		 * - It must be NULL if the registered web handler should be used.
		 * - It must be "" if Opera should be used (e.g. to check if it can handle it
		 * or to take further actions like asking the user).
		 * - It must contain the path to the system application the user pointed if
		 * the application is supposed to be used.
		 */
		virtual void OnOK(BOOL dont_ask_again, const uni_char* app) = 0;
		/** Should be called when a user discards the dialog.
		 * When this method is called, the url using protocol or mime type handled
		 * by the registered web-handler will not be passed to it and
		 * loading of the url will be stopped.
		 */
		virtual void OnCancel() = 0;
		/** Gets the registered web handler description */
		virtual const uni_char* GetDescription() const = 0;
		/** Gets the protocol or content type the handler is meant to handle */
		virtual const char* GetMimeTypeOrProtocol() const = 0;
		/** Gets the name of the handler's host */
		virtual const uni_char* GetTargetHostName() const = 0;
		/** Indicates whether the 'never ask me again'option should be displayed in the dialog.
		 * Note that for various reasons this option may not always be allowed to be displayed.
		 * Thus there's a need to ask WebHandlerCallback about it each time the dialog is supposed to be displayed.
		 */
		virtual BOOL ShowNeverAskMeAgain() const = 0;
		/** Returns whether a web hander is a protocol or a content handler.
		 *
		 * @return TRUE if the web hander is the protocol handler. FALSE if the handler handles some mime type.
		 */
		virtual BOOL IsProtocolHandler() const = 0;
	};

	/** Callback class used for answering whether the mailto web handler identified by a url is registered and handled
	 * by the UI. An instance of it is passed to IsMailToWebHandlerRegistered().
	 *
	 * @see OnMailToWebHandlerRegister()
	 * @see OnMailToWebHandlerUnegister()
	 * @see IsMailToWebHandlerRegistered()
	 */
	class MailtoWebHandlerCheckCallback
	{
	public:
		/** Returns the handler's URL. It has a form containing "%s" as described in
		 * http://www.whatwg.org/specs/web-apps/current-work/multipage/browsers.html#custom-handlers.
		 */
		virtual const uni_char* GetURL() = 0;
		/** Should be called when the check if the mailto web handler identified by the url
		 * is registered by the UI is completed.
		 *
		 * @param is_registered TRUE if the mailto web handler identified by the url is registered by the UI.
		 */
		virtual void OnCompleted(BOOL is_registered) = 0;
	};

	/** Called just before the registered protocol/content handler is going to be used.
	 * Its purpose is to ask a user for permission to use the handler.
	 *
	 * @param commander The windowcommander
	 * @param cb The web handler callback. All required information can be got from it.
	 * @see WebHandlerCallback
	 */
	virtual void OnWebHandler(OpWindowCommander* commander, WebHandlerCallback* cb) = 0;

	/** Called if core no longer wants the result of a previously requested dialog
	 * thus the dialog should be cancelled.
	 *
	 * The only thing the UI needs to make sure is not to use the WebHandlerCallback object
	 * after this method has been called, as it is about to be destroyed by core.
	 *
	 * @param commander is the OpWindowCommander for which the web-handler
	 *  dialog with the specified callback was requested.
	 * @param cb - callback which needs to be cancelled
	 */
	virtual void OnWebHandlerCancelled(OpWindowCommander* commander, WebHandlerCallback* cb) = 0;

	/**
	 * Called when the mailto:// web handler is being registered
	 * with navigator.registerProtocolHandler(). If the implementer wants to register the handler itself,
	 * i.e. store information about it and pass all mailto URLs to the handler, it should
	 * store information and return TRUE from this function. In such case core will not save any information
	 * about the handler. Also in such case OnMailTo() will be called when mailto:// scheme usage is detected
	 * as normal so the mailto URLs may be passed to the handler by the implementer.
	 * However if FALSE is returned from this function core will store handler's
	 * information and will be using it (OnMailTo() will not be called).
	 * @see OnMailTo().
	 *
	 * @param commander the window commander
	 * @param url the handler's URL. It has a form containing "%s" as described in
	 * http://www.whatwg.org/specs/web-apps/current-work/multipage/browsers.html#custom-handlers
	 * @param description the handler's description
	 *
	 * @return TRUE if the implementer takes care of mailto:// handler, FALSE if core should do it.
	 */
	virtual BOOL OnMailToWebHandlerRegister(OpWindowCommander* commander, const uni_char* url, const uni_char* description) = 0;

	/**
	 * Called when the mailto:// web handler is being unregistered
	 * with navigator.unregisterProtocolHandler(). If the implementer
	 * registered the handler represented by the url when OnMailToWebHandlerRegister()
	 * was called and haven't unregistered it so far, it must stop using it from now on
	 * by removing information about it and not passing any mailto URLs to it.
	 *
	 * @param commander the window commander
	 * @param url the handler's URL. It has a form containing "%s" as described in
	 * http://www.whatwg.org/specs/web-apps/current-work/multipage/browsers.html#custom-handlers
	 *
	 * @see OnMailToWebHandlerRegister()
	 */
	virtual void OnMailToWebHandlerUnregister(OpWindowCommander* commander, const uni_char* url) = 0;

	/**
	 * Called when the mailto:// web handler registration is checked
	 * with navigator.isProtocolHandlerRegistered(). If the implementer
	 * registered the handler represented by the url when OnMailToWebHandlerRegister()
	 * was called and haven't unregistered it so far, TRUE must be passed to
	 * MailtoWebHandlerCheckCallback::OnCompleted() and FALSE otherwise.
	 *
	 * @param commander the window commander
	 * @param cb The callbackto be called when the check is done. All required information can be got from it.
	 *
	 * @see MailtoWebHandlerCheckCallback
	 * @see OnMailToWebHandlerRegister()
	 * @see OnMailToWebHandlerUnregister()
	 */
	virtual void IsMailToWebHandlerRegistered(OpWindowCommander* commander, MailtoWebHandlerCheckCallback* cb) = 0;
#endif // WEB_HANDLERS_SUPPORT

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	/**
	 *  Callback class used when requesting a quota increase for web storage, web sql databases
	 *  and other APIs that might need user or platform consent to use more storage space.
	 *
	 *  OnQuotaReply or OnCancel must always be called before disposing this object.
	 *
	 *  OnCancel should be called when the user has not provided a meaningful answer but the
	 *  dialog/prompt needs to be disposed, like when closing the tab, or shutting down.
	 */
	class QuotaCallback
	{
	public:
		virtual ~QuotaCallback() {}
		/**
		 * Called by the platform when a decision has been made about increasing the quota.
		 *
		 * @param allow_increase    TRUE if the quota is allowed to increase, FALSE otherwise
		 * @param new_quota_size    New quota size.
		 *                          - If allow_increase is TRUE and this parameter is not zero, this will be the new upper bound.
		 *                          - If allow_increase is TRUE and this parameter is zero, quota will be unbounded
		 *                          - If allow_increase is FALSE and this parameter is not zero, this will be the new upper
		 *                            bound and quota increase will be denied when its hit.
		 *                          - If allow_increase is FALSE and this parameter is zero, quota limit will be kept intact but
		 *                            forbidden from further increases
		 *                        */
		virtual void OnQuotaReply(BOOL allow_increase, OpFileLength new_quota_size) = 0;

		/**
		 * Implemented then the dialog is cancelled, like when reloading the page, without
		 * a meaningful answer from the user. This means that no preferences will be stored
		 * and the quota increase will fail gracefully.
		 */
		virtual void OnCancel() = 0;
	};

	/**
	 * Used by the web storage and web databases DOM APIs.
	 *
	 * This function is called when opera wants the ui to ask the user the user if
	 * he wants to increase the quota when an operation is about to exceed it's
	 * allowed quota.
	 *
	 * Note: the callback can use used synchronously immediately when this function is called.
	 *
	 * A naive implementation (just to get code compiling) can just call the callback with
	 * allow_increase = FALSE and new_quota_size = 0
	 *
	 * In case of doubts contact: stighal, joaoe, pflouret
	 *
	 * @param commander          The windowcommander
	 * @param db_name            The name of the database as given by the web page author, if ANY. Can be NULL
	 * @param db_domain          The domain of the web page which triggered the operation that does the request
	 * @param db_type            The type of storage area. Possible values:
	 *                            - localstorage for web storage's window.locaStorage
	 *                            - webdatabase for web databases' Database objects
	 *                            - widgetpreferences for the widget.preferences collection
	 *                            - other for types used for testing and other purpouses
	 *                           New types might be added, so check opdatabasemanager.cpp for the array
	 *                           op_database_manager_use_case_desc
	 * @param current_quota_size The current quota limit that has been reached, in bytes
	 * @param quota_hint         A hint for the new limit that the database might use. This hint is provided
	 *                           by the webpage itself, so don't trust it blindly, but offer the user a choice
	 * @param callback           Callback object to notify when the user replies.
	 **/
	virtual void OnIncreaseQuota(OpWindowCommander* commander, const uni_char* db_name, const uni_char* db_domain, const uni_char* db_type, OpFileLength current_quota_size, OpFileLength quota_hint, QuotaCallback *callback) = 0;
#endif // defined DATABASE_MODULE_MANAGER_SUPPORT

#ifdef WIC_USE_ANCHOR_SPECIAL
	class AnchorSpecialInfo
	{
		const uni_char *rel_str;
		const uni_char *ttl_str;
		const uni_char *urlnamerel_str;
		const uni_char *urlname_str;
	public:
		AnchorSpecialInfo(const uni_char *rel, const uni_char *ttl_str, const uni_char * urlnamerel, const uni_char * urlname)
			: rel_str(rel), ttl_str(ttl_str), urlnamerel_str(urlnamerel), urlname_str(urlname)
			{}
		const uni_char * GetRel() const { return rel_str; }
		const uni_char * GetTitle() const { return ttl_str; }
		const uni_char * GetURLNameRel() const { return urlnamerel_str; }
		const uni_char * GetURLName() const { return urlname_str; }
	};

	/** Called when clicking on an anchor to let the listener determine if this is a special anchor
	 * element and let the listener determine the appropriate action. The implementation should
	 * return without blocking when one have determined the nature of the anchor, subsequent interaction
	 * with the user would be per listener design (suggestion: post a message, use non-blocking, modal dialogs)
	 * @param commander the windowcommander in question
	 * @param info relevant information for the listener
	 * @return TRUE if anchor is of special type handled by the listener, FALSE if not
	 * 				(return value not depending on the result of the possible ui interaction)
	 * 				A default handling of the callback will be to return FALSE */

	virtual BOOL OnAnchorSpecial(OpWindowCommander * commander, const AnchorSpecialInfo & info) = 0;
#endif //WIC_USE_ANCHOR_SPECIAL

#ifdef WIC_USE_ASK_PLUGIN_DOWNLOAD
	/** Called to ask if user wants to donwload plug-in to handle content */
	virtual void OnAskPluginDownload(OpWindowCommander* commander) = 0;
#endif

#ifdef WIC_USE_VIS_RECT_CHANGE
	/** Called when the visible rectangle (viewport of top-level document in window) changes.
	 *
	 * This happens for instance when scrollbars appear/disappear, or when the
	 * window is resized.
	 *
	 * @param commander the windowcommander
	 * @param visible_rect defining where the content will be drawn inside scrollbars
	 */
	virtual void OnVisibleRectChanged(OpWindowCommander* commander, const OpRect& visible_rect) = 0;
#endif //WIC_USE_VIS_RECT_CHANGE

	/** Called when state of the on-demand plugin placeholder has changed.
	 *
	 * Provides platform with a way to update state of any UI widget it might
	 * have for activating on-demand plugins - i.e. hide widget when there
	 * are no more placeholders to activate and show it when new placeholders
	 * appear. Decision what to do with the widget should be based on the state
	 * of the ACTION_ACTIVATE_ON_DEMAND_PLUGINS action.
	 * If the UI widget is hooked into OpInputAction system, it should be enough
	 * to call g_input_manager->UpdateAllInputStates() to update its state.
	 *
	 * @param commander the windowcommander
	 * @param has_placeholders The indication whether Window still has
	 * unactivated placeholders.
	 */
	virtual void OnOnDemandStateChangeNotify(OpWindowCommander* commander, BOOL has_placeholders) = 0;

#ifdef WIC_ORIENTATION_CHANGE
	/** @short The orientation of the screen should be changed.
	 *
	 * @param commander the window commander for which this happened
	 * @param orientation the new orientation of screen
	 */
	virtual void OnOrientationChangeRequest(OpWindowCommander* commander, Orientation orientation) = 0;
#endif //WIC_ORIENTATION_CHANGE

#ifdef WIDGETS_IMS_SUPPORT
	/**
	 * IMSCallback should be used by the platform to either retrieve data from core about a requested IMS,
	 * or submit information (committing, updating or cancelling).
	 */
	class IMSCallback
	{
		public:
		/** Commit the changes made with OnUpdateIMS() to the IMS. Will close the IMS (no other functions can be used after this function has been called). */
		virtual void OnCommitIMS() = 0;
		/** Notify core that an item has been (de)selected in the list. Changes will not take effect until OnCommitIMS() has been called. */
		virtual void OnUpdateIMS(int item, BOOL selected) = 0;
		/** Cancel an IMS. Changes done with OnUpdateIMS() will be discarded. Will close the IMS (no other functions can be used after this function has been called). */
		virtual void OnCancelIMS() = 0;

		enum IMSAttribute
		{
			MULTISELECT,      /** This IMS allow multiple selections. */
			HAS_SCROLLBAR,    /** This IMS has a scrollbar. */
			DROPDOWN,         /** This IMS is a dropdown (single select widget which displays only one entry). */
		};

		enum IMSItemAttribute
		{
			SELECTED,         /** Item is selected. */
			ENABLED,          /** Item is enabled. If not enabled, platform should not allow user to be able to select it. */
			SEPARATOR,        /** Item is a separator. */
			GROUPSTART,       /** Item marks the start of a group (see HTML OPTGROUP start-tag). */
			GROUPSTOP         /** Item marks the end of a group (HTML OPTGROUP end-tag). */
		};

		/** Get number of items. */
		virtual int GetItemCount() = 0;
		/** Get focused item, returns -1 if no focused item. */
		virtual int GetFocusedItem() = 0;
		/** Get the value for an attribute for the widget. */
		virtual int GetIMSAttribute(IMSAttribute attr) = 0;
		/** Get the value for a certain attribute for an item. */
		virtual int GetIMSAttributeAt(int index, IMSItemAttribute attr) = 0;
		/** Get the label for an item. */
		virtual const uni_char * GetIMSStringAt(int index) = 0;
		/** Get Rect (in document coordinates) occupied by the inactive widget. */
		virtual OpRect GetIMSRect() = 0;
	};

	/** The platform should open an IMS.
	 *
	 * The callback object is owned by core, and is valid until any of the submit methods (OnCommitIMS, OnUpdateIMS, OnCancelIMS) has been called,
	 * or until OnIMSCancelled() has been received for the same callback.
	 */
	virtual OP_STATUS OnIMSRequested(OpWindowCommander * commander, IMSCallback* callback) = 0;

	/** The IMS was closed by someone else. The callback object cannot be used when the platform has returned from this method. */
	virtual void OnIMSCancelled(OpWindowCommander * commander, IMSCallback* callback) = 0;

	/** The IMS object has been updated; please use getter methods to retrieve new information. Any updates changed with OnUpdateIMS() will be discarded. */
	virtual void OnIMSUpdated(OpWindowCommander * commander, IMSCallback* callback) = 0;
#endif // WIDGETS_IMS_SUPPORT

#ifdef COOKIE_WARNING_SUPPORT
	/** @short Notification triggered when some document could not write a cookie, because cookies are disabled.
	 *
	 * The purpose of this call is to inform the platform that Opera could not write the cookie.
	 * This can be used to inform a user that a page needs cookies to work correctly.
	 *
	 * @note The document which wanted to write a cookie is not further specified.
	 * It could be the currently loaded document, or some document in a sub-frame,
	 * or some other document which is somehow related to the specified window
	 * commander instance.
	 *
	 * @note Details about the cookie are not available.
	 *
	 * @note Opera may have denied writing a cookie, because the global cookie
	 * preference is disabled, or because a site-preference for the document
	 * (which you don't know) disabled cookies."
	 *
	 * @note This notification can be used to present the user with a useless
	 * and annoying dialog about the fact that some document could not write
	 * a cookie. Useless because the user does not know what to do about this
	 * problem and doesn't know which cookie or document it refers to, and
	 * annoying because the user can't disable the dialog (this was one of CMCC
	 * requiriments - Chineses Operator).
	 *
	 * @param commander the window commander for which this happened
	 */
	virtual void OnCookieWriteDenied(OpWindowCommander* commander) = 0;
#endif // COOKIE_WARNING_SUPPORT

#ifdef DOC_RETURN_TOUCH_EVENT_TO_SENDER
	/** @short A platform supplied touch event has been processed by the document and its response is available.
	 *
	 * The primary purpose of this call is to inform the platform of how the document wishes it to behave,
	 * e.g. if the UI is to pan the display upon a swipe gesture, or if it is to ignore it completely because
	 * the document has handled the swipe gesture itself. An example is Google Maps.
	 *
	 * In addition, the return value of this call allows the platform to affect if and how mouse events are
	 * simulated. If the call returns FALSE, then core will generate suitable mouse events depending on
	 * tweaks defined at compile time. A platform wishing improved control of which mouse events are
	 * generated may use a mechanism such as gogi_mouse_evt to create its own events, and then return
	 * TRUE, whereupon core will do nothing.
	 *
	 * The reason we default to simulating mouse events is that most documents do not support touch events
	 * natively, and hence require a translation. In addition, the unofficial touch event specification
	 * mandates additional mouse events, meaning some documents may expect them regardless. The two methods
	 * currently supported by Core are documented and toggled by TWEAK_DOC_TOUCH_SMARTPHONE_COMPATIBILITY.
	 *
	 * @param commander The window commander controlling the document in which this happened.
	 * @param user_data The identifier originally supplied to the gogi_touch_evt call inducing this touch event.
	 * @param prevented TRUE if the document wishes the platform to perform its default UI response to the event,
	 * FALSE if it wishes the platform to pretend as if the event never occurred.
	 *
	 * @return TRUE if mouse simulation has been taken care of, FALSE if Core should generate suitable
	 * mouse events to ensure compatibility with other browsers and non-touch aware documents.
	 */
	virtual BOOL OnTouchEventProcessed(OpWindowCommander* commander, void* user_data, BOOL prevented) = 0;
#endif // DOC_RETURN_TOUCH_EVENT_TO_SENDER
#ifdef PLUGIN_AUTO_INSTALL
	/** @short Plug-in detected notification
	 *
	 * Called when a plug-in is detected during asynchronous plug-in detection.
	 * This is needed to notify the platform missing plug-in manager about a
	 * newly detected plug-in and that the need to download it is no longer
	 * present.
	 *
	 @param commander The window commander controlling the document which contains the content for which a plug-in is detected
	 @param mime_type The mimetype of the plug-in content.
	 */
	virtual void NotifyPluginDetected(OpWindowCommander* commander, const OpStringC& mime_type) = 0;

	/** @short Plugin missing notification
	 *
	 * Called when a plugin is missing for some content that core encounters while parsing a page. This is
	 * needed to notify the platform missing plugin manager about the fact, which in turn causes the manager
	 * to check availibility of the plugin installer via opera autoupdate mechanism. This allows user to install
	 * the plugin seamlessly with a few clicks.
	 *
	 @param commander The window commander controlling the document which contains the content for which a plugin is missing
	 @param mime_type The mimetype of the content that needs a plugin.
	 */
	virtual void NotifyPluginMissing(OpWindowCommander* commander, const OpStringC& mime_type) = 0;

	/** @short Get plugin information request
	 *
	 * Called when core needs information about a plugin for the given mimetype, this is needed to display the information in
	 * the plugin placeholder. If the platform missing plugin manager was already able to find a plugin for this mimetype,
	 * a call to this method will return the plugin name and availability of the plugin's installer. If the plugin was not
	 * found by the platform manager, this should be called again when platform notifies about the plugin availability via
	 * OnPluginAvailable() callback.
	 *
	 @param commander The window commander controlling the document which contains the content for which a plugin is missing
	 @param mime_type The mimetype of the content that needs a plugin.
	 @param plugin_name[out] If platform has already found a plugin for this mimetype, the plugin name will be returned here, empty
	 otherwise.
	 @param available[out] TRUE if plugin was found to be available for install by the platform, FALSE otherwise.
	 */
	virtual void RequestPluginInfo(OpWindowCommander* commander, const OpStringC& mime_type, OpString& plugin_name, BOOL& available) = 0;

	/** @short Notification about a plugin placeholder being clicked
	 *
	 * Called when core plugin placeholder is clicked. Platform is free to interpret the meaning of this, currently in the desktop
	 * implementation a plugin install dialog is shown when a plugin for the given mimetype is available for installation, a redirect
	 * dialog is shown when Opera can't install the plugin automatically but is able to show a Web page regarding its installation,
	 * UI for enabling a plugin/plugins is shown if a plugin for the given mimetype is disabled.
	 *
	 @param commander The window commander controlling the document which contains the content for which a plugin is missing
	 @param information The PluginInstallInformation struct that describes the clicked placeholder (mime-type, plugin URL).
	 */
	virtual void RequestPluginInstallDialog(OpWindowCommander* commander, const PluginInstallInformation& information) = 0;
#endif // PLUGIN_AUTO_INSTALL

	/** @short Called whenever a top-level document or sub-frame is scrolled.
	 *
	 * @param commander the window commander for which this happened
	 */
	virtual void OnDocumentScroll(OpWindowCommander* commander) = 0;

#ifdef WIC_SETUP_INSTALL_CALLBACK
	/** @short Require user interaction when installing custom setup types
	 *
	 * @param commander the window commander for the document/window that
	 * initiated the download
	 * @param url The url of the object requiring user interaction to
	 * @param content_type The content type as sent by the server
	 * install.
	 */
	virtual void OnSetupInstall(OpWindowCommander* commander, const uni_char* url, URLContentType content_type) = 0;
#endif // WIC_SETUP_INSTALL_CALLBACK

#ifdef REFRESH_DELAY_OVERRIDE
	/** @short Meta refresh descriptor
	 *
	 * Objects of this class describe a meta refresh. It also provide callback
	 * methods that allow to modify the meta refresh.
	 */
	class ReviewRefreshDelayCallback
	{
	public:
		/** @name Methods providing information about the refresh
		 * @{ */
		/** The delay of the refresh */
		virtual int  GetDelaySeconds()           const = 0;
		/** Indicates if this meta refresh is a redirect */
		virtual BOOL IsRedirect()                const = 0;
		/** Is this meta refresh situated in the top document? */
		virtual BOOL IsTopDocumentRefresh()      const = 0;
		/** The URL of the document containing this meta refresh */
		virtual const uni_char *GetDocumentURL() const = 0;
		/** The meta refresh target URL */
		virtual const uni_char *GetRefreshURL()  const = 0;
		/** @} */

		/** @name Callback methods
		 * @{ */
		/** Cancel refresh */
		virtual void CancelRefresh()                   = 0;
		/** Modifies the delay of the meta refresh */
		virtual void SetRefreshSeconds(int seconds)    = 0;
		/** Call it when you don't know what to do with this meta refresh */
		virtual void Default()                         = 0;
		/** @} */

		virtual ~ReviewRefreshDelayCallback() {};
	};

	/** @short Notification about a meta refresh being processed
	 *
	 * This method is called for every meta refresh encountered during parsing
	 * of the document. It allows the platform to modify/cancel/bypass the
	 * meta refresh by calling the respective methods of the cb object
	 * (SetRefreshSeconds/CancelRefresh/Default). The cb object is
	 * automatically destroyed after one of the mentioned callback methods have
	 * been called. The cb object also provides information about the meta
	 * refresh.
	 *
	 * The platform shall either call one of the mentioned callback methods or
	 * delete the callback object. The meta refresh is not scheduled until the
	 * platform calls SetRefreshSeconds(). The call to this method schedules
	 * the meta refresh into the future for the number of seconds given as a
	 * parameter (from the moment the method has been called).
	 *
	 * Calling a callback on an object associated with a FramesDocument that
	 * has been disposed of (deleted) has no effect, although the object still
	 * needs to be deleted or a callback method has to be called.
	 *
	 * @param wc The window commander controlling the document which contains the
	 *           content for which this meta refresh has been encountered.
	 * @param cb An object that provides information about the meta refresh and a
	 *           callback function to modify/cancel the meta refresh.
	 */
	virtual void OnReviewRefreshDelay(OpWindowCommander* wc, OpDocumentListener::ReviewRefreshDelayCallback *cb) = 0;
#endif // REFRESH_DELAY_OVERRIDE

	/** @short A component responsible for this document exited unexpectedly
	 *
	 * Called when a component responsible for (parts of) the document unexpectedly exits (aka. crashes). The UI might choose
	 * to display this information to the user, to give them the opportunity to reload the page or in other ways handle the
	 * unexpected exit.
	 *
	 * @param commander The window commander controlling the document for which a component disappeared
	 * @param type Type of the component that disappeared
	 * @param address Address of the component that disappeared - this uniquely identifies the component
	 * @param information If available, contains extra information about the component that can be passed on to the user
	 *        eg. for type COMPONENT_PLUGIN, contains the path of the plugin that failed
	 */
	virtual void OnUnexpectedComponentGone(OpWindowCommander* commander, OpComponentType type, const OpMessageAddress& address, const OpStringC& information) = 0;

#ifdef KEYBOARD_SELECTION_SUPPORT
	/**
	 * Called when keyboard selection mode has been enabled or
	 * disabled. Enabled via OpWindowCommander::SetKeyboardSelectable.
	 *
	 * @param enabled Set to TRUE when keyboard selection mode was
	 * enabled and FALSE if disabled.
	 */
	virtual void OnKeyboardSelectionModeChanged(OpWindowCommander* commander, BOOL enabled) = 0;
#endif // KEYBOARD_SELECTION_SUPPORT

#ifdef GEOLOCATION_SUPPORT
	/** Notification sent when the set of documents in the specified window commander instance
	 *  which access the geographical location data has changed.
	 *  geographical location.
	 *
	 *  @param wc - The window commander controlling the document which accesses
	 *              the location data.
	 *  @param hostnames - Hostnames of documents which access the location data.
	 *              If the vector is empty, the location data is no longer accessed
	 *              from the specified window commander instance.
	 */
	virtual void OnGeolocationAccess(OpWindowCommander* wc, const OpVector<uni_char>* hostnames) = 0;
#endif // GEOLOCATION_SUPPORT

#ifdef MEDIA_CAMERA_SUPPORT
	/** Notification sent when the set of documents in the specified window commander instance
	 *  which access camera has changed.
	 *
	 *  @param wc - The window commander controlling the document which accesses
	 *              camera.
	 *  @param hostnames - Hostnames of documents which access the camera.
	 *              If the vector is empty, the camera is no longer accessed
	 *              from the specified window commander instance.
	 */
	virtual void OnCameraAccess(OpWindowCommander* wc, const OpVector<uni_char>* hostnames) = 0;
#endif // MEDIA_CAMERA_SUPPORT

#ifdef DOM_FULLSCREEN_MODE
	/**
	 * @short called when the DOM Fullscreen API is used and core requests
	 * the platform to hide the UI and go fullscreen.
	 *
	 * This method is called by core to request the platform to display the
	 * document's tab this listener belongs too as fullscreen or called to
	 * switch back out of fullscreen.
	 *
	 * During the call to element.requestFullscreen(), core will only call
	 * OnJSFullScreenRequest() if the current fullscreen state is
	 * FullScreenState::FULLSCREEN_NONE. This way, if browser was already in
	 * projection mode, or user initiated fullscreen, core will not interfere.
	 *
	 * If the platform acknowledges the request it must call
	 * OpWindowCommander::SetDocumentFullscreenState() on all tabs with a proper
	 * fullscreen state value. The value should be FullScreenState::FULLSCREEN_NORMAL.
	 * Using the FullScreenState::FULLSCREEN_PROJECTION is acceptable.
	 *
	 * During the call to exitFullscreen from the DOM API, if the current fullscreen
	 * mode is FullScreenState::FULLSCREEN_NORMAL, core will call OnJSFullScreenRequest
	 * to disable fullscreen. Else if the state is FullScreenState::FULLSCREEN_PROJECTION
	 * core will assume this is a user initiated fullscreen and will not call
	 * OnJSFullScreenRequest(). Like for the callback triggered by requestFullscreen(),
	 * the platform is also responsible for calling SetDocumentFullscreenState() on all
	 * tabs when exiting fullscreen.
	 *
	 * OnJSFullScreenRequest() will be called for the top document.
	 * The UI shall do whatever it thinks is necessary to implement the request and finally
	 * call OpWindowCommander::SetDocumentFullscreenState(). If it is necessary to resize
	 * the window (i.e., change the visual viewport), then the UI is encouraged to perform
	 * the resizing and the call to OpWindowCommander::SetDocumentFullscreenState() with as
	 * little delay as possible to limit the number of reflows needed when changing the
	 * fullscreen state.
	 *
	 * The platform is responsible for calling OpWindowCommander::SetDocumentFullscreenState()
	 * if the request is accepted, and to call it as well when the user exits fullscreen mode
	 * from the user interface, e.g., by pressing F11 or ESC which is the default key-binding
	 * for toggling the fullscreen or projection mode.
	 *
	 * @param commander          The window commander for the window that did the request.
	 * @param enable_fullscreen  Boolean which tells if fullscreen should be enabled or disabled.
	 *                           If TRUE, fullscreen is to be enabled, if FALSE it is to be disabled.
	 *
	 * @retval OpStatus::OK if the request was acknowledged. In this case the UI is expected to call
	 *         OpWindowCommander::SetDocumentFullscreenState() to report back if the request was accepted or not.
	 *         Calling OpWindowCommander::SetDocumentFullscreenState() may be done synchronously or asynchronously.
	 * @retval OpStatus::ERR_NOT_SUPPORTED if the platform does not support going fullscreen.
	 * @retval OpStatus::ERR_NO_ACCESS if the platform does not allow going fullscreen.
	 * @retval OpStatus::ERR for other errors that don't fit the previous cases.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	virtual OP_STATUS OnJSFullScreenRequest(OpWindowCommander* commander, BOOL enable_fullscreen) = 0;
#endif // DOM_FULLSCREEN_MODE
};

#ifdef INTERNAL_SPELLCHECK_SUPPORT

/** @short Spell checker session.
 *
 * This class encapsulates the actions the platform can perform which is
 * associated with a particular instance of a texarea, input-field or document
 * using designMode or contenteditable. It may also be associated with a
 * particular word in that context. If, e.g. the user right-clicks at a
 * misspelled word in a textarea, then will a session ID be created (placed in
 * PopupMenuInfo::spell_session_id). If an OpSpellUiSession then is created
 * using OpSpellUiSession::Create("spell session ID delivered by core"..., and
 * the platform later performs session->AddWord(), in that case will the
 * associated word be added to the dictionary.
 *
 * Another example is if the user clicks somewhere in the textarea where there
 * is no text - then will an session ID be created as well, but when an
 * OpSpellUiSession is created from that id will session->CanAddWord() return
 * FALSE and session->AddWord() will have no effect. But other functions like
 * session->EnableSpellcheck() and session->DisableSpellcheck() will work,
 * depending on if Spellchecking is already enabled or if it's disabled.  If
 * however the platforms are given an spell session ID of 0 from core, this
 * means that no session has been created and that it's no need to create any
 * OpSpellUiSession object.
 */
class OpSpellUiSession
{
public:
	virtual ~OpSpellUiSession() { }

	/** @short Create an OpSpellUiSession instance.
	 *
	 * Static create function which takes a spell session id delivered by core
	 * and creates an OpSpellUiSession object.
	 *
	 * @param spell_session_id The session ID given by core, if it's 0 - the
	 * session object will not be valid (IsValid() will return FALSE, see
	 * below).
	 * @param session The session object created, will be set to NULL in case
	 * of an error.
	 * @return Error status code in case of an error.
	 */
	static OP_STATUS Create(int spell_session_id, OpSpellUiSession **session);

	/** @short Is the instance valid?
	 *
	 * Returns whether this instance is still valid or not, it might be invalid
	 * if e.g. the associated word is changed, if the textarea dissapears,
	 * etc. Then all the actions, like AddWord() will have no effect.
	 */
	virtual BOOL IsValid() = 0;

	/** @return TRUE if the session is associated with a word that could be
	 * added/ignored OR removed from the current dictionary. */
	virtual BOOL HasWord() = 0;

	/** Write the word associated with the session into word, or "" if there is
	 * no such word. */
	virtual OP_STATUS GetCurrentWord(OpString &word) = 0;

	/** @short Get the current selected language index.
	 *
	 * @return the index (see the two functions below) of the currently
	 * selected language (dictionary), or -1 if spellchecking is disabled or
	 * the session is not valid anymore, OR in the special-case that
	 * spellchecking is enabled, but the dictionary file has been deleted after
	 * it was loaded.
	 */
	virtual int GetSelectedLanguageIndex() = 0;

	/** @short Get the number of languages.
	 *
	 * @return the number of installed dictionaries in the system.
	 */
	virtual int GetInstalledLanguageCount() = 0;

	/** @return a string-identifier for the dictionary with index index, index
	 * should be >= 0 AND <= GetInstalledLanguageCount()-1, otherwise NULL
	 * will be returned. A string-identifier could be something like "en_US"
	 * or "sv_SE".
	 */
	virtual const uni_char* GetInstalledLanguageStringAt(int index) = 0;

#ifdef WIC_SPELL_DICTIONARY_FULL_NAME
	/** @return the full language name for the dictionary with index index, index
		* should be >= 0 AND <= GetInstalledLanguageCount()-1, otherwise NULL
		* will be returned.
		*/
	virtual const uni_char* GetInstalledLanguageNameAt(int index) = 0;
#endif // WIC_SPELL_DICTIONARY_FULL_NAME

	/** @short Get the version number of the dictionary used for the language.
	 *
	 * The version number is read from the Opera dictionary meta data file.
	 * Valid version numbers are integers greater than 0.
	 *
	 * @return The version number if it can be determined. 0 if no metadata
	 * file exists (e.g. the dictionary doesn't come from Opera). -1 on
	 * failure.
	 */
	virtual int GetInstalledLanguageVersionAt(int index) = 0;

	/** @short Set active language
	 *
	 * Set the spellchecking-language (dictionary) for the associated
	 * textarea/input-field/document to the lanugage identified by
	 * language-identifier lang, like "en_US" or "sv_SE". Could be used for
	 * changing the current language or to enable spellchecking with the
	 * specified language.
	 */
	virtual BOOL SetLanguage(const uni_char *lang) = 0;

	/** @short Get the number of alternatives to a misspelled word.
	 *
	 * @return the number of replacement-suggestions for the associated
	 * misspelled word, or 0 if there are no associated misspelled word, or if
	 * there simply are no suggestions.
	 */
	virtual int GetReplacementStringCount() = 0;

	/** @short Get one of the replacement-suggestions.
	 *
	 * Get replacement-suggestion with index replacement_index into value.
	 *
	 * @param replace_index The index which should be >= 0 && <=
	 * GetReplacementStringCount()-1
	 * @param value The replacement-suggestion, or "" if replace_index is not
	 * within the valid range or if the session is not valid anymore.
	 * @return Error status code in case of an error.
	 */
	virtual OP_STATUS GetReplacementString(int replace_index, OpString &value) = 0;

	/** @return TRUE if spellchecking is disabled and can be enabled (with the
	 * current default-dictionary) by calling EnableSpellcheck() */
	virtual BOOL CanEnableSpellcheck() = 0;

	/** @return TRUE if spellchecking can be disabled, i.e. if
	 * DisableSpellcheck() will have any effect. */
	virtual BOOL CanDisableSpellcheck() = 0;

	/** @return TRUE if a misspelled word is associated with the session,
	 * i.e. if AddWord() will have any effect. */
	virtual BOOL CanAddWord() = 0;

	/** @return TRUE if a misspelled word is associated with the session,
	 * i.e. if IgnoreWord() will have any effect. */
	virtual BOOL CanIgnoreWord() = 0;

	/** @return TRUE if a correctly spelled word is associated with the
	 * session, i.e. if RemoveWord() will have any effect, that is, to remove
	 * the word from the dictionary. */
	virtual BOOL CanRemoveWord() = 0;

	/** @short Enable spellchecking.
	 *
	 * Enables spellchecking with the current default-dictionary if
	 * spellchecking isn't already enabled, return TRUE if the operation
	 * succeeded and spellchecking wasn't already enabled.
	 */
	virtual BOOL EnableSpellcheck() = 0;

	/** @short Disable spellchecking
	 *
	 * Disables spellchecking, returns FALSE if spellchecking was already
	 * disabled.
	 */
	virtual BOOL DisableSpellcheck() = 0;

	/** @short Add a word to the dictionary.
	 *
	 * Adds the associated misspelled word to the dictionary (if there is
	 * such), return error upon failure. OpStatus::OK will be returned even
	 * though the word wasn't added (e.g. if it already was in the dictionary),
	 * unless OOM or such error occured.
	 */
	virtual OP_STATUS AddWord() = 0;

	/** @short Ignore misspelled word.
	 *
	 * Ignores all misspellings of the associated misspelled word in the
	 * context (e.g. the textarea), return error upon failure. OpStatus::OK
	 * will be returned even though the word wasn't ignored (e.g. if it was
	 * present in the dictionary), unless OOM or such error occured.
	 */
	virtual OP_STATUS IgnoreWord() = 0;

	/** @short Remove word from dictionary.
	 *
	 * Removes the associated correctly spelled word from the dictionary (if
	 * there is such), return error upon failure. OpStatus::OK will be returned
	 * even though the word wasn't removed (e.g. if it wasn't in the dictionary
	 * or ignored), unless OOM or such error occured.
	 */
	virtual OP_STATUS RemoveWord() = 0;

	/** @short Replace word.
	 *
	 * Replaces the associated misspelled word (if there is such) with the
	 * replacement suggestion identified by replace_index, return error upon
	 * failure.  OpStatus::OK will be returned even though the replacement
	 * didn't take place (e.g. if the index isn't correct or if the session
	 * isn't valid anymore.  Other error-codes will only be returned upon OOM
	 * errors and similary "severe" problems.
	 */
	virtual OP_STATUS ReplaceWord(int replace_index) = 0;

    /** @short Reads the license file
     *
     * Reads the license file if there is one, and insert the license
     * text into license_text. If the license file path can't be
     * determined, the return value will be
     * OpStatus::ERR_FILE_NOT_FOUND and license_text will be an empty
     * string.
     *
     * @param index is the index of a "valid" language. See also
     *  GetInstalledLanguageCount() get the number of all installed
     *  languages.
     * @param license_text receives on output the content of the
     *  associated license file.
     * @return OpStatus::OK on success;
     *  OpStatus::ERR_FILE_NOT_FOUND if the license file path or the
     *  file is not available;
     *  OpStatus::ERR_NO_MEMORY if there is not enough memory to load
     *  the content of the license file.
     */
	virtual OP_STATUS GetInstalledLanguageLicenceAt(int index, OpString& license_text) = 0;
};

#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION

class OpDocumentContext;

/**	This class repesents one interactive item inside a document.
	The instances of that class store: the type of the object, it's region represented
	by an array of parallelograms (rectangles or rectangles with a transformation matix attached),
	and some basic data like an url and a title strings associated with the object.
	Subclasses (depending on a type) store also: OpDocumentContext, checkbox state. */

class InteractiveItemInfo : public ListElement<InteractiveItemInfo>
{
public:

    enum Type
	{
		INTERACTIVE_ITEM_TYPE_ANCHOR,
		INTERACTIVE_ITEM_TYPE_BUTTON,
		INTERACTIVE_ITEM_TYPE_CUSTOM_POINTER_ACTION,
#ifdef TOUCH_EVENTS_SUPPORT
		INTERACTIVE_ITEM_TYPE_CUSTOM_TOUCH_ACTION,
#endif // TOUCH_EVENTS_SUPPORT
		INTERACTIVE_ITEM_TYPE_FORM_INPUT,
		INTERACTIVE_ITEM_TYPE_FORM_CHECKBOX,
		INTERACTIVE_ITEM_TYPE_FORM_RADIO,
		INTERACTIVE_ITEM_TYPE_FORM_SELECT,
		INTERACTIVE_ITEM_TYPE_SCROLLBAR,
		INTERACTIVE_ITEM_TYPE_USEMAP_AREA,
		INTERACTIVE_ITEM_TYPE_OTHER
    };

	struct ItemRect
	{
		OpRect		rect;
		AffinePos*	affine_pos;
	};

	/** Creates an interactive item info instance.
	 * @return an interactive item info instance or NULL in case of OOM. */
	static InteractiveItemInfo* CreateInteractiveItemInfo(unsigned int num_rects, Type type);

	virtual			~InteractiveItemInfo();

	Type			GetType() const { return m_type; }
	OpString&		GetURL() { return this->url; }

	OP_STATUS		SetURL(const uni_char* url) { return this->url.Set(url); }
	OP_STATUS		SetURL(const OpString8& url) { return this->url.Set(url); }

	/** Prepends pos (translation or transform) to the each rect in ItemRects array.
	 *	If possible just skips the affine_pos of a given rect and offsets it.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY in case of OOM. */
	OP_STATUS		PrependPos(const AffinePos& pos);

#ifdef CSS_TRANSFORMS
	/**	Computes a BBox of each rectangle and sets its affine_pos to NULL. */
	void			ChangeToBBoxes();
#endif // CSS_TRANSFORMS

	/** Returns the array of item's rects.
	 *	The array length is the same as the item's number of rectangles (GetNumberOfRects()).
	 * @return An array. Never NULL. */
	ItemRect*		GetRects() {return item_rects;}
	unsigned int	GetNumberOfRects() const { return num_rects; }

	/** If type is INTERACTIVE_ITEM_TYPE_FORM_CHECKBOX, this function will return state of checkbox (TRUE if checked). */
	virtual BOOL	GetCheckboxState() const { return FALSE; }
	/** Not dummy in case of INTERACTIVE_ITEM_TYPE_FORM_CHECKBOX. */
	virtual void	SetCheckboxState(BOOL state) {}

	// The following two are not dummy in case of InputFieldInfo
	virtual void	SetDocumentContext(OpDocumentContext* ctx) {}
	virtual OpDocumentContext* GetDocumentContext() {return NULL;}

protected:

					InteractiveItemInfo(unsigned int num_rects, Type type)
					: m_type(type)
					, item_rects(NULL)
					, num_rects(num_rects) {}

private:

	Type			m_type;
	ItemRect*		item_rects;
	unsigned int	num_rects;
	OpString		url;
};

// Used for checkboxes

class CheckboxInfo : public InteractiveItemInfo
{
public:
	friend class InteractiveItemInfo;

	virtual BOOL	GetCheckboxState() const {return m_state;}
	virtual void	SetCheckboxState(BOOL state) {m_state = state;}

protected:
					CheckboxInfo(int num_rects)
					: InteractiveItemInfo(num_rects, INTERACTIVE_ITEM_TYPE_FORM_CHECKBOX)
					, m_state(FALSE) {}
private:

	BOOL			m_state;
};

// Used for text fields and areas

class InputFieldInfo : public InteractiveItemInfo
{
public:
	friend class InteractiveItemInfo;

					~InputFieldInfo();

	virtual void	SetDocumentContext(OpDocumentContext* ctx) {this->ctx = ctx;}
	virtual OpDocumentContext* GetDocumentContext() {return ctx;}

protected:
					InputFieldInfo(int num_rects)
					: InteractiveItemInfo(num_rects, INTERACTIVE_ITEM_TYPE_FORM_INPUT)
					, ctx(NULL) {}

private:

	OpDocumentContext* ctx;
};
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

/** @short Interface that allows the UI to remember a certain context and get information about it.
 *
 * The object is created by core code and sent to the UI. The UI can create a
 * copy of it to keep or extract information from it immediately. Any copy is
 * owned by the UI. An object might stop being useful if the document the
 * object is based on changes too much or is removed.
 *
 * This class is on an interface and has no state at all. All methods are
 * getters and the object can't be modified by the user.
 */
class OpDocumentContext
{
private:
	/** @short Not implemented. Copy with CreateCopy(). */
	OpDocumentContext& operator=(const OpDocumentContext& other);
	/** @short Not implemented. Copy with CreateCopy(). */
	OpDocumentContext(const OpDocumentContext& other);
protected:
	/** @short This class will only be an abstract interface. Should not exist in itself. */
	OpDocumentContext() {}
public:
	virtual ~OpDocumentContext() {}

	/** @short Create a copy that can be saved by the UI and is owned by the creator.
	 *
	 * @return NULL if OOM.
	 */
	virtual OpDocumentContext* CreateCopy() = 0;

	/** @short Return the position on the screen for the document position.
	 *
	 * Typically used to position context menus correctly. In case of keyboard
	 * initiated actions this point may be far from the current mouse position.
	 */
	virtual OpPoint GetScreenPosition() = 0;

	/** @return TRUE if there was a link at the context position. */
	virtual BOOL HasLink() = 0;

	/** @return TRUE if there was an image at the document position. */
	virtual BOOL HasImage() = 0;

	/** @return TRUE if there is a background image in the document. */
	virtual BOOL HasBGImage() = 0;

	/** @return TRUE if the context is connected to a text selection. */
	virtual BOOL HasTextSelection() = 0;

	/** @return TRUE if there was a mailto link at the context position. */
	virtual BOOL HasMailtoLink() = 0;

#ifdef SVG_SUPPORT
	/** @return TRUE if there was an SVG document fragment at the context position. */
	virtual BOOL HasSVG() = 0;

	/** @return TRUE if there are SVG animations in the SVG document fragment. */
	virtual BOOL HasSVGAnimation() = 0;

	/** @return TRUE if the SVG is currently running animations. */
	virtual BOOL IsSVGAnimationRunning() = 0;

	/** @return TRUE if the SVG allows zooming and panning. */
	virtual BOOL IsSVGZoomAndPanAllowed() = 0;
#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
	/** @return TRUE if there is a media element (running or paused) at the context position. */
	virtual BOOL HasMedia() = 0;

	/** @return TRUE if there is a video element (running or paused) at the context position. */
	virtual BOOL IsVideo() = 0;

	/** @return TRUE if the media content is currently playing. */
	virtual BOOL IsMediaPlaying() = 0;

	/** @return TRUE if the media content is muted. */
	virtual BOOL IsMediaMuted() = 0;

	/** @return TRUE is media controls are enabled. */
	virtual BOOL HasMediaControls() = 0;

	/** @return TRUE if the media content is available. */
	virtual BOOL IsMediaAvailable() = 0;

	/** @short Get media address (URL).
	 *
	 * Returns the address to the media element that was at the document position
	 * or an empty string if there was none.
	 *
	 * The string might be usable for display. It doesn't contain all relevant
	 * information related to the URL so it might not work fully as a
	 * web address. For instance password information will be lost.
	 *
	 * @param[out] media_address Send in a pointer to an OpString object that
	 * will receive the string. Sending in NULL here will result in a
	 * crash. The returned string might be empty.
	 *
	 * @return OK if successful, or ERR_NO_MEMORY if the operation couldn't be
	 * completed because of lack of memory.
	 */
	virtual OP_STATUS GetMediaAddress(OpString* media_address) = 0;

	/** @short Get the URL context of the media address (URL).
	 *
	 * Not all addresses are the same even though they look the same. To
	 * separate them, they have a context number. This returns this.
	 */
	virtual URL_CONTEXT_ID GetMediaAddressContext() = 0;
#endif // MEDIA_HTML_SUPPORT

#ifdef _PLUGIN_SUPPORT_
	/** @return TRUE if the context position is over a plug-in. */
	virtual BOOL HasPlugin() = 0;
#endif

	/** @return TRUE if the context position is in editable text (textarea, text field, editable HTML document, ...) */
	virtual BOOL HasEditableText() = 0;

	/** @return TRUE if the creation of the context was initiated from the keyboard event. */
	virtual BOOL HasKeyboardOrigin() = 0;

	/** @return TRUE if the context position is at a form element. */
	virtual BOOL IsFormElement() = 0;

#ifdef WIC_TEXTFORM_CLIPBOARD_CONTEXT
	/** @return TRUE if input type of text form element is set to password. */
	virtual BOOL IsTextFormElementInputTypePassword() = 0;

	/** @return TRUE if input type of text form element is set to file. */
	virtual BOOL IsTextFormElementInputTypeFile() = 0;

	/** @return TRUE if text form element does not contain any text. */
	virtual BOOL IsTextFormElementEmpty() = 0;

	/** @return TRUE if text form element is multiline. */
	virtual BOOL IsTextFormMultiline() = 0;
#endif // WIC_TEXTFORM_CLIPBOARD_CONTEXT

	/** @short Are we in a "read-only" context?
	 *
	 * Basically checks if a "Paste" operation would succeed at this point.
	 *
	 * @return TRUE if the position is read-only so a paste would fail. Returns
	 * FALSE otherwise.
	 */
	virtual BOOL IsReadonly() = 0;

	/**
	 * @short Get link address (URL).
	 *
	 * Returns the url that the link at the context position has. Returns an
	 * empty string if there was no link at the position. See HasLink().
	 *
	 * The string might be usable for display. It doesn't contain all relevant
	 * information related to the URL so it might not work fully as a
	 * string. For instance password information will be lost.
	 *
	 * @param[out] link_address Send in a pointer to an OpString object that
	 * will receive the address string. Sending in NULL here will result in a
	 * crash. The returned string might be empty.
	 *
	 * @return OK if successful, or ERR_NO_MEMORY if the operation couldn't be
	 * completed because of lack of memory.
	 */
	virtual OP_STATUS GetLinkAddress(OpString* link_address) = 0;

	/** @short Get link title.
	 *
	 * If the context position has a link (see HasLink()), then this
	 * method returns the title attribute of the link. If the context
	 * position has no link, the output parameter is set to an empty
	 * string.
	 *
	 * Example: For the link <a title="foo">text</a>, this method
	 * returns "foo" in the output parameter.
	 *
	 * @param[out] link_title Send in a pointer to an OpString object
	 *  that receives the title-attribute of the link. The pointer
	 *  shall not be NULL. The value of the returned string may be
	 *  empty.
	 *
	 * @return OK if successful, or ERR_NO_MEMORY if out of memory.
	 */
	virtual OP_STATUS GetLinkTitle(OpString* link_title) = 0;

#ifdef WIC_DOCCTX_LINK_DATA
	/**
	 * @short Get text inside an anchor (link)
	 *
	 * If the context position has a link (see HasLink()), then this
	 * method returns the text inside the anchor element. If the
	 * context position has no link, the output parameter is set to an
	 * empty string.
	 *
	 * Example: For the link <a href="foo">text</a>, this method
	 * returns "text" in the output parameter.
	 *
	 * @note If the anchor element includes multiple nested
	 *  child-elements, then the behavior of this method is currently
	 *  undefined.
	 *
	 * @param[out] link_text Send in a pointer to an OpString object
	 *  that receives the text of the link. The pointer shall not be
	 *  NULL. The value of the returned string may be empty.
	 *
	 * @return OK if successful, or ERR_NO_MEMORY if out of memory.
	 */
	virtual OP_STATUS GetLinkText(OpString* link_text) = 0;
#endif // WIC_DOCCTX_LINK_DATA

	/**
	 * Enum to specify different types of data to extract from the link.
	 *
	 * ForUI means that the password will be removed/hidden if there was one in the url.
	 */
	enum LinkDataType
	{	                        // OpString  OpString8   Short explanation
		AddressForUI,           ///<  Yes        No       - w/fragment
		AddressForUIEscaped,    ///<  No         Yes      - w/fragment escaped
		AddressNotForUI,        ///<  Yes        No       - w/fragment/username/password
		AddressNotForUIEscaped  ///<  Yes        No       - w/fragment/username/password escaped
	};

	/**
	 * Extracts data (address in different formats typically) in the form of a string.
	 * @see LinkDataType for possible values. Only those that support OpString can be
	 * used in this method.
	 *
	 * @return OK if successful, or ERR_NO_MEMORY if the operation couldn't be
	 * completed because of lack of memory.
	 */
	virtual OP_STATUS GetLinkData(LinkDataType data_type, OpString* link_data) = 0;

	/**
	 * Extracts data (address in different formats typically) in the form of a string.
	 * @see LinkDataType for possible values. Only those that support OpString can be
	 * used in this method.
	 *
	 * @return OK if successful, or ERR_NO_MEMORY if the operation couldn't be
	 * completed because of lack of memory.
	 */
	virtual OP_STATUS GetLinkData(LinkDataType data_type, OpString8* link_data) = 0;

	/** @short Get image address (URL).
	 *
	 * Returns the address to the image that was at the document position or an
	 * empty string if there was none.
	 *
	 * The string might be usable for display. It doesn't contain all relevant
	 * information related to the URL so it might not work fully as a
	 * web address. For instance password information will be lost.
	 *
	 * @param[out] image_address Send in a pointer to an OpString object that
	 * will receive the string. Sending in NULL here will result in a
	 * crash. The returned string might be empty.
	 *
	 * @return OK if successful, or ERR_NO_MEMORY if the operation couldn't be
	 * completed because of lack of memory.
	 */
	virtual OP_STATUS GetImageAddress(OpString* image_address) = 0;

	/** @short Get background image address (URL).
	 *
	 * Returns the address to the background image of the document or an
	 * empty string if there was none.
	 *
	 * The string might be usable for display. It doesn't contain all relevant
	 * information related to the URL so it might not work fully as a
	 * string. For instance password information will be lost.
	 *
	 * @param[out] image_address Send in a pointer to an OpString object that
	 * will receive the string. Sending in NULL here will result in a
	 * crash. The returned string might be empty.
	 *
	 * @return OK if successful, or ERR_NO_MEMORY if the operation couldn't be
	 * completed because of lack of memory.
	 */
	virtual OP_STATUS GetBGImageAddress(OpString* image_address) = 0;

	/** @short Get suggested filename for image
	 *
	 * Returns a suggested filename for the image that was at the document position
	 * or an empty string if there was none.
	 *
	 * @param[out] filename Send in a pointer to an OpString object that
	 * will receive the string. Sending in NULL here will result in a
	 * crash. The returned string might be empty.
	 *
	 * @return OK if successful, or ERR_NO_MEMORY if the operation couldn't be
	 * completed because of lack of memory.
	 */
	virtual OP_STATUS GetSuggestedImageFilename(OpString* filename) = 0;

	/** @short Get the URL context of the image address (URL).
	 *
	 * Not all addresses are the same even though they look the same. To
	 * separate them, they have a context number. This returns this.
	 */
	virtual URL_CONTEXT_ID GetImageAddressContext() = 0;

	/** @short Get the URL context of the background image address (URL).
	 *
	 * Not all addresses are the same even though they look the same. To
	 * separate them, they have a context number. This returns this.
	 */
	virtual URL_CONTEXT_ID GetBGImageAddressContext() = 0;

	/** @short Get an image's ALT text.
	 *
	 * Returns an alternative text set for the image. If there was no alt text
	 * set image_alt_text.CStr() will be NULL. If it was the empty string it
	 * will be the empty string.
	 *
	 * @param[out] image_alt_text Send in a pointer to an OpString object that
	 * will receive the string. Sending in NULL here will result in a
	 * crash. The returned string might be empty.
	 *
	 * @return OK if successful, or ERR_NO_MEMORY if the operation couldn't be
	 * completed because of lack of memory.
	 */
	virtual OP_STATUS GetImageAltText(OpString* image_alt_text) = 0;

	/** @short Get an image's LONGDESC text.
	 *
	 * Returns a long description text set for the image. If there was no longdesc text
	 * set image_longdesc.CStr() will be NULL. If it was the empty string it
	 * will be the empty string.
	 *
	 * @param[out] image_longdesc Send in a pointer to an OpString object that
	 * will receive the string. Sending in NULL here will result in a
	 * crash. The returned string might be empty.
	 *
	 * @return OK if successful, or ERR_NO_MEMORY if the operation couldn't be
	 * completed because of lack of memory.
	 */
	virtual OP_STATUS GetImageLongDesc(OpString* image_longdesc) = 0;

	/** @short Does the image (if any) have its data cached?
	 *
	 * Checks if the image has raw data that is cached, so that it can be saved
	 * to disk for instance.
	 *
	 * Only meaningful if HasImage() returns TRUE. Otherwise this will return
	 * FALSE.
	 */
	virtual BOOL HasCachedImageData() = 0;

	/** @short Does the background image (if any) have its data cached?
	 *
	 * Checks if the background image has raw data that is cached, so that it can be saved
	 * to disk for instance.
	 *
	 * Only meaningful if HasBGImage() returns TRUE. Otherwise this will return
	 * FALSE.
	 */
	virtual BOOL HasCachedBGImageData() = 0;

#ifdef WEB_TURBO_MODE
	/** @short Is the image (if any) loaded in full quality?
	 *
	 * Checks if the image is loaded in full quality. Will return FALSE if
	 * FEATURE_WEB_TURBO_MODE is enabled and the image was not loaded directly
	 * from the origin server.
	 *
	 * Only meaningful if HasImage() returns TRUE. Otherwise this will return
	 * FALSE.
	 */
	virtual BOOL HasFullQualityImage() = 0;
#endif // WEB_TURBO_MODE

	/** @short Get the OpView.
	 *
	 * @return The OpView this document context position is in, or NULL if no
	 * such view could be located. Typically used for positioning a context
	 * menu in the platform window hierarchy.
	 */
	virtual OpView* GetView() = 0;

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/** @short Get Spell checker session ID.
	 *
	 * @return The spell session ID related to this document position. Can be
	 * used by the platform to create an OpSpellUiSession object with
	 * OpSpellUiSession::Create().
	 */
	virtual int GetSpellSessionId() = 0;
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef WIC_USE_DOWNLOAD_CALLBACK
	/** @short Create URLInformation object for image.
	 *
	 * Returns a heavy weight object for more advanced interaction with the
	 * address.
	 *
	 * @param[out] url_info If the method returns OpStatus::OK, then this is
	 * the created url_info object. Is owned by the caller and should be freed
	 * with OP_DELETE.
	 *
	 * @return ERR if there was no address or ERR_NO_MEMORY if an out of memory
	 * situation occurred. If the object was created correctly, OK will be
	 * returned.
	 *
	 * See HasImage() and GetImageAddress().
	 */
	virtual OP_STATUS CreateImageURLInformation(URLInformation** url_info) = 0;

	/** @short Create URLInformation object for background image.
	 *
	 * Returns a heavy weight object for more advanced interaction with the
	 * address.
	 *
	 * @param[out] url_info If the method returns OpStatus::OK, then this is
	 * the created url_info object. Is owned by the caller and should be freed
	 * with OP_DELETE.
	 *
	 * @return ERR if there was no address or ERR_NO_MEMORY if an out of memory
	 * situation occurred. If the object was created correctly, OK will be
	 * returned.
	 *
	 * See HasBGImage() and GetBGImageAddress().
	 */
	virtual OP_STATUS CreateBGImageURLInformation(URLInformation** url_info) = 0;

	/** @short Create URLInformation object for document.
	 *
	 * Returns a heavy weight object for more advanced interaction with the
	 * address.
	 *
	 * @param[out] url_info If the method returns OpStatus::OK, then this is
	 * the created url_info object. Is owned by the caller and should be freed
	 * with OP_DELETE.
	 *
	 * @return ERR if there was no address or ERR_NO_MEMORY if an out of memory
	 * situation occurred. If the object was created correctly, OK will be
	 * returned.
	 *
	 */
	virtual OP_STATUS CreateDocumentURLInformation(URLInformation** url_info) = 0;

	/** @short Create URLInformation object for link.
	 *
	 * Returns a heavy weight object for more advanced interaction with the
	 * address.
	 *
	 * @param[out] url_info If the method returns OpStatus::OK, then this is
	 * the created url_info object. Is owned by the caller and should be freed
	 * with OP_DELETE.
	 *
	 * @return ERR if there was no address or ERR_NO_MEMORY if an out of memory
	 * situation occurred. If the object was created correctly, OK will be
	 * returned.
	 *
	 * See HasLink(), GetLinkTitle() and GetLinkAddress().
	 */
	virtual OP_STATUS CreateLinkURLInformation(URLInformation** url_info) = 0;
#endif // WIC_USE_DOWNLOAD_CALLBACK
};

#ifdef _POPUP_MENU_SUPPORT_

class OpMenuListener
{
public:
	virtual ~OpMenuListener() {}

	/** @short Core wants to display a popup menu.
	 *
	 * @param commander The windowcommander of a Window which requested a menu.
	 *
	 * @param context The context for the popup menu (provider of information
	 * about the selected link, image, text, etc.). Must be copied by the
	 * callee (OpDocumentContext::CreateCopy()) if it wants to keep it after it
	 * returns, as the original 'context' object may be deleted at any point
	 * then.
	 * @param message Additional information about content of context menu requested.
	 * Core keeps ownership of it and listener must not assume it lives beyond the
	 * scope of this method.
	 * @see OpWindowCommander::OnMenuItemClicked()
	 * @see OpWindowCommander::GetMenuItemIcon()
	 */
	virtual void OnPopupMenu(OpWindowCommander* commander, OpDocumentContext& context, const OpPopupMenuRequestMessage* message) = 0;

# ifdef WIC_MIDCLICK_SUPPORT
	/** @short Core is informing UI that there has been a middle button click
	 *
	 * @param commander The windowcommander
	 *
	 * @param context The context for the popup menu (provider of information
	 * about the selected link, image, text, etc.). Must be copied by the
	 * callee (OpDocumentContext::CreateCopy()) if it wants to keep it after it
	 * returns, as the original 'context' object may be deleted at any point
	 * then.
	 *
	 * @param mousedown info on whether the event is on a mouse-down or mouse-up
	 *
	 * @param modifiers info on key modifiers (shift keys)
	 */
	virtual void OnMidClick(OpWindowCommander * commander, OpDocumentContext& context,
	                        BOOL mousedown, ShiftKeyState modifiers) = 0;
# endif // WIC_MIDCLICK_SUPPORT

	/** Called when the menu should be disposed, for example when the
        user clicks somewhere else. */
	virtual void OnMenuDispose(OpWindowCommander* commander) = 0;
};
#endif // _POPUP_MENU_SUPPORT_

/** Listens for stuff coming in the "link" element.*/
class OpLinkListener
{
public:
	virtual ~OpLinkListener() {}

	/** Called when a certain type of link element is available. */
	virtual void OnLinkElementAvailable(OpWindowCommander* commander) = 0;

	/** Called when there is alternative css available.
		Question: can we reflow and remove these?
	*/
	virtual void OnAlternateCssAvailable(OpWindowCommander* commander) = 0;
};

#ifdef ACCESS_KEYS_SUPPORT

/** @short Listener for access keys that are being added and removed. */
class OpAccessKeyListener
{
public:
	virtual ~OpAccessKeyListener() {}

	/** @short An access key was added.
	 *
	 * @param commander The windowcommander
	 * @param key Unicode or OP_KEY representing the key added
	 * @param caption The caption that should be displayed for the access key
	 * @param url The URL that will be activated when pressing the access key
	 */
	virtual void OnAccessKeyAdded(OpWindowCommander* commander, OpKey::Code key, const uni_char* caption, const uni_char* url) = 0;

	/** @short An access key was removed.
	 *
	 * @param commander The windowcommander
	 * @param key Unicode or OP_KEY representing the key removed
	 */
	virtual void OnAccessKeyRemoved(OpWindowCommander* commander, OpKey::Code key) = 0;
};

#endif // ACCESS_KEYS_SUPPORT

#ifdef _PRINT_SUPPORT_
/** Listens for printing activity. */
class OpPrintingListener
{
public:

	class PrintInfo
	{
	public:
		enum PrintStatus { PRINTING_STARTED, PAGE_PRINTED, PRINTING_DONE, PRINTING_ABORTED };

		PrintInfo() : status(PRINTING_DONE), lastPage(0), maxPage(0), currentPage(0) {}

		PrintStatus status;
		UINT32 lastPage;
		UINT32 maxPage;
		UINT32 currentPage;
	};

	virtual ~OpPrintingListener() {}

	/* Called when core attempts to start a print session */
	virtual void OnStartPrinting(OpWindowCommander* commander) = 0;

	/* Called when a page has been printed. */
	virtual void OnPrintStatus(OpWindowCommander* commander, const PrintInfo* info) = 0;
};
#endif // _PRINT_SUPPORT_

class OpErrorListener
{
public:
	enum OperaDocumentError { EC_DOCUMENT_ALLOC = 0, EC_DOCUMENT_UNSUPPORTED,
						   EC_DOCUMENT_EMPTY_DOC, EC_DOCUMENT_INVALID_DOC,
						   EC_DOCUMENT_JAVASCRIPT, EC_DOCUMENT_PLUGIN };
	enum FileError { EC_FILE_NOT_FOUND = 0, EC_FILE_REFUSED };
	enum HttpError { EC_HTTP_CONTINUE = 0, EC_HTTP_SWITCHING_PROTOCOLS,
					 EC_HTTP_OK, EC_HTTP_CREATED, EC_HTTP_ACCEPTED,
					 EC_HTTP_NON_AUTHORITIVE_INFORMATION, EC_HTTP_NO_CONTENT,
					 EC_HTTP_RESET_CONTENT, EC_HTTP_PARTIAL_CONTENT,
					 EC_HTTP_MULTIPLE_CHOICES, EC_HTTP_MOVED_PERMANENTLY,
					 EC_HTTP_MOVE_TEMPORARILY, EC_HTTP_SEE_OTHER,
					 EC_HTTP_NOT_MODIFIED, EC_HTTP_USE_PROXY,
					 EC_HTTP_BAD_REQUEST, EC_HTTP_UNAUTHORISED,
					 EC_HTTP_PAYMENT_REQUIRED, EC_HTTP_FORBIDDEN,
					 EC_HTTP_NOT_FOUND, EC_HTTP_METHOD_NOT_ALLOWED,
					 EC_HTTP_NOT_ACCEPTABLE, EC_HTTP_PROXY_AUTH_REQUIRED,
					 EC_HTTP_REQUEST_TIMEOUT, EC_HTTP_CONFLICT, EC_HTTP_GONE,
					 EC_HTTP_LENGTH_REQUIRED, EC_HTTP_PRECONDITION_FAILED,
					 EC_HTTP_REQUEST_ENTITY_TOO_LARGE,
					 EC_HTTP_REQUEST_URI_TOO_LONG,
					 EC_HTTP_UNSUPPORTED_MEDIA_TYPE,
					 EC_HTTP_INTERNAL_SERVER_ERROR,
					 EC_HTTP_NOT_IMPLEMENTED, EC_HTTP_BAD_GATEWAY,
					 EC_HTTP_SERVICE_UNAVAILABLE, EC_HTTP_GATEWAY_TIMEOUT,
					 EC_HTTP_HTTP_VERSION_NOT_SUPPORTED };
	enum DsmccError { EC_DSMCC_NOT_FOUND = 0,
					   EC_DSMCC_DOWNLOAD_HAS_FAILED };
#ifdef _SSL_SUPPORT_
	enum TlsError { EC_TLS_UNEXPECTED_MESSAGE = 0,
					EC_TLS_BAD_RECORD_MAC, EC_TLS_DECRYPTION_FAILED,
					EC_TLS_RECORD_OVERFLOW, EC_TLS_DECOMPRESSION_FAILURE,
					EC_TLS_HANDSHAKE_FAILURE, EC_TLS_BAD_CERTIFICATE,
					EC_TLS_UNSUPPORTED_CERTIFICATE, EC_TLS_CERTIFICATE_REVOKED,
					EC_TLS_CERTIFICATE_EXPIRED, EC_TLS_CERTIFICATE_UNKNOWN,
					EC_TLS_ILLEGAL_PARAMETER, EC_TLS_UNKNOWN_CA,
					EC_TLS_ACCESS_DENIED, EC_TLS_DECODE_ERROR,
					EC_TLS_DECRYPT_ERROR, EC_TLS_EXPORT_RESTRICTION,
					EC_TLS_PROTOCOL_VERSION, EC_TLS_INSUFFICIENT_SECURITY,
					EC_TLS_INTERNAL_ERROR, EC_TLS_USER_CANCELED,
					EC_TLS_NO_RENEGOCIATION };
#endif // __SSL_SUPPORT_
	enum NetworkError { EC_NET_INVALID_SERVER = 0,
						EC_NET_CONNECTION,
						EC_NET_TIME_OUT,
						EC_NET_NO_NET,
						EC_NET_NO_FREE_SOCKETS };
	enum BoundaryError { EC_BOUND_BAD_EXPRESSION = 0,
						 EC_BOUND_OUT_OF_RANGE };

	virtual ~OpErrorListener() {}

	virtual void OnOperaDocumentError(OpWindowCommander* commander, OperaDocumentError error) = 0;
	virtual void OnFileError(OpWindowCommander* commander, FileError error) = 0;
	virtual void OnHttpError(OpWindowCommander* commander, HttpError error) = 0;
	virtual void OnDsmccError(OpWindowCommander* commander, DsmccError error) = 0;
#ifdef _SSL_SUPPORT_
	virtual void OnTlsError(OpWindowCommander* commander, TlsError error) = 0;
#endif // _SSL_SUPPORT_
	virtual void OnNetworkError(OpWindowCommander* commander, NetworkError error) = 0;
	virtual void OnBoundaryError(OpWindowCommander* commander, BoundaryError error) = 0;
	virtual void OnRedirectionRegexpError(OpWindowCommander* commander) = 0;
	virtual void OnOomError(OpWindowCommander* commander) = 0;
	virtual void OnSoftOomError(OpWindowCommander* commander) = 0;
};


class CssInformation
{
public:
	enum CSS_type { OP_CSS_STYLE_PREFERRED, OP_CSS_STYLE_ALTERNATE };

	CssInformation() : kind(OP_CSS_STYLE_ALTERNATE), title(NULL), enabled(FALSE), href(NULL) {}

	CSS_type kind;
	const uni_char* title;
	BOOL enabled;
	const uni_char* href;
};

#ifdef LINK_SUPPORT

/**
 * Represents a link in the document. You get this from the
 * GetLinkElementInfo function.
 */
class LinkElementInfo
{
public:
	enum LinkElement_kind
	{
		OP_LINK_ELT_ALTERNATE,
		OP_LINK_ELT_STYLESHEET,
		OP_LINK_ELT_START,
		OP_LINK_ELT_NEXT,
		OP_LINK_ELT_PREV,
		OP_LINK_ELT_CONTENTS,
		OP_LINK_ELT_INDEX,
		OP_LINK_ELT_GLOSSARY,
		OP_LINK_ELT_COPYRIGHT,
		OP_LINK_ELT_CHAPTER,
		OP_LINK_ELT_SECTION,
		OP_LINK_ELT_SUBSECTION,
		OP_LINK_ELT_APPENDIX,
		OP_LINK_ELT_HELP,
		OP_LINK_ELT_BOOKMARK,
		OP_LINK_ELT_ICON,
		OP_LINK_ELT_END,
		OP_LINK_ELT_FIRST,
		OP_LINK_ELT_LAST,
		OP_LINK_ELT_UP,
		OP_LINK_ELT_FIND,
		OP_LINK_ELT_AUTHOR,
		OP_LINK_ELT_MADE,
		OP_LINK_ELT_ALTERNATE_STYLESHEET,
		OP_LINK_ELT_ARCHIVES,
		OP_LINK_ELT_LICENSE,
		OP_LINK_ELT_NOFOLLOW,
		OP_LINK_ELT_NOREFERRER,
		OP_LINK_ELT_PINGBACK,
		OP_LINK_ELT_PREFETCH,
		OP_LINK_ELT_OTHER,
		OP_LINK_ELT_APPLE_TOUCH_ICON,
		OP_LINK_ELT_IMAGE_SRC,
		OP_LINK_ELT_APPLE_TOUCH_ICON_PRECOMPOSED
	};

	LinkElementInfo() : kind(OP_LINK_ELT_OTHER), title(NULL), href(NULL), rel(NULL), type(NULL) {}
	~LinkElementInfo() {}

	/** The kind of link. Strongly related to the attribute rel on <link> elements */
	LinkElement_kind kind;
	/** A describing string if any exists. */
	const uni_char* title;
	/** The link address if any exists. */
	const uni_char* href;

	/** The string related to |kind|. Normally uninteresting. */
	const uni_char* rel;
	/** The mime type related to the link. Used by some kinds of links. */
	const uni_char* type;
};
#endif // LINK_SUPPORT

#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER

/** @short Listener for SSL related events that require user attention. */
class OpSSLListener
{
public:
	virtual ~OpSSLListener() {}

	/** @short Certificate action flag enum.
	 *
	 * This is an options/actions bit field that is used both for core to
	 * signal which options that are available when
	 * OnCertificateBrowsingNeeded() is called, and when done with certificate
	 * user interaction to signal what options/actions that should be taken.
	 */
	enum SSLCertificateOption {
		SSL_CERT_OPTION_NONE	= 0,		///< no option/action provided

		SSL_CERT_OPTION_OK		= 0x001,
		SSL_CERT_OPTION_ACCEPT	= 0x002,
		SSL_CERT_OPTION_REFUSE	= 0x004,
		SSL_CERT_OPTION_INSTALL	= 0x008,
		SSL_CERT_OPTION_DELETE	= 0x010,
		SSL_CERT_OPTION_IMPORT	= 0x020,
		SSL_CERT_OPTION_EXPORT	= 0x040, //maybe this is better to do via for example SSLCertificate::ExportToFile?

		SSL_CERT_OPTION_REMEMBER= 0x100	///< the remember flag can be set on ACCEPT and REFUSE
	};

	/** @short Settings flag enum that are kept to the certificate. */
	enum SSLCertificateSetting {
		SSL_CERT_SETTING_NONE				= 0,

		SSL_CERT_SETTING_ALLOW_CONNECTIONS	= 0x01,
		SSL_CERT_SETTING_WARN_WHEN_USE		= 0x02
	};

	/** @short Reason for calling OnCertificateBrowsingNeeded(). */
	enum SSLCertificateReason {

		/** Unknown reason. */
		SSL_REASON_UNKNOWN = 0,

		/** Certificate and server names mismatch. */
		SSL_REASON_DIFFERENT_CERTIFICATE =  0x01,

		/** One or more self-signed certificates found. */
		SSL_REASON_INCOMPLETE_CHAIN      =  0x02,

		/** Expired certificate. */
		SSL_REASON_CERT_EXPIRED          =  0x04,

		/** Missing certificates in the trust chain. */
		SSL_REASON_NO_CHAIN              =  0x08,

		/** Anonymous connection. */
		SSL_REASON_ANONYMOUS_CONNECTION  =  0x10,

		/** Certificate usage warning. */
		SSL_REASON_CERTIFICATE_WARNING   =  0x20,

		/** No encryption used. */
		SSL_REASON_AUTHENTICATED_ONLY    =  0x40,

		/** Disabled cipher detected. */
		SSL_REASON_DISABLED_CIPHER       =  0x80,

		/** Weak cipher detected. */
		SSL_REASON_LOW_ENCRYPTION        = 0x100
	};

	/** @short SSL certificate information.
	 *
	 * This class holds information about a certificate. Use
	 * SSLCertificateContext::GetNumberOfCertificates() and
	 * SSLCertificateContext::GetCertificate() to iterate over all certificates
	 * in the chain. The certificates are owned by core and will be valid until
	 * OnCertificateBrowsingDone() is called.
	 */
	class SSLCertificate : public OpCertificate
	{
	public:
		virtual ~SSLCertificate() {}

		/** @short Get the short name for the certificate. */
		virtual const uni_char* GetShortName() const = 0;

		/** @short Get the full name for the certificate. */
		virtual const uni_char* GetFullName() const = 0;

		/** @short Get the certificate issuer. */
		virtual const uni_char* GetIssuer() const = 0;

		/** @short Get the date the certificate is valid from. */
		virtual const uni_char* GetValidFrom() const = 0;

		/** @short Get the date the certificate is valid to. */
		virtual const uni_char* GetValidTo() const = 0;

		/** @short Get other certificate information. */
		virtual const uni_char* GetInfo() const = 0;

		/** @short Get certificate hash. Used to uniquely identify the certificate. */
		virtual const char* GetCertificateHash(unsigned int &length) const = 0;

		/** @short Is the certificate installed?
		 *
		 * @retval  TRUE        If the certificate is already installed.
		 * @retval  FALSE       Otherwise.
		 */
		virtual BOOL IsInstalled() const = 0;

		/** @short Get the settings bitfield for this certificate.
		 *
		 *  @return             Certificate settings, values of
		 *                      enum OpSSLListener::SSLCertificateSetting
		 *                      ORed together.
		 */
		virtual int GetSettings() const = 0;

		/** @short Set or update the setting bitfield for this certificate.
		 *
		 *  @param  settings    Certificate settings, values of
		 *                      enum OpSSLListener::SSLCertificateSetting
		 *                      ORed together.
		 */
		virtual void UpdateSettings(int settings) = 0;
	};

	/** @short Certificate call-back context.
	 *
	 * This is a callback context for user interaction with SSL
	 * certificates. The context is owned by core and is valid until
	 * OnCertificateBrowsingDone() is called.
	 *
	 * The UI/platform MUST call the method OnCertificateBrowsingDone(). It can
	 * be called immediately when the interaction is signalled (if the
	 * UI/platform wants to ignore it).
	 */
	class SSLCertificateContext
	{
	public:
		virtual ~SSLCertificateContext() {}

		/** @short Get the number of available certificates in the current context */
		virtual unsigned int GetNumberOfCertificates() const = 0;

		/** @short Get a specific certificate.
		 *
		 * @param comment Index of the certificate, starting from 0.
		 *
		 * If an invalid certificate number is given (larger than
		 * GetNumberOfCertificates()-1), NULL will be returned. The
		 * SSLCertificate object is owned by core and is valid until
		 * OnCertificateBrowsingDone() is called.
		 */
		virtual SSLCertificate* GetCertificate(unsigned int certificate) = 0;

		/** @short Get the number of available certificate comments in the current context. */
		virtual unsigned int GetNumberOfComments() const = 0;

		/** @short Get a comment from the current context.
		 *
		 * @param comment Index of the comment, starting from 0.
		 *
		 * If an invalid number (larger than GetNumberOfComments()-1)
		 * is provided, NULL will be returned.
		 */
		virtual const uni_char* GetCertificateComment(unsigned int comment) const = 0;

		/** @short Get the name of the server with which this certificate context is associated.
		 *
		 * @param[out] server_name Set to the server name associated with this
		 * certificate context. If there is no server name associated with this
		 * certificate context, it will be set to an empty string (and
		 * OpStatus::OK will be returned).
		 *
		 * @return ERR_NO_MEMORY on OOM, otherwise OK.
		 */
		virtual OP_STATUS GetServerName(OpString* server_name) const = 0;

#ifdef SECURITY_INFORMATION_PARSER
		/** @short Create a security information parser for the current context, prime it and
		 * parse it so that information is available.
		 *
		 * @param[out] osip Set to an instantiation of a parser that must be deleted when not needed.
		 * will be NULL on OOM.
		 *
		 * @return ERR_NO_MEMORY on OOM, otherwise OK.
		 */
		virtual OP_STATUS CreateSecurityInformationParser(OpSecurityInformationParser ** osip) = 0;

#endif // SECURITY_INFORMATION_PARSER

		/** @short Certificate browsing is done. Tell core what to do.
		 *
		 * This must always be called, as a response to
		 * OnCertificateBrowsingNeeded().
		 *
		 * @param ok if FALSE, nothing will be done. If TRUE, the options
		 * argument will be used to determine appropriate action(s). FIXME:
		 * Could this parameter sensibly be removed, in the favor of
		 * options=SSL_CERT_OPTION_NONE?
		 * @param options If 'ok' is TRUE, this parameter contains the
		 * options/actions to be performed for the certificate. FIXME: Should
		 * instead take an int consisting of SSLCertificateOption values OR'ed
		 * together.
		 */
		virtual void OnCertificateBrowsingDone(BOOL ok, SSLCertificateOption options) = 0;
	};

	/** @short Called when core needs user interaction regarding a certificate.
	 *
	 * OnCertificateBrowsingDone() must always be called as a response to
	 * this. It can be done synchronously or asynchronously.
	 *
	 * @param wic The windowcommander in question. May be NULL, since
	 * OpSSLListener may be associated with an OpWindowCommanderManager instead
	 * of an OpWindowCommander.
	 * @param context Provider of information about the certificate(s) and a
	 * callback method to call when finished
	 * @param reason The reason why user interaction is needed
	 * @param options a bitfield with possible options to do upon a call to
	 * OnCertificateBrowsingDone. FIXME: Should instead take an int consisting
	 * of SSLCertificateOption values OR'ed together.
	 */
	virtual void OnCertificateBrowsingNeeded(OpWindowCommander* wic, SSLCertificateContext* context, SSLCertificateReason reason, SSLCertificateOption options) = 0;

	/** @short Called if core no longer wants the result of a previously issued and unfinished certificate browsing request.
	 *
	 * This means that the SSLCertificateContext call-back object previously
	 * passed to OnCertificateBrowsingNeeded() is no longer to be used, because
	 * it may be deleted at any time after this.
	 *
	 * @param wic The windowcommander
	 * @param context Object that was previously passed to
	 * OnCertificateBrowsingNeeded(). Must not be accessed in the
	 * implementation of this method, or ever again after that.
	 */
	virtual void OnCertificateBrowsingCancel(OpWindowCommander* wic, SSLCertificateContext* context) = 0;

	/** @short Security password prompt call-back context.
	 *
	 * Callback for SSL certificate password. The UI must always call the
	 * call-back method OnSecurityPasswordDone(). The callback can be called
	 * either directly (from within OnSecurityPasswordNeeded()) (synchronously)
	 * or at a later stage (asynchronously).
	 */
	class SSLSecurityPasswordCallback
	{
	public:
		/** @short Password dialog mode. */
		enum PasswordDialogMode {
			/** Ask for current password. */
			JustPassword,

			/** Ask for new password. */
			NewPassword,

			/** Ask both for current and new password. */
			ChangePassword
		};

		/** @short Password request reason. */
		enum PasswordRequestReason {
			/** Password needed to export a certificate. */
			CertificateExport,

			/** Password needed to import a certificate. */
			CertificateImport,

			/** Password needed to unlock a certificate. */
			CertificateUnlock,

			/** Password request for local encryption of wand items. */
			WandEncryption
		};

	public:
		virtual ~SSLSecurityPasswordCallback() {}

		/** @short Get dialog title. */
		virtual Str::LocaleString GetTitle() const = 0;

		/** @short Get dialog message. */
		virtual Str::LocaleString GetMessage() const = 0;

		/** @short Get dialog mode. */
		virtual PasswordDialogMode GetMode() const = 0;

		/** @short Get request reason. */
		virtual PasswordRequestReason GetReason() const = 0;

		/** @short Security password prompt done.
		 *
		 * @param ok           If TRUE, the password request was fulfilled by the
		 *                     platform/UI/user, and a password parameter is provided.
		 *
		 * @param old_password Old/existing password provided
		 *                     by the platform/UI/user for checking.
		 *                     Used in modes JustPassword and ChangePassword.
		 *
		 * @param new_password New password provided by the platform/UI/user.
		 *                     Used in modes NewPassword and ChangePassword.
		 *
		 * Applies to both old_password and new_password parameters:
		 *
		 * - Both password parameters are ignored if 'ok' is not TRUE.
		 *
		 * - If a parameter must not be used in a particular mode,
		 *   NULL must be provided. If non-NULL was provided nevertheless -
		 *   it's undefined behaviour. A parameter will likely be ignored,
		 *   but you can not rely on it.
		 *
		 * - A parameter must also be NULL if the password was not provided,
		 *   i.e. the user typed nothing in the password field.
		 *
		 * - A parameter's pointed data must be encoded as UTF-8,
		 *   if the parameter is not NULL.
		 *
		 * - The callback does not take ownership over the pointed data.
		 *   It's caller's responsibility to wipe and deallocate it.
		 */
		virtual void OnSecurityPasswordDone(BOOL ok, const char* old_password, const char* new_password) = 0;
	};

	/** @short Called when core needs the SSL security password from the user.
	 *
	 * callback->OnSecurityPasswordDone() must always be called in response to
	 * this method when finished, when the password request was either
	 * fulfilled or rejected. This can be done either synchronously or
	 * asynchronously.
	 *
	 * @param wic The windowcommander in question. May be NULL, since
	 * OpSSLListener may be associated with an OpWindowCommanderManager instead
	 * of an OpWindowCommander.
	 * @param callback A call-back used to report when finished.
	 */
	virtual void OnSecurityPasswordNeeded(OpWindowCommander* wic, SSLSecurityPasswordCallback* callback) = 0;

	/** @short Called if core no longer wants the security password.
	 *
	 * This cancels the request that was issued by calling
	 * OnSecurityPasswordNeeded(). The SSLSecurityPasswordCallback object
	 * passed to that method must no longer be used, because it may be deleted
	 * at any time after this.
	 *
	 * @param wic is the windowcommander instance that was passed to
	 *  OnSecurityPasswordNeeded().
	 * @param callback is the SSLSecurityPasswordCallback object that
	 *  was passed to OnSecurityPasswordNeeded() and that must no longer be
	 *  used.
	 */
	virtual void OnSecurityPasswordCancel(OpWindowCommander* wic, SSLSecurityPasswordCallback* callback) = 0;
};

#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER

#ifdef WIC_COLORSELECTION_LISTENER
/** @short
 * A listener for color selection requests. A platform can implement this listener if it can provide a UI for selecting colors.
 * The platform should register an instance of this class by calling OpWindowCommander::SetColorSelectionListener().
 * If the listener is not implemented or not registered, the default listener will return the initial color for the request upon requests from core.
 * So if you don't implement or register this it's better to disable any feature/tweak that enables this API completly as that makes core remove any UI to
 * ask the platform for colors.
*/

class OpColorSelectionListener
{
public:
	class ColorSelectionCallback
	{
	public:
		virtual ~ColorSelectionCallback() {}

		/** @short Called when product/UI is done with the color selection.
		 *
		 * After this call, this ColorSelectionCallback object is no longer to be
		 * used, as it may be deleted at any time after this.
		 *
		 * @param color The color selected by the UI.
		 */
		virtual void OnColorSelected(COLORREF color) = 0;

		/** @short Called when product/UI is done with the color selection but didn't choose any color. */
		virtual void OnCancel() = 0;

		/** @short Get the initial color for the color selection operation.
		 */
		virtual COLORREF GetInitialColor() = 0;
	};

	virtual ~OpColorSelectionListener() {}

	/** @short Called when a color selector is requested.
	 *
	 * The UI is expected to report the chosen color to the callback by
	 * calling ColorSelectionCallback::OnColorSelected() or cancel the request
	 * by calling ColorSelectionCallback::OnCancel().
	 *
	 * It may be cancelled from the core side by calling OnColorSelectionCancel(), if core
	 * is no longer interested in any color.
	 *
	 * @param commander the window commander for which this happened
	 * @param callback Context object for this operation. The product/UI code
	 * uses this object to query core for additional information about the color
	 * request, and to tell core that a color have been selected, or cancelled.
	 */
	virtual void OnColorSelectionRequest(OpWindowCommander* commander, ColorSelectionCallback* callback) = 0;

	/** @short Called if core no longer wants color selection.
	 *
	 * This also means that the callback object previously passed to
	 * OnColorSelectionRequest() is no longer to be used, because it may be deleted at
	 * any time after this.
	 * The way to implement this can be any of the following:
	 *   -Close the UI
	 *   -Keep the UI open until user close it, but ignore the user input.
	 *
	 * This method may be called even if there is no color selection in
	 * progress.
	 */
	virtual void OnColorSelectionCancel(OpWindowCommander* commander) = 0;
};

#endif // WIC_COLORSELECTION_LISTENER

#ifdef WIC_FILESELECTION_LISTENER
/** @short Listener for file chooser / selection requests.
 *
 * When core needs the product/UI to select one or multiple files or
 * directories for a specified purpose, methods on this class will be called.
 *
 * One typical way to implement this on the product/UI side is to display some
 * sort of file dialog.
 */
class OpFileSelectionListener
{
public:
	virtual ~OpFileSelectionListener() {}

	/** @short Is file selection permitted?
	 *
	 * Core may want to behave differently if file selection is
	 * disabled/forbidden.
	 *
	 * @return TRUE if this product allows file selection, FALSE otherwise.
	 */
	virtual BOOL OnRequestPermission(OpWindowCommander* commander) = 0;

	/** @short File type filter.
	 *
	 * Used when core is only interested in certain types of files.
	 */
	struct MediaType
	{
		/** Media/content/MIME type identifier (e.g. text/html). */
		OpString media_type;

		/** List of file name extensions for this media type.
		 *
		 * Does not include the dot ('.') delimiter (e.g. "htm", "html").
		 */
		OpAutoVector <OpString> file_extensions;
	};

	/** @short File selection context.
	 *
	 * Passed via various methods in OpFileSelectionListener that initiate a file
	 * selection operation.
	 *
	 * An object of this type serves two purposes:
	 *
	 *   - report selected file(s) to core (or cancel selection)
	 *
	 *   - ask core for additional information, such as accepted media types,
	 *   initial file name suggestion, and so on. This information is only
	 *   advisory; the OpFileSelectionListener implementation is not required to
	 *   check or honor any of this information.
	 */
	class FileSelectionCallback
	{
	public:
		virtual ~FileSelectionCallback() {}

		/** @short Called when product/UI is done with the file selection.
		 *
		 * After this call, this FileSelectionCallback object is no longer to be
		 * used, as it may be deleted at any time after this.
		 *
		 * @param files The full path to the file(s). May be empty or even
		 * NULL, which means that no files were selected (e.g. because the user
		 * closed the file dialog box without selecting files). The caller of
		 * this method (product/UI) is free to delete this data as soon as it
		 * has returned (meaning that if core wants to keep the data for any
		 * longer, it must be copied).
		 */
		virtual void OnFilesSelected(const OpAutoVector<OpString>* files) = 0;

		/** @short Get one of the accepted media types, if any.
		 *
		 * An implementation of OpFileSelectionListener may want to provide
		 * some sort of file type filtering in a file dialog based on this
		 * information, but should probably always allow the user to select any
		 * type of file; especially if no accepted media types were specified.
		 *
		 * @param index Media type index. 0 is the first media type in the
		 * list, 1 is the second and so on. Order is arbitrary.
		 *
		 * @return The media type at the requested index, or NULL if there were
		 * not that many media types.
		 */
		virtual const MediaType* GetMediaType(unsigned int index) = 0;

		/** @short Get the initial path, if any, for the file selection operation.
		 *
		 * This may be a file or a directory. NULL may be returned, which means
		 * that core doesn't suggest any initial path.
		 */
		virtual const uni_char* GetInitialPath() = 0;
	};

# ifdef _FILE_UPLOAD_SUPPORT_
	/** @short File upload selection context. */
	class UploadCallback : public FileSelectionCallback
	{
	public:
		virtual ~UploadCallback() {}

		/** @short Get the maximum number of files to select.
		 *
		 * @return The maximum number of files to select. 0 is illegal.
		 */
		virtual unsigned int GetMaxFileCount() = 0;
	};

	/** @short Called when file names for an upload are requested.
	 *
	 * The product/UI is expected to pick the file(s) to be uploaded respond
	 * via UploadCallback::OnFilesSelected().
	 *
	 * Only one operation of this type at a time is allowed. It may be
	 * cancelled from the core side by calling OnUploadFilesCancel(), if core
	 * is no longer interested in any file names.
	 *
	 * @param commander the window commander for which this happened
	 * @param callback Context object for this operation. The product/UI code
	 * uses this object to query core for additional information about the file
	 * name request, and to tell core that files have been selected, or if file
	 * selection is cancelled.
	 */
	virtual void OnUploadFilesRequest(OpWindowCommander* commander, UploadCallback* callback) = 0;

	/** @short Called if core no longer wants file names for an upload.
	 *
	 * This also means that the callback object previously passed to
	 * OnUploadRequest() is no longer to be used, because it may be deleted at
	 * any time after this.
	 *
	 * This method may be called even if there is no file selection in
	 * progress.
	 */
	virtual void OnUploadFilesCancel(OpWindowCommander* commander) = 0;
# endif // _FILE_UPLOAD_SUPPORT_

# ifdef DOM_GADGET_FILE_API_SUPPORT
	/** @short Selection context for DOM filesystem. */
	class DomFilesystemCallback : public FileSelectionCallback
	{
	public:
		/** @short Detail specifier for the file selection operation. */
		enum SelectionType
		{
			/** @short Core wants one file that is going to be read.
			 *
			 * The file must exist when OnFilesSelected() is called.
			 */
			OPEN_FILE,

			/** @short Core wants one or several files that are going to be read.
			 *
			 * The files must exist when OnFilesSelected() is called.
			 */
			OPEN_MULTIPLE_FILES,

			/** @short Core wants one file that is going to be written.
			 *
			 * The file may but does not have to exist when OnFilesSelected()
			 * is called.
			 */
			SAVE_FILE,

			/** @short Core wants one directory.
			 *
			 * The directory may but does not have to exist when
			 * OnFilesSelected() is called.
			 */
			GET_DIRECTORY,

			/** Core wants one directory, to be used as a mountpoint in DOM filesystem.
			 *
			 * The directory must  exist when OnFilesSelected() is called.
			 */
			GET_MOUNTPOINT
		};

		virtual ~DomFilesystemCallback() {}

		/** @short Get details about the requested file selection operation.
		 *
		 * @return Whether core is looking for files to open or save, or
		 * directories, etc.
		 */
		virtual SelectionType GetSelectionType() = 0;

		/** @short Get caption text for the file selection operation.
		 *
		 * May be NULL if there is no caption text.
		 */
		virtual const uni_char* GetCaption() = 0;
	};

	/** @short Core needs to be provided with one or more files / directories.
	 *
	 * This happens when some DOM method in opera.io.fileSystem has been
	 * called. Product/UI code is expected to pick files(s) to be selected and
	 * then call DomFilesystemCallback::OnFilesSelected().
	 *
	 * Only one operation of this type at a time is allowed. It may be
	 * cancelled from the core side by calling OnDomFilesystemFilesCancel().
	 *
	 * @param commander the window commander for which this happened
	 * @param callback Context object for this operation. The product/UI code
	 * uses this object to query core for additional information about the file
	 * name request, and to tell core that files have been selected, or if file
	 * selection is cancelled. Any method on the callback object may be called
	 * synchronously (before OnDomFilesystemFilesRequest() has returned), or
	 * asynchronously. The object is valid in the UI/product code until either
	 * OnDomFilesystemFilesCancel() is called from the core side, or
	 * callback->OnFilesSelected() is called from the UI/product side.
	 */
	virtual void OnDomFilesystemFilesRequest(OpWindowCommander* commander, DomFilesystemCallback* callback) = 0;

	/** @short Called if core no longer wants file names for the DOM operation.
	 *
	 * This also means that the callback object previously passed to
	 * OnDomFilesystemFilesRequest() is no longer to be used, because it may be
	 * deleted at any time after this.
	 *
	 * This method may be called even if there is no file selection in
	 * progress.
	 */
	virtual void OnDomFilesystemFilesCancel(OpWindowCommander* commander) = 0;
# endif // DOM_GADGET_FILE_API_SUPPORT
};
#endif // WIC_FILESELECTION_LISTENER

#ifdef DOM_TO_PLATFORM_MESSAGES
class OpPlatformMessageListener
{
public:
	virtual ~OpPlatformMessageListener() {}

	class PlatformMessageContext
	{
	public:
		virtual void Return(const uni_char *answer) = 0;
	};

	virtual void OnMessage(OpWindowCommander* wic, PlatformMessageContext *context, const uni_char *message, const uni_char *url) = 0;
};
#endif // DOM_TO_PLATFORM_MESSAGES

#ifdef NEARBY_ELEMENT_DETECTION
/** @short Listener class for fingertouch events. */
class OpFingertouchListener
{
public:
	virtual ~OpFingertouchListener() {}

	/** @short Elements for fingertouch are about to be expanded.
	 *
	 * Called right before the elements are expanded. HideFingertouchElements()
	 * in OpWindowCommander may be called if the user interface wants to hide
	 * them again explicitly.
	 */
	virtual void OnElementsExpanding(OpWindowCommander* wic) = 0;

	/** @short Elements for fingertouch have been expanded.
	 *
	 * Called after the elements for fingertouch have been
	 * expanded. HideFingertouchElements() in OpWindowCommander may be called
	 * if the user interface wants to hide them again explicitly.
	 */
	virtual void OnElementsExpanded(OpWindowCommander* wic) = 0;

	/** @short Previously expanded elements for fingertouch are about to be hidden.
	 *
	 * Called right before the expanded elements are to be hidden, either on core's own
	 * initiative (e.g. something was clicked), or because the UI explicitly
	 * requested it.
	 */
	virtual void OnElementsHiding(OpWindowCommander* wic) = 0;

	/** @short Previously expanded elements for fingertouch have been hidden.
	 *
	 * Called when the expanded elements have been hidden, either on core's own
	 * initiative (e.g. something was clicked), or because the UI explicitly
	 * requested it.
	 */
	virtual void OnElementsHidden(OpWindowCommander* wic) = 0;
};
#endif // NEARBY_ELEMENT_DETECTION

#ifdef WIC_PERMISSION_LISTENER
/** @short Listener for permission API.
 *
 * Sometimes core needs to ask the user for permission to do certain
 * things in a less obtrusive manner. The platform is expected to show
 * a minimal ui where the user can accept or deny the request.  UI can show
 * four choices for user, see
 * OpPermissionListener::PermissionCallback::PersistenceType, or simply ignore
 * it by calling OpPermissionListener::PermissionCallback::OnDisallowed(
 * OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS) without
 * displaying any dialog.
 *
 * For instance, a script might want to retrieve the user's geolocation
 * coordinates.
 *
 * UI needs to create instance of OpPermissionListener implementation and
 * register the listener by calling OpWindowCommander::SetPermissionListener().
 */
class OpPermissionListener
{
public:
	class PermissionCallback
	{
	public:

		virtual ~PermissionCallback() {}

		/** @short Permission type. */
		typedef enum {
			PERMISSION_TYPE_UNKNOWN                    ///< Other data-sensitive operation is requested.
#ifdef GEOLOCATION_SUPPORT
			, PERMISSION_TYPE_GEOLOCATION_REQUEST      ///< Access to geolocation data is requested.
#endif //GEOLOCATION_SUPPORT
#ifdef DOM_STREAM_API_SUPPORT
			, PERMISSION_TYPE_CAMERA_REQUEST           ///< Access to a camera is requested.
#endif // DOM_STREAM_API_SUPPORT
#if defined DOM_JIL_API_SUPPORT
			, PERMISSION_TYPE_DOM_ACCESS_REQUEST       ///< This means that the object is actually a DOMPermissionCallback and may be downcasted.
#endif // DOM_JIL_API_SUPPORT
#if defined WEB_HANDLERS_SUPPORT
			, PERMISSION_TYPE_WEB_HANDLER_REGISTRATION ///< Permission for registering a web handler is requested.
#endif // WEB_HANDLERS_SUPPORT
		} PermissionType;

		/** @short Persistence type.
		 *
		 * The PersistenceType answer to PermissionCallback is connected
		 * to OpPermissionListener::DOMPermissionCallback::OperationType,
		 * regardless of its arguments.
		 */
		typedef enum {
			PERSISTENCE_TYPE_NONE		= 1,	///< Don't save the answer, use it just once
			PERSISTENCE_TYPE_RUNTIME	= 2,	///< Store the answer in the runtime. Eg. it will go away on reload.
			PERSISTENCE_TYPE_SESSION	= 4,	///< Save the answer for session
			PERSISTENCE_TYPE_ALWAYS		= 8		///< Save the answer in persistent storage (e.g. disk)
		} PersistenceType;

		/** @short Called when the user has given permission to go forward with the request.
		 *
		 * @param persistence For how long should core remember the choice for this context (site or gadget etc.).
		 */
		virtual void OnAllowed(PersistenceType persistence = PERSISTENCE_TYPE_NONE) = 0;

		/** @short Called when the user has denied permission to go forward with the request.
		 *
		 * @param persistence For how long should core remember the choice for this context (site or gadget etc.).
		 */
		virtual void OnDisallowed(PersistenceType persistence = PERSISTENCE_TYPE_NONE) = 0;

		/** @short What type of permission request this is. */
		virtual PermissionType Type() = 0;

		/** @short Returns the URL of the document that asks for permission.
		 *
		 * @note The memory of the returned value belongs to the
		 *   implementation of this method. The caller should create a
		 *   copy of the string, if it is needed for a longer time.
		 *
		 * @return the URL of the document of NULL if no url is available.
		 */
		virtual const uni_char* GetURL() = 0;

		/** @short Which persistence choices are allowed for affirmative answers.
		 *
		 * @return Logical OR of values of PersistenceType.
		 */
		virtual int AffirmativeChoices()	{ return PERSISTENCE_TYPE_NONE | PERSISTENCE_TYPE_ALWAYS; }

		/** @short Which persistence choices are allowed for negative answers.
		 *
		 * @return Logical OR of values of PersistenceType.
		 */
		virtual int NegativeChoices()		{ return PERSISTENCE_TYPE_NONE | PERSISTENCE_TYPE_ALWAYS; }
	};

# ifdef WEB_HANDLERS_SUPPORT
	/** The permission callback for web handlers registration requests. */
	class WebHandlerPermission : public PermissionCallback
	{
	public:
		/** @short Returns the name of a protocol or the type of a content
		 * a web handler is registered for.
		 *
		 * @note The memory of the returned value belongs to the
		 *   implementation of this method. The caller should create a
		 *   copy of the string, if it is needed for a longer time.
		 *
		 * @return The name of a protocol or the type of a content
		 * a web handler is registered for.
		 */
		virtual const uni_char* GetName() = 0;

		/** @short Returns the web handler's URL.
		 *
		 * @note The memory of the returned value belongs to the
		 *   implementation of this method. The caller should create a
		 *   copy of the string, if it is needed for a longer time.
		 *
		 * @return The URL of the web handler.
		 */
		virtual const uni_char* GetHandlerURL() = 0;

		/** @short Returns the web handler's description.
		 *
		 * @note The memory of the returned value belongs to the
		 *   implementation of this method. The caller should create a
		 *   copy of the string, if it is needed for a longer time.
		 *
		 * @return The description of the web handler.
		 */
		virtual const uni_char* GetDescription() = 0;

		/** @short Checks if the web handler is a protocol or a content handler.
		 *
		 * @return TRUE if the web handler handlers a protocol.
		 */
		virtual BOOL IsProtocolHandler() = 0;
	};
# endif // WEB_HANDLERS_SUPPORT

# ifdef DOM_JIL_API_SUPPORT
	/** @short Permission callback for DOM access operations
	 *
	 * An instance of this class is passed as argument \c callback
	 * to OpPermissionListener::OnAskForPermission() if the
	 * OpPermissionListener::PermissionCallback::Type() equals
	 * OpPermissionListener::PermissionCallback::PERMISSION_TYPE_DOM_ACCESS_REQUEST.
	 * The implementation of OpPermissionListener::OnAskForPermission() can use
	 * GetOperation() to get the operation that is requested. Some operations
	 * use additional arguments. To get the number of arguments the implementation can
	 * use GetNumberOfArguments(). To get the ith argument the implementation can
	 * use GetArgument(i).
	 * @see GetNumberOfArguments(), GetArgument()
	 */
	class DOMPermissionCallback : public PermissionCallback
	{
	public:
		typedef enum {
			OPERATION_TYPE_JIL_ACCOUNTINFO_PHONEUSERUNIQUEID,
			OPERATION_TYPE_JIL_ACCOUNTINFO_PHONEMSISDN,
			OPERATION_TYPE_JIL_ACCOUNTINFO_USERSUBSCRIPTIONTYPE,
			OPERATION_TYPE_JIL_ACCOUNTINFO_USERACCOUNTBALANCE,

			OPERATION_TYPE_JIL_CONFIG_MSGRINGTONEVOLUME,
			OPERATION_TYPE_JIL_CONFIG_RINGTONEVOLUME,
			OPERATION_TYPE_JIL_CONFIG_VIBRATIONSETTING,
			OPERATION_TYPE_JIL_CONFIG_SETASWALLPAPER, ///< Argument: filename.
			OPERATION_TYPE_JIL_CONFIG_SETDEFAULTRINGTONE, ///< Argument: filename.

			OPERATION_TYPE_JIL_ADDRESSBOOKITEM_UPDATE,

			OPERATION_TYPE_JIL_CALENDARITEM_UPDATE,

			OPERATION_TYPE_JIL_CAMERA_CAPTUREIMAGE, ///< Argument: filename.
			OPERATION_TYPE_JIL_CAMERA_STARTVIDEOCAPTURE, ///< Argument: filename.
			OPERATION_TYPE_JIL_CAMERA_STOPVIDEOCAPTURE,
			OPERATION_TYPE_JIL_CAMERA_ONCAMERACAPTURED,

			OPERATION_TYPE_JIL_DEVICE_CLIPBOARDSTRING,
			OPERATION_TYPE_JIL_DEVICE_COPYFILE, ///< Argument: filename.
			OPERATION_TYPE_JIL_DEVICE_DELETEFILE, ///< Argument: filename.
			OPERATION_TYPE_JIL_DEVICE_FINDFILES,
			OPERATION_TYPE_JIL_DEVICE_GETDIRECTORYFILENAMES, ///< Argument: directory.
			OPERATION_TYPE_JIL_DEVICE_GETFILE, ///< Argument: filename.
			OPERATION_TYPE_JIL_DEVICE_GETFILESYSTEMROOTS,
			OPERATION_TYPE_JIL_DEVICE_GETAVAILABLEAPPLICATIONS,
			OPERATION_TYPE_JIL_DEVICE_LAUNCHAPPLICATION, ///< Arguments: application name and application argument.
			OPERATION_TYPE_JIL_DEVICE_MOVEFILE, ///< Argument: filename.
			OPERATION_TYPE_JIL_DEVICE_SETRINGTONE,

			OPERATION_TYPE_JIL_DEVICEINFO_OWNERINFO,

			OPERATION_TYPE_JIL_DEVICESTATEINFO_REQUESTPOSITIONINFO, ///< Argument: method of position info.

			OPERATION_TYPE_JIL_MESSAGE_DELETEADDRESS,
			OPERATION_TYPE_JIL_MESSAGE_DELETEATTACHMENT,
			OPERATION_TYPE_JIL_MESSAGE_SAVEATTACHMENT,
			OPERATION_TYPE_JIL_MESSAGING_COPYMESSAGETOFOLDER, ///< Argument: directory.
			OPERATION_TYPE_JIL_MESSAGING_CREATEFOLDER, ///< Argument: directory.
			OPERATION_TYPE_JIL_MESSAGING_DELETEALLMESSAGES,
			OPERATION_TYPE_JIL_MESSAGING_DELETEEMAILACCOUNT,
			OPERATION_TYPE_JIL_MESSAGING_DELETEFOLDER, ///< Argument: directory.
			OPERATION_TYPE_JIL_MESSAGING_DELETEMESSAGE,
			OPERATION_TYPE_JIL_MESSAGING_FINDMESSAGES,
			OPERATION_TYPE_JIL_MESSAGING_GETCURRENTEMAILACCOUNT,
			OPERATION_TYPE_JIL_MESSAGING_GETEMAILACCOUNTS,
			OPERATION_TYPE_JIL_MESSAGING_GETFOLDERNAMES,
			OPERATION_TYPE_JIL_MESSAGING_GETMESSAGE,
			OPERATION_TYPE_JIL_MESSAGING_MOVEMESSAGETOFOLDER, ///< Argument: directory.
			OPERATION_TYPE_JIL_MESSAGING_SENDMESSAGE,
			OPERATION_TYPE_JIL_MESSAGING_SETCURRENTEMAILACCOUNT,

			OPERATION_TYPE_JIL_PIM_ADDCALENDARITEM,
			OPERATION_TYPE_JIL_PIM_GETADDRESSBOOKGROUPMEMBERS,
			OPERATION_TYPE_JIL_PIM_DELETECALENDARITEM,
			OPERATION_TYPE_JIL_PIM_FINDCALENDARITEMS,
			OPERATION_TYPE_JIL_PIM_GETCALENDARITEM,
			OPERATION_TYPE_JIL_PIM_GETCALENDARITEMS,
			OPERATION_TYPE_JIL_PIM_EXPORTASVCARD,
			OPERATION_TYPE_JIL_PIM_ADDADDRESSBOOKITEM,
			OPERATION_TYPE_JIL_PIM_FINDADDRESSBOOKITEMS,
			OPERATION_TYPE_JIL_PIM_CREATEADDRESSBOOKGROUP, ///< Argument: name of group.
			OPERATION_TYPE_JIL_PIM_DELETEADDRESSBOOKGROUP, ///< Argument: name of group.
			OPERATION_TYPE_JIL_PIM_DELETEADDRESSBOOKITEM,
			OPERATION_TYPE_JIL_PIM_GETADDRESSBOOKITEM,
			OPERATION_TYPE_JIL_PIM_GETADDRESSBOOKITEMSCOUNT,
			OPERATION_TYPE_JIL_PIM_GETAVAILABLEADDRESSGROUPNAMES,

			OPERATION_TYPE_JIL_TELEPHONY_DELETEALLCALLRECORDS,
			OPERATION_TYPE_JIL_TELEPHONY_DELETECALLRECORD,
			OPERATION_TYPE_JIL_TELEPHONY_FINDCALLRECORDS,
			OPERATION_TYPE_JIL_TELEPHONY_GETCALLRECORD,
			OPERATION_TYPE_JIL_TELEPHONY_GETCALLRECORDCOUNT,
			OPERATION_TYPE_JIL_TELEPHONY_INITIATEVOICECALL, ///< Argument: phone number.

			OPERATION_TYPE_JIL_WIDGETMANAGER_CHECKWIDGETINSTALLATIONSTATUS,

			OPERATION_TYPE_WIDGET_OPENURL, ///< No permission needed.

			OPERATION_TYPE_XMLHTTPREQUEST,
# ifdef SELFTEST
			OPERATION_TYPE_SELFTEST_ALLOW,
			OPERATION_TYPE_SELFTEST_DENY,
# endif // SELFTEST
		} OperationType;

		/** @short Returns type of DOM operation to ask for. */
		virtual OperationType GetOperation() = 0;

		/** @short Provides number of operation arguments */
		virtual unsigned int GetNumberOfArguments() = 0;
		/** @short Provides an operation argument
		 *
		 * Provides index-th argument to the operation.
		 *
		 * @param index index of the argument to get. It must be within 0 - (GetNumberOfArguments() - 1).
		 * @see GetNumberOfArguments()
		 */
		virtual const uni_char * GetArgument(unsigned int index) = 0;

		/** @short Gets translation of message, corresponding to OperationType.
		 * This message should be displayed in the confirmation dialog.  If the
		 * permission request uses some arguments - as specified by the
		 * OperationType - then the values of the arguments are already
		 * integrated into the message text. */
		OP_STATUS GetMessage(OpString& message);
	};
# endif // DOM_JIL_API_SUPPORT

	/** @short Called when core wants to asker the user for permission to execute
	 * a given task, which might be considered a privacy issue.
	 *
	 * @param callback Context object for this operation.
	 * If OpPermissionListener::PermissionCallback::Type() equals
	 * PERMISSION_TYPE_DOM_ACCESS_REQUEST, callback is of type
	 * DOMPermissionCallback.
	 */
	virtual void OnAskForPermission(OpWindowCommander *wic, PermissionCallback *callback) = 0;

	/** @short Called when core wants to cancel the outstanding request. Page might have been closed etc. etc.
	 *
	 * @param callback Context object for this operation.
	 */
	virtual void OnAskForPermissionCancelled(OpWindowCommander *wic, PermissionCallback *callback) = 0;

# ifdef WIC_SET_PERMISSION
	class ConsentInformation
	{
	public:
		OpString hostname;
		BOOL is_allowed;
		PermissionCallback::PermissionType permission_type;
		PermissionCallback::PersistenceType persistence_type;
	};

	/** Called with results of GetUserConsent call.
	 *
	 * @param wic Window commander for which the information is provided.
	 * @param consent_information Consent information entries.
	 *        Each entry contains information for one hostname and permission
	 *        type. There may be multiple entries for multiple hosts (for pages
	 *        loaded in iframes) and multiple permissions. If there is no entry
	 *        for a given permission and host pair then Core will call
	 *        OnAskForPermission to get permission once it is required.
	 *        The data in the vector is only valid during the call.
	 * @param user_data user data which were passed in call of OpWindowCommander::GetUserConsent
	 */
	virtual void OnUserConsentInformation(OpWindowCommander *wic, const OpVector<ConsentInformation> &consent_information, INTPTR user_data) = 0;

	/** Called with error result if GetUserConsent call fails.
	 */
	virtual void OnUserConsentError(OpWindowCommander *wic, OP_STATUS error, INTPTR user_data) = 0;
# endif // WIC_SET_PERMISSION
};
#endif // WIC_PERMISSION_LISTENER

#ifdef APPLICATION_CACHE_SUPPORT

/**
 * Offline application cache event listener. Must be implemented
 * by platform projects
 */
class OpApplicationCacheListener
{
public:

	/**
	 * callback class to answer on events from core.
	 *
	 * The class is implemented by core.
	 */
	class InstallAppCacheCallback {
	protected:
		InstallAppCacheCallback() {}
		virtual ~InstallAppCacheCallback() {}

	public:

		/**
		 * Asynchronous callback for OnDownloadAppCache event
		 */
		virtual void OnDownloadAppCacheReply(BOOL allow_installation) = 0;

		/**
		 * Asynchronous callback for OnCheckForNewAppCacheVersion event
		 */
		virtual void OnCheckForNewAppCacheVersionReply(BOOL allow_checking_for_update) = 0;

		/**
		 * Asynchronous callback for OnDownloadNewAppCacheVersion event
		 */
		virtual void OnDownloadNewAppCacheVersionReply(BOOL allow_installing_update) = 0;
	};

	/**
	 *  This method is called before we start to download all resources
	 *  for a offline web application to cache for the first time ("install it").
	 *
	 *  The installation process is triggered by a manifest attribute on
	 *  the html tag.
	 *
	 *  The listener can stop the application from being installed in the
	 *  application cache (in which case it will work continue working
	 *  like a regular web page).  The decision about installing could
	 *  be made by e.g. asking the user, or by having a policy.
	 *
	 *  The reply for if the application is to be downloaded into the application
	 *  cache is given by calling callback->OnDownloadAppCacheReply().  This
	 *  is an asynchronous reply, so that e.g. the user may be asked through
	 *  the UI.  This reply callback MUST be called exactly once, EXCEPT if
	 *  a call to CancelDownloadAppCache with the same id is received before
	 *  the callback is called.
	 *  It is safe to call it directly from the OnDownloadAppCache function if
	 *  the answer is immediately available.
	 *
	 *  There is one OpApplicationCacheListener per WindowCommander. There
	 *  will be sent an event to the listeners on all windows loading
	 *  the same html document that triggers the application cache download process.
	 *  Only one should answer with callback->OnDownloadAppCacheReply(), as the
	 *  other listeners will  receive an CancelCheckForNewAppCacheVersion when core has
	 *  received exactly one answer.
	 *
	 *
	 *  @param commander  The OpWindowCommander which this application cache is
	 *                    being installed from.
	 *  @param id         ID of this request.  You could receive several calls
	 *                    to OnDownloadAppCache with the same ID, in which case
	 *                    they refer to the same application, in this case you
	 *                    only need to call the callback for one of them (the others
	 *                    will receive a CancelDownloadAppCache after the first
	 *                    answer is received).
	 *  @param callback   Function which should be called when you have determined
	 *                    if this web application should be installed.
	 */
	virtual void OnDownloadAppCache(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback) = 0;

	/**
	 *  This function will be called after a call to OnDownloadAppCache with the same
	 *  OpWindowCommander and ID, and before that function has called the callback
	 *  function.
	 *
	 *  It indicates that appcache isn't interested in the answer to the callback
	 *  for this window commander anymore, and that you should NOT call the callback.
	 *  If you e.g. asked the user through a dialog you can now close this dialog.
	 *
	 *  Most likely to happen if you have several windows associated with
	 *  the same application cache and it appcache got the answer it wanted from
	 *  one of the other  windows, or if the user navigated away from a window
	 *  receiving the event.
	 *
	 *  @param commander  The OpWindowCommander which this application cache is
	 *                    being installed from.
	 *  @param id         ID of this request.  You should have already receive
	 *                    a OnDownloadAppCache with the same OpWindowCommander
	 *                    and ID, and the callback function for that call should not
	 *                    be called anymore.
	 */
	virtual void CancelDownloadAppCache(OpWindowCommander* commander, UINTPTR id) = 0;

	/**
	 *  This method is called before an application cache attempts to check if
	 *  there is an new version on the server. The check is done by reconnecting to
	 *  the original site and download the manifest. If the manifest has changed,
	 *  a new version is available.
	 *
	 *  The listener can stop the cache from checking for new version.
	 *
	 *  The reply given by calling callback->OnCheckForNewAppCacheVersionReply().
	 *  This is an asynchronous reply, so that e.g. the user may be asked through
	 *  the UI.  This reply callback MUST be called exactly once, EXCEPT if
	 *  a call to CancelCheckForNewAppCacheVersion with the same id is received before
	 *  the callback is called.
	 *  It is safe to call it directly from the OnCheckForNewAppCacheVersion function if
	 *  the answer is immediately available.
	 *
	 *  There is one OpApplicationCacheListener per WindowCommander. There
	 *  will be sent an event to the listeners on all windows using the
	 *  cache that will be updated. Only one should answer with
	 *  callback->OnDownloadAppCacheReply(), as the other listeners will
	 *  receive an CancelCheckForNewAppCacheVersion when core has received an answer.
	 *
	 *  It may be sensible for devices which are always connected or were otherwise
	 *  bandwidth is not an issue to always return TRUE for this event.  For other
	 *  devices it may make more sense to check some other setting, like offline/online
	 *  mode to decide if to allow, than to ask the user each time.
	 *
	 *  @param commander  The OpWindowCommander which this application cache is
	 *                    being installed from.
	 *  @param id         ID of this request.  You could receive several calls
	 *                    to OnDownloadAppCache with the same ID, in which case
	 *                    they refer to the same application, in this case you
	 *                    only need to call the callback for one of them (the others
	 *                    will receive a CancelDownloadAppCache after the first
	 *                    answer is received).
	 *  @param callback   Function which should be called when you have determined
	 *                    if this web application should be installed.
	 */
	virtual void OnCheckForNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback) = 0;

	/**
	 *  This function will be called after a call to OnCheckForNewAppCacheVersion with the same
	 *  OpWindowCommander and ID, and before that function has called the callback
	 *  function.
	 *
	 *  It indicates that appcache isn't interested in the answer to the callback
	 *  for this window commander anymore, and that you should NOT call the callback.
	 *  If you e.g. asked the user through a dialog you can now close this dialog.
	 *
	 *  Most likely to happen if you have several windows associated with (using)
	 *  the same application cache and it appcache got the answer it wanted from
	 *  one of the other windows, or if the user navigated away from a window
	 *  receiving the event.
	 *
	 *  @param commander  The OpWindowCommander which this application cache is
	 *                    being installed from.
	 *  @param id         ID of this request.  You should have already receive
	 *                    a OnCheckForNewAppCacheVersion with the same OpWindowCommander
	 *                    and ID, and the callback function for that call should not
	 *                    be called anymore.
	 */
	virtual void CancelCheckForNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id) = 0;

	/**
	 *  This event is called if an new version of the cache is available.
	 *  The event should only be received if you have first got an OnCheckForNewAppCacheVersion
	 *  and answered TRUE on OnCheckForNewAppCacheVersionReply().
	 *
	 *  The reply is given by calling callback->OnCheckForNewAppCacheVersionReply().
	 *  This an asynchronous reply, so that e.g. the user may be asked through
	 *  the UI.  This reply callback MUST be called exactly once, EXCEPT if
	 *  a call to CancelCheckForNewAppCacheVersion with the same id is received before
	 *  the callback is called.
	 *
	 *  It is safe to call it directly from the OnDownloadNewAppCacheVersion function if
	 *  the answer is immediately available.
	 *
	 *  There is one OpApplicationCacheListener per WindowCommander. There
	 *  will be sent an event to the listeners on all windows using the
	 *  cache that will be updated. Only one should answer with
	 *  callback->OnDownloadNewAppCacheVersionReply(), as the other listeners will
	 *  receive an CancelDownloadNewAppCacheVersion when core has received an answer.
	 *
	 *  @param commander  The OpWindowCommander which this application cache is
	 *                    being installed from.
	 *  @param id         ID of this request.  You could receive several calls
	 *                    to OnDownloadAppCache with the same ID, in which case
	 *                    they refer to the same application, in this case you
	 *                    only need to call the callback for one of them (the others
	 *                    will receive a CancelDownloadAppCache after the first
	 *                    answer is received).
	 *  @param callback   Function which should be called when you have determined
	 *                    if this web application should be installed.
	 */
	virtual void OnDownloadNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback) = 0;

	/**
	 *  This function will be called after a call to OnDownloadNewAppCacheVersion with the same
	 *  OpWindowCommander and ID, and before that function has called the callback
	 *  function.
	 *
	 *  It indicates that appcache isn't interested in the answer to the callback
	 *  for this window commander anymore, and that you should NOT call the callback.
	 *  If you e.g. asked the user through a dialog you can now close this dialog.
	 *
	 *  Most likely to happen if you have several windows associated with (using)
	 *  the same application cache and it appcache got the answer it wanted from
	 *  one of the other windows, or if the user navigated away from a window
	 *  receiving the event.
	 *
	 *  @param commander  The OpWindowCommander which this application cache is
	 *                    being installed from.
	 *  @param id         ID of this request.  You should have already receive
	 *                    a OnCheckForNewAppCacheVersion with the same OpWindowCommander
	 *                    and ID, and the callback function for that call should not
	 *                    be called anymore.
	 */
	virtual void CancelDownloadNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id) = 0;

	class QuotaCallback
	{
	public:
		virtual ~QuotaCallback() {}

		/** Called by the UI when a decision has been made about increasing the quota.
		 *  @param allow_increase TRUE if the quota is allowed to increase.
		 *	@param new_quota_size Size to increase to (in bytes). Ignored if not allowed to increase, 0 if
		 *                        the quota should be unlimited. */
		virtual void OnQuotaReply(BOOL allow_increase, OpFileLength new_quota_size) = 0;
	};

	/**
	 *  Called when opera wants the ui to ask the user the user if he wants to increase
	 *	the application cache quota when a cache has exceeded its limit.
	 *	We unfortunately don't know how much space the application will need, so
     *  this event may be received several times for the same install/update of
     *  this application if the previous quota increase was not sufficient.
     *
	 *	QuotaCallback::OnQuotaReply MUST be called exactly once, EXCEPT if
	 *  a call to CancelIncreaseAppCacheQuota with the same id is received before
	 *  the callback is called.
	 *	It is safe to call directly from OnIncreaseAppCacheQuota.
	 *
	 *	@param commander the windowcommander in question
	 *  @param id        ID of this request.  You could receive several calls
	 *                   to OnIncreaseAppCacheQuota with the same ID, in which case
	 *                   they refer to the same application, in this case you
	 *                   only need to call the callback for one of them (the others
	 *                   will receive a CancelIncreaseAppCacheQuota after the first
	 *                   answer is received).
	 *	@param cache_domain The domain where cache comes from
	 *	@param current_quota_size The current quota limit that has been reached.
	 *	@param callback callback object to notify when the user replies
	 */
	virtual void OnIncreaseAppCacheQuota(OpWindowCommander* commander, UINTPTR id, const uni_char* cache_domain, OpFileLength current_quota_size, QuotaCallback *callback) = 0;

	/**
	 *  This function will be called after a call to OnIncreaseAppCacheQuota with the same
	 *  OpWindowCommander and ID, and before that function has called the callback
	 *  function.
	 *
	 *  It indicates that appcache isn't interested in the answer to the callback
	 *  for this window commander anymore, and that you should NOT call the callback.
	 *  If you e.g. asked the user through a dialog you can now close this dialog.
	 *
	 *  Most likely to happen if you have several windows with the same application
	 *  cache and it appcache got the answer it wanted from one of the other
	 *  windows, or if the user navigated away from a window receiving the event.
	 *
	 *  @param commander  The OpWindowCommander which this application cache is
	 *                    being installed from.
	 *  @param id         ID of this request.  You should have already receive
	 *                    a OnIncreaseAppCacheQuota with the same OpWindowCommander
	 *                    and ID, and the callback function for that call should not
	 *                    be called anymore.
	 */
	virtual void CancelIncreaseAppCacheQuota(OpWindowCommander* commander, UINTPTR id) = 0;


	/**
	 * The next events are sent to send various status updates when
	 * an application cache is updating or installing.  They may be used
	 * to e.g. update a progress bar in the UI.  (But they may also be ignored
	 * since this is something which happens in the background).
	 *
	 * The most interesting of them is likely OnAppCacheCached which signals
	 * that an application cache has finished installing or updating.
	 *
	 * Exactly when each event is sent is defined by the Offline Web applications
	 * specification of HTML 5.
	 *
	 * Also note that Javascript may cancel the event, so that you may not
	 * always receive them.
	 */
	virtual void OnAppCacheChecking(OpWindowCommander* commander) = 0;
	virtual void OnAppCacheError(OpWindowCommander* commander) = 0;
	virtual void OnAppCacheNoUpdate(OpWindowCommander* commander) = 0;
	virtual void OnAppCacheDownloading(OpWindowCommander* commander) = 0;
	virtual void OnAppCacheProgress(OpWindowCommander* commander) = 0;
	virtual void OnAppCacheUpdateReady(OpWindowCommander* commander) = 0;
	virtual void OnAppCacheCached(OpWindowCommander* commander) = 0;
	virtual void OnAppCacheObsolete(OpWindowCommander* commander) = 0;
};
#endif // APPLICATION_CACHE_SUPPORT

#ifdef WIC_EXTERNAL_APPLICATIONS_ENUMERATION
/** @short Discovery and execution of installed applications.
 *
 * This listener provides functionality for enumerating installed/known
 * applications.
 *
 * This mechanism serves two purposes:
 *  - starting a default application of a given type (e.g. default text editor
 *    or media player). This is different than opening a file with the associated
 *    application because it doesn't require any file to open.
 *  - enumerating all applications installed or available or configured by the
 *    user. This list of applications may be provided to the user or be exposed
 *    in JavaScript.
 *
 * Applications are divided into two groups: well-known and custom ones.
 *
 * The well-known applications have values in ApplicationType enum that are
 * used to identify them. The custom applications are identified by values
 * APPLICATION_CUSTOM and above (treated as integers).
 *
 * It is important that once an identifier is assigned to a custom application
 * it doesn't get reused for a different one (e.g. after the application is
 * removed and another one added). Identifiers may stop being valid
 * (application has been removed) and new ones may be assinged (new application
 * becomes available).
 *
 * In particular cases there may be no well-known applications or no custom
 * ones.
 */
class OpApplicationListener
{
public:
	/** A list of well-known application types.
	 */
	enum ApplicationType {
		/** Launches application for managing alarms.
		 *
		 * No arguments.
		 */
		APPLICATION_ALARM = 0,

		/** Launches browser.
		 * Arguments:
		 * - URL (optional)	- open the specified URL.
		 */
		APPLICATION_BROWSER,

		/** Launches calculator.
		 *
		 * No arguments.
		 */
		APPLICATION_CALCULATOR,

		/** Launches calendar.
		 *
		 * No arguments.
		 */
		APPLICATION_CALENDAR,

		/** Launches camera application.
		 *
		 * No arguments.
		 */
		APPLICATION_CAMERA,

		/** Launches contacts/address book application.
		 */
		APPLICATION_CONTACTS,

		/** Launches file browser/file manager.
		 *
		 * Arguments:
		 * - PATH (optional) - open the given folder or file (if associated with an application)
		 */
		APPLICATION_FILE_MANAGER,

		/** Launches application for starting games.
		 *
		 * May fall back to opening the games folder in file manager.
		 *
		 * Arguments:
		 * - GAME_NAME (optional) - launch game GAME_NAME.
		 */
		APPLICATION_GAME_MANAGER,

		/** Launches e-mail client.
		 *
		 * Arguments:
		 * - EMAIL (optional) - start e-mail composition addressed to EMAIL.
		 */
		APPLICATION_MAIL,

		/** Launches media player.
		 *
		 * Arguments:
		 * - URL (optional) - opens URL for playing.
		 */
		APPLICATION_MEDIA_PLAYER,

		/** Launches messaging application (SMS, MMS etc.)
		 *
		 * Arguments:
		 * - PHONE_NUMBER (optional) - start composition of a message addressed to PHONE_NUMBER.
		 *
		 * The phone number format should be the same as for APPLICATION_PHONE.
		 */
		APPLICATION_MESSAGING,

		/** Launches phone application.
		 *
		 * Arguments:
		 * - PHONE_NUMBER (optional) - dials the given PHONE_NUMBER.
		 *
		 * The phone number format should be: \+?[0-9wp*#]+
		 * where 'w' means "wait for dial-tone/busy"
		 * and 'p' is a pause.
		 */
		APPLICATION_PHONE,

		/** Launches image viewer.
		 *
		 * Arguments:
		 * - PATH (optional) - opens the specified image file.
		 */
		APPLICATION_IMAGE_VIEWER,

		/** Launcher program manager.
		 *
		 * May fall back to opening applications folder in file manager.
		 */
		APPLICATION_PROGRAM_MANAGER,

		/** Opens system settings.
		 *
		 * No arguments.
		 */
		APPLICATION_SETTINGS,

		/** Launches "to do list" application.
		 *
		 * No arguments.
		 */
		APPLICATION_TODO_TASKS,

		/** Launches widget manager.
		 *
		 * No arguments.
		 */
		APPLICATION_WIDGET_MANAGER,

		/** Launches text editor.
		 *
		 * Should be the same as GetDefaultTextEditorL.
		 *
		 * Arguments:
		 * - PATH (optional) - path to file to edit.
		 */
		APPLICATION_TEXT_EDITOR,

		APPLICATION_LAST,				///< Indicates the end of predefined values

		APPLICATION_CUSTOM = APPLICATION_LAST	///< Base value for custom application IDs.
	};

	/** Callback for GetInstalledApplications.
	 *
	 * It receives information about available applications, both known
	 * (specified in ApplicationType enum) and custom ones.
	 */
	class GetInstalledApplicationsCallback
	{
	public:
		virtual ~GetInstalledApplicationsCallback() { }

		/** Provides information about a known application being installed.
		 *
		 * @param type Type of the application. It may later be passed to
		 *        enum-based ExecuteApplication.
		 * @return See OpStatus.
		 *         No more calls may be performed to the callback if an error is
		 *         returned, not even to OnFailed().
		 */
		virtual OP_STATUS OnKnownApplication(ApplicationType type) = 0;

		/** Provides information about a custom application.
		 *
		 * @param path Unique path to the executable. It may later be passed to the
		 *        path-based ExecuteApplication function.
		 *        On platforms with no ExecuteApplication it may be any identifier
		 *        but it must be unique.
		 * @param type Unique type of the application. It must be equal to or greater
		 *        than ApplicationType::APPLICATION_CUSTOM and cannot be reused for
		 *        a different application.
		 *        It may later be passed to the enum-based ExecuteApplication.
		 * @return See OpStatus.
		 *         No more calls may be performed to the callback if an error is
		 *         returned, not even to OnFailed().
		 */
		virtual OP_STATUS OnCustomApplication(ApplicationType type, const uni_char * path) = 0;

		/** Indicates that an error occurred and it is impossible to provide the results.
		 *
		 * No more calls to the callback objects may be performed after a call to
		 * this method.
		 */
		virtual void OnFailed(OP_STATUS error) = 0;

		/** Indicates that all the results have been provided.
		 *
		 * No more calls to the callback objects may be performed after a call to
		 * this method.
		 */
		virtual void OnFinished() = 0;
	};

	/** Provides list of available applications.
	 *
	 * @param callback callback object that will receive results. Methods on the
	 * callback may be called synchronously from the method (e.g. to immediately
	 * report an error).
	 */
	virtual void GetInstalledApplications(GetInstalledApplicationsCallback * callback) = 0;

	/** Execute application.
	 *
	 * @param type A predefined application to run.
	 * @param argc Number of arguments.
	 * @param argv Array of arguments, may be NULL if argc is 0.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_SUCH_RESOURCE if there
	 *         is no application of given type, other error values.
	 */
	virtual OP_STATUS ExecuteApplication(ApplicationType type, unsigned int argc, const uni_char * const * argv) = 0;
};
#endif // WIC_EXTERNAL_APPLICATION_ENUMERATION

#ifdef WIC_OS_INTERACTION

class OpOSInteractionListener
{
public:
	/** Callback object used to respond to the query about desktop image format
	 *
	 * @param error - OpStatus::OK or appropriate error code if any occurred.
	 * @param content_type - content type requested by platform. If this is different than
	 *        content_type parameter passed to OnQueryDesktopBackgroundImageFormat then
	 *	      core will try to convert image to approriate image format.
	 *        Currently only conversion to BMP is supported.
	 * @param dir_path - path to where image file will be saved. It MUST always end with
	 *        path separator
	 */
	class QueryDesktopBackgroundImageFormatCallback
	{
	public:
		virtual void OnFinished(OP_STATUS error, URLContentType content_type, const uni_char* dir_path) = 0;
	};

	/** Callback object used to notify about finishing setting desktop image
	 *
	 * @param error - OpStatus::OK if settting desktop image succeeded
	 *        or appropriate error code if any occurred.
	 */
	class SetDesktopBackgroundImageCallback
	{
	public:
		virtual void OnFinished(OP_STATUS error) = 0;
	};

	/** Queries product about accepted content format of background image and
	 *  desired image file location.
	 *
	 * @param content_type - suggested_file format.
	 * @param callback - callback object called to reply to the query.
	 */
	virtual void OnQueryDesktopBackgroundImageFormat(URLContentType content_type, QueryDesktopBackgroundImageFormatCallback* callback) = 0;

	/** Requests setting desktop image to a specified file
	 *
	 * @param file_path - location of the file to set to desktop image.
	 * @param callback - callback object called to notify about finishing setting background image.
	 */
	virtual void OnSetDesktopBackgroundImage(const uni_char* file_path, SetDesktopBackgroundImageCallback* callback) = 0;
};
#endif // WIC_OS_INTERACTION

#ifdef DOM_LOAD_TV_APP
/**
 * Listener used to pass down to gogi a request to launch a tv application.
 */
class OpTVAppMessageListener
{
public:
	virtual ~OpTVAppMessageListener() {}
	/**
	 * @short Called when JS calls opera.loadTVApp.
	 *
	 * Notify integration that a tv application should be launched.
	 * Passed parameters should enable it to display basic information about it to the user
	 * as well as launch it using op_open_tv_app. All passed parameters are
	 * owned by caller and if they're needed after OnLoadTVApp returns, copies
	 * should be made!
	 *
	 * @param wic WindowCommander of a window from which opera.loadTVApp originated
	 * @param app_id Application's url encoded with AES and then base64.
	 *               Should be passed to integration which will pass it to op_open_tv_app if it decided to launch the application.
	 * @param name Human readable name of the application.
	 * @param icon_url URL pointing to the application's icon. To be used for display.
	 * @param whitelist Collection of urls (files or directories) that can be loaded in the application, or NULL (everything is allowed then).
	 *
	 * @return
	 * OK if integration received and handled the GOGI_OPERA_EVT_LOAD_TV_APP event.
	 *         This does not mean the application will be launched.
	 * ERR_NO_MEMORY in case of OOM condition.
	 * ERR_OUT_OF_RANGE if integration does not handle this event for whatever the reason.
	 * ERR for any other error.
	 */
	virtual OP_STATUS OnLoadTVApp(OpWindowCommander* wic, const uni_char* app_url, const uni_char* name, const uni_char* icon_url, OpVector<OpStringC>* whitelist) = 0;

	/**
	 * @short Called when the TV Application tries to open url that is not contained on it's whitelist.
	 *
     * @param wic WindowCommander of a window of a TV Application
     * @param forbidden_url the full path that was attempted to be loaded
     * @return
	 * ERR_NO_MEMORY in case of OOM condition.
	 * OK in any other case
     */
	virtual OP_STATUS OnOpenForbiddenUrl(OpWindowCommander* wic, const uni_char* forbidden_url) = 0;
};
#endif //DOM_LOAD_TV_APP

/** @short Core window commander.
 *
 * This represents core window towards the outside (user interface). It allows
 * manipulation ("commanding") of the core representation of a window, such as
 * "go to a URL", "go back in history", "reload current page", "disable display
 * of images", etc.
 *
 * Core implements and owns OpWindowCommander. User interface code calls its
 * methods.
 *
 * There is also a set of listeners that allow the user interface to get
 * notifications from core. Notifications may be "document loading finished",
 * "loading failed", "web page title changed", "file name for upload
 * needed". Each listener class covers a certain category of events, such as
 * document related events, loading related events, error events, and so
 * on. The user interface needs to re-implement and set (in OpWindowCommander)
 * the listeners that represent types of events that the user interface is
 * interested in.
 *
 * The user interface implements and owns listeners. Core code calls their
 * methods.
 *
 * Some listener method calls may, in spite of looking like pure events,
 * require the user interface code to call back into core (usually via some
 * sort of call-back context objects) when it is "done with something" (this is
 * always documented, per method). For example: If core needs a filename for
 * upload (\<input type="file"> in HTML), core will call
 * OpFileSelectionListener::OnUploadFilesRequest(). One of the parameters is
 * UploadCallback, on which the user interface code must call OnFilesSelected()
 * when the user is done selecting files. The UploadCallback object also
 * provides relevant information to the user interface, such as expected media
 * types, number of files expected, and so on.
 *
 * Core implements and owns the call-back classes. User interface code calls
 * their methods.
 */
class OpWindowCommander
{
public:

	enum Encoding {
		ENCODING_AUTOMATIC,
		ENCODING_UTF8,
		ENCODING_UTF16,
		ENCODING_USASCII,
		ENCODING_ISO8859_1,
		ENCODING_ISO8859_2,
		ENCODING_ISO8859_3,
		ENCODING_ISO8859_4,
		ENCODING_ISO8859_5,
		ENCODING_ISO8859_6,
		ENCODING_ISO8859_7,
		ENCODING_ISO8859_8,
		ENCODING_ISO8859_9,
		ENCODING_ISO8859_10,
		ENCODING_ISO8859_11,
		ENCODING_ISO8859_13,
		ENCODING_ISO8859_14,
		ENCODING_ISO8859_15,
		ENCODING_ISO8859_16,
		ENCODING_KOI8_R,
		ENCODING_KOI8_U,
		ENCODING_CP_1250,
		ENCODING_CP_1251,
		ENCODING_CP_1252,
		ENCODING_CP_1253,
		ENCODING_CP_1254,
		ENCODING_CP_1255,
		ENCODING_CP_1256,
		ENCODING_CP_1257,
		ENCODING_CP_1258,
		ENCODING_SHIFTJIS,
		ENCODING_ISO2022_JP,
		ENCODING_ISO2022_JP_1,
		ENCODING_ISO2022_CN,
		ENCODING_ISO2022_KR,
		ENCODING_BIG5,
		ENCODING_BIG5_HKSCS,
		ENCODING_EUC_JP,
		ENCODING_GB2312,
		ENCODING_VISCII,
		ENCODING_EUC_KR,
		ENCODING_HZ_GB2312,
		ENCODING_GBK,
		ENCODING_EUC_TW,
		ENCODING_IBM866,
		ENCODING_WINDOWS_874,
		ENCODING_JAPANESE_AUTOMATIC,
		ENCODING_CHINESE_AUTOMATIC,
		ENCODING_KOREAN_AUTOMATIC,
		ENCODING_GB18030,
		ENCODING_MACROMAN,
		ENCODING_MAC_CE,
		ENCODING_MAC_CYRILLIC,
		ENCODING_MAC_GREEK,
		ENCODING_MAC_TURKISH,
		ENCODING_X_USER_DEFINED,
		NUM_ENCODINGS
	};

	/** @short Document save mode.
	 *
	 * Specifies how a document is saved to file.
	 */
	enum SaveAsMode
	{
		/** @short Just save the document, without any conversion. */
		SAVE_ONLY_DOCUMENT = 0,

		/** @short Save the document with inlines.
		 *
		 * This will save the document itself as one file, and store each of
		 * the external resources to which the document's markup links (images,
		 * scripts, style sheets, etc.) as individual files in a subdirectory.
		 */
		SAVE_DOCUMENT_AND_INLINES = 1,

#ifdef MHTML_ARCHIVE_SAVE_SUPPORT
		/** @short Save the document with inlines as one MHTML file. */
		SAVE_AS_MHTML = 2,
#endif // MHTML_ARCHIVE_SAVE_SUPPORT

#ifdef WIC_SAVE_DOCUMENT_AS_TEXT_SUPPORT
		/** @short Save the document converted to text.
		 *
		 * This may be used to save an HTML document as plain text. Some basic
		 * layout (paragraphs, line breaks, etc.) will be retained in the text
		 * version.
		 */
		SAVE_AS_TEXT = 3,
#endif // WIC_SAVE_DOCUMENT_AS_TEXT_SUPPORT

#ifdef SVG_SUPPORT
		/** @short Save the document converted to a PNG image.
		 *
		 * This currently only works with a very limited set of document types;
		 * in fact it will only work if DocumentType is DOC_SVG.
		 */
		SAVE_AS_PNG = 4
#endif // SVG_SUPPORT
	};

	/** @short Type of document.
	 *
	 * A value from this enum may be used to describe the type of the document
	 * currently displayed.
	 */
	enum DocumentType {
		DOC_PLAINTEXT,
		DOC_HTML,
		DOC_MHTML,
		DOC_XHTML,
		DOC_WML,
		DOC_SVG,
		DOC_WEBFEED,
		DOC_PLUGIN,

		/** @short Other type of text document. */
		DOC_OTHER_TEXT,

		/** @short Other type of graphics document (e.g. JPEG). */
		DOC_OTHER_GRAPHICS,

		/** @short Other type of document. */
		DOC_OTHER
	};

	/** @short Is the specified document type a "web document" type?
	 *
	 * A "web document" is a document of such a type that it may require
	 * additional resources to be displayed, so that saving it "with inlines"
	 * or as MHTML makes sense. For example, an HTML document may contain an
	 * IMG / IFRAME / LINK / whatever element that refers to additional
	 * resources. An HTML document is a "web document". The opposite of a "web
	 * document" is a document of such type that it is always
	 * self-contained. For example, a GIF image it is not a "web document".
	 */
	static inline BOOL IsWebDocumentType(DocumentType type)
	{
		switch (type)
		{
		case DOC_HTML:
		case DOC_MHTML:
		case DOC_XHTML:
		case DOC_WML:
			return TRUE;

		default:
			return FALSE;
		}
	}

	virtual ~OpWindowCommander() {}

	virtual OP_STATUS Init() = 0;

	// === Browsing stuff ===

	/** This class collects the various parameters to how OpenURL()
	 * should behave.  Creating one of these objects should give you
	 * reasonable default values, so you only need to set the ones you
	 * care about.
	 *
	 * The default values are based on the assumption that the user
	 * performed an action similar to clicking a link on a web page.
	 * Most cases of OpenURL being called from the UI is probably
	 * because the user explicitly chose a particular URL (e.g. typed
	 * into the address bar or activated a bookmark).  In these cases
	 * 'entered_by_user' should be set to TRUE.  This is NOT the
	 * default value because it reduces the security constraints on
	 * the URL.
	 */
	class OpenURLOptions
	{
	public:
		OpenURLOptions() :
			entered_by_user(FALSE),
			requested_externally(FALSE),
			context_id((URL_CONTEXT_ID)-1),
			replace_prev_history_entry(FALSE)
			{
			};

		/** Specifies that the user consciously requested loading of
		 * this URL.
		 *
		 * There is a set of parameters that describes the source of
		 * the URL.  (Should they be replaced by a single enum?)
		 *
		 * entered_by_user should be TRUE iff the user explicitly
		 * chose to load this particular URL.  So it should be TRUE
		 * when the user types an address into the address bar or
		 * selects a bookmark.  It should be FALSE when the user
		 * clicks a link on a web page.  (I don't know what value it
		 * should have in cases like some javascript on a web page
		 * automatically invokes a bookmark).  NOTE: setting
		 * entered_by_user to TRUE reduces security checks (e.g. to
		 * allow bookmarklets, http://www.bookmarklets.com/).  This is
		 * why the default value is FALSE.
		 *
		 * requested_externally should be TRUE iff the URL was
		 * requested opened by an external source.  For example,
		 * through an IPC request or as a command-line parameter.  In
		 * this case extra safety measures may be taken, e.g. to avoid
		 * bouncing a url between multiple applications (see
		 * e.g. DSK-278988).
		 */
		BOOL entered_by_user;

		/** Specifies that the URL was requested by an external
		 * application.
		 *
		 * See 'entered_by_user' for details.
		 */
		BOOL requested_externally;

		/** Specifies the context this URL belongs to.
		 *
		 * For example each widget (gadget) has their own context.
		 */
		URL_CONTEXT_ID context_id;

		/** Specifies that this URL should replace the previous URL in
		 * the history (does this mean tab or global or both?) rather
		 * than create a new entry.
		 */
		BOOL replace_prev_history_entry;
	};

	/** Open the given url.
	 *
	 * @param url the url to open.
	 * @param options how to open this url (See OpenURLOptions).
	 *
	 * @return TRUE if everything went well and Opera will start
	 * loading the url, FALSE otherwise.
	 */
	virtual BOOL OpenURL(const uni_char* url, const OpenURLOptions & options) = 0;

#ifdef DOM_LOAD_TV_APP
	/** Open the given tv application.
	 *
	 * The whitelist parameter may be NULL, and then every url is allowed to
	 * be loaded by the application, or it may be a NULL-terminated array
	 * of strings, and then the url is allowed to be loaded by the application
	 * if and only if at least one of the whitelist items is a prefix of it.
	 * If the whitelist is not NULL, WindowCommander takes ownership of it
	 * (even if LoadTvApp() fails), and the whitelist and it's elements
	 * should be allocated by OP_NEWA().
	 *
	 * @param url the url to open.
	 * @param options how to open this url (See OpenURLOptions).
	 * @param whitelist the list of pages that this tv application is allowed to load.
	 *
	 * @return TRUE if everything went well and Opera will start
	 * loading the url, FALSE otherwise.
	 */
	virtual BOOL LoadTvApp(const uni_char* url, const OpenURLOptions & options, OpAutoVector<OpString>* whitelist) = 0;

	/** Clean internal data related to the given tv application (currently only deletes a whitelist) */
	virtual void CleanupTvApp() = 0;
#endif

	/** Open the given url as if the user clicked a link.
	 *
	 * (I don't know what this actually means, compared to OpenURL.
	 * But it seems to involve re-using behavioural traits from
	 * already loaded versions of url and referrer_url rather than
	 * making new ones.  If someone does know what this method is
	 * supposed to do, it would be nice if they could document it
	 * properly.)
	 *
	 * This method will interpret options.context_id slightly
	 * differently: The default value -1 will be handled as "no
	 * context id" and should reflect this behaviour in sub-calls.
	 * (although it is beyond me how "no context id" differs from
	 * "default context id".  If somebody actually knows what this
	 * means, it would be nice if they could properly document it.)
	 *
	 * @param url the url to open.
	 *
	 * @param referrer_url the referring url, i.e. the url from where
	 * this url originated.  For example the currently active document
	 * url.
	 *
	 * @param options how to open this url (See OpenURLOptions).
	 */
	virtual BOOL FollowURL(const uni_char* url,
	                       const uni_char* referrer_url,
	                       const OpenURLOptions & options) = 0;

	/** Open the url in url.
	    @deprecated Use the form taking OpWindowCommander::OpenURLOptions
	    instead.
		@param url the url to open
		@param entered_by_user pass in TRUE if the url can be
		considered to be consciously entered by the user (at some time).
		example: TRUE if url comes from an address bar or from a bookmark,
		FALSE if the user clicked on a link. Reduces security checks if TRUE
		(necessary for eg bookmarklets, http://www.bookmarklets.com/ ).
		@param context_id URL context ID. For example, each widget
		(gadget) has their own context
		@param use_prev_entry if TRUE, than no history entry will be created about this url,
		just used the previous entry
		@return TRUE if everything went well and Opera will start
		loading the url, FALSE otherwise.
	*/
	virtual BOOL OpenURL(const uni_char* url, BOOL entered_by_user,
						 URL_CONTEXT_ID context_id = (URL_CONTEXT_ID)-1,
						 BOOL use_prev_entry = FALSE) = 0;

	/** Follow the url in url.
		Will behave more similar as 'clicking' a link, than when using OperURL
	    @deprecated Use the form taking OpWindowCommander::OpenURLOptions
	    instead.
		@param url the url to open
		@param referrer_url the referring url, for example the active document url
		@param entered_by_user pass in TRUE if the url can be
		considered to be consciously entered by the user (at some time).
		example: TRUE if url comes from an address bar or from a bookmark,
		FALSE if the user clicked on a link. Reduces security checks if TRUE
		(necessary for eg bookmarklets, http://www.bookmarklets.com/ ).
		@param context_id URL context ID. For example, each widget
		(gadget) has their own context. The default -1 should be handeled as
		"No context id", and should reflect this behaviour in sub-calls.
	*/
	virtual BOOL FollowURL(const uni_char* url, const uni_char* referrer_url,
						   BOOL entered_by_user,
						   URL_CONTEXT_ID context_id = (URL_CONTEXT_ID)-1) = 0;

	/** Reload the top/main document. Sub resources may or may not be reloaded depending
	 *  on the force_inline_reload flag and cache policies.
	 *  @param force_inline_reload If TRUE inline resources (CSS, Scripts, Images, ...) will also be reloaded
	 *         even if the cache flags would have allowed them to be reused. This is a behaviour that UIs
	 *         often present as Ctrl+Reload, Shift+Reload or both. Default is to not reload inline
	 *         resources if it's not required. */
	virtual void Reload(BOOL force_inline_reload = FALSE) = 0;

	/** Go to the next in history
		@return BOOL TRUE if there was a next, FALSE otherwise */
	virtual BOOL Next() = 0;

	/** Go to the previous in history
		@return BOOL TRUE if there was a previous, FALSE otherwise */
	virtual BOOL Previous() = 0;

	/**
	 * Retrieves the current minimum and maximum window history positions.
	 * @see OpWindowCommander::GetCurrentHistoryPos()
	 *
	 * @param[out] min the lowest window history position (cannot go back past this)
	 * @param[out] max the highest window history position (cannot go forwards past this)
	 */
	virtual void GetHistoryRange(int* min, int* max) = 0;

	/**
	 * This will return the current history position.
	 * The returned value is between the minimum and the maximum values
	 * returned by GetHistoryRange().
	 * @see OpWindowCommander::GetHistoryRange()
	 * @see OpWindowCommander::SetCurrentHistoryPos()
	 *
	 * @returns the history position.
	 */
	virtual int GetCurrentHistoryPos() = 0;

	/**
	 * Set the history position.
	 * The position must be within bounds of the GetHistoryRange min and max values.
	 * However, if trying to set a value either lower, or higher will set the current
	 * lowest or highest.
	 * @see OpWindowCommander::GetHistoryRange()
	 * @see OpWindowCommander::GetCurrentHistoryPos()
	 *
	 * @returns the new position set.
	 */
	virtual int SetCurrentHistoryPos(int pos) = 0;

	/**
	 * Store user data for a specific history position.
	 *
	 * The data can later be retrieved with GetHistoryUserData().
	 *
	 * Core will take the ownership of the specified instance of user
	 * data and may delete it when the associated history entry is no
	 * longer kept in memory or when new history user data is
	 * set. This also implies that there is no guarantee that the data
	 * is still left when GetHistoryUserData() is later called, but if
	 * any data is returned it is guaranteed to be the same data for
	 * the same history_ID.
	 *
	 * @param history_ID the ID for the history position, previously
	 * sent in OpViewportInfoListener::OnNewPage().
	 * @param user_data a pointer to the user data to set.
	 * @retval OpStatus::OK if operation was successful
	 * @retval OpStatus::ERR if operation failed, for example if the history_ID could not be found
	 */
	virtual OP_STATUS SetHistoryUserData(int history_ID, OpHistoryUserData* user_data) = 0;
	/**
	 * Returns previously stored user data for a specific history
	 * position.
	 *
	 * @see SetHistoryUserData().
	 *
	 * @param history_ID the ID for the history position
	 * @param[out] user_data where to fill in the pointer to the user
	 * data. If no user data had previously been set, it is filled
	 * with NULL. The returned instance is still owned by core and may
	 * be deleted by core when the associated history entry is no
	 * longer kept in memory.
	 * @retval OpStatus::OK if operation was successfull
	 * @retval OpStatus::ERR if an error occured, for example if the history_ID could not be found
	 */
	virtual OP_STATUS GetHistoryUserData(int history_ID, OpHistoryUserData** user_data) const = 0;

	/** Stops the loading of the current document */
	virtual void Stop() = 0;

	/** Navigate in the specified direction. The actual algorithm that
        is being used is decided elsewhere. Talk to your local
        navigation dealer for directions. */
	virtual BOOL Navigate(UINT16 direction_degrees) = 0;

	/** @short Get the DOCTYPE of the current document.
	 *
	 * @param doctype The string returned should be copied immediately
	 *   you can't depend on it being alive after you have returned.
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	 */
	virtual OP_STATUS GetDocumentTypeString(const uni_char** doctype) = 0;

	/** @short Mark the toplevel window as being closable by script.
	 *
	 * Closing a window by script (using window.close()) is constrained to
	 * windows that were opened by script, window.open(). To also have the
	 * window controlled by this window commander be script closable, use
	 * SetCanBeClosedByScript(). Typically only used when creating and
	 * initializing a new window.
	 *
	 * @param can_close If TRUE then allow window closing, FALSE to revoke
	 *        any window.close() allowances.
	 */
	virtual void SetCanBeClosedByScript(BOOL can_close) = 0;

#ifdef SAVE_SUPPORT

#ifdef SELFTEST
	/** @short Save a rectangle of the window as a PNG file
	 *
	 * @param file_name the file name to save the image to
	 * @param rect The part of the window to include
	 *   (Note x and y are currently not used: will be treated as 0,0)
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	 */
	virtual OP_STATUS SaveToPNG(const uni_char* file_name, const OpRect& rect ) = 0;
#endif // SELFTEST

	/** @short Suggest a file name for the current document.
	 *
	 * Asking core to suggest a suitable file name for the current document may
	 * be useful if the product/UI wants to save it to file (by calling
	 * SaveDocument()). Note that the suggested file name may contain
	 * characters that are illegal in file names on some platforms. It is the
	 * platform's responsibility to filter out these characters before passing
	 * it to SaveDocument(), for instance.
	 *
	 * @param focused_frame If TRUE, address the document in the current
	 * frame. If FALSE, address the top level document.
	 * @param[out] suggested_filename Set to the suggested file name.
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	 */
	virtual OP_STATUS GetSuggestedFileName(BOOL focused_frame, OpString* suggested_filename) = 0;

	/** @short Get the type of the current document.
	 *
	 * @param focused_frame If TRUE, address the document in the current
	 * frame. If FALSE, address the top level document.
	 *
	 * @return The document type.
	 */
	virtual DocumentType GetDocumentType(BOOL focused_frame) = 0;

	/** @short Save the current document to disk.
	 *
	 * To get the suggested file name for the current document, call
	 * GetSuggestedFileName().
	 *
	 * @param file_name is the name file to save the document to.
	 * @param mode specifies if inline elements should be saved and
	 *  what the target format is @see OpWindowCommander::SaveAsMode.
	 * @param focused_frame if TRUE and the current document is a
	 *  frames document, only the currently focused frame (document)
	 *  will be saved; if FALSE and the current document is a frames
	 *  document, the top-level frame document will be saved. If the
	 *  current document does not contain frames, then this argument
	 *  is ignored.
	 * @param max_size is the maximum size in bytes that the saved
	 *  page is allowed to use on saving mhtml documents (mode ==
	 *  OpWindowCommander::SAVE_AS_MHTML). If the page exceeds the
	 *  limit, Opera tries to reduce its size by omitting parts of the
	 *  document. If the page can't be reduced to a sufficiently small
	 *  size this operation fails and returns an error status.
	 *
	 *  Use 0 for no limit. For any other mode than mhtml, you must
	 *  use 0.
	 * @param page_size is an optional pointer to a variable that
	 *  (if not null) receives on output the original size of the
	 *  page.
	 * @param saved_size is an optional pointer to a variable that
	 *  (if not null) receives on output the actual bytes written.
	 *
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY if there is not enough memory
	 *  to complete the operation.
	 * @retval OpStatus::ERR_NOT_SUPPORTED when max_size is not 0 in a
	 *  non supported SaveAsMode.
	 * @retval OpStatus::ERR for other errors.
	 */
	virtual OP_STATUS SaveDocument(const uni_char* file_name, SaveAsMode mode, BOOL focused_frame, unsigned int max_size=0, unsigned int* page_size = NULL, unsigned int* saved_size = NULL) = 0;

#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	/**
	 * Retrieve the original URL which a MHTML archive was loaded from.
	 *
	 * If FEATURE_MHTML_ARCHIVE_REDIRECT is enabled, the loading of an
	 * MHTML archive may cause a redirect from the MHTML URL to the
	 * document loaded from the archive.
	 *
	 * For example, if 'http://www.example.com' is saved to
	 * 'file://localhost/C:/pages/example.mht', and then 'example.mht' is
	 * loaded, the contents is loaded into cache, and Opera redirects to
	 * 'http://www.example.com', whereupon the page is loaded from cache.
	 *
	 * The url of the document is therefore http://www.example.com
	 *
	 * This method returns the original URL for the current document,
	 * (in the  example above: 'file://localhost/C:/pages/example.mht')
	 * or an empty string if no such redirection has taken place.
	 *
	 *
	 * @param url string containing the url upon return, or empty if no such redirect has taken place
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	 */
	virtual OP_STATUS GetOriginalURLForMHTML(OpString& url) = 0;
#endif // MHTML_ARCHIVE_REDIRECT_SUPPORT

#endif // SAVE_SUPPORT

	/** Initiates a new transfer, as a response to the DocumentListener::OnDownloadRequest
		callback
		@param wic the windowcommander that the download belongs to
		@param url the url for the download
		@param action the preferred TransferAction for this download
		@param filename to which target file the download shall be written
	*/
	virtual OP_STATUS InitiateTransfer(OpWindowCommander* wic, uni_char* url, OpTransferItem::TransferAction action, const uni_char* filename) = 0;

	/** Cancels a transfer, as a response to the DocumentListener::OnDownloadRequest
		callback
		@param wic the windowcommander that the download belongs to
		@param url the url for the download
	*/
	virtual void CancelTransfer(OpWindowCommander* wic, const uni_char* url) = 0;

#ifdef SUPPORT_GENERATE_THUMBNAILS
	/** Create a thumbnail generator that can be used to synchronously create thumbnails for the current page
	  * @return thumbnail generator for current window, valid while the current window is valid
	  *         NB: Caller is responsible for destroying the object
	  */
	virtual class ThumbnailGenerator* CreateThumbnailGenerator() = 0;
#endif // SUPPORT_GENERATE_THUMBNAILS

#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	/** @short Inspect an element in the debugger.
	 *
	 * It is up to the scope module and the debugger to choose how to handle
	 * it, but opening a DOM inspector would be a good idea.
	 *
	 * A debugger should already be connected, or should be launched soon after
	 * a call to this function.
	 *
	 * @param[in] context Used to find a suitable element to inspect
	 */
	virtual OP_STATUS DragonflyInspect(OpDocumentContext& context) = 0;
#endif // SCOPE_ECMASCRIPT_DEBUGGER

#ifdef INTEGRATED_DEVTOOLS_SUPPORT
	/** @short Open the URL as a developer tool (e.g. Dragonfly).
	 *
	 * Opening a URL with this function will give the window access to additional
	 * DOM APIs normally only available to Dragonfly. Any URL can be opened as
	 * a developer tool, but keep in mind that this will give the window access
	 * to your entire browser session.
	 *
	 * @param[in] url The URL to open. If NULL, then the current developer tool
	 *                URL in user preferences will be used (if possible).
	 */
	virtual BOOL OpenDevtools(const uni_char *url = NULL) = 0;
#endif //INTEGRATED_DEVTOOLS_SUPPORT

	/** @short Return the rect describing the position and size of the element captured by the context.
	 *
	 * @param context The context describing the element
	 * @param[out] rect The rectangle containing the element described in screen coordinates
	 * @return
	 *      OpStatus::OK if the rect was successfully fetched and is available in #rect
	 *      OpStatus::ERR if the rect was not accessible. The values in #rect does not
	 *      represent the elements rectangle.
	 */
	virtual OP_STATUS GetElementRect(OpDocumentContext& context, OpRect& rect) = 0;

#ifdef SVG_SUPPORT
	/** @short Zoom an SVG in place by some delta.
	 *
	 * @param context Used to find which SVG document fragment to zoom
	 * @param zoomdelta A percentage delta (100% is represented by the value
	 * 100), negative means zoom out, positive means zoom in
	 */
	virtual OP_STATUS SVGZoomBy(OpDocumentContext& context, int zoomdelta) = 0;

	/** @short Zoom an SVG in place to a particular value.
	 *
	 * @param context Used to find which SVG document fragment to zoom
	 * @param zoomlevel A percentage to zoom to (100% is represented by the
	 * value 100)
	 */
	virtual OP_STATUS SVGZoomTo(OpDocumentContext& context, int zoomlevel) = 0;

	/** @short Resets pan position to original values in an SVG.
	 *
	 * @param context Used to find which SVG document fragment to apply
	 * operation to.
	 */
	virtual OP_STATUS SVGResetPan(OpDocumentContext& context) = 0;

	/** @short Start/resume playing SVG animations.
	 *
	 * @param context Used to find which SVG document fragment to apply
	 * operation to.
	 */
	virtual OP_STATUS SVGPlayAnimation(OpDocumentContext& context) = 0;

	/** @short Pause playing SVG animations.
	 *
	 * @param context Used to find which SVG document fragment to apply
	 * operation to.
	 */
	virtual OP_STATUS SVGPauseAnimation(OpDocumentContext& context) = 0;

	/** @short Stop playing SVG animations.
	 *
	 * @param context Used to find which SVG document fragment to apply
	 * operation to.
	 */
	virtual OP_STATUS SVGStopAnimation(OpDocumentContext& context) = 0;
#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
	/** @short Play or pause media content.
	 *
	 * @param context Used to find which media element to apply operation to.
	 * @param play If TRUE the media contents is played, if FALSE it is paused.
	 */
	virtual OP_STATUS MediaPlay(OpDocumentContext& context, BOOL play) = 0;

	/** @short Mute or unmute media content.
	 *
	 * @param context Used to find which media element to apply operation to.
	 * @param mute If TRUE the media content is muted, otherwise it is unmuted.
	 */
	virtual OP_STATUS MediaMute(OpDocumentContext& context, BOOL mute) = 0;

	/** @short Show or hide media controls.
	 *
	 * @param context Used to find which media element to apply operation to.
	 * @param controls If TRUE the media content controls are shown, otherwise
	 *     they are hidden.
	 */
	virtual OP_STATUS MediaShowControls(OpDocumentContext& context, BOOL show) = 0;
#endif // MEDIA_HTML_SUPPORT

	// === Listener methods dpt (must be called properly!) ===

	/** Must be called when the ui has created a ui window and wishes
        Opera to put an Opera window in it.

	    This method couples an OpWindow and a WindowCommander. */

	virtual OP_STATUS OnUiWindowCreated(OpWindow* opWindow) = 0;

	/** Must be called when the ui is about to close a ui window.
		After this call, no listeners will be called on the windowcommander.
		Also no methods may be called on the windowcommander object.

		A window can not safely be closed by the UI when the UI is
		called by core, unless the call is to a message handler
		registered by the UI.  In other words, to safely close a
		window, the UI should wait until the next time Opera::Run()
		returns or post a message to itself and close the window from
		the message handler. */
	virtual void OnUiWindowClosing() = 0;

#ifndef DOXYGEN_DOCUMENTATION
	/** This method is going away very soon.
	 *
	 * See ViolateCoreEncapsulationAndGetWindow().
	 */
	Window* GetWindow();  // DEPRECATED

	/** Violate core encapsulation.
	 *
	 * Please avoid calling this method! It will be deprecated and removed any
	 * year soon.
	 *
	 * The other methods in OpWindowCommander should provide all you need in
	 * order to control a core browser window from the product/UI side. If this
	 * isn't the case, please attempt to fix it or report a bug to the
	 * windowcommander module owner.
	 *
	 * @return You don't really know.
	 */
	virtual Window* ViolateCoreEncapsulationAndGetWindow() = 0;
#endif // !DOXYGEN_DOCUMENTATION

	/**
	 * Set the title on the Window object and inform listeners about the new title
	 * so that the UI can be updated. Implementers should pass its arguments to
	 * underlying Window.
	 *
	 * @param[in] str The (new) title.
	 *
	 * @param[in] force If the UI should be told about the title even if it's the same as we already have.
	 *
	 * @param[in] generated If the title comes from the document or e.g. tab.update (FALSE) or if generated from the url (TRUE).
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	virtual OP_STATUS SetWindowTitle(const OpString& str, BOOL force, BOOL generated = FALSE) = 0;

	/** Layout modes available for documents. */
	enum LayoutMode
	{
		NORMAL,		///< Normal desktop rendering.
		SSR,		///< Small screen rendering.
		CSSR,		///< Color SSR
		AMSR,
		MSR,		///< Medium screen rendering.
		ERA			///< Automated selection between the above
#ifdef TV_RENDERING
		,TVR            ///< TV rendering
#endif
	};

	/** Retrieve the current layout mode for the associated document window. */
	virtual LayoutMode GetLayoutMode() = 0;
	/** Change the current layout mode for the associated document window. */
	virtual void SetLayoutMode(LayoutMode) = 0;

	/** @short Enable or disable smart frames.
	 *
	 * In smart frames mode, no HTML frames are allowed to create scrollbars;
	 * instead each frame is expanded until no scrollbars are needed. The
	 * top-level frameset will display scrollbars if necessary instead. This
	 * mode is automatically enabled in certain layout modes (SetLayoutMode()).
	 *
	 * @param enable TRUE to enable, FALSE to disable
	 */
	virtual void SetSmartFramesMode(BOOL enable) = 0;

	/** @short Is smart frames mode enabled?
	 *
	 * @see SetSmartFramesMode().
	 *
	 * @return TRUE if smart frames is enabled, FALSE otherwise. */
	virtual BOOL GetSmartFramesMode() = 0;

#ifndef DOXYGEN_DOCUMENTATION
	COREAPI_DEPRECATED(BOOL GetHandheld());
	COREAPI_DEPRECATED(void SetHandheld(BOOL handheld));
#endif // !DOXYGEN_DOCUMENTATION

	virtual BOOL HasCurrentElement() = 0;

	// === State information dpt ===

	virtual BOOL HasNext() = 0;
	virtual BOOL HasPrevious() = 0;
	virtual BOOL IsLoading() = 0;

	/** Get the security mode */
	virtual OpDocumentListener::SecurityMode GetSecurityMode() = 0;

	/** @short Get privacy mode for this OpWindowCommander.
	 *
	 * If privacy mode is enabled then Opera won't store URLs loaded in this
	 * OpWindowCommander. That includes global history, visited pages and
	 * search history.
	 */
	virtual BOOL GetPrivacyMode() = 0;

	/** @short Set privacy mode for this OpWindowCommander.
	 *
	 * See GetPrivacyMode() for explanation of what privacy mode is.
	 */
	virtual void SetPrivacyMode(BOOL privacy_mode) = 0;

	virtual DocumentURLType GetURLType() = 0;

	virtual UINT32 SecurityLowStatusReason() = 0;

#ifdef SECURITY_INFORMATION_PARSER
	virtual OP_STATUS CreateSecurityInformationParser(OpSecurityInformationParser** parser) = 0;
	virtual void GetSecurityInformation(OpString& string, BOOL isEV) = 0;
#endif // SECURITY_INFORMATION_PARSER
#ifdef TRUST_INFO_RESOLVER
	virtual OP_STATUS CreateTrustInformationResolver(OpTrustInformationResolver** resolver, OpTrustInfoResolverListener * listener) = 0;
#endif // TRUST_INFO_RESOLVER

	virtual const uni_char* ServerUniName() const = 0;

	/** URLTypes available for Document */
	enum WIC_URLType {
		WIC_URLType_Unknown,
		WIC_URLType_FTP,
		WIC_URLType_HTTP,
		WIC_URLType_HTTPS
	};
	virtual WIC_URLType GetWicURLType() = 0;

#ifdef TRUST_RATING

	/** Get information about the http response, if the result is indetermined MAYBE will be returned */
	virtual BOOL3 HttpResponseIs200() = 0;

	/** Get the trust mode */
	virtual OpDocumentListener::TrustMode GetTrustMode() = 0;

	/**
	 * Initate a trust check.
	 *
	 * @param force Forces the check to go ahead even if the fraud-protection feature is off
	 * @param offline The check should not do an online check.
	 * @return OK if success, ERR otherwise
	 */
	virtual OP_STATUS CheckDocumentTrust(BOOL force, BOOL offline) = 0;
#endif // TRUST_RATING

#ifdef WEB_TURBO_MODE
	/** @short Get turbo mode for this OpWindowCommander.
	*
	* When turbo mode is enabled Opera will try to load all URLs in this
	* OpWindowCommander via one of two traffic optimizing services. Either
	* Opera Mini Transcoders or a Web Optimizing Proxy (WOP).
	*/
	virtual BOOL GetTurboMode() = 0;

	/** @short Set turbo mode for this OpWindowCommander.
	*
	* @see GetTurboMode()
	*/
	virtual void SetTurboMode(BOOL turbo_mode) = 0;
#endif // WEB_TURBO_MODE

	/** @short Get the URL Context ID if relevant for the current window associated with the windowcommander
	 */
	virtual URL_CONTEXT_ID GetURLContextID() = 0;

	/**
	   @param showPassword indicates whether the password should be shown or hidden.
	   @return the current url.
	*/
	virtual const uni_char* GetCurrentURL(BOOL showPassword) = 0;

	/**
	   Gets the current loading url. Difference between GetCurrentURL
	   and GetLoadingURL is that this method returns requested url
	   even when the server is not responding.
	   @param showPassword indicates whether the password should be shown or hidden.
	   @return the current loading url.
	*/
	virtual const uni_char* GetLoadingURL(BOOL showPassword) = 0;

	/**
	   Gets the current title for the page loaded in the window
	   @return the current title
	*/
	virtual const uni_char* GetCurrentTitle() = 0;

	/**
	   Gets the current MIME type for the document loaded in the window
	   @param mime_type (out) Set to current MIME type
	 * @param original (in) Indicate whether the original mime type
	 *                      without any guessing or parsing by Opera.
	   @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	*/
	virtual OP_STATUS GetCurrentMIMEType(OpString8& mime_type, BOOL original = FALSE) = 0;

	/**
	 * Gets the current MIME type for the document loaded in the window exactly
	 * as returned by server, including all parameters.
	 * @param mime_type (out) Set to current MIME type
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	*/
	virtual OP_STATUS GetServerMIMEType(OpString8& mime_type) = 0;

	/**
	   Gets the HTTP response code for the top document shown in the window.
	   @return HTTP response code, or 0 if none is known.
	 */
	virtual unsigned int GetCurrentHttpResponseCode() = 0;

	/**
	   Gets history information
	   @param index 1..n, where n is the first index where the method returns FALSE
	   @param result pointer to a result object to be filled in
	   @return TRUE on success, FALSE otherwise
	*/
	virtual BOOL GetHistoryInformation(int index, HistoryInformation* result) = 0;

#ifdef HISTORY_SUPPORT
	/** @short Disable global history for this window.
	 *
	 * This makes sure that URLs visited in this window are not added to the
	 * global history (opera:history). This may be useful for a window that is
	 * part of the user interface, and not a regular web page.
	 */
	virtual void DisableGlobalHistory() = 0;
#endif // HISTORY_SUPPORT

	/** Utility function to get back the OpWindow that the ui put in originally.
		Remember that it is still owned by the ui.
	    @return the associated OpWindow
	*/
	virtual OpWindow* GetOpWindow() = 0;

	/**
	   Gets the ID of the window controlled by this WindowCommander.
	   @return ID of the window.
	*/
	virtual unsigned long GetWindowId() = 0;

#ifdef HAS_ATVEF_SUPPORT
	/** Executes a script in the main document's context. */
	virtual OP_STATUS ExecuteES(const uni_char* script) = 0;
#endif // HAS_ATVEF_SUPPORT

	/**
	  Set the true zoom of the window.
	  @param enable TRUE for enabling true zoom for the window, FALSE for disabling
	*/
	virtual void SetTrueZoom(BOOL enable) = 0;

	/**
	   Gets the true zoom status for the window.
	   @return the status of true zoom
	   @see SetTrueZoom
	*/
	virtual BOOL GetTrueZoom() = 0;

	/**
	  Set the scale of the window.
	  @param scale the scale to set, in percentage (100 is normal, 0 very small, more is bigger
	*/
	virtual void SetScale(UINT32 scale) = 0;

	/**
	   Gets the current scale for the window.
	   @return the scale, in percentage.
	   @see SetScale
	*/
	virtual UINT32 GetScale() = 0;

	/**
	   Sets the current scale for text in the window.
	   @param scale the scale to set, in percentage (100 is normal, 0 very small, more is bigger).
	*/
    virtual void SetTextScale(UINT32 scale) = 0;

    /**
       Gets the current scale for text in the window.
       @return the scale, in percentage.
       @see SetTextScale
     */
    virtual UINT32 GetTextScale() = 0;

	virtual void SetImageMode(OpDocumentListener::ImageDisplayMode mode) = 0;
	virtual OpDocumentListener::ImageDisplayMode GetImageMode() = 0;
	virtual void SetCssMode(OpDocumentListener::CssDisplayMode mode) = 0;
	virtual OpDocumentListener::CssDisplayMode GetCssMode() = 0;

	typedef struct
	{
		unsigned char alpha;
		unsigned char red;
		unsigned char green;
		unsigned char blue;
	} OpColor;

	/** Gets the current background color */
	virtual OP_STATUS GetBackgroundColor(OpColor& background_color) = 0;

	/** Sets the default background color, used when no background color is known */
	virtual void SetDefaultBackgroundColor(const OpColor& background_color) = 0;

	/** Shows or hides the scrollbars. */
	virtual void SetShowScrollbars(BOOL show) = 0;

	/** Sets the encoding to use for the current window. Default is automatic. */
	virtual void ForceEncoding(Encoding encoding) = 0;

	/** Gets the encoding to use for the current window or else the current document. Default is automatic. */
	virtual Encoding GetEncoding() = 0;

#ifdef FONTSWITCHING
	/**
	 * Gets the preferred script to use when displaying the document in
	 * the current window. If Opera is not able to determine a suitable
	 * script from the available information (content, encoding, etc),
	 * WritingSystem::Unknown is returned.
	 *
	 * @param focused_frame If TRUE, address the document in the current
	 * frame. If FALSE, address the top level document.
	 *
	 * @see WritingSystem::Script
	 */
	virtual WritingSystem::Script GetPreferredScript(BOOL focused_frame) = 0;
#endif

#ifndef HAS_NO_SEARCHTEXT
	/** @short Result of text search. See Search(). */
	enum SearchResult
	{
		/** @short No match with the specified search criteria. */
		SEARCH_NOT_FOUND,

		/** @short Match found and highlighted. */
		SEARCH_FOUND,

		/** @short Wrapped to the beginning/end of the document, match found and highlighted. */
		SEARCH_FOUND_WRAPPED,

		/** @short Reached the end of the document.
		 *
		 * Any previous highlighting is unaffected. Used when the end of the
		 * document is reached and SearchInfo::allow_wrapping is FALSE.
		 *
		 * This value is currently not supported.
		 */
		SEARCH_END
	};

	/** @short Text search direction. */
	enum SearchDirection
	{
		DIR_FORWARD, ///< Search from top (or current search position) to bottom
		DIR_BACKWARD ///< Search from bottom (or current search position) to top
	};

	/** @short Search parameters. */
	class SearchInfo
	{
	public:
		/** @short Which direction to search in the document. */
		SearchDirection direction;

		/** @short If TRUE, perform case-sensitive match. */
		BOOL case_sensitive;

		/** @short If TRUE, match whole words only. */
		BOOL whole_words_only;

		/** @short If TRUE, match links (anchor elements) only. */
		BOOL links_only;

		/** @short If TRUE, allow wrapping when the end/beginning of the document is reached. */
		BOOL allow_wrapping;

		/** @short If TRUE, hightlight all matches in the document.
		 *
		 * Only supported if FEATURE_SEARCH_MATCHES_ALL is enabled.
		 *
		 * Will use a different highlight color for the current match.
		 */
		BOOL highlight_all;

		SearchInfo()
			: direction(DIR_FORWARD),
			  case_sensitive(FALSE),
			  whole_words_only(FALSE),
			  links_only(FALSE),
			  allow_wrapping(FALSE),
			  highlight_all(FALSE) { }

		SearchInfo(SearchDirection direction, BOOL case_sensitive, BOOL whole_words_only, BOOL links_only, BOOL allow_wrapping, BOOL highlight_all)
			: direction(direction),
			  case_sensitive(case_sensitive),
			  whole_words_only(whole_words_only),
			  links_only(links_only),
			  allow_wrapping(allow_wrapping),
			  highlight_all(highlight_all) { }
	};

	/** @short Search the document for the specified text.
	 *
	 * @param search_string The string to search for. If two subsequent calls
	 * to this method specify a different string here, searching will start at
	 * the same position as the previous match. This is useful for incremental
	 * inline search, for instance. If the strings in the two calls are
	 * identical, search position will always advance.
	 * @param search_info Search parameters
	 * @return Result of search.
	 */
	virtual SearchResult Search(const uni_char* search_string, const SearchInfo& search_info) = 0;

	/** @short Reset search position. */
	virtual void ResetSearchPosition() = 0;

	/**
	 * This method returns the rectangles for all visible search hits
	 * in all documents in the window. This method returns only valid
	 * data if the document was painted.
	 *
	 * @param all_rects receives on output the vector of the
	 *  rectangles for all matches.
	 * @param active_rects receives on output the vector of rectangles
	 *  for the active match only (if there is one).
	 * @return TRUE if successful. FALSE if there is no search.
	 */
	virtual BOOL GetSearchMatchRectangles(OpVector<OpRect> &all_rects, OpVector<OpRect> &active_rects) = 0;
#endif // !HAS_NO_SEARCHTEXT

#ifndef HAS_NOTEXTSELECTION
#ifdef USE_OP_CLIPBOARD
   	/** Pastes the current text from the clipboard into the targeted element. */
   	virtual void Paste() = 0;

	/** Cuts the current text selection to the clipboard. */
	virtual void Cut() = 0;

	/** Copies the current text selection to the clipboard. */
	virtual void Copy() = 0;
#endif // USE_OP_CLIPBOARD

	/** @short Select text starting position.
	 *
	 * This will set the start position of the text selection. It will not
	 * necessarily be the point from which selection will be made, but
	 * it indicates that selection should start and where the user initally
	 * started it.
	 *
	 * SelectTextMoveAnchor() or SelectTextEnd() should later be called
	 * for any intermediate points during the selection and also the end point
	 * before any operations with OpWindowCommander methods like
	 * GetSelectedText() and Copy() can be used.
	 *
	 * The intermediate points are necessary to locate the correct element to
	 * select based on the movements of the selection point.
	 *
	 * @note In contrast to SelectTextMoveAnchor() this function will
	 *       cancel any existing text selection and start a new selection at
	 *       the given coordinates.
	 *
	 * @param p The first point, in screen coordinates.
	 */
	virtual void SelectTextStart(const OpPoint& p) = 0;

	/** @short Sets text selection starting position.
	 *
	 * This will set the start position of the text selection. If there
	 * is an active text selection it will move the starting point without
	 * cancelling the previous selection. If there is no active text selection
	 * calling this function will be semantically identical to calling
	 * SelectTextStart().
	 *
	 * SelectTextMoveAnchor() or SelectTextEnd() should later be called
	 * for any intermediate points during the selection and also the end point
	 * before any operations with OpWindowCommander methods like
	 * GetSelectedText() and Copy() can be used.
	 *
	 * In order for this function to have effect when updating an existing
	 * selection the coordinates should reside in the same frame/iframe as
	 * where the text selection was started. This function will accept
	 * coordinates from other frames/iframes but will not use them to update
	 * the selection. To start a new selection using this function you must
	 * first clear any existing selections.
	 *
	 * The intermediate points are necessary to locate the correct element to
	 * select based on the movements of the selection point.
	 *
	 * @note This function behaves like SelectTextStart() with the exception
	 *       that it moves the start position of an existing text selection
	 *       instead of cancelling it.
	 *
	 * @param p The first point, in screen coordinates.
	 */
	virtual void SelectTextMoveAnchor(const OpPoint& p) = 0;

	/** @short Select text current end point.
	 *
	 * This will set the current end point of the text selection. An end
	 * point can be either an intermediate point made during the selection
	 * or the final point entered by the user.
	 *
	 * In order for this function to have effect the coordinates should reside
	 * in the same frame/iframe as where the text selection was started. This
	 * function will accept coordinates from other frames/iframes but will not
	 * use them to update the selection.
	 *
	 * For the purpose of user feedback, the method can, and should, be called
	 * many times with points describing the current selection end point during the
	 * selection phase in a way so that a curve described by the points entered
	 * to SelectTextEnd will more or less accurately describe how the user
	 * moved about the selection end point in the view.
	 *
	 * Operations on the selected text may be done with OpWindowCommander
	 * methods like GetSelectedText(), Copy(), and so on.
	 *
	 * @param p The current end point, in screen coordinates.
	 */
	virtual void SelectTextEnd(const OpPoint& p) = 0;

#ifdef _PRINT_SUPPORT_
	/** Starts printing after having shown the print dialog. */
	virtual OP_STATUS Print(int from, int to, BOOL selected_only) = 0;
	/** Stops a current print job */
	virtual void CancelPrint() = 0;
	/** Prints next page. To be used when OpPrintingListener::OnPrintStatus with PAGE_PRINTED is received */
	virtual void PrintNextPage() = 0;
	/** Returns TRUE if window contains printable data */
	virtual BOOL IsPrintable() = 0;
	/** Returns TRUE if printing is currently in progress */
	virtual BOOL IsPrinting() = 0;
#endif

	/** @short Select word at point.
	 *
	 * This will select the word at the specified point. If no word exists
	 * at the point any active selection will be cleared.
	 *
	 * Operations on the selected text may be done with OpWindowCommander
	 * methods like GetSelectedText(), Copy(), and so on.
	 *
	 * @param p Point in screen coordinates.
	 */
	virtual void SelectWord(const OpPoint& p) = 0;

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
	/** @short Select link at point.
	 *
	 * This will select all selectable elements inside the link at or near
	 * the specified point. If no link is available at the specified point
	 * any existing selection will be cleared and nothing new selected.
	 *
	 * Operations on the selected text may be done with OpWindowCommander
	 * methods like GetSelectedText(), Copy(), and so on.
	 *
	 * @param p Point in screen coordinates.
	 *
	 * @return OpStatus::OK if the operation was successfull and a link was
	 *         selected. If no link was found at the specified coordinates or
	 *         if the operation failed OpStatus::ERR is returned. If OOM
	 *         OpStatus::ERR_NO_MEMORY is returned.
	 */
	virtual OP_STATUS SelectNearbyLink(const OpPoint& p) = 0;
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

	/** @short Get the text selection start position and line height.
	 *
	 * Retrieves the screen coordinates for the text selection anchor
	 * point (i.e. the point where the user started the selection). If
	 * there is no active selection the output values are undefined and the
	 * function returns OpStatus::ERR.
	 *
	 * @return OpStatus::OK if the operation was successful and there was an
	 *         active selection. If there was no active selection or some error
	 *         occurred OpStatus::ERR is returned. Other error codes might also
	 *         be returned for other kinds of errors.
	 */
	virtual OP_STATUS GetSelectTextStart(OpPoint& p,int &line_height) = 0;

	/** @short Get the text selection end position and line height.
	 *
	 * Retrieves the screen coordinates for the text selection focus
	 * point (i.e. the point where the user ended the selection). If
	 * there is no active selection the output values are undefined and the
	 * function returns OpStatus::ERR.
	 *
	 * @return OpStatus::OK if the operation was successful and there was an
	 *         active selection. If there was no active selection or some error
	 *         occurred OpStatus::ERR is returned. Other error codes might also
	 *         be returned for other kinds of errors.
	 */
	virtual OP_STATUS GetSelectTextEnd(OpPoint& p,int &line_height) = 0;

	/** Returns whether the document has selected text or not.
		@return TRUE if there is selected text.
	*/
	virtual BOOL HasSelectedText() = 0;

	/** If HasSelectedText() returns TRUE, returns the selected text
	    in the document as a string allocated using new[], owned by
	    the caller, or NULL on OOM.  If HasSelectedText() returns
	    FALSE, returns an empty string allocated using new[], owned by
	    the caller, or NULL on OOM.

	    @return A string owned by the caller or NULL on OOM. */
	virtual uni_char *GetSelectedText() = 0;

	/** Clear the current text selection. */
	virtual void ClearSelection() = 0;

# if defined KEYBOARD_SELECTION_SUPPORT || defined DOCUMENT_EDIT_SUPPORT
	enum CaretMovementDirection
	{
		CARET_RIGHT,
		CARET_UP,
		CARET_LEFT,
		CARET_DOWN,
		CARET_LINE_HOME,
		CARET_LINE_END,
		CARET_WORD_RIGHT,
		CARET_WORD_LEFT,
		CARET_PARAGRAPH_UP,
		CARET_PARAGRAPH_DOWN,
		CARET_DOCUMENT_HOME,
		CARET_DOCUMENT_END,
		CARET_PAGEUP,
		CARET_PAGEDOWN
	};
#endif // KEYBOARD_SELECTION || DOCUMENT_EDIT_SUPPORT

#ifdef KEYBOARD_SELECTION_SUPPORT
	/** Enable/disable keyboard selection mode, using a caret.
	 *
	 * @param enable enable/disable
	 */
	virtual OP_STATUS SetKeyboardSelectable(BOOL enable) = 0;

	/**
	 * Command the keyboard caret to move in a specified direction, optionally
	 * also selecting while doing so.
	 *
	 * @param direction The direction to move. See the CaretMovementDirection
	 * values for more information.
	 *
	 * @param in_selection_mode If TRUE we will select in the direction (which may
	 * grow or shrink an existing selection).
	 *
	 * If the caret can not move in the specified direction, nothing will happen.
	 */
	virtual void MoveCaret(CaretMovementDirection direction, BOOL in_selection_mode) = 0;

	/** Get the position and dimensions of the text selection caret.
	 *
	 * @param[out] rect receives on success the rectangle which
	 *  describes the current position and dimensions of the caret in
	 *  document coordinates.
	 *
	 * @retval OpStatus::OK when the data was retrieved correctly.
	 * @retval OpStatus::ERR or another error status in any other case
	 *   (i.e. not in text selection mode or not in selection mode)
	 */
	virtual OP_STATUS GetCaretRect(OpRect& rect) = 0;

	/**
	 * Set the keyboard caret to a specified position. There is no guarantee that
	 * the caret will be at the exact position requested. This will more or less
	 * match what a mousedown event at the requested position would do.
	 *
	 * @param position The point where the caret should be placed in document
	 * coordinates relative to the top most document.
	 *
	 * @returns OpStatus::ERR if if was impossible to create a selection,
	 * OpStatus::ERR_NO_MEMORY if an out-of-memory situation occured or
	 * OpStatus::OK if the command was completed successfully.
	 */
	virtual OP_STATUS SetCaretPosition(const OpPoint& position) = 0;

# endif // KEYBOARD_SELECTION_SUPPORT
#endif // !HAS_NOTEXTSELECTION

#ifdef USE_OP_CLIPBOARD
	/**
	   @param url the url of the image to copy to the clipboard.
	   @return TRUE if all went well and the image was copied to the clipboard, FALSE otherwise.
	 */
	virtual BOOL CopyImageToClipboard(const uni_char* url) = 0;

	/**
	 * Copy the link (if any) that the OpDocumentContext indicates to
	 * the clipboard.
	 *
	 * @param context Used to find which link use.
	 */
	virtual void CopyLinkToClipboard(OpDocumentContext& ctx) = 0;
#endif // USE_OP_CLIPBOARD

#ifdef SHORTCUT_ICON_SUPPORT
	/** @return whether the document has a document/fav/shortcut icon */
	virtual BOOL HasDocumentIcon() = 0;

	/**
	   Gets the shortcut icon. The ownership of the allocated icon is transferred to the caller. Don't forget to delete it!
	   @param icon where the icon is returned. only valid if the return value fulfills OpStatus::IsSuccess(status);
	   @return error code on error.
	*/
	virtual OP_STATUS GetDocumentIcon(OpBitmap** icon) = 0;

	/**
		Gets the URL for the shortcut icon. The string returned in the uni_char** should be copied immediately --
		you can't depend on it being alive after you have returned.
		@param iconUrl pointer for returning the url
		@return error code on error.
	*/
	virtual OP_STATUS GetDocumentIconURL(const uni_char** iconUrl) = 0;

#endif // SHORTCUT_ICON_SUPPORT

#ifdef LINK_SUPPORT
	virtual UINT32 GetLinkElementCount() = 0;
	virtual BOOL GetLinkElementInfo(UINT32 index, LinkElementInfo* information) = 0;
#endif // LINK_SUPPORT

	virtual BOOL GetAlternateCssInformation(UINT32 index, CssInformation* information) = 0;
	virtual BOOL SetAlternateCss(const uni_char* title) = 0;

	/** Shows the image whos id was specified in id
		@return TRUE if the id was valid, FALSE otherwise. */
	virtual BOOL ShowCurrentImage() = 0;

	virtual void Focus() = 0;

	// === Listener dpt ===

	/** Methods in the loading listener are called when things happen
        that are related to the loading of the within the window. */
	virtual void SetLoadingListener(OpLoadingListener* listener) = 0;

	/** Methods in the document listener are called when things happen
        that are related to the document within the window. */
	virtual void SetDocumentListener(OpDocumentListener* listener) = 0;

	virtual void SetMailClientListener(OpMailClientListener *listener) = 0;

#ifdef _POPUP_MENU_SUPPORT_
	/** A menu should be activated. */
	virtual void SetMenuListener(OpMenuListener* listener) = 0;
#endif // _POPUP_MENU_SUPPORT_

	/** The setting of this listener indicates that somebody is interested in the link element. */
	virtual void SetLinkListener(OpLinkListener* listener) = 0;

#ifdef ACCESS_KEYS_SUPPORT
	/** Access keys are apparently interesting. */
	virtual void SetAccessKeyListener(OpAccessKeyListener* listener) = 0;
#endif // ACCESS_KEYS_SUPPORT

#ifdef DOM_TO_PLATFORM_MESSAGES
	virtual void SetPlatformMessageListener(OpPlatformMessageListener* listener) = 0;
#endif // DOM_TO_PLATFORM_MESSAGES

#ifdef _PRINT_SUPPORT_
	/** Listener for printing activity. */
	virtual void SetPrintingListener(OpPrintingListener* listener) = 0;
	/**
	   print the window
	 */
	virtual void StartPrinting() = 0;
#endif // _PRINT_SUPPORT_

	/** Listener for errors. */
	virtual void SetErrorListener(OpErrorListener* listener) = 0;


#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	/** Set listener for certificate dialogs */
	virtual void SetSSLListener(OpSSLListener* listener) = 0;
#endif

#ifdef APPLICATION_CACHE_SUPPORT
	/** Set listener for application cache events */
	virtual void SetApplicationCacheListener(OpApplicationCacheListener* listener) = 0;
#endif // APPLICATION_CACHE_SUPPORT

#ifdef EXTENSION_SUPPORT
	/** Set listener for extension UI events */
	virtual void SetExtensionUIListener(OpExtensionUIListener* listener) = 0;
#endif // EXTENSION_SUPPORT

#ifdef _ASK_COOKIE
	/** Set listener for cookies */
	virtual void SetCookieListener(OpCookieListener* listener) = 0;
#endif // _ASK_COOKIE

#ifdef WIC_FILESELECTION_LISTENER
	/** Set listener for file choosing. */
	virtual void SetFileSelectionListener(OpFileSelectionListener* listener) = 0;
#endif // WIC_FILESELECTION_LISTENER

#ifdef WIC_COLORSELECTION_LISTENER
	/** Set listener for color choosing. */
	virtual void SetColorSelectionListener(OpColorSelectionListener* listener) = 0;
#endif // WIC_COLORSELECTION_LISTENER

#ifdef NEARBY_ELEMENT_DETECTION
	/** Set listener for fingertouch events. */
	virtual void SetFingertouchListener(OpFingertouchListener* listener) = 0;
#endif

#ifdef WIC_PERMISSION_LISTENER
	/** Set listener for permission asking. */
	virtual void SetPermissionListener(OpPermissionListener* listener) = 0;
#endif // WIC_PERMISSION_LISTENER

#ifdef WIC_EXTERNAL_APPLICATIONS_ENUMERATION
	/** Set listener for application. */
	virtual void SetApplicationListener(OpApplicationListener* listener) = 0;
#endif // WIC_EXTERNAL_APPLICATIONS_ENUMERATION

#ifdef WIC_OS_INTERACTION
	/** Set listener for operating system interaction. */
	virtual void SetOSInteractionListener(OpOSInteractionListener* listener) = 0;
#endif // WIC_OS_INTERACTION

#ifdef SCOPE_URL_PLAYER
	/** Set listener for url-player callback events. */
	virtual void SetUrlPlayerListener(WindowCommanderUrlPlayerListener* listener) = 0;
#endif //SCOPE_URL_PLAYER

#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
	/** Set listener for selftest finished events. */
	virtual void SetSelftestListener(OpSelftestListener* listener) = 0;
#endif // GOGI_SELFTEST_FINISHED_NOTIFICATION

#ifdef DOM_LOAD_TV_APP
	/** Set listener for requests to load a tv application */
	virtual void SetTVAppMessageListener(OpTVAppMessageListener* listener) = 0;
#endif //DOM_LOAD_TV_APP

	/** @short Create an OpDocumentContext for the current active position.
	 *
	 * Creates an OpDocumentContext for the current active position (caret,
	 * element, ...)  in the document. This can be used for inspecting the
	 * current word or element.
	 *
	 * The context is owned by the caller and must be deleted by the caller. It
	 * may stop being useful at any time depending on other actions or events
	 * in the document and should not be kept around for long. Too many active
	 * OpDocumentContext objects at the same time might degrade browser
	 * performance.
	 *
	 * If spell checking is enabled this may be used indirectly to create a
	 * spelling session.
	 *
	 * @param[out] ctx The created OpDocumentContext if the method returns
	 * OpStatus::OK.
	 *
	 * @return OK on success or an error code if failing. The error code
	 * ERR_NOT_SUPPORTED will be used if it is not possible to create a context
	 * because of the current document state (or lack of state).
	 */
	virtual OP_STATUS CreateDocumentContext(OpDocumentContext** ctx) = 0;

	/** @short Create an OpDocumentContext for the element at the specified screen position.
	 *
	 * Creates an OpDocumentContext for the specified screen position.
	 * This can be used for inspecting an element at a specific screen position.
	 *
	 * The context is owned by the caller and must be deleted by the caller. It
	 * may stop being useful at any time depending on other actions or events
	 * in the document and should not be kept around for long. Too many active
	 * OpDocumentContext objects at the same time might degrade browser
	 * performance.
	 *
	 * If spell checking is enabled this may be used indirectly to create a
	 * spelling session.
	 *
	 * @param[out] ctx The created OpDocumentContext if the method returns
	 * OpStatus::OK.
	 *
	 * @param[in] screen_pos The position in screen coordinates to inspect.
	 * OpStatus::OK.
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, ERR if it is not possible
	 * to create a context because of the current document state (or lack of state).
	 *
	 */
	virtual OP_STATUS CreateDocumentContextForScreenPos(OpDocumentContext** ctx, const OpPoint& screen_pos) = 0;

#ifdef _POPUP_MENU_SUPPORT_
	/** Request a popup menu.
	 *
	 * UI will receive (possibly asynchronous) notification about this via
	 * OpMenuListener::OnPopupMenu().
	 * @param point Point at which the document context should be created
	 *        as if it has been triggered by a mouse click.
	 *        If NULL the menu is triggered in context of currently focused element.
	 */
	virtual void RequestPopupMenu(const OpPoint* point) = 0;

	/** Notify Core that document menu item has been clicked, or selected in any other way.
	 *
	 * @param context The document context describing a point in document at which the menu
	 *        was triggered. It can be the same document context as received in
	 *        OpMenuListenr::OnPopupMenu() event or its copy.
	 * @param id Id of the menu item that was clicked.
	 */
	virtual void OnMenuItemClicked(OpDocumentContext* context, UINT32 id) = 0;

	/** Gets the icon for the menu item.
	 *
	 *  The caller should not assume anything about lifetime of the bitmap, it should
	 *  use the bitmap right away or copy it.
	 *  @note this is temporary solution. When all the multi-process infrastructure is in place
	 *  the cross-process icon image identifier will be included in the OpPopupMenuRequestMessage.
	 */
	virtual OP_STATUS GetMenuItemIcon(UINT32 icon_id, OpBitmap*& icon_bitmap) = 0;
#endif // _POPUP_MENU_SUPPORT_

	/** @short Load the image specified by the context.
	 *
	 * Loads an image identified by the context or does nothing if it's not an
	 * image or it couldn't be loaded for some reason, security or other.
	 *
	 * This method is useful when in "show only cached images" mode, and the
	 * user wants to load a certain image from the network.
	 *
	 * @param[in] ctx The document context. As received through OnPopupMenu or
	 * created with CreateDocumentContext().
	 * @param[in] disable_turbo Flag used to determine if Opera Turbo should be
	 * disabled before loading the image. Only used if the image was loaded using
	 * Opera Turbo.
	 */
	virtual void LoadImage(OpDocumentContext& ctx, BOOL disable_turbo=FALSE) = 0;

	/** @short Save the image specified by the context.
	 *
	 * Saves an image identified by the context or does nothing if it's not an
	 * image.
	 *
	 * @param[in] ctx The document context. As received through OnPopupMenu or
	 *   created with CreateDocumentContext().
	 * @param[in] filename Name of file to save the image to.
	 *
	 * @return OpStatus::OK if image was saved or an error code if it failed.
	 */
	virtual OP_STATUS SaveImage(OpDocumentContext& ctx, const uni_char* filename) = 0;

 #ifdef USE_OP_CLIPBOARD
	/**
	 * @short Copy the image specified by the context to clipboard
	 *
	 * @param[in] ctx The document context. As received through OnPopupMenu or
	 * created with CreateDocumentContext().
	 *
	 * @return TRUE if all went well and the image was copied to the
	 * clipboard, FALSE otherwise.
	 */
	virtual BOOL CopyImageToClipboard(OpDocumentContext& ctx) = 0;

	/**
	 * @short Copy the background image to clipboard
	 *
	 * @param[in] ctx The document context. As received through OnPopupMenu or
	 * created with CreateDocumentContext().
	 *
	 * @return TRUE if all went well and the image was copied to the
	 * clipboard, FALSE otherwise.
	 */
	virtual BOOL CopyBGImageToClipboard(OpDocumentContext& ctx) = 0;
#endif // USE_OP_CLIPBOARD

	/** @short Open the image specified by the context as separate link
	 *
	 * Loads an image, identified by the context, as main document in the
	 * current tab. Or does nothing if it's not an image or it couldn't be
	 * loaded for some reason, security or other.
	 *
	 * @param[in] ctx The document context. As received through OnPopupMenu or
	 * created with CreateDocumentContext().
	 * @param[in] new_tab Indicates whether the image should be opened in a new
	 * tab or in the current one.
	 * @param[in] in_background Indicates whether the potentially new tab should
	 * be opened in the background. Not used if new_tab != TRUE.
	 */
	virtual void OpenImage(OpDocumentContext& ctx, BOOL new_tab=FALSE, BOOL in_background=FALSE) = 0;

# ifdef WIC_TEXTFORM_CLIPBOARD_CONTEXT
	/** @short Copy text specified by the context to the clipboard.
	 *
	 * Copies the value of the text identified by the context or does
	 * nothing if the input type is set to password. If the context
	 * has nothing to copy a copy action is performed on the document.
	 */
	virtual void CopyTextToClipboard(OpDocumentContext& ctx) = 0;

	/** @short Cut text specified by the context to the clipboard.
	 *
	 * Cuts the text from the form identified by the context or does
	 * nothing if it's not a text form, the input type is set to
	 * password or the element is read-only.
	 */
	virtual void CutTextToClipboard(OpDocumentContext& ctx) = 0;

	/** @short Paste content of clipboard to text form specified by the context.
	 *
	 * Pastes the content of the clipboard to the text form element
	 * identified by the context or does nothing if it's not a text
	 * form or the element is read-only.
	 */
	virtual void PasteTextFromClipboard(OpDocumentContext& ctx) = 0;
# endif // WIC_TEXTFORM_CLIPBOARD_CONTEXT

	/** @short Clear text form of any content.
	 *
	 * Clears the content of the text form element identified by the
	 * context of any content, or does nothing if it's not a text
	 * form.
	 */
	virtual void ClearText(OpDocumentContext& ctx) = 0;

#ifdef ACCESS_KEYS_SUPPORT
	/** Enables/disables accesskeys
	@param enable if TRUE accesskey support will be enabled, disabled otherwise
	*/
	virtual void SetAccessKeyMode(BOOL enable) = 0;

	/** Gets the current access key mode
	@returns TRUE if accesskeys are enabled
	*/
	virtual BOOL GetAccessKeyMode() = 0;
#endif // ACCESS_KEYS_SUPPORT

#ifdef CONTENT_MAGIC
	/** Enables/disables content magic indicator on the specified position. x parameter is ignored right now.
		@param enable enable/disable
		@param x x position to enable at. -1 to let core decide.
		@param y y position to enable at. -1 to let core decide
	*/
	virtual void EnableContentMagicIndicator(BOOL enable, INT32 x, INT32 y) = 0;
#endif

	/**
	 * @param[out] url url string associated with a selected link.
	 * @param[out] title title string associated with a selected link. */
	virtual OP_STATUS GetSelectedLinkInfo(OpString& url, OpString& title) = 0;

	/** Returns TRUE if scripting has been disabled in this window through a call to SetScriptingDisabled.
	    Note: TRUE doesn't mean scripting is necessarily enabled at all in the window or that scripting
	    hasn't been disabled in preferences, it simply means that SetScriptingDisabled hasn't been used
	    to disable it. */
	virtual BOOL GetScriptingDisabled() = 0;

	/** Disable (or enable again) scripting in all documents displayed in this window.  Note that
	    SetScriptingDisabled(FALSE) doesn't by itself mean scripting will be active in the window.  A
	    reload is usually required for scripts in documents to come to life again, and scripting might
	    still be disabled in preferences, or this might be a type of window in which scripting is
	    permanently disabled. */
	virtual void SetScriptingDisabled(BOOL disabled) = 0;

	/** Returns TRUE if scripting has been paused in this window through a call to SetScriptingPaused. */
	virtual BOOL GetScriptingPaused() = 0;

	/** Pause (or unpause) scripting in all documents displayed in this window.  Many fundamental
	    mechanisms in the window (such as loading new documents in it) are likely paused as well, so
	    typically this should only be used either very temporarily or while the window is not
	    available for use anyway (hidden, blocked by a modal dialog, et. c.). */
	virtual void SetScriptingPaused(BOOL paused) = 0;

	/** Any timeout registered by a script in the window using setTimeout() or setInterval() is
	    canceled, except those that have already timed out and started executing.  The timeouts are
	    deleted completely; there is no way to reactivate them later.  To only temporarily "freeze
	    time", use SetScriptingPaused() instead. */
	virtual void CancelAllScriptingTimeouts() = 0;

	/**
	 * Sets a flag informing the browser core that this window is
	 * visible, or not visible to the user. When a window is set to
	 * not visble certain then unneeded background processing will be
	 * disabled and other processing might prioritize windows that are
	 * visible, thus giving the user a perceived performance increase.
	 *
	 * For the moment this doesn't prevent plugins in background
	 * windows from using high amounts of CPU, and scripts will
	 * continue running.
	 *
	 * If this is set to FALSE when the window is actually showing,
	 * animations might not run (neither gifs, nor SVGs).
	 *
	 * Typically this is set to false whenever the window gets covered
	 * by any other window and then set to true when the operating
	 * system sends a paint event.
	 *
	 * For instance if the window is in a background tab it should be
	 * set to FALSE.
	 *
	 * @param is_visible Set to TRUE if the user can see the Window,
	 * FALSE otherwise.
	 */
	virtual void SetVisibleOnScreen(BOOL is_visible) = 0;

	/**
	* Enables/disables drawing of spatial navigation highlight rects.
	* @param enable set to TRUE to enable drawing, FALSE otherwise.
	*/
	virtual void SetDrawHighlightRects(BOOL enable) = 0;

	/** @short Get the viewport controller associated with this window.
	 *
	 * @return The viewport controller. Will never be NULL. Owned by the window
	 * commander.
	 */
	virtual OpViewportController* GetViewportController() const = 0;

#ifdef NEARBY_ELEMENT_DETECTION
	/** @short Hide elements expanded for fingertouch, if any.
	 *
	 * This call will lead to a call to
	 * OpDocumentListener::OnFingertouchElementsHidden(), at least if elements
	 * were currently expanded.
	 */
	virtual void HideFingertouchElements() = 0;
#endif // NEARBY_ELEMENT_DETECTION

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
	/** Finds the interactive items that are near to the given rectangle in current document tree.
	 *	See FEATURE_NEARBY_INTERACTIVE_ITEM_DETECTION.
	 * @param rect the rectangle in the unscaled coordinates (relative to the top document root
	 *		  or top left of the top-leftmost frame)
	 * @param[out] list the list of InteractiveItemInfo objects.
	 *					The rectangles of the InteractiveItemInfos will be either
	 *					in doc root coordinates or in local coordinates of a transform context
	 *					with a transformation matrix to the doc root attached.
	 *					Each InteractiveItemInfo is guaranted to have at least zero length
	 *					url and title strings.
	 *					The caller is responsible for the list cleanup regardless of the call result.
	 * @param bbox_rectangles if TRUE all the rectangles will be converted to the untransformed ones, relative
	 *		  to the top document's top left. The transformed rectangles will therefore become bboxes, which means
	 *		  they might be bigger then the exact parallelogram they are.
	 * @returns OpStatus::OK
	 *			OpStatus::ERR_NULL_POINTER if there is no top document
	 *			OpStatus::ERR_NO_MEMORY in case of OOM. */
	virtual OP_STATUS FindNearbyInteractiveItems(const OpRect& rect, List<InteractiveItemInfo>& list, BOOL bbox_rectangles = FALSE) = 0;
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

#ifdef DOM_JSPLUGIN_CUSTOM_EVENT_HOOK
	/** @short Send a custom DOM event targeted to the window object.
	 *
	 * If the window displays a frameset, the DOM window object in question is
	 * the top-level document's.
	 *
	 * @param event_name Name of the event to be send.
	 * @param event_param Event specific data that will be stored inside the
	 * event object.
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, ERR on other error
	 */
	virtual OP_STATUS SendCustomWindowEvent(const uni_char* event_name, int event_param) = 0;

# ifdef GADGET_SUPPORT
	/** @short Send a custom DOM event targeted to widget object.
	 *
	 * If the widget displays a frameset, the DOM window object in question is
	 * the top-level document's.
	 *
	 * @param event_name Name of the event to be send.
	 * @param event_param Event specific data that will be stored inside the
	 * event object.
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, ERR on other error
	 */
	virtual OP_STATUS SendCustomGadgetEvent(const uni_char* event_name, int event_param) = 0;
# endif // GADGET_SUPPORT
#endif // DOM_JSPLUGIN_CUSTOM_EVENT_HOOK

	/** tell window that screen has changed, could be that window is moved
	 * from a screen to another, OR that the properties of the current
	 * screen has changed
	 */
	virtual void ScreenPropertiesHaveChanged() = 0;
#ifdef GADGET_SUPPORT
	/** @short Get gadget associated with this window.
	 *
	 * @return The OpGadget. Could be NULL. Owned by the window
	 * commander.
	 */
	virtual OpGadget* GetGadget() const = 0;
#endif // GADGET_SUPPORT

	/**
	 Fetches size information for current document. The argument
	 include_inlines specifies whether or not to include the size
	 of all inline elements

	 @param size receives on output the size of the current document in bytes.
     @param include_inlines decides if inlines should be included with size
	 */
	virtual OP_STATUS GetCurrentContentSize(OpFileLength& size, BOOL include_inlines) = 0;

#ifdef WEBFEEDS_DISPLAY_SUPPORT
	/**
	 * Returns true if a webfeed is opened in the window.
	 */
	virtual BOOL IsInlineFeed() = 0;
#endif // WEBFEEDS_DISPLAY_SUPPORT

#ifdef PLUGIN_AUTO_INSTALL
	/** @short Called by the platform to signal that a plugin just became available for installation.
	 *
	 * Called by the platform to notify the EmbedContent for which a plugin is missing, that the plugin just
	 * became available for installation. This does't mean that the plugin has been installed yet. This is to
	 * change the plugin placeholder accordingly, after getting information about plugin name.
	 *
	 @param mime_type The mimetype for which a plugin just became available.
	 */
	virtual OP_STATUS OnPluginAvailable(const uni_char* mime_type) = 0;
#endif // PLUGIN_AUTO_INSTALL

	/**
	  * Returns TRUE if the current url resolves to an address in the
	  * intranet. Returns FALSE otherwise, or if the current url is
	  * empty or invalid.
	  * @see CurrentURL()
	  */
	virtual BOOL IsIntranet() = 0;

	/**
	 * Returns true if a form element in the document has focus.
	 */
	virtual BOOL HasDocumentFocusedFormElement() = 0;

	/**
	 * Create a search URL string.
	 *
	 * If the provided document context isn't referencing a text input form
	 * element (<input type=text>, <textarea> or similar), OpStatus::ERR is
	 * returned and all the other arguments are left unchanged.
	 *
	 * Otherwise, a URL is constructed as if the form containing the selected
	 * text input was to be submitted, with the value of the selected text input
	 * replaced by the string "%s".
	 *
	 * If the form would be submitted using the POST method, as indicated by the
	 * value stored in the 'is_post' argument, the string 'query' is set to
	 * contain the POST data, and the 'url' string to contain the URL to which
	 * the data is posted.  If the form would be submitted using the GET method,
	 * the string 'url' is set to contain the full URL and the string 'query' is
	 * set to contain the query part (which in that case is redundant and can be
	 * ignored.)
	 *
	 * The string 'charset' is set to contain the charset using which the
	 * submitted form values were encoded.  This charset should also be used to
	 * encode the search term when constructing a final URL to load when
	 * performing a search.
	 *
	 * @param context Used to find form field to create search for
	 * @param url Set to search URL on success
	 * @param query Set to URL query part on success
	 * @param is_post Set to TRUE on success if form is submitted using POST method
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR if no search URL could be
	 *         constructed from the document context, and OpStatus::ERR_NO_MEMORY
	 *         on OOM.
	 */
	virtual OP_STATUS CreateSearchURL(OpDocumentContext& context, OpString8* url, OpString8* query, OpString8* charset, BOOL* is_post) = 0;

	/** Returns whether document supports given view_mode. The document
	 *  is considered to support given mode if it explicitly adjusts it's content
	 *  to this mode using view-mode media queries.
	 */
	virtual BOOL IsDocumentSupportingViewMode(WindowViewMode view_mode) const = 0;

	/** @short Disable/Enable plugins
	 *
	 * This will override any pref; use if you want to block plugins from
	 * running in this document.
	 *
	 * @param[in] disabled Pass TRUE to disable, FALSE to enable
	 */
	virtual void ForcePluginsDisabled(BOOL disabled) = 0;

	/** @short Get state of plugin override */
	virtual BOOL GetForcePluginsDisabled() const = 0;

	/**
	 * Flags for GetDocumentFullscreenState and
	 * SetDocumentFullscreenState methods.
	 */
	enum FullScreenState {
		/** Fullscreen disabled */
		FULLSCREEN_NONE,
		/** Regular fullscreen, browser chrome invisible. */
		FULLSCREEN_NORMAL,
		/** Regular fullscreen + projection/presentation mode, browser chrome invisible. */
		FULLSCREEN_PROJECTION
	};
	/**
	 * Retrieves if the top document is being rendered as a fullscreen document.
	 */
	virtual FullScreenState GetDocumentFullscreenState() const = 0;

	/**
	 * Sets the document fullscreen state, so the document is rendered
	 * appropriately.
	 * @param value  TRUE enabled fullscreen, FALSE disables it.
	 * @return
	 *  - OK on success,
	 *  - ERR_NOT_SUPPORTED if the state was not allowed to be set,
	 *  - ERR_NO_MEMORY.
	 */
	virtual OP_STATUS SetDocumentFullscreenState(FullScreenState value) = 0;

#ifdef WIC_SET_PERMISSION
	/** Set permission for privacy-sensitive functionality.
	 *
	 * The setting will affect all documents from a given hostname within
	 * the window associated with this window commander.
	 *
	 * The persisted permissions affect whether
	 * OpPermissionListener::OnAskForPermission is called, i.e. if
	 * PERMISSION_TYPE_GEOLOCATION_REQUEST is set to allowed for
	 * PERSISTENCE_TYPE_RUNTIME then OpPermissionListener will not be called to
	 * ask for permssion to geolocation until the document is reloaded.
	 *
	 * Any previously specified settings are cleared.
	 *
	 * Not all permission types are supported, ERR_OUT_OF_RANGE will be returned
	 * in this case. Currently PERMISSION_TYPE_CAMERA_REQUEST and
	 * PERMISSION_TYPE_GEOLOCATION_REQUEST are supported.
	 *
	 * @param[in] permission_type Operation for which permission is to be set.
	 * @param[in] persistence_type Persistence of the setting. PERSISTENCE_TYPE_NONE is invalid.
	 * @param[in] hostname Host name for which to set the permission.
	 * @param[in] allowed Whether the operation is to be allowed (YES), denied (NO) or should the user be asked (MAYBE).
	 *
	 * @return
	 *  - OK on success,
	 *  - ERR_OUT_OF_RANGE if permission_type or persistency_type (or combination thereof) is not supported,
	 *  - ERR_NO_MEMORY.
	 */
	virtual OP_STATUS SetUserConsent(OpPermissionListener::PermissionCallback::PermissionType permission_type, OpPermissionListener::PermissionCallback::PersistenceType persistence_type, const uni_char *hostname, BOOL3 allowed) = 0;


	/** Get user permission for privacy-sensitive functionlity.
	 *
	 * Provides information about any permissions that are set for documents
	 * in the window associated with this window commander.
	 *
	 * The result is delivered asynchronously by calling
	 * OpPermissionListener::OnUserConsentInformation or
	 * OpPermissionListener::OnUserConsentError.
	 * @param [in] user_data user data which will be passed to a caller as a parameter of
	 *             OnUserConsentInformation or OnUserConsentError
	 */
	virtual void GetUserConsent(INTPTR user_data) = 0;
#endif // WIC_SET_PERMISSION

#ifdef GOGI_APPEND_HISTORY_LIST
	/** Append the given urls at the end of the window's history and activate
	 * the url which index is given in activate_url_index.
	 *
	 * Only the title and url fields of the HistoryInformation elements need to
	 * be set when calling this method.
	 */
	virtual OP_STATUS AppendHistoryList(const OpVector<HistoryInformation>& urls, unsigned int activate_url_index) = 0;
#endif // GOGI_APPEND_HISTORY_LIST
};



class OpMailClientListener
{
public:
	virtual void GetExternalEmbedPermission(OpWindowCommander* commander, BOOL& has_permission) = 0;
	virtual void OnExternalEmbendBlocked(OpWindowCommander* commander) = 0;
};

#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
/** @short Listener for selftest finished events. */
class OpSelftestListener
{
public:
	virtual ~OpSelftestListener() {}
	/** @short All started selftests in a test run have finished.
	  *
	  * This method is called when all started selftests in a test run
	  * have finished. A selftest run can be started by using the
	  * different --test flags when starting the Opera binary or by
	  * starting the test run from the opera:selftest page.
	  */
	virtual void OnSelftestFinished() = 0;
};
#endif //GOGI_SELFTEST_FINISHED_NOTIFICATION

#ifndef DOXYGEN_DOCUMENTATION
inline BOOL OpWindowCommander::GetHandheld() { return GetLayoutMode() == SSR; }
inline void OpWindowCommander::SetHandheld(BOOL handheld) { SetLayoutMode(handheld ? SSR : NORMAL); }
inline Window* OpWindowCommander::GetWindow() { return ViolateCoreEncapsulationAndGetWindow(); }
#endif // !DOXYGEN_DOCUMENTATION

#ifdef DEBUG
/**
 * This operator prints the specified enum value in human readable form to the
 * specified Debug instance. This can be used in side an OP_DBG statement like
 * @code
 * OpAuthenticationCallback* callback = ...;
 * OP_DBG(("type: ") << callback->GetType());
 * @endcode
 */
inline Debug& operator<<(Debug& d, enum OpAuthenticationCallback::AuthenticationType t)
{
	switch (t) {
# define CASE_OUT_ENUM(e) case OpAuthenticationCallback::e: return d << "OpAuthenticationCallback::" # e;
		CASE_OUT_ENUM(AUTHENTICATION_NEEDED);
		CASE_OUT_ENUM(AUTHENTICATION_WRONG);
		CASE_OUT_ENUM(PROXY_AUTHENTICATION_NEEDED);
		CASE_OUT_ENUM(PROXY_AUTHENTICATION_WRONG);
# undef CASE_OUT_ENUM
	default:
		return d << "OpAuthenticationCallback::AuthenticationType(unknown:" << static_cast<int>(t) << ")";
	}
}
#endif // DEBUG

#endif // OP_WINDOW_COMANDER_H
