/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_VALUEOF_H
#define XSLT_VALUEOF_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_string.h"

class XPathExpression;

class XSLT_ValueOf
  : public XSLT_EmptyTemplateContent
{
protected:
  XSLT_String select;
  BOOL disable_output_escaping;

public:
  XSLT_ValueOf ()
    : disable_output_escaping (FALSE)
  {
  }

  virtual void CompileL (XSLT_Compiler *compiler);

  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_VALUEOF_H
