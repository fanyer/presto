/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#include "core/pch.h" 
 
#ifdef M2_SUPPORT 

#include "adjunct/desktop_util/image_utils/fileimagecontentprovider.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/engine/index/indeximage.h"

#include "modules/display/vis_dev.h"
#include "modules/pi/OpBitmap.h"
#include "modules/img/imagedump.h"

IndexImage::~IndexImage()
{
	OP_DELETE(m_image);
}

OP_STATUS IndexImage::GetImageAsBase64(OpString& image) const
{
	if (!m_image)
	{
		return image.Set(UNI_L(""));
	}
	TempBuffer base64;
	OpBitmap *bitmap = m_image->GetImage().GetBitmap(NULL);
	if(bitmap)
	{
		OP_STATUS s = GetOpBitmapAsBase64PNG(bitmap, &base64);
		m_image->GetImage().ReleaseBitmap();
		RETURN_IF_ERROR(s);
	}
	return image.Set(base64.GetStorage());
}

OP_STATUS IndexImage::SetImageFromBase64(const OpStringC8& buffer)
{
	unsigned char* content = NULL;
	unsigned long len = 0;
	
	RETURN_IF_ERROR(DecodeBase64(buffer, content, len));

	if (m_image)
		OP_DELETE(m_image);
	
	m_image = OP_NEW(SimpleFileImage, ((UINT8*)content, len));
	
	OP_DELETEA(content);
	return m_image == NULL? OpStatus::ERR_NO_MEMORY : OpStatus::OK;
}


OP_STATUS IndexImage::SetImageFromFileName(const OpStringC& filename)
{
	if (filename.IsEmpty())
		return OpStatus::ERR;

	SimpleFileImage* simple_image = OP_NEW(SimpleFileImage, (filename));
	if (!simple_image)
		return OpStatus::ERR_NO_MEMORY;

	if (m_image)
	{
		OP_DELETE(m_image);
	}
	m_image = simple_image;

	Image image = m_image->GetImage();

	if (image.Width() > 16 || image.Height() > 16)
	{
		// scale image
		OpBitmap* bitmap = 0;
		if (OpStatus::IsSuccess(OpBitmap::Create(&bitmap, 16, 16, FALSE, TRUE, 0, 0, TRUE)))
		{
			VisualDevice vd;
			vd.painter = bitmap->GetPainter();		// returns NULL on OOM
			if (vd.painter)
			{
				OpRect dst_rect(0, 0, 16, 16);
				OpRect src_rect(0,0,image.Width(),image.Height());
				vd.ClearRect(dst_rect);
				vd.ImageOutEffect(image, src_rect, dst_rect, 0, 0);
				vd.painter = NULL;
				bitmap->ReleasePainter();
			}
			image.ReplaceBitmap(bitmap);
		}
	}

	return OpStatus::OK;
}

Image IndexImage::GetBitmapImage() const
{
	return m_image->GetImage();
}

#endif // M2_SUPPORT 
