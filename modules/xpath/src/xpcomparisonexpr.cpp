/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpcomparisonexpr.h"
#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xpnodeset.h"
#include "modules/xpath/src/xpcontext.h"
#include "modules/xpath/src/xpparser.h"
#include "modules/xpath/src/xpstep.h"
#include "modules/xpath/src/xpunknown.h"
#include "modules/xpath/src/xpstringset.h"

#include "modules/util/tempbuf.h"

static BOOL XPath_CompareNumbersEqual(double lhs, double rhs)
{
  return !op_isnan (lhs) && !op_isnan (rhs) && lhs == rhs;
}

static BOOL XPath_CompareNumbersNotEqual(double lhs, double rhs)
{
  return op_isnan (lhs) || op_isnan (rhs) || lhs != rhs;
}

static BOOL XPath_CompareNumbersLessThan(double lhs, double rhs)
{
  return !op_isnan (lhs) && !op_isnan (rhs) && lhs < rhs;
}

static BOOL XPath_CompareNumbersLessThanOrEqual(double lhs, double rhs)
{
  return !op_isnan (lhs) && !op_isnan (rhs) && lhs <= rhs;
}

static BOOL XPath_CompareNumbersGreaterThan(double lhs, double rhs)
{
  return !op_isnan (lhs) && !op_isnan (rhs) && lhs > rhs;
}

static BOOL XPath_CompareNumbersGreaterThanOrEqual(double lhs, double rhs)
{
  return !op_isnan (lhs) && !op_isnan (rhs) && lhs >= rhs;
}

static BOOL XPath_CompareStringsEqual(const uni_char *lhs, const uni_char *rhs)
{
  return uni_strcmp (lhs, rhs) == 0;
}

static BOOL XPath_CompareStringsNotEqual(const uni_char *lhs, const uni_char *rhs)
{
  return uni_strcmp (lhs, rhs) != 0;
}

static BOOL (*(XPath_NumberComparison) (XPath_ExpressionType type)) (double lhs, double rhs)
{
  switch (type)
    {
    case XP_EXPR_LESS_THAN:
      return XPath_CompareNumbersLessThan;

    case XP_EXPR_GREATER_THAN:
      return XPath_CompareNumbersGreaterThan;

    case XP_EXPR_LESS_THAN_OR_EQUAL:
      return XPath_CompareNumbersLessThanOrEqual;

    case XP_EXPR_GREATER_THAN_OR_EQUAL:
      return XPath_CompareNumbersGreaterThanOrEqual;

    case XP_EXPR_EQUAL:
      return XPath_CompareNumbersEqual;

    case XP_EXPR_NOT_EQUAL:
      return XPath_CompareNumbersNotEqual;

    default:
      OP_ASSERT (FALSE);
      return 0;
    }
}

static BOOL (*(XPath_StringComparison (XPath_ExpressionType type))) (const uni_char *lhs, const uni_char *rhs)
{
  switch (type)
    {
    case XP_EXPR_LESS_THAN:
    case XP_EXPR_GREATER_THAN:
    case XP_EXPR_LESS_THAN_OR_EQUAL:
    case XP_EXPR_GREATER_THAN_OR_EQUAL:
      return 0;

    case XP_EXPR_EQUAL:
      return XPath_CompareStringsEqual;

    case XP_EXPR_NOT_EQUAL:
      return XPath_CompareStringsNotEqual;

    default:
      OP_ASSERT (FALSE);
      return 0;
    }
}

static XPath_ExpressionType
XPath_MirrorComparison (XPath_ExpressionType type)
{
  switch (type)
    {
    case XP_EXPR_LESS_THAN:
      return XP_EXPR_GREATER_THAN;

    case XP_EXPR_GREATER_THAN:
      return XP_EXPR_LESS_THAN;

    case XP_EXPR_LESS_THAN_OR_EQUAL:
      return XP_EXPR_GREATER_THAN_OR_EQUAL;

    case XP_EXPR_GREATER_THAN_OR_EQUAL:
      return XP_EXPR_LESS_THAN_OR_EQUAL;

    default:
      return type;
    }
}

static BOOL
XPath_CompareNodesetsEqual (XPath_Context *context, XPath_Producer *lhs, XPath_Producer *rhs, BOOL initial, BOOL lhs_initial, BOOL rhs_initial, unsigned state_index, unsigned lhs_stringset_index, unsigned rhs_stringset_index)
{
  unsigned &state = context->states[state_index];
  XPath_StringSet &lhs_stringset = context->stringsets[lhs_stringset_index];
  XPath_StringSet &rhs_stringset = context->stringsets[rhs_stringset_index];

  /* States: state & 1 == 0: fetch next node from lhs
             state & 1 == 1: fetch next node from rhs

             state & 2 == 0: lhs not finished
             state & 2 == 2: lhs finished

             state & 4 == 0: rhs not finished
             state & 4 == 4: rhs finished */

  if (initial)
    state = 0;

  if (lhs_initial)
    {
      lhs->Reset (context);
      lhs_stringset.Clear ();
    }

  if (rhs_initial)
    {
      rhs->Reset (context);
      rhs_stringset.Clear ();
    }

  TempBuffer buffer; ANCHOR (TempBuffer, buffer);
  BOOL result = TRUE;

  while (TRUE)
    {
      unsigned local_state = state;

      if ((local_state & 1) == 0)
        {
          if ((local_state & 2) == 0)
            if (XPath_Node *node = lhs->GetNextNodeL (context))
              {
                node->GetStringValueL (buffer);
                XPath_Node::DecRef (context, node);

                if (rhs_stringset.Contains (buffer.GetStorage ()))
                  goto return_true;
                else
                  lhs_stringset.AddL (buffer.GetStorage ());

                buffer.Clear ();
              }
            else if ((local_state & 4) == 4 || lhs_stringset.GetCount () == 0)
              break;
            else
              state = local_state |= 2;

          state = local_state ^= 1;

          XP_SEQUENCE_POINT (context);
        }

      if ((local_state & 1) == 1)
        {
          if ((local_state & 4) == 0)
            if (XPath_Node *node = rhs->GetNextNodeL (context))
              {
                node->GetStringValueL (buffer);
                XPath_Node::DecRef (context, node);

                if (lhs_stringset.Contains (buffer.GetStorage ()))
                  goto return_true;
                else
                  rhs_stringset.AddL (buffer.GetStorage ());

                buffer.Clear ();
              }
            else if ((local_state & 2) == 2 || rhs_stringset.GetCount () == 0)
              break;
            else
              state = local_state |= 4;

          state = local_state ^= 1;

          XP_SEQUENCE_POINT (context);
        }
    }

  result = FALSE;

 return_true:
  lhs_stringset.Clear ();
  rhs_stringset.Clear ();

  return result;
}

static BOOL
XPath_CompareNodesetsUnequal (XPath_Context *context, XPath_Producer *lhs, XPath_Producer *rhs, BOOL initial, BOOL lhs_initial, BOOL rhs_initial, unsigned state_index, unsigned buffer_index)
{
  unsigned &state = context->states[state_index];
  TempBuffer &buffer = context->buffers[buffer_index];

  /* States: state &  1 ==  0: fetch next node from lhs
             state &  1 ==  1: fetch next node from rhs

             state &  2 ==  0: lhs not finished
             state &  2 ==  2: lhs finished

             state &  4 ==  0: lhs possibly empty
             state &  4 ==  4: lhs not empty

             state &  8 ==  0: rhs not finished
             state &  8 ==  8: rhs finished

             state & 16 ==  0: rhs possibly empty
             state & 16 == 16: rhs not empty */

  if (initial)
    {
      state = 0;
      buffer.Clear ();
    }

  if (lhs_initial)
    lhs->Reset (context);

  if (rhs_initial)
    rhs->Reset (context);

  TempBuffer local_buffer; ANCHOR (TempBuffer, local_buffer);
  unsigned local_state = state;

  while (TRUE)
    {
      XPath_Node *node = 0;

      XPath_Producer *producer;
      unsigned shift;
      if ((local_state & 1) == 0)
        producer = lhs, shift = 0;
      else
        producer = rhs, shift = 2;

      /* If producer isn't finished ... */
      if ((local_state & (2 << shift)) == 0)
        /* ... read next node and ... */
        if ((node = producer->GetNextNodeL (context)) != 0)
          /* ... set bit saying the producer isn't empty, or ... */
          local_state = local_state | (4 << shift);
        else if ((local_state & (4 << shift)) == 0)
          /* ... return false if the producer actually was empty, or ... */
          return FALSE;
        else
          /* ... set bit saying the producer is finished. */
          local_state = local_state | (2 << shift);

      /* Toggle bit that decides which producer to read a node from next. */
      local_state = local_state ^ 1;

      if (node)
        {
          if ((local_state & 16) == 0)
            /* First node ever (rhs producer "possibly empty") => just record its
               string value. */
            node->GetStringValueL (buffer);
          else if (!node->HasStringValueL (buffer.GetStorage ()))
            {
              /* Node has different string value; result is true. */
              XPath_Node::DecRef (context, node);
              return TRUE;
            }

          XPath_Node::DecRef (context, node);
        }
      else if ((local_state & 10) == 10)
        /* Both producers finished and we haven't returned true yet, so we
           should return false. */
        return FALSE;

      state = local_state;

      XP_SEQUENCE_POINT (context);
    }
}

/* Does a "smaller < larger" (or_equal == FALSE) or "smaller <= larger"
   (or_equal == TRUE) comparison between two nodesets.  The so far smallest
   value from 'smaller' is stored in number 'smallest_index', and the so far
   largest value from 'larger' is stored in number 'largest_index'. */
static BOOL
XPath_CompareNodesetsRelational (XPath_Context *context, XPath_Producer *smaller, XPath_Producer *larger, BOOL initial, BOOL smaller_initial, BOOL larger_initial, BOOL or_equal, unsigned state_index, unsigned smallest_index, unsigned largest_index)
{
  unsigned &state = context->states[state_index];
  double &smallest = context->numbers[smallest_index];
  double &largest = context->numbers[largest_index];

  /* States: state &  1 ==  0: fetch next node from smaller
             state &  1 ==  1: fetch next node from larger

             state &  2 ==  0: smaller not finished
             state &  2 ==  2: smaller finished

             state &  4 ==  0: smaller possibly empty
             state &  4 ==  4: smaller not empty

             state &  8 ==  0: larger not finished
             state &  8 ==  8: larger finished

             state & 16 ==  0: larger possibly empty
             state & 16 == 16: larger not empty */

  if (initial)
    state = 0;

  if (smaller_initial)
    smaller->Reset (context);

  if (larger_initial)
    larger->Reset (context);

  TempBuffer buffer; ANCHOR (TempBuffer, buffer);
  unsigned local_state = state;

  while (TRUE)
    {
      XPath_Node *node = 0;

      XPath_Producer *producer;
      double *number;
      unsigned shift;
      if ((local_state & 1) == 0)
        producer = smaller, number = &smallest, shift = 0;
      else
        producer = larger, number = &largest, shift = 2;

      /* If producer isn't finished ... */
      if ((local_state & (2 << shift)) == 0)
        /* ... read next node and ... */
        if ((node = producer->GetNextNodeL (context)) != 0)
          {
            node->GetStringValueL (buffer);
            XPath_Node::DecRef (context, node);

            double local_number = XPath_Value::AsNumber (buffer.GetStorage ());
            buffer.Clear ();

            if (!op_isnan (local_number))
              {
                /* ... if this was the first non-NaN number from the producer ... */
                if ((local_state & (4 << shift)) == 0)
                  {
                    /* ... store the number as the currently smallest/largest, and ... */
                    *number = local_number;

                    /* ... set bit saying the producer isn't empty, otherwise ... */
                    local_state = local_state | (4 << shift);
                  }
                else if (number == &smallest ? local_number < *number : local_number > *number)
                  /* ... store the number if it is smaller/larger than the currently smallest/largest, or ...*/
                  *number = local_number;
              }
          }
        else if ((local_state & (4 << shift)) == 0)
          /* ... return false if the producer actually was empty, or ... */
          return FALSE;
        else
          /* ... set bit saying the producer is finished. */
          local_state = local_state | (2 << shift);

      /* Toggle bit that decides which producer to read a node from next. */
      local_state = local_state ^ 1;

      if ((local_state & 20) == 20 && (smallest < largest || or_equal && smallest == largest))
        /* If we've read at least one non-NaN number from each side, and the
           currently smallest is less than (or equal) to the current largest,
           return TRUE. */
        return TRUE;
      else if ((local_state & 10) == 10)
        /* Otherwise, if both sides are finished, return FALSE. */
        return FALSE;

      state = local_state;

      XP_SEQUENCE_POINT (context);
    }
}

class XPath_NumberComparisonExpression
  : public XPath_BooleanExpression
{
private:
  XPath_NumberExpression *lhs, *rhs;
  BOOL (*comparison) (double lhs, double rhs);
  unsigned state_index, lhs_result_index;

  XPath_NumberComparisonExpression (XPath_Parser *parser, XPath_NumberExpression *lhs, XPath_NumberExpression *rhs, BOOL (*comparison) (double lhs, double rhs))
    : XPath_BooleanExpression (parser),
      lhs (lhs),
      rhs (rhs),
      comparison (comparison),
      state_index (parser->GetStateIndex ()),
      lhs_result_index (parser->GetNumberIndex ())
  {
  }

public:
  static XPath_BooleanExpression *MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, BOOL (*comparison) (double lhs, double rhs))
  {
    OpStackAutoPtr<XPath_Expression> rhs_anchor (rhs);

    XPath_NumberExpression *lhs_number = XPath_NumberExpression::MakeL (parser, lhs);
    OpStackAutoPtr<XPath_Expression> lhs_number_anchor (lhs_number);

    rhs_anchor.release ();
    XPath_NumberExpression *rhs_number = XPath_NumberExpression::MakeL (parser, rhs);
    OpStackAutoPtr<XPath_Expression> rhs_number_anchor (rhs_number);

    XPath_BooleanExpression *expr = OP_NEW_L (XPath_NumberComparisonExpression, (parser, lhs_number, rhs_number, comparison));

    lhs_number_anchor.release ();
    rhs_number_anchor.release ();

    return expr;
  }

  virtual ~XPath_NumberComparisonExpression ()
  {
    OP_DELETE (lhs);
    OP_DELETE (rhs);
  }

  virtual unsigned GetExpressionFlags ()
  {
    return ((lhs->GetExpressionFlags () | rhs->GetExpressionFlags ()) & MASK_INHERITED) | FLAG_BOOLEAN;
  }

  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial)
  {
    unsigned &state = context->states[state_index];

    /* States: 0 = evaluate lhs initial
               1 = continue lhs + evaluate rhs initial
               2 = continue rhs */

    double lhs_result;
    BOOL rhs_initial;

    if (initial)
      state = 0;

    if (state < 2)
      {
        BOOL lhs_initial;

        if (state == 0)
          {
            lhs_initial = TRUE;
            state = 1;
          }
        else
          lhs_initial = FALSE;

        lhs_result = context->numbers[lhs_result_index] = lhs->EvaluateToNumberL (context, lhs_initial);

        state = 2;
        rhs_initial = TRUE;
      }
    else
      {
        lhs_result = context->numbers[lhs_result_index];
        rhs_initial = FALSE;
      }

    double rhs_result = rhs->EvaluateToNumberL (context, rhs_initial);

    return comparison (lhs_result, rhs_result);
  }
};

class XPath_BooleanComparisonExpression
  : public XPath_BooleanExpression
{
private:
  XPath_BooleanExpression *lhs, *rhs;
  XPath_ExpressionType type;
  unsigned state_index;

  XPath_BooleanComparisonExpression (XPath_Parser *parser, XPath_BooleanExpression *lhs, XPath_BooleanExpression *rhs, XPath_ExpressionType type)
    : XPath_BooleanExpression (parser),
      lhs (lhs),
      rhs (rhs),
      type (type),
      state_index (parser->GetStateIndex ())
  {
  }

public:
  static XPath_BooleanExpression *MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, XPath_ExpressionType type)
  {
    OpStackAutoPtr<XPath_Expression> rhs_anchor (rhs);
    XPath_BooleanExpression *lhs_boolean = XPath_BooleanExpression::MakeL (parser, lhs);
    OpStackAutoPtr<XPath_Expression> lhs_bool_anchor (lhs_boolean);

    rhs_anchor.release ();
    XPath_BooleanExpression *rhs_boolean = XPath_BooleanExpression::MakeL (parser, rhs);
    OpStackAutoPtr<XPath_Expression> rhs_bool_anchor (rhs_boolean);

    XPath_BooleanExpression *expr = OP_NEW_L (XPath_BooleanComparisonExpression, (parser, lhs_boolean, rhs_boolean, type));

    lhs_bool_anchor.release ();
    rhs_bool_anchor.release ();

    return expr;
  }

  virtual ~XPath_BooleanComparisonExpression ()
  {
    OP_DELETE (lhs);
    OP_DELETE (rhs);
  }

  virtual unsigned GetExpressionFlags ()
  {
    return ((lhs->GetExpressionFlags () | rhs->GetExpressionFlags ()) & MASK_INHERITED) | FLAG_BOOLEAN;
  }

  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial)
  {
    unsigned &state = context->states[state_index];

    /* States: 0 = evaluate lhs initial
               1 = continue lhs + evaluate rhs initial
               2 = lhs returned true, continue rhs
               3 = lhs returned false, continue rhs */

    BOOL rhs_initial;

    if (initial)
      state = 0;

    if (state < 2)
      {
        BOOL lhs_initial;

        if (state == 0)
          {
            lhs_initial = TRUE;
            state = 1;
          }
        else
          lhs_initial = FALSE;

        state = lhs->EvaluateToBooleanL (context, lhs_initial) ? 2 : 3;
        rhs_initial = TRUE;
      }
    else
      rhs_initial = FALSE;

    BOOL lhs_result = state == 2;
    BOOL rhs_result = !!rhs->EvaluateToBooleanL (context, rhs_initial);

    if (type == XP_EXPR_EQUAL)
      return lhs_result == rhs_result;
    else if (type == XP_EXPR_NOT_EQUAL)
      return lhs_result != rhs_result;
    else
      return XPath_NumberComparison (type) (lhs_result, rhs_result);
  }
};

class XPath_StringComparisonExpression
  : public XPath_BooleanExpression
{
private:
  XPath_StringExpression *lhs, *rhs;
  BOOL (*comparison) (const uni_char *lhs, const uni_char *rhs);
  unsigned state_index, lhs_result_index;

  XPath_StringComparisonExpression (XPath_Parser *parser, XPath_StringExpression *lhs, XPath_StringExpression *rhs, BOOL (*comparison) (const uni_char *lhs, const uni_char *rhs))
    : XPath_BooleanExpression (parser),
      lhs (lhs),
      rhs (rhs),
      comparison (comparison),
      state_index (parser->GetStateIndex ()),
      lhs_result_index (parser->GetValueIndex ())
  {
  }

public:
  static XPath_BooleanExpression *MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, BOOL (*comparison) (const uni_char *lhs, const uni_char *rhs))
  {
    OpStackAutoPtr<XPath_Expression> rhs_anchor (rhs);
    XPath_StringExpression *lhs_string = XPath_StringExpression::MakeL (parser, lhs);
    OpStackAutoPtr<XPath_Expression> lhs_string_anchor (lhs_string);

    rhs_anchor.release ();
    XPath_StringExpression *rhs_string = XPath_StringExpression::MakeL (parser, rhs);
    OpStackAutoPtr<XPath_Expression> rhs_string_anchor (rhs_string);

    XPath_BooleanExpression *expr = OP_NEW_L (XPath_StringComparisonExpression, (parser, lhs_string, rhs_string, comparison));

    lhs_string_anchor.release ();
    rhs_string_anchor.release ();

    return expr;
  }

  virtual ~XPath_StringComparisonExpression ()
  {
    OP_DELETE (lhs);
    OP_DELETE (rhs);
  }

  virtual unsigned GetExpressionFlags ()
  {
    return ((lhs->GetExpressionFlags () | rhs->GetExpressionFlags ()) & MASK_INHERITED) | FLAG_BOOLEAN;
  }

  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial)
  {
    unsigned &state = context->states[state_index];

    /* States: 0 = evaluate lhs initial
               1 = continue lhs + evaluate rhs initial
               2 = continue rhs */

    const uni_char *lhs_result, *rhs_result;
    BOOL rhs_initial;

    if (initial)
      state = 0;

    TempBuffer lhs_buffer; ANCHOR (TempBuffer, lhs_buffer);

    if (state < 2)
      {
        BOOL lhs_initial;

        if (context->values[lhs_result_index])
          {
            XPath_Value::DecRef (context, context->values[lhs_result_index]);
            context->values[lhs_result_index] = 0;
          }

        if (state == 0)
          {
            lhs_initial = TRUE;
            state = 1;
          }
        else
          lhs_initial = FALSE;

        lhs_result = lhs->EvaluateToStringL (context, lhs_initial, lhs_buffer);

        if (*lhs_result)
          context->values[lhs_result_index] = XPath_Value::MakeStringL (context, lhs_result);

        state = 2;
        rhs_initial = TRUE;
      }
    else
      {
        if (XPath_Value *lhs_value = context->values[lhs_result_index])
          lhs_result = lhs_value->data.string;
        else
          lhs_result = UNI_L ("");

        rhs_initial = FALSE;
      }

    TempBuffer rhs_buffer; ANCHOR (TempBuffer, rhs_buffer);

    rhs_result = rhs->EvaluateToStringL (context, rhs_initial, rhs_buffer);

    return comparison (lhs_result, rhs_result);
  }
};

/**< Comparison between a nodeset and a number. */
class XPath_NodeSetNumberComparison
  : public XPath_BooleanExpression
{
protected:
  XPath_Producer *nodes;
  XPath_NumberExpression *numberexpression;

  unsigned state_index, number_index;

  BOOL (*numbercomparison) (double lhs, double rhs);

public:
  XPath_NodeSetNumberComparison (XPath_Parser *parser, XPath_Producer *nodes, XPath_NumberExpression *expression, BOOL (*numbercomparison) (double lhs, double rhs))
    : XPath_BooleanExpression (parser),
      nodes (nodes),
      numberexpression (expression),
      state_index (parser->GetStateIndex ()),
      number_index (parser->GetNumberIndex ()),
      numbercomparison (numbercomparison)
  {
  }

  virtual ~XPath_NodeSetNumberComparison ()
  {
    OP_DELETE (nodes);
    OP_DELETE (numberexpression);
  }

  virtual unsigned GetExpressionFlags ()
  {
    return ((nodes->GetExpressionFlags () | numberexpression->GetExpressionFlags ()) & MASK_INHERITED) | FLAG_BOOLEAN;
  }

  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial)
  {
    unsigned &state = context->states[state_index];
    double &number = context->numbers[number_index];

    if (initial)
      {
        nodes->Reset (context);
        state = 0;
      }

    if (state == 0)
      {
        number = numberexpression->EvaluateToNumberL (context, initial);
        state = 1;
      }

    double number_local = number;

    TempBuffer stringvalue; ANCHOR (TempBuffer, stringvalue);

    while (XPath_Node *node = nodes->GetNextNodeL (context))
      {
        node->GetStringValueL (stringvalue);

        XPath_Node::DecRef (context, node);

        if (numbercomparison (XPath_Value::AsNumber (stringvalue.GetStorage ()), number_local))
          return TRUE;

        stringvalue.Clear ();
      }

    return FALSE;
  }
};

/**< Comparison between a nodeset and a string. */
class XPath_NodeSetStringComparison
  : public XPath_BooleanExpression
{
protected:
  XPath_Producer *nodes;
  XPath_StringExpression *stringexpression;
  BOOL equal;

  unsigned state_index, buffer_index;

public:
  XPath_NodeSetStringComparison (XPath_Parser *parser, XPath_Producer *nodes, XPath_StringExpression *expression, BOOL equal)
    : XPath_BooleanExpression (parser),
      nodes (nodes),
      stringexpression (expression),
      equal (equal),
      state_index (parser->GetStateIndex ()),
      buffer_index (parser->GetBufferIndex ())
  {
  }

  virtual ~XPath_NodeSetStringComparison ()
  {
    OP_DELETE (nodes);
    OP_DELETE (stringexpression);
  }

  virtual unsigned GetExpressionFlags ()
  {
    return ((nodes->GetExpressionFlags () | stringexpression->GetExpressionFlags ()) & MASK_INHERITED) | FLAG_BOOLEAN;
  }

  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial)
  {
    unsigned &state = context->states[state_index];
    TempBuffer &buffer = context->buffers[buffer_index];

    if (initial)
      {
        nodes->Reset (context);
        buffer.Clear ();
        state = 0;
      }

    const uni_char *string;

    if (state == 0)
      {
        string = stringexpression->EvaluateToStringL (context, initial, buffer);
        state = 1;

        if (*string && string != buffer.GetStorage ())
          {
            buffer.AppendL (string);
            string = buffer.GetStorage ();
          }
      }
    else if (!(string = buffer.GetStorage ()))
      string = UNI_L ("");

    TempBuffer strbuf; ANCHOR (TempBuffer, strbuf);

    while (XPath_Node *node = nodes->GetNextNodeL (context))
      {
        node->GetStringValueL (strbuf);

        XPath_Node::DecRef (context, node);

        const uni_char* stringvalue = strbuf.GetStorage () ? strbuf.GetStorage () : UNI_L("");
        if (!(uni_str_eq (stringvalue, string)) == !equal)
          return TRUE;

        strbuf.Clear ();
      }

    return FALSE;
  }

  virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data)
  {
    if (transform == TRANSFORM_XMLTREEACCESSOR_FILTER)
      {
        BOOL has_name = FALSE, has_value = FALSE;
        XMLExpandedName name;

        if (equal && stringexpression->HasFlag (FLAG_CONSTANT))
          has_value = TRUE;

        XPath_Step::Axis *previous_axis = static_cast<XPath_Step::Axis *> (nodes->GetPrevious (XPath_Producer::PREVIOUS_AXIS, TRUE));

        if (!previous_axis || previous_axis->GetAxis () != XP_AXIS_ATTRIBUTE || previous_axis->GetPrevious (XPath_Producer::PREVIOUS_CONTEXT_NODE, FALSE) != previous_axis->GetPrevious (XPath_Producer::PREVIOUS_ANY, FALSE))
          return FALSE;

        /* Previous axis is 'attribute', and is not relative another axis or
           node-set expression (is immediately preceded by an initial context
           producer, that is.) */

        XPath_Step::NodeTest *previous_nodetest = static_cast<XPath_Step::NodeTest *> (nodes->GetPrevious (XPath_Producer::PREVIOUS_NODETEST, TRUE));
        if (nodes == previous_nodetest)
          {
            /* We don't expect a node test not immediately preceded by an
               axis, do we? */
            OP_ASSERT (previous_nodetest->GetPrevious (XPath_Producer::PREVIOUS_ANY, FALSE) == previous_axis);

            if (previous_nodetest->GetType () == XP_NODETEST_NAME)
              {
                has_name = TRUE;
                name = static_cast<XPath_NameTest *> (previous_nodetest)->GetName ();
              }
            else if (previous_nodetest->GetType () == XP_NODETEST_NODETYPE)
              if (static_cast<XPath_NodeTypeTest *> (previous_nodetest)->GetNodeType () != XP_NODE_ATTRIBUTE)
                return FALSE;
          }
        else if (nodes != previous_axis)
          /* Probably means there were predicates. */
          return FALSE;
        else if (!previous_nodetest)
          {
            XPath_Producer::TransformData new_data;

            if (previous_axis->TransformL (parser, XPath_Producer::TRANSFORM_ATTRIBUTE_NAME, new_data))
              {
                has_name = TRUE;
                name = *new_data.name;
              }
          }

        if (!has_name && !has_value ||
            (data.filter.filter->flags & XPath_XMLTreeAccessorFilter::FLAG_HAS_ATTRIBUTE_NAME) != 0 ||
            (data.filter.filter->flags & XPath_XMLTreeAccessorFilter::FLAG_HAS_ATTRIBUTE_VALUE) != 0)
          /* No filtering possible, or conflict with already registered
             filters. */
          return FALSE;

        if (has_name)
          {
            XMLExpandedName filter_name;
            if (name.GetLocalPart ()[0] == '*' && !name.GetLocalPart ()[1])
              filter_name = XMLExpandedName (name.GetUri (), UNI_L (""));
            else
              filter_name = name;

            data.filter.filter->attribute_name.SetL (filter_name);
            data.filter.filter->flags |= XPath_XMLTreeAccessorFilter::FLAG_HAS_ATTRIBUTE_NAME;
          }
        if (has_value)
          {
            TempBuffer buffer; ANCHOR (TempBuffer, buffer);

            const uni_char *value = stringexpression->EvaluateToStringL (0, TRUE, buffer);

            data.filter.filter->attribute_value.SetL (value);
            data.filter.filter->flags |= XPath_XMLTreeAccessorFilter::FLAG_HAS_ATTRIBUTE_VALUE;
          }
        else
          data.filter.partial = TRUE;

        return TRUE;
      }

    return FALSE;
  }
};

/* static */ XPath_Expression *
XPath_ComparisonExpression::MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, XPath_ExpressionType type)
{
  unsigned lhs_type = lhs->GetResultType (), rhs_type = rhs->GetResultType ();
  unsigned lhs_flags = lhs->GetExpressionFlags (), rhs_flags = rhs->GetExpressionFlags ();

  BOOL (*numbercomparison) (double lhs, double rhs) = XPath_NumberComparison (type);
  BOOL (*stringcomparison) (const uni_char *lhs, const uni_char *rhs) = XPath_StringComparison (type);

#ifdef XPATH_EXTENSION_SUPPORT
  if ((lhs_flags & FLAG_UNKNOWN) != 0 || (rhs_flags & FLAG_UNKNOWN) != 0)
    {
      XPath_ComparisonExpression *comparison = OP_NEW (XPath_ComparisonExpression, (parser, lhs, rhs, type, numbercomparison, stringcomparison));
      if (!comparison)
        {
          OP_DELETE (lhs);
          OP_DELETE (rhs);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }
      OpStackAutoPtr<XPath_Expression> comparison_anchor (comparison);
      BOOL lhs_nodeset = (lhs_flags & FLAG_UNKNOWN) != 0 || (lhs_flags & FLAG_PRODUCER) != 0;
      BOOL rhs_nodeset = (rhs_flags & FLAG_UNKNOWN) != 0 || (rhs_flags & FLAG_PRODUCER) != 0;

      if (lhs_nodeset)
        {
          // Passing 'lhs' to GetProducerL(); give up comparison's ownership for the duration.
          comparison->lhs = 0;
          comparison->lhs_producer = XPath_Expression::GetProducerL (parser, lhs);
          if ((lhs_flags & FLAG_UNKNOWN) != 0)
            comparison->lhs = lhs;
        }

      if (rhs_nodeset)
        {
          // Passing 'rhs' to GetProducerL(); give up comparison's ownership for the duration.
          comparison->rhs = 0;
          comparison->rhs_producer = XPath_Expression::GetProducerL (parser, rhs);
          if ((rhs_flags & FLAG_UNKNOWN) != 0)
            comparison->rhs = rhs;
        }
      comparison_anchor.release ();
      return comparison;
    }
#endif // XPATH_EXTENSION_SUPPORT

  if ((lhs_flags & FLAG_PRODUCER) && (rhs_flags & FLAG_PRODUCER))
    {
      /* Two node sets. */
      OpStackAutoPtr<XPath_Expression> rhs_anchor (rhs);

      XPath_Producer *lhs_producer = XPath_Expression::GetProducerL (parser, lhs);
      OpStackAutoPtr<XPath_Producer> lhs_producer_anchor (lhs_producer);

      rhs_anchor.release ();
      XPath_Producer *rhs_producer = XPath_Expression::GetProducerL (parser, rhs);
      OpStackAutoPtr<XPath_Producer> rhs_producer_anchor (rhs_producer);

      XPath_Expression *expression;

      if (type == XP_EXPR_EQUAL)
        expression = OP_NEW_L (XPath_TwoNodesetsEqualExpression, (parser, lhs_producer, rhs_producer));
      else if (type == XP_EXPR_NOT_EQUAL)
        expression = OP_NEW_L (XPath_TwoNodesetsUnequalExpression, (parser, lhs_producer, rhs_producer));
      else
        expression = OP_NEW_L (XPath_TwoNodesetsRelationalExpression, (parser, lhs_producer, rhs_producer, type));

      lhs_producer_anchor.release ();
      rhs_producer_anchor.release ();

      return expression;
    }
  else if ((lhs_type == XP_VALUE_BOOLEAN || rhs_type == XP_VALUE_BOOLEAN) && (type == XP_EXPR_EQUAL || type == XP_EXPR_NOT_EQUAL || (lhs_flags & FLAG_PRODUCER) != 0 || (rhs_flags & FLAG_PRODUCER) != 0))
    return XPath_BooleanComparisonExpression::MakeL (parser, lhs, rhs, type);
  else if ((lhs_flags & FLAG_PRODUCER) != 0 || (rhs_flags & FLAG_PRODUCER) != 0)
    {
      XPath_Expression *nodeset, *primitive;

      if (lhs_flags & FLAG_PRODUCER)
        {
          nodeset = lhs;
          primitive = rhs;
        }
      else
        {
          nodeset = rhs;
          primitive = lhs;
          type = XPath_MirrorComparison (type);
          numbercomparison = XPath_NumberComparison (type);
          stringcomparison = XPath_StringComparison (type);
        }

      OpStackAutoPtr<XPath_Expression> primitive_anchor (primitive);

      XPath_Producer *producer = XPath_Expression::GetProducerL (parser, nodeset);
      OpStackAutoPtr<XPath_Producer> producer_anchor (producer);

      XPath_Expression *expression;
      XPath_BooleanExpression *comparison;

      primitive_anchor.release ();
      if (!stringcomparison || primitive->GetResultType () == XP_VALUE_NUMBER)
        {
          expression = XPath_NumberExpression::MakeL (parser, primitive);
          comparison = OP_NEW (XPath_NodeSetNumberComparison, (parser, producer, static_cast<XPath_NumberExpression *> (expression), numbercomparison));
        }
      else
        {
          expression = XPath_StringExpression::MakeL (parser, primitive);
          comparison = OP_NEW (XPath_NodeSetStringComparison, (parser, producer, static_cast<XPath_StringExpression *> (expression), type == XP_EXPR_EQUAL));
        }

      if (!comparison)
        {
          OP_DELETE (expression);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      producer_anchor.release ();
      return comparison;
    }
  else
    {
      unsigned lhs_type = lhs->GetResultType (), rhs_type = rhs->GetResultType ();

      if (type != XP_EXPR_EQUAL && type != XP_EXPR_NOT_EQUAL || lhs_type == XP_VALUE_NUMBER || rhs_type == XP_VALUE_NUMBER)
        return XPath_NumberComparisonExpression::MakeL (parser, lhs, rhs, numbercomparison);
      else
        return XPath_StringComparisonExpression::MakeL (parser, lhs, rhs, stringcomparison);
    }
}

XPath_ComparisonExpression::XPath_ComparisonExpression (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs, XPath_ExpressionType type, BOOL (*numbercomparison) (double lhs, double rhs), BOOL (*stringcomparison) (const uni_char *lhs, const uni_char *rhs))
  : XPath_BooleanExpression (parser),
    lhs (lhs),
    rhs (rhs),
    lhs_producer (0),
    rhs_producer (0),
    other_producer (0),
    type (type),
    state_index (parser->GetStateIndex ()),
    lhs_type_index (parser->GetStateIndex ()),
    rhs_type_index (parser->GetStateIndex ()),
    lhs_value_index (parser->GetValueIndex ()),
    rhs_value_index (parser->GetValueIndex ()),
    comparison_state_index (parser->GetStateIndex ()),
    numbercomparison (numbercomparison),
    stringcomparison (stringcomparison)
{
  if (type == XP_EXPR_EQUAL || type == XP_EXPR_NOT_EQUAL)
    {

      if (type == XP_EXPR_EQUAL)
        {
          lhs_stringset_index = parser->GetStringSetIndex ();
          rhs_stringset_index = parser->GetStringSetIndex ();
        }
      else
        buffer_index = parser->GetBufferIndex ();
    }
  else
    {
      smaller_number_index = parser->GetNumberIndex ();
      larger_number_index = parser->GetNumberIndex ();
    }
}

/* virtual */
XPath_ComparisonExpression::~XPath_ComparisonExpression ()
{
  if (lhs)
    OP_DELETE (lhs);
  else
    OP_DELETE (lhs_producer);

  if (rhs)
    OP_DELETE (rhs);
  else
    OP_DELETE (rhs_producer);
}

/* virtual */ unsigned
XPath_ComparisonExpression::GetExpressionFlags ()
{
  unsigned flags;

  if (lhs)
    flags = lhs->GetExpressionFlags ();
  else
    flags = lhs_producer->GetExpressionFlags ();

  if (rhs)
    flags |= rhs->GetExpressionFlags ();
  else
    flags |= rhs_producer->GetExpressionFlags ();

  return (flags & MASK_INHERITED) | FLAG_BOOLEAN;
}

/* virtual */ BOOL
XPath_ComparisonExpression::EvaluateToBooleanL (XPath_Context *context, BOOL initial)
{
  unsigned &state = context->states[state_index];
  XPath_Value *&lhs_value = context->values[lhs_value_index], *&rhs_value = context->values[rhs_value_index];

#define LHS_GET_IS_INITIAL() ((context->states[lhs_type_index] & 0x80000000u) == 0)
#define LHS_SET_IS_INITIAL() (context->states[lhs_type_index] &= ~0x80000000u)
#define LHS_SET_IS_NOT_INITIAL() (context->states[lhs_type_index] |= 0x80000000u)
#define LHS_GET_TYPE() static_cast<XPath_ValueType> (context->states[lhs_type_index] & ~0x80000000u)
#define LHS_SET_TYPE(type) (context->states[lhs_type_index] = type | (context->states[lhs_type_index] & 0x80000000u))

#define RHS_GET_IS_INITIAL() ((context->states[rhs_type_index] & 0x80000000u) == 0)
#define RHS_SET_IS_INITIAL() (context->states[rhs_type_index] &= ~0x80000000u)
#define RHS_SET_IS_NOT_INITIAL() (context->states[rhs_type_index] |= 0x80000000u)
#define RHS_GET_TYPE() static_cast<XPath_ValueType> (context->states[rhs_type_index] & ~0x80000000u)
#define RHS_SET_TYPE(type) (context->states[rhs_type_index] = type | (context->states[rhs_type_index] & 0x80000000u))

  if (initial)
    {
      state = 0;

      LHS_SET_IS_INITIAL ();
      RHS_SET_IS_INITIAL ();

      XPath_Value::DecRef (context, lhs_value);
      lhs_value = 0;
      XPath_Value::DecRef (context, rhs_value);
      rhs_value = 0;
    }

  XPath_ValueType lhs_type, rhs_type;

  if (state == 0)
    {
      if (lhs)
        if ((lhs->GetExpressionFlags () & FLAG_UNKNOWN) != 0)
          {
            BOOL lhs_initial = LHS_GET_IS_INITIAL ();
            LHS_SET_IS_NOT_INITIAL ();
            lhs_type = static_cast<XPath_Unknown *> (lhs)->GetActualResultTypeL (context, lhs_initial);
          }
        else
          lhs_type = static_cast<XPath_ValueType> (lhs->GetResultType ());
      else
        lhs_type = XP_VALUE_NODESET;

      OP_ASSERT (XPath_IsValidType (lhs_type));

      state = 1;
      LHS_SET_TYPE (lhs_type);
    }
  else
    lhs_type = LHS_GET_TYPE ();

  if (state == 1)
    {
      if (rhs)
        if ((rhs->GetExpressionFlags () & FLAG_UNKNOWN) != 0)
          {
            BOOL rhs_initial = RHS_GET_IS_INITIAL ();
            RHS_SET_IS_NOT_INITIAL ();
            rhs_type = static_cast<XPath_Unknown *> (rhs)->GetActualResultTypeL (context, rhs_initial);
          }
        else
          rhs_type = static_cast<XPath_ValueType> (rhs->GetResultType ());
      else
        rhs_type = XP_VALUE_NODESET;

      OP_ASSERT (XPath_IsValidType (rhs_type));

      state = 2;
      RHS_SET_TYPE (rhs_type);
    }
  else
    rhs_type = RHS_GET_TYPE ();

  BOOL lhs_is_primitive = XPath_IsPrimitive (lhs_type), rhs_is_primitive = XPath_IsPrimitive (rhs_type);

  if (!lhs_is_primitive && !rhs_is_primitive)
    {
      /* Comparing two node-sets by performing the comparison between each
         pair of string values of nodes from the two node-sets.  Collect the
         values from all nodes in one of the node-sets, and then iterate
         through the other node-set comparing its value to all the values from
         the other node-set.  If only one of the expressions claim to be
         expensive, collect the values from the other expression. */

      BOOL initial_state = state == 2, lhs_initial = LHS_GET_IS_INITIAL (), rhs_initial = RHS_GET_IS_INITIAL ();

      state = 3;
      LHS_SET_IS_NOT_INITIAL ();
      RHS_SET_IS_NOT_INITIAL ();

      if (stringcomparison)
        /* We only get a string comparison function for = or !=, so we know
           this isn't <, >, <= or >=, and thus isn't number comparisons. */
        if (type == XP_EXPR_EQUAL)
          return XPath_CompareNodesetsEqual (context, lhs_producer, rhs_producer, initial_state, lhs_initial, rhs_initial, comparison_state_index, lhs_stringset_index, rhs_stringset_index);
        else
          return XPath_CompareNodesetsUnequal (context, lhs_producer, rhs_producer, initial_state, lhs_initial, rhs_initial, comparison_state_index, buffer_index);
      else
        {
          XPath_Producer *smaller, *larger;
          BOOL smaller_initial, larger_initial;

          if (type == XP_EXPR_LESS_THAN || type == XP_EXPR_LESS_THAN_OR_EQUAL)
            {
              smaller = lhs_producer, smaller_initial = lhs_initial;
              larger = rhs_producer, larger_initial = rhs_initial;
            }
          else
            {
              smaller = rhs_producer, smaller_initial = rhs_initial;
              larger = lhs_producer, larger_initial = lhs_initial;
            }

          BOOL or_equal = type == XP_EXPR_LESS_THAN_OR_EQUAL || type == XP_EXPR_GREATER_THAN_OR_EQUAL;

          return XPath_CompareNodesetsRelational (context, smaller, larger, initial_state, smaller_initial, larger_initial, or_equal, comparison_state_index, smaller_number_index, larger_number_index);
        }
    }

  XPath_ValueType conversion[4] = { XP_VALUE_NUMBER, XP_VALUE_BOOLEAN, XP_VALUE_STRING, XP_VALUE_INVALID };

  if (type != XP_EXPR_EQUAL && type != XP_EXPR_NOT_EQUAL)
    conversion[2] = XP_VALUE_NUMBER;

  if (lhs_is_primitive && !lhs_value)
    {
      BOOL lhs_initial = LHS_GET_IS_INITIAL ();

      LHS_SET_IS_NOT_INITIAL ();

      lhs_value = lhs->EvaluateL (context, lhs_initial, conversion);
    }

  if (rhs_is_primitive && !rhs_value)
    {
      BOOL rhs_initial = RHS_GET_IS_INITIAL ();

      RHS_SET_IS_NOT_INITIAL ();

      rhs_value = rhs->EvaluateL (context, rhs_initial, conversion);
    }

  BOOL result = FALSE;

  if (!lhs_is_primitive || !rhs_is_primitive)
    {
      XPath_Producer *producer;
      XPath_Value *value;
      XPath_ExpressionType type0;

      if (lhs_is_primitive)
        {
          producer = rhs_producer;
          value = lhs_value;
          type0 = type;

          if (RHS_GET_IS_INITIAL ())
            {
              producer->Reset (context);
              RHS_SET_IS_NOT_INITIAL ();
            }
        }
      else
        {
          producer = lhs_producer;
          value = rhs_value;
          type0 = XPath_MirrorComparison (type);

          if (LHS_GET_IS_INITIAL ())
            {
              producer->Reset (context);
              LHS_SET_IS_NOT_INITIAL ();
            }
        }

      TempBuffer buffer; ANCHOR (TempBuffer, buffer);

      if (value->type == XP_VALUE_BOOLEAN)
        {
          int number;

          if (XPath_Node *node = producer->GetNextNodeL (context))
            {
              XPath_Node::DecRef (context, node);
              number = 1;
            }
          else
            number = 0;

          if (type == XP_EXPR_EQUAL || type == XP_EXPR_NOT_EQUAL)
            result = (number == 1) == (type == XP_EXPR_EQUAL);
          else
            result = XPath_NumberComparison (type0) (value->data.boolean ? 1 : 0, number);
        }
      else
        while (XPath_Node *node = producer->GetNextNodeL (context))
          {
            node->GetStringValueL (buffer);

            XPath_Node::DecRef (context, node);

            const uni_char *string = buffer.GetStorage () ? buffer.GetStorage (): UNI_L("");

            if (value->type == XP_VALUE_NUMBER)
              {
                double number = XPath_Value::AsNumber (string);

                if (lhs_is_primitive ? numbercomparison (value->data.number, number) : numbercomparison (number, value->data.number))
                  result = TRUE;
              }
            else if (lhs_is_primitive ? stringcomparison (value->data.string, string) : stringcomparison (string, value->data.string))
              result = TRUE;

            if (result)
              break;
          }
    }
  else
    {
      if ((type == XP_EXPR_EQUAL || type == XP_EXPR_NOT_EQUAL) && (lhs_type == XP_VALUE_BOOLEAN || rhs_type == XP_VALUE_BOOLEAN || lhs_type == XP_VALUE_STRING && rhs_type == XP_VALUE_STRING))
        if (lhs_type == XP_VALUE_BOOLEAN || rhs_type == XP_VALUE_BOOLEAN)
          result = (!lhs_value->AsBoolean () == !rhs_value->AsBoolean ()) == (type == XP_EXPR_EQUAL);
        else
          result = stringcomparison (lhs_value->data.string, rhs_value->data.string);
      else
        result = numbercomparison (lhs_value->AsNumberL (), rhs_value->AsNumberL ());
    }

  XPath_Value::DecRef (context, lhs_value);
  lhs_value = 0;
  XPath_Value::DecRef (context, rhs_value);
  rhs_value = 0;

  return result;
}

XPath_TwoNodesetsEqualExpression::XPath_TwoNodesetsEqualExpression (XPath_Parser *parser, XPath_Producer *lhs, XPath_Producer *rhs)
  : XPath_BooleanExpression (parser),
    lhs (lhs),
    rhs (rhs),
    state_index (parser->GetStateIndex ()),
    lhs_stringset_index (parser->GetStringSetIndex ()),
    rhs_stringset_index (parser->GetStringSetIndex ())
{
}

/* virtual */
XPath_TwoNodesetsEqualExpression::~XPath_TwoNodesetsEqualExpression ()
{
  OP_DELETE (lhs);
  OP_DELETE (rhs);
}

/* virtual */ unsigned
XPath_TwoNodesetsEqualExpression::GetExpressionFlags ()
{
  return ((lhs->GetExpressionFlags () | rhs->GetExpressionFlags ()) & MASK_INHERITED) | FLAG_BOOLEAN;
}

/* virtual */ BOOL
XPath_TwoNodesetsEqualExpression::EvaluateToBooleanL (XPath_Context *context, BOOL initial)
{
  return XPath_CompareNodesetsEqual (context, lhs, rhs, initial, initial, initial, state_index, lhs_stringset_index, rhs_stringset_index);
}

XPath_TwoNodesetsUnequalExpression::XPath_TwoNodesetsUnequalExpression (XPath_Parser *parser, XPath_Producer *lhs, XPath_Producer *rhs)
  : XPath_BooleanExpression (parser),
    lhs (lhs),
    rhs (rhs),
    state_index (parser->GetStateIndex ()),
    buffer_index (parser->GetBufferIndex ())
{
}

/* virtual */
XPath_TwoNodesetsUnequalExpression::~XPath_TwoNodesetsUnequalExpression ()
{
  OP_DELETE (lhs);
  OP_DELETE (rhs);
}

/* virtual */ unsigned
XPath_TwoNodesetsUnequalExpression::GetExpressionFlags ()
{
  return ((lhs->GetExpressionFlags () | rhs->GetExpressionFlags ()) & MASK_INHERITED) | FLAG_BOOLEAN;
}

/* virtual */ BOOL
XPath_TwoNodesetsUnequalExpression::EvaluateToBooleanL (XPath_Context *context, BOOL initial)
{
  return XPath_CompareNodesetsUnequal (context, lhs, rhs, initial, initial, initial, state_index, buffer_index);
}

XPath_TwoNodesetsRelationalExpression::XPath_TwoNodesetsRelationalExpression (XPath_Parser *parser, XPath_Producer *lhs, XPath_Producer *rhs, XPath_ExpressionType type)
  : XPath_BooleanExpression (parser),
    or_equal (type == XP_EXPR_LESS_THAN_OR_EQUAL || type == XP_EXPR_GREATER_THAN_OR_EQUAL),
    state_index (parser->GetStateIndex ()),
    smaller_number_index (parser->GetNumberIndex ()),
    larger_number_index (parser->GetNumberIndex ())
{
  if (type == XP_EXPR_LESS_THAN || type == XP_EXPR_LESS_THAN_OR_EQUAL)
    smaller = lhs, larger = rhs;
  else
    smaller = rhs, larger = lhs;
}

/* virtual */
XPath_TwoNodesetsRelationalExpression::~XPath_TwoNodesetsRelationalExpression ()
{
  OP_DELETE (smaller);
  OP_DELETE (larger);
}

/* virtual */ unsigned
XPath_TwoNodesetsRelationalExpression::GetExpressionFlags ()
{
  return ((smaller->GetExpressionFlags () | larger->GetExpressionFlags ()) & MASK_INHERITED) | FLAG_BOOLEAN;
}

/* virtual */ BOOL
XPath_TwoNodesetsRelationalExpression::EvaluateToBooleanL (XPath_Context *context, BOOL initial)
{
  return XPath_CompareNodesetsRelational (context, smaller, larger, initial, initial, initial, or_equal, state_index, smaller_number_index, larger_number_index);
}

#endif // XPATH_SUPPORT
