/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** OpBitmap.h -- platform-specific implementation of bitmaps.
*/

#ifndef OPBITMAP_H
#define OPBITMAP_H

class OpPainter;
class OpRect;

#define OPBITMAP_POINTER_ACCESS

/** @short An image class.
 *
 * An OpBitmap object holds an image's pixels, which can be manipulated in
 * different ways. An OpPainter can draw an OpBitmap on a paint device, using
 * the OpPainter::DrawBitmap...() methods.
 *
 * An OpBitmap may also serve as an off-screen paint device for a defined
 * period, by using GetPainter() and ReleasePainter().
 *
 * Initial pixel data or contents of an OpBitmap is undefined. Before painting
 * it, the API user must be sure to initialize the pixels that are to be
 * painted, or output will be unpredictable.
 *
 * Normally the OpBitmap interface uses 32bpp ARGB pixel word values. However,
 * the platform implementation may use a completely different format
 * internally. API users need to take this into account; certain method calls
 * may involve more work than just doing a memcpy() (which one might assume for
 * AddLine() or GetLineData()) or returning a pointer (GetPointer()).
 *
 * FIXME: This API is somewhat quirky and confusing in general. Should be
 * possible to clean it up in a future core version. Can we get rid of support
 * for indexed bitmaps?
 */
class OpBitmap
{
public:
	/** Used in GetPointer() to specify intended type of access. */
	enum AccessType
	{
		/** Data will only be read. */
		ACCESS_READONLY,

		/** Data will only be written. */
		ACCESS_WRITEONLY,

		/** Data will be both read and written. */
		ACCESS_READWRITE
	};

	/** Create and return an OpBitmap object.
	 *
	 * @param bitmap (out) Set to the new OpBitmap created, or NULL if creation
	 * failed.
	 * @param width Image width in pixels
	 * @param height Image height in pixels
	 * @param transparent If TRUE, the image may contain fully opaque and fully
	 * transparent pixels. If the 'alpha' parameter is TRUE, the value of this
	 * parameter is ignored.
	 * @param alpha If TRUE, the image may contain translucent pixels,
	 * i.e. fully opaque, fully transparent, and anything inbetween, with a
	 * resolution of 8 bits. API users should avoid using alpha carelessly, as
	 * this is expensive on some platforms.
	 * @param transpcolor Index used to represent transparent pixels. Only paid
	 * attention to if the 'indexed' parameter is non-zero.
	 * @param indexed If 0, the API will consistently use 32bpp ARGB word
	 * values for each pixel. If non-zero, some extra methods
	 * (AddIndexedLine(), SetPalette()) for palette index usage will be made
	 * available. The number will then specify the number of bits per
	 * pixel. Maximum is 8. In indexed mode are 'alpha' and 'transparent'
	 * parameters ignored; there will be no alpha, but there will be
	 * transparency - as specified by the 'transpcolor' parameter.
	 * @param must_support_painter If TRUE, the bitmap created must support
	 * GetPainter() and ReleasePainter(), and return TRUE from
	 * Supports(SUPPORTS_PAINTER). If this requirement cannot be satisfied by
	 * the implementation, bitmap creation will fail.
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR if any of the
	 * parameters were unacceptable for the implementation.
	 */
	static OP_STATUS Create(OpBitmap **bitmap, UINT32 width, UINT32 height, BOOL transparent = FALSE, BOOL alpha = TRUE, UINT32 transpcolor = 0, INT32 indexed = 0, BOOL must_support_painter = FALSE);

	virtual ~OpBitmap() {}

	/** Optional features enum. */
	enum SUPPORTS {
		/** Can access and modify the bits directly */
		SUPPORTS_POINTER,

		/** Can be painted on by the OpPainter */
		SUPPORTS_PAINTER,

		/** Can use the indexed mode (palette) */
		SUPPORTS_INDEXED,

		/** Can create a tile optimized bitmap */
		SUPPORTS_CREATETILE,

#ifndef DOXYGEN_DOCUMENTATION
		/** No longer used. Implementations shall NOT support this. */
		SUPPORTS_EFFECT,
#endif // !DOXYGEN_DOCUMENTATION

		/** Supports SetOpacity */
		SUPPORTS_OPACITY
	};

	/** Query the implementation about optional feature support.
	 *
	 * @param supports The feature to check for
	 * @return TRUE if the feature is supported, FALSE otherwise
	 */
	virtual BOOL Supports(SUPPORTS supports) const = 0;

	/** Create a tiled version of this OpBitmap.
	 *
	 * FIXME: ARCH - should be abstract, dangerous with non-abstract functions in an implementation interface. (kilsmo)
	 *
	 * This will create a new OpBitmap with the given width and height and blit
	 * itself into it as many times as needed. At least one of the height or
	 * width parameters ought to be larger than the size of this OpBitmap, or
	 * the operation is pointless.
	 *
	 * This method may NOT be called if Supports(SUPPORTS_CREATETILE) returns
	 * FALSE.
	 *
	 * @param width Width of the tiled OpBitmap
	 * @param height Height of the tiled OpBitmap
	 * @return A tiled OpBitmap, or NULL if memory allocation failed.
	 */
	virtual OpBitmap* CreateTile(UINT32 width, UINT32 height) { (void)width; (void)height; return NULL; };

	/** Get a pointer to the pixel data buffer, lock the OpBitmap.
	 *
	 * This will lock the OpBitmap. ReleasePointer() must be called before
	 * accessing the OpBitmap in any way (painting it, calling AddLine(),
	 * GetLine(), GetPainter(), GetPointer(), etc.).
	 *
	 * Note that this may be an expensive operation. While some platform
	 * implementations will simply return a pointer to a buffer that already
	 * exists, others may internally use different objects for an image
	 * optimized for pixel manipulation and for an image optimized for
	 * painting, so that this call (and ReleasePointer()) triggers some sort of
	 * allocation and conversion. Using the access_type parameter correctly is
	 * cruicial for performance in such implementations. Since the API user
	 * cannot make assumptions about how this is implemented, use of
	 * GetPointer() and ReleasePointer() is discouraged if GetLineData() and/or
	 * AddLine() are feasible alternatives.
	 *
	 * This method may NOT be called if Supports(SUPPORTS_POINTER) returns
	 * FALSE.
	 *
	 * @param access_type Minimum access type requirement. Support for pointers
	 * to pixel data may be expensive in some implementations, but it may be
	 * amended somewhat if the implementation knows that the data buffer is
	 * only to be read from or written to. If this value is ACCESS_READONLY,
	 * the buffer is guaranteed to contain the OpBitmap's pixel data when this
	 * method returns, while modifications made to it are not guaranteed to be
	 * reflected in the OpBitmap by ReleasePointer(). If it is
	 * ACCESS_WRITEONLY, buffer contents are undefined when this method
	 * returns, but its contents are guaranteed to be reflected in the OpBitmap
	 * by ReleasePointer() (provided that the 'changed' parameter is TRUE). If
	 * it is ACCESS_READWRITE, the buffer is guaranteed to contain the
	 * OpBitmap's pixel data when this method returns, and its potentially
	 * changed contents are guaranteed to be reflected in the OpBitmap by
	 * ReleasePointer() (provided that the 'changed' parameter is TRUE). Note
	 * that this parameter just specifies minimum access requirements, meaning
	 * that it is allowed for an implementation to ignore this argument and
	 * always provide read+write access.
	 * @return A pointer to the pixel data for the entire bitmap, in contiguous
	 * memory, 32 bits per pixel, in ARGB format, one byte per component
	 * (component order, starting with the highest byte in the word: alpha,
	 * red, green, blue - e.g. 0xFF112233 means alpha=0xFF, red=0x11,
	 * green=0x22, blue=0x33). Will return NULL if memory allocation fails.
	 */
	virtual void* GetPointer(AccessType access_type = ACCESS_READWRITE) = 0;

	/** Release the pointer to the pixel data buffer, unlock the OpBitmap.
	 *
	 * Follows a call to GetPointer(), and will instruct the implementation to
	 * unlock the OpBitmap.
	 *
	 * This method may NOT be called if Supports(SUPPORTS_POINTER) returns
	 * FALSE.
	 *
	 * @param changed TRUE if the data was changed, meaning that the changes
	 * made to the data buffer once provided by GetPointer() is guaranteed to
	 * be reflected in the OpBitmap (provided that GetPointer() was not called
	 * with access_type=ACCESS_READONLY) now. If FALSE, pixel data may be
	 * ignored by the implementation. Note that this parameter just specifies
	 * minimum change requirements, meaning that an implementation is allowed
	 * to always reflect the changed buffer in OpBitmap, even if this parameter
	 * is FALSE.
	 */
	virtual void ReleasePointer(BOOL changed = TRUE) = 0;

	/** Create and return a painter that will paint on this bitmap, lock the OpBitmap.
	 *
	 * This will lock the OpBitmap and make it behave like a paint device. The
	 * OpPainter returned will have exclusive rights to the OpBitmap while it
	 * is locked. ReleasePainter() must be called before accessing the OpBitmap
	 * in any way (painting it, calling AddLine(), GetLine(), GetPainter(),
	 * GetPointer(), etc.).
	 *
	 * FIXME: ARCH - should be abstract, dangerous with non-abstract functions in an implementation interface. (kilsmo)
	 *
	 * This method may NOT be called if Supports(SUPPORTS_PAINTER) returns
	 * FALSE.
	 *
	 * @return An OpPainter that will be paint on this OpBitmap. NULL is returned on memory allocation failure.
	 */
	virtual OpPainter* GetPainter() { return NULL; };

	/** Release the painter, unlock the OpBitmap.
	 *
	 * Follows a call to GetPainter(), and will instruct the implementation to
	 * unlock the OpBitmap. Do not delete the painter afterwards; the
	 * implementation will take care of it.
	 *
	 * FIXME: ARCH - should be abstract, dangerous with non-abstract functions in an implementation interface. (kilsmo)
	 *
	 * This method may NOT be called if Supports(SUPPORTS_PAINTER) returns
	 * FALSE.
	 */
	virtual void ReleasePainter() { };

	/** Get the number of bytes per line in the image.
	 *
	 * *OPTIONAL!* Can ONLY be used if Support(SUPPORTS_INDEXED) => TRUE
	 * FIXME: DOC - I do not think that this constrain is true. Could it be SUPPORTS_POINTER
	 * that is the correct documentation? (kilsmo)
	 */
	virtual UINT32 GetBytesPerLine() const = 0;

	/** Set pixel data of one line, based on a TrueColor input buffer.
	 *
	 * Sets the pixel data of one full scanline in the OpBitmap.
	 *
	 * @param data New TrueColor pixel data to set, in contiguous memory, with
	 * one UINT32 per pixel, in ARGB format, one byte per component (component
	 * order, starting with the highest byte in the word: alpha, red, green,
	 * blue - e.g. 0xFF112233 means alpha=0xFF, red=0x11, green=0x22,
	 * blue=0x33).
	 * @param line The number of the line to modify. 0 is the topmost line.
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	virtual OP_STATUS AddLine(void* data, INT32 line) = 0;

#ifdef SUPPORT_INDEXED_OPBITMAP
	/** Set pixel data of one line, based on palette index input buffer.
	 *
	 * Sets the pixel data of one full scanline in the OpBitmap.
	 *
	 * FIXME: *OPTIONAL!* Can ONLY be used if Support(SUPPORTS_INDEXED) => TRUE and
	 *             bitmap was created with indexed=TRUE.
	 *
	 * @param data New palette index pixel data to set, in contiguous memory,
	 * with one UINT8 per pixel, where the value is a palette index. If the
	 * value is outside the defined palette (done with SetPalette()), it is to
	 * be treated as a transparent pixel.
	 * @param line The number of the line to modify
	 * @return OK, ERR_NO_MEMORY or ERR.
	 *
	 */
	virtual OP_STATUS AddIndexedLine(void* data,  INT32 line) = 0;

	/** Set the palette used for indexed bitmaps.
	 *
	 * AddIndexedLine() will find the TrueColor color values by looking up the
	 * specified pixel values in the palette array.
	 *
	 * Initially the palette is empty (0 entries).
	 *
	 * FIXME: ARCH - should be abstract, dangerous with non-abstract functions in an implementation interface. (kilsmo)
	 *
	 * @param pal New TrueColor values to to set, in contiguous memory, with
	 * three UINT8 bytes per color (palette entry), in RGB order (so that the
	 * three first bytes in the buffer define the first palette entry (first
	 * byte for red component, second for green component, third for blue
	 * component), the three next bytes define the second palette entry, and so
	 * on).
	 * @param num_colors Number of colors defined in the 'pal' parameter.
	 * Maximum value is 256. If this value is lower than the number of entries
	 * already in the palette, only the specified amount of colors will be set;
	 * the remaining palette remains unchanged.
	 */
	virtual BOOL SetPalette(UINT8* pal,  UINT32 num_colors) { (void)pal; (void)num_colors; return FALSE; };

	/** @return the color index that is transparent. The return value is only valid if IsIndexed returns a value > 0 .
	    FIXME: DOC - I see no IsIndexed anywhere.
	    FIXME: DOC - If a bitmap is indexed or not depends on if Create() got an index parameter.
		             This is not very consistent with HasAlpha(), IsTransparent(), since alpha and transparent
					 is also given to Create().
	  */
	virtual UINT32 GetTransparentColorIndex() const = 0;

	/** Get pixel data of one line for palette based images.
	 *
	 * Only done on images created as indexed and if Support(SUPPORTS_INDEXED)
	 * returns TRUE.
	 *
	 * @param data (out) Buffer to write the data to. Format is the same as in
	 * AddIndexedLine() (one UINT8 per pixel to represent the palette index).
	 * @param line The number of the line for which to get the pixel data. 0 is
	 * the topmost line.
	 * @return TRUE if everything went well. Then 'data' contains the pixels of
	 * the requested line. If FALSE is returned, something failed, and the
	 * contents of 'data' is undefined.
	 */
	virtual BOOL GetIndexedLineData(void* data, UINT32 line) const = 0;

	/** Get palette data that has previously been set with SetPalette().
	 *
	 * @param pal (out) The buffer to which the palette should be written. This
	 * buffer is guaranteed to be at least 768 bytes (256*3). The format is
	 * same as in SetPalette() (three UINT8 per color in RGB order).
	 * @return TRUE if everything went well. If FALSE is returned, something
	 * failed, and the contents of 'pal' is undefined.
	 */
	virtual BOOL GetPalette(UINT8* pal) const = 0;
#endif // SUPPORT_INDEXED_OPBITMAP

#if 1 // FIXME: Weird method (although it might be useful if cleaned up) - can we get rid of it?
	/** Sets the entire bitmap to a color.

	    @param color             color to use, RGBA format
		@param all_transparent   if TRUE and color is not NULL, set transparent or
		                         alpha bitmap to all transparent
	    @param rect              if not NULL, work on subset of bitmap indicated by rect
		@return                  TRUE normally, FALSE if rect not NULL but subrectangle
								 operation was not supported

        FIXME: DOC - created the above from the following, please check.  In particular,
		is the 'color' referenced by all_transparent the same as the 'color' that is
		a parameter?
	    FIXME: ARCH - should be abstract, dangerous with non-abstract functions in an implementation interface. (kilsmo)
	    FIXME: ARCH - not used by the core code or the ui, and the function has a strange interface anyway.
		              I think this function should be removed. (kilsmo)

		If bitmap is 24 bpp, the color will be RGB, if bitmap is indexed the color
		will be one char (the index). all_transparent means set a transparent or alpha bitmap
		all transparent, if color is NULL and all_transparent FALSE this is a NOP,
		rect if not a null pointer means a subset of the bitmap, if not supported
		return FALSE and another strategy might be used by the caller */
	virtual BOOL SetColor(UINT8* color, BOOL all_transparent, const OpRect* rect) { return FALSE; }
#endif

	/** Get pixel data of one line, set a TrueColor output buffer.
	 *
	 * @param data (out) Set to the TrueColor pixel data of the specified line,
	 * in contiguous memory, with one UINT32 per pixel, in ARGB format, one
	 * byte per component (component order, starting with the highest byte in
	 * the word: alpha, red, green, blue - e.g. 0xFF112233 means alpha=0xFF,
	 * red=0x11, green=0x22, blue=0x33).
	 * @param line The number of the line for which to get the pixel data. 0 is
	 * the topmost line.
	 * @return TRUE if everything went well. Then 'data' contains the pixels of
	 * the requested line. If FALSE is returned, something failed, and the
	 * contents of 'data' is undefined.
	 */
	virtual BOOL GetLineData(void* data, UINT32 line) const = 0;

	/** @return The width of the image, in pixels */
	virtual UINT32 Width() const = 0;

	/** @return The height of the image, in pixels */
	virtual UINT32 Height() const = 0;

	/** Get number of bits per pixel.
	 *
	 * FIXME: ARCH - should be abstract, dangerous with non-abstract functions in an implementation interface. (kilsmo)
	 *
	 * Will be 32, unless this is an indexed bitmap.
	 *
	 * @return a constant to be used for pixel encoding and decoding.
	 */
	virtual UINT8 GetBpp() const { return 32; }

	/** @return TRUE if the image has alpha. */
	virtual BOOL HasAlpha() const = 0;

	/** @return TRUE if the image is transparent. If a pixel's alpha is not 0,
	 * the pixel should be fully visible. */
	virtual BOOL IsTransparent() const = 0;

	/** Enable alpha.
	 *
	 * Will make sure that the bitmap gets an alpha channel (which may not be
	 * the case if it was Create()d with alpha==false).
	 *
	 * FIXME: ARCH - should be abstract
	 */
	virtual	void ForceAlpha() {}

	/** Set the opacity of the entire bitmap.
	 *
	 * All pixels in the bitmap will be set to the specified opacity.
	 *
	 * FIXME: ARCH - should be abstract
	 *
	 * @param opacity Opacity, can be 0-254. 255 (fully opaque) will be handled by core.
	 * @return TRUE if supported and successful.
	 */
	virtual BOOL SetOpacity(UINT8 opacity) { return FALSE; }

#ifndef DOXYGEN_DOCUMENTATION
	DEPRECATED(OP_STATUS InitStatus());
	DEPRECATED(OpBitmap* GetTile(UINT32, UINT32));
	DEPRECATED(void ReleaseTile(OpBitmap*));
	DEPRECATED(OpBitmap* GetEffectBitmap(INT32, INT32));

	DEPRECATED(static OP_STATUS CreateFromIcon(OpBitmap **bitmap, const uni_char* icon_path));
#endif // !DOXYGEN_DOCUMENTATION
};

#ifndef DOXYGEN_DOCUMENTATION
inline OP_STATUS OpBitmap::InitStatus()
{
	return OpStatus::OK;
}

inline OpBitmap* OpBitmap::GetTile(UINT32, UINT32)
{
	return this;
}

inline void OpBitmap::ReleaseTile(OpBitmap*)
{
}

inline OpBitmap* OpBitmap::GetEffectBitmap(INT32, INT32)
{
	return this;
}
#endif // !DOXYGEN_DOCUMENTATION

#endif // OPBITMAP_H
