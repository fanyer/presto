/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_UNSUPPORTED_H
#define XSLT_UNSUPPORTED_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xslt/src/xslt_namespacecollection.h"

class XSLT_Unsupported
  : public XSLT_TemplateContent
{
private:
  XSLT_NamespaceCollection extension_elements;

public:
  XSLT_Unsupported ()
    : extension_elements (XSLT_NamespaceCollection::TYPE_EXTENSION_ELEMENT_PREFIXES)
  {
  }

  virtual void CompileL (XSLT_Compiler *compiler);

  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_UNSUPPORTED_H
