/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xplogicalexpr.h"
#include "modules/xpath/src/xpparser.h"

XPath_LogicalExpression::XPath_LogicalExpression (XPath_Parser *parser, XPath_BooleanExpression *lhs, XPath_BooleanExpression *rhs, XPath_ExpressionType type)
  : XPath_BooleanExpression (parser),
    lhs (lhs),
    rhs (rhs),
    type (type),
    state_index (parser->GetStateIndex ())
{
}

/* virtual */
XPath_LogicalExpression::~XPath_LogicalExpression ()
{
  OP_DELETE (lhs);
  OP_DELETE (rhs);
}

/* static */ XPath_BooleanExpression *
XPath_LogicalExpression::MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, XPath_ExpressionType type)
{
  OpStackAutoPtr<XPath_Expression> rhs_anchor (rhs);

  XPath_BooleanExpression *lhs_boolean, *rhs_boolean;

  lhs_boolean = XPath_BooleanExpression::MakeL (parser, lhs);
  OpStackAutoPtr<XPath_BooleanExpression> lhs_boolean_anchor (lhs_boolean);

  rhs_anchor.release ();
  rhs_boolean = XPath_BooleanExpression::MakeL (parser, rhs);
  OpStackAutoPtr<XPath_BooleanExpression> rhs_boolean_anchor (rhs_boolean);

  XPath_LogicalExpression *expression = OP_NEW_L (XPath_LogicalExpression, (parser, lhs_boolean, rhs_boolean, type));

  lhs_boolean_anchor.release ();
  rhs_boolean_anchor.release ();

  return expression;
}

/* virtual */ unsigned
XPath_LogicalExpression::GetExpressionFlags ()
{
  return ((lhs->GetExpressionFlags () | rhs->GetExpressionFlags ()) & MASK_INHERITED) | FLAG_BOOLEAN;
}

/* virtual */ BOOL
XPath_LogicalExpression::EvaluateToBooleanL (XPath_Context *context, BOOL initial)
{
  unsigned &state = context->states[state_index];

  /* States: 0 = initial
             1 = continue evaluation of lhs
             2 = continue evaluation of rhs */

  if (initial)
    state = 0;

  BOOL lhs_initial = state < 1, rhs_initial = state < 2;

  if (state < 2)
    {
      if (lhs_initial)
        state = 1;

      if (!lhs->EvaluateToBooleanL (context, lhs_initial) == (type == XP_EXPR_AND))
        return type == XP_EXPR_OR;

      state = 2;
    }

  return rhs->EvaluateToBooleanL (context, rhs_initial);
}

#endif // XPATH_SUPPORT
