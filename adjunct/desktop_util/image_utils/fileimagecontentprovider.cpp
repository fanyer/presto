/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "fileimagecontentprovider.h"
#include "modules/util/opfile/opfile.h"

OP_STATUS FileImageContentProvider::LoadFromFile(const uni_char* filename, Image& image)
{
	if (filename == NULL)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	UINT8* data = NULL;

	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename));

	BOOL exists;
	RETURN_IF_ERROR(file.Exists(exists));
	if(!exists)
	{
		return OpStatus::ERR_FILE_NOT_FOUND;
	}
	
	RETURN_IF_ERROR(file.Open(OPFILE_READ));
	
	OpFileLength length;
	RETURN_IF_ERROR(file.GetFileLength(length));
	
	data = OP_NEWA(UINT8, (int)length);
	if(!data)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	OpFileLength bytes_read = 0;
	if (file.Read(data, length, &bytes_read) != OpStatus::OK)
	{
		OP_DELETEA(data);
		return OpStatus::ERR;
	}

	m_data = (char*)data;
	m_num_bytes = (int)length;

	image = imgManager->GetImage(this);

	image.IncVisible(null_image_listener); // IMG-EMIL Must be balanced and don't use the provider in DecVisible
	image.OnLoadAll(this);

	OP_DELETEA(data);

	m_data = 0;
	m_num_bytes = 0;

	return image.ImageDecoded() ? OpStatus::OK : OpStatus::ERR;
}


OP_STATUS FileImageContentProvider::LoadFromBuffer(const UINT8* buffer, UINT32 num_bytes, Image& image)
{
	UINT8* data = OP_NEWA(UINT8, (int)num_bytes);
	if(!data)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	op_memcpy(data, buffer, num_bytes);

	m_data = (char*)data;
	m_num_bytes = (int)num_bytes;

	image = imgManager->GetImage(this);

	image.IncVisible(null_image_listener); // IMG-EMIL Must be balanced and don't use the provider in DecVisible
	image.OnLoadAll(this);

	OP_DELETEA(data);

	m_data = 0;
	m_num_bytes = 0;

	return image.ImageDecoded() ? OpStatus::OK : OpStatus::ERR;
}


INT32 FileImageContentProvider::CheckImageType(const UCHAR* data, INT32 len)
{
	if (len < 4)
	{
		return -1;
	}
	if (memcmp(data, "\0\0\1\0", 4) == 0)
	{
		return URL_ICO_CONTENT;
	}

	return -1;
}	


OP_STATUS SimpleImageReader::Read(const OpStringC& file_path)
{
	// Must create the new provider before destroying the old one.  This
	// guarantees the new one will have a different address.  Without that
	// guarantee, ImageManager can return the same ImageRep even though we
	// loaded a different image.
	SimpleFileImage* provider = OP_NEW(SimpleFileImage, (file_path.CStr()));
	OP_DELETE(m_provider);
	m_provider = provider;
	RETURN_OOM_IF_NULL(m_provider);

	return GetImage().ImageDecoded() ? OpStatus::OK : OpStatus::ERR;
}
