/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _EXTERNAL_SSL_SUPPORT_

#include "modules/externalssl/externalssl_module.h"
#include "modules/externalssl/externalssl_displaycontext.h"
#include "modules/externalssl/extssl.h"
#include "modules/externalssl/src/ExternalSSLLibrary.h"
#include "modules/url/tools/arrays.h"


ExternalsslModule::ExternalsslModule()
	: m_ssl_display_context_manager(0)
	, m_external_ssl_options(0)
	, m_external_ssl_library(0)
{
}


void ExternalsslModule::InitL(const OperaInitInfo& info)
{
	CONST_ARRAY_INIT_L(ExtSSL_Cipher_list);

	m_ssl_display_context_manager = OP_NEW_L(External_SSL_DisplayContextManager, ());
	OP_ASSERT(m_ssl_display_context_manager);

	m_external_ssl_options = OP_NEW_L(External_SSL_Options, ());
	OP_ASSERT(m_external_ssl_options);

	m_external_ssl_library = ExternalSSLLibrary::CreateL();
	OP_ASSERT(m_external_ssl_library);
	m_external_ssl_library->InitL(info);
}


void ExternalsslModule::Destroy()
{
	OP_ASSERT(m_external_ssl_library);
	m_external_ssl_library->Destroy();
	OP_DELETE(m_external_ssl_library);
	m_external_ssl_library = 0;

	OP_DELETE(m_external_ssl_options);
	m_external_ssl_options = 0;
	
	OP_DELETE(m_ssl_display_context_manager);
	m_ssl_display_context_manager = 0;

	CONST_ARRAY_SHUTDOWN(ExtSSL_Cipher_list);
}


void* ExternalsslModule::GetGlobalData()
{
	if (!m_external_ssl_library)
		return 0;

	void* global_data = m_external_ssl_library->GetGlobalData();
	return global_data;
}

#endif // _EXTERNAL_SSL_SUPPORT_
