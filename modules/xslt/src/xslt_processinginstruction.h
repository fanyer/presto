/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_PROCESSINGINSTRUCTION_H
#define XSLT_PROCESSINGINSTRUCTION_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xmlutils/xmltypes.h"
#include "modules/xslt/src/xslt_string.h"

class XSLT_ProcessingInstruction
  : public XSLT_TemplateContent
{
protected:
	XSLT_String name;
  XMLVersion xmlversion;

public:
  XSLT_ProcessingInstruction (XMLVersion xmlversion);

  virtual void CompileL (XSLT_Compiler *compiler);

  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_PROCESSINGINSTRUCTION_H
