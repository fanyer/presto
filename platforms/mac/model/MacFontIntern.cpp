/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/FontInfo.h"
#include "platforms/mac/model/MacFontIntern.h"
#include "platforms/mac/pi/MacOpFont.h"
#include "platforms/mac/pi/MacOpFontManager.h"
#include "platforms/mac/model/UnicodeRangeDetectors.h"
#include "platforms/mac/util/CTextConverter.h"

extern BOOL gInternalFontSwitching;

#if defined(_NEW_FONTSWITCHING_)

void MacFontIntern::SortFonts()
{
#if defined(REARRANGE_FONTS_BY_SUITABLITY)
	OP_NEW_DBG("MacFontIntern::SortFonts()", "Fonts");
	
	InternalFontID normal_font_sort_list[] = { LATIN_FONT_IDS, JAPANESE_FONT_IDS, TRADITIONAL_CN_FONT_IDS, SIMPLIFIED_CN_FONT_IDS, KOREAN_FONT_IDS, OTHER_FONT_IDS, kInternalFontIDUnknown };
	InternalFontID japanese_font_sort_list[] = { JAPANESE_FONT_IDS, LATIN_FONT_IDS, TRADITIONAL_CN_FONT_IDS, SIMPLIFIED_CN_FONT_IDS, KOREAN_FONT_IDS, OTHER_FONT_IDS, kInternalFontIDUnknown };
	InternalFontID korean_font_sort_list[] = { KOREAN_FONT_IDS, LATIN_FONT_IDS, JAPANESE_FONT_IDS, TRADITIONAL_CN_FONT_IDS, SIMPLIFIED_CN_FONT_IDS, OTHER_FONT_IDS, kInternalFontIDUnknown };
	InternalFontID traditional_font_sort_list[] = { TRADITIONAL_CN_FONT_IDS, LATIN_FONT_IDS, JAPANESE_FONT_IDS, SIMPLIFIED_CN_FONT_IDS, KOREAN_FONT_IDS, OTHER_FONT_IDS, kInternalFontIDUnknown };
	InternalFontID simplified_font_sort_list[] = { SIMPLIFIED_CN_FONT_IDS, LATIN_FONT_IDS, JAPANESE_FONT_IDS, TRADITIONAL_CN_FONT_IDS, KOREAN_FONT_IDS, OTHER_FONT_IDS, kInternalFontIDUnknown };
	InternalFontID const * font_sort_list = normal_font_sort_list;
	
	switch (mSystemLanguage)
	{
		case kSystemLanguageJapanese:
			font_sort_list = japanese_font_sort_list;
			break;
		case kSystemLanguageKorean:
			font_sort_list = korean_font_sort_list;
			break;
		case kSystemLanguageSimplifiedChinese:
			font_sort_list = simplified_font_sort_list;
			break;
		case kSystemLanguageTraditionalChinese:
			font_sort_list = traditional_font_sort_list;
			break;
		case kSystemLanguageUnknown:
		default:
			break;
	}
	
	int i = 0;
	int j;
	int index = i;
	int count = fontList.GetCount();
	while (font_sort_list[i] && (i < count))
	{
		InternalFontItem *wrongItem = fontList.Get(i);
		for (j = 0; j < count; j++)
		{
			InternalFontItem *rightItem = fontList.Get(j);
			if (rightItem && rightItem->internalID == font_sort_list[i])
			{
				OP_ASSERT((i != j) || (rightItem == wrongItem));
				fontList.Replace(i, rightItem);
				fontList.Replace(j, wrongItem);
				index++;
				break;
			}
		}
		OP_DBG((UNI_L("Font %d - %s"), i, ((InternalFontItem*)fontList.Get(i))->name));
		i++;
	}
	
#ifdef _DEBUG
	for (i = 0; i < count; i++)
#endif
		OP_DBG((UNI_L("Font %d - %s"), i, ((InternalFontItem*)fontList.Get(i))->name));

#endif // REARRANGE_FONTS_BY_SUITABLITY	
}

void MacFontIntern::ParseCMAPTables(const ATSFontRef font, uint32 **characterMap)
{
	*characterMap = NULL;

	ByteCount length;
	OSErr err = ATSFontGetTable(font, FOUR_CHAR_CODE('cmap'), 0, 0, NULL, &length);
	if (err == noErr)
	{
		unsigned char *buffer = OP_NEWA(unsigned char, length);
		if (buffer)
		{
			err = ATSFontGetTable(font, FOUR_CHAR_CODE('cmap'), 0, length, buffer, &length);
			if (err == noErr)
			{
				ProcessCMAP(characterMap, buffer, length);
			}
			OP_DELETEA(buffer);
		}
	}
}

void MacFontIntern::ParseCMAPTables(const CTFontRef font, uint32 **characterMap)
{
	*characterMap = NULL;

	CFIndex length;
	CFDataRef data = CTFontCopyTable(font, kCTFontTableCmap, kCTFontTableOptionNoOptions);
	if (data)
	{
		length = CFDataGetLength(data);
		unsigned char *buffer = OP_NEWA(unsigned char, length);
		if (buffer)
		{
			CFDataGetBytes(data, CFRangeMake(0,length), buffer);
			CFRelease(data);
			ProcessCMAP(characterMap, buffer, length);
			OP_DELETEA(buffer);
		}
	}
}

void MacFontIntern::ProcessCMAP(uint32 **characterMap, unsigned char *buffer, int length)
{
	// Skip a short
	uint16 *current = (uint16 *)buffer;
	current++;
	uint16 subtableCount = FONT_TO_NATIVE_16(*current++);
	
	BOOL appleFound = FALSE;
	uint16 appleEncoding = 0;
	uint32 appleOffset = 0;
	BOOL microsoftFound = FALSE;
	uint16 microsoftEncoding = 0;
	uint32 microsoftOffset = 0;
	BOOL unicodeFound = FALSE;
	uint16 unicodeEncoding = 0;
	uint32 unicodeOffset = 0;
	
	for (int i=0; i<subtableCount; i++)
	{
		uint16 platformID = FONT_TO_NATIVE_16(*current++);
		uint16 platformEncoding = FONT_TO_NATIVE_16(*current++);
		uint32 *temp = (uint32 *)current;
		uint32 offset = FONT_TO_NATIVE_32(*temp++);
		current = (uint16 *)temp;
		
		if (platformID == PlatformIDApple)
		{
			appleFound = TRUE;
			appleEncoding = platformEncoding;
			appleOffset = offset;
		}
		else if (platformID == PlatformIDMicrosoft)
		{
			microsoftFound = TRUE;
			microsoftEncoding = platformEncoding;
			microsoftOffset = offset;
		}
		else if (platformID == PlatformIDUnicode)
		{
			unicodeFound = TRUE;
			unicodeEncoding = platformEncoding;
			unicodeOffset = offset;
		}
	}
	
	if (unicodeFound)
	{
		unsigned char *unicodeData = buffer + unicodeOffset;
		current = (uint16 *)unicodeData;
		
		uint16 unicodeFormat = FONT_TO_NATIVE_16(*current++);
		
		if (unicodeFormat == UnicodeFormatSegmentLookup)
		{
			/*uint16 subtablelength = FONT_TO_NATIVE_16*/(*current++);
			/*uint16 language = FONT_TO_NATIVE_16*/(*current++);
			uint16 segmentCountX2 = FONT_TO_NATIVE_16(*current++);
			/*uint16 searchRange = FONT_TO_NATIVE_16*/(*current++);
			/*uint16 entrySelector = FONT_TO_NATIVE_16*/(*current++);
			/*uint16 rangeShift = FONT_TO_NATIVE_16*/(*current++);
			
			uint16 segmentCount = segmentCountX2 / 2;
			
			uint16 *endCode = current;
			uint16 *startCode = endCode + segmentCount + 1;
			//uint16 *idDelta = startCode + segmentCount;
			//uint16 *idRangeOffset = idDelta + segmentCount;
			//uint16 *glyphIndexArray = idRangeOffset + segmentCount;
			
			*characterMap = OP_NEWA(uint32, 2048);
			memset(*characterMap, 0, 2048 * sizeof(uint32));
			
			for (int i=0; i<segmentCount; i++)
			{
				uint16 start = FONT_TO_NATIVE_16(startCode[i]);
				uint16 end = FONT_TO_NATIVE_16(endCode[i]);
				
				for (uint32 ch = start; ch <= end; ch++)
				{
					(*characterMap)[ch >> 5] |= 1 << (ch & 0x1F);
				}
			}
		}
		else if (unicodeFormat == UnicodeFormatSegmentedCoverage)
		{
			uint32* here = (uint32*)(unicodeData+12);
			uint32 groups = FONT_TO_NATIVE_32(*here++);
			
			*characterMap = OP_NEWA(uint32, 2048);
			memset(*characterMap, 0, 2048 * sizeof(uint32));
			
			for (uint32 i=0; i<groups; i++)
			{
				uint32 start = FONT_TO_NATIVE_32(*here++);
				uint32 end = FONT_TO_NATIVE_32(*here++);
				/*uint32 startGlyph = FONT_TO_NATIVE_32*/(*here++);
				
				for (uint32 ch = start; ch <= end; ch++)
				{
					if (!(ch&0xffff0000))
						(*characterMap)[ch >> 5] |= 1 << (ch & 0x1F);
				}
			}
		}
		else
		{
			OP_ASSERT(!"Unsupported unicode format!!!");
			return;
		}
	}
	else if (microsoftFound)
	{
		if (microsoftEncoding == MicrosoftStandardEncoding)
		{
			byte *raw_table = (byte *)buffer;
			byte *raw_table_end = raw_table + length;
			
			CMapGlyphTable * sub_table = (CMapGlyphTable *)(raw_table + microsoftOffset);
			
			if (sub_table && (byte *)(sub_table+1) <= raw_table_end)
			{
				if (B2LENDIAN(sub_table->format) == 4)
				{
					*characterMap = OP_NEWA(uint32, 2048);
					memset(*characterMap, 0, 2048 * sizeof(uint32));
					
					uint16 segCountX2 = B2LENDIAN(sub_table->f4.segCountX2);
					
					uint16 * endCode         = (uint16 *)&(sub_table->f4.rest);
					uint16 * startCode       = (uint16 *)(&(sub_table->f4.rest) + segCountX2 +2);
					//uint16 * idDelta         = (uint16 *)(&(sub_table->f4.rest) + segCountX2*2 +2);
					//uint16 * idRangeOffset   = (uint16 *)(&(sub_table->f4.rest) + segCountX2*3 +2);
					//uint16 * glyphIndexArray = (uint16 *)(&(sub_table->f4.rest) + segCountX2*4 +2);
					//uint16 * glyphIndex      = NULL;
					
					int segment = 0;
					
					while (true)
					{
						if ((byte*)&startCode[segment+1] > raw_table_end)
							break;
						
						uint16 endc = B2LENDIAN(endCode[segment]), k;
						if (endc == 0xFFFF)
							endc--;
						
						for (k=B2LENDIAN(startCode[segment]); k<=endc; k++)  // Set a bit for all characters in segments
						{
							int i = k / 32;
							int j = k % 32;
							OP_ASSERT(i < 2048);
							if (i < 2048)
							{
								(*characterMap)[i] = (*characterMap)[i] | (1 << j);
							}
						}
						
						if (endc == 0xFFFE)
							break;
						
						segment++;
					}
				}
			}
		}
	}
}

#endif // _NEW_FONTSWITCHING_

BOOL MacFontIntern::GetSerifs(InternalFontID font_id, OpFontInfo::FontSerifs &serif)
{
	switch(font_id)
	{
		case kInternalFontIDTimes:
		case kInternalFontIDTimesNewRoman:
		case kInternalFontIDHiraginoMincho:
		case kInternalFontIDHiraginoMinchoProW6:
		case kInternalFontIDAmericanTypewriter:
		case kInternalFontIDAmericanTypewriterCondensed:
		case kInternalFontIDAmericanTypewriterLight:
		case kInternalFontIDAmericanTypewriterCondensedLight:
		case kInternalFontIDAppleChancery:
		case kInternalFontIDAppleLiSung:
		case kInternalFontIDAppleMyungjo:
		case kInternalFontIDBaskerville:
		case kInternalFontIDBaskervilleSemibold:
		case kInternalFontIDBrushScriptMT:
		case kInternalFontIDCourier:
		case kInternalFontIDCourierNew:
		case kInternalFontIDGeorgia:
		case kInternalFontIDHoeflerText:
		case kInternalFontIDPlantagenetCherokee:
		case kInternalFontIDZapfino:
		case kInternalFontIDLiSong:
		case kInternalFontIDKai:
		case kInternalFontIDBigCaslon:
		case kInternalFontIDCochin:
		case kInternalFontIDCopperplate:
		case kInternalFontIDCopperplateLight:
		case kInternalFontIDDevanagariMT:
		case kInternalFontIDDevanagariMTBold:
		case kInternalFontIDDidot:
		case kInternalFontIDSong:
		case kInternalFontIDFangSong:
		case kInternalFontIDSTFangSong:
		case kInternalFontIDMarkerFelt:
		case kInternalFontIDSTSong:
		case kInternalFontIDSymbol:
		case kInternalFontIDHerculanum:
		{
			serif = OpFontInfo::WithSerifs;
			return true;
		}
		case kInternalFontIDAppleLiGothic:
		case kInternalFontIDAppleGothic:
		case kInternalFontIDGeneva:
		case kInternalFontIDGenevaCY:
		case kInternalFontIDLucidaGrande:
		case kInternalFontIDOsaka:
		case kInternalFontIDOsakaMono:
		case kInternalFontIDHiraginoKakuGothic:
		case kInternalFontIDHiraginoKakuGothicProW6:
		case kInternalFontIDHiraginoKakuGothicStdW8:
		case kInternalFontIDHiraginoMaruGothicProW4:
		case kInternalFontIDHei:
		case kInternalFontIDLiHei:
		case kInternalFontIDSTHeiti:
		case kInternalFontIDSTHeitiLight:
		case kInternalFontIDAndaleMono:
		case kInternalFontIDArial:
		case kInternalFontIDArialBlack:
		case kInternalFontIDArialNarrow:
		case kInternalFontIDArialHebrew:
		case kInternalFontIDArialHebrewBold:
		case kInternalFontIDArialRoundedMTBold:
		case kInternalFontIDEuphemiaUCASBold:
		case kInternalFontIDEuphemiaUCASItalic:
		case kInternalFontIDEuphemiaUCAS:
		case kInternalFontIDFuturaCondensed:
		case kInternalFontIDGillSans:
		case kInternalFontIDGillSansLight:
		case kInternalFontIDHelvetica:
		case kInternalFontIDHelveticaCY:
		case kInternalFontIDHelveticaNeue:
		case kInternalFontIDHelveticaNeueBoldCondensed:
		case kInternalFontIDHelveticaNeueUltraLight:
		case kInternalFontIDHelveticaNeueLight:
		case kInternalFontIDHelveticaNeueBlackCondensed:
		case kInternalFontIDImpact:
		case kInternalFontIDSkia:
		case kInternalFontIDTrebuchetMS:
		case kInternalFontIDVerdana:
		case kInternalFontIDAyuthaya:
		case kInternalFontIDChalkboard:
		case kInternalFontIDChalkboardBold:
		case kInternalFontIDCharcoalCY:
		case kInternalFontIDComicSansMS:
		case kInternalFontIDCorsivaHebrew:
		case kInternalFontIDCorsivaHebrewBold:
		case kInternalFontIDFutura:
		case kInternalFontIDInaiMathi:
		case kInternalFontIDKrungthep:
		case kInternalFontIDMonaco:
		case kInternalFontIDOptima:
		case kInternalFontIDOptimaExtraBlack:
		case kInternalFontIDSathu:
		case kInternalFontIDSilom:
		case kInternalFontIDThonburi:
		case kInternalFontIDSTXihei:
		case kInternalFontIDHeitiSCLight:
		case kInternalFontIDHeitiTCLight:
		{
			serif = OpFontInfo::WithoutSerifs;
			return true;
		}
		case kInternalFontIDGeezaPro:
		case kInternalFontIDGeezaProBold:
		case kInternalFontIDLastResort:
		case kInternalFontIDAlBayanPlain:
		case kInternalFontIDAlBayanBold:
		case kInternalFontIDAppleSymbols:
		case kInternalFontIDBaghdad:
		case kInternalFontIDDecoTypeNaskh:
		case kInternalFontIDGujaratiMT:
		case kInternalFontIDGujaratiMTBold:
		case kInternalFontIDGurmukhiMT:
		case kInternalFontIDHoeflerTextOrnaments:
		case kInternalFontIDKufiStandardGK:
		case kInternalFontIDMshtakan:
		case kInternalFontIDMshtakanBold:
		case kInternalFontIDMshtakanBoldOblique:
		case kInternalFontIDMshtakanOblique:
		case kInternalFontIDNadeem:
		case kInternalFontIDNewPeninimMT:
		case kInternalFontIDNewPeninimMTBold:
		case kInternalFontIDNewPeninimMTBoldInclined:
		case kInternalFontIDNewPeninimMTInclined:
		case kInternalFontIDGB18030Bitmap:
		case kInternalFontIDRaanana:
		case kInternalFontIDRaananaBold:
		case kInternalFontIDWebdings:
		case kInternalFontIDZapfDingbats:
		case kInternalFontIDBiauKai:
		case kInternalFontIDSTKaiti:
		case kInternalFontIDPapyrus:
		{
			serif = OpFontInfo::UnknownSerifs;
			return true;
		}
	}
	return false;
}

void MacFontIntern::GenerateUsb(FONTSIGNATURE &fontSig, const uint32 *characterMap)
{
	memset(&fontSig, 0, sizeof(FONTSIGNATURE));
	if (characterMap)
	{
		if(UnicodeRangeDetectors::hasLatin1(characterMap))
		{
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_LATIN1 | UNICODE_RANGE_MASK_GENERALPUNCTUATION;
			fontSig.fsCsb[0] |= UNICODE_CODEPAGE_MASK_LATIN_1;
		}

		if(UnicodeRangeDetectors::hasLatin1Supplement(characterMap))
		{
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_LATIN1SUPPLEMENT;
			fontSig.fsCsb[0] |= UNICODE_CODEPAGE_MASK_LATIN_2;
		}

		if(UnicodeRangeDetectors::hasLatinExtendedA(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_LATINEXTENDEDA;

		if(UnicodeRangeDetectors::hasLatinExtendedB(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_LATINEXTENDEDB;

		if(UnicodeRangeDetectors::hasIPAExtensions(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_IPAEXTENSIONS;

		if(UnicodeRangeDetectors::hasSpacingModifiedLetters(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_SPACINGMODIFIEDLETTERS;

		if(UnicodeRangeDetectors::hasCombiningDiacriticalMarks(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_COMBININGDIACRITICALMARKS;

		if(UnicodeRangeDetectors::hasGreek(characterMap))
		{
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_GREEK;
			fontSig.fsCsb[0] |= UNICODE_CODEPAGE_MASK_GREEK;
		}

		if(UnicodeRangeDetectors::hasCyrillic(characterMap))
		{
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_CYRILLIC;
			fontSig.fsCsb[0] |= UNICODE_CODEPAGE_MASK_CYRILLIC;
		}

		if(UnicodeRangeDetectors::hasArmenian(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_ARMENIAN;

		if(UnicodeRangeDetectors::hasHebrew(characterMap))
		{
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_HEBREW;
			fontSig.fsCsb[0] |= UNICODE_CODEPAGE_MASK_HEBREW;
		}

		if(UnicodeRangeDetectors::hasArabic(characterMap))
		{
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_ARABIC;
			fontSig.fsCsb[0] |= UNICODE_CODEPAGE_MASK_ARABIC;
		}

		if(UnicodeRangeDetectors::hasDevanagari(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_DEVANAGARI;

		if(UnicodeRangeDetectors::hasBengali(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_BENGALI;

		if(UnicodeRangeDetectors::hasGurmukhi(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_GURMUKHI;

		if(UnicodeRangeDetectors::hasGujarati(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_GUJARATI;

		if(UnicodeRangeDetectors::hasOriya(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_ORIYA;

		if(UnicodeRangeDetectors::hasTamil(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_TAMIL;

		if(UnicodeRangeDetectors::hasTelugu(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_TELUGU;

		if(UnicodeRangeDetectors::hasKannada(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_KANNADA;

		if(UnicodeRangeDetectors::hasMalayalam(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_MALAYALAM;

		if(UnicodeRangeDetectors::hasThai(characterMap))
		{
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_THAI;
			fontSig.fsCsb[0] |= UNICODE_CODEPAGE_MASK_THAI;
		}

		if(UnicodeRangeDetectors::hasLao(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_LAO;

		if(UnicodeRangeDetectors::hasBasicGeorgian(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_BASICGEORGIAN;

		if(UnicodeRangeDetectors::hasHangulJamo(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_HANGULJAMO;

		if(UnicodeRangeDetectors::hasLatinExtendedAdditional(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_LATINEXTENDEDADDITIONAL;

		if(UnicodeRangeDetectors::hasGreekExtended(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_GREEKEXTENDED;

		if(UnicodeRangeDetectors::hasGeneralPunctuation(characterMap))
			fontSig.fsUsb[0] |= UNICODE_RANGE_MASK_GENERALPUNCTUATION;

		if(UnicodeRangeDetectors::hasSuperscriptsAndSubscripts(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_SUPERSCRIPTSANDSUBSCRIPTS;

		if(UnicodeRangeDetectors::hasCurrencySymbols(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_CURRENCYSYMBOLS;

		if(UnicodeRangeDetectors::hasCombiningDiacriticalMarksForSymbols(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_COMBININGDIACRITICALMARKSFORSYMBOLS;

		if(UnicodeRangeDetectors::hasLetterLikeSymbols(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_LETTERLIKESYMBOLS;

		if(UnicodeRangeDetectors::hasNumberForms(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_NUMBERFORMS;

		if(UnicodeRangeDetectors::hasArrows(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_ARROWS;

		if(UnicodeRangeDetectors::hasMathematicalOperators(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_MATHEMATICALOPERATORS;

		if(UnicodeRangeDetectors::hasMiscellaneousTechnical(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_MISCELLANEOUSTECHNICAL;

		if(UnicodeRangeDetectors::hasControlPictures(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_CONTROLPICTURES;

		if(UnicodeRangeDetectors::hasOpticalCharacterRecognition(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_OPTICALCHARACTERRECOGNITION;

		if(UnicodeRangeDetectors::hasEnclosedAlphanumerics(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_ENCLOSEDALPHANUMERICS;

		if(UnicodeRangeDetectors::hasBoxDrawing(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_BOXDRAWING;

		if(UnicodeRangeDetectors::hasBlockElements(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_BLOCKELEMENTS;

		if(UnicodeRangeDetectors::hasBlockGeometricShapes(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_GEOMETRICSHAPES;

		if(UnicodeRangeDetectors::hasMiscellaneousSymbols(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_MISCELLANEOUSSYMBOLS;

		if(UnicodeRangeDetectors::hasDingbats(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_DINGBATS;

		if(UnicodeRangeDetectors::hasCJKSymbolsAndPunctuation(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_CJKSYMBOLSANDPUNCTUATION;

		if(UnicodeRangeDetectors::hasHiragana(characterMap))
		{
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_HIRAGANA;
			fontSig.fsCsb[0] |= UNICODE_CODEPAGE_MASK_JAPANESE;
		}

		if(UnicodeRangeDetectors::hasKatakana(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_KATAKANA;

		if(UnicodeRangeDetectors::hasBopomofo(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_BOPOMOFO;

		if(UnicodeRangeDetectors::hasHangulCompatibilityJamo(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_HANGULCOMPATIBILITYJAMO;

		if(UnicodeRangeDetectors::hasCJKMiscellaneous(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_CJKMISCELLANEOUS;

		if(UnicodeRangeDetectors::hasEnclosedCJKLettersAndMonths(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_ENCLOSEDCJKLETTERSANDMONTHS;

		if(UnicodeRangeDetectors::hasCJKCompatibility(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_CJKCOMPATIBILITY;

		if(UnicodeRangeDetectors::hasHangul(characterMap))
		{
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_HANGUL;
			fontSig.fsCsb[0] |= UNICODE_CODEPAGE_MASK_KOREAN;
		}

		if(UnicodeRangeDetectors::hasCJKUnifiedIdeographs(characterMap))
		{
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_CJKUNIFIEDIDEOGRAPHS;
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_BOPOMOFO;

			if(UnicodeRangeDetectors::isSimplified(characterMap))
				fontSig.fsCsb[0] |= UNICODE_CODEPAGE_MASK_SIMPLIFIEDCHINESE;

			if(UnicodeRangeDetectors::isTraditional(characterMap))
				fontSig.fsCsb[0] |= UNICODE_CODEPAGE_MASK_TRADITIONALCHINESE;
		}
		if(UnicodeRangeDetectors::hasPrivateUseArea(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_PRIVATEUSEAREA;

		if(UnicodeRangeDetectors::hasCJKCompatibilityIdeographs(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_CJKCOMPATIBILITYIDEOGRAPHS;

		if(UnicodeRangeDetectors::hasAlphabeticPresentationForms(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_ALPHABETICPRESENTATIONFORMS;

		if(UnicodeRangeDetectors::hasArabicPresentationFormsA(characterMap))
			fontSig.fsUsb[1] |= UNICODE_RANGE_MASK_ARABICPRESENTATIONFORMSA;

		if(UnicodeRangeDetectors::hasCombiningHalfMarks(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_COMBININGHALFMARKS;

		if(UnicodeRangeDetectors::hasCJKCompatibilityForms(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_CJKCOMPATIBILITYFORMS;

		if(UnicodeRangeDetectors::hasSmallFormVariants(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_SMALLFORMVARIANTS;

		if(UnicodeRangeDetectors::hasArabicPresentationFormsB(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_ARABICPRESENTATIONFORMSB;

		if(UnicodeRangeDetectors::hasHalfwidthAndFullwidthForms(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_HALFWIDTHANDFULLWIDTHFORMS;

		if(UnicodeRangeDetectors::hasTibetian(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_TIBETIAN;

		if(UnicodeRangeDetectors::hasSyriac(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_SYRIAC;

		if(UnicodeRangeDetectors::hasThaana(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_THAANA;

		if(UnicodeRangeDetectors::hasSinhala(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_SINHALA;

		if(UnicodeRangeDetectors::hasMyanmar(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_MYANMAR;

		if(UnicodeRangeDetectors::hasEthiopic(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_ETHIOPIC;

		if(UnicodeRangeDetectors::hasCherokee(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_CHEROKEE;

		if(UnicodeRangeDetectors::hasCanadianAboriginalSyllabics(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_CANADIANABORIGINALSYLLABICS;

		if(UnicodeRangeDetectors::hasOgham(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_OGHAM;

		if(UnicodeRangeDetectors::hasRunic(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_RUNIC;

		if(UnicodeRangeDetectors::hasKhmer(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_KHMER;

		if(UnicodeRangeDetectors::hasMongolian(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_MONGOLIAN;

		if(UnicodeRangeDetectors::hasBraille(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_BRAILLE;

		if(UnicodeRangeDetectors::hasYi(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_YI;

		if(UnicodeRangeDetectors::hasPhilipino(characterMap))
			fontSig.fsUsb[2] |= UNICODE_RANGE_MASK_PHILIPINO;
	}
}

uint32 * MacFontIntern::GetCharacterMap(UINT32 index)
{
	uint32 *characterMap = NULL;
	InternalFontItem *fontItem = fontList.Get(index);
	if (!fontItem)
		return NULL;

#ifdef _GLYPHTESTING_SUPPORT_
	if (fontItem->characterMap)
		return fontItem->characterMap;

	Boolean checkFontAnyway = ((!gInternalFontSwitching) &&	(
		(uni_stricmp(fontItem->name, FONT_TIMES) == 0) ||
		(uni_stricmp(fontItem->name, FONT_LUCIDA_GRANDE) == 0) ||
		(uni_strnicmp(fontItem->name, FONT_HIRAGINO_GOTHIC, 13) == 0) ||
		(uni_strnicmp(fontItem->name, FONT_HIRAGINO_MINCHO, 13) == 0) ||
		(uni_stricmp(fontItem->name, FONT_APPLE_MYUNGJO) == 0) ||
		(uni_strnicmp(fontItem->name, FONT_ST_HEITI, 4) == 0) ));

	if (gInternalFontSwitching || checkFontAnyway)
	{
		if (fontItem->ctFontRef)
			ParseCMAPTables(fontItem->ctFontRef, &characterMap);
		if (fontItem->atsFontRef && !characterMap)
			ParseCMAPTables(fontItem->atsFontRef, &characterMap);
	}
	GenerateUsb(fontItem->signature, characterMap);
	fontItem->characterMap = characterMap;
#endif

	return characterMap;
}

OpFontInfo::FontSerifs MacFontIntern::GetHasSerifs(UINT32 index)
{
	OP_ASSERT(index >= 0 && index < Count());
	InternalFontItem * fontItem = fontList.Get(index);
	if (!fontItem)
		return OpFontInfo::UnknownSerifs;
#ifdef LOAD_SERIFS_ON_DEMAND
	if (!fontItem->serifsCalculated)
	{
		fontItem->serifsCalculated = true;
		Boolean checkFontAnyway = ((!gInternalFontSwitching) &&	(
			(uni_stricmp(fontItem->name, TIMES_FONT_NAME) == 0) ||
			(uni_stricmp(fontItem->name, LUCIDA_GRANDE_FONT_NAME) == 0) ||
			(uni_strnicmp(fontItem->name, HIRAGINO_GOTHIC_FONT_NAME, 13) == 0) ||
			(uni_strnicmp(fontItem->name, HIRAGINO_MINCHO_FONT_NAME, 13) == 0) ||
			(uni_stricmp(fontItem->name, APPLE_MYUNGJO_FONT_NAME) == 0) ||
			(uni_strnicmp(fontItem->name, ST_HEITI_FONT_NAME, 4) == 0) ));
		Boolean isSerif;
		if (gInternalFontSwitching || checkFontAnyway) {
			if (fontItem->ctFontRef && ParseOS2Tag(fontItem->ctFontRef, isSerif) || !fontItem->ctFontRef && ParseOS2Tag(fontItem->atsFontRef, isSerif)) {
				if(isSerif)
					fontItem->serifs = OpFontInfo::WithSerifs;
				else
					fontItem->serifs = OpFontInfo::WithoutSerifs;
			} else {
				if ((uni_strnicmp(fontItem->name, UNI_L("Times"), 5) == 0) ||
					(uni_strnicmp(fontItem->name, UNI_L("New York"), 8) == 0) ||
					(uni_strnicmp(fontItem->name, UNI_L("Georgia"), 7) == 0) ||
					(uni_strnicmp(fontItem->name, APPLE_MYUNGJO_FONT_NAME, 7) == 0) ||
					(uni_strnicmp(fontItem->name, ST_KAITI_FONT_NAME, 5) == 0) ||
					(uni_strstr(fontItem->name, UNI_L("Sung"))) ||
					(uni_strstr(fontItem->name, UNI_L("Mincho"))) ||
					(uni_strstr(fontItem->name, UNI_L("\x660E\x671D"))) )		// Mincho
					fontItem->serifs = OpFontInfo::WithSerifs;
				else if ((uni_strnicmp(fontItem->name, LUCIDA_GRANDE_FONT_NAME, 13) == 0) ||
					(uni_strnicmp(fontItem->name, UNI_L("Arial"), 5) == 0) ||
					(uni_strnicmp(fontItem->name, UNI_L("Geneva"), 6) == 0) ||
					(uni_strnicmp(fontItem->name, UNI_L("Helvetica"), 9) == 0) ||
					(uni_strnicmp(fontItem->name, UNI_L("Verdana"), 7) == 0) ||
					(uni_strnicmp(fontItem->name, UNI_L("Osaka"), 5) == 0) ||
					(uni_strnicmp(fontItem->name, ST_HEITI_FONT_NAME, 5) == 0) ||
					(uni_strstr(fontItem->name, UNI_L("Gothic"))) ||
					(uni_strstr(fontItem->name, UNI_L("\x89D2\x30B4"))) )	// Kaku Gothic
					fontItem->serifs = OpFontInfo::WithoutSerifs;
				else
					fontItem->serifs = OpFontInfo::UnknownSerifs;
			}
		}
	}
#endif
	return fontItem->serifs;
}

BOOL MacFontIntern::ParseOS2Tag(const ATSFontRef font, BOOL &isSerif)
{
	ByteCount length;
	OSErr err = ATSFontGetTable(font, 'OS/2', 0, 0, NULL, &length);
	if (err == noErr)
	{
		if (sizeof(OS2FontTable) < length)
			return false;

		OS2FontTable table;

		err = ATSFontGetTable(font, 'OS/2', 0, length, &table, &length);
		if (err == noErr)
		{
			if (table.panose.serifStyle > kPanoseSerifStyleNoFit) {
				if ((table.panose.serifStyle != kPanoseSerifStyleNormalSans) &&
					(table.panose.serifStyle != kPanoseSerifStyleObtuseSans) &&
					(table.panose.serifStyle != kPanoseSerifStylePerpSans))
					isSerif = true;
				else
					isSerif = false;

				return true;
			}
		}
	}

	return false;
}

BOOL MacFontIntern::ParseOS2Tag(const CTFontRef font, BOOL &isSerif)
{
	CFIndex length;
	CFDataRef data = CTFontCopyTable(font, kCTFontTableOS2, kCTFontTableOptionNoOptions);
	if (data)
	{
		OS2FontTable table;
		length = CFDataGetLength(data);
		if (sizeof(OS2FontTable) < length)
			return false;

		CFDataGetBytes(data, CFRangeMake(0,length), (UInt8*)&table);

		if (table.panose.serifStyle > kPanoseSerifStyleNoFit) {
			if ((table.panose.serifStyle != kPanoseSerifStyleNormalSans) &&
				(table.panose.serifStyle != kPanoseSerifStyleObtuseSans) &&
				(table.panose.serifStyle != kPanoseSerifStylePerpSans))
				isSerif = true;
			else
				isSerif = false;

			return true;
		}
	}

	return false;
}
