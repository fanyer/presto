/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_copy.h"

#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_attributeset.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_xpathextensions.h"
#include "modules/xpath/xpath.h"

XSLT_Copy::XSLT_Copy ()
  : use_attribute_sets (NULL)
{
}

XSLT_Copy::~XSLT_Copy ()
{
  OP_DELETE (use_attribute_sets);
}

/* virtual */ void
XSLT_Copy::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_USE_ATTRIBUTE_SET:
      use_attribute_sets = XSLT_UseAttributeSets::MakeL (parser, value, value_length);
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (use_attribute_sets)
        use_attribute_sets->FinishL (parser, this);
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, completename, value, value_length);
    }
}

/* virtual */ void
XSLT_Copy::CompileL (XSLT_Compiler *compiler)
{
#ifdef XSLT_PROGRAM_DUMP_SUPPORT
  child_program.program_description.SetL("copy");
#endif // XSLT_PROGRAM_DUMP_SUPPORT

  XSLT_Compiler *child_compiler = compiler->GetNewCompilerL ();
  OpStackAutoPtr<XSLT_Compiler> child_compiler_anchor (child_compiler);

  XSLT_TemplateContent::CompileL (child_compiler);
  child_compiler->FinishL(&child_program);

  unsigned child_program_index = compiler->AddProgramL (&child_program);

  unsigned after_code_1 = XSLT_ADD_JUMP_INSTRUCTION (XSLT_Instruction::IC_ADD_COPY_AND_JUMP_IF_NO_END_ELEMENT);
  unsigned to_root_node = XSLT_ADD_JUMP_INSTRUCTION (XSLT_Instruction::IC_JUMP);

  if (use_attribute_sets)
    use_attribute_sets->CompileL(compiler);

  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_CALL_PROGRAM_ON_NODE_NO_SCOPE, child_program_index);
  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_END_ELEMENT, ~0u);

  unsigned after_code_2 = XSLT_ADD_JUMP_INSTRUCTION (XSLT_Instruction::IC_JUMP);

  compiler->SetJumpDestination (to_root_node);

  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_CALL_PROGRAM_ON_NODE_NO_SCOPE, child_program_index);

  compiler->SetJumpDestination (after_code_1);
  compiler->SetJumpDestination (after_code_2);
}


#endif // XSLT_SUPPORT
