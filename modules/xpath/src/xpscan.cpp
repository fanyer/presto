/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpscan.h"
#include "modules/xpath/src/xpstep.h"

XPath_Scan::XPath_Scan (XPath_XMLTreeAccessorFilter **filters, unsigned filters_count)
  : filters (filters),
    filters_count (filters_count),
    current_nodes (0)
{
}

XMLTreeAccessor::Node *
XPath_Scan::Prepare (XMLTreeAccessor *tree, XMLTreeAccessor::Node *origin)
{
  filters[current_index]->SetFilter (tree);
  if (current_index != 0)
    {
      for (unsigned index = current_index; index != 0;)
        if (XMLTreeAccessor::Node *node = current_nodes[--index])
          {
            tree->SetStopAtFilter (node);
            break;
          }
    }
  if (XMLTreeAccessor::Node *last_match = current_nodes[current_index])
    return last_match;
  else
    return origin;
}

XMLTreeAccessor::Node *
XPath_Scan::Handle (XMLTreeAccessor *tree, XMLTreeAccessor::Node *result, BOOL &finished)
{
  tree->ResetFilters ();
  finished = FALSE;
  if ((current_nodes[current_index] = result) != 0)
    {
      if (current_index + 1 == filters_count)
        return result;
      else
        {
          ++current_index;
          return 0;
        }
    }
  else if (++current_index == filters_count)
    {
      while (current_index != 0)
        if (current_nodes[--current_index])
          {
            XMLTreeAccessor::Node *node = current_nodes[current_index];
            for (unsigned index = current_index + 1; index < filters_count; ++index)
              current_nodes[index] = node;
            return node;
          }
      finished = TRUE;
      return 0;
    }
  else
    return 0;
}

/* static */ XPath_Scan *
XPath_Scan::MakeL (XPath_XMLTreeAccessorFilter **filters, unsigned filters_count)
{
  XPath_Scan *scan = OP_NEW_L (XPath_Scan, (filters, filters_count));

  scan->current_nodes = OP_NEWA (XMLTreeAccessor::Node *, filters_count);
  if (!scan->current_nodes)
    {
      OP_DELETE (scan);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  scan->Reset ();
  return scan;
}

XPath_Scan::~XPath_Scan ()
{
  OP_DELETEA (current_nodes);
}

XMLTreeAccessor::Node *
XPath_Scan::GetParent (XMLTreeAccessor *tree, XMLTreeAccessor::Node *child)
{
  if (initial)
    {
      initial = FALSE;
      XMLTreeAccessor::Node *parent = tree->GetParent (child);
      for (unsigned index = 0; index < filters_count; ++index)
        {
          filters[index]->SetFilter (tree);
          BOOL include = tree->FilterNode (parent);
          tree->ResetFilters ();
          if (include)
            return parent;
        }
    }
  return 0;
}

XMLTreeAccessor::Node *
XPath_Scan::GetAncestor (XMLTreeAccessor *tree, XMLTreeAccessor::Node *descendant)
{
  XMLTreeAccessor::Node *node;
  BOOL finished;
  while (!(node = Handle (tree, tree->GetAncestor (Prepare (tree, descendant)), finished)) && !finished);
  return node;
}

XMLTreeAccessor::Node *
XPath_Scan::GetAncestorOrSelf (XMLTreeAccessor *tree, XMLTreeAccessor::Node *descendant)
{
  if (initial)
    {
      initial = FALSE;
      for (unsigned index = 0; index < filters_count; ++index)
        {
          filters[index]->SetFilter (tree);
          BOOL include = tree->FilterNode (descendant);
          tree->ResetFilters ();
          if (include)
            return descendant;
        }
    }
  return GetAncestor (tree, descendant);
}

XMLTreeAccessor::Node *
XPath_Scan::GetPrecedingSibling (XMLTreeAccessor *tree, XMLTreeAccessor::Node *sibling)
{
  XMLTreeAccessor::Node *node;
  BOOL finished;
  while (!(node = Handle (tree, tree->GetPreviousSibling (Prepare (tree, sibling)), finished)) && !finished);
  return node;
}

XMLTreeAccessor::Node *
XPath_Scan::GetChild (XMLTreeAccessor *tree, XMLTreeAccessor::Node *parent)
{
  XMLTreeAccessor::Node *node;
  BOOL finished;
  do
    {
      XMLTreeAccessor::Node *origin = Prepare (tree, parent);
      if (origin == parent)
        node = tree->GetFirstChild (origin);
      else
        node = tree->GetNextSibling (origin);
    }
  while (!(node = Handle (tree, node, finished)) && !finished);
  return node;
}

XMLTreeAccessor::Node *
XPath_Scan::GetDescendant (XMLTreeAccessor *tree, XMLTreeAccessor::Node *ancestor)
{
  XMLTreeAccessor::Node *node;
  BOOL finished;
  while (!(node = Handle (tree, tree->GetNextDescendant (Prepare (tree, ancestor), ancestor), finished)) && !finished);
  return node;
}

void
XPath_Scan::Reset ()
{
  current_index = 0;
  initial = TRUE;
  op_memset (current_nodes, 0, filters_count * sizeof *current_nodes);
}

#endif // XPATH_SUPPORT
