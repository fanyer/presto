/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpnode.h"
#include "modules/xpath/src/xputils.h"
#include "modules/xpath/src/xpcontext.h"

#include "modules/util/tempbuf.h"
#include "modules/xmlutils/xmlutils.h"

#ifdef _DEBUGX
# define CHECK_NODE() \
  do \
    { \
      if (tree && treenode) \
        { \
          XMLTreeAccessor::NodeType treenode_type = tree->GetNodeType (treenode); \
          if (treenode_type == XMLTreeAccessor::TYPE_TEXT || treenode_type == XMLTreeAccessor::TYPE_CDATA_SECTION) \
            if (XMLTreeAccessor::Node *previous_sibling = tree->GetPreviousSibling (treenode)) \
              { \
                XMLTreeAccessor::NodeType previous_sibling_type = tree->GetNodeType (previous_sibling); \
                if (previous_sibling_type == XMLTreeAccessor::TYPE_TEXT || previous_sibling_type == XMLTreeAccessor::TYPE_CDATA_SECTION) \
                  OP_ASSERT (FALSE); \
              } \
        } \
    } \
  while (0)
#else // _DEBUG
# define CHECK_NODE()
#endif // _DEBUG

/* static */ XPath_Node *
XPath_Node::NewL (XPath_Context *context, BOOL independent)
{
  if (!independent)
    {
      XPath_Node *node = context->global->nodes_cache.GetL ();
      node->refcount = 1;
      return node;
    }
  else
    return OP_NEW_L (XPath_Node, (TRUE));
}

/* static */ XPath_Node *
XPath_Node::NewL (XPath_Context *context, XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, BOOL independent)
{
  XPath_Node *node = NewL (context, independent);
  node->Set (tree, treenode);
  return node;
}

/* static */ void
XPath_Node::Free (XPath_Context *context, XPath_Node *node)
{
  //OP_ASSERT (!node->temporary);
  //node->refcount = 0xffff;

  if (node->independent)
    OP_DELETE (node);
  else if (!node->temporary)
    context->global->nodes_cache.Free (context, node);
}

XPath_Node::XPath_Node (BOOL independent)
  : refcount (1),
    temporary (TRUE),
    independent (independent),
    type (XP_NODE_ROOT),
    tree (0),
    treenode (0)
{
}

XPath_Node::XPath_Node (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, BOOL independent)
  : refcount (1),
    temporary (TRUE),
    independent (independent),
    type (XPath_Utils::GetNodeType (tree, treenode)),
    tree (tree),
    treenode (treenode)
{
  /* Make sure the node actually exists. */
  CHECK_NODE ();
}

XPath_Node::~XPath_Node ()
{
  /* Nice assertion, but I suspect it will fail "incorrectly" on execution
     errors and OOM. */
  OP_ASSERT (temporary ? refcount == 1 : refcount == 0);

  Reset ();
}

void
XPath_Node::Reset ()
{
  type = XP_NODE_ROOT;
  tree = 0;
  treenode = 0;
  name = XMLCompleteName ();
}

void
XPath_Node::SetFree (XPath_Context *context, XPath_Node *nextfree0)
{
  Reset ();

  data.nextfree = nextfree0;
}

void
XPath_Node::Set (XMLTreeAccessor *tree0, XMLTreeAccessor::Node *treenode0)
{
  Reset ();

  if (treenode0)
    {
      type = XPath_Utils::GetNodeType (tree0, treenode0);
      tree = tree0;
      treenode = treenode0;

      /* Make sure the node actually exists. */
      CHECK_NODE ();
    }
}

/* static */ XPath_Node *
XPath_Node::MakeL (XPath_Context *context, XPath_Node *node, BOOL independent)
{
  if (node->temporary || !independent != !node->independent)
    {
      XPath_Node *copy = NewL (context, node->tree, node->treenode, independent);

      if (node->type == XP_NODE_ATTRIBUTE || node->type == XP_NODE_NAMESPACE)
        {
          copy->type = node->type;
          copy->name.SetL (node->name);
        }

      copy->temporary = FALSE;
      return copy;
    }
  else
    return IncRef (node);
}

/* static */ XPath_Node *
XPath_Node::MakeL (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode)
{
  XPath_Node *node = NewL (0, tree, treenode, TRUE);
  node->temporary = FALSE;
  return node;
}

void
XPath_Node::SetAttributeL (XMLTreeAccessor *tree0, XMLTreeAccessor::Node *treenode0, const XMLCompleteName &name0)
{
  Reset ();

  name.SetL (name0);

  type = XP_NODE_ATTRIBUTE;
  tree = tree0;
  treenode = treenode0;
}

void
XPath_Node::SetNamespaceL (XMLTreeAccessor *tree0, XMLTreeAccessor::Node *treenode0, const uni_char *prefix, const uni_char *uri)
{
  Reset ();

  XMLCompleteName temporary (uri, NULL, prefix ? prefix : UNI_L (""));
  name.SetL (temporary);

  type = XP_NODE_NAMESPACE;
  tree = tree0;
  treenode = treenode0;
}

void
XPath_Node::CopyL (XPath_Node *other)
{
  if (this != other)
    {
      Set (other->tree, other->treenode);

      if (other->type == XP_NODE_ATTRIBUTE || other->type == XP_NODE_NAMESPACE)
        {
          type = other->type;
          name.SetL (other->name);
        }
    }
}

XPath_NodeType
XPath_Node::GetType ()
{
  return type;
}

unsigned
XPath_Node::GetHash ()
{
  /* Ignoring 'tree' here.  All tree nodes are unique objects anyway,
     regardless of which tree they belong to. */
  unsigned hash = XP_HASH_POINTER (treenode);

  if (type == XP_NODE_ATTRIBUTE || type == XP_NODE_NAMESPACE)
    /* Skipping prefix for attributes here to make the hash calculation
       cheaper.  Including it probably wouldn't make the hashing much less
       prone to collisions anyway.  And besides, most attributes probably
       don't have prefixes. */
    hash = hash ^ XPath_Utils::HashString (name.GetLocalPart ()) ^ XPath_Utils::HashString (name.GetUri ());

  return hash;
}

unsigned
XPath_Node::GetAttributeIndex ()
{
  return tree->GetAttributeIndex (tree->GetAttributes (treenode, FALSE, TRUE), name);
}

unsigned
XPath_Node::GetNamespaceIndex ()
{
  return tree->GetNamespaceIndex (treenode, name.GetUri (), name.GetLocalPart ());
}

void
XPath_Node::GetExpandedName (XMLExpandedName &name0)
{
  const uni_char *localpart = 0;

  switch (type)
    {
    case XP_NODE_ATTRIBUTE:
      name0 = name;
      return;

    case XP_NODE_NAMESPACE:
      localpart = name.GetLocalPart ();
      break;

    case XP_NODE_PI:
      localpart = tree->GetPITarget (treenode);
      break;

    default:
      break;

    case XP_NODE_ELEMENT:
      XMLCompleteName completename;
      tree->GetName (completename, treenode);
      name0 = completename;
      return;
    }

  name0 = XMLExpandedName (localpart);
}

void
XPath_Node::GetQualifiedNameL (TempBuffer &qname)
{
  OP_ASSERT (type == XP_NODE_ELEMENT || type == XP_NODE_ATTRIBUTE);

  XMLCompleteName temporary, *completename;

  if (type == XP_NODE_ELEMENT)
    {
      tree->GetName (temporary, treenode);
      completename = &temporary;
    }
  else
    completename = &name;

  const uni_char *prefix = completename->GetPrefix ();
  if (prefix)
    {
      qname.AppendL (prefix);
      qname.AppendL (":");
    }

  qname.AppendL (completename->GetLocalPart ());
}

void
XPath_Node::GetStringValueL (TempBuffer &value)
{
  XMLTreeAccessor::Node *iter = treenode, *first = 0;
  const uni_char *data = 0;
  BOOL id, specified;

  switch (type)
    {
    case XP_NODE_ROOT:
    case XP_NODE_ELEMENT:
      LEAVE_IF_ERROR (tree->GetCharacterDataContent (data, treenode, &value));
      break;

    case XP_NODE_TEXT:
      while (XPath_Utils::GetNodeType (tree, iter) == XP_NODE_TEXT)
        {
          first = iter;

          if (XMLTreeAccessor::Node *previous = tree->GetPreviousSibling (iter))
            iter = previous;
          else
            break;
        }
      while (first && XPath_Utils::GetNodeType (tree, first) == XP_NODE_TEXT)
        {
          LEAVE_IF_ERROR (tree->GetData (data, first, &value));
          if (data != value.GetStorage ())
            /* Tree accessor returned text without generating into 'value' as
               an optimization.  But we really want it in 'value'. */
            value.AppendL (data);
          first = tree->GetNextSibling (first);
        }
      return;

    case XP_NODE_ATTRIBUTE:
      LEAVE_IF_ERROR (tree->GetAttribute (tree->GetAttributes (treenode, FALSE, TRUE), name, data, id, specified, &value));
      break;

    case XP_NODE_NAMESPACE:
      {
        const uni_char *uri = name.GetUri ();
        if (uri)
          value.AppendL (uri);
        return;
      }

    case XP_NODE_PI:
    case XP_NODE_COMMENT:
      LEAVE_IF_ERROR (tree->GetData (data, iter, &value));
      break;
    }

  /* Common case for XP_NODE_ROOT, XP_NODE_ELEMENT, XP_NODE_ATTRIBUTE,
     XP_NODE_PI and XP_NODE_COMMENT: check if the tree accessor generated the
     returned string into the buffer or if it returned a pointer to a string
     owned by the tree accessor. */

  if (data != value.GetStorage ())
    value.AppendL (data);
}

OP_STATUS
XPath_Node::GetStringValue (TempBuffer &value)
{
  TRAPD (status, GetStringValueL (value));
  return status;
}

BOOL
XPath_Node::HasStringValueL (const uni_char *value)
{
  unsigned value_length;

  if (!value)
    value = UNI_L (""), value_length = 0;
  else
    value_length = uni_strlen (value);

  XMLTreeAccessor::Node *iter = treenode, *first = 0;
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);
  const uni_char *data = 0;
  BOOL id, specified;

  switch (type)
    {
    case XP_NODE_ROOT:
    case XP_NODE_ELEMENT:
      LEAVE_IF_ERROR (tree->GetCharacterDataContent (data, treenode, &buffer));
      return uni_strcmp (value, data) == 0;

    case XP_NODE_TEXT:
      while (XPath_Utils::GetNodeType (tree, iter) == XP_NODE_TEXT)
        {
          first = iter;

          if (XMLTreeAccessor::Node *previous = tree->GetPreviousSibling (iter))
            iter = previous;
          else
            break;
        }
      while (first && XPath_Utils::GetNodeType (tree, first) == XP_NODE_TEXT)
        {
          LEAVE_IF_ERROR (tree->GetData (data, first, &buffer));
          unsigned data_length = uni_strlen (data);

          if (value_length < data_length || uni_strncmp (value, data, data_length) != 0)
            return FALSE;

          value += data_length;
          value_length -= data_length;

          buffer.Clear ();
          first = tree->GetNextSibling (first);
        }
      return TRUE;

    case XP_NODE_ATTRIBUTE:
      LEAVE_IF_ERROR (tree->GetAttribute (tree->GetAttributes (treenode, FALSE, TRUE), name, data, id, specified, &buffer));
      return uni_strcmp (value, data) == 0;

    case XP_NODE_NAMESPACE:
      {
        const uni_char *uri = name.GetUri ();
        if (uri)
          return uni_strcmp (value, uri) == 0;
        else
          return value_length == 0;
      }

    case XP_NODE_PI:
    case XP_NODE_COMMENT:
      LEAVE_IF_ERROR (tree->GetData (data, iter, &buffer));
      return uni_strcmp (value, data) == 0;
    }

  return FALSE;
}

BOOL
XPath_Node::IsWhitespaceOnly ()
{
  if (type == XP_NODE_TEXT)
    {
      XMLTreeAccessor::Node *iter = treenode, *first = 0;

      while (XPath_Utils::GetNodeType (tree, iter) == XP_NODE_TEXT)
        {
          first = iter;

          if (XMLTreeAccessor::Node *previous = tree->GetPreviousSibling (iter))
            iter = previous;
          else
            break;
        }

      while (first && XPath_Utils::GetNodeType (tree, first) == XP_NODE_TEXT)
        {
          if (!tree->IsWhitespaceOnly (first))
            return FALSE;

          first = tree->GetNextSibling (first);
        }
    }

  return TRUE;
}


XMLTreeAccessor::Node *
XPath_Node::GetTreeNodeByIdL (const uni_char *id, unsigned id_length)
{
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);
  buffer.AppendL (id, id_length);

  XMLTreeAccessor::Node *node;
  LEAVE_IF_ERROR (tree->GetElementById (node, buffer.GetStorage ()));

  return node;
}

/* static */ BOOL
XPath_Node::Equals (XPath_Node *node1, XPath_Node *node2)
{
  if (node1 == node2)
    return TRUE;
  else if (node1->type == node2->type)
    if (node1->treenode == node2->treenode)
      switch (node1->type)
        {
        case XP_NODE_ATTRIBUTE:
        case XP_NODE_NAMESPACE:
          return node1->name == node2->name;

        default:
          return TRUE;
        }

  return FALSE;
}

/* static */ BOOL
XPath_Node::Precedes (XPath_Node *node1, XPath_Node *node2)
{
  if (Equals (node1, node2))
    return TRUE;
  else if (node1->treenode == node2->treenode)
    {
      if (node1->type == node2->type)
        switch (node1->type)
          {
          case XP_NODE_ATTRIBUTE:
            return node1->GetAttributeIndex () < node2->GetAttributeIndex ();

          case XP_NODE_NAMESPACE:
            return node1->GetNamespaceIndex () < node2->GetNamespaceIndex ();

          default:
            /* Should not happen; nodes of other types with the same element
               should compare equal. */
            OP_ASSERT (FALSE);
            return TRUE;
          }
      else if (node1->type == XP_NODE_NAMESPACE)
        return node2->type == XP_NODE_ATTRIBUTE;
      else if (node2->type == XP_NODE_NAMESPACE)
        return node1->type != XP_NODE_ATTRIBUTE;
      else
        /* One must be an attribute, the other is not an attribute or a
           namespace.  The one that is not an attribute precedes the other. */
        return node2->type == XP_NODE_ATTRIBUTE;
    }
  else if (node1->tree == node2->tree)
    return node1->tree->Precedes (node1->treenode, node2->treenode);
  else
    {
      URL url1 = node1->tree->GetDocumentURL (), url2 = node2->tree->GetDocumentURL ();
      OpString uri1, uri2;

      if (OpStatus::IsSuccess (url1.GetAttribute (URL::KUniName_Username_Password_NOT_FOR_UI, uri1)) &&
          OpStatus::IsSuccess (url2.GetAttribute (URL::KUniName_Username_Password_NOT_FOR_UI, uri2)))
        {
          int diff = uri1.Compare (uri2);

          if (diff < 0)
            return TRUE;
          else if (diff > 0)
            return FALSE;
        }

      /* This should probably never happen.  The specification requires
         "consistent" ordering and this comparison can vary between
         executions, so it is non-compliant.  If it happens, investigate why,
         and if it cannot be avoided, remove the assertion.

         Note that this also happens on OOM.  That just happens because it
         ought to be rare, and if this fails due to OOM, chances are slim the
         rest of whatever we're doing is going to go very far. */
      OP_ASSERT (FALSE);

      return reinterpret_cast<UINTPTR> (node1->tree) < reinterpret_cast<UINTPTR> (node2->tree);
    }
}

/* static */ BOOL
XPath_Node::SameExpandedName (XPath_Node *node1, XPath_Node *node2)
{
  OP_ASSERT (node1->type == node2->type);
  OP_ASSERT (node1->type != XP_NODE_ROOT);
  OP_ASSERT (node1->type != XP_NODE_COMMENT);
  OP_ASSERT (node1->type != XP_NODE_TEXT);

  XMLExpandedName name1, name2;

  node1->GetExpandedName (name1);
  node2->GetExpandedName (name2);

  return name1 == name2;
}

/* static */ XPath_Node *
XPath_Node::IncRef (XPath_Node *node)
{
  OP_ASSERT (node->refcount != 0xffff);

  ++node->refcount;
  return node;
}

/* static */ void
XPath_Node::DecRef (XPath_Context *context, XPath_Node *node)
{
  if (node)
    {
      OP_ASSERT (node->refcount != 0xffff);

      if (!--node->refcount)
        Free (context, node);
    }
}

#endif // XPATH_SUPPORT
