/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPSCAN_H
#define XPSCAN_H

#include "modules/xpath/src/xpdefs.h"

class XPath_XMLTreeAccessorFilter;

class XPath_Scan
{
private:
  XPath_XMLTreeAccessorFilter **filters;
  unsigned filters_count;
  unsigned current_index;
  BOOL initial;
  XMLTreeAccessor::Node **current_nodes;

  XPath_Scan (XPath_XMLTreeAccessorFilter **filters, unsigned filters_count);

  XMLTreeAccessor::Node *Prepare (XMLTreeAccessor *tree, XMLTreeAccessor::Node *origin);
  XMLTreeAccessor::Node *Handle (XMLTreeAccessor *tree, XMLTreeAccessor::Node *result, BOOL &finished);

public:
  static XPath_Scan *MakeL (XPath_XMLTreeAccessorFilter **filters, unsigned filters_count);

  ~XPath_Scan ();

  XMLTreeAccessor::Node *GetParent (XMLTreeAccessor *tree, XMLTreeAccessor::Node *child);
  /**< Filters parent axis of 'chidl'. */
  XMLTreeAccessor::Node *GetAncestor (XMLTreeAccessor *tree, XMLTreeAccessor::Node *descendant);
  /**< Filters ancestor axis of 'descendant'. */
  XMLTreeAccessor::Node *GetAncestorOrSelf (XMLTreeAccessor *tree, XMLTreeAccessor::Node *descendant);
  /**< Filters ancestor-or-self axis of 'descendant'. */
  XMLTreeAccessor::Node *GetPrecedingSibling (XMLTreeAccessor *tree, XMLTreeAccessor::Node *sibling);
  /**< Filters previous-sibling axis of 'sibling'. */
  XMLTreeAccessor::Node *GetChild (XMLTreeAccessor *tree, XMLTreeAccessor::Node *parent);
  /**< Filters child axis of 'parent'. */
  XMLTreeAccessor::Node *GetDescendant (XMLTreeAccessor *tree, XMLTreeAccessor::Node *ancestor);
  /**< Filters descendant axis of 'ancestor'. */

  void Reset ();
  /**< Resets scan.  Required before changing axis or "context node." */
};

#endif // XPSCAN_H
