/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPNODE_H
#define XPNODE_H

#include "modules/xpath/src/xpdefs.h"

#include "modules/xmlutils/xmltreeaccessor.h"

class XPath_Context;

class XPath_Node
{
protected:
  unsigned refcount:30;
  unsigned temporary:1;
  unsigned independent:1;

public:
  XPath_NodeType type;

  XMLTreeAccessor *tree;
  XMLTreeAccessor::Node *treenode;
  XMLCompleteName name;

  /* This struct is here just to make X.data.nextfree compatible between
     XPath_Value (which actually has a 'data' union) and XPath_Node. */
  struct
  {
    XPath_Node *nextfree;
  } data;

  static XPath_Node *NewL (XPath_Context *context, BOOL independent = FALSE);
  static XPath_Node *NewL (XPath_Context *context, XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, BOOL independent = FALSE);
  static void Free (XPath_Context *context, XPath_Node *value);

  XPath_Node (BOOL independent = FALSE);
  XPath_Node (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, BOOL independent = FALSE);
  ~XPath_Node ();

  BOOL IsValid () { return tree != 0; }
  void Reset ();

  void SetFree (XPath_Context *context, XPath_Node *nextfree);

  void Set (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode);

  static XPath_Node *MakeL (XPath_Context *context, XPath_Node *node, BOOL independent = FALSE);
  static XPath_Node *MakeL (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode);

  void SetAttributeL (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, const XMLCompleteName &name);
  void SetNamespaceL (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, const uni_char *prefix, const uni_char *uri);
  void CopyL (XPath_Node *other);

  BOOL IsTemporary () { return temporary != 0; }
  BOOL IsIndependent () { return independent != 0; }
  XPath_NodeType GetType ();

  unsigned GetHash ();
  unsigned GetAttributeIndex ();
  unsigned GetNamespaceIndex ();

  void GetExpandedName (XMLExpandedName &expandedname);
  void GetQualifiedNameL (TempBuffer &qname);
  void GetStringValueL (TempBuffer &value);
  OP_STATUS GetStringValue (TempBuffer &value);
  BOOL HasStringValueL (const uni_char *value);
  BOOL IsWhitespaceOnly ();

  XMLTreeAccessor::Node *GetTreeNodeByIdL (const uni_char *id, unsigned id_length);

  static BOOL Equals (XPath_Node *node1, XPath_Node *node2);
  static BOOL Precedes (XPath_Node *node1, XPath_Node *node2);
  static BOOL SameExpandedName (XPath_Node *node1, XPath_Node *node2);

  static XPath_Node *IncRef (XPath_Node *node);
  static void DecRef (XPath_Context *context, XPath_Node *node);
};

#endif // XPNODE_H
