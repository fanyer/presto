/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_ROOTSTORE_MODULE_H
#define MODULES_ROOTSTORE_MODULE_H

#if defined(_SSL_SUPPORT_) && defined(_SSL_USE_OPENSSL_) && !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)

class RootStore_API;

class RootstoreModule : public OperaModule
{
public:
	RootStore_API	*m_root_store_api;

#ifdef ROOTSTORE_SIGNKEY
	const byte *m_rootstore_sign_pubkey;
	uint32 m_rootstore_sign_pubkey_len;
#endif

public:
	RootstoreModule();
	virtual ~RootstoreModule(){};
	virtual void InitL(const OperaInitInfo& info);

	virtual void Destroy();
};

#define g_rootstore_module	g_opera->rootstore_module

#define g_root_store_api	g_rootstore_module.m_root_store_api

#ifdef ROOTSTORE_SIGNKEY
#define g_rootstore_sign_pubkey g_rootstore_module.m_rootstore_sign_pubkey
#define g_rootstore_sign_pubkey_len g_rootstore_module.m_rootstore_sign_pubkey_len
#endif

#define ROOTSTORE_MODULE_REQUIRED

#endif // _SSL_SUPPORT_

#endif // !MODULES_LIBSSL_LIBSSL_MODULE_H
