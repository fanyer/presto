/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef _SEARCH_ENGINE_CAPABILITIES_H_
#define _SEARCH_ENGINE_CAPABILITIES_H_

/* WordSegmenter::WordBreak(...) works correctly over grapheme cluster boundaries */
#define SEARCH_ENGINE_CAP_GRAPHEME_CLUSTER_WORDBREAK

#define SEARCH_ENGINE_CAP_WH_CONTAINS_WORDS

// StringTable::Recover() and SingleBTree::Recover()
#define SEARCH_ENGINE_CAP_CONTAINS_RECOVERY

// Has FilterIterator and SearchFilter
#define SEARCH_ENGINE_CAP_FILTERITERATOR

// Has VisitedSearch::Result::GetPlaintext
#define SEARCH_ENGINE_CAP_VISITEDSEARCH_GETPLAINTEXT

#endif // _SEARCH_ENGINE_CAPABILITIES_H_

