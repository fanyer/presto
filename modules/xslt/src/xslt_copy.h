/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_COPY_H
#define XSLT_COPY_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xslt/src/xslt_program.h"

class XSLT_UseAttributeSets;

class XSLT_Copy
  : public XSLT_TemplateContent
{
protected:
  XSLT_UseAttributeSets *use_attribute_sets;

  XSLT_Program child_program;

public:
  XSLT_Copy ();
  ~XSLT_Copy ();

  virtual void CompileL (XSLT_Compiler *compiler);

  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_COPY_H
