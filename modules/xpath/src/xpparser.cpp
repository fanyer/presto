/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpparser.h"
#include "modules/xpath/src/xplexer.h"
#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xputils.h"
#include "modules/xpath/src/xpnamespaces.h"
#include "modules/xpath/src/xpcontext.h"
#include "modules/xpath/src/xppattern.h"

#include "modules/xpath/src/xppredicateexpr.h"
#include "modules/xpath/src/xpnumericexpr.h"
#include "modules/xpath/src/xpcomparisonexpr.h"
#include "modules/xpath/src/xpliteralexpr.h"
#include "modules/xpath/src/xplocationpathexpr.h"
#include "modules/xpath/src/xplogicalexpr.h"
#include "modules/xpath/src/xpunionexpr.h"
#include "modules/xpath/src/xpfunction.h"
#include "modules/xpath/src/xpunknown.h"

#include "modules/util/str.h"
#include "modules/util/tempbuf.h"

void
XPath_Parser::ResolveQNameL (TempBuffer &buffer, XMLExpandedName &name, const XPath_Token &token)
{
  XMLCompleteNameN completename (token.value, token.length);

  const uni_char *uri;
  unsigned uri_length;

  if (completename.GetPrefix ())
    {
      buffer.AppendL (completename.GetPrefix (), completename.GetPrefixLength ());

      if (namespaces)
        uri = namespaces->GetURI (buffer.GetStorage ());
      else
        uri = 0;

      if (!uri)
        ParseErrorL ("undeclared prefix in qualified name: ''", token.GetLocation (), XPath_Token (XP_TOKEN_INVALID, buffer.GetStorage (), buffer.Length ()));

      uri_length = uni_strlen (uri);
    }
  else
    {
      uri = 0;
      uri_length = 0;
    }

  XMLExpandedNameN temporary (uri, uri_length, completename.GetLocalPart (), completename.GetLocalPartLength ());

  name.SetL (temporary);
}

BOOL
XPath_Parser::GetIsEmpty ()
{
  return expressions.GetCount () - expressions_skip == 0;
}

unsigned
XPath_Parser::GetCount ()
{
  return expressions.GetCount () - expressions_skip;
}

void
XPath_Parser::PushExpressionL (XPath_Expression *expr)
{
  if (OpStatus::IsMemoryError (expressions.Add (expr)))
    {
      OP_DELETE (expr);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }
}

void
XPath_Parser::PushBinaryExpressionL (XPath_ExpressionType type)
{
  XPath_Expression *rhs = PopExpression ();
  XPath_Expression *lhs = PopExpression ();
  XPath_Expression *expr;

#ifdef XPATH_ERRORS
  current_start = lhs->location.start;
  current_end = rhs->location.end;
#endif // XPATH_ERRORS

  switch (type)
    {
    case XP_EXPR_PREDICATE:
      expr = XPath_PredicateExpression::MakeL (this, lhs, rhs);
      break;

    case XP_EXPR_UNION:
      expr = XPath_UnionExpression::MakeL (this, lhs, rhs);
      break;

    case XP_EXPR_MULTIPLY:
    case XP_EXPR_DIVIDE:
    case XP_EXPR_REMAINDER:
    case XP_EXPR_ADD:
    case XP_EXPR_SUBTRACT:
      expr = XPath_NumericExpression::MakeL (this, lhs, rhs, type);
      break;

    case XP_EXPR_LESS_THAN:
    case XP_EXPR_GREATER_THAN:
    case XP_EXPR_LESS_THAN_OR_EQUAL:
    case XP_EXPR_GREATER_THAN_OR_EQUAL:
    case XP_EXPR_EQUAL:
    case XP_EXPR_NOT_EQUAL:
      expr = XPath_ComparisonExpression::MakeL (this, lhs, rhs, type);
      break;

    default:
      expr = XPath_LogicalExpression::MakeL (this, lhs, rhs, type);
    }

  PushExpressionL (expr);
}

void
XPath_Parser::PushProducerExpressionL (XPath_ExpressionType type)
{
  OP_ASSERT (type == XP_EXPR_PREDICATE);

  XPath_Expression *rhs = PopExpression ();
  XPath_Expression *lhs = PopExpression ();

#ifdef XPATH_ERRORS
  current_start = lhs->location.start;
#endif // XPATH_ERRORS

  PushExpressionL (XPath_PredicateExpression::MakeL (this, lhs, rhs));
}

void
XPath_Parser::PushNegateExpressionL (XPath_Expression *expr)
{
#ifdef XPATH_ERRORS
  current_end = expr->location.end;
#endif // XPATH_ERRORS

  XPath_Expression *negate = XPath_NegateExpression::MakeL (this, expr);

  if (!negate)
    {
      OP_DELETE (expr);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  PushExpressionL (negate);
}

void
XPath_Parser::PushLocationPathExpressionL (XPath_Producer *producer)
{
  XPath_Expression *expr = OP_NEW (XPath_LocationPathExpression, (this, producer));

  if (!expr)
    {
      OP_DELETE (producer);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  PushExpressionL (expr);
}

void
XPath_Parser::PushVariableReferenceExpressionL (const XPath_Token &token)
{
#ifdef XPATH_EXTENSION_SUPPORT
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);

  ResolveQNameL (buffer, current_name, token);

  if (extensions)
    {
      XPath_VariableReader *reader = readers;

      while (reader)
        if (reader->GetName () == current_name)
          break;
        else
          reader = reader->GetNext ();

      if (!reader)
        reader = GetVariableReaderL (current_name);

      if (reader)
        {
#ifdef XPATH_ERRORS
          current_start = token.GetLocation ().start;
          current_end = token.GetLocation ().end;
#endif // XPATH_ERRORS

          PushExpressionL (XPath_Unknown::MakeL (this, reader));
          return;
        }
    }
#endif // XPATH_EXTENSION_SUPPORT

#ifdef XPATH_ERRORS
  CompilationErrorL ("reference to undefined variable: ''", token.GetLocation (), &current_name);
#else // XPATH_ERRORS
  LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
}

void
XPath_Parser::PushLiteralExpressionL (const XPath_Token &token)
{
#ifdef XPATH_ERRORS
  current_start = token.GetLocation ().start;
  current_end = token.GetLocation ().end;
#endif // XPATH_ERRORS

  PushExpressionL (XPath_LiteralExpression::MakeL (this, token.value, token.length));
}

void
XPath_Parser::PushNumberExpressionL (const XPath_Token &token)
{
#ifdef XPATH_ERRORS
  current_start = token.GetLocation ().start;
  current_end = token.GetLocation ().end;
#endif // XPATH_ERRORS

  PushExpressionL (XPath_LiteralExpression::MakeL (this, XPath_Value::AsNumberL (token.value, token.length)));
}

OP_STATUS
XPath_FunctionCallExpression_Make (XPath_Expression *&call, XPath_Parser *parser, const XMLExpandedName &name, XPath_Expression **arguments, unsigned arguments_count)
{
  TRAPD (status, call = XPath_FunctionCallExpression::MakeL (parser, name, arguments, arguments_count));
  return status;
}

void
XPath_Parser::PushFunctionCallExpressionL (const XPath_Token &token, unsigned argc)
{
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);
  XMLExpandedName name; ANCHOR (XMLExpandedName, name);

  ResolveQNameL (buffer, name, token);

  XPath_Expression **arguments = argc == 0 ? 0 : OP_NEWA_L (XPath_Expression *, argc);
  ANCHOR_ARRAY (XPath_Expression *, arguments);

  for (unsigned count = expressions.GetCount (), index = count - argc, aindex = 0; index < count; ++index, ++aindex)
    arguments[aindex] = expressions.Get (index);

  expressions.Remove (expressions.GetCount () - argc, argc);

  XPath_Expression *call;

  current_name = name;
  OP_STATUS status = XPath_FunctionCallExpression_Make (call, this, name, arguments, argc);
  current_name = XMLExpandedName ();

  if (OpStatus::IsError (status))
    {
      for (unsigned index = 0; index < argc; ++index)
        OP_DELETE (arguments[index]);

      /* Specific error message will already have been reported. */
      LEAVE (status);
    }

  PushExpressionL (call);
}

XPath_Expression *
XPath_Parser::PopExpression ()
{
  XPath_Expression *expr = expressions.Get (expressions.GetCount () - 1);
  expressions.Remove (expressions.GetCount () - 1);
  return expr;
}

XPath_Expression *
XPath_Parser::LastExpression ()
{
  return expressions.Get (expressions.GetCount () - 1);
}

void
XPath_Parser::Reset ()
{
  lexer.Reset ();

  state_counter = -1;
  value_counter = -1;
  number_counter = -1;
  buffer_counter = -1;
  node_counter = -1;
  nodeset_counter = -1;
  nodelist_counter = -1;
  stringset_counter = -1;
  ci_counter = -1;

#ifdef XPATH_EXTENSION_SUPPORT
  variablestate_counter = -1;
  functionstate_counter = -1;

  OP_DELETE (readers);
  readers = 0;
#endif // XPATH_EXTENSION_SUPPORT
}

XPath_Parser::XPath_Parser (XPath_Namespaces *namespaces, const uni_char *expression)
  : lexer (expression),
    namespaces (namespaces),
    expressions_skip (0)
{
#ifdef XPATH_EXTENSION_SUPPORT
  readers = 0;
#endif // XPATH_EXTENSION_SUPPORT

  Reset ();
}

XPath_Parser::~XPath_Parser ()
{
#ifdef XPATH_EXTENSION_SUPPORT
  OP_DELETE (readers);
#endif // XPATH_EXTENSION_SUPPORT
  expressions.DeleteAll ();
}

#ifdef XPATH_EXTENSION_SUPPORT

XPath_VariableReader *
XPath_Parser::GetVariableReaderL (const XMLExpandedName &name)
{
  XPathVariable *variable;

  OP_STATUS status = extensions->GetVariable (variable, name);

  if (status == OpStatus::ERR_NO_MEMORY)
    LEAVE (status);
  else if (status == OpStatus::ERR)
    return 0;

  XPath_VariableReader *reader = readers;

  while (reader)
    if (reader->GetVariable () == variable)
      return reader;
    else
      reader = reader->GetNext ();

  reader = XPath_VariableReader::MakeL (this, name, variable);

  reader->SetNext (readers);
  readers = reader;

  return reader;
}

XPath_VariableReader *
XPath_Parser::TakeVariableReaders ()
{
  XPath_VariableReader *temporary = readers;
  readers = 0;
  return temporary;
}

#endif // XPATH_EXTENSION_SUPPORT

#ifdef XPATH_ERRORS

void
XPath_Parser::ParseErrorL (const char *message, const XPath_Location &location, const XPath_Token &token)
{
  if (error_message_storage)
    {
      OpString location_string; ANCHOR (OpString, location_string);
      if (location.IsValid ())
        if (location.start == location.end)
          LEAVE_IF_ERROR (location_string.AppendFormat (UNI_L ("character %u"), location.start + 1));
        else
          LEAVE_IF_ERROR (location_string.AppendFormat (UNI_L ("characters %u-%u"), location.start + 1, location.end));
      else
        location_string.SetL ("unknown location");

      LEAVE_IF_ERROR (error_message_storage->AppendFormat (UNI_L ("parse error (%s): "), location_string.CStr ()));
      error_message_storage->AppendL (message);

      int index = error_message_storage->Find ("''");
      if (index != KNotFound)
        if (token == XP_TOKEN_END)
          LEAVE_IF_ERROR (error_message_storage->Insert (index + 1, "<EOF>"));
        else
          LEAVE_IF_ERROR (error_message_storage->Insert (index + 1, lexer.GetSource () + token.location.start, token.location.end - token.location.start));
    }

  LEAVE (OpStatus::ERR);
}

void
XPath_Parser::CompilationErrorL (const char *message, const XPath_Location &location, const XMLExpandedName *name)
{
  if (error_message_storage)
    {
      OpString location_string; ANCHOR (OpString, location_string);
      if (location.IsValid ())
        {
          LEAVE_IF_ERROR (location_string.AppendFormat (UNI_L ("characters %u-%u, \""), location.start + 1, location.end));
          location_string.AppendL (lexer.GetSource () + location.start, location.end - location.start);
          location_string.AppendL ("\"");
        }
      else
        location_string.SetL ("unknown location");

      LEAVE_IF_ERROR (error_message_storage->AppendFormat (UNI_L ("compilation error (%s): "), location_string.CStr ()));
      error_message_storage->AppendL (message);

      int index = error_message_storage->Find ("''");
      if (index != KNotFound)
        if (name->GetUri ())
          {
            OpString expanded_name_string; ANCHOR (OpString, expanded_name_string);
            LEAVE_IF_ERROR (expanded_name_string.AppendFormat (UNI_L ("{ %s, %s }"), name->GetUri (), name->GetLocalPart ()));
            LEAVE_IF_ERROR (error_message_storage->Insert (index + 1, expanded_name_string.CStr ()));
          }
        else
          error_message_storage->Insert (index + 1, name->GetLocalPart ());
    }

  LEAVE (OpStatus::ERR);
}

XPath_Location
XPath_Parser::GetCurrentLocation ()
{
  return XPath_Location (current_start, current_end);
}

const XMLExpandedName *
XPath_Parser::GetCurrentName ()
{
  return current_name.GetLocalPart () ? &current_name : 0;
}

#endif // XPATH_ERRORS

XPath_Expression *
XPath_Parser::ParseToPrimitiveL ()
{
  return ParseExpressionL (XP_EXPR_OR, TRUE);
}

XPath_Producer *
XPath_Parser::ParseToProducerL (BOOL ordered, BOOL no_duplicates)
{
  XPath_Expression *expression = ParseExpressionL (XP_EXPR_OR, TRUE);

  BOOL is_compatible = expression->HasFlag (XPath_Expression::FLAG_PRODUCER);

#ifdef XPATH_EXTENSION_SUPPORT
  if (is_compatible && expression->HasFlag (XPath_Expression::FLAG_UNKNOWN) && expression->GetResultType () != XP_VALUE_NODESET)
    is_compatible = FALSE;
#endif // XPATH_EXTENSION_SUPPORT

  if (!is_compatible)
    {
      OP_DELETE (expression);
      return 0;
    }

  XPath_Producer *producer = XPath_Expression::GetProducerL (this, expression);

  if (ordered || no_duplicates)
    producer = XPath_Producer::EnsureFlagsL (this, producer, (ordered ? XPath_Producer::FLAG_DOCUMENT_ORDER : 0) | (no_duplicates ? XPath_Producer::FLAG_NO_DUPLICATES : 0));

  return producer;
}

void
XPath_Parser::CopyStateSizes (XPath_ContextStateSizes &state_sizes)
{
  state_sizes.states_count = GetStateIndex ();
  state_sizes.values_count = GetValueIndex ();
  state_sizes.numbers_count = GetNumberIndex ();
  state_sizes.buffers_count = GetBufferIndex ();
  state_sizes.nodes_count = GetNodeIndex ();
  state_sizes.nodesets_count = GetNodeSetIndex ();
  state_sizes.nodelists_count = GetNodeListIndex ();
  state_sizes.stringsets_count = GetStringSetIndex ();
  state_sizes.cis_count = GetContextInformationIndex ();

#ifdef XPATH_EXTENSION_SUPPORT
  state_sizes.variablestates_count = GetVariableStateIndex ();
  state_sizes.functionstates_count = GetFunctionStateIndex ();
#endif // XPATH_EXTENSION_SUPPORT
}

static BOOL
XPath_TokenStartsLocationPath (XPath_Token &token)
{
  if (token == XP_TOKEN_OPERATOR)
    return token == "/" || token == "//";
  else if (token == XP_TOKEN_PUNCTUATOR)
    return token == "@" || token == "." || token == "..";
  else
    return token == XP_TOKEN_AXISNAME || token == XP_TOKEN_NODETYPE || token == XP_TOKEN_NAMETEST;
}

XPath_Expression *
XPath_Parser::ParseExpressionL (XPath_ExpressionType type, BOOL top_expr)
{
  unsigned previous_expressions_skip = expressions_skip;
  expressions_skip = expressions.GetCount ();

 iterate:
  XPath_Token token = lexer.GetNextTokenL ();

  switch (GetIsEmpty () && type > XP_EXPR_NEGATE ? XP_EXPR_NEGATE : type)
    {
    case XP_EXPR_OR:
      if (token == XP_TOKEN_OPERATOR && token == "or")
        {
          lexer.ConsumeToken ();
          PushExpressionL (ParseExpressionL (XP_EXPR_AND));
          PushBinaryExpressionL (XP_EXPR_OR);
          goto iterate;
        }

    case XP_EXPR_AND:
      if (token == XP_TOKEN_OPERATOR && token == "and")
        {
          lexer.ConsumeToken ();
          PushExpressionL (ParseExpressionL (XP_EXPR_EQUALITY));
          PushBinaryExpressionL (XP_EXPR_AND);
          goto iterate;
        }

    case XP_EXPR_EQUALITY:
      if (token == XP_TOKEN_OPERATOR && (token == "=" || token == "!="))
        {
          lexer.ConsumeToken ();
          PushExpressionL (ParseExpressionL (XP_EXPR_RELATIONAL));
          PushBinaryExpressionL (token == "=" ? XP_EXPR_EQUAL : XP_EXPR_NOT_EQUAL);
          goto iterate;
        }

    case XP_EXPR_RELATIONAL:
      if (token == XP_TOKEN_OPERATOR && (token == "<" || token == "<=" || token == ">" || token == ">="))
        {
          lexer.ConsumeToken ();
          PushExpressionL (ParseExpressionL (XP_EXPR_ADDITIVE));
          XPath_ExpressionType subtype;

          if (token == "<")
            subtype = XP_EXPR_LESS_THAN;
          else if (token == "<=")
            subtype = XP_EXPR_LESS_THAN_OR_EQUAL;
          else if (token == ">")
            subtype = XP_EXPR_GREATER_THAN;
          else
            subtype = XP_EXPR_GREATER_THAN_OR_EQUAL;

          PushBinaryExpressionL (subtype);
          goto iterate;
        }

    case XP_EXPR_ADDITIVE:
      if (token == XP_TOKEN_OPERATOR && (token == "+" || token == "-"))
        {
          lexer.ConsumeToken ();
          PushExpressionL (ParseExpressionL (XP_EXPR_MULTIPLICATIVE));
          PushBinaryExpressionL (token == "+" ? XP_EXPR_ADD : XP_EXPR_SUBTRACT);
          goto iterate;
        }

    case XP_EXPR_MULTIPLICATIVE:
      if (token == XP_TOKEN_OPERATOR && (token == "*" || token == "div" || token == "mod"))
        {
          lexer.ConsumeToken ();
          PushExpressionL (ParseExpressionL (XP_EXPR_NEGATE));
          XPath_ExpressionType subtype;

          if (token == "*")
            subtype = XP_EXPR_MULTIPLY;
          else if (token == "div")
            subtype = XP_EXPR_DIVIDE;
          else
            subtype = XP_EXPR_REMAINDER;

          PushBinaryExpressionL (subtype);
          goto iterate;
        }

    case XP_EXPR_NEGATE:
      if (GetIsEmpty () && token == XP_TOKEN_OPERATOR && token == "-")
        {
          lexer.ConsumeToken ();
#ifdef XPATH_ERRORS
          unsigned start = token.GetLocation ().start;
          XPath_Expression *operand = ParseExpressionL (XP_EXPR_UNION);
          current_start = start;
          PushNegateExpressionL (operand);
#else // XPATH_ERRORS
          PushNegateExpressionL (ParseExpressionL (XP_EXPR_UNION));
#endif // XPATH_ERRORS
          goto iterate;
        }

    case XP_EXPR_UNION:
      if (!GetIsEmpty () && token == XP_TOKEN_OPERATOR && token == "|")
        {
          lexer.ConsumeToken ();
          PushExpressionL (ParseExpressionL (XP_EXPR_PATH));
          PushBinaryExpressionL (XP_EXPR_UNION);
          goto iterate;
        }

    case XP_EXPR_PATH:
      if (GetIsEmpty () && XPath_TokenStartsLocationPath (token))
        {
#ifdef XPATH_ERRORS
          unsigned start = token.GetLocation ().start;
          XPath_Producer *producer = ParseLocationPathL (0);
          current_start = start;
          current_end = lexer.GetOffset ();
          PushLocationPathExpressionL (producer);
#else // XPATH_ERRORS
          PushLocationPathExpressionL (ParseLocationPathL (0));
#endif // XPATH_ERRORS
          goto iterate;
        }
      else if (!GetIsEmpty () && token == XP_TOKEN_OPERATOR && (token == "/" || token == "//"))
        {
          XPath_Expression *base_expr = LastExpression ();
#ifdef XPATH_ERRORS
          unsigned start = base_expr->location.start;
#endif // XPATH_ERRORS
          BOOL is_descendants = token == "//";
          XPath_Producer *base_producer = XPath_Expression::GetProducerL (this, base_expr);
          if (!base_producer)
#ifdef XPATH_ERRORS
            CompilationErrorL ("expected node-set expression", base_expr->location, 0);
#else // XPATH_ERRORS
            LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
          PopExpression ();
          lexer.ConsumeToken ();
          XPath_Producer *producer = ParseLocationPathL (base_producer, is_descendants);
#ifdef XPATH_ERRORS
          current_start = start;
          current_end = lexer.GetOffset ();
#endif // XPATH_ERRORS
          PushLocationPathExpressionL (producer);
          goto iterate;
        }

    case XP_EXPR_PREDICATE:
      if (!GetIsEmpty () && token == XP_TOKEN_PUNCTUATOR && token == "[")
        {
          lexer.ConsumeToken ();
          PushExpressionL (ParseExpressionL (XP_EXPR_OR));
          token = lexer.GetNextTokenL ();
          if (token != XP_TOKEN_PUNCTUATOR || token != "]")
#ifdef XPATH_ERRORS
            ParseErrorL ("expected ']', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
            LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
          lexer.ConsumeToken ();
#ifdef XPATH_ERRORS
          current_end = token.GetLocation ().end;
#endif // XPATH_ERRORS
          PushProducerExpressionL (XP_EXPR_PREDICATE);
          goto iterate;
        }

    default:
      if (!GetIsEmpty ())
        if (GetCount () > 1)
#ifdef XPATH_ERRORS
          /* FIXME: figure out if/when this happens? */
          ParseErrorL ("parse error", token.GetLocation (), token);
#else // XPATH_ERRORS
          LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
        else
          {
            if (top_expr && token != XP_TOKEN_END)
              {
#ifdef XPATH_ERRORS
                ParseErrorL ("expected '<EOF>', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
                LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
              }

            expressions_skip = previous_expressions_skip;
            return PopExpression ();
          }
      else if (token == XP_TOKEN_VARIABLEREFERENCE)
        {
          PushVariableReferenceExpressionL (token);
          lexer.ConsumeToken ();
          goto iterate;
        }
      else if (token == XP_TOKEN_PUNCTUATOR && token == "(")
        {
          lexer.ConsumeToken ();
          PushExpressionL (ParseExpressionL (XP_EXPR_OR));
          token = lexer.GetNextTokenL ();
          if (token != XP_TOKEN_PUNCTUATOR || token != ")")
#ifdef XPATH_ERRORS
            ParseErrorL ("expected ')' got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
            LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
          lexer.ConsumeToken ();
          goto iterate;
        }
      else if (token == XP_TOKEN_LITERAL)
        {
          PushLiteralExpressionL (token);
          lexer.ConsumeToken ();
          goto iterate;
        }
      else if (token == XP_TOKEN_NUMBER)
        {
          PushNumberExpressionL (token);
          lexer.ConsumeToken ();
          goto iterate;
        }
      else if (token == XP_TOKEN_FUNCTIONNAME)
        {
          XPath_Token name = token;
          unsigned argc = 0;

          lexer.ConsumeToken ();

          /* Consume the '(' that made the lexer call this a function
             name. */
          token = lexer.GetNextTokenL ();
          OP_ASSERT (token == "(");
          lexer.ConsumeToken ();

          token = lexer.GetNextTokenL ();
          if (token != XP_TOKEN_PUNCTUATOR || token != ")")
            while (TRUE)
              {
                PushExpressionL (ParseExpressionL (XP_EXPR_OR));

                ++argc;

                token = lexer.GetNextTokenL ();
                if (token != XP_TOKEN_PUNCTUATOR || token != "," && token != ")")
#ifdef XPATH_ERRORS
                  ParseErrorL ("expected ')' got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
                  LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
                else if (token == ")")
                  break;
                else
                  lexer.ConsumeToken ();
              }

#ifdef XPATH_ERRORS
          current_start = name.GetLocation ().start;
          current_end = token.GetLocation ().end;
#endif // XPATH_ERRORS

          lexer.ConsumeToken ();
          PushFunctionCallExpressionL (name, argc);
          goto iterate;
        }
      else
#ifdef XPATH_ERRORS
        /* FIXME: figure out if/when this happens? */
        ParseErrorL ("unexpected token ''", token.GetLocation (), token);
#else // XPATH_ERRORS
        LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
    }

  // Never happens.
  return 0;
}

class XPath_ProducerGenerator
{
private:
  XPath_Parser *parser;
  XPath_Producer *producer;
  XPath_Step::Axis *current_axis;
  BOOL has_context_dependent_predicate;

  XPath_Axis GetCurrentAxisType ();
  void AddFilterL (XPath_ChainProducer *filter);
  void SillinessDetected ();

public:
  XPath_ProducerGenerator (XPath_Parser *parser, XPath_Producer *producer);
  ~XPath_ProducerGenerator ();

  void InitializeL (BOOL absolute);
  void AddStepL (XPath_Axis axis);
  void AddNodeTypeTestL (XPath_NodeType type);
  void AddNameTestL (const XMLExpandedName &name);
  void AddPITestL (const uni_char *target, unsigned length);
  void AddPredicateL (XPath_Expression *expression);

  XPath_Producer *FinishL ();
};

XPath_Axis
XPath_ProducerGenerator::GetCurrentAxisType ()
{
  if (XPath_Step::Axis *axis = current_axis)
    {
      while (axis && axis->GetAxis () == XP_AXIS_SELF)
        if (XPath_Step::Axis *previous_axis = axis->GetPreviousAxis ())
          axis = previous_axis;
        else
          break;

      return axis->GetAxis ();
    }

  return XP_AXIS_SELF;
}

void
XPath_ProducerGenerator::AddFilterL (XPath_ChainProducer *filter)
{
  if (producer)
    {
      XPath_Producer *previous = producer;
      producer = filter;

      if (current_axis && !has_context_dependent_predicate)
        {
          XPath_Producer::TransformData data;
          data.filter.filter = current_axis->GetFilter ();
          data.filter.partial = FALSE;

          if (filter->TransformL (parser, XPath_Producer::TRANSFORM_XMLTREEACCESSOR_FILTER, data) && !data.filter.partial)
            {
              filter->ResetProducer ();
              OP_DELETE (filter);

              producer = previous;

              if (producer == current_axis)
                /* In case we added the first predicate and then removed it
                   again. */
                current_axis->SetManualLocalContextReset (FALSE);
            }
        }
    }
}

void
XPath_ProducerGenerator::SillinessDetected ()
{
  OP_DELETE (producer);
  producer = 0;
}

XPath_ProducerGenerator::XPath_ProducerGenerator (XPath_Parser *parser, XPath_Producer *producer)
  : parser (parser),
    producer (producer),
    current_axis (0),
    has_context_dependent_predicate (FALSE)
{
}

XPath_ProducerGenerator::~XPath_ProducerGenerator ()
{
  OP_DELETE (producer);
}

void
XPath_ProducerGenerator::InitializeL (BOOL absolute)
{
  producer = OP_NEW_L (XPath_InitialContextProducer, (parser, absolute));
}

void
XPath_ProducerGenerator::AddStepL (XPath_Axis axis_type)
{
  if (producer)
    {
      XPath_Axis current_axis_type = GetCurrentAxisType ();

      if (current_axis_type == XP_AXIS_ATTRIBUTE || current_axis_type == XP_AXIS_NAMESPACE)
        switch (axis_type)
          {
          case XP_AXIS_ATTRIBUTE:
          case XP_AXIS_CHILD:
          case XP_AXIS_DESCENDANT:
          case XP_AXIS_FOLLOWING_SIBLING:
          case XP_AXIS_NAMESPACE:
          case XP_AXIS_PRECEDING_SIBLING:
            /* These are always empty when applied to an attribute or namespace
               node. */
            SillinessDetected ();
            return;
          }

      producer = current_axis = XPath_Step::Axis::MakeL (parser, producer, axis_type);
      has_context_dependent_predicate = FALSE;
    }
}

void
XPath_ProducerGenerator::AddNodeTypeTestL (XPath_NodeType type)
{
  if (current_axis && (current_axis->GetAxis () == XP_AXIS_ATTRIBUTE && type != XP_NODE_ATTRIBUTE || current_axis->GetAxis () == XP_AXIS_NAMESPACE && type != XP_NODE_NAMESPACE))
    SillinessDetected ();
  else
    AddFilterL (XPath_NodeTypeTest::MakeL (producer, type));
}

void
XPath_ProducerGenerator::AddNameTestL (const XMLExpandedName &name)
{
  if (producer)
    {
      XPath_Axis axis_type = current_axis->GetAxis ();
      XPath_NodeType type;

      if (axis_type == XP_AXIS_ATTRIBUTE)
        type = XP_NODE_ATTRIBUTE;
      else if (axis_type == XP_AXIS_NAMESPACE)
        type = XP_NODE_NAMESPACE;
      else
        {
          switch (GetCurrentAxisType ())
            {
            case XP_AXIS_ATTRIBUTE:
            case XP_AXIS_NAMESPACE:
              /* Attribute or namespace axis followed by one or more self axes,
                 and a self axis has a name test.  The name test will only match
                 elements, so it cannot match. */
              SillinessDetected ();
              return;
            }

          type = XP_NODE_ELEMENT;
        }

      BOOL is_wild = name.GetLocalPart ()[0] == '*' && name.GetLocalPart ()[1] == 0;

      if (name.GetUri () && GetCurrentAxisType () == XP_AXIS_NAMESPACE)
        /* Namespace nodes never have a namespace URI. */
        SillinessDetected ();
      else if (!name.GetUri () && is_wild)
        AddNodeTypeTestL (type);
      else if (!is_wild && type == XP_NODE_ATTRIBUTE)
        {
          XPath_Producer *previous_producer = current_axis->GetPrevious (XPath_Producer::PREVIOUS_ANY, FALSE);

          current_axis->ResetProducer ();
          OP_DELETE (current_axis);
          producer = NULL;
          current_axis = NULL;

          producer = current_axis = XPath_SingleAttribute::MakeL (parser, previous_producer, name);
        }
      else
        AddFilterL (XPath_NameTest::MakeL (producer, type, name));
    }
}

void
XPath_ProducerGenerator::AddPITestL (const uni_char *target, unsigned length)
{
  if (producer)
    {
      switch (GetCurrentAxisType ())
        {
        case XP_AXIS_ATTRIBUTE:
        case XP_AXIS_ANCESTOR:
        case XP_AXIS_NAMESPACE:
        case XP_AXIS_PARENT:
          /* These won't contain processing instructions. */
          SillinessDetected ();
          return;
        }

      AddFilterL (XPath_PITest::MakeL (producer, target, length));
    }
}

void
XPath_ProducerGenerator::AddPredicateL (XPath_Expression *expression)
{
  if (producer)
    {
      if ((expression->GetPredicateExpressionFlags () & (XPath_Expression::FLAG_CONTEXT_POSITION | XPath_Expression::FLAG_CONTEXT_SIZE)) != 0)
        has_context_dependent_predicate = TRUE;

      OpStackAutoPtr<XPath_Expression> expression_anchor (expression);
      XPath_Step::Predicate *filter = XPath_Step::Predicate::MakeL (parser, producer, expression, FALSE);
      expression_anchor.release ();
      AddFilterL (filter);
    }
}

XPath_Producer *
XPath_ProducerGenerator::FinishL ()
{
  if (producer)
    {
      producer->OptimizeL (0);

      XPath_Producer *finished = producer;
      producer = 0;
      return finished;
    }
  else
    return OP_NEW_L (XPath_EmptyProducer, (parser));
}

XPath_Producer *
XPath_Parser::ParseLocationPathL (XPath_Producer *base_producer, BOOL is_descendants)
{
  BOOL absolute = FALSE, descendant_or_self_first = is_descendants;

  XPath_ProducerGenerator generator (this, base_producer); ANCHOR (XPath_ProducerGenerator, generator);

  XPath_Token token = lexer.GetNextTokenL ();

  if (token == XP_TOKEN_OPERATOR)
    if (token == "/" || token == "//")
      if (base_producer)
#ifdef XPATH_ERRORS
        ParseErrorL ("expected axis name, got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
        LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
      else
        {
          absolute = TRUE;

          if (token == "//")
            descendant_or_self_first = TRUE;

          token = lexer.ConsumeAndGetNextTokenL ();
        }

  if (!base_producer)
    generator.InitializeL (absolute);

  if (descendant_or_self_first)
    generator.AddStepL (XP_AXIS_DESCENDANT_OR_SELF);

  while (TRUE)
    {
      BOOL want_nodetest = TRUE, consume = TRUE;

      if (token == XP_TOKEN_PUNCTUATOR)
        if (token == ".")
          {
            want_nodetest = FALSE;
            generator.AddStepL (XP_AXIS_SELF);
          }
        else if (token == "..")
          {
            want_nodetest = FALSE;
            generator.AddStepL (XP_AXIS_PARENT);
          }
        else if (token == "@")
          generator.AddStepL (XP_AXIS_ATTRIBUTE);
        else if (absolute)
          break;
        else
#ifdef XPATH_ERRORS
          ParseErrorL ("expected axis name, got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
          LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
      else if (token == XP_TOKEN_AXISNAME)
        {
          XPath_Axis axis = XPath_Utils::GetAxis (token);

          token = lexer.ConsumeAndGetNextTokenL ();

          /* The lexer only calls names followed by '::' axis names, so we can
             be quite sure the next token is '::' and nothing else. */
          OP_ASSERT (token == XP_TOKEN_PUNCTUATOR && token == "::");

          generator.AddStepL (axis);
        }
      else if (token == XP_TOKEN_NODETYPE || token == XP_TOKEN_NAMETEST)
        {
          generator.AddStepL (XP_AXIS_CHILD);
          consume = FALSE;
        }
      else if (absolute)
        break;
      else
#ifdef XPATH_ERRORS
        ParseErrorL ("expected axis name, got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
        LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

      absolute = FALSE;

      if (consume)
        token = lexer.ConsumeAndGetNextTokenL ();

      if (want_nodetest)
        {
          if (token == XP_TOKEN_NODETYPE)
            {
              XPath_NodeType type = XPath_Utils::GetNodeType (token);

              token = lexer.ConsumeAndGetNextTokenL ();

              /* The lexer only calls names followed by '(' node type tests, so
                 we can be quite sure the next token is '(' and nothing else. */
              OP_ASSERT (token == XP_TOKEN_PUNCTUATOR && token == "(");

              token = lexer.ConsumeAndGetNextTokenL ();

              if (type == XP_NODE_PI && token == XP_TOKEN_LITERAL)
                {
                  generator.AddPITestL (token.value, token.length);

                  token = lexer.ConsumeAndGetNextTokenL ();
                }
              else if (type != XP_NODE_ROOT)
                generator.AddNodeTypeTestL (type);

              if (token != XP_TOKEN_PUNCTUATOR || token != ")")
#ifdef XPATH_ERRORS
                ParseErrorL ("expected ')', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
                LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
            }
          else if (token == XP_TOKEN_NAMETEST)
            {
              TempBuffer buffer; ANCHOR (TempBuffer, buffer);
              XMLExpandedName name; ANCHOR (XMLExpandedName, name);

              ResolveQNameL (buffer, name, token);

              generator.AddNameTestL (name);
            }
          else
#ifdef XPATH_ERRORS
            ParseErrorL ("expected node test, got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
            LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

          token = lexer.ConsumeAndGetNextTokenL ();
        }

      while (token == XP_TOKEN_PUNCTUATOR && token == "[")
        {
          lexer.ConsumeToken ();

          generator.AddPredicateL (ParseExpressionL ());

          token = lexer.GetNextTokenL ();

          if (token != XP_TOKEN_PUNCTUATOR || token != "]")
#ifdef XPATH_ERRORS
            ParseErrorL ("expected ']', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
            LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

          token = lexer.ConsumeAndGetNextTokenL ();
        }

      if (token == XP_TOKEN_OPERATOR)
        {
          if (token == "//")
            generator.AddStepL (XP_AXIS_DESCENDANT_OR_SELF);
          else if (token != "/")
            break;

          token = lexer.ConsumeAndGetNextTokenL ();
        }
      else
       break;
    }

  return generator.FinishL ();
}

#ifdef XPATH_PATTERN_SUPPORT

XPath_Pattern *
XPath_Parser::ParsePatternL (float &priority)
{
  XPath_SimplePattern *simple_pattern = OP_NEW_L (XPath_SimplePattern, ());
  OpStackAutoPtr<XPath_SimplePattern> pattern_anchor (simple_pattern);

  XPath_Token token = lexer.GetNextTokenL ();

  /* If we decide to implement this pattern by evaluating it as a location path
     that returns all matching nodes and the first token is neither a function
     name ('id' or 'key') or an operator ('/' or '//'), we should prepend a '//'
     and thus make the location path "try all possible context nodes". */
  BOOL prepend_slashslash = token != XP_TOKEN_FUNCTIONNAME && token != XP_TOKEN_OPERATOR;

  if (token == XP_TOKEN_FUNCTIONNAME)
    {
      priority = 0.5f;

      pattern_anchor.reset (0);
      simple_pattern = 0;

      unsigned arguments_count = 0;

      if (token.length == 2 && uni_strncmp (token.value, "id", 2) == 0)
        arguments_count = 1;
      else if (token.length == 3 && uni_strncmp (token.value, "key", 3) == 0)
        arguments_count = 2;
      else
        {
#ifdef XPATH_ERRORS
          TempBuffer buffer; ANCHOR (TempBuffer, buffer);
          XMLExpandedName name; ANCHOR (XMLExpandedName, name);

          ResolveQNameL (buffer, name, token);

          CompilationErrorL ("invalid function called in pattern: ''", token.GetLocation (), &name);
#else // XPATH_ERRORS
          LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
        }

      token = lexer.ConsumeAndGetNextTokenL ();

      if (token.type != XP_TOKEN_PUNCTUATOR || token != "(")
#ifdef XPATH_ERRORS
        ParseErrorL ("expected '(', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
        LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

      token = lexer.ConsumeAndGetNextTokenL ();

      if (token.type != XP_TOKEN_LITERAL)
#ifdef XPATH_ERRORS
        ParseErrorL ("expected string literal, got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
        LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

      if (arguments_count == 2)
        {
          token = lexer.ConsumeAndGetNextTokenL ();

          if (token.type != XP_TOKEN_PUNCTUATOR || token != ",")
#ifdef XPATH_ERRORS
            ParseErrorL ("expected ',', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
            LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

          token = lexer.ConsumeAndGetNextTokenL ();

          if (token.type != XP_TOKEN_LITERAL)
#ifdef XPATH_ERRORS
            ParseErrorL ("expected string literal, got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
            LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
        }

      token = lexer.ConsumeAndGetNextTokenL ();

      if (token.type != XP_TOKEN_PUNCTUATOR || token != ")")
#ifdef XPATH_ERRORS
        ParseErrorL ("expected ')', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
        LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

      token = lexer.ConsumeAndGetNextTokenL ();

      if (token != XP_TOKEN_END && (token != XP_TOKEN_OPERATOR || token != "/" && token != "//"))
#ifdef XPATH_ERRORS
        ParseErrorL ("expected '/' or '//', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
        LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
    }
  else
    priority = -0.5f;

  if (token == XP_TOKEN_OPERATOR && (token == "/" || token == "//"))
    {
      priority = 0.5f;

      BOOL descendant = token == "//";

      if (simple_pattern && !descendant)
        simple_pattern->AddSeparatorL (this, FALSE);

      token = lexer.ConsumeAndGetNextTokenL ();

      if (descendant && token == XP_TOKEN_END)
#ifdef XPATH_ERRORS
        ParseErrorL ("unexpected end of pattern", token.GetLocation (), token);
#else // XPATH_ERRORS
        LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
    }

  BOOL has_complex_predicates = FALSE;

  while (token != XP_TOKEN_END)
    {
      XPath_Axis axis = XP_AXIS_CHILD;

      if (token == XP_TOKEN_AXISNAME && (token == "child" || token == "attribute"))
        {
          axis = XPath_Utils::GetAxis (token);
          token = lexer.ConsumeAndGetNextTokenL ();

          if (token != XP_TOKEN_PUNCTUATOR || token != "::")
#ifdef XPATH_ERRORS
            ParseErrorL ("expected '::', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
            LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

          token = lexer.ConsumeAndGetNextTokenL ();
        }
      else if (token == XP_TOKEN_PUNCTUATOR && token == "@")
        {
          axis = XP_AXIS_ATTRIBUTE;
          token = lexer.ConsumeAndGetNextTokenL ();
        }
      else if (token != XP_TOKEN_NODETYPE && token != XP_TOKEN_NAMETEST)
#ifdef XPATH_ERRORS
        ParseErrorL ("expected node test, got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
        LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

      if (simple_pattern)
        simple_pattern->AddAxisL (this, axis);

      if (token == XP_TOKEN_NODETYPE)
        {
          XPath_NodeType type = XPath_Utils::GetNodeType (token);
          token = lexer.ConsumeAndGetNextTokenL ();

          if (token.type != XP_TOKEN_PUNCTUATOR || token != "(")
#ifdef XPATH_ERRORS
            ParseErrorL ("expected '(', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
            LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

          token = lexer.ConsumeAndGetNextTokenL ();

          if (type != XP_NODE_ROOT)
            if (type != XP_NODE_PI)
              {
                if (simple_pattern)
                  simple_pattern->AddNodeTypeTestL (this, type);
              }
            else
              {
                const uni_char *target;
                unsigned length;

                if (token.type == XP_TOKEN_LITERAL)
                  {
                    if (priority < 0.0f)
                      priority = 0.0f;

                    target = token.value;
                    length = token.length;
                    token = lexer.ConsumeAndGetNextTokenL ();
                  }
                else
                  {
                    target = 0;
                    length = 0;
                  }

                if (simple_pattern)
                  simple_pattern->AddPITestL (this, target, length);
              }

          if (token.type != XP_TOKEN_PUNCTUATOR || token != ")")
#ifdef XPATH_ERRORS
            ParseErrorL ("expected ')', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
            LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

          token = lexer.ConsumeAndGetNextTokenL ();
        }
      else if (token == XP_TOKEN_NAMETEST)
        {
          if (simple_pattern)
            {
              TempBuffer buffer; ANCHOR (TempBuffer, buffer);
              XMLExpandedName name; ANCHOR (XMLExpandedName, name);

              ResolveQNameL (buffer, name, token);
              simple_pattern->AddNameTestL (this, name);

              if (name.GetLocalPart ()[0] == '*' && !name.GetLocalPart ()[1])
                {
                  if (name.GetUri () && priority < -0.25f)
                    priority = -0.25f;
                }
              else if (priority < 0.0f)
                priority = 0.0f;
            }

          token = lexer.ConsumeAndGetNextTokenL ();
        }
      else
#ifdef XPATH_ERRORS
        ParseErrorL ("expected node test or name test, got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
        LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

      while (token == XP_TOKEN_PUNCTUATOR && token == "[")
        {
          priority = 0.5f;

          lexer.ConsumeToken ();

          XPath_Expression *predicate = ParseExpressionL ();

          if (simple_pattern)
            if ((predicate->GetPredicateExpressionFlags () & (XPath_Expression::FLAG_CONTEXT_POSITION | XPath_Expression::FLAG_CONTEXT_SIZE | XPath_Expression::FLAG_BLOCKING)) != 0)
              {
                pattern_anchor.reset (0);
                simple_pattern = 0;
              }
            else
              {
                if (!has_complex_predicates && (predicate->GetExpressionFlags () & XPath_Expression::FLAG_COMPLEXITY_COMPLEX) != 0)
                  has_complex_predicates = TRUE;
                simple_pattern->AddPredicateL (this, predicate);
              }

          if (!simple_pattern)
            OP_DELETE (predicate);

          token = lexer.GetNextTokenL ();

          if (token.type != XP_TOKEN_PUNCTUATOR || token != "]")
#ifdef XPATH_ERRORS
            ParseErrorL ("expected ']', got ''", token.GetLocation (), token);
#else // XPATH_ERRORS
            LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS

          token = lexer.ConsumeAndGetNextTokenL ();
        }

      if (token == XP_TOKEN_OPERATOR && (token == "/" || token == "//"))
        {
          priority = 0.5f;

          if (simple_pattern)
            {
              BOOL descendant = token == "//";
              if (descendant && has_complex_predicates)
                {
                  /* A pattern with a step with a complex predicate followed by
                     a '//' has a potentially bad complexity.  Dealing with it
                     as a complex pattern is both simpler and safer. */
                  pattern_anchor.reset (0);
                  simple_pattern = 0;
                }
              else
                simple_pattern->AddSeparatorL (this, descendant);
            }
          token = lexer.ConsumeAndGetNextTokenL ();
        }
      else if (token != XP_TOKEN_END)
#ifdef XPATH_ERRORS
        ParseErrorL ("unexpected token ''", token.GetLocation (), token);
#else // XPATH_ERRORS
        LEAVE (OpStatus::ERR);
#endif // XPATH_ERRORS
    }

  if (simple_pattern)
    {
      simple_pattern->CloseL (this);

      pattern_anchor.release ();
      return simple_pattern;
    }
  else
    {
      Reset ();

      if (prepend_slashslash)
        lexer.PrependToken (XPath_Token (XP_TOKEN_OPERATOR, UNI_L ("//"), 2));

      XPath_Producer *producer = ParseToProducerL (FALSE, FALSE);

      /* The syntax only allows expressions that are known to return
         nodesets. */
      OP_ASSERT (producer);

      XPath_ComplexPattern *complex_pattern = OP_NEW (XPath_ComplexPattern, (producer));

      if (!complex_pattern)
        {
          OP_DELETE (producer);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      return complex_pattern;
    }
}

#endif // XPATH_PATTERN_SUPPORT
#endif // XPATH_SUPPORT
