/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPNAMESPACES_H
#define XPNAMESPACES_H

#include "modules/xpath/src/xpdefs.h"

#include "modules/util/adt/opvector.h"
#include "modules/util/OpHashTable.h"

class XPath_Namespace
{
public:
  XPath_Namespace ();
  ~XPath_Namespace ();

  uni_char *prefix, *uri;
};

class XPath_Namespaces
  : public OpHashFunctions
{
private:
  OpVector<XPath_Namespace> indexed;
  OpHashTable hashed;
  unsigned refcount;

public:
  XPath_Namespaces ();
  ~XPath_Namespaces ();

  void SetL (const uni_char *prefix, const uni_char *uri);
  void RemoveL (const uni_char *prefix);
  void SetURIL (unsigned index, const uni_char *uri);
  //void CollectPrefixesFromElementL (XPath_Element *element);
  void CollectPrefixesFromExpressionL (const uni_char *expression);

  unsigned GetCount ();
  const uni_char *GetPrefix (unsigned index);
  const uni_char *GetURI (unsigned index);
  const uni_char *GetURI (const uni_char *prefix);

  virtual UINT32 Hash (const void *key);
  virtual BOOL KeysAreEqual (const void *key1, const void *key2);

  static XPath_Namespaces *IncRef (XPath_Namespaces *namespaces);
  static void DecRef (XPath_Namespaces *namespaces);
};

#endif // XPNAMESPACES_H
