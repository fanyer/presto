/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OP_GENERATED_RESPONSE_IMPL_H__
#define __OP_GENERATED_RESPONSE_IMPL_H__

#include "modules/network/op_generated_response.h"
#include "modules/network/op_request.h"

class OpGeneratedResponseImpl : public OpGeneratedResponse
{
public:
	~OpGeneratedResponseImpl();

	OpGeneratedResponseImpl(OpURL url);

	OP_STATUS SetGeneratedByOpera(BOOL value) { return m_url.SetAttribute(URL::KIsGeneratedByOpera, (UINT32) value);}

	OP_STATUS SetCachePolicyNoStore(BOOL value) { return m_url.SetAttribute(URL::KCachePolicy_NoStore, (UINT32) value);}

	OP_STATUS SetCachePolicyAlwaysVerify(BOOL value) { return m_url.SetAttribute(URL::KCachePolicy_AlwaysVerify, (UINT32) value);}

	OP_STATUS SetCachePolicyMustRevalidate(BOOL value) { return m_url.SetAttribute(URL::KCachePolicy_MustRevalidate, (UINT32) value);}

	OP_STATUS ForceMIMEContentType(const OpStringC8 &value) { return m_url.SetAttribute(URL::KMIME_ForceContentType, value); }

	OP_STATUS SetHTTPFrameOptions(const OpStringC8 &value) { return m_url.SetAttribute(URL::KHTTPFrameOptions, value); }

	OP_STATUS WriteDocumentDataUniSprintf(const uni_char *format, ...);

#ifdef NEED_URL_WRITE_DOC_IMAGE
	OP_STATUS WriteDocumentImage(const char * ctype, const char * data, int size) { return m_url.WriteDocumentImage(ctype, data, size); }
#endif // NEED_URL_WRITE_DOC_IMAGE

	OP_STATUS WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC &data, unsigned int len= (unsigned int) KAll) { return m_url.WriteDocumentData(conversion, data, len); }

	OP_STATUS WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC8 &data, unsigned int len= (unsigned int) KAll) { return m_url.WriteDocumentData(conversion, data, len); }

	OP_STATUS WriteDocumentData(URL::URL_WriteDocumentConversion conversion, URL_DataDescriptor *data, unsigned int len= (unsigned int) KAll) { return m_url.WriteDocumentData(conversion, data, len); }


	void WriteDocumentDataFinished() { m_url.WriteDocumentDataFinished(); }

#ifdef _MIME_SUPPORT_
	OP_STATUS GetBodyCommentString(OpString &value) const { return m_url.GetAttribute(URL::KBodyCommentString, value); }

	OP_STATUS SetBodyCommentString(OpString &value) { return m_url.SetAttribute(URL::KBodyCommentString, value); }

	OP_STATUS SetBigAttachmentIcons(BOOL value) { return m_url.SetAttribute(URL::KBigAttachmentIcons, (UINT32) value);}

	OP_STATUS SetPreferPlaintext(BOOL value) { return m_url.SetAttribute(URL::KPreferPlaintext, (UINT32) value);}

	OP_STATUS SetIgnoreWarnings(BOOL value) { return m_url.SetAttribute(URL::KIgnoreWarnings, (UINT32) value);}
#endif
};

#endif
