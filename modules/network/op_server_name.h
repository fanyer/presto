/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OP_SERVER_NAME_H__
#define __OP_SERVER_NAME_H__

#include "modules/url/url_sn.h"
/** @class OpServerName
 *
 */

class OpServerName
{
public:
	/** Destructor. */
	virtual ~OpServerName() {}

	/** Get the current SocketAddress. */
	virtual OpSocketAddress* SocketAddress() = 0;

	/** Set this Host's IP address from the string */
	virtual OP_STATUS SetHostAddressFromString(const OpStringC& address) = 0;

	/** Return the unicode version of this server name. */
	virtual const uni_char*	UniName() const = 0;

	/** Return the 8-bit version of this server's name. */
	virtual const char *Name() const = 0;

	/** Return the number of components in the servername. */
	virtual uint32 GetNameComponentCount() = 0;

	/** Return a OpStringC8 object referencing the given name component item of the servername, empty if no component. */
	virtual OpStringC8 GetNameComponent(uint32 item) = 0;

	/** Return a pointer to the servername object for the parent domain.
	 *
	 *	@param servername Pointer to the servername object for the parent domain. Servername will be NULL if no parent domain exists.
	 *	@return OK or ERR_NO_MEMORY
	 */
	virtual OP_STATUS GetParentDomain(OpServerName *&servername) = 0;

	/** Get a pointer for the most specific common domain name the two domains, NULL if no common domain exists.
	 *
	 *  E.g. the common domain of "www.opera.com" and "web.opera.com" is "opera.com", for
	 *	"www.microsoft.com" and "www.opera.com" it is "com", and for "www.opera.com" and
	 *	"www.opera.com" there is no common domain and it will return a NULL pointer.
	 *
	 *	@param	servername	The servername we are comparing with.
	 *	@param	result		The domain common between this name and the other name, may be NULL.
	 *	@return OK or ERR_NO_MEMORY
	 */
	virtual OP_STATUS GetCommonDomain(OpServerName *servername, OpServerName *&result) = 0;

#ifdef _ASK_COOKIE
	/** Set the cookie filter setting. */
	virtual void SetAcceptCookies(COOKIE_MODES mode) = 0;

	/** Get the cookie filter setting.
	 *
	 * @param follow_domain	If set the top parent domain is used.
	 * @param use_local		Use the domain "local" if follow_domain==TRUE and there is no parent domain.
	 */
	virtual COOKIE_MODES GetAcceptCookies(BOOL follow_domain = FALSE, BOOL use_local = TRUE) = 0;

	/** Set the illegal path filter.
	 *
	 * @param mode Should cookies be deleted on exit.
	 */
	virtual void SetAcceptIllegalPaths(COOKIE_ILLPATH_MODE mode) = 0;

	/** Get the illegal path filter.
	 *
	 * @param follow_domain	If set the top parent domain is used.
	 * @param use_local		Use the domain "local" if follow_domain==TRUE and there is no parent domain.
	 */
	virtual COOKIE_ILLPATH_MODE GetAcceptIllegalPaths(BOOL follow_domain = FALSE, BOOL use_local = TRUE) = 0;

	/** Set Domain specific empty on exit.
	 *
	 * @param mode Should cookies be deleted on exit.
	 */
	virtual void SetDeleteCookieOnExit(COOKIE_DELETE_ON_EXIT_MODE mode) = 0;

	/** Get the delete cookie on exit setting.
	 *
	 * @param follow_domain	If set the top parent domain is used.
	 * @param use_local		Use the domain "local" if follow_domain==TRUE and there is no parent domain.
	 */
	virtual COOKIE_DELETE_ON_EXIT_MODE GetDeleteCookieOnExit(BOOL follow_domain = FALSE, BOOL use_local = TRUE) = 0;
#endif
	/** Set the third party filter setting. */
	virtual void SetAcceptThirdPartyCookies(COOKIE_MODES mod) = 0;

	/** Get the third party filter setting.
	 * @param follow_domain	If set the top parent domain is used.
	 * @param first 		If set, the "local" domain is used.
	 */
	virtual COOKIE_MODES GetAcceptThirdPartyCookies(BOOL follow_domain = FALSE, BOOL first = TRUE) = 0;

	/** Are we permitted to pass usernames (and passwords) embedded in the URL to this server without challenging the user? */
	virtual BOOL GetPassUserNameURLsAutomatically(const OpStringC8 &p_name) = 0;

	/** Set whether or not we are permitted to pass usernames (and passwords) embedded in the URL to this server without challenging the user? */
	virtual OP_STATUS SetPassUserNameURLsAutomatically(const OpStringC8 &p_name) = 0;

#ifdef TRUST_RATING
	/** Set Time To Live for the trust rating. */
	virtual void SetTrustTTL(unsigned int TTL) = 0;

	/** Get trust rating level. */
	virtual TrustRating GetTrustRating() = 0;

	/** Set trust rating level. */
	virtual void SetTrustRating(TrustRating new_trust_rating) = 0;

	/** Is the trust rating bypassed for this server? */
	virtual BOOL GetTrustRatingBypassed() = 0;

	/** Specify that trust rating warning is to be bypassed for this server. */
	virtual void SetTrustRatingBypassed(BOOL new_trust_rating_bypassed) = 0;

	/** Add a url to the list of fraud urls we will warn about. */
	virtual OP_STATUS AddTrustRatingUrl(const OpStringC &url, int id) = 0;

	/** See if a fraud 	url can be found. */
	virtual BOOL FindTrustRatingUrl(const OpStringC &url) = 0;

	/** Add a regular expression to the list of fraud expressions we will warn about. */
	virtual OP_STATUS AddTrustRatingRegExp(const OpStringC &regexp) = 0;

	/** Retrieve first regular expression item. */
	virtual FraudListItem *GetFirstTrustRatingRegExp() = 0;

	/** Is fraud list empty? */
	virtual BOOL FraudListIsEmpty() = 0;

	/** Add advisory for a URL.
	 *
	 *  @param homepage_url	url for the advisory.
	 *  @param advisory_url	url for the advisory.
	 *  @param text
	 *  @param type
	 */
	virtual void AddAdvisoryInfo(const uni_char *homepage_url, const uni_char *advisory_url, const uni_char *text, unsigned int src, ServerSideFraudType type) = 0;

	/** Retrieve the advisory for a URL.
	 *
	 *  @param url	url for the advisory.
	 *  @return 	Advisory containing more info about the url.
	 */
	virtual Advisory *GetAdvisory(const OpStringC &url) = 0;
#endif
};

class ServerNameCallback
{
public:
	virtual void ServerNameResult(OpServerName *server_name, OP_STATUS result) = 0;
};

#endif
