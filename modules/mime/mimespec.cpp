/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

//#include "url_man.h"
#include "modules/url/url2.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/mime/mimedec2.h"
#include "modules/upload/upload.h"

//#include "url_cs.h"
//#include "mime_cs.h"

#include "modules/mime/mimetnef.h"
#include "modules/mime/smimefun.h"
#include "modules/mime/mime_enum.h"

#include "modules/hardcore/mem/mem_man.h"

#include "modules/olddebug/tstdump.h"

#ifdef M2_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#endif

#include "modules/util/handy.h"

#ifdef _MIME_SUPPORT_

/**
 *  mimespec.cpp
 *
 *  This file is used to determine MIME content types, and whether 
 *  or not they are to be handled by special classes
 *
 *  The types must be represented in the MIME_ContentTypeID enumerated
 *  value defined in mimedec.h
 *
 *  Add the necessary specific types in mimecontent_keyword table below,
 *  and any non-specific types in mimecontent_keyword1.
 *
 *  Add the required id's to the MIME_Decoder::SpecialHandling to
 *  trigger special handling
 *
 *  Allocation of the appropriate class objects must then be added in the 
 *  case selection in the CreateSubElement functions
 */

#include "modules/url/tools/arrays.h"

#define CONST_KEYWORD_ARRAY(name) PREFIX_CONST_ARRAY(static, KeywordIndex, name, mime)
#define CONST_KEYWORD_ENTRY(x,y) CONST_DOUBLE_ENTRY(keyword, x, index, y)
#define CONST_KEYWORD_END(name) CONST_END(name)

/**
 *  Add specific content types to be specially handled here
 *  **** Note: Alphabetiacally Sorted ****
 */
CONST_KEYWORD_ARRAY(mimecontent_keyword)
	CONST_KEYWORD_ENTRY((char*) NULL,MIME_Other)
	CONST_KEYWORD_ENTRY("application/mime", MIME_Mime)
	CONST_KEYWORD_ENTRY("application/ms-tnef", MIME_MS_TNEF_data)
	CONST_KEYWORD_ENTRY("application/octet-stream", MIME_Binary)
#ifdef _SUPPORT_OPENPGP_
	CONST_KEYWORD_ENTRY("application/pgp", MIME_PGP_File)
	CONST_KEYWORD_ENTRY("application/pgp-encrypted", MIME_PGP_Encrypted)
	CONST_KEYWORD_ENTRY("application/pgp-signature", MIME_PGP_Signed)
#endif
#ifdef _SUPPORT_SMIME_
	CONST_KEYWORD_ENTRY("application/pkcs7-mime", MIME_SMIME_pkcs7)
#endif
#ifdef WBMULTIPART_MIXED_SUPPORT
	CONST_KEYWORD_ENTRY("application/vnd.wap.multipart.mixed", MIME_Multipart_Binary)
	CONST_KEYWORD_ENTRY("application/vnd.wap.multipart.related", MIME_Multipart_Binary)
#endif
#ifdef WBXML_SUPPORT
	CONST_KEYWORD_ENTRY("application/vnd.wap.wbxml", MIME_XML_text)
	CONST_KEYWORD_ENTRY("application/vnd.wap.wmlc", MIME_XML_text)
#endif
	CONST_KEYWORD_ENTRY("application/vnd.wap.xhtml+xml", MIME_XML_text)
#ifdef _SUPPORT_SMIME_
	CONST_KEYWORD_ENTRY("application/x-pkcs7-signature", MIME_SMIME_pkcs7)
#endif
	CONST_KEYWORD_ENTRY("application/x-shockwave-flash", MIME_Flash_plugin)
	CONST_KEYWORD_ENTRY("application/xhtml+xml", MIME_XML_text)
	CONST_KEYWORD_ENTRY("application/xml",MIME_XML_text)
	CONST_KEYWORD_ENTRY("image/bmp", MIME_BMP_image)
	CONST_KEYWORD_ENTRY("image/gif", MIME_GIF_image)
	CONST_KEYWORD_ENTRY("image/jpeg", MIME_JPEG_image)
	CONST_KEYWORD_ENTRY("image/png", MIME_PNG_image)
#if defined(SVG_SUPPORT)
	CONST_KEYWORD_ENTRY("image/svg+xml", MIME_SVG_image)
#endif
#ifdef WBMP_SUPPORT
	CONST_KEYWORD_ENTRY("image/vnd.wap.wbmp", MIME_BMP_image)
#endif
	CONST_KEYWORD_ENTRY("image/x-windows-bmp", MIME_BMP_image)
	CONST_KEYWORD_ENTRY("message/external-body", MIME_ExternalBody)
	CONST_KEYWORD_ENTRY("multipart/alternative" ,MIME_Alternative)
#if defined(_SUPPORT_SMIME_) || defined(_SUPPORT_OPENPGP_)
	CONST_KEYWORD_ENTRY("multipart/encrypted", MIME_Multipart_Encrypted)
#endif
	CONST_KEYWORD_ENTRY("multipart/related", MIME_Multipart_Related)
#if defined(_SUPPORT_SMIME_) || defined(_SUPPORT_OPENPGP_)
	CONST_KEYWORD_ENTRY("multipart/signed", MIME_Multipart_Signed)
#endif
	CONST_KEYWORD_ENTRY("text", MIME_Plain_text)
	CONST_KEYWORD_ENTRY("text/calendar", MIME_calendar_text)
	CONST_KEYWORD_ENTRY("text/css", MIME_CSS_text)
	CONST_KEYWORD_ENTRY("text/html",MIME_HTML_text)
	CONST_KEYWORD_ENTRY("text/javascript",MIME_Javascript_text)
	CONST_KEYWORD_ENTRY("text/plain", MIME_Plain_text)
	CONST_KEYWORD_ENTRY("text/vnd.wap.wml",MIME_XML_text)
	CONST_KEYWORD_ENTRY("text/wml",MIME_XML_text)
	CONST_KEYWORD_ENTRY("text/xml",MIME_XML_text)
CONST_KEYWORD_END(mimecontent_keyword)

/**
 *  Add non-specific content types to be specially handled here
 *  **** Note: Alphabetiacally Sorted ****
 */
CONST_KEYWORD_ARRAY(mimecontent_keyword1)
	CONST_KEYWORD_ENTRY((char*) NULL,MIME_Other)
	CONST_KEYWORD_ENTRY("multipart/", MIME_MultiPart)
	CONST_KEYWORD_ENTRY("text/", MIME_Text)
	CONST_KEYWORD_ENTRY("audio/", MIME_audio)
	CONST_KEYWORD_ENTRY("video/", MIME_video)
	CONST_KEYWORD_ENTRY("image/", MIME_image)
	CONST_KEYWORD_ENTRY("message/", MIME_Message)
CONST_KEYWORD_END(mimecontent_keyword1)

MIME_ContentTypeID MIME_Decoder::FindContentTypeId(const char *type_string)
{
	MIME_ContentTypeID type_id;

	type_id  = (MIME_ContentTypeID) CheckKeywordsIndex(type_string,g_mimecontent_keyword, (int)CONST_ARRAY_SIZE(mime, mimecontent_keyword));
	
	if(type_id == MIME_Other)
	{
		type_id  = (MIME_ContentTypeID) CheckStartsWithKeywordIndex(type_string,g_mimecontent_keyword1, (int)CONST_ARRAY_SIZE(mime, mimecontent_keyword1));
	}

	return type_id;
}

void MIME_MultipartBase::CreateNewBodyPartWithNewHeaderL(const OpStringC8 &p_content_type, 
														const OpStringC &p_filename, 
														const OpStringC8 &p_content_encoding)
{
	ANCHORD(Header_List, temp_header_list);

	if(p_filename.HasContent())
	{
		temp_header_list.AddParameterL("Content-Disposition", "attachment");
		temp_header_list.AddRFC2231ParameterL("Content-Disposition","filename",p_filename, NULL);
	}

	CreateNewBodyPartWithNewHeaderL(temp_header_list, p_content_type, p_content_encoding);
}

void MIME_MultipartBase::CreateNewBodyPartWithNewHeaderL(const OpStringC8 &p_content_type, 
														 const OpStringC8 &p_filename, 
														 const OpStringC8 &p_content_encoding)
{
	ANCHORD(Header_List, temp_header_list);

	if(p_filename.HasContent())
	{
		temp_header_list.AddParameterL("Content-Disposition", "attachment");
		temp_header_list.AddParameterL("Content-Disposition","filename",p_filename);
	}

	CreateNewBodyPartWithNewHeaderL(temp_header_list, p_content_type, p_content_encoding);
}

void MIME_MultipartBase::CreateNewBodyPartWithNewHeaderL(/*upload*/ Header_List &headers, const OpStringC8 &p_content_type, const OpStringC8 &p_content_encoding)
{
	if(p_content_type.HasContent())
		headers.AddParameterL("Content-Type", p_content_type);
	if(p_content_encoding.HasContent())
		headers.AddParameterL("Content-Transfer-Encoding", p_content_encoding);

	ANCHORD(OpString8, tempheader);
	tempheader.ReserveL(headers.CalculateLength()+30); // A little extra, just in case
	char *hdr_pos = headers.OutputHeaders(tempheader.DataPtr());
	if(!hdr_pos)
		LEAVE(OpStatus::ERR_NULL_POINTER);
	*hdr_pos = '\0';

	CreateNewBodyPartL((unsigned char *) tempheader.CStr(), tempheader.Length());
}

void MIME_MultipartBase::CreateNewBodyPartL(HeaderList &hdrs)
{
	FinishSubElementL();

	OP_MEMORY_VAR MIME_ContentTypeID id = MIME_Plain_text; // if no MIME type, assume it is text
	HeaderEntry *hdr = hdrs.GetHeaderByID(HTTP_Header_Content_Type);
	if(hdr)
	{
		ParameterList *parameters = hdr->GetParametersL(PARAM_SEP_SEMICOLON| PARAM_ONLY_SEP | PARAM_STRIP_ARG_QUOTES );

		if(parameters && parameters->First())
		{
			Parameters *ctyp = parameters->First();

			id = FindContentTypeId(ctyp->Name());
		}
	}

	TRAPD(op_err, current_element = CreateNewBodyPartL(id, hdrs, base_url_type));
	if(OpStatus::IsError(op_err))
	{
		current_element = NULL;
		g_memory_manager->RaiseCondition(op_err);
		RaiseDecodeWarning();
		return;
	}

	if(current_element)
	{
		current_element->Into(&sub_elements);
		++*number_of_parts_counter;
	}
	else
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);

}

MIME_Decoder *MIME_MultipartBase::CreateNewBodyPartL(MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type)
{
	return MIME_Decoder::CreateDecoderL(this, id, hdrs, url_type, base_url
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
										, GetContextID()
#endif
		);
}

MIME_Decoder *MIME_Decoder::CreateDecoderL(const MIME_Decoder *parent, MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type, URL_Rep* url
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
												, URL_CONTEXT_ID ctx_id
#endif
							)
{
	BOOL attachment_flag = FALSE;

	if(parent)
	{
		HeaderEntry *header = hdrs.GetHeaderByID(HTTP_Header_Content_Disposition);
		if(header)
		{
			ParameterList *parameters = header->GetParametersL((PARAM_SEP_SEMICOLON| PARAM_ONLY_SEP | PARAM_STRIP_ARG_QUOTES | PARAM_HAS_RFC2231_VALUES), KeywordIndex_HTTP_General_Parameters);
			
			attachment_flag = (parameters && !parameters->Empty() && 
				parameters->First()->GetNameID() != HTTP_General_Tag_Inline);
		}
		if (parent->nesting_depth >= 16)
		{
			id = MIME_ignored_body;
		}
	}

	OpStackAutoPtr<MIME_Decoder> decoder(NULL);

#if defined(_SUPPORT_SMIME_) || defined(_SUPPORT_OPENPGP_)
retry_id:;
#endif
	switch(id)
	{
		/** 
		 *  NOTE: Add Allocation for Content Types that require special handling here
		 *  NOTE: For classes: ALWAYS execute SetHeaders() after construction of MIME_Decoder
		 *        NEVER use the headerlist construction (virtual function calls are not yet initialized !!
		 */
	case MIME_MS_TNEF_data:
		decoder.reset(OP_NEW_L(MS_TNEF_Decoder, (hdrs, url_type)));
		break;
#ifdef _SUPPORT_SMIME_
	case MIME_SMIME_pkcs7:
	case MIME_SMIME_Signed:
		decoder.reset(OP_NEW_L(SMIME_Decoder, (hdrs, url_type)));
		if(decoder.get() == NULL)
		{
			id = MIME_Text;
			goto retry_id;
		}
		break;
#endif
#ifdef _SUPPORT_OPENPGP_
	case MIME_PGP_File:
	case MIME_PGP_Signed:
	case MIME_PGP_Encrypted:
	case MIME_PGP_Keys:
		decoder.reset((MIME_Decoder *)CreatePGPDecoderL(id, hdrs, url_type));
		if(decoder.get() == NULL)
		{
			id = MIME_Text;
			goto retry_id;
		}
		break;
#endif
#if defined(_SUPPORT_SMIME_) || defined(_SUPPORT_OPENPGP_)
	case MIME_Multipart_Signed:
	case MIME_Multipart_Encrypted:
		decoder.reset(CreateSecureMultipartDecoderL(id, hdrs, url_type));
		break;
#endif
#ifdef WBMULTIPART_MIXED_SUPPORT
	case MIME_Multipart_Binary:
		decoder.reset(OP_NEW_L(MIME_Multipart_Decoder, (hdrs, url_type, TRUE)));
		break;
#endif
	case MIME_Alternative:
		decoder.reset(OP_NEW_L(MIME_Multipart_Alternative_Decoder, (hdrs, url_type)));
		break;

	case MIME_MultiPart:
		decoder.reset(OP_NEW_L(MIME_Multipart_Decoder, (hdrs, url_type)));
		break;

	case MIME_Multipart_Related:
		decoder.reset(OP_NEW_L(MIME_Multipart_Related_Decoder, (hdrs, url_type)));
		break;

	case MIME_Mime:
	case MIME_Message:
		decoder.reset(OP_NEW_L(MIME_Mime_Payload, (hdrs, url_type)));
		break;

	case MIME_HTML_text:
	case MIME_XML_text:
		decoder.reset(OP_NEW_L(MIME_Formatted_Payload, (hdrs, url_type)));
		break;

	case MIME_ExternalBody:
		decoder.reset(OP_NEW_L(MIME_External_Payload, (hdrs, url_type)));
		break;

	case MIME_ignored_body:
		decoder.reset(OP_NEW_L(MIME_IgnoredBody, (hdrs, url_type)));
		break;

	case MIME_Plain_text:
	case MIME_Text:
		{

			if(!attachment_flag)
			{
				decoder.reset(OP_NEW_L(MIME_Unprocessed_Text, (hdrs, url_type)));
				break;
			}
			// Fall through to MIME_Text
		}

	case MIME_CSS_text:
	case MIME_Javascript_text:
	case MIME_GIF_image:
	case MIME_JPEG_image:
	case MIME_PNG_image:
	case MIME_BMP_image:
	case MIME_SVG_image:
	case MIME_Flash_plugin:
#if defined(M2_SUPPORT)
		if(!attachment_flag ||
		   (id==MIME_GIF_image || id==MIME_JPEG_image || id==MIME_PNG_image || id==MIME_BMP_image || id == MIME_SVG_image)
#if defined(NEED_URL_MIME_DECODE_LISTENERS) 
		   || g_pcm2->GetIntegerPref(PrefsCollectionM2::ShowAttachmentsInline)
#endif // NEED_URL_MIME_DECODE_LISTENERS 
		   )
#endif
		{
			decoder.reset(OP_NEW_L(MIME_Displayable_Payload, (hdrs, url_type)));
			break;
		}
		// Use payload decoder instead (as attachment);
	case MIME_image:
	case MIME_calendar_text:
	default:
		// MIME_CSS_text might be probably also handled here
		decoder.reset(CreateNewMIME_PayloadDecoderL(NULL, hdrs, url_type));
		break;
	}

	if(parent)
		decoder->InheritAttributes(parent);

#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
	if(parent && !ctx_id)
		ctx_id = parent->GetContextID();
	decoder->SetContextID(ctx_id);
#endif
	decoder->SetBaseURL(url);
	
	decoder->ConstructL();

	return decoder.release();
}

MIME_Decoder *MIME_Decoder::CreateDecoderL(const MIME_Decoder *parent, const unsigned char *src, unsigned long src_len, URLType url_type, URL_Rep* url
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
												, URL_CONTEXT_ID ctx_id
#endif
										   )
{
	HeaderList headers(KeywordIndex_HTTP_MIME);
	ANCHOR(HeaderList, headers);
	ANCHORD(OpString8, header_string);

	header_string.SetL((const char *) src, src_len);

	headers.SetValueL(header_string.CStr(), 0); 

	MIME_ContentTypeID id = MIME_Other;
	HeaderEntry *hdr = headers.GetHeaderByID(HTTP_Header_Content_Type);
	if(hdr)
	{
		ParameterList *parameters = hdr->GetParametersL(PARAM_SEP_SEMICOLON| PARAM_ONLY_SEP | PARAM_STRIP_ARG_QUOTES );

		if(parameters && parameters->First())
		{
			Parameters *ctyp = parameters->First();

			id = FindContentTypeId(ctyp->Name());
		}
		else
			id = MIME_Plain_text;
	}
	else
		id = MIME_Plain_text;

	return CreateDecoderL(parent, id, headers, url_type, url
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
							, ctx_id
#endif
		);
}

MIME_Decoder *MIME_Decoder::CreateNewMIME_PayloadDecoderL(const MIME_Decoder *parent, HeaderList &hdr, URLType url_type)
{
	MIME_Decoder *decoder = OP_NEW_L(MIME_Payload, (hdr, url_type)); // Safe, nothing leaves after this call

	if(parent)
		decoder->InheritAttributes(parent);

	return decoder;
}

void MIME_MultipartBase::CreateNewMIME_PayloadBodyPartL(HeaderList &hdrs)
{
	TRAPD(op_err, current_element = CreateNewMIME_PayloadDecoderL(this, hdrs, base_url_type));
	if(OpStatus::IsError(op_err))
	{
		current_element = NULL;
		g_memory_manager->RaiseCondition(op_err);
		RaiseDecodeWarning();
		LEAVE(op_err);
	}

	OP_ASSERT(current_element);

	current_element->Into(&sub_elements);
	++*number_of_parts_counter;
}


// Do not add special handling
void MIME_MultipartBase::CreateNewBodyPartL(const unsigned char *src, unsigned long src_len)
{
	HeaderList headers(KeywordIndex_HTTP_MIME);
	ANCHOR(HeaderList, headers);
	ANCHORD(OpString8, header_string);

	header_string.SetL((const char *) src, src_len);

	headers.SetValueL(header_string.CStr(), 0); 

	CreateNewBodyPartL(headers);
}

#endif
