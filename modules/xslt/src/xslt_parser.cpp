/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_lexer.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_template.h"
#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xslt/src/xslt_functions.h"
#include "modules/xslt/src/xslt_variable.h"
#include "modules/xslt/src/xslt_key.h"
#include "modules/xslt/src/xslt_utils.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_program.h"
#include "modules/xslt/src/xslt_literalresultelement.h"

#include "modules/xmlutils/xmlutils.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xpath/xpath.h"
#include "modules/util/str.h"
#include "modules/util/tempbuf.h"

/* virtual */ OP_STATUS
XSLT_StylesheetParser::Callback::LoadOtherStylesheet (URL stylesheet_url, XMLTokenHandler *token_handler, BOOL is_import)
{
  return OpStatus::ERR;
}

/* virtual */ void
XSLT_StylesheetParser::Callback::CancelLoadOtherStylesheet (XMLTokenHandler *token_handler)
{
}

#ifdef XSLT_ERRORS

/* virtual */ OP_BOOLEAN
XSLT_StylesheetParser::Callback::HandleMessage (MessageType type, const uni_char *message)
{
  return OpBoolean::IS_FALSE;
}

#endif // XSLT_ERRORS

/* virtual */ OP_STATUS
XSLT_StylesheetParser::Callback::ParsingFinished(XSLT_StylesheetParser *parser)
{
  return OpStatus::OK;
}

/* static */ OP_STATUS
XSLT_StylesheetParser::Make (XSLT_StylesheetParser *&parser, XSLT_StylesheetParser::Callback *callback)
{
  parser = OP_NEW (XSLT_StylesheetParserImpl, (callback));

  if (!parser || OpStatus::IsMemoryError (((XSLT_StylesheetParserImpl *) parser)->Construct ()))
    return OpStatus::ERR_NO_MEMORY;
  else
    return OpStatus::OK;
}

/* static */ void
XSLT_StylesheetParser::Free (XSLT_StylesheetParser *parser)
{
  if (parser)
    {
      XSLT_StylesheetParserImpl *impl = static_cast<XSLT_StylesheetParserImpl *> (parser);
      OP_DELETE (impl);
    }
}

/* virtual */
XSLT_StylesheetParser::~XSLT_StylesheetParser ()
{
}

XSLT_Element::XSLT_Element ()
  : parent (0),
    is_inserted (FALSE),
    extensions (this)
{
}

/* virtual */
XSLT_Element::~XSLT_Element ()
{
}

XMLVersion
XSLT_Element::GetXMLVersion ()
{
  XSLT_Element *element = this;
  while (element && element->type != XSLTE_STYLESHEET)
    element = element->parent;
  return element ? static_cast<XSLT_StylesheetElement *> (element)->GetImport ()->xmlversion : XMLVERSION_1_0;
}

BOOL
XSLT_Element::IsExcludedNamespace (const uni_char *uri)
{
  if (uni_strcmp (uri, "http://www.w3.org/1999/XSL/Transform") == 0)
    return TRUE;

  XSLT_Element *element = this;
  while (element)
    {
      if (element->type == XSLTE_OTHER)
        {
          if (static_cast<XSLT_LiteralResultElement *> (element)->GetExcludedNamespaces ()->ContainsURI (uri))
            return TRUE;
        }
      else if (element->type == XSLTE_STYLESHEET)
        {
          if (static_cast<XSLT_StylesheetElement *> (element)->GetExcludedNamespaces ()->ContainsURI (uri))
            return TRUE;
        }

      element = element->parent;
    }

  return FALSE;
}

BOOL
XSLT_Element::IsExtensionNamespace (const uni_char *uri)
{
  XSLT_Element *element = this;
  while (element)
    {
      if (element->type == XSLTE_OTHER)
        {
          if (static_cast<XSLT_LiteralResultElement *> (element)->GetExtensionNamespaces ()->ContainsURI (uri))
            return TRUE;
        }
      else if (element->type == XSLTE_STYLESHEET)
        {
          if (static_cast<XSLT_StylesheetElement *> (element)->GetExtensionNamespaces ()->ContainsURI (uri))
            return TRUE;
        }

      element = element->parent;
    }

  return FALSE;
}

void
XSLT_Element::SignalErrorL (XSLT_MessageHandler *messagehandler, const char *reason)
{
#ifdef XSLT_ERRORS
#ifdef XML_ERRORS
  XSLT_SignalErrorL (messagehandler, this, reason, &location);
#else // XML_ERRORS
  XSLT_SignalErrorL (messagehandler, this, reason);
#endif // XML_ERRORS
#else // XSLT_ERRORS
  LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
}

void
XSLT_Element::SignalErrorL (XSLT_MessageHandler *messagehandler, const uni_char *reason)
{
#ifdef XSLT_ERRORS
#ifdef XML_ERRORS
  XSLT_SignalErrorL (messagehandler, this, reason, &location);
#else // XML_ERRORS
  XSLT_SignalErrorL (messagehandler, this, reason);
#endif // XML_ERRORS
#else // XSLT_ERRORS
  LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
}

#ifdef XSLT_ERRORS

void
XSLT_Element::SignalMessageL (XSLT_MessageHandler *messagehandler, const uni_char *message, BOOL terminate)
{
#if defined XSLT_ERRORS && defined XML_ERRORS
  XSLT_SignalMessageL (messagehandler, this, message, terminate, &location);
#else // XSLT_ERRORS && XML_ERRORS
  XSLT_SignalMessageL (messagehandler, this, message, terminate);
#endif // XSLT_ERRORS && XML_ERRORS
}

#endif // XSLT_ERRORS

/* virtual */ void
XSLT_Element::AddCharacterDataL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetParser::CharacterDataType type, const uni_char *value, unsigned value_length)
{
  if (type != XSLT_StylesheetParser::CHARACTERDATA_TEXT_WHITESPACE)
#ifdef XSLT_ERRORS
    SignalErrorL (parser, "unexpected character data");
#else // XSLT_ERRORS
    LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
}

/* virtual */ void
XSLT_Element::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  if (parser->GetCurrentImport ()->version != XSLT_VERSION_FUTURE)
    {
#ifdef XSLT_ERRORS
      XSLT_String string; ANCHOR (XSLT_String, string);
      parser->SetStringL (string, name, value, value_length);
      string.SignalErrorL (parser, "invalid attribute", TRUE);
#else // XSLT_ERRORS
      LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
    }
}

#ifdef XSLT_ERRORS

/* virtual */ OP_BOOLEAN
XSLT_StylesheetParserImpl::HandleMessage (XSLT_MessageHandler::MessageType type, const uni_char *message)
{
  OP_ASSERT (type != XSLT_MessageHandler::MESSAGE_TYPE_MESSAGE);
  /* A static_cast should be valid for converting between enumerated types, but
     apparently GCC 2.95 doesn't think so. */
  return callback->HandleMessage ((XSLT_StylesheetParser::Callback::MessageType) type, message);
}

#endif // XSLT_ERRORS

void
XSLT_StylesheetParserImpl::StartEntityL (const URL &url, const XMLDocumentInformation &docinfo, BOOL entity_reference)
{
  OP_ASSERT (!parsing_failed);

  LEAVE_IF_ERROR (HandleStartEntity (url, docinfo, entity_reference));

  if (!entity_reference)
    {
      XSLT_Import *import;

      if (next_import)
        {
          OP_ASSERT (next_import->parent == current_import);
          import = next_import;
        }
      else
        import = OP_NEW_L (XSLT_Import, (~0u, url, XSLTE_STYLESHEET, 0));

      if (current_import)
        {
          current_import->current_element = current_element;
          current_element = 0;
          current_stylesheet = FALSE;
        }

      current_import = import;
      next_import = 0;

      if (!base_import)
        base_import = import;

      if (!stylesheet)
        stylesheet = OP_NEW_L (XSLT_StylesheetImpl, ());
    }
}

void
XSLT_StylesheetParserImpl::StartElementL (const XMLCompleteNameN &completename, BOOL fragment_start, BOOL &ignore_element)
{
  OP_ASSERT (current_import);
  OP_ASSERT (!parsing_failed);

  FlushCharacterDataL ();

#ifdef XSLT_ERRORS
  current_elementname.SetL (completename);

#ifdef XML_ERRORS
  current_location_type = XSLT_Location::TYPE_START_TAG;
#endif // XML_ERRORS
#endif // XSLT_ERRORS

  LEAVE_IF_ERROR (HandleStartElement ());

  ++depth;

  XSLT_ElementType type = XSLT_Lexer::GetElementType (completename);

  if (type == XSLTE_UNRECOGNIZED)
    {
      if (current_import->version != XSLT_VERSION_FUTURE)
        SignalErrorL ("unrecognized element in the XSLT namespace");
    }
  // Unknown elements in templates (not top-level) can be extension elements
  else if (type == XSLTE_OTHER && current_element && current_element->GetType() != XSLTE_STYLESHEET && completename.GetUri ())
    {
      OpString uri; ANCHOR (OpString, uri);
      uri.AppendL (completename.GetUri (), completename.GetUriLength ());
      if (current_element->IsExtensionNamespace (uri.CStr ()))
        type = XSLTE_UNSUPPORTED;
    }

  if (type == XSLTE_TRANSFORM)
    type = XSLTE_STYLESHEET;

  if (!current_stylesheet)
    {
      if (fragment_start)
        if (type == XSLTE_STYLESHEET)
          {
            current_stylesheet = TRUE;
            current_import->root_element = current_element = OP_NEW_L (XSLT_StylesheetElement, (stylesheet, current_import));
            current_element->SetType (XSLTE_STYLESHEET);

            stylesheet->AddStylesheetElementL (static_cast<XSLT_StylesheetElement *> (current_element));

#ifdef XSLTE_ERRORS
            current_element->SetNameL (completename);
#endif // XSLTE_ERRORS
          }
        else if (type == XSLTE_OTHER)
          {
            StartElementL (XMLCompleteName (UNI_L ("http://www.w3.org/1999/XSL/Transform"), 0, UNI_L ("stylesheet")), TRUE, ignore_element);
            current_element->SetIsInserted ();

            StartElementL (XMLCompleteName (UNI_L ("http://www.w3.org/1999/XSL/Transform"), 0, UNI_L ("template")), FALSE, ignore_element);
            current_element->SetIsInserted ();

            AddAttributeL (XMLCompleteName (0, 0, UNI_L ("match")), UNI_L ("/"), 1, TRUE, FALSE);

            goto handle_element;
          }
        else
          {
            SignalErrorL ("expected stylesheet element");
            ignore_element = TRUE;
          }
    }
  else if (current_element)
    {
    handle_element:
      if (current_import->import_precedence == ~0u && type != XSLTE_IMPORT)
        {
          unsigned use_index;

          if (current_import->type == XSLTE_INCLUDE)
            use_index = current_import->GetNonInclude ()->import_precedence;
          else
            use_index = AllocateNewImportIndex ();

          current_import->import_precedence = use_index;
        }

      XSLT_Element *new_current_element = current_element->StartElementL (this, type, completename, ignore_element);
      if (!ignore_element)
        {
          current_element = new_current_element;
#ifdef XSLT_ERRORS
          current_element->SetNameL (completename);
#endif // XSLT_ERRORS
        }
    }

  if (ignore_element)
    ignored_element = TRUE;
}

void
XSLT_StylesheetParserImpl::AddAttributeL (const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length, BOOL specified, BOOL id)
{
  OP_ASSERT (!parsing_failed);

#ifdef XSLT_ERRORS
#ifdef XML_ERRORS
  current_location_type = XSLT_Location::TYPE_ATTRIBUTE;
#endif // XML_ERRORS
#endif // XSLT_ERRORS

  LEAVE_IF_ERROR (HandleAttribute (completename, value, value_length));

  if (current_element)
    {
      XSLT_AttributeType type = XSLT_Lexer::GetAttributeType (current_element->GetType () != XSLTE_OTHER && current_element->GetType () != XSLTE_UNSUPPORTED, completename);
      current_element->AddAttributeL (this, type, completename, value, value_length);
    }
}

void
XSLT_StylesheetParserImpl::StartContentL ()
{
  OP_ASSERT (!parsing_failed);

  if (current_element)
    {
      XMLCompleteNameN completename;
      current_element->SetBaseURL (GetCurrentBaseUrl ());
      current_element->SetNamespaceDeclaration (GetCurrentNamespaceDeclaration ());
      current_element->AddAttributeL (this, XSLTA_NO_MORE_ATTRIBUTES, completename, 0, 0);
    }

#ifdef XSLT_ERRORS
  current_elementname = XMLCompleteName ();
#endif // XSLT_ERRORS
}

void
XSLT_StylesheetParserImpl::AddCharacterDataL (CharacterDataType type, const uni_char *value, unsigned value_length)
{
  OP_ASSERT (!parsing_failed);

#if defined XSLT_ERRORS && defined XML_ERRORS
  current_location_type = XSLT_Location::TYPE_CHARACTER_DATA;
#endif // XSLT_ERRORS && XML_ERRORS

  if (current_element)
    character_data.AppendL (value, value_length);
}

void
XSLT_StylesheetParserImpl::FlushCharacterDataL ()
{
  const uni_char *value = character_data.GetStorage ();
  if (value && *value)
    {
      unsigned value_length = character_data.Length ();
      BOOL is_whitespace = XMLUtils::IsWhitespace (value, value_length);

      if (GetCurrentWhitespaceHandling () == XMLWHITESPACEHANDLING_PRESERVE || !is_whitespace)
        current_element->AddCharacterDataL (this, is_whitespace ? CHARACTERDATA_TEXT_WHITESPACE : CHARACTERDATA_TEXT, value, value_length);

      character_data.Clear ();
    }
}

void
XSLT_StylesheetParserImpl::AddProcessingInstructionL (const uni_char *target, unsigned target_length, const uni_char *data, unsigned data_length)
{
  OP_ASSERT (!parsing_failed);
}

void
XSLT_StylesheetParserImpl::EndElementL (BOOL &block, BOOL &finished)
{
  OP_ASSERT (current_import);
  OP_ASSERT (!parsing_failed);

  FlushCharacterDataL ();

#if defined XSLT_ERRORS && defined XML_ERRORS
  current_location_type = XSLT_Location::TYPE_END_TAG;
#endif // XSLT_ERRORS && XML_ERRORS

  if (!ignored_element)
    {
      if (current_element)
        {
#ifdef XSLT_ERRORS
          current_elementname = current_element->GetName ();
#endif // XSLT_ERRORS

          BOOL delete_element = current_element->EndElementL (this);

#ifdef XSLT_ERRORS
          current_elementname = XMLCompleteName ();
#endif // XSLT_ERRORS

          XSLT_Element *previous_current_element = current_element;
          current_element = current_element->GetParent ();

          if (delete_element)
            OP_DELETE (previous_current_element);

          --depth;

          if (next_import)
            block = TRUE;

          while (current_element && current_element->GetIsInserted ())
            EndElementL (block, finished);
        }
    }
  else
    depth--;

  XMLNamespaceDeclaration::Reference nsdeclaration_before (GetCurrentNamespaceDeclaration ()); ANCHOR (XMLNamespaceDeclaration::Reference, nsdeclaration_before);

  OP_STATUS status = HandleEndElement ();

  if (OpStatus::IsMemoryError (status))
    LEAVE (status);

  if (!ignored_element && !current_element && current_stylesheet)
    current_stylesheet = FALSE;

  ignored_element = FALSE;
}

void
XSLT_StylesheetParserImpl::EndEntityL ()
{
  OP_ASSERT (!parsing_failed);

  FlushCharacterDataL ();

  XSLT_Import *ended_import = current_import;
  current_import = ended_import->parent;

  if (ended_import->source_callback)
    ended_import->source_callback->Continue (XMLLanguageParser::SourceCallback::STATUS_CONTINUE);

  if (current_import)
    current_element = current_import->current_element;
  else
    callback->ParsingFinished (this);

  current_stylesheet = TRUE;

  if (!ended_import->root_element)
    {
      OP_ASSERT (FALSE);
      OP_DELETE (ended_import);
    }

  LEAVE_IF_ERROR (HandleEndEntity ());
}

void
XSLT_StylesheetParserImpl::XMLErrorL ()
{
  /* When parsing fails in an nested import, we'll get here once per current
     import. */

  parsing_failed = TRUE;

  if (next_import || current_import)
    {
      XSLT_Import *import;

      if (next_import)
        {
          import = next_import;
          next_import = 0;
        }
      else
        {
          while (current_element)
            {
              XSLT_Element *element = current_element;
              current_element = element->GetParent ();

              if (element->EndElementL (0))
                OP_DELETE (element);
            }

          import = current_import;
          current_import = import->parent;

          current_element = current_import ? current_import->current_element : 0;
        }

      OP_ASSERT (!current_import || import->source_callback);

      if (import->source_callback)
        import->source_callback->Continue (XMLLanguageParser::SourceCallback::STATUS_ABORT_FAILED);
      else
        callback->ParsingFinished (this);

      if (!import->root_element)
        OP_DELETE (import);
    }
}

XSLT_StylesheetParserImpl::XSLT_StylesheetParserImpl (XSLT_StylesheetParser::Callback *callback)
  : callback (callback),
    stylesheet (0),
    current_stylesheet (FALSE),
    ignored_element (FALSE),
    parsing_failed (FALSE),
    current_element (0),
    base_import (0),
    current_import (0),
    next_import (0),
    import_index (0),
#if defined XSLT_ERRORS && defined XML_ERRORS
    current_location_type (XSLT_Location::TYPE_NO_RANGE),
#endif // XSLT_ERRORS && XML_ERRORS
    depth (0)
{
  character_data.SetExpansionPolicy (TempBuffer::AGGRESSIVE);
  character_data.SetCachedLengthPolicy (TempBuffer::TRUSTED);
}

/* virtual */
XSLT_StylesheetParserImpl::~XSLT_StylesheetParserImpl ()
{
  while (current_import)
    {
      while (current_element)
        {
          XSLT_Element *element = current_element;
          current_element = element->GetParent ();

          if (element->EndElementL (0))
            OP_DELETE (element);
        }

      XSLT_Import *import = current_import->parent;

      if (current_import)
        {
          if (current_import->xmltokenhandler)
            callback->CancelLoadOtherStylesheet (current_import->xmltokenhandler);

          if (!current_import->root_element)
            OP_DELETE (current_import);
        }

      current_import = import;

      if (current_import)
        current_element = current_import->current_element;
    }

  if (next_import && next_import->xmltokenhandler)
    callback->CancelLoadOtherStylesheet (next_import->xmltokenhandler);

  OP_DELETE (next_import);
  OP_DELETE (stylesheet);
}

OP_STATUS
XSLT_StylesheetParserImpl::Construct ()
{
  return OpStatus::OK;
}

/* virtual */ OP_STATUS
XSLT_StylesheetParserImpl::StartEntity (const URL &url, const XMLDocumentInformation &docinfo, BOOL entity_reference)
{
  TRAPD (status, StartEntityL (url, docinfo, entity_reference));
  return status;
}

/* virtual */ OP_STATUS
XSLT_StylesheetParserImpl::StartElement (const XMLCompleteNameN &completename, BOOL fragment_start, BOOL &ignore_element)
{
  TRAPD (status, StartElementL (completename, fragment_start, ignore_element));
  return status;
}

/* virtual */ OP_STATUS
XSLT_StylesheetParserImpl::AddAttribute (const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length, BOOL specified, BOOL id)
{
  TRAPD (status, AddAttributeL (completename, value, value_length, specified, id));
  return status;
}

/* virtual */ OP_STATUS
XSLT_StylesheetParserImpl::StartContent ()
{
  TRAPD (status, StartContentL ());
  return status;
}

/* virtual */ OP_STATUS
XSLT_StylesheetParserImpl::AddCharacterData (CharacterDataType type, const uni_char *value, unsigned value_length)
{
  TRAPD (status, AddCharacterDataL (type, value, value_length));
  return status;
}

/* virtual */ OP_STATUS
XSLT_StylesheetParserImpl::AddProcessingInstruction (const uni_char *target, unsigned target_length, const uni_char *data, unsigned data_length)
{
  TRAPD (status, AddProcessingInstructionL (target, target_length, data, data_length));
  return status;
}

/* virtual */ OP_STATUS
XSLT_StylesheetParserImpl::EndElement (BOOL &block, BOOL &finished)
{
  TRAPD (status, EndElementL (block, finished));
  return status;
}

/* virtual */ void
XSLT_StylesheetParserImpl::SetSourceCallback(XMLLanguageParser::SourceCallback *source_callback)
{
  next_import->source_callback = source_callback;
}

/* virtual */ OP_STATUS
XSLT_StylesheetParserImpl::EndEntity ()
{
  TRAPD (status, EndEntityL ());
  return status;
}

/* virtual */ OP_STATUS
XSLT_StylesheetParserImpl::XMLError ()
{
  TRAPD (status, XMLErrorL ());
  return status;
}

/* virtual */ OP_STATUS
XSLT_StylesheetParserImpl::GetStylesheet (XSLT_Stylesheet *&stylesheet_out)
{
  if (!parsing_failed)
    {
      RETURN_IF_ERROR (stylesheet->Finish (this));

      stylesheet_out = stylesheet;
      stylesheet = 0;

      return OpStatus::OK;
    }
  else
    return OpStatus::ERR;
}

const uni_char *
XSLT_StylesheetParserImpl::GetNamespaceURI (const uni_char *prefix)
{
  return XMLNamespaceDeclaration::FindUri (GetCurrentNamespaceDeclaration (), prefix);
}

XMLNamespaceDeclaration *
XSLT_StylesheetParserImpl::GetNamespaceDeclaration ()
{
  return GetCurrentNamespaceDeclaration ();
}

void
XSLT_StylesheetParserImpl::SetQNameAttributeL (const uni_char *value, unsigned value_length, BOOL use_default, XMLExpandedName *expandedname, XMLCompleteName *completename)
{
  if (!XMLUtils::IsValidQName (GetCurrentVersion (), value, value_length))
    SignalErrorL ("invalid QName");
  else
    {
      XMLCompleteNameN temporary (value, value_length);

      if (!XMLNamespaceDeclaration::ResolveName (GetCurrentNamespaceDeclaration (), temporary, use_default))
        SignalErrorL ("undeclared prefix in QName");
      else if (expandedname)
        expandedname->SetL (temporary);
      else if (completename)
        completename->SetL (temporary);
    }
}

void
XSLT_StylesheetParserImpl::HandleImportOrIncludeL (const uni_char *href, XSLT_ElementType type)
{
  URL baseurl (GetCurrentBaseUrl ()); ANCHOR (URL, baseurl);
  URL url (g_url_api->GetURL (baseurl, href)); ANCHOR (URL, url);

  XSLT_Import *parent = current_import;
  while (parent)
    {
      if (parent->url == url)
        {
          const char *parent_rel = parent->url.RelName();
          const char *rel = url.RelName();

          if (!parent_rel || !rel || op_strcmp (parent_rel, rel) == 0)
            /* Recursive inclusion.  Note: if one fragment is empty
               and the other is not, it is really okay, unless the
               non-empty fragment is the id of the root element in the
               document. */
            SignalErrorL ("recursive stylesheet import or inclusion");
        }

      parent = parent->parent;
    }

  next_import = OP_NEW_L (XSLT_Import, (~0u, url, type, current_import));

  LEAVE_IF_ERROR (XMLLanguageParser::MakeTokenHandler (next_import->xmltokenhandler, this, url.UniRelName ()));
  LEAVE_IF_ERROR (callback->LoadOtherStylesheet (url, next_import->xmltokenhandler, type == XSLTE_IMPORT));
}

XMLVersion
XSLT_StylesheetParserImpl::GetCurrentXMLVersion ()
{
  return GetCurrentVersion ();
}

XMLWhitespaceHandling
XSLT_StylesheetParserImpl::GetCurrentWhitespaceHandling ()
{
  if (current_element->GetType () == XSLTE_TEXT)
    return XMLWHITESPACEHANDLING_PRESERVE;
  else
    return XMLLanguageParser::GetCurrentWhitespaceHandling ();
}

void
XSLT_StylesheetParserImpl::AddWhiteSpaceControlL (const uni_char *elements, unsigned elements_length, BOOL strip)
{
  XMLVersion version = GetCurrentXMLVersion ();

  while (elements_length != 0)
    {
      const uni_char *qname = elements;

      if (!XMLUtils::IsSpace (XMLUtils::GetNextCharacter (elements, elements_length)))
        {
          unsigned qname_length = 1;

          while (elements_length != 0 && !XMLUtils::IsSpace (XMLUtils::GetNextCharacter (elements, elements_length)))
            ++qname_length;

          if (!(qname_length == 1 && *qname == '*') &&
              !XMLUtils::IsValidQName (version, qname, qname_length) &&
              !(qname_length > 2 && qname[qname_length - 1] == '*' && qname[qname_length - 2] == ':' && XMLUtils::IsValidNCName (version, qname, qname_length - 2)))
            SignalErrorL ("invalid NameTest in attribute value");

          XMLCompleteNameN nametest (qname, qname_length);

          if (!XMLNamespaceDeclaration::ResolveName (GetCurrentNamespaceDeclaration (), nametest, FALSE))
            SignalErrorL ("undeclared prefix in NameTest");

          stylesheet->AddWhiteSpaceControlL (current_import, nametest, strip);
        }
    }
}

void
XSLT_StylesheetParserImpl::AddCDATASectionElementsL (const uni_char *elements, unsigned elements_length)
{
  XMLVersion version = GetCurrentXMLVersion ();

  while (elements_length != 0)
    {
      const uni_char *qname = elements;

      if (!XMLUtils::IsSpace (XMLUtils::GetNextCharacter (elements, elements_length)))
        {
          unsigned qname_length = 1;

          while (elements_length != 0 && !XMLUtils::IsSpace (XMLUtils::GetNextCharacter (elements, elements_length)))
            ++qname_length;

          if (!XMLUtils::IsValidQName (version, qname, qname_length))
            SignalErrorL ("invalid QName in xsl:output/@cdata-section-elements");

          XMLCompleteNameN name (qname, qname_length);

          if (!XMLNamespaceDeclaration::ResolveName (GetCurrentNamespaceDeclaration (), name, TRUE))
            SignalErrorL ("undeclared prefix in xsl:output/@cdata-section-elements");

          stylesheet->AddCDATASectionElementL (name);
        }
    }
}

void
XSLT_StylesheetParserImpl::AddNamespaceAliasL (const uni_char *stylesheet_prefix, const uni_char *result_prefix)
{
  XMLNamespaceDeclaration *nsdeclaration = GetCurrentNamespaceDeclaration ();

  if (uni_strcmp (stylesheet_prefix, "#default") == 0)
    stylesheet_prefix = 0;

  if (uni_strcmp (result_prefix, "#default") == 0)
    result_prefix = 0;

  const uni_char *stylesheet_uri = XMLNamespaceDeclaration::FindUri (nsdeclaration, stylesheet_prefix);
  const uni_char *result_uri = XMLNamespaceDeclaration::FindUri (nsdeclaration, result_prefix);

  if (!stylesheet_uri || !result_uri)
    SignalErrorL ("undeclared prefix in xsl:namespace-alias");
  else
    stylesheet->AddNamespaceAliasL (current_import, stylesheet_prefix, stylesheet_uri, result_prefix, result_uri);
}

void
XSLT_StylesheetParserImpl::SetStringL (XSLT_String &string, const XMLCompleteNameN &attributename, const uni_char *value, unsigned value_length)
{
  string.SetStringL (value, value_length);
#ifdef XSLT_ERRORS
#ifdef XML_ERRORS
  string.SetDebugInformationL (current_elementname, attributename, location, current_import->url);
#else // XML_ERRORS
  string.SetDebugInformationL (current_elementname, attributename, current_import->url);
#endif // XML_ERRORS
#endif // XSLT_ERRORS
}

void
XSLT_StylesheetParserImpl::SignalErrorL (const char *reason)
{
#ifdef XSLT_ERRORS
#ifdef XML_ERRORS
  XSLT_Location location (GetCurrentLocation ());
  XSLT_SignalErrorL (this, current_element, reason, current_import ? &location : 0);
#else // XML_ERRORS
  XSLT_SignalErrorL (this, current_element, reason);
#endif // XML_ERRORS
#else // XSLT_ERRORS
  LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
}

#if defined XSLT_ERRORS && defined XML_ERRORS

XSLT_Location
XSLT_StylesheetParserImpl::GetCurrentLocation ()
{
  return XSLT_Location (current_location_type, current_import, location);
}

#endif // XSLT_ERRORS && XML_ERRORS

unsigned
XSLT_StylesheetParserImpl::AllocateNewImportIndex ()
{
  XSLT_Import *import = current_import->parent;
  unsigned use_index = ~0u;

  while (import)
    {
      if (import->import_precedence != ~0u)
        {
          if (use_index == ~0u)
            {
              use_index = import->import_precedence;
              ++import_index;
            }

          unsigned old_index = import->import_precedence;

          if (stylesheet)
            stylesheet->RenumberImportPrecedence (old_index, ++import->import_precedence);
        }

      import = import->parent;
    }

  if (use_index == ~0u)
    use_index = ++import_index;

  return use_index;
}

#endif // XSLT_SUPPORT
