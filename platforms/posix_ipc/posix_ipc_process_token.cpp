/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/posix_ipc/posix_ipc_process_token.h"

#include "modules/otl/list.h"

PosixIpcProcessToken::PosixIpcProcessToken(OpMessageAddress requester, OpComponentType type, int cm_id, int read_pipe, int write_pipe)
	: m_token()
	, m_requester(requester)
	, m_type(type)
	, m_cm_id(cm_id)
	, m_read_fd(read_pipe)
	, m_write_fd(write_pipe)
{
}

const char* PosixIpcProcessToken::Encode()
{
	if (m_token.Length() == 0)
	{
		if (OpStatus::IsError(m_token.AppendFormat("%d,%d.%d.%d,%d,%d,%d",
			(int) m_type, m_requester.cm, m_requester.co, m_requester.ch, m_cm_id, m_read_fd, m_write_fd)))
		{
			return NULL;
		}
	}

	const char* data = m_token.Data(TRUE);
	return data;
}

/* static */
PosixIpcProcessToken* PosixIpcProcessToken::Decode(const char* encoded)
{
	OpData token;
	if (OpStatus::IsError(token.SetConstData(encoded)))
		return NULL;
	OpAutoPtr< OtlCountedList<OpData> > parts(token.Split(','));

	if (parts->Length() != 5)
		return NULL;

	int component_type;
	OpMessageAddress remoteAddress;
	int cm_id;
	int read_pipe;
	int write_pipe;

	if (OpStatus::IsError(parts->PopFirst().ToInt(&component_type)) ||
		!(remoteAddress = OpMessageAddress::Deserialize(parts->PopFirst())).IsValid() ||
		OpStatus::IsError(parts->PopFirst().ToInt(&cm_id)) ||
		OpStatus::IsError(parts->PopFirst().ToInt(&read_pipe)) ||
		OpStatus::IsError(parts->PopFirst().ToInt(&write_pipe)))
		return NULL;

	OP_ASSERT(component_type >= 0 && component_type < COMPONENT_LAST);

	return OP_NEW(PosixIpcProcessToken, (remoteAddress, static_cast<OpComponentType>(component_type), cm_id, read_pipe, write_pipe));
}

