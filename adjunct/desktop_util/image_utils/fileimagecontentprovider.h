/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef _FILEIMAGE_CONTENT_PROVIDER_H_
#define _FILEIMAGE_CONTENT_PROVIDER_H_

#include "modules/img/image.h"

// Gives you an image from the image file with a minimum of work.
//
// m_image = new Image();
// m_image_content_provider = new FileImageContentProvider();
// m_image_content_provider->LoadFromFile( filename, *m_image );

class FileImageContentProvider : public ImageContentProvider
{
public:

	/**

	*/
	FileImageContentProvider(char* data, UINT32 num_bytes)
		: m_data(data)
		, m_num_bytes(num_bytes)
		, m_content_type(0)
		, m_loaded(FALSE) {}


	/**

	*/
	FileImageContentProvider()
		: m_data(0)
		, m_num_bytes(0)
		, m_content_type(0)
		, m_loaded(FALSE) {}


	/**
	   Read from file, decode and return image
	*/
	OP_STATUS LoadFromFile(const uni_char* filename, Image& image);

	/**
	   Read from file, decode and return image
	*/
	OP_STATUS LoadFromBuffer(const UINT8* buffer, UINT32 num_bytes, Image& image);

	/**
	   Implement ImageContentProvider
	*/
	virtual OP_STATUS GetData(const char*& data, INT32& num_bytes, BOOL& more)
		{ data = m_data; num_bytes = m_num_bytes; more = FALSE; return OpStatus::OK; }


	/**

	*/
	virtual INT32 CheckImageType(const UCHAR* data, INT32 len);


	/**

	*/
	OP_STATUS Grow() { return OpStatus::ERR; }


	/**

	*/
	INT32 GetDataLen() { return m_num_bytes; }


	/**

	*/
	void  ConsumeData(INT32 len) { m_loaded = TRUE; }


	/**

	*/
	void Reset() {}

	/**

	*/
	void Rewind() {}

	/**

	*/
	BOOL IsLoaded() { return TRUE; }


	/**

	*/
	INT32 ContentType() { return m_content_type; }


	/**

	*/
	void SetContentType(INT32 type) { m_content_type = type; }

	/**

	*/
	BOOL IsEqual(ImageContentProvider* content_provider) { return FALSE; } // FIXME:IMG-EMIL

private:

	char*  m_data;
	UINT32 m_num_bytes;
	INT32  m_content_type;
	BOOL   m_loaded;
};


/**
 * Simple wrapper class for loading an a file into an Image hiding away 
 * image management issues.
 */ 
class SimpleFileImage
{
public:
	SimpleFileImage(const uni_char* filename)
	{
		m_image_content_provider.LoadFromFile(filename, m_image);
	}

	SimpleFileImage(const UINT8* buffer, UINT32 num_bytes)
	{
		m_image_content_provider.LoadFromBuffer(buffer, num_bytes, m_image);
	}

	~SimpleFileImage()
	{
		m_image.DecVisible(null_image_listener);
	}

	Image GetImage() const { return m_image; }

private:
	Image m_image;
	// Must exists as long as the image. We get image duplication
	// problems otherwise (same icon shown twice or more when it should not)
	FileImageContentProvider m_image_content_provider;
};


/**
 * An image file reader.
 *
 * Use it when required to repeatedly load images from various files.
 */
class SimpleImageReader
{
public:
	SimpleImageReader() : m_provider(NULL) {}
	~SimpleImageReader() { OP_DELETE(m_provider); }

	OP_STATUS Read(const OpStringC& file_path);
	Image GetImage() const { return m_provider ? m_provider->GetImage() : Image(); }

private:
	SimpleFileImage* m_provider;
};


#endif
