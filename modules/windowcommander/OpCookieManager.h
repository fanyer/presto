/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPCOOKIEMANAGER_H
#define OPCOOKIEMANAGER_H

#ifdef _ASK_COOKIE

class OpWindowCommander;

class OpCookieListener
{
public:
	virtual ~OpCookieListener() {}

	/**
	 * Actions used by the UI/platform to signal how the cookie should be handled.
	 */
	enum CookieAction {
		COOKIE_ACTION_ACCEPT_AND_UPDATES,	///< Accept cookie and any updates before shutting down
		COOKIE_ACTION_ACCEPT_SHOW_UPDATES,	///< Accept without changes
		COOKIE_ACTION_ACCEPT_BUT_DISCARD,	///< Accept cookie, but delete when shutting down
		COOKIE_ACTION_ACCEPT_AND_SAVE,		///< Accept cookie and save when shutting down

		COOKIE_ACTION_ACCEPT_ALL_FROM_DOMAIN_BUT_DISCARD,	///< Accept all cookies from domain, but delete when shutting down

		COOKIE_ACTION_ACCEPT_ALL_FROM_DOMAIN,	///< Accept all cookies from domain
		COOKIE_ACTION_ACCEPT_ALL_3P_FROM_DOMAIN,///< Accept all third party cookies from domain

		COOKIE_ACTION_REFUSE,					///< Refuse cookie
		COOKIE_ACTION_REFUSE_ALL_FROM_DOMAIN,	///< Refuse all cookies from domain
		COOKIE_ACTION_REFUSE_ALL_3P_FROM_DOMAIN	///< Refuse all third party cookies from domain
	};

	/**
	 * Called when Opera needs to display a cookie security warning.
	 *
	 * @param wic The windowcommander in question.
	 * @param url The URL
	 * @param cookie The value of the cookie
	 * @param no_ip_address
	 */
	virtual void OnCookieSecurityWarning(OpWindowCommander* wic, const char* url, const char* cookie, BOOL no_ip_address) = 0;

	/**
	 * Callback context used to interact with cookies. The context object is owned
	 * by core and is valid until OnAskCookieDone or OnAskCookieCancel has been called.
	 */
	class AskCookieContext
	{
	public:
		virtual ~AskCookieContext() {}

		/** returns the server name of the cookie */
		virtual const uni_char* GetServerName() = 0;

		/** returns the name of the cookie */
		virtual const char* GetName() const = 0;

		/** returns the value of the cookie */
		virtual const char* GetValue() const = 0;

		/** returns the domain of the cookie */
		virtual const char* GetDomain() const = 0;

		/** returns the path of the cookie */
		virtual const char* GetPath() const = 0;

		/** return the cookie comment */
		virtual const char* GetComment() const = 0;

		/** return comment URL */
		virtual const char* GetCommentURL() const = 0;

		/** returns the number of ports in the port list */
		virtual unsigned GetPortCount() const = 0;

		/** returns the port list */
		virtual const unsigned short* GetPortList() const = 0;

		/** returns a string representation of the port list */
		virtual const char* GetPort() const = 0;

		/** returns the time and date the cookie expires */
		virtual time_t GetExpire() const = 0;

		/** returns the time and date the cookie was last used */
		virtual time_t GetLastUsed() const = 0;

		/** returns true if the cookie was stored when visiting a server via HTTPS */
		virtual bool GetSecure() const = 0;

		/**
		 * Called as a response to OnAskCookie indicating that the request has succeeded. Note
		 * that refused requests should call OnAskCookieDone with the appropriate action flag
		 * and not OnAskCookieCancel.
		 *
		 * @param action contains option on how the cookie should be handled.
		 */
		virtual void OnAskCookieDone(CookieAction action) = 0;

		/**
		 * Called as a response to OnAskCookie indicating the request has failed, e.g.
		 * as a result of OOM or the parent window being destroyed.
		 */
		virtual void OnAskCookieCancel() = 0;
	};

	/**
	 * Called when Opera needs to ask the user whether to accept a cookie. The UI/platform must respond
	 * by calling one of the callbacks in context (OnAskCookieDone or OnAskCookieCancel). The callback can be called
	 * directly (from within OnAskCookie) or later on.
	 *
	 * @param wic The windowcommander in question.
	 * @param context Information on the cookie and callbacks used to report when done.
	 */
	virtual void OnAskCookie(OpWindowCommander* wic, AskCookieContext* context) = 0;
};
#endif // _ASK_COOKIE

#ifdef WIC_COOKIES_LISTENER
/** @short Listener for cookies being added or removed.
 *
 * When the iterator is registered in OpWindowCommanderManager it will be
 * informed every time when cookie is added, removed or when cookies iteration
 * is initialized.
*/
class OpCookieIteratorListener
{
public:
	virtual ~OpCookieIteratorListener() {}

	/** This method will be invoked when new cookie is added. */
	virtual void OnSetCookie(const Cookie& cookie) = 0;

	/** This method will be invoked when any cookie is deleted. */
	virtual void OnDeleteCookie(const Cookie& cookie) = 0;

	/** When cookies iteration is initialized (by calling method
	 * OpWindowCommanderManager::IterateAllCookies) this method
	 * will be called for every cookie present at the moment.
	 */
	virtual void OnIterateCookies(const Cookie& cookie) = 0;
};
#endif // WIC_COOKIES_LISTENER

#ifdef WIC_COOKIE_MANAGER_LISTENER
/** @short Listener for events from cookie manager
 */
class OpCookieManagerListener
{
public:
	virtual ~OpCookieManagerListener() {}

	/** @short Called when platform/UI requests saving cookies to file, and the
	 * operation finishes.
	 *
	 * @param successful True if cookies were successfully saved to file,
	 * false otherwise.
	 * @param requested_by_platform True if the action was taken after
	 * platform request, false if it was initiated by Opera.
	 */
	virtual void OnCookieFileSaveFinished(BOOL successful, BOOL requested_by_platform = FALSE) = 0;
};
#endif

/** Cookie Manager
 *
 * Interface for various Cookie functions
 */
class OpCookieManager
{
public:
	static OpCookieManager* CreateL();
	virtual ~OpCookieManager() {}

#ifdef _ASK_COOKIE
	/** Set the listener for cookie dialogs that does not explicitly belong to a windowcommander.
	 * This is the same listener that is used in OpWindowCommander, but when used in this
	 * context, the windowcommander argument will always be NULL.
	 * @param listener The listener to be set, or NULL to stop listening.
	 */
	virtual void SetCookieListener(OpCookieListener* listener) = 0;
	/** Get the current cookie listener. If no listener is set, a
	 * dummy (non-NULL) listener will be returned. */
	virtual OpCookieListener* GetCookieListener() = 0;
#endif // _ASK_COOKIE

#ifdef WIC_COOKIES_LISTENER
	/** Sets the listener which will be informed every time when new cookie is added or removed.
	 * It's also possible to iterate all cookies.
	 * @param listener The listener to be set, or NULL to stop listening. */
	virtual void SetCookieIteratorListener(OpCookieIteratorListener* listener) = 0;
	virtual OpCookieIteratorListener* GetCookieIteratorListener() = 0;

	/** Initialize cookies iteration. */
	virtual void IterateAllCookies() = 0;
#endif // WIC_COOKIES_LISTENER

#ifdef WIC_COOKIE_MANAGER_LISTENER
	/** Set the cookie manager listener. Only one listener may be set.
	 * @param listener The listener to be set, or NULL to stop listening. */
	virtual void SetCookieManagerListener(OpCookieManagerListener* listener) = 0;
	/** Get the current cookie manager listener. If no listener is set, a
	 * dummy (non-NULL) listener will be returned. */
	virtual OpCookieManagerListener* GetCookieManagerListener() = 0;
#endif

#ifdef COOKIE_MANUAL_MANAGEMENT
	/** Request saving all cookie data to file. */
	virtual void RequestSavingCookies() = 0;
#endif
};

#endif // OPCOOKIEMANAGER_H
