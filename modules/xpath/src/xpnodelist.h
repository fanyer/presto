/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPNODELIST_H
#define XPNODELIST_H

class XPath_Node;
class XPath_NodeSet;
class XPath_Context;
class XMLTreeAccessor;

class XPath_NodeList
{
protected:
  XPath_Node **indexed;
  unsigned count, total;
  unsigned contains_attributes:1;
  unsigned contains_namespaces:1;
  XMLTreeAccessor *tree;

  void TreeSort ();

public:
  XPath_NodeList ();
  ~XPath_NodeList ();

  void AddL (XPath_Context *context, XPath_Node *node);
  void AddL (XPath_Context *context, XPath_NodeList *nodelist);

  void SortL ();
  void Clear (XPath_Context *context);

  XPath_Node **GetNodes () { return indexed; }
  unsigned GetCount () { return count; }
  XPath_Node *Get (unsigned index, BOOL reversed = FALSE) { return indexed[!reversed ? index : count - index - 1]; }
  XPath_Node *GetLast () { return count == 0 ? 0 : indexed[count - 1]; }
  XPath_Node *Pop (unsigned index);
};

#endif // XPNODELIST_H
