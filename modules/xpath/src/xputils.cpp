/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xputils.h"
#include "modules/xpath/src/xpcontext.h"
#include "modules/xpath/src/xpnode.h"
#include "modules/xpath/src/xplexer.h"
#include "modules/xpath/src/xpvalue.h"

#include "modules/doc/frm_doc.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/util/tempbuf.h"
#include "modules/util/str.h"
#include "modules/xmlutils/xmldocumentinfo.h"

/* static */ uni_char *
XPath_Utils::CopyStringL (const uni_char *string, unsigned length)
{
  uni_char *result;

  if (string)
    result = UniSetNewStrN (string, length == ~0u ? uni_strlen (string) : length);
  else
    result = UniSetNewStr (UNI_L (""));

  if (!result)
    LEAVE (OpStatus::ERR_NO_MEMORY);

  return result;
}

/* static */ unsigned
XPath_Utils::HashString (const uni_char *string)
{
  if (string)
    {
      UINT32 hash = uni_strlen (string), index = hash;

      while (index-- > 0)
        hash = hash + hash + hash + string[index];

      return hash;
    }
  else
    return 0;
}

/* static */ XPath_Axis
XPath_Utils::GetAxis (const XPath_Token &token)
{
  if (token == "ancestor")
    return XP_AXIS_ANCESTOR;
  else if (token == "ancestor-or-self")
    return XP_AXIS_ANCESTOR_OR_SELF;
  else if (token == "attribute")
    return XP_AXIS_ATTRIBUTE;
  else if (token == "child")
    return XP_AXIS_CHILD;
  else if (token == "descendant")
    return XP_AXIS_DESCENDANT;
  else if (token == "descendant-or-self")
    return XP_AXIS_DESCENDANT_OR_SELF;
  else if (token == "following")
    return XP_AXIS_FOLLOWING;
  else if (token == "following-sibling")
    return XP_AXIS_FOLLOWING_SIBLING;
  else if (token == "namespace")
    return XP_AXIS_NAMESPACE;
  else if (token == "parent")
    return XP_AXIS_PARENT;
  else if (token == "preceding")
    return XP_AXIS_PRECEDING;
  else if (token == "preceding-sibling")
    return XP_AXIS_PRECEDING_SIBLING;
  else
    return XP_AXIS_SELF;
}

/* static */ XPath_NodeType
XPath_Utils::GetNodeType (const XPath_Token &token)
{
  if (token == "text")
    return XP_NODE_TEXT;
  else if (token == "comment")
    return XP_NODE_COMMENT;
  else if (token == "processing-instruction")
    return XP_NODE_PI;
  else
    return XP_NODE_ROOT;
}

/* static */ XPath_NodeType
XPath_Utils::GetNodeType (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode)
{
  switch (tree->GetNodeType (treenode))
    {
    case XMLTreeAccessor::TYPE_ROOT:
      return XP_NODE_ROOT;

    case XMLTreeAccessor::TYPE_TEXT:
    case XMLTreeAccessor::TYPE_CDATA_SECTION:
      return XP_NODE_TEXT;

    case XMLTreeAccessor::TYPE_PROCESSING_INSTRUCTION:
      return XP_NODE_PI;

    case XMLTreeAccessor::TYPE_COMMENT:
      return XP_NODE_COMMENT;

    case XMLTreeAccessor::TYPE_DOCTYPE:
      return XP_NODE_DOCTYPE;

    default:
      return XP_NODE_ELEMENT;
    }
}

/* static */ XMLTreeAccessor::NodeType
XPath_Utils::ConvertNodeType (XPath_NodeType nodetype)
{
  switch (nodetype)
    {
    case XP_NODE_ROOT:
      return XMLTreeAccessor::TYPE_ROOT;

    case XP_NODE_ELEMENT:
      return XMLTreeAccessor::TYPE_ELEMENT;

    case XP_NODE_TEXT:
      return XMLTreeAccessor::TYPE_TEXT;

    case XP_NODE_PI:
      return XMLTreeAccessor::TYPE_PROCESSING_INSTRUCTION;

    case XP_NODE_COMMENT:
      return XMLTreeAccessor::TYPE_COMMENT;

    case XP_NODE_DOCTYPE:
      return XMLTreeAccessor::TYPE_DOCTYPE;

    default:
      OP_ASSERT (FALSE);
      return XMLTreeAccessor::TYPE_ROOT;
    }
}

/* static */ XMLTreeAccessor::NodeType
XPath_Utils::ConvertNodeType (XPathNode::Type nodetype)
{
  switch (nodetype)
    {
    case XPathNode::ROOT_NODE:
      return XMLTreeAccessor::TYPE_ROOT;

    case XPathNode::ELEMENT_NODE:
      return XMLTreeAccessor::TYPE_ELEMENT;

    case XPathNode::TEXT_NODE:
      return XMLTreeAccessor::TYPE_TEXT;

    case XPathNode::PI_NODE:
      return XMLTreeAccessor::TYPE_PROCESSING_INSTRUCTION;

    case XPathNode::COMMENT_NODE:
      return XMLTreeAccessor::TYPE_COMMENT;

#ifdef XPATH_PATTERN_SUPPORT
    case XPathNode::DOCTYPE_NODE:
      return XMLTreeAccessor::TYPE_DOCTYPE;
#endif // XPATH_PATTERN_SUPPORT

    default:
      OP_ASSERT (FALSE);
      return XMLTreeAccessor::TYPE_ROOT;
    }
}

/* static */ double
XPath_Utils::Round (double number)
{
  if (number >= -.5 && number < 0.)
    /* "-0." evaluates to positive zero, while negating the return
       from a function or the value of a variable evaluates to
       negative zero. */
    return -XPath_Value::Zero ();
  else
    return op_floor (number + .5);
}

#endif // XPATH_SUPPORT
