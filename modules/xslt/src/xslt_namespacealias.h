/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_NAMESPACEALIAS_H
#define XSLT_NAMESPACEALIAS_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_parser.h"

class XSLT_NamespaceAlias
  : public XSLT_Element
{
protected:
  uni_char *stylesheet_prefix, *result_prefix;
  XMLVersion xmlversion;

public:
  XSLT_NamespaceAlias (XMLVersion xmlversion);
  ~XSLT_NamespaceAlias ();

  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element);
  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_NAMESPACEALIAS_H
