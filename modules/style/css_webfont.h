/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_WEBFONT_H
#define CSS_WEBFONT_H

/** CSS web font interface. */

#include "modules/util/simset.h"

class CSS_WebFont : public Link
{
public:

	/** Supported formats. Can be combined in a bitset. */
	typedef enum {
		FORMAT_UNKNOWN = 0x0,
		FORMAT_TRUETYPE = 0x1,
		FORMAT_TRUETYPE_AAT = 0x2,
		FORMAT_OPENTYPE = 0x4,
		FORMAT_EMBEDDED_OPENTYPE = 0x8,
		FORMAT_WOFF = 0x10,
		FORMAT_SVG = 0x20,
		FORMAT_UNSUPPORTED = 0x40
	} Format;

	typedef enum {
		WEBFONT_NOTLOADED = 0,
		WEBFONT_LOADING,
		WEBFONT_LOADED,
		WEBFONT_NOTFOUND
	} LoadStatus;

	CSS_WebFont() : m_timestamp(0) {}

	virtual ~CSS_WebFont() { Out(); }

	/** Called when the font is removed.
		@param doc The document the font is removed from. */
	virtual void RemovedWebFont(FramesDocument* doc) = 0;

	/** Get the font-family descriptor specified in the
		@font-face rule. Currently only one name supported.
		We will return the first one. If the @font-face rule
		doesn't have a font-family descriptor, this method
		will return NULL. */
	virtual const uni_char* GetFamilyName() = 0;

	/** Return a url of the src descriptor of the @font-face
		rule which is used. If the LoadStatus is not WEBFONT_LOADED,
		the returned url will be the one that is currently being
		loaded or the empty url. If the @font-face rule doesn't
		have a src descriptor, or if we used a local(...) font,
		the empty url will also be returned (URL::IsEmpty() will
		return true). */
	virtual URL GetSrcURL() = 0;

	/** Return the format of the loaded font.

		@return FORMAT_UNKNOWN if the load status is different from WEBFONT_LOADED,
				otherwise the format of the loaded font. */
	virtual Format GetFormat() = 0;

	/** Return the font-style descriptor specified in the @font-face rule.
		If the @font-face rule doesn't have a font-style descriptor, this
		method will return CSS_VALUE_normal. */
	virtual short GetStyle() = 0;

	/** Return the font-weight descriptor specified in the @font-face rule.
		If the @font-face rule doesn't have a font-weight descriptor, this
		method will return -1. */
	virtual short GetWeight() = 0;

	/** Check the load status of this web font. */
	virtual LoadStatus GetLoadStatus() = 0;

	/** Set the load status for a font. This method should only
		be used by the svg module to set the load status of an svg font. */
	virtual OP_STATUS SetLoadStatus(FramesDocument* doc, LoadStatus status) = 0;

	/** Start loading the web font. This should be done when
		the web font is requested when loading css properties,
		and the font cannot be found in the StyleManager. If
		the font was already in the cache, the font will be
		added to the StyleManager, otherwise it will start
		loading the font, if it that hasn't already been tried.
		The method will try to load the src descriptors in the
		prioritized order. If one fails, it will start loading
		the next one.

		@param doc The document for which this webfont is loaded.
		@return OpStatus::ERR_NO_MEMORY on OOM, otherwise OpStatus::OK. */
	virtual OP_STATUS Load(FramesDocument* doc) = 0;

	/** Method to check if we are using a system font (from local()).

		@return TRUE if a local() font is used, otherwise FALSE. */
	virtual BOOL IsLocalFont() = 0;

	/** Checks if the fontface rule is equal to another rule (NOTE: not taking the src url into account) */
	BOOL IsDescriptorEqual(CSS_WebFont* other)
	{
		return other &&
			other->GetStyle() == GetStyle() &&
			other->GetFormat() == GetFormat() &&
			other->GetWeight() == GetWeight() &&
			other->GetFamilyName() &&
			GetFamilyName() &&
			uni_str_eq(GetFamilyName(), other->GetFamilyName());
	}

	/** Sets the timestamp for when the font is first requested to be loaded by layout.

		The timestamp should only be set once. It is used to determine when we should stop
		waiting for the webfont to load and apply the fallback font. I.e. when to do the
		transition from not painting the webfont text to paint it with the fallback font.

		@param t the time of the first request. */
	void SetTimeStamp(double t) { m_timestamp = t; }

	/** Gets the timestamp for when the font was first requested to be loaded by layout. */
	double GetTimeStamp() { return m_timestamp; }

private:

	/** Timestamp for when the font was first requested to be loaded by layout. */
	double m_timestamp;
};

#endif // CSS_WEBFONT_H
