/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_APPLYIMPORTS_H
#define XSLT_APPLYIMPORTS_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xslt/src/xslt_simple.h"

class XSLT_ApplyImports
  : public XSLT_EmptyTemplateContent
{
public:
  virtual void CompileL (XSLT_Compiler *compiler);
};

#endif // XSLT_SUPPORT
#endif // XSLT_APPLYIMPORTS_H
