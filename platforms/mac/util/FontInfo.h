/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef _MAC_FONT_INFO_H
#define _MAC_FONT_INFO_H

//#define UNIT_TEXT_OUT
#define USE_CG_SHOW_GLYPHS     // Faster, but getting a strange crash when printing ligatures in prview mode.
#define USE_TRANSIENT_FONTMATCHING
//#define TRY_BASELINE_ADJUST
//#define USE_ONE_PIXEL_FONTS	   // Reduce the number of outline hits ATS gets be using one font size throughout.
#define _NEW_FONTSWITCHING_
#define DELAYED_CODEPAGE_CALCULATION
#define REARRANGE_FONTS_BY_SUITABLITY
#define NEW_FONT_SORTING
//#define REMOVE_BAD_CODEPAGES   // Various hacks to ensure only one font handles problematic codepages.

#define GLYPH_BUFFER_SIZE 1024

#define PlatformIDUnicode  0
#define PlatformIDApple  1
#define PlatformIDMicrosoft  3
#define MicrosoftStandardEncoding 1
#define UnicodeFormatSegmentLookup  4
#define UnicodeFormatSegmentedCoverage  12

inline short SwapShort(short x) { return ((((x) & 0x00FF) << 8) | (((x) & 0xFF00) >> 8)); }
inline long SwapLong(long x) { return ((((x) & 0x000000FF) << 24) | (((x) & 0x0000FF00) << 8) | (((x) & 0x00FF0000) >> 8) | (((x) & 0xFF000000) >> 24)); }

#define B2LENDIAN(x) (((x & 0xff) << 8) | ((x >> 8) & 0xff)) 
#define fixed1              ((Fixed) 0x00010000L)
#define FixedToInt(a)       ((short)(((Fixed)(a) + fixed1/2) >> 16))

#ifdef OPERA_BIG_ENDIAN
# define FONT_TO_NATIVE_16(x) (x)
# define FONT_TO_NATIVE_32(x) (x)
#else
# define FONT_TO_NATIVE_16(x) SwapShort(x)
# define FONT_TO_NATIVE_32(x) SwapLong(x)
#endif

// Default font names:
#define FONT_LUCIDA_GRANDE      UNI_L("Lucida Grande")
#define FONT_HELVETICA  		UNI_L("Helvetica")
#define FONT_TIMES  			UNI_L("Times")
#define FONT_COURIER			UNI_L("Courier")

# define FONT_HIRAGINO_MINCHO	UNI_L("Hiragino Mincho Pro W3")
# define FONT_HIRAGINO_GOTHIC	UNI_L("Hiragino Kaku Gothic Pro W3")
# define FONT_OSAKA_MONO		UNI_L("Osaka-Mono")

#define FONT_APPLE_MYUNGJO	    UNI_L("AppleMyungjo")
#define FONT_APPLE_GOTHIC	    UNI_L("AppleGothic")

# define FONT_ST_HEITI		    UNI_L("STHeiti")
# define FONT_ST_KAITI			UNI_L("STKaiti")

#define FONT_APPLE_LI_GOTHIC	UNI_L("Apple LiGothic Medium")
#define FONT_APPLE_LI_SUNG		UNI_L("Apple LiSung Light")

// Other scripts
#define FONT_SYMBOL			    UNI_L("Symbol")
#define FONT_ARABIC			    UNI_L("Geeza Pro")
#define FONT_ARMENIAN			UNI_L("Mshtakan")
#define FONT_DEVANAGARI		    UNI_L("Devanagari MT")
#define FONT_GURMUKHI			UNI_L("Gurmukhi MT")
#define FONT_GUJARATI			UNI_L("Gujarati MT")
#define FONT_CHEROKEE			UNI_L("Plantagenet Cherokee")
#define FONT_TAMIL				UNI_L("InaiMathi")
#define FONT_UCAS				UNI_L("Euphemia UCAS")


#ifdef USE_CG_SHOW_GLYPHS
typedef struct GlyphPage {
	long    mask[8];
	GlyphID glyph[256];
} GlyphRange;
#endif // USE_CG_SHOW_GLYPHS

typedef struct AdvanceRange {
	long mask[8];
	short width[256];
} AdvanceRange;

typedef struct tagFONTSIGNATURE
{
	uint32_t	fsUsb[4];
	uint32_t	fsCsb[2];

} FONTSIGNATURE, *PFONTSIGNATURE;


// Used when parsing name tags
struct FontTableName
{
	uint16_t	formatSelector;
	uint16_t	numberOfNameRecords;
	uint16_t	offsetOfStringStorage;
};

// Used when parsing name tags
struct FontTableNameRecord
{
	uint16_t	platformID;
	uint16_t	platformSpecificEncodingID;
	uint16_t	languageID;
	uint16_t	nameID;
	uint16_t	stringLength;
	uint16_t	stringOffset;
};

struct PANOSE
{
	uint8_t		familyType;
	uint8_t		serifStyle;
	uint8_t		weight;
	uint8_t		proportion;
	uint8_t		contrast;
	uint8_t		strokeVariation;
	uint8_t		armStyle;
	uint8_t		letterForm;
	uint8_t		midline;
	uint8_t		xHeight;
};

struct OS2FontTable
{
	uint16_t	version;
	uint16_t	xAverageCharWidth;
	uint16_t	usWeightClass;
	uint16_t	usWidthClass;
	uint16_t	fsType;
	int16_t		ySubscriptXSize;
	int16_t		ySubscriptYSize;
	int16_t		ySubscriptXOffset;
	int16_t		ySubscriptYOffset;
	int16_t		ySuperscriptXSize;
	int16_t		ySuperscriptYSize;
	int16_t		ySuperscriptXOffset;
	int16_t		ySuperscriptYOffset;
	int16_t		yStrikeoutSize;
	int16_t		yStrikeoutPosition;
	int16_t		sFamilyClass;
	PANOSE		panose;
	uint32_t	ulUnicodeRange1;		// Bits 0-31
	uint32_t	ulUnicodeRange2;		// Bits 32-63
	uint32_t	ulUnicodeRange3;		// Bits 64-95
	uint32_t	ulUnicodeRange4;		// Bits 96-127
	uint8_t		achVendID[4];
	uint16_t	fsSelection;
	uint16_t	usFirstCharacterIndex;
	uint16_t	usLastCharacterIndex;
	int16_t		sTypoAscender;
	int16_t		sTypoDescender;
	int16_t		sTypoLineGap;
	uint16_t	usWinAscent;
	uint16_t	usWinDescent;
	uint32_t	ulCodePageRange1;		// Bits 0-31
	uint32_t	ulCodePageRange2;		// Bits 32-63
	int16_t		sxHeight;
	int16_t		sCapHeight;
	uint16_t	usDefaultChar;
	uint16_t	usBreakChar;
	uint16_t	usMaxContent;
};

typedef struct
{
	uint16 format;
	union
	{
		struct // format 0, 2, 6
		{
			uint16 length;
			uint16 language;
		} f0;
		struct // format 4
		{
			uint16 length;
			uint16 language;
			uint16 segCountX2;    // 2 * segCount
			uint16 searchRange;   // 2 * (2**FLOOR(log2(segCount)))
			uint16 entrySelector; // log2(searchRange/2)
			uint16 rangeShift;    // (2 * segCount) - searchRange
				
			byte   rest;
				
			// The rest of the subtable is dynamic
			/*
			 uint16 endCode[segCount] // Ending character code for each segment, last = 0xFFFF.
			 uint16 reservedPad; // This value should be zero
			 uint16 startCode[segCount]; // Starting character code for each segment
			 uint16 idDelta[segCount];// Delta for all character codes in segment
			 uint16 idRangeOffset[segCount];// Offset in bytes to glyph indexArray, or 0
			 uint16 glyphIndexArray[variable];
			 */
		} f4;
		struct // format 8.0, 10.0, 12.0
		{
		} f8;
	};
} CMapGlyphTable;


enum PanoseSerifStyle {
	kPanoseSerifStyleAny				= 0,
	kPanoseSerifStyleNoFit				= 1,
	kPanoseSerifStyleCove				= 2,
	kPanoseSerifStyleObtuseCove			= 3,
	kPanoseSerifStyleSquareCove			= 4,
	kPanoseSerifStyleObtuseSquareCove	= 5,
	kPanoseSerifStyleSquare				= 6,
	kPanoseSerifStyleThin				= 7,
	kPanoseSerifStyleBone				= 8,
	kPanoseSerifStyleExaggerated		= 9,
	kPanoseSerifStyleTriangle			= 10,
	kPanoseSerifStyleNormalSans			= 11,
	kPanoseSerifStyleObtuseSans			= 12,
	kPanoseSerifStylePerpSans			= 13,
	kPanoseSerifStyleFlared				= 14,
	kPanoseSerifStyleRounded			= 15
};

enum InternalFontID {
	kInternalFontIDUnknown = 0,
	kInternalFontIDLucidaGrande,
	kInternalFontIDHelvetica,
	kInternalFontIDHelveticaCY,
	kInternalFontIDHelveticaNeue,
	kInternalFontIDHelveticaNeueBoldCondensed,
	kInternalFontIDHelveticaNeueUltraLight,
	kInternalFontIDHelveticaNeueLight,
	kInternalFontIDHelveticaNeueBlackCondensed,
	kInternalFontIDGeneva,
	kInternalFontIDGenevaCY,
	kInternalFontIDArial,
	kInternalFontIDArialBlack,
	kInternalFontIDArialNarrow,
	kInternalFontIDArialRoundedMTBold,
	kInternalFontIDArialHebrew,
	kInternalFontIDArialHebrewBold,
	kInternalFontIDArialUnicodeMS,
	kInternalFontIDTimes,
	kInternalFontIDTimesRoman,
	kInternalFontIDNewYork,
	kInternalFontIDTimesNewRoman,
	kInternalFontIDMonaco,
	kInternalFontIDAppleMenlo,
	kInternalFontIDCourier,
	kInternalFontIDCourierNew,
	kInternalFontIDAppleChancery,
	kInternalFontIDSand,
	kInternalFontIDComicSansMS,
	kInternalFontIDPapyrus,
	kInternalFontIDHiraginoMincho,
	kInternalFontIDHiraginoMinchoProN,
	kInternalFontIDHiraginoMinchoProW6,
	kInternalFontIDHiraginoKakuGothic,
	kInternalFontIDHiraginoKakuGothicProN,
	kInternalFontIDHiraginoKakuGothicProW6,
	kInternalFontIDHiraginoKakuGothicStdW8,
	kInternalFontIDHiraginoMaruGothicProW4,
	kInternalFontIDOsaka,
	kInternalFontIDOsakaMono,
	kInternalFontIDAppleMyungjo,
	kInternalFontIDAppleGothic,
	kInternalFontIDSTHeitiLight,
	kInternalFontIDSTHeiti,
	kInternalFontIDSTKaiti,
	kInternalFontIDAppleLiGothic,
	kInternalFontIDAppleLiSung,
	kInternalFontIDSymbol,
	kInternalFontIDGeezaPro,
	kInternalFontIDGeezaProBold,
	kInternalFontIDMshtakan,
	kInternalFontIDDevanagariMT,
	kInternalFontIDDevanagariMTBold,
	kInternalFontIDGurmukhiMT,
	kInternalFontIDGujaratiMT,
	kInternalFontIDPlantagenetCherokee,
	kInternalFontIDInaiMathi,
	kInternalFontIDEuphemiaUCAS,
	kInternalFontIDEuphemiaUCASBold,
	kInternalFontIDEuphemiaUCASItalic,
	kInternalFontIDHei,
	kInternalFontIDLastResort,
	kInternalFontIDZapfDingbats,
	kInternalFontIDLiHei,
	kInternalFontIDAlBayanPlain,
	kInternalFontIDAlBayanBold,
	kInternalFontIDAmericanTypewriter,
	kInternalFontIDAmericanTypewriterCondensed,
	kInternalFontIDAmericanTypewriterLight,
	kInternalFontIDAmericanTypewriterCondensedLight,
	kInternalFontIDAndaleMono,
	kInternalFontIDAppleSymbols,
	kInternalFontIDAyuthaya,
	kInternalFontIDBaghdad,
	kInternalFontIDBaskerville,
	kInternalFontIDBaskervilleSemibold,
	kInternalFontIDBiauKai,
	kInternalFontIDBigCaslon,
	kInternalFontIDBrushScriptMT,
	kInternalFontIDChalkboard,
	kInternalFontIDChalkboardBold,
	kInternalFontIDCharcoalCY,
	kInternalFontIDCochin,
	kInternalFontIDCapitals,
	kInternalFontIDCopperplate,
	kInternalFontIDCopperplateLight,
	kInternalFontIDCorsivaHebrew,
	kInternalFontIDCorsivaHebrewBold,
	kInternalFontIDDecoTypeNaskh,
	kInternalFontIDDidot,
	kInternalFontIDFangSong,
	kInternalFontIDFutura,
	kInternalFontIDFuturaCondensed,
	kInternalFontIDGeorgia,
	kInternalFontIDGillSans,
	kInternalFontIDGillSansLight,
	kInternalFontIDGujaratiMTBold,
	kInternalFontIDHerculanum,
	kInternalFontIDHoeflerText,
	kInternalFontIDHoeflerTextOrnaments,
	kInternalFontIDImpact,
	kInternalFontIDKai,
	kInternalFontIDKrungthep,
	kInternalFontIDKufiStandardGK,
	kInternalFontIDMarkerFelt,
	kInternalFontIDMicrosoftSansSerif,
	kInternalFontIDMshtakanBold,
	kInternalFontIDMshtakanBoldOblique,
	kInternalFontIDMshtakanOblique,
	kInternalFontIDNadeem,
	kInternalFontIDNewPeninimMT,
	kInternalFontIDNewPeninimMTBold,
	kInternalFontIDNewPeninimMTBoldInclined,
	kInternalFontIDNewPeninimMTInclined,
	kInternalFontIDGB18030Bitmap,
	kInternalFontIDOptima,
	kInternalFontIDOptimaExtraBlack,
	kInternalFontIDRaanana,
	kInternalFontIDRaananaBold,
	kInternalFontIDSathu,
	kInternalFontIDSilom,
	kInternalFontIDSkia,
	kInternalFontIDSong,
	kInternalFontIDTahoma,
	kInternalFontIDThonburi,
	kInternalFontIDTrebuchetMS,
	kInternalFontIDVerdana,
	kInternalFontIDWebdings,
	kInternalFontIDZapfino,
	kInternalFontIDLiSong,
	kInternalFontIDSTFangSong,
	kInternalFontIDSTSong,
	kInternalFontIDSTXihei,
	kInternalFontIDHeitiSCLight,
	kInternalFontIDHeitiTCLight
};



// Prioritized Font Lists
#define LATIN_FONT_IDS				kInternalFontIDLucidaGrande, kInternalFontIDHelvetica, kInternalFontIDTimes, kInternalFontIDTimesRoman, kInternalFontIDAppleMenlo, kInternalFontIDMonaco
#define JAPANESE_FONT_IDS			kInternalFontIDHiraginoKakuGothicProN, kInternalFontIDHiraginoKakuGothic, kInternalFontIDHiraginoMinchoProN, kInternalFontIDHiraginoMincho, kInternalFontIDOsakaMono
#define KOREAN_FONT_IDS				kInternalFontIDAppleGothic, kInternalFontIDAppleMyungjo
#define SIMPLIFIED_CN_FONT_IDS		kInternalFontIDHeitiSCLight, kInternalFontIDSTXihei, kInternalFontIDSTHeiti, kInternalFontIDSTKaiti
#define TRADITIONAL_CN_FONT_IDS		kInternalFontIDHeitiTCLight, kInternalFontIDLiSong, kInternalFontIDLiHei, kInternalFontIDBiauKai
#define OTHER_FONT_IDS				kInternalFontIDSymbol, kInternalFontIDGeezaPro, kInternalFontIDMshtakan, kInternalFontIDDevanagariMT, kInternalFontIDGurmukhiMT, \
                                    kInternalFontIDGujaratiMT, kInternalFontIDPlantagenetCherokee, kInternalFontIDInaiMathi, kInternalFontIDEuphemiaUCAS


// Ranges
// These ranges are defined in data/i18ndata/tables/sources/uniblocks.txt
// Which in turn is based on Microsoft's definitions.

#define UNICODE_RANGE_MASK_LATIN1								(1 << 0)		// 0		// 0020-007e
#define UNICODE_RANGE_MASK_LATIN1SUPPLEMENT						(1 << 1)		// 1		// 00a0-00ff
#define UNICODE_RANGE_MASK_LATINEXTENDEDA						(1 << 2)		// 2		// 0100-017f
#define UNICODE_RANGE_MASK_LATINEXTENDEDB						(1 << 3)		// 3		// 0180-024f
#define UNICODE_RANGE_MASK_IPAEXTENSIONS						(1 << 4)		// 4		// 0250-02af
#define UNICODE_RANGE_MASK_SPACINGMODIFIEDLETTERS				(1 << 5)		// 5		// 02b0-02ff
#define UNICODE_RANGE_MASK_COMBININGDIACRITICALMARKS			(1 << 6)		// 6		// 0300-036f
#define UNICODE_RANGE_MASK_GREEK								(1 << 7)		// 7		// 0370-03ff
#define UNICODE_RANGE_MASK_COPTIC								(1 << 8)		// 8		// 2c80-2cff
#define UNICODE_RANGE_MASK_CYRILLIC								(1 << 9)		// 9		// 0400-04ff, 0500-052f (Cyrillic Supplementary)
#define UNICODE_RANGE_MASK_ARMENIAN								(1 << 10)		// 10		// 0530-058f
#define UNICODE_RANGE_MASK_HEBREW 								(1 << 11)		// 11		// 0590-05ff
#define UNICODE_RANGE_MASK_VAI									(1 << 12)		// 12		// a500-a63f
#define UNICODE_RANGE_MASK_ARABIC 								(1 << 13)		// 13		// 0600-077f (+ Arabic Supplement)
#define UNICODE_RANGE_MASK_NKO									(1 << 14)		// 14		// 07c0-07ff
#define UNICODE_RANGE_MASK_DEVANAGARI							(1 << 15)		// 15		// 0900-097f
#define UNICODE_RANGE_MASK_BENGALI								(1 << 16)		// 16		// 0980-09ff
#define UNICODE_RANGE_MASK_GURMUKHI								(1 << 17)		// 17		// 0a00-0a7f
#define UNICODE_RANGE_MASK_GUJARATI								(1 << 18)		// 18		// 0a80-0aff
#define UNICODE_RANGE_MASK_ORIYA								(1 << 19)		// 19		// 0b00-0b7f
#define UNICODE_RANGE_MASK_TAMIL								(1 << 20)		// 20		// 0b80-0bff
#define UNICODE_RANGE_MASK_TELUGU								(1 << 21)		// 21		// 0c00-0c7f
#define UNICODE_RANGE_MASK_KANNADA								(1 << 22)		// 22		// 0c80-0cff
#define UNICODE_RANGE_MASK_MALAYALAM							(1 << 23)		// 23		// 0d00-0d7f
#define UNICODE_RANGE_MASK_THAI									(1 << 24)		// 24		// 0e00-0e7f
#define UNICODE_RANGE_MASK_LAO									(1 << 25)		// 25		// 0e80-0eff
#define UNICODE_RANGE_MASK_BASICGEORGIAN						(1 << 26)		// 26		// 10a0-10ff
#define UNICODE_RANGE_MASK_BALINESE								(1 << 27)		// 27		// 1b00-1b7f
#define UNICODE_RANGE_MASK_HANGULJAMO							(1 << 28)		// 28		// 1100-11ff
#define UNICODE_RANGE_MASK_LATINEXTENDEDADDITIONAL				(1 << 29)		// 29		// 1e00-1eff
#define UNICODE_RANGE_MASK_GREEKEXTENDED						(1 << 30)		// 30		// 1f00-1fff
#define UNICODE_RANGE_MASK_GENERALPUNCTUATION					(1 << 31)		// 31		// 2000-206f

#define UNICODE_RANGE_MASK_SUPERSCRIPTSANDSUBSCRIPTS			(1 << 0)		// 32		// 2070-209f
#define UNICODE_RANGE_MASK_CURRENCYSYMBOLS						(1 << 1)		// 33		// 20a0-20cf
#define UNICODE_RANGE_MASK_COMBININGDIACRITICALMARKSFORSYMBOLS	(1 << 2)		// 34		// 20d0-20ff
#define UNICODE_RANGE_MASK_LETTERLIKESYMBOLS					(1 << 3)		// 35		// 2100-214f
#define UNICODE_RANGE_MASK_NUMBERFORMS							(1 << 4)		// 36		// 2150-218f
#define UNICODE_RANGE_MASK_ARROWS								(1 << 5)		// 37		// 2190-21ff, 27F0-27FF (Supplemental Arrows-A), 2900-297F (Supplemental Arrows-B)
#define UNICODE_RANGE_MASK_MATHEMATICALOPERATORS				(1 << 6)		// 38		// 2200-22ff, 2A00-2AFF (Supplemental Mathematical Operators), 27C0-27EF (Miscellaneous Mathematical Symbols-A), 2980-29FF (Miscellaneous Mathematical Symbols-B)
#define UNICODE_RANGE_MASK_MISCELLANEOUSTECHNICAL				(1 << 7)		// 39		// 2300-23ff
#define UNICODE_RANGE_MASK_CONTROLPICTURES						(1 << 8)		// 40		// 2400-243f
#define UNICODE_RANGE_MASK_OPTICALCHARACTERRECOGNITION			(1 << 9)		// 41		// 2440-245f
#define UNICODE_RANGE_MASK_ENCLOSEDALPHANUMERICS				(1 << 10)		// 42		// 2460-24ff
#define UNICODE_RANGE_MASK_BOXDRAWING							(1 << 11)		// 43		// 2500-257f
#define UNICODE_RANGE_MASK_BLOCKELEMENTS						(1 << 12)		// 44		// 2580-259f
#define UNICODE_RANGE_MASK_GEOMETRICSHAPES						(1 << 13)		// 45		// 25a0-25ff
#define UNICODE_RANGE_MASK_MISCELLANEOUSSYMBOLS					(1 << 14)		// 46		// 2600-26ff
#define UNICODE_RANGE_MASK_DINGBATS								(1 << 15)		// 47		// 2700-27bf
#define UNICODE_RANGE_MASK_CJKSYMBOLSANDPUNCTUATION				(1 << 16)		// 48		// 3000-303f
#define UNICODE_RANGE_MASK_HIRAGANA								(1 << 17)		// 49		// 3040-309f
#define UNICODE_RANGE_MASK_KATAKANA								(1 << 18)		// 50		// 30a0-30ff, 31F0-31FF (Katakana Phonetic Extensions)
#define UNICODE_RANGE_MASK_BOPOMOFO								(1 << 19)		// 51		// 3100-312f, 31A0-31BF (Bopomofo Extended)
#define UNICODE_RANGE_MASK_HANGULCOMPATIBILITYJAMO				(1 << 20)		// 52		// 3130-318f
#define UNICODE_RANGE_MASK_CJKMISCELLANEOUS						(1 << 21)		// 53		// 3190-319f
#define UNICODE_RANGE_MASK_ENCLOSEDCJKLETTERSANDMONTHS			(1 << 22)		// 54		// 3200-32ff
#define UNICODE_RANGE_MASK_CJKCOMPATIBILITY						(1 << 23)		// 55		// 3300-33ff
#define UNICODE_RANGE_MASK_HANGUL								(1 << 24)		// 56		// ac00-d7a3
#define UNICODE_RANGE_MASK_SURROGATES							(1 << 25)		// 57		// d800-dfff
#define UNICODE_RANGE_MASK_PHOENICIAN							(1 << 26)		// 58		// 10900-1091f
#define UNICODE_RANGE_MASK_CJKUNIFIEDIDEOGRAPHS					(1 << 27)		// 59		// 4e00-9fff, 2E80-2EFF (CJK Radicals Supplement), 2F00-2FDF (Kangxi Radicals), 2FF0-2FFF (Ideographic Description Characters), 3400-4DBF (CJK Unified Ideograph Extension A), 3190-319F (Kanbun)
#define UNICODE_RANGE_MASK_PRIVATEUSEAREA						(1 << 28)		// 60		// e000-f8ff
#define UNICODE_RANGE_MASK_CJKCOMPATIBILITYIDEOGRAPHS			(1 << 29)		// 61		// f900-faff
#define UNICODE_RANGE_MASK_ALPHABETICPRESENTATIONFORMS			(1 << 30)		// 62		// fb00-fb4f
#define UNICODE_RANGE_MASK_ARABICPRESENTATIONFORMSA				(1 << 31)		// 63		// fb50-fdff

#define UNICODE_RANGE_MASK_COMBININGHALFMARKS					(1 << 0)		// 64		// fe20-fe2f
#define UNICODE_RANGE_MASK_CJKCOMPATIBILITYFORMS				(1 << 1)		// 65		// fe30-fe4f
#define UNICODE_RANGE_MASK_SMALLFORMVARIANTS					(1 << 2)		// 66		// fe50-fe6f
#define UNICODE_RANGE_MASK_ARABICPRESENTATIONFORMSB				(1 << 3)		// 67		// fe70-fefe
#define UNICODE_RANGE_MASK_HALFWIDTHANDFULLWIDTHFORMS			(1 << 4)		// 68		// ff00-ffef
#define UNICODE_RANGE_MASK_SPECIALS								(1 << 5)		// 69		// fff0-fffd
#define UNICODE_RANGE_MASK_TIBETIAN								(1 << 6)		// 70		// 0f00-0fff
#define UNICODE_RANGE_MASK_SYRIAC								(1 << 7)		// 71		// 0700-074f
#define UNICODE_RANGE_MASK_THAANA								(1 << 8)		// 72		// 0780-07bf
#define UNICODE_RANGE_MASK_SINHALA								(1 << 9)		// 73		// 0d80-0dff
#define UNICODE_RANGE_MASK_MYANMAR								(1 << 10)		// 74		// 1000-109f
#define UNICODE_RANGE_MASK_ETHIOPIC								(1 << 11)		// 75		// 1200-137f
#define UNICODE_RANGE_MASK_CHEROKEE								(1 << 12)		// 76		// 13a0-13ff
#define UNICODE_RANGE_MASK_CANADIANABORIGINALSYLLABICS			(1 << 13)		// 77		// 1400-167f
#define UNICODE_RANGE_MASK_OGHAM								(1 << 14)		// 78		// 1680-169f
#define UNICODE_RANGE_MASK_RUNIC								(1 << 15)		// 79		// 16a0-16ff
#define UNICODE_RANGE_MASK_KHMER								(1 << 16)		// 80		// 1780-17ff
#define UNICODE_RANGE_MASK_MONGOLIAN							(1 << 17)		// 81		// 1800-18af
#define UNICODE_RANGE_MASK_BRAILLE								(1 << 18)		// 82		// 2800-28ff
#define UNICODE_RANGE_MASK_YI									(1 << 19)		// 83		// a000-a48f, A490-A4CF (Yi Radicals)
#define UNICODE_RANGE_MASK_PHILIPINO							(1 << 20)		// 84		// 1700-171F (Tagalog), 1720-173F (Hanunoo), 1740-175F (Buhid), 1760-177F (Tagbanwa)

#define UNICODE_CODEPAGE_MASK_LATIN_1							(1 << 0)
#define UNICODE_CODEPAGE_MASK_LATIN_2							(1 << 1)
#define UNICODE_CODEPAGE_MASK_CYRILLIC							(1 << 2)
#define UNICODE_CODEPAGE_MASK_GREEK								(1 << 3)
#define UNICODE_CODEPAGE_MASK_TURKISH							(1 << 4)
#define UNICODE_CODEPAGE_MASK_HEBREW							(1 << 5)
#define UNICODE_CODEPAGE_MASK_ARABIC							(1 << 6)
#define UNICODE_CODEPAGE_MASK_BALTIC							(1 << 7)
#define UNICODE_CODEPAGE_MASK_VIETNAMESE						(1 << 8)
#define UNICODE_CODEPAGE_MASK_THAI								(1 << 16)
#define UNICODE_CODEPAGE_MASK_JAPANESE							(1 << 17)
#define UNICODE_CODEPAGE_MASK_SIMPLIFIEDCHINESE					(1 << 18)
#define UNICODE_CODEPAGE_MASK_KOREAN							(1 << 19)
#define UNICODE_CODEPAGE_MASK_TRADITIONALCHINESE				(1 << 20)

#endif //_MAC_FONT_INFO_H
