/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_template.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_utils.h"
#include "modules/xslt/src/xslt_program.h"
#include "modules/xpath/xpath.h"
#include "modules/util/tempbuf.h"

XSLT_Template::XSLT_Template (XSLT_Import *import, unsigned import_precedence)
  : import (import),
    import_url (import ? import->url : URL()),
    import_precedence (import_precedence),
    match (GetXPathExtensions (), 0),
    has_name (FALSE),
    has_mode (FALSE),
    has_priority (FALSE),
    priority (0.f),
    params (NULL),
    program (XSLT_Program::TYPE_TEMPLATE)
{
  OP_ASSERT (import_precedence != ~0u);

#ifdef XSLT_PROGRAM_DUMP_SUPPORT
  program.program_description.SetL("anonymous template program");
#endif // XSLT_PROGRAM_DUMP_SUPPORT
}

XSLT_Template::~XSLT_Template ()
{
  while (params)
    {
      Param* next = params->next;
      OP_DELETE (params);
      params = next;
    }
}

void
XSLT_Template::AddParamL (const XMLExpandedName &name, XSLT_Variable* variable)
{
  OpStackAutoPtr<Param> param (OP_NEW_L (Param, ()));
  param->name.SetL (name);
  param->variable = variable;
  param->next = params;
  params = param.release ();
}

void
XSLT_Template::CompileTemplateL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetImpl *stylesheet)
{
  XSLT_XPathExtensions extensions (this, TRUE);
  match.PreprocessL (parser, &extensions);

  XSLT_Compiler compiler (stylesheet, parser); ANCHOR (XSLT_Compiler, compiler);

  XSLT_TemplateContent::CompileL (&compiler);
  compiler.FinishL (&program);

  if (has_mode)
    LEAVE_IF_ERROR (program.SetMode (mode));

#ifdef XSLT_PROGRAM_DUMP_SUPPORT
  if (has_name)
    {
      OpString8 template_name;
      template_name.SetUTF8FromUTF16L (name.GetLocalPart ());
      program.program_description.Empty ();
      program.program_description.AppendFormat ("template program '%s'", template_name.CStr ());
    }
  else if (match.GetSource())
    {
      OpString8 source;
      source.SetUTF8FromUTF16L (match.GetSource ());
      program.program_description.Empty ();
      program.program_description.AppendFormat ("template program matching '%s'", source.CStr ());
    }
#endif // XSLT_PROGRAM_DUMP_SUPPORT
}

float
XSLT_Template::GetPatternPriority (unsigned index)
{
  if (has_priority)
    return priority;
  else
    return match.GetPatterns ()[index]->GetPriority ();
}

/* virtual */ BOOL
XSLT_Template::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  if (parser)
    {
      match.SetNamespaceDeclaration (parser);
      parser->GetStylesheet ()->AddTemplateL (this);
      return FALSE;
    }
  else
    return TRUE;
}

/* virtual */ void
XSLT_Template::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_NAME:
      parser->SetQNameAttributeL (value, value_length, FALSE, 0, &name);
      has_name = TRUE;
      break;

    case XSLTA_MATCH:
      match.SetSourceL (parser, completename, value, value_length);
      break;

    case XSLTA_MODE:
      parser->SetQNameAttributeL (value, value_length, FALSE, 0, &mode);
      has_mode = TRUE;
      break;

    case XSLTA_PRIORITY:
      if (XSLT_Utils::ParseFloatL (priority, value, value_length))
        has_priority = TRUE;
      else
        SignalErrorL (parser, "invalid priority attribute value");
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, completename, value, value_length);
    }
}

#endif // XSLT_SUPPORT
