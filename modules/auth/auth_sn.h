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

#ifndef _AUTH_SN_H_
#define _AUTH_SN_H_

#ifdef _ENABLE_AUTHENTICATE

class AuthElm;
class CookiePath;
class Sequence_Splitter;
typedef Sequence_Splitter ParameterList;
struct DebugUrlMemory;

class ServerNameAuthenticationManager
{
private:
	/** Last username used in authentication dialog */
	OpString username;
	/** Last username used in proxy authentication dialog */
	OpString proxyusername; // Last registered name
	
	/** List of proxy authentication credentials. Only one password per port */
	AutoDeleteHead authenticate_proxy;

#ifndef NO_FTP_SUPPORT
	/** List of FTP authentication credentials */
	AutoDeleteHead authenticate_ftp;
#endif // NO_FTP_SUPPORT
	
	/** List of HTTP authentication credentials. May have more than one password per port. */
	AutoDeleteHead authenticate_http;

	/** List of HTTPS authentication credentials. May have more than one password per port. */
	AutoDeleteHead authenticate_https;

#ifdef _USE_PREAUTHENTICATION_
	/** HTTP Authentication credential aliases by path */
	CookiePath *auth_paths_http;

	/** HTTPS Authentication credential aliases by path */
	CookiePath *auth_paths_https;
#endif

protected:
	ServerNameAuthenticationManager();
	~ServerNameAuthenticationManager();

private:

public:

#ifndef AUTH_RESTRICTED_USERNAME_STORAGE
	/** Set the last used username for authentication */
	OP_STATUS SetLastUserName(const OpStringC &nam);
	/** Set the last used username for proxy authentication */
	OP_STATUS SetLastProxyUserName(const OpStringC &nam);
#endif
	/** Get the last used username for authentication */
	OpStringC GetLastUserName() const {return username;}
	/** Get the last used username for proxy authentication */
	OpStringC GetLastProxyUserName() const {return proxyusername;}
	/** Add a new authentication element for the specified path */
	void Add_Auth(AuthElm *, const OpStringC8 &path);
	/** Get the currently used authentication credetials used for the given port, path, protocol and realm, usingthe specified authentication scheme */
	AuthElm *Get_Auth(const char *realm, unsigned short port, const char *path, URLType, AuthScheme schm, URL_CONTEXT_ID context_id);

#ifdef _USE_PREAUTHENTICATION_
	/** 
	 *  Try to resolve HTTP authentication realm internally.
	 *  This function is meant to be called before unauthenticated connect in order to resolve realm for particular URL path. 
	 *  If internal search is successful - we save time and traffic.
	 *
	 *  @return HTTP authentication realm, if it could be resolved internally, NULL otherwise.
	 */
	const char *ResolveRealm(unsigned short port, const char *path, URLType typ, AuthScheme schm, URL_CONTEXT_ID a_context_id) const;
#endif // _USE_PREAUTHENTICATION_

	/** Clear all authentication credentials */
	void Clear_Authentication_List();
	/** Update the credentials for the authentication element having the given id, with the argument parameter list */
	OP_STATUS Update_Authenticate(unsigned long aid, ParameterList *);

	BOOL SafeToDelete();
	void RemoveSensitiveData();

#ifdef _OPERA_DEBUG_DOC_
	void GetMemUsed(DebugUrlMemory &debug);
#endif


};
#endif

#endif // _AUTH_SN_H_
