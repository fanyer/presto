/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#ifndef INDEXIMAGE_H
#define INDEXIMAGE_H

#ifdef M2_SUPPORT

class SimpleFileImage;

class IndexImage
{
public:
	IndexImage() : m_image(NULL) {}
	~IndexImage();
	
	/** Get the image as Base64 in an OpString, so it's easily saved to index.ini
	*/
	OP_STATUS	GetImageAsBase64(OpString& image) const;

	/** Set the image member variable based on an image filename and automatically resize to 16*16px
	*/
	OP_STATUS	SetImageFromFileName(const OpStringC& filename);

	/** Set the image from base64, usually done when reading from index.ini
	*/
	OP_STATUS	SetImageFromBase64(const OpStringC8& buffer);
	
	/** Get the Image for drawing
	*/
	Image		GetBitmapImage() const;

private:

	SimpleFileImage* m_image;

};

#endif //M2_SUPPORT

#endif // INDEXIMAGE_H
