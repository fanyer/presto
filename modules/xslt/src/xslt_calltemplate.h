/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_CALLTEMPLATE_H
#define XSLT_CALLTEMPLATE_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xslt/src/xslt_simple.h"

class XSLT_CallTemplate
  : public XSLT_TemplateContent
{
protected:
  XMLExpandedName name;
  BOOL has_name;

  XSLT_Template* FindTemplate(XSLT_StylesheetImpl* stylesheet);
public:
  XSLT_CallTemplate ();

  virtual void CompileL (XSLT_Compiler *compiler);
  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_CALLTEMPLATE_H
