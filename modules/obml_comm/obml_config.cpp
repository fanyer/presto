/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef WEB_TURBO_MODE

#include "modules/obml_comm/obml_config.h"

#include "modules/url/url2.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/obml_comm/config/turbo_config_key.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"

#ifdef CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT
# include "modules/libcrypto/include/CryptoVerifySignedTextFile.h"
#else
# include "modules/libssl/tools/signed_textfile.h"
#endif

OBML_Config::OBML_Config():
	turbo_http_proxy(NULL),
	m_use_encryption(TRUE),
	expired(FALSE),
	time_expires(0)
{
	turbo_protocol = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseSpdyForTurbo) ? NP_SPDY3 : NP_HTTP;
}

OBML_Config::~OBML_Config()
{
	g_pcfiles->UnregisterFilesListener(this);

	op_free(turbo_http_proxy);
}

void OBML_Config::ConstructL()
{
	g_pcfiles->RegisterFilesListenerL(this);
	g_pcfiles->GetFileL(PrefsCollectionFiles::WebTurboConfigFile, m_config_file);
	OP_STATUS status = LoadConfigFile();
	if( OpStatus::IsError(status) && m_config_file.IsOpen() )
		m_config_file.Close();

	if( OpStatus::IsMemoryError(status) )
	{
		LEAVE(status);
	}
#ifdef OBML_COMM_FORCE_CONFIG_FILE
	else if( OpStatus::IsError(status) )
	{
		expired = TRUE;
		time_expires = 0;
	}
#endif // OBML_COMM_FORCE_CONFIG_FILE

	if( turbo_http_proxy8.IsEmpty() )
		turbo_http_proxy8.SetUTF8FromUTF16L(TURBO_HTTP_PROXY_NAME);
}

BOOL OBML_Config::GetExpired()
{
	if (expired && time_expires < g_timecache->CurrentTime())
		return TRUE;
	return FALSE;
}

OP_STATUS OBML_Config::CheckOBMLConfigSignature(const OpFile &file)
{
	OpString resolved;
	OP_MEMORY_VAR BOOL successful;

	RETURN_IF_LEAVE(successful = g_url_api->ResolveUrlNameL(file.GetFullPath(), resolved));

	if (!successful || resolved.Find("file://") != 0)
		return OpStatus::ERR;

	URL url = g_url_api->GetURL(resolved.CStr());

	if (!url.QuickLoad(TRUE))
		return OpStatus::ERR;
#ifdef CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT
	if (!CryptoVerifySignedTextFile(url, "//", TURBO_CONFIG_KEY, sizeof TURBO_CONFIG_KEY))
#else	
	if (!VerifySignedTextFile(url, "//", TURBO_CONFIG_KEY, sizeof TURBO_CONFIG_KEY))
#endif		
		return OpStatus::ERR;
	else
		return OpStatus::OK;
}

OP_STATUS OBML_Config::LoadConfigFile()
{
	BOOL exists = FALSE;
	RETURN_IF_ERROR(m_config_file.Exists(exists));
	if( !exists )
		return OpStatus::ERR_FILE_NOT_FOUND;

#ifdef OBML_COMM_VERIFY_CONFIG_SIGNATURE
	RETURN_IF_ERROR(CheckOBMLConfigSignature(m_config_file));
#endif // OBML_COMM_VERIFY_CONFIG_SIGNATURE
	
	RETURN_IF_ERROR(m_config_file.Open(OPFILE_READ | OPFILE_TEXT));

	OpFileLength length;
	RETURN_IF_ERROR(m_config_file.GetFileLength(length));

	if( length >= INT_MAX || length <= 0 ) // File too large for memory or empty
		return OpStatus::ERR;

	char *source = OP_NEWA(char, (int)length + 1);
	if( !source )
		return OpStatus::ERR_NO_MEMORY;

	OpHeapArrayAnchor<char> source_anchor(source);
	char *ptr = source;
	OpFileLength total = 0;

	while (!m_config_file.Eof() && length != static_cast<OpFileLength>(0))
	{
		OpFileLength bytes_read;
		RETURN_IF_ERROR(m_config_file.Read(ptr, length, &bytes_read));

		ptr += bytes_read;
		length -= bytes_read;
		total += bytes_read;
	}

	source[total] = 0;
	ByteBuffer buffer;
	OP_ASSERT(total <= UINT_MAX);
	buffer.AppendBytes(source, static_cast<unsigned int>(total));
#ifdef OBML_COMM_VERIFY_CONFIG_SIGNATURE
	buffer.Consume(sizeof TURBO_CONFIG_KEY);
#endif // OBML_COMM_VERIFY_CONFIG_SIGNATURE
	// Parse the textfile as xml
	XMLFragment f;
	f.SetDefaultWhitespaceHandling(XMLWHITESPACEHANDLING_PRESERVE);
	RETURN_IF_ERROR(f.Parse(&buffer));

	if (f.EnterElement(UNI_L("turbo_settings")))
	{
		const uni_char *elem_name = NULL;
		while( f.EnterAnyElement() )
		{
			elem_name = f.GetElementName().GetLocalPart();
			if( !elem_name || !*elem_name )
			{
				f.LeaveElement();
				continue;
			}

			if( uni_strcmp(elem_name,UNI_L("validation")) == 0 )
			{
				const uni_char* date = f.GetAttribute(UNI_L("expires"));
				if( date && *date )
				{
					int eday, emonth, eyear;
					if(uni_sscanf(date, UNI_L("%d-%d-%d"), &eday, &emonth, &eyear) == 3)
					{
						expired = TRUE;
						time_t time_now = g_timecache->CurrentTime();
						struct tm *newtime_p= op_localtime(&time_now);
						newtime_p->tm_year = eyear - 1900;
						newtime_p->tm_mon = emonth - 1;
						newtime_p->tm_mday = eday;
						time_expires = op_mktime(newtime_p);
						if (time_expires < time_now)
							return OpStatus::ERR;
					}
				}
			}
			else if( uni_strcmp(elem_name,UNI_L("obml")) == 0 )
			{
				const uni_char* brand = f.GetAttribute(UNI_L("brand"));
				if( brand && *brand )
				{
					RETURN_IF_ERROR(obml_brand.SetUTF8FromUTF16(brand));
				}
			}
			else if( uni_strcmp(elem_name,UNI_L("wop")) == 0 )
			{
				turbo_http_proxy = uni_strdup(f.GetAttribute(UNI_L("proxy")));
				if( turbo_http_proxy && *turbo_http_proxy )
				{
					RETURN_IF_ERROR(turbo_http_proxy8.SetUTF8FromUTF16(turbo_http_proxy));
				}

				const uni_char *turbo_protocol_str = f.GetAttribute(UNI_L("protocol"));
				if( turbo_protocol_str && *turbo_protocol_str)
				{
					if (uni_strcmp(turbo_protocol_str, UNI_L("spdy/2")) == 0)
						turbo_protocol = NP_SPDY2;
					else if (uni_strcmp(turbo_protocol_str, UNI_L("spdy/3")) == 0)
						turbo_protocol = NP_SPDY3;
				}
			}
			f.LeaveElement();
		}
	}

	m_config_file.Close();
	return OpStatus::OK;
}

void OBML_Config::FileChangedL(PrefsCollectionFiles::filepref pref, const OpFile *newvalue)
{
	if( pref != PrefsCollectionFiles::WebTurboConfigFile )
		return;

	m_config_file.Copy(newvalue);
	OP_STATUS status = LoadConfigFile();
	if( OpStatus::IsError(status) && m_config_file.IsOpen() )
		m_config_file.Close();

	if( OpStatus::IsMemoryError(status) )
	{
		LEAVE(status);
	}
#ifdef OBML_COMM_FORCE_CONFIG_FILE
	else if( OpStatus::IsError(status) )
	{
		expired = TRUE;
		time_expires = 0;
	}
#endif // OBML_COMM_FORCE_CONFIG_FILE
}

#endif // WEB_TURBO_MODE
