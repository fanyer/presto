/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#include "core/pch.h"

#ifdef _MIME_SEC_ENABLED_

#include "modules/url/url2.h"

#include "modules/mime/mimedec2.h"
#include "modules/mime/mimesec.h"
#include "modules/mime/smimefun.h"

#include "modules/datastream/fl_lib.h"

#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"

#include "modules/libssl/sslhead.h"
#include "modules/libssl/sslopt.h"
#include "modules/libssl/ssldlg.h"

/**
 *  Add specific MICalg types and their associated SSL_HashAlgorithmType enum values here
 *  **** Note: Alphabetiacally Sorted ****
 */
static const KeywordIndex mic_alg_keyword[] = {
	{(char*) NULL,SSL_NoHash},
#if defined(_SUPPORT_OPENPGP_)
	{"pgp-md5", SSL_MD5},
	{"pgp-ripemd160", SSL_RIPEMD160},
	{"pgp-sha1", SSL_SHA},
#endif
#if defined(_SUPPORT_SMIME_)
#endif
};



MIME_Decoder *CreateSecureMultipartDecoderL(MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type)
{
	switch(id)
	{
	case MIME_Multipart_Signed:
		return OP_NEW_L(MIME_Multipart_Signed_Decoder, (hdrs, url_type));
	case MIME_Multipart_Encrypted:
		return OP_NEW_L(MIME_Multipart_Encrypted_Decoder, (hdrs, url_type));
	}
	LEAVE(OpRecStatus::ILLEGAL_FORMAT);
	return NULL;
}

MIME_Signed_Processor::MIME_Signed_Processor()
{
	digests.SetBinary(FALSE);
}

MIME_Signed_Processor::~MIME_Signed_Processor()
{
}

void MIME_Signed_Processor::AddDigestAlgL(SSL_HashAlgorithmType alg)
{
	digests.AddMethodL(alg);
}

void MIME_Signed_Processor::PerformDigestProcessingL(const unsigned char *src, unsigned long src_len)
{
	digests.WriteDataL(src, src_len);

}

MIME_Signed_MIME_Content::MIME_Signed_MIME_Content(HeaderList &hdrs, URLType url_type)
: MIME_Mime_Payload(hdrs, url_type)
{
}

MIME_Signed_MIME_Content::~MIME_Signed_MIME_Content()
{
}


void MIME_Signed_MIME_Content::PerformSpecialProcessingL(const unsigned char *src, unsigned long src_len)
{
	MIME_Signed_Processor::PerformDigestProcessingL(src, src_len);
}


MIME_Signed_Text_Content::MIME_Signed_Text_Content(HeaderList &hdrs, URLType url_type)
: MIME_Unprocessed_Text(hdrs, url_type)
{
}

MIME_Signed_Text_Content::~MIME_Signed_Text_Content()
{
}

void MIME_Signed_Text_Content::PerformSpecialProcessingL(const unsigned char *src, unsigned long src_len)
{
	MIME_Signed_Processor::PerformDigestProcessingL(src, src_len);
}


MIME_KeyName::MIME_KeyName()
{
	op_memset(KeyId, 0, sizeof(KeyId));
}

MIME_KeyName::~MIME_KeyName()
{
	if(InList())
		Out();
}

MIME_Security_Body::MIME_Security_Body(HeaderList &hdrs, URLType url_type)
: MIME_MultipartBase(hdrs, url_type)
{
	datasource = NULL;
	delay_processing = FALSE;
	payload_is_mime = FALSE;
	displayed_header = FALSE;
}

MIME_Security_Body::~MIME_Security_Body()
{
	//delete datasource;
	datasource = NULL;
	password.Wipe();
	password.Empty();
}

void MIME_Security_Body::SetCalculatedDigestL(Hash_Head *digests)
{
	mime_clearsigned_md.CopyL(digests);
}


void MIME_Security_Body::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
}

void MIME_Security_Body::ProcessDecodedDataL(BOOL more)
{
	//Processing is done later;
}

BOOL MIME_Security_Body::CheckSourceL()
{
	datasource = &AccessDecodedStream();
	return TRUE;
}

OP_STATUS MIME_Security_Body::AskPasswordL(Str::LocaleString msg_id, MIME_KeyNameList &names)
{
	password.Wipe();
	password.Empty();

	SSL_secure_varvector32 passwd;

	if(!::AskPassword(msg_id, passwd, NULL))
		return OpStatus::ERR;

	password.SetL((const char *) passwd.GetDirect(), passwd.GetLength());

	return OpStatus::OK;
}


void MIME_Security_Body::WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *storage)
{
	if(!info.finished)
		return;

	if(!displayed_header)
	{
		WriteSignatureInfoL(target, storage);
		WriteEncryptionInfoL(target, storage);
		displayed_header = TRUE;
	}

	MIME_MultipartBase::WriteDisplayDocumentL(target, storage);

	if(info.displayed)
	{
		WriteAttachmentListL(target, storage);
	}
}

void MIME_Security_Body::WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links)
{
	// already done
}

MIME_Multipart_Encrypted_Parameters::MIME_Multipart_Encrypted_Parameters(HeaderList &hdrs, URLType url_type)
 : MIME_Decoder(hdrs, url_type)
{
}

MIME_Multipart_Encrypted_Parameters::~MIME_Multipart_Encrypted_Parameters()
{
}


void MIME_Multipart_Encrypted_Parameters::ProcessDecodedDataL(BOOL more)
{
	uint32 len1 = header_string.Length();
	uint32 src_len = GetLengthDecodedData();
	
	header_string.AppendL((const char *) AccessDecodedData(), src_len);
	CommitDecodedDataL(src_len);

	uint32 len2 = header_string.Length();

	if(len2 != len1+src_len)
		RaiseDecodeWarning();
}

void MIME_Multipart_Encrypted_Parameters::HandleFinishedL()
{
	int pos = header_string.Find("\n\r\n");
	if(pos == KNotFound)
		pos = header_string.Find("\n\n");

	if(pos != KNotFound)
		header_string.Delete(pos+1); // we want the last linefeed;

	headers.SetValueL(header_string);

	HeaderEntry *header = headers.GetHeaderByID(HTTP_Header_Version);
	if(!header || !header->Value() || !*header->Value())
	{
		RaiseDecodeWarning();
		return;
	}

	if(op_atoi(header->Value()) != 1)
		RaiseDecodeWarning();
}

void MIME_Multipart_Encrypted_Parameters::WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target)
{
	// No Action
}
void MIME_Multipart_Encrypted_Parameters::WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_linksTRUE)
{
	// No Action
}

MIME_Multipart_Signed_Decoder::MIME_Multipart_Signed_Decoder(HeaderList &hdrs, URLType url_type)
: MIME_Multipart_Decoder(hdrs, url_type)
{
	signature_type = MIME_Other;
	mic_alg = SSL_NoHash;
	signed_content_processor = NULL;
	signature_body = NULL;
}

void MIME_Multipart_Signed_Decoder::HandleHeaderLoadedL()
{
	MIME_Multipart_Decoder::HandleHeaderLoadedL();

	OP_ASSERT(content_type_header != NULL);
	if(content_type_header == NULL)
	{
		RaiseDecodeWarning();
		return;
	}

	ParameterList *params = content_type_header->SubParameters();
	OP_ASSERT(params != NULL);

	Parameters *param = params->GetParameterByID(HTTP_General_Tag_Protocol, PARAMETER_ASSIGNED_ONLY);
	if(param == NULL)
	{
		RaiseDecodeWarning();
		return;
	}

	signature_type = FindContentTypeId(param->Value());
	if(signature_type != MIME_PGP_Signed && signature_type != MIME_SMIME_pkcs7)
	{
		RaiseDecodeWarning();
		return;
	}

	param = params->GetParameterByID(HTTP_General_Tag_Micalg, PARAMETER_ASSIGNED_ONLY);
	if(param == NULL)
	{
		RaiseDecodeWarning();
		return;
	}

	mic_alg = (SSL_HashAlgorithmType) CheckStartsWithKeywordIndex(param->Value(),mic_alg_keyword, ARRAY_SIZE(mic_alg_keyword));
	if(mic_alg == SSL_NoHash)
	{
		RaiseDecodeWarning();
	}
}

MIME_Multipart_Signed_Decoder::~MIME_Multipart_Signed_Decoder()
{
	signed_content_processor = NULL;
	signature_body = NULL;
}

MIME_Decoder *MIME_Multipart_Signed_Decoder::CreateNewBodyPartL(MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type)
{
	if(HaveDecodeWarnings())
		return NULL;

	if(signed_content_processor == NULL)
	{
		MIME_Signed_MIME_Content *signed_content = OP_NEW_L(MIME_Signed_MIME_Content, (hdrs, url_type));
		signed_content->ConstructL();

		signed_content->AddDigestAlgL(mic_alg);
		signed_content_processor = signed_content;
		return signed_content;
	}

	if(signature_body != NULL)
	{
		// More than two bodies is an error
		RaiseDecodeWarning();
		return MIME_MultipartBase::CreateNewBodyPartL(id, hdrs, url_type);
	}

	if(signature_type != id)
	{
		RaiseDecodeWarning();
		return NULL;
	}

	switch(signature_type)
	{
#if defined(_SUPPORT_OPENPGP_)
	case MIME_PGP_Signed:
		signature_body = (MIME_Security_Body *) CreatePGPDecoderL(signature_type, hdrs, url_type);
		break;
#endif
	}

	if(signature_body)
	{
		signature_body->ConstructL();
		signature_body->DelayProcessing();
		signature_body->SetCalculatedDigestL(&signed_content_processor->AccessDigests());
	}

	return signature_body;
}

void MIME_Multipart_Signed_Decoder::CreateNewBodyPartL(const unsigned char *src, unsigned long src_len)
{
	if(signed_content_processor == NULL)
	{
		OpStringC8 header("Content-Type: application/mime\r\n\r\n");
		MIME_MultipartBase::CreateNewBodyPartL((const unsigned char *) header.CStr(), header.Length());

		SaveDataInSubElementL(src, src_len);
	}
	else
		MIME_MultipartBase::CreateNewBodyPartL(src, src_len);
}

void MIME_Multipart_Signed_Decoder::HandleFinishedL()
{
	MIME_MultipartBase::HandleFinishedL();
	if(sub_elements.Cardinal() != 2)
	{
		RaiseDecodeWarning();
		return;
	}

	signature_body->VerifySignaturesL();
}

void MIME_Multipart_Signed_Decoder::WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target)
{
	if(info.finished)
	{
		if(signature_body)
		{
			signature_body->WriteSignatureInfoL(target, attach_target);
			signature_body->SetDisplayed(TRUE);
		}

		MIME_Multipart_Decoder::WriteDisplayDocumentL(target,attach_target);
		display_attachment_list = FALSE;
	}
}



MIME_Multipart_Encrypted_Decoder::MIME_Multipart_Encrypted_Decoder(HeaderList &hdrs, URLType url_type)
: MIME_Multipart_Decoder(hdrs, url_type)
{
	encryption_type = MIME_Other;
	parameters = NULL;
	encrypted_body = NULL;
}

void MIME_Multipart_Encrypted_Decoder::HandleHeaderLoadedL()
{
	MIME_Multipart_Decoder::HandleHeaderLoadedL();

	OP_ASSERT(content_type_header != NULL);
	if(content_type_header == NULL)
	{
		RaiseDecodeWarning();
		return;
	}

	ParameterList *params = content_type_header->SubParameters();
	OP_ASSERT(params != NULL);

	Parameters *param = params->GetParameterByID(HTTP_General_Tag_Protocol, PARAMETER_ASSIGNED_ONLY);
	if(param == NULL)
	{
		RaiseDecodeWarning();
		return;
	}

	encryption_type = FindContentTypeId(param->Value());
}

MIME_Multipart_Encrypted_Decoder::~MIME_Multipart_Encrypted_Decoder()
{
	parameters = NULL;
	encrypted_body = NULL;
}

MIME_Decoder *MIME_Multipart_Encrypted_Decoder::CreateNewBodyPartL(MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type)
{
	if(HaveDecodeWarnings())
		return NULL;

	if(parameters == NULL)
	{
		if(encryption_type != id)
		{
			RaiseDecodeWarning();
			return NULL;
		}
		switch(encryption_type)
		{
#if defined(_SUPPORT_OPENPGP_)
		case MIME_PGP_Encrypted:
			parameters =  CreatePGPEncParametersL(encryption_type, hdrs, url_type);
			break;
#endif
		}

		if(parameters)
			parameters->ConstructL();

		return parameters;
	}

	if(encrypted_body != NULL)
	{
		RaiseDecodeWarning();
		return MIME_MultipartBase::CreateNewBodyPartL(id, hdrs, url_type);
	}

	if(id != MIME_Binary || parameters->HaveDecodeWarnings())
	{
		RaiseDecodeWarning();
		return NULL;
	}

	encrypted_body = parameters->SetUpEncryptedDecoderL(hdrs, url_type);

	if(encrypted_body)
	{
		encrypted_body->ConstructL();
		encrypted_body->DelayProcessing();
		encrypted_body->SetPayloadIsMIME();
	}
	
	return encrypted_body;
}

void MIME_Multipart_Encrypted_Decoder::HandleFinishedL()
{
	MIME_MultipartBase::HandleFinishedL();

	encrypted_body->DecryptL();
	display_attachment_list = FALSE;
}

void MIME_Multipart_Encrypted_Decoder::WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target)
{
	if(info.finished)
	{
		if(encrypted_body)
		{
			encrypted_body->WriteSignatureInfoL(target, attach_target);
			encrypted_body->WriteEncryptionInfoL(target, attach_target);
		}

		MIME_Multipart_Decoder::WriteDisplayDocumentL(target,attach_target);
		display_attachment_list = FALSE;
	}
}

#endif


