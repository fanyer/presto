/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPFUNCTION_H
#define XPFUNCTION_H

#include "modules/xpath/src/xpexpr.h"

#include "modules/util/adt/opvector.h"

class XPath_Function;
#ifdef XPATH_EXTENSION_SUPPORT
class XPath_Unknown;
#endif // XPATH_EXTENSION_SUPPORT

class XPath_FunctionCallExpression
{
public:
  enum FunctionType
    {
      BUILTIN_LAST,
      BUILTIN_POSITION,
      BUILTIN_COUNT,
      BUILTIN_ID,
      BUILTIN_LOCAL_NAME,
      BUILTIN_NAMESPACE_URI,
      BUILTIN_NAME,
      BUILTIN_STRING,
      BUILTIN_CONCAT,
      BUILTIN_STARTS_WITH,
      BUILTIN_CONTAINS,
      BUILTIN_SUBSTRING_BEFORE,
      BUILTIN_SUBSTRING_AFTER,
      BUILTIN_SUBSTRING,
      BUILTIN_STRING_LENGTH,
      BUILTIN_NORMALIZE_SPACE,
      BUILTIN_TRANSLATE,
      BUILTIN_BOOLEAN,
      BUILTIN_NOT,
      BUILTIN_TRUE,
      BUILTIN_FALSE,
      BUILTIN_LANG,
      BUILTIN_NUMBER,
      BUILTIN_SUM,
      BUILTIN_FLOOR,
      BUILTIN_CEILING,
      BUILTIN_ROUND,

      EXTENSION_FUNCTION,
      ECMASCRIPT_FUNCTION
    };

  static XPath_Expression *MakeL (XPath_Parser *parser, const XMLExpandedName &name, XPath_Expression **arguments, unsigned arguments_count);
};

class XPath_IdProducer
  : public XPath_Producer
{
protected:
  XPath_Producer *producer;
  XPath_StringExpression *stringexpression;
#ifdef XPATH_EXTENSION_SUPPORT
  XPath_Unknown *unknown;
  unsigned unknownstate_index;
#endif // XPATH_EXTENSION_SUPPORT

  unsigned state_index, offset_index, value_index, node_index, localci_index;

  XPath_IdProducer (XPath_Parser *parser, XPath_Producer *producer, XPath_StringExpression *stringexpression);
#ifdef XPATH_EXTENSION_SUPPORT
  XPath_IdProducer (XPath_Parser *parser, XPath_Unknown *unknown);
#endif // XPATH_EXTENSION_SUPPORT

public:
  static XPath_Producer *MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count);

  virtual ~XPath_IdProducer ();

  virtual unsigned GetProducerFlags ();

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);

  virtual XPath_Node *GetNextNodeL (XPath_Context *context);
};

class XPath_CumulativeNodeSetFunctionCall
  : public XPath_NumberExpression
{
public:
  enum Type
    {
      TYPE_count,
      TYPE_sum
    };

private:
  Type type;
  XPath_Producer *producer;
  unsigned count_index, sum_index;

  XPath_CumulativeNodeSetFunctionCall (XPath_Parser *parser, Type type, XPath_Producer *producer);

public:
  static XPath_Expression *MakeL (XPath_Parser *parser, Type type, XPath_Expression **arguments, unsigned arguments_count);

  virtual ~XPath_CumulativeNodeSetFunctionCall ();

  virtual double EvaluateToNumberL (XPath_Context *context, BOOL initial);
};

/* Functions that take an optional node-set argument and process the
   first node in document order in that node-set, or the context node
   if the argument was omitted. */
class XPath_SingleNodeFunctionCall
  : public XPath_StringExpression
{
public:
  enum Type
    {
      TYPE_localname,
      TYPE_namespaceuri,
      TYPE_name
    };

protected:
  XPath_SingleNodeFunctionCall (XPath_Parser *parser, Type type, XPath_Producer *argument);

  Type type;
  XPath_Producer *argument;

public:
  static XPath_Expression *MakeL (XPath_Parser *parser, Type type, XPath_Expression **arguments, unsigned arguments_count);

  virtual ~XPath_SingleNodeFunctionCall ();
  virtual unsigned GetExpressionFlags ();
  virtual const uni_char *EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer);
};

class XPath_LangFunctionCall
  : public XPath_BooleanExpression
{
private:
  XPath_LangFunctionCall (XPath_Parser *parser, XPath_StringExpression *argument)
    : XPath_BooleanExpression (parser),
      argument (argument)
  {
  }

  XPath_StringExpression *argument;

public:
  static XPath_BooleanExpression *MakeL (XPath_Parser *parser, XPath_Expression **arguments);

  virtual ~XPath_LangFunctionCall ();
  virtual unsigned GetExpressionFlags ();
  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial);
};

class XPath_NumberFunctionCall
  : public XPath_NumberExpression
{
public:
  enum Type
    {
      TYPE_floor,
      TYPE_ceiling,
      TYPE_round
    };

private:
  Type type;
  XPath_NumberExpression *argument;

  XPath_NumberFunctionCall (XPath_Parser *parser, Type type, XPath_NumberExpression *argument);

public:
  static XPath_NumberExpression *MakeL (XPath_Parser *parser, Type type, XPath_Expression **arguments, unsigned arguments_count);

  virtual ~XPath_NumberFunctionCall ();
  virtual unsigned GetExpressionFlags ();
  virtual double EvaluateToNumberL (XPath_Context *context, BOOL initial);
};

class XPath_ContextFunctionCall
  : public XPath_NumberExpression
{
public:
  enum Type
    {
      TYPE_position,
      TYPE_last
    };

private:
  Type type;

public:
  XPath_ContextFunctionCall (XPath_Parser *parser, Type type);

  virtual unsigned GetExpressionFlags ();
  virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data);
  virtual double EvaluateToNumberL (XPath_Context *context, BOOL initial);
};

class XPath_ConcatFunctionCall
  : public XPath_StringExpression
{
private:
  XPath_StringExpression **arguments;
  unsigned arguments_count;

  unsigned state_index, buffer_index;

  XPath_ConcatFunctionCall (XPath_Parser *parser);

public:
  static XPath_StringExpression *MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count);

  virtual ~XPath_ConcatFunctionCall ();
  virtual unsigned GetExpressionFlags ();
  virtual const uni_char *EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer);
};

class XPath_SubstringFunctionCall
  : public XPath_StringExpression
{
private:
  XPath_StringExpression *base;
  XPath_NumberExpression *offset, *length;
  unsigned state_index, offset_index, length_index;

  XPath_SubstringFunctionCall (XPath_Parser *parser, XPath_StringExpression *base, XPath_NumberExpression *offset, XPath_NumberExpression *length);

public:
  static XPath_StringExpression *MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count);

  virtual ~XPath_SubstringFunctionCall ();
  virtual unsigned GetExpressionFlags ();
  virtual const uni_char *EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer);
};

class XPath_TranslateFunctionCall
  : public XPath_StringExpression
{
private:
  XPath_StringExpression *base, *from, *to;
  unsigned state_index, from_buffer_index, to_buffer_index;

  XPath_TranslateFunctionCall (XPath_Parser *parser, XPath_StringExpression *base, XPath_StringExpression *from, XPath_StringExpression *to);

public:
  static XPath_StringExpression *MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count);

  virtual ~XPath_TranslateFunctionCall ();
  virtual unsigned GetExpressionFlags ();
  virtual const uni_char *EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer);
};

class XPath_NormalizeSpaceFunctionCall
  : public XPath_StringExpression
{
private:
  XPath_StringExpression *base;

  XPath_NormalizeSpaceFunctionCall (XPath_Parser *parser, XPath_StringExpression *base);

public:
  static XPath_StringExpression *MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count);

  virtual ~XPath_NormalizeSpaceFunctionCall ();
  virtual unsigned GetExpressionFlags ();
  virtual const uni_char *EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer);
};

class XPath_StringLengthFunctionCall
  : public XPath_NumberExpression
{
private:
  XPath_StringExpression *base;

  XPath_StringLengthFunctionCall (XPath_Parser *parser, XPath_StringExpression *base);

public:
  static XPath_NumberExpression *MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count);

  virtual ~XPath_StringLengthFunctionCall ();
  virtual unsigned GetExpressionFlags ();
  virtual double EvaluateToNumberL (XPath_Context *context, BOOL initial);
};

class XPath_BinaryStringFunctionCallBase
{
protected:
  XPath_StringExpression *argument1, *argument2;
  unsigned state_index, buffer1_index, buffer2_index;

  XPath_BinaryStringFunctionCallBase (XPath_Parser *parser);
  ~XPath_BinaryStringFunctionCallBase ();

  void SetArgumentsL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count);
  void EvaluateArgumentsL (XPath_Context *context, BOOL initial, const uni_char *&value1, const uni_char *&value2);
  unsigned GetExpressionFlags ();
};

class XPath_StartsWithOrContainsFunctionCall
  : public XPath_BooleanExpression,
    private XPath_BinaryStringFunctionCallBase
{
public:
  enum Type
    {
      TYPE_starts_with,
      TYPE_contains
    };

private:
  Type type;

  XPath_StartsWithOrContainsFunctionCall (XPath_Parser *parser, Type type);

public:
  static XPath_BooleanExpression *MakeL (XPath_Parser *parser, Type type, XPath_Expression **arguments, unsigned arguments_count);

  virtual unsigned GetExpressionFlags ();
  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial);
};

class XPath_SubstringBeforeOrAfterFunctionCall
  : public XPath_StringExpression,
    private XPath_BinaryStringFunctionCallBase
{
public:
  enum Type
    {
      TYPE_substring_before,
      TYPE_substring_after
    };

private:
  Type type;

  XPath_SubstringBeforeOrAfterFunctionCall (XPath_Parser *parser, Type type);

public:
  static XPath_StringExpression *MakeL (XPath_Parser *parser, Type type, XPath_Expression **arguments, unsigned arguments_count);

  virtual unsigned GetExpressionFlags ();
  virtual const uni_char *EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer);
};

class XPath_NotFunctionCall
  : public XPath_BooleanExpression
{
private:
  XPath_NotFunctionCall (XPath_Parser *parser, XPath_BooleanExpression *argument)
    : XPath_BooleanExpression (parser),
      argument (argument)
  {
  }

  XPath_BooleanExpression *argument;

public:
  static XPath_Expression *MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count);

  virtual ~XPath_NotFunctionCall ();
  virtual unsigned GetExpressionFlags ();
  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial);
};

#endif // XPFUNCTION_H
