/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#include "core/pch.h"

#ifdef WEBP_SUPPORT

#include "modules/img/image.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/webp/lib/webp/types.h"
#include "modules/webp/webp.h"

/***********************************************************************************
**
**	WebPDecoder
**
***********************************************************************************/

// static
BOOL3 WebPDecoder::Peek(const UCHAR* data, INT32 len, INT32& width, INT32& height)
{
	// looking in WebPGetInfo, it appears the size is stored in 4
	// continuous bytes starting at offset 6 from the start of the VP8
	// chunk, which starts at offset 20 of the WebP data.
	if (len < WEBP_HEADER_SIZE)
		return MAYBE;
	INT32 w, h;
	if (!WebPGetInfo(data, len, &w, &h))
		return NO;
	width = w;
	height = h;
	return YES;
}

// static
BOOL3 WebPDecoder::Check(const UCHAR* data, INT32 len)
{
	OP_ASSERT(len >= 0);

	const UINT8 expected[] = {
		'R',  'I',  'F',  'F',
		0x00, 0x00, 0x00, 0x00, // size of image data - ignored
		'W',  'E',  'B',  'P',
		'V',  'P',
	};
	const UINT8 mask[] = {
		0xff, 0xff, 0xff, 0xff, // "RIFF"
		0x00, 0x00, 0x00, 0x00, // size of image data - ignored
		0xff, 0xff, 0xff, 0xff, // "WEBP"
		0xff, 0xff,             // "VP"
	};
	UINT8 res[sizeof(mask)];

	const INT32 to_check = MIN((size_t)len, sizeof(mask));

	// first to_check bytes of data are bit-wize and:ed with mask, and
	// should match expected
	op_memcpy(res, data, to_check);
	for (INT32 i = 0; i < to_check; ++i)
		res[i] &= mask[i];
	if (op_memcmp(res, expected, to_check))
		return NO;
	return (size_t)len >= sizeof(mask) ? YES : MAYBE;
}

WebPDecoder::WebPDecoder()
    : m_listener(0)
	, m_pidec(NULL)
	, m_start_y(0)
	, m_hdr_len(0)
{
}

WebPDecoder::~WebPDecoder()
{
	if (m_pidec)
	{
		WebPIDelete(m_pidec);
		WebPFreeDecBuffer(&m_buffer);
	}
}

void WebPDecoder::SendFrameInfo(const WebPBitstreamFeatures& features)
{
	OP_ASSERT(m_listener);

	// mainframe
	m_listener->OnInitMainFrame(features.width, features.height);

	// frame data
	ImageFrameData fd;
	fd.rect.width  = features.width;
	fd.rect.height = features.height;
	fd.interlaced = FALSE;
	fd.bits_per_pixel = 32;
	fd.alpha = features.has_alpha;
	fd.bottom_to_top = FALSE;
	m_listener->OnNewFrame(fd);
}

OP_STATUS WebPDecoder::Decode(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes)
{
	OP_ASSERT(m_listener);

	// Lazy initialization of the libwebp decoder object.
	if (!m_pidec)
	{
		if (!WebPInitDecBuffer(&m_buffer))
			return OpStatus::ERR;

#if defined(PLATFORM_COLOR_IS_RGBA)
		m_buffer.colorspace = MODE_RGBA;
#else
		m_buffer.colorspace = MODE_BGRA;
#endif

		m_pidec = WebPINewDecoder(&m_buffer);

		if (!m_pidec)
		{
			WebPFreeDecBuffer(&m_buffer);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	// Parse header.
	if (m_hdr_len < WEBP_HEADER_SIZE)
	{
		INT32 need_len = WEBP_HEADER_SIZE - m_hdr_len;
		INT32 copy_len = MIN(need_len, numBytes);
		op_memcpy(m_hdr_buf + m_hdr_len, data, copy_len);
		m_hdr_len += copy_len;

		if (m_hdr_len == WEBP_HEADER_SIZE)
		{
			WebPBitstreamFeatures features;
			switch (WebPGetFeatures(m_hdr_buf, m_hdr_len, &features))
			{
			case VP8_STATUS_OK:
				SendFrameInfo(features);
				break;

			case VP8_STATUS_OUT_OF_MEMORY:
				return OpStatus::ERR_NO_MEMORY;

				// Treat any other status code as a generic/decoding error.
			default:
				return OpStatus::ERR; // Not a WebP image.
			}
		}
	}

	VP8StatusCode res = WebPIAppend(m_pidec, data, numBytes);
	switch (res)
	{
		case VP8_STATUS_OK:         // Decoding finished.
		case VP8_STATUS_SUSPENDED:  // Decoding not finished, needs more data.
		{
			int last_y = 0, height = 0, stride = 0;
			uint8_t* outbuf = WebPIDecGetRGB(m_pidec, &last_y, NULL, &height, &stride);
			if (!outbuf)
			{
				// Return error if we're not done and there is no more data.
				if (res == VP8_STATUS_SUSPENDED)
					return more ? OpStatus::OK : OpStatus::ERR;

				return OpStatus::ERR;
			}

			for (int y = m_start_y; y < last_y; ++y)
				m_listener->OnLineDecoded(outbuf + y * stride, y, 1);

			m_start_y = last_y;

			// Check if we're done.
			if (last_y == height)
			{
				m_listener->OnDecodingFinished();
				return OpStatus::OK;
			}

			if (!more)
				return OpStatus::ERR;

			return OpStatus::OK;
		}

		case VP8_STATUS_OUT_OF_MEMORY:
			return OpStatus::ERR_NO_MEMORY;

		case VP8_STATUS_NOT_ENOUGH_DATA:
		case VP8_STATUS_INVALID_PARAM:
		case VP8_STATUS_BITSTREAM_ERROR:
		case VP8_STATUS_UNSUPPORTED_FEATURE:
		case VP8_STATUS_USER_ABORT:
		default:
			return OpStatus::ERR;
	}
}

#endif // WEBP_SUPPORT
