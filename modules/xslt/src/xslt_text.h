/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_TEXT_H
#define XSLT_TEXT_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/util/tempbuf.h"

class XSLT_Text
  : public XSLT_TemplateContent
{
protected:
  TempBuffer content;
  BOOL disable_output_escaping;

public:
  XSLT_Text ()
    : disable_output_escaping (FALSE)
  {
  }

  virtual void CompileL (XSLT_Compiler *compiler);

  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element);
  virtual void AddCharacterDataL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetParser::CharacterDataType type, const uni_char *value, unsigned value_length);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_TEXT_H
