/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#if defined(SEARCH_ENGINE_PHRASESEARCH) || defined(USE_SEARCH_ENGINE_WORDHIGHLIGHTER)

#include "modules/search_engine/PhraseSearch.h"
#include "modules/unicode/unicode.h"

PhraseMatcher::PhraseMatcher()
	: m_query(NULL)
	, m_phrases(PtrDescriptor<TVector<Word> >())
	, m_phrase_flags(0)
{
}

PhraseMatcher::~PhraseMatcher()
{
	if ((m_phrase_flags & DontCopyInputString) == 0)
		OP_DELETEA(const_cast<uni_char*>(m_query));
}

OP_STATUS PhraseMatcher::Init(const uni_char *query, int phrase_flags)
{
	BOOL in_quote = FALSE, quote_found;
	const uni_char *q, *q2, *end;
	WordSegmenter ws(WordSegmenter::DontCopyInputString);
	OpAutoPtr<TVector<WordSegmenter::Word> > plain_words;
	OpAutoPtr<TVector<Word> > phrase;
	Word word;
	UnicodePoint c;
	int inc;

	if ((m_phrase_flags & DontCopyInputString) == 0) // Using the old flags!
		OP_DELETEA(const_cast<uni_char*>(m_query));
	m_query = NULL;
	m_phrases.Clear();
	m_phrase_flags = phrase_flags;

	if (query == NULL || *query == 0 || (phrase_flags & (AllPhrases|FullSearch)) == 0)
		return OpStatus::OK;

	if ((m_phrase_flags & DontCopyInputString) == 0)
		RETURN_OOM_IF_NULL(m_query = WordSegmenter::PreprocessDup(query));
	else
		m_query = query;
	q = m_query;
	end = m_query + uni_strlen(m_query);

	RETURN_IF_ERROR(ws.Set(m_query));
	plain_words.reset(ws.GetTokens());

	phrase.reset(OP_NEW(TVector<Word>, ()));
	RETURN_OOM_IF_NULL(phrase.get());

	while (*q)
	{
		c = Unicode::GetUnicodePoint(q, (int)(end+1-q), inc);
		while (TreatAsSpace(c))
		{
			q += inc;
			c = Unicode::GetUnicodePoint(q, (int)(end+1-q), inc);
		}

		q2 = q;
		while (c && !IsDoubleQuote(c) && !TreatAsSpace(c))
		{
			q2 += inc;
			c = Unicode::GetUnicodePoint(q2, (int)(end+1-q2), inc);
		}
		
		word.Set(q, (int)(q2-q));
		if (c)
			q2 += inc;

		quote_found = IsDoubleQuote(c) && (phrase_flags & QuotedPhrases) != 0;

		if (word.len > 0 && (in_quote || plain_words->Find(word) == -1))
			RETURN_IF_ERROR(phrase->Add(word));

		if (phrase->GetCount() != 0 && (!in_quote || quote_found || !*q2))
		{
			if (phrase->GetCount() == 1 &&
				((phrase_flags & (CJKPhrases|PunctuationPhrases)) == 0 ||
				 plain_words->Find(phrase->Get(0)) >= 0))
			{
				phrase->Clear();
			}
			else
			{
				RETURN_IF_ERROR(m_phrases.Add(phrase.release()));
				phrase.reset(OP_NEW(TVector<Word>, ()));
				RETURN_OOM_IF_NULL(phrase.get());
			}
		}

		if (quote_found)
			in_quote = !in_quote;

		q = q2;
	}

	// In case of FullSearch, add all plain words that are not part of a phrase
	if ((phrase_flags & FullSearch) != 0)
	{
		UINT32 p=0;
		for (UINT32 w=0; w < plain_words->GetCount(); w++)
		{
			const WordSegmenter::Word plain_word = plain_words->Get(w);
			const uni_char *word_start = plain_word.ptr;
			const uni_char *word_end = plain_word.ptr + plain_word.len;
			const uni_char *phrase_start = NULL, *phrase_end = NULL;
			while (p < m_phrases.GetCount())
			{
				TVector<Word> *phr = m_phrases[p];
				phrase_start = (*phr)[0].ptr;
				phrase_end = (*phr)[phr->GetCount()-1].ptr + (*phr)[phr->GetCount()-1].len;
				if (word_end <= phrase_end)
					break;
				p++;
			}
			if (p >= m_phrases.GetCount() || word_start < phrase_start || word_end > phrase_end)
			{
				phrase.reset(OP_NEW(TVector<Word>, ()));
				RETURN_OOM_IF_NULL(phrase.get());
				word.Set(plain_word.ptr, plain_word.len);
				RETURN_IF_ERROR(phrase->Add(word));
				RETURN_IF_ERROR(m_phrases.Insert(p, phrase.release()));
				p++;
			}
		}
	}

	if ((phrase_flags & PrefixSearch) != 0 && m_phrases.GetCount() != 0)
	{
		TVector<Word> *last_phrase = m_phrases[m_phrases.GetCount()-1];
		Word &last_word = (*last_phrase)[last_phrase->GetCount()-1];
		if (last_word.ptr + last_word.len == end)
			last_word.is_last = TRUE;
	}

	return OpStatus::OK;
}

BOOL PhraseMatcher::Matches(const uni_char *haystack) const
{
	if (Empty())
		return TRUE;

	if (haystack == NULL || *haystack == 0)
		return FALSE;

	for (UINT32 i=0; i < m_phrases.GetCount(); i++)
		if (!FindPhrase(m_phrases[i], haystack))
			return FALSE;

	return TRUE;
}

BOOL PhraseMatcher::IsDoubleQuote(UnicodePoint c)
{
	// Unicode doesn't have a specific "class" for double quotes, so make it hardcoded according to the current spec
	return c == '"' || (c >= 0x201C && c <= 0x201F) || (c >= 0x301D && c <= 0x301F) || c == 0xFF02;
}

BOOL PhraseMatcher::TreatAsSpace(UnicodePoint c) const
{
	return c && (Unicode::IsSpace(c) || Unicode::IsCntrl(c) || c == '+' ||
		((m_phrase_flags & PunctuationPhrases) == 0 && !IsDoubleQuote(c) && !Unicode::IsAlphaOrDigit(c)));
}

BOOL PhraseMatcher::FindPhrase(TVector<Word> *phrase, const uni_char *haystack)
{
	const uni_char *pos0 = haystack;
	const uni_char *pos;
	Word &word_0 = (*phrase)[0];
	UINT32 i;

	while (FindWord(word_0, pos0, haystack, TRUE))
	{
		pos0 = word_0.found_pos;
		pos  = word_0.found_pos + word_0.found_len;
		for (i=1; i < phrase->GetCount(); i++)
		{
			Word &word_i = (*phrase)[i];
			if (!FindWord(word_i, pos, haystack, FALSE))
				break;
			pos = word_i.found_pos + word_i.found_len;
		}
		if (i == phrase->GetCount())
			return TRUE;
		pos0 ++;
	}
	return FALSE;
}

BOOL PhraseMatcher::FindWord(Word &word, const uni_char *s, const uni_char *haystack, BOOL first_word)
{
	// Does not support case insensitive search for surrogate pairs, title-case characters, or Turkish i's

	// Cache lower-case and upper-case version of the first character in word
	UnicodePoint c = word.ptr[0];
	UnicodePoint w0_lo = Unicode::ToLower(c);
	UnicodePoint w0_up = Unicode::ToUpper(c);
	do
	{
		// Scan until we find the first letter of the word
		if (!first_word)
		{
			// A word following the first must be found with no intervening word characters
			while ((c = *s) != 0 && c != w0_lo && c != w0_up && !Unicode::IsAlphaOrDigit(c) && c != '\f')
				++s;
			if (c == 0 || (c != w0_lo && c != w0_up))
				return FALSE;
		}
		else
		{
			while ((c = *s) != 0 && c != w0_lo && c != w0_up)
				s++;
			if (c == 0)
				return FALSE;
		}

		// After the first letter, match haystack and needle letter for letter
		int i,j;
		for (i=j=1; i < word.len; i++,j++)
		{
			while (WordSegmenter::IsInvisibleWordCharacter(s[j]))
				j++;
			if (Unicode::ToLower(s[j]) != Unicode::ToLower(word.ptr[i]))
				break;
		}
		if (i == word.len)
		{
			// Also match the remainder invisible characters to be sure we get to a word boundary
			while (WordSegmenter::IsInvisibleWordCharacter(s[j]))
				j++;

			if (WordSegmenter::WordBreak(haystack, s, TRUE) &&
				(word.is_last || WordSegmenter::WordBreak(haystack, s+j, TRUE)))
			{
				word.found_pos = s;
				word.found_len = j;
				return TRUE;
			}
		}
		s++;
	}
	while (TRUE);
}

OP_STATUS PhraseMatcher::AppendHighlight(OpString &dst,
										 const uni_char *src,
										 int max_chars,
										 const OpStringC &start_tag,
										 const OpStringC &end_tag,
										 int prefix_ratio)
{
	if (!src || *src == 0)
		return OpStatus::OK;

	// Max chars has not been set, the whole text should be used
	// Note that this is costly - users are encouraged to set max_chars
	if (max_chars <= 0)
	{
		max_chars = (int)uni_strlen(src);
		prefix_ratio = 0; // Start from beginning of text
	}

	// Make sure the buffer has all the size we'll need
	OpString buffer;
	int buffer_size = (max_chars * (start_tag.Length() + end_tag.Length())) + max_chars;
	uni_char *dst_ptr = buffer.Reserve(buffer_size);
	RETURN_OOM_IF_NULL(dst_ptr);

	// Reserve room for all the found phrases
	TVector<Word> results;
	Word result;
	RETURN_IF_ERROR(results.Reserve(m_phrases.GetCount()));

	const uni_char *first_phrase_start = NULL;

	// Find the occurrences of each phrase
	for (UINT32 p = 0; p < m_phrases.GetCount(); p++)
	{
		TVector<Word> *phrase = m_phrases[p];
		const uni_char *current_pos = src;
		while (FindPhrase(phrase, current_pos))
		{
			const uni_char *phrase_start = (*phrase)[0].found_pos;
			const uni_char *phrase_end = (*phrase)[phrase->GetCount()-1].found_pos + (*phrase)[phrase->GetCount()-1].found_len;
			result.Set(phrase_start, (int)(phrase_end-phrase_start));
			RETURN_IF_ERROR(results.Insert(result));

			if (first_phrase_start == NULL || phrase_start < first_phrase_start)
				first_phrase_start = phrase_start;

			current_pos = phrase_start+1;
		}
	}

	const uni_char *start = FindStart(src, first_phrase_start, max_chars, prefix_ratio);
	const uni_char *end = FindEnd(start, max_chars);

	// Merge overlapping results, clamp results at <end>
	UINT32 r = 0;
	while (r < results.GetCount())
	{
		while (r+1 < results.GetCount() && results[r+1].ptr <= results[r].ptr+results[r].len)
		{
			if (results[r+1].ptr + results[r+1].len > results[r].ptr + results[r].len)
				results[r].len = (int)(results[r+1].ptr + results[r+1].len - results[r].ptr); // Merge
			results.Remove(r+1);
		}

		if (results[r].ptr >= end)
			results.Remove(r);
		else
		{
			if (results[r].ptr + results[r].len > end)
				results[r].len = (int)(end - results[r].ptr); // Clamp
			r++;
		}
	}

	// Build the formatted quote :
	int  appended_chars = 0;
	Word last;
	last.ptr = start;

	while (results.GetCount() > 0)
	{
		if (appended_chars + (results[0].ptr-(last.ptr+last.len)) > max_chars)
			break;
		appended_chars += CopyText(dst_ptr, last.ptr + last.len, results[0].ptr);

		if (appended_chars + results[0].len > max_chars)
			break;
		appended_chars += CopyResult(dst_ptr, results[0], start_tag, end_tag);

		last = results.Remove(0);
	}

	// Copy the rest of the string until end
	CopyText(dst_ptr, (last.ptr + last.len), end);

	return dst.Set(buffer);
}

/**
 * Copies text compressing whitespaces and returning the number
 * of characters copied.
 */
int PhraseMatcher::CopyText(uni_char *&dst_ptr, const uni_char *from, const uni_char *end)
{
	int appended_chars = 0;

	while (from < end)
	{
		if (uni_isspace(*from))
		{
			*dst_ptr++ = ' ';
			from++;

			// Skip the additional spaces
			while (uni_isspace(*from))
				from++;
		}
		else
		{
			*dst_ptr++ = *from++;
		}

		appended_chars++;
	}

	*dst_ptr = 0;

	return appended_chars;
}

/**
 * Copies a search result with formatting into the destination text and
 * returning number of characters copied - not including formatting
 * tags
 */
int PhraseMatcher::CopyResult(uni_char *&dst_ptr, Word &result, const OpStringC &start_tag, const OpStringC &end_tag)
{
	int appended_chars = 0;

	// Add on the prefix tag
	uni_strcpy(dst_ptr, start_tag.CStr());
	dst_ptr += start_tag.Length();

	// Add on the result itself
	appended_chars += CopyText(dst_ptr, result.ptr, result.ptr+result.len);

	// Add on the suffix tag
	uni_strcpy(dst_ptr, end_tag.CStr());
	dst_ptr += end_tag.Length();

	return appended_chars;
}

/**
 * Finds a uni_char pointer to the end of the text where compressing
 * of whitespaces is assumed.
 */
const uni_char *PhraseMatcher::FindEnd(const uni_char *start, int length)
{
	const uni_char *end = start;

	while (length > 0 && *end)
	{
		end++;
		if (uni_isspace(*(end-1)))
		{
			while (uni_isspace(*end))
				end++;
		}

		length--;
	}

	return end;
}

/**
 * Finds an appropriate start for a quote, attempting to get some context
 * for the first matching word which it assumes is the word pointed to by
 * first_start
 */
const uni_char *PhraseMatcher::FindStart(const uni_char *text, const uni_char *first_start, int length, int prefix_ratio)
{
	const uni_char *start = first_start;

	if (prefix_ratio <= 0 || first_start == NULL)
		return text;

	// Start some place before the first match, but don't cross "excerpt boundary" ('\r')
	while (start > text && start > first_start-length/prefix_ratio && *(start-1) != '\r')
		start --;

	while (start < first_start && !WordSegmenter::WordBreak(text, start))
		start++;

	while(uni_isspace(*start))
		start++;

	return start;
}

#ifdef SELFTEST
uni_char *PhraseMatcher::GetPhrases() const
{
	OpString s;
	for (UINT32 i=0; i < m_phrases.GetCount(); i++)
	{
		TVector<Word> *phrase = m_phrases[i];
		if (phrase->GetCount() > 1)
			s.Append("\"");
		for (UINT32 j=0; j < phrase->GetCount(); j++)
		{
			s.Append((*phrase)[j].ptr, (*phrase)[j].len);
			if (j < phrase->GetCount()-1)
				s.Append(" ");
		}
		if (phrase->GetCount() > 1)
			s.Append("\"");
		if (i < m_phrases.GetCount()-1)
			s.Append(" ");
	}
	if (m_phrases.GetCount() != 0)
	{
		TVector<Word> *last_phrase = m_phrases[m_phrases.GetCount()-1];
		Word &last_word = (*last_phrase)[last_phrase->GetCount()-1];
		if (last_word.is_last)
			s.Append("-");
	}
	return UniSetNewStr(s.IsEmpty() ? UNI_L("") : s.CStr());
}
#endif // SELFTEST

#endif // defined(SEARCH_ENGINE_PHRASESEARCH) || defined(USE_SEARCH_ENGINE_WORDHIGHLIGHTER)
