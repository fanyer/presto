/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpliteralexpr.h"
#include "modules/xpath/src/xpvalue.h"

class XPath_NumberLiteralExpression
  : public XPath_NumberExpression
{
public:
  double number;

public:
  XPath_NumberLiteralExpression (XPath_Parser *parser, double number)
    : XPath_NumberExpression (parser),
      number (number)
  {
  }

  virtual unsigned GetExpressionFlags ()
  {
    return FLAG_CONSTANT | FLAG_NUMBER;
  }

  virtual double EvaluateToNumberL (XPath_Context *context, BOOL initial)
  {
    return number;
  }
};

class XPath_BooleanLiteralExpression
  : public XPath_BooleanExpression
{
public:
  BOOL boolean;

public:
  XPath_BooleanLiteralExpression (XPath_Parser *parser, BOOL boolean)
    : XPath_BooleanExpression (parser),
      boolean (boolean)
  {
  }

  virtual unsigned GetExpressionFlags ()
  {
    return FLAG_CONSTANT | FLAG_BOOLEAN;
  }

  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial)
  {
    return boolean;
  }
};

class XPath_StringLiteralExpression
  : public XPath_StringExpression
{
public:
  OpString string;

public:
  XPath_StringLiteralExpression (XPath_Parser *parser)
    : XPath_StringExpression (parser)
  {
  }

  BOOL Set (const uni_char *value, unsigned value_length)
  {
    return OpStatus::IsSuccess (string.Set (value, value_length));
  }

  virtual unsigned GetExpressionFlags ()
  {
    return FLAG_CONSTANT | FLAG_STRING;
  }

  virtual const uni_char *EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer)
  {
    return string.CStr ();
  }
};

/* static */ XPath_Expression *
XPath_LiteralExpression::MakeL (XPath_Parser *parser, double number)
{
  return OP_NEW_L (XPath_NumberLiteralExpression, (parser, number));
}

/* static */ XPath_Expression *
XPath_LiteralExpression::MakeL (XPath_Parser *parser, BOOL boolean)
{
  return OP_NEW_L (XPath_BooleanLiteralExpression, (parser, boolean));
}

/* static */ XPath_Expression *
XPath_LiteralExpression::MakeL (XPath_Parser *parser, const uni_char *string, unsigned string_length)
{
  XPath_StringLiteralExpression *expression = OP_NEW_L (XPath_StringLiteralExpression, (parser));
  if (!expression->Set (string, string_length))
    {
      OP_DELETE (expression);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }
  return expression;
}

#endif // XPATH_SUPPORT
