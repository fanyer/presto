/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_copyof.h"

#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xpath/xpath.h"

/* virtual */ void
XSLT_CopyOf::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_SELECT:
      parser->SetStringL (select, name, value, value_length);
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!select.IsSpecified ())
        SignalErrorL (parser, "xsl:copy-of missing required select attribute");
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, name, value, value_length);
    }
}

/* virtual */ void
XSLT_CopyOf::CompileL (XSLT_Compiler *compiler)
{
  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_EVALUATE_TO_VARIABLE_VALUE, compiler->AddExpressionL (select, GetXPathExtensions (), GetNamespaceDeclaration ()));
  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_ADD_COPY_OF_EVALUATE, reinterpret_cast<UINTPTR> (this));
}

#endif // XSLT_SUPPORT
