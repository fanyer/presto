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

#ifndef _URL_TYPES_H_
#define _URL_TYPES_H_

#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
#define CONST_ENTRY_PROT_ISEXTERNAL	CONST_ENTRY_SINGLE(is_external, FALSE)
#else
#define CONST_ENTRY_PROT_ISEXTERNAL
#endif
#ifdef URL_USE_SCHEME_ALIAS
#define CONST_ENTRY_ALIAS(alias)	CONST_ENTRY_SINGLE(alias_scheme, alias)
#else
#define CONST_ENTRY_ALIAS(alias)	
#endif

#define ADD_URL_TYPENAME_FULL(name, uni_name, type, alt_type, has_servername, port, alias)		\
		CONST_ENTRY_START(protocolname, name)				\
		CONST_ENTRY_SINGLE(uni_protocolname, uni_name)		\
		CONST_ENTRY_SINGLE(real_urltype, type)				\
		CONST_ENTRY_SINGLE(used_urltype, alt_type)			\
		CONST_ENTRY_SINGLE(have_servername, has_servername)	\
		CONST_ENTRY_SINGLE(default_port, port)				\
		CONST_ENTRY_PROT_ISEXTERNAL							\
		CONST_ENTRY_ALIAS(alias)									\
		CONST_ENTRY_END

#define ADD_URL_TYPENAME(name, type, alt_type, has_servername, port)		\
		ADD_URL_TYPENAME_FULL(name, UNI_L(name), type, alt_type, has_servername, port, NULL)

#define URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(name, type)					ADD_URL_TYPENAME(name, type, type, FALSE, 0)
#define URL_PROTOCOL_ARRAY_ENTRY(name, type, port)						ADD_URL_TYPENAME(name, type, type, TRUE, port)
#define URL_PROTOCOL_ARRAY_ENTRY_ALT_NO_SERVER(name, type, alt_type)	ADD_URL_TYPENAME(name, type, alt_type, FALSE, 0)
#define URL_PROTOCOL_ARRAY_ENTRY_ALT(name, type, alt_type, port)		ADD_URL_TYPENAME(name, type, alt_type, TRUE, port)
#define URL_PROTOCOL_ARRAY_ENTRY_NULL(type)								ADD_URL_TYPENAME_FULL(NULL, NULL, type, type, FALSE, 0, NULL)
#define URL_PROTOCOL_ARRAY_ENTRY_ALIAS(name, type, port, alias)			ADD_URL_TYPENAME_FULL(name, UNI_L(name), type, type, TRUE, port, alias)


CONST_ARRAY(protocol_selentry, URL_typenames, url)
/** MUST BE ALPHABETHICAL */
	URL_PROTOCOL_ARRAY_ENTRY_NULL(							URL_UNKNOWN					)
	URL_PROTOCOL_ARRAY_ENTRY_ALT_NO_SERVER( "about",		URL_ABOUT,	URL_OPERA		)
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"attachment",	URL_ATTACHMENT				)
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"cid",			URL_CONTENT_ID				)
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"data",			URL_DATA					)
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"ed2k",			URL_ED2K					)

	URL_PROTOCOL_ARRAY_ENTRY(				"file",			URL_FILE,				0	)
	URL_PROTOCOL_ARRAY_ENTRY(				"ftp",			URL_FTP,				21	)
	URL_PROTOCOL_ARRAY_ENTRY(				"gopher",		URL_Gopher,				70	)
	URL_PROTOCOL_ARRAY_ENTRY(				"http",			URL_HTTP,				80	)
	URL_PROTOCOL_ARRAY_ENTRY(				"https",		URL_HTTPS,				443	)
#ifdef M2_SUPPORT
	URL_PROTOCOL_ARRAY_ENTRY(				"irc",			URL_IRC,				0	)
#endif
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"javascript",	URL_JAVASCRIPT				)
#ifdef IMODE_EXTENSIONS
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"mail",			URL_MAIL					)
#endif // IMODE_EXTENSIONS

	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"mailto",		URL_MAILTO					)

#ifdef DOM_GADGET_FILE_API_SUPPORT
	URL_PROTOCOL_ARRAY_ENTRY(				"mountpoint",	URL_MOUNTPOINT,			0	)
#endif // DOM_GADGET_FILE_API_SUPPORT

	URL_PROTOCOL_ARRAY_ENTRY(				"news",			URL_NEWS,				119	)

	URL_PROTOCOL_ARRAY_ENTRY_ALT(			"nntp",			URL_NNTP,	URL_NEWS,	119	)
	URL_PROTOCOL_ARRAY_ENTRY_ALT(			"nntps",		URL_NNTPS,	URL_SNEWS,	563	)
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"opera",		URL_OPERA					)
#ifdef M2_SUPPORT
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"opera-chattransfer",	URL_CHAT_TRANSFER	)
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"operaemail",	URL_EMAIL					)
#endif
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER( "s",				URL_OPERA)				//25
	URL_PROTOCOL_ARRAY_ENTRY(				"snews",		URL_SNEWS,				563	)
#ifdef SOCKS_SUPPORT
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"socks",		URL_SOCKS					)
#endif // SOCKS_SUPPORT
#ifdef IMODE_EXTENSIONS
	URL_PROTOCOL_ARRAY_ENTRY_NO_SERVER(		"tel",			URL_TEL						)
#endif // IMODE_EXTENSIONS
	URL_PROTOCOL_ARRAY_ENTRY(				"telnet",		URL_TELNET,				0	)
	URL_PROTOCOL_ARRAY_ENTRY(				"tn3270",		URL_TN3270,				0	)
#ifdef HAS_ATVEF_SUPPORT
	URL_PROTOCOL_ARRAY_ENTRY(				"tv",			URL_TV,					0	)
#endif /* HAS_ATVEF_SUPPORT */
#if defined(WEBSERVER_SUPPORT)
	URL_PROTOCOL_ARRAY_ENTRY(				"unite",		URL_UNITE,				0	)
#endif
	URL_PROTOCOL_ARRAY_ENTRY(				"wais",			URL_WAIS,				210	)
#if defined(GADGET_SUPPORT)
	URL_PROTOCOL_ARRAY_ENTRY(				"widget",		URL_WIDGET,				0	)
#endif /* GADGET_SUPPORT */
#ifdef WEBSOCKETS_SUPPORT
	URL_PROTOCOL_ARRAY_ENTRY(				"ws",			URL_WEBSOCKET,			80	)
#endif
#ifdef URL_USE_SCHEME_ALIAS_WSP
	URL_PROTOCOL_ARRAY_ENTRY_ALIAS(			"wsp",			URL_UNKNOWN,			0,		"http") // Alias to http; urltype,port nulled to avoid confusion.
#endif
#ifdef WEBSOCKETS_SUPPORT
	URL_PROTOCOL_ARRAY_ENTRY(				"wss",			URL_WEBSOCKET_SECURE,	443	)
#endif // WEBSOCKETS_SUPPORT
CONST_END(URL_typenames)

#endif
