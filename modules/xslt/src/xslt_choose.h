/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_CHOOSE_H
#define XSLT_CHOOSE_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"

class XSLT_Compiler;

class XSLT_Choose
  : public XSLT_TemplateContent
{
protected:
  BOOL has_when, has_otherwise;

public:
  XSLT_Choose ();

  virtual void CompileL (XSLT_Compiler *compiler);

  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element);
  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddCharacterDataL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetParser::CharacterDataType type, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_CHOOSE_H
