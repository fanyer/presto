/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef PREFSMAP_USE_HASH

#include "modules/prefsfile/impl/backend/prefshashfunctions.h"
#include "modules/util/hash.h"

UINT32 PrefsHashFunctions::Hash(const void *k)
{
	return static_cast<UINT32>(djb2hash(*static_cast<const uni_char * const *>(k)));
}

BOOL PrefsHashFunctions::KeysAreEqual(const void *key1, const void *key2)
{
	return uni_strcmp(*static_cast<const uni_char * const *>(key1),
	                  *static_cast<const uni_char * const *>(key2)) == 0;
}

#endif
