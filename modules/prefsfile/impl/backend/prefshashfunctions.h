/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined PREFSHASHFUNCTIONS_H && defined PREFSMAP_USE_HASH
#define PREFSHASHFUNCTIONS_H

#include "modules/util/OpHashTable.h"
#include "modules/util/smartptr.h"

/** Hashing function for preference functionality used by OpHashTable.
  * This is a singleton class with no internal data, the instance is
  * needed for the virtual function table only.
  *
  * Since the pointer to the key string can change during the lifetime
  * of a PrefsEntry even though the string it points to cannot, our
  * hashing key is a double pointer instead of a single pointer. This
  * saves us the problem from having to re-hash when we change the data
  * of an entry.
  */
class PrefsHashFunctions
	: public OpHashFunctions
{
public:
	// Inherited interfaces --
	virtual UINT32 Hash(const void *);
	virtual BOOL KeysAreEqual(const void *, const void *);
};

#endif
