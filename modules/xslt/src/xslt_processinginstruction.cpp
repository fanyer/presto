/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_processinginstruction.h"
#include "modules/xslt/src/xslt_attributevaluetemplate.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/util/tempbuf.h"

XSLT_ProcessingInstruction::XSLT_ProcessingInstruction (XMLVersion xmlversion)
  : xmlversion (xmlversion)
{
}


/* virtual */ void
XSLT_ProcessingInstruction::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_NAME:
      parser->SetStringL (name, completename, value, value_length);
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!name.IsSpecified())
        SignalErrorL (parser, "xsl:processing-instruction missing required name attribute");
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, completename, value, value_length);
    }
}

/* virtual */ void
XSLT_ProcessingInstruction::CompileL (XSLT_Compiler *compiler)
{
  XSLT_AttributeValueTemplate::CompileL (compiler, this, name);

  XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_SET_QNAME);
  XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_START_COLLECT_TEXT);

  XSLT_TemplateContent::CompileL (compiler);

  XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_END_COLLECT_TEXT);
  XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_ADD_PROCESSING_INSTRUCTION);
}

#endif // XSLT_SUPPORT
