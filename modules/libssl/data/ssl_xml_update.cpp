/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#ifdef LIBSSL_AUTO_UPDATE

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/data/ssl_xml_update.h"
#include "modules/formats/base64_decode.h"
#include "modules/url/url2.h"
#include "modules/cache/url_stream.h"
#include "modules/formats/uri_escape.h"
#include "modules/libcrypto/include/CryptoVerifySignedTextFile.h"

SSL_XML_Updater::SSL_XML_Updater()
 : optionsManager(NULL)
{
}

SSL_XML_Updater::~SSL_XML_Updater()
{
	if(optionsManager && optionsManager->dec_reference() == 0)
		OP_DELETE(optionsManager);
	optionsManager = NULL;
}

BOOL SSL_XML_Updater::CheckOptionsManager(int stores)
{
	if(optionsManager != NULL)
		return TRUE;

	optionsManager = g_ssl_api->CreateSecurityManager(TRUE);

	if(!optionsManager || OpStatus::IsError(optionsManager->Init(stores)))
	{
		OP_DELETE(optionsManager);
		optionsManager = NULL;
	}
	else
		optionsManager->register_updates = TRUE;

	return (optionsManager != NULL ? TRUE : FALSE);
}

void SSL_XML_Updater::SetFinished(BOOL success)
{
	if(Finished())
		return;

	if(success)
	{
		success = FALSE;
		if(optionsManager)
		{
			g_ssl_api->CommitOptionsManager(optionsManager);
			success = TRUE;
		}
	}
	XML_Updater::SetFinished(success);
}

#endif // LIBSSL_AUTO_UPDATE

#endif
