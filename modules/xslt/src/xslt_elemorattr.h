/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_ELEMORATTR_H
#define XSLT_ELEMORATTR_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xmlutils/xmltypes.h"
#include "modules/xslt/src/xslt_string.h"

class XSLT_AttributeValueTemplate;
class XSLT_UseAttributeSets;
class XMLNamespaceDeclaration;
class XMLParser;

class XSLT_ElementOrAttribute
  : public XSLT_TemplateContent
{
protected:
  BOOL is_attribute;
  XSLT_String name;
  XSLT_String ns;
  XSLT_UseAttributeSets *use_attribute_sets;
  XMLNamespaceDeclaration::Reference nsdeclaration;
  XMLVersion xmlversion;

public:
  XSLT_ElementOrAttribute (BOOL is_attribute, XMLVersion xmlversion);
  virtual ~XSLT_ElementOrAttribute ();

  virtual void CompileL (XSLT_Compiler *compiler);

  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_ELEMORATTR_H
