/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifdef IMGLOADER_NO_PREMULTIPLY
#ifndef IMGLOADER_NO_PREMULTIPLY_H
#define IMGLOADER_NO_PREMULTIPLY_H

#include "modules/img/src/imagemanagerimp.h"

// Forward declarations.
class VEGASWBuffer;

/**
 * This class takes an ImageContentProvider and decodes the image data into a VEGASWBuffer without
 * doing any alpha premultiplication and optionally also ignoring the color space conversions in the
 * image decoders.
 */
class ImageLoaderNoPremultiply : public ImageDecoderListener
{
public:
	/** Load (decode) a complete image. Will not yeild until the entire first
	 * frame of the image is decoded and will return an error if not enough of
	 * the images data has been loaded to decode the first frame. */
	static OP_STATUS Load(ImageContentProvider* content_provider, VEGASWBuffer **swBuffer, BOOL ignoreColorSpaceConversion);

private:
	// Private constructor and destructor as we only want this to be used via Load().
	ImageLoaderNoPremultiply();
	~ImageLoaderNoPremultiply();

	OP_STATUS DoLoad();

	// Overloaded methods from ImageDecoderListener.
	virtual void OnLineDecoded(void* data, INT32 line, INT32 lineHeight);

	virtual BOOL OnInitMainFrame(INT32 width, INT32 height);

	virtual void OnNewFrame(const ImageFrameData& image_frame_data);

	virtual void OnAnimationInfo(INT32 nrOfRepeats);

	virtual void OnDecodingFinished();

#ifdef IMAGE_METADATA_SUPPORT
	virtual void OnMetaData(ImageMetaData id, const char* data);
#endif // IMAGE_METADATA_SUPPORT

#ifdef EMBEDDED_ICC_SUPPORT
	void OnICCProfileData(const UINT8* data, unsigned datalen);
#endif // EMBEDDED_ICC_SUPPORT

	virtual BOOL IgnoreColorSpaceConversions() const;


	ImageContentProvider    *m_contentProvider;
	ImageDecoder            *m_imageDecoder;
	VEGASWBuffer            **m_swBuffer;
	UINT8                   *m_palette;
	unsigned int            m_scanlinesAdded;
	BOOL                    m_ignoreColorSpaceConversion;
	BOOL					m_firstFrameLoaded;
	unsigned int			m_framesAdded;
#ifdef EMBEDDED_ICC_SUPPORT
	ImageColorTransform		*m_color_transform;
#endif // EMBEDDED_ICC_SUPPORT
};

#endif //IMGLOADER_NO_PREMULTIPLY_H
#endif //IMGLOADER_NO_PREMULTIPLY
