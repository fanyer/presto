/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Haraldur Karlsson (haralkar)
 */

#ifndef __THUMBNAIL_GENERATOR_H__
#define __THUMBNAIL_GENERATOR_H__

#ifdef SUPPORT_GENERATE_THUMBNAILS

/** @brief Class to generate a thumbnail from a Window.
 */
class ThumbnailGenerator
{
public:
	/** Constructor
	  * @param window Window to generate the thumbnail from
	  */
	ThumbnailGenerator(Window* window);

	/** Set the size of the resulting thumbnail.
	 *  @param width Width of the thumbnail, THUMBNAIL_WIDTH by default
	 *  @param height Height of the thumbnail, THUMBNAIL_HEIGHT by default
	 */
	void SetThumbnailSize(long width, long height) {m_target_height = height; m_target_width = width;}

	/** Set whether to use bitmap scaling (high quality) or direct document scaling (low quality) if possible
	 *  @param high_quality Whether to use high quality - Default is high quality
	 */
	void SetHighQuality(BOOL high_quality) { m_high_quality = high_quality; }

	/** Generate a thumbnail
	 *  @return Thumbnail according to given specifications
	 */
	Image GenerateThumbnail();

	/** Generate a full sized snapshot
	*  @return Image in full size
	*/
	Image GenerateSnapshot();

#ifdef THUMBNAILS_LOGO_FINDER
	/** Find the top left corner of a logo on this page, if any.
	 *  This function should only be used after calling GenerateThumbnail().
	 *  @return Coordinates of the logo relative to the generated thumbnail (if found)
	 */
	OpPoint FindLogo();

private:
	virtual OpRect FindLogoInOriginalWindow();
#endif

protected:
	void SetArea(const OpRect &r) { m_area = r; }

private:
	Window *m_window;
	long m_target_height;
	long m_target_width;
	OpRect m_area;
	BOOL m_high_quality;
};

#endif // SUPPORT_GENERATE_THUMBNAILS
#endif // __THUMBNAIL_GENERATOR_H__
