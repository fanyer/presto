/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#include "core/pch.h"

#ifdef WEBP_SUPPORT

#include "modules/img/src/imagedecoderwebp.h"

OP_STATUS ImageDecoderWebP::DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all/* = FALSE*/)
{
	return m_dec.Decode(data, numBytes, more, resendBytes);
}

#endif // WEBP_SUPPORT
