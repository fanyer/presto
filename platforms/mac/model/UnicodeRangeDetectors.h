#ifndef UNICODE_RANGE_DETECTORS_H
#define UNICODE_RANGE_DETECTORS_H

class UnicodeRangeDetectors
{
public:
	static Boolean hasLatin1(const uint32 *characterMap);
	static Boolean hasLatin1Supplement(const uint32 *characterMap);
	static Boolean hasLatinExtendedA(const uint32 *characterMap);
	static Boolean hasLatinExtendedB(const uint32 *characterMap);
	static Boolean hasIPAExtensions(const uint32 *characterMap);
	static Boolean hasSpacingModifiedLetters(const uint32 *characterMap);
	static Boolean hasCombiningDiacriticalMarks(const uint32 *characterMap);
	static Boolean hasGreek(const uint32 *characterMap);
//	static Boolean hasCoptic(const uint32 *characterMap) { return false; }
	static Boolean hasCyrillic(const uint32 *characterMap);
	static Boolean hasArmenian(const uint32 *characterMap);
	static Boolean hasHebrew(const uint32 *characterMap);
//	This line has been left blank on purpose
	static Boolean hasArabic(const uint32 *characterMap);
//	static Boolean hasNKo(const uint32 *characterMap) { return false; }
	static Boolean hasDevanagari(const uint32 *characterMap);
	static Boolean hasBengali(const uint32 *characterMap);
	static Boolean hasGurmukhi(const uint32 *characterMap);
	static Boolean hasGujarati(const uint32 *characterMap);
	static Boolean hasOriya(const uint32 *characterMap);
	static Boolean hasTamil(const uint32 *characterMap);
	static Boolean hasTelugu(const uint32 *characterMap);
	static Boolean hasKannada(const uint32 *characterMap);
	static Boolean hasMalayalam(const uint32 *characterMap);
	static Boolean hasThai(const uint32 *characterMap);
	static Boolean hasLao(const uint32 *characterMap);
	static Boolean hasBasicGeorgian(const uint32 *characterMap);
//	static Boolean hasBalinese(const uint32 *characterMap) { return false; }
	static Boolean hasHangulJamo(const uint32 *characterMap);
	static Boolean hasLatinExtendedAdditional(const uint32 *characterMap);
	static Boolean hasGreekExtended(const uint32 *characterMap);
	static Boolean hasGeneralPunctuation(const uint32 *characterMap);

	static Boolean hasSuperscriptsAndSubscripts(const uint32 *characterMap);
	static Boolean hasCurrencySymbols(const uint32 *characterMap);
	static Boolean hasCombiningDiacriticalMarksForSymbols(const uint32 *characterMap);
	static Boolean hasLetterLikeSymbols(const uint32 *characterMap);
	static Boolean hasNumberForms(const uint32 *characterMap);
	static Boolean hasArrows(const uint32 *characterMap);
	static Boolean hasMathematicalOperators(const uint32 *characterMap);
	static Boolean hasMiscellaneousTechnical(const uint32 *characterMap);
	static Boolean hasControlPictures(const uint32 *characterMap);
	static Boolean hasOpticalCharacterRecognition(const uint32 *characterMap);
	static Boolean hasEnclosedAlphanumerics(const uint32 *characterMap);
	static Boolean hasBoxDrawing(const uint32 *characterMap);
	static Boolean hasBlockElements(const uint32 *characterMap);
	static Boolean hasBlockGeometricShapes(const uint32 *characterMap);
	static Boolean hasMiscellaneousSymbols(const uint32 *characterMap);
	static Boolean hasDingbats(const uint32 *characterMap);
	static Boolean hasCJKSymbolsAndPunctuation(const uint32 *characterMap);
	static Boolean hasHiragana(const uint32 *characterMap);
	static Boolean hasKatakana(const uint32 *characterMap);
	static Boolean hasBopomofo(const uint32 *characterMap);
	static Boolean hasHangulCompatibilityJamo(const uint32 *characterMap);
	static Boolean hasCJKMiscellaneous(const uint32 *characterMap);
	static Boolean hasEnclosedCJKLettersAndMonths(const uint32 *characterMap);
	static Boolean hasCJKCompatibility(const uint32 *characterMap);
	static Boolean hasHangul(const uint32 *characterMap);
//	static Boolean hasSurrogates(const uint32 *characterMap) { return false; }
//	static Boolean hasPhoenician(const uint32 *characterMap) { return false; }
	static Boolean hasCJKUnifiedIdeographs(const uint32 *characterMap);
	static Boolean hasPrivateUseArea(const uint32 *characterMap);
	static Boolean hasCJKCompatibilityIdeographs(const uint32 *characterMap);
	static Boolean hasAlphabeticPresentationForms(const uint32 *characterMap);
	static Boolean hasArabicPresentationFormsA(const uint32 *characterMap);

	static Boolean hasCombiningHalfMarks(const uint32 *characterMap);
	static Boolean hasCJKCompatibilityForms(const uint32 *characterMap);
	static Boolean hasSmallFormVariants(const uint32 *characterMap);
	static Boolean hasArabicPresentationFormsB(const uint32 *characterMap);
	static Boolean hasHalfwidthAndFullwidthForms(const uint32 *characterMap);
//	static Boolean hasSpecials(const uint32 *characterMap) { return false; }
	static Boolean hasTibetian(const uint32 *characterMap);
	static Boolean hasSyriac(const uint32 *characterMap);
	static Boolean hasThaana(const uint32 *characterMap);
	static Boolean hasSinhala(const uint32 *characterMap);
	static Boolean hasMyanmar(const uint32 *characterMap);
	static Boolean hasEthiopic(const uint32 *characterMap);
	static Boolean hasCherokee(const uint32 *characterMap);
	static Boolean hasCanadianAboriginalSyllabics(const uint32 *characterMap);
	static Boolean hasOgham(const uint32 *characterMap);
	static Boolean hasRunic(const uint32 *characterMap);
	static Boolean hasKhmer(const uint32 *characterMap);
	static Boolean hasMongolian(const uint32 *characterMap);
	static Boolean hasBraille(const uint32 *characterMap);
	static Boolean hasYi(const uint32 *characterMap);
	static Boolean hasPhilipino(const uint32 *characterMap);

	static Boolean isSimplified(const uint32 *characterMap);
	static Boolean isTraditional(const uint32 *characterMap);
};

#endif // UNICODE_RANGE_DETECTORS_H
