/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#include "core/pch.h"

#ifdef HISTORY_SUPPORT

#include "modules/history/src/HistoryModel.h"
#include "modules/history/history_structs.h"

//___________________________________________________________________________
// Supported prefixes in history : 
// 
// Make sure that the toplevel prefix (eg http://) is listed before a contained
// prefix (eg http://www.).
//___________________________________________________________________________

OP_STATUS HistoryModel::MakeHistoryPrefixes(HistoryPrefix *& history_prefixes, int & size)
{
	// --------------------
	// Calculate the arrays size
	// --------------------

	UINT array_size = 14;

#ifndef NO_FTP_SUPPORT
	array_size += 4;
#endif //NO_FTP_SUPPORT

#if defined(_LOCALHOST_SUPPORT_) && defined(HISTORY_FILE_SUPPORT)
	array_size += 3;
#endif // _LOCALHOST_SUPPORT_ && HISTORY_FILE_SUPPORT

	// --------------------
	// Allocate the array
	// --------------------

	history_prefixes = OP_NEWA(HistoryPrefix, array_size);
	if(!history_prefixes)
		return OpStatus::ERR_NO_MEMORY;

	// --------------------
	// Fill in the array
	// --------------------

	UINT i = 0;
	
	//                                    Prefix                      Suffix             Prefix/   Protocol      Show_all
	//                                                                                   Suffix
	//                                                                                   Lengths
	history_prefixes[i++] = HistoryPrefix(UNI_L(""),                 UNI_L(""),           0, 0,    NONE_PROTOCOL,  FALSE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("opera:"),           UNI_L(""),           6, 0,    OPERA_PROTOCOL, TRUE);
#if defined(_LOCALHOST_SUPPORT_) && defined(HISTORY_FILE_SUPPORT)
	history_prefixes[i++] = HistoryPrefix(UNI_L("file:/"),           UNI_L(""),           6, 0,    FILE_PROTOCOL,  TRUE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("file://"),          UNI_L(""),           7, 0,    FILE_PROTOCOL,  TRUE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("file://localhost"), UNI_L("localhost"), 16, 9,    FILE_PROTOCOL,  TRUE);
#endif // _LOCALHOST_SUPPORT_ && HISTORY_FILE_SUPPORT
#ifndef NO_FTP_SUPPORT
	history_prefixes[i++] = HistoryPrefix(UNI_L("ftp://"),           UNI_L(""),           6, 0,    FTP_PROTOCOL,   TRUE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("ftp://ftp."),       UNI_L("ftp."),      10, 4,    FTP_PROTOCOL,   TRUE);
#endif //NO_FTP_SUPPORT
	history_prefixes[i++] = HistoryPrefix(UNI_L("http://"),          UNI_L(""),           7, 0,    HTTP_PROTOCOL,  FALSE);
#ifndef NO_FTP_SUPPORT
	history_prefixes[i++] = HistoryPrefix(UNI_L("http://ftp."),      UNI_L("ftp."),      11, 4,    HTTP_PROTOCOL,  TRUE);
#endif //NO_FTP_SUPPORT
	history_prefixes[i++] = HistoryPrefix(UNI_L("http://home."),     UNI_L("home."),     12, 5,    HTTP_PROTOCOL,  TRUE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("http://wap."),      UNI_L("wap."),      11, 4,    HTTP_PROTOCOL,  TRUE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("http://web."),      UNI_L("web."),      11, 4,    HTTP_PROTOCOL,  TRUE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("http://www."),      UNI_L("www."),      11, 4,    HTTP_PROTOCOL,  FALSE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("http://www2."),     UNI_L("www2."),     12, 5,    HTTP_PROTOCOL,  FALSE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("https://"),         UNI_L(""),           8, 0,    HTTPS_PROTOCOL, FALSE);
#ifndef NO_FTP_SUPPORT
	history_prefixes[i++] = HistoryPrefix(UNI_L("https://ftp."),     UNI_L("ftp."),      12, 4,    HTTPS_PROTOCOL, TRUE);
#endif //NO_FTP_SUPPORT
	history_prefixes[i++] = HistoryPrefix(UNI_L("https://home."),    UNI_L("home."),     13, 5,    HTTPS_PROTOCOL, TRUE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("https://wap."),     UNI_L("wap."),      12, 4,    HTTPS_PROTOCOL, TRUE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("https://web."),     UNI_L("web."),      12, 4,    HTTPS_PROTOCOL, TRUE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("https://www."),     UNI_L("www."),      12, 4,    HTTPS_PROTOCOL, FALSE);
	history_prefixes[i++] = HistoryPrefix(UNI_L("https://www2."),    UNI_L("www2."),     13, 5,    HTTPS_PROTOCOL, FALSE);

	// --------------------
	// Do some sanity checking
	// --------------------

	OP_ASSERT(i == array_size);

	// --------------------
	// Set the size parameter
	// --------------------

	size = array_size;

	return OpStatus::OK;
}

#endif // HISTORY_SUPPORT
