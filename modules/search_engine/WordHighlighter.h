/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef WORDHIGHLIGHTER_H
#define WORDHIGHLIGHTER_H

#include "modules/search_engine/Vector.h"
#include "modules/search_engine/PhraseSearch.h"

/**
 * @brief Highlight a searched phrase in a plaintext excerpt
 * @author Pavel Studeny <pavels@opera.com>
 *
 * Call Init with the search phrase and then AppendHighlight for each result.
 */
class WordHighlighter : public PhraseMatcher
{
public:
	/**
	 * initialize with a search query
	 * @param query the same query used for searching
	 * @param phrase_flags flags built up from PhraseFlags, controlling which phrases are matched.
	 *    Since it is meant for word highlighting, the flag FullSearch is automatically added
	 * @return OK if there were no errors
	 */
	CHECK_RESULT(OP_STATUS Init(const uni_char *query, int phrase_flags = NoPhrases | PrefixSearch))
	{
		return PhraseMatcher::Init(query, phrase_flags | FullSearch);
	}

    /**
     * Search the haystack for the all the words in the query
     * @param haystack to search
     * @return true if all the words are present in the haystack
     */
    BOOL ContainsWords(OpStringC& haystack) const
    {
    	return Matches(haystack.CStr());
    }
};

#endif  // WORDHIGHLIGHTER_H

