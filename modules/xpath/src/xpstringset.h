/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPSTRINGSET_H
#define XPSTRINGSET_H

#include "modules/util/OpHashTable.h"

class XPath_StringSet
  : public OpGenericStringHashTable
{
public:
  XPath_StringSet () : OpGenericStringHashTable(TRUE) {}
  ~XPath_StringSet ();

  BOOL AddL (const uni_char *string);
  BOOL Contains (const uni_char *string);
  void Clear ();
};

#endif // XPSTRINGSET_H
