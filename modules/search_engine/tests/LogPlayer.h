/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef LOGPLAYER_H
#define LOGPLAYER_H

#include "modules/search_engine/StringTable.h"

#if defined SEARCH_ENGINE_FOR_MAIL && defined SELFTEST && defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)

struct StringTablePlayer
{
	OP_STATUS Play(StringTable &table, const uni_char *path, const uni_char *table_name, const uni_char *log_name, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER);

	int error_count;
};

#endif  // SEARCH_ENGINE_FOR_MAIL

#endif  // LOGPLAYER_H

