/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPEXPR_H
#define XPEXPR_H

#include "modules/xpath/src/xpdefs.h"
#include "modules/xpath/src/xpproducer.h"
#include "modules/xpath/src/xpcontext.h"

class XPath_Value;
class XPath_Context;
class XPath_Parser;

class XPath_Expression
{
private:
  /* Functions that should be implemented (in some cases only if needed) by
     sub-classes, but that are only called by non-virtual functions in this
     class and its friends. */

  friend class XPath_NumberConversionExpression;
  friend class XPath_BooleanConversionExpression;
  friend class XPath_StringConversionExpression;

  XPath_Value *EvaluateToValueL (XPath_Context *context, BOOL initial);
  /**< Evaluate expression to a value.  Normally, one of the EvaluateL()
       variants should be called instead, for value conversions.  This
       function will be used by GetConstantValueL() to fetch an expression's
       constant value, if it reports FLAG_CONSTANT.  In such a call, the
       'context' argument will be NULL.

       The 'initial' argument indicates whether the caller is starting a
       new evaluation of the expression or is just continuing a previously
       suspended evaluation.  Implementations should be prepared to abandon
       any suspended evaluation if the next call has 'initial==TRUE'. */

  virtual XPath_Producer *GetProducerInternalL (XPath_Parser *parser);
  /**< Only called if the expression reports FLAG_PRODUCER.  See
       GetProducerL(). */

public:
  XPath_Expression (XPath_Parser *parser);
  virtual ~XPath_Expression ();

  virtual unsigned GetResultType () = 0;

  enum Flags
    {
      FLAG_CONTEXT_SIZE = 0x0001,
      /**< Expression depends on the context size. */

      FLAG_CONTEXT_POSITION = 0x0002,
      /**< Expression depends on the context position. */

      FLAG_EXPENSIVE = 0x0004,
      /**< Expression is expensive to evaluate (guesstimate.) */

      FLAG_PRODUCER = 0x0008,
      /**< Can produce nodes as an XPath_Producer. */

      FLAG_CONSTANT = 0x0010,
      /**< Expression has a constant value. */

      FLAG_NUMBER = 0x0020,
      /**< Is sub-class of XPath_NumberExpression. */

      FLAG_BOOLEAN = 0x0040,
      /**< Is sub-class of XPath_BooleanExpression. */

      FLAG_STRING = 0x0080,
      /**< Is sub-class of XPath_StringExpression. */

      FLAG_UNION = 0x0100,
      /**< Is a union expression. */

      FLAG_FUNCTIONCALL = 0x0200,
      /**< Is a function call expression. */

      FLAG_UNKNOWN = 0x0400,
      /**< Is an expression with (possibly) unknown result type, that is, an
           XPath_Unknown object.  Can be cast into an XPath_Producer instead
           of transformed into one. */

      FLAG_BLOCKING = 0x0800,
      /**< Expression can block. */

      FLAG_COMPLEXITY_SIMPLE = 0x1000,
      FLAG_COMPLEXITY_COMPLEX = 0x2000,

      COMPLEXITY_TRIVIAL = 0,
      /**< No location paths, no variable references, no extension function
           calls, no calls to possibly expensive functions. */

      COMPLEXITY_SIMPLE = FLAG_COMPLEXITY_SIMPLE,
      /**< Like COMPLEXITY_SIMPLE, but can include single-step location paths
           that use reasonably safe axes (attribute, namespace, child, parent,
           previous-sibling and following-sibling) and that have no predicates
           that aren't trivial. */

      MASK_COMPLEXITY = FLAG_COMPLEXITY_SIMPLE | FLAG_COMPLEXITY_COMPLEX,
      /**< Expression complexity. */

      MASK_INHERITED = FLAG_CONTEXT_SIZE | FLAG_CONTEXT_POSITION | FLAG_BLOCKING | MASK_COMPLEXITY
      /**< Only FLAG_CONTEXT_SIZE, FLAG_CONTEXT_POSITION and FLAG_EXPENSIVE
           are reasonable to inherit. */
    };

  virtual unsigned GetExpressionFlags ();

  unsigned GetPredicateExpressionFlags ();

  BOOL HasFlag (Flags flag) { return (GetExpressionFlags () & flag) != 0; }

  /** Indeces into the 'types' argument array to EvaluateL(). */
  enum WhenType
    {
      WHEN_NUMBER,
      WHEN_BOOLEAN,
      WHEN_STRING,
      WHEN_NODESET
    };

  XPath_Value *EvaluateL (XPath_Context *context, BOOL initial, const XPath_ValueType *types = 0, XPath_ValueType known_resulttype = XP_VALUE_INVALID);
  OP_BOOLEAN Evaluate (XPath_Value *&value, XPath_Context *context, BOOL initial, const XPath_ValueType types[4]);

  enum Transform
    {
      TRANSFORM_PRODUCER,
      /**< Sets TransformData::producer.  Destructive. */
      TRANSFORM_XMLTREEACCESSOR_FILTER,
      /**< Updates TransformData::filter.  Not destructive. */
      TRANSFORM_XMLEXPANDEDNAME
      /**< Updates TransformData::name with the expression's name (function
           name for function call, variable name for variable reference.)  Not
           destructive.  The 'parser' argument may be null. */
    };

  union TransformData
  {
  public:
    XPath_Producer *producer;
    struct Filter
    {
      XPath_XMLTreeAccessorFilter *filter;
      bool partial;
    } filter;
    XMLExpandedName *name;
  };

  virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data);

  BOOL IsFunctionCall (XMLExpandedName &name);
  /**< Returns TRUE if this expression is a function call, and if so, also
       sets 'name' to the called function's name. */

  static XPath_Producer *GetProducerL (XPath_Parser *parser, XPath_Expression *expression);
  /**< If 'expression' has FLAG_PRODUCER, transforms 'expression' to a
       producer, and deletes 'expression' afterwards if appropriate.  On
       failure, also deletes 'expression' before leaving.  If 'expression'
       doesn't have FLAG_PRODUCER, it is not deleted and 0 is returned. */

#ifdef XPATH_ERRORS
  XPath_Location location;
#endif // XPATH_ERRORS
};

class XPath_ProducerExpression
  : public XPath_Expression
{
protected:
  XPath_Producer *producer;

  virtual XPath_Producer *GetProducerInternalL (XPath_Parser *parser);

public:
  XPath_ProducerExpression (XPath_Parser *parser, XPath_Producer *producer);
  ~XPath_ProducerExpression ();

  virtual unsigned GetResultType ();
  virtual unsigned GetExpressionFlags ();
};

class XPath_NumberExpression
  : public XPath_Expression
{
protected:
  XPath_NumberExpression (XPath_Parser *parser)
    : XPath_Expression (parser)
  {
  }

public:
  static XPath_NumberExpression *MakeL (XPath_Parser *parser, XPath_Expression *expression);

  virtual unsigned GetResultType ();
  virtual unsigned GetExpressionFlags ();

  virtual double EvaluateToNumberL (XPath_Context *context, BOOL initial) = 0;
};

class XPath_BooleanExpression
  : public XPath_Expression
{
protected:
  XPath_BooleanExpression (XPath_Parser *parser)
    : XPath_Expression (parser)
  {
  }

public:
  static XPath_BooleanExpression *MakeL (XPath_Parser *parser, XPath_Expression *expression);

  virtual unsigned GetResultType ();
  virtual unsigned GetExpressionFlags ();

  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial) = 0;
};

class XPath_StringExpression
  : public XPath_Expression
{
protected:
  XPath_StringExpression (XPath_Parser *parser)
    : XPath_Expression (parser)
  {
  }

public:
  static XPath_StringExpression *MakeL (XPath_Parser *parser, XPath_Expression *expression);

  virtual unsigned GetResultType ();
  virtual unsigned GetExpressionFlags ();

  virtual const uni_char *EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer) = 0;
  /**< Evaluate to string.  If the string needs to be generated/copied before
       it can be returned, 'buffer' will be cleared and the string will be
       stored in it.  In any case, 'string' will be set to a null-terminated
       string on success. */
};

#endif // XPEXPR_H
