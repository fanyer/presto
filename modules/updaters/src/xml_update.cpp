/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#ifdef UPDATERS_ENABLE_AUTO_FETCH

#include "modules/updaters/xml_update.h"
#include "modules/formats/uri_escape.h"

#include "modules/formats/base64_decode.h"
#include "modules/cache/url_stream.h"


#ifdef CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT
# include "modules/libcrypto/include/CryptoVerifySignedTextFile.h"
# define VERIFIER_FUNC CryptoVerifySignedTextFile
# define VERIFIER_ALG CRYPTO_HASH_TYPE_SHA256
#else
# include "modules/libssl/tools/signed_textfile.h"
# define VERIFIER_FUNC VerifySignedTextFile
# define VERIFIER_ALG SSL_SHA_256
#endif

#ifdef ROOTSTORE_SIGNKEY
#include "modules/rootstore/auto_update_versions.h"
#else
#include "modules/libssl/auto_update_version.h"

#include AUTOUPDATE_PUBKEY_INCLUDE
#endif

XML_Updater::XML_Updater()
:	verify_file(TRUE),
#ifdef ROOTSTORE_SIGNKEY
	pub_sigkey(g_rootstore_sign_pubkey),
	pub_sigkey_len(g_rootstore_sign_pubkey_len),
#else
	pub_sigkey(AUTOUPDATE_CERTNAME),
	pub_sigkey_len(sizeof(AUTOUPDATE_CERTNAME)),
#endif
	pub_sigalg(VERIFIER_ALG)
{
}

XML_Updater::~XML_Updater()
{
}

OP_STATUS XML_Updater::Construct(URL &url, OpMessage fin_msg, MessageHandler *mh)
{
	RETURN_IF_ERROR(URL_Updater::Construct(url,fin_msg, mh));

	return OpStatus::OK;
}

BOOL XML_Updater::VerifyFile(URL &url)
{
	OP_ASSERT(pub_sigkey && pub_sigkey_len);
	if(!pub_sigkey || !pub_sigkey_len || url.IsEmpty())
		return FALSE;

	return VERIFIER_FUNC(url, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n<!--", 
					pub_sigkey, pub_sigkey_len, pub_sigalg);
}


OP_STATUS XML_Updater::ResourceLoaded(URL &url)
{
	if (verify_file && !VerifyFile(url))
	{
		return OpRecStatus::ILLEGAL_SIGNATURE;
	}

	URL_DataStream xml_reader(url);
	DataStream_ByteArray_Base xml_data;
	OpString xml_string;

	xml_data.SetNeedDirectAccess(TRUE);
	TRAPD(op_err, xml_data.AddContentL(&xml_reader));
	if(OpStatus::IsSuccess(op_err))
	{
		op_err = xml_string.Set((const uni_char *)xml_data.GetDirectPayload(), UNICODE_DOWNSIZE(xml_data.GetLength()));
		if(OpStatus::IsSuccess(op_err) && xml_string.IsEmpty())
			op_err = OpStatus::ERR;
	}

	if(OpStatus::IsError(op_err) ||
		OpStatus::IsError(op_err = parser.Parse(xml_string.CStr())) ||
		OpStatus::IsError(op_err = ProcessFile()))
	{
		return op_err;
	}
	return OpStatus::OK;
}

#ifdef UPDATERS_XML_GET_FLAG
BOOL XML_Updater::GetFlag(const OpStringC &id)
{
	if(parser.EnterElement(id.CStr()))
	{
		parser.LeaveElement();
		return TRUE;
	}
	return FALSE;
}
#endif

#ifdef UPDATERS_XML_GET_TEXT
OP_STATUS XML_Updater::GetTextData(OpString &target)
{
	TempBuffer data;

	target.Empty();

	RAISE_AND_RETURN_IF_ERROR(parser.GetAllText(data));

	return target.Set(data.GetStorage(), data.Length());
}
#endif

OP_STATUS XML_Updater::GetBase64Data(DataStream_ByteArray_Base &target)
{
	TempBuffer data;

	target.Resize(0);

	RAISE_AND_RETURN_IF_ERROR(parser.GetAllText(data));

	OpString8 raw_data;
	RAISE_AND_RETURN_IF_ERROR(raw_data.SetUTF8FromUTF16(data.GetStorage(), data.Length()));

	size_t len = raw_data.Length();
	target.SetNeedDirectAccess(TRUE);
	RAISE_AND_RETURN_IF_ERROR(target.Resize((len/4)*3+3, TRUE,TRUE));

	unsigned long decode_len, read_pos = 0;
	BOOL warning = FALSE;
	
	decode_len = GeneralDecodeBase64((const unsigned char *) raw_data.CStr(), len, read_pos, target.GetDirectPayload(), warning, target.GetLength(), NULL, NULL);

	if(read_pos != len)
	{
		target.Resize(0);
		return OpStatus::ERR_PARSING_FAILED;
	}

	RETURN_IF_ERROR(target.Resize(decode_len));

	return OpStatus::OK;
}

OP_STATUS XML_Updater::GetHexData(DataStream_ByteArray_Base &target)
{
	target.Resize(0);

	const uni_char *data = parser.GetText();

	OpString8 raw_data;
	if(data)
		RAISE_AND_RETURN_IF_ERROR(raw_data.SetUTF8FromUTF16(data, uni_strlen(data)));

	size_t len = raw_data.Length();
	target.SetNeedDirectAccess(TRUE);
	RAISE_AND_RETURN_IF_ERROR(target.Resize((len+1)/2, TRUE,TRUE));

	unsigned char *input;
	unsigned char val1=0, val2=0;
	byte *output;
	size_t write_pos =0;

	input = (unsigned char *) raw_data.CStr();
	output = target.GetDirectPayload();
	while(*input)
	{
		do{
			val1 = *(input++);
		}while(val1 && !uni_isxdigit(val1));  // Ignore non-hex characters when they are between pairs
		if(!val1)
			break; 
		val2 = *(input++);
		if(!val2 || !uni_isxdigit(val1) || !uni_isxdigit(val2))
		{
			target.Resize(0);
			return OpStatus::ERR_PARSING_FAILED;
		}

		write_pos++;
		*(output++)= UriUnescape::Decode(val1, val2);
	}
	
	RAISE_AND_RETURN_IF_ERROR(target.Resize(write_pos));

	return OpStatus::OK;
}

#ifdef _NATIVE_SSL_SUPPORT_
#include "modules/libssl/sslv3.h"

OP_STATUS XML_Updater::GetBase64Data(SSL_varvector32 &target)
{
	return GetBase64Data(target.AccessPayloadRecord());
}

OP_STATUS XML_Updater::GetHexData(SSL_varvector32 &target)
{
	return GetHexData(target.AccessPayloadRecord());
}

OP_STATUS XML_Updater::GetHexData(SSL_varvector24 &target)
{
	return GetHexData(target.AccessPayloadRecord());
}

#endif

#endif
