/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file ExternalSSLLibrary.h
 *
 * External SSL Library object, OpenSSL specific.
 *
 * Main purposes of this class:
 * 1) Initinialization/uninitialization of the External SSL Library.
 * 2) Storing External SSL Library global data.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OPENSSL_LIBRARY_H
#define OPENSSL_LIBRARY_H

#include "modules/externalssl/src/ExternalSSLLibrary.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/ssl.h"


class OpenSSLLibrary: public ExternalSSLLibrary
{
public:
	OpenSSLLibrary();
	virtual ~OpenSSLLibrary();

public:
	// ExternalSSLLibrary methods.
	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
	virtual void* GetGlobalData() { return reinterpret_cast <void*> (this); }

public:
	SSL_CTX* Get_SSL_CTX() { return m_ssl_ctx; }

private:
	void Create_SSL_CTX_L();
#ifdef DIRECTORY_SEARCH_SUPPORT
	void LoadCertsL();
	void LoadCertsFromFile(OpFile& file);
	size_t ParsePEMBuffer(const char* buf, size_t buf_len);
	void LoadPEMChunk(const char* pem_chunk, int pem_len);
#else
	// No certs will be loaded on startup, so all cert checks will fail.
#endif

private:
	SSL_CTX* m_ssl_ctx;
};

#endif // OPENSSL_LIBRARY_H
