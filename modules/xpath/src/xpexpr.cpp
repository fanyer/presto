/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpexpr.h"
#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xpcontext.h"
#include "modules/xpath/src/xpnodeset.h"
#include "modules/xpath/src/xputils.h"
#include "modules/xpath/src/xpunknown.h"
#include "modules/xpath/src/xpparser.h"

BOOL
XPath_IsPrimitive (XPath_ValueType type)
{
  return type == XP_VALUE_NUMBER || type == XP_VALUE_BOOLEAN || type == XP_VALUE_STRING;
}

#ifdef DEBUG_ENABLE_OPASSERT

BOOL
XPath_IsValidType (XPath_ValueType type)
{
  return type == XP_VALUE_NODESET || type == XP_VALUE_NUMBER || type == XP_VALUE_BOOLEAN || type == XP_VALUE_STRING;
}

#endif // DEBUG_ENABLE_OPASSERT

XPath_Value *
XPath_Expression::EvaluateToValueL (XPath_Context *context, BOOL initial)
{
  unsigned expression_flags = GetExpressionFlags ();

#ifdef XPATH_EXTENSION_SUPPORT
  if ((expression_flags & FLAG_UNKNOWN) != 0)
    return static_cast<XPath_Unknown *> (this)->EvaluateToValueL (context, initial);
#endif // XPATH_EXTENSION_SUPPORT

  if ((expression_flags & FLAG_NUMBER) != 0)
    return XPath_Value::MakeNumberL (context, static_cast<XPath_NumberExpression *> (this)->EvaluateToNumberL (context, initial));
  else if ((expression_flags & FLAG_BOOLEAN) != 0)
    return XPath_Value::MakeBooleanL (context, static_cast<XPath_BooleanExpression *> (this)->EvaluateToBooleanL (context, initial));
  else
    {
      OP_ASSERT ((expression_flags & FLAG_STRING) != 0);
      TempBuffer buffer; ANCHOR (TempBuffer, buffer);
      return XPath_Value::MakeStringL (context, static_cast<XPath_StringExpression *> (this)->EvaluateToStringL (context, initial, buffer));
    }
}

XPath_Producer *
XPath_Expression::GetProducerInternalL (XPath_Parser *parser)
{
  /* Shouldn't be called. */
  OP_ASSERT (FALSE);

  return 0;
}

XPath_Expression::XPath_Expression (XPath_Parser *parser)
{
#ifdef XPATH_ERRORS
  location = parser->GetCurrentLocation ();
#endif // XPATH_ERRORS
}

/* virtual */
XPath_Expression::~XPath_Expression ()
{
}

/* virtual */ unsigned
XPath_Expression::GetExpressionFlags ()
{
  return 0;
}

unsigned
XPath_Expression::GetPredicateExpressionFlags ()
{
  unsigned flags = GetExpressionFlags ();

  switch (GetResultType ())
    {
    case XP_VALUE_NUMBER:
    case XP_VALUE_INVALID:
      flags |= XPath_Expression::FLAG_CONTEXT_POSITION;
    }

  return flags;
}

XPath_Value *
XPath_Expression::EvaluateL (XPath_Context *context, BOOL initial, const XPath_ValueType *types, XPath_ValueType known_resulttype)
{
  unsigned resulttype = known_resulttype == XP_VALUE_INVALID ? GetResultType () : static_cast<unsigned> (known_resulttype);
  XPath_Value *value;

  if (resulttype == XP_VALUE_INVALID)
    {
      value = EvaluateToValueL (context, initial);
      resulttype = value->type;
    }
  else
    value = 0;

  WhenType when;

  switch (resulttype)
    {
    case XP_VALUE_NUMBER: when = WHEN_NUMBER; break;
    case XP_VALUE_BOOLEAN: when = WHEN_BOOLEAN; break;
    case XP_VALUE_STRING: when = WHEN_STRING; break;
    default: when = WHEN_NODESET;
    }

  unsigned targettype = types ? static_cast<unsigned> (types[when]) : resulttype;

  if (resulttype == targettype)
    return value ? value : EvaluateToValueL (context, initial);
  else if (targettype == XP_VALUE_NUMBER || targettype == XP_VALUE_BOOLEAN || targettype == XP_VALUE_STRING)
    {
      if (!value)
        value = EvaluateToValueL (context, initial);

      XP_ANCHOR_VALUE (context, value);

      if (targettype == XP_VALUE_NUMBER)
        return value->ConvertToNumberL (context);
      else if (targettype == XP_VALUE_BOOLEAN)
        return value->ConvertToBooleanL (context);
      else
        return value->ConvertToStringL (context);
    }
  else
    {
      XPATH_EVALUATION_ERROR ("expected node-set", this);
      return 0;
    }
}

OP_BOOLEAN
XPath_Expression::Evaluate (XPath_Value *&value, XPath_Context *context, BOOL initial, const XPath_ValueType types[4])
{
  TRAPD (status, value = EvaluateL (context, initial, types));
  return status == OpStatus::OK ? OpBoolean::IS_TRUE : status;
}

/* virtual */ BOOL
XPath_Expression::TransformL (XPath_Parser *parser, Transform transform, TransformData &data)
{
  if (transform == TRANSFORM_PRODUCER)
    {
      if (HasFlag (FLAG_PRODUCER))
        {
          data.producer = GetProducerInternalL (parser);
          OP_ASSERT (data.producer);
          return TRUE;
        }
    }

  return FALSE;
}

BOOL
XPath_Expression::IsFunctionCall (XMLExpandedName &name)
{
  if (HasFlag (XPath_Expression::FLAG_FUNCTIONCALL))
    {
      XPath_Expression::TransformData data;
      data.name = &name;

      if (TransformL (0, TRANSFORM_XMLEXPANDEDNAME, data))
        return TRUE;
    }

  return FALSE;
}

/* static */ XPath_Producer *
XPath_Expression::GetProducerL (XPath_Parser *parser, XPath_Expression *expression)
{
#ifdef XPATH_EXTENSION_SUPPORT
  if ((expression->GetExpressionFlags () & FLAG_UNKNOWN) != 0)
    return static_cast<XPath_Unknown *> (expression);
  else
#endif // XPATH_EXTENSION_SUPPORT
    {
      OpStackAutoPtr<XPath_Expression> expression_anchor (expression);
      TransformData data;

      if (expression->TransformL (parser, TRANSFORM_PRODUCER, data))
        return data.producer;
      else
        {
          expression_anchor.release ();
          return 0;
        }
    }
}

/* virtual */ XPath_Producer *
XPath_ProducerExpression::GetProducerInternalL (XPath_Parser *parser)
{
  XPath_Producer *temporary = producer;
  producer = 0;
  return temporary;
}

XPath_ProducerExpression::XPath_ProducerExpression (XPath_Parser *parser, XPath_Producer *producer)
  : XPath_Expression (parser),
    producer (producer)
{
}

/* virtual */
XPath_ProducerExpression::~XPath_ProducerExpression ()
{
  OP_DELETE (producer);
}

/* virtual */ unsigned
XPath_ProducerExpression::GetResultType ()
{
  return XP_VALUE_NODESET;
}

/* virtual */ unsigned
XPath_ProducerExpression::GetExpressionFlags ()
{
  return producer->GetExpressionFlags () | FLAG_PRODUCER;
}

class XPath_ConversionExpressionHelper
{
private:
  XPath_NumberExpression *numberexpression;
  XPath_BooleanExpression *booleanexpression;
  XPath_StringExpression *stringexpression;
  XPath_Producer *producer;
#ifdef XPATH_EXTENSION_SUPPORT
  XPath_Unknown *unknown;
  unsigned state_index;
#endif // XPATH_EXTENSION_SUPPORT

  BOOL GetNodeL (XPath_Context *context, BOOL initial, XPath_Node *&node);
  BOOL GetStringValueL (XPath_Context *context, BOOL initial, TempBuffer &buffer);

public:
  XPath_ConversionExpressionHelper ();
  ~XPath_ConversionExpressionHelper ();

  void InitializeL (XPath_Parser *parser, XPath_Expression *expression, BOOL ordered);
  void StartL (XPath_Context *context, BOOL &initial);

  unsigned GetExpressionFlags ();

  double GetNumberL (XPath_Context *context, BOOL initial);
  BOOL GetBooleanL (XPath_Context *context, BOOL initial);
  const uni_char *GetStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer);
};

BOOL
XPath_ConversionExpressionHelper::GetNodeL (XPath_Context *context, BOOL initial, XPath_Node *&node)
{
#ifdef XPATH_EXTENSION_SUPPORT
  if (unknown)
    {
      OP_ASSERT (!initial);
      if (context->states[state_index] == XP_VALUE_NODESET)
        {
          node = producer->GetNextNodeL (context);
          return TRUE;
        }
      else
        return FALSE;
    }
#endif // XPATH_EXTENSION_SUPPORT

  if (producer)
    {
      if (initial)
        producer->Reset (context);
      node = producer->GetNextNodeL (context);
      return TRUE;
    }
  else if (!numberexpression && !booleanexpression && !stringexpression)
    {
      node = XPath_Node::IncRef (context->node);
      return TRUE;
    }
  else
    return FALSE;
}

BOOL
XPath_ConversionExpressionHelper::GetStringValueL (XPath_Context *context, BOOL initial, TempBuffer &buffer)
{
  XPath_Node *node;

  if (GetNodeL (context, initial, node))
    {
      if (node)
        {
          node->GetStringValueL (buffer);
          XPath_Node::DecRef (context, node);
        }
      return TRUE;
    }
  else
    return FALSE;
}

XPath_ConversionExpressionHelper::XPath_ConversionExpressionHelper ()
  : numberexpression (0),
    booleanexpression (0),
    stringexpression (0),
    producer (0)
{
#ifdef XPATH_EXTENSION_SUPPORT
  unknown = 0;
#endif // XPATH_EXTENSION_SUPPORT
}

XPath_ConversionExpressionHelper::~XPath_ConversionExpressionHelper ()
{
  OP_DELETE (numberexpression);
  OP_DELETE (booleanexpression);
  OP_DELETE (stringexpression);
#ifdef XPATH_EXTENSION_SUPPORT
  if (producer != unknown)
    OP_DELETE (producer);
  OP_DELETE (unknown);
#else // XPATH_EXTENSION_SUPPORT
  OP_DELETE (producer);
#endif // XPATH_EXTENSION_SUPPORT
}

void
XPath_ConversionExpressionHelper::InitializeL (XPath_Parser *parser, XPath_Expression *expression, BOOL ordered)
{
  if (expression)
    {
      unsigned flags = expression->GetExpressionFlags ();

      if ((flags & XPath_Expression::FLAG_NUMBER) != 0)
        numberexpression = static_cast<XPath_NumberExpression *> (expression);
      else if ((flags & XPath_Expression::FLAG_BOOLEAN) != 0)
        booleanexpression = static_cast<XPath_BooleanExpression *> (expression);
      else if ((flags & XPath_Expression::FLAG_STRING) != 0)
        stringexpression = static_cast<XPath_StringExpression *> (expression);
#ifdef XPATH_EXTENSION_SUPPORT
      else if ((flags & XPath_Expression::FLAG_UNKNOWN) != 0)
        {
          unknown = static_cast<XPath_Unknown *> (expression);
          producer = XPath_Producer::EnsureFlagsL (parser, unknown, (ordered ? XPath_Producer::FLAG_DOCUMENT_ORDER : 0) | XPath_Producer::FLAG_SECONDARY_WRAPPER);
          state_index = parser->GetStateIndex ();
        }
#endif // XPATH_EXTENSION_SUPPORT
      else
        producer = XPath_Producer::EnsureFlagsL (parser, XPath_Expression::GetProducerL (parser, expression), (ordered ? XPath_Producer::FLAG_DOCUMENT_ORDER : 0) | XPath_Producer::FLAG_SINGLE_NODE);
    }
}

void
XPath_ConversionExpressionHelper::StartL (XPath_Context *context, BOOL &initial)
{
#ifdef XPATH_EXTENSION_SUPPORT
  if (unknown)
    {
      if (initial)
        producer->Reset (context);

      BOOL is_initial = initial;
      initial = FALSE;

      XPath_ValueType type;

      context->states[state_index] = type = unknown->GetActualResultTypeL (context, is_initial);
    }
#endif // XPATH_EXTENSION_SUPPORT
}

unsigned
XPath_ConversionExpressionHelper::GetExpressionFlags ()
{
  XPath_Expression *expression = 0;

  if (numberexpression)
    expression = numberexpression;
  else if (booleanexpression)
    expression = booleanexpression;
  else if (stringexpression)
    expression = stringexpression;
#ifdef XPATH_EXTENSION_SUPPORT
  else if (unknown)
    expression = unknown;
#endif // XPATH_EXTENSION_SUPPORT
  else if (producer)
    return producer->GetExpressionFlags ();

  if (expression)
    return expression->GetExpressionFlags ();
  else
    return 0;
}

double
XPath_ConversionExpressionHelper::GetNumberL (XPath_Context *context, BOOL initial)
{
  OP_ASSERT (!numberexpression);

  StartL (context, initial);

  TempBuffer buffer; ANCHOR (TempBuffer, buffer);
  if (GetStringValueL (context, initial, buffer))
    return XPath_Value::AsNumber (buffer.GetStorage ());
  else if (booleanexpression)
    return booleanexpression->EvaluateToBooleanL (context, initial) ? 1. : 0.;
  else if (stringexpression)
    return XPath_Value::AsNumber (stringexpression->EvaluateToStringL (context, initial, buffer));

#ifdef XPATH_EXTENSION_SUPPORT
  OP_ASSERT (unknown);
  XPath_Value *value = unknown->EvaluateL (context, initial);
  double number = value->AsNumberL ();
  XPath_Value::DecRef (context, value);
  return number;
#else // XPATH_EXTENSION_SUPPORT
  OP_ASSERT (FALSE);
  return op_nan (0);
#endif // XPATH_EXTENSION_SUPPORT
}

BOOL
XPath_ConversionExpressionHelper::GetBooleanL (XPath_Context *context, BOOL initial)
{
  OP_ASSERT (!booleanexpression);

  StartL (context, initial);

  XPath_Node *node;
  if (GetNodeL (context, initial, node))
    {
      BOOL result = node != 0;
      XPath_Node::DecRef (context, node);
      return result;
    }
  else if (numberexpression)
    {
      double number = numberexpression->EvaluateToNumberL (context, initial);
      return !op_isnan (number) && number != 0.;
    }
  else if (stringexpression)
    {
      TempBuffer buffer; ANCHOR (TempBuffer, buffer);
      return *stringexpression->EvaluateToStringL (context, initial, buffer) != 0;
    }

#ifdef XPATH_EXTENSION_SUPPORT
  OP_ASSERT (unknown);
  XPath_Value *value = unknown->EvaluateL (context, initial);
  BOOL boolean = value->AsBoolean ();
  XPath_Value::DecRef (context, value);
  return boolean;
#else // XPATH_EXTENSION_SUPPORT
  OP_ASSERT (FALSE);
  return FALSE;
#endif // XPATH_EXTENSION_SUPPORT
}

const uni_char *
XPath_ConversionExpressionHelper::GetStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer)
{
  OP_ASSERT (!stringexpression);

  StartL (context, initial);

  if (GetStringValueL (context, initial, buffer))
    return buffer.GetStorage () ? buffer.GetStorage () : UNI_L ("");
  else if (numberexpression)
    return XPath_Value::AsStringL (numberexpression->EvaluateToNumberL (context, initial), buffer);
  else if (booleanexpression)
    return XPath_Value::AsString (booleanexpression->EvaluateToBooleanL (context, initial));

#ifdef XPATH_EXTENSION_SUPPORT
  OP_ASSERT (unknown);
  XPath_Value *value = unknown->EvaluateL (context, initial);
  const uni_char *string = value->AsStringL (buffer);
  XPath_Value::DecRef (context, value);
  return string;
#else // XPATH_EXTENSION_SUPPORT
  OP_ASSERT (FALSE);
  return op_nan (0);
#endif // XPATH_EXTENSION_SUPPORT
}

class XPath_NumberConversionExpression
  : public XPath_NumberExpression
{
private:
  XPath_ConversionExpressionHelper helper;

public:
  XPath_NumberConversionExpression (XPath_Parser *parser)
    : XPath_NumberExpression (parser)
  {
  }

  void InitializeL (XPath_Parser *parser, XPath_Expression *expression)
  {
    helper.InitializeL (parser, expression, TRUE);
  }

  virtual unsigned GetExpressionFlags ()
  {
    return (helper.GetExpressionFlags () & MASK_INHERITED) | FLAG_NUMBER;
  }

  virtual double EvaluateToNumberL (XPath_Context *context, BOOL initial)
  {
    return helper.GetNumberL (context, initial);
  }
};

/* virtual */ unsigned
XPath_NumberExpression::GetResultType ()
{
  return XP_VALUE_NUMBER;
}

/* virtual */ unsigned
XPath_NumberExpression::GetExpressionFlags ()
{
  return FLAG_NUMBER;
}

/* static */ XPath_NumberExpression *
XPath_NumberExpression::MakeL (XPath_Parser *parser, XPath_Expression *expression)
{
  unsigned flags = expression ? expression->GetExpressionFlags () : 0;

  if ((flags & FLAG_NUMBER) != 0)
    return static_cast<XPath_NumberExpression *> (expression);
  else
    {
      XPath_NumberConversionExpression *conversion = OP_NEW (XPath_NumberConversionExpression, (parser));

      if (!conversion)
        {
          OP_DELETE (expression);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      OpStackAutoPtr<XPath_Expression> conversion_anchor (conversion);
      conversion->InitializeL (parser, expression);
      conversion_anchor.release ();

      return conversion;
    }
}

class XPath_BooleanConversionExpression
  : public XPath_BooleanExpression
{
private:
  XPath_ConversionExpressionHelper helper;

public:
  XPath_BooleanConversionExpression (XPath_Parser *parser)
    : XPath_BooleanExpression (parser)
  {
  }

  void InitializeL (XPath_Parser *parser, XPath_Expression *expression)
  {
    helper.InitializeL (parser, expression, FALSE);
  }

  virtual unsigned GetExpressionFlags ()
  {
    return (helper.GetExpressionFlags () & MASK_INHERITED) | FLAG_BOOLEAN;
  }

  virtual BOOL EvaluateToBooleanL (XPath_Context *context, BOOL initial)
  {
    return helper.GetBooleanL (context, initial);
  }
};

/* virtual */ unsigned
XPath_BooleanExpression::GetResultType ()
{
  return XP_VALUE_BOOLEAN;
}

/* virtual */ unsigned
XPath_BooleanExpression::GetExpressionFlags ()
{
  return FLAG_BOOLEAN;
}

/* static */ XPath_BooleanExpression *
XPath_BooleanExpression::MakeL (XPath_Parser *parser, XPath_Expression *expression)
{
  if ((expression->GetExpressionFlags () & FLAG_BOOLEAN) != 0)
    return static_cast<XPath_BooleanExpression *> (expression);
  else
    {
      XPath_BooleanConversionExpression *conversion = OP_NEW (XPath_BooleanConversionExpression, (parser));

      if (!conversion)
        {
          OP_DELETE (expression);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      OpStackAutoPtr<XPath_Expression> conversion_anchor (conversion);
      conversion->InitializeL (parser, expression);
      conversion_anchor.release ();

      return conversion;
    }
}

class XPath_StringConversionExpression
  : public XPath_StringExpression
{
private:
  XPath_ConversionExpressionHelper helper;

public:
  XPath_StringConversionExpression (XPath_Parser *parser)
    : XPath_StringExpression (parser)
  {
  }

  void InitializeL (XPath_Parser *parser, XPath_Expression *expression)
  {
    helper.InitializeL (parser, expression, TRUE);
  }

  virtual unsigned GetExpressionFlags ()
  {
    return (helper.GetExpressionFlags () & MASK_INHERITED) | FLAG_STRING;
  }

  virtual const uni_char *EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer)
  {
    return helper.GetStringL (context, initial, buffer);
  }
};

/* virtual */ unsigned
XPath_StringExpression::GetResultType ()
{
  return XP_VALUE_STRING;
}

/* virtual */ unsigned
XPath_StringExpression::GetExpressionFlags ()
{
  return FLAG_STRING;
}

/* static */ XPath_StringExpression *
XPath_StringExpression::MakeL (XPath_Parser *parser, XPath_Expression *expression)
{
  if (expression && (expression->GetExpressionFlags () & FLAG_STRING) != 0)
    return static_cast<XPath_StringExpression *> (expression);
  else
    {
      XPath_StringConversionExpression *conversion = OP_NEW (XPath_StringConversionExpression, (parser));

      if (!conversion)
        {
          OP_DELETE (expression);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      OpStackAutoPtr<XPath_Expression> conversion_anchor (conversion);
      conversion->InitializeL (parser, expression);
      conversion_anchor.release ();

      return conversion;
    }
}

#endif // XPATH_SUPPORT
