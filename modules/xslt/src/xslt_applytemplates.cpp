/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_applytemplates.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_sort.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_template.h"
#include "modules/xpath/xpath.h"

XSLT_ApplyTemplates::XSLT_ApplyTemplates ()
  : has_mode (FALSE),
    sort (0)
{
}

XSLT_ApplyTemplates::~XSLT_ApplyTemplates ()
{
  OP_DELETE (sort);
}

XSLT_Sort *
XSLT_ApplyTemplates::AddSort (XSLT_StylesheetParserImpl *parser, XSLT_Sort *new_sort)
{
  XSLT_Sort::Push (sort, new_sort);

#if defined XSLT_ERRORS && defined XML_ERRORS
  new_sort->SetLocation (parser->GetCurrentLocation ());
#endif // XSLT_ERRORS && XML_ERRORS

  return new_sort;
}

/* virtual */ void
XSLT_ApplyTemplates::CompileL (XSLT_Compiler *compiler)
{
  if (children_count != 0)
    {
      XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_START_COLLECT_PARAMS);

      XSLT_TemplateContent::CompileL (compiler);
    }

  select_index = compiler->AddExpressionL (select, GetXPathExtensions (), GetNamespaceDeclaration ());

  if (!sort)
    {
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_EVALUATE_TO_NODESET_ITERATOR, select_index);
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_APPLY_TEMPLATES, reinterpret_cast<UINTPTR> (this));
    }
  else
    {
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_EVALUATE_TO_NODESET_SNAPSHOT, select_index);

      sort->CompileL (compiler);

      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_APPLY_TEMPLATES, reinterpret_cast<UINTPTR> (this));
    }

  if (children_count != 0)
    XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_RESET_PARAMS_COLLECTED_FOR_CALL);
}

/* virtual */ XSLT_Element *
XSLT_ApplyTemplates::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  if (type == XSLTE_SORT)
    {
      XSLT_Sort* sort = OP_NEW_L (XSLT_Sort, (this, extensions.GetPreviousVariable ()));
      return AddSort (parser, sort);
    }
  else if (type == XSLTE_WITH_PARAM)
    return XSLT_TemplateContent::StartElementL (parser, type, name, ignore_element);
  else
    {
      parser->SignalErrorL ("unexpected element in xsl:apply-templates");
      ignore_element = TRUE;
      return this;
    }
}

/* virtual */ void
XSLT_ApplyTemplates::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_MODE:
      parser->SetQNameAttributeL (value, value_length, FALSE, &mode, 0);
      has_mode = TRUE;
      break;

    case XSLTA_SELECT:
      parser->SetStringL (select, name, value, value_length);
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!select.IsSpecified ())
        parser->SetStringL (select, XMLCompleteName (0, 0, UNI_L ("select")), UNI_L ("node()"), 6);
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, name, value, value_length);
    }
}
#endif // XSLT_SUPPORT
