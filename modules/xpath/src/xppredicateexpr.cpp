/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xppredicateexpr.h"
#include "modules/xpath/src/xpstep.h"
#include "modules/xpath/src/xpexpr.h"
#include "modules/xpath/src/xpparser.h"

/* static */ XPath_Expression *
XPath_PredicateExpression::MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs)
{
  OpStackAutoPtr<XPath_Expression> rhs_anchor (rhs);

  XPath_Producer *lhs_producer = XPath_Expression::GetProducerL (parser, lhs);

  if (!lhs_producer)
    {
      OpStackAutoPtr<XPath_Expression> lhs_anchor(lhs);
      XPATH_COMPILATION_ERROR ("expected node-set expression", lhs->location);
    }

  XPath_Step::Predicate *predicate = XPath_Step::Predicate::MakeL (parser, lhs_producer, rhs, TRUE);
  rhs_anchor.release ();

  XPath_Expression *expression = OP_NEW (XPath_ProducerExpression, (parser, predicate));

  if (!expression)
    {
      OP_DELETE (predicate);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  return expression;
}

#endif // XPATH_SUPPORT
