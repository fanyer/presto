/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef QUERY_PARSER_H_
#define QUERY_PARSER_H_
#ifdef OPERA_AUTH_SUPPORT

#include "modules/url/url2.h"
#include "modules/opera_auth/opera_auth_oauth.h"
#include "modules/url/url_docload.h"
#include "modules/hardcore/timer/optimer.h"

#define OAUTH_VERSION "1.0"
#define OPERA_OAUTH_SIGNATURE_METHOD "HMAC-SHA1"
struct ParameterItem
{
	enum Type
	{
		OAUTH_AUTHORIZATION_PARAMETER,  // Parameters in the customary AOauth Authorization header
		REQUEST_PARAMETER				// Parameters of type name=value given in the request string.

	} type;

	ParameterItem(Type oauth_header_parameter)
	: type(oauth_header_parameter) {}

	OpString8 name;
	OpString8 value;
};


class OauthParameterHandler
{
public:
	OauthParameterHandler() : m_sorted(FALSE), m_nonce(0){}
	virtual ~OauthParameterHandler(){}
	void Clear();

	static OP_STATUS SimpleOauthPercentEscape(const char *unescaped, int unescaped_length, OpString8 &escaped_result);
	static OP_STATUS PercentUnescapeEscape(const char *escaped, int escaped_length, OpString8 &unescaped_result);

	OP_STATUS SetStandardAuthHeaderParameters();
	OP_STATUS SetNonceAndTimeStamp();

	/* Add OAuth parameter
	 *
	 * Replaces any previous parameter with same name
	 *
	 * @param name	parameter name
	 * @param value	parameter name
	 * @return 		OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS AddAuthHeaderParameter(const char *name, const char *value) { return AddParameter(name, op_strlen(name), value, op_strlen(value), ParameterItem::OAUTH_AUTHORIZATION_PARAMETER);}
	OP_STATUS AddQueryParameter(const char *name, const char *value) { return AddParameter(name, op_strlen(name), value, op_strlen(value), ParameterItem::REQUEST_PARAMETER);}

	OP_STATUS CollectAndAddQueryParamentersFromUrl(URL &url);

	OP_STATUS GetNormalizedRequestParameterString(OpString8 &normalized_request_parameters);
	OP_STATUS GetUnsignedAuthorizationHeaderValue(OpString8 &authorization_header_value);

	/* Search from from_index. updates index to where parameter was found */
	ParameterItem *FindParameter(const char *name, int name_length, ParameterItem::Type type, UINT32 &from_index);

	OP_STATUS Parse(const char *query_string, int length = KAll);
private:
	OP_STATUS AddParameter(const char *name, int name_length, const char *value, int value_length, ParameterItem::Type type = ParameterItem::REQUEST_PARAMETER);

	OpAutoVector<ParameterItem> m_name_value_queries;
	BOOL m_sorted;
	UINT m_nonce;
};

class OauthSignatureGeneratorImpl : public OauthSignatureGenerator
{
public:

	static OauthSignatureGeneratorImpl *Create(BOOL auto_set_nonce_and_timestamp = TRUE, BOOL set_standard_auth_parameters = TRUE);

	virtual ~OauthSignatureGeneratorImpl(){}

	virtual void Clear();
	virtual OP_STATUS SetStandardAuthHeaderParameters();
	virtual OP_STATUS AddAuthHeaderParameter(const char *name, const char *value) { return m_parameter_handler.AddAuthHeaderParameter(name, value); }
	virtual OP_STATUS AddQueryParameter(const char *name, const char *value) { return m_parameter_handler.AddAuthHeaderParameter(name, value); }

	virtual OP_STATUS SetToken(const char *oauth_auth_token_value) { return m_parameter_handler.AddAuthHeaderParameter("oauth_token", oauth_auth_token_value); }
	virtual OP_STATUS SetTokenSecret(const char *oauth_auth_token_secret) { return m_oauth_auth_token_secret.Set(oauth_auth_token_secret); }

	virtual OP_STATUS SignUrl(URL &url, HTTP_Method http_method, const char *realm = NULL);

	virtual OP_STATUS SetClientSecret(const char *client_secret) { return m_client_secret.Set(client_secret); }
	virtual OP_STATUS SetClientIdentityKey(const char *oauth_consumer_key) { return m_parameter_handler.AddAuthHeaderParameter("oauth_consumer_key", oauth_consumer_key); }

	/* Low level, must be public to to selftests */
	OP_STATUS GetSignedAuthorizationHeaderValue(OpString8 &signed_authorization_header_value, URL &url, HTTP_Method http_method);
	OP_STATUS CreateOauthSignature(OpString8 &signature, URL &url, HTTP_Method http_method);

	OP_STATUS CreateRequestTokenString(OpString8 &request_string, const OpStringC8 &username, const OpStringC8 &password);
#ifdef SELFTEST
	const char* GetNormalizedRequestParameterString() { return m_signature_base_string.CStr(); }
#endif // SELFTEST

private:
	OauthSignatureGeneratorImpl(BOOL auto_set_auth_parameters = TRUE) : m_auto_set_nonce(auto_set_auth_parameters) {}
	OP_STATUS CreateMacKey(OpString8 &key);
	OP_STATUS CreateAndDigestSignatureBaseString(OpString8 &digest_base_64, URL &url, HTTP_Method http_method, const UINT8 *key, int key_length);

	OpString8 m_oauth_auth_token_secret;
	OpString8 m_client_secret;
	OauthParameterHandler m_parameter_handler;

	BOOL m_auto_set_nonce;
#ifdef SELFTEST
	OpString8 m_signature_base_string;
#endif // SELFTEST
};

class OperaOauthManagerImpl : public OperaOauthManager, public URL_DocumentLoader, public OpTimerListener
{
public:
	virtual ~OperaOauthManagerImpl(){};
	OperaOauthManagerImpl(BOOL set_default_client_credentials = TRUE)
	: m_is_in_progress(FALSE)
	, m_logged_in(FALSE)
	, m_set_default_client_credentials(set_default_client_credentials)
	{
		m_timeout_handler.SetTimerListener(this);
	};

	static void CreateL(OperaOauthManagerImpl*& opera_oauth_account_manager, BOOL set_default_client_credentials);

	virtual OP_STATUS SetAccessTokenURI(const char *access_token_uri) { return m_access_token_uri.Set(access_token_uri); }
	virtual OP_STATUS SetClientSecret(const char *client_secret) { return m_oauth_signature_generator->SetClientSecret(client_secret); }
	virtual OP_STATUS SetClientIdentityKey(const char *oauth_consumer_key) { return m_oauth_signature_generator->SetClientIdentityKey(oauth_consumer_key); }

	BOOL IsLoggedIn(){ return m_logged_in; }
	BOOL IsInLoginProgress(){ return m_is_in_progress; }

	virtual OP_STATUS Login(const OpStringC& username, const OpStringC& password);
	virtual void Logout();

	virtual OP_STATUS AddListener(LoginListener *listener) { return m_listeners.Add(listener); }
	virtual OP_STATUS RemoveListener(LoginListener *listener) { return m_listeners.RemoveByItem(listener); }
	virtual OP_STATUS OauthSignUrl(URL &url, HTTP_Method http_method, const char *realm = NULL);

	virtual void OnURLLoadingFailed(URL &url, Str::LocaleString error, OpAutoPtr<URL_DataDescriptor> &stored_desc);
	virtual void OnURLLoadingStopped(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc);

	virtual void OnURLLoadingFinished(URL &url);

private:
	void ClearUserCredentials();
	void LoadingStopped();
	virtual void OnTimeOut(OpTimer* timer);

	void BroadCastOnLoginStatusChange(OAuthStatus status, const OpStringC &server_message);

	OP_STATUS GetRequestTokenString(OpString8& post_string_utf8, const OpStringC& username, const OpStringC& password);

	BOOL m_is_in_progress;
	BOOL m_logged_in;
	BOOL m_set_default_client_credentials;
	BOOL m_default_client_credentials_is_set;
	OpString8 m_access_token_uri;
	OpAutoPtr<OauthSignatureGeneratorImpl> m_oauth_signature_generator;
	OpVector<LoginListener> m_listeners;
	OpTimer m_timeout_handler;

	URL m_access_token_url;

#ifdef SELFTEST
	friend class SelftestHelper;
public:
	/** This class is a helper class which can be used in selftests to access
	 * private members and methods of an OperaOauthManagerImpl instance. */
	class SelftestHelper {
		OperaOauthManagerImpl* m_impl;
	public:
		SelftestHelper(OperaOauthManagerImpl* impl) : m_impl(impl) {}
		OP_STATUS GetRequestTokenString(OpString8& post_string_utf8, const OpStringC& username, const OpStringC& password)
		{ return m_impl->GetRequestTokenString(post_string_utf8, username, password); }
	};
#endif // SELFTEST
};
#endif // OPERA_AUTH_SUPPORT
#endif /* QUERY_PARSER_H_ */
