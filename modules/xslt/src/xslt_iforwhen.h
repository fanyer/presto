/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_IFORWHEN_H
#define XSLT_IFORWHEN_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_string.h"
#include "modules/xslt/src/xslt_program.h"

class XSLT_IfOrWhen
  : public XSLT_TemplateContent
{
protected:
  XSLT_String test;
//  XSLT_Program program;

public:
  virtual void CompileL (XSLT_Compiler *compiler);
  /**< Only used for 'xsl:if', 'xsl:when' is compiled directly from XSLT_Choose
       by calling CompileConditionalCodeL (). */

  unsigned CompileConditionalCodeL(XSLT_Compiler *compiler);
  /**< Returns the handle to jump to (see SetJumpDestination ()) if the code was
       executed.  Otherwise it will just continue with the next instruction. */

  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_IFORWHEN_H
