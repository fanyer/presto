/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#ifndef HISTORY_STRUCTS_H
#define HISTORY_STRUCTS_H

#include "modules/history/history_enums.h"
#include "modules/locale/locale-enum.h"

//___________________________________________________________________________
// 
//___________________________________________________________________________
struct HistoryTimePeriod
{
    Str::LocaleString title;  //The title of the folder
    TimePeriod        period; //The period of the folder
    INT               seconds;

    HistoryTimePeriod(Str::LocaleString t, 
		      TimePeriod p, 
		      INT s) 
	: title(t), period(p), seconds(s) {}

    HistoryTimePeriod() : title(Str::NOT_A_STRING), period(TODAY), seconds(0) {}
};

//___________________________________________________________________________
// 
//___________________________________________________________________________
struct HistoryPrefix
{
    const uni_char * prefix;           // The full prefix
    const uni_char * suffix;           // The suffix of the prefix - if any
    INT32            prefix_len;       // The length of the full prefix
    INT32            suffix_len;       // The length of the suffix of the prefix
    HistoryProtocol  protocol;         // The protocol
    BOOL             show_all_matches; // TRUE if matches to this should be shown without typing more

	HistoryPrefix() : prefix(NULL), suffix(NULL), prefix_len(-1), suffix_len(-1), protocol(NONE_PROTOCOL), show_all_matches(FALSE) {}

    HistoryPrefix(const uni_char * p, 
				  const uni_char * s,
				  INT32 p_len,
				  INT32 s_len,
				  HistoryProtocol  pro,
				  BOOL all)
	: prefix(p), suffix(s), prefix_len(p_len), suffix_len(s_len), protocol(pro), show_all_matches(all) {}
};

//___________________________________________________________________________
// 
//___________________________________________________________________________
struct HistoryInternalPage
{
    Str::LocaleString title; //The title to be used in autocompletion etc
    const uni_char *        url;   //The url to be used

    HistoryInternalPage(Str::LocaleString t, 
			const uni_char * u) 
	: title(t), url(u) {}

    HistoryInternalPage() : title(Str::NOT_A_STRING), url(0) {}
};

#endif //HISTORY_STRUCTS_H
