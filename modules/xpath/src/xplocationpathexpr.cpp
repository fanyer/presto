/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xplocationpathexpr.h"
#include "modules/xpath/src/xpnodeset.h"
#include "modules/xpath/src/xpvalue.h"

XPath_LocationPathExpression::XPath_LocationPathExpression (XPath_Parser *parser, XPath_Producer *producer)
  : XPath_Expression (parser),
    producer (producer)
{
}

/* virtual */
XPath_LocationPathExpression::~XPath_LocationPathExpression ()
{
  OP_DELETE (producer);
}

/* virtual */ unsigned
XPath_LocationPathExpression::GetResultType ()
{
  return XP_VALUE_NODESET;
}

/* virtual */ unsigned
XPath_LocationPathExpression::GetExpressionFlags ()
{
  return (producer->GetExpressionFlags () & (FLAG_BLOCKING | MASK_COMPLEXITY)) | FLAG_PRODUCER;
}

/* virtual */ XPath_Producer *
XPath_LocationPathExpression::GetProducerInternalL (XPath_Parser *parser)
{
  XPath_Producer *temporary = producer;
  producer = 0;
  return temporary;
}

#endif // XPATH_SUPPORT
