/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/model/UnicodeRangeDetectors.h"

/** Detect if the font supports a good range of glyphs in a unicode range.
 * characterMap contains 2,048 32-bit ints, so a given utf-16 codepoint, uc, would be found in characterMap[uc >> 5].
 * The bit-pattern is in big endian, first character is last (least significant) bit.
 * For exaple, codepoint U+0041 (65, 'A') would be tested with (characterMap[2] & 0x00000002) or (characterMap[65 >> 5] & (1 << (65 & 31)))
 * Conversely, (characterMap[a] & (1 << b)) is codepoint a * 32 + b
 * The names are a tiny bit misleading, as these functions return Microsoft Usb ranges rather than the official unicode ranges.
 * This will probably be ammended in the future, so this class solely reports unicode ranges, and Usb is calculated in MacFontIntern.cpp
 *
 * In the days before _GLYPHTESTING_SUPPORT_, these functions had to be pretty accurate at reporting what codepages they supported.
 * So, only fonts that were likely support MOST of the CP were reported.
 * Now, though, anything is better than a square box, so report ANY match.
 */

Boolean UnicodeRangeDetectors::hasLatin1(const uint32 *characterMap)
{
//	U+0020 - U+007e
	if (characterMap[1] || characterMap[2] || characterMap[3])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasLatin1Supplement(const uint32 *characterMap)
{
//	U+00a0 - U+00ff
	if (characterMap[5] || characterMap[6] || characterMap[7])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasLatinExtendedA(const uint32 *characterMap)
{
//	U+0100 - U+017f
	if (characterMap[8] || characterMap[9] || characterMap[10] || characterMap[11])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasLatinExtendedB(const uint32 *characterMap)
{
//	U+0180 - U+024f
	if (characterMap[12] || characterMap[13] || characterMap[14] || characterMap[15] ||
		characterMap[16] || characterMap[17]  || (characterMap[18] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasIPAExtensions(const uint32 *characterMap)
{
//	U+0250 - U+02af
	if((characterMap[18] & 0xFFFF0000) || characterMap[19] || characterMap[20] || (characterMap[21] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasSpacingModifiedLetters(const uint32 *characterMap)
{
//	U+02b0 - U+02ff
	if((characterMap[21] & 0xFFFF0000) || characterMap[22] || characterMap[23])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasCombiningDiacriticalMarks(const uint32 *characterMap)
{
//	U+0300 - U+036f
	if (characterMap[24] || characterMap[25] || characterMap[26] || (characterMap[27] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasGreek(const uint32 *characterMap)
{
//	U+0370 - U+03ff
	if ((characterMap[27] & 0xFFFF0000) || characterMap[28] || characterMap[29] || characterMap[30] || characterMap[31])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasCyrillic(const uint32 *characterMap)
{
//	U+0400 - U+04ff
	if (characterMap[32] || characterMap[33] || characterMap[34] || characterMap[35] ||
		characterMap[36] || characterMap[37] || characterMap[38] || characterMap[39])
		return true;

//	U+0500 - U+052f, Cyrillic Supplementary
	if (characterMap[40] || (characterMap[41] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasArmenian(const uint32 *characterMap)
{
//	U+0530 - U+058F
	if((characterMap[41] & 0xFFFF0000) || characterMap[42] || characterMap[43] || (characterMap[44] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasHebrew(const uint32 *characterMap)
{
//	U+0590 - U+05FF
	if((characterMap[44] & 0xFFFF0000) || characterMap[45] || characterMap[46] || characterMap[47])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasArabic(const uint32 *characterMap)
{
//	U+0600 - U+06ff
	if (characterMap[48] || characterMap[49] || characterMap[50] || characterMap[51] ||
		characterMap[52] || characterMap[53] || characterMap[54] || characterMap[55])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasDevanagari(const uint32 *characterMap)
{
//	U+0900 - U+097f
	if (characterMap[72] || characterMap[73] || characterMap[74] || characterMap[75])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasBengali(const uint32 *characterMap)
{
//	U+0980 - U+09ff
	if (characterMap[76] || characterMap[77] || characterMap[78] || characterMap[79])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasGurmukhi(const uint32 *characterMap)
{
//	U+0a00 - U+0a7f
	if (characterMap[80] || characterMap[81] || characterMap[82] || characterMap[83])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasGujarati(const uint32 *characterMap)
{
//	U+0a80 - U+0aff
	if (characterMap[84] || characterMap[85] || characterMap[86] || characterMap[87])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasOriya(const uint32 *characterMap)
{
//	U+0b00 - U+0b7f
	if (characterMap[88] || characterMap[89] || characterMap[90] || characterMap[91])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasTamil(const uint32 *characterMap)
{
//	U+0b80 - U+0bff
	if (characterMap[92] || characterMap[93] || characterMap[94] || characterMap[95])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasTelugu(const uint32 *characterMap)
{
//	U+0c00 - U+0c7f
	if (characterMap[96] || characterMap[97] || characterMap[98] || characterMap[99])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasKannada(const uint32 *characterMap)
{
//	U+0c80 - U+0cff
	if (characterMap[100] || characterMap[101] || characterMap[102] || characterMap[103])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasMalayalam(const uint32 *characterMap)
{
//	U+0d00 - U+0d7f
	if (characterMap[104] || characterMap[105] || characterMap[106] || characterMap[107])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasThai(const uint32 *characterMap)
{
//	U+0e00 - U+0e7f
	if (characterMap[112] || characterMap[113] || characterMap[114] || characterMap[115])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasLao(const uint32 *characterMap)
{
//	U+0e80 - U+0eff
	if (characterMap[116] || characterMap[117] || characterMap[118] || characterMap[119])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasBasicGeorgian(const uint32 *characterMap)
{
//	U+10a0 - U+10ff
	if (characterMap[133] || characterMap[134] || characterMap[135])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasHangulJamo(const uint32 *characterMap)
{
//	U+1100 - U+11ff
	if (characterMap[136] || characterMap[137] || characterMap[138] || characterMap[139] ||
		characterMap[140] || characterMap[141] || characterMap[142] || characterMap[143])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasLatinExtendedAdditional(const uint32 *characterMap)
{
//	U+1e00 - U+1eff
	if (characterMap[240] || characterMap[241] || characterMap[242] || characterMap[243] ||
		characterMap[244] || characterMap[245] || characterMap[246] || characterMap[247])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasGreekExtended(const uint32 *characterMap)
{
//	U+1f00 - U+1fff
	if (characterMap[248] || characterMap[249] || characterMap[250] || characterMap[251] ||
		characterMap[252] || characterMap[253] || characterMap[254] || characterMap[255])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasGeneralPunctuation(const uint32 *characterMap)
{
//	U+2000 - U+206f
	if (characterMap[256] || characterMap[257] || characterMap[258] || (characterMap[259] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasSuperscriptsAndSubscripts(const uint32 *characterMap)
{
//	U+2070 - U+209f
	if ((characterMap[259] & 0xFFFF0000) || characterMap[260])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasCurrencySymbols(const uint32 *characterMap)
{
//	U+20a0 - U+20cf
	if (characterMap[261] || (characterMap[262] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasCombiningDiacriticalMarksForSymbols(const uint32 *characterMap)
{
//	U+20d0 - U+20ff
	if ((characterMap[262] & 0xFFFF0000) || characterMap[263])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasLetterLikeSymbols(const uint32 *characterMap)
{
//	U+2100 - U+214f
	if (characterMap[264] || characterMap[265] || (characterMap[266] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasNumberForms(const uint32 *characterMap)
{
//	U+2150 - U+218f
	if ((characterMap[266] & 0xFFFF0000) || characterMap[267] || (characterMap[268] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasArrows(const uint32 *characterMap)
{
//	U+2190 - U+21ff
	if ((characterMap[268] & 0xFFFF0000) || characterMap[269] || characterMap[270] || characterMap[271])
		return true;

//	U+27F0 - U+27FF, Supplemental Arrows-A
	if (characterMap[319] & 0xFFFF0000)
		return true;

//	U+2900 - U+297F, Supplemental Arrows-B
	if (characterMap[328] || characterMap[329] || characterMap[330] || characterMap[331])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasMathematicalOperators(const uint32 *characterMap)
{
//	U+2200 - U+22ff
	if (characterMap[272] || characterMap[273] || characterMap[274] || characterMap[275] ||
		characterMap[276] || characterMap[277] || characterMap[278] || characterMap[279])
		return true;

//	U+2A00 - U+2AFF, Supplemental Mathematical Operators
	if (characterMap[336] || characterMap[337] || characterMap[338] || characterMap[339] ||
		characterMap[340] || characterMap[341] || characterMap[342] || characterMap[343])
		return true;

//	U+27C0 - U+27EF, Miscellaneous Mathematical Symbols-A
	if (characterMap[318] || (characterMap[319] & 0x0000FFFF))
		return true;

//	U+2980 - U+29FF, Miscellaneous Mathematical Symbols-B
	if (characterMap[332] || characterMap[333] || characterMap[334] || characterMap[335])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasMiscellaneousTechnical(const uint32 *characterMap)
{
//	U+2300 - U+23ff
	if (characterMap[280] || characterMap[281] || characterMap[282] || characterMap[283] ||
		characterMap[284] || characterMap[285] || characterMap[286] || characterMap[287])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasControlPictures(const uint32 *characterMap)
{
//	U+2400 - U+243f
	if (characterMap[288] || characterMap[289])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasOpticalCharacterRecognition(const uint32 *characterMap)
{
//	U+2440 - U+245f
	if (characterMap[290])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasEnclosedAlphanumerics(const uint32 *characterMap)
{
//	U+2460 - U+24ff
	if (characterMap[291] || characterMap[292] || characterMap[293] || characterMap[294] || characterMap[295])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasBoxDrawing(const uint32 *characterMap)
{
//	U+2500 - U+257f
	if (characterMap[296] || characterMap[297] || characterMap[298] || characterMap[299])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasBlockElements(const uint32 *characterMap)
{
//	U+2580 - U+259f
	if (characterMap[300])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasBlockGeometricShapes(const uint32 *characterMap)
{
//	U+25a0 - U+25ff
	if (characterMap[301] || characterMap[302] || characterMap[303])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasMiscellaneousSymbols(const uint32 *characterMap)
{
//	U+2600 - U+26ff
	if (characterMap[304] || characterMap[305] || characterMap[306] || characterMap[307] ||
		characterMap[308] || characterMap[309] || characterMap[310] || characterMap[311])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasDingbats(const uint32 *characterMap)
{
//	U+2700 - U+27bf
	if (characterMap[312] || characterMap[313] || characterMap[314] ||
		characterMap[315] || characterMap[316] || characterMap[317])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasCJKSymbolsAndPunctuation(const uint32 *characterMap)
{
//	U+3000 - U+303f
	if(characterMap[384] || characterMap[385])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasHiragana(const uint32 *characterMap)
{
//	U+3040 - U+309f
	if (characterMap[386] || characterMap[387] || characterMap[388])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasKatakana(const uint32 *characterMap)
{
//	U+30a0 - U+30ff
	if (characterMap[389] || characterMap[390] || characterMap[391])
		return true;
//	U+31f0 - U+31ff, Katakana Phonetic Extensions
	if (characterMap[399] & 0xFFFF0000)
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasBopomofo(const uint32 *characterMap)
{
//	U+3100 - U+312f
	if (characterMap[392] || (characterMap[393] & 0x0000FFFF))
		return true;

//	U+31a0 - U+31bf, Bopomofo Extended
	if (characterMap[397])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasHangulCompatibilityJamo(const uint32 *characterMap)
{
//	U+3130 - U+318f
	if ((characterMap[393] & 0xFFFF0000) || (characterMap[394]) || (characterMap[395]) || (characterMap[396] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasCJKMiscellaneous(const uint32 *characterMap)
{
//	U+3190 - U+319f
	if (characterMap[396] & 0xFFFF0000)
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasEnclosedCJKLettersAndMonths(const uint32 *characterMap)
{
//	U+3200 - U+32ff
	if (characterMap[400] || characterMap[401] || characterMap[402] || characterMap[403] ||
		characterMap[404] || characterMap[405] || characterMap[406] || characterMap[407])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasCJKCompatibility(const uint32 *characterMap)
{
//	U+3300 - U+33ff
	if(characterMap[408] || characterMap[409] || characterMap[410] || characterMap[411])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasHangul(const uint32 *characterMap)
{
//	U+ac00 - U+d7a3
//	Keep original test
	if(characterMap[1706] & 0x10000000)
		if(characterMap[1387] & 0x00002000)
			if(characterMap[1581] & 0x00100000)
				return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasCJKUnifiedIdeographs(const uint32 *characterMap)
{
//	U+4e00 - U+9fff
//	U+2e80 - U+2eff, CJK Radicals Supplement
//	U+2F00 - U+2FDF, Kangxi Radicals
//	U+2FF0 - U+2FFF, Ideographic Description Characters
//	U+3400 - U+4DBF, CJK Unified Ideograph Extension A
//	U+3190 - U+319F, Kanbun

//	Keep original test
	if(!(characterMap[625] & 0x00002000)) return false; // return true;
//	if(!(characterMap[671] & 0x00800000)) return false;
	if(!(characterMap[812] & 0x00000080)) return false; //  return true;
	if(!(characterMap[815] & 0x00000020)) return false; //  return true;
	if(!(characterMap[825] & 0x00001000)) return false; //  return true;
	if(!(characterMap[1108] & 0x40000000)) return false; //  return true;

	return true;
	//return false;
}

Boolean UnicodeRangeDetectors::hasPrivateUseArea(const uint32 *characterMap)
{
//	U+e000 - U+f8ff
	for (int i = 1792; i < 1992; i++)
		if (characterMap[i])
			return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasCJKCompatibilityIdeographs(const uint32 *characterMap)
{
//	U+f900 - U+faff
	// This might be overdoing it.
	if (characterMap[1992] || characterMap[1993] || characterMap[1994] || characterMap[1995] ||
		characterMap[1996] || characterMap[1997] || characterMap[1998] || characterMap[1999] ||
		characterMap[2000] || characterMap[2001] || characterMap[2002] || characterMap[2003] ||
		characterMap[2004] || characterMap[2005] || characterMap[2006] || characterMap[2007])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasAlphabeticPresentationForms(const uint32 *characterMap)
{
//	U+fb00 - U+fb4f
	if(characterMap[2008] || characterMap[2009] || (characterMap[2010] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasArabicPresentationFormsA(const uint32 *characterMap)
{
//	U+fb50 - U+fdff
	if((characterMap[2010] & 0xFFFF0000) || characterMap[2011] || characterMap[2012] ||
		characterMap[2013] || characterMap[2014] || characterMap[2015])
		return true;
		// Don't bother testing rest: fc00-fdff (2016-2031)

	return false;
}

Boolean UnicodeRangeDetectors::hasCombiningHalfMarks(const uint32 *characterMap)
{
//	fe20-fe2f
	if (characterMap[2033] & 0x0000FFFF)
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasCJKCompatibilityForms(const uint32 *characterMap)
{
//	fe30-fe4f
	if ((characterMap[2033] & 0xFFFF0000) || (characterMap[2034] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasSmallFormVariants(const uint32 *characterMap)
{
//	fe50-fe6f
	if ((characterMap[2034] & 0xFFFF0000) || (characterMap[2035] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasArabicPresentationFormsB(const uint32 *characterMap)
{
//	fe70-fefe
	if ((characterMap[2035] & 0xFFFF0000) || characterMap[2036] || characterMap[2037] ||
		characterMap[2038] || (characterMap[2039] & 0xEFFFFFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasHalfwidthAndFullwidthForms(const uint32 *characterMap)
{
//	U+ff00 - U+ffef
	if (characterMap[2040] || characterMap[2041] || characterMap[2042] || characterMap[2043] ||
		characterMap[2044] || characterMap[2045] || characterMap[2046] || (characterMap[2047] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasTibetian(const uint32 *characterMap)
{
//	U+0f00 - 0fff
	if (characterMap[120] || characterMap[121] || characterMap[122] || characterMap[123] ||
		characterMap[124] || characterMap[125] || characterMap[126] || characterMap[127])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasSyriac(const uint32 *characterMap)
{
//	U+0700 - U+074f
	if (characterMap[56] || characterMap[57] || (characterMap[58] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasThaana(const uint32 *characterMap)
{
//	U+0780 - U+07bf
	if (characterMap[60] || characterMap[61])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasSinhala(const uint32 *characterMap)
{
//	U+0d80 - U+0dff
	if(characterMap[108] || characterMap[109] || characterMap[110] || characterMap[111])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasMyanmar(const uint32 *characterMap)
{
//	U+1000 - U+109f
	if(characterMap[128] || characterMap[129] || characterMap[130] ||
		characterMap[131] || characterMap[132])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasEthiopic(const uint32 *characterMap)
{
//	U+1200 - U+137f
	if(characterMap[144] || characterMap[145] || characterMap[146] || characterMap[147])
		return true;
		// Don't bother testing rest: 1280-137f (148-155)

	return false;
}

Boolean UnicodeRangeDetectors::hasCherokee(const uint32 *characterMap)
{
//	U+13a0 - U+13ff
	if (characterMap[157] || characterMap[158] || characterMap[159])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasCanadianAboriginalSyllabics(const uint32 *characterMap)
{
//	U+1400 - U+167f
	if (characterMap[160] || characterMap[161] || characterMap[162] || characterMap[163] ||
		characterMap[164] || characterMap[165] || characterMap[166] || characterMap[167])
		return true;
		// Don't bother testing rest: 1500-167f (168-179)

	return false;
}

Boolean UnicodeRangeDetectors::hasOgham(const uint32 *characterMap)
{
//	U+1680 - U+169f
	if(characterMap[180])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasRunic(const uint32 *characterMap)
{
//	U+16a0 - U+16ff
	if(characterMap[181] || characterMap[182] || characterMap[183])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasKhmer(const uint32 *characterMap)
{
//	U+1780 - U+17ff
	if(characterMap[188] || characterMap[189] || characterMap[190] || characterMap[191])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasMongolian(const uint32 *characterMap)
{
//	U+1800 - U+18af
	if (characterMap[192] || characterMap[193] || characterMap[194] || characterMap[195] ||
		characterMap[196] || (characterMap[197] & 0x0000FFFF))
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasBraille(const uint32 *characterMap)
{
//	U+2800 - U+28ff
	if (characterMap[320] || characterMap[321] || characterMap[322] || characterMap[323] ||
		characterMap[324] || characterMap[325] || characterMap[326] || characterMap[327])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::hasYi(const uint32 *characterMap)
{
//	U+a000 - U+a48f
	if (characterMap[1280] || characterMap[1281] || characterMap[1282] || characterMap[1283] ||
		characterMap[1284] || characterMap[1285] || characterMap[1286] || characterMap[1287])
		return true;
		// Don't bother testing rest

//	U+A490 - U+A4CF, Yi Radicals

	return false;
}

Boolean UnicodeRangeDetectors::hasPhilipino(const uint32 *characterMap)
{
//	U+1700 - U+171F, Tagalog
	if (characterMap[184])
		return true;

//	U+1720 - U+173F, Hanunoo
	if (characterMap[185])
		return true;

//	U+1740 - U+175F, Buhid
	if (characterMap[186])
		return true;

//	U+1760 - U+177F, Tagbanwa
	if (characterMap[187])
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::isSimplified(const uint32 *characterMap)
{
	struct CharacterTestTable
	{
		unsigned short character;
		unsigned short mapLocation;
		unsigned int mask;
	};

	static const CharacterTestTable testTable[] =
	{
		{ 0x4E00, 624, 0x00000001 },
		{ 0x4E0A, 624, 0x00000400 },
		{ 0x4E16, 624, 0x00400000 },
		{ 0x4E2A, 625, 0x00000400 },
		{ 0x4E2D, 625, 0x00002000 },
		{ 0x4E48, 626, 0x00000100 },
		{ 0x4E61, 627, 0x00000002 },
		{ 0x4E86, 628, 0x00000040 },
		{ 0x4EC0, 630, 0x00000001 },
		{ 0x4ECA, 630, 0x00000400 },
		{ 0x4ECE, 630, 0x00004000 },
		{ 0x4ED6, 630, 0x00400000 },
		{ 0x4EEC, 631, 0x00001000 },
		{ 0x4F1A, 632, 0x04000000 },
		{ 0x4F5C, 634, 0x10000000 },
		{ 0x4F60, 635, 0x00000001 },
		{ 0x538C, 668, 0x00001000 },
		{ 0x53BB, 669, 0x08000000 },
		{ 0x53EB, 671, 0x00000800 },
		{ 0x5403, 672, 0x00000008 },
		{ 0x540D, 672, 0x00002000 },
		{ 0x54EA, 679, 0x00000400 },
		{ 0x559C, 684, 0x10000000 },
		{ 0x5668, 691, 0x00000100 },
		{ 0x56DE, 694, 0x40000000 },
		{ 0x56FD, 695, 0x20000000 },
		{ 0x5728, 697, 0x00000100 },
		{ 0x574F, 698, 0x00008000 },
		{ 0x5927, 713, 0x00000080 },
		{ 0x5929, 713, 0x00000200 },
		{ 0x597D, 715, 0x20000000 },
		{ 0x5B50, 730, 0x00010000 },
		{ 0x5B57, 730, 0x00800000 },
		{ 0x5BB6, 733, 0x00400000 },
		{ 0x5DE5, 751, 0x00000020 },
		{ 0x5F88, 764, 0x00000100 },
		{ 0x5FAE, 765, 0x00004000 },
		{ 0x60F3, 775, 0x00080000 },
		{ 0x6211, 784, 0x00020000 },
		{ 0x6587, 812, 0x00000080 },
		{ 0x662F, 817, 0x00008000 },
		{ 0x6700, 824, 0x00000001 },
		{ 0x6765, 827, 0x00000020 },
		{ 0x6B22, 857, 0x00000004 },
		{ 0x6C14, 864, 0x00100000 },
		{ 0x6D4B, 874, 0x00000800 },
		{ 0x6D4F, 874, 0x00008000 },
		{ 0x70DF, 902, 0x80000000 },
		{ 0x754C, 938, 0x00001000 },
		{ 0x7684, 948, 0x00000010 },
		{ 0x771F, 952, 0x80000000 },
		{ 0x809A, 1028, 0x04000000 },
		{ 0x86CB, 1078, 0x00000800 },
		{ 0x8857, 1090, 0x00800000 },
		{ 0x89C8, 1102, 0x00000100 },
		{ 0x8BA8, 1117, 0x00000100 },
		{ 0x8BD5, 1118, 0x00200000 },
		{ 0x8BF4, 1119, 0x00100000 },
		{ 0x8D77, 1131, 0x00800000 },
		{ 0x8F6F, 1147, 0x00008000 },
		{ 0x901B, 1152, 0x08000000 },
		{ 0x91CC, 1166, 0x00001000 },
		{ 0x996D, 1227, 0x00002000 },
		{ 0x997F, 1227, 0x80000000 }
	};
	unsigned int testedOk=0;
	unsigned int i;

	for(i=0; i<ARRAY_SIZE(testTable); i++)
	{
		if((characterMap[testTable[i].mapLocation] & testTable[i].mask) == testTable[i].mask)
			testedOk ++;
	}

	if(testedOk > ARRAY_SIZE(testTable) * 9 / 10)
		return true;

	return false;
}

Boolean UnicodeRangeDetectors::isTraditional(const uint32 *characterMap)
{
	struct CharacterTestTable
	{
		unsigned short character;
		unsigned short mapLocation;
		unsigned int mask;
	};

	static const CharacterTestTable testTable[] =
	{
		{ 0x4E8B,  628, 0x00000800 },
		{ 0x4E9B,  628, 0x08000000 },
		{ 0x4EE5,  631, 0x00000020 },
		{ 0x4EF6,  631, 0x00400000 },
		{ 0x4F5C,  634, 0x10000000 },
		{ 0x4F7F,  635, 0x80000000 },
		{ 0x4F86,  636, 0x00000040 },
		{ 0x4F9D,  636, 0x20000000 },
		{ 0x5148,  650, 0x00000100 },
		{ 0x5206,  656, 0x00000040 },
		{ 0x5236,  657, 0x00400000 },
		{ 0x529B,  660, 0x08000000 },
		{ 0x52D9,  662, 0x02000000 },
		{ 0x52D9,  662, 0x02000000 },
		{ 0x5316,  664, 0x00400000 },
		{ 0x53CA,  670, 0x00000400 },
		{ 0x53EF,  671, 0x00008000 },
		{ 0x5408,  672, 0x00000100 },
		{ 0x5473,  675, 0x00080000 },
		{ 0x548C,  676, 0x00001000 },
		{ 0x5546,  682, 0x00000040 },
		{ 0x5730,  697, 0x00010000 },
		{ 0x5747,  698, 0x00000080 },
		{ 0x597D,  715, 0x20000000 },
		{ 0x5B9A,  732, 0x04000000 },
		{ 0x5E0C,  752, 0x00001000 },
		{ 0x5F0F,  760, 0x00008000 },
		{ 0x5F71,  763, 0x00020000 },
		{ 0x5F97,  764, 0x00800000 },
		{ 0x5FEB,  767, 0x00000800 },
		{ 0x6027,  769, 0x00000080 },
		{ 0x60A8,  773, 0x00000100 },
		{ 0x610F,  776, 0x00008000 },
		{ 0x6167,  779, 0x00000080 },
		{ 0x61C9,  782, 0x00000200 },
		{ 0x6240,  786, 0x00000001 },
		{ 0x63F4,  799, 0x00100000 },
		{ 0x64CD,  806, 0x00002000 },
		{ 0x652F,  809, 0x00008000 },
		{ 0x6574,  811, 0x00100000 },
		{ 0x6587,  812, 0x00000080 },
		{ 0x65B9,  813, 0x02000000 },
		{ 0x660E,  816, 0x00004000 },
		{ 0x667A,  819, 0x04000000 },
		{ 0x6703,  824, 0x00000008 },
		{ 0x6709,  824, 0x00000200 },
		{ 0x671B,  824, 0x08000000 },
		{ 0x672C,  825, 0x00001000 },
		{ 0x6790,  828, 0x00010000 },
		{ 0x6848,  834, 0x00000100 },
		{ 0x696D,  843, 0x00002000 },
		{ 0x6BCB,  862, 0x00000800 },
		{ 0x6C42,  866, 0x00000004 },
		{ 0x6C7A,  867, 0x04000000 },
		{ 0x7372,  923, 0x00040000 },
		{ 0x7528,  937, 0x00000100 },
		{ 0x7684,  948, 0x00000010 },
		{ 0x7B26,  985, 0x00000040 },
		{ 0x7B56,  986, 0x00400000 },
		{ 0x7BC4,  990, 0x00000010 },
		{ 0x7D44, 1002, 0x00000010 },
		{ 0x7E54, 1010, 0x00100000 },
		{ 0x7FA9, 1021, 0x00000200 },
		{ 0x800C, 1024, 0x00001000 },
		{ 0x80FD, 1031, 0x20000000 },
		{ 0x81EA, 1039, 0x00000400 },
		{ 0x8457, 1058, 0x00800000 },
		{ 0x89E3, 1103, 0x00000008 },
		{ 0x8A02, 1104, 0x00000004 },
		{ 0x8A2D, 1105, 0x00002000 },
		{ 0x8AAA, 1109, 0x00000400 },
		{ 0x8ABF, 1109, 0x80000000 },
		{ 0x9019, 1152, 0x02000000 },
		{ 0x901F, 1152, 0x80000000 },
		{ 0x9069, 1155, 0x00000200 },
		{ 0x90A3, 1157, 0x00000008 },
		{ 0x9375, 1179, 0x00200000 },
		{ 0x95DC, 1198, 0x10000000 },
		{ 0x9650, 1202, 0x00010000 },
		{ 0x9700, 1208, 0x00000001 },
		{ 0x97FF, 1215, 0x80000000 },
		{ 0x9806, 1216, 0x00000040 }
	};
	unsigned int testedOk=0;
	unsigned int i;

	for(i=0; i<ARRAY_SIZE(testTable); i++)
	{
		if((characterMap[testTable[i].mapLocation] & testTable[i].mask) == testTable[i].mask)
			testedOk ++;
	}

	if(testedOk > ARRAY_SIZE(testTable) * 9 / 10)
		return true;

	return false;
}

