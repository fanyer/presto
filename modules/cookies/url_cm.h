/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _URL_CM_H_
#define _URL_CM_H_

// URL Cookie Management

#include "modules/util/simset.h"
#include "modules/util/tree.h"
#include "modules/url/url2.h"
#include "modules/url/url_sn.h"

class DataFile;
class Comm;
class ServerName;
class OpFile;

#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#endif

#if defined _ASK_COOKIE
#include "modules/windowcommander/OpWindowCommander.h"
#endif // defined _ASK_COOKIE

#if defined(_ASK_COOKIE)
void WarnCookieDomainError(const char *url, const char *cookie, BOOL no_ip_address= FALSE);
void WarnCookieDomainError(Window* win, URL_Rep *url, const char *cookie, BOOL no_ip_address = FALSE);
#endif // _ASK_COOKIE

/**
   actual cookie type for cookie - even if bogus version is passed
*/
enum CookieType { NETSCAPE_COOKIE, COOKIE2_COOKIE, UNKNOWN_COOKIE };

class Cookie_Item_Handler : public Link
#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
	, public ProtocolComm
#endif
#if defined _ASK_COOKIE
	, public OpCookieListener::AskCookieContext
#endif
{
public:
	URL_InUse	 url;
	Window *first_window;
	URL_CONTEXT_ID context_id;
	OpStringS8 request;
	
	OpStringS8 name;
	OpStringS8 value;
	OpStringS8 domain;
	OpStringS8 recvd_domain;
	OpStringS8 path;
	OpStringS8 recvd_path;
	
	time_t expire;
	time_t last_used;
	time_t last_sync;
	
	OpStringS8 comment; // Encoded with UTF-8 (rfc2965, 3.2.2)
	OpStringS8 comment_URL;
	OpStringS8 port;
	unsigned short *portlist;
	unsigned port_count;
	
	int version;
	CookieType cookie_type; ///< actual cookie type
	
	struct cookie_item_flags_st{
		BYTE secure:1;
		BYTE httponly:1;
		BYTE set_fromhttp:1;
		BYTE recv_dom:1;
		BYTE recv_path:1;
		BYTE delete_cookie:1;
		BYTE discard_at_exit:1;
		BYTE only_server:1;
		BYTE protected_cookie:1;
		BYTE accept_updates:1;
		BYTE accepted_as_third_party:1;
		BYTE full_path_only:1;
		BYTE illegal_path:1;
		BYTE assigned:1;  // TRUE if the cookie has the "=" (even if no value has been set). See CORE-30416 and CORE-25786
		BYTE have_password:1;
		BYTE have_authentication:1;
#ifdef _SSL_USE_SMARTCARD_
		BYTE smart_card_authenticated:1;
#endif
#if defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
		BYTE do_dns_lookup:1;
		BYTE waiting_for_dns:1;
		BYTE do_not_set:1;
#endif
#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
		BYTE http_done_same_port:1;
		BYTE force_direct_lookup:1;
#endif
	}flags;

#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
	Comm *lookup;
	URL_InUse lookup_url;
#endif
	ServerName_Pointer domain_sn;

private:
	void InternalInit();

public:
	
	Cookie_Item_Handler();
	virtual ~Cookie_Item_Handler();
	
	void Clear();
#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
	virtual OP_STATUS SetCallbacks(MessageObject* master, MessageObject* parent);
#endif
#ifdef _OPERA_DEBUG_DOC_
	void GetMemUsed(DebugUrlMemory &debug);
#endif

#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	BOOL Start_Lookup();
	BOOL Is_LookingUp();
#endif

#if defined _ASK_COOKIE
	// OpCookieListener::AskCookieContext implementation

	virtual const uni_char *GetServerName() {return url->GetAttribute(URL::KUniHostName).CStr();}
	virtual const char* GetName() const { return name.CStr(); }
	virtual const char* GetValue() const { return value.CStr(); }
	virtual const char* GetDomain() const { return domain.CStr(); }
	virtual const char* GetPort() const { return port.CStr(); }
	virtual const char* GetPath() const { return path.CStr(); }
	virtual const char* GetComment() const { return comment.CStr(); }
	virtual const char* GetCommentURL() const { return comment_URL.CStr(); }

	virtual const unsigned short* GetPortList() const { return portlist; }
	virtual unsigned GetPortCount() const { return port_count; }

	virtual time_t GetExpire() const { return expire; }
	virtual time_t GetLastUsed() const { return last_used; }
	virtual time_t GetLastSync() const { return last_sync; }

	virtual bool GetSecure() const { return flags.secure ? true : false; }

	virtual void OnAskCookieDone(OpCookieListener::CookieAction response);
	virtual void OnAskCookieCancel();
#endif
};

//class Cookie;
//class CookiePath;
class CookieDomain;
class AuthenticationCookie ;

class Cookie : public Link
{
  private:
	OpStringS8 name;
	OpStringS8 value;
	time_t	expires;
	time_t	last_used;
	time_t	last_sync;
	OpStringS8 comment;
	OpStringS8 commenturl;
	OpStringS8 received_domain;
	OpStringS8 received_path;
	OpStringS8 port;
	unsigned short *portlist,portlen;
	unsigned int version; // 0 = original version; 1 = Set-Cookie2
	CookieType cookie_type; ///< actual cookie type

	struct cookie_flag_st
	{
		BYTE secure:1;
		BYTE httponly:1;
		BYTE port_received:1;
		BYTE discard_at_exit:1;
		BYTE only_server:1;
		BYTE protected_cookie:1;
		BYTE accept_updates:1;
		BYTE full_path_only:1;
		BYTE sync_added:1;
		BYTE accepted_as_third_party:1;
		BYTE assigned: 1;  // TRUE if the cookie has the "=" (even if no value has been set). See CORE-30416 and CORE-25786
		BYTE have_password:1;
		BYTE have_authentication:1;
#ifdef _SSL_USE_SMARTCARD_
		BYTE smart_card_authenticated:1;
#endif
	} flags;

#ifdef NEED_URL_COOKIE_ARRAY
    CookieDomain * domain;
    CookiePath   * path;
#endif // NEED_URL_COOKIE_ARRAY

	Cookie(Cookie_Item_Handler *params, OpStringS8 &rpath);
	void InternalInit(Cookie_Item_Handler *params, OpStringS8 &rpath);
	void InternalDestruct();
  public:
	static Cookie* CreateL(Cookie_Item_Handler *params);
	virtual ~Cookie();

	Cookie* Suc() const { return (Cookie*) Link::Suc(); }

	const OpStringC8 &Name() { return name; }
	const OpStringC8 &Value() { return value; }
	time_t		Expires() const { return expires; }
	BOOL		Secure() { return flags.secure; }
	BOOL		HTTPOnly() { return flags.httponly; }
	BOOL		FullPathOnly(){return flags.full_path_only;}
	// TRUE if the cookie has the "=" (even if no value has been set). See CORE-30416 and CORE-25786
	BOOL		Assigned() { return flags.assigned; }

	const OpStringC8 &Comment() { return comment; }
	const OpStringC8 &CommentURL() { return commenturl; }
	unsigned int Version() { return version; }
	/**
	   get actual cookie type - may differ from Version(), that will
	   return actual version passed by server regardless of sanity
	 */
	CookieType GetCookieType() const { return cookie_type; }
	const OpStringC8 &Received_Domain() { return received_domain; }
	const OpStringC8 &Received_Path() { return received_path; }
	const OpStringC8 &Port() { return port; }
	BOOL PortReceived() { return flags.port_received; }
	BOOL CheckPort(unsigned short portnum);
	BOOL SendOnlyToServer() const { return flags.only_server; }
	BOOL ProtectedCookie() const { return flags.protected_cookie; }
	BOOL DiscardAtExit() const { return flags.discard_at_exit; }
	BOOL AcceptUpdates() const{return flags.accept_updates;}
	BOOL GetHavePassword() const{return flags.have_password;}
	BOOL GetHaveAuthentication() const{return flags.have_authentication;}
	void SetHavePassword(BOOL flag) { flags.have_password = (flag? 1: 0); }
	void SetHaveAuthentication(BOOL flag) { flags.have_authentication = (flag? 1: 0); }
#ifdef _SSL_USE_SMARTCARD_
	BOOL GetHaveSmartCardAuthentication() const{return flags.smart_card_authenticated;}
	void SetHaveSmartCardAuthentication(BOOL flag) { flags.smart_card_authenticated = (flag? 1: 0); }
#endif
	BOOL GetAcceptedAsThirdParty(){return flags.accepted_as_third_party;}

	time_t		GetLastUsed() { return last_used; }
	void		SetLastUsed(time_t t_used) { last_used = t_used; }
	time_t		GetLastSync() { return last_sync; }
	void		SetLastSync(time_t t_sync) { last_sync = t_sync; }

#if defined(QUICK_COOKIE_EDITOR_SUPPORT)
	BOOL SetName( const OpStringC8 &name );
	BOOL SetValue( const OpStringC8 &name );
	void SetExpires(time_t t_expires) { expires = t_expires; }
	void BuildCookieEditorListL(CookieDomain* domain, CookiePath* path);
#endif // QUICK_COOKIE_EDITOR_SUPPORT

#ifdef _OPERA_DEBUG_DOC_
	void GetMemUsed(DebugUrlMemory &debug);
#endif

#ifdef NEED_URL_COOKIE_ARRAY
    OP_STATUS BuildCookieList(Cookie ** cookieArray, int * pos, CookieDomain * domainHolder, CookiePath * pathHolder, BOOL is_export);
    CookieDomain * GetDomain(){return domain;}
    CookiePath   * GetPath(){return path;}
	BOOL		SyncAdded() { return flags.sync_added; }
#endif // NEED_URL_COOKIE_ARRAY

	BOOL Persistent(time_t this_time) const { return (!DiscardAtExit() && Expires() && Expires() > this_time); }

#ifdef DISK_COOKIES_SUPPORT
	void FillDataFileRecordL(DataFile_Record &rec);
#endif // DISK_COOKIES_SUPPORT
};

class CookiePath : public Tree
{
  private:
	OpStringS8	path;
	AutoDeleteHead cookie_list;

	Cookie*	LocalGetCookie(const OpStringC8 &nme);

	CookiePath(OpStringS8 &pth);
  public:
	static CookiePath* CreateL(const OpStringC8 &pth);
	static CookiePath* Create(const OpStringC8 &pth, OP_STATUS &op_err);
	virtual ~CookiePath();

	CookiePath* LastChild() const { return (CookiePath*) Tree::LastChild(); }
	CookiePath* Pred() const { return (CookiePath*) Tree::Pred(); }
	CookiePath* Suc() const { return (CookiePath*) Tree::Suc(); }
	CookiePath* Parent() const { return (CookiePath*) Tree::Parent(); }

	/** Get full cookie path, including trailing and leading slashes 
	 *
	 *  @param fullpath target OpString8
	 *  @return OP_STATUS
	 */
	OP_STATUS GetFullPath(OpString8& fullpath) const;

	void Clear();

	const OpStringC8 &PathPart() const { return path; }

	Cookie *GetCookieL(Cookie_Item_Handler *params, BOOL create);
	CookiePath*	GetCookiePathL(const char* pth, BOOL create, BOOL &is_full_path);
	CookiePath *GetNextPrefix();
	int			GetCookieRequest(time_t this_time, BOOL is_secure, 
		BOOL is_server, 
		unsigned short port,
		int &version,
		int &max_version,
		BOOL third_party_only,
		BOOL already_have_password,
		BOOL already_have_authentication,
		BOOL &have_password,
		BOOL &have_authentication,
		BOOL is_full_path,
		char* buf, int buf_len,
		BOOL for_http,
		BOOL allow_dolllar_cookies,
		BOOL& seen_cookie2);

#ifdef DISK_COOKIES_SUPPORT
	void	ReadCookiesL(DataFile &fp, unsigned long ver_no);
	// dry_run can be used to estimate length of cookies instead of writing them down to file
	// when dry_run is TRUE, no writing to file will be done but method will return aggregated size of cookies
	size_t	WriteCookiesL(DataFile &fp, time_t this_time, BOOL dry_run=FALSE);
#endif // DISK_COOKIES_SUPPORT

	BOOL	HasCookies(time_t this_time);
	int		GetCookiesCount();

	Cookie*	GetLeastRecentlyUsed(time_t last_used, time_t this_time);
	BOOL FreeUnusedResources();

#ifdef _DEBUG_COOKIES_
	void	DebugWritePath(FILE* fp);
	void	DebugWriteCookies(FILE* fp);
#endif

#ifdef _USE_PREAUTHENTICATION_
	CookiePath* GetAuthenticationPath(const char* pth, BOOL create);
	AuthenticationCookie* GetAuthenticationCookie(unsigned short pport ,OpStringC8 rlm, AuthScheme pscheme, URLType typ, BOOL create, URL_CONTEXT_ID id);
	AuthenticationCookie *GetAuthenticationCookie(unsigned short port,AuthScheme scheme, URLType typ, URL_CONTEXT_ID id);
#endif

#if defined(QUICK_COOKIE_EDITOR_SUPPORT)
	void BuildCookieEditorListL(CookieDomain* domain);
#endif // QUICK_COOKIE_EDITOR_SUPPORT
#ifdef NEED_URL_COOKIE_ARRAY
    OP_STATUS BuildCookieList(Cookie ** cookieArray, int * pos, char * pathstr, CookieDomain * domainHolder, BOOL is_export, BOOL match_subpaths=FALSE);
    OP_STATUS RemoveCookieList(char * pathstr, char * namestr);
#endif // NEED_URL_COOKIE_ARRAY

	void DeleteAllCookies();
	void RemoveNonPersistentCookies();
#ifdef WIC_COOKIES_LISTENER
	void IterateAllCookies();
#endif // WIC_COOKIES_LISTENER
#ifdef _OPERA_DEBUG_DOC_
	void GetMemUsed(DebugUrlMemory &debug);
#endif

#ifdef _SSL_USE_SMARTCARD_
	/** Return TRUE if this item is empty and can be deleted */
	BOOL CleanSmartCardAuthenticatedCookies();
#endif

};

class CookieDomain : public Tree
{
  private:
	OpStringS8		domain;
	ServerName_Pointer domain_sn;
	CookiePath*	path_root;

	CookieDomain(OpStringS8 &dom, CookiePath *cookie_path);
  public:
	static CookieDomain* CreateL(const OpStringC8 &dom);
	virtual ~CookieDomain();

	CookieDomain* LastChild() const { return (CookieDomain*) Tree::LastChild(); };
	CookieDomain* Pred() const { return (CookieDomain*) Tree::Pred(); };
	CookieDomain* Suc() const { return (CookieDomain*) Tree::Suc(); };
	CookieDomain* Parent() const { return (CookieDomain*) Tree::Parent(); };

	CookiePath*	GetPathRoot() { return path_root; };
	const OpStringC8 &DomainPart() { return domain; };
	ServerName *GetFullDomain();
#ifdef COOKIE_ENABLE_FULLDOMAIN2_API
	void GetFullDomain(char *domstring, unsigned int maxlen, BOOL exclude_local_domain=FALSE);
#endif
	CookiePath*	GetCookiePathL(const char* dom, const char* pth,
			BOOL *is_server, 
			BOOL create, CookieDomain* &lowest_domain, BOOL &is_full_path);
#ifdef DISK_COOKIES_SUPPORT
	void	ReadCookiesL(DataFile &fp, unsigned long ver_no);
	// dry_run can be used to estimate length of cookies instead of writing them down to file
	// when dry_run is TRUE, no writing to file will be done but method will return aggregated size of cookies
	size_t	WriteCookiesL(DataFile &fp, time_t this_time, BOOL dry_run=FALSE);
#endif // DISK_COOKIES_SUPPORT

	BOOL	HasCookies(time_t this_time);
	int		GetCookiesCount();

	void	Clear();

	Cookie*	GetLeastRecentlyUsed(time_t last_used, time_t this_time, BOOL current_only=FALSE);

	void DeleteAllCookies(BOOL delete_filters);
	void RemoveNonPersistentCookies();
#ifdef WIC_COOKIES_LISTENER
	void IterateAllCookies();
#endif // WIC_COOKIES_LISTENER

	BOOL	FreeUnusedResources();

#if defined(QUICK_COOKIE_EDITOR_SUPPORT)
	void BuildCookieEditorListL();
#endif // QUICK_COOKIE_EDITOR_SUPPORT

#ifdef NEED_URL_COOKIE_ARRAY
	OP_STATUS BuildCookieList(Cookie ** cookieArray, int * pos, char * domainstr, char * pathstr, BOOL is_export, BOOL match_subpaths);
	OP_STATUS RemoveCookieList(char * domainstr, char * pathstr, char * namestr);
#endif //NEED_URL_COOKIE_ARRAY

#ifdef _DEBUG_COOKIES_
	void	DebugWriteDomain(FILE* fp);
	void	DebugWriteCookies(FILE* fp);
#endif

#ifdef _OPERA_DEBUG_DOC_
	void GetMemUsed(DebugUrlMemory &debug);
#endif
};

#ifdef __cplusplus
extern "C" {
#endif
int uint_cmp(const void *arg1,const void *arg2);
#ifdef __cplusplus
}
#endif



// Cookie File Tags
#ifndef MSB_VALUE
#define MSB_VALUE 0x80000000
#endif

#define URL_COOKIE_FILE_VERSION				0x2000

#define TAG_COOKIE_DOMAIN_ENTRY				0x0001 // Record sequence
#define TAG_COOKIE_PATH_ENTRY				0x0002 // Record sequence
#define TAG_COOKIE_ENTRY					0x0003 // Record sequence
#define TAG_COOKIE_DOMAIN_END			   (0x0004 | MSB_VALUE) // no content, End of domainsequence
#define TAG_COOKIE_PATH_END				   (0x0005 | MSB_VALUE) // no content, End of path sequence

#define TAG_COOKIE_NAME						0x0010 // string
#define TAG_COOKIE_VALUE					0x0011 // string
#define TAG_COOKIE_EXPIRES					0x0012 // unsigned, truncated
#define TAG_COOKIE_LAST_USED				0x0013 // unsigned, truncated
#define TAG_COOKIE_COMMENT					0x0014 // string
#define TAG_COOKIE_COMMENT_URL				0x0015 // string
#define TAG_COOKIE_RECVD_DOMAIN				0x0016 // string
#define TAG_COOKIE_RECVD_PATH				0x0017 // string
#define TAG_COOKIE_PORT						0x0018 // string
#define TAG_COOKIE_SECURE				   (0x0019 | MSB_VALUE) // True if present
#define TAG_COOKIE_VERSION					0x001A // unsigned, truncated
#define TAG_COOKIE_ONLY_SERVER			   (0x001B | MSB_VALUE) // True if present
#define TAG_COOKIE_PROTECTED			   (0x001C | MSB_VALUE) // True if present
#define TAG_COOKIE_PATH_NAME				0x001D // string
#define TAG_COOKIE_DOMAIN_NAME				0x001E // string
#define TAG_COOKIE_SERVER_ACCEPT_FLAG		0x001F // 8 bit value, values are:
				// 1 All cookies from this domain are accepted
				// 2 No cookies from this domain are accepted
				// 3 All cookies from this server are accepted. Overrides 1 and 2 for higher level domains automatics (show cookie will work)
				// 4 No cookies from this server are accepted. Overrides 1 and 2 for higher level domains (show cookie will work)
#define TAG_COOKIE_NOT_FOR_PREFIX		   (0x0020 | MSB_VALUE) // True if present (Cookie not to be used for prefix path matches)
#define TAG_COOKIE_DOMAIN_ILLPATH			0x0021  // 8bit value, values are
				// 1 Cookies with illegal paths are refused automatically for this domain
				// 2 Cookies with illegal paths are accepted automatically for this domain
#define TAG_COOKIE_HAVE_PASSWORD			   (0x0022 | MSB_VALUE) // True is present
#define TAG_COOKIE_HAVE_AUTHENTICATION		   (0x0023 | MSB_VALUE) // True is present
#define TAG_COOKIE_ACCEPTED_AS_THIRDPARTY	   (0x0024 | MSB_VALUE) // True is present
#define TAG_COOKIE_SERVER_ACCEPT_THIRDPARTY		0x0025	// 8 bit value, values are:
				// 1 All thirdparty cookies from this domain are accepted
				// 2 No thirdpartycookies from this domain are accepted
				// 3 All thirdparty cookies from this server are accepted. Overrides 1 and 2 for higher level domains automatics (show cookie will work)
				// 4 No thirdparty cookies from this server are accepted. Overrides 1 and 2 for higher level domains (show cookie will work)
#define TAG_COOKIE_DELETE_COOKIES_ON_EXIT		0x0026	// 8 bit value, values are:
				// 1 No cookies in this domain is to be discarded on exit by default
				// 2 All cookies in this domain is to be discarded on exit by default
#define TAG_COOKIE_HTTP_ONLY			   (0x0027 | MSB_VALUE) // True if present
#define TAG_COOKIE_LAST_SYNC				0x0028 // unsigned, truncated
#define TAG_COOKIE_ASSIGNED			   (0x0029 | MSB_VALUE) // True if the cookie is assigned. For example, it's TRUE for "c=3" and "c=", but not for "c"

#endif // !_URL_CM_H_
