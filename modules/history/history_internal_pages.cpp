/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
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
//
// Note that any pages added here should be added to the selftests
//___________________________________________________________________________

OP_STATUS HistoryModel::MakeHistoryInternalPages(HistoryInternalPage *& history_internal_pages, int & size)
{

	HistoryInternalPage internal_pages[] =
		{ /*  Title                      URL                     */

			HistoryInternalPage( Str::MI_IDM_HistoryWindow, UNI_L("opera:history")        ),

# ifdef OPERAHISTORYSEARCH_URL
			HistoryInternalPage( Str::S_HISTORY_SEARCH,     UNI_L("opera:historysearch")  ),
# endif //OPERAHISTORYSEARCH_URL

# ifdef OPERACONFIG_URL
			HistoryInternalPage( Str::S_CONFIG_TITLE,       UNI_L("opera:config")         ),
# endif //OPERACONFIG_URL

#ifdef CPUUSAGETRACKING
			HistoryInternalPage( Str::S_IDABOUT_CPU,        UNI_L("opera:cpu")            ),
#endif // CPUUSAGETRACKING

# ifdef OPERAWIDGETS_URL
			HistoryInternalPage( Str::S_WIDGETS_TITLE,      UNI_L("opera:widgets")        ),
# endif //OPERAWIDGETS_URL

# ifdef OPERAUNITE_URL
			HistoryInternalPage( Str::S_UNITE_TITLE,        UNI_L("opera:unite")          ),
# endif //OPERAUNITE_URL
# ifdef OPERAEXTENSIONS_URL
			HistoryInternalPage( Str::S_EXTENSIONS_TITLE,   UNI_L("opera:extensions")     ),
# endif //OPERAEXTENSIONS_URL

			HistoryInternalPage( Str::MI_IDM_About,         UNI_L("opera:about")          ),

# ifdef _PLUGIN_SUPPORT_
			HistoryInternalPage( Str::MI_IDM_PluginsWindow, UNI_L("opera:plugins")        ),
# endif //_PLUGIN_SUPPORT_

# ifdef ABOUT_OPERA_DEBUG
			HistoryInternalPage( Str::S_IDDEBUG_TITLE,      UNI_L("opera:debug")          ),
# endif // ABOUT_OPERA_DEBUG

# ifdef SELFTEST
			HistoryInternalPage( Str::S_SELFTEST_TITLE,     UNI_L("opera:selftest")       ),
# endif // SELFTEST

#ifdef CLIENTSIDE_STORAGE_SUPPORT
			HistoryInternalPage( Str::S_WEBSTORAGE_TITLE,  UNI_L("opera:webstorage")      ),
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DATABASE_STORAGE_SUPPORT
			HistoryInternalPage( Str::S_WEBDATABASES_TITLE,  UNI_L("opera:webdatabases")  ),
#endif // DATABASE_STORAGE_SUPPORT

#ifdef HELP_SUPPORT
			HistoryInternalPage( Str::M_OPERA_HELP,  UNI_L("opera:help")                  ),
#endif // HELP_SUPPORT

#ifdef OPERABOOKMARKS_URL
			HistoryInternalPage( Str::SI_IDSTR_HL_BOOKMARKS_ROOT_FOLDER_NAME, UNI_L("opera:bookmarks") ),
#endif // OPERABOOKMARKS_URL

#ifdef OPERASPEEDDIAL_URL
			HistoryInternalPage( Str::S_SPEED_DIAL,         UNI_L("opera:speeddial")      ),
#endif // OPERASPEEDDIAL_URL

			HistoryInternalPage( Str::SI_CACHE_TITLE_TEXT,  UNI_L("opera:cache")          )

		};

	history_internal_pages = OP_NEWA(HistoryInternalPage, ARRAY_SIZE(internal_pages));

	if(!history_internal_pages)
		return OpStatus::ERR_NO_MEMORY;

	for(UINT i = 0; i < ARRAY_SIZE(internal_pages); i++)
	{
		history_internal_pages[i] = internal_pages[i];
	}

	size = ARRAY_SIZE(internal_pages);

	return OpStatus::OK;

}

#endif // HISTORY_SUPPORT
