/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPSCREENINFO_H
#define OPSCREENINFO_H

class OpRect;
class OpPoint;
class OpWindow;

#ifdef OPSCREENINFO_ORIENTATION
	/** Screen orientation.
	 *
	 * Orientation is measured in degrees clockwise from the default, upright,
	 * orientation.
	 */
	enum OpScreenOrientation {
		SCREEN_ORIENTATION_0,		//< Upright: the default orientation.
		SCREEN_ORIENTATION_90,		//< Top of device is to the right of the screen.
		SCREEN_ORIENTATION_180,	//< Upside-down.
		SCREEN_ORIENTATION_270		//< Top of device is to the left of the screen.
	};
#endif // OPSCREENINFO_ORIENTATION

/** Screen properties.
 *
 * Filled in by OpScreenInfo::GetProperties().
 *
 * These properties are optionally for a given OpWindow, and then optionally at
 * a given coordinate in that OpWindow.
 */
class OpScreenProperties
{
public:
    /** The full screen rectangle.
     *
     * The x and y values will typically be non-zero when there are several
     * screens participating in a common virtual coordinate system.
     */
    OpRect screen_rect;

    /** The workspace rectangle on the screen.
     *
     * This is the same as screen_rect, with elements taken up by the window
     * manager / operating system (task bar, and so on) subtracted. This is
     * typically the rectangle of a maximized window.
     */
    OpRect workspace_rect;

    /** Screen width, in pixels. */
    UINT32 width;

    /** Screen height, in pixels. */
    UINT32 height;

    /** Number of pixels per inch horizontally. */
    UINT32 horizontal_dpi;

    /** Number of pixels per inch vertically. */
    UINT32 vertical_dpi;

    /** Number of bits per pixel. */
    UINT8 bits_per_pixel;

    /** Number of bitplanes. */
    UINT8 number_of_bitplanes;

#ifdef OPSCREENINFO_ORIENTATION
	/** Orientation of the screen.
	 */
	OpScreenOrientation orientation;
#endif // OPSCREENINFO_ORIENTATION

};

/** @short Interface definition for Opera abstract screen info class
 *
 * A utility class to get information from the platform about the screen
 * properties.  In the future, we should make provisions for giving out
 * information about systems with several workspaces and/or physical screens.
 */
class OpScreenInfo
{
public:
	/** Create an OpScreenInfo object.
	 *
	 * @param new_opscreeninfo (out) Set to the OpScreeninfo object created. Must
	 * be ignored by the caller if OpStatus::ERR_NO_MEMORY is returned.
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM. No other errors may occur.
	 */
	static OP_STATUS Create(OpScreenInfo** new_opscreeninfo);

	virtual ~OpScreenInfo() {}

    /** Get screen-related properties.
     *
     * An OpWindow belongs to one or more physical screens. This method
     * fills in the supplied OpScreenProperties object with information
     * about the screen where the supplied window lives, optionally at
     * the specified coordinates ('point').
     *
     * @param[out] properties The properties object to fill in. All values must
     * be set by the implementation, unless an error value is returned.
     * @param[in] window The window to query. Must be non-NULL.
     * @param[in] point If non-NULL, the point to ask about (in window's
     * coordinate system). This may be useful if the window spans several
     * screens.
     *
     * The "main screen" should not be used, however currently most
     * implementations have some sort of "main screen" where properties
     * are collected from, and the "main screen" will be that screen.
     * It is also possible for platform implementations to ignore the window
     * property if multiple screens dont make any sence.
     *
     * @return OK on success, ERR for other errors. The
     * contents of the properties out parameter is undefined unless OK is
     * returned.
     */
    virtual OP_STATUS GetProperties(OpScreenProperties* properties, OpWindow* window, const OpPoint* point = NULL) = 0;

	/** Get only the DPI properties
	 *
	 * DPI is used often when the other screen properties isnt needed.
	 * DPI is also often static and global, so in many many cases where
	 * you dont need the other stuff, AND DPI is static, this will be
	 * a superfast way to access this information.
	 *
	 * Platform implementation should care about
	 * TWEAK_PI_STATIC_GLOBAL_SCREEN_DPI and TWEAK_PI_STATIC_GLOBAL_SCREEN_DPI_VALUE
	 *
	 * @return OK on success.
	 */
	virtual OP_STATUS GetDPI(UINT32* horizontal, UINT32* vertical, OpWindow* window, const OpPoint* point = NULL) = 0;

	/** Register main-screen properties
	 *
	 * The "main screen" should not be used, however currently most
	 * implementations have some sort of "main screen" where properties
	 * are collected from, and the "main screen" will be that screen.
	 * It is also possible for platform implementations to ignore the window
	 * property if multiple screens dont make any sence.
	 * In most cases it is a good idea to assert that we have a window
	 *
	 * To simplify the main screen fallback, RegisterMainScreenProperties
	 * may be used to register the mainscreen information. However the
	 * registration of this information AND the use of this information
	 * will be hevily asserted, and platforms realy need to decide that
	 * they will use the "main screen" properties.
	 *
	 * Also, some selftests dont have the full screen context, so
	 * in these cases, the fallback will also be helpfull.
	 *
	 * NOTE: platforms are alowed to use other kind of fallbacks,
	 * for example is it better to return the real/fresh screen properties,
	 * then the ones "registered".
	 *
	 * @param properties (the properties to register)
	 *
	 * @return OK on success
	 */
	virtual OP_STATUS RegisterMainScreenProperties(OpScreenProperties& properties) = 0;

	/** Return the amount of memory a bitmap with the specified characteristics would need.
	 *
	 * How a bitmap is stored in memory is up to the platform. An image in core
	 * is typically always represented as 8 or 32 bits per pixel, but the
	 * platform may convert it to another depth internally, and it will also
	 * have its own ways of representing transparency and alpha.
	 *
	 * @param width Width of the bitmap in pixels
	 * @param height Height of the bitmap in pixels
	 * @param transparent Is the bitmap transparent? If the 'alpha' parameter
	 * is TRUE, the value of this parameter is ignored
	 * @param alpha Does the bitmap have an alpha channel (translucent pixels)?
	 * @param indexed Number of bits per pixel if in indexed mode. Maximum is
	 * 8. In indexed mode the 'alpha' parameter is ignored. If it is 0, the
	 * bitmap is not indexed (i.e. TrueColor mode).
	 *
	 * @return The number of bytes of memory required by the platform to store
	 * the bitmap
	 */
	virtual UINT32 GetBitmapAllocationSize(UINT32 width, UINT32 height, BOOL transparent, BOOL alpha, INT32 indexed) = 0;
};

#endif // OPSCREENINFO_H
