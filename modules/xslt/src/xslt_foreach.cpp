/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_foreach.h"

#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_sort.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_xpathextensions.h"
#include "modules/xpath/xpath.h"

XSLT_ForEach::XSLT_ForEach ()
  : sort (0),
    program (XSLT_Program::TYPE_FOR_EACH),
    compiled (FALSE)
{
}

XSLT_ForEach::~XSLT_ForEach ()
{
  OP_DELETE (sort);
}

XSLT_Sort *
XSLT_ForEach::AddSort (XSLT_StylesheetParserImpl *parser, XSLT_Sort *new_sort)
{
  XSLT_Sort::Push (sort, new_sort);

#if defined XSLT_ERRORS && defined XML_ERRORS
  new_sort->SetLocation (parser->GetCurrentLocation ());
#endif // XSLT_ERRORS && XML_ERRORS

  return new_sort;
}

/* virtual */ XSLT_Element *
XSLT_ForEach::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &completename, BOOL &ignore_element)
{
  if (type == XSLTE_SORT)
    {
      XSLT_Sort *sort = OP_NEW_L (XSLT_Sort, (this, extensions.GetPreviousVariable ()));
      return AddSort (parser, sort);
    }
  else
    return XSLT_TemplateContent::StartElementL (parser, type, completename, ignore_element);
}

/* virtual */ void
XSLT_ForEach::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_SELECT:
      parser->SetStringL (select, name, value, value_length);
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!select.IsSpecified ())
        SignalErrorL (parser, "xsl:for-each missing required select attribute");
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, name, value, value_length);
    }
}

/* virtual */ void
XSLT_ForEach::CompileL (XSLT_Compiler *compiler)
{
  if (!compiled)
    {
#ifdef XSLT_PROGRAM_DUMP_SUPPORT
      OpString8 select_description;
      select_description.SetUTF8FromUTF16L(select.GetString());
      OpStatus::Ignore(program.program_description.AppendFormat("foreach %s", select_description.CStr()));
#endif // XSLT_PROGRAM_DUMP_SUPPORT

      XSLT_Compiler *child_compiler = compiler->GetNewCompilerL ();
      OpStackAutoPtr<XSLT_Compiler> child_compiler_anchor (child_compiler);

      XSLT_TemplateContent::CompileL (child_compiler);
      child_compiler->FinishL (&program);

      child_compiler_anchor.reset (0);

      compiled = TRUE;
    }

  unsigned program_index = compiler->AddProgramL (&program);
  unsigned select_index = compiler->AddExpressionL (select, GetXPathExtensions (), GetNamespaceDeclaration ());

  if (sort)
    {
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_EVALUATE_TO_NODESET_SNAPSHOT, select_index);
      sort->CompileL (compiler);
    }
  else
    XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_EVALUATE_TO_NODESET_ITERATOR, select_index);

  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_CALL_PROGRAM_ON_NODES_NO_SCOPE, program_index);
}

#endif // XSLT_SUPPORT
