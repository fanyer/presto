/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef OPERA_AUTH_SUPPORT
#include "modules/opera_auth/src/opera_auth_oauth_impl.h"
#include "modules/prefs/prefsmanager/collections/pc_opera_account.h"
#include "modules/libcrypto/include/CryptoUtility.h"
#include "modules/libcrypto/include/CryptoHMAC.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/formats/uri_escape.h"
#include "modules/upload/upload.h"
#include "modules/url/tools/url_util.h"
#include "modules/util/timecache.h"
#include "modules/url/url_docload.h"
#include "modules/formats/hex_converter.h"

static BOOL oauth_percent_escape(char c)
{
	return (!op_isalnum(c) && c != '-' && c != '.' && c != '_' && c != '~' );
}

void OauthParameterHandler::Clear()
{
	m_name_value_queries.DeleteAll();
	m_sorted = FALSE;
}

/* static */ OP_STATUS OauthParameterHandler::SimpleOauthPercentEscape(const char *unescaped, int unescaped_length, OpString8 &escaped_result)
{
	/* This escaping method is specific to OAuth, thus UriEscape cannot be used */
	escaped_result.Wipe();

	int count_escapes = 0;
	int i;
	for (i = 0; i < unescaped_length; i++)
		count_escapes += oauth_percent_escape(unescaped[i]) ? 1 : 0;

	int escaped_length = unescaped_length + 2*count_escapes;
	RETURN_OOM_IF_NULL(escaped_result.Reserve(escaped_length + 1));

	int escape_index = 0;
	for (i = 0; i < unescaped_length; i++)
	{
		char c = unescaped[i];

		if (oauth_percent_escape(c))
		{
			// escape
			escaped_result[escape_index++] = '%';
			HexConverter::ToHex(&escaped_result[escape_index], c, HexConverter::UseUppercase | HexConverter::DontNullTerminate);
			escape_index += 2;
		}
		else
		{
			escaped_result[escape_index++] = c;
		}
	}
	// null terminate
	escaped_result[escape_index] = '\0';

	OP_ASSERT(escape_index == escaped_length);

	return OpStatus::OK;
}

/* static */ OP_STATUS OauthParameterHandler::PercentUnescapeEscape(const char *escaped, int escaped_length, OpString8 &unescaped_result)
{
	RETURN_OOM_IF_NULL(unescaped_result.Reserve(escaped_length + 1));

	UriUnescape::Unescape(unescaped_result.CStr(), escaped, escaped_length, UriUnescape::ConvertUtf8 | UriUnescape::ExceptCtrl | UriUnescape::TranslatePlusToSpace);
	return OpStatus::OK;
}

OP_STATUS OauthParameterHandler::AddParameter(const char *name, int name_length, const char *value, int value_length, ParameterItem::Type type)
{
	m_sorted = FALSE;
	OpAutoPtr<ParameterItem> item;

	if (type == ParameterItem::OAUTH_AUTHORIZATION_PARAMETER)
	{
		if (op_strcmp(value, "realm") == 0)
		{
			OP_ASSERT(!"Don't set the realm here, set it when signing the request url instead");
			return OpStatus::OK;
		}

		/* replace previous auth parameters with the same name */
		UINT32 index = 0;
		item.reset(FindParameter(name, name_length, type, index));

		OP_ASSERT(!item.get() || item.get() == m_name_value_queries.Get(index));

		if (item.get())
			m_name_value_queries.Remove(index);
	}

	if (!item.get())
	{
		item.reset((OP_NEW(ParameterItem, (type))));
		RETURN_OOM_IF_NULL(item.get());
	}

	RETURN_IF_ERROR(SimpleOauthPercentEscape(name, name_length, item->name));
	if (value && *value != '\0')
		RETURN_IF_ERROR(SimpleOauthPercentEscape(value, value_length, item->value));

	RETURN_IF_ERROR(m_name_value_queries.Add(item.get()));
	item.release();

	return OpStatus::OK;
}

OP_STATUS OauthParameterHandler::Parse(const char *query_string, int query_length)
{
	if (!query_string || query_length == 0)
		return OpStatus::OK;

	const char *start_param = query_string;
	size_t length = query_length < 0 ? op_strlen(start_param) : (size_t)query_length;

	do
	{
		const char *next_param = static_cast<const char*>(op_memchr(start_param, '&', length));
		size_t param_length;
		if (next_param)
			param_length = next_param++ - start_param;
		else
			param_length = length;

		if (param_length)
		{
			const char *split_param = static_cast<const char*>(op_memchr(start_param, '=', param_length));
			size_t name_length = split_param ? split_param - start_param : param_length;
			size_t value_length = split_param ? param_length - name_length - 1 : 0;
			OpString8 name;
			if (name_length)
				RETURN_IF_ERROR(PercentUnescapeEscape(start_param, name_length, name));
			OpString8 value;
			if (value_length)
				RETURN_IF_ERROR(PercentUnescapeEscape(split_param + 1, value_length, value));

			if (name.HasContent() || value.HasContent())
				RETURN_IF_ERROR(AddParameter(name, name.Length(), value, value.Length(), ParameterItem::REQUEST_PARAMETER));
		}
		start_param = next_param;
		length -= (param_length+1);
	} while (start_param);

	return OpStatus::OK;
}

static int CompareQueryItems(const ParameterItem **item_a, const ParameterItem **item_b)
{
	if (item_a == item_b || *item_a == *item_b)
		return 0;

	OP_ASSERT((*item_a)->name.HasContent() && (*item_b)->name.HasContent());

	int name_compare = op_strcmp((*item_a)->name.CStr(), (*item_b)->name.CStr());

	if (name_compare != 0)
		return name_compare;
	else
	{
		const char *value_a = (*item_a)->value.HasContent() ? (*item_a)->value.CStr() : "\0"; // in case of NULL
		const char *value_b = (*item_b)->value.HasContent() ? (*item_b)->value.CStr() : "\0"; // in case of NULL
		return op_strcmp(value_a, value_b);
	}
}

OP_STATUS OauthParameterHandler::SetStandardAuthHeaderParameters()
{
	RETURN_IF_ERROR(AddAuthHeaderParameter("oauth_version", OAUTH_VERSION));
	return AddAuthHeaderParameter("oauth_signature_method", OPERA_OAUTH_SIGNATURE_METHOD);
}

OP_STATUS OauthParameterHandler::SetNonceAndTimeStamp()
{
	OpString8 time_parameter;
	RETURN_IF_ERROR(time_parameter.AppendFormat("%u", g_timecache->CurrentTime()));
	RETURN_IF_ERROR(AddAuthHeaderParameter("oauth_timestamp", time_parameter.CStr()));

	OpString8 nonce_parameter;
	RETURN_IF_ERROR(g_libcrypto_random_generator->GetRandomHexString(nonce_parameter, 8));

	RETURN_IF_ERROR(nonce_parameter.AppendFormat("%08x", m_nonce++));
	return AddAuthHeaderParameter("oauth_nonce", nonce_parameter.CStr());
}

OP_STATUS OauthParameterHandler::CollectAndAddQueryParamentersFromUrl(URL &url)
{
	OpString8 query;
	RETURN_IF_ERROR(url.GetAttribute (URL::KQuery, query));

	if (query.HasContent())
		RETURN_IF_ERROR(Parse(query.CStr() + 1));

	/* If the body content-type is application/x-www-form-urlencoded, and follow the requirements
	 * of for that content-type, also retrieve the body parameters. */

	/* We skip this for now */

	return OpStatus::OK;
}

OP_STATUS OauthParameterHandler::GetNormalizedRequestParameterString(OpString8 &normalized_request_parameters)
{
	if (!m_sorted)
		m_name_value_queries.QSort(&CompareQueryItems);

	m_sorted = TRUE;
	BOOL first = TRUE;
	const char *format_string = "&%s=%s";

	UINT32 count = m_name_value_queries.GetCount();
	for (UINT32 index = 0; index < count; index++)
	{
		ParameterItem *item = m_name_value_queries.Get(index);
		const char *value = item->value.HasContent() ? item->value.CStr() : "\0"; // in case of NULL

		// The realm if defined in auth header, and oauth_signature is excluded from signature strings
		if (!(item->type == ParameterItem::OAUTH_AUTHORIZATION_PARAMETER && item->name.CompareI("realm") == 0) && item->name.CompareI("oauth_signature") != 0)
			RETURN_IF_ERROR(normalized_request_parameters.AppendFormat(first ? format_string + 1 : format_string,  item->name.CStr(), value));

		first = FALSE;
	}
	return OpStatus::OK;
}

OP_STATUS OauthParameterHandler::GetUnsignedAuthorizationHeaderValue(OpString8 &authorization_header_value)
{
	if (!m_sorted)
		m_name_value_queries.QSort(&CompareQueryItems);

	m_sorted = TRUE;

	UINT32 count = m_name_value_queries.GetCount();
	for (UINT32 index = 0; index < count; index++)
	{
		ParameterItem *item = m_name_value_queries.Get(index);
		const char *value = item->value.HasContent() ? item->value.CStr() : "\0"; // in case of NULL

		if (item->type == ParameterItem::OAUTH_AUTHORIZATION_PARAMETER && item->name.CompareI("oauth_signature") != 0)
			RETURN_IF_ERROR(authorization_header_value.AppendFormat("%s=\"%s\",",  item->name.CStr(), value));
	}
	return OpStatus::OK;
}

ParameterItem *OauthParameterHandler::FindParameter(const char *name, int name_length, ParameterItem::Type type, UINT32 &from_index)
{
	UINT32 count = m_name_value_queries.GetCount();

	if (from_index >= count)
		return NULL;

	for (unsigned index = from_index; index < count; index++)
	{
		ParameterItem *found_item = m_name_value_queries.Get(index);

		if (found_item->type == type && found_item->name.Compare(name, name_length) == 0)
		{
			from_index = index;
			return found_item;
		}
	}
	return NULL;
}
/*static */ OauthSignatureGenerator *OauthSignatureGenerator::Create()
{
	return OauthSignatureGeneratorImpl::Create();
}

/*static */ OauthSignatureGeneratorImpl *OauthSignatureGeneratorImpl::Create(BOOL auto_set_nonce_and_timestamp, BOOL set_standard_auth_parameters)
{
	OauthSignatureGeneratorImpl *generator = OP_NEW(OauthSignatureGeneratorImpl, (auto_set_nonce_and_timestamp));
	if (!generator || (set_standard_auth_parameters && OpStatus::IsError(generator->SetStandardAuthHeaderParameters())))
	{
		OP_DELETE(generator);
		return NULL;
	}

	return generator;
}

void OauthSignatureGeneratorImpl::Clear()
{
	m_parameter_handler.Clear();
	m_oauth_auth_token_secret.Wipe();
	m_client_secret.Wipe();
}

OP_STATUS OauthSignatureGeneratorImpl::SetStandardAuthHeaderParameters()
{
	return m_parameter_handler.SetStandardAuthHeaderParameters();
}

OP_STATUS OauthSignatureGeneratorImpl::SignUrl(URL &url, HTTP_Method http_method, const char *realm)
{
	OpString8 signed_authorization_header_value;
	RETURN_IF_ERROR(GetSignedAuthorizationHeaderValue(signed_authorization_header_value, url, http_method));

	URL_Custom_Header header_item;
	header_item.bypass_security = TRUE;

	RETURN_IF_ERROR(header_item.name.Set("Authorization"));

	RETURN_IF_ERROR(header_item.value.AppendFormat("OAuth realm=\"%s\", ", realm ? realm : ""));
	RETURN_IF_ERROR(header_item.value.Append(signed_authorization_header_value.CStr()));
	return url.SetAttribute(URL::KAddHTTPHeader, &header_item);
}

OP_STATUS OauthSignatureGeneratorImpl::GetSignedAuthorizationHeaderValue(OpString8 &signed_authorization_header_value, URL &url, HTTP_Method http_method)
{
	OpString8 signature;
	RETURN_IF_ERROR(CreateOauthSignature(signature, url, http_method));

	OpString8 encoded_signature;
	RETURN_IF_ERROR(OauthParameterHandler::SimpleOauthPercentEscape(signature, signature.Length(), encoded_signature));

	RETURN_IF_ERROR(m_parameter_handler.GetUnsignedAuthorizationHeaderValue(signed_authorization_header_value));

	return signed_authorization_header_value.AppendFormat("oauth_signature=\"%s\"", encoded_signature.CStr());
}

OP_STATUS OauthSignatureGeneratorImpl::CreateOauthSignature(OpString8 &signature, URL &url, HTTP_Method http_method)
{
	UINT32 index = 0;
	if (m_oauth_auth_token_secret.IsEmpty() || !m_parameter_handler.FindParameter("oauth_token", 11, ParameterItem::OAUTH_AUTHORIZATION_PARAMETER, index))
		return OpStatus::ERR_NO_ACCESS;

	OpString8 key;
	RETURN_IF_ERROR(CreateMacKey(key));
	return CreateAndDigestSignatureBaseString(signature, url, http_method, reinterpret_cast<const UINT8*>(key.CStr()), key.Length());
}

OP_STATUS OauthSignatureGeneratorImpl::CreateRequestTokenString(OpString8 &request_string, const OpStringC8 &username, const OpStringC8 &password)
{
	request_string.Wipe();
	int escape_flags = UriEscape::AllUnsafe | UriEscape::QueryUnsafe;
	OpString8 escaped_username_utf8;
	RETURN_OOM_IF_NULL(escaped_username_utf8.Reserve(UriEscape::GetEscapedLength(username.CStr(), escape_flags) + 1));

	OpString8 escaped_password_utf8;
	RETURN_OOM_IF_NULL(escaped_password_utf8.Reserve(UriEscape::GetEscapedLength(password.CStr(), escape_flags) + 1));

	UriEscape::Escape(escaped_username_utf8.CStr(), username.CStr(), escape_flags);
	UriEscape::Escape(escaped_password_utf8.CStr(), password.CStr(), escape_flags);

	UINT32 from_index = 0;
	ParameterItem *oauth_consumer_key = m_parameter_handler.FindParameter("oauth_consumer_key", 18, ParameterItem::OAUTH_AUTHORIZATION_PARAMETER, from_index);
	if (!oauth_consumer_key)
		return OpStatus::ERR_NO_ACCESS;

	return request_string.AppendFormat("x_username=%s&x_password=%s&x_consumer_key=%s", escaped_username_utf8.CStr(), escaped_password_utf8.CStr(), oauth_consumer_key->value.CStr());
}

OP_STATUS OauthSignatureGeneratorImpl::CreateMacKey(OpString8 &key)
{
	if (m_client_secret.HasContent())
	{
		RETURN_IF_ERROR(OauthParameterHandler::SimpleOauthPercentEscape(m_client_secret.CStr(), m_client_secret.Length(), key));
		RETURN_IF_ERROR(key.Append("&"));
	}
	else
		return OpStatus::ERR_NO_ACCESS;

	OpString8 escaped_token_secret;
	RETURN_IF_ERROR(OauthParameterHandler::SimpleOauthPercentEscape(m_oauth_auth_token_secret.CStr(), m_oauth_auth_token_secret.Length(), escaped_token_secret));

	return key.Append(escaped_token_secret.CStr());
}

OP_STATUS OauthSignatureGeneratorImpl::CreateAndDigestSignatureBaseString(OpString8 &digest_base_64, URL &url, HTTP_Method http_method, const UINT8 *key, int key_length)
{
#ifdef SELFTEST
	m_signature_base_string.Wipe();
#endif // SELFTEST


	OpAutoPtr<CryptoHMAC> hmac_sha1(CryptoHMAC::CreateHMAC(CryptoHash::CreateSHA1(), key, key_length));
	if (!hmac_sha1.get())
		return OpStatus::ERR_NO_MEMORY;

	hmac_sha1->InitHash();

	/* method */
	OpString8 http_method_string;
	RETURN_IF_ERROR(http_method_string.Set(GetHTTPMethodString(http_method)));
	op_strupr(http_method_string.CStr()); // Must be uppercase. Make upper just in case.

#ifdef SELFTEST
	RETURN_IF_ERROR(m_signature_base_string.AppendFormat("%s&", http_method_string.CStr()));
#endif // SELFTEST

	hmac_sha1->CalculateHash(http_method_string.CStr());
	hmac_sha1->CalculateHash("&");

	/* base string URI */
	OpString8 base_string_uri;
	RETURN_IF_ERROR (url.GetAttribute (URL::KName, base_string_uri));
	int query_start = base_string_uri.FindFirstOf('?');
	if (query_start != KNotFound)
		base_string_uri[query_start] = '\0';

	OpString8 base_string_uri_encoded;
	RETURN_IF_ERROR(OauthParameterHandler::SimpleOauthPercentEscape(base_string_uri.CStr(), base_string_uri.Length(), base_string_uri_encoded));

#ifdef SELFTEST
	RETURN_IF_ERROR(m_signature_base_string.AppendFormat("%s&", base_string_uri_encoded.CStr()));
#endif // SELFTEST

	hmac_sha1->CalculateHash(base_string_uri_encoded.CStr());
	hmac_sha1->CalculateHash("&");

	/* parameters */
	if (m_auto_set_nonce)
		RETURN_IF_ERROR(m_parameter_handler.SetNonceAndTimeStamp());

	RETURN_IF_ERROR(m_parameter_handler.CollectAndAddQueryParamentersFromUrl(url));

	OpString8 base_string;
	RETURN_IF_ERROR(m_parameter_handler.GetNormalizedRequestParameterString(base_string));

	OpString8 base_string_encoded;
	RETURN_IF_ERROR(OauthParameterHandler::SimpleOauthPercentEscape(base_string.CStr(), base_string.Length(), base_string_encoded));

#ifdef SELFTEST
	RETURN_IF_ERROR(m_signature_base_string.AppendFormat("%s", base_string_encoded.CStr()));
#endif // SELFTEST

	hmac_sha1->CalculateHash(base_string_encoded.CStr());

	/* create digest */
	UINT8 *digest = OP_NEWA(UINT8, hmac_sha1->Size());
	ANCHOR_ARRAY(UINT8, digest);
	RETURN_OOM_IF_NULL(digest);

	hmac_sha1->ExtractHash(digest);
	return CryptoUtility::ConvertToBase64(digest, hmac_sha1->Size(), digest_base_64);
}

/* static */ void OperaOauthManager::CreateL(OperaOauthManager*& operaAccountManager, BOOL set_default_client_secrets)
{
	OperaOauthManagerImpl *impl = NULL;
	OperaOauthManagerImpl::CreateL(impl, set_default_client_secrets);
	OpAutoPtr<OperaOauthManagerImpl> impl_anchor(impl);
	operaAccountManager = impl_anchor.release();
}

/* static */ void OperaOauthManagerImpl::CreateL(OperaOauthManagerImpl*& opera_oauth_account_manager, BOOL set_default_client_credentials)
{
	LEAVE_IF_NULL(opera_oauth_account_manager = OP_NEW(OperaOauthManagerImpl, (set_default_client_credentials)));
	opera_oauth_account_manager->m_oauth_signature_generator.reset(OauthSignatureGeneratorImpl::Create());
	LEAVE_IF_NULL(opera_oauth_account_manager->m_oauth_signature_generator.get());
	LEAVE_IF_ERROR(opera_oauth_account_manager->m_oauth_signature_generator->SetStandardAuthHeaderParameters());
	LEAVE_IF_ERROR(opera_oauth_account_manager->m_access_token_uri.SetUTF8FromUTF16(g_pcopera_account->GetStringPref(PrefsCollectionOperaAccount::OAuthOperaAccountAccessTokenURI)));
	LEAVE_IF_ERROR(opera_oauth_account_manager->Construct(g_main_message_handler));
}

OP_STATUS OperaOauthManagerImpl::Login(const OpStringC& username, const OpStringC& password)
{
	if (m_is_in_progress)
		return OpStatus::OK;

	m_timeout_handler.Stop();
	if (m_set_default_client_credentials)
	{
#ifdef PREFS_HAVE_OPERA_ACCOUNT

		OpString8 secret;
		OpString8 consumer_key;
		if (g_pcopera_account)
		{
			RETURN_IF_ERROR(secret.SetUTF8FromUTF16(g_pcopera_account->GetStringPref(PrefsCollectionOperaAccount::OAuthOperaAccountConsumerSecret)));
			RETURN_IF_ERROR(consumer_key.SetUTF8FromUTF16(g_pcopera_account->GetStringPref(PrefsCollectionOperaAccount::OAuthOperaAccountConsumerKey)));

		}
		else
#endif // PREFS_HAVE_OPERA_ACCOUNT
		{
			RETURN_IF_ERROR(secret.SetUTF8FromUTF16(OAUTH_OPERA_ACCOUNT_CONSUMER_SECRET_DEFAULT));
			RETURN_IF_ERROR(consumer_key.SetUTF8FromUTF16(OAUTH_OPERA_ACCOUNT_CONSUMER_KEY_DEFAULT));
		}

		RETURN_IF_ERROR(SetClientSecret(secret.CStr()));
		RETURN_IF_ERROR(SetClientIdentityKey(consumer_key.CStr()));
	}

	URL reference;
	m_access_token_url = g_url_api->GetURL(reference, m_access_token_uri.CStr(), static_cast<const char *>(NULL), TRUE);

	m_access_token_url.SetHTTP_Method(HTTP_METHOD_POST);

	OpAutoPtr<Upload_BinaryBuffer> upload(OP_NEW(Upload_BinaryBuffer, ()));
	RETURN_OOM_IF_NULL(upload.get());

	OpString8 post_string_utf8;
	RETURN_IF_ERROR(GetRequestTokenString(post_string_utf8, username, password));

	RETURN_IF_LEAVE(upload->InitL(reinterpret_cast<unsigned char*>(post_string_utf8.CStr()), post_string_utf8.Length(), UPLOAD_COPY_BUFFER, "application/x-www-form-urlencoded"));
	RETURN_IF_LEAVE(upload->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION));

	m_access_token_url.SetHTTP_Data(upload.get(), TRUE);
	upload.release();

	URL_LoadPolicy loadpolicy(FALSE, URL_Reload_Full, FALSE);

	URL referrer;
	RETURN_IF_ERROR(LoadDocument(m_access_token_url, referrer, loadpolicy));

	m_timeout_handler.Start(3*60*1000);
	m_is_in_progress = TRUE;
	return OpStatus::OK;
}

void OperaOauthManagerImpl::Logout()
{
	StopLoading(m_access_token_url);
	ClearUserCredentials();
	BroadCastOnLoginStatusChange(OAUTH_LOGGED_OUT, UNI_L(""));
}

OP_STATUS OperaOauthManagerImpl::OauthSignUrl(URL &url, HTTP_Method http_method, const char *realm)
{
	if (IsLoggedIn())
		return m_oauth_signature_generator->SignUrl(url, http_method, realm);
	else
		return OpStatus::ERR_NO_ACCESS;
}

void OperaOauthManagerImpl::OnURLLoadingFailed(URL &url, Str::LocaleString error, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	LoadingStopped();
	m_logged_in = FALSE;
	BroadCastOnLoginStatusChange(OAUTH_LOGIN_ERROR_NETWORK, UNI_L(""));
}

void OperaOauthManagerImpl::OnURLLoadingStopped(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	LoadingStopped();
	m_logged_in = FALSE;
}

void OperaOauthManagerImpl::OnURLLoadingFinished(URL &url)
{
	if (m_is_in_progress && url.Status(TRUE) != URL_LOADED)
	{
		// Send event if OnURLLoadingFailed has not been received for some reason.
		BroadCastOnLoginStatusChange(OAUTH_LOGIN_ERROR_NETWORK, UNI_L(""));
		return;
	}

	LoadingStopped();
	ClearUserCredentials();

	do
	{
		OpAutoPtr<URL_DataDescriptor> stored_desc;
		stored_desc.reset(url.GetDescriptor(g_main_message_handler, TRUE, TRUE, TRUE));
		if (!stored_desc.get())
			break;

		BOOL more;
		TRAPD(err, stored_desc->RetrieveDataL(more));
		if (OpStatus::IsError(err))
			break;

		OauthParameterHandler parameters;
		if (OpStatus::IsError(parameters.Parse(stored_desc->GetBuffer(), stored_desc->GetBufSize())))
			break;

		UINT32 from_index = 0;

		ParameterItem *error_token = parameters.FindParameter("error", 5, ParameterItem::REQUEST_PARAMETER, from_index = 0);

		if (error_token)
		{
			OpString server_message;
			if (server_message.Reserve(error_token->value.Length() + 1) == NULL)
				break;

			UriUnescape::Unescape(server_message.CStr(), error_token->value, error_token->value.Length(), UriUnescape::ConvertUtf8 | UriUnescape::ExceptCtrl | UriUnescape::TranslatePlusToSpace);
			BroadCastOnLoginStatusChange(OAUTH_LOGIN_ERROR_WRONG_USERNAME_PASSWORD, server_message);
			return;
		}
		else
		{
			ParameterItem *oauth_token = parameters.FindParameter("oauth_token", 11, ParameterItem::REQUEST_PARAMETER, from_index = 0);
			if (!oauth_token  || OpStatus::IsError(m_oauth_signature_generator->SetToken(oauth_token->value.CStr())))
				break;

			ParameterItem *oauth_token_secret = parameters.FindParameter("oauth_token_secret", 18, ParameterItem::REQUEST_PARAMETER, from_index = 0);
			if (!oauth_token_secret || OpStatus::IsError(m_oauth_signature_generator->SetTokenSecret(oauth_token_secret->value.CStr())))
				break;

			m_logged_in = TRUE;

			BroadCastOnLoginStatusChange(OAUTH_LOGIN_SUCCESS_NEW_TOKEN_DOWNLOADED, UNI_L(""));
			return;
		}
	} while(0);

	BroadCastOnLoginStatusChange(OAUTH_LOGIN_ERROR_GENERIC, UNI_L(""));
}

void OperaOauthManagerImpl::ClearUserCredentials()
{
	m_logged_in = FALSE;
	m_oauth_signature_generator->SetTokenSecret("");
	m_oauth_signature_generator->SetToken("");
}

void OperaOauthManagerImpl::LoadingStopped()
{
	m_is_in_progress = FALSE;
	m_timeout_handler.Stop();
}

void OperaOauthManagerImpl::OnTimeOut(OpTimer* timer)
{
	StopLoading(m_access_token_url);
	LoadingStopped();
	BroadCastOnLoginStatusChange(OAUTH_LOGIN_ERROR_TIMEOUT, UNI_L(""));
}

void OperaOauthManagerImpl::BroadCastOnLoginStatusChange(OAuthStatus status, const OpStringC &server_message)
{
	UINT32 count = m_listeners.GetCount();
	for (unsigned idx = 0; idx < count; idx ++)
		m_listeners.Get(idx)->OnLoginStatusChange(status, server_message);
}

OP_STATUS OperaOauthManagerImpl::GetRequestTokenString(OpString8& post_string_utf8, const OpStringC& username, const OpStringC& password)
{
	OpString8 username_utf8;
	RETURN_IF_ERROR(username_utf8.SetUTF8FromUTF16(username));

	OpString8 password_utf8;
	RETURN_IF_ERROR(password_utf8.SetUTF8FromUTF16(password));

	return m_oauth_signature_generator->CreateRequestTokenString(post_string_utf8, username_utf8, password_utf8);
}

#endif // OPERA_AUTH_SUPPORT
