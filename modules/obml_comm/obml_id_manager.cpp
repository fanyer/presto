/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#undef CRYPTO_ENCRYPTED_FILE_SUPPORT

#ifdef WEB_TURBO_MODE

#include "modules/obml_comm/obml_id_manager.h"
#include "modules/obml_comm/obml_profile.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/libcrypto/include/CryptoHash.h"

#ifdef _DEBUG
#include "modules/debug/debug.h"
#endif // _DEBUG

#define OBML_CLIENT_ID_SIZE		32
#define OBML_CLIENT_ID_STR_LEN	64

#include "modules/util/opfile/opfile.h"
#ifndef CRYPTO_ENCRYPTED_FILE_SUPPORT
# include "modules/libcrypto/include/CryptoMasterPasswordEncryption.h"
#endif // CRYPTO_ENCRYPTED_FILE_SUPPORT
#define TURBO_DATA_FILE_MAX_SIZE 0x20000 // Current max size is 128 Kb
static const char TURBO_DATA_FILE_OBFUSCATION_PASSWORD[] = {
	0x59, 0x58, 0x3a, 0x33, 0x2a, 0x64, 0x52, 0x42, 0x35, 0x79, 0x5b, 0x36, 0x70, 0x2b, 0x63, 0x64, 
	0x20, 0x62, 0x6d, 0x2f, 0x3f, 0x67, 0x41, 0x77, 0x2f, 0x25, 0x62, 0x49, 0x2c, 0x33, 0x63, 0x53, 
	0x70, 0x7d, 0x33, 0x30 
};

static const unsigned char TURBO_PROXY_AUTH_SECRET[] = {
	0x91, 0x8F, 0xF2, 0x32, 0x59, 0x06, 0xB2, 0xEB,
	0xED, 0x4A, 0x2E, 0x76, 0x9C, 0x0A, 0x5B, 0x48,
	0x07, 0xC1, 0xD8, 0xD9, 0x3A, 0x1E, 0x53, 0x4E,
	0x85, 0x6F, 0xD8, 0x6A, 0xDF, 0xF0, 0x18, 0x2F,
	0x68, 0x69, 0xDE, 0xB1, 0xE6, 0xF9, 0x5D, 0x21,
	0x7A, 0xCA, 0x43, 0xC1, 0x3D, 0x76, 0x49, 0x2F,
	0x9B, 0xEC, 0x55, 0xF4, 0xDA, 0xDE, 0x26, 0xEC,
	0x31, 0xA5, 0xE0, 0x64, 0x5E, 0x0E, 0xEF, 0xB0
};

/*static*/
OBML_Id_Manager* OBML_Id_Manager::CreateL(OBML_Config *config)
{
	OpStackAutoPtr<OBML_Id_Manager> new_item(OP_NEW_L(OBML_Id_Manager,(config)));
	new_item->ConstructL();
	return new_item.release();
}

void OBML_Id_Manager::ConstructL()
{
	LoadDataFileL();
}

OBML_Id_Manager::~OBML_Id_Manager()
{
	g_main_message_handler->UnsetCallBacks(this);

	if( m_turbo_proxy_auth_buf )
	{
		OP_DELETEA(m_turbo_proxy_auth_buf);
	}
}

void OBML_Id_Manager::LoadDataFileL()
{
	OP_STATUS status;
	OpFile file;
	OpFileLength flen = 0;
	BOOL generate_new_id = FALSE;

	if( 
#ifdef CRYPTO_ENCRYPTED_FILE_SUPPORT
		OpStatus::IsError(status = file.ConstructEncrypted(UNI_L("optrb.dat"), reinterpret_cast<const byte*>(TURBO_DATA_FILE_OBFUSCATION_PASSWORD), 32, OPFILE_HOME_FOLDER)) ||
#else
		OpStatus::IsError(status = file.Construct(UNI_L("optrb.dat"), OPFILE_HOME_FOLDER)) ||
#endif // CRYPTO_ENCRYPTED_FILE_SUPPORT
		OpStatus::IsError(status = file.Open(OPFILE_READ)) ||
		OpStatus::IsError(status = file.GetFileLength(flen)) )
	{
		LEAVE_IF_FATAL(status);
		generate_new_id = TRUE;
	}
	else if( flen <= 0 || flen >= TURBO_DATA_FILE_MAX_SIZE ) // File empty or way too large (probably corrupted)
		generate_new_id = TRUE;

	if( !generate_new_id )
	{
		char *source = OP_NEWA_L(char, (int)flen + 1);
		ANCHOR_ARRAY(char,source);

		OP_STATUS status = OpStatus::OK;
		char *ptr = source;
		OpFileLength bytes_read = 0, total = 0;

		while (!file.Eof() && flen != static_cast<OpFileLength>(0))
		{
			if( OpStatus::IsError(status = file.Read(ptr, flen, &bytes_read)) )
			{
				if( OpStatus::IsMemoryError(status) )
				{
					LEAVE(status);
				}
				generate_new_id = TRUE;
				break;
			}
			ptr += bytes_read;
			flen -= bytes_read;
			total += bytes_read;
		}

		if( !generate_new_id )
		{
			source[total] = 0;

			// Currently data file only contains encrypted ID string...
#ifdef CRYPTO_ENCRYPTED_FILE_SUPPORT
			// ... so assign directly
			if( op_strlen(source) == OBML_CLIENT_ID_STR_LEN )
			{
				ANCHOR_ARRAY_RELEASE(source);
				m_obml_client_id.TakeOver(source);
			}
			else
				generate_new_id = TRUE;
#else
			// ... so decrypt directly and assign result
			UINT8* id_str = NULL;
			int id_str_len = 0;

			if( OpStatus::IsSuccess(
					status = g_libcrypto_master_password_encryption->EncryptPasswordWithSecurityMasterPassword(
							id_str,
							id_str_len,
							reinterpret_cast<UINT8*>(source),
							(int)total,
							(const UINT8*) TURBO_DATA_FILE_OBFUSCATION_PASSWORD,
							op_strlen(TURBO_DATA_FILE_OBFUSCATION_PASSWORD)))
					&& id_str && id_str_len == OBML_CLIENT_ID_STR_LEN )
			{
				m_obml_client_id.SetL((char*)id_str,id_str_len);
				OP_DELETEA(id_str);
			}
			else
			{
				if( id_str )
				{
					OP_DELETEA(id_str);
				}
				generate_new_id = TRUE;
			}
#endif // CRYPTO_ENCRYPTED_FILE_SUPPORT
		}
	}

	if( generate_new_id )
		GenerateRandomObmlIdL();

	LEAVE_IF_ERROR(UpdateTurboClientId());

	if( file.IsOpen() )
		OpStatus::Ignore(file.Close());
}

void OBML_Id_Manager::WriteDataFileL()
{
	OP_STATUS status;
	OpFile file;

	if( 
#ifdef CRYPTO_ENCRYPTED_FILE_SUPPORT
		OpStatus::IsError(status = file.ConstructEncrypted(UNI_L("optrb.dat"), reinterpret_cast<const byte*>(TURBO_DATA_FILE_OBFUSCATION_PASSWORD), 32, OPFILE_HOME_FOLDER)) ||
#else
		OpStatus::IsError(status = file.Construct(UNI_L("optrb.dat"), OPFILE_HOME_FOLDER)) ||
#endif // CRYPTO_ENCRYPTED_FILE_SUPPORT
		OpStatus::IsError(status = file.Open(OPFILE_WRITE)) )
	{
		LEAVE_IF_FATAL(status);
	}
	else
	{
#ifndef CRYPTO_ENCRYPTED_FILE_SUPPORT
		UINT8* tmp_str = NULL;
		int tmp_str_len = 0;

		if(OpStatus::IsSuccess(g_libcrypto_master_password_encryption->DecryptPasswordWithSecurityMasterPassword(
				tmp_str, tmp_str_len,
				(const UINT8*)m_obml_client_id.CStr(),
				OBML_CLIENT_ID_STR_LEN,
				(const UINT8*) TURBO_DATA_FILE_OBFUSCATION_PASSWORD,
				op_strlen(TURBO_DATA_FILE_OBFUSCATION_PASSWORD))))
		{
			OpStatus::Ignore(file.Write(tmp_str, tmp_str_len));
		}

		if( tmp_str )
		{
			OP_DELETEA(tmp_str);
		}
#else
		OpStatus::Ignore(file.Write(m_obml_client_id.CStr(), m_obml_client_id.Length()));
#endif // CRYPTO_ENCRYPTED_FILE_SUPPORT
	}

	if( file.IsOpen() )
		OpStatus::Ignore(file.Close());
}

void OBML_Id_Manager::HandleCallback(OpMessage msg, MH_PARAM_1, MH_PARAM_2)
{
	if( msg == MSG_OBML_COMM_WRITE_ID_TO_FILE )
	{
		g_main_message_handler->UnsetCallBack(this, MSG_OBML_COMM_WRITE_ID_TO_FILE);		
		TRAPD(err,WriteDataFileL());
	}
}

/* static */
void OBML_Id_Manager::HexAsciiEncode(byte* indata, unsigned int indata_size, char* outdata)
{
	const char hexchars[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

	for( unsigned int i = 0; i < indata_size; i++ )
	{
		unsigned char val = indata[i];
		*(outdata++) = hexchars[(val>>4) & 0x0f];
		*(outdata++) = hexchars[val & 0x0f];
	}
	*(outdata++) = '\0';
}

void OBML_Id_Manager::GenerateRandomObmlIdL()
{
	OP_NEW_DBG("OBML_Id_Manager::GenerateRandomObmlIdL","obml_id");
	byte* rnd = OP_NEWA_L(byte,OBML_CLIENT_ID_SIZE);
	ANCHOR_ARRAY(byte,rnd);

	SSL_RND(rnd, OBML_CLIENT_ID_SIZE);

	m_obml_client_id.ReserveL(OBML_CLIENT_ID_STR_LEN + 1);
	OBML_Id_Manager::HexAsciiEncode(rnd, OBML_CLIENT_ID_SIZE, m_obml_client_id.CStr());
	OP_DBG(("Generated new random OBML Id: %s",m_obml_client_id.CStr()));

	if( OpStatus::IsSuccess(g_main_message_handler->SetCallBack(this, MSG_OBML_COMM_WRITE_ID_TO_FILE, 0)) )
		g_main_message_handler->PostMessage(MSG_OBML_COMM_WRITE_ID_TO_FILE, 0, 0);
}

OP_STATUS OBML_Id_Manager::UpdateTurboClientId()
{
	OP_NEW_DBG("OBML_Id_Manager::UpdateTurboClientId","obml_id");
	if( !m_obml_client_id.HasContent() )
	{
		OP_DBG(("No ID to update from!"));
		TRAPD(err, GenerateRandomObmlIdL());
		RETURN_IF_ERROR(err);
	}

#ifdef CRYPTO_HASH_SHA256_SUPPORT
	OpStackAutoPtr<CryptoHash> digester(CryptoHash::CreateSHA256());
	if( !digester.get() || OpStatus::IsError(digester->InitHash()) )
		return OpStatus::ERR_NO_MEMORY;

	digester->CalculateHash(reinterpret_cast<UINT8*>(m_obml_client_id.CStr()), m_obml_client_id.Length());

	int digest_size = digester->Size();
	byte *digest = OP_NEWA(byte, digest_size);
	if( !digest )
		return OpStatus::ERR_NO_MEMORY;
	ANCHOR_ARRAY(byte, digest);

	digester->ExtractHash(digest);
#else
	int digest_size = SHA256_DIGEST_LENGTH;
	byte *digest = OP_NEWA(byte, digest_size);
	if( !digest )
		return OpStatus::ERR_NO_MEMORY;
	ANCHOR_ARRAY(byte, digest);

	SHA256_CTX state;
	SHA256_Init(&state);
	SHA256_Update(&state, m_obml_client_id.CStr(), m_obml_client_id.Length());
	SHA256_Final(digest, &state);
#endif // CRYPTO_HASH_SHA256_SUPPORT

	if( !m_turbo_proxy_client_id.Reserve(digest_size*2) )
		return OpStatus::ERR_NO_MEMORY;

	OBML_Id_Manager::HexAsciiEncode(digest, digest_size, m_turbo_proxy_client_id.CStr());
	OP_DBG(("New Turbo ID: %s",m_turbo_proxy_client_id.CStr()));

	return OpStatus::OK;
}

OP_STATUS OBML_Id_Manager::CreateTurboProxyAuth(OpString8& auth, unsigned int auth_id)
{
	auth.Empty(); // Just to make sure

	if( !m_turbo_proxy_auth_challenge.HasContent() )
		return OpStatus::ERR;

	int sec_len = ARRAY_SIZE(TURBO_PROXY_AUTH_SECRET);
	unsigned int buf_len = 20 + m_turbo_proxy_auth_challenge.Length() + sec_len; // room for 64 bit UINT_MAX ascii representation, challenge and secret

	if( m_turbo_proxy_auth_buf_len < buf_len )
	{
		if( m_turbo_proxy_auth_buf )
		{
			OP_DELETEA(m_turbo_proxy_auth_buf);
		}
		m_turbo_proxy_auth_buf_len = buf_len;
		m_turbo_proxy_auth_buf = OP_NEWA(byte, m_turbo_proxy_auth_buf_len);
		if( !m_turbo_proxy_auth_buf )
			return OpStatus::ERR_NO_MEMORY;
	}

	char* buf_ptr = (char*)m_turbo_proxy_auth_buf;
	int printed = op_snprintf(buf_ptr, 21, "%u", auth_id);
	buf_ptr += printed;
	unsigned int buf_content_len = printed + m_turbo_proxy_auth_challenge.Length() + sec_len;

	op_memcpy(buf_ptr,m_turbo_proxy_auth_challenge.CStr(),m_turbo_proxy_auth_challenge.Length());
	buf_ptr += m_turbo_proxy_auth_challenge.Length();
	op_memcpy(buf_ptr,TURBO_PROXY_AUTH_SECRET,sec_len);

#ifdef CRYPTO_HASH_SHA256_SUPPORT
	OpStackAutoPtr<CryptoHash> digester(CryptoHash::CreateSHA256());
	if( !digester.get() || OpStatus::IsError(digester->InitHash()) )
		return OpStatus::ERR_NO_MEMORY;

	digester->CalculateHash(m_turbo_proxy_auth_buf, buf_content_len);

	int digest_size = digester->Size();
	byte *digest = OP_NEWA(byte, digest_size);
	if( !digest )
		return OpStatus::ERR_NO_MEMORY;
	ANCHOR_ARRAY(byte, digest);

	digester->ExtractHash(digest);
#else
	int digest_size = SHA256_DIGEST_LENGTH;
	byte *digest = OP_NEWA(byte, digest_size);
	if( !digest )
		return OpStatus::ERR_NO_MEMORY;
	ANCHOR_ARRAY(byte, digest);

	SHA256_CTX state;
	SHA256_Init(&state);
	SHA256_Update(&state, m_turbo_proxy_auth_buf, buf_content_len);
	SHA256_Final(digest, &state);
#endif // CRYPTO_HASH_SHA256_SUPPORT

	// Hexascii encode
	if( !auth.Reserve(digest_size*2 + 22) ) // Room for ID + space + encoded string + NULL termination
		return OpStatus::ERR_SOFT_NO_MEMORY;

	printed = op_snprintf(auth.CStr(),22,"%u ",auth_id);
	char* auth_ptr = auth.CStr() + printed;

	OBML_Id_Manager::HexAsciiEncode(digest, digest_size, auth_ptr);

	return OpStatus::OK;
}

#endif // WEB_TURBO_MODE
