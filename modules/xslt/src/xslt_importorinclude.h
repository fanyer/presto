/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_IMPORTORINCLUDE_H
#define XSLT_IMPORTORINCLUDE_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_parser.h"

class XSLT_ImportOrInclude
  : public XSLT_Element
{
protected:
  uni_char *href;

public:
  XSLT_ImportOrInclude ();
  ~XSLT_ImportOrInclude ();

  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element);
  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_IMPORTORINCLUDE_H
