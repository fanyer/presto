/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_utils.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_template.h"
#include "modules/xslt/src/xslt_lexer.h"
#include "modules/xslt/src/xslt_literalresultelement.h"
#include "modules/xslt/src/xslt_attributeset.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xpath/xpath.h"

XSLT_Import::XSLT_Import (unsigned import_precedence, const URL &url, XSLT_ElementType type, XSLT_Import *parent)
  : type (type),
    version (XSLT_VERSION_NONE),
    parent (parent),
    root_element (0),
    current_element (0),
    import_precedence (import_precedence),
    import_allowed (TRUE),
    xmltokenhandler (0),
    source_callback (0),
    url (url)
{
}

XSLT_Import::~XSLT_Import ()
{
  OP_DELETE (xmltokenhandler);
}

XSLT_Import *
XSLT_Import::GetNonInclude ()
{
  XSLT_Import *import = this;
  while (import && import->type == XSLTE_INCLUDE)
    import = import->parent;
  return import;
}

XSLT_XPathExpressionOrPattern::XSLT_XPathExpressionOrPattern (XPathExtensions* extensions, XMLNamespaceDeclaration *nsdeclaration)
  : nsdeclaration (nsdeclaration),
    extensions (extensions)
{
}

XPathNamespaces *
XSLT_XPathExpressionOrPattern::MakeNamespacesL ()
{
  XPathNamespaces *namespaces;

  LEAVE_IF_ERROR (XPathNamespaces::Make (namespaces, GetSource ()));

  for (unsigned index = 0, count = namespaces->GetCount (); index < count; ++index)
    {
      const uni_char *uri = XMLNamespaceDeclaration::FindUri (nsdeclaration, namespaces->GetPrefix (index));
      if (uri && OpStatus::IsMemoryError (namespaces->SetURI (index, uri)))
        {
          XPathNamespaces::Free (namespaces);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }
    }

  return namespaces;
}

void
XSLT_XPathExpressionOrPattern::SetSourceL (XSLT_StylesheetParserImpl *parser, const XMLCompleteNameN &attributename, const uni_char *new_source, unsigned new_source_length)
{
  parser->SetStringL (source, attributename, new_source, new_source_length);

#ifdef XML_ERRORS
//  location = parser->GetCurrentLocation ();
#endif // XML_ERRORS
}

void
XSLT_XPathExpressionOrPattern::SetNamespaceDeclaration (XSLT_StylesheetParserImpl *parser)
{
  nsdeclaration = parser->GetNamespaceDeclaration ();
}

XPathExpression *
XSLT_XPathExpression::GetExpressionL (XSLT_MessageHandler *messagehandler)
{
  if (!expression)
    {
      XPathExpression::ExpressionData data;
      data.source = GetSource ();
      data.namespaces = MakeNamespacesL ();
      data.extensions = extensions;

#ifdef XSLT_ERRORS
      OpString error_message; ANCHOR (OpString, error_message);
      data.error_message = &error_message;
#endif // XSLT_ERRORS

      OP_STATUS status = XPathExpression::Make (expression, data);

      XPathNamespaces::Free (data.namespaces);

#ifdef XSLT_ERRORS
      if (status == OpStatus::ERR)
        {
          LEAVE_IF_ERROR (error_message.Insert (0, "\nDetails: "));
          LEAVE_IF_ERROR (error_message.Insert (0, GetSource ()));
          LEAVE_IF_ERROR (error_message.Insert (0, "XPath expression compilation failed: "));
          source.SignalErrorL (messagehandler, error_message.CStr ());
        }
      else if (status == OpStatus::ERR_NO_MEMORY)
        LEAVE (status);
#else // XSLT_ERRORS
      LEAVE_IF_ERROR(status);
#endif // XSLT_ERRORS
    }

  return expression;
}

XSLT_XPathExpression::XSLT_XPathExpression (XPathExtensions* extensions, XMLNamespaceDeclaration *nsdeclaration)
  : XSLT_XPathExpressionOrPattern(extensions, nsdeclaration),
  expression (0)
{
}

XSLT_XPathExpression::~XSLT_XPathExpression ()
{
  XPathExpression::Free (expression);
}

void
XSLT_XPathPattern::Reset ()
{
  OP_DELETEA (pattern_sources);
  for (unsigned index = 0; index < patterns_count; ++index)
    XPathPattern::Free (pattern_objects[index]);
  OP_DELETEA (pattern_objects);

  pattern_sources = 0;
  pattern_objects = 0;
  patterns_count = 0;
}

XSLT_XPathPattern::XSLT_XPathPattern (XPathExtensions* extensions, XMLNamespaceDeclaration *nsdeclaration)
  : XSLT_XPathExpressionOrPattern(extensions, nsdeclaration),
    pattern_sources (0),
    pattern_objects (0),
    patterns_count (0),
    patterns_disabled_count (0)
{
}

XSLT_XPathPattern::~XSLT_XPathPattern ()
{
  Reset ();
}

void
XSLT_XPathPattern::PreprocessL (XSLT_MessageHandler *messagehandler, XPathExtensions *extensions)
{
  if (source.IsSpecified() && !pattern_objects)
    {
      OP_BOOLEAN result = OpBoolean::IS_FALSE;

      const uni_char *pattern;
      unsigned pattern_length;

      unsigned count = 0;

      const uni_char *string = source.GetString();
      while ((result = XPathPattern::GetNextAlternative (pattern, pattern_length, string)) == OpBoolean::IS_TRUE)
        ++count;

      if (OpStatus::IsMemoryError (result))
        LEAVE (result);
      else
        {
          if (result == OpBoolean::IS_FALSE && count == 0)
            source.SignalErrorL (messagehandler, "empty pattern");

          if (result == OpStatus::ERR)
            /* Let XPathPattern::Make give us a detailed error message for the
               pattern alternative that was invalid. */
            ++count;

          XPathNamespaces *namespaces = MakeNamespacesL ();

          pattern_sources = OP_NEWA (OpString, count);
          pattern_objects = OP_NEWA (XPathPattern *, count);

          OP_STATUS status = OpStatus::OK;
#ifdef XSLT_ERRORS
          OpString error_message, *failed_source = 0; ANCHOR (OpString, error_message);
#endif // XSLT_ERRORS

          if (pattern_sources && pattern_objects)
            {
              const uni_char *string = source.GetString();
              while ((result = XPathPattern::GetNextAlternative (pattern, pattern_length, string)) == OpBoolean::IS_TRUE)
                if (OpStatus::IsSuccess (status = pattern_sources[patterns_count].Set (pattern, pattern_length)))
                  {
                    XPathPattern::PatternData data;
                    data.source = pattern_sources[patterns_count].CStr ();
                    data.namespaces = namespaces;
                    data.extensions = extensions;
#ifdef XSLT_ERRORS
                    data.error_message = &error_message;
#endif // XSLT_ERRORS
                    if (OpStatus::IsError (status = XPathPattern::Make (pattern_objects[patterns_count], data)))
                      {
#ifdef XSLT_ERRORS
                        failed_source = &pattern_sources[patterns_count];
#endif // XSLT_ERRORS
                        break;
                      }
                    else
                      ++patterns_count;
                  }
            }

          XPathNamespaces::Free (namespaces);

          if (!pattern_sources || !pattern_objects || OpStatus::IsMemoryError (status))
            {
              Reset ();
              LEAVE (OpStatus::ERR_NO_MEMORY);
            }
          else if (status == OpStatus::ERR)
#ifdef XSLT_ERRORS
            if (!error_message.IsEmpty())
              {
                LEAVE_IF_ERROR (error_message.Insert (0, "\nDetails: "));
                if (failed_source)
                  LEAVE_IF_ERROR (error_message.Insert (0, failed_source->CStr ()));
                LEAVE_IF_ERROR (error_message.Insert (0, "invalid pattern: "));
                source.SignalErrorL (messagehandler, error_message.CStr ());
              }
            else
#endif // XSLT_ERRORS
              source.SignalErrorL (messagehandler, "invalid pattern");
        }
    }
}

void
XSLT_XPathPattern::DisablePattern (unsigned index)
{
  XPathPattern::Free (pattern_objects[index]);
  pattern_objects[index] = 0;
  ++patterns_disabled_count;
}

XSLT_DecimalFormatData::XSLT_DecimalFormatData ()
  : next (0),
    infinity (0),
    nan (0)
{
}

XSLT_DecimalFormatData::~XSLT_DecimalFormatData ()
{
  OP_DELETEA (infinity);
  OP_DELETEA (nan);
  OP_DELETE (next);
}

void
XSLT_DecimalFormatData::SetDefaultsL ()
{
  SetStrL (infinity, "Infinity");
  SetStrL (nan, "NaN");

  decimal_separator = '.';
  grouping_separator = ',';
  minus_sign = '-';
  percent = '%';
  per_mille = 0x2030;
  zero_digit = '0';
  digit = '#';
  pattern_separator = ';';
}

/* static */ XSLT_DecimalFormatData *
XSLT_DecimalFormatData::PushL (XSLT_DecimalFormatData *&existing)
{
  XSLT_DecimalFormatData *df = OP_NEW_L (XSLT_DecimalFormatData, ()), **ptr = &existing;

  while (*ptr)
    ptr = &(*ptr)->next;

  return *ptr = df;
}

/* static */ XSLT_DecimalFormatData *
XSLT_DecimalFormatData::Find (XSLT_DecimalFormatData *existing, const XMLExpandedName &name)
{
  while (existing)
    if (existing->name == name)
      return existing;
    else
      existing = existing->next;

  return 0;
}

/* static */ XSLT_DecimalFormatData *
XSLT_DecimalFormatData::FindDefault (XSLT_DecimalFormatData *existing)
{
  while (existing)
    if (!existing->name.GetLocalPart ())
      return existing;
    else
      existing = existing->next;

  return 0;
}

BOOL
XSLT_CompareStrings (const uni_char *string1, unsigned string1_length, const char *string2)
{
  if (string1_length == op_strlen (string2))
    return uni_strncmp (string1, string2, string1_length) == 0;
  else
    return FALSE;
}

unsigned
XSLT_GetLength (const uni_char *value, unsigned value_length)
{
  if (value_length == ~0u)
    return uni_strlen (value);
  else
    return value_length;
}

BOOL
XSLT_GetPrefix (const uni_char *qname, const uni_char *&prefix, unsigned &prefix_length)
{
  const uni_char *colon = uni_strchr (qname, ':');

  if (colon)
    {
      prefix = qname;
      prefix_length = colon - qname;
      return TRUE;
    }
  else
    return FALSE;
}

#ifdef XSLT_ERRORS

static void
XSLT_AppendQName (OpString &string, const XMLCompleteName &name)
{
  if (name.GetPrefix ())
    {
      string.AppendL (name.GetPrefix ());
      string.AppendL (":");
    }

  string.AppendL (name.GetLocalPart ());
}

static void
XSLT_AppendQNameAttribute (OpString &string, const char *name, const XMLCompleteName &value)
{
  string.AppendL ("[@");
  string.AppendL (name);
  string.AppendL ("='");
  XSLT_AppendQName (string, value);
  string.AppendL ("']");
}

void
XSLT_GetElementContextL (OpString &context, URL &import_url, XSLT_Element *element)
{
  while (element)
    {
      OpString local;

      XSLT_ElementType type = element->GetType ();

      if (type == XSLTE_LITERAL_RESULT_TEXT_NODE)
        local.AppendL ("text()");
      else if (type == XSLTE_OTHER)
        XSLT_AppendQName (local, ((XSLT_LiteralResultElement *) element)->GetName ());
      else
        {
          local.AppendL ("xsl:");
          local.AppendL (XSLT_Lexer::GetElementName (type));
        }

      if (element->GetType () == XSLTE_TEMPLATE)
        {
          XSLT_Template *tmplate = (XSLT_Template *) element;

          import_url = tmplate->GetImportURL ();

          if (tmplate->HasName ())
            XSLT_AppendQNameAttribute (local, "name", tmplate->GetName ());
          else
            {
              if (tmplate->GetMatch ().IsSpecified ())
                {
                  local.AppendL ("[@match='");
                  local.AppendL (tmplate->GetMatch ().GetSource ());
                  local.AppendL ("']");
                }

              if (tmplate->HasMode ())
                XSLT_AppendQNameAttribute (local, "mode", tmplate->GetMode ());
            }
        }
      else if (element->GetType () == XSLTE_ATTRIBUTE_SET)
        XSLT_AppendQNameAttribute (local, "name", ((XSLT_AttributeSet *) element)->GetName ());

      if (!context.IsEmpty ())
        local.AppendL ("/");

      LEAVE_IF_ERROR (context.Insert (0, local));

      element = element->GetParent ();
    }
}

#ifndef XML_ERRORS
typedef void XSLT_Location;
#endif // XML_ERRORS

#ifdef OPERA_CONSOLE

void
XSLT_InitializeConsoleMessageL (OpConsoleEngine::Message &message, XSLT_Element *element, const XSLT_Location *location, const uni_char *context = 0, URL *url = 0)
{
  URL import_url; ANCHOR(URL, import_url);

  if (context)
    message.context.SetL (context);
#ifdef XML_ERRORS
  else if (location && location->type != XSLT_Location::TYPE_NO_RANGE && location->range.start.IsValid ())
    {
      import_url = location->import_url;

      OP_ASSERT (location->range.start.IsValid ());

      OP_STATUS status;

      const uni_char *item;

      switch (location->type)
        {
        case XSLT_Location::TYPE_START_TAG:
          item = UNI_L ("start-tag");
          break;
        case XSLT_Location::TYPE_END_TAG:
          item = UNI_L ("end-tag");
          break;
        case XSLT_Location::TYPE_ATTRIBUTE:
          item = UNI_L ("attribute");
          break;
        case XSLT_Location::TYPE_PROCESSING_INSTRUCTION:
          item = UNI_L ("processing instruction");
          break;
        default:
          item = UNI_L ("character data");
        }

      if (location->range.end.IsValid () && (location->range.start.line != location->range.end.line || location->range.end.column - location->range.start.column < 2))
        if (location->range.start.line == location->range.end.line)
          status = message.context.AppendFormat (UNI_L ("%s at line %d, column %d-%d"), item, location->range.start.line + 1, location->range.start.column, location->range.end.column - 1);
        else
          status = message.context.AppendFormat (UNI_L ("%s at line %d, column %d to line %d, column %d"), item, location->range.start.line + 1, location->range.start.column, location->range.end.line + 1, location->range.end.column - 1);
      else
        status = message.context.AppendFormat (UNI_L ("%s at line %d, column %d"), item, location->range.start.line + 1, location->range.start.column);

      LEAVE_IF_ERROR (status);
    }
  else
#endif // XML_ERRORS
    if (element)
      XSLT_GetElementContextL (message.context, import_url, element);
    else
      message.context.SetL ("global");

  if (url)
    import_url = *url;

  if (!import_url.IsEmpty ())
    import_url.GetAttributeL (URL::KUniName_Username_Password_Hidden, message.url);
}

#endif // OPERA_CONSOLE

static void
XSLT_PostConsoleMessageL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const XSLT_Location *location, const char *reason8, const uni_char *reason16, XSLT_MessageHandler::MessageType type, const char *title = 0, const uni_char *context = 0, URL *url = 0)
{
#ifdef OPERA_CONSOLE
  OpConsoleEngine::Message console_message (OpConsoleEngine::XSLT, type == XSLT_MessageHandler::MESSAGE_TYPE_ERROR ? OpConsoleEngine::Error : OpConsoleEngine::Information); ANCHOR (OpConsoleEngine::Message, console_message);
  OpString &message = console_message.message;
#else // OPERA_CONSOLE
  OpString message; ANCHOR (OpString, message);
#endif // OPERA_CONSOLE

  message.SetL (title ? title : "Error: ");

  if (reason8)
    message.AppendL (reason8);
  else
    message.AppendL (reason16);

  OP_BOOLEAN handled = messagehandler->HandleMessage (type, message.CStr ());

  if (handled == OpStatus::ERR_NO_MEMORY)
    LEAVE (handled);
#ifdef OPERA_CONSOLE
  else if (handled == OpBoolean::IS_FALSE && g_console->IsLogging())
    {
      XSLT_InitializeConsoleMessageL (console_message, element, location, context, url);
      g_console->PostMessageL (&console_message);
      console_message.Clear();
    }
#endif // OPERA_CONSOLE
}

#ifdef XML_ERRORS

void
XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const char *reason, const XSLT_Location *location)
{
  XSLT_PostConsoleMessageL (messagehandler, element, location, reason, 0, XSLT_MessageHandler::MESSAGE_TYPE_ERROR);
  LEAVE (OpStatus::ERR);
}

void
XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *reason, const XSLT_Location *location)
{
  XSLT_PostConsoleMessageL (messagehandler, element, location, 0, reason, XSLT_MessageHandler::MESSAGE_TYPE_ERROR);
  LEAVE (OpStatus::ERR);
}

void
XSLT_SignalWarningL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const char *reason, const XSLT_Location *location)
{
  XSLT_PostConsoleMessageL (messagehandler, element, location, reason, 0, XSLT_MessageHandler::MESSAGE_TYPE_WARNING, "Warning: ");
}

void
XSLT_SignalWarningL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *warning, const XSLT_Location *location)
{
  XSLT_PostConsoleMessageL (messagehandler, element, location, 0, warning, XSLT_MessageHandler::MESSAGE_TYPE_WARNING, "Warning: ");
}

void
XSLT_SignalMessageL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *message, BOOL terminate, const XSLT_Location *location)
{
  XSLT_PostConsoleMessageL (messagehandler, element, location, 0, message, terminate ? XSLT_MessageHandler::MESSAGE_TYPE_ERROR : XSLT_MessageHandler::MESSAGE_TYPE_MESSAGE, terminate ? "Terminated: " : "Message: ");
  if (terminate)
    LEAVE(OpStatus::ERR);
}

#else // XML_ERRORS

void
XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const char *reason)
{
  XSLT_PostConsoleMessageL (messagehandler, element, 0, reason, 0, XSLT_MessageHandler::MESSAGE_TYPE_ERROR);
  LEAVE (OpStatus::ERR);
}

void
XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *reason)
{
  XSLT_PostConsoleMessageL (messagehandler, element, 0, 0, reason, XSLT_MessageHandler::MESSAGE_TYPE_ERROR);
  LEAVE (OpStatus::ERR);
}

void
XSLT_SignalWarningL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const char *reason)
{
  XSLT_PostConsoleMessageL (messagehandler, element, 0, reason, 0, XSLT_MessageHandler::MESSAGE_TYPE_WARNING, "Warning: ");
}

void
XSLT_SignalWarningL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *warning)
{
  XSLT_PostConsoleMessageL (messagehandler, element, 0, 0, warning, XSLT_MessageHandler::MESSAGE_TYPE_WARNING, "Warning: ");
}

void
XSLT_SignalMessageL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *warning, BOOL terminate)
{
  XSLT_PostConsoleMessageL (messagehandler, 0, 0, 0, warning, terminate ? XSLT_MessageHandler::MESSAGE_TYPE_ERROR : XSLT_MessageHandler::MESSAGE_TYPE_MESSAGE, terminate ? "Terminated: " : "Message: ");
}

#endif // XML_ERRORS

void
XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, const char *reason, const uni_char *context, URL *url)
{
  XSLT_PostConsoleMessageL (messagehandler, 0, 0, reason, 0, XSLT_MessageHandler::MESSAGE_TYPE_ERROR, 0, context, url);
  LEAVE (OpStatus::ERR);
}

void
XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, const uni_char *reason, const uni_char *context, URL *url)
{
  XSLT_PostConsoleMessageL (messagehandler, 0, 0, 0, reason, XSLT_MessageHandler::MESSAGE_TYPE_ERROR, 0, context, url);
  LEAVE (OpStatus::ERR);
}

#else // XSLT_ERRORS

void
XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const char *reason)
{
  LEAVE (OpStatus::ERR);
}

void
XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *reason)
{
  LEAVE (OpStatus::ERR);
}

void
XSLT_SignalWarningL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const char *reason)
{
}

void
XSLT_SignalWarningL (XSLT_MessageHandler *messagehandler, XSLT_Import *import, const uni_char *warning)
{
}

#endif // XSLT_ERRORS
#endif // XSLT_SUPPORT
