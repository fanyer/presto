/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/network/src/op_generated_response_impl.h"
#include "modules/network/op_resource.h"

OpGeneratedResponseImpl::~OpGeneratedResponseImpl()
{
}

OpGeneratedResponseImpl::OpGeneratedResponseImpl(OpURL url)
:OpGeneratedResponse(url)
{
}

OP_STATUS OpGeneratedResponseImpl::WriteDocumentDataUniSprintf(const uni_char *format, ...)
{
	OpString temp_buffer;
	va_list args;

	va_start(args, format);
	RETURN_IF_ERROR(temp_buffer.AppendVFormat(format, args));
	va_end(args);

	return WriteDocumentData(URL::KNormal, temp_buffer);
}
