/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_valueof.h"

#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_xpathextensions.h"
#include "modules/xpath/xpath.h"

/* virtual */ void
XSLT_ValueOf::CompileL (XSLT_Compiler *compiler)
{
  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_EVALUATE_TO_STRING, compiler->AddExpressionL (select, GetXPathExtensions(), GetNamespaceDeclaration()));
  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_ADD_TEXT, disable_output_escaping);
}

/* virtual */ void
XSLT_ValueOf::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_SELECT:
      parser->SetStringL (select, name, value, value_length);
      break;

    case XSLTA_DISABLE_OUTPUT_ESCAPING:
      if (value_length == 3 && uni_strncmp (value, "yes", 3) == 0)
        disable_output_escaping = TRUE;
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!select.IsSpecified ())
        SignalErrorL (parser, "missing required select attribute");
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, name, value, value_length);
    }
}

#endif // XSLT_SUPPORT
