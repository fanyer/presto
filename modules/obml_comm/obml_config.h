/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _OBML_CONFIG_H
#define _OBML_CONFIG_H

#ifdef WEB_TURBO_MODE

class OpFile;
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/url/protocols/next_protocol.h"

class OBML_Config
	: public PrefsCollectionFilesListener
{
private:
	uni_char *turbo_http_proxy;
	OpString8 turbo_http_proxy8;
	NextProtocol turbo_protocol;

	OpFile		m_config_file;
	OpString8	obml_brand;
	BOOL		m_use_encryption;
	BOOL		expired;
	time_t		time_expires;

public:
	OBML_Config();
	~OBML_Config();
	void ConstructL();

	BOOL GetExpired();
	BOOL GetUseEncryption() { return m_use_encryption; }

	// Use instead of OBML_BRAND define
	const char* GetBranding() { return obml_brand.HasContent() ? obml_brand.CStr() : OBML_BRAND; }

	const uni_char*	GetTurboProxyName() { return (turbo_http_proxy && *turbo_http_proxy ? turbo_http_proxy : TURBO_HTTP_PROXY_NAME); }
	const char*		GetTurboProxyName8() { return turbo_http_proxy8.HasContent() ? turbo_http_proxy8.CStr() : NULL; }
	NextProtocol	GetTurboProxyProtocol() { return turbo_protocol; }

	void FileChangedL(PrefsCollectionFiles::filepref pref, const OpFile *newvalue);

private:	
#ifdef OBML_USE_XML_SIGNATURE
	OP_STATUS ReadSettingsL();
#else // OBML_USE_XML_SIGNATURE
	OP_STATUS CheckOBMLConfigSignature(const OpFile &file);
	OP_STATUS LoadConfigFile();
#endif // OBML_USE_XML_SIGNATURE
};

#endif // WEB_TURBO_MODE


#endif // _OBML_CONFIG_H
