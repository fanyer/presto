/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_calltemplate.h"

#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_template.h"

#include "modules/util/tempbuf.h"

XSLT_CallTemplate::XSLT_CallTemplate ()
  : has_name (FALSE)
{
}

/* virtual */ XSLT_Element *
XSLT_CallTemplate::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  if (type == XSLTE_WITH_PARAM)
    return XSLT_TemplateContent::StartElementL (parser, type, name, ignore_element);
  else
    {
      parser->SignalErrorL ("unexpected element in xsl:call-template");
      ignore_element = TRUE;
      return this;
    }
}

/* virtual */ void
XSLT_CallTemplate::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_NAME:
      parser->SetQNameAttributeL (value, value_length, FALSE, &name, 0);
      has_name = TRUE;
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!has_name)
        SignalErrorL (parser, "xsl:call-template missing required name attribute");
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, completename, value, value_length);
    }
}

XSLT_Template *
XSLT_CallTemplate::FindTemplate (XSLT_StylesheetImpl* stylesheet)
{
  unsigned template_index, templates_count = stylesheet->GetTemplatesCount ();
  XSLT_Template **templates = stylesheet->GetTemplates (), *best = 0;

  for (template_index = templates_count; template_index > 0;)
    {
      --template_index;

      XSLT_Template *tmplate = templates[template_index];

      if (tmplate->HasName () && name == tmplate->GetName ())
        {
          best = tmplate;
          break;
        }
    }

  return best;
}

/* virtual */ void
XSLT_CallTemplate::CompileL (XSLT_Compiler *compiler)
{
  XSLT_Template* tmplate = FindTemplate (compiler->GetStylesheet ());

  if (!tmplate)
    /* Mismatching name. */
    LEAVE(OpStatus::ERR);

  if (children_count != 0)
    {
      XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_START_COLLECT_PARAMS);

      XSLT_TemplateContent::CompileL (compiler);
    }

  /* Need to pick up the argments, evaluate those, find out which
     template to call and then call that template's program.  Not sure
     yet who should bind the variables, the caller or the method. */
  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_CALL_PROGRAM_ON_NODE, compiler->AddProgramL (tmplate->GetProgram ()));

  if (children_count != 0)
    XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_RESET_PARAMS_COLLECTED_FOR_CALL);
}

#endif // XSLT_SUPPORT
