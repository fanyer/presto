/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpnumericexpr.h"
#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xpparser.h"

XPath_NumericExpression::XPath_NumericExpression (XPath_Parser *parser, XPath_NumberExpression *lhs, XPath_NumberExpression *rhs, XPath_ExpressionType type)
  : XPath_NumberExpression (parser),
    lhs (lhs),
    rhs (rhs),
    type (type),
    state_index (parser->GetStateIndex ()),
    lhs_number_index (parser->GetNumberIndex ())
{
}

/* static */ XPath_NumberExpression *
XPath_NumericExpression::MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, XPath_ExpressionType type)
{
  OpStackAutoPtr<XPath_Expression> rhs_anchor (rhs);

  XPath_NumberExpression *lhs_number = XPath_NumberExpression::MakeL (parser, lhs);
  OpStackAutoPtr<XPath_NumberExpression> lhs_number_anchor (lhs_number);

  rhs_anchor.release ();
  XPath_NumberExpression *rhs_number = XPath_NumberExpression::MakeL (parser, rhs);
  OpStackAutoPtr<XPath_NumberExpression> rhs_number_anchor (rhs_number);

  XPath_NumberExpression *expression = OP_NEW_L (XPath_NumericExpression, (parser, lhs_number, rhs_number, type));

  lhs_number_anchor.release ();
  rhs_number_anchor.release ();

  return expression;
}

XPath_NumericExpression::~XPath_NumericExpression ()
{
  OP_DELETE (lhs);
  OP_DELETE (rhs);
}

/* virtual */ unsigned
XPath_NumericExpression::GetResultType ()
{
  return XP_VALUE_NUMBER;
}

/* virtual */ unsigned
XPath_NumericExpression::GetExpressionFlags ()
{
  return ((lhs->GetExpressionFlags () | rhs->GetExpressionFlags ()) & MASK_INHERITED) | FLAG_NUMBER;
}

/* virtual */ double
XPath_NumericExpression::EvaluateToNumberL (XPath_Context *context, BOOL initial)
{
  double lhs_number, rhs_number;

  if (context)
    {
      unsigned &state = context->states[state_index];
      double &lhs_number_storage = context->numbers[lhs_number_index];
      BOOL lhs_initial, rhs_initial;

#ifdef _DEBUG
      OP_ASSERT (initial || state < 4);
#endif // _DEBUG

      if (initial)
        state = 0;

      if (state < 2)
        {
          if (state == 0)
            {
              lhs_initial = TRUE;
              ++state;
            }
          else
            lhs_initial = FALSE;

          lhs_number_storage = lhs_number = lhs->EvaluateToNumberL (context, lhs_initial);

          ++state;
        }
      else
        lhs_number = lhs_number_storage;

      if (state == 2)
        {
          rhs_initial = TRUE;
          ++state;
        }
      else
        rhs_initial = FALSE;

      rhs_number = rhs->EvaluateToNumberL (context, rhs_initial);

#ifdef _DEBUG
      ++state;
#endif // _DEBUG
    }
  else
    {
      lhs_number = lhs->EvaluateToNumberL (0, TRUE);
      rhs_number = rhs->EvaluateToNumberL (0, TRUE);
    }

  switch (type)
    {
    case XP_EXPR_MULTIPLY:
      return lhs_number * rhs_number;

    case XP_EXPR_DIVIDE:
      return lhs_number == 0 && rhs_number == 0 ? op_nan (0) : lhs_number / rhs_number;

    case XP_EXPR_REMAINDER:
      return op_fmod (lhs_number, rhs_number);

    case XP_EXPR_ADD:
      return lhs_number + rhs_number;

    case XP_EXPR_SUBTRACT:
    default:
      return lhs_number - rhs_number;
    }
}

XPath_NegateExpression::XPath_NegateExpression (XPath_Parser *parser, XPath_NumberExpression *base)
  : XPath_NumberExpression (parser),
    base (base)
{
}

/* static */ XPath_NumberExpression *
XPath_NegateExpression::MakeL (XPath_Parser *parser, XPath_Expression *base)
{
  XPath_NumberExpression *base_number = XPath_NumberExpression::MakeL (parser, base);

  XPath_NumberExpression *expression = OP_NEW (XPath_NegateExpression, (parser, base_number));

  if (!expression)
    {
      OP_DELETE (base_number);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  return expression;
}

/* virtual */
XPath_NegateExpression::~XPath_NegateExpression ()
{
  OP_DELETE (base);
}

/* virtual */ unsigned
XPath_NegateExpression::GetResultType ()
{
  return XP_VALUE_NUMBER;
}

/* virtual */ unsigned
XPath_NegateExpression::GetExpressionFlags ()
{
  return (base->GetExpressionFlags () & MASK_INHERITED) | FLAG_NUMBER;
}

/* virtual */ double
XPath_NegateExpression::EvaluateToNumberL (XPath_Context *context, BOOL initial)
{
  double result = base->EvaluateToNumberL (context, initial);
  if (op_isnan (result))
    return result;
  else
    return -result;
}

#endif // XPATH_SUPPORT
