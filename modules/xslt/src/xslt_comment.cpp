/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_comment.h"

#include "modules/xslt/src/xslt_compiler.h"
#include "modules/util/tempbuf.h"

void
XSLT_Comment::CompileL (XSLT_Compiler *compiler)
{
  XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_START_COLLECT_TEXT);

  XSLT_TemplateContent::CompileL (compiler);

  XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_END_COLLECT_TEXT);
  XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_ADD_COMMENT);
}

#endif // XSLT_SUPPORT
