/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_MIME_MODULE_H
#define MODULES_MIME_MODULE_H

#include "modules/url/tools/arrays_decl.h"
#include "modules/url/url2.h"
#include "modules/url/url_dynattr.h"

class MimeModule : public OperaModule
{
public:
#ifndef HAS_COMPLEX_GLOBALS
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, mime_encoding_keywords);
#ifdef _MIME_SUPPORT_
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, TranslatedHeaders);
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, MIME_AddressHeaderNames);
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, mime_accesstype_keywords);
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, mimecontent_keyword);
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, mimecontent_keyword1);
#endif
#ifdef WBMULTIPART_MIXED_SUPPORT
	DECLARE_MODULE_CONST_ARRAY(const char*, binary_mpp_wellknownheadername);
	DECLARE_MODULE_CONST_ARRAY(const char*, binary_mpp_wellknowncachecontrol);
	DECLARE_MODULE_CONST_ARRAY(const char*, binary_mpp_wellknowncontenttype);
	DECLARE_MODULE_CONST_ARRAY(const char*, binary_mpp_wellknownparameter);
#endif
#endif // HAS_COMPLEX_GLOBALS
public:
	MimeModule()
		: originalContentTypeAttribute(URL::KNoStr)
		, contentIDAttribute(URL::KNoStr)
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
		, internalRedirectAttribute(URL::KNoInt)
		, originalURLAttribute(URL::KNoUrl)
#endif
	{}

	virtual ~MimeModule() {}

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	/**
	 * Get the original ContentType of an attachment.
	 * 
	 * @param attachment URL of the attachment to query
	 * @param content_type return value
	 * @return OK if no error
	 */
	OP_STATUS GetOriginalContentType(const URL &attachment, OpString8 &content_type);
	void GetOriginalContentTypeL(const URL &attachment, OpString8 &content_type) { LEAVE_IF_ERROR(GetOriginalContentType(attachment,content_type)); }

	/**
	 * For internal usage in the mime module, setting the original content-type
	 * @param attachment URL of the attachment to set content-type for
	 * @param content_type value to set
	 */
	void SetOriginalContentTypeL(URL &attachment, const OpStringC8 &content_type);

	/**
	 * Query whether an attachment has a Content-ID header
	 * @param attachment URL of the attachment
	 * @return TRUE if the attachment has a content ID
	 */
	BOOL HasContentID(const URL &attachment);

	/**
	 * Get the Content-ID header of an attachment
	 * @param attachment URL of the attachment
	 * @param content_id return value
	 * @return OK if no error
	 */
	OP_STATUS GetContentID(const URL &attachment, OpString8 &content_id);

	/**
	 * For internal usage in the mime module, setting the Content-ID header of
	 * an attachment.
	 * @param attachment URL of the attachment
	 * @param content_id value to set
	 */
	void SetContentID_L(URL &attachment, const char *content_id);

#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	/**
	 * For internal use, to enable "internal redirection" without causing the directed-to URL to be reloaded
	 * @return Attribute id to use in url->GetAttribute()
	 */
	URL::URL_Uint32Attribute GetInternalRedirectAttribute();

	/**
	 * If FEATURE_MHTML_ARCHIVE_REDIRECT is enabled, the loading of an MHTML archive
	 * may cause a redirect from the MHTML URL to the document loaded from the archive.
	 * The original URL of the MHTML archive is stored as an attribute of the
	 * redirected-to URL, and can be queried with GetAttribute() using the attribute
	 * id returned by this function. If the query returns an empty URL, it means that
	 * such redirection has not taken place.
	 *  
	 * @return Attribute id to use in url->GetAttribute()
	 */
	URL::URL_URLAttribute GetOriginalURLAttribute();
#endif

private:
	URL_DynamicStringAttributeHandler originalContentTypeAttributeHandler;
	URL::URL_StringAttribute originalContentTypeAttribute;
	URL_DynamicStringAttributeHandler contentIDAttributeHandler;
	URL::URL_StringAttribute contentIDAttribute;

#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	URL_DynamicUIntAttributeHandler internalRedirectAttributeHandler;
	URL::URL_Uint32Attribute internalRedirectAttribute;
	URL_DynamicURLAttributeHandler originalURLAttributeHandler;
	URL::URL_URLAttribute originalURLAttribute;
#endif
};

#define MIME_MODULE_REQUIRED
#define g_mime_module g_opera->mime_module

#ifndef HAS_COMPLEX_GLOBALS
#define g_mime_encoding_keywords	CONST_ARRAY_GLOBAL_NAME(mime, mime_encoding_keywords)
#ifdef _MIME_SUPPORT_
#define g_TranslatedHeaders	CONST_ARRAY_GLOBAL_NAME(mime, TranslatedHeaders)
#define g_MIME_AddressHeaderNames	CONST_ARRAY_GLOBAL_NAME(mime, MIME_AddressHeaderNames)
#define g_mime_accesstype_keywords	CONST_ARRAY_GLOBAL_NAME(mime, mime_accesstype_keywords)
#define g_mimecontent_keyword		CONST_ARRAY_GLOBAL_NAME(mime, mimecontent_keyword)
#define g_mimecontent_keyword1		CONST_ARRAY_GLOBAL_NAME(mime, mimecontent_keyword1)
#endif
#ifdef WBMULTIPART_MIXED_SUPPORT
#define g_binary_mpp_wellknownheadername       CONST_ARRAY_GLOBAL_NAME(mime, binary_mpp_wellknownheadername)
#define g_binary_mpp_wellknowncachecontrol     CONST_ARRAY_GLOBAL_NAME(mime, binary_mpp_wellknowncachecontrol)
#define g_binary_mpp_wellknowncontenttype      CONST_ARRAY_GLOBAL_NAME(mime, binary_mpp_wellknowncontenttype)
#define g_binary_mpp_wellknownparameter        CONST_ARRAY_GLOBAL_NAME(mime, binary_mpp_wellknownparameter)
#endif
#endif // HAS_COMPLEX_GLOBALS

#endif // !MODULES_MIME_MODULE_H
