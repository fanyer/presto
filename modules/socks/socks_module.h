/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_SOCKS_MODULE_H
#define MODULES_SOCKS_MODULE_H

#ifdef SOCKS_SUPPORT

#define SOCKS_MODULE_REQUIRED // used in hardcore/opera/opera.h

#include "modules/hardcore/opera/module.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/pi/network/OpHostResolver.h"

struct SocksModule : public OperaModule, public OpPrefsListener, public OpHostResolverListener
{
	void  InitL(const OperaInitInfo& info);
	void  Destroy();

	SocksModule() { /*InitL(*(OperaInitInfo*)NULL);*/ }
	~SocksModule() { /*Destroy();*/ }

	/*
	enum protocol_version_t
	{
		v4a = 0,
		v5  = 1
	};
	protocol_version_t protocol_version;
	*/
	enum socket_t
	{
		CONNECT   = 1,
		BIND      = 2,
		UDP_ASSOC = 3
	};

private:

	BOOL              m_is_dirty;            // if TRUE: SOCKS prefs have changed and should be reloaded.
	BOOL              m_listener_registered; // if TRUE: this module has been registered as listener to network prefs
	BOOL              m_socks_enabled;       // if TRUE: SOCKS proxy should be used for outbound TCP connections.
	OpString8         m_user_pass_datagram;  // e.g. "\x04user\x08password" (see SOCKS spec)
	OpSocketAddress*  m_proxy_socket_address;
	OpHostResolver*   m_host_resolver;

	ServerName_Store* m_autoproxy_socks_server_store;  // used by autoproxy config (if enabled); Initially NULL, instantiated on demand.

	void  refresh();

	BOOL  resolveHost(const uni_char* hostname);


public:
	                                                    	                              
	/** Is socks generally enabled in browser prefs?
	 *  It may not be enabled, but still in use through the autoproxy config mechanism
	 */
	BOOL             IsSocksEnabled()  { if (m_is_dirty) refresh(); return m_socks_enabled; }

	/** The general socks proxy (when enabled) specified in browser prefs.
	 *  Note that autoproxy configured socks proxy will take precedence over this one.
	 */
	OpSocketAddress* GetSocksServerAddress() { if (m_is_dirty) refresh(); return m_proxy_socket_address; }

	/** Get the socks server name corresponding to the specified original (original created by URL_Manager::GetServerName).
	 *  This has nothing to do with the socks server (if any) mentioned in the prefs settings (see the above 2 methods)
	 */
	ServerName*      GetSocksServerName(ServerName* original);

	//    That's how user-pass datagram looks like:
	//    +----+------+----------+------+----------+
	//    |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
	//    +----+------+----------+------+----------+
	//    | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
	//    +----+------+----------+------+----------+
	BOOL             HasUserPassSpecified() { return IsSocksEnabled() && m_user_pass_datagram[0] == 1 && m_user_pass_datagram[1] != 0; }
	const char*      GetUserPassDatagram() { OP_ASSERT(HasUserPassSpecified()); return m_user_pass_datagram; }
	UINT             GetUserPassDatagramLength() { OP_ASSERT(HasUserPassSpecified()); return m_user_pass_datagram[1] + m_user_pass_datagram[m_user_pass_datagram[1]+2] + 3; }

	/** Signals a change in an integer preference.
	  *
	  * @param id       Identity of the collection that has changed the value.
	  * @param pref     Identity of the preference. This is the integer
	  *                 representation of the enumeration value used by the
	  *                 associated OpPrefsCollection.
	  * @param newvalue The new value of the preference.
	  */
	virtual void PrefChanged(OpPrefsCollection::Collections /* id */, int /* pref */,
	                         int /* newvalue */);

	/** Signals a change in a string preference.
	  *
	  * @param id       Identity of the collection that has changed the value.
	  * @param pref     Identity of the preference. This is the integer
	  *                 representation of the enumeration value used by the
	  *                 associated OpPrefsCollection.
	  * @param newvalue The new value of the preference.
	  */
	virtual void PrefChanged(OpPrefsCollection::Collections /* id */, int /* pref */,
	                         const OpStringC & /* newvalue */);


	/** Implementation of interface OpHostResolverListener */
	virtual void OnHostResolved(OpHostResolver* host_resolver);

	/** Implementation of interface OpHostResolverListener */
	virtual void OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error);
};

#endif//#ifdef SOCKS_SUPPORT

#endif//MODULES_SOCKS_MODULE_H
