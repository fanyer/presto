/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_message.h"

#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_compiler.h"

XSLT_Message::XSLT_Message ()
  : terminate (FALSE)
{
}


/* virtual */ void
XSLT_Message::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_TERMINATE:
      terminate = XSLT_CompareStrings (value, value_length, "yes");
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, completename, value, value_length);
    }
}

/* virtual */ void
XSLT_Message::CompileL (XSLT_Compiler *compiler)
{
  XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_START_COLLECT_TEXT);

  XSLT_TemplateContent::CompileL (compiler);

  XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_END_COLLECT_TEXT);
  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_SEND_MESSAGE, terminate);
}

#endif // XSLT_SUPPORT
