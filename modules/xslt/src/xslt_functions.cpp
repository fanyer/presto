/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_functions.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_key.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_lexer.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_utils.h"
#include "modules/xslt/src/xslt_engine.h"
#include "modules/xslt/src/xslt_transformation.h"
#include "modules/xslt/src/xslt_tree.h"
#include "modules/util/tempbuf.h"

#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xmlparser/xmldoctype.h"
#include "modules/url/url_api.h"

#define RETURN_IF_ERRORX(expr) do { OP_BOOLEAN statusx = (expr); if (statusx == OpBoolean::IS_FALSE) return XPathFunction::RESULT_BLOCKED; else if (OpStatus::IsError (statusx)) return OpStatus::IsMemoryError (statusx) ? RESULT_OOM : RESULT_FAILED; } while (0)
#define RETURN_UNLESS_FINISHED(expr) do { XPathFunction::Result RESULT = (expr); if (RESULT != XPathFunction::RESULT_FINISHED) return RESULT; } while (0)

static XPathFunction::Result
XSLT_ConvertOpBooleanToXPathFunctionResult (OP_BOOLEAN result, XPathExtensions::Context *extensions_context)
{
  switch (result)
    {
    case OpBoolean::IS_TRUE: return XPathFunction::RESULT_FINISHED;
    case OpBoolean::IS_FALSE: return XPathFunction::RESULT_BLOCKED;
    case OpBoolean::ERR_NO_MEMORY: return XPathFunction::RESULT_OOM;
    }

  return XPathFunction::RESULT_FAILED;
}

static XPathFunction::Result
XSLT_EvaluateToString (XPathExpression::Evaluate *evaluate, const uni_char *&string, XPathExtensions::Context *extensions_context)
{
  evaluate->SetRequestedType (XPathExpression::Evaluate::PRIMITIVE_STRING);
  return XSLT_ConvertOpBooleanToXPathFunctionResult (evaluate->GetStringResult (string), extensions_context);
}

static XPathFunction::Result
XSLT_EvaluateToNumber (XPathExpression::Evaluate *evaluate, double &number, XPathExtensions::Context *extensions_context)
{
  evaluate->SetRequestedType (XPathExpression::Evaluate::PRIMITIVE_NUMBER);
  return XSLT_ConvertOpBooleanToXPathFunctionResult (evaluate->GetNumberResult (number), extensions_context);
}

static XPathFunction::Result
XSLT_EvaluateToSingleNode (XPathExpression::Evaluate *evaluate, XPathNode *&node, XPathExtensions::Context *extensions_context)
{
  evaluate->SetRequestedType (XPathExpression::Evaluate::NODESET_SINGLE | XPathExpression::Evaluate::NODESET_FLAG_ORDERED);
  return XSLT_ConvertOpBooleanToXPathFunctionResult (evaluate->GetSingleNode (node), extensions_context);
}

static XPathFunction::Result
XSLT_EvaluateToNextNode (XPathExpression::Evaluate *evaluate, XPathNode *&node, XPathExtensions::Context *extensions_context)
{
  return XSLT_ConvertOpBooleanToXPathFunctionResult (evaluate->GetNextNode (node), extensions_context);
}

/* virtual */ unsigned
XSLT_Functions::Current::GetResultType (unsigned *arguments, unsigned arguments_count)
{
  return XPathFunction::TYPE_NODESET;
}

/* virtual */ unsigned
XSLT_Functions::Current::GetFlags ()
{
  return 0;
}

/* virtual */ XPathFunction::Result
XSLT_Functions::Current::Call (XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state)
{
  if (context->arguments_count > 0)
    return RESULT_FAILED;

  RETURN_IF_ERRORX (result.SetNodeSet (TRUE, FALSE));

  XPathNode *copy;
  RETURN_IF_ERRORX (XPathNode::MakeCopy (copy, XSLT_Engine::GetCurrentNode (extensions_context)));

  XPathValue::AddNodeStatus addstatus;
  RETURN_IF_ERRORX (result.AddNode (copy, addstatus));

  return RESULT_FINISHED;
}

/* virtual */ unsigned
XSLT_Functions::SystemProperty::GetResultType (unsigned *arguments, unsigned arguments_count)
{
  return XPathFunction::TYPE_STRING;
}

/* virtual */ unsigned
XSLT_Functions::SystemProperty::GetFlags()
{
  return 0;
}

/* virtual */ XPathFunction::Result
XSLT_Functions::SystemProperty::Call(XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state)
{
  if (context->arguments_count != 1)
    return RESULT_FAILED;

  const uni_char* qname;

  RETURN_UNLESS_FINISHED (XSLT_EvaluateToString (context->arguments[0], qname, extensions_context));

  XMLCompleteNameN completename (qname, uni_strlen (qname));

  if (!XMLNamespaceDeclaration::ResolveName (nsdeclaration, completename, FALSE))
    return RESULT_FAILED;

  XMLExpandedName name;

  RETURN_IF_ERRORX (name.Set (completename));

  const uni_char *nsuri = name.GetUri ();
  const uni_char *string = UNI_L ("");

  if (nsuri && uni_strcmp (nsuri, "http://www.w3.org/1999/XSL/Transform") == 0)
    {
      const uni_char *localpart = name.GetLocalPart ();

      if (uni_strcmp (localpart, "version") == 0)
        string = UNI_L ("1.0");
      else if (uni_strcmp (localpart, "vendor") == 0)
        string = UNI_L ("Opera");
      else if (uni_strcmp (localpart, "vendor-url") == 0)
        string = UNI_L ("http://www.opera.com/");
    }

  RETURN_IF_ERRORX (result.SetString (string));

  return RESULT_FINISHED;
}

static void
XSLT_GenerateIDFromNodeL (TempBuffer &buffer, XPathNode *node, const char* base_address)
{
  // Make the id not easily mapped to a heap address.
  INTPTR diff = static_cast<INTPTR> ((reinterpret_cast<char*> (node->GetNode ()) - base_address) / 4);
  buffer.ExpandL (sizeof (INTPTR) * 4);
  buffer.AppendL ('o');
  if (diff < 0)
    {
      buffer.AppendL ('p');
      diff = -diff;
    }
  buffer.AppendUnsignedLongL (static_cast<UINTPTR> (diff));

  if (node->GetType () == XPathNode::ATTRIBUTE_NODE)
    {
      XMLCompleteName name;
      node->GetNodeName(name);

      buffer.AppendL ('a');
      buffer.AppendL (name.GetLocalPart ());
      buffer.AppendL ('i');
      buffer.AppendUnsignedLongL (name.GetNsIndex ());
    }
  else if (node->GetType () == XPathNode::NAMESPACE_NODE)
    {
      buffer.AppendL ('n');
      buffer.AppendL (node->GetNamespacePrefix ());
    }
}

static OP_STATUS
XSLT_GenerateIDFromNode (TempBuffer &buffer, XPathNode *node, void* base_address)
{
  TRAPD (status, XSLT_GenerateIDFromNodeL (buffer, node, reinterpret_cast<const char*> (base_address)));
  return status;
}

/* virtual */ unsigned
XSLT_Functions::GenerateID::GetResultType (unsigned *arguments, unsigned arguments_count)
{
  return XPathFunction::TYPE_STRING;
}

/* virtual */ unsigned
XSLT_Functions::GenerateID::GetFlags ()
{
  return 0;
}

/* virtual */ XPathFunction::Result
XSLT_Functions::GenerateID::Call(XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state)
{
  XPathNode *node;

  if (context->arguments_count == 0)
    node = context->context_node;
  else if (context->arguments_count == 1)
    RETURN_UNLESS_FINISHED (XSLT_EvaluateToSingleNode (context->arguments[0], node, extensions_context));
  else
    return RESULT_FAILED;

  TempBuffer buffer;
  const uni_char *id;

  if (node)
    {
      OP_STATUS status = XSLT_GenerateIDFromNode (buffer, node, extensions_context);
      RETURN_IF_ERRORX (status);
      id = buffer.GetStorage ();
    }
  else
    id = UNI_L ("");

  RETURN_IF_ERRORX (result.SetString (id));
  return RESULT_FINISHED;
}

XSLT_Functions::Key::Key (XSLT_StylesheetImpl *stylesheet)
  : stylesheet (stylesheet)
{
}

/* virtual */ unsigned
XSLT_Functions::Key::GetResultType (unsigned *arguments, unsigned arguments_count)
{
  return XPathFunction::TYPE_NODESET;
}

/* virtual */ unsigned
XSLT_Functions::Key::GetFlags ()
{
  return FLAG_BLOCKING;
}

/* virtual */ XPathFunction::Result
XSLT_Functions::Key::Call (XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state)
{
  if (context->arguments_count != 2)
    return RESULT_FAILED;

  KeyState *keystate;

  if (!state)
    {
      keystate = OP_NEW (KeyState, ());
      if (!keystate)
        return RESULT_OOM;
      state = keystate;
    }
  else
    keystate = static_cast<KeyState *> (state);

  if (keystate->state == KeyState::PROCESS_ARGUMENT0)
    {
      RETURN_IF_ERRORX (result.SetNodeSet (FALSE, TRUE));

      const uni_char* qname;

      RETURN_UNLESS_FINISHED (XSLT_EvaluateToString (context->arguments[0], qname, extensions_context));

      XMLCompleteNameN completename (qname, uni_strlen (qname));

      if (!XMLNamespaceDeclaration::ResolveName (nsdeclaration, completename, FALSE))
        return RESULT_FAILED;

      RETURN_IF_ERRORX (keystate->keyname.Set (completename));

      if (!stylesheet->FindKey (keystate->keyname))
        return RESULT_FINISHED;

      keystate->state = KeyState::PROCESS_ARGUMENT1;
    }

  if (keystate->state == KeyState::PROCESS_ARGUMENT1)
    {
      /* String conversion unless argument is a node-set, in which case we
         iterate through the node-set and use the string-value of each node. */
      context->arguments[1]->SetRequestedType (XPathExpression::Evaluate::PRIMITIVE_STRING, XPathExpression::Evaluate::PRIMITIVE_STRING, XPathExpression::Evaluate::PRIMITIVE_STRING, XPathExpression::Evaluate::NODESET_ITERATOR);

      unsigned type;

      RETURN_UNLESS_FINISHED (XSLT_ConvertOpBooleanToXPathFunctionResult (context->arguments[1]->CheckResultType (type), extensions_context));

      keystate->state = type == XPathExpression::Evaluate::PRIMITIVE_STRING ? KeyState::PROCESS_STRING : KeyState::PROCESS_NODES;
    }

  XPathFunction::Result normal_result = keystate->state == KeyState::PROCESS_STRING ? RESULT_FINISHED : RESULT_PAUSED;

  TempBuffer buffer;
  const uni_char *value;

  if (keystate->has_value)
    {
      value = keystate->value.CStr ();
      keystate->has_value = FALSE;
    }
  else if (keystate->state == KeyState::PROCESS_STRING)
    RETURN_UNLESS_FINISHED (XSLT_EvaluateToString (context->arguments[1], value, extensions_context));
  else
    {
      XPathNode *node;

      RETURN_UNLESS_FINISHED (XSLT_EvaluateToNextNode (context->arguments[1], node, extensions_context));

      if (node)
        {
          RETURN_IF_ERRORX (node->GetStringValue (buffer));
          value = buffer.GetStorage () ? buffer.GetStorage () : UNI_L ("");
        }
      else
        return RESULT_FINISHED;
    }

  OP_BOOLEAN is_finished = XSLT_Engine::FindKeyedNodes (extensions_context, keystate->keyname, context->context_node->GetTreeAccessor (), value, &result);

  if (is_finished == OpBoolean::IS_FALSE)
    {
      RETURN_IF_ERRORX (keystate->value.Set (value));
      keystate->has_value = TRUE;
      return RESULT_BLOCKED;
    }

  RETURN_IF_ERRORX (is_finished);
  return normal_result;
}

XSLT_Functions::ElementOrFunctionAvailable::ElementOrFunctionAvailable (BOOL element_available)
  : element_available (element_available)
{
}


/* virtual */ unsigned
XSLT_Functions::ElementOrFunctionAvailable::GetResultType (unsigned *arguments, unsigned arguments_count)
{
  return XPathFunction::TYPE_BOOLEAN;
}

/* virtual */ unsigned
XSLT_Functions::ElementOrFunctionAvailable::GetFlags ()
{
  return 0;
}

/* virtual */ XPathFunction::Result
XSLT_Functions::ElementOrFunctionAvailable::Call (XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state)
{
  if (context->arguments_count != 1)
    return RESULT_FAILED;

  const uni_char* qname;

  RETURN_UNLESS_FINISHED (XSLT_EvaluateToString (context->arguments[0], qname, extensions_context));

  XMLCompleteNameN completename (qname, uni_strlen (qname));

  if (!XMLNamespaceDeclaration::ResolveName (nsdeclaration, completename, element_available))
    return RESULT_FAILED;

  XMLExpandedName name;

  RETURN_IF_ERRORX (name.Set (completename));

  BOOL available = FALSE;

  if (element_available)
    {
      XSLT_ElementType type = XSLT_Lexer::GetElementType (name);

      if (type != XSLTE_UNRECOGNIZED && type != XSLTE_OTHER)
        available = TRUE;
    }
  else
    {
      if (XPath::IsSupportedFunction (name))
        available = TRUE;
      else if (!name.GetUri ())
        {
          const uni_char *localpart = name.GetLocalPart ();

          if (uni_strcmp (localpart, "key") == 0 ||
              uni_strcmp (localpart, "current") == 0 ||
              uni_strcmp (localpart, "document") == 0 ||
              uni_strcmp (localpart, "generate-id") == 0 ||
              uni_strcmp (localpart, "format-number") == 0 ||
              uni_strcmp (localpart, "system-property") == 0 ||
              uni_strcmp (localpart, "element-available") == 0 ||
              uni_strcmp (localpart, "function-available") == 0 ||
              uni_strcmp (localpart, "unparsed-entity-uri") == 0)
            available = TRUE;
        }
      else if (name == XMLExpandedName (UNI_L ("http://exslt.org/common"), UNI_L ("node-set")))
        available = TRUE;
    }

  result.SetBoolean(available);
  return RESULT_FINISHED;
}

/* virtual */
unsigned XSLT_Functions::UnparsedEntityUri::GetResultType(unsigned *arguments, unsigned arguments_count)
{
  if (arguments_count != 1)
    return XPathFunction::TYPE_INVALID;
  else
    return XPathFunction::TYPE_STRING;
}

/* virtual */
unsigned XSLT_Functions::UnparsedEntityUri::GetFlags()
{
  return 0;
}

/* virtual */
XPathFunction::Result XSLT_Functions::UnparsedEntityUri::Call(XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state)
{
  const uni_char *name, *uri = UNI_L ("");

  RETURN_UNLESS_FINISHED (XSLT_EvaluateToString (context->arguments[0], name, extensions_context));

  XMLTreeAccessor *tree = context->context_node->GetTreeAccessor ();

  const XMLDocumentInformation *docinfo = tree->GetDocumentInformation ();
  if (docinfo)
    if (XMLDoctype *doctype = docinfo->GetDoctype ())
      if (XMLDoctype::Entity *entity = doctype->GetEntity (XMLDoctype::Entity::TYPE_General, name))
        if (entity->GetValueType () == XMLDoctype::Entity::VALUE_TYPE_External_NDataDecl)
          {
            URL baseurl (entity->GetBaseURL ());
            URL url = g_url_api->GetURL (baseurl, entity->GetSystem ());
            uri = url.GetAttribute (URL::KUniName).CStr ();
          }

  RETURN_IF_ERRORX (result.SetString (uri));
  return RESULT_FINISHED;
}

static BOOL
XSLT_IsSpecialCharacter (unsigned ch, XSLT_DecimalFormatData *df)
{
  if (ch == df->digit || ch == df->zero_digit || ch == df->grouping_separator || ch == df->decimal_separator || ch == df->pattern_separator)
    return TRUE;
  else
    return FALSE;
}

static OP_STATUS
XSLT_ParsePrefixOrSuffix (const uni_char *format, TempBuffer &prefixbuffer, TempBuffer &suffixbuffer, BOOL &is_percent, BOOL &is_per_mille, XSLT_DecimalFormatData *df)
{
  TempBuffer *buffer = &prefixbuffer;

  while (TRUE)
    {
      while (*format && !XSLT_IsSpecialCharacter (*format, df) && *format != df->pattern_separator)
        {
          unsigned ch = *format++;

          if (!is_percent && !is_per_mille)
            if (ch == df->percent)
              is_percent = TRUE;
            else if (ch == df->per_mille)
              is_per_mille = TRUE;

          if (ch == '\'')
            if (*format == '\'')
              {
                RETURN_IF_ERROR (buffer->Append((uni_char) ch));
                ++format;
              }
            else
              {
                while (*format && *format != '\'')
                  RETURN_IF_ERROR (buffer->Append(*format++));
                if (*format == '\'')
                  ++format;
              }
          else
            RETURN_IF_ERROR (buffer->Append((uni_char) ch));
        }

      if (buffer == &suffixbuffer)
        break;

      buffer = &suffixbuffer;

      while (*format && XSLT_IsSpecialCharacter (*format, df) && *format != '\'' && *format != df->pattern_separator)
        ++format;
    }

  return OpStatus::OK;
}

static OP_STATUS
XSLT_ParseFormat (const uni_char *format, TempBuffer &prefixbuffer, TempBuffer &suffixbuffer, unsigned &digitsmax, unsigned &digitsmin, unsigned &fdigitsmax, unsigned &fdigitsmin, unsigned &groupingoffset, BOOL &is_negative, BOOL &is_percent, BOOL &is_per_mille, XSLT_DecimalFormatData *df)
{
  const uni_char *prefix_suffix_format = format;

  if (is_negative)
    {
      const uni_char *pattern_separator = uni_strchr (format, df->pattern_separator);

      if (pattern_separator)
        {
          prefix_suffix_format = pattern_separator + 1;
          is_negative = FALSE;
        }
    }

  RETURN_IF_ERROR (XSLT_ParsePrefixOrSuffix (prefix_suffix_format, prefixbuffer, suffixbuffer, is_percent, is_per_mille, df));

  while (*format && !XSLT_IsSpecialCharacter (*format, df) && *format != df->pattern_separator)
    ++format;

  BOOL grouping_separator_seen = FALSE;

  digitsmax = 0;
  groupingoffset = static_cast<unsigned>(-1);

  while (*format == df->digit || *format == df->grouping_separator)
    {
      if (*format == df->digit)
        ++digitsmax, ++groupingoffset;
      else
        {
          groupingoffset = 0;
          grouping_separator_seen = TRUE;
        }

      ++format;
    }

  digitsmin = 0;

  while (*format == df->zero_digit || *format == df->grouping_separator)
    {
      if (*format == df->zero_digit)
        ++digitsmin, ++groupingoffset;
      else
        {
          groupingoffset = 0;
          grouping_separator_seen = TRUE;
        }

      ++format;
    }

  digitsmax += digitsmin;

  if (!grouping_separator_seen)
    groupingoffset = 0;

  fdigitsmax = 0;
  fdigitsmin = 0;

  if (*format == df->decimal_separator)
    {
      ++format;

      while (*format == df->zero_digit)
        ++format, ++fdigitsmin;

      while (*format == df->digit)
        ++format, ++fdigitsmax;

      fdigitsmax += fdigitsmin;
    }

  return OpStatus::OK;
}

static void
XSLT_ConvertDigits (uni_char *digits, unsigned length, unsigned zero_digit)
{
  for (unsigned index = 0; index < length; ++index)
    digits[index] = digits[index] - '0' + zero_digit;
}

XSLT_Functions::FormatNumber::FormatNumber (XSLT_StylesheetImpl *stylesheet)
  : stylesheet (stylesheet)
{
}


/* virtual */ unsigned
XSLT_Functions::FormatNumber::GetResultType (unsigned *arguments, unsigned arguments_count)
{
  if (arguments_count != 2 && arguments_count != 3)
    return XPathFunction::TYPE_INVALID;
  else
    return XPathFunction::TYPE_STRING;
}

/* virtual */ unsigned
XSLT_Functions::FormatNumber::GetFlags ()
{
  return 0;
}

/* virtual */
XPathFunction::Result XSLT_Functions::FormatNumber::Call(XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state)
{
  if (context->arguments_count != 2 && context->arguments_count != 3)
    return RESULT_FAILED;

  XSLT_DecimalFormatData *df;
  XSLT_DecimalFormatData temporary;

  if (context->arguments_count == 3)
    {
      const uni_char* qname;

      RETURN_UNLESS_FINISHED (XSLT_EvaluateToString (context->arguments[2], qname, extensions_context));

      unsigned qname_length = uni_strlen (qname);

      if (qname_length == 0)
        return RESULT_FAILED;

      XMLCompleteNameN completename (qname, qname_length);

      if (!XMLNamespaceDeclaration::ResolveName (nsdeclaration, completename, FALSE))
        return RESULT_FAILED;

      XMLExpandedName name;

      RETURN_IF_ERRORX (name.Set (completename));

      df = stylesheet->FindDecimalFormat (name);
      if (!df)
        return RESULT_FAILED;
    }
  else
    {
      df = stylesheet->FindDefaultDecimalFormat ();

      if (!df)
        {
          temporary.SetDefaultsL ();
          df = &temporary;
        }
    }

  double numbervalue;

  RETURN_UNLESS_FINISHED (XSLT_EvaluateToNumber (context->arguments[0], numbervalue, extensions_context));

  BOOL is_nan = op_isnan (numbervalue) != 0;
  BOOL is_infinity = !is_nan && !op_isfinite (numbervalue);
  BOOL is_negative = !is_nan && op_signbit (numbervalue) == 1;
  BOOL is_percent = FALSE;
  BOOL is_per_mille = FALSE;

  TempBuffer prefixbuffer, suffixbuffer;
  unsigned digitsmax, digitsmin, fdigitsmax, fdigitsmin, groupingoffset;

  const uni_char *format;

  RETURN_UNLESS_FINISHED (XSLT_EvaluateToString (context->arguments[1], format, extensions_context));

  RETURN_IF_ERRORX (XSLT_ParseFormat (format, prefixbuffer, suffixbuffer, digitsmax, digitsmin, fdigitsmax, fdigitsmin, groupingoffset, is_negative, is_percent, is_per_mille, df));

  if (!is_nan && !is_infinity)
    if (is_percent)
      numbervalue *= 100.;
    else if (is_per_mille)
      numbervalue *= 1000.;

  TempBuffer numberbuffer;

  RETURN_IF_ERRORX (XSLT_Utils::ConvertToString (numberbuffer, numbervalue, fdigitsmax));

  const uni_char *number = numberbuffer.GetStorage ();

  if (number[0] == '-')
    ++number;

  TempBuffer resultbuffer;

  /* Just to ensure memory is at all allocated.  '16' is not significant. */
  RETURN_IF_ERRORX(resultbuffer.Expand (16));

  if (is_negative)
    resultbuffer.Append ((uni_char) df->minus_sign);

  if (prefixbuffer.Length () != 0)
    RETURN_IF_ERRORX (resultbuffer.Append (prefixbuffer.GetStorage ()));

  if (is_nan)
    RETURN_IF_ERRORX (resultbuffer.Append (df->nan));
  else if (is_infinity)
    RETURN_IF_ERRORX (resultbuffer.Append (df->infinity));
  else
    {
      const uni_char *integer0 = number, *fraction0 = uni_strchr (number, '.');
      unsigned integerlength, fractionlength;
      TempBuffer integerbuffer, fractionbuffer;

      /* Just to ensure memory is at all allocated.  '16' is not significant. */
      RETURN_IF_ERRORX(integerbuffer.Expand (16));
      RETURN_IF_ERRORX(fractionbuffer.Expand (16));

      if (fraction0)
        {
          integerlength = fraction0 - integer0;
          fractionlength = uni_strlen (++fraction0);
        }
      else
        {
          integerlength = uni_strlen (integer0);
          fractionlength = 0;
        }

      while (integerlength != 0 && *integer0 == '0')
        --integerlength, ++integer0;

      RETURN_IF_ERRORX (integerbuffer.Append (integer0, integerlength));
      RETURN_IF_ERRORX (fractionbuffer.Append (fraction0, fractionlength));

      unsigned fraction_offset = 0;

      while (fdigitsmin > fractionlength)
        {
          RETURN_IF_ERRORX (fractionbuffer.Append ('0'));
          ++fractionlength;
        }

      if (digitsmin > integerlength)
        {
          integerbuffer.Expand (digitsmin + 1);

          uni_char *integer = integerbuffer.GetStorage ();

          op_memmove (integer + digitsmin - integerlength, integer, integerlength * sizeof integer[0]);
          op_memset (integer, 0, (digitsmin - integerlength) * sizeof integer[0]);

          unsigned index = 0;

          while (integer[index] == 0)
            integer[index++] = '0';

          integerlength = digitsmin;
        }

      uni_char *integer = integerbuffer.GetStorage (), *fraction = fractionbuffer.GetStorage () + fraction_offset;

      while (fdigitsmin < fractionlength && fraction[fractionlength - 1] == '0')
        --fractionlength;

      if (integerlength == 0 && fractionlength == 0)
        integerlength = 1, *integer = '0';

      XSLT_ConvertDigits (integer, integerlength, df->zero_digit);
      XSLT_ConvertDigits (fraction, fractionlength, df->zero_digit);

      if (groupingoffset == 0 || integerlength <= groupingoffset)
        RETURN_IF_ERRORX (resultbuffer.Append (integer, integerlength));
      else
        while (integerlength > 0)
          {
            unsigned length = integerlength % groupingoffset;

            if (length == 0)
              length = groupingoffset;

            RETURN_IF_ERRORX (resultbuffer.Append (integer, length));

            integer += length;
            integerlength -= length;

            if (integerlength != 0)
              RETURN_IF_ERRORX (resultbuffer.Append ((uni_char) df->grouping_separator));
          }

      if (fractionlength != 0)
        {
          RETURN_IF_ERRORX (resultbuffer.Append ((uni_char) df->decimal_separator));
          RETURN_IF_ERRORX (resultbuffer.Append (fraction, fractionlength));
        }
    }

  if (suffixbuffer.Length () != 0)
    RETURN_IF_ERRORX (resultbuffer.Append (suffixbuffer.GetStorage ()));

  RETURN_IF_ERRORX (result.SetString (resultbuffer.GetStorage ()));

  return RESULT_FINISHED;
}

/* virtual */ unsigned
XSLT_Functions::NodeSet::GetResultType (unsigned *arguments, unsigned arguments_count)
{
  return XPathFunction::TYPE_NODESET;
}

/* virtual */ unsigned
XSLT_Functions::NodeSet::GetFlags()
{
  return 0;
}

/* virtual */ XPathFunction::Result
XSLT_Functions::NodeSet::Call (XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state)
{
  if (context->arguments_count != 1)
    return RESULT_FAILED;

  RETURN_IF_ERRORX (result.SetNodeSet (FALSE, TRUE));

  context->arguments[0]->SetRequestedType (XPathExpression::Evaluate::NODESET_ITERATOR);

  OP_BOOLEAN finished;
  unsigned type;

  RETURN_IF_ERRORX (finished = context->arguments[0]->CheckResultType (type));

  if ((type & XPathExpression::Evaluate::NODESET_ITERATOR) == 0)
    return RESULT_FAILED;

  while (TRUE)
    {
      XPathNode *node;

      RETURN_IF_ERRORX (finished = context->arguments[0]->GetNextNode (node));

      if (!node)
        break;

      XPathValue::AddNodeStatus addnodestatus;
      RETURN_IF_ERRORX (result.AddNode (node, addnodestatus));

      if (addnodestatus == XPathValue::ADDNODE_STOP)
        break;
      else if (addnodestatus == XPathValue::ADDNODE_PAUSE)
        return RESULT_PAUSED;
    }

  return RESULT_FINISHED;
}

/* virtual */ unsigned
XSLT_Functions::Document::GetResultType (unsigned *arguments, unsigned arguments_count)
{
  if (arguments_count < 1 || arguments_count > 2)
    return XPathFunction::TYPE_INVALID;
  else
    return XPathFunction::TYPE_NODESET;
}

/* virtual */ unsigned
XSLT_Functions::Document::GetFlags ()
{
  return FLAG_BLOCKING;
}

static URL
XSLT_CalculateBaseURLFromNode (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode)
{
  while (treenode)
    {
      if (tree->GetNodeType (treenode) == XMLTreeAccessor::TYPE_ELEMENT)
        {
          TempBuffer buffer;
          const uni_char *value;
          BOOL id, specified;

          XMLTreeAccessor::Attributes *attributes;
          tree->GetAttributes (attributes, treenode, FALSE, TRUE);
          if (OpStatus::IsSuccess (tree->GetAttribute (attributes, XMLExpandedName (UNI_L ("http://www.w3.org/XML/1998/namespace"), UNI_L ("base")), value, id, specified, &buffer)))
            {
              URL baseurl (XSLT_CalculateBaseURLFromNode (tree, tree->GetParent (treenode)));
              return g_url_api->GetURL (baseurl, value);
            }
        }

      treenode = tree->GetParent (treenode);
    }

  return tree->GetDocumentURL ();
}

static URL
XSLT_CalculateBaseURLFromNode (XPathNode *node)
{
  return XSLT_CalculateBaseURLFromNode (node->GetTreeAccessor (), node->GetNode ());
}

/* virtual */ XPathFunction::Result
XSLT_Functions::Document::Call (XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state)
{
  if (context->arguments_count != 1 && context->arguments_count != 2)
    return RESULT_FAILED;

  if (!state)
    {
      state = OP_NEW (DocumentState, (baseurl));
      if (!state)
        return RESULT_OOM;
    }

  DocumentState *document_state = static_cast<DocumentState *> (state);

  if (document_state->state == DocumentState::INITIAL)
    {
      RETURN_IF_ERRORX (result.SetNodeSet (FALSE, TRUE));
      if (context->arguments_count == 2)
        document_state->state = DocumentState::SETUP_ARGUMENT2;
      else
        document_state->state = DocumentState::SETUP_ARGUMENT1;
    }

  if (document_state->state == DocumentState::SETUP_ARGUMENT2)
    {
      context->arguments[1]->SetRequestedType (XPathExpression::Evaluate::NODESET_SINGLE | XPathExpression::Evaluate::NODESET_FLAG_ORDERED);
      document_state->state = DocumentState::PROCESS_ARGUMENT2;
    }

  if (document_state->state == DocumentState::PROCESS_ARGUMENT2)
    {
      XPathNode *node;
      OP_BOOLEAN finished;

      RETURN_IF_ERRORX (finished = context->arguments[1]->GetSingleNode (node));

      if (node)
        document_state->baseurl = XSLT_CalculateBaseURLFromNode (node);

      document_state->state = DocumentState::SETUP_ARGUMENT1;
    }

  if (document_state->state == DocumentState::SETUP_ARGUMENT1)
    {
      context->arguments[0]->SetRequestedType (XPathExpression::Evaluate::PRIMITIVE_STRING, XPathExpression::Evaluate::PRIMITIVE_STRING, XPathExpression::Evaluate::PRIMITIVE_STRING, XPathExpression::Evaluate::NODESET_ITERATOR);
      document_state->state = DocumentState::PROCESS_ARGUMENT1;
    }

  if (document_state->state == DocumentState::PROCESS_ARGUMENT1)
    {
      unsigned type;
      OP_BOOLEAN finished;

      RETURN_IF_ERRORX (finished = context->arguments[0]->CheckResultType (type));

      if (type == XPathExpression::Evaluate::PRIMITIVE_STRING)
        {
          const uni_char *string;

          RETURN_IF_ERRORX (finished = context->arguments[0]->GetStringResult (string));

          document_state->url = g_url_api->GetURL (document_state->baseurl, string);
          document_state->state = DocumentState::LOAD_TREE_FROM_STRING;
        }
      else
        document_state->state = DocumentState::LOAD_TREES_FROM_NODE;
    }

  do
    {
      if (document_state->state == DocumentState::LOAD_TREES_FROM_NODE && document_state->url.IsEmpty ())
        {
          XPathNode *node;
          OP_BOOLEAN finished;

          RETURN_IF_ERRORX (finished = context->arguments[0]->GetNextNode (node));

          if (!node)
            return RESULT_FINISHED;

          TempBuffer buffer;

          RETURN_IF_ERRORX (node->GetStringValue (buffer));

          document_state->url = g_url_api->GetURL (document_state->baseurl, buffer.GetStorage () ? buffer.GetStorage () : UNI_L (""));
        }

      XSLT_Tree *tree = XSLT_Engine::GetTransformation (extensions_context)->GetTreeByURL (document_state->url);

      if (!tree)
        {
          OP_BOOLEAN is_loading = XSLT_Engine::GetTransformation (extensions_context)->LoadTree (document_state->url);

          if (is_loading == OpBoolean::IS_TRUE)
            {
              XSLT_Engine::SetBlocked (extensions_context);
              return RESULT_BLOCKED;
            }
          else if (is_loading == OpBoolean::IS_FALSE)
            return RESULT_FINISHED;
          else if (OpStatus::IsMemoryError (is_loading))
            return RESULT_OOM;

          document_state->url = URL ();
        }
      else
        {
          XMLTreeAccessor::Node *treenode;
          const uni_char *fragment_id = document_state->url.UniRelName ();

          if (fragment_id)
            RETURN_IF_ERRORX (tree->GetElementById (treenode, fragment_id));
          else
            treenode = tree->GetRoot ();

          document_state->url = URL ();

          if (treenode)
            {
              XPathNode *node;
              RETURN_IF_ERRORX (XPathNode::Make (node, tree, treenode));

              XPathValue::AddNodeStatus addnodestatus;
              RETURN_IF_ERRORX (result.AddNode (node, addnodestatus));

              return document_state->state == DocumentState::LOAD_TREES_FROM_NODE ? RESULT_PAUSED : RESULT_FINISHED;
            }
        }
    }
  while (document_state->state == DocumentState::LOAD_TREES_FROM_NODE);

  return RESULT_FINISHED;
}

#endif // XSLT_SUPPORT
