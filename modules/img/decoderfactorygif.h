/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DECODERFACTORYGIF_H
#define DECODERFACTORYGIF_H

#include "modules/img/imagedecoderfactory.h"
#include "modules/img/image.h"

class DecoderFactoryGif : public ImageDecoderFactory
#ifndef ASYNC_IMAGE_DECODERS
, public ImageDecoderListener
#endif
{
public:
#if defined(INTERNAL_GIF_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)
	ImageDecoder* CreateImageDecoder(ImageDecoderListener* listener);
#endif // INTERNAL_GIF_SUPPORT
	BOOL3 CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height);
	BOOL3 CheckType(const UCHAR* data, INT32 len);

#ifndef ASYNC_IMAGE_DECODERS
	virtual void OnLineDecoded(void* data, INT32 line, INT32 lineHeight) {}
	virtual BOOL OnInitMainFrame(INT32 width, INT32 height) { peekwidth = width; peekheight = height; return FALSE; }
	virtual void OnNewFrame(const ImageFrameData& image_frame_data) {}
	virtual void OnAnimationInfo(INT32 nrOfRepeats) {}
	virtual void OnDecodingFinished() {}
#ifdef IMAGE_METADATA_SUPPORT
	virtual void OnMetaData(ImageMetaData id, const char* data){}
#endif // IMAGE_METADATA_SUPPORT
#ifdef EMBEDDED_ICC_SUPPORT
	virtual void OnICCProfileData(const UINT8* data, unsigned datalen){}
#endif // EMBEDDED_ICC_SUPPORT
private:
	INT32 peekwidth, peekheight;
#endif
};

#endif // !DECODERFACTORYGIF_H
