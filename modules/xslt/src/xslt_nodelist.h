/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_NODELIST_H
#define XSLT_NODELIST_H

class XPathNode;

// Make a flag that controls if it owns or just refers to the nodes? They are reference counted anyway so that shouldn't be ncessary
class XSLT_NodeList
{
private:
  XPathNode **nodes;
  unsigned nodes_used;
  unsigned nodes_total;
  unsigned offset;

public:
  XSLT_NodeList()
    : nodes (0),
      nodes_used (0),
      nodes_total (0),
      offset (0)
  {
  }

  ~XSLT_NodeList ();

  void AddL (XPathNode *node) { LEAVE_IF_ERROR (Add (node)); }
  OP_STATUS Add (XPathNode *node);
  OP_STATUS AddAll (XSLT_NodeList &other);
  static XSLT_NodeList *CopyL (XSLT_NodeList &other);
  void SetOffset (unsigned offset0) { offset = offset0; }

  unsigned GetCount () { return nodes_used + offset; }
  XPathNode *Get (unsigned index) { return nodes[index - offset]; }
  XPathNode *Steal (unsigned index) { XPathNode *node = nodes[index - offset]; nodes[index - offset] = 0; return node; }
  XPathNode **GetFlatArray () { return nodes; }
};

#endif // XSLT_NODELIST_H
