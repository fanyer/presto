/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPUTILS_H
#define XPUTILS_H

#include "modules/xpath/xpath.h"
#include "modules/xpath/src/xpdefs.h"
#include "modules/xpath/src/xpnode.h"

class XPath_Token;

class XPath_Utils
{
public:
  static uni_char *CopyStringL (const uni_char *string, unsigned length = ~0u);
  static unsigned HashString (const uni_char *string);

  static XPath_Axis GetAxis (const XPath_Token &token);
  static XPath_NodeType GetNodeType (const XPath_Token &token);
  static XPath_NodeType GetNodeType (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode);
  static XMLTreeAccessor::NodeType ConvertNodeType (XPath_NodeType nodetype);
  static XMLTreeAccessor::NodeType ConvertNodeType (XPathNode::Type nodetype);

  static BOOL IsLanguage (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, const uni_char *language);
  static BOOL IsId (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, const uni_char *id);

  static double Round (double number);
};

#endif // XPUTILS_H
