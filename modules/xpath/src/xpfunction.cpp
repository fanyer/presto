/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpfunction.h"
#include "modules/xpath/src/xpcontext.h"
#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xpnode.h"
#include "modules/xpath/src/xputils.h"
#include "modules/xpath/src/xpapiimpl.h"
#include "modules/xpath/src/xpliteralexpr.h"
#include "modules/xpath/src/xpparser.h"
#include "modules/xpath/src/xpunknown.h"

#include "modules/util/str.h"
#include "modules/xmlutils/xmlutils.h"

/* static */ XPath_Expression *
XPath_FunctionCallExpression::MakeL (XPath_Parser *parser, const XMLExpandedName &name, XPath_Expression **arguments, unsigned arguments_count)
{
  XPath_Expression *expression = 0;
  XPath_Producer *producer = 0;

  unsigned type;
  BOOL is_count, is_true, is_position;

#define XPATH_WRONG_NUMBER_OF_ARGUMENTS() XPATH_COMPILATION_ERROR_NAME ("wrong number of arguments to function ''", parser->GetCurrentLocation ())

  if ((type = XP_VALUE_STRING, name == XMLExpandedName (UNI_L ("string"))) ||
      (type = XP_VALUE_NUMBER, name == XMLExpandedName (UNI_L ("number"))))
    {
      XPath_Expression *argument = 0;

      if (arguments_count == 1)
        {
          if (arguments[0]->GetResultType () == type)
            expression = arguments[0];
          else
            argument = arguments[0];

          arguments[0] = 0;
        }
      else if (arguments_count > 1)
        XPATH_COMPILATION_ERROR_NAME ("too many arguments to function: ''", parser->GetCurrentLocation ());

      if (!expression)
        if (type == XP_VALUE_STRING)
          expression = XPath_StringExpression::MakeL (parser, argument);
        else
          expression = XPath_NumberExpression::MakeL (parser, argument);
    }
  else if (name == XMLExpandedName (UNI_L ("boolean")))
    if (arguments_count == 1)
      expression = XPath_BooleanExpression::MakeL (parser, arguments[0]);
    else
      XPATH_WRONG_NUMBER_OF_ARGUMENTS ();
  else if ((is_position = name == XMLExpandedName (UNI_L ("position"))) ||
           name == XMLExpandedName (UNI_L ("last")))
    if (arguments_count != 0)
      XPATH_WRONG_NUMBER_OF_ARGUMENTS ();
    else
      expression = OP_NEW_L (XPath_ContextFunctionCall, (parser, is_position ? XPath_ContextFunctionCall::TYPE_position : XPath_ContextFunctionCall::TYPE_last));
  else if (name == XMLExpandedName (UNI_L ("id")))
    producer = XPath_IdProducer::MakeL (parser, arguments, arguments_count);
  else if ((is_count = name == XMLExpandedName (UNI_L ("count"))) ||
           name == XMLExpandedName (UNI_L ("sum")))
    expression = XPath_CumulativeNodeSetFunctionCall::MakeL (parser, is_count ? XPath_CumulativeNodeSetFunctionCall::TYPE_count : XPath_CumulativeNodeSetFunctionCall::TYPE_sum, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("local-name")))
    expression = XPath_SingleNodeFunctionCall::MakeL (parser, XPath_SingleNodeFunctionCall::TYPE_localname, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("namespace-uri")))
    expression = XPath_SingleNodeFunctionCall::MakeL (parser, XPath_SingleNodeFunctionCall::TYPE_namespaceuri, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("name")))
    expression = XPath_SingleNodeFunctionCall::MakeL (parser, XPath_SingleNodeFunctionCall::TYPE_name, arguments, arguments_count);
  else if ((is_true = name == XMLExpandedName (UNI_L ("true"))) ||
           name == XMLExpandedName (UNI_L ("false")))
    if (arguments_count == 0)
      expression = XPath_LiteralExpression::MakeL (parser, is_true);
    else
      XPATH_WRONG_NUMBER_OF_ARGUMENTS ();
  else if (name == XMLExpandedName (UNI_L ("not")))
    expression = XPath_NotFunctionCall::MakeL (parser, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("lang")))
    if (arguments_count == 1)
      expression = XPath_LangFunctionCall::MakeL (parser, arguments);
    else
      XPATH_WRONG_NUMBER_OF_ARGUMENTS ();
  else if (name == XMLExpandedName (UNI_L ("floor")))
    expression = XPath_NumberFunctionCall::MakeL (parser, XPath_NumberFunctionCall::TYPE_floor, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("ceiling")))
    expression = XPath_NumberFunctionCall::MakeL (parser, XPath_NumberFunctionCall::TYPE_ceiling, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("round")))
    expression = XPath_NumberFunctionCall::MakeL (parser, XPath_NumberFunctionCall::TYPE_round, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("concat")))
    expression = XPath_ConcatFunctionCall::MakeL (parser, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("starts-with")))
    expression = XPath_StartsWithOrContainsFunctionCall::MakeL (parser, XPath_StartsWithOrContainsFunctionCall::TYPE_starts_with, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("contains")))
    expression = XPath_StartsWithOrContainsFunctionCall::MakeL (parser, XPath_StartsWithOrContainsFunctionCall::TYPE_contains, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("substring-before")))
    expression = XPath_SubstringBeforeOrAfterFunctionCall::MakeL (parser, XPath_SubstringBeforeOrAfterFunctionCall::TYPE_substring_before, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("substring-after")))
    expression = XPath_SubstringBeforeOrAfterFunctionCall::MakeL (parser, XPath_SubstringBeforeOrAfterFunctionCall::TYPE_substring_after, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("substring")))
    expression = XPath_SubstringFunctionCall::MakeL (parser, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("string-length")))
    expression = XPath_StringLengthFunctionCall::MakeL (parser, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("normalize-space")))
    expression = XPath_NormalizeSpaceFunctionCall::MakeL (parser, arguments, arguments_count);
  else if (name == XMLExpandedName (UNI_L ("translate")))
    expression = XPath_TranslateFunctionCall::MakeL (parser, arguments, arguments_count);

  if (producer)
    {
      expression = OP_NEW (XPath_ProducerExpression, (parser, producer));
      if (!expression)
        {
          OP_DELETE (producer);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }
    }

#ifdef XPATH_EXTENSION_SUPPORT
  if (!expression)
    if (XPathExtensions *extensions = parser->GetExtensions ())
      {
        XPathFunction *function;

        OP_STATUS status = extensions->GetFunction (function, name);

        if (OpStatus::IsSuccess (status))
          expression = XPath_Unknown::MakeL (parser, function, arguments, arguments_count);
    }
#endif // XPATH_EXTENSION_SUPPORT

  if (!expression)
    XPATH_COMPILATION_ERROR_NAME ("unknown function called: ''", parser->GetCurrentLocation ());

  return expression;
}

XPath_IdProducer::XPath_IdProducer (XPath_Parser *parser, XPath_Producer *producer, XPath_StringExpression *stringexpression)
  : producer (producer),
    stringexpression (stringexpression),
#ifdef XPATH_EXTENSION_SUPPORT
    unknown (0),
#endif // XPATH_EXTENSION_SUPPORT
    state_index (parser->GetStateIndex ()),
    offset_index (parser->GetStateIndex ()),
    value_index (parser->GetValueIndex ()),
    node_index (parser->GetNodeIndex ()),
    localci_index (parser->GetContextInformationIndex ())
{
  ci_index = localci_index;
}

#ifdef XPATH_EXTENSION_SUPPORT

XPath_IdProducer::XPath_IdProducer (XPath_Parser *parser, XPath_Unknown *unknown)
  : producer (0),
    stringexpression (0),
#ifdef XPATH_EXTENSION_SUPPORT
    unknown (unknown),
    unknownstate_index (parser->GetStateIndex ()),
#endif // XPATH_EXTENSION_SUPPORT
    state_index (parser->GetStateIndex ()),
    offset_index (parser->GetStateIndex ()),
    value_index (parser->GetValueIndex ()),
    node_index (parser->GetNodeIndex ())
{
}

#endif // XPATH_EXTENSION_SUPPORT

/* static */ XPath_Producer *
XPath_IdProducer::MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count)
{
  if (arguments_count != 1)
    XPATH_WRONG_NUMBER_OF_ARGUMENTS ();

#ifdef XPATH_EXTENSION_SUPPORT
  XPath_Unknown *unknown;
  if (arguments[0]->HasFlag (XPath_Expression::FLAG_UNKNOWN))
    {
      unknown = static_cast<XPath_Unknown *> (arguments[0]);

      XPath_Producer *idproducer = OP_NEW_L (XPath_IdProducer, (parser, unknown));

      arguments[0] = 0;

      return idproducer;
    }
  else
    {
#endif // XPATH_EXTENSION_SUPPORT
      XPath_Producer *producer;
      XPath_StringExpression *stringexpression;

      producer = XPath_Expression::GetProducerL (parser, arguments[0]);

      if (producer)
        stringexpression = 0;
      else
        stringexpression = XPath_StringExpression::MakeL (parser, arguments[0]);

      arguments[0] = 0;

      XPath_Producer *idproducer = OP_NEW (XPath_IdProducer, (parser, producer, stringexpression));

      if (!idproducer)
        {
          OP_DELETE (idproducer);
          OP_DELETE (stringexpression);

          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      return idproducer;
    }
}

/* virtual */
XPath_IdProducer::~XPath_IdProducer ()
{
  OP_DELETE (producer);
  OP_DELETE (stringexpression);
#ifdef XPATH_EXTENSION_SUPPORT
  OP_DELETE (unknown);
#endif // XPATH_EXTENSION_SUPPORT
}

/* virtual */ unsigned
XPath_IdProducer::GetProducerFlags ()
{
  if (producer)
    return producer->GetProducerFlags () & (MASK_INHERITED & ~MASK_ORDER);
#ifdef XPATH_EXTENSION_SUPPORT
  else if (unknown)
    return unknown->GetProducerFlags () & (MASK_INHERITED & ~MASK_ORDER);
#endif // XPATH_EXTENSION_SUPPORT
  else
    {
      unsigned expression_flags = stringexpression->GetExpressionFlags (), flags = 0;

      if ((expression_flags & XPath_Expression::FLAG_CONTEXT_SIZE) != 0)
        flags = FLAG_CONTEXT_SIZE;
      if ((expression_flags & XPath_Expression::FLAG_CONTEXT_POSITION) != 0)
        flags = FLAG_CONTEXT_POSITION;

      return flags;
    }
}

/* virtual */ BOOL
XPath_IdProducer::Reset (XPath_Context *context, BOOL local_context_only)
{
  if (producer)
    producer->Reset (context, local_context_only);

  context->states[state_index] = 0;

  XPath_Value::DecRef (context, context->values[value_index]);
  context->values[value_index] = 0;

#ifdef XPATH_EXTENSION_SUPPORT
  if (unknown)
    {
      unknown->Reset (context);
      context->states[unknownstate_index] = 0;
    }
#endif // XPATH_EXTENSION_SUPPORT

  return FALSE;
}

/* virtual */ XPath_Node *
XPath_IdProducer::GetNextNodeL (XPath_Context *context)
{
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);

  unsigned &state = context->states[state_index];
  unsigned &offset = context->states[offset_index];
  XPath_Value *&value = context->values[value_index];
  XPath_Node &node = context->nodes[node_index];
  XPath_Producer *use_producer = producer;

  const uni_char *base, *string;
  BOOL initial;

#ifdef XPATH_EXTENSION_SUPPORT
  if (unknown)
    {
      XPath_ValueType unknown_type;

      if (context->states[unknownstate_index] < 2)
        {
          BOOL unknown_initial = context->states[unknownstate_index] == 0;

          context->states[unknownstate_index] = 1;

          unknown_type = unknown->GetActualResultTypeL (context, unknown_initial);

          context->states[unknownstate_index] = 2 | (unknown_type << 2);
        }
      else
        unknown_type = static_cast<XPath_ValueType> ((context->states[unknownstate_index] & ~3) >> 2);

      if (!XPath_IsPrimitive (unknown_type))
        use_producer = unknown;
    }
#endif // XPATH_EXTENSION_SUPPORT

  if (state < 2)
    {
      offset = 0;
      base = string = 0;
      initial = state == 0;
    }
  else
    {
      base = value->data.string;
      string = base + offset;
      initial = FALSE;
    }

  while (TRUE)
    {
      if (!string)
        {
          buffer.Clear ();

          if (use_producer)
            if (XPath_Node *node = use_producer->GetNextNodeL (context))
              {
                node->GetStringValueL (buffer);
                XPath_Node::DecRef (context, node);
                base = string = buffer.GetStorage () ? buffer.GetStorage () : UNI_L("");
              }
            else
              return 0;
#ifdef XPATH_EXTENSION_SUPPORT
          else if (unknown)
            {
              XPath_ValueType conversion[4] = { XP_VALUE_STRING, XP_VALUE_STRING, XP_VALUE_STRING, XP_VALUE_INVALID };
              XPath_Value *value = unknown->EvaluateL (context, initial, conversion);
              buffer.AppendL (value->data.string);
              base = string = buffer.GetStorage ();
              XPath_Value::DecRef (context, value);
            }
#endif // XPATH_EXTENSION_SUPPORT
          else
            {
              state = 1;
              base = string = stringexpression->EvaluateToStringL (context, initial, buffer);
            }

          XPath_Value::DecRef (context, value);
          value = 0;

          state = 2;
          value = XPath_Value::MakeStringL (context, base);

          XP_SEQUENCE_POINT (context);
        }

      while (TRUE)
        {
          while (*string && XMLUtils::IsSpace (*string)) ++string;

          if (!*string)
            break;

          const uni_char *id = string;

          while (*string && !XMLUtils::IsSpace (*string)) ++string;

          if (XMLTreeAccessor::Node *treenode = context->node->GetTreeNodeByIdL (id, string - id))
            {
              offset = string - base;

              node.Set (context->node->tree, treenode);
              return XPath_Node::IncRef (&node);
            }

          if (!*string)
            break;
        }

      if (!use_producer)
        return 0;

      string = 0;
      state = 1;

      XP_SEQUENCE_POINT (context);
    }
}

XPath_CumulativeNodeSetFunctionCall::XPath_CumulativeNodeSetFunctionCall (XPath_Parser *parser, Type type, XPath_Producer *producer)
  : XPath_NumberExpression (parser),
    type (type),
    producer (producer)
{
  if (type == TYPE_count)
    count_index = parser->GetStateIndex ();
  else
    sum_index = parser->GetNumberIndex ();
}

XPath_CumulativeNodeSetFunctionCall::~XPath_CumulativeNodeSetFunctionCall ()
{
  OP_DELETE (producer);
}

/* static */ XPath_Expression *
XPath_CumulativeNodeSetFunctionCall::MakeL (XPath_Parser *parser, Type type, XPath_Expression **arguments, unsigned arguments_count)
{
  if (arguments_count != 1)
    XPATH_WRONG_NUMBER_OF_ARGUMENTS ();

  XPath_Producer *producer = XPath_Expression::GetProducerL (parser, arguments[0]);

  if (!producer)
    XPATH_COMPILATION_ERROR ("expected node-set expression", arguments[0]->location);

  arguments[0] = 0;

  producer = XPath_Producer::EnsureFlagsL (parser, producer, XPath_Producer::FLAG_NO_DUPLICATES);

  XPath_NumberExpression *expression = OP_NEW (XPath_CumulativeNodeSetFunctionCall, (parser, type, producer));

  if (!expression)
    {
      OP_DELETE (producer);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  return expression;
}

/* virtual */ double
XPath_CumulativeNodeSetFunctionCall::EvaluateToNumberL (XPath_Context *context, BOOL initial)
{
  if (initial)
    producer->Reset (context);

  if (type == TYPE_count)
    {
      unsigned &count = context->states[count_index];

      if (initial)
        count = 0;

      while (XPath_Node *node = producer->GetNextNodeL (context))
        {
          ++count;
          XPath_Node::DecRef (context, node);
        }

      return (double) count;
    }
  else
    {
      double &sum = context->numbers[sum_index];

      if (initial)
        sum = 0;

      TempBuffer *buffer = context->GetTempBuffer ();

      while (XPath_Node *node = producer->GetNextNodeL (context))
        {
          buffer->Clear ();
          node->GetStringValueL (*buffer);
          sum += XPath_Value::AsNumber (buffer->GetStorage ());

          XPath_Node::DecRef (context, node);
        }

      return sum;
    }
}

XPath_SingleNodeFunctionCall::XPath_SingleNodeFunctionCall (XPath_Parser *parser, Type type, XPath_Producer *argument)
  : XPath_StringExpression (parser),
    type (type),
    argument (argument)
{
}

/* static */ XPath_Expression *
XPath_SingleNodeFunctionCall::MakeL (XPath_Parser *parser, Type type, XPath_Expression **arguments, unsigned arguments_count)
{
  if (arguments_count != 0 && arguments_count != 1)
    XPATH_WRONG_NUMBER_OF_ARGUMENTS ();

  if (arguments_count == 1 && (arguments[0]->GetExpressionFlags () & XPath_Expression::FLAG_PRODUCER) == 0)
    XPATH_COMPILATION_ERROR ("expected node-set expression", arguments[0]->location);

  if (arguments_count == 1)
    {
      XPath_Producer *producer = XPath_Expression::GetProducerL (parser, arguments[0]);

      arguments[0] = 0;

      producer = XPath_Producer::EnsureFlagsL (parser, producer, XPath_Producer::FLAG_DOCUMENT_ORDER | XPath_Producer::FLAG_SINGLE_NODE);
      XPath_Expression *expression = OP_NEW (XPath_SingleNodeFunctionCall, (parser, type, producer));

      if (!expression)
        {
          OP_DELETE (producer);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      return expression;
    }
  else
    return OP_NEW_L (XPath_SingleNodeFunctionCall, (parser, type, 0));
}

XPath_SingleNodeFunctionCall::~XPath_SingleNodeFunctionCall ()
{
  OP_DELETE (argument);
}

/* virtual */ unsigned
XPath_SingleNodeFunctionCall::GetExpressionFlags ()
{
  return ((argument ? argument->GetExpressionFlags () : 0) & MASK_INHERITED) | FLAG_STRING;
}

/* virtual */ const uni_char *
XPath_SingleNodeFunctionCall::EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer)
{
  XPath_Node *node;

  if (argument)
    {
      if (initial)
        argument->Reset (context);

      node = argument->GetNextNodeL (context);
    }
  else
    node = XPath_Node::IncRef (context->node);

  if (node)
    {
      const uni_char *string;

      if (type == TYPE_name && (node->type == XP_NODE_ELEMENT || node->type == XP_NODE_ATTRIBUTE))
        {
          node->GetQualifiedNameL (buffer);
          string = buffer.GetStorage ();
        }
      else
        {
          XMLExpandedName name;

          node->GetExpandedName (name);

          if (type == TYPE_localname || type == TYPE_name)
            string = name.GetLocalPart ();
          else
            string = name.GetUri ();
        }

      XPath_Node::DecRef (context, node);

      if (string)
        return string;
    }

  return UNI_L ("");
}

/* static */ XPath_BooleanExpression *
XPath_LangFunctionCall::MakeL (XPath_Parser *parser, XPath_Expression **arguments)
{
  XPath_Expression *argument = arguments[0];

  arguments[0] = 0;

  XPath_StringExpression *stringargument = XPath_StringExpression::MakeL (parser, argument);

  OpStackAutoPtr<XPath_Expression> stringargument_anchor (stringargument);

  XPath_LangFunctionCall *lang = OP_NEW_L (XPath_LangFunctionCall, (parser, stringargument));

  stringargument_anchor.release ();
  return lang;
}

/* virtual */
XPath_LangFunctionCall::~XPath_LangFunctionCall ()
{
  OP_DELETE (argument);
}

/* virtual */ unsigned
XPath_LangFunctionCall::GetExpressionFlags ()
{
  return (argument->GetExpressionFlags () & MASK_INHERITED) | FLAG_BOOLEAN;
}

/* virtual */ BOOL
XPath_LangFunctionCall::EvaluateToBooleanL (XPath_Context *context, BOOL initial)
{
  if (context->node->type == XP_NODE_ROOT)
    return FALSE;
  else
    {
      TempBuffer argument_buffer; ANCHOR (TempBuffer, argument_buffer);

      const uni_char *argument_string = argument->EvaluateToStringL (context, initial, argument_buffer);

      XMLTreeAccessor *tree = context->node->tree;
      XMLTreeAccessor::Node *treenode = context->node->treenode;

      XMLExpandedName lang_name (UNI_L ("http://www.w3.org/XML/1998/namespace"), UNI_L ("lang"));

      LEAVE_IF_ERROR (tree->SetAttributeNameFilter (lang_name));

      if (!tree->FilterNode (treenode))
        treenode = tree->GetParent (treenode);

      tree->ResetFilters ();

      if (treenode)
        {
          const uni_char *lang_string;
          BOOL id, specified;
          TempBuffer lang_buffer; ANCHOR (TempBuffer, lang_buffer);

          LEAVE_IF_ERROR (tree->GetAttribute (tree->GetAttributes (treenode, FALSE, TRUE), lang_name, lang_string, id, specified, &lang_buffer));

          unsigned argument_length = uni_strlen (argument_string);

          return uni_strni_eq (argument_string, lang_string, argument_length) && (!lang_string[argument_length] || lang_string[argument_length] == '-');
        }
      else
        return FALSE;
    }
}

XPath_NumberFunctionCall::XPath_NumberFunctionCall (XPath_Parser *parser, Type type, XPath_NumberExpression *argument)
  : XPath_NumberExpression (parser),
    type (type),
    argument (argument)
{
}

/* static */ XPath_NumberExpression *
XPath_NumberFunctionCall::MakeL (XPath_Parser *parser, Type type, XPath_Expression **arguments, unsigned arguments_count)
{
  if (arguments_count != 1)
    XPATH_WRONG_NUMBER_OF_ARGUMENTS ();

  XPath_Expression *argument0 = arguments[0];
  arguments[0] = 0;
  XPath_NumberExpression *numberargument = XPath_NumberExpression::MakeL (parser, argument0);

  XPath_NumberFunctionCall *call = OP_NEW (XPath_NumberFunctionCall, (parser, type, numberargument));
  if (!call)
    {
      OP_DELETE (numberargument);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  return call;
}

/* virtual */
XPath_NumberFunctionCall::~XPath_NumberFunctionCall ()
{
  OP_DELETE (argument);
}

/* virtual */ unsigned
XPath_NumberFunctionCall::GetExpressionFlags ()
{
  return (argument->GetExpressionFlags () & MASK_INHERITED) | FLAG_NUMBER;
}

/* virtual */ double
XPath_NumberFunctionCall::EvaluateToNumberL (XPath_Context *context, BOOL initial)
{
  double argument_number = argument->EvaluateToNumberL (context, initial);

  if (type == TYPE_floor)
    return op_floor (argument_number);
  else if (type == TYPE_ceiling)
    return op_ceil (argument_number);
  else
    return XPath_Utils::Round (argument_number);
}

XPath_ContextFunctionCall::XPath_ContextFunctionCall (XPath_Parser *parser, Type type)
  : XPath_NumberExpression (parser),
    type (type)
{
}

/* virtual */ unsigned
XPath_ContextFunctionCall::GetExpressionFlags ()
{
  return (type == TYPE_position ? FLAG_CONTEXT_POSITION : FLAG_CONTEXT_SIZE) | FLAG_FUNCTIONCALL | FLAG_NUMBER;
}

/* virtual */ BOOL
XPath_ContextFunctionCall::TransformL (XPath_Parser *parser, Transform transform, TransformData &data)
{
  if (transform == TRANSFORM_XMLEXPANDEDNAME)
    {
      *data.name = type == TYPE_position ? XMLExpandedName (UNI_L ("count")) : XMLExpandedName (UNI_L ("last"));
      return TRUE;
    }

  return FALSE;
}

/* virtual */ double
XPath_ContextFunctionCall::EvaluateToNumberL (XPath_Context *context, BOOL initial)
{
  if (type == TYPE_position)
    return static_cast<double> (context->position);
  else
    return static_cast<double> (context->size);
}

XPath_ConcatFunctionCall::XPath_ConcatFunctionCall (XPath_Parser *parser)
  : XPath_StringExpression (parser),
    arguments (0),
    arguments_count (0),
    state_index (parser->GetStateIndex ()),
    buffer_index (parser->GetBufferIndex ())
{
}

/* static */ XPath_StringExpression *
XPath_ConcatFunctionCall::MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count)
{
  if (arguments_count < 2)
    XPATH_WRONG_NUMBER_OF_ARGUMENTS ();

  XPath_ConcatFunctionCall *call = OP_NEW_L (XPath_ConcatFunctionCall, (parser));
  OpStackAutoPtr<XPath_Expression> call_anchor (call);

  call->arguments = OP_NEWA_L (XPath_StringExpression *, arguments_count);

  for (unsigned index = 0; index < arguments_count; ++index)
    {
      XPath_Expression *argument = arguments[index];
      arguments[index] = 0;
      call->arguments[index] = XPath_StringExpression::MakeL (parser, argument);

      ++call->arguments_count;
    }

  call_anchor.release ();
  return call;
}

/* virtual */
XPath_ConcatFunctionCall::~XPath_ConcatFunctionCall ()
{
  for (unsigned index = 0; index < arguments_count; ++index)
    OP_DELETE (arguments[index]);
  OP_DELETEA (arguments);
}

/* virtual */ unsigned
XPath_ConcatFunctionCall::GetExpressionFlags ()
{
  unsigned flags = FLAG_STRING;

  for (unsigned index = 0; index < arguments_count; ++index)
    flags |= arguments[index]->GetExpressionFlags () & MASK_INHERITED;

  return flags;
}

/* virtual */ const uni_char *
XPath_ConcatFunctionCall::EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &)
{
  unsigned &state = context->states[state_index];
  TempBuffer &buffer = context->buffers[buffer_index];

  /* States: state & 1 == 0: evaluate argument (initial)
             state & 1 == 1: evaluate argument (continued)

             state / 2: current argument index */

  if (initial)
    {
      state = 0;
      buffer.Clear ();
    }

  unsigned argument_index = state / 2;
  BOOL argument_initial = (state & 1) == 0;
  TempBuffer local; ANCHOR (TempBuffer, local);

  while (argument_index < arguments_count)
    {
      state |= 1;

      const uni_char *argument = arguments[argument_index]->EvaluateToStringL (context, argument_initial, local);

      ++state;
      ++argument_index;

      argument_initial = TRUE;

      buffer.AppendL (argument);
      local.Clear ();
    }

  const uni_char *storage = buffer.GetStorage ();

  if (!storage)
    storage = UNI_L ("");

  return storage;
}

XPath_SubstringFunctionCall::XPath_SubstringFunctionCall (XPath_Parser *parser, XPath_StringExpression *base, XPath_NumberExpression *offset, XPath_NumberExpression *length)
  : XPath_StringExpression (parser),
    base (base),
    offset (offset),
    length (length),
    state_index (parser->GetStateIndex ()),
    offset_index (parser->GetNumberIndex ()),
    length_index (parser->GetNumberIndex ())
{
}

/* static */ XPath_StringExpression *
XPath_SubstringFunctionCall::MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count)
{
  if (arguments_count != 2 && arguments_count != 3)
    XPATH_WRONG_NUMBER_OF_ARGUMENTS ();

  XPath_Expression *argument0 = arguments[0];
  arguments[0] = 0;
  XPath_StringExpression *base = XPath_StringExpression::MakeL (parser, argument0);
  OpStackAutoPtr<XPath_Expression> base_anchor (base);

  XPath_Expression *argument1 = arguments[1];
  arguments[1] = 0;
  XPath_NumberExpression *offset = XPath_NumberExpression::MakeL (parser, argument1);
  OpStackAutoPtr<XPath_Expression> offset_anchor (offset);

  XPath_NumberExpression *length;
  if (arguments_count == 3)
    {
      XPath_Expression *argument2 = arguments[2];
      arguments[2] = 0;
      length = XPath_NumberExpression::MakeL (parser, argument2);
    }
  else
    length = 0;
  OpStackAutoPtr<XPath_Expression> length_anchor (length);

  XPath_SubstringFunctionCall *call = OP_NEW_L (XPath_SubstringFunctionCall, (parser, base, offset, length));

  base_anchor.release ();
  offset_anchor.release ();
  length_anchor.release ();
  return call;
}

/* virtual */
XPath_SubstringFunctionCall::~XPath_SubstringFunctionCall ()
{
  OP_DELETE (base);
  OP_DELETE (offset);
  OP_DELETE (length);
}

/* virtual */ unsigned
XPath_SubstringFunctionCall::GetExpressionFlags ()
{
  unsigned flags = base->GetExpressionFlags () | offset->GetExpressionFlags ();

  if (length)
    flags |= length->GetExpressionFlags ();

  return (flags & MASK_INHERITED) | FLAG_STRING;
}

/* virtual */ const uni_char *
XPath_SubstringFunctionCall::EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer)
{
  unsigned &state = context->states[state_index];
  double &offset_value = context->numbers[offset_index];
  double &length_value = context->numbers[length_index];

  if (initial)
    state = 0;

  if (state < 2)
    {
      BOOL offset_initial = state == 0;
      state = 1;
      offset_value = offset->EvaluateToNumberL (context, offset_initial);
      state = 2;

      if (op_isnan (offset_value) || !op_isfinite (offset_value) && op_signbit (offset_value) == 0)
        return UNI_L ("");
    }

  if (state < 4)
    {
      if (length)
        {
          BOOL length_initial = state == 2;
          state = 3;
          length_value = length->EvaluateToNumberL (context, length_initial);

          if (op_isnan (length_value))
            return UNI_L ("");

          double dend = XPath_Utils::Round (offset_value) + XPath_Utils::Round (length_value);
          if (op_isnan (dend) || !(1 < dend))
            return UNI_L ("");
        }
      state = 4;
    }

  BOOL base_initial = state == 4;
  state = 5;
  const uni_char *base_value = base->EvaluateToStringL (context, base_initial, buffer);

  if (!*base_value)
    return base_value;
  else if (base_value != buffer.GetStorage ())
    buffer.AppendL (base_value);

  double roffset;
  unsigned start = 0, end = 0, string_length = buffer.Length ();

  OP_ASSERT (!op_isnan (offset_value));
  if (!op_isfinite (offset_value))
    {
      OP_ASSERT (op_signbit (offset_value) == 1);
      start = 0;
      roffset = 0;
    }
  else
    {
      roffset = XPath_Utils::Round (offset_value);

      if (roffset > (double) string_length)
        return UNI_L ("");

      if (roffset < 1.)
        start = 0;
      else
        start = (unsigned) roffset - 1;
    }

  if (!length)
    end = string_length;
  else if (!op_isfinite (length_value))
    end = string_length;
  else
    {
      double rlength = XPath_Utils::Round (length_value), dend = roffset + rlength - 1.;

      if (dend > (double) string_length)
        end = string_length;
      else if (dend < (double) start)
        end = start;
      else
        end = (unsigned) dend;
    }

  if (end - start == 0)
    return UNI_L ("");
  else
    {
      uni_char *result = buffer.GetStorage ();
      if (start > 0)
        op_memmove (result, result + start, (end - start) * sizeof *result);
      result[end - start] = 0;
      return result;
    }
}

XPath_TranslateFunctionCall::XPath_TranslateFunctionCall (XPath_Parser *parser, XPath_StringExpression *base, XPath_StringExpression *from, XPath_StringExpression *to)
  : XPath_StringExpression (parser),
    base (base),
    from (from),
    to (to),
    state_index (parser->GetStateIndex ()),
    from_buffer_index (parser->GetBufferIndex ()),
    to_buffer_index (parser->GetBufferIndex ())
{
}

/* static */ XPath_StringExpression *
XPath_TranslateFunctionCall::MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count)
{
  if (arguments_count != 3)
    XPATH_WRONG_NUMBER_OF_ARGUMENTS ();

  XPath_Expression *argument0 = arguments[0];
  arguments[0] = 0;
  XPath_StringExpression *base = XPath_StringExpression::MakeL (parser, argument0);
  OpStackAutoPtr<XPath_Expression> base_anchor (base);

  XPath_Expression *argument1 = arguments[1];
  arguments[1] = 0;
  XPath_StringExpression *from = XPath_StringExpression::MakeL (parser, argument1);
  OpStackAutoPtr<XPath_Expression> from_anchor (from);

  XPath_Expression *argument2 = arguments[2];
  arguments[2] = 0;
  XPath_StringExpression *to = XPath_StringExpression::MakeL (parser, argument2);
  OpStackAutoPtr<XPath_Expression> to_anchor (to);

  XPath_TranslateFunctionCall *call = OP_NEW_L (XPath_TranslateFunctionCall, (parser, base, from, to));

  base_anchor.release ();
  from_anchor.release ();
  to_anchor.release ();
  return call;
}

/* virtual */
XPath_TranslateFunctionCall::~XPath_TranslateFunctionCall ()
{
  OP_DELETE (base);
  OP_DELETE (from);
  OP_DELETE (to);
}

/* virtual */ unsigned
XPath_TranslateFunctionCall::GetExpressionFlags ()
{
  return ((base->GetExpressionFlags () | from->GetExpressionFlags () | to->GetExpressionFlags ()) & MASK_INHERITED) | FLAG_STRING;
}

/* virtual */ const uni_char *
XPath_TranslateFunctionCall::EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer)
{
  unsigned &state = context->states[state_index];
  TempBuffer &from_buffer = context->buffers[from_buffer_index];
  TempBuffer &to_buffer = context->buffers[to_buffer_index];

  if (initial)
    {
      state = 0;
      from_buffer.Clear ();
      to_buffer.Clear ();
    }

  if (state < 2)
    {
      BOOL from_initial = state == 0;
      state = 1;
      const uni_char *from_value = from->EvaluateToStringL (context, from_initial, from_buffer);
      state = 2;

      if (from_value != from_buffer.GetStorage ())
        from_buffer.AppendL (from_value);
    }

  if (state < 4)
    {
      BOOL to_initial = state == 2;
      state = 3;
      const uni_char *to_value = to->EvaluateToStringL (context, to_initial, to_buffer);
      state = 4;

      if (to_value != to_buffer.GetStorage ())
        to_buffer.AppendL (to_value);
    }

  BOOL base_initial = state == 4;
  state = 5;
  const uni_char *base_value = base->EvaluateToStringL (context, base_initial, buffer);
  const uni_char *from_value = from_buffer.GetStorage ();

  if (!from_value)
    return base_value;
  if (base_value != buffer.GetStorage ())
    buffer.AppendL (base_value);

  uni_char *writep = buffer.GetStorage ();
  const uni_char *readp = writep, *to_value = to_buffer.GetStorage ();

  unsigned to_value_length;
  if (!to_value)
    to_value_length = 0;
  else
    to_value_length = uni_strlen (to_value);

  while (*readp)
    {
      const uni_char *location = uni_strchr (from_value, *readp);
      if (location)
        {
          unsigned index = location - from_value;
          if (index < to_value_length)
            *writep++ = to_value[index];
        }
      else
        *writep++ = *readp;
      ++readp;
    }

  *writep = 0;
  return buffer.GetStorage ();
}

XPath_NormalizeSpaceFunctionCall::XPath_NormalizeSpaceFunctionCall (XPath_Parser *parser, XPath_StringExpression *base)
  : XPath_StringExpression (parser),
    base (base)
{
}

/* static */ XPath_StringExpression *
XPath_NormalizeSpaceFunctionCall::MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count)
{
  if (arguments_count > 1)
    XPATH_WRONG_NUMBER_OF_ARGUMENTS ();

  XPath_Expression *argument0;

  if (arguments_count == 1)
    {
      argument0 = arguments[0];
      arguments[0] = 0;
    }
  else
    argument0 = 0;

  XPath_StringExpression *base = XPath_StringExpression::MakeL (parser, argument0);

  XPath_NormalizeSpaceFunctionCall *call = OP_NEW (XPath_NormalizeSpaceFunctionCall, (parser, base));
  if (!call)
    {
      OP_DELETE (base);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }
  return call;
}

/* virtual */
XPath_NormalizeSpaceFunctionCall::~XPath_NormalizeSpaceFunctionCall ()
{
  OP_DELETE (base);
}

/* virtual */ unsigned
XPath_NormalizeSpaceFunctionCall::GetExpressionFlags ()
{
  return (base->GetExpressionFlags () & MASK_INHERITED) | FLAG_STRING;
}

/* virtual */ const uni_char *
XPath_NormalizeSpaceFunctionCall::EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer)
{
  const uni_char *value = base->EvaluateToStringL (context, initial, buffer);

  if (!value || !*value)
    return UNI_L ("");

  if (value != buffer.GetStorage ())
    buffer.AppendL (value);

  uni_char *result = buffer.GetStorage (), *start = result;

  while (XMLUtils::IsSpace (*start))
    ++start;

  const uni_char *readp = start;
  uni_char *writep = start;

  while (*readp)
    {
      if (!XMLUtils::IsSpace (*readp))
        *writep++ = *readp++;
      else
        {
          *writep++ = ' ';
          while (XMLUtils::IsSpace (*readp))
            ++readp;
          if (!*readp)
            {
              --writep;
              break;
            }
          else
            continue;
        }
    }

  if (result != start)
    op_memmove (result, start, (writep - start) * sizeof *result);

  result[writep - start] = 0;
  return result;
}

XPath_StringLengthFunctionCall::XPath_StringLengthFunctionCall (XPath_Parser *parser, XPath_StringExpression *base)
  : XPath_NumberExpression (parser),
    base (base)
{
}

/* static */ XPath_NumberExpression *
XPath_StringLengthFunctionCall::MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count)
{
  if (arguments_count > 1)
    XPATH_WRONG_NUMBER_OF_ARGUMENTS ();

  XPath_Expression *argument0;

  if (arguments_count == 1)
    {
      argument0 = arguments[0];
      arguments[0] = 0;
    }
  else
    argument0 = 0;

  XPath_StringExpression *base = XPath_StringExpression::MakeL (parser, argument0);

  XPath_StringLengthFunctionCall *call = OP_NEW (XPath_StringLengthFunctionCall, (parser, base));
  if (!call)
    {
      OP_DELETE (base);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }
  return call;
}

/* virtual */
XPath_StringLengthFunctionCall::~XPath_StringLengthFunctionCall ()
{
  OP_DELETE (base);
}

/* virtual */ unsigned
XPath_StringLengthFunctionCall::GetExpressionFlags ()
{
  return (base->GetExpressionFlags () & MASK_INHERITED) | FLAG_NUMBER;
}

/* virtual */ double
XPath_StringLengthFunctionCall::EvaluateToNumberL (XPath_Context *context, BOOL initial)
{
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);

  const uni_char *value = base->EvaluateToStringL (context, initial, buffer);

  return (double) uni_strlen (value);
}

XPath_BinaryStringFunctionCallBase::XPath_BinaryStringFunctionCallBase (XPath_Parser *parser)
  : argument1 (0),
    argument2 (0),
    state_index (parser->GetStateIndex ()),
    buffer1_index (parser->GetBufferIndex ()),
    buffer2_index (parser->GetBufferIndex ())
{
}

XPath_BinaryStringFunctionCallBase::~XPath_BinaryStringFunctionCallBase ()
{
  OP_DELETE (argument1);
  OP_DELETE (argument2);
}

void
XPath_BinaryStringFunctionCallBase::SetArgumentsL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count)
{
  if (arguments_count != 2)
    XPATH_WRONG_NUMBER_OF_ARGUMENTS ();

  XPath_Expression *argument1in = arguments[0];
  arguments[0] = 0;
  argument1 = XPath_StringExpression::MakeL (parser, argument1in);

  XPath_Expression *argument2in = arguments[1];
  arguments[1] = 0;
  argument2 = XPath_StringExpression::MakeL (parser, argument2in);
}

void
XPath_BinaryStringFunctionCallBase::EvaluateArgumentsL (XPath_Context *context, BOOL initial, const uni_char *&value1, const uni_char *&value2)
{
  unsigned &state = context->states[state_index];
  TempBuffer &buffer1 = context->buffers[buffer1_index];
  TempBuffer &buffer2 = context->buffers[buffer2_index];

  if (initial)
    {
      state = 0;
      buffer1.Clear ();
      buffer2.Clear ();
    }

  if (state < 2)
    {
      BOOL initial1 = state == 0;
      state = 1;
      value1 = argument1->EvaluateToStringL (context, initial1, buffer1);
      if (*value1 && value1 != buffer1.GetStorage ())
        buffer1.AppendL (value1);
      state = 2;

      XP_SEQUENCE_POINT (context);
    }
  value1 = buffer1.GetStorage ();

  if (state < 4)
    {
      BOOL initial2 = state == 2;
      state = 3;
      value2 = argument2->EvaluateToStringL (context, initial2, buffer2);
      if (*value2 && value2 != buffer2.GetStorage ())
        buffer2.AppendL (value2);
      state = 4;

      XP_SEQUENCE_POINT (context);
    }
  value2 = buffer2.GetStorage ();

  if (!value1)
    value1 = UNI_L ("");
  if (!value2)
    value2 = UNI_L ("");
}

unsigned
XPath_BinaryStringFunctionCallBase::GetExpressionFlags ()
{
  return (argument1->GetExpressionFlags () | argument2->GetExpressionFlags ()) & XPath_Expression::MASK_INHERITED;
}

XPath_StartsWithOrContainsFunctionCall::XPath_StartsWithOrContainsFunctionCall (XPath_Parser *parser, Type type)
  : XPath_BooleanExpression (parser),
    XPath_BinaryStringFunctionCallBase (parser),
    type (type)
{
}

/* static */ XPath_BooleanExpression *
XPath_StartsWithOrContainsFunctionCall::MakeL (XPath_Parser *parser, Type type, XPath_Expression **arguments, unsigned arguments_count)
{
  XPath_StartsWithOrContainsFunctionCall *call = OP_NEW_L (XPath_StartsWithOrContainsFunctionCall, (parser, type));
  OpStackAutoPtr<XPath_Expression> call_anchor (call);

  call->SetArgumentsL (parser, arguments, arguments_count);

  call_anchor.release ();
  return call;
}

/* virtual */ unsigned
XPath_StartsWithOrContainsFunctionCall::GetExpressionFlags ()
{
  return XPath_BinaryStringFunctionCallBase::GetExpressionFlags () | FLAG_BOOLEAN;
}

/* virtual */ BOOL
XPath_StartsWithOrContainsFunctionCall::EvaluateToBooleanL (XPath_Context *context, BOOL initial)
{
  const uni_char *value1, *value2;

  EvaluateArgumentsL (context, initial, value1, value2);

  if (type == TYPE_starts_with)
    {
      unsigned length1 = uni_strlen (value1), length2 = uni_strlen (value2);
      return length1 >= length2 && op_memcmp (value1, value2, length2 * sizeof *value2) == 0;
    }
  else
    return uni_strstr (value1, value2) != 0;
}

XPath_SubstringBeforeOrAfterFunctionCall::XPath_SubstringBeforeOrAfterFunctionCall (XPath_Parser *parser, Type type)
  : XPath_StringExpression (parser),
    XPath_BinaryStringFunctionCallBase (parser),
    type (type)
{
}

/* static */ XPath_StringExpression *
XPath_SubstringBeforeOrAfterFunctionCall::MakeL (XPath_Parser *parser, Type type, XPath_Expression **arguments, unsigned arguments_count)
{
  XPath_SubstringBeforeOrAfterFunctionCall *call = OP_NEW_L (XPath_SubstringBeforeOrAfterFunctionCall, (parser, type));
  OpStackAutoPtr<XPath_Expression> call_anchor (call);

  call->SetArgumentsL (parser, arguments, arguments_count);

  call_anchor.release ();
  return call;
}

/* virtual */ unsigned
XPath_SubstringBeforeOrAfterFunctionCall::GetExpressionFlags ()
{
  return XPath_BinaryStringFunctionCallBase::GetExpressionFlags () | FLAG_STRING;
}

/* virtual */ const uni_char *
XPath_SubstringBeforeOrAfterFunctionCall::EvaluateToStringL (XPath_Context *context, BOOL initial, TempBuffer &buffer)
{
  const uni_char *value1, *value2;

  EvaluateArgumentsL (context, initial, value1, value2);

  const uni_char *location = uni_strstr (value1, value2);
  if (location)
    {
      if (type == TYPE_substring_before)
        buffer.AppendL (value1, location - value1);
      else
        buffer.AppendL (location + uni_strlen (value2));

      if (buffer.GetStorage ())
        return buffer.GetStorage ();
    }

  return UNI_L ("");
}

/* static */ XPath_Expression *
XPath_NotFunctionCall::MakeL (XPath_Parser *parser, XPath_Expression **arguments, unsigned arguments_count)
{
  if (arguments_count != 1)
    XPATH_WRONG_NUMBER_OF_ARGUMENTS ();

  XPath_Expression *argument0 = arguments[0];
  arguments[0] = 0;
  XPath_BooleanExpression *argument = XPath_BooleanExpression::MakeL (parser, argument0);

  OpStackAutoPtr<XPath_Expression> argument_anchor (argument);

  if (argument->HasFlag (FLAG_CONSTANT))
    {
      BOOL value = !argument->EvaluateToBooleanL (0, TRUE);
      return XPath_LiteralExpression::MakeL (parser, value);
    }
  else
    {
      XPath_NotFunctionCall *call = OP_NEW_L (XPath_NotFunctionCall, (parser, argument));
      argument_anchor.release ();
      return call;
    }
}

/* virtual */
XPath_NotFunctionCall::~XPath_NotFunctionCall ()
{
  OP_DELETE (argument);
}

/* virtual */ unsigned
XPath_NotFunctionCall::GetExpressionFlags ()
{
  return (argument->GetExpressionFlags () & MASK_INHERITED) | FLAG_BOOLEAN;
}

/* virtual */ BOOL
XPath_NotFunctionCall::EvaluateToBooleanL (XPath_Context *context, BOOL initial)
{
  return !argument->EvaluateToBooleanL (context, initial);
}

#endif // XPATH_SUPPORT
