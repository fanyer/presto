/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

// -----------------------------------------------------------------------

#ifndef IMAGEDECODERASYNC_H
#define IMAGEDECODERASYNC_H

// -----------------------------------------------------------------------

#include "modules/img/image.h"
#include "modules/img/src/imagedecodergif.h"

#include "modules/hardcore/mh/mh.h"

class OpBitmap;

// -----------------------------------------------------------------------

/**
 * A class for "emulating" a asynchronous decoder
 * using a synchronous decoder
 **/

// -----------------------------------------------------------------------

class ImageDecoderAsync :
	public ImageDecoder,
	public ImageDecoderListener,
	public MessageObject
{
public:

// -----------------------------------------------------------------------

	ImageDecoderAsync(ImageDecoder* impl);

	~ImageDecoderAsync();

// -----------------------------------------------------------------------

	// ImageDecoder

	OP_STATUS DecodeData(const BYTE* data, UINT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE);

	void SetImageDecoderListener(ImageDecoderListener* imageDecoderListener);

// -----------------------------------------------------------------------

	// MessageObject

	void HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

// -----------------------------------------------------------------------

	// ImageDecoderListener

	void OnLineDecoded(void* data, UINT32 line, UINT32 lineHeight, BOOL indexed = FALSE);

	void GetLineFromBitmap(void* data, UINT32 line, UINT32 lineHeight);

	BOOL OnInitMainFrame(UINT32 width, UINT32 height);

	void OnNewFrame(OpBitmap* bitmap);

	void OnNewFrame(UINT32			width,
					UINT32			height,
					INT32			x,
					INT32			y,
					BOOL			interlaced,
					BOOL			transparent,
					BOOL			alpha,
					UINT32			duration,
					DisposalMethod	disposalMethod,
					UINT32			bitsPerPixel,
					// Parameters for indexed bitmaps:
					INT32			indexed = 0,
					INT32			transparent_index = 0,
					UINT8*			pal = NULL,
					UINT32			num_colors = 0);

	void OnDecodingFailed(OP_STATUS reason);

	void OnAnimationInfo(INT32 nrOfRepeats);

	void OnDecodingFinished();

	OpBitmap* GetCurrentBitmap();

	void OnPortionDecoded();

// -----------------------------------------------------------------------

private:

	enum { MSG_ASYNC_BASE = 0x6666,
		   MSG_ASYNC_INIT,
		   MSG_ASYNC_NEW_FRAME,
		   MSG_ASYNC_NEW_LINE,
		   MSG_ASYNC_DONE }; // not used?

	ImageDecoderListener* m_image_decoder_listener;

	ImageDecoder* m_impl;

	bool m_message_pending;

// -----------------------------------------------------------------------

	class Buffer
	{

	public:

		Buffer();

		OP_STATUS   Append(const char* buf, size_t length);
		const char* GetContent() const;
		OP_STATUS   Consume(size_t length);
		size_t      GetSize() const;
		OP_STATUS   Clear();
		bool        IsEmpty();

	private:

		char*  m_buf;
		size_t m_size;

		Buffer(const Buffer&);
		Buffer& operator =(const Buffer&);

	};

	Buffer m_buf;

	ImageDecoderListener* m_listener;

// -----------------------------------------------------------------------

	enum { MSG_ASYNC_DECODER = 0xdada };

};

// -----------------------------------------------------------------------

#endif // IMAGEDECODERASYNC_H
