/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpnodelist.h"
#include "modules/xpath/src/xpnodeset.h"
#include "modules/xpath/src/xputils.h"

XPath_NodeList::XPath_NodeList ()
  : indexed (0),
    count (0),
    total (0),
    contains_attributes (0),
    contains_namespaces (0),
    tree (0)
{
}

XPath_NodeList::~XPath_NodeList ()
{
  /* Must be manually cleared before destruction. */
  OP_ASSERT (!indexed);
}

void
XPath_NodeList::AddL (XPath_Context *context, XPath_Node *node)
{
  if (count == 0)
    tree = node->tree;
  else if (node->tree != tree)
    tree = 0;

  if (count == total)
    {
      unsigned newtotal = total == 0 ? 8 : total + total;
      XPath_Node **newindexed = OP_NEWA_L (XPath_Node *, newtotal);

      op_memcpy (newindexed, indexed, count * sizeof indexed[0]);
      OP_DELETEA (indexed);

      total = newtotal;
      indexed = newindexed;
    }

  indexed[count] = XPath_Node::MakeL (context, node);
  ++count;
}

void
XPath_NodeList::AddL (XPath_Context *context, XPath_NodeList *nodelist)
{
  for (unsigned index = 0, count = nodelist->GetCount (); index < count; ++index)
    AddL (context, nodelist->Get (index));
}

static BOOL
XPath_NodeList_MergeSort (XPath_Node **nodes, unsigned count)
{
  if (count < 2)
    return TRUE;
  else if (count == 2)
    {
      if (XPath_Node::Precedes (nodes[1], nodes[0]))
        {
          XPath_Node *temporary = nodes[0];
          nodes[0] = nodes[1];
          nodes[1] = temporary;
        }

      return TRUE;
    }
  else
    {
      unsigned index = 0, count1 = count >> 1, count2 = count - count1;

      XPath_Node **nodes1 = OP_NEWA (XPath_Node *, count), **nodes2 = nodes1 + count1;

      if (!nodes1)
        return FALSE;

      op_memcpy (nodes1, nodes, count1 * sizeof nodes[0]);
      op_memcpy (nodes2, nodes + count1, count2 * sizeof nodes[0]);

      BOOL success = XPath_NodeList_MergeSort (nodes1, count1) && XPath_NodeList_MergeSort (nodes2, count2);

      if (success)
        {
          unsigned index1, index2;

          for (index = 0, index1 = 0, index2 = 0; index < count; ++index)
            if (index2 == count2 || index1 < count1 && XPath_Node::Precedes (nodes1[index1], nodes2[index2]))
              nodes[index] = nodes1[index1++];
            else
              nodes[index] = nodes2[index2++];
        }

      OP_DELETEA (nodes1);
      return success;
    }
}

void
XPath_NodeList::SortL ()
{
  if (!XPath_NodeList_MergeSort (indexed, count))
    LEAVE (OpStatus::ERR_NO_MEMORY);
}

void
XPath_NodeList::Clear (XPath_Context *context)
{
  for (unsigned index = 0; index < count; ++index)
    XPath_Node::DecRef (context, indexed[index]);

  OP_DELETEA (indexed);

  indexed = 0;
  count = total = 0;
}

XPath_Node *
XPath_NodeList::Pop (unsigned index)
{
  XPath_Node *node = indexed[index];

  if (index < count - 1)
    op_memmove (indexed + index, indexed + index + 1, (count - index - 1) * sizeof *indexed);

  --count;
  return node;
}

#endif // XPATH_SUPPORT
