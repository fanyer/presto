/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#if !defined SEARCH_TYPES_H && !defined SEARCH_ENGINES
#define SEARCH_TYPES_H

enum SearchType
{
	SEARCH_TYPE_GOOGLE = 0,
/*
	SEARCH_TYPE_FAST, //1
	SEARCH_TYPE_SUPERSEARCH, //2
	SEARCH_TYPE_AMAZON, //3
	SEARCH_TYPE_PRICE_COMPARISON, //4
	SEARCH_TYPE_SOFTWARE, //5
	SEARCH_TYPE_DOMAINNAME, //6
	//SEARCH_TYPE_STOCKQUOTE,
	SEARCH_TYPE_IMAGE, //7
	SEARCH_TYPE_VIDEO, //8
	SEARCH_TYPE_MP3AUDIO, //9
	SEARCH_TYPE_GOOGLEGROUPS, //10
	SEARCH_TYPE_OPERASUPPORT, //11
*/
	SEARCH_TYPE_INPAGE = 12,  //12
//	SEARCH_TYPE_NEWS, //13, inserted here 2001-01-31
	SEARCH_TYPE_HISTORY = 14,

	// being able to add searches to toolbar.ini independent of search.ini

	SEARCH_TYPE_FIRST_STARTBAR_POS = 40,
	SEARCH_TYPE_SECOND_STARTBAR_POS = 41,

	//below not in the search-menu but in selectionmenu:

	SEARCH_TYPE_DICTIONARY = 50,
	SEARCH_TYPE_ENCYCLOPEDIA = 51,
	SEARCH_TYPE_MONEY = 52,
	SEARCH_TYPE_OPEN_AS_URL = 53,
	SEARCH_TYPE_SEND_MAIL = 54,

	// then comes translations

	SEARCH_TYPE_TRANSLATE_FIRST = 100,
	SEARCH_TYPE_TRANSLATE_LAST = SEARCH_TYPE_TRANSLATE_FIRST + 19,

	SEARCH_TYPE_DEFAULT = 200,
	SEARCH_TYPE_MAX	= 300,		// always the last

	SEARCH_TYPE_SEARCH = INT_MAX-1,
	SEARCH_TYPE_UNDEFINED = INT_MAX
};

#endif // !SEARCH_TYPES_H
