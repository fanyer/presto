/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_fallback.h"

XSLT_Fallback::XSLT_Fallback (BOOL enabled)
  : enabled (enabled)
{
}

/* virtual */ void
XSLT_Fallback::CompileL (XSLT_Compiler *compiler)
{
  if (enabled)
    XSLT_TemplateContent::CompileL (compiler);
}

#endif // XSLT_SUPPORT
