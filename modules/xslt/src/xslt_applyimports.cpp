/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_applyimports.h"

#include "modules/xslt/src/xslt_template.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_stylesheet.h"

/* virtual */ void
XSLT_ApplyImports::CompileL (XSLT_Compiler *compiler)
{
  XSLT_Element *element = GetParent ();

  while (element && element->GetType () != XSLTE_TEMPLATE)
    if (element->GetType () == XSLTE_FOR_EACH)
      {
        XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_ERROR, reinterpret_cast<UINTPTR> ("xsl:apply-imports instantiated with null current template rule"));
        return;
      }
    else
      element = element->GetParent ();

  if (!element)
    XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_ERROR, reinterpret_cast<UINTPTR> ("xsl:apply-imports instantiated outside xsl:template"));
  else
    {
      XSLT_Template *tmplate = static_cast<XSLT_Template *> (element);
      XSLT_Program *program = compiler->GetStylesheet ()->GetApplyTemplatesProgramL (tmplate->HasMode () ? &tmplate->GetMode () : 0, tmplate->GetImport ());
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_APPLY_IMPORTS_ON_NODE, compiler->AddProgramL (program));
    }
}

#endif // XSLT_SUPPORT
