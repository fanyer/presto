/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPCOMPARISONEXPR_H
#define XPCOMPARISONEXPR_H

#include "modules/xpath/src/xpexpr.h"

/** Comparison where at least one operand is of unknown type. */
class XPath_ComparisonExpression
  : public XPath_BooleanExpression
{
private:
  XPath_Expression *lhs, *rhs;
  XPath_Producer *lhs_producer, *rhs_producer, *other_producer;
  XPath_ExpressionType type;
  unsigned state_index, lhs_type_index, rhs_type_index, lhs_value_index, rhs_value_index, comparison_state_index, lhs_stringset_index, rhs_stringset_index, buffer_index, smaller_number_index, larger_number_index;

  BOOL (*numbercomparison) (double lhs, double rhs);
  BOOL (*stringcomparison) (const uni_char *lhs, const uni_char *rhs);

  BOOL CompareL (XPath_Value *lhs_result, XPath_Value *rhs_result);

public:
  static XPath_Expression *MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, XPath_ExpressionType type);

  XPath_ComparisonExpression (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, XPath_ExpressionType type, BOOL (*numbercomparison) (double lhs, double rhs), BOOL (*stringcomparison) (const uni_char *lhs, const uni_char *rhs));
  virtual ~XPath_ComparisonExpression ();

  virtual unsigned GetExpressionFlags ();

  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial);
};

/** Comparison between two non-nodesets. */
class XPath_PrimitiveComparisonExpression
  : public XPath_BooleanExpression
{
protected:
  XPath_Expression *lhs, *rhs;
  XPath_ExpressionType type;

  BOOL (*numbercomparison) (double lhs, double rhs);
  BOOL (*stringcomparison) (const uni_char *lhs, const uni_char *rhs);

public:
  XPath_PrimitiveComparisonExpression (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, XPath_ExpressionType type, BOOL (*numbercomparison) (double lhs, double rhs), BOOL (*stringcomparison) (const uni_char *lhs, const uni_char *rhs));
  virtual ~XPath_PrimitiveComparisonExpression ();

  virtual unsigned GetResultType ();
  virtual unsigned GetExpressionFlags ();

  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial);
};

/**< Equality (=) comparison between two nodesets. */
class XPath_TwoNodesetsEqualExpression
  : public XPath_BooleanExpression
{
protected:
  XPath_Producer *lhs, *rhs;
  unsigned state_index, lhs_stringset_index, rhs_stringset_index;

public:
  XPath_TwoNodesetsEqualExpression (XPath_Parser *parser, XPath_Producer *lhs, XPath_Producer *rhs);
  virtual ~XPath_TwoNodesetsEqualExpression ();

  virtual unsigned GetExpressionFlags ();
  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial);
};

/**< Equality (!=) comparison between two nodesets. */
class XPath_TwoNodesetsUnequalExpression
  : public XPath_BooleanExpression
{
protected:
  XPath_Producer *lhs, *rhs;
  unsigned state_index, buffer_index;

public:
  XPath_TwoNodesetsUnequalExpression (XPath_Parser *parser, XPath_Producer *lhs, XPath_Producer *rhs);
  virtual ~XPath_TwoNodesetsUnequalExpression ();

  virtual unsigned GetExpressionFlags ();
  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial);
};

/**< Comparison between two nodesets. */
class XPath_TwoNodesetsRelationalExpression
  : public XPath_BooleanExpression
{
protected:
  XPath_Producer *smaller, *larger;
  BOOL or_equal;
  unsigned state_index, smaller_number_index, larger_number_index;

public:
  XPath_TwoNodesetsRelationalExpression (XPath_Parser *parser, XPath_Producer *lhs, XPath_Producer *rhs, XPath_ExpressionType type);
  virtual ~XPath_TwoNodesetsRelationalExpression ();

  virtual unsigned GetExpressionFlags ();
  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial);
};

#endif // XPCOMPARISONEXPR_H
