/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FONTATT_H
#define FONTATT_H

/**
   The Font Attribute class. Opera uses this class to ask the OS
   for a matching font.

   It is much used as a font template, to change various attributes of
   a font and then instantiate it from there.
*/

class FontAtt
{
public:
	static const UINT32 FA_FACESIZE;

	FontAtt()
		: m_overline(FALSE), m_changed(TRUE), m_weight(4), m_underline(FALSE),
		  m_fontHeight(10), m_strikeOut(FALSE), m_smallCaps(FALSE), m_italic(FALSE),
		  m_hasgetoutline(FALSE), m_blurRadius(0), m_fontnumber(0)
	{
	}

	~FontAtt()
	{
	}

	void Clear() 
	{ 
		m_overline = FALSE;
		m_weight = 4; 
		m_underline = FALSE; 
		m_fontHeight = 10; 
		m_strikeOut = FALSE; 
		m_smallCaps = FALSE;
		m_italic = FALSE;
		m_hasgetoutline = FALSE;
		m_blurRadius = 0;
		m_fontnumber = 0;
		m_changed = TRUE;
	}

	/** Gets the changed status */
	BOOL GetChanged() const
	{
		return m_changed;
	}

	/** Sets the changed status to be TRUE */
	void SetChanged()
	{
		m_changed = TRUE;
	}

	/** Sets the changed status to FALSE. */
	void ClearChanged()
	{
	    m_changed = FALSE;
	}

	/** Sets the font's weight. */
	void SetWeight(INT32 weight)
	{
		if (m_weight != weight)
		{
			m_weight = weight;
			m_changed = TRUE;
		}
	}

	/** Sets the font to be underlined or not. */
	void SetUnderline(BOOL underline)
	{
		// we don't set changed to TRUE here, this is because underlined is
		// drawn by ourselves.
		m_underline = underline;		
	}
	
	/** Sets the font to be overlined or not. */
	void SetOverline(BOOL overline)
	{
		// we don't set changed to TRUE here, this is because overlined is
		// not part of the font itself, but drawn by ourselves.
		m_overline = overline;
	}

	/** Sets the font's height. */
	void SetHeight(INT32 height)
	{
		OP_ASSERT(height >= 0);
		if (m_fontHeight != height)
		{
			m_fontHeight = height;
			m_changed = TRUE;
		}
	}

	/** Sets the font to be striked out or not. */

	void SetStrikeOut(BOOL strikeOut)
	{
		// we don't set changed to TRUE here, this is because strikeout is
		// drawn by ourselves.
		m_strikeOut = strikeOut;		
	}

	/** Sets the font's small-caps attribute. */
	void SetSmallCaps(BOOL smallCaps)
	{
		if (m_smallCaps != smallCaps)
		{
			m_smallCaps = smallCaps;
			m_changed = TRUE;
		}
	}

	/** Sets the font's italic attribute. */
	void SetItalic(BOOL italic)
	{
		if (m_italic != italic)
		{
			m_italic = italic;
			m_changed = TRUE;
		}
	}

	/** Sets the font's **/
	void SetHasGetOutline(BOOL hasGetOutline)
	{
		if(m_hasgetoutline != hasGetOutline)
		{
			m_hasgetoutline = hasGetOutline;
			m_changed = TRUE;
		}
	}

	/** Sets the blur radius to be applied to
	 * each glyph when rendering text. */
	void SetBlurRadius(INT32 br)
	{
		if (br != m_blurRadius)
		{
			m_blurRadius = br;
			m_changed = TRUE;
		}
	}

	/** Sets the fontnumber of the font. */
	void SetFontNumber(UINT32 fontnumber)
	{
		if ((short)fontnumber < 0) // invalid font number
			return;
		if (m_fontnumber != fontnumber)
		{
			m_fontnumber = fontnumber;
			m_changed = TRUE;
		}
	}

	/** Sets the face name of the font. */
	void SetFaceName(const uni_char* faceName);	

	/** @return the font weight */
	INT32 GetWeight() const
	{
		return m_weight;
	}

	/** @return the underline status */
	BOOL GetUnderline() const
	{
		return m_underline;
	}

	/** @return the overline status */
	BOOL GetOverline() const
	{
		return m_overline;
	}

	/** @return the font height */
	INT32 GetHeight() const
	{
		return m_fontHeight;
	}

	/** @return the strikeout status */
	BOOL GetStrikeOut() const
	{
		return m_strikeOut;
	}

	/** @return the small-caps status */
	BOOL GetSmallCaps() const
	{
		return m_smallCaps;
	}

	/** @return the italic status */
	BOOL GetItalic() const
	{
		return m_italic;
	}

	/** @return the getoutline status */
	BOOL GetHasOutline() const
	{
		return m_hasgetoutline;
	}

	/** @return the blur radius applied to glyphs */
	INT32 GetBlurRadius() const
	{
		return m_blurRadius;
	}

	/** @return the face name */
	const uni_char*	GetFaceName() const;

	/** @return the font number */
	UINT32 GetFontNumber() const { return m_fontnumber; };

	/** For use instead of the == operator from font cache.
		It only checks the attributes that matters for font creation. */
	BOOL IsFontSpecificAttEqual(const FontAtt& X)
	{
		return (m_weight   == X.m_weight &&
				m_fontHeight == X.m_fontHeight &&
				m_italic == X.m_italic &&
				m_hasgetoutline == X.m_hasgetoutline &&
				m_blurRadius == X.m_blurRadius &&
				m_fontnumber == X.m_fontnumber);
	}

	/** Equality operator.
		Note that this also compares attributes not needed for font creation (such as
		underline, strikeOut etc. which we draw ourselves). So any comparision of
		only font related attributes should be done with IsFontSpecificAttEqual. */
	BOOL operator ==(const FontAtt& X)
	{
		return (m_overline == X.m_overline &&
				m_weight   == X.m_weight &&
				m_underline == X.m_underline &&
				m_fontHeight == X.m_fontHeight &&
				m_strikeOut == X.m_strikeOut &&
				m_smallCaps == X.m_smallCaps &&
				m_italic == X.m_italic &&
				m_hasgetoutline == X.m_hasgetoutline &&
				m_blurRadius == X.m_blurRadius &&
				m_fontnumber == X.m_fontnumber);
	}

	/** Assignment operator */
	FontAtt& operator =(const FontAtt& X)
	{
		if (!(*this == X))
		{
			m_overline = X.m_overline;
			m_changed = TRUE;
			m_weight = X.m_weight;
			m_underline = X.m_underline;
			m_fontHeight = X.m_fontHeight;
			m_strikeOut = X.m_strikeOut;
			m_italic = X.m_italic;
			m_smallCaps = X.m_smallCaps;
			m_hasgetoutline = X.m_hasgetoutline;
			m_blurRadius = X.m_blurRadius;
			m_fontnumber = X.m_fontnumber;
		}
		return *this;
	}

	/** @return the size of the font (deprecated, very Windows specific) */
	INT32 GetSize() const { return m_fontHeight; }

	/** Sets the font's size. */
	void SetSize(INT32 size)
	{
		if (m_fontHeight != size)
		{
			m_fontHeight = size;
			m_changed = TRUE;
		}
	}

#if defined PREFS_WRITE || defined PREFS_HAVE_STRING_API
	OP_STATUS Serialize(OpString* target) const;
#endif // PREFS_WRITE || PREFS_HAVE_STRING_API

#ifdef PREFS_READ
	BOOL Unserialize(const uni_char *str);
#endif // PREFS_READ

	/**
	 * Gets the x-height of the OpFont corresponding to this. That is achieved
	 * by getting a vertical distance in pixels from the baseline to the top
	 * of the 'x' glyph's bounding box. Temporarily asks for OpFont instance.
	 *
	 * @return The x-height. If there was an error fetching 'x' glyph properties,
	 *		   the value of m_fontHeight / 2 will be returned as an approximation
	 *		   of x-height.
	 */
	INT32 GetExHeight();

private:
	BOOL m_overline;
	BOOL m_changed;
	INT32 m_weight;
	BOOL m_underline;
	INT32 m_fontHeight;
	BOOL m_strikeOut;
	BOOL m_smallCaps;
	BOOL m_italic;
	BOOL m_hasgetoutline;
	INT32 m_blurRadius;
	UINT32 m_fontnumber;
	
	FontAtt(const FontAtt& /*X*/) {}
};

#endif // FONTATT_H
