/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_nodelist.h"

#include "modules/xslt/src/xslt_utils.h"
#include "modules/xpath/xpath.h"

XSLT_NodeList::~XSLT_NodeList ()
{
  for (unsigned index = 0; index < nodes_used; ++index)
    XPathNode::Free (nodes[index]);

  OP_DELETEA (nodes);
}

OP_STATUS
XSLT_NodeList::Add (XPathNode *node)
{
  void **nodes0 = reinterpret_cast<void **> (nodes);
  RETURN_IF_ERROR (XSLT_Utils::GrowArray (&nodes0, nodes_used, nodes_used + 1, nodes_total));
  nodes = reinterpret_cast<XPathNode **> (nodes0);

  RETURN_IF_ERROR (XPathNode::MakeCopy (nodes[nodes_used], node));
  ++nodes_used;

  return OpStatus::OK;
}

OP_STATUS
XSLT_NodeList::AddAll (XSLT_NodeList &other)
{
  void **nodes0 = reinterpret_cast<void **> (nodes);
  RETURN_IF_ERROR (XSLT_Utils::GrowArray (&nodes0, nodes_used, nodes_used + other.nodes_used, nodes_total));
  nodes = reinterpret_cast<XPathNode **> (nodes0);

  for (unsigned index = 0; index < other.nodes_used; ++index)
    {
      RETURN_IF_ERROR (XPathNode::MakeCopy (nodes[nodes_used], other.nodes[index]));
      ++nodes_used;
    }

  return OpStatus::OK;
}

/* static */ XSLT_NodeList *
XSLT_NodeList::CopyL (XSLT_NodeList &other)
{
  XSLT_NodeList *copy = OP_NEW_L (XSLT_NodeList, ());

  if (OpStatus::IsMemoryError (copy->AddAll (other)))
    {
      OP_DELETE (copy);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  return copy;
}

#endif // XSLT_SUPPORT
