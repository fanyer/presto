/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef TRUST_RATING

#include "modules/dochand/fraud_check.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"

#include "modules/libssl/sslv3.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#include "modules/util/tempbuf.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/pi/OpSystemInfo.h"

#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmlnames.h"

#include "modules/regexp/include/regexp_api.h"

#include "modules/about/operafraudwarning.h"
#include "modules/url/protocols/comm.h"

#include "modules/forms/tempbuffer8.h"

#ifdef CRYPTO_HASH_MD5_SUPPORT
#include "modules/libcrypto/include/CryptoHash.h"
#endif // CRYPTO_HASH_MD5_SUPPORT

#ifdef URL_FILTER
#  include "modules/content_filter/content_filter.h"
#endif //URL_FILTER

#include "modules/formats/encoder.h"

#define SITECHECK_LOAD_TIMEOUT (30 * 1000)   // 30 seconds

/***************************************************
 *
 *  ServerTrustChecker
 *
 ***************************************************/

ServerTrustChecker::ServerTrustChecker(UINT id, DocumentManager* docman)
	: m_id(id), m_parser(NULL), m_server_name(NULL), m_docman(docman)
{
}

ServerTrustChecker::~ServerTrustChecker()
{
	OP_DELETE(m_parser);
}

OP_STATUS ServerTrustChecker::Init(URL& url)
{
    m_server_name = const_cast<ServerName *>(static_cast<const ServerName *>(url.GetAttribute(URL::KServerName, NULL)));

    if (!m_server_name)
		return OpStatus::ERR_NULL_POINTER;

	m_initial_url = url;
	RETURN_IF_ERROR(AddURL(url));

	return OpStatus::OK;
}

OP_STATUS ServerTrustChecker::AddURL(URL& url)
{
	OP_ASSERT(URLBelongsToThisServer(url));
	OP_ASSERT(!IsCheckingURL(url));

	URLCheck* new_check = OP_NEW(URLCheck, ());
	if (!new_check)
		return OpStatus::ERR_NO_MEMORY;

	new_check->url = url;
	new_check->Into(&m_checking_urls);

	return OpStatus::OK;
}

BOOL ServerTrustChecker::URLBelongsToThisServer(URL& url)
{
	return m_initial_url.SameServer(url);
}

BOOL ServerTrustChecker::IsCheckingURL(URL& url)
{
	for (URLCheck* c = (URLCheck*)m_checking_urls.First(); c; c = (URLCheck*)c->Suc())
		if (c->url == url)
			return TRUE;

	return FALSE;
}

OP_STATUS ServerTrustChecker::StartCheck(BOOL resolve_first)
{
	if (!g_fraud_check_request_throttler.AllowRequestNow())
		return CheckDone(FALSE);

	m_parser = OP_NEW(TrustInfoParser, (this, g_main_message_handler));
	if (!m_parser)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = m_parser->Init();

	if (status == OpStatus::OK)
	{
		if (resolve_first)
			status = m_parser->ResolveThenCheckURL(m_initial_url, NULL);
		else
			status = m_parser->CheckURL(m_initial_url, NULL);
	}

	if (status != OpStatus::OK)
	{
		OP_DELETE(m_parser);
		m_parser = NULL;
	}

	return status;
}

static OP_STATUS AddAdvisoryInfoForServerName(ServerName* server_name, const uni_char *homepage_url, const uni_char *advisory_url, const uni_char *text, unsigned int src, unsigned int type)
{
	// FIXME: Rename the function below with an L suffix since it leaves
	TRAPD(status, server_name->AddAdvisoryInfo(homepage_url, advisory_url, text, src, type));
	return status;
}

OP_STATUS ServerTrustChecker::CheckDone(BOOL check_successful)
{
	if (!m_docman->GetMessageHandler()->PostMessage(MSG_TRUST_CHECK_DONE, m_id, check_successful))
		return OpStatus::ERR_NO_MEMORY;

	if (check_successful)
	{
		URLCheck* check_url = (URLCheck*)m_checking_urls.First();
		OP_ASSERT(check_url);

		if (!check_url)
			return OpStatus::ERR_NULL_POINTER;

		m_server_name->SetTrustRating(Unknown_Trust); // all checked servers have a unknown rating in version 1.1
		URL current_url = m_docman->GetCurrentURL();

		if (!m_server_name->FraudListIsEmpty() || m_server_name->GetFirstRegExpItem() || URLBelongsToThisServer(current_url))
		{
			for (; check_url; check_url = (URLCheck*)check_url->Suc())
			{
				ServerName* server_name = check_url->url.GetServerName();
				if (server_name)
				{
					OpString url_string;
                    const uni_char *matching_text = NULL;

                    TrustInfoParser::Advisory *advisory = static_cast<TrustInfoParser::Advisory*>(m_parser->GetAdvisoryList().First());
                    for (; advisory; advisory = static_cast<TrustInfoParser::Advisory*>(advisory->Suc()))
						RETURN_IF_ERROR(AddAdvisoryInfoForServerName(server_name, advisory->homepage_url, advisory->advisory_url, advisory->text, advisory->id, advisory->type));

					if (current_url == check_url->url)
					{
						RETURN_IF_ERROR(check_url->url.GetAttribute(URL::KUniName, url_string));
						OP_STATUS status = TrustInfoParser::MatchesUntrustedURL(url_string.CStr(), server_name, &matching_text);
						RETURN_IF_MEMORY_ERROR(status);
						if (status == OpBoolean::IS_TRUE)
						{
							TrustInfoParser::Advisory *elm = m_parser->GetAdvisory(matching_text);
							TrustRating rating = (elm->type == SERVER_SIDE_FRAUD_MALWARE) ? Malware
								: (elm->type == SERVER_SIDE_FRAUD_PHISHING) ? Phishing
								: Unknown_Trust;
							m_docman->GetWindow()->SetTrustRating(rating);
							return m_docman->GenerateAndShowFraudWarningPage(current_url, elm);
						}
						else if (m_docman->GetWindow())
							m_docman->GetWindow()->SetTrustRating(m_server_name->GetTrustRating());
					}
				}
			}
		}
	}
	else
		// Make sure we'll not end up with rating Not_Set as it means the rating hasn't been
		// got from the server *yet* which implies it'll be set to something different at some point.
		m_docman->GetWindow()->SetTrustRating(Unknown_Trust);

	return OpStatus::OK;
}

/* static */
OP_STATUS ServerTrustChecker::GetTrustRating(URL& url, TrustRating& rating, BOOL& needs_online_check)
{
	rating = Not_Set;
	needs_online_check = FALSE;

	ServerName *server_name = (ServerName *) url.GetAttribute(URL::KServerName, NULL);
	if (server_name
		&& (url.Type() == URL_HTTP || url.Type() == URL_HTTPS || url.Type() == URL_FTP))
	{
		rating = server_name->GetTrustRating();  // only check's if we've already gotten a reply for this server

		if (rating != Not_Set)  // this server has had the blacklists downloaded
		{
			OpString url_string;
			RETURN_IF_ERROR(url.GetAttribute(URL::KUniName, url_string));

			OP_STATUS status = TrustInfoParser::MatchesUntrustedURL(url_string.CStr(), server_name);
			RETURN_IF_MEMORY_ERROR(status);
			if (status == OpBoolean::IS_TRUE)
				rating = Untrusted_Ask_Advisory;

			return OpStatus::OK;
		}

		if (server_name->GetTrustRatingBypassed())
			return OpStatus::OK;

		// The site check host itself is always trusted
		if (op_strcmp(server_name->Name(), SITECHECK_HOST) == 0)
		{
			rating = Unknown_Trust;  // only use trusted rating in version 1
			return OpStatus::OK;
		}
		else
		{
			needs_online_check = TRUE;
			return OpStatus::OK;
		}
	}
	else if (url.Type() == URL_OPERA && url.GetAttribute(URL::KIsGeneratedByOpera))
		rating = Domain_Trusted;

	return OpStatus::OK;
}

/* static */
OP_STATUS ServerTrustChecker::IsLocalURL(URL& url, BOOL& is_local, BOOL& need_to_resolve)
{
	is_local = FALSE;
	need_to_resolve = FALSE;

	if (ServerName *server_name = (ServerName *) url.GetAttribute(URL::KServerName, NULL))
		if (OpSocketAddress* socket_address = server_name->SocketAddress())
		{
			if (socket_address->GetNetType() == NETTYPE_LOCALHOST || socket_address->GetNetType() == NETTYPE_PRIVATE)
				is_local = TRUE;
			else if (socket_address->GetNetType() == NETTYPE_UNDETERMINED && !url.GetAttribute(URL::KUseProxy))
				need_to_resolve = TRUE;
		}

	return OpStatus::OK;
}

/***************************************************
 *
 *  TrustInfoParser
 *
 ***************************************************/

TrustInfoParser::TrustInfoParser(ServerTrustChecker* checker, MessageHandler* mh) :
	m_checker(checker), m_xml_parser(NULL), m_current_element(OtherElement),
	m_message_handler(mh), m_current_advisory(NULL), m_current_url(NULL)
{
	OP_ASSERT(mh);
}

TrustInfoParser::~TrustInfoParser()
{
	m_message_handler->UnsetCallBacks(this);

	OP_DELETE(m_xml_parser);

	UrlListItem *item = static_cast<UrlListItem*>(m_url_list.First());
	while (item)
	{
		UrlListItem *tmp = item;
		item = static_cast<UrlListItem*>(item->Suc());
		tmp->Out();
		OP_DELETE(tmp);
	}

	Advisory *advisory = static_cast<Advisory*>(m_advisory_list.First());
	while (advisory)
	{
		Advisory *tmp = advisory;
		advisory = static_cast<Advisory*>(advisory->Suc());
		tmp->Out();
		OP_DELETE(tmp);
	}
}

OP_STATUS TrustInfoParser::Init()
{
	return OpStatus::OK;
}


char* TrustInfoParser::EscapePluses(char* string)
{
	if (!string)
		return NULL;

	UINT plus_count = 0, length = 0;
	char* src = string;

	while (*src)
	{
		if (*(src++) == '+')
			plus_count++;

		length++;
	}

	// No escapes needed, return original string instead of deleting it
	if (plus_count == 0)
		return string;

	char* result = OP_NEWA(char, length + (2 * plus_count) + 1);  // each plus is replaced by a three char escape code i.e. two more chars

	if (!result)
	{
		OP_DELETEA(string);
		return NULL;
	}

	char* dest = result;
	for (src = string; *src; src++)
	{
		if (*src == '+')
		{ // escape code for + is %2B
			*(dest++) = '%';
			*(dest++) = '2';
			*(dest++) = 'B';
		}
		else
			*(dest++) = *src;
	}

	*dest = '\0';
	OP_DELETEA(string);

	return result;
}

/* static */
OP_BOOLEAN TrustInfoParser::RegExpMatchUrl(const uni_char* reg_exp_string, const uni_char* url)
{
	OpStackAutoPtr<OpRegExp> reg_exp(NULL);
	OpREFlags re_flags;
	OpREMatchLoc match;

	re_flags.multi_line = FALSE;
	re_flags.case_insensitive = TRUE;
	re_flags.ignore_whitespace = FALSE;

	OpRegExp* tmp_reg_exp;
	RETURN_IF_ERROR(OpRegExp::CreateRegExp(&tmp_reg_exp, reg_exp_string, &re_flags));
	reg_exp.reset(tmp_reg_exp);

	RETURN_IF_ERROR(reg_exp->Match(url, &match));
	if (match.matchloc != REGEXP_NO_MATCH)
		return OpBoolean::IS_TRUE;

	return OpBoolean::IS_FALSE;
}

/* static */
OP_BOOLEAN TrustInfoParser::MatchesUntrustedURL(const uni_char* url_string, ServerName* server_name, const uni_char **matching_text
    )
{
	// First check if the specific URL is blocked
	if (server_name->IsUrlInFraudList(url_string, matching_text))
		return OpBoolean::IS_TRUE;

	// check if url matches any of the reg-exps
	for (FraudListItem* regexp = server_name->GetFirstRegExpItem(); regexp; regexp = (FraudListItem*)regexp->Suc())
	{
		OP_STATUS status = RegExpMatchUrl(regexp->value.CStr(), url_string);
		RETURN_IF_MEMORY_ERROR(status);
		if (status == OpBoolean::IS_TRUE)
		{
			if (matching_text)
				*matching_text = regexp->value.CStr();
			return OpBoolean::IS_TRUE;
		}
	}

	return OpBoolean::IS_FALSE;
}

/* static */
OP_STATUS TrustInfoParser::CalculateMD5Hash(OpString& input, char*& md5hash, BOOL url_escape_hash)
{
	// Calculate MD5 from utf-8 encoding of input
	OpString8 buffer;
	RETURN_IF_ERROR(buffer.SetUTF8FromUTF16(input.CStr()));

	return CalculateMD5Hash(buffer.CStr(), md5hash, url_escape_hash);
}

/* static */
OP_STATUS TrustInfoParser::CalculateMD5Hash(const char* buffer, char*& md5hash, BOOL url_escape_hash)
{
	md5hash = NULL;
#ifdef CRYPTO_HASH_MD5_SUPPORT
	OpAutoPtr<CryptoHash> hasher(CryptoHash::CreateMD5());
	if (!hasher.get())
		return OpStatus::ERR_NO_MEMORY;
#else
	SSL_Hash_Pointer hasher(SSL_MD5);
#endif // CRYPTO_HASH_MD5_SUPPORT
	
	hasher->InitHash();

    hasher->CalculateHash(buffer);
    // 128bit MD5 hash == 16 bytes
	unsigned char digest[16]; /* ARRAY OK 2008-02-28 jl */
	hasher->ExtractHash((unsigned char *) digest);

	// Base64 encode the MD5-sum
	char* temp_buf = NULL;
	int targetlen = 0;
	// MIME_Encode_SetStr allocates a buffer of appropriate size for temp_buf, has to be deallocated
	MIME_Encode_Error err = MIME_Encode_SetStr(temp_buf, targetlen, (char*)digest, (int)sizeof(digest), NULL, GEN_BASE64_ONELINE);

	if (err == MIME_NO_ERROR)
	{
		if (url_escape_hash)
			md5hash = EscapePluses(temp_buf);
		else
			md5hash = temp_buf;

		if (md5hash)
			return OpStatus::OK;
		else
			return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		OP_DELETEA(temp_buf);

		return OpStatus::ERR;
	}
}

/* static */
OP_STATUS TrustInfoParser::GetNormalizedHostname(URL& url, OpString8& hostname)
{
	RETURN_IF_ERROR(hostname.Set(url.GetAttribute(URL::KHostName)));

	// trim trailing dots in server name
	int length = hostname.Length();
	int end = length;
	while (end > 0 && hostname[end - 1] == '.')
		end--;
	if (end < length)
		hostname.Delete(end);

	return OpStatus::OK;
}

/* static */
OP_STATUS TrustInfoParser::NormalizeURL(URL& url, OpString8& normalized_url8, OpString8& hostname)
{
	/* Steps for normalizing and hashing URL
	 * + url decode (if applicable)
	 * + remove trailing . from server name
	 * + lower case
	 * + strip off protocol
	 * + trim off parameters(?,#,;)
	 * + trim off trailing '/'s */

     /* arneh, 20061002 - yes, point 5 won't really strip parameters at
     * that point, as after decoding we can't really tell what's parameters
     * and not.  We just strip whatever still looks like parameters after
     * decoding.  This is correct according to Geotrust.
     *
     * This might be to guard agains servers (like Apache) which interpret %3F
     * as a parameter delimiter if the path would otherwise return a 404.
     */

	RETURN_IF_ERROR(GetNormalizedHostname(url, hostname));
	if (hostname.IsEmpty())
		return OpStatus::ERR;

	// Get decoded path
	OpString path;
	RETURN_IF_ERROR(path.Set(url.GetAttribute(URL::KUniPath)));

	// trim off parameters (?,;) and trailing /
	int length = path.Length();
	int end = length;
	int param_start = path.FindFirstOf(OpStringC16(UNI_L(";?")));
	if (param_start != KNotFound)
		end = param_start;

	while (end > 0 && path[end - 1] == '/')
		end--;

	TempBuffer path_buffer;
	RETURN_IF_ERROR(path_buffer.Expand(end + 1));

	// collapse consecutive slashes - fix for bug #244961
	BOOL last_was_slash = FALSE;
	for (int i = 0; i < end; i++)
	{
		OP_ASSERT(path[i]);

		if (path[i] == '/')
		{
			if (last_was_slash)
				continue;
			else
				last_was_slash = TRUE;
		}
		else
			last_was_slash = FALSE;

		path_buffer.Append(path[i]);
	}

	OpString normalized_url16;
	RETURN_IF_ERROR(normalized_url16.Append(hostname));

	// have to include port number if it is non-standard one
	UINT32 port = url.GetAttribute(URL::KServerPort);
	if (port)   // port != 0 is non-standard
		RETURN_IF_ERROR(normalized_url16.AppendFormat(UNI_L(":%d"), port));

	RETURN_IF_ERROR(normalized_url16.Append(path_buffer.GetStorage()));
	normalized_url16.MakeLower();

	// Calculate MD5 from utf-8 encoding of input
	RETURN_IF_ERROR(normalized_url8.SetUTF8FromUTF16(normalized_url16.CStr()));

	return OpStatus::OK;
}

OP_STATUS TrustInfoParser::GenerateURLHash(URL& url, OpString8& hash)
{
	OpString8 normalized_url;
	OpString8 hostname;
	char* cstr_hash = NULL;

	RETURN_IF_ERROR(NormalizeURL(url, normalized_url, hostname));
	RETURN_IF_ERROR(CalculateMD5Hash(normalized_url.CStr(), cstr_hash, FALSE));

	hash.TakeOver(cstr_hash);

	return OpStatus::OK;
}

/* static */
OP_STATUS TrustInfoParser::GenerateRequestURL(URL& url, URL& request_url, BOOL request_info_page, const char* full_path_md5)
{
	OpString8 hostname;
	OpString8 str_full_path_md5;

	RETURN_IF_ERROR(GetNormalizedHostname(url, hostname));

	// Take MD5 of hdn value
	OpString8 hdn_value;
	RETURN_IF_ERROR(hdn_value.Set(hostname));
	RETURN_IF_ERROR(hdn_value.Append(SITECHECK_HDN_SUFFIX));

	char* hdn_md5 = NULL;
	RETURN_IF_ERROR(CalculateMD5Hash(hdn_value.CStr(), hdn_md5, TRUE));

	OpString8 str_hdn_md5;
	str_hdn_md5.TakeOver(hdn_md5);

	/** Final request URL should look like:
	 *  http://sitecheck.opera.com/?host=$hostname&ph=&hash&hdn=$hdn
	 *  where
	 *    $hostname is the hostname part of the URL
	 *    $hash is a MD5 sum of the normalized URL (see below)  (only used in version 1)
	 *    $hdn is a MD5 sum of hostname with an encryption string concatenated */

	OpString8 request_string;

	if (url.Type() == URL_HTTPS)
		RETURN_IF_ERROR(request_string.AppendFormat("https://%s/", SITECHECK_HOST));
	else
		RETURN_IF_ERROR(request_string.AppendFormat("http://%s/", SITECHECK_HOST));

	if (request_info_page)
		RETURN_IF_ERROR(request_string.Append("info/"));

	RETURN_IF_ERROR(request_string.AppendFormat("?host=%s&hdn=%s", hostname.CStr(), hdn_md5));

	if (request_info_page)
	{
		TempBuffer8 phishing_url;
		RETURN_IF_ERROR(phishing_url.AppendURLEncoded(url.GetAttribute(URL::KName).CStr()));
		RETURN_IF_ERROR(request_string.AppendFormat("&site=%s", phishing_url.GetStorage()));
	}

	request_url = g_url_api->GetURL(url, request_string.CStr());
	request_url.SetAttribute(URL::KBlockUserInteraction, TRUE);
	request_url.SetAttribute(URL::KDisableProcessCookies, TRUE);
#ifdef URL_FILTER
	request_url.SetAttribute(URL::KSkipContentBlocker, TRUE);
#endif // URL_FILTER

	return OpStatus::OK;
}

OP_STATUS TrustInfoParser::ResolveThenCheckURL(URL& url, const char* full_path_hash)
{
	m_host_url = url;
	if (full_path_hash)
		RETURN_IF_ERROR(m_full_path_hash.Set(full_path_hash));

	OpString hostname;
	RETURN_IF_ERROR(hostname.Set(url.GetAttribute(URL::KUniHostName)));

	IPAddress ip_ignored;
	RETURN_IF_ERROR(g_secman_instance->ResolveURL(url, ip_ignored, m_state));
	if (m_state.suspended)
	{
		m_message_handler->SetCallBack(this, MSG_COMM_NAMERESOLVED, m_state.host_resolving_comm->Id());
		m_message_handler->SetCallBack(this, MSG_COMM_LOADING_FAILED, m_state.host_resolving_comm->Id());
	}
	else
		OnHostResolved(url.GetServerName()->SocketAddress());

	return OpStatus::OK;
}

void TrustInfoParser::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(m_state.host_resolving_comm &&
	          par1 == static_cast<MH_PARAM_1>(m_state.host_resolving_comm->Id()) &&
	          (msg == MSG_COMM_LOADING_FAILED || msg == MSG_COMM_NAMERESOLVED));

	m_message_handler->UnsetCallBacks(this);
	m_state.suspended = FALSE;

	if (msg == MSG_COMM_LOADING_FAILED)
		m_checker->CheckDone(FALSE);
	else
		OnHostResolved(m_state.host_resolving_comm->HostName()->SocketAddress());
}

OP_STATUS TrustInfoParser::CheckURL(URL& url, const char* full_path_hash)
{
	m_host_url = url;

	URL request_url;
	RETURN_IF_ERROR(GenerateRequestURL(url, request_url, FALSE, full_path_hash));

	// Make a parser for result
	RETURN_IF_ERROR(XMLParser::Make(m_xml_parser, this,
					m_message_handler, this, request_url));

	XMLParser::Configuration configuration;
	configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
	configuration.max_tokens_per_call = 0;  // unlimited

	m_xml_parser->SetConfiguration(configuration);

	URL referer_url;
	return m_xml_parser->Load(referer_url, TRUE, SITECHECK_LOAD_TIMEOUT);
}

// Callbacks from XML parser
XMLTokenHandler::Result TrustInfoParser::HandleToken(XMLToken &token)
{
	OP_STATUS status = OpStatus::OK;

	switch (token.GetType())
	{
	case XMLToken::TYPE_CDATA :
	case XMLToken::TYPE_Text :
		status = HandleTextToken(token);
		break;
	case XMLToken::TYPE_STag :
		status = HandleStartTagToken(token);
		break;
	case XMLToken::TYPE_ETag :
		status = HandleEndTagToken(token);
		break;
	case XMLToken::TYPE_EmptyElemTag :
		status = HandleStartTagToken(token);
		if (OpStatus::IsSuccess(status))
			status = HandleEndTagToken(token);
		break;
	default:
		break;
	}

	if (OpStatus::IsMemoryError(status))
		return XMLTokenHandler::RESULT_OOM;
	else if (OpStatus::IsError(status))
		return XMLTokenHandler::RESULT_ERROR;

	return XMLTokenHandler::RESULT_OK;
}

OP_STATUS TrustInfoParser::HandleTextToken(XMLToken &token)
{
	ServerName *sn = (ServerName *) m_host_url.GetAttribute(URL::KServerName, NULL);

	if (m_current_element == RegExpElement || m_current_element == UrlElement)
	{
		if (sn)
		{
			OpString value;
			RETURN_IF_ERROR(value.Set(token.GetLiteralSimpleValue(), token.GetLiteralLength()));
			if (m_current_element == UrlElement)
                RETURN_IF_LEAVE(sn->AddUrlL(value, m_current_url ? m_current_url->src : 0));
			else
				RETURN_IF_LEAVE(sn->AddRegExpL(value));

            if (m_current_url)
            {
                m_current_url->url = token.GetLiteralAllocatedValue();
                if (!m_current_url->url)
                    return OpStatus::ERR_NO_MEMORY;
                m_current_url = NULL;
            }
		}
	}
    else if (m_current_element == SourceElement && m_current_advisory)
    {
        m_current_advisory->text = token.GetLiteralAllocatedValue();
        if (!m_current_advisory->text)
            return OpStatus::ERR_NO_MEMORY;
        m_current_advisory = NULL;
    }
	else if (m_current_element == ClientExpireElement)
	{
		if (sn)
		{
			uni_char* value = token.GetLiteralAllocatedValue();
			if (value)
			{
				UINT expire_time = uni_atoi(value);
				sn->SetTrustTTL(expire_time);
				OP_DELETEA(value);
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS TrustInfoParser::HandleStartTagToken(XMLToken &token)
{
	// Fetch information about the element.
	const XMLCompleteNameN &elemname = token.GetName();

	if (uni_strncmp(UNI_L("host"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
		m_current_element = HostElement;
	else if (uni_strncmp(UNI_L("ce"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
		m_current_element = ClientExpireElement;
	else if (uni_strncmp(UNI_L("r"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
    {
        OpAutoPtr<UrlListItem> url_item(OP_NEW(UrlListItem, ()));
        if (!url_item.get())
            return OpStatus::ERR_NO_MEMORY;

        XMLToken::Attribute *xml_attr;
        xml_attr = token.GetAttribute(UNI_L("src"));
        if (xml_attr)
        {
            unsigned len = xml_attr->GetValueLength();
            uni_char *src_string = OP_NEWA(uni_char, len+1);
            if (!src_string)
                return OpStatus::ERR_NO_MEMORY;
            uni_strncpy(src_string, xml_attr->GetValue(), len);
            src_string[len] = 0;

            url_item->src = uni_atoi(src_string);
            OP_DELETEA(src_string);
        }

        m_current_url = url_item.release();
        m_current_url->Into(&m_url_list);
		m_current_element = RegExpElement;
	}
	else if (uni_strncmp(UNI_L("u"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
    {
        OpAutoPtr<UrlListItem> url_item(OP_NEW(UrlListItem, ()));
        if (!url_item.get())
            return OpStatus::ERR_NO_MEMORY;

        XMLToken::Attribute *xml_attr;
        xml_attr = token.GetAttribute(UNI_L("src"));
        if (xml_attr)
        {
            unsigned len = xml_attr->GetValueLength();
            uni_char *src_string = OP_NEWA(uni_char, len+1);
            if (!src_string)
                return OpStatus::ERR_NO_MEMORY;
            uni_strncpy(src_string, xml_attr->GetValue(), len);
            src_string[len] = 0;

            url_item->src = uni_atoi(src_string);
            OP_DELETEA(src_string);
        }

        m_current_url = url_item.release();
        m_current_url->Into(&m_url_list);
		m_current_element = UrlElement;
	}
    else if (uni_strncmp(UNI_L("source"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
    {
        XMLToken::Attribute *xml_attr;
        OpAutoPtr<Advisory> advisory(OP_NEW(Advisory, ()));
        m_current_advisory = advisory.get();
        if (!advisory.get())
            return OpStatus::ERR_NO_MEMORY;

        xml_attr = token.GetAttribute(UNI_L("id"));
        if (xml_attr)
        {
            unsigned len = xml_attr->GetValueLength();
            uni_char *id_string = OP_NEWA(uni_char, len+1);
            if (!id_string)
                return OpStatus::ERR_NO_MEMORY;
            uni_strncpy(id_string, xml_attr->GetValue(), len);
            id_string[len] = 0;

            advisory->id = uni_atoi(id_string);
            OP_DELETEA(id_string);
        }

        xml_attr = token.GetAttribute(UNI_L("type"));
        if (xml_attr)
        {
            unsigned len = xml_attr->GetValueLength();
            uni_char *type_string = OP_NEWA(uni_char, len+1);
            if (!type_string)
                return OpStatus::ERR_NO_MEMORY;
            uni_strncpy(type_string, xml_attr->GetValue(), len);
            type_string[len] = 0;

            advisory->type = uni_atoi(type_string);
            OP_DELETEA(type_string);
        }

        xml_attr = token.GetAttribute(UNI_L("homepage"));
        if (xml_attr)
        {
            unsigned len = xml_attr->GetValueLength();
            uni_char *homepage = OP_NEWA(uni_char, len+1);
            if (!homepage)
                return OpStatus::ERR_NO_MEMORY;
            uni_strncpy(homepage, xml_attr->GetValue(), len);
            homepage[len] = 0;

            advisory->homepage_url = homepage;
        }

        xml_attr = token.GetAttribute(UNI_L("advisory"));
        if (xml_attr)
        {
            unsigned len = xml_attr->GetValueLength();
            uni_char *advisory_url = OP_NEWA(uni_char, len+1);
            if (!advisory_url)
                return OpStatus::ERR_NO_MEMORY;
            uni_strncpy(advisory_url, xml_attr->GetValue(), len);
            advisory_url[len] = 0;

            advisory->advisory_url = advisory_url;
        }

        advisory.release()->Into(&m_advisory_list);
        m_current_element = SourceElement;
    }

	return OpStatus::OK;
}

OP_STATUS TrustInfoParser::HandleEndTagToken(XMLToken &token)
{
	m_current_element = OtherElement;

	return OpStatus::OK;
}

void TrustInfoParser::Continue(XMLParser* parser)
{
}

void TrustInfoParser::Stopped(XMLParser* parser)
{
	OP_ASSERT(parser->IsFinished() || parser->IsFailed());

	m_xml_parser = NULL;  // The XML parser deletes itself, and we can't do it
		                      //here anyway, as it's in a callback from it

	if (parser->IsFailed())  // don't retry request until sometime later.  Avoids DOS-ing the sitecheck server
		g_fraud_check_request_throttler.RequestFailed();
	else
		g_fraud_check_request_throttler.RequestSucceeded();

	m_checker->CheckDone(TRUE);
}


void TrustInfoParser::OnHostResolved(OpSocketAddress* address)
{
	OP_ASSERT(address);
	OP_STATUS opstatus = OpStatus::OK;

	if (address->GetNetType() != NETTYPE_LOCALHOST && address->GetNetType() != NETTYPE_PRIVATE)
		if (OpStatus::IsSuccess(opstatus = CheckURL(m_host_url, m_full_path_hash.CStr())))
			return;  // URL is being checked

	if (OpStatus::IsMemoryError(opstatus))
		g_memory_manager->RaiseCondition(opstatus);

	m_checker->CheckDone(FALSE);
}

TrustInfoParser::Advisory* TrustInfoParser::GetAdvisory(const uni_char *matching_text)
{
    UrlListItem *matching_item = NULL;
    UrlListItem *item = static_cast<UrlListItem*>(m_url_list.First());
	for (; item; item = static_cast<UrlListItem*>(item->Suc()))
	{
        if (uni_strcmp(item->url, matching_text) == 0)
        {
            matching_item = item;
            break;
        }
	}

    if (matching_item)
    {
        Advisory *elm = static_cast<Advisory*>(m_advisory_list.First());
        for (; elm; elm = static_cast<Advisory*>(elm->Suc()))
        {
            if (elm->id == matching_item->src)
                return elm;
        }
    }

    return NULL;
}


void FraudCheckRequestThrottler::RequestFailed()
{
	
	if (m_grace_period == 0)
		m_grace_period = FRAUD_CHECK_MINIMUM_GRACE_PERIOD;
	else // increase grace period for each failed attempt
	{
		m_grace_period *= 2;
		if (m_grace_period > FRAUD_CHECK_MAXIMUM_GRACE_PERIOD)
			m_grace_period = FRAUD_CHECK_MAXIMUM_GRACE_PERIOD;
	}

	m_last_failed_request = g_op_time_info->GetRuntimeMS();
}

BOOL FraudCheckRequestThrottler::AllowRequestNow()
{
	// Always TRUE if m_grace_period = 0
	return (g_op_time_info->GetRuntimeMS() - m_last_failed_request) > m_grace_period;
}

#endif // TRUST_RATING
