/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_text.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_compiler.h"

/* virtual */ void
XSLT_Text::CompileL (XSLT_Compiler *compiler)
{
  if (content.GetStorage ())
    {
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_SET_STRING, compiler->AddStringL (content.GetStorage ()));
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_ADD_TEXT, disable_output_escaping);
    }
}

/* virtual */ XSLT_Element *
XSLT_Text::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  parser->SignalErrorL ("unexpected element in xsl:text");
  ignore_element = TRUE;
  return this;
}

/* virtual */ void
XSLT_Text::AddCharacterDataL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetParser::CharacterDataType type, const uni_char *value, unsigned value_length)
{
  content.AppendL (value, value_length);
}

/* virtual */ void
XSLT_Text::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_DISABLE_OUTPUT_ESCAPING:
      if (value_length == 3 && uni_strncmp (value, "yes", 3) == 0)
        disable_output_escaping = TRUE;
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, name, value, value_length);
    }
}

#endif // XSLT_SUPPORT
