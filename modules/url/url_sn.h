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

// URL Server Name
// URL Name

#ifndef _URL_SN_H_
#define _URL_SN_H_

#include "modules/util/smartptr.h"
#include "modules/util/linkobjectstore.h"
#include "modules/util/adt/opvector.h"
#include "modules/url/url_enum.h"
#include "modules/url/url_id.h"
#include "modules/url/url_scheme2.h"
#include "modules/pi/network/OpCertificate.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/url/url_debugmem.h"
#include "modules/probetools/src/probetimer.h"

#include "modules/auth/auth_sn.h"

#include "modules/util/adt/bytebuffer.h"

class AuthElm;
class CookiePath;
class URL_RelRep;
class Sequence_Splitter;
typedef Sequence_Splitter ParameterList;


class SSL_varvector24;
template<class T, int len_size, int max_len> class SSL_LoadAndWriteableListType;
typedef SSL_LoadAndWriteableListType<SSL_varvector24, 3, 0xffffff> SSL_varvector24_list;

class SSL_Port_Sessions;
class SSL_AcceptCert_Item;
class OpSocketAddress;

class ServerName;
typedef OpSmartPointerNoDelete<ServerName> ServerName_Pointer;

enum CookieDNS_status {CookieDNS_NotDone, CookieDNS_Failed, CookieDNS_Succeded};

// Defined here because the class must be defined in ServerName

class OpSocketAddressContainer : public Link
{
private:
	OpSocketAddress* m_SocketAddress;

	struct {
		/** Did we try to connect to this address?
		 *  True also if the connection is currently in progress. If this flag
		 *  is true, we should not try this address again on the next
		 *  connection attempt, but rather choose another address.
		 */
		UINT tried:1;

		/** Is the connection to this address currently in progress?
		 *  If "in_progress" is true, "tried" must also be true.
		 */
		UINT in_progress:1;

		/** Did the last connection attempt succeed?
		 *  If "last_try_succeeded" is true, "tried" must also be true
		 *  and "last_try_failed" must be false.
		 */
		UINT last_try_succeeded:1;

		/** Did the last connection attempt fail?
		 *  If "last_try_failed" is true, "tried" must also be true
		 *  and "last_try_succeeded" must be false.
		 */
		UINT last_try_failed:1;
	} info;

public:
	OpSocketAddressContainer(OpSocketAddress* a_socketaddress);
	/** Destroys the item using the factory */
	virtual ~OpSocketAddressContainer();

	OpSocketAddress* GetSocketAddress(){return m_SocketAddress;}
	//OpSocketAddress* Release();

	BOOL IsTried()            const { return info.tried;              }
	BOOL IsInProgress()       const { return info.in_progress;        }
	BOOL IsLastTrySucceeded() const { return info.last_try_succeeded; }
	BOOL IsLastTryFailed()    const { return info.last_try_failed;    }

	void SetTried(BOOL flag)            { info.tried              = !!flag; }
	void SetInProgress(BOOL flag)       { info.in_progress        = !!flag; }
	void SetLastTrySucceeded(BOOL flag) { info.last_try_succeeded = !!flag; }
	void SetLastTryFailed(BOOL flag)    { info.last_try_failed    = !!flag; }

	void ClearFlags() { op_memset(&info, 0, sizeof(info)); }

	OpSocketAddressContainer *Suc() const{return (OpSocketAddressContainer *) Link::Suc();}
	OpSocketAddressContainer *Pred() const{return (OpSocketAddressContainer *) Link::Pred();}
};

/** Head implementation for OpSocketAddress Lists 
 */
class OpSocketAddressHead : public AutoDeleteHead
{
public:
	OP_STATUS AddSocketAddress(OpSocketAddress* a_socketaddress);
	OpSocketAddressContainer *First() const { return (OpSocketAddressContainer *) Head::First(); }
	OpSocketAddressContainer *Last()  const { return (OpSocketAddressContainer *) Head::Last();  }
};

#ifdef _CERTICOM_SSL_SUPPORT_
class CerticomAcceptedCertificate : public Link
{
public:
	char * m_cert_id;
	int m_cert_id_length;
	int m_accept_type;
	CerticomAcceptedCertificate(char * id, int id_length, int type)
		: m_cert_id(id), m_cert_id_length(id_length), m_accept_type(type)
		{}

	virtual ~CerticomAcceptedCertificate()
		{
			OP_DELETEA(m_cert_id);
		}
};
#endif // _CERTICOM_SSL_SUPPORT_



#ifdef TRUST_RATING
struct FraudListItem : public Link
{
	OpString value;
	int id;
};

enum ServerSideFraudType
{
	SERVER_SIDE_FRAUD_UNKNOWN,
	SERVER_SIDE_FRAUD_PHISHING = 1,
	SERVER_SIDE_FRAUD_MALWARE = 2
};

struct Advisory : public Link
{
	OpString homepage;
	OpString advisory;
	OpString text;
	unsigned int src;
	unsigned int type;  // ServerSideFraudType
};
#endif // TRUST_RATING


/** This class represent each individual servername used by a URL, proxy or cookie manager item.
 *
 *	Theobject contains information about the following:
 *
 *		The server's name, 8-bit and unichar 
 *		The IP-address(es) for the server (when lookup have been performed)
 *		Authentication information
 *		Cookie filters
 *		SSL secure session information and manually accepted certificates
 */
class ServerName : public HashedLink, public OpReferenceCounter, public ServerNameAuthenticationManager
{
	friend class URL_Manager;

public:
#if defined PUBSUFFIX_ENABLED
	enum DomainType {
		/** Unknown domain status */
		DOMAIN_UNKNOWN,
		/** Valid values start after this */
		DOMAIN_VALID_START,
		/** A Normal domain */
		DOMAIN_NORMAL,
		/** A registry domain */
		DOMAIN_REGISTRY,
		/** A TLD */
		DOMAIN_TLD,
		/** Valid values end before this */
		DOMAIN_VALID_END,
		/** Wait for Update */
		DOMAIN_WAIT_FOR_UPDATE
	};
#endif

private:
	/** The 8-bit version of the server's name */
	OpStringS8	server_name;

	/** The 16-bit version of the server's name */
	OpStringS16 server_nameW;

	/** List of the individual components of the servername.
	 *  Index 0 is the toplevel domain, index 1 the next-to-highest domain etc. 
	 *	
	 *	E.g. www.opera.com  is organized like this
	 *
	 *	  0: com
	 *	  1: opera
	 *	  2: www
	 */
	OpAutoVector<OpStringS8> servername_components;

	/** Pointer to this servernames parent domain, only non-NULL when needed
	  * E.g. for the name "www.opera.com" this will point to "opera.com"
	  */
	ServerName_Pointer parent_domain;

	/** List of URL_Scheme_Authority_Components for this servername */
	URL_Scheme_Authority_List	scheme_and_authority_list;

		/** The currently used socket address */
	OpSocketAddress* m_SocketAddress;
	/** The list of alternative socketaddresses */
	OpSocketAddressHead m_SocketAddressList;
	/** When do these IP addresses expire, and we have to look them up again (10 minute lifetime) */
	time_t m_AddressExpires; 
	/** What type of addresses are this servername using. First socket address sets the rule */
	OpSocketAddressNetType nettype, attempted_nettype;

#if defined PUBSUFFIX_ENABLED
	DomainType domain_type;
#endif
	
	struct{
		/** Is this a non-localhost host with the IP address 127.0.0.1 ?*/
		UINT is_local_host:1;
		/** Are we currently looking up the IP address for this server ? */
		UINT is_resolving_host:1;
		/** Is this a Wap server (wap.mydomain.com)? */
		UINT is_wap_server : 1;
		/** Did the HTTP simulated namelookup succeed? Used to validate cookies */
		UINT http_lookup_succeded:1;

#ifdef OPERA_PERFORMANCE
		/** Did we prefetch this address? */
		UINT prefetchDNSStarted:1;
#endif // OPERA_PERFORMANCE
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
		/** Is this server trusted to set the never-flush cache directive? (used in extended cache management)*/
		UINT	never_flush_trust:2;
#endif
		UINT checked_parent_domain:1;
		UINT never_expires:1;
		UINT idna_restricted:1;
		UINT connection_restricted:1; // restrict number of persistent connections to 1 (one) for this server
#ifdef PERFORM_SERVERNAME_LENGTH_CHECK
		/** This server name is too long or has at least one component that is too long according to RFC 1035 */
		UINT has_illegal_length:1;
#endif // PERFORM_SERVERNAME_LENGTH_CHECK
		/** Did the DNS lookup find that this is a cross network request? */
		UINT cross_network:1;

#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
		UINT strict_transport_security_support:1;
		UINT strict_transport_security_include_subdomains:1;
#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY

	} info;

#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	time_t strict_transport_security_expires;
#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY



#ifdef TRUST_RATING
	TrustRating trust_rating;
	BOOL trust_rating_bypassed;
	AutoDeleteHead	fraud_list;
	AutoDeleteHead	regexp_list;
	time_t trust_expires;
	Head advisory_list;
#endif // TRUST_RATING
	
	/** How many connections are currently active towards this server ? */
	unsigned int connection_count;
	

#ifdef _ASK_COOKIE
	/** Cookie acceptance filter setting */
	COOKIE_MODES cookie_status;

	/** Illegal path cookie filter setting */
	COOKIE_ILLPATH_MODE cookie_accept_illegal_path;

	/** Delete cookie on exit? */
	COOKIE_DELETE_ON_EXIT_MODE cookie_delete_on_exit;
#endif
	/** Thirdparty cookie filter setting */
	COOKIE_MODES third_party_cookie_status;
	
	/** Special UA string to use with this server */
	UA_BaseStringId use_special_UA;
	
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	/** List of proxy ports on this address and when they were last active */
	AutoDeleteHead proxies_last_active;
#endif
#if defined(_EXTERNAL_SSL_SUPPORT_)
	OpAutoVector<ByteBuffer> Session_Accepted_Certificate_hashes;
#elif defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_) 
	/** List of SSL session states. One pr. port; coded in modules/libssl/ssl_sess.cpp */
	AutoDeleteHead	Session_data; // List of class SSL_Port_Sessions
#endif
	
	/** List of Usernames in URLs that can be passed directly, without further challenge */
	AutoDeleteHead pass_usernameURLs_automatically;

	/** When does the cookie DNS lookup for this name expire (24 hours) */
	time_t cookie_dns_expires;
	
	/** Have we performed a DNS lookup for this domain? */
	CookieDNS_status cookiedns_lookup_done;
	
#if defined(_EXTERNAL_SSL_SUPPORT_) 
	/** ID of connection presently connecting securely with external SSL library.
	 *	The Connection currently connecting to a secure server sets this, and all 
	 *	other connections must wait until it is finished, before the next may try
	 *	TODO: Remove for Hurricane 
	 */
	int id_currently_connecting_securely;
#endif
	
#ifdef URL_BLOCK_FOR_MULTIPART
	/** Is this server currently sending a page containg mixed page 
	 *	and images (multipart mixed, with content-location headers)?
	 *	If so, all other URLs pointing to this server should wait until all 
	 *	the bodyparts have been decoded, to avoid duplicate loads 
	 */
	int blocked_by_multipart_count;
#endif

#ifdef  SOCKS_SUPPORT
	/** Implies this server is to be accessed through the bellow specified SOCKS proxy (see CT-387) */
	ServerName_Pointer  socks_server_name;
	UINT                socks_server_port;
#endif//SOCKS_SUPPORT


private:
	/** Constructor */
	ServerName();
	
	/** Construct for the given name */
	OP_STATUS Construct(const OpStringC8 &name);
	
	/** internal initialization (used to work around duplicate code generated by gcc) */
	void InternalInit();
	
	/** Internal destructor (used to work around triplicate code generated by gcc) */
	void InternalDestruct();
	

	OP_STATUS CheckSocketAddress();

public:
	/**
	 *	Creates a ServerName object. 
	 *	
	 *	@param	server_name		the created object. server_name is set to NULL on failure,
	 *							DON'T use this as a way to check for errors, check the
	 *		                    return value instead!
	 *	@param	name
	 *	@return	OP_STATUS		Always check this.
	 */
	static ServerName *Create(const OpStringC8 &name, OP_STATUS &op_err);

	/** Destructor */
	virtual ~ServerName();
	
	//***************************************************************

	void FreeUnusedResources(BOOL all);
	BOOL SafeToDelete();

	/** Set the first socket address (IP address) */
	OP_STATUS SetSocketAddress(OpSocketAddress* aSocketAddress);
	/** Get the current SocketAddress */
	OpSocketAddress* SocketAddress();
	/** Is this host currently resolved? */
	BOOL IsHostResolved();
	/** Set this Host's IP address from the string */
	OP_STATUS SetHostAddressFromString(const OpStringC& aIPAddrString);
	void DEPRECATED(SetHostAddressFromStringL(const OpStringC& aIPAddrString));

	/** Add the alternative socket address. NOTE: Takes over aSocketAddress */
	OP_STATUS AddSocketAddress(OpSocketAddress* aSocketAddress);
	/** Remove all registered IPaddresses, but leaves current item */
	void ClearSocketAddresses(); 
	/** Reset the last_try_failed flag of all the alternative socketaddresses */
	void ResetLastTryFailed();
	/** Get next socket address to try.
	 *  @return socket address or NULL, if no next address can be found.
	 */
	OpSocketAddress* GetNextSocketAddress() const;
	/** Get alternative socket address for simultaneous connection.
	 *  This function is suitable for Happy Eyeballs algorithm. It will
	 *  attempt to find IPv4 address, if the current "main" address is IPv6,
	 *  and vice versa.
	 *  @return alternative socket address or NULL, if no alternative can be found.
	 */
	OpSocketAddress* GetAltSocketAddress(OpSocketAddress::Family aCurrentFamily) const;
	/** Get OpSocketAddressContainer, containing particular OpSocketAddress.
	 *  @return container or NULL, if not found.
	 */
	OpSocketAddressContainer* GetContainerByAddress(const OpSocketAddress* aSocketAddress) const;
	/** Mark address as connecting, i.e. set appropriate flags. */
	void MarkAddressAsConnecting(const OpSocketAddress* aSocketAddress);
	/** Mark address as succeeded and move it to the beginning of the address list. */
	void MarkAddressAsSucceeded(const OpSocketAddress* aSocketAddress);
	/** Mark address as failed and move it to the end of the address list. */
	void MarkAddressAsFailed(const OpSocketAddress* aSocketAddress);
	/** Mark all addresses as untried.
	 *  Resetting the "tried" flag is needed so that the next attempt
	 *  to connect to this host will still try the addresses that were tried
	 *  in this connection round. This function is intended to be called
	 *  when Comm object stopped trying to connect to the host. It can happen
	 *  on successful connection, final failure or some kind of interruption.
	 *  Last-try-successful/unsuccessful flags will not be reset, this way
	 *  we can try a better address first on the next connection round.
	 */
	void MarkAllAddressesAsUntried();

#ifdef _DEBUG
	static void LogSocketAddress(const char* prefix, OpSocketAddress* aSocketAddress);
	void LogAddressInfo();
#endif // _DEBUG

	/** Which nettype is this server? Unknown if not yet looked up */
	OpSocketAddressNetType GetNetType() const{return nettype;}
	/** Sets which nettype this server is. */
	void SetNetType(OpSocketAddressNetType a_nettype){nettype=a_nettype;}
	/** Which attempted nettype is this server? Unknown if not yet looked up */
	OpSocketAddressNetType GetAttemptedNetType() const{return attempted_nettype;}
	/** Sets which attempted nettype this server is. */
	void SetAttemptedNetType(OpSocketAddressNetType a_nettype){attempted_nettype=a_nettype;}
	void SetCrossNetwork(BOOL value) { info.cross_network = value; }
	BOOL GetCrossNetwork() { return info.cross_network; }

//***************************************************************

	/** Is this a 127.0.0.1 IP address, and not a locahost name? */
	BOOL GetIsLocalHost() const{ return info.is_local_host;}
	/** Update the flag showing that this a 127.0.0.1 IP address, and not a locahost name? */
	void SetIsLocalHost(BOOL val){ info.is_local_host = (val ? TRUE : FALSE);}

	/** Is this a wap.domain.com server ? */
	BOOL GetIsWAPServer() const{ return info.is_wap_server;}

	/** Return the unicode version of this server name */
	virtual const uni_char*	UniName() const { return server_nameW.CStr(); }
	const OpStringC &GetUniName() const { return server_nameW; }
	/** Return the 8-bit version of this server's name */
	const char *Name() const { return server_name.CStr(); }
	const OpStringC8 &GetName() const { return server_name; }

	/** Return the server's name, for use in the sorting of the list */
	virtual	const char* LinkId() { return Name(); }

	/** Check and if necessary create the name components */
	void CheckNameComponents();

	/** Return the number of components in the filename */
	uint32 GetNameComponentCount() {CheckNameComponents(); return servername_components.GetCount();}

	/** Return a OpStringC8 object referencing the given name component item of the servername, empty if no component */
	OpStringC8 GetNameComponent(uint32 item);

	/** Return a pointer to the servername object for the parent domain.
	 *	NULL if no parent domain, but may also be NULL in case of OOM
	 */
	ServerName *GetParentDomain();
	void UnsetParentDomain(){info.checked_parent_domain = FALSE; parent_domain = NULL;}

	/** Get a pointer for the most specific common domain name the two domains, NULL if no common domain exists
	 *	NULL may also be returned in case of OOM	
	 *
	 *  E.g. the common domain of "www.opera.com" and "web.opera.com" is "opera.com", for
	 *	"www.microsoft.com" and "www.opera.com" it is "com", and for "www.opera.com" and 
	 *	"www.opera.com" the is no common domain, i.t a NULL pointer
	 *
	 *	@param	other	The other domain
	 *	@return ServerName *	The domain common between this name and the other name, may be NULL
	 */
	ServerName *GetCommonDomain(ServerName *other);

	/** Find and, optionally, create a URL_Scheme_Authority_Components object that
	 *	matches the URL_Scheme_Authority_Components structure argument 
	 *	The components object is updated with the found/created pointer
	 */
	URL_Scheme_Authority_Components *FindSchemeAndAuthority(OP_STATUS &op_err, URL_Name_Components_st *components, BOOL create=FALSE){return scheme_and_authority_list.FindSchemeAndAuthority(op_err, components, create);}

	/** How many connections are active against this server? */
	unsigned int GetConnectCount() { return connection_count; }
	/** Increment the number of connections that are currently active against this server */
	unsigned int IncConnectCount() { return ++connection_count; }
	/** Decrement the number of connections that are currently active against this server */
	unsigned int DecConnectCount() { return (connection_count ? --connection_count : 0); }

#ifdef _ASK_COOKIE
	/** Set the cookie filter setting */
	void SetAcceptCookies(COOKIE_MODES mod){cookie_status = mod;}
	/** Get the cookie filter setting */
	COOKIE_MODES GetAcceptCookies(BOOL follow_domain=FALSE, BOOL first = TRUE);
	/** Set the illegal path filter */
	void SetAcceptIllegalPaths(COOKIE_ILLPATH_MODE mod){cookie_accept_illegal_path = mod;}
	/** Get the illegal path filter */
	COOKIE_ILLPATH_MODE GetAcceptIllegalPaths(BOOL follow_domain=FALSE, BOOL first = TRUE);

	/** Set Domain specific empty on exit */
	void SetDeleteCookieOnExit(COOKIE_DELETE_ON_EXIT_MODE mod){cookie_delete_on_exit = mod;}
	COOKIE_DELETE_ON_EXIT_MODE GetDeleteCookieOnExit(BOOL follow_domain=FALSE, BOOL first = TRUE);
#endif
	/** Set the third party filter setting */
	void SetAcceptThirdPartyCookies(COOKIE_MODES mod){third_party_cookie_status = mod;}
	/** Get the third party filter setting */
	COOKIE_MODES GetAcceptThirdPartyCookies(BOOL follow_domain=FALSE, BOOL first = TRUE);
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	/** The proxy on the given port failed to respond */
	void SetFailedProxy(unsigned short port);
	/** Can we use the proxy on the given port failed? */
	BOOL MayBeUsedAsProxy(unsigned short port);
#endif
#if defined (_CERTICOM_SSL_SUPPORT_) || defined(_EXTERNAL_SSL_SUPPORT_)
		OP_STATUS AddAcceptedCertificate(OpCertificate *accepted_certicate);
		BOOL IsCertificateAccepted(OpCertificate *accepted_certicate);
#elif defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_) 
	/** Get the secure session handler for the given port */
	SSL_Port_Sessions *GetSSLSessionHandler(unsigned short port);

	/** Get the first session handler */
	SSL_Port_Sessions *GetFirstSSLSessionHandler();

	/** The server on the given port is an IIS 4 or IIS 5 server; special consideration is needed for these servers */
	void SetSSLSessionIIS4(unsigned short port, BOOL is_IIS5);
	/** Forget used client certificates after 5 minutes */
	void CheckTimeOuts(time_t curtime);

	/** Forget all manually accepted certificates */
	void ForgetCertificates();

	/** Remove or invalidate all secure sessions */
	void InvalidateSessionCache(BOOL url_shutdown = FALSE);
	/** Invalidate secure sessions for the given certificate */
	void InvalidateSessionForCertificate(SSL_varvector24 &);

	void FreeUnusedSSLResources(BOOL all);
#ifdef _SSL_USE_SMARTCARD_
	/** Invalidate smartcard authenticated sessions for the given certificate */
	BOOL InvalidateSmartCardSession(const SSL_varvector24_list &);
#endif
#endif
	/** Delete all sensitive data (passwords, certificates, secure sessions etc. */
	void RemoveSensitiveData();
	
	/** Update the user agent overrides */
	void UpdateUAOverrides();

	/** Get the current user agentoverrid */
	UA_BaseStringId GetOverRideUserAgent();

	/** Set the current user agentoverrid */
	void SetOverRideUserAgent(UA_BaseStringId uaid) {use_special_UA = uaid;}

	/** Are we permitted to pass usernames (and passwords) embedded in the URL to this server without challenging the user ? */
	BOOL GetPassUserNameURLsAutomatically(const OpStringC8 &p_name);

	/** Set whether or not we are permitted to pass usernames (and passwords) embedded in the URL to this server without challenging the user ? */
	OP_STATUS SetPassUserNameURLsAutomatically(const OpStringC8 &p_name);
	
#ifdef _OPERA_DEBUG_DOC_
	void GetMemUsed(DebugUrlMemory &debug);
#endif
	
	/** Specify that the HTTP "DNS" lookup through proxy worked, or not */
	void SetHTTP_Lookup_Succceded(BOOL flag){info.http_lookup_succeded = (flag ? TRUE : FALSE);}
	/** Did the previous HTTP "DNS" lookup through proxy work? */
	BOOL GetHTTP_Lookup_Succceded() const {return info.http_lookup_succeded;}
	
	/** Update the lookup status of this servername. If TRUE, we are currently looking up the address of this servername */
	void	SetIsResolvingHost(BOOL flag){info. is_resolving_host = (flag ? TRUE : FALSE);}
	/** If TRUE, we are currently looking up the address of this servername */
	BOOL	IsResolvingHost(){return info.is_resolving_host;}
	
	/**
	* Sets Time To Live Value 
	*
	* @param attl The number of seconds until this address must be rechecked
	*/
	void SetTTL(unsigned int aTtl);
	
	/**
	* Set Never Expires so that the address do not have to be rechecked.
	*/
	void SetNeverExpire() { info.never_expires = TRUE; }
	
	/**
	* Gets the expiration time for this address
	*
	* return time_t - the time when the address expires (0 if not set)
	*/
	time_t Expires() const;

#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	/**
	 * Check if this server supports strict transport security (STS).
	 *
	 * The function will also check time out, and turn off STS if
	 * max-age has been passed.
	 *
	 * If true, any http request to this server will be redirected to
	 * https before the request is sent.
	 *
	 * http://tools.ietf.org/html/draft-hodges-strict-transport-sec-02
	 *
	 * This function is normally only called internally by URL loading code.
	 *
	 * @return TRUE if the server is registered to support strict transport security,
	 * 				and the registration has not expired.
	 *
	 */
	BOOL SupportsStrictTransportSecurity();

	/**
	 * Get the expiry date of the registration of strict transport security
	 * 
	 * @return expiry date in seconds.
	 */
	time_t GetStrictTransportSecurityExpiry() const { return strict_transport_security_expires; }

	/*
	 * Check if strict transport security should be used in subdomains.
	 * @return TRUE if subdomains are included.
	 */
	BOOL GetStrictTransportSecurityIncludeSubdomains() const { return info.strict_transport_security_include_subdomains; }

	/**
	 * Register this server to support strict transport security.
	 * 
	 * @param strict_transport_security TRUE if the server supports strict transport security.
	 * @param include_subdomains If TRUE, all subdomains of this server domain will also support STS.
	 * @param expiry_date The date in seconds for which the STS expires.
	 */
	void SetStrictTransportSecurity(BOOL strict_transport_security, BOOL include_subdomains, time_t expiry_date);

#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY

	/** When does the DNS lookup for cookie verification expire? */
	time_t GetCookieDNSExpires(){return cookie_dns_expires;}
	/** Set when the DNS lookup for cookie verification expires */
	void SetCookieDNSExpires(time_t t){cookie_dns_expires = t;}
	/** Set the cookie DNS lookup status for this servername */
	void SetCookieDNS_Lookup_Done(CookieDNS_status status){cookiedns_lookup_done = status;}
	/** Get the cookie DNS lookup status for this servername */
	CookieDNS_status GetCookieDNS_Lookup_Done() const {return cookiedns_lookup_done;}
	
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	/** Update the neverflush trust status for this servername, if needed, and return the current setting or the result */
	TrustedNeverFlushLevel TestAndSetNeverFlushTrust();
	
	/* Get the neverflush trust level */
	TrustedNeverFlushLevel	GetNeverFlushTrust() const{ return (TrustedNeverFlushLevel) info.never_flush_trust;}
#endif

#ifdef TRUST_RATING
	void SetTrustTTL(unsigned int aTtl);
	TrustRating GetTrustRating();
	void SetTrustRating(TrustRating new_trust_rating) { trust_rating = new_trust_rating; }
	BOOL GetTrustRatingBypassed() { return trust_rating_bypassed;  }
	void SetTrustRatingBypassed(BOOL new_trust_rating_bypassed) { trust_rating_bypassed = new_trust_rating_bypassed; }
	void AddUrlL(const OpStringC &url, int id);

	/** Checks if fraud item exists at the given url.
	 *
	 *  Fraud item is related to the given url when it has same value or url
	 *  is a subpath of fraud item url.
	 *
	 *  @param url url which will be checked if it has fraud entry related to it.
	 *  @param fraud_item_url will be filled with pointer to the fraud item
	 *         url STORED IN THIS OBJECT, if for given url exists related
	 *         fraud item otherwise it will be filled with NULL.
	 *
	 *  @return TRUE if for the given url fraud item has been found, otherwise
	 *          FALSE.
	 */
	BOOL IsUrlInFraudList(const OpStringC &url, const uni_char **fraud_item_url);

	void AddRegExpL(const OpStringC &regexp);
	FraudListItem *GetFirstRegExpItem();
	BOOL FraudListIsEmpty() { return fraud_list.Empty(); }
	void AddAdvisoryInfo(const uni_char *homepage_url, const uni_char *advisory_url, const uni_char *text, unsigned int src, unsigned int type);
	Advisory *GetAdvisory(const OpStringC &url);
#endif
	
#ifdef URL_BLOCK_FOR_MULTIPART
	/** How many mixed multiparts are currently being loaded? */
	BOOL GetBlockedByMultipartCount(unsigned short port) const{return blocked_by_multipart_count>0;}
	/** Register that a mixed multiparts is being loaded */
	void BlockByMultipart(unsigned short port){blocked_by_multipart_count ++;}
	/** Register that a mixed multiparts is no longer being loaded */
	void UnblockByMultipart(unsigned short port){if(blocked_by_multipart_count>0) blocked_by_multipart_count--;} 
#endif

	BOOL GetIsIDNA_Restricted() const{return info.idna_restricted;}
	/** Get the IDN security level required for this domain name. */
	enum URL_IDN_SecurityLevel GetIDNSecurityLevelL();

	void SetConnectionNumRestricted(BOOL flag){info.connection_restricted = (flag ? TRUE : FALSE);}
	BOOL GetConnectionNumRestricted(){return info.connection_restricted;}

#ifdef PERFORM_SERVERNAME_LENGTH_CHECK
	BOOL HasIllegalLength(){return info.has_illegal_length;}
#endif // PERFORM_SERVERNAME_LENGTH_CHECK

#ifdef SOCKS_SUPPORT
	ServerName*  GetSocksServerName() { return socks_server_name; }
	UINT         GetSocksServerPort() { return socks_server_port; }

	void         SetSocksServerName(ServerName* name) { socks_server_name = name; }
	void         SetSocksServerPort(UINT port) { socks_server_port = port; }

	/** Indicates the autoproxy config *does not* mention socks server */
	void         SetNoSocksServer() { socks_server_name = NULL; socks_server_port = 1; }
	/** Checks if autoproxy config *did not* mention socks server */
	BOOL         IsSetNoSocksServer() { return socks_server_name == NULL && socks_server_port == 1; }
#endif//SOCKS_SUPPORT

#if defined PUBSUFFIX_ENABLED
	/** Get domain type. If necessary initiate online download of the domain list.
	 *
	 *  If the returned value is DOMAIN_WAIT_FOR_UPDATE then the caller must wait for a 
	 *	MSG_PUBSUF_FINISHED_AUTO_UPDATE_ACTION message (the message will be posted for multiple 
	 *	retrievals, so the caller must be prepared to resume waiting.
	 */
	DomainType GetDomainTypeASync(){return GetDomainTypeASync(this);}
private:
	DomainType GetDomainTypeASync(ServerName *checking_domain);

public:

	void SetDomainType(DomainType val){domain_type = val;}

	DomainType GetCurrentDomainType(){return domain_type;};
	BOOL IsValidDomainType(){return (domain_type >=DOMAIN_VALID_START && domain_type <=DOMAIN_VALID_END);}
#endif
};

class ServerName_Store : public LinkObjectStore
{
public:
	ServerName_Store(unsigned int size);
	OP_STATUS Construct();
	virtual ~ServerName_Store();

	ServerName *GetServerName(const char *name, BOOL create = FALSE);

	ServerName*	GetFirstServerName() { return (ServerName*) GetFirstLinkObject(); }
	ServerName*	GetNextServerName() { return (ServerName*) GetNextLinkObject(); }
	void RemoveServerName(ServerName *ptr) { RemoveLinkObject(ptr); }

};


#endif //_URL_SN_H_
