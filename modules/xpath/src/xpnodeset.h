/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPNODESET_H
#define XPNODESET_H

#include "modules/xpath/src/xpnode.h"
#include "modules/xpath/src/xpnodelist.h"

class XPath_NodeSet
  : public XPath_NodeList
{
protected:
  friend class XPath_NodeList;

  XPath_Node **hashed;
  unsigned capacity;

  void InitL ();
  void GrowL ();
  static void TreeSort (XMLTreeAccessor *tree, XPath_NodeSet *nodeset);
  XPath_Node *Find (XPath_Node *node);

public:
  XPath_NodeSet ();

  BOOL AddL (XPath_Context *context, XPath_Node *node);

  void SortL ();
  void Clear (XPath_Context *context);

  BOOL Contains (XPath_Node *node);
};

#endif // XPNODESET_H
