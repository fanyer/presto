/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#if !defined ST_SEARCH_NET_H && defined DESKTOP_UTIL_SEARCH_ENGINES
#define ST_SEARCH_NET_H

#include "adjunct/desktop_util/search/search_net.h"

class PrefsFile;
class UserSearches;

/***********************************************************************************
 ** SearchTemplate
 ** --------------
 **
 **
 **
 **
 ***********************************************************************************/

class ST_SearchTemplate : public SearchTemplate
{

public:

  ST_SearchTemplate(BOOL filtered = FALSE) : SearchTemplate(filtered) {}

  void SetIsPackageSearch() { m_store = PACKAGE; }

  void SetIsCustomSearch() { m_store = CUSTOM; }
};

#endif
