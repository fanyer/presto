/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_COMMENT_H
#define XSLT_COMMENT_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"

class XSLT_Comment
  : public XSLT_TemplateContent
{
public:
  virtual void CompileL (XSLT_Compiler *compiler);
};

#endif // XSLT_SUPPORT
#endif // XSLT_COMMENT_H
