/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "adjunct/desktop_util/image_utils/iconutils.h"
#include "adjunct/desktop_util/image_utils/fileimagecontentprovider.h"
#include "modules/minpng/minpng.h"
#include "modules/minpng/minpng_encoder.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpPainter.h"

OpBitmap* IconUtils::GetBitmap(const OpStringC& filename, int width, int height)
{
	OpFile file;
	OpFileLength length;
	if (OpStatus::IsError(file.Construct(filename)) || OpStatus::IsError(file.Open(OPFILE_READ)) ||
		OpStatus::IsError(file.GetFileLength(length)) || length == 0)
		return 0;

	OpFileLength bytes_read;
	char* data = OP_NEWA(char, length);
	if (!data || OpStatus::IsError(file.Read(data, length, &bytes_read)) || bytes_read != length)
	{
		OP_DELETEA(data);
		return 0;
	}

	StreamBuffer<UINT8> buffer;
	buffer.TakeOver(data, length);

	return GetBitmap(buffer, width, height);
}


OpBitmap* IconUtils::GetBitmap(const UINT8* buffer, UINT32 buffer_size, int width, int height)
{
	FileImageContentProvider provider;
	Image image;

	provider.LoadFromBuffer(buffer, buffer_size, image);

	OpBitmap* loaded_bitmap = image.GetBitmap(NULL);
	if (!loaded_bitmap)
	{
		image.DecVisible(null_image_listener);
		return 0;
	}

	int src_width  = loaded_bitmap->Width();
	int src_height = loaded_bitmap->Height();
	if (src_width <= 0 || src_height <= 0)
	{
		image.DecVisible(null_image_listener);
		return 0;
	}
	if (width<=0)
		width = src_width;

	if (height<=0)
		height = src_height;

	OpBitmap* bitmap = 0;
	OP_STATUS rc = OpBitmap::Create(&bitmap, width, height, loaded_bitmap->IsTransparent(), loaded_bitmap->HasAlpha(), 0, 0, TRUE);
	if (OpStatus::IsError(rc))
	{
		image.ReleaseBitmap();
		image.DecVisible(null_image_listener);
		return 0;
	}

	OpPainter* painter = bitmap->GetPainter();
	if (!painter)
	{
		image.ReleaseBitmap();
		image.DecVisible(null_image_listener);
		OP_DELETE(bitmap);
		return 0;
	}

	painter->ClearRect(OpRect(0,0,width,height));
	painter->DrawBitmapScaled(loaded_bitmap, OpRect(0,0,src_width,src_height), OpRect(0,0,width,height));

	bitmap->ReleasePainter();

	image.ReleaseBitmap();
	image.DecVisible(null_image_listener);
	return bitmap;
}


OpBitmap* IconUtils::GetBitmap(const StreamBuffer<UINT8>& buffer, int width, int height)
{
	return GetBitmap(buffer.GetData(), buffer.GetFilled(), width, height);
}


OP_STATUS IconUtils::WriteBitmapToPNGBuffer(OpBitmap* bitmap, StreamBuffer<UINT8>& buffer)
{
	PngEncFeeder feeder;
	PngEncRes res;

	minpng_init_encoder_feeder(&feeder);

	feeder.has_alpha = 1;
	feeder.scanline = 0;
	feeder.xsize = bitmap->Width();
	feeder.ysize = bitmap->Height();

	feeder.scanline_data = OP_NEWA(UINT32, bitmap->Width());
	if (!feeder.scanline_data)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	if (!bitmap->GetLineData(feeder.scanline_data, feeder.scanline))
	{
		OP_DELETEA(feeder.scanline_data);
		minpng_clear_encoder_feeder(&feeder);
		return OpStatus::ERR;
	}

	OP_STATUS result = OpStatus::OK;
	BOOL again = TRUE;
	while (again)
	{
		switch (minpng_encode(&feeder, &res))
		{
		case PngEncRes::AGAIN:
			break;
		case PngEncRes::ILLEGAL_DATA:
			result = OpStatus::ERR;
			again = FALSE;
			break;
		case PngEncRes::OOM_ERROR:
			result = OpStatus::ERR_NO_MEMORY;
			again = FALSE;
			break;
		case PngEncRes::OK:
			again = FALSE;
			break;
		case PngEncRes::NEED_MORE:
			++feeder.scanline;
			if (feeder.scanline >= bitmap->Height() ||
				!bitmap->GetLineData(feeder.scanline_data, feeder.scanline))
			{
				OP_DELETEA(feeder.scanline_data);
				minpng_clear_encoder_feeder(&feeder);
				return OpStatus::ERR;
			}
			break;
		}

		if (res.data_size)
		{
			if (OpStatus::IsError(buffer.Append(res.data, res.data_size)))
			{
				OP_DELETEA(feeder.scanline_data);
				minpng_clear_encoder_feeder(&feeder);
				return OpStatus::ERR_NO_MEMORY;
			}
		}

		minpng_clear_encoder_result(&res);
	}

	OP_DELETEA(feeder.scanline_data);

	minpng_clear_encoder_feeder(&feeder);

	return result;
}

