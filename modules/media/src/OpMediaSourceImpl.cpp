/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/media/src/OpMediaSourceImpl.h"

/* static */ OP_STATUS
OpMediaSource::Create(OpMediaSource** source_out, OpMediaHandle handle)
{
	if (!source_out)
		return OpStatus::ERR_NULL_POINTER;

	MediaSource* source_rep = NULL;
	RETURN_IF_ERROR(g_media_source_manager->GetSource(source_rep, handle));
	OP_ASSERT(source_rep);

	OpMediaSourceImpl* source_impl = OP_NEW(OpMediaSourceImpl, (source_rep));
	if (!source_impl)
	{
		g_media_source_manager->PutSource(source_rep);
		return OpStatus::ERR_NO_MEMORY;
	}

	source_rep->AddClient(source_impl);
	*source_out = source_impl;

	return OpStatus::OK;
}

OpMediaSourceImpl::OpMediaSourceImpl(MediaSource* source) :
	m_source(source)
{
}

/* virtual */
OpMediaSourceImpl::~OpMediaSourceImpl()
{
	m_source->RemoveClient(this);
	g_media_source_manager->PutSource(m_source);
}

/* virtual */ OP_STATUS
OpMediaSourceImpl::Read(void* buffer, OpFileLength offset, OpFileLength size)
{
	OP_ASSERT(buffer);
	OP_ASSERT(offset != FILE_LENGTH_NONE);
	OP_ASSERT(size > 0 && size != FILE_LENGTH_NONE);
	if (!buffer)
		return OpStatus::ERR_NULL_POINTER;
	OP_STATUS status = m_source->Read(buffer, offset, size);
	if (OpStatus::IsSuccess(status))
	{
		m_pending = MediaByteRange(); // empty range
	}
	else if (m_pending.start != offset || m_pending.Length() != size)
	{
		m_pending = MediaByteRange(offset);
		m_pending.SetLength(size);
		m_source->HandleClientChange();
	}
	return status;
}

/* virtual */ OP_STATUS
OpMediaSourceImpl::Preload(OpFileLength offset, OpFileLength size)
{
	OP_ASSERT(offset != FILE_LENGTH_NONE);
	m_preload = MediaByteRange(offset);
	m_preload.SetLength(size);
	m_source->HandleClientChange();
	return OpStatus::OK;
}

#endif // MEDIA_PLAYER_SUPPORT
