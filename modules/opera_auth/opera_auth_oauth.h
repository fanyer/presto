/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef QUERY_PARSER_IMPL_H_
#define QUERY_PARSER_IMPL_H_
#ifdef OPERA_AUTH_SUPPORT
#include "modules/url/url2.h"


/**
 * API to handle 2-legged OAUth authentication.
 *
 * In Normal 3-legged OAuth mode there are 3 parties:
 * The resource owner (the desktop user), which lets
 * a client (or consumer for backward compatibility)
 * access resources on a server on behalf of the resource owner.
 *
 * In 2-legged OAuth, the consumer and the resource owner is
 * one entity. Thus the consumer secrets are the same for all
 * instances of opera.
 *
 * All OAuth parameters and consumer credentials are set automatically by the API.
 *
 * Global API is g_opera_oauth_manager, but your own custom manager
 * can also be created.
 *
 * To get access to resources protected by OAuth do the following.
 *
 * 1. Set appropriate listener
 * 2. Call Login with appropriate username and password
 * 3. Wait for the OAUTH_LOGIN_SUCCESS_NEW_TOKEN_DOWNLOADED callback event
 * 4. Sign URL with OauthSignUrl().
 *
 * Please note that OauthSignUrl() must be called for every new request, even if the
 * same URL is used over again.
 *
 */
class OperaOauthManager
{
public:
	virtual ~OperaOauthManager(){}
	OperaOauthManager(){}

	/** Create an opera_oauth_manager
	 *
	 * @param opera_oauth_manager the manager to create
	 * @param set_default_client_credentials The client credentials are normally
	 * 										 the same for all instances of Opera.
	 * 									 	 May vary for different opera projects
	 * 										 in the feature.
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */

	static void CreateL(OperaOauthManager*& opera_oauth_manager, BOOL set_default_client_credentials = TRUE);

	/**
	 * Set the URI for accessing the tokens
	 *
	 * Only use if different from the default URI given in pref
	 *
	 */
	virtual OP_STATUS SetAccessTokenURI(const char *access_token_uri) = 0;

	/** Set OAuth Client secret
	 *
	 * This call is normally not needed, as the client
	 * credentials are set by CreateL
	 *
	 * @param client_secret The client secret
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 *
	 */
	virtual OP_STATUS SetClientSecret(const char *client_secret) = 0;

	/** Set OAuth Client identity key (also called consumer key)
	 *
	 * This call is normally not needed, as the client
	 * credentials are set by CreateL
	 *
	 * @param 	client_secret The client secret
	 * @return	OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 *
	 */
	virtual OP_STATUS SetClientIdentityKey(const char *oauth_consumer_key) = 0;

	enum OAuthStatus
	{
		OAUTH_LOGIN_SUCCESS_NEW_TOKEN_DOWNLOADED,  // The log n was a success and an OAuth token was downloaded
		OAUTH_LOGIN_ERROR_WRONG_USERNAME_PASSWORD, // Wrong username and/or password
		OAUTH_LOGIN_ERROR_TIMEOUT, 				   // Timeout while downloading the tokens.
		OAUTH_LOGIN_ERROR_NETWORK,				   // Generic network error, downloading token failed
		OAUTH_LOGIN_ERROR_GENERIC,				   // Generic/internal error, such as memory problems

		OAUTH_LOGGED_OUT						   // Someone has called Logout()
	};

	class LoginListener
	{
	public:
		virtual ~LoginListener() {}

		/** The login status has changed
		 *
		 * @param status		 The new login status.
		 * @param server_message Message from auth server in case of
		 * 						 login error. Might be Empty.
		 */
		virtual void OnLoginStatusChange(OAuthStatus status, const OpStringC &server_error_message) = 0;
	};

	/**
	 * Add login listener
	 *
	 * @param listener		listener
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	virtual OP_STATUS AddListener(LoginListener *listener) = 0;

	/**
	 * Remove login listener
	 *
	 * @param listener		listener
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	virtual OP_STATUS RemoveListener(LoginListener *listener) = 0;

	/**
	 * Check if OAuth tokens are downloaded, and
	 * OauthSignUrl can be called.
	 *
	 * @return  If TRUE the OAuth tokens have been
	 * 			downloaded.
	 */
	virtual BOOL IsLoggedIn() = 0;

	/**
	 * Check if OAuth tokens are in progress
	 * of being downloaded.
	 *
	 * @return If TRUE the login process has started
	 * 		   and tokens are being downloaded.
	 */
	virtual BOOL IsInLoginProgress() = 0;
	/**
	 * Login to the auth server. This will trigger
	 * download of the tokens needed for signing urls.
	 *
	 * @param username 	The username
	 * @param password	The password
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY or OpStatus::ERR for generic error
	 */
	virtual OP_STATUS Login(const OpStringC& username, const OpStringC& password) = 0;

	/**
	 * Logout from auth server. This will remove all tokens,
	 * and signing URLs are not possible
	 */
	virtual void Logout() = 0;

	/**
	 * Sign a request URL.
	 *
	 * @param url				The URL to sign
	 * @param http_method		The method used in the request
	 * @param realm				Set the realm for this request. Can be NULL.
	 * 							The realm is not a part of signature generation.
	 * 							Thus it's only needed if server demands it.
	 * @return OpStatus::OK,
	 * 		   OpStatus::ERR_NO_ACCESS if not logged in,
	 * 		   OpStatus::ERR_NO_MEMORY for OOM.
	 */
	virtual OP_STATUS OauthSignUrl(URL &url, HTTP_Method http_method, const char *realm = NULL) = 0;
};

/**
 * OAuth Signature Generator
 *
 * Lower level API for handling general OAuth. Can be used
 * to handle 3-legged OAuth signatures.
 *
 * Please Note that queries in the body when posting
 * content-type "application/x-www-form-urlencoded" is
 * not currently added to the signatures as required.
 *
 * Thus, requests which posts content-type "application/x-www-form-urlencoded"
 * will not be correctly signed.
 */

class OauthSignatureGenerator
{
public:
	OauthSignatureGenerator(){}
	virtual ~OauthSignatureGenerator(){}

	static OauthSignatureGenerator *Create();

	/**
	 * Clear all login credentials.
	 */
	virtual void Clear() = 0;

	/**
	 * Sets the oauth_auth_token parameter retrieved from the
	 * authorization server (opera-auth.com)
	 *
	 * @param oauth_auth_token the OAuth token
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	virtual OP_STATUS SetToken(const char *oauth_auth_token) = 0;

	/**
	 * Sets the OAuth parameter retrieved from the
	 * authorization server (opera-auth.com)
	 *
	 * @param oauth_auth_token_secret the secret OAuth token
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	virtual OP_STATUS SetTokenSecret(const char *oauth_auth_token_secret) = 0;

	/**
	 * Set the client secret (consumer secret)
	 *
	 * You do normally not have to call this function.
	 *
	 * @param override_client_secret The client secret
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	virtual OP_STATUS SetClientSecret(const char *override_client_secret) = 0;

	/**
	 * Set The client identity key (oauth_consumer_key)
	 *
	 * @param override_client_secret The client secret
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */

	virtual OP_STATUS SetClientIdentityKey(const char *override_client_secret) = 0;

	/**
	 * Add OAuth parameters to the OAuth custom "Authorization" header
	 *
	 * If a parameter with same name has previously been added,
	 * the previous parameter will be overwritten.
	 *
	 * Calling this function is normally not needed.
	 *
	 * oauth_auth_token is set by the explicit SetToken function
	 *
	 * The following parameters are set by the API up front
	 * oauth_consumer_key
	 * oauth_version
	 * oauth_signature_method
	 *
	 * These parameters will be set when signing the url.
	 * oauth_timestamp
	 * oauth_nonce
	 *
	 * The oauth_signature will be generated by SignUrl below using
	 * the oauth_auth_token_secret.
	 *
	 * You cannot set realm here, set this in SignUrl if needed.
	 *
	 * @param name	The name of the parameter to set
	 * @param value The value of the parameter
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	virtual OP_STATUS AddAuthHeaderParameter(const char *name, const char *value) = 0;

	/**
	 * Sign a url that is requesting a OAuth protected resource.
	 *
	 * The url must be signed for every new request.
	 *
	 * The method must be the same used in the request. This
	 * is important, as the method is part of signing, and
	 * the user will not be authenticated if method is wrong.
	 *
	 * @param url			The url to sign.
	 * @param http_method	The method used in the request.
	 * @param realm			Set the realm for this request. Can be NULL.
	 * 						The realm is not a part of signature generation.
	 * 						Thus it's only needed if server demands it.
	 */
	virtual OP_STATUS SignUrl(URL &url, HTTP_Method http_method, const char *realm = NULL) = 0;
};

#endif // OPERA_AUTH_SUPPORT
#endif /* QUERY_PARSER_IMPL_H_ */
