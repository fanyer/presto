/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"
#ifdef _ENABLE_AUTHENTICATE

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/util/handy.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/url/url2.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/uri_escape.h"
#include "modules/auth/auth_elm.h"
#include "modules/auth/auth_basic.h"
#include "modules/auth/auth_digest.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/loadhandler/url_lhn.h"
#include "modules/url/url_pd.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/http_req2.h"
#ifndef NO_FTP_SUPPORT
#include "modules/url/protocols/ftp.h"
#endif // NO_FTP_SUPPORT


#include "modules/dochand/win.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/src/WindowCommanderManager.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/encodings/detector/charsetdetector.h"

#include "modules/util/cleanse.h"
#include "modules/util/OpHashTable.h"
#include "modules/network/src/op_request_impl.h"
#ifdef WAND_SUPPORT
# include "modules/wand/wandmanager.h"
#endif // WAND_SUPPORT

// URL Authentication Management

#ifndef CLEAR_PASSWD_FROM_MEMORY
#error "For security reasons FEATURE_CLEAR_PASSWORDS *MUST* be YES when HTTP Authentication is used"
#endif

#ifdef SECURITY_INFORMATION_PARSER
# include "modules/windowcommander/src/SecurityInfoParser.h"
# include "modules/windowcommander/src/TransferManagerDownload.h"
#endif // SECURITY_INFORMATION_PARSER
#ifdef TRUST_INFO_RESOLVER
# include "modules/windowcommander/src/TrustInfoResolver.h"
#endif // TRUST_INFO_RESOLVER

class waiting_url : public URLLink
{
public:
	authenticate_params auth_params;
	BOOL reauth;
	BOOL handled;
	BOOL authenticated;
	OpPointerSet<Window> windows;
	/*< Keeps track of which Windows/WindowCommanders we have notified.
		We need to know this in the case where more than one Window requests
		the same authorization. */
	OpAutoVector<OpAuthenticationCallback> callbacks;
	/*< Keeps a list of all associated OpAuthenticationCallback instances.
	    When one of the callbacks receives an answer (either
	    OpAuthenticationCallback::Authenticate() or
		OpAuthenticationCallback::CancelAuthentication()) we cancel the other
	    callbacks (see BroadcastAuthenticationCancelled()). */
public:
	waiting_url(const URL &p_url, BOOL p_reauth);
	~waiting_url();

	/**
	 * Notifies OpAuthenticationListener::OnAuthenticationCancelled() for each
	 * OpAuthenticationCallback instance that is associated with this instance
	 * and deletes all callback instances. After the OpAuthenticationListener is
	 * notified, the callback shall no longer be called.
	 */
	void BroadcastAuthenticationCancelled();
};

class AuthenticationCallback : public OpAuthenticationCallback
{
private:
	waiting_url* m_wurl;
	Window* m_window;
	const URL m_url;
	AuthenticationType m_type;
	const ServerName* m_servername;
	enum AuthenticationSecurityLevel m_security_level;
#ifdef WAND_SUPPORT
	OpString m_wand_id;
	enum WandAction m_wand_action;
#endif // WAND_SUPPORT
	OpString m_authentication_name;
	OpString8 m_message;
	OpString m_url_string;

	OP_STATUS SetAuthenticationName(Str::LocaleString string_id);
	const waiting_url* GetWUrl() const {return m_wurl;}

public:
	const URL &GetURL() { return m_url; }
	AuthenticationCallback(waiting_url* wurl, Window* window, const struct authenticate_params& auth_params);
	virtual ~AuthenticationCallback();

	OP_STATUS Init(const struct authenticate_params& auth_params);

	/**
	 * @name Implementation of OpAuthenticationCallback
	 *
	 * @{
	 */

	virtual AuthenticationType GetType() const { return m_type; }
	virtual const char* ServerNameC() const { return m_servername->Name(); }
	virtual const uni_char* ServerUniName() const { return m_servername->UniName(); }

	virtual int GetSecurityMode() const;
	virtual UINT32 SecurityLowStatusReason() const;
#ifdef TRUST_RATING
	virtual int GetTrustMode() const;
#endif
	virtual int GetWicURLType() const;
#if defined SECURITY_INFORMATION_PARSER || defined TRUST_INFO_RESOLVER
	virtual OP_STATUS CreateURLInformation(URLInformation** url_info) const;
#endif
	
	virtual const uni_char* LastUserName() const;
	virtual const char* GetUserName() const { return m_url.GetAttribute(URL::KUserName).CStr(); }
	virtual const uni_char* Url() const { return m_url_string.CStr(); }
	virtual UINT16 Port() const { return static_cast<UINT16>(m_url.GetAttribute(URL::KResolvedPort)); }
	virtual URL_ID UrlId() const { return m_url.Id(); }
	virtual DocumentURLType GetURLType() const { return WindowCommander::URLType2DocumentURLType(m_url.Type()); }
	virtual enum AuthenticationSecurityLevel SecurityLevel() const { return m_security_level; }
	virtual const uni_char* AuthenticationName() const { return m_authentication_name.CStr(); }
	virtual const char* Message() const { return m_message.CStr(); }

#ifdef WAND_SUPPORT
	virtual const uni_char* WandID() const { return m_wand_id.CStr(); }
	virtual void SetWandAction(enum WandAction action, const uni_char* wand_id);
#endif // WAND_SUPPORT

	virtual void Authenticate(const uni_char* user_name, const uni_char* password);
	virtual void CancelAuthentication();
	virtual BOOL IsSameAuth(const OpAuthenticationCallback* callback) const;

	/** @} */ // Implementation of OpAuthenticationCallback
};

// ============= AuthenticationCallback =======================================

AuthenticationCallback::AuthenticationCallback(waiting_url* wurl, Window* window, const struct authenticate_params& auth_params)
	: m_wurl(wurl)
	, m_window(window)
	, m_url(auth_params.url)
	, m_type(AUTHENTICATION_NEEDED)
	, m_servername(auth_params.servername)
	, m_security_level(AUTH_SEC_PLAIN_TEXT)
#ifdef WAND_SUPPORT
	, m_wand_action(WAND_NO_CHANGES)
#endif // WAND_SUPPORT
{
	OP_NEW_DBG("AuthenticationCallback::AuthenticationCallback()", "auth");
	OP_DBG(("this: %p; waiting_url: %p; server: %s", this, m_wurl, ServerNameC()));
}

AuthenticationCallback::~AuthenticationCallback()
{
	OP_NEW_DBG("AuthenticationCallback::~AuthenticationCallback()", "auth");
	OP_DBG(("this: %p; waiting_url: %p; server: %s", this, m_wurl, ServerNameC()));
}

BOOL AuthenticationCallback::IsSameAuth(const OpAuthenticationCallback* ext_callback) const
{
	if (!ext_callback)
		return FALSE;
 
	const waiting_url* ext_wurl = static_cast<const AuthenticationCallback *>(ext_callback)->GetWUrl();
	OP_ASSERT(ext_wurl);
 
	const authenticate_params& this_auth_params =   m_wurl->auth_params;
	const authenticate_params&  ext_auth_params = ext_wurl->auth_params;

	// Compare context IDs.
	URL_CONTEXT_ID this_context_id = this_auth_params.url.GetRep()->GetContextId();
	URL_CONTEXT_ID  ext_context_id =  ext_auth_params.url.GetRep()->GetContextId();
	if (this_context_id != ext_context_id)
		return FALSE;
 
	// Compare proxy-non-proxy.
	if (( this_auth_params.proxy && !ext_auth_params.proxy) ||
	    (!this_auth_params.proxy &&  ext_auth_params.proxy)  )
		return FALSE;
 
	// Compare auth schemes.
	AuthScheme this_scheme = this_auth_params.scheme;
	AuthScheme ext_scheme  =  ext_auth_params.scheme;
	if (this_scheme != ext_scheme)
		return FALSE;
 
	// Compare hostnames.
	const uni_char* this_hostname = this_auth_params.servername->UniName();
	OP_ASSERT(this_hostname);
	const uni_char*  ext_hostname =  ext_auth_params.servername->UniName();
	OP_ASSERT(ext_hostname);
	if (uni_strcmp(this_hostname, ext_hostname))
		return FALSE;
 
	// Compare ports.
	unsigned short this_port = this_auth_params.port;
	unsigned short ext_port  =  ext_auth_params.port;
	if (this_port != ext_port)
		return FALSE;
 
	// Comparing protocols and realms only makes sense if it is not proxy authentication.
	if (!this_auth_params.proxy)
	{
		// Compare protocols.
		URLType this_type =   (URLType) m_wurl->url.GetAttribute(URL::KRealType);
		URLType  ext_type = (URLType) ext_wurl->url.GetAttribute(URL::KRealType);
		if (this_type != ext_type)
			return FALSE;
 
		// Compare realms. It only makes sense for HTTP.
		if (this_type == URL_HTTP || this_type == URL_HTTPS)
		{
			const char* this_realm = this_auth_params.message;
			const char*  ext_realm =  ext_auth_params.message;
 
#ifdef _USE_PREAUTHENTICATION_
			if (!this_realm)
				this_realm = this_auth_params.servername->ResolveRealm(this_port, NULL, this_type, this_scheme, this_context_id);
			if (!ext_realm)
				ext_realm = ext_auth_params.servername->ResolveRealm(ext_port, NULL, ext_type, ext_scheme, ext_context_id);
#endif // _USE_PREAUTHENTICATION_
			
			if (!this_realm || !ext_realm || op_strcmp(this_realm, ext_realm))
				return FALSE;
		}
	}
	return TRUE;
}

OP_STATUS AuthenticationCallback::SetAuthenticationName(Str::LocaleString string_id)
{
	int len = 0;
	TRAP_AND_RETURN(err, len = g_languageManager->GetStringL(string_id, m_authentication_name));
	return len ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS AuthenticationCallback::Init(const struct authenticate_params& auth_params)
{
	OP_NEW_DBG("AuthenticationCallback::Init()", "auth");
	RETURN_IF_ERROR(m_url_string.Set(m_url.GetAttribute(URL::KUniName)));

	switch (auth_params.type)
	{
	case AUTH_NEEDED:		m_type = AUTHENTICATION_NEEDED; break;
	case AUTH_WRONG:		m_type = AUTHENTICATION_WRONG; break;
	case PROXY_AUTH_NEEDED:	m_type = PROXY_AUTHENTICATION_NEEDED; break;
	case PROXY_AUTH_WRONG:	m_type = PROXY_AUTHENTICATION_WRONG; break;
	default:
		OP_ASSERT(!"Unsupported authentication type");
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	/* This switch sets the security level:
	 * - OpAuthenticationCallback::AUTH_SEC_PLAIN_TEXT:
	 *   Plain text (even when encrypted)
	 * - OpAuthenticationCallback::AUTH_SEC_LOW:
	 *   Some security, Cryptographical authentication, but low connection
	 *   security
	 * - OpAuthenticationCallback::AUTH_SEC_GOOD:
	 *   Better security, Cryptographical authentication, with good connection
	 *   security
	 */
	switch (auth_params.scheme)
	{
	case AUTH_SCHEME_FTP:
	case AUTH_SCHEME_HTTP_BASIC:
	case AUTH_SCHEME_HTTP_PROXY_BASIC:
		RETURN_IF_ERROR(SetAuthenticationName(Str::S_BASIC_AUTH_DESCRIPTION));
		m_security_level = (auth_params.url.GetAttribute(URL::KSecurityStatus) < SECURITY_STATE_FULL ? AUTH_SEC_PLAIN_TEXT : AUTH_SEC_LOW);
		break;

	case AUTH_SCHEME_HTTP_DIGEST:
		RETURN_IF_ERROR(SetAuthenticationName(Str::S_DIGEST_AUTH_DESCRIPTION));
		m_security_level = (auth_params.url.GetAttribute(URL::KSecurityStatus) < SECURITY_STATE_FULL ? AUTH_SEC_LOW : AUTH_SEC_GOOD);
		break;

	case AUTH_SCHEME_HTTP_PROXY_DIGEST:
		RETURN_IF_ERROR(SetAuthenticationName(Str::S_DIGEST_AUTH_DESCRIPTION));
		m_security_level = AUTH_SEC_LOW;
		break;

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	case AUTH_SCHEME_HTTP_NTLM:
	case AUTH_SCHEME_HTTP_PROXY_NTLM:
		RETURN_IF_ERROR(SetAuthenticationName(Str::S_NTLM_AUTH_DESCRIPTION));
		m_security_level = AUTH_SEC_LOW;
		break;

	case AUTH_SCHEME_HTTP_NEGOTIATE:
	case AUTH_SCHEME_HTTP_NTLM_NEGOTIATE:
	case AUTH_SCHEME_HTTP_PROXY_NEGOTIATE:
	case AUTH_SCHEME_HTTP_PROXY_NTLM_NEGOTIATE:
		RETURN_IF_ERROR(SetAuthenticationName(Str::S_MSNEGOTIATE_AUTH_DESCRIPTION));
		m_security_level = AUTH_SEC_GOOD;
		break;
#endif // _SUPPORT_PROXY_NTLM_AUTH_

	default:
		OP_ASSERT(!"Unsupported authentication scheme!");
		return OpStatus::ERR_NOT_SUPPORTED;
	}

#ifdef WAND_SUPPORT
	switch (m_type)
	{
	case OpAuthenticationCallback::AUTHENTICATION_NEEDED:
	case OpAuthenticationCallback::AUTHENTICATION_WRONG:
	{	// wand-id is "*<url>"
		RETURN_IF_ERROR(m_wand_id.Set(UNI_L("*")));
		RETURN_IF_ERROR(m_wand_id.Append(Url()));
		const OpStringC query(m_url.GetAttribute(URL::KUniQuery_Escaped));
		if (query.HasContent())
		{
			int pos = m_wand_id.Find(query);
			if (pos > 0)
				m_wand_id.Delete(pos);
		}
		break;
	}

	case OpAuthenticationCallback::PROXY_AUTHENTICATION_NEEDED:
	case OpAuthenticationCallback::PROXY_AUTHENTICATION_WRONG:
		// wand-id is "<servername>:<port>"
		RETURN_IF_ERROR(m_wand_id.Set(ServerUniName()));
		RETURN_IF_ERROR(m_wand_id.AppendFormat(":%d", Port()));
		break;

	default:
		OP_ASSERT(!"Unsupported authentication type.");
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	OP_DBG((UNI_L("this: %p; wand-id: %s"), this, m_wand_id.CStr()));
#endif // WAND_SUPPORT

	if (auth_params.message)
		RETURN_IF_ERROR(m_message.Set(auth_params.message));
	else if (auth_params.scheme == AUTH_SCHEME_FTP)
		OpStatus::Ignore(m_message.SetUTF8FromUTF16(auth_params.url.GetAttribute(URL::KInternalErrorString))); // Nothing really bad happens if this fails

	return OpStatus::OK;
}

const uni_char* AuthenticationCallback::LastUserName() const
{
	switch (m_type) {
	case AUTHENTICATION_NEEDED:
	case AUTHENTICATION_WRONG:
		{
			/* return m_servername->GetLastUserName().CStr() will cause
			 * ADS12 compiler panic, need an extra copy here. */
			OpStringC str = m_servername->GetLastUserName();
			return str.CStr();
		}

	case PROXY_AUTHENTICATION_NEEDED:
	case PROXY_AUTHENTICATION_WRONG:
		{
			/* return m_servername->GetLastProxyUserName().CStr() will cause
			 * ADS12 compiler panic, need an extra copy here. */
			OpStringC str = m_servername->GetLastProxyUserName();
			return str.CStr();
		}

	default:
		OP_ASSERT(!"Unsupported authentication type");
		return 0;
	}
}

#ifdef WAND_SUPPORT
/* virtual */
void AuthenticationCallback::SetWandAction(enum WandAction action, const uni_char* wand_id)
{
	OP_NEW_DBG("AuthenticationCallback::SetWandAction()", "auth");
	m_wand_action = action;
	if ((action == WAND_STORE_LOGIN || action == WAND_DELETE_LOGIN) && wand_id)
	{
		OpString new_wand_id;
		if (OpStatus::IsSuccess(new_wand_id.Set(wand_id)))
			// note: this ensures that we don't loose the m_wand_id on error
			m_wand_id.TakeOver(new_wand_id);
		else
			/* Don't store the user-name,password pair if we fail to set the
			 * wand-id this is probably OOM. */
			m_wand_action = WAND_NO_CHANGES;
	}
}
#endif // WAND_SUPPORT

void AuthenticationCallback::Authenticate(const uni_char* user_name, const uni_char* password)
{
	OP_NEW_DBG("AuthenticationCallback::Authenticate()", "auth");
	URL_ID authid = UrlId();
	OP_DBG(("this: %p; waiting_url: %p; id: %p", this, m_wurl, authid));

	/* Remove this instance from the associated m_wurl before calling
	 * urlManager->Authenticate() (Authentication_Manager::Authenticate()),
	 * which calls waitung_url::BroadcastAuthenticationCancelled(), which
	 * deletes all AuthenticationCallback instances, so this instance will
	 * continue to live until the end of this method... */
	OpStatus::Ignore(m_wurl->callbacks.RemoveByItem(this));

#ifdef WAND_SUPPORT
	switch (m_wand_action) {
	case WAND_STORE_LOGIN:
		OP_DBG((UNI_L("WAND_STORE_LOGIN: wand-id: '%s'; username: '%s'"), m_wand_id.CStr(), user_name));
		if (m_window)
			OpStatus::Ignore(g_wand_manager->StoreLogin(m_window, m_wand_id.CStr(), user_name, password));
		else
			OpStatus::Ignore(g_wand_manager->StoreLoginWithoutWindow(m_wand_id.CStr(), user_name, password));
		break;

	case WAND_DELETE_LOGIN:
		OP_DBG((UNI_L("WAND_DELETE_LOGIN: wand-id: '%s'; username: '%s'"), m_wand_id.CStr(), user_name));
		g_wand_manager->DeleteLogin(m_wand_id.CStr(), user_name);
		break;

	// case WAND_NO_CHANGES:	// nothing to do ...
	// default:
	}
#endif // WAND_SUPPORT

	urlManager->Authenticate(user_name, password, authid);
	urlManager->AuthenticationDialogFinished(authid);

	OP_DELETE(this);
}

int AuthenticationCallback::GetSecurityMode() const
{
	switch (m_url.GetAttribute(URL::KSecurityStatus))
	{
	case SECURITY_STATE_FULL:
		return OpDocumentListener::HIGH_SECURITY;
# ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	case SECURITY_STATE_FULL_EXTENDED:
		return OpDocumentListener::EXTENDED_SECURITY;
#  ifdef SECURITY_STATE_SOME_EXTENDED
	case SECURITY_STATE_SOME_EXTENDED:
		return OpDocumentListener::SOME_EXTENDED_SECURITY;
#  endif // SECURITY_STATE_SOME_EXTENDED
# endif // SSL_CHECK_EXT_VALIDATION_POLICY
	case SECURITY_STATE_HALF:
		return OpDocumentListener::MEDIUM_SECURITY;
	case SECURITY_STATE_LOW:
		return OpDocumentListener::LOW_SECURITY;
	case SECURITY_STATE_NONE:
		return OpDocumentListener::NO_SECURITY;
	}
	return OpDocumentListener::UNKNOWN_SECURITY;
}

UINT32 AuthenticationCallback::SecurityLowStatusReason() const
{
	return m_url.GetAttribute(URL::KSecurityLowStatusReason);
}

#ifdef TRUST_RATING
int AuthenticationCallback::GetTrustMode() const
{
	// TODO: arneh
	return OpDocumentListener::TRUST_NOT_SET;
}
#endif

int AuthenticationCallback::GetWicURLType() const
{
	OpWindowCommander::WIC_URLType wic_type = OpWindowCommander::WIC_URLType_Unknown;
	URLType url_type = m_url.Type();
	switch(url_type)
	{
	case URL_HTTP:
		wic_type = OpWindowCommander::WIC_URLType_HTTP;
		break;
	case URL_HTTPS:
		wic_type = OpWindowCommander::WIC_URLType_HTTPS;
		break;
	case URL_FTP:
		wic_type = OpWindowCommander::WIC_URLType_FTP;
		break;
	}
	return wic_type;
}

#if defined SECURITY_INFORMATION_PARSER || defined TRUST_INFO_RESOLVER
OP_STATUS AuthenticationCallback::CreateURLInformation(URLInformation** url_info) const
{
	ViewActionFlag view_action_flag;
	URL url = m_url;
	TransferManagerDownloadCallback * download_callback = OP_NEW(TransferManagerDownloadCallback, (NULL, url, NULL, view_action_flag));
	if (download_callback)
	{
		*url_info = download_callback;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}
#endif

void AuthenticationCallback::CancelAuthentication()
{
	OP_NEW_DBG("AuthenticationCallback::CancelAuthentication()", "auth");
	URL_ID authid = UrlId();
	OP_DBG(("this: %p; waiting_url: %p; id: %p", this, m_wurl, authid));

	/* Remove this instance from the associated m_wurl before calling
	 * urlManager->CancelAuthentication()
	 * (Authentication_Manager::CancelAuthentication()), which calls
	 * waitung_url::BroadcastAuthenticationCancelled(), which deletes all
	 * AuthenticationCallback instances, so this instance will continue to live
	 * until the end of this method... */
	OpStatus::Ignore(m_wurl->callbacks.RemoveByItem(this));

	urlManager->CancelAuthentication(authid);
	urlManager->AuthenticationDialogFinished(authid);

	OP_DELETE(this);
}

// ============= waiting_url ==================================================

waiting_url::waiting_url(const URL &p_url, BOOL p_reauth)
	: URLLink(p_url)
	, reauth(p_reauth)
	, handled(FALSE)
	, authenticated(FALSE)
{
	OP_NEW_DBG("waiting_url::waiting_url()", "auth");
	OP_DBG(("this: %p; id: %p", this, url.Id()));
}

waiting_url::~waiting_url()
{
	OP_NEW_DBG("waiting_url::~waiting_url()", "auth");
	OP_DBG(("this: %p", this));
	OP_ASSERT(callbacks.GetCount() == 0 || !"Somebody deleted this instance without cancelling all callbacks. That may cause a crash if the UI calls the callback instance.");
}

void waiting_url::BroadcastAuthenticationCancelled()
{
	OP_NEW_DBG("waiting_url::BroadcastAuthenticationCancelled()", "auth");
	OP_DBG(("this: %p; id: %p", this, url.Id()));
	if (g_opera && g_windowCommanderManager)
	{
		while (callbacks.GetCount() > 0)
		{
			OpAuthenticationCallback* callback = callbacks.Remove(callbacks.GetCount()-1);
			OP_DBG((UNI_L("callback: %p"), callback));

			URL_DataStorage *url_ds = url.GetRep()->GetDataStorage();
			OpRequestImpl *request = url_ds ? url_ds->GetOpRequestImpl() : NULL;
			if (!url.GetAttribute(URL::KUseGenericAuthenticationHandling) && request)
				request->AuthenticationCancelled(callback);
			else
				static_cast<WindowCommanderManager*>(g_windowCommanderManager)->GetAuthenticationListener()->OnAuthenticationCancelled(callback);
			OP_DELETE(callback);
		}
	}
}

#ifdef HTTP_DIGEST_AUTH
extern digest_alg CheckDigestAlgs(const char *val
#ifdef EXTERNAL_DIGEST_API
							, ServerName *host
							, External_Digest_Handler *&digest
#endif
						);
#endif /* HTTP_DIGEST_AUTH */

// ============= Authentication_Manager =======================================

Authentication_Manager::Authentication_Manager()
{
	InternalInit();
}

void Authentication_Manager::InternalInit()
{
	active = FALSE;
}

void Authentication_Manager::InitL()
{
}

Authentication_Manager::~Authentication_Manager()
{
	InternalDestruct();
}

void Authentication_Manager::InternalDestruct()
{
}

BOOL Authentication_Manager::HandleAuthentication(URL_Rep *auth_obj, BOOL reauth)
{
	if(!auth_obj)
		return FALSE;

	URL auth_url(auth_obj, (const char *) NULL);
	waiting_url* wurl = OP_NEW(waiting_url, (auth_url, reauth));

	if(!wurl || OpStatus::IsError(waiting_urls.Append(wurl)))
	{
		OP_DELETE(wurl);
		auth_obj->FailAuthenticate(AUTH_MEMORY_FAILURE);
		return FALSE;
	}

	OP_STATUS op_err;
	if( OpStatus::IsError(op_err = StartAuthentication()) )	// FIXME: Temporary fix for bug #52366. Should perhaps be handled diffrently. <no>
	{
		if( op_err == OpStatus::ERR_NO_MEMORY )
			auth_obj->FailAuthenticate(AUTH_MEMORY_FAILURE);
		return FALSE;
	}
	return TRUE;
}

BOOL Authentication_Manager::OnMessageHandlerAdded(URL_Rep *auth_obj)
{
	OP_NEW_DBG("Authentication_Manager::OnMessageHandlerAdded()", "auth");
	if (!auth_obj)
		return FALSE;

	MsgHndlList::Iterator itr = auth_obj->GetMessageHandlerList()->Begin();
	MsgHndlList::ConstIterator end = auth_obj->GetMessageHandlerList()->End();
	for(; itr != end; ++itr)
	{
		MessageHandler *mh = (*itr)->GetMessageHandler();
		Window* window = mh ? mh->GetWindow() : NULL;

		waiting_url *wurl = FindWaitingURL(auth_obj);
		OP_DBG(("mh: %p; window: %p; wurl: %p", mh, window, wurl));
		if(wurl && !wurl->windows.Contains(window))
		{
			OP_DBG(("wurl: ") << *wurl);
			/* Note: AuthenticationDialogFinished() might be triggered by this
			 * call (e.g. when the authentication is cancelled immediately), so
			 * both wurl and *itr may no longer be valid after this call. */
			if (OpStatus::IsError(NotifyAuthenticationRequired(wurl, window, wurl->auth_params)))
				return FALSE;
		}
	}

	return TRUE;
}

#define NO_AUTH_HEADER 0
#define AUTH_HEADER_DIALOG 1
#define AUTH_HEADER_FINISHED 2
#define AUTH_HEADER_FAILURE 3

int Authentication_Manager::SetUpHTTPAuthentication(authenticate_params &auth_params, authdata_st *authinfo, unsigned int header_id, BOOL proxy)
{
	URLType type = proxy ? URL_HTTP : (URLType) auth_params.url.GetAttribute(URL::KType);
	if(authinfo == NULL || authinfo->auth_headers == NULL)
		return NO_AUTH_HEADER;

	HeaderList *auth_headers = authinfo->auth_headers;
	OP_ASSERT(auth_headers);

	URL_Rep *url_rep = auth_params.url.GetRep();
	OP_ASSERT(url_rep);

	HeaderEntry *header = auth_headers->GetHeaderByID(header_id);
	ParameterList *params;
	Parameters *param, *realm=NULL;
	HeaderEntry *basic_header = NULL;
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	HeaderEntry *ntlm_header = NULL;
	HeaderEntry *negotiate_header = NULL;

	auth_params.ntlm_auth_arguments = NULL;
	BOOL ntlm_enabled = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableNTLM);
#endif

	if(!header)
		return NO_AUTH_HEADER;

	auth_params.servername = proxy ? authinfo->connect_host : authinfo->origin_host ;

	auth_params.port = proxy ? authinfo->connect_port : authinfo->origin_port;
	if(auth_params.port == 0)
		auth_params.port = proxy ? 80 : url_rep->GetAttribute(URL::KResolvedPort);

	auth_params.proxy = proxy;
	if(proxy)
		auth_params.type = (auth_params.type == AUTH_WRONG ? PROXY_AUTH_WRONG: PROXY_AUTH_NEEDED);

	do{
		// Check Ordinary Authentication
		params = header->GetParameters((PARAM_SEP_COMMA | PARAM_WHITESPACE_FIRST_ONLY | PARAM_STRIP_ARG_QUOTES), KeywordIndex_Authentication) ;
		if(!params || !params->First())
			continue;
		param = params->First();
		if(!param->Name())
			continue;

		AuthScheme scheme = (AuthScheme) param->GetNameID();
		AuthScheme actual_scheme = scheme;

		if(proxy && scheme != AUTH_SCHEME_HTTP_UNKNOWN)
			scheme |= AUTH_SCHEME_HTTP_PROXY;

		realm = params->GetParameterByID(HTTP_Authentication_Realm,PARAMETER_ASSIGNED_ONLY);
		const char *realm_name = (realm ? realm->Value() : (actual_scheme == AUTH_SCHEME_HTTP_BASIC ? "" : NULL));

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
		if(ntlm_enabled)
		{
			if(actual_scheme == AUTH_SCHEME_HTTP_NEGOTIATE)
			{
				negotiate_header = header;
				realm_name = "";
			}
			else if(actual_scheme == AUTH_SCHEME_HTTP_NTLM)
			{
				ntlm_header = header;
				realm_name = "";
			}
		}
#endif

		URL_CONTEXT_ID context_id = (proxy ? 0 : url_rep->GetContextId());

		if(actual_scheme != AUTH_SCHEME_HTTP_UNKNOWN && realm_name)
		{
			auth_params.scheme = scheme;

			AuthElm *elm = auth_params.servername->Get_Auth(realm_name,auth_params.port,NULL, type, scheme, context_id);

			if(!elm && !proxy)
			{
				AuthElm *elm1 = auth_params.servername->Get_Auth(NULL,auth_params.port,
					(/*proxy ? NULL : */auth_params.url.GetAttribute(URL::KPath).CStr()), type, scheme, context_id);

				if(elm1 && elm1->GetRealm() && OpStringC8(realm_name).Compare(elm1->GetRealm()) != 0)
				{
					elm1->Out();
					OP_DELETE(elm1);
				}
			}

#ifdef HTTP_DIGEST_AUTH
			if(elm && !proxy && actual_scheme == AUTH_SCHEME_HTTP_DIGEST && elm->Aliased())
			{
				AuthElm *elm1 = auth_params.servername->Get_Auth(elm->GetRealm(), auth_params.port,
					auth_params.url.GetAttribute(URL::KPath).CStr(), type, scheme,
					context_id);
				if(elm == elm1 || elm != elm1->AliasOf())
					elm = NULL; // digests with domains cannot be reused, even if the realm is the same
			}
#endif

			if(elm && !url_rep->CheckSameAuthorization(elm, proxy))
			{
				url_rep->Authenticate(elm);
				if(elm->GetScheme() == AUTH_SCHEME_HTTP_BASIC)
					auth_params.servername->Add_Auth(elm, auth_params.url.GetAttribute(URL::KPath));
				return AUTH_HEADER_FINISHED;
			}
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
			else if(ntlm_enabled && (actual_scheme == AUTH_SCHEME_HTTP_NEGOTIATE || 
				    actual_scheme == AUTH_SCHEME_HTTP_NTLM))
			{
				continue;
			}
			else if(actual_scheme == AUTH_SCHEME_HTTP_BASIC && 
				(negotiate_header || ntlm_header))
			{
				continue; // Don't use Basic if NTLM/Negotiate is offered
			}
#endif
#ifdef HTTP_DIGEST_AUTH
			else if(elm && actual_scheme == AUTH_SCHEME_HTTP_DIGEST)
			{
				Parameters *stale = params->GetParameterByID(HTTP_Authentication_Stale);

				if(stale && stale->GetValue().CompareI("true")==0)
				{
					int next_redir_count = url_rep->GetAttribute(URL::KRedirectCount);
					if( next_redir_count > 20)
					{
						url_rep->FailAuthenticate(AUTH_GENERAL_FAILURE);
						return AUTH_HEADER_FAILURE;
					}
					url_rep->SetAttribute(URL::KRedirectCount, next_redir_count+1);

					if(elm->IsAlias())
						elm = elm->AliasOf();

					if(!elm)
					{
						url_rep->FailAuthenticate(AUTH_GENERAL_FAILURE);
						return AUTH_HEADER_FAILURE;
					}

					OpStatus::Ignore(((Digest_AuthElm *) elm)->Init_Digest(params, auth_params.servername));
					url_rep->Authenticate(elm);
					return AUTH_HEADER_FINISHED;
				}
			}

			if(actual_scheme == AUTH_SCHEME_HTTP_DIGEST)
			{
				param = params->GetParameterByID(HTTP_Authentication_Algorithm);
				if(param)
				{
#ifdef EXTERNAL_DIGEST_API
					External_Digest_Handler *temp_digest = NULL;
#endif

					if(CheckDigestAlgs(param->Value()
#ifdef EXTERNAL_DIGEST_API
									, auth_params.servername, temp_digest
#endif
									) == DIGEST_UNKNOWN)
					{
						OP_ASSERT(0);
						continue;
					}
#ifdef EXTERNAL_DIGEST_API
					auth_params.digest_handler = temp_digest;
#endif
				}
#if defined EXTERNAL_DIGEST_API && (defined EXTERNAL_DIGEST_API_TEST || defined SELFTEST)
				else
				{
					External_Digest_Handler *temp_digest = NULL;

					if(CheckDigestAlgs("md5", auth_params.servername, temp_digest) == DIGEST_UNKNOWN)
						continue;
					auth_params.digest_handler = temp_digest;
				}
#endif
			}
#endif
			if(actual_scheme == AUTH_SCHEME_HTTP_BASIC)
			{
				if(!basic_header)
					basic_header = header;
				continue;
			}
			break;
		}
	}
	while((header = auth_headers->GetHeaderByID(header_id, header)) != NULL);

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	if(!header && (negotiate_header || ntlm_header) &&
		(proxy ||  authinfo->session_based_support ||
				(authinfo->connect_host == authinfo->origin_host &&
				authinfo->connect_port == authinfo->origin_port) ||
				type == URL_HTTPS
				) ) // No NTLM/Negotiate for origin server if a proxy is used
	{
		if(negotiate_header)
		{
			auth_params.scheme = AUTH_SCHEME_HTTP_NEGOTIATE;
			header = negotiate_header;
			if(ntlm_header)
			{
				auth_params.ntlm_auth_arguments = ntlm_header->SubParameters();
				auth_params.scheme = AUTH_SCHEME_HTTP_NTLM_NEGOTIATE;
			}
		}
		else
		{
			header = ntlm_header;
			auth_params.scheme = AUTH_SCHEME_HTTP_NTLM;
		}

		params = header->SubParameters();
		if(proxy)
			auth_params.scheme |= AUTH_SCHEME_HTTP_PROXY;
		auth_params.message = "The server has requested your domain\\username and password."; // TODO: Language string.
	}
	else
#endif
	if(!header)
		header = basic_header;

	if(!header)
	{
		url_rep->FailAuthenticate(proxy ? AUTH_NO_PROXY_METH : AUTH_NO_METH);
		return AUTH_HEADER_FAILURE;
	}

	auth_params.auth_arguments = params;
	const char *realm_value = realm ? realm->Value() : NULL;
	if (realm_value)
	{
		CharsetDetector d;
		d.PeekAtBuffer(realm_value, op_strlen(realm_value));
		if (d.GetDetectedCharsetIsNotUTF8())
			realm_value = NULL;
	}
	// Use realm value as authentication message if it's valid.
	auth_params.message = realm_value;

#ifdef EXTERNAL_DIGEST_API
	if(auth_params.digest_handler != NULL)
	{
		OpString username, password;
		BOOL got_credentials = FALSE;
		OpParamsStruct1  ext_digest_param;

		ext_digest_param.digest_uri = (auth_params.proxy ? NULL : auth_params.url.GetAttribute(URL::KName_Escaped).CStr());
		ext_digest_param.servername = auth_params.servername->Name();
		ext_digest_param.port_number = auth_params.port;
		ext_digest_param.method = NULL;
		ext_digest_param.realm = auth_params.message;
		ext_digest_param.username = NULL;
		ext_digest_param.password = NULL;
		ext_digest_param.nonce = NULL;
		ext_digest_param.nc_value = NULL;
		ext_digest_param.cnonce = NULL;
		ext_digest_param.qop_value = NULL;
		ext_digest_param.opaque = NULL;

		if(OpStatus::IsError(auth_params.digest_handler->GetCredentials(got_credentials, username, password, &ext_digest_param)))
			return AUTH_HEADER_FAILURE;

		if(got_credentials)
		{
			Authenticate((username.CStr() ? username.CStr() : UNI_L("")), (password.CStr() ? password.CStr() : UNI_L("")), auth_params.url.Id());
			username.Wipe();
			password.Wipe();
			return AUTH_HEADER_FAILURE;
		}
		username.Wipe();
		password.Wipe();
		return AUTH_HEADER_FINISHED;
	}
#endif
	return AUTH_HEADER_DIALOG;
}

void Authentication_Manager::NotifyAuthenticationRequired(Window *window, OpAuthenticationCallback* callback)
{
	OP_NEW_DBG("Authentication_Manager::NotifyAuthenticationRequired()", "auth");

	AuthenticationCallback* cb = static_cast<AuthenticationCallback*>(callback);
	OpRequestImpl *request = cb->GetURL().GetRep()->GetDataStorage()->GetOpRequestImpl();
	if (!cb->GetURL().GetAttribute(URL::KUseGenericAuthenticationHandling) && request)
	{
		request->AuthenticationRequired(callback);
	}
	else
	{
		WindowCommander* window_commander = window ? window->GetWindowCommander() : NULL;
		if(window_commander)
			window_commander->GetLoadingListener()->OnAuthenticationRequired(window_commander, callback);
		else
			static_cast<WindowCommanderManager*>(g_windowCommanderManager)->GetAuthenticationListener()->OnAuthenticationRequired(callback);
	}}

OP_STATUS Authentication_Manager::NotifyAuthenticationRequired(waiting_url* wurl, Window *window, struct authenticate_params &auth_params)
{
	OP_NEW_DBG("Authentication_Manager::NotifyAuthenticationRequired()", "auth");
	RETURN_IF_ERROR(wurl->windows.Add(window));

	OpAutoPtr<AuthenticationCallback> callback(OP_NEW(AuthenticationCallback, (wurl, window, auth_params)));
	RETURN_OOM_IF_NULL(callback.get());
	RETURN_IF_ERROR(callback->Init(auth_params));
	RETURN_IF_ERROR(wurl->callbacks.Add(callback.get()));

	OP_DBG(("created callback %p for waiting url %p; id: %p", callback.get(), wurl, wurl->url.Id()));
	NotifyAuthenticationRequired(window, callback.release());
	return OpStatus::OK;
}

waiting_url* Authentication_Manager::FindWaitingURL(URL_ID authid)
{
	OtlList<waiting_url*>::Iterator itr = waiting_urls.Begin();
	OtlList<waiting_url*>::ConstIterator end = waiting_urls.End();
	for(; itr != end; ++itr)
	{
		waiting_url* wurl = *itr;
		if(wurl->handled && wurl->url.Id() == authid)
			return wurl;
	}
	return NULL;
}

OP_STATUS Authentication_Manager::StartAuthentication()
{
	OP_NEW_DBG("Authentication_Manager::StartAuthentication()", "auth");
 	if(waiting_urls.IsEmpty())
		return OpStatus::OK;

	OtlList<waiting_url*>::Iterator itr = waiting_urls.Begin();
	OtlList<waiting_url*>::ConstIterator end = waiting_urls.End();
	for(; itr != end; ++itr)
	{
		waiting_url* wurl = *itr;
		OP_ASSERT(wurl);
		if(wurl->handled)
			continue;

		wurl->handled = TRUE;

		authenticate_params &auth_params = wurl->auth_params;
		auth_params.url = wurl->url;
		auth_params.first_window = NULL;
		auth_params.auth_arguments = NULL;
		auth_params.servername= NULL;
		auth_params.port= 0;
		auth_params.message = NULL;
		auth_params.type = (wurl->reauth ? AUTH_WRONG : AUTH_NEEDED);
		auth_params.scheme = AUTH_SCHEME_HTTP_UNKNOWN;
		auth_params.proxy = FALSE;

		URLType type = (URLType) auth_params.url.GetAttribute(URL::KType);
		URL_Rep *url_rep = auth_params.url.GetRep();
		URL_DataStorage *url_ds = url_rep->GetDataStorage();

#ifndef NO_FTP_SUPPORT
		if(type == URL_FTP && url_ds->http_data == NULL)
		{
			auth_params.servername = (ServerName *) auth_params.url.GetAttribute(URL::KServerName, (const void*) 0);

			auth_params.port = auth_params.url.GetAttribute(URL::KServerPort);
			auth_params.scheme = AUTH_SCHEME_FTP;
			if(auth_params.port == 0)
				auth_params.port = 21;

			AuthElm *elm = auth_params.servername->Get_Auth("",auth_params.port,NULL, type, AUTH_SCHEME_FTP, url_rep->GetContextId());

			if(elm && !url_rep->CheckSameAuthorization(elm, FALSE))
			{
				url_rep->Authenticate(elm);
				goto finish;
			}
		}
		else
#endif // NO_FTP_SUPPORT
		if(type == URL_HTTP || type == URL_HTTPS || type == URL_FTP || type == URL_Gopher || type == URL_WAIS)
		{
			OP_ASSERT(url_ds);
			URL_HTTP_ProtocolData *hptr = url_ds->http_data;
			OP_ASSERT(hptr);
			OP_ASSERT(hptr->authinfo);

			int stat = SetUpHTTPAuthentication(auth_params, hptr->authinfo, HTTP_Header_Proxy_Authenticate, TRUE);
			if(stat == NO_AUTH_HEADER)
				stat = SetUpHTTPAuthentication(auth_params, hptr->authinfo, HTTP_Header_WWW_Authenticate, FALSE);

			if(stat == NO_AUTH_HEADER)
				url_rep->FailAuthenticate(AUTH_NO_METH);

			if(stat != AUTH_HEADER_DIALOG)
				goto finish; // We're finished, either no authentication will be made, or it's been sent without bothering the user
		}
		else
#ifdef WEBSOCKETS_SUPPORT
		if (type == URL_WEBSOCKET)
		{
			authdata_st *authData = url_rep->GetAuthData();

			int stat = SetUpHTTPAuthentication(auth_params, authData, HTTP_Header_Proxy_Authenticate, TRUE);

			if(stat == NO_AUTH_HEADER)
				url_rep->FailAuthenticate(AUTH_NO_METH);

			if(stat != AUTH_HEADER_DIALOG)
				goto finish; // We're finished, either no authentication will be made, or it's been sent without bothering the user
		}
		else
#endif
			goto finish;

#ifdef URL_DISABLE_INTERACTION
		if(url_rep->GetAttribute(URL::KBlockUserInteraction))
			goto finish;
#endif
		if(url_ds || url_rep->GetMessageHandlerList())
		{
			MsgHndlList::Iterator itr = url_rep->GetMessageHandlerList()->Begin();
			MsgHndlList::ConstIterator end = url_rep->GetMessageHandlerList()->End();
			for(; itr != end; ++itr)
			{
				MessageHandler* mh = (*itr)->GetMessageHandler();
				if (!mh)
					continue;

				URL_HTTP_ProtocolData *hptr = url_ds ? url_ds->GetHTTPProtocolData() : NULL;
				if (hptr != NULL && hptr->flags.auth_has_credetentials)
				{
					if (!hptr->flags.auth_credetentials_used)
					{
						OpString username;
						OpString password;

						RETURN_IF_ERROR(username.SetFromUTF8(url_rep->GetAttribute(URL::KHTTPUsername)));
						RETURN_IF_ERROR(password.SetFromUTF8(url_rep->GetAttribute(URL::KHTTPPassword)));
						Authenticate(username, password, auth_params.url.Id(), TRUE);
						AuthenticationDialogFinished(auth_params.url.Id());
						hptr->flags.auth_credetentials_used = TRUE;
						urlManager->MakeUnique(url_rep);
					}
					else
					{
						hptr->flags.auth_credetentials_used = FALSE;
						url_rep->FailAuthenticate(AUTH_USER_CANCELLED);
						AuthenticationDialogFinished(auth_params.url.Id());
					}
					break; // No need to go to the next message handler since we have already
						   // finished authenticating this url. Also the waiting_url has been
						   // deleted so continuing will cause a crash.
				}
				else
				{
					Window* window = mh->GetWindow();
					/* Note: AuthenticationDialogFinished() might be triggered
					 * by this call (e.g. when the authentication is cancelled
					 * immediately), so 'wurl' may no longer be valid after this
					 * call. */
					if (OpStatus::IsError(NotifyAuthenticationRequired(wurl, window, auth_params)))
						return FALSE;
				}
			}
		}

		continue;

finish:;
		AuthenticationDialogFinished(wurl->url.Id());
		break;
	}
	return OpStatus::OK;
}

void Authentication_Manager::RemoveWaitingUrl(OtlList<waiting_url*>::Iterator wurl_iterator)
{
	if (waiting_urls.IsAtEnd(wurl_iterator))
		return;
	waiting_url* wurl = *wurl_iterator;
	if (wurl == NULL)
	{
		waiting_urls.Remove(wurl_iterator);
		return;
	}

	// noone knows why do we need this :
	wurl->authenticated = TRUE;

	// notify and remove all associated callbacks
	wurl->BroadcastAuthenticationCancelled();

	CleanupAuthenticateParams(wurl->auth_params);
	waiting_urls.Remove(wurl_iterator);
	OP_DELETE(wurl);
	*wurl_iterator = NULL;

}

void Authentication_Manager::RemoveWaitingUrl(waiting_url* wurl)
{
	if (wurl != NULL)
		RemoveWaitingUrl(waiting_urls.FindItem(wurl));
}

void Authentication_Manager::AuthenticationDialogFinished(URL_ID authid)
{
	OP_NEW_DBG("Authentication_Manager::AuthenticationDialogFinished()", "auth");
	waiting_url* wurl = FindWaitingURL(authid);
	if(wurl)
	{
		OP_DBG(("finished authentication for waiting url %p; id: %p", wurl, wurl->url.Id()));
		RemoveWaitingUrl(wurl);
	}

	if(!waiting_urls.IsEmpty())
		g_main_message_handler->PostMessage(MSG_URL_NEED_AUTHENTICATION,0,0);
}

void Authentication_Manager::Authenticate(const OpStringC &user_name1, const OpStringC &user_password1, URL_ID authid, BOOL authenticate_once)
{
	OP_NEW_DBG("Authentication_Manager::Authenticate()", "auth");
	waiting_url* wurl = FindWaitingURL(authid);
	if(!wurl)
		return;

	wurl->authenticated = TRUE;
	authenticate_params &auth_params = wurl->auth_params;

	OP_STATUS op_err, op_err_pass;

	if(auth_params.url.IsEmpty())
		return;

	URLType type = (auth_params.proxy ? URL_HTTP : (URLType) auth_params.url.GetAttribute(URL::KType));
	URL_Rep *url_rep = auth_params.url.GetRep();

#ifndef AUTH_RESTRICTED_USERNAME_STORAGE
	if(auth_params.servername != NULL)
	{
		if (auth_params.proxy)
			auth_params.servername->SetLastProxyUserName(user_name1);
		else
			auth_params.servername->SetLastUserName(user_name1);
	}
#endif

	char *user_name = NULL;
	char *user_password = NULL;

	op_err      = user_name1.UTF8Alloc(&user_name);
	op_err_pass = user_password1.UTF8Alloc(&user_password);
	if(OpStatus::IsMemoryError(op_err) || OpStatus::IsMemoryError(op_err_pass) )
	{
		url_rep->FailAuthenticate(AUTH_MEMORY_FAILURE);
		return;
	}

	const char *realm = auth_params.message ? auth_params.message : "";
	AuthElm *auth_elm = NULL;
	switch(auth_params.scheme & ~AUTH_SCHEME_HTTP_PROXY)
	{
	case AUTH_SCHEME_FTP:
	case AUTH_SCHEME_HTTP_BASIC:
		{
			Basic_AuthElm *auth_elm1 =
				OP_NEW(Basic_AuthElm, (auth_params.port,auth_params.scheme, type, authenticate_once)); // FIXME:OOM (not reported)
			if(auth_elm1)
			{
				if(OpStatus::IsSuccess(auth_elm1->Construct(realm, user_name, user_password)))
				{
					auth_elm = auth_elm1;
					auth_elm->SetContextId(url_rep->GetContextId());
				}
				else
					OP_DELETE(auth_elm1); // FIXME:OOM (not reported)
			}

			break;
		}
#ifdef HTTP_DIGEST_AUTH
	case AUTH_SCHEME_HTTP_DIGEST :
		{
			Digest_AuthElm *auth_elm1 =
				OP_NEW(Digest_AuthElm, (auth_params.scheme, type, auth_params.port, authenticate_once)); // FIXME:OOM (not reported)

			if(auth_elm1)
			{
				if(OpStatus::IsSuccess(auth_elm1->Construct(auth_params.auth_arguments, realm, user_name, user_password, auth_params.servername)))
				{
					auth_elm = auth_elm1;
					auth_elm->SetContextId(url_rep->GetContextId());
				}
				else
					OP_DELETE(auth_elm1); // FIXME:OOM (not reported)
			}
			break;
		}
#endif
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	case AUTH_SCHEME_HTTP_NTLM:
	case AUTH_SCHEME_HTTP_NEGOTIATE:
	case AUTH_SCHEME_HTTP_NTLM_NEGOTIATE:
		{
			extern AuthElm *CreateNTLM_Element(authenticate_params &auth_params,const char *user_name, const char *user_password);

			auth_elm = CreateNTLM_Element(auth_params, user_name, user_password);
			auth_elm->SetContextId(url_rep->GetContextId());
			break;
		}
#endif
	}

	if(user_name)
		OPERA_cleanse_heap(user_name, op_strlen(user_name));
	OP_DELETEA(user_name);
	user_name = NULL;
	if(user_password)
		OPERA_cleanse_heap(user_password, op_strlen(user_password));
	OP_DELETEA(user_password);
	user_password = NULL;

	if(!auth_elm)
	{
		url_rep->FailAuthenticate(AUTH_MEMORY_FAILURE);
		return;
	}

	OpStringC8 param_path = auth_params.url.GetAttribute(URL::KPath);

#ifdef HTTP_DIGEST_AUTH
	if(auth_params.scheme == AUTH_SCHEME_HTTP_DIGEST)
	{
		AuthElm *auth_elm1;

		Parameters *domains = auth_params.auth_arguments->GetParameterByID(HTTP_Authentication_Domain, PARAMETER_ASSIGNED_ONLY);
		if(domains && domains->GetParameters(PARAM_SEP_WHITESPACE | PARAM_ONLY_SEP | PARAM_NO_ASSIGN))
		{
			ParameterList *domainlist = domains->SubParameters();
			Parameters *current_domain;
			BOOL matched= FALSE;

			current_domain = domainlist->First();
			if(!current_domain)
			{
				matched = TRUE;
				auth_params.servername->Add_Auth(auth_elm, "");
			}
			else
			{
				while(current_domain)
				{
					const char *dom = current_domain->Name();
					if(dom)
					{
						char *temp;
						URL url;

						temp = (char*) op_strchr(dom,'#');
						if(temp)
							*temp = '\0';

						url = g_url_api->GetURL(auth_params.url, dom);

						URLType url_type = (URLType) url.GetAttribute(URL::KType);

						if(!url.IsEmpty() && (url_type == URL_HTTP || url_type == URL_HTTPS))
						{
							ServerName *server = (ServerName *) url.GetAttribute(URL::KServerName, (const void*) 0);
							unsigned short port = url.GetAttribute(URL::KResolvedPort);

							auth_elm1 = NULL;
							OpStringC8 url_path = url.GetAttribute(URL::KPath);

							if(auth_params.servername == server && port == auth_params.port && url_path.HasContent() && param_path.HasContent() )
							{
								size_t lenpath = url_path.Length();
								if((size_t) param_path.Length() >= lenpath &&
									param_path.Compare(url_path.CStr(), lenpath) == 0)
								{
									matched = TRUE;
									auth_elm1 = auth_elm;
								}
							}
							if(!auth_elm1)
								auth_elm1 = OP_NEW(AuthElm_Alias, (auth_elm, port, url_type));

							if(auth_elm1)
								server->Add_Auth(auth_elm1, url_path);
							else
								g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						}
					}
					current_domain = current_domain->Suc();
				}
			}

			if(!matched)
			{
				OP_DELETE(auth_elm);
				url_rep->FailAuthenticate(AUTH_NO_DOMAIN_MATCH);
				return;
			}
		}
		else
		{
			auth_params.servername->Add_Auth(auth_elm, "/");
		}
	}
	else
#endif
	{
		auth_params.servername->Add_Auth(auth_elm, param_path);
	}

	// Cleaning up
	auth_params.auth_arguments = NULL;
	auth_params.message = NULL;
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	auth_params.ntlm_auth_arguments=NULL;
#endif

	auth_params.url.GetRep()->Authenticate(auth_elm);
	if (authenticate_once)
	{
		if (auth_elm->InList())
			auth_elm->Out();
		OP_DELETE(auth_elm);
		auth_elm = NULL;
	}
	wurl->BroadcastAuthenticationCancelled();

	OtlList<waiting_url*>::Iterator itr = waiting_urls.Begin();
	OtlList<waiting_url*>::ConstIterator end = waiting_urls.End();
	for(; itr != end; ++itr)
	{
		waiting_url* wurl = *itr;
		if(wurl->handled && !wurl->authenticated)
		{
			AuthElm *elm = wurl->auth_params.servername->Get_Auth(
				wurl->auth_params.message, wurl->auth_params.port, NULL,
				(wurl->auth_params.proxy ? URL_HTTP: (URLType)wurl->url.GetAttribute(URL::KRealType)),
				wurl->auth_params.scheme,
				wurl->auth_params.url.GetRep()->GetContextId());

			if(elm && ! wurl->url.GetRep()->CheckSameAuthorization(elm, wurl->auth_params.proxy))
			{
				wurl->url.GetRep()->Authenticate(elm);
				RemoveWaitingUrl(itr);
			}

			if (auth_elm && auth_elm->GetAuthenticateOnce())
			{
				auth_elm->Out();
				OP_DELETE(auth_elm);
				auth_elm = NULL;
			}
		}
	}
}

void Authentication_Manager::CancelAuthentication(URL_ID authid)
{
	OP_NEW_DBG("Authentication_Manager::CancelAuthentication()", "auth");
	waiting_url* wurl = FindWaitingURL(authid);
	if(!wurl)
		return;

	wurl->BroadcastAuthenticationCancelled();
	wurl->authenticated = TRUE;
	authenticate_params &auth_params = wurl->auth_params;

	auth_params.auth_arguments = NULL;
	auth_params.message = NULL;
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	auth_params.ntlm_auth_arguments=NULL;
#endif
#ifndef AUTH_RESTRICTED_USERNAME_STORAGE
	if(auth_params.servername != NULL)
	{
		OpStringC emptystring;

		if (auth_params.proxy)
			auth_params.servername->SetLastProxyUserName(emptystring);
		else
			auth_params.servername->SetLastUserName(emptystring);
	}
#endif
	if(!auth_params.url.IsEmpty())
		auth_params.url.GetRep()->FailAuthenticate(AUTH_USER_CANCELLED);
}

void Authentication_Manager::StopAuthentication(URL_Rep *url)
{
	OP_NEW_DBG("Authentication_Manager::StopAuthentication()", "auth");
	if(!url)
		return;
	waiting_url* wurl = FindWaitingURL(url);
	if(wurl)
		RemoveWaitingUrl(wurl);
}

void Authentication_Manager::Clear_Authentication_List()
{
	ServerName *server;
	server = g_url_api->GetFirstServerName();

	while(server)
	{
		server->Clear_Authentication_List();
		server = g_url_api->GetNextServerName();
	}
}

/* static */
void Authentication_Manager::CleanupAuthenticateParams(authenticate_params& auth_params)
{
	OP_NEW_DBG("Authentication_Manager::CleanupAuthenticateParams()", "auth");
	auth_params.auth_arguments = NULL;
	auth_params.message = NULL;
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	auth_params.ntlm_auth_arguments = NULL;
#endif
	if(!auth_params.url.IsEmpty())
	{
		URL_DataStorage *url_ds = auth_params.url.GetRep()->GetDataStorage();
		if(url_ds && url_ds->http_data && url_ds->http_data->authinfo)
		{
			OP_DELETE(url_ds->http_data->authinfo);
			url_ds->http_data->authinfo = NULL;
		}
		auth_params.url = URL();
		auth_params.servername = NULL;
	}
}

#ifdef _DEBUG
Debug& operator<<(Debug& d, const waiting_url& wurl)
{
	d << "waiting_url(";
	bool first = true;
#define DEBUG_PRINT_IF(a)						\
	do {										\
		if (wurl.a)								\
		{										\
			if (!first) d << "+";				\
			d << #a;							\
			first = false;						\
		}										\
	} while (0)
	DEBUG_PRINT_IF(reauth);
	DEBUG_PRINT_IF(handled);
	DEBUG_PRINT_IF(authenticated);
#undef DEBUG_PRINT_IF
	if (!first) d << ";";
	return d << wurl.auth_params << ")";
}

Debug& operator<<(Debug& d, const struct authenticate_params& a)
{
	d << "authenticate_params(scheme:";
	bool first = true;
#define DEBUG_PRINT_FLAG(f, name)											\
	do { if (f) { if (!first) d << "+"; d << name; first = false; } } while(0)
	DEBUG_PRINT_FLAG(a.scheme & AUTH_SCHEME_FTP, "ftp");
	DEBUG_PRINT_FLAG(a.scheme & OBSOLETE_AUTH_SCHEME_NEWS, "news");
	DEBUG_PRINT_FLAG(a.scheme & AUTH_SCHEME_HTTP_PROXY, "proxy");
#undef DEBUG_PRINT_FLAG
	if (!first) d << "+";
	switch (a.scheme & AUTH_SCHEME_HTTP_MASK) {
#define CASE_PRINT(f, name) case f: d << name; break
		CASE_PRINT(AUTH_SCHEME_HTTP_UNKNOWN, "unknown");
		CASE_PRINT(AUTH_SCHEME_HTTP_BASIC, "basic");
		CASE_PRINT(AUTH_SCHEME_HTTP_DIGEST, "digest");
		CASE_PRINT(AUTH_SCHEME_HTTP_NTLM, "ntlm");
		CASE_PRINT(AUTH_SCHEME_HTTP_NEGOTIATE, "negotiate");
		CASE_PRINT(AUTH_SCHEME_HTTP_NTLM_NEGOTIATE, "ntlm-negotiate");
#undef CASE_PRINT
	default: d << "other:" << static_cast<unsigned int>(a.scheme & AUTH_SCHEME_HTTP_MASK);
	}
	d << ";first window:" << a.first_window
	  << ";server:" << a.servername->Name() << ":" << a.port;
	if (a.proxy) d << "(proxy)";
	return d << ";message:\"" << a.message << "\")";
}
#endif // _DEBUG

#endif // _ENABLE_AUTHENTICATE
