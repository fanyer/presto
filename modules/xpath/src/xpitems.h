/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPITEMS_H
#define XPITEMS_H

class XPath_Value;
class XPath_Node;
class XPath_Context;

template <class XPath_Item>
class XPath_Items
{
private:
  XPath_Item **blocks, *firstfree;
  unsigned blocksused, blockstotal;

#ifdef _DEBUG
  unsigned itemsfree, itemstotal;
#endif // _DEBUG

public:
  XPath_Items ();
  ~XPath_Items ();

  XPath_Item *GetL ();
  void Free (XPath_Context *context, XPath_Item *value);
  void Clean ();

#ifdef _DEBUG
  BOOL IsAllFree ();
#endif // _DEBUG
};

typedef XPath_Items<XPath_Value> XPath_Values;
typedef XPath_Items<XPath_Node> XPath_Nodes;

#endif // XPITEMS_H
