/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OpenSSLLibrary.cpp
 *
 * External SSL Library object, OpenSSL implementation.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#ifdef EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

#include "modules/externalssl/src/openssl_impl/OpenSSLLibrary.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"
#include "modules/util/opfile/opfile.h"


OpenSSLLibrary::OpenSSLLibrary()
	: m_ssl_ctx(0)
{
}


OpenSSLLibrary::~OpenSSLLibrary()
{
	OP_ASSERT(m_ssl_ctx == 0);
}


void OpenSSLLibrary::InitL(const OperaInitInfo& info)
{
	// Can't do it because it tries to access files
	// not via OpFile interface.
	//SSL_load_error_strings();

	// OpenSSL initialization.
	SSL_library_init();

#ifdef DIRECTORY_SEARCH_SUPPORT
	Create_SSL_CTX_L();
	LoadCertsL();
#endif

	OPENSSL_LEAVE_IF(ERR_peek_error());
}


void OpenSSLLibrary::Destroy()
{
	if (m_ssl_ctx)
	{
		SSL_CTX_free(m_ssl_ctx);
		m_ssl_ctx = 0;
	}

	// OpenSSL deinitialization.
	// It's not docummented that this function deinitializes
	// SSL part of OpenSSL, but it works, i.e. deallocates the memory,
	// allocated by SSL_library_init().
	EVP_cleanup();

	// Commented for now because SSL_load_error_strings() is also commented.
	//ERR_free_strings();
}


void OpenSSLLibrary::Create_SSL_CTX_L()
{
	// SSL_METHOD
	// Most new and secure method.
	// const SSL_METHOD* ssl_method = TLSv1_client_method();
	// All methods, but unable to link currently.
	// const SSL_METHOD* ssl_method = SSLv23_client_method();
	// Most compatible method.
	const SSL_METHOD* ssl_method = SSLv3_client_method();
	OP_ASSERT(ssl_method);

	// SSL_CTX
	OP_ASSERT(m_ssl_ctx == 0);
	m_ssl_ctx = SSL_CTX_new(ssl_method);
	LEAVE_IF_NULL(m_ssl_ctx);

	// Switch off insecure SSLv2 for SSLv23_client_method()
	//SSL_CTX_set_options(m_ssl_ctx, SSL_OP_NO_SSLv2);

	// Switch on all workarounds.
	/*
	SSL_CTX_set_options(m_ssl_ctx,
		SSL_OP_ALL |
		SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
		SSL_OP_NO_TICKET |
		SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION |
		SSL_OP_LEGACY_SERVER_CONNECT);
	*/
}


#ifdef DIRECTORY_SEARCH_SUPPORT
void OpenSSLLibrary::LoadCertsL()
{
	// Create folder lister.
	OpFolderLister* lister = 0;
	OP_STATUS status = OpFolderLister::Create(&lister);
	LEAVE_IF_ERROR(status);
	OP_ASSERT(lister);
	OpAutoPtr <OpFolderLister> lister_autoremover(lister);

	// Search for "*.pem" files in X509_CERT_DIR.
	// OpenSSL's default for it is /usr/local/ssl/certs/.
	const char* x509_cert_dir = X509_CERT_DIR;
	OpString cert_dir;
	cert_dir.SetL(x509_cert_dir);
	status = lister->Construct(cert_dir.CStr(), UNI_L("*.pem"));
	LEAVE_IF_ERROR(status);

	while(lister->Next())
	{
		const uni_char* filename = lister->GetFullPath();
		OP_ASSERT(filename);

		OpFile file;
		status = file.Construct(filename);
		if (status != OpStatus::OK)
			continue;

		// Not using OPFILE_TEXT here. It's non-portable: on POSIX systems
		// it doesn't remove CR in the end of line, thus it needs to be done
		// manually anyway.
		status = file.Open(OPFILE_READ);
		if (status != OpStatus::OK)
			continue;

		LoadCertsFromFile(file);
	}

}


void OpenSSLLibrary::LoadCertsFromFile(OpFile& file)
{
	OP_ASSERT(file.IsOpen());

	// BUFSIZE is also the maximum size of PEM chunk with certificate.
	const size_t BUFSIZE = 64 * 1024;
	char buf[BUFSIZE]; /* ARRAY OK 2009-10-20 alexeik */

	// Length of buffer's useful data.
	size_t buf_len = 0;

	while (!file.Eof())
	{
		// Read.
		{
			char*  buf_free_pos = buf + buf_len;
			size_t buf_free_len = BUFSIZE - buf_len;
			OP_ASSERT(buf_free_pos >= buf);
			OP_ASSERT(buf_free_pos + buf_free_len == buf + BUFSIZE);
	
			OpFileLength bytes_read = 0;
			OP_STATUS status = file.Read(buf_free_pos, buf_free_len, &bytes_read);
			if (status != OpStatus::OK || bytes_read == 0)
				break;
			
			OP_ASSERT(bytes_read > 0);
			OP_ASSERT(bytes_read < (OpFileLength)buf_free_len);
	
			buf_len += bytes_read;
			OP_ASSERT(buf_len == BUFSIZE || file.Eof());
		}

		// Parse.
		{
			size_t bytes_parsed = ParsePEMBuffer(buf, buf_len);
			OP_ASSERT(bytes_parsed <= buf_len);
	
			if (bytes_parsed == buf_len)
			{
				// The whole buffer is parsed.
				// Consider it free.
				buf_len = 0;
			}
			else if (bytes_parsed != 0)
			{
				// The buffer is partially parsed.
				// Let's move the unparsed data to the beginning.
				buf_len -= bytes_parsed;
				op_memmove(buf, buf + bytes_parsed, buf_len);
			}
			else
			{
				// Nothing is parsed. Not enough data for parsing
				// or invalid data. Stop parsing.
				break;
			}
		}
	}
}


size_t OpenSSLLibrary::ParsePEMBuffer(const char* buf, size_t buf_len)
{
	OP_ASSERT(buf);
	
	const char* const begin_marker = "-----BEGIN CERTIFICATE-----";
	const char* const   end_marker = "-----END CERTIFICATE-----";
	const size_t begin_marker_len = op_strlen(begin_marker);
	const size_t   end_marker_len = op_strlen(  end_marker);

	const char* cur_pos = buf;
	const char* const buf_end = buf + buf_len;

	// Process PEM chunks.
	while (cur_pos < buf_end)
	{
		// Length of current "parsing space".
		size_t cur_len = buf_end - cur_pos;
		if (cur_len < begin_marker_len)
			break;

		// Look for begin marker.
		const char* const begin_pos = (const char*) op_memmem(
			cur_pos, cur_len, begin_marker, begin_marker_len);
		if (!begin_pos)
		{
			// End marker not found. Maybe a PEM chunk crosses
			// the buffer border. Thus only eat the buffer up to
			// its possible location.
			cur_pos = buf_end - begin_marker_len + 1;
			break;
		}

		// Check result and update cur_len/cur_pos.
		OP_ASSERT(begin_pos >= cur_pos);
		cur_pos = begin_pos + begin_marker_len;
		OP_ASSERT(cur_pos <=  buf_end);
		cur_len = buf_end - cur_pos;

		// Look for end marker.
		const char* const end_pos = (const char*) op_memmem(
			cur_pos, cur_len, end_marker, end_marker_len);
		if (!end_pos)
		{
			// End marker not found. Maybe a PEM chunk crosses
			// the buffer border. Thus only eat the buffer up to
			// the beginning of the PEM chunk.
			cur_pos = begin_pos;
			break;
		}

		// Check result and update cur_len/cur_pos.
		OP_ASSERT(end_pos >= cur_pos);
		cur_pos = end_pos + end_marker_len;
		OP_ASSERT(cur_pos <=  buf_end);
		// Not updating cur_len, it's not used till the end of the cycle.

		// Found a full PEM chunk inside the buffer: [begin_pos, cur_pos).
		OP_ASSERT(cur_pos > begin_pos);
		const int pem_len = cur_pos - begin_pos;

		// Load a certificate from the PEM chunk.
		LoadPEMChunk(begin_pos, pem_len);
	}
	// No more full PEM chunks in this buffer.

	// Eat whitespace.
	while (cur_pos < buf_end && op_isspace(*cur_pos))
		cur_pos++;

	OP_ASSERT(cur_pos >= buf);
	OP_ASSERT(cur_pos <= buf_end);
	size_t parsed_bytes = cur_pos - buf;

	return parsed_bytes;
}


void OpenSSLLibrary::LoadPEMChunk(const char* pem_chunk, int pem_len)
{
	OP_ASSERT(pem_chunk);
	OP_ASSERT(pem_len > 0);
	OP_ASSERT(m_ssl_ctx);

	BIO* bio = BIO_new_mem_buf((void*) pem_chunk, pem_len);
	if (!bio)
		return;

	// Load certificate into X509 structure.
	X509* x509 = PEM_read_bio_X509_AUX(bio, 0, 0, 0);
	if (!x509)
	{
		BIO_free(bio);
		return;
	}
	// We own x509.
	
	// Get cert store from the global SSL context.
	X509_STORE* x509_store = SSL_CTX_get_cert_store(m_ssl_ctx);
	OP_ASSERT(x509_store);
	// m_ssl_ctx owns x509_store.

	// Add cert to the store.
	int res = X509_STORE_add_cert(x509_store, x509);
	// We still own x509.
	if (res != 1)
		// Ignore the error.
		ERR_clear_error();

	X509_free(x509);
	BIO_free(bio);
}

#endif // DIRECTORY_SEARCH_SUPPORT

#endif // EXTERNAL_SSL_OPENSSL_IMPLEMENTATION
