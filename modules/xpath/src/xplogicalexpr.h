/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPLOGICALEXPR_H
#define XPLOGICALEXPR_H

#include "modules/xpath/src/xpexpr.h"

class XPath_LogicalExpression
  : public XPath_BooleanExpression
{
protected:
  XPath_BooleanExpression *lhs, *rhs;
  XPath_ExpressionType type;
  unsigned state_index;

  XPath_LogicalExpression (XPath_Parser *parser, XPath_BooleanExpression *lhs, XPath_BooleanExpression *rhs, XPath_ExpressionType type);

public:
  static XPath_BooleanExpression *MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, XPath_ExpressionType type);

  virtual ~XPath_LogicalExpression ();

  virtual unsigned GetExpressionFlags ();

  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial);
};

#endif // XPLOGICALEXPR_H
