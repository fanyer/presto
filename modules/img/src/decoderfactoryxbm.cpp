/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef _XBM_SUPPORT_

#include "modules/img/decoderfactoryxbm.h"

#if defined(INTERNAL_XBM_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)

#include "modules/img/src/imagedecoderxbm.h"

class CheckSizeXbmDecoderListener : public ImageDecoderListener
{
public:
	CheckSizeXbmDecoderListener() : w(0), h(0) {}

	int w, h;

	virtual void OnLineDecoded(void* data, INT32 line, INT32 lineHeight) {}
	virtual BOOL OnInitMainFrame(INT32 width, INT32 height) { w = width; h = height; return FALSE; }
	virtual void OnNewFrame(const ImageFrameData& image_frame_data) {}
	virtual void OnAnimationInfo(INT32 nrOfRepeats) {}
	virtual void OnDecodingFinished() {}

#ifdef IMAGE_METADATA_SUPPORT
	virtual void OnMetaData(ImageMetaData id, const char* data) {}
#endif // IMAGE_METADATA_SUPPORT
#ifdef EMBEDDED_ICC_SUPPORT
	virtual void OnICCProfileData(const UINT8* data, unsigned datalen) {}
#endif // EMBEDDED_ICC_SUPPORT

#ifdef ASYNC_IMAGE_DECODERS
	virtual OpBitmap* GetCurrentBitmap() { return NULL; }
	virtual void OnPortionDecoded() {}
	virtual void OnDecodingFailed(OP_STATUS reason) {}
	virtual INT32 GetContentType() { return 0; }
	virtual INT32 GetContentSize() { return 0; }
#endif // ASYNC_IMAGE_DECODERS
};

ImageDecoder* DecoderFactoryXbm::CreateImageDecoder(ImageDecoderListener* listener)
{
	ImageDecoder* image_decoder = OP_NEW(ImageDecoderXbm, ());
	if (image_decoder != NULL)
	{
		image_decoder->SetImageDecoderListener(listener);
	}
	return image_decoder;
}

#endif // INTERNAL_XBM_SUPPORT

BOOL3 DecoderFactoryXbm::CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height)
{
	CheckSizeXbmDecoderListener* listener = OP_NEW(CheckSizeXbmDecoderListener, ());
	if (!listener)
		return NO; // FIXME:OOM
	ImageDecoder* decoder = DecoderFactoryXbm::CreateImageDecoder(listener);
	if (!decoder)
	{
		OP_DELETE(listener);
		return NO; // FIXME:OOM
	}
	int resendBytes;
	OP_STATUS ret_val = decoder->DecodeData((BYTE*)data, len, FALSE, resendBytes);
	OP_DELETE(decoder);
	BOOL3 ret = MAYBE;
	if (OpStatus::IsSuccess(ret_val))
	{
		width = listener->w;
		height = listener->h;
		if (width > 0 && height > 0)
			ret = YES;
	}
	else
	{
		ret = NO;
	}
	OP_DELETE(listener);

	return ret;
}

BOOL3 DecoderFactoryXbm::CheckType(const UCHAR* data, INT32 len)
{
	if (len < 8)
	{
		return MAYBE;
	}

	if (op_strncmp((const char*)data, "#define ", 8) == 0)
	{
		return YES;
	}

	return NO;
}

#endif // _XBM_SUPPORT_
