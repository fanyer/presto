/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef IMAGECOLORMANAGER_H
#define IMAGECOLORMANAGER_H

/**
 * Opaque object representing an ICC color profile.
 *
 * Can be either an input profile (as acquired from image decoders) or
 * an output profile (generally the screen profile - sRGB in some
 * form). Could theoretically also be other types of profiles as
 * defined by the ICC specification, but there is currently no need
 * for those.
 */
class ICCProfile
{
public:
	virtual ~ICCProfile() {}
};

/**
 * A transform from one (ICC) profile to another (ICC) profile.
 *
 * This is the workhorse for colorspace transformations. In general
 * terms, it forms the connection between an input profile and an
 * output profile.
 */
class ImageColorTransform
{
public:
	virtual ~ImageColorTransform() {}

	/**
	 * Apply the colorspace transformation to run of pixels.
	 *
	 * Applies the transform from the input colorspace to the output
	 * colorspace. Hence, the source/input data is assumed to be in
	 * the input colorspace, while the destination/output data is
	 * assumed to be in the output colorspace.
	 *
	 * The pixel format used is BGRA8888 (same as for
	 * OpBitmap::AddLine and friends).
	 *
	 * In-place transformation should be supported (i.e, when dst ==
	 * src).
	 *
	 * @param[out] dst Buffer where the transformed pixels are written.
	 * @param[in] src Buffer of pixels to transform.
	 * @param[in] length The number of pixels to transform. Both the
	 *                   source and destination buffers must be able to
	 *                   store at least this amount of pixels.
	 */
	virtual void Apply(UINT32* dst, const UINT32* src, unsigned length) = 0;

	/**
	 * Apply the colorspace transformation to run of pixels.
	 *
	 * Applies the color transform to temporary buffer and return the
	 * result. The result is owned by the callee, and should be copied immediately
	 * 
	 * @param[in] src Buffer of pixels to transform.
	 * @param[in] length The number of pixels to transform. Both the
	 *                   source and destination buffers must be able to
	 *                   store at least this amount of pixels.
	 * @return dst Buffer where the transformed pixels are written. NULL if fail.
	 */
	virtual UINT32* Apply(const UINT32* src, unsigned length) = 0;
};

/**
 * Manager for decoding of ICC profiles and creation of color transforms.
 */
class ImageColorManager
{
public:
	static ImageColorManager* Create();

	virtual ~ImageColorManager() {}

	/**
	 * Create an ICC profile from a blob of data.
	 *
	 * Create a color profile object representing the ICC color
	 * profile that is stored in the block of length datalen pointed
	 * to by data.
	 *
	 * @param[out] profile The color profile object.
	 * @param[in] data The ICC profile data.
	 * @param[in] datalen The length of the datablock in bytes.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR if the profile
	 *         data was invalid, and OpStatus::ERR_NO_MEMORY if OOM
	 *         was encountered.
	 */
	virtual OP_STATUS CreateProfile(ICCProfile** profile, const UINT8* data, unsigned datalen) = 0;

	/**
	 * Create a transform from one color profile to another.
	 *
	 * Create a color transform object that allows pixels to be
	 * transformed from the input/source colorspace to the
	 * output/destination colorspace.
	 *
	 * @see ImageColorTransform for usage.
	 *
	 * Profiles may be disposed of/deleted after a color transform was
	 * successfully created.
	 *
	 * @param[out] transform The color transform.
	 * @param[in] src The input/source profile.
	 * @param[in] dst The output/destination profile, NULL means use the device profile.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR if some error
	 *         was encountered while trying to create the color
	 *         transform ("connect the
	 *         profiles"). OpStatus::ERR_NO_MEMORY on OOM.
	 */
	virtual OP_STATUS CreateTransform(ImageColorTransform** transform, ICCProfile* src, ICCProfile* dst = NULL) = 0;
};

#endif // IMAGECOLORMANAGER_H
