/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_iforwhen.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_xpathextensions.h"
#include "modules/xpath/xpath.h"
#include "modules/util/tempbuf.h"

/* virtual */ void
XSLT_IfOrWhen::CompileL (XSLT_Compiler *compiler)
{
  if (GetType() == XSLTE_WHEN)
    {
      OP_ASSERT(!(GetParent() && GetParent()->GetType() == XSLTE_CHOOSE) || !"Wouldn't have been called since XSLT_Choose::Compile calls CompileConditionalCodeL");
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL (XSLT_Instruction::IC_ERROR, reinterpret_cast<UINTPTR> ("xsl:when outside xsl:choose"), this);
      return;
    }

  CompileConditionalCodeL (compiler);
}

unsigned
XSLT_IfOrWhen::CompileConditionalCodeL (XSLT_Compiler *compiler)
{
  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_EVALUATE_TO_BOOLEAN, compiler->AddExpressionL (test, GetXPathExtensions (), GetNamespaceDeclaration ()));

  unsigned false_destination = XSLT_ADD_JUMP_INSTRUCTION (XSLT_Instruction::IC_JUMP_IF_FALSE);
  unsigned true_destination = 0;

  XSLT_TemplateContent::CompileL (compiler);

  if (GetType () == XSLTE_WHEN)
    true_destination = XSLT_ADD_JUMP_INSTRUCTION (XSLT_Instruction::IC_JUMP);

  compiler->SetJumpDestination (false_destination);
  return true_destination;
}

/* virtual */ void
XSLT_IfOrWhen::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_TEST:
      parser->SetStringL (test, name, value, value_length);
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!test.IsSpecified ())
        SignalErrorL (parser, GetType () == XSLTE_WHEN ? "xsl:when missing required test attribute" : "xsl:if missing required test attribute");
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, name, value, value_length);
    }
}

#endif // XSLT_SUPPORT
