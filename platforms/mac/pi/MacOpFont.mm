/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/MacOpFont.h"
#include "platforms/mac/pi/MacOpFontManager.h"
#include "platforms/mac/util/CTextConverter.h"
#include "modules/mdefont/processedstring.h"

#if defined(SVG_SUPPORT) && defined(SVG_CAP_HAS_SVGPATH_API)
#include "modules/svg/svg_path.h"
#endif

#define BOOL NSBOOL
#import <Foundation/NSString.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSBezierPath.h>
#undef BOOL

inline BOOL IsJoinedAlphabet(uni_char c)
{
	if (c < 0x0590)
		return FALSE;
	if (c < 0x07c0)
		return TRUE; // Hebrew, Arabic, Syriac & Thaana
	if (c < 0x0900)
		return FALSE;
	if (c < 0x10a0)
		return TRUE; // Devanagari, Bengali, Gurmuki, Gujirati, Oriya, Tamil, Telugu, Kannada, Malayalam, Sinhala, Thai, Lao, Tibetan, Myanmar.
	if (c < 0xfb1d)
		return FALSE;
	if (c < 0xfe00)
		return TRUE;	// Hebrew Presentation Forms, Arabic Presentation Forms A
	if (c < 0xfe70)
		return FALSE;
	if (c < 0xff00)
		return TRUE;	// Arabic Presentation Forms B
	
	return FALSE;
	
	/* Documentation for what is going on above:
	 return (
	 //		   ((c >= 0x0590) && (c <= 0x05FF))	// Hebrew (not really joined, but since it is RTL, we have to let the system treat it word-by-word, otherwise either it or arabic will be wrong)
	 //		|| ((c >= 0x0600) && (c <= 0x06FF))	// Arabic - RTL & joined
	 //		|| ((c >= 0x0700) && (c <= 0x074f))	// Syriac - RTL
	 //		|| ((c >= 0x0750) && (c <= 0x077f))	// Arabic Supplement, Persian - RTL
	 //		|| ((c >= 0x0780) && (c <= 0x07bf))	// Thaana - RTL
	 ((c >= 0x0590) && (c <= 0x07bf))	// Hebrew, Arabic, Syriac & Thaana
	 //		|| ((c >= 0x0900) && (c <= 0x097f))	// Devanagari - Is this needed?
	 //		|| ((c >= 0x0980) && (c <= 0x09ff))	// Bengali - Is this needed?
	 //		|| ((c >= 0x0a00) && (c <= 0x0a7f))	// Gurmuki - Is this needed?
	 //		|| ((c >= 0x0a80) && (c <= 0x0aff))	// Gujirati - Is this needed?
	 //		|| ((c >= 0x0b00) && (c <= 0x0b7f))	// Oriya - Is this needed?
	 //		|| ((c >= 0x0b80) && (c <= 0x0bff))	// Tamil - Is this needed?
	 //		|| ((c >= 0x0c00) && (c <= 0x0c7f))	// Telugu - Is this needed?
	 //		|| ((c >= 0x0c80) && (c <= 0x0cff))	// Kannada - Is this needed?
	 //		|| ((c >= 0x0d00) && (c <= 0x0d7f))	// Malayalam - Is this needed?
	 //		|| ((c >= 0x0d80) && (c <= 0x0dff))	// Sinhala - Is this needed?
	 //		|| ((c >= 0x0e00) && (c <= 0x0e7f))	// Thai - Is this needed?
	 //		|| ((c >= 0x0e80) && (c <= 0x0eff))	// Lao - Is this needed?
	 //		|| ((c >= 0x0f00) && (c <= 0x0fff))	// Tibetan - Is this needed?
	 //		|| ((c >= 0x1000) && (c <= 0x109f))	// Myanmar
	 || ((c >= 0x0900) && (c <= 0x0fff))	// Devanagari, Bengali, Gurmuki, Gujirati, Oriya, Tamil, Telugu, Kannada, Malayalam, Sinhala, Thai, Lao, Tibetan. Probably overkill.
	 || ((c >= 0xfb1d) && (c <= 0xfb4f)) // Hebrew Presentation Forms (part of Alphabetic Presentation Forms, unicode table UFB00) - RTL
	 || ((c >= 0xfb50) && (c <= 0xfdff))	// Arabic Presentation Forms A - RTL
	 || ((c >= 0xfe70) && (c <= 0xfefe))	// Arabic Presentation Forms B - RTL
	 );	// Got to figure out what the status is Oriya, Tamil, ... on all the other weird ones.
	 // Are they joined or just "normal"?
	 */
}

inline BOOL IsHighSurrogate(uni_char c)
{
	return ((c >= 0xD800) && (c <= 0xDBFF));
}

inline BOOL IsLowSurrogate(uni_char c)
{
	return ((c >= 0xDC00) && (c <= 0xDFFF));
}

inline BOOL IsSpecialJoiner(uni_char c)
{
    // 0x200B = ZERO WIDTH SPACE, 0x200C = ZERO WIDTH NON-JOINER, 0x200D = ZERO WIDTH JOINER and 0x00AD = SOFT HYPHEN
	return ((c >= 0x200B) && (c <= 0x200D) || (c == 0x00AD));
}

inline BOOL IsIndicCombiningPattern(char c)
{
	// Pattern that seems to repeat itself for certain scripts.
	return ((c >= 0x01) && (c <= 0x03)) || (c == 0x3C) || ((c >= 0x3E) && (c <= 0x4D)) || ((c >= 0x51) && (c <= 0x57)) || (c == 0x62) || (c == 0x63);
}

inline BOOL IsCombining(uni_char c)	// This is a pretty complete table for all the alphabets I know.
{
	return (
			// Combining diacriticals, note: 0x034F uses both preceding and succeding characters to build one glyph
			((c >= 0x0300) && (c <= 0x036F)) ||
			// Cyrillic combining stuff, mostly historic
			((c >= 0x0483) && (c <= 0x0489) && (c != 0x0487)) ||
			// Hebrew, mainly wowel marks (handled by IsJoinedAlphabet).
			//		((c >= 0x0591) && (c <= 0x05C7) && (c != 0x05BE) && (c != 0x05C0) && (c != 0x05C3) && (c != 0x05C6)) ||
			// Arabic combinings are more complicated, as they in some cases should be considered with the entire preceding word, such as honorifics on names. Luckilly all arabic is handeled special (whew)
			//		((c >= 0x0610) && (c <= 0x0615)) || ((c >= 0x064B) && (c <= 0x065E)) || (c == 0x0670) || ((c >= 0x06D6) && (c <= 0x06DC)) || ((c >= 0x06DF) && (c <= 0x06E4)) || (c == 0x06E7) || (c == 0x06E8) || ((c >= 0x06EA) && (c <= 0x06ED)) ||
			// Syriac wowel signs (handled by IsJoinedAlphabet).
			//		(c == 0x0711) || ((c >= 0x0730) && (c <= 0x034A)) ||
			// Thaana (handled by IsJoinedAlphabet)
			//		((c >= 0x07A6) && (c <= 0x07B0)) ||
			//N'Ko
			((c >= 0x07EB) && (c <= 0x07F3)) ||
			
			// Devanagari, Bengali, Gurmukhi, Gujirati, Oriya, Tamil, Telugu, Kannada, Malayalam
			//		((c >= 0x0900) && (c <= 0x0D7F) && IsIndicCombiningPattern(c >> 9) && (c != 0x0B83)) || (c == 0x0A70) || (c == 0x0A71) ||
			// Devanagari (handled by IsJoinedAlphabet)
			//		((c >= 0x0901) && (c <= 0x0903)) || (c == 0x093C) || ((c >= 0x093E) && (c <= 0x094D)) || ((c >= 0x0951) && (c <= 0x0957)) || (c == 0x0962) || (c == 0x0963) ||
			// Bengali (handled by IsJoinedAlphabet)
			//		((c >= 0x0981) && (c <= 0x0983)) || (c == 0x09BC) || ((c >= 0x09BE) && (c <= 0x09CD)) || ((c >= 0x09D1) && (c <= 0x09D7)) || (c == 0x09E2) || (c == 0x09E3) ||
			// Gurmukhi (handled by IsJoinedAlphabet)
			//		((c >= 0x0A01) && (c <= 0x0A03)) || (c == 0x0A3C) || ((c >= 0x0A3E) && (c <= 0x0A4D)) || (c == 0x0A70) || (c == 0x0A71) ||
			// Gujirati (handled by IsJoinedAlphabet)
			//		((c >= 0x0A81) && (c <= 0x0A83)) || (c == 0x0ABC) || ((c >= 0x0ABE) && (c <= 0x0ACD)) || ((c >= 0x0AD1) && (c <= 0x0AD7)) || (c == 0x0AE2) || (c == 0x0AE3) ||
			// Oriya (handled by IsJoinedAlphabet)
			//		((c >= 0x0B01) && (c <= 0x0B03)) || (c == 0x0B3C) || ((c >= 0x0B3E) && (c <= 0x0B4D)) || ((c >= 0x0B51) && (c <= 0x0B57)) || (c == 0x0B62) || (c == 0x0B63) ||
			// Tamil (handled by IsJoinedAlphabet)
			//		((c >= 0x0B81) && (c <= 0x0B82)) || (c == 0x0BBC) || ((c >= 0x0BBE) && (c <= 0x0BCD)) || ((c >= 0x0BD1) && (c <= 0x0BD7)) || (c == 0x0BE2) || (c == 0x0BE3) ||
			// Telugu (handled by IsJoinedAlphabet)
			//		((c >= 0x0C01) && (c <= 0x0C03)) || (c == 0x0C3C) || ((c >= 0x0C3E) && (c <= 0x0C4D)) || ((c >= 0x0C51) && (c <= 0x0C57)) || (c == 0x0C62) || (c == 0x0C63) ||
			// Kannada (handled by IsJoinedAlphabet)
			//		((c >= 0x0C81) && (c <= 0x0C83)) || (c == 0x0CBC) || ((c >= 0x0CBE) && (c <= 0x0CCD)) || ((c >= 0x0CD1) && (c <= 0x0CD7)) || (c == 0x0CE2) || (c == 0x0CE3) ||
			// Malayalam (handled by IsJoinedAlphabet)
			//		((c >= 0x0D01) && (c <= 0x0D03)) || (c == 0x0D3C) || ((c >= 0x0D3E) && (c <= 0x0D4D)) || ((c >= 0x0D51) && (c <= 0x0D57)) || (c == 0x0D62) || (c == 0x0D63) ||
			// Sinhala (handled by IsJoinedAlphabet)
			//		((c >= 0x0D81) && (c <= 0x0D83)) || ((c >= 0x0DCA) && (c <= 0x0DF3)) ||
			// Thai (handled by IsJoinedAlphabet)
			//		(c == 0x0E31) || ((c >= 0x0E34) && (c <= 0x0E3A)) || ((c >= 0x0E47) && (c <= 0x0E4E)) ||
			// Lao (handled by IsJoinedAlphabet)
			//		(c == 0x0EB1) || ((c >= 0x0EB4) && (c <= 0x0EBC)) || ((c >= 0x0EC8) && (c <= 0x0ECD)) ||
			// Tibetan (handled by IsJoinedAlphabet)
			//		(c == 0x0F18) || (c == 0x0F19) || (c == 0x0F35) || (c == 0x0F37) || (c == 0x0F39) || (c == 0x0F3E) || (c == 0x0F3F) || ((c >= 0x0F71) && (c <= 0x0F87) && (c != 0x0F85)) || ((c >= 0x0F90) && (c <= 0x0FBC)) || (c == 0x0FC6) ||
			
			// FIXME: Add tables for Mynamar (u1000)...
			
			// Combining marks for symbols
			((c >= 0x20D0) && (c <= 0x20FF)) ||
			// CJK puntuation. note: 0x303E uses both preceding and succeding characters to build one glyph
			((c >= 0x302A) && (c <= 0x302F)) || (c == 0x303E) ||
			// combining katakana-hiragana voiced and semi-voiced marks
			(c == 0x3099) || (c == 0x309A) ||
			// Hebrew, variant of 0x05BF (handled by IsJoinedAlphabet)
			//		(c == 0xFB1E) ||
			// Variant selectors
			((c >= 0xFE00) && (c <= 0xFE0F)) ||
			// Combining half marks
			((c >= 0xFE20) && (c <= 0xFE23))
			);
}

inline BOOL IsChinese(uni_char c)
{
	return ((c >= 0x4E00) && (c <= 0x9FFF));
}

inline BOOL IsFallbackOnlyScript(uni_char c)
{
	return IsHighSurrogate(c) || IsLowSurrogate(c) || IsSpecialJoiner(c);
}

MacOpFont::~MacOpFont()
{
	if (this == sContextFont)
		sContextFont = NULL;

	if (mTextAlphaMask)
	{
		free(mTextAlphaMask);
	}

	CGFontRelease(mCGFontRef);
	
	if (mCTFontRef)
		CFRelease(mCTFontRef);
	
	for (int i = 0; i < 256; i++)
	{
		delete mGlyphPages[i];
		delete mGlyphAdvances[i];
	}
	
	OpHashIterator* it = mFallbackFontNames.GetIterator();
	if (OpStatus::IsSuccess(it->First()))
	{
		do {
			CFRelease(it->GetData());
		} while (OpStatus::IsSuccess(it->Next()));
	}	
	
	OP_DELETEA(mFontName);
	
	[mDictionary release];
}

void MacOpFont::Initialize()
{
#if defined(VEGA_3DDEVICE) && defined(PIXEL_SCALE_RENDERING_SUPPORT)
	if (g_opera->libvega_module.rasterBackend == LibvegaModule::BACKEND_HW3D)
		mVegaScale = (float)static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetMaximumHiresScaleFactor() / 100.0;
#endif	// VEGA_3DDEVICE && PIXEL_SCALE_RENDERING_SUPPORT

	if (!sCGContext)
	{
		CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
		CGBitmapInfo alpha = kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst;
		sCGContext = CGBitmapContextCreate(malloc(4), 1, 1, 8, 4, colorSpace, alpha);
	}
	
	NSAttributedString *s = [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar*)L"xxxx" length:4] attributes:mDictionary];
	if (s)
	{
		CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)s);
		if (line)
		{
			CGFloat ascent, descent;
			double width;
			width = CTLineGetTypographicBounds(line, &ascent, &descent, NULL);
			mAscent = ceil(ascent);
			mDescent = ceil(descent);
			mAvgCharWidth = width / 4;
			CGRect bounds;
			bounds = CTLineGetImageBounds(line, sCGContext);
			mExHeight = bounds.size.height + 0.5;
			CFRelease(line);
			mBoundingBox = CTFontGetBoundingBox(mCTFontRef);
		}
		
		[s release];
	}
	
	if (GetShouldSynthesizeItalic())
	{
		static const float degrees2rad = M_PI / 180.0;
		static const float skew = tan(14.0 * degrees2rad);
		mOverhang = skew * (mAscent+mDescent);
	}
	
	CFStringRef str = CTFontCopyName(mCTFontRef, kCTFontFullNameKey);
	if (str)
	{
		UniChar src[128];
		int len = min(CFStringGetLength(str), 127);
		CFStringGetCharacters(str, CFRangeMake(0,len), src);
		src[len] = 0;
		mFontName = OP_NEWA(char, len+1);
		if (mFontName)
		{
			gTextConverter->ConvertStringToMacC(src, mFontName, len+1);
		}
		if (mATSUid)
			mCGFontRef = CGFontCreateWithPlatformFont(&mATSUid);
		else
			mCGFontRef = CGFontCreateWithFontName(str);
		CFRelease(str);
	}

	mIsStronglyContextualFont = false;
	mIsFallbackOnlyFont = false;
	if (mFontName)
	{
		mIsStronglyContextualFont = !strcmp(mFontName, "Zapfino") || !strcmp(mFontName, "Apple Chancery");
		mIsFallbackOnlyFont = !strncmp(mFontName, "Nafees Nastaleeq", 16);
	}
	
	for (int i = 0; i < 256; i++)
	{
		mGlyphPages[i] = 0;
		mGlyphAdvances[i] = 0;
	}

	if (mFontName && (!strnicmp(mFontName, "Hiragino Kaku Gothic Pro", 24) || !strnicmp(mFontName, "Hiragino Mincho Pro", 19) || !strnicmp(mFontName, "Hiragino Maru Gothic Pro", 24)))
	{
		mAscent++;
		mDescent++;
	}
	else if (mFontName && !strncmp(mFontName, "MS Gothic", 9))
	{
		mAscent += 3;
	}
	
	mFilterGlyphs[0] = 0;
}

short MacOpFont::GetGlyphAdvance(GlyphID glyph)
{
    if (!s_advanceBufferInitialized)
	{
		for (int i=0; i<GLYPH_BUFFER_SIZE; i++)
		{
			mAdvanceBuffer[i].height = 0;
		}
		s_advanceBufferInitialized = true;
	}

	unsigned char page = (glyph >> 8) & 0xFF;
	if (!mGlyphAdvances[page])
	{
		mGlyphAdvances[page] = new AdvanceRange;
		for (int i = 0; i < 8; i++)
			mGlyphAdvances[page]->mask[i] = 0;
	}
	
	if (!(mGlyphAdvances[page]->mask[(glyph >> 5) & 0x07] & (1 << (glyph & 0x1F))))
	{
		CGSize size;
		CTFontGetAdvancesForGlyphs(mCTFontRef, kCTFontHorizontalOrientation, &glyph, &size, 1);
		mGlyphAdvances[page]->width[glyph & 0xFF] = size.width + 0.5;
		mGlyphAdvances[page]->mask[(glyph >> 5) & 0x07] |= (1 << (glyph & 0x1F));
	}
	
	return (mGlyphAdvances[page]->width[glyph & 0xFF]);
}

GlyphID MacOpFont::GetGlyphID(UniChar c)
{
	BOOL success = TRUE;
	unsigned char page = (c >> 8) & 0xFF;
	if (!mGlyphPages[page])
	{
		mGlyphPages[page] = new GlyphPage;
		if (!mGlyphPages[page])
			return 0;
		for (int i = 0; i < 8; i++)
			mGlyphPages[page]->mask[i] = 0;
	}
	
	if (!(mGlyphPages[page]->mask[(c >> 5) & 0x07] & (1 << (c & 0x1F))))
	{
		NSAttributedString *s = [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar*)&c length:1] attributes:mDictionary];
		if (s)
		{
			CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)s);
			if (line)
			{
				CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
				if (glyphRuns)
				{
					CFIndex runCount = CFArrayGetCount(glyphRuns);
					if (runCount > 0)
					{
						CGGlyph g;
						CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(glyphRuns, 0);
						if (CFStringRef fallbackFontName = CopySubstitutedFontNameForGlyphRun(run))
						{
							success = FALSE;
							CFRelease(fallbackFontName);
						}
						else
						{
							CTRunGetGlyphs(run, CFRangeMake(0, 1), &g);
						}
						mGlyphPages[page]->glyph[c & 0xFF] = (success?g:0);
						mGlyphPages[page]->mask[(c >> 5) & 0x07] |= (1 << (c & 0x1F));
					}
				}
				CFRelease(line);
			}
			
			[s release];
		}
	}
	
	return (mGlyphPages[page]->glyph[c & 0xFF]);
}

#if defined(SVG_SUPPORT) && defined(SVG_CAP_HAS_SVGPATH_API)
OP_STATUS MacOpFont::GetGlyphOutline(GlyphID glyph, SVGPath* path)
{					
	if (!GetFontName())
		return OpStatus::ERR;

	NSGlyph glyphArray[1];
	glyphArray[0] = glyph;
	NSFont *font = [NSFont fontWithName:[NSString stringWithCString:GetFontName() encoding:[NSString defaultCStringEncoding]] size:GetSize()];
	if (font)
	{
		NSBezierPath *bp = [NSBezierPath bezierPath];
		if (bp)
		{
			[bp moveToPoint:(NSPoint){0.,0.}];
			[bp appendBezierPathWithGlyphs:glyphArray count:1 inFont:font];

			for (int i = 0; i < [bp elementCount]; ++i)
			{
				NSPoint points[3];
				NSBezierPathElement elem = [bp elementAtIndex:i associatedPoints:points];
				switch (elem)
				{
					case NSMoveToBezierPathElement:
						path->MoveTo(points[0].x, points[0].y, FALSE);
						break;
					case NSLineToBezierPathElement:
						path->LineTo(points[0].x, points[0].y, FALSE);
						break;
					case NSCurveToBezierPathElement:
						path->CubicCurveTo(points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y, FALSE, FALSE);
						break;
					case NSClosePathBezierPathElement:
						path->Close();
						break;
				}
			}
			
			return OpStatus::OK;
		}
	}
	
	return OpStatus::ERR_NO_MEMORY;
}
#endif // defined(SVG_SUPPORT) && defined(SVG_CAP_HAS_SVGPATH_API)

BOOL MacOpFont::GetGlyphs(const uni_char* str, unsigned int len, GlyphID *oGlyphInfo, unsigned int& ionumGlyphs, BOOL force /* = FALSE */)
{
	int substr_len = 0;
	unsigned int glyphs_generated = 0;
	BOOL success = TRUE;

	if (len > ionumGlyphs)
		len = ionumGlyphs;

	// Turn off synthesizing bold on Chinese (presumably) fonts
	if (GetShouldSynthesizeBold() && IsChinese(*str))
		mStyle &= ~bold;

	if (!force && (mIsFallbackOnlyFont || !GetFontName() || GetShouldSynthesizeBold() || mSize <= mFontManager->GetAntialisingTreshhold()))
		return FALSE;

	UniChar c, next_c = 0;
	if (len > 0)
		next_c = *str;
	
	for (unsigned int i = 0; i < len; i++)
	{
		c = next_c;
		success &= !IsFallbackOnlyScript(c) || force;
		if (!success)
			break;

		/*	We are cheating here, and it has some known side-effects, that I am willing to live with.
		 For instance ligatures will not happen as the font intended with this 1-1 unicode code point to glyph ID lookup.
		 If you encounter circumstances (like arabic and surrogates, handled below) where this causes unacceptable behaviour,
		 simply call GetGlyphsUncached for the range you prefer.
		 If there are other codepages that needs special treatment, add them to the test below.
		 Other things you might want to do include special testing for common ligatures (fi, fl), rarer ligatures
		 (ff, ffi, ffl, ss, tt, st, ...) or even completely recalculating known ligature-rich fonts,
		 such as Zapfino.
		 */
		if ((i+1) < len)
			next_c = str[1];
		
		BOOL combining = FALSE;
		BOOL was_combining = FALSE;

		if (mIsStronglyContextualFont)
		{
			substr_len = len;
		}
		else while ((was_combining && IsCombining(c)) ||	// Support multiple diacriticals.
					(((i+substr_len+1) < len) && (combining = IsCombining(next_c))) || IsJoinedAlphabet(c))
		{
			if (combining)
			{
				// Combining points need to be considered with the preceding code.
				if (((i+substr_len+2) < len) && ((next_c == 0x034F) || (next_c == 0x303E)))	// Consider both sides of the equation.
					substr_len += 3;
				else
					substr_len += 2;
			}
			else
			{
				substr_len++;
			}
			
			if ((i+substr_len) < len)
				c = str[substr_len];
			else
				break;

			if ((i+substr_len+1) < len)
				next_c = str[substr_len+1];

			was_combining = combining;
			combining = FALSE;
		}

		if (substr_len)
		{
			unsigned int glyph_count = ionumGlyphs - glyphs_generated;
			success &= GetGlyphsUncached(str, substr_len, oGlyphInfo, glyph_count, force);
			str += substr_len;
			i += substr_len - 1;
			next_c = *str;
			substr_len = 0;
			oGlyphInfo += glyph_count;
			glyphs_generated += glyph_count;
		}
		else
		{
			if (GlyphID g = GetGlyphID(*str++))
			{
				*oGlyphInfo++ = g;
				glyphs_generated++;
			}
			else
			{
				return FALSE;
			}
		}
	}

	ionumGlyphs = glyphs_generated;
	return success;
}

BOOL MacOpFont::GetGlyphsUncached(const uni_char* str, unsigned int len, GlyphID *oGlyphInfo, unsigned int& ionumGlyphs, BOOL force /* = FALSE */)
{
	BOOL success = TRUE;
	if (len >= 1)
		success &= !IsFallbackOnlyScript(*str) || force;
	
	if (!force && (!success || GetShouldSynthesizeBold()))
	{
		ionumGlyphs = 0;
		return FALSE;
	}

	success = FALSE;
	NSAttributedString *s = [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar*)str length:len] attributes:mDictionary];
	if (s)
	{
		CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)s);
		if (line)
		{
			CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
			if (glyphRuns)
			{
				CFIndex runCount = CFArrayGetCount(glyphRuns);
				unsigned int added = 0;
				for (CFIndex runIndex = 0; runIndex < runCount; ++runIndex)
				{
					CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(glyphRuns, runIndex);
					CFIndex glyphCount = CTRunGetGlyphCount(run);
					OpAutoArray<CGPoint> positions(OP_NEWA(CGPoint, glyphCount));
					CTRunGetPositions(run, CFRangeMake(0,glyphCount), positions.get());
					for (int i=0; i<glyphCount; i++)
					{
						// If significant y position, revert to fallback
						if (positions[i].y >= 0.5)
						{
							CFRelease(line);
							[s release];
							return FALSE;
						}
					}
					if (CFStringRef fallbackFontName = CopySubstitutedFontNameForGlyphRun(run))
					{
						CFRelease(fallbackFontName);
						CFRelease(line);
						[s release];
						return FALSE;
					}
					CTRunGetGlyphs(run, CFRangeMake(0, glyphCount), (CGGlyph*)oGlyphInfo+added);
					added += glyphCount;
				}
				ionumGlyphs = added;
			}
			success = TRUE;
			CFRelease(line);
		}
		
		[s release];
	}
	
	return success;
}

CFStringRef MacOpFont::CopySubstitutedFontNameForGlyphRun(CTRunRef run)
{
	CFStringRef retval = NULL;
	CFDictionaryRef dict = CTRunGetAttributes(run);
	if (dict)
	{
		CTFontRef font = (CTFontRef)CFDictionaryGetValue(dict, kCTFontAttributeName);
		if (font)
		{
			CFStringRef name = CTFontCopyFullName(mCTFontRef);
			CFStringRef new_name = CTFontCopyFullName(font);
			if (new_name)
			{
				if (!name || CFStringCompare(name, new_name, 0) != kCFCompareEqualTo)
					retval = CFStringCreateCopy(kCFAllocatorDefault, new_name);
				CFRelease(new_name);
			}
			if (name)
				CFRelease(name);
		}
	}
	
	return retval;
}

UINT32 MacOpFont::StringWidth(const uni_char* str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing /*= 0*/)
{
	BOOL success = TRUE;
	unsigned int ioNumGlyphs = GLYPH_BUFFER_SIZE;
	success = GetGlyphs(str, len, mGlyphBuffer, ioNumGlyphs);
	if (success)
	{
		len = ioNumGlyphs;
		int total_width = 0;
		for (unsigned int i = 0; i<len; i++)
			total_width += GetGlyphAdvance(mGlyphBuffer[i]) + extra_char_spacing;
		
		return total_width;
	}
	else
	{
		UINT32 width = 0;
		NSAttributedString *s = [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar*)str length:len] attributes:mDictionary];
		if (s)
		{
			CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)s);
			if (line)
			{
				width = CTLineGetTypographicBounds(line, NULL, NULL, NULL) + len*(extra_char_spacing + ((GetShouldSynthesizeBold()&&!IsChinese(*str))?1:0)) + 0.5;
				CFRelease(line);
			}
			
			[s release];
		}
		
		return width;
	}
}

#ifdef MDF_CAP_PROCESSEDSTRING
OP_STATUS MacOpFont::ProcessString(struct ProcessedString* processed_string,
								   const uni_char* str, const size_t len,
								   INT32 extra_char_spacing, short word_width,
								   bool use_glyph_indices)
{
	float scale = 1.0;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	scale = float(mScaler.GetScale()) / 100.0f;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	unsigned int ioNumGlyphs = GLYPH_BUFFER_SIZE;
	BOOL success;
	int x_accum = 0;
	success = GetGlyphs(str, len, mGlyphBuffer, ioNumGlyphs);
	if (success)
	{
		RETURN_IF_ERROR(mFontManager->GetGlyphBuffer().Grow(ioNumGlyphs));
		processed_string->m_processed_glyphs = mFontManager->GetGlyphBuffer().Storage();
		RETURN_OOM_IF_NULL(processed_string->m_processed_glyphs);
		for (unsigned int i=0; i<ioNumGlyphs; i++)
		{
			processed_string->m_processed_glyphs[i].m_id = mGlyphBuffer[i];
			processed_string->m_processed_glyphs[i].m_pos.x = x_accum;
			x_accum += scale*(processed_string->m_processed_glyphs[i].m_id?(GetGlyphAdvance(mGlyphBuffer[i]) + extra_char_spacing + (GetShouldSynthesizeBold()?1:0)):0);
			processed_string->m_processed_glyphs[i].m_pos.y = 0;
		}
	}
	else
	{
		NSAttributedString *s;
		if (len == 1 && str[0] == 0x00AD)
			s = [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar*)"-" length:1] attributes:mDictionary];
		else
			s = [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar*)str length:len] attributes:mDictionary];
		if (s)
		{
			CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)s);
			if (line)
			{
				CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
				if (glyphRuns)
				{
					ioNumGlyphs = 0;
					BOOL activate_filtering = (uni_strpbrk(str, FILTER_CHARACTERS) != NULL);
					if (activate_filtering)
						SetupGlyphFilters();
					CFIndex runCount = CFArrayGetCount(glyphRuns);
					for (CFIndex runIndex = 0; runIndex < runCount; ++runIndex)
					{
						CFStringRef fallbackFontName = NULL;
						CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(glyphRuns, runIndex);
						fallbackFontName = CopySubstitutedFontNameForGlyphRun(run);
						CFIndex glyphCount = CTRunGetGlyphCount(run);
						RETURN_IF_ERROR(mFontManager->GetGlyphBuffer().Grow(ioNumGlyphs + glyphCount));
						processed_string->m_processed_glyphs = mFontManager->GetGlyphBuffer().Storage();
						RETURN_OOM_IF_NULL(processed_string->m_processed_glyphs);
						for (CFIndex i=0; i<glyphCount; i++)
						{
							CGSize size;
							CGPoint position;
							CGGlyph g;
							CTRunGetGlyphs(run, CFRangeMake(i, 1), &g);
							CTRunGetAdvances(run, CFRangeMake(i, 1), &size);
							CTRunGetPositions(run, CFRangeMake(i, 1), &position);
							BOOL skip = false;
							if (activate_filtering)
								for (int j=0; j<MAX_FILTER_GLYPHS && !skip; ++j)
									skip = (g == mFilterGlyphs[j]);
							ProcessedGlyph pg;
							pg.m_id = 0;
							pg.m_pos.x = x_accum;
							pg.m_pos.y = -position.y;
							if (!skip && g)
							{
								pg.m_id = g;
								if (fallbackFontName)
								{
									pg.m_id += MAGIC_NUMBER;
									if (!mFallbackFontNames.Contains((const void *)pg.m_id))
									{
										mFallbackFontNames.Add((const void *)pg.m_id, (void *)CFStringCreateCopy(kCFAllocatorDefault, fallbackFontName));
									}
									else
									{
										CFStringRef oldFallbackFontName;
										mFallbackFontNames.GetData((const void *)pg.m_id, (void**)&oldFallbackFontName);
										if (CFStringCompare(fallbackFontName, oldFallbackFontName, 0) != kCFCompareEqualTo)
										{
											CFRelease(oldFallbackFontName);
											mFallbackFontNames.Remove((const void *)pg.m_id, (void**)&oldFallbackFontName);
											mFallbackFontNames.Add((const void *)pg.m_id, (void *)CFStringCreateCopy(kCFAllocatorDefault, fallbackFontName));
										}
									}
								}
								x_accum += scale*(size.width + extra_char_spacing + (GetShouldSynthesizeBold()?1:0) + 0.5);
							}
							processed_string->m_processed_glyphs[ioNumGlyphs+i] = pg;
						}
						ioNumGlyphs += glyphCount;
						if (fallbackFontName)
							CFRelease(fallbackFontName);
					}
				}
				CFRelease(line);
			}
			
			[s release];
		}
	}

	processed_string->m_length = ioNumGlyphs;
	processed_string->m_advance = x_accum/scale;
	processed_string->m_is_glyph_indices = TRUE;
	processed_string->m_top_left_positioned = FALSE;	
	
	return OpStatus::OK;
}

#endif

void MacOpFont::SetupGlyphFilters()
{
	if (mFilterGlyphs[0] != 0)
		return;
	
	NSAttributedString *s = [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar*)FILTER_CHARACTERS length:MAX_FILTER_GLYPHS] attributes:mDictionary];
	if (s)
	{
		CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)s);
		if (line)
		{
			CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
			if (glyphRuns)
			{
				CFIndex runCount = CFArrayGetCount(glyphRuns);
				if (runCount > 0)
				{
					CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(glyphRuns, 0);
					CTRunGetGlyphs(run, CFRangeMake(0, MAX_FILTER_GLYPHS), mFilterGlyphs);
				}
			}
			CFRelease(line);
		}
		
		[s release];
	}
}

void MacOpFont::DrawStringUsingFallback(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, CGContextRef ctx, int context_height)
{
	if (!ctx)
		return;

	short ascent = (short)Ascent();
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	ascent = TO_DEVICE_PIXEL(mScaler, ascent);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	Point position;
	position.h = pos.x;
	position.v = (short)context_height - pos.y - ascent;

	if (GetShouldSynthesizeItalic())
	{
		static const float degrees2rad = M_PI / 180.0;
		static const float skew = tan(14.0 * degrees2rad);
		static const CGAffineTransform italicCTM = CGAffineTransformMake(1, 0, skew, 1, 0, 0);
		CGContextConcatCTM(ctx, italicCTM);
		position.h -= skew *  position.v;
	}
	
	NSAttributedString *s;
	if (len == 1 && str[0] == 0x00AD)
		s = [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar*)"-" length:1] attributes:mDictionary];
	else
		s = [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar*)str length:len] attributes:mDictionary];
	if (s)
	{
		CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)s);
		if (line)
		{
			if (!uni_strpbrk(str, FILTER_CHARACTERS))
			{
				CGPoint positions = {m_blur_radius?position.h+OFF_SCREEN:position.h,position.v};
				CGContextSetTextPosition(ctx, positions.x, positions.y);
				CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
				if (glyphRuns)
				{
					CFIndex runCount = CFArrayGetCount(glyphRuns);
					for (CFIndex runIndex = 0; runIndex < runCount; ++runIndex)
					{
						CGFontRef fallbackFont = NULL;
						CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(glyphRuns, runIndex);
						if (CFStringRef fallbackFontName = CopySubstitutedFontNameForGlyphRun(run))
						{
							fallbackFont = CGFontCreateWithFontName(fallbackFontName);
							CGContextSetFont(ctx, fallbackFont);
							CFRelease(fallbackFontName);
						}
						CFIndex glyphCount = CTRunGetGlyphCount(run);
						CTRunGetGlyphs(run, CFRangeMake(0, 0), mGlyphBuffer);
						CTRunGetAdvances(run, CFRangeMake(0, 0), mAdvanceBuffer);
						s_advanceBufferInitialized = false;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
						for (int i=0; i<glyphCount; i++)
						{
							mAdvanceBuffer[i].width = TO_DEVICE_PIXEL(mScaler, (float)mAdvanceBuffer[i].width);
						}
#endif // PIXEL_SCALE_RENDERING_SUPPORT
						if (extra_char_spacing || GetShouldSynthesizeBold())
						{
							for (int i=0; i<glyphCount; i++)
							{
								mAdvanceBuffer[i].width += extra_char_spacing + (GetShouldSynthesizeBold()?1:0);
							}
						}
						CGContextShowGlyphsWithAdvances(ctx, (const CGGlyph*)mGlyphBuffer, mAdvanceBuffer, glyphCount);
						if (GetShouldSynthesizeBold())
						{
							CGContextSetTextPosition(ctx, positions.x+1, positions.y);
							CGContextShowGlyphsWithAdvances(ctx, (const CGGlyph*)mGlyphBuffer, mAdvanceBuffer, glyphCount);
						}
						if (fallbackFont)
						{
							CGContextSetFont(ctx, GetCGFontRef());
							CGFontRelease(fallbackFont);
						}
					}
				}
			}
			else
			{
				SetupGlyphFilters();
				CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
				if (glyphRuns)
				{
					CFIndex runCount = CFArrayGetCount(glyphRuns);
					for (int i = 0; i <= (GetShouldSynthesizeBold()?1:0); ++i)
					{
						CGPoint positions = {m_blur_radius?position.h+OFF_SCREEN:position.h,position.v};
						CGContextSetTextPosition(ctx, positions.x + i, positions.y);
						for (CFIndex runIndex = 0; runIndex < runCount; ++runIndex)
						{
							CGFontRef fallbackFont = NULL;
							CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(glyphRuns, runIndex);
							if (CFStringRef fallbackFontName = CopySubstitutedFontNameForGlyphRun(run))
							{
								fallbackFont = CGFontCreateWithFontName(fallbackFontName);
								CGContextSetFont(ctx, fallbackFont);
								CFRelease(fallbackFontName);
							}
							CFIndex glyphCount = CTRunGetGlyphCount(run);
							for (CFIndex i=0; i<glyphCount; i++)
							{
								CGSize size;
								CGGlyph g;
								BOOL skip = false;
								CTRunGetGlyphs(run, CFRangeMake(i, 1), &g);
								CTRunGetAdvances(run, CFRangeMake(i, 1), &size);
								for (int j=0; j<MAX_FILTER_GLYPHS && !skip; ++j)
									skip = (g == mFilterGlyphs[j]);
								if (!skip)
								{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
									size.width = TO_DEVICE_PIXEL(mScaler, (float)size.width);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
									size.width += extra_char_spacing + (GetShouldSynthesizeBold()?1:0);
									CGContextShowGlyphsWithAdvances(ctx, &g, &size, 1);
								}
							}
							if (fallbackFont)
							{
								CGContextSetFont(ctx, GetCGFontRef());
								CGFontRelease(fallbackFont);
							}
						}
					}
				}
			}
			CFRelease(line);
		}
		
		[s release];
	}
}

CTFontRef MacOpFont::TryToFindNewFont(CTFontSymbolicTraits trait_in)
{
	CTFontRef retval = NULL;
	CFStringRef family, newFamily;
	CTFontSymbolicTraits trait_out;
	family = CTFontCopyFamilyName(mCTFontRef);
	if (family)
	{
		CTFontRef newFont = CTFontCreateCopyWithSymbolicTraits(mCTFontRef, GetSize(), NULL, trait_in, kCTFontBoldTrait | kCTFontItalicTrait);
		if (newFont)
		{
			newFamily = CTFontCopyFamilyName(newFont);
			if (newFamily)
			{
				trait_out = CTFontGetSymbolicTraits(newFont) & (kCTFontBoldTrait | kCTFontItalicTrait);
				if (CFStringCompare(family, newFamily, 0) || trait_out != trait_in)
					CFRelease(newFont);
				else
					retval = newFont;
				CFRelease(newFamily);
			}
		}
		CFRelease(family);
	}
	
	return retval;
}

BOOL MacOpFont::UpdateCTFontRefAndATSUId(CTFontRef newFont, CTFontSymbolicTraits trait)
{
	ATSUFontID newId;
	unsigned char style;
	CFStringRef name = CTFontCopyPostScriptName(newFont);
	newId = ATSFontFindFromPostScriptName(name, kATSOptionFlagsDefault);
	CFRelease(name);
	if (newId && newId != mATSUid)
	{
		CFRelease(mCTFontRef);
		style = (trait & kCTFontBoldTrait)?bold:0 | (trait & kCTFontItalicTrait)?italic:0;
		mCTFontRef = newFont;
		mATSUid = newId;
		mStyle &= ~style;

		[mDictionary release];
		mDictionary = [[NSDictionary alloc] initWithObjectsAndKeys:(id)mCTFontRef, (id)kCTFontAttributeName, nil];
		OP_ASSERT(mDictionary);
		
		return TRUE;
	}
	
	return FALSE;
}

void MacOpFont::CreateCTFont()
{
	if (mCTFontRef)
	{
		CFStringRef str = CTFontCopyName(mCTFontRef, kCTFontFullNameKey);
		if (str)
		{
			mCTFontRef = CTFontCreateWithName(str, GetSize(), NULL);
			CFRelease(str);
		}
		else
		{
			mCTFontRef = CTFontCreateCopyWithAttributes(mCTFontRef, GetSize(), NULL, NULL);
		}
	}
	else
	{
		mCTFontRef = CTFontCreateWithPlatformFont(mATSUid, GetSize(), NULL, NULL);
	}
	OP_ASSERT(mCTFontRef);
	mDictionary = [[NSDictionary alloc] initWithObjectsAndKeys:(id)mCTFontRef, (id)kCTFontAttributeName, nil];
	OP_ASSERT(mDictionary);

	if (mStyle)
	{
		CTFontSymbolicTraits trait_in;
		CTFontRef newFont;
		
		// Try to find a font with native italic/bold
		trait_in = (isBold()?kCTFontBoldTrait:0) | (isItalic()?kCTFontItalicTrait:0);
		newFont = TryToFindNewFont(trait_in);
		if (!newFont && mStyle == (bold | italic))
		{
			trait_in = kCTFontBoldTrait;
			newFont = TryToFindNewFont(trait_in);
			if (!newFont)
			{
				trait_in = kCTFontItalicTrait;
				newFont = TryToFindNewFont(trait_in);
			}
		}
		if (newFont)
		{
			if (!UpdateCTFontRefAndATSUId(newFont, trait_in))
				CFRelease(newFont);
		}
	}
}

bool MacOpFont::UseGlyphInterpolation()
{
	return mSize > mFontManager->GetAntialisingTreshhold();
}
