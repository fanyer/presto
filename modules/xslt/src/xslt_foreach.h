/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_FOREACH_H
#define XSLT_FOREACH_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_program.h"

class XSLT_Sort;

class XSLT_ForEach
  : public XSLT_TemplateContent
{
protected:
  XSLT_String select;
  XSLT_Sort *sort;

  XSLT_Program program;
  BOOL compiled;

public:
  XSLT_ForEach ();
  ~XSLT_ForEach ();

  XSLT_Sort *AddSort (XSLT_StylesheetParserImpl *parser, XSLT_Sort *sort);

  virtual void CompileL (XSLT_Compiler *compiler);

  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &completename, BOOL &ignore_element);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_FOREACH_H
