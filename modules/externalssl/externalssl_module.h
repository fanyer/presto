/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */


#ifndef EXTERNALSSL_MODULE_H
#define EXTERNALSSL_MODULE_H

#ifdef _EXTERNAL_SSL_SUPPORT_

#include "modules/hardcore/opera/module.h"

#include "modules/libssl/base/sslenum2.h"
#include "modules/url/tools/arrays_decl.h"

class External_SSL_DisplayContextManager;
class External_SSL_Options;
struct External_Cipher_spec;
class ExternalSSLLibrary;


class ExternalsslModule : public OperaModule
{
public:
	ExternalsslModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

public:
	DECLARE_MODULE_CONST_ARRAY(External_Cipher_spec, ExtSSL_Cipher_list);
	External_SSL_DisplayContextManager *m_ssl_display_context_manager;	
	External_SSL_Options* m_external_ssl_options;

public:
	void* GetGlobalData();

private:
	ExternalSSLLibrary* m_external_ssl_library;
};

#define g_ssl_display_context_manager (g_opera->externalssl_module.m_ssl_display_context_manager)
#define g_external_ssl_options (g_opera->externalssl_module.m_external_ssl_options)

#ifndef HAS_COMPLEX_GLOBALS
# define g_ExtSSL_Cipher_list	CONST_ARRAY_GLOBAL_NAME(externalssl, ExtSSL_Cipher_list)
#endif

#define EXTERNALSSL_MODULE_REQUIRED

#endif // _EXTERNAL_SSL_SUPPORT_

#endif // !EXTERNALSSL_MODULE_H
