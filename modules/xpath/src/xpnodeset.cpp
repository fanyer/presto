/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpnodeset.h"
#include "modules/xpath/src/xpnodelist.h"
#include "modules/xpath/src/xpcontext.h"
#include "modules/xpath/src/xputils.h"

void
XPath_NodeSet::InitL ()
{
  capacity = 8;
  total = 16;
  indexed = OP_NEWA_L (XPath_Node *, capacity + total);
  hashed = indexed + capacity;
  op_memset (hashed, 0, total * sizeof hashed[0]);
}

void
XPath_NodeSet::GrowL ()
{
  XPath_Node **oldindexed = indexed;
  unsigned oldcount = count;

  count = 0;
  capacity = total;
  total += total;
  indexed = OP_NEWA_L (XPath_Node *, capacity + total);
  hashed = indexed + capacity;
  op_memset (hashed, 0, total * sizeof hashed[0]);

  for (unsigned index = 0; index < oldcount; ++index)
    {
      /* No context to AddL (and XPath_Node::MakeL) here.  Only needed if the
         node is temporary (on the stack) and needs to be allocated on the
         heap, and our nodes are always on the heap (since they've already
         been processed by AddL at least once.) */
#ifdef _DEBUG
      BOOL added = AddL (0, oldindexed[index]);
      if (!added)
        AddL (0, oldindexed[index]);
#else // _DEBUG
      AddL (0, oldindexed[index]);
#endif // _DEBUG

      /* No context to DecRef here.  AddL just incremented the reference
         counter to at least 2, so this will never free the node. */
      XPath_Node::DecRef (0, oldindexed[index]);
    }

  OP_DELETEA (oldindexed);
}

/* Don't know if this is a good limit or not.  It really depends on the size
   and shape of the tree in relation to the size of the node set.  But at
   least the "tree sort" is always O(n) (where n is the number of elements in
   the tree).  The merge sort below can be considerably more expensive,
   because node comparison is expensive.  On the other hand, the "tree sort"
   gets complicated if there are attribute or namespace nodes or nodes from
   different trees involved, so it is not supported for node sets containing
   such nodes. */
#define XPATH_TREESORT_LIMIT 32

/* static */ void
XPath_NodeSet::TreeSort (XMLTreeAccessor *tree, XPath_NodeSet *nodeset)
{
  XMLTreeAccessor::Node *iter = tree->GetRoot ();
  XPath_Node temporary, **indexed = nodeset->GetNodes ();

  unsigned index = 0, count = nodeset->GetCount ();

  while (1)
    {
      temporary.Set (tree, iter);

      if (XPath_Node *node = nodeset->Find (&temporary))
        {
          indexed[index++] = node;
          if (index == count)
            return;
        }

      iter = tree->GetNext (iter);
    }
}

XPath_Node *
XPath_NodeSet::Find (XPath_Node *node)
{
  unsigned hash_value = node->GetHash ();
  unsigned hash = hash_value;
  unsigned index = hash;
  unsigned mask = total - 1;

  while (1)
    {
      XPath_Node *entry = hashed[index & mask];

      if (!entry)
        break;
      else if (XPath_Node::Equals (node, entry))
        return entry;

      index = (index << 2) + index + hash + 1;
      hash >>= 5;
    }

  return 0;
}

XPath_NodeSet::XPath_NodeSet ()
  : hashed (0),
    capacity (0)
{
}

BOOL
XPath_NodeSet::AddL (XPath_Context *context, XPath_Node *node)
{
  if (capacity == 0)
    InitL ();

  if (node->GetType () == XP_NODE_ATTRIBUTE)
    contains_attributes = 1;
  else if (node->GetType () == XP_NODE_NAMESPACE)
    contains_namespaces = 1;

  if (count == 0)
    tree = node->tree;
  else if (node->tree != tree)
    tree = 0;

  unsigned hash_value = node->GetHash ();
  unsigned hash = hash_value;
  unsigned index = hash;
  unsigned mask = total - 1;

  while (1)
    {
      XPath_Node *entry = hashed[index & mask];

      if (!entry)
        break;
      else if (XPath_Node::Equals (node, entry))
        return FALSE;

      index = (index << 2) + index + hash + 1;
      hash >>= 5;
    }

  if (count == capacity)
    {
      GrowL ();
      AddL (context, node);
    }
  else
    indexed[count++] = hashed[index & mask] = XPath_Node::MakeL (context, node);

  return TRUE;
}

void
XPath_NodeSet::SortL ()
{
  if (count > XPATH_TREESORT_LIMIT && !contains_attributes && !contains_namespaces && tree)
    TreeSort (tree, this);
  else
    XPath_NodeList::SortL ();
}

void
XPath_NodeSet::Clear (XPath_Context *context)
{
  XPath_NodeList::Clear (context);
  hashed = 0;
  capacity = 0;
}

#endif // XPATH_SUPPORT
