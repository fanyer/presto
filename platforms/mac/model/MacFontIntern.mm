/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#define BOOL NSBOOL
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#undef BOOL

#include "platforms/mac/util/FontInfo.h"
#include "platforms/mac/model/MacFontIntern.h"

extern BOOL gInternalFontSwitching;

#if defined(_NEW_FONTSWITCHING_)

// Store only the base version of fonts for various dubious Microsoft fonts?
// This was a needed fix (see DSK-338003) for the old font enumeration code
#define STORE_ONLY_BASE_VERSION_OF_FONTS 0

// Used to give a ID for all the common font names, so we don't have to use string compare all the time.
InternalFontID GetInternalFontIDFromName(const uni_char* name)
{
	int len = uni_strlen(name);
	switch (len)
	{
		case 3:
			if (0 == uni_strcmp(name, UNI_L("Hei")))
				return kInternalFontIDHei;
			if (0 == uni_strcmp(name, UNI_L("Kai")))
				return kInternalFontIDKai;
			break;
		case 4:
			if (0 == uni_strcmp(name, UNI_L("Sand")))
				return kInternalFontIDSand;
			if (0 == uni_strcmp(name, UNI_L("Skia")))
				return kInternalFontIDSkia;
			if (0 == uni_strcmp(name, UNI_L("Song")))
				return kInternalFontIDSong;
#ifdef USE_FAMILY_NAME
			if (0 == uni_strcmp(name, UNI_L("\x534E\x6587\x4EFF\x5B8B")))
				return kInternalFontIDSTFangSong;
			if (0 == uni_strcmp(name, UNI_L("\x534E\x6587\x5B8B\x4F53")))
				return kInternalFontIDSTSong;
#endif
			break;
		case 5:
			if (0 == uni_strcmp(name, UNI_L("Arial")))
				return kInternalFontIDArial;
			if (0 == uni_strcmp(name, UNI_L("Times")))
				return kInternalFontIDTimes;
			if (0 == uni_strcmp(name, UNI_L("Times Roman")))
				return kInternalFontIDTimesRoman;
			if (0 == uni_strcmp(name, UNI_L("Osaka")))
				return kInternalFontIDOsaka;
			if (0 == uni_strcmp(name, UNI_L("Didot")))
				return kInternalFontIDDidot;
			if (0 == uni_strcmp(name, UNI_L("Sathu")))
				return kInternalFontIDSathu;
			if (0 == uni_strcmp(name, UNI_L("Silom")))
				return kInternalFontIDSilom;
			if (0 == uni_strcmp(name, UNI_L("Menlo")))
				return kInternalFontIDAppleMenlo;
			break;
		case 6:
#ifndef USE_FAMILY_NAME
			if (0 == uni_strcmp(name, UNI_L("STSong")))
				return kInternalFontIDSTSong;
#endif
			if (0 == uni_strcmp(name, UNI_L("Geneva")))
				return kInternalFontIDGeneva;
			if (0 == uni_strcmp(name, UNI_L("Monaco")))
				return kInternalFontIDMonaco;
			if (0 == uni_strcmp(name, UNI_L("Symbol")))
				return kInternalFontIDSymbol;
#ifdef USE_FAMILY_NAME
			if (0 == uni_strcmp(name, UNI_L("\x5137\x9ED1 Pro")))
				return kInternalFontIDLiHei;
			if (0 == uni_strcmp(name, UNI_L("\x5137\x5B8B Pro")))
				return kInternalFontIDLiSong;
#endif
			if (0 == uni_strcmp(name, UNI_L("Cochin")))
				return kInternalFontIDCochin;
			if (0 == uni_strcmp(name, UNI_L("Futura")))
				return kInternalFontIDFutura;
			if (0 == uni_strcmp(name, UNI_L("Impact")))
				return kInternalFontIDImpact;
			if (0 == uni_strcmp(name, UNI_L("Nadeem")))
				return kInternalFontIDNadeem;
			if (0 == uni_strcmp(name, UNI_L("Optima")))
				return kInternalFontIDNadeem;
			if (0 == uni_strcmp(name, UNI_L("Tahoma")))
				return kInternalFontIDTahoma;
			break;
		case 7:
#ifndef USE_FAMILY_NAME
			if (0 == uni_strcmp(name, UNI_L("STXihei")))
				return kInternalFontIDSTXihei;
#endif
			if (0 == uni_strcmp(name, UNI_L("Courier")))
				return kInternalFontIDCourier;
			if (0 == uni_strcmp(name, UNI_L("Papyrus")))
				return kInternalFontIDPapyrus;
			if (0 == uni_strcmp(name, UNI_L("STHeiti")))
				return kInternalFontIDSTHeiti;
			if (0 == uni_strcmp(name, UNI_L("STKaiti")))
				return kInternalFontIDSTKaiti;
			if (0 == uni_strcmp(name, UNI_L("Baghdad")))
				return kInternalFontIDBaghdad;
			if (0 == uni_strcmp(name, UNI_L("BiauKai")))
				return kInternalFontIDBiauKai;
			if (0 == uni_strcmp(name, UNI_L("Georgia")))
				return kInternalFontIDGeorgia;
			if (0 == uni_strcmp(name, UNI_L("Raanana")))
				return kInternalFontIDRaanana;
			if (0 == uni_strcmp(name, UNI_L("Verdana")))
				return kInternalFontIDVerdana;
			if (0 == uni_strcmp(name, UNI_L("Zapfino")))
				return kInternalFontIDZapfino;
			break;
		case 8:
			if (0 == uni_strcmp(name, UNI_L("Capitals")))
				return kInternalFontIDCapitals;
			if (0 == uni_strcmp(name, UNI_L("New York")))
				return kInternalFontIDNewYork;
			if (0 == uni_strcmp(name, UNI_L("Mshtakan")))
				return kInternalFontIDMshtakan;
			if (0 == uni_strcmp(name, UNI_L("Ayuthaya")))
				return kInternalFontIDAyuthaya;
			if (0 == uni_strcmp(name, UNI_L("Thonburi")))
				return kInternalFontIDThonburi;
			if (0 == uni_strcmp(name, UNI_L("Webdings")))
				return kInternalFontIDWebdings;
			break;
		case 9:
#ifndef USE_FAMILY_NAME
			if (0 == uni_strcmp(name, UNI_L("LiHei Pro")))
				return kInternalFontIDLiHei;
#endif
			if (0 == uni_strcmp(name, UNI_L("Helvetica")))
				return kInternalFontIDHelvetica;
			if (0 == uni_strcmp(name, UNI_L("Geeza Pro")))
				return kInternalFontIDGeezaPro;
			if (0 == uni_strcmp(name, UNI_L("InaiMathi")))
				return kInternalFontIDInaiMathi;
			if (0 == uni_strcmp(name, UNI_L("Fang Song")))
				return kInternalFontIDFangSong;
			if (0 == uni_strcmp(name, UNI_L("Geneva CY")))
				return kInternalFontIDGenevaCY;
			if (0 == uni_strcmp(name, UNI_L("Gill Sans")))
				return kInternalFontIDGillSans;
			if (0 == uni_strcmp(name, UNI_L("Krungthep")))
				return kInternalFontIDKrungthep;
			break;
		case 10:
#ifndef USE_FAMILY_NAME
			if (0 == uni_strcmp(name, UNI_L("LiSong Pro")))
				return kInternalFontIDLiSong;
#endif
			if (0 == uni_strcmp(name, UNI_L("Osaka-Mono")))
				return kInternalFontIDOsakaMono;
			if (0 == uni_strcmp(name, UNI_L("Big Caslon")))
				return kInternalFontIDBigCaslon;
			if (0 == uni_strcmp(name, UNI_L("Chalkboard")))
				return kInternalFontIDChalkboard;
			if (0 == uni_strcmp(name, UNI_L("Herculanum")))
				return kInternalFontIDHerculanum;
			if (0 == uni_strcmp(name, UNI_L("LastResort")))
				return kInternalFontIDLastResort;
			break;
		case 11:
			if (0 == uni_strcmp(name, UNI_L("Apple Menlo")))
				return kInternalFontIDAppleMenlo;
			if (0 == uni_strcmp(name, UNI_L("Courier New")))
				return kInternalFontIDCourierNew;
			if (0 == uni_strcmp(name, UNI_L("AppleGothic")))
				return kInternalFontIDAppleGothic;
			if (0 == uni_strcmp(name, UNI_L("Gurmukhi MT")))
				return kInternalFontIDGurmukhiMT;
			if (0 == uni_strcmp(name, UNI_L("Gujarati MT")))
				return kInternalFontIDGujaratiMT;
			if (0 == uni_strcmp(name, UNI_L(".LastResort")))
				return kInternalFontIDLastResort;
			if (0 == uni_strcmp(name, UNI_L("Andale Mono")))
				return kInternalFontIDAndaleMono;
			if (0 == uni_strcmp(name, UNI_L("Arial Black")))
				return kInternalFontIDArialBlack;
			if (0 == uni_strcmp(name, UNI_L("Baskerville")))
				return kInternalFontIDBaskerville;
			if (0 == uni_strcmp(name, UNI_L("Charcoal CY")))
				return kInternalFontIDCharcoalCY;
			if (0 == uni_strcmp(name, UNI_L("Copperplate")))
				return kInternalFontIDCopperplate;
			if (0 == uni_strcmp(name, UNI_L("Marker Felt")))
				return kInternalFontIDMarkerFelt;
			break;
		case 12:
			if (0 == uni_strcmp(name, UNI_L("AppleMyungjo")))
				return kInternalFontIDAppleMyungjo;
			if (0 == uni_strcmp(name, UNI_L("Arial Narrow")))
				return kInternalFontIDArialNarrow;
			if (0 == uni_strcmp(name, UNI_L("Arial Hebrew")))
				return kInternalFontIDArialHebrew;
			if (0 == uni_strcmp(name, UNI_L("Helvetica CY")))
				return kInternalFontIDHelveticaCY;
			if (0 == uni_strcmp(name, UNI_L("Hoefler Text")))
				return kInternalFontIDHoeflerText;
			if (0 == uni_strcmp(name, UNI_L("Raanana Bold")))
				return kInternalFontIDRaananaBold;
			if (0 == uni_strcmp(name, UNI_L("Trebuchet MS")))
				return kInternalFontIDTrebuchetMS;
			break;
		case 13:
			if (0 == uni_strcmp(name, UNI_L("Lucida Grande")))
				return kInternalFontIDLucidaGrande;
			if (0 == uni_strcmp(name, UNI_L("Devanagari MT")))
				return kInternalFontIDDevanagariMT;
			if (0 == uni_strcmp(name, UNI_L("Euphemia UCAS")))
				return kInternalFontIDEuphemiaUCAS;
			if (0 == uni_strcmp(name, UNI_L("Comic Sans MS")))
				return kInternalFontIDComicSansMS;
			if (0 == uni_strcmp(name, UNI_L("Zapf Dingbats")))
				return kInternalFontIDZapfDingbats;
			if (0 == uni_strcmp(name, UNI_L("Al Bayan Bold")))
				return kInternalFontIDAlBayanBold;
			if (0 == uni_strcmp(name, UNI_L("Apple Symbols")))
				return kInternalFontIDAppleSymbols;
			if (0 == uni_strcmp(name, UNI_L("Mshtakan Bold")))
				return kInternalFontIDMshtakanBold;
			break;
		case 14:
			if (0 == uni_strcmp(name, UNI_L("Apple Chancery")))
				return kInternalFontIDAppleChancery;
			if (0 == uni_strcmp(name, UNI_L("Geeza Pro Bold")))
				return kInternalFontIDGeezaProBold;
			if (0 == uni_strcmp(name, UNI_L("Al Bayan Plain")))
				return kInternalFontIDAlBayanPlain;
			if (0 == uni_strcmp(name, UNI_L("Corsiva Hebrew")))
				return kInternalFontIDCorsivaHebrew;
			if (0 == uni_strcmp(name, UNI_L("DecoType Naskh")))
				return kInternalFontIDDecoTypeNaskh;
			if (0 == uni_strcmp(name, UNI_L("Helvetica Neue")))
				return kInternalFontIDHelveticaNeue;
			if (0 == uni_strcmp(name, UNI_L("KufiStandardGK")))
				return kInternalFontIDKufiStandardGK;
			if (0 == uni_strcmp(name, UNI_L("New Peninim MT")))
				return kInternalFontIDNewPeninimMT;
			if (0 == uni_strcmp(name, UNI_L("GB18030 Bitmap")))
				return kInternalFontIDGB18030Bitmap;
			if (0 == uni_strcmp(name, UNI_L("Heiti SC Light")))
				return kInternalFontIDHeitiSCLight;
			if (0 == uni_strcmp(name, UNI_L("Heiti TC Light")))
				return kInternalFontIDHeitiTCLight;
			break;
		case 15:
			if (0 == uni_strcmp(name, UNI_L("Times New Roman")))
				return kInternalFontIDTimesNewRoman;
			if (0 == uni_strcmp(name, UNI_L("Brush Script MT")))
				return kInternalFontIDBrushScriptMT;
			if (0 == uni_strcmp(name, UNI_L("Chalkboard Bold")))
				return kInternalFontIDChalkboardBold;
			if (0 == uni_strcmp(name, UNI_L("Gill Sans Light")))
				return kInternalFontIDGillSansLight;
			break;
		case 16:
			if (0 == uni_strcmp(name, UNI_L("Arial Unicode MS")))
				return kInternalFontIDArialUnicodeMS;
			if (0 == uni_strcmp(name, UNI_L("Futura Condensed")))
				return kInternalFontIDFuturaCondensed;
			if (0 == uni_strcmp(name, UNI_L("Gujarati MT Bold")))
				return kInternalFontIDGujaratiMTBold;
			if (0 == uni_strcmp(name, UNI_L("Mshtakan Oblique")))
				return kInternalFontIDMshtakanOblique;
			break;
		case 17:
			if (0 == uni_strcmp(name, UNI_L("Arial Hebrew Bold")))
				return kInternalFontIDArialHebrewBold;
			if (0 == uni_strcmp(name, UNI_L("Copperplate Light")))
				return kInternalFontIDCopperplateLight;
			if (0 == uni_strcmp(name, UNI_L("Optima ExtraBlack")))
				return kInternalFontIDOptimaExtraBlack;
			break;
		case 18:
			if (0 == uni_strcmp(name, UNI_L("Apple LiSung Light")))
				return kInternalFontIDAppleLiSung;
			if (0 == uni_strcmp(name, UNI_L("Devanagari MT Bold")))
				return kInternalFontIDDevanagariMTBold;
			if (0 == uni_strcmp(name, UNI_L("Euphemia UCAS Bold")))
				return kInternalFontIDEuphemiaUCASBold;
			break;
		case 19:
			if (0 == uni_strcmp(name, UNI_L("American Typewriter")))
				return kInternalFontIDAmericanTypewriter;
			if (0 == uni_strcmp(name, UNI_L("Corsiva Hebrew Bold")))
				return kInternalFontIDCorsivaHebrewBold;
			if (0 == uni_strcmp(name, UNI_L("New Peninim MT Bold")))
				return kInternalFontIDNewPeninimMTBold;
			break;
		case 20:
			if (0 == uni_strcmp(name, UNI_L("Microsoft Sans Serif")))
				return kInternalFontIDMicrosoftSansSerif;
			if (0 == uni_strcmp(name, UNI_L("Plantagenet Cherokee")))
				return kInternalFontIDPlantagenetCherokee;
			if (0 == uni_strcmp(name, UNI_L("Baskerville Semibold")))
				return kInternalFontIDBaskervilleSemibold;
			if (0 == uni_strcmp(name, UNI_L("Euphemia UCAS Italic")))
				return kInternalFontIDEuphemiaUCASItalic;
			if (0 == uni_strcmp(name, UNI_L("Helvetica Neue Light")))
				return kInternalFontIDHelveticaNeueLight;
			if (0 == uni_strcmp(name, UNI_L("Mshtakan BoldOblique")))
				return kInternalFontIDMshtakanBoldOblique;
			break;
		case 21:
			if (0 == uni_strcmp(name, UNI_L("Apple LiGothic Medium")))
				return kInternalFontIDAppleLiGothic;
			if (0 == uni_strcmp(name, UNI_L("Arial Rounded MT Bold")))
				return kInternalFontIDArialRoundedMTBold;
			break;
		case 22:
#ifndef USE_FAMILY_NAME
			if (0 == uni_strcmp(name, UNI_L("Hiragino Mincho Pro W3")))
				return kInternalFontIDHiraginoMincho;
			if (0 == uni_strcmp(name, UNI_L("Hiragino Mincho Pro W6")))
				return kInternalFontIDHiraginoMinchoProW6;
#endif
			if (0 == uni_strcmp(name, UNI_L("Hoefler Text Ornaments")))
				return kInternalFontIDHoeflerTextOrnaments;
			break;
		case 23:
#ifndef USE_FAMILY_NAME
			if (0 == uni_strcmp(name, UNI_L("Hiragino Mincho ProN W3")))
				return kInternalFontIDHiraginoMinchoProN;
#endif
			if (0 == uni_strcmp(name, UNI_L("New Peninim MT Inclined")))
				return kInternalFontIDNewPeninimMTInclined;
			break;
		case 25:
			if (0 == uni_strcmp(name, UNI_L("American Typewriter Light")))
				return kInternalFontIDAmericanTypewriterLight;
			if (0 == uni_strcmp(name, UNI_L("Helvetica Neue UltraLight")))
				return kInternalFontIDHelveticaNeueUltraLight;
			break;
		case 27:
#ifndef USE_FAMILY_NAME
			if (0 == uni_strcmp(name, UNI_L("Hiragino Maru Gothic Pro W4")))
				return kInternalFontIDHiraginoMaruGothicProW4;
			if (0 == uni_strcmp(name, UNI_L("Hiragino Kaku Gothic Pro W3")))
				return kInternalFontIDHiraginoKakuGothic;
			if (0 == uni_strcmp(name, UNI_L("Hiragino Kaku Gothic Pro W6")))
				return kInternalFontIDHiraginoKakuGothicProW6;
#endif
			break;
		case 28:
#ifndef USE_FAMILY_NAME
			if (0 == uni_strcmp(name, UNI_L("Hiragino Kaku Gothic ProN W3")))
				return kInternalFontIDHiraginoKakuGothicProN;
#endif
			if (0 == uni_strcmp(name, UNI_L("New Peninim MT Bold Inclined")))
				return kInternalFontIDNewPeninimMTBoldInclined;
			break;
		case 29:
			if (0 == uni_strcmp(name, UNI_L("American Typewriter Condensed")))
				return kInternalFontIDAmericanTypewriterCondensed;
			if (0 == uni_strcmp(name, UNI_L("Helvetica Neue Bold Condensed")))
				return kInternalFontIDHelveticaNeueBoldCondensed;
			break;
		case 30:
			if (0 == uni_strcmp(name, UNI_L("Helvetica Neue Black Condensed")))
				return kInternalFontIDHelveticaNeueBlackCondensed;
			break;
		case 35:
			if (0 == uni_strcmp(name, UNI_L("American Typewriter Condensed Light")))
				return kInternalFontIDAmericanTypewriterCondensedLight;
			break;
	}
	return kInternalFontIDUnknown;
}

MacFontIntern::MacFontIntern()
{
	OP_NEW_DBG("MacFontIntern::MacFontIntern()", "Fonts");
	
#ifdef REARRANGE_FONTS_BY_SUITABLITY
	mSystemLanguage = GetSystemLanguage();
#endif
	
	uni_char *fontName;
	INT32 fontNum = 0;
	CFStringRef name;
	ATSFontRef atsFont = 0;
	CTFontRef ctFont = 0;
	
	NSDictionary *options = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:1] forKey:(NSString*)kCTFontCollectionRemoveDuplicatesOption];
	CTFontCollectionRef collection = CTFontCollectionCreateFromAvailableFonts((CFDictionaryRef)options);
	CFArrayRef fonts = CTFontCollectionCreateMatchingFontDescriptors(collection);
	
	CFIndex n = CFArrayGetCount(fonts);
	for (CFIndex i=0; i<n; i++)
	{
		CTFontDescriptorRef f = (CTFontDescriptorRef)CFArrayGetValueAtIndex(fonts, i);
		name = (CFStringRef)CTFontDescriptorCopyAttribute(f, kCTFontDisplayNameAttribute);
		
		GetFontRefs(name, atsFont, ctFont);
		
		fontNum++;
		
		int name_len = CFStringGetLength(name);
		fontName = OP_NEWA(uni_char, name_len + 1);
		CFStringGetCharacters(name, CFRangeMake(0, name_len), (UniChar*)fontName);
		CFRelease(name);
		fontName[name_len] = 0;
		
		if (!fontName || fontName[0] == 0 || fontName[0] == '.' || fontName[0] == '#' || !uni_strcmp(fontName, "Apple Color Emoji"))
		{
			if (ctFont)
				CFRelease(ctFont);
			OP_DELETEA(fontName);
			continue;
		}

		// Make the bloody testcases work
		if ((name_len > 7) && (0 == uni_strcmp(fontName + name_len - 7, UNI_L(" Normal"))))
			fontName[name_len-7] = 0;
		else if ((name_len> 8) && (0 == uni_strcmp(fontName + name_len - 8, UNI_L(" Regular"))))
			fontName[name_len-8] = 0;
		
		OP_DBG((UNI_L("Trying %d - %s"), fontNum, fontName));
		
		InternalFontItem *fontItem = OP_NEW(InternalFontItem, (atsFont, ctFont));
		if (!fontItem)
		{
			if (ctFont)
				CFRelease(ctFont);
			OP_DELETEA(fontName);
			break;
		}
		
		fontList.Add(fontItem);
		fontItem->name = fontName;
		fontItem->internalID = GetInternalFontIDFromName(fontItem->name);
		
		OP_DBG((UNI_L("Fontnr %d - %s"), fontNum, fontName));
		
		if (fontItem->internalID == kInternalFontIDCopperplate || fontItem->internalID == kInternalFontIDCapitals)
			fontItem->supportSmallCaps = TRUE;
		
		if ((0 == uni_strncmp(fontName, FONT_COURIER, 7)) || (0 == uni_strncmp(fontName, UNI_L("Monaco"), 6)) || (0 == uni_stricmp(fontName, FONT_OSAKA_MONO)) || (0 == uni_stricmp(fontName, UNI_L("Menlo"))))
			fontItem->monospace = TRUE;
		
		memset(&(fontItem->signature), 0, sizeof(FONTSIGNATURE));
		
#if !defined(DELAYED_CODEPAGE_CALCULATION) || !defined(LOAD_SERIFS_ON_DEMAND)
		BOOL checkFontAnyway = ((!gInternalFontSwitching) &&	((uni_stricmp(fontItem->name,  FONT_TIMES) == 0) ||
															 	 (uni_stricmp(fontItem->name,  FONT_LUCIDA_GRANDE) == 0) ||
																 (uni_strnicmp(fontItem->name, FONT_HIRAGINO_GOTHIC, 13) == 0) ||
																 (uni_strnicmp(fontItem->name, FONT_HIRAGINO_MINCHO, 13) == 0) ||
																 (uni_stricmp (fontItem->name, FONT_APPLE_MYUNGJO) == 0) ||
																 (uni_strnicmp(fontItem->name, FONT_ST_HEITI, 4) == 0) ));
#endif // !DELAYED_CODEPAGE_CALCULATION || !LOAD_SERIFS_ON_DEMAND
		
#ifndef DELAYED_CODEPAGE_CALCULATION		
		uint32 *characterMap = NULL;
		if (gInternalFontSwitching || checkFontAnyway)
		{
			if (ctFont)
				ParseCMAPTables(ctFont, &characterMap);
			if (atsFont && !characterMap)
				ParseCMAPTables(fontItem->atsFontRef, &characterMap);
		}
		
		if (characterMap)
		{
			GenerateUsb(fontItem->signature, characterMap);
#ifdef _GLYPHTESTING_SUPPORT_
			fontItem->characterMap = characterMap;
#else
			OP_DELETEA(characterMap);
#endif
		}
#endif // DELAYED_CODEPAGE_CALCULATION
		
#ifndef LOAD_SERIFS_ON_DEMAND
		if (gInternalFontSwitching || checkFontAnyway) {
			BOOL isSerif;
			if (!GetSerifs(fontItem->internalID, fontItem->serifs))
			{
				if (ctFont && ParseOS2Tag(ctFont, isSerif) || !ctFont && ParseOS2Tag(atsFont, isSerif)) {
					if (isSerif)
						fontItem->serifs = OpFontInfo::WithSerifs;
					else
						fontItem->serifs = OpFontInfo::WithoutSerifs;
				}
				else
					fontItem->serifs = OpFontInfo::UnknownSerifs;
			}
		}
#endif // LOAD_SERIFS_ON_DEMAND
	}
	
	CFRelease(collection);
	CFRelease(fonts);
}

MacFontIntern::~MacFontIntern()
{
	fontList.DeleteAll();
}

void MacFontIntern::GetFontRefs(CFStringRef name, ATSFontRef& atsFont, CTFontRef& ctFont)
{
	atsFont = ATSFontFindFromName(name, kATSOptionFlagsDefault);
#ifdef CREATE_CORE_TEXT_FONT_REFS
	ctFont = CTFontCreateWithName(name, 0.0, NULL);
#endif
#if STORE_ONLY_BASE_VERSION_OF_FONTS
	if (ctFont)
	{
		NSDictionary *traits, *attrs;
		CTFontDescriptorRef desc;
		CTFontRef newFont;
		CFStringRef copyright = CTFontCopyName(ctFont, kCTFontCopyrightNameKey);
		if (copyright)
		{
			CFRange r = CFStringFind(copyright, CFSTR("Microsoft"), 0);
			if (r.location != kCFNotFound)
			{
				CFStringRef family = CTFontCopyFamilyName(ctFont);
				if (family)
				{
					traits = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:0] forKey:(NSString*)kCTFontSymbolicTrait];
					attrs = [NSDictionary dictionaryWithObjectsAndKeys:(NSString*)family, kCTFontFamilyNameAttribute, traits, kCTFontTraitsAttribute, nil];
					desc = CTFontDescriptorCreateWithAttributes((CFDictionaryRef)attrs);
					newFont = CTFontCreateWithFontDescriptor(desc, 0.0, NULL);
					CFRelease(desc);
					CFRelease(family);
					if (newFont)
					{
						CFStringRef name = CTFontCopyPostScriptName(newFont);
						atsFont = ATSFontFindFromPostScriptName(name, kATSOptionFlagsDefault);
						CFRelease(ctFont);
#ifdef CREATE_CORE_TEXT_FONT_REFS
						ctFont = CTFontCreateWithName(name, 0.0, NULL);
#endif
						CFRelease(name);
						CFRelease(newFont);
					}
				}
			}
			CFRelease(copyright);
		}
	}
#endif
}

#endif // _NEW_FONTSWITCHING_
