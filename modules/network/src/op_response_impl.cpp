/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/network/src/op_resource_impl.h"
#include "modules/network/op_response.h"
#include "modules/network/src/op_response_impl.h"
#include "modules/network/op_request.h"

OpResponseImpl::~OpResponseImpl()
{
	if (m_resource)
		OP_DELETE(m_resource);
}

OP_STATUS OpResponseImpl::CreateResource(OpRequest *request)
{
	if (!GetResource())
	{
		//If we have a HEAD request, no response body so no need to create a resource object.
		if (request->GetHTTPMethod() == HTTP_Head && GetHTTPResponseCode()<400)
			return OpStatus::OK;

		m_resource = OP_NEW(OpResourceImpl,(m_url, request));
		if (!m_resource)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	return OpStatus::OK;
}

OpResponseImpl::OpResponseImpl(OpURL url)
:m_url(url.GetURL())
,m_op_url(url)
,m_resource(NULL)
{
}

OP_STATUS OpResponseImpl::GetHeader(OpString8 &value, const char* header_name, BOOL concatenate) const
{
	OP_STATUS result= OpStatus::ERR;
	RETURN_IF_ERROR(value.Set(header_name));

	// Need special case for X-Frame-Options, as it can be overriden by OpGeneratedResponse::SetHTTPFrameOptions, and the KHTTPSpecificResponseHeaderL does not read from the right location
	if (op_strcmp(header_name, "X-Frame-Options") == 0)
		return value.Set(m_url.GetAttribute(URL::KHTTPFrameOptions));

	if (concatenate)
		RETURN_IF_LEAVE(result = m_url.GetAttribute(URL::KHTTPSpecificResponseHeadersL, value, URL::KNoRedirect));
	else
		RETURN_IF_LEAVE(result = m_url.GetAttribute(URL::KHTTPSpecificResponseHeaderL, value, URL::KNoRedirect));

	return result;
}
