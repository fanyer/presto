/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _MIME_SEC_ENABLED_


// #include "modules/libssl/sslhead.h"
#include "modules/libssl/sslv3.h"
#include "modules/upload/uploadsec.h"


Upload_Signature_Generator::Upload_Signature_Generator()
{
	prepared = FALSE;
	SetOnlyOutputBody(TRUE);
}

Upload_Signature_Generator::~Upload_Signature_Generator()
{
}

void Upload_Signature_Generator::InitL(Upload_Base *elm)
{
	Upload_EncapsulatedElement::InitL(elm, NULL, ENCODING_AUTODETECT);
}

uint32 Upload_Signature_Generator::PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions)
{
	if(prepared)
		return GetMinimumBufferSize();

	uint32 ret = Upload_EncapsulatedElement::PrepareL(boundaries, transfer_restrictions);

	ProcessPayloadActionL();

	return ret;
}

void Upload_Signature_Generator::PerformPayloadActionL(unsigned char *src, uint32 src_len)
{
	PerformDigestProcessingL(src, src_len);
}


Upload_Secure_Base::Upload_Secure_Base()
{
}

Upload_Secure_Base::~Upload_Secure_Base()
{
}

void Upload_Secure_Base::InitL(Upload_Base *elm, const OpStringC8 &content_type, MIME_Encoding enc, const char **header_names)
{
	Upload_EncapsulatedElement::InitL(elm, content_type, enc, header_names);
	SetContentTypeL(content_type);
}


Upload_Multipart_Signed::Upload_Multipart_Signed()
{
	signed_content = NULL;
	signature = NULL;
	prepared = FALSE;
}

Upload_Multipart_Signed::~Upload_Multipart_Signed()
{
	OP_DELETE(signed_content); // in the list of multiparts, but delete to be on the safe side
	OP_DELETE(signature); // in the list of multiparts, but delete to be on the safe side
	signed_content = NULL;
	signature = NULL;
}

void Upload_Multipart_Signed::InitL(const OpStringC8 &content_type, const char **header_names)
{
	Upload_Multipart::InitL((content_type.HasContent() ? content_type : "multipart/signed"), header_names);
}

void Upload_Multipart_Signed::AddContentL(Upload_Base *signd_content)
{
	OpStackAutoPtr<Upload_Base> sgnd_cnt(signd_content);

	signed_content = OP_NEW_L(Upload_Signature_Generator, ());

	signed_content->InitL(sgnd_cnt.release());

	AddElement(signed_content);
}

void Upload_Multipart_Signed::AddSignatureL(Upload_Secure_Base *sig)
{
	signature = sig;

	if(signature == NULL)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	AddElement(signature);

	AccessHeaders().AddParameterL("Content-Type", "protocol", signature->GetContentType(),TRUE);
	AccessHeaders().AddParameterL("Content-Type", "mic-alg", signature->GetSignatureDigestAlg(),TRUE);
}

uint32 Upload_Multipart_Signed::PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions)
{
	if(prepared)
		return GetMinimumBufferSize();

	uint32 min_len = Upload_Multipart::PrepareL(boundaries, transfer_restrictions);

	signature->GenerateSignatureL(signed_content->AccessDigests());

	prepared = TRUE;
	SetMinimumBufferSize(min_len);
	return min_len;
}

Upload_Multipart_Encrypted::Upload_Multipart_Encrypted()
{
	encrypted_content = NULL;
	prepared = FALSE;
}

Upload_Multipart_Encrypted::~Upload_Multipart_Encrypted()
{
	OP_DELETE(encrypted_content);
	encrypted_content = NULL;
}

void Upload_Multipart_Encrypted::InitL(Upload_Base *encryptd_parameters, Upload_Secure_Base *encryptd_content, const OpStringC8 &content_type, const char **header_names)
{
	encrypted_content = encryptd_content;
	OpStackAutoPtr<Upload_Base> encrypted_parameters(encryptd_parameters);

	if(encrypted_content == NULL || encrypted_parameters.get() == NULL)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	Upload_Multipart::InitL((content_type.HasContent() ? content_type : "multipart/encrypted"), header_names);

	AccessHeaders().AddParameterL("Content-Type", "protocol", encrypted_parameters->GetContentType(),TRUE);

	AddElement(encrypted_parameters.release());
	AddElement(encrypted_content);
}

uint32 Upload_Multipart_Encrypted::PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions)
{
	if(prepared)
		return GetMinimumBufferSize();

	uint32 min_len = Upload_Multipart::PrepareL(boundaries, transfer_restrictions);

	prepared = TRUE;

	return min_len;
}

#endif
