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
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */


#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#include "modules/libssl/sslbase.h"

// Modified from PEM_read_bio
BOOL load_PEM_certificates2(SSL_varvector32 &data_source, SSL_varvector32 &pem_content)
{
	pem_content.Resize(0);

	OpString8 sourcestring;

	if(OpStatus::IsError(sourcestring.Set((const char *)data_source.GetDirect(), data_source.GetLength())))
		return FALSE;

	if(sourcestring.Length() == 0)
		return FALSE;

	int nohead=0;
	size_t line_len=0;
	const unsigned char *pemname = NULL;
	size_t pemname_len = 0;
	const unsigned char *source = (unsigned char *) sourcestring.CStr();

	while(*source != '\0')
	{
		BOOL found = FALSE;
		for (;*source != '\0';)
		{
			line_len= op_strcspn((const char *)source, "\r\n");
			
			if (op_strncmp((const char *)source,"-----BEGIN ",11) == 0)
			{
				
				if (op_strncmp((const char *)source+line_len-5 ,"-----",5) != 0)
				{
					source += line_len;
					if(*source == '\r')
						source++;
					if(*source == '\n')
						source++;
					continue;
				}
				pemname = source+11;
				pemname_len = line_len -11 -5;

				if((pemname_len == STRINGLENGTH("CERTIFICATE") && op_strncmp((const char *) pemname,"CERTIFICATE", pemname_len) == 0)  || 
					(pemname_len == STRINGLENGTH("X509 CERTIFICATE") && op_strncmp((const char *) pemname,"X509 CERTIFICATE", pemname_len)== 0) || 
					(pemname_len == STRINGLENGTH("X509 CRL") && op_strncmp((const char *) pemname,"X509 CRL", pemname_len)== 0) || 
					(pemname_len >=  STRINGLENGTH("PKCS12") && op_strncmp((const char *) pemname,"PKCS12", 6) == 0))
					found = TRUE;

				source += line_len;
				if(*source == '\r')
					source++;
				if(*source == '\n')
					source++;
				break;
			}
			source += line_len;
			if(*source == '\r')
				source++;
			if(*source == '\n')
				source++;
		}

		const unsigned char *content_start = source;
		for (;*source != '\0';)
		{
			line_len= op_strcspn((const char *) source, "\r\n");

			if(line_len == 0)
			{
				source += line_len;
				if(*source == '\r')
					source++;
				if(*source == '\n')
					source++;
				break;
			}
			
			if (op_strncmp((const char *) source,"-----END ",9) == 0)
			{
				source += line_len;
				if(*source == '\r')
					source++;
				if(*source == '\n')
					source++;
				nohead = TRUE;
				break;
			}

			source += line_len;
			if(*source == '\r')
				source++;
			if(*source == '\n')
				source++;
		}

		line_len=0;
		if (found)
		{
			if(nohead)
				source = content_start;
			unsigned long content_len = 0;
			const unsigned char *src_content = source;

			while(*source != '\0')
			{
				line_len= op_strcspn((const char *) source, "\r\n");

				if (op_strncmp((const char *) source,"-----END ",9) == 0)
				{
					break;
				}

				source += line_len;
				content_len += line_len;
				if(*source == '\r')
				{
					source++;
					content_len++;
				}
				if(*source == '\n')
				{
					source++;
					content_len++;
				}
			}

			if(*source)
			{
				// Always "-----END "
				if(line_len == 9+5+pemname_len &&
					op_strncmp((const char *) source+9, (const char *) pemname, pemname_len) == 0 && 
					op_strncmp((const char *) source+9+pemname_len, "-----",5) == 0)
				{
					unsigned long read_pos=0;
					BOOL warning = FALSE;

					pem_content.Resize((content_len/4)*3);
					if(!pem_content.Valid())
						return FALSE;

					extern unsigned long GeneralDecodeBase64(const unsigned char *source, unsigned long len, unsigned long &read_pos, unsigned char *target, BOOL &warning, unsigned long max_write_len=0, unsigned char *decode_buf=NULL, unsigned int *decode_len=NULL);
					unsigned long decoded_len = GeneralDecodeBase64(src_content, content_len, read_pos, pem_content.GetDirect(), warning);

					OP_ASSERT(read_pos == content_len);
					
					pem_content.Resize(decoded_len);
					
					return TRUE;
				}
			}
		}

		// Previous line
		source += line_len;
		if(*source == '\r')
			source++;
		if(*source == '\n')
			source++;
	}

	return FALSE;
}

#endif
