/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/network/op_data_descriptor.h"

OpDataDescriptor::OpDataDescriptor(URL_DataDescriptor *data_descriptor)
:m_data_descriptor(data_descriptor)
{
}

OpDataDescriptor::~OpDataDescriptor()
{
	OP_DELETE(m_data_descriptor);
}

OP_STATUS OpDataDescriptor::RetrieveData(OpData &dst, OpFileLength retrieve_limit)
{
	size_t length = 0;
	BOOL more = TRUE;
	BOOL some_data_transferred = FALSE;
	while (more)
	{
		RETURN_IF_LEAVE(length = (size_t)m_data_descriptor->RetrieveDataL(more));
		if (length == 0)
			break;
		if (retrieve_limit && length > (size_t)retrieve_limit)
		{
			length = (size_t)retrieve_limit;
			more = FALSE;
		}
		// TODO: Use OpData for streaming internally in url module to avoid this copying:
		OP_STATUS status = dst.AppendCopyData(m_data_descriptor->GetBuffer(), length);
		if (OpStatus::IsError(status))
		{
			g_memory_manager->RaiseCondition(status);
			return some_data_transferred ? OpStatus::OK : status;
		}
		m_data_descriptor->ConsumeData((unsigned int)length);
		some_data_transferred = TRUE;
		if (retrieve_limit)
			retrieve_limit -= (OpFileLength)length;
	}
	return OpStatus::OK;
}

OP_STATUS OpDataDescriptor::RetrieveData(UniString &dst, size_t retrieve_limit)
{
	size_t length = 0;
	BOOL more = TRUE;
	BOOL some_data_transferred = FALSE;
	while (more)
	{
		RETURN_IF_LEAVE(length = (size_t)UNICODE_DOWNSIZE(m_data_descriptor->RetrieveDataL(more)));
		if (length == 0)
			break;
		if (retrieve_limit && length > retrieve_limit)
		{
			length = retrieve_limit;
			more = FALSE;
		}
		// TODO: Use UniString for streaming text internally in url module to avoid this copying:
		OP_STATUS status = dst.AppendCopyData(reinterpret_cast<const uni_char*>(m_data_descriptor->GetBuffer()), length);
		if (OpStatus::IsError(status))
		{
			g_memory_manager->RaiseCondition(status);
			return some_data_transferred ? OpStatus::OK : status;
		}
		m_data_descriptor->ConsumeData((unsigned int)UNICODE_SIZE(length));
		some_data_transferred = TRUE;
		if (retrieve_limit)
			retrieve_limit -= length;
	}
	return OpStatus::OK;
}

OP_STATUS OpDataDescriptor::SetPosition(OpFileLength pos)
{
	if ((URLCacheType)m_data_descriptor->GetURL().GetAttribute(URL::KCacheType) == URL_CACHE_STREAM)
		return OpStatus::ERR_OUT_OF_RANGE; // We do not want to create a new request
	return m_data_descriptor->SetPosition(pos);
}
