/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/display/writingsystemheuristic.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/htm_elm.h"

#ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC

// # define WRITINGSYSTEM_DEBUG // prints detected writing system
// # define WRITINGSYSTEM_DEBUG_TEXT // prints text to  be analyzed

# ifdef WRITINGSYSTEM_DEBUG
static const char* const ScriptToString(WritingSystem::Script script)
{
    switch (script)
    {
    case WritingSystem::LatinWestern: return "LatinWestern"; break;
    case WritingSystem::LatinEastern: return "LatinEastern"; break;
    case WritingSystem::LatinUnknown: return "LatinUnknown"; break;
    case WritingSystem::Cyrillic: return "Cyrillic"; break;
    case WritingSystem::Baltic: return "Baltic"; break;
    case WritingSystem::Turkish: return "Turkish"; break;
    case WritingSystem::Greek: return "Greek"; break;
    case WritingSystem::Hebrew: return "Hebrew"; break;
    case WritingSystem::Arabic: return "Arabic"; break;
    case WritingSystem::Persian: return "Persian"; break;
    case WritingSystem::IndicDevanagari: return "IndicDevanagari"; break;
    case WritingSystem::IndicBengali: return "IndicBengali"; break;
    case WritingSystem::IndicGurmukhi: return "IndicGurmukhi"; break;
    case WritingSystem::IndicGujarati: return "IndicGujarati"; break;
    case WritingSystem::IndicOriya: return "IndicOriya"; break;
    case WritingSystem::IndicTamil: return "IndicTamil"; break;
    case WritingSystem::IndicTelugu: return "IndicTelugu"; break;
    case WritingSystem::IndicKannada: return "IndicKannada"; break;
    case WritingSystem::IndicMalayalam: return "IndicMalayalam"; break;
    case WritingSystem::IndicSinhala: return "IndicSinhala"; break;
    case WritingSystem::IndicLao: return "IndicLao"; break;
    case WritingSystem::IndicTibetan: return "IndicTibetan"; break;
    case WritingSystem::IndicMyanmar: return "IndicMyanmar"; break;
    case WritingSystem::IndicUnknown: return "IndicUnknown"; break;
    case WritingSystem::Thai: return "Thai"; break;
    case WritingSystem::Vietnamese: return "Vietnamese"; break;
    case WritingSystem::ChineseSimplified: return "ChineseSimplified"; break;
    case WritingSystem::ChineseTraditional: return "ChineseTraditional"; break;
    case WritingSystem::ChineseUnknown: return "ChineseUnknown"; break;
    case WritingSystem::Japanese: return "Japanese"; break;
    case WritingSystem::Korean: return "Korean"; break;
    case writingSystem::Cherokee: return "Cherokee"; break;
    case WritingSystem::Unknown:
    default:
        return "Unknown"; break;
    }
}
# endif // WRITINGSYSTEM_DEBUG

WritingSystemHeuristic::WritingSystemHeuristic()
{
    Reset();
}

void WritingSystemHeuristic::Reset()
{
	m_analyzed_count = 0;

	for (size_t i = 0; i < ARRAY_SIZE(m_scorecard); ++i)
		m_scorecard[i] = 0;
}

void WritingSystemHeuristic::Analyze(const uni_char *text, int text_length/* = -1*/)
{
	OP_ASSERT(m_analyzed_count <= MAX_ANALYZE_LENGTH);

	// >= instead of ==, just to be safe
	if (m_analyzed_count >= MAX_ANALYZE_LENGTH)
		return;

	if (text_length < 0)
		text_length = uni_strlen(text);

	// <= instead of ==, just to be safe
	if (text_length <= 0)
		return;

	AnalyzeInternal(text, text_length);
}

BOOL WritingSystemHeuristic::WantsMoreText() const
{
	OP_ASSERT(m_analyzed_count <= MAX_ANALYZE_LENGTH);

	return m_analyzed_count < MAX_ANALYZE_LENGTH;
}

WritingSystem::Script WritingSystemHeuristic::GuessScript() const
{
	// Find the scorecard entry with the highest score, ignoring characters of
	// unknown script and penalizing scripts that commonly occur in other
	// contexts.

	if (m_analyzed_count == 0)
		return ScriptFromLocale();

	OP_ASSERT(ARRAY_SIZE(m_scorecard) == WritingSystem::NumScripts);

	int max_frequency = 0;
	int max_index = WritingSystem::Unknown;
	for (int i = 0; i < WritingSystem::NumScripts; ++i)
	{
		int frequency;

		frequency = m_scorecard[i];

		if (i == WritingSystem::LatinWestern)
		{
			// As LatinWestern often appears interspersed in other scripts, it
			// is penalized below by dividing its score by 10, and then with a
			// maximum value of 20(NON_LATIN_THRESHOLD). In order to
			// prevent problems on pages with less than 10 characters however,
			// it should still become the current leader if we have seen
			// LatinWestern characters and max_frequency == 0.

			if (max_frequency == 0 && frequency > 0)
				max_index = WritingSystem::LatinWestern;

			frequency = MIN((frequency/10), NON_LATIN_THRESHOLD);
		}

		if (frequency > max_frequency)
		{
			max_frequency = frequency;
			max_index = i;
		}
	}

	// 'script' now holds the script that wins on raw score, but there's still
	// additional analysis to do
	WritingSystem::Script script =
		static_cast<WritingSystem::Script>(max_index);

	// Like LatinWestern, CJK also contains many characters shared between
	// scripts
	if (script == WritingSystem::ChineseUnknown)
    {
        // It should be enough with just one threshold value here,
        // since kana and hangul should be much more common than the
        // discerning traditional/simplified glyphs in japanese/korean
        // text

        // Find highest CJK
        size_t highest = WritingSystem::Korean;
        if (m_scorecard[WritingSystem::Japanese] > m_scorecard[highest])
            highest = WritingSystem::Japanese;
        if (m_scorecard[WritingSystem::ChineseTraditional] > m_scorecard[highest])
            highest = WritingSystem::ChineseTraditional;
        if (m_scorecard[WritingSystem::ChineseSimplified] > m_scorecard[highest])
            highest = WritingSystem::ChineseSimplified;

        script = m_scorecard[highest] ?
			static_cast<WritingSystem::Script>(highest) : WritingSystem::ChineseUnknown;
    }
	// Like LatinWestern, Arabic also contains many characters shared between
	// scripts
	else if (script == WritingSystem::Arabic)
	{
		if (m_scorecard[WritingSystem::Persian] > 0)
			script = WritingSystem::Persian;
	}

	// In case of an unknown script we try to derive a suitable script from the
	// system locale. If that fails as well, we fall back on LatinWestern.
	if (script == WritingSystem::Unknown)
	{
		script = ScriptFromLocale();

		if (script == WritingSystem::Unknown)
			script = WritingSystem::LatinWestern;
	}

	return script;
}

void WritingSystemHeuristic::AnalyzeInternal(const uni_char *text, size_t text_length)
{
    for (size_t i = 0; i < text_length; ++i)
    {
		if (text[i] <= 0x40)
			// These are punctuations and numbers that are language independent
			continue;
		else if (text[i] < 128)
			m_scorecard[WritingSystem::LatinWestern]++;
		else
		switch (Unicode::GetScriptType(text[i]))
        {
        case SC_Latin:
			// LatinWestern covered in fast path
			m_scorecard[WritingSystem::LatinUnknown]++;
            break;
        case SC_Cyrillic:
            m_scorecard[WritingSystem::Cyrillic]++;
            break;
        case SC_Greek:
            m_scorecard[WritingSystem::Greek]++;
            break;
        case SC_Hebrew:
            m_scorecard[WritingSystem::Hebrew]++;
            break;
        case SC_Arabic:
            AnalyzeArabic(text[i]);
            break;
        case SC_Devanagari:
            m_scorecard[WritingSystem::IndicDevanagari]++;
            break;
        case SC_Bengali:
            m_scorecard[WritingSystem::IndicBengali]++;
            break;
        case SC_Gurmukhi:
            m_scorecard[WritingSystem::IndicGurmukhi]++;
            break;
        case SC_Gujarati:
            m_scorecard[WritingSystem::IndicGujarati]++;
            break;
        case SC_Oriya:
            m_scorecard[WritingSystem::IndicOriya]++;
            break;
        case SC_Tamil:
            m_scorecard[WritingSystem::IndicTamil]++;
            break;
        case SC_Telugu:
            m_scorecard[WritingSystem::IndicTelugu]++;
            break;
        case SC_Kannada:
            m_scorecard[WritingSystem::IndicKannada]++;
            break;
        case SC_Malayalam:
            m_scorecard[WritingSystem::IndicMalayalam]++;
            break;
        case SC_Sinhala:
            m_scorecard[WritingSystem::IndicSinhala]++;
            break;
        case SC_Lao:
            m_scorecard[WritingSystem::IndicLao]++;
            break;
        case SC_Tibetan:
            m_scorecard[WritingSystem::IndicTibetan]++;
            break;
        case SC_Myanmar:
            m_scorecard[WritingSystem::IndicMyanmar]++;
            break;
        case SC_Thai:
            m_scorecard[WritingSystem::Thai]++;
            break;
        case SC_Han: // This also occurs in Korean and Japanese, but not without Hangul or Hiragana/Katakana as well.
            AnalyzeHan(text[i]);
            break;
        case SC_Hiragana:
        case SC_Katakana:
            m_scorecard[WritingSystem::Japanese]++;
            break;
        case SC_Hangul:
            m_scorecard[WritingSystem::Korean]++;
            break;
        case SC_Bopomofo:
            m_scorecard[WritingSystem::ChineseUnknown]++;
            break;
        case SC_Cherokee:
            m_scorecard[WritingSystem::Cherokee]++;
            break;
        case SC_Unknown:
			continue;
        default:
			OP_ASSERT(!"Uncategorized script found while analyzing text during heuristic language detection");
        }

		m_analyzed_count ++;
		if (m_analyzed_count >= MAX_ANALYZE_LENGTH)
			break;
    }
}

//
// Analysis of Han characters
//

// Try to determine wheter the text is simplified or traditional by scanning
// for common characters

// List of common simplified codepoints - ordered for binary search
static const uni_char g_common_simp[] = {
	0x4E00, 0x4E07, 0x4E09, 0x4E0A, 0x4E0B, 0x4E0D, 0x4E0E, 0x4E13,
	0x4E14, 0x4E16, 0x4E1A, 0x4E1C, 0x4E24, 0x4E2A, 0x4E2D, 0x4E3A,
	0x4E3B, 0x4E48, 0x4E49, 0x4E4B, 0x4E4E, 0x4E5D, 0x4E5F, 0x4E66,
	0x4E86, 0x4E89, 0x4E8B, 0x4E8C, 0x4E8E, 0x4E94, 0x4E9A, 0x4E9B,
	0x4EA4, 0x4EA7, 0x4EB2, 0x4EBA, 0x4EC0, 0x4EC5, 0x4ECA, 0x4ECE,
	0x4ED6, 0x4EE3, 0x4EE4, 0x4EE5, 0x4EEC, 0x4EF6, 0x4EF7, 0x4EFB,
	0x4F01, 0x4F17, 0x4F1A, 0x4F20, 0x4F3C, 0x4F46, 0x4F4D, 0x4F4F,
	0x4F53, 0x4F55, 0x4F5C, 0x4F60, 0x4F7F, 0x4FBF, 0x4FDD, 0x4FE1,
	0x5019, 0x505A, 0x50CF, 0x513F, 0x5143, 0x5148, 0x5149, 0x514B,
	0x515A, 0x5165, 0x5168, 0x516B, 0x516C, 0x516D, 0x5171, 0x5173,
	0x5175, 0x5176, 0x5177, 0x5185, 0x518D, 0x5199, 0x519B, 0x519C,
	0x51B3, 0x51B5, 0x51C6, 0x51E0, 0x51FA, 0x51FB, 0x5206, 0x5207,
	0x5217, 0x5219, 0x5229, 0x522B, 0x5230, 0x5236, 0x524D, 0x529B,
	0x529E, 0x529F, 0x52A0, 0x52A1, 0x52A8, 0x52BF, 0x5305, 0x5316,
	0x5317, 0x533A, 0x533B, 0x5341, 0x534E, 0x5355, 0x5357, 0x5373,
	0x5374, 0x5386, 0x539F, 0x53BB, 0x53C2, 0x53C8, 0x53CA, 0x53CD,
	0x53D1, 0x53D6, 0x53D7, 0x53D8, 0x53E3, 0x53E4, 0x53E6, 0x53EA,
	0x53EB, 0x53EF, 0x53F0, 0x53F2, 0x53F7, 0x53F8, 0x5403, 0x5404,
	0x5408, 0x540C, 0x540D, 0x540E, 0x5411, 0x5417, 0x5427, 0x542C,
	0x544A, 0x5458, 0x5462, 0x5468, 0x547D, 0x548C, 0x54C1, 0x54CD,
	0x5546, 0x5668, 0x56DB, 0x56DE, 0x56E0, 0x56E2, 0x56FD, 0x56FE,
	0x5728, 0x5730, 0x573A, 0x57CE, 0x57FA, 0x589E, 0x58EB, 0x58F0,
	0x5904, 0x5907, 0x590D, 0x5916, 0x591A, 0x5927, 0x5929, 0x592A,
	0x592B, 0x5931, 0x5934, 0x5973, 0x5979, 0x597D, 0x5982, 0x59CB,
	0x59D4, 0x5B50, 0x5B57, 0x5B58, 0x5B66, 0x5B83, 0x5B89, 0x5B8C,
	0x5B98, 0x5B9A, 0x5B9E, 0x5BB6, 0x5BB9, 0x5BF9, 0x5BFC, 0x5C06,
	0x5C0F, 0x5C11, 0x5C14, 0x5C31, 0x5C3D, 0x5C40, 0x5C55, 0x5C71,
	0x5DE5, 0x5DF1, 0x5DF2, 0x5E02, 0x5E03, 0x5E08, 0x5E0C, 0x5E26,
	0x5E38, 0x5E72, 0x5E73, 0x5E74, 0x5E76, 0x5E7F, 0x5E94, 0x5E9C,
	0x5EA6, 0x5EFA, 0x5F00, 0x5F0F, 0x5F15, 0x5F20, 0x5F3A, 0x5F53,
	0x5F62, 0x5F71, 0x5F80, 0x5F88, 0x5F97, 0x5FB7, 0x5FC3, 0x5FC5,
	0x5FEB, 0x5FF5, 0x600E, 0x601D, 0x6027, 0x603B, 0x606F, 0x60C5,
	0x60F3, 0x610F, 0x611F, 0x6210, 0x6211, 0x6216, 0x6218, 0x623F,
	0x6240, 0x624B, 0x624D, 0x6253, 0x627E, 0x6280, 0x628A, 0x62A5,
	0x62C9, 0x6301, 0x6307, 0x636E, 0x63A5, 0x63A8, 0x63D0, 0x652F,
	0x6536, 0x6539, 0x653E, 0x653F, 0x6559, 0x6570, 0x6574, 0x6587,
	0x65AD, 0x65AF, 0x65B0, 0x65B9, 0x65E0, 0x65E5, 0x65E9, 0x65F6,
	0x660E, 0x6613, 0x662F, 0x663E, 0x66F4, 0x66FE, 0x6700, 0x6708,
	0x6709, 0x670D, 0x671B, 0x671F, 0x672A, 0x672C, 0x672F, 0x673A,
	0x6743, 0x674E, 0x6761, 0x6765, 0x6781, 0x6784, 0x6797, 0x679C,
	0x67E5, 0x6807, 0x6837, 0x6839, 0x683C, 0x6B21, 0x6B63, 0x6B64,
	0x6B65, 0x6B66, 0x6B7B, 0x6BCF, 0x6BD4, 0x6C11, 0x6C14, 0x6C34,
	0x6C42, 0x6CA1, 0x6CBB, 0x6CD5, 0x6CE8, 0x6D3B, 0x6D41, 0x6D4E,
	0x6D77, 0x6D88, 0x6DF1, 0x6E05, 0x6EE1, 0x706B, 0x70B9, 0x7136,
	0x7167, 0x7231, 0x7247, 0x7269, 0x7279, 0x738B, 0x73B0, 0x7406,
	0x751F, 0x7528, 0x7531, 0x7535, 0x754C, 0x75C5, 0x767D, 0x767E,
	0x7684, 0x76EE, 0x76F4, 0x76F8, 0x770B, 0x771F, 0x773C, 0x7740,
	0x77E5, 0x77F3, 0x7814, 0x786E, 0x793A, 0x793E, 0x795E, 0x79BB,
	0x79CD, 0x79D1, 0x79F0, 0x7A0B, 0x7A76, 0x7A7A, 0x7A81, 0x7ACB,
	0x7B11, 0x7B2C, 0x7B49, 0x7B97, 0x7BA1, 0x7C7B, 0x7CBE, 0x7CFB,
	0x7EA2, 0x7EA6, 0x7EA7, 0x7EBF, 0x7EC4, 0x7ECF, 0x7ED3, 0x7ED9,
	0x7EDF, 0x7F57, 0x7F8E, 0x8001, 0x8003, 0x8005, 0x800C, 0x8054,
	0x80FD, 0x81EA, 0x81F3, 0x8272, 0x82B1, 0x82F1, 0x843D, 0x867D,
	0x884C, 0x8868, 0x88AB, 0x88C5, 0x897F, 0x8981, 0x89C1, 0x89C2,
	0x89C4, 0x89C6, 0x89C9, 0x89E3, 0x8A00, 0x8BA1, 0x8BA4, 0x8BA9,
	0x8BAE, 0x8BB0, 0x8BB8, 0x8BBA, 0x8BBE, 0x8BC1, 0x8BC6, 0x8BDD,
	0x8BE5, 0x8BED, 0x8BF4, 0x8BF7, 0x8C03, 0x8C08, 0x8C61, 0x8D28,
	0x8D39, 0x8D44, 0x8D70, 0x8D77, 0x8D8A, 0x8DEF, 0x8EAB, 0x8F66,
	0x8F6C, 0x8F7B, 0x8F83, 0x8FB9, 0x8FBE, 0x8FC7, 0x8FD0, 0x8FD1,
	0x8FD8, 0x8FD9, 0x8FDB, 0x8FDC, 0x8FDE, 0x9009, 0x901A, 0x9020,
	0x9053, 0x90A3, 0x90E8, 0x90FD, 0x91CC, 0x91CD, 0x91CF, 0x91D1,
	0x957F, 0x95E8, 0x95EE, 0x95F4, 0x961F, 0x963F, 0x9645, 0x9662,
	0x9664, 0x968F, 0x96BE, 0x96C6, 0x9700, 0x9752, 0x975E, 0x9762,
	0x987B, 0x9886, 0x9898, 0x98CE, 0x98DE, 0x9996, 0x9A6C, 0x9AD8,
};

// List of common traditional codepoints - ordered for binary search
static const uni_char g_common_trad[] = {
	0x4E00, 0x4E09, 0x4E0A, 0x4E0B, 0x4E0D, 0x4E14, 0x4E16, 0x4E26,
	0x4E2D, 0x4E3B, 0x4E45, 0x4E4B, 0x4E4E, 0x4E5F, 0x4E86, 0x4E8B,
	0x4E8C, 0x4E94, 0x4E9B, 0x4EA4, 0x4EBA, 0x4EC0, 0x4ECA, 0x4ED6,
	0x4EE3, 0x4EE4, 0x4EE5, 0x4EF6, 0x4EFB, 0x4EFD, 0x4F3C, 0x4F46,
	0x4F4D, 0x4F4F, 0x4F55, 0x4F5C, 0x4F60, 0x4F7F, 0x4F86, 0x4FBF,
	0x4FDD, 0x4FE1, 0x4FEE, 0x500B, 0x5011, 0x5019, 0x505A, 0x5099,
	0x50B3, 0x50CF, 0x50F9, 0x5143, 0x5144, 0x5148, 0x5149, 0x5152,
	0x5165, 0x5167, 0x5168, 0x5169, 0x516B, 0x516C, 0x516D, 0x5171,
	0x5176, 0x518D, 0x51F0, 0x51FA, 0x5206, 0x5225, 0x5229, 0x5230,
	0x5236, 0x5247, 0x524D, 0x529B, 0x529F, 0x52A0, 0x52D5, 0x52D9,
	0x5316, 0x5317, 0x5340, 0x5341, 0x534A, 0x5357, 0x5361, 0x5373,
	0x537B, 0x539F, 0x53BB, 0x53C3, 0x53C8, 0x53CA, 0x53CB, 0x53CD,
	0x53D6, 0x53D7, 0x53E3, 0x53E6, 0x53EA, 0x53EB, 0x53EF, 0x53F0,
	0x5403, 0x5404, 0x5408, 0x540C, 0x540D, 0x5411, 0x5426, 0x5427,
	0x544A, 0x5462, 0x5475, 0x547D, 0x548C, 0x54C1, 0x54C8, 0x54E1,
	0x54EA, 0x554A, 0x554F, 0x5566, 0x5594, 0x559C, 0x55AE, 0x55CE,
	0x5668, 0x56DB, 0x56DE, 0x56E0, 0x570B, 0x5716, 0x5728, 0x5730,
	0x578B, 0x57CE, 0x57FA, 0x5831, 0x5834, 0x58EB, 0x5916, 0x591A,
	0x5922, 0x5927, 0x5929, 0x592A, 0x592E, 0x5931, 0x5947, 0x5973,
	0x5979, 0x597D, 0x5982, 0x59B3, 0x59CB, 0x5B50, 0x5B57, 0x5B69,
	0x5B78, 0x5B83, 0x5B89, 0x5B8C, 0x5B9A, 0x5BA4, 0x5BB6, 0x5BB9,
	0x5BE6, 0x5BEB, 0x5C07, 0x5C08, 0x5C0D, 0x5C0F, 0x5C11, 0x5C31,
	0x5C71, 0x5DE5, 0x5DEE, 0x5DF1, 0x5DF2, 0x5E02, 0x5E0C, 0x5E2B,
	0x5E36, 0x5E38, 0x5E6B, 0x5E73, 0x5E74, 0x5E7E, 0x5E95, 0x5EA6,
	0x5EFA, 0x5F0F, 0x5F15, 0x5F1F, 0x5F35, 0x5F37, 0x5F62, 0x5F71,
	0x5F88, 0x5F8C, 0x5F97, 0x5F9E, 0x5FC3, 0x5FC5, 0x5FEB, 0x5FF5,
	0x600E, 0x601D, 0x6027, 0x602A, 0x60A8, 0x60C5, 0x60F3, 0x610F,
	0x611B, 0x611F, 0x61C9, 0x6210, 0x6211, 0x6216, 0x6230, 0x6240,
	0x624B, 0x624D, 0x6253, 0x627E, 0x628A, 0x6295, 0x62FF, 0x6301,
	0x6307, 0x6389, 0x63A5, 0x63D0, 0x63DB, 0x652F, 0x6536, 0x6539,
	0x653E, 0x653F, 0x6545, 0x6559, 0x6574, 0x6578, 0x6587, 0x6599,
	0x65B0, 0x65B9, 0x65BC, 0x65E5, 0x660E, 0x661F, 0x662F, 0x6642,
	0x66F4, 0x66F8, 0x66FE, 0x6700, 0x6703, 0x6708, 0x6709, 0x670B,
	0x670D, 0x671B, 0x671F, 0x672A, 0x672C, 0x674E, 0x6771, 0x677F,
	0x6797, 0x679C, 0x6821, 0x683C, 0x6848, 0x696D, 0x6A02, 0x6A19,
	0x6A23, 0x6A5F, 0x6A94, 0x6B0A, 0x6B21, 0x6B4C, 0x6B61, 0x6B63,
	0x6B64, 0x6B7B, 0x6BCF, 0x6BD4, 0x6C11, 0x6C23, 0x6C34, 0x6C42,
	0x6C7A, 0x6C92, 0x6CD5, 0x6D3B, 0x6D41, 0x6D77, 0x6DF1, 0x6E05,
	0x6EFF, 0x6F14, 0x7063, 0x70BA, 0x7121, 0x7136, 0x7247, 0x7248,
	0x7269, 0x7279, 0x7368, 0x738B, 0x73A9, 0x73ED, 0x73FE, 0x7403,
	0x7406, 0x751F, 0x7528, 0x7531, 0x7537, 0x754C, 0x7559, 0x756B,
	0x7576, 0x767C, 0x767D, 0x7684, 0x76EE, 0x76F4, 0x76F8, 0x770B,
	0x771F, 0x773C, 0x77E5, 0x7814, 0x7834, 0x78BA, 0x793E, 0x795E,
	0x7968, 0x79D1, 0x7A0B, 0x7A2E, 0x7A76, 0x7A7A, 0x7ACB, 0x7AD9,
	0x7AE0, 0x7AF9, 0x7B11, 0x7B2C, 0x7B49, 0x7B54, 0x7B97, 0x7BA1,
	0x7CBE, 0x7CFB, 0x7D04, 0x7D1A, 0x7D44, 0x7D50, 0x7D66, 0x7D71,
	0x7D93, 0x7DB2, 0x7DDA, 0x7E3D, 0x7F8E, 0x7FA9, 0x8001, 0x8003,
	0x8005, 0x800C, 0x8036, 0x806F, 0x8072, 0x807D, 0x80FD, 0x8166,
	0x81EA, 0x81F3, 0x81FA, 0x8207, 0x8208, 0x820D, 0x822C, 0x8272,
	0x82B1, 0x82E5, 0x82F1, 0x83EF, 0x842C, 0x8457, 0x8655, 0x865F,
	0x884C, 0x8868, 0x88AB, 0x88DD, 0x88E1, 0x897F, 0x8981, 0x898B,
	0x8996, 0x89BA, 0x89C0, 0x89E3, 0x8A00, 0x8A08, 0x8A0A, 0x8A0E,
	0x8A18, 0x8A2D, 0x8A31, 0x8A34, 0x8A66, 0x8A71, 0x8A72, 0x8A8D,
	0x8A9E, 0x8AAA, 0x8AB0, 0x8AB2, 0x8ACB, 0x8AD6, 0x8B1B, 0x8B1D,
	0x8B58, 0x8B70, 0x8B80, 0x8B8A, 0x8B93, 0x8C61, 0x8C93, 0x8CB7,
	0x8CBB, 0x8CC7, 0x8CFD, 0x8D70, 0x8D77, 0x8D85, 0x8DDF, 0x8DEF,
	0x8EAB, 0x8ECA, 0x8ECD, 0x8F03, 0x8F49, 0x8FA6, 0x8FD1, 0x8FF7,
	0x9019, 0x901A, 0x901F, 0x9020, 0x9023, 0x9032, 0x904B, 0x904E,
	0x9053, 0x9054, 0x9060, 0x9078, 0x9084, 0x908A, 0x90A3, 0x90E8,
	0x90FD, 0x91CD, 0x91CF, 0x91D1, 0x9304, 0x9322, 0x932F, 0x9577,
	0x9580, 0x958B, 0x9593, 0x95DC, 0x963F, 0x9664, 0x9673, 0x968A,
	0x96C4, 0x96D6, 0x96E3, 0x96FB, 0x9700, 0x975E, 0x9762, 0x97F3,
	0x982D, 0x984C, 0x9858, 0x985E, 0x98A8, 0x98DB, 0x99AC, 0x9A57,
	0x9AD4, 0x9AD8, 0x9CF3, 0x9EBC, 0x9EC3, 0x9EDE, 0x9EE8, 0x9F8D,
};

inline void WritingSystemHeuristic::AnalyzeHan(uni_char up)
{
	uni_char* r;
	BOOL unknown = TRUE;

	// simplified
	const size_t simp_size = ARRAY_SIZE(g_common_simp);
	r = (uni_char*)op_bsearch(&up, g_common_simp, simp_size, sizeof(*g_common_simp), CompareChars);
	if (r)
	{
		OP_ASSERT(*r == up);
		m_scorecard[WritingSystem::ChineseSimplified]++;
		unknown = FALSE;
	}

	// traditional
	const size_t trad_size = ARRAY_SIZE(g_common_trad);
	r = (uni_char*)op_bsearch(&up, g_common_trad, trad_size, sizeof(*g_common_trad), CompareChars);
	if (r)
	{
		OP_ASSERT(*r == up);
		m_scorecard[WritingSystem::ChineseTraditional]++;
		unknown = FALSE;
	}

	if (unknown)
		m_scorecard[WritingSystem::ChineseUnknown]++;
}

// List of common discerning Persian codepoints - ordered for binary search
static const uni_char g_common_persian[] = {
	0x067e, // Peh
	0x0686, // Tcheh
	0x0698, // Jeh
	0x06a9, // Keheh
	0x06af, // Gaf
	0x06cc, // Yeh
	// Presentation forms:
	0xfb56, // Peh Isolated Form
	0xfb57, // Peh Final Form
	0xfb58, // Peh Initial Form
	0xfb59, // Peh Medial Form
	0xfb7a, // Tcheh Isolated Form
	0xfb7b, // Tcheh Final Form
	0xfb7c, // Tcheh Initial Form
	0xfb7d, // Tcheh Medial Form
	0xfb8a, // Jeh Isolated Form
	0xfb8b, // Jeh Final Form
	0xfb8e, // Keheh Isolated Form
	0xfb8f, // Keheh Final Form
	0xfb90, // Keheh Initial Form
	0xfb91, // Keheh Medial Form
	0xfb92, // Gaf Isolated Form
	0xfb93, // Gaf Final Form
	0xfb94, // Gaf Initial Form
	0xfb95, // Gaf Medial Form
	0xfbfc, // Yeh Isolated Form
	0xfbfd, // Yeh Final Form
	0xfbfe, // Yeh Initial Form
	0xfbff // Yeh Medial Form
};

inline void WritingSystemHeuristic::AnalyzeArabic(uni_char up)
{
	uni_char* r;

	// Persian
	const size_t persian_size = ARRAY_SIZE(g_common_persian);
	r = (uni_char*)op_bsearch(&up, g_common_persian, persian_size, sizeof(*g_common_persian), CompareChars);
	if (r)
	{
		OP_ASSERT(*r == up);
		m_scorecard[WritingSystem::Persian]++;
		return;
	}

	m_scorecard[WritingSystem::Arabic]++;
}

/* static */
int WritingSystemHeuristic::CompareChars(const void* kp, const void* ep)
{
	const uni_char k = *(uni_char*)kp;
	const uni_char e = *(uni_char*)ep;
	return k == e ? 0 : (k > e ? 1 : -1);
}

/* static */
WritingSystem::Script WritingSystemHeuristic::ScriptFromLocale()
{
	WritingSystem::Script script = WritingSystem::Unknown;

	// Initialize to avoid the GCC warning "'encoding' may be used
	// uninitialized in this function". GetSystemEncodingL() could longjmp()
	// before 'encoding' gets assigned, so the warning makes sense.
	const char* encoding = 0;

	TRAPD(err, encoding=g_op_system_info->GetSystemEncodingL());
	if (OpStatus::IsSuccess(err) && encoding && *encoding)
	{
		script = WritingSystem::FromEncoding(encoding);
	}

	// Latin is a very weak hint, try to use UI language
	if (script == WritingSystem::Unknown 
		|| script == WritingSystem::LatinUnknown
		|| script == WritingSystem::LatinWestern)
	{
		const OpStringC lang = g_languageManager->GetLanguage();
		WritingSystem::Script ui_script = WritingSystem::FromLanguageCode(lang.CStr());
		if (ui_script != WritingSystem::Unknown
			&& ui_script != WritingSystem::LatinUnknown
			&& ui_script != WritingSystem::LatinWestern)
			// Found a stronger hint
			script = ui_script;
	}

	return script;
}

#endif // DISPLAY_WRITINGSYSTEM_HEURISTIC
