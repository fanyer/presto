/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_key.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xpath/xpath.h"
#include "modules/util/tempbuf.h"
#include "modules/util/str.h"

XSLT_Key::XSLT_Key ()
  : has_name (FALSE)
  , match (GetXPathExtensions(), NULL) // no namespacedeclaration yet, will be set later
  , next (0)
  , extensions (this, TRUE)
  , program (0)
  , search (0)
{
}

XSLT_Key::~XSLT_Key ()
{
  OP_DELETE (next);
  OP_DELETE (program);

  XPathPattern::Search::Free (search);
}

/* virtual */ XSLT_Element *
XSLT_Key::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  parser->SignalErrorL ("unexpected element in xsl:key");
  ignore_element = TRUE;
  return this;
}

/* virtual */ BOOL
XSLT_Key::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  if (parser)
    {
      match.SetNamespaceDeclaration (parser);
      parser->GetStylesheet ()->AddKey (this);
      return FALSE;
    }
  else
    return TRUE;
}

/* virtual */ void
XSLT_Key::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_MATCH:
      match.SetSourceL (parser, completename, value, value_length);
      break;

    case XSLTA_NAME:
      parser->SetQNameAttributeL (value, value_length, FALSE, &name, 0);
      has_name = TRUE;
      break;

    case XSLTA_USE:
      parser->SetStringL (use, completename, value, value_length);
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
#if defined XSLT_ERRORS && defined XML_ERRORS
# define LOCATION_ARG , &location
#else // XSLT_ERRORS && XML_ERRORS
# define LOCATION_ARG
#endif // XSLT_ERRORS && XML_ERRORS

      if (!has_name)
        SignalErrorL (parser, "xsl:key missing required name attribute");
      else if (!match.IsSpecified ())
        SignalErrorL (parser, "xsl:key missing required match attribute");
      else if (!use.IsSpecified ())
        SignalErrorL (parser, "xsl:key missing required use attribute");
    }
}

XSLT_Key *
XSLT_Key::Find (const XMLExpandedName &name)
{
  XSLT_Key *key = this;

  while (key)
    if (key->name == name)
      return key;
    else
      key = key->next;

  return 0;
}

/* static */ void
XSLT_Key::Push (XSLT_Key *&existing, XSLT_Key *key)
{
  XSLT_Key **pointer = &existing;

  while (*pointer)
    pointer = &(*pointer)->next;

  *pointer = key;
}

XPathPattern::Search *
XSLT_Key::GetSearchL (XPathPatternContext *pattern_context)
{
  if (!search)
    LEAVE_IF_ERROR (XPathPattern::Search::Make (search, pattern_context, match.GetPatterns (), match.GetPatternsCount ()));

  return search;
}

void
XSLT_Key::CompileL (XSLT_Compiler *compiler)
{
  match.PreprocessL (compiler->GetMessageHandler (), &extensions);

  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_SEARCH_PATTERNS, reinterpret_cast<UINTPTR> (this));

  use_index = compiler->AddExpressionL (use, &extensions, GetNamespaceDeclaration ());

  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_PROCESS_KEY, reinterpret_cast<UINTPTR> (this));

  if (XSLT_Key *key = FindNext (name))
    key->CompileL (compiler);
}

XSLT_Program *
XSLT_Key::CompileProgramL (XSLT_StylesheetImpl *stylesheet, XSLT_MessageHandler *messagehandler)
{
  if (!program)
    {
      XSLT_Compiler compiler (stylesheet, messagehandler); ANCHOR (XSLT_Compiler, compiler);

      CompileL (&compiler);

      program = OP_NEW_L (XSLT_Program, (XSLT_Program::TYPE_KEY));

      compiler.FinishL (program);
    }

  return program;
}

#endif // XSLT_SUPPORT
