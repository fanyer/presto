/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_FALLBACK_H
#define XSLT_FALLBACK_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"

class XSLT_Fallback
  : public XSLT_TemplateContent
{
protected:
  BOOL enabled;

public:
  XSLT_Fallback (BOOL enabled);

  virtual void CompileL (XSLT_Compiler *compiler);
};

#endif // XSLT_SUPPORT
#endif // XSLT_FALLBACK_H
