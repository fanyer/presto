/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url_sn.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/handy.h"
#include "modules/util/gen_str.h"
#include "modules/formats/uri_escape.h"
#include "modules/url/uamanager/ua.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/pi/OpSystemInfo.h"

#include "modules/url/prot_sel.h"

#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/decoders/utf8-decoder.h"

//#include "comm.h"
#include "modules/url/url2.h"
#include "modules/url/url_rel.h"
#include "modules/url/url_man.h"
#include "modules/cookies/url_cm.h"
#include "modules/idna/idna.h"
#if defined _ENABLE_AUTHENTICATE
#endif
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
#include "modules/libssl/sslv3.h"
#endif

#include "modules/olddebug/tstdump.h"

#ifdef DEFAULT_IPADDRESS_TIME_TO_LIVE
#error "Please remove the define DEFAULT_IPADDRESS_TIME_TO_LIVE and use the preference PrefsCollectionNetwork::IPAddressSecondsToLive instead."
#endif

BOOL CheckIsInDomainList(const char *domain, const char *domain_list);

ServerName::ServerName()
: servername_components(4) // Most servernames have 4 or less components
{
	InternalInit();
}

void ServerName::InternalInit()
{
	m_SocketAddress = NULL;
	m_AddressExpires = 0;
	nettype = NETTYPE_UNDETERMINED;

	connection_count = 0;
#if defined PUBSUFFIX_ENABLED
	domain_type = DOMAIN_UNKNOWN;
#endif

	info.is_local_host = FALSE;
	info.is_resolving_host = FALSE;
	info.cross_network = FALSE;


#ifdef OPERA_PERFORMANCE
	info.prefetchDNSStarted = FALSE;
#endif // OPERA_PERFORMANCE

#ifdef _ASK_COOKIE
	cookie_status = COOKIE_DEFAULT;
	cookie_accept_illegal_path = COOKIE_ILLPATH_DEFAULT;
	cookie_delete_on_exit = COOKIE_EXIT_DELETE_DEFAULT;
#endif
	third_party_cookie_status = COOKIE_DEFAULT;
	cookie_dns_expires = 0;
	cookiedns_lookup_done = CookieDNS_NotDone;

	info.is_wap_server = FALSE;
	info.http_lookup_succeded = FALSE;

#if defined(_EXTERNAL_SSL_SUPPORT_)
	// TODO: Remove for Hurricane
	id_currently_connecting_securely = 0;
#endif

#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	info.never_flush_trust = NeverFlush_Undecided;
#endif

#ifdef URL_BLOCK_FOR_MULTIPART
	blocked_by_multipart_count = 0;
#endif
	info.idna_restricted = FALSE;
	info.checked_parent_domain = FALSE;
	info.never_expires = FALSE;
	info.connection_restricted = FALSE;
#ifdef PERFORM_SERVERNAME_LENGTH_CHECK
	info.has_illegal_length = FALSE;
#endif // PERFORM_SERVERNAME_LENGTH_CHECK

#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	info.strict_transport_security_support = FALSE;
	info.strict_transport_security_include_subdomains = FALSE;
	strict_transport_security_expires = 0;
#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY

#ifdef  SOCKS_SUPPORT
	// socks_server_name = NULL; this is auto init-ed to NULL
	socks_server_port = 0;
#endif//SOCKS_SUPPORT

	use_special_UA = UA_PolicyUnknown;

#ifdef TRUST_RATING
	trust_rating = Not_Set;
	trust_rating_bypassed = FALSE;
	trust_expires = 0;
#endif
}

OP_STATUS ServerName::Construct(const OpStringC8 &name)
{
	RETURN_IF_ERROR(server_name.Set(name.HasContent() ? name : OpStringC8("")));

	int len = server_name.Length();

#ifdef PERFORM_SERVERNAME_LENGTH_CHECK
#define MAX_DOMAIN_LABEL_LEN 63 // Maximum length of label is 64 including length encoding (or dot)
	if( len > 255 ) // Maximum length is 256 including NULL termination
		info.has_illegal_length = TRUE;
	else if( len > MAX_DOMAIN_LABEL_LEN)
	{
		int end, start = 0, rest = len;
		do
		{
			end = server_name.FindFirstOf(".", start);
			if( (end == KNotFound && rest > MAX_DOMAIN_LABEL_LEN) ||
				(end-start > MAX_DOMAIN_LABEL_LEN) )
			{
				info.has_illegal_length = TRUE;
				break;
			}
			rest -= (end+1)-start;
			start = end+1;
		} while( end != KNotFound );
	}
#endif // PERFORM_SERVERNAME_LENGTH_CHECK

	info.is_wap_server = (server_name.CompareI("wap.",4) == 0 ? TRUE : FALSE);

	if (server_name.HasContent())
	{
		if(
#ifdef SUPPORT_LITERAL_IPV6_URL
			server_name[0] != '[' &&  // IPv6 addresses cannot contain international characters
#endif
			server_name.Find(IDNA_PREFIX) != KNotFound &&  // Servernames without the IDNA prefix ("xn--") does not need to be converted or checked
			UNICODE_SIZE((unsigned int) len)+100 < g_memory_manager->GetTempBuf2kLen() && (unsigned int) len < g_memory_manager->GetTempBuf2Len() )
		{
			char *temp1 = (char *) g_memory_manager->GetTempBuf2();
			uni_char *temp2 = (uni_char *) g_memory_manager->GetTempBuf2k();
			OpStringC8 domain_whitelist = urlManager->GetTLDWhiteList();

			if(domain_whitelist.IsEmpty() || domain_whitelist.Compare("::") == 0)
				goto do_not_convert;

			OP_MEMORY_VAR BOOL permit_full_IDNA = (domain_whitelist.Compare(":*:") == 0 ? TRUE : FALSE);
			BOOL reverse_to_blacklist=( (domain_whitelist[0] == '~') ? TRUE : FALSE);

			if(!permit_full_IDNA)
			{
				char *domain = op_strrchr(server_name.CStr(), '.');
				if(domain)
				{
					domain++;

					permit_full_IDNA = CheckIsInDomainList(domain, domain_whitelist.CStr());
					if (reverse_to_blacklist)
						permit_full_IDNA =!permit_full_IDNA;
				}
			}

			op_strcpy(temp1, server_name.CStr());
			UriUnescape::ReplaceChars(temp1, UriUnescape::NonCtrlAndEsc);
			char sep_char = '.';
			char *OP_MEMORY_VAR source = temp1;
			uni_char *OP_MEMORY_VAR target = temp2;
			while(source)
			{
				char *sep = op_strchr(source,sep_char);

				if(sep)
					*sep = '\0';

				if (op_strnicmp(source, IDNA_PREFIX, STRINGLENGTH(IDNA_PREFIX))==0)
				{
					BOOL is_mailaddress_tmp = FALSE;
					TRAPD(op_err, target = IDNA::ConvertALabelToULabel_L(source, target, (temp2 + UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()) - target - 1), is_mailaddress_tmp));
					if(OpStatus::IsError(op_err))
						goto do_not_convert;
				}
				else
				{
					UTF8toUTF16Converter converter;
					RETURN_IF_ERROR(converter.Construct());
					int read=0;

					len = op_strlen(source);
					len = converter.Convert(source, len, (char *) target, g_memory_manager->GetTempBuf2kLen() - UNICODE_SIZE(target - temp2+1), &read);
					if(len == -1)
						goto do_not_convert;

					len = UNICODE_DOWNSIZE(len);
					target += len;
				}
				if(sep)
					*(target++) = sep_char;
				source = sep ? sep + 1 : NULL;
			}
			*target = 0;

			if(!permit_full_IDNA)
			{
				URL_IDN_SecurityLevel level = IDN_Invalid;
				RETURN_IF_LEAVE(level = IDNA::ComputeSecurityLevel_L(temp2));
				switch(level)
				{
				case IDN_Minimal:
				case IDN_Moderate:
					break;
				case IDN_Unrestricted:
				default:
					info.idna_restricted = TRUE;
					goto do_not_convert;
				}
			}

			RETURN_IF_ERROR(server_nameW.Set(temp2));
		}
do_not_convert:;

		if(server_nameW.IsEmpty())
			RETURN_IF_ERROR(server_nameW.Set(server_name));
	}

	return OpStatus::OK;
}

ServerName *ServerName::Create(const OpStringC8 &name, OP_STATUS &op_err)
{
	OpAutoPtr<ServerName> new_server_name(OP_NEW(ServerName, ()));

	if(new_server_name.get() == NULL)
	{
		op_err = OpStatus::ERR_NO_MEMORY;
		return NULL;
	}

	op_err = new_server_name->Construct(name);
	RETURN_VALUE_IF_ERROR(op_err, NULL);

	op_err = OpStatus::OK;

	return new_server_name.release();
}

ServerName::~ServerName()
{
	InternalDestruct();
}

void ServerName::InternalDestruct()
{
#ifdef YNP_WORK
	//OP_ASSERT(Get_Reference_Count() == 0);
#endif

#ifdef TRUST_RATING
	Advisory *temp_advisory, *advisory_item = (Advisory *) advisory_list.First();
	while (advisory_item)
	{
		temp_advisory = advisory_item;
		advisory_item = (Advisory *) advisory_item->Suc();
		temp_advisory->Out();
		OP_DELETE(temp_advisory);
	}
#endif

	m_SocketAddress = NULL; // Automativally destroyed by the destruction of m_SocketAddressList
}

void ServerName::FreeUnusedResources(BOOL all)
{
	if(all)
	{
		ClearSocketAddresses();
		servername_components.DeleteAll();
		info.checked_parent_domain = FALSE;
		parent_domain = NULL;
	}
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
	FreeUnusedSSLResources(all);
#endif
}

BOOL ServerName::SafeToDelete()
{
	if (
		Get_Reference_Count() == 0 &&
#ifdef _ASK_COOKIE
		GetAcceptIllegalPaths() == COOKIE_ILLPATH_DEFAULT  &&
		GetAcceptCookies() == COOKIE_DEFAULT &&
#endif // _ASK_COOKIE
		(GetCookieDNS_Lookup_Done() == CookieDNS_NotDone || !GetCookieDNSExpires() ||
		GetCookieDNSExpires() < (time_t) (g_op_time_info->GetTimeUTC()/1000.0)) &&
		!GetHTTP_Lookup_Succceded() &&
		ServerNameAuthenticationManager::SafeToDelete() &&
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
		//Session_Accepted_Certificates == NULL &&
		Session_data.Empty() &&
#endif // _SSL_SUPPORT_
		pass_usernameURLs_automatically.Empty())
	{
		return TRUE;
	}
	return FALSE;
}

#ifdef TRUST_RATING
void ServerName::AddRegExpL(const OpStringC &regexp)
{
	OpStackAutoPtr<FraudListItem> temp(OP_NEW_L(FraudListItem, ()));
	temp->value.SetL(regexp);
	temp->Into(&regexp_list);
	temp.release();
}

FraudListItem *ServerName::GetFirstRegExpItem()
{
	return (FraudListItem *) regexp_list.First();
}

void ServerName::AddUrlL(const OpStringC &url, int id)
{
	OpStackAutoPtr<FraudListItem> temp(OP_NEW_L(FraudListItem, ()));
	temp->value.SetL(url);
	temp->id = id;
	temp->Into(&fraud_list);
	temp.release();
}

BOOL ServerName::IsUrlInFraudList(const OpStringC &url, const uni_char **fraud_item_url)
{
	if (fraud_item_url)
	{
		*fraud_item_url = NULL;
	}
	FraudListItem *temp = (FraudListItem *) fraud_list.First();
	while (temp)
	{
		// FraudItems end with '/' - Length() - 1 <- ignores '/' from fraud item
		// (url may end without '/'); if url is equal or greater than fraud item
		// then '/' in url at last position of fraud item means that url
		// is a fraud item or a subpath of it.
		int fraud_item_length = temp->value.Length();
		if (temp->value.Compare(url, fraud_item_length - 1) == 0 &&
			((fraud_item_length > url.Length() ||
			  url[fraud_item_length - 1] == '/')))
		{
			if (fraud_item_url)
			{
				*fraud_item_url = temp->value.CStr();
			}
			return TRUE;
		}
		temp = (FraudListItem *) temp->Suc();
	}
	return FALSE;
}

void ServerName::SetTrustTTL(unsigned int aTtl)
{
	if (aTtl < 5)
		aTtl = 5;

	trust_expires = g_timecache->CurrentTime() + aTtl;
}

TrustRating ServerName::GetTrustRating()
{
	if (trust_expires < g_timecache->CurrentTime())
	{
		trust_rating = Not_Set;
	}

	return trust_rating;
}

void ServerName::AddAdvisoryInfo(const uni_char *homepage_url, const uni_char *advisory_url, const uni_char *text, unsigned int src, unsigned int type)
{
	OpStackAutoPtr<Advisory> advisory(OP_NEW_L(Advisory, ()));
	advisory->homepage.SetL(homepage_url);
	advisory->advisory.SetL(advisory_url);
	advisory->text.SetL(text);
	advisory->src = src;
	advisory->type = type;
	advisory->Into(&advisory_list);
	advisory.release();
}
Advisory *ServerName::GetAdvisory(const OpStringC &url)
{
	unsigned int id = 0;
	FraudListItem *temp = (FraudListItem *) fraud_list.First();
	while (temp)
	{
		if (temp->value == url)
		{
			id = temp->id;
			break;
		}
		temp = (FraudListItem *) temp->Suc();
	}

	Advisory *advisory = static_cast<Advisory*>(advisory_list.First());
	for (; advisory; advisory = static_cast<Advisory*>(advisory->Suc()))
		if (advisory->src == id)
			return advisory;
	return NULL;
}
#endif //TRUST_RATING

#ifdef _ASK_COOKIE
COOKIE_MODES ServerName::GetAcceptCookies(BOOL follow_domain,BOOL first)
{
	if(!follow_domain ||
		server_name.IsEmpty())
		return cookie_status;

	ServerName *prnt = GetParentDomain();
	if(!prnt && first)
		prnt = urlManager->GetServerName("local");

	if(prnt)
	{
		COOKIE_MODES pmode = prnt->GetAcceptCookies(TRUE, FALSE);
		return (pmode == COOKIE_DEFAULT ? cookie_status : pmode);
	}

	return cookie_status;
}

COOKIE_ILLPATH_MODE ServerName::GetAcceptIllegalPaths(BOOL follow_domain,BOOL first)
{
	if(!follow_domain || cookie_accept_illegal_path != COOKIE_ILLPATH_DEFAULT ||
		server_name.IsEmpty())
		return cookie_accept_illegal_path;

	ServerName *prnt = GetParentDomain();
	if(!prnt && first)
		prnt = urlManager->GetServerName("local");

	if(prnt)
	{
		COOKIE_ILLPATH_MODE pmode = prnt->GetAcceptIllegalPaths(TRUE, FALSE);
		return (pmode != COOKIE_ILLPATH_DEFAULT ? pmode : cookie_accept_illegal_path);
	}

	return cookie_accept_illegal_path;
}

COOKIE_DELETE_ON_EXIT_MODE ServerName::GetDeleteCookieOnExit(BOOL follow_domain, BOOL first)
{
	if(!follow_domain ||
		cookie_delete_on_exit != COOKIE_EXIT_DELETE_DEFAULT ||
		server_name.IsEmpty())
		return cookie_delete_on_exit;

	ServerName *prnt = GetParentDomain();
	if(!prnt && first)
		prnt = urlManager->GetServerName("local");

	if(prnt)
	{
		return prnt->GetDeleteCookieOnExit(TRUE, FALSE);
	}

	return cookie_delete_on_exit;
}
#endif

COOKIE_MODES ServerName::GetAcceptThirdPartyCookies(BOOL follow_domain,BOOL first)
{
	if(!follow_domain ||
		server_name.IsEmpty())
		return third_party_cookie_status;

	ServerName *prnt = GetParentDomain();
	if(!prnt && first)
		prnt = urlManager->GetServerName("local");

	if(prnt)
	{
		COOKIE_MODES pmode = prnt->GetAcceptThirdPartyCookies(TRUE, FALSE);
		return (pmode == COOKIE_DEFAULT ? third_party_cookie_status : pmode);
	}

	return third_party_cookie_status;
}

UA_BaseStringId ServerName::GetOverRideUserAgent()
{
	if(use_special_UA == UA_PolicyUnknown)
	{
		 use_special_UA = UAManager::OverrideUserAgent(this);
	}
	return use_special_UA;
}


void ServerName::UpdateUAOverrides()
{
	if(use_special_UA != UA_PolicyUnknown)
	{
		use_special_UA = UAManager::OverrideUserAgent(this);
	}
}


#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
struct FailedProxyTime : public Link
{
	unsigned short port;
	time_t last_try;
};

void ServerName::SetFailedProxy(unsigned short port)
{
	FailedProxyTime *current;

	current = (FailedProxyTime *) proxies_last_active.First();
	while(current && current->port != port)
		current = (FailedProxyTime *)  current->Suc();

	if(!current)
	{
		current = OP_NEW(FailedProxyTime, ());
		if(current == NULL)
			return; // Not really important if this fails
		current->port = port;
		current->Into(&proxies_last_active);
	}

	current->last_try = g_timecache->CurrentTime();
}

BOOL ServerName::MayBeUsedAsProxy(unsigned short port)
{
	FailedProxyTime *current;

	current = (FailedProxyTime *) proxies_last_active.First();
	while(current && current->port != port)
		current = (FailedProxyTime *)  current->Suc();

	if(!current)
	{
		return TRUE;
	}

	return (current->last_try + (30 *60) < g_timecache->CurrentTime() ? TRUE : FALSE);
}
#endif


#if defined(_CERTICOM_SSL_SUPPORT_) || defined(_EXTERNAL_SSL_SUPPORT_)
OP_STATUS ServerName::AddAcceptedCertificate(OpCertificate *accepted_certificate)
{
	if (!accepted_certificate)
		return OpStatus::OK;

	unsigned int len;
	const char *cert = accepted_certificate->GetCertificateHash(len);
	ByteBuffer *added_accepted_certificate = NULL;

	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if 	(  (added_accepted_certificate = new ByteBuffer()) == NULL ||
			OpStatus::IsError(status = added_accepted_certificate->AppendBytes(cert, len)) ||
			OpStatus::IsError(status = Session_Accepted_Certificate_hashes.Add(added_accepted_certificate))
		)
	{
		OP_DELETE(added_accepted_certificate);
	}

	return status;
}


BOOL ServerName::IsCertificateAccepted(OpCertificate *opcertificate)
{
	if (!opcertificate)
		return FALSE;

	unsigned int certificate_len;

	const char *certificate = opcertificate->GetCertificateHash(certificate_len);
	unsigned int i;

	for (i = 0; i < Session_Accepted_Certificate_hashes.GetCount(); i++)
	{
		unsigned int accepted_certificate_len;
		char *accepted_certificate = Session_Accepted_Certificate_hashes.Get(i)->GetChunk(0, &accepted_certificate_len);
		if (accepted_certificate_len == certificate_len && !op_memcmp(accepted_certificate, certificate, accepted_certificate_len))
			return TRUE;
	}
	return FALSE;
}
#endif // _CERTICOM_SSL_SUPPORT_ || _EXTERNAL_SSL_SUPPORT_


void ServerName::RemoveSensitiveData()
{
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)
	ForgetCertificates();
	InvalidateSessionCache();

#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	SetStrictTransportSecurity(FALSE, FALSE, 0);
#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY

#endif
	ServerNameAuthenticationManager::RemoveSensitiveData();
	pass_usernameURLs_automatically.Clear();
	ClearSocketAddresses();
}

ServerName_Store::ServerName_Store(unsigned int size)
	: LinkObjectStore(size, op_strcmp)
{
}

OP_STATUS ServerName_Store::Construct()
{
	OP_STATUS op_err = LinkObjectStore::Construct();
	if(OpStatus::IsError(op_err))
		return op_err;
	return OpStatus::OK;
}

ServerName_Store::~ServerName_Store()
{
	ServerName* serv_name = (ServerName*) GetFirstLinkObject();
	while(serv_name)
	{
		RemoveLinkObject(serv_name);
#ifndef YNP_DEBUG_NAME
		OP_DELETE(serv_name);
#endif
		serv_name = (ServerName *) GetNextLinkObject();
	}
}

ServerName *ServerName_Store::GetServerName(const char *name, BOOL create)
{
	if(name == NULL)
		return NULL;

	unsigned int full_index = 0;
	ServerName *serv_name = (ServerName*) GetLinkObject(name, &full_index);
	if (!serv_name && create)
	{
		OP_STATUS op_err = OpStatus::OK;
		serv_name = ServerName::Create(name,op_err);

		if(OpStatus::IsError(op_err))
			return NULL;

		AddLinkObject(serv_name, &full_index);
	}
	return serv_name;
}

struct username_list : public Link
{
	OpStringS8 user_name;

	username_list(){};
	OP_STATUS Construct(const OpStringC8 &name){return user_name.Set(name.HasContent() ? name : OpStringC8(""));} // OOM: user_name checked on creation in SetPassUserNameURLsAutomatically.
	virtual ~username_list(){}
	BOOL Match(const OpStringC8 &name){return (user_name.Compare(name) == 0);};
};

BOOL ServerName::GetPassUserNameURLsAutomatically(const OpStringC8 &name)
{
	if(name.CStr() == NULL)
		return TRUE;

	username_list *item = (username_list *) pass_usernameURLs_automatically.First();
	while(item)
	{
		if(item->Match(name))
			return TRUE;
		item = (username_list *) item->Suc();
	}

	return FALSE;
}

OP_STATUS ServerName::SetPassUserNameURLsAutomatically(const OpStringC8 &p_name)
{
	OpStringC8 name(p_name.CStr() ? p_name : OpStringC8(""));

	if(GetPassUserNameURLsAutomatically(p_name))
		return OpStatus::OK;

	OpStackAutoPtr<username_list> new_item(OP_NEW(username_list, ()));
	if(new_item.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_item->Construct(name));

	new_item->Into(&pass_usernameURLs_automatically);
	new_item.release();

	return OpStatus::OK;
}

#ifdef _OPERA_DEBUG_DOC_
void ServerName::GetMemUsed(DebugUrlMemory &debug)
{
	debug.number_servernames++;
	debug.memory_servernames += sizeof(*this);
	if(server_name.CStr())
		debug.memory_servernames += server_name.Length()+1;
/*	if(server_nameW.CStr())
		debug.memory_servernames += UNICODE_SIZE(server_nameW.Length())+1);*/

	// Security structures not included
}
#endif

//***************************************************************

OpSocketAddress* ServerName::SocketAddress()
{
#ifdef _DEBUG
	OP_NEW_DBG(":SocketAddress", "ServerName");
	//OP_DBG(("") << "ServerName::SocketAddress() called");
	LogSocketAddress("m_SocketAddress: ", m_SocketAddress);
#endif

	CheckSocketAddress();
	return m_SocketAddress;
}

OP_STATUS ServerName::CheckSocketAddress()
{
	if(m_SocketAddress != NULL)
		return OpStatus::OK;

	RETURN_IF_ERROR(OpSocketAddress::Create(&m_SocketAddress));
	OP_STATUS op_err = m_SocketAddressList.AddSocketAddress(m_SocketAddress);
	if (OpStatus::IsError(op_err))
	{
        // m_SocketAddress is normally destroyed by the lists destructor,
		// since we failed to add it to the list we must destroy it here.
		OP_DELETE(m_SocketAddress);
		m_SocketAddress = NULL;
		return op_err;
	}
	return OpStatus::OK;
}

OP_STATUS ServerName::SetSocketAddress(OpSocketAddress* aSocketAddress)
{
#ifdef _DEBUG
	OP_NEW_DBG(":SetSocketAddress", "ServerName");
#endif

	RETURN_IF_ERROR(CheckSocketAddress());
	ClearSocketAddresses(); // This is the first address in a new lookup, clear out the old ones
	if(aSocketAddress)
	{
		OpSocketAddressNetType a_nettype =  aSocketAddress->GetNetType();
		if(nettype == NETTYPE_UNDETERMINED)
			nettype = a_nettype;
		else if(nettype != a_nettype)
		{
			if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowCrossNetworkNavigation, UniName()))
				nettype = a_nettype;
			else
				return OpStatus::ERR_OUT_OF_RANGE; // Error if different nettype
		}
#ifdef _DEBUG
		OP_DBG(("") << "Setting the main socket address:");
		LogSocketAddress("aSocketAddress: ", aSocketAddress);
#endif
		RETURN_IF_ERROR(m_SocketAddress->Copy(aSocketAddress));
		SetTTL(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::IPAddressSecondsToLive, UniName()));
	}
	else
		m_SocketAddress->FromString(UNI_L("0.0.0.0"));

	return OpStatus::OK;
}

OP_STATUS ServerName::AddSocketAddress(OpSocketAddress* aSocketAddress)
{
#ifdef _DEBUG
	OP_NEW_DBG(":AddSocketAddress", "ServerName");
#endif

	OP_STATUS op_err = OpStatus::OK;
	if(aSocketAddress)
	{
		if(aSocketAddress->IsValid())
		{
#ifdef _DEBUG
			OP_DBG(("") << "Adding socket address:");
			LogSocketAddress("aSocketAddress: ", aSocketAddress);
#endif
			if(
				nettype == aSocketAddress->GetNetType() && // Ignore if different nettype
				OpStatus::IsSuccess(op_err = m_SocketAddressList.AddSocketAddress(aSocketAddress)))
				aSocketAddress = NULL; // Do not delete
		}

		OP_DELETE(aSocketAddress);
	}

	return op_err;
}

void ServerName::ClearSocketAddresses()
{
#ifdef _DEBUG
	OP_NEW_DBG(":ClearSocketAddresses", "ServerName");
	OP_DBG(("") << "ServerName::ClearSocketAddresses() called");
#endif

	OpSocketAddressContainer *container = m_SocketAddressList.First();

	if(container)
	{
		container->Out();
		m_SocketAddressList.Clear();
		container->Into(&m_SocketAddressList);

		container->ClearFlags();
		m_SocketAddress = container->GetSocketAddress();
		m_SocketAddress->FromString(UNI_L("0.0.0.0"));
	}
	else
		m_SocketAddress = NULL;

#ifdef _DEBUG
	OP_DBG(("") << "After clearing:");
	LogAddressInfo();
#endif
}

void ServerName::ResetLastTryFailed()
{
	for (OpSocketAddressContainer* container = m_SocketAddressList.First();
	     container; container = container->Suc())
	{
		container->SetLastTryFailed(FALSE);
	}
}

OpSocketAddress* ServerName::GetNextSocketAddress() const
{
	OP_NEW_DBG(":GetNextSocketAddress", "ServerName");

	for (OpSocketAddressContainer* container = m_SocketAddressList.First();
	     container; container = container->Suc())
	{
		if (!container->IsLastTryFailed())
			return container->GetSocketAddress();
	}

	return NULL;
}

OpSocketAddress* ServerName::GetAltSocketAddress(OpSocketAddress::Family aCurrentFamily) const
{
	OP_NEW_DBG(":GetAltSocketAddress", "ServerName");

	OpSocketAddressContainer* found = NULL;

	for (OpSocketAddressContainer* container = m_SocketAddressList.First();
	     container; container = container->Suc())
	{
		if (container->IsTried())
			continue;

		OpSocketAddress* address = container->GetSocketAddress();
		OP_ASSERT(address);

		OpSocketAddress::Family family = address->GetAddressFamily();
		if (family != aCurrentFamily)
		{
			found = container;
			break;
		}
		// If an address from another family will not be found -
		// an untried address from the same family will be tried, if any.
		else if (!found)
			found = container;
	}

	if (found)
		return found->GetSocketAddress();
	else
		return NULL;
}

OpSocketAddressContainer* ServerName::GetContainerByAddress(const OpSocketAddress* aSocketAddress) const
{
	if (!aSocketAddress)
		return NULL;

	// OpSocketAddress::IsHostEqual() call below requires non-const pointer
	// as an argument. We could receive aSocketAddress as non-const pointer,
	// but it would make the interface worse. So let's better work it around
	// in the implementation.
	OpSocketAddress* arg_address = const_cast <OpSocketAddress*> (aSocketAddress);

	for (OpSocketAddressContainer* container = m_SocketAddressList.First();
	     container; container = container->Suc())
	{
		OpSocketAddress* address = container->GetSocketAddress();
		OP_ASSERT(address);

		if (address->IsHostEqual(arg_address))
			return container;
	}

	return NULL;
}

void ServerName::MarkAddressAsConnecting(const OpSocketAddress* aSocketAddress)
{
	OP_ASSERT(aSocketAddress);

	OpSocketAddressContainer* container = GetContainerByAddress(aSocketAddress);
	if (!container)
	{
		// Someone has already removed the container.
		// Should never happen really, but let's return anyway.
		OP_ASSERT(!"ServerName::MarkAddressAsConnecting: Container not found!");
		return;
	}

	container->SetTried(TRUE);
	container->SetInProgress(TRUE);
	container->SetLastTrySucceeded(FALSE);
	container->SetLastTryFailed(FALSE);
}

void ServerName::MarkAddressAsSucceeded(const OpSocketAddress* aSocketAddress)
{
	OP_ASSERT(aSocketAddress);

	OpSocketAddressContainer* container = GetContainerByAddress(aSocketAddress);
	if (!container)
	{
		// Someone has already removed the container.
		// It's too late to mark it.
		return;
	}

	container->SetInProgress(FALSE);
	container->SetLastTrySucceeded(TRUE);
	container->SetLastTryFailed(FALSE);

	// Successfull address "pinning".
	// Move container with the succeeded address to the beginning
	// of the container list.
	container->Out();
	container->IntoStart(&m_SocketAddressList);

	// Update "main" address of the host.
	OP_ASSERT(m_SocketAddress);
	m_SocketAddress = container->GetSocketAddress();
}

void ServerName::MarkAddressAsFailed(const OpSocketAddress* aSocketAddress)
{
	OP_ASSERT(aSocketAddress);

	OpSocketAddressContainer* container = GetContainerByAddress(aSocketAddress);
	if (!container)
	{
		// Someone has already removed the container.
		// It's too late to mark it.
		return;
	}

	container->SetInProgress(FALSE);
	container->SetLastTrySucceeded(FALSE);
	container->SetLastTryFailed(TRUE);

	// Failed address "pinning".
	// Move container with the failed address to the end
	// of the container list.
	container->Out();
	container->Into(&m_SocketAddressList);
}

void ServerName::MarkAllAddressesAsUntried()
{
	for (OpSocketAddressContainer* container = m_SocketAddressList.First();
	     container; container = container->Suc())
	{
		container->SetTried(FALSE);
	}
}

#ifdef _DEBUG
void ServerName::LogSocketAddress(const char* prefix, OpSocketAddress* aSocketAddress)
{
	OP_NEW_DBG(":LogSocketAddress", "ServerName");

	if (aSocketAddress)
	{
		OpString addr_string;
		aSocketAddress->ToString(&addr_string);
		OP_DBG(("") << prefix << addr_string);
	}
	else
		OP_DBG(("") << prefix << "NULL");
}

void ServerName::LogAddressInfo()
{
	OP_NEW_DBG(":LogAddressInfo", "ServerName");

	OP_DBG(("") << "Name(): " << Name());

	OpString addr_string;
	if (m_SocketAddress)
		m_SocketAddress->ToString(&addr_string);
	else
		addr_string.Set("NULL");
	OP_DBG(("") << "SocketAddress(): " << addr_string);

	unsigned int container_index = 0;
	for (OpSocketAddressContainer* container = m_SocketAddressList.First();
	     container; container = container->Suc(), container_index++)
	{
		OpSocketAddress* address = container->GetSocketAddress();
		OP_ASSERT(address);

		address->ToString(&addr_string);
		OP_DBG(("") << "Container " << container_index << ": " << addr_string);
	}

}
#endif // _DEBUG

BOOL ServerName::IsHostResolved()
{
	if (m_SocketAddress && m_SocketAddress->IsValid() && (info.never_expires || !m_AddressExpires ||
		g_timecache->CurrentTime() < m_AddressExpires))
		return TRUE;

	return FALSE;
}

OP_STATUS ServerName::SetHostAddressFromString(const OpStringC& aIPAddrString)
{
	RETURN_IF_ERROR(CheckSocketAddress());
	RETURN_IF_ERROR(m_SocketAddress->FromString(aIPAddrString.CStr()));
	nettype =  m_SocketAddress->GetNetType();
	SetTTL(0);
	return OpStatus::OK;
}

void ServerName::SetHostAddressFromStringL(const OpStringC& aIPAddrString)
{
	LEAVE_IF_ERROR(SetHostAddressFromString(aIPAddrString));
}

void ServerName::SetTTL(unsigned int aTtl)
{
	m_AddressExpires = 0;
	if(!m_SocketAddress || !m_SocketAddress->IsValid() || !aTtl)
		return;

	m_AddressExpires = g_timecache->CurrentTime() + aTtl;
}

#ifdef UNUSED_CODE
time_t ServerName::Expires() const
{
	return m_AddressExpires;
}
#endif // UNUSED_CODE

OpSocketAddressContainer::OpSocketAddressContainer(OpSocketAddress* aSocketAddress)
	: m_SocketAddress(aSocketAddress)
{
	ClearFlags();
	OP_ASSERT(m_SocketAddress);
}

OpSocketAddressContainer::~OpSocketAddressContainer()
{
	if(InList())
		Out();

	OP_DELETE(m_SocketAddress);
}

/*
OpSocketAddress* OpSocketAddressContainer::Release()
{
	OpSocketAddress* aSocketAddress;

	aSocketAddress = m_SocketAddress;
	m_SocketAddress = NULL;
	return aSocketAddress;
}
*/

OP_STATUS OpSocketAddressHead::AddSocketAddress(OpSocketAddress* aSocketAddress)
{
	OpSocketAddressContainer *container = OP_NEW(OpSocketAddressContainer, (aSocketAddress));
	if(container == NULL)
		return OpStatus::ERR_NO_MEMORY;
	container->Into(this);
	return OpStatus::OK;
}


//***************************************************************

#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
/** Determines if a server name is trusted given a list of traused domains. */
BOOL ServerNameIsTrusted(const OpStringC& aName,const OpStringC& aTrustedList)
    {
    if (aTrustedList.IsEmpty())
        return FALSE;

	const uni_char *current_filter = aTrustedList.CStr();
    int nameLen = aName.Length();
	int length;
	BOOL match = FALSE;

	do
        {
		length = uni_strcspn(current_filter, UNI_L(";"));

		if(length)
		    {
			if(*current_filter == '.')
			    {
				// valid matches must either be ".server_name" == filter
				// or server_name == "X"+ filter
				if(length == nameLen+1)
				    {
					if(aName.CompareI(current_filter+1, nameLen) == 0)
						match = TRUE;
				    }
				else if(length < nameLen)
				    {
					if(uni_strnicmp(aName.CStr()+(nameLen-length), current_filter, length) == 0)
						match = TRUE;
				    }
			    }
			else if(length == nameLen)
			    {
				if(aName.CompareI(current_filter, length) == 0)
					match = TRUE;
			    }
#ifdef IGNORE_PORT_IN_TRUSTED_SERVER_CHECK
			else
				{
				int tmp_length = uni_strcspn(current_filter, UNI_L(":"));
				if (tmp_length && aName.CompareI(current_filter, tmp_length) == 0)
					match = TRUE;
				}
#endif // IGNORE_PORT_IN_TRUSTED_SERVER_CHECK

			if(match)
				break;
		    }
		current_filter += length;
		if(*current_filter)
			current_filter ++;
	    }
    while(*current_filter);

    return match;
    }


TrustedNeverFlushLevel ServerName::TestAndSetNeverFlushTrust()
{
	if(info.never_flush_trust != NeverFlush_Undecided)
		return (TrustedNeverFlushLevel) info.never_flush_trust;

	const uni_char *t_server_name = UniName();

	if(t_server_name == NULL)
		return (TrustedNeverFlushLevel) (info.never_flush_trust = NeverFlush_Untrusted);

	TrustedNeverFlushLevel trusted = NeverFlush_Untrusted;

	const OpStringC filter(g_pcnet->GetStringPref(PrefsCollectionNetwork::NeverFlushTrustedServers));
    if (ServerNameIsTrusted(t_server_name,filter))
		// Setting Never Flush permitted
		trusted = NeverFlush_Trusted;

	info.never_flush_trust = trusted;

	return trusted;
}
#endif

void ServerName::CheckNameComponents()
{
	if(server_name.HasContent() && servername_components.GetCount() == 0)
	{
		int idx = 0;
		int count = 0;

		while(idx != KNotFound)
		{
			if(idx > 255 && count > 32)
			{
				servername_components.Clear();
				return;
			}

			int next_idx = server_name.FindFirstOf(".", idx);

			OpAutoPtr<OpStringS8> component(OP_NEW(OpStringS8, ()));

			if(component.get() == NULL ||
				OpStatus::IsError(component->Set(server_name.CStr() + idx, (next_idx != KNotFound ? next_idx-idx : KAll))) ||
				OpStatus::IsError(servername_components.Insert(0, component.get())))
			{
				servername_components.Clear();
				return;
			}

			component.release();
			count ++;

			idx = next_idx;
			if(idx != KNotFound)
				idx ++; // skip "."
		}
	}
}

#ifndef RETURN_CLASS_COPY
#if defined BREW || defined ADS12
#define RETURN_CLASS_COPY(class_name, operation) \
	do{\
		class_name ret_copy( operation );\
		return ret_copy;\
	}while(0)
#define RETURN_DEFAULT_COPY(class_name) \
	do{\
		class_name ret_copy;\
		return ret_copy;\
	}while(0)
#else
#define RETURN_CLASS_COPY(class_name, operation) return operation
#define RETURN_DEFAULT_COPY(class_name) return class_name ();
#endif
#endif

OpStringC8 ServerName::GetNameComponent(uint32 item)
{
	CheckNameComponents();

	OpStringS8 *comp = servername_components.Get(item);

	if(comp)
		return *comp;

	RETURN_DEFAULT_COPY(OpStringC8);
}

ServerName *ServerName::GetParentDomain()
{
	if(info.checked_parent_domain)
		return parent_domain;

	info.checked_parent_domain = TRUE;
	parent_domain = NULL;

	int s_len = server_name.Length();

	if((server_name.HasContent() && server_name[0] == '[') || server_name.SpanOf("01234567890.") == s_len)
		return NULL;

	int idx = server_name.FindFirstOf('.');
	if(idx == KNotFound)
		return NULL;

	if(s_len > 255)
	{
		uint32 count = GetNameComponentCount();

		if(!count || // Allocation failure
			count > 32 ) // Max limit of subdomains for long name; shorter names can have up to 128
			return NULL;
	}


	idx ++;

	parent_domain = g_url_api->GetServerName(server_name.CStr()+idx, TRUE);

	return parent_domain;
}

ServerName *ServerName::GetCommonDomain(ServerName *other)
{
	// If they are the same
	if(other == this)
		return this;

	// if there is no other servername
	if(!other)
		return NULL;

	ServerName *current_parent, *top,*current_parent_other, *top_other;

	top = this;
	top_other = other;
	current_parent = GetParentDomain();
	current_parent_other = other->GetParentDomain();

	// both servernames must at least be a subdomain
	if(current_parent == NULL || current_parent_other == NULL)
		return NULL;

	// If the parents are the same
	if(current_parent == current_parent_other)
		return current_parent;

	while(current_parent_other)
	{
		// is this servername a parent of other
		if(current_parent_other == this)
			return this;

		top_other = current_parent_other;
		current_parent_other = current_parent_other->GetParentDomain();
	}

	while(current_parent)
	{
		// is other a parent of this servername
		if(current_parent == other)
			return other;

		top = current_parent;
		current_parent = current_parent->GetParentDomain();
	}

	// If the do not end up at the same domain, there will never be a match;
	if(top != top_other)
		return NULL;

	current_parent = GetParentDomain();
	while(current_parent)
	{
		current_parent_other = other->GetParentDomain();
		while(current_parent_other)
		{
			// is this servername parent the same as the current parent of other
			if(current_parent_other == current_parent)
				return current_parent;

			current_parent_other = current_parent_other->GetParentDomain();
		}

		current_parent = current_parent->GetParentDomain();
	}

	return NULL;
}

BOOL CheckIsInDomainList(const char *domain, const char *domain_list)
{
	if(domain && domain_list)
	{
		char *tmpdom = (char *) g_memory_manager->GetTempBuf();

		tmpdom[0] = '\0';
		StrMultiCat(tmpdom,":",domain,":");
		//strlwr(tmpdom); // input is assumed to be lower case already

		if(op_strstr(domain_list,tmpdom) != NULL)
			return TRUE;
	}

	return FALSE;
}

#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
BOOL ServerName::SupportsStrictTransportSecurity()
{
	// Check timeout. Do this test first. If timed out, we must check parent domain
	if (info.strict_transport_security_support && strict_transport_security_expires < g_timecache->CurrentTime())
	{
		// Timed out
		SetStrictTransportSecurity(FALSE, FALSE, 0);
	}

	if (info.strict_transport_security_support)
	{
		return TRUE;
	}
	else
	{
		ServerName *check_parent_domain = GetParentDomain();
		if (check_parent_domain && check_parent_domain->SupportsStrictTransportSecurity())
		{
			// return true only if the parent includes sub domains.
			return check_parent_domain->GetStrictTransportSecurityIncludeSubdomains();
		}
	}
	return FALSE;
}

void ServerName::SetStrictTransportSecurity(BOOL strict_transport_security, BOOL include_subdomains, time_t expiry_date)
{
	info.strict_transport_security_support = strict_transport_security;
	info.strict_transport_security_include_subdomains = include_subdomains;
	strict_transport_security_expires = expiry_date;
}
#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY

#if defined PUBSUFFIX_ENABLED
ServerName::DomainType ServerName::GetDomainTypeASync(ServerName *checking_domain)
{
	if(domain_type != DOMAIN_UNKNOWN)
		return domain_type;

	ServerName *parent = GetParentDomain();
	if(parent != NULL)
	{
		DomainType parent_type = parent->GetDomainTypeASync(checking_domain);

		if(parent_type == DOMAIN_NORMAL || parent_type == DOMAIN_WAIT_FOR_UPDATE || parent_type == DOMAIN_UNKNOWN)
			return (domain_type = parent_type);
	}
	else
	{
		if(server_name.FindFirstOf('.') != KNotFound)
			return domain_type; // This is a non-tld, and we've actually encountered an OOM. ooops!
	}

	OP_STATUS op_err = g_pubsuf_api->CheckDomain(checking_domain);
	if(OpStatus::IsError(op_err))
		return (domain_type = DOMAIN_UNKNOWN);

	if(op_err == OpRecStatus::FINISHED)
		return domain_type;

	return (domain_type = DOMAIN_WAIT_FOR_UPDATE);
}
#endif
