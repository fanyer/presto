/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

// -----------------------------------------------------------------------

#include "core/pch.h"

#ifdef ASYNC_IMAGE_DECODERS_EMULATION

#include "modules/img/src/ImageDecoderAsync.h"

#include "modules/hardcore/mh/mh.h"

#include "modules/pi/OpBitmap.h"

#include <cassert>

// -----------------------------------------------------------------------

ImageDecoderAsync::Buffer::Buffer() : m_buf(NULL),
									  m_size(0)
{}

OP_STATUS
ImageDecoderAsync::Buffer::Append(const char* buf, size_t size)
{
    assert(((m_buf != NULL) && (m_size > 0)) || // either not empty with nonzero size
           ((m_buf == NULL) && (m_size == 0))); // or empty with zero size

    assert(((buf != NULL) && (size > 0)) || // either not empty with nonzero size
           ((buf == NULL) && (size == 0))); // or empty with zero size

    if (buf == NULL)
    {
        return OpStatus::ERR;
    }

	size_t old_size = m_size;
	size_t new_size = old_size + size;

	char* old_buf = m_buf;
	char* new_buf = OP_NEWA(char, new_size);
	if (new_buf == NULL)
		return OpStatus::ERR_NO_MEMORY;

    op_memcpy(new_buf, old_buf, old_size);
    op_memcpy(new_buf + old_size, buf, size);

    OP_DELETEA(old_buf);

    m_buf  = new_buf;
    m_size = new_size;

    return OpStatus::OK;
}

OP_STATUS
ImageDecoderAsync::Buffer::Consume(size_t size)
{
	size_t old_size = m_size;
	size_t new_size = old_size - size;

	char* old_buf = m_buf;
	char* new_buf = (new_size != 0 ? OP_NEWA(char, new_size) : NULL);

	if (new_size != 0)
	{
		op_memcpy(new_buf, old_buf + size, new_size);
	}

	OP_DELETEA(old_buf);

	m_buf  = new_buf;
	m_size = new_size;

	return OpStatus::OK;
}

const char*
ImageDecoderAsync::Buffer::GetContent() const
{
    return m_buf;
}

size_t
ImageDecoderAsync::Buffer::GetSize() const
{
    return m_size;
}

OP_STATUS
ImageDecoderAsync::Buffer::Clear()
{
    OP_DELETEA(m_buf);
    m_buf = NULL;
    m_size = 0;
    return OpStatus::OK;
}

bool
ImageDecoderAsync::Buffer::IsEmpty()
{
    assert(((m_buf != NULL) && (m_size > 0)) || // either not empty with nonzero size
           ((m_buf == NULL) && (m_size == 0))); // or empty with zero size

    return (m_size == 0);
}

// -----------------------------------------------------------------------

ImageDecoderAsync::ImageDecoderAsync(ImageDecoder* impl)
		: m_image_decoder_listener(NULL),
		  m_impl(impl),
		  m_message_pending(false),
		  m_buf()
{
	m_impl->SetImageDecoderListener(this);
	g_main_message_handler->SetCallBack(this, MSG_ASYNC_DECODER, (int)this, 0);
}

ImageDecoderAsync::~ImageDecoderAsync()
{
	g_main_message_handler->UnsetCallBack(this, MSG_ASYNC_DECODER);
	OP_DELETE(m_impl);
	// delete m_bitmap;  -- deleted by ImageFrame
}

// -----------------------------------------------------------------------

// ImageDecoder

OP_STATUS
ImageDecoderAsync::DecodeData(const BYTE* data, UINT32 size, BOOL more, int& resendBytes, BOOL/* load_all = FALSE*/)
{
	resendBytes = 0;
	// here we don't want to have to copy the data.
	OP_STATUS ret = m_buf.Append((char*)data, size);

	if (!m_message_pending)
	{
		g_main_message_handler->PostDelayedMessage(MSG_ASYNC_DECODER, (int)this, 0, 1000);
		m_message_pending = true;
	}

	return ret;
}

void
ImageDecoderAsync::SetImageDecoderListener(ImageDecoderListener* listener)
{
	m_listener = listener;
}

// -----------------------------------------------------------------------

void
ImageDecoderAsync::HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if ((ImageDecoderAsync*)par1 != this)
	{
		return;
	}

	if (msg != MSG_ASYNC_DECODER)
	{
		// error!
		return;
	}

	m_message_pending = false;

	// consume some data and post a new message

	if (!m_buf.IsEmpty())
	{
		// consume at most 4k at a time
		size_t bytes_to_consume = min(4096, m_buf.GetSize());
		int resendBytes;
		m_impl->DecodeData(m_buf.GetContent(), bytes_to_consume, TRUE, resendBytes);
		m_buf.Consume(bytes_to_consume);

		OnPortionDecoded();
	}

	if (!m_buf.IsEmpty())
	{
		g_main_message_handler->PostDelayedMessage(MSG_ASYNC_DECODER, (int)this, 0, 1000);
		m_message_pending = true;
	}
}

// -----------------------------------------------------------------------

// ImageDecoderListener

void
ImageDecoderAsync::GetLineFromBitmap(void* data, UINT32 line, UINT32 lineHeight)
{
	m_listener->GetLineFromBitmap(data, line, lineHeight);
}

void
ImageDecoderAsync::OnLineDecoded(void* data, UINT32 line, UINT32 lineHeight)
{
	m_listener->OnLineDecoded(data, line, lineHeight);
}

void
ImageDecoderAsync::OnInitMainFrame(UINT32 width, UINT32 height)
{
	m_listener->OnInitMainFrame(width, height);
}

void
ImageDecoderAsync::OnNewFrame(OpBitmap* bitmap)
{
	m_listener->OnNewFrame(bitmap);
}

void
ImageDecoderAsync::OnNewFrame(UINT32		 width,
							  UINT32		 height,
							  INT32			 x,
							  INT32			 y  ,
							  BOOL			 interlaced,
							  BOOL			 transparent,
							  BOOL			 alpha,
							  UINT32		 duration,
							  DisposalMethod disposalMethod,
							  UINT32		 bitsPerPixel)
{
	m_listener->OnNewFrame(width, height, x, y, interlaced, transparent, alpha, duration, disposalMethod, bitsPerPixel);
}

void
ImageDecoderAsync::OnAnimationInfo(INT32 nrOfRepeats)
{
	m_listener->OnAnimationInfo(nrOfRepeats);
}

void
ImageDecoderAsync::OnDecodingFinished()
{
	m_listener->OnDecodingFinished();
}

OpBitmap*
ImageDecoderAsync::GetCurrentBitmap()
{
	return m_listener->GetCurrentBitmap();
}

void
ImageDecoderAsync::OnPortionDecoded()
{
	m_listener->OnPortionDecoded();
}

// -----------------------------------------------------------------------

#endif // ASYNC_IMAGE_DECODERS_EMULATION
