/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPNUMERICEXPR_H
#define XPNUMERICEXPR_H

#include "modules/xpath/src/xpexpr.h"

/** Implements the *, /, %, + and - operators. */
class XPath_NumericExpression
  : public XPath_NumberExpression
{
private:
  XPath_NumberExpression *lhs, *rhs;
  XPath_ExpressionType type;
  unsigned state_index, lhs_number_index;

  XPath_NumericExpression (XPath_Parser *parser, XPath_NumberExpression *lhs, XPath_NumberExpression *rhs, XPath_ExpressionType type);

public:
  static XPath_NumberExpression *MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, XPath_ExpressionType type);

  virtual ~XPath_NumericExpression ();

  virtual unsigned GetResultType ();
  virtual unsigned GetExpressionFlags ();

  virtual double EvaluateToNumberL (XPath_Context *context, BOOL initial);
};

/** Implements the ! operator. */
class XPath_NegateExpression
  : public XPath_NumberExpression
{
private:
  XPath_NumberExpression *base;

  XPath_NegateExpression (XPath_Parser *parser, XPath_NumberExpression *base);

public:
  static XPath_NumberExpression *MakeL (XPath_Parser *parser, XPath_Expression *base);

  virtual ~XPath_NegateExpression ();

  virtual unsigned GetResultType ();
  virtual unsigned GetExpressionFlags ();

  virtual double EvaluateToNumberL (XPath_Context *context, BOOL initial);
};

#endif // XPNUMERICEXPR_H
