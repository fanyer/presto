/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/url/url_module.h"
#include "modules/url/tools/arrays.h"

void
MimeModule::InitL(const OperaInitInfo& info)
{
#ifndef HAS_COMPLEX_GLOBALS
	CONST_ARRAY_INIT_L(mime_encoding_keywords);
#ifdef _MIME_SUPPORT_
	CONST_ARRAY_INIT_L(mimecontent_keyword);
	CONST_ARRAY_INIT_L(mimecontent_keyword1);
	CONST_ARRAY_INIT_L(TranslatedHeaders);
	CONST_ARRAY_INIT_L(MIME_AddressHeaderNames);
	CONST_ARRAY_INIT_L(mime_accesstype_keywords);
#endif
#ifdef WBMULTIPART_MIXED_SUPPORT
	CONST_ARRAY_INIT_L(binary_mpp_wellknownheadername);
	CONST_ARRAY_INIT_L(binary_mpp_wellknowncachecontrol);
	CONST_ARRAY_INIT_L(binary_mpp_wellknowncontenttype);
	CONST_ARRAY_INIT_L(binary_mpp_wellknownparameter);
#endif
#endif // HAS_COMPLEX_GLOBALS
}

void
MimeModule::Destroy()
{
#ifndef HAS_COMPLEX_GLOBALS
	CONST_ARRAY_SHUTDOWN(mime_encoding_keywords);
#ifdef _MIME_SUPPORT_
	CONST_ARRAY_SHUTDOWN(mimecontent_keyword);
	CONST_ARRAY_SHUTDOWN(mimecontent_keyword1);
	CONST_ARRAY_SHUTDOWN(TranslatedHeaders);
	CONST_ARRAY_SHUTDOWN(MIME_AddressHeaderNames);
	CONST_ARRAY_SHUTDOWN(mime_accesstype_keywords);
#endif
#ifdef WBMULTIPART_MIXED_SUPPORT
	CONST_ARRAY_SHUTDOWN(binary_mpp_wellknownheadername);
	CONST_ARRAY_SHUTDOWN(binary_mpp_wellknowncachecontrol);
	CONST_ARRAY_SHUTDOWN(binary_mpp_wellknowncontenttype);
	CONST_ARRAY_SHUTDOWN(binary_mpp_wellknownparameter);
#endif
#endif // HAS_COMPLEX_GLOBALS
}

#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
URL::URL_Uint32Attribute MimeModule::GetInternalRedirectAttribute()
{
	if (internalRedirectAttribute == URL::KNoInt)
	{
		internalRedirectAttributeHandler.SetIsFlag(TRUE);
		TRAPD(status, internalRedirectAttribute = g_url_api->RegisterAttributeL(&internalRedirectAttributeHandler));
		if (OpStatus::IsError(status))
			internalRedirectAttribute = URL::KNoInt;
	}
	return internalRedirectAttribute;
}

URL::URL_URLAttribute MimeModule::GetOriginalURLAttribute()
{
	if (originalURLAttribute == URL::KNoUrl)
	{
		TRAPD(status, originalURLAttribute = g_url_api->RegisterAttributeL(&originalURLAttributeHandler));
		if (OpStatus::IsError(status))
			originalURLAttribute = URL::KNoUrl;
	}
	return originalURLAttribute;
}
#endif

OP_STATUS MimeModule::GetOriginalContentType(const URL &attachment, OpString8 &content_type)
{
	RETURN_IF_ERROR(attachment.GetAttribute(originalContentTypeAttribute, content_type));
	if (!content_type.IsEmpty())
		return OpStatus::OK;
	return attachment.GetAttribute(URL::KMIME_Type, content_type);
}

void MimeModule::SetOriginalContentTypeL(URL &attachment, const OpStringC8 &content_type)
{
	if (originalContentTypeAttribute == URL::KNoStr)
		originalContentTypeAttribute = g_url_api->RegisterAttributeL(&originalContentTypeAttributeHandler);
	attachment.SetAttributeL(originalContentTypeAttribute, content_type);
}

BOOL MimeModule::HasContentID(const URL &attachment)
{
	OpString8 content_id;
	GetContentID(attachment, content_id);
	return content_id.HasContent();
}

void MimeModule::SetContentID_L(URL &attachment, const char *content_id)
{
	if (contentIDAttribute == URL::KNoStr)
		contentIDAttribute = g_url_api->RegisterAttributeL(&contentIDAttributeHandler);
	attachment.SetAttributeL(contentIDAttribute, content_id);
}

OP_STATUS MimeModule::GetContentID(const URL &attachment, OpString8 &content_id)
{
	return attachment.GetAttribute(contentIDAttribute, content_id);
}
