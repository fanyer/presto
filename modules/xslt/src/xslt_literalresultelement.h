/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_LITERALRESULTELEMENT_H
#define XSLT_LITERALRESULTELEMENT_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_string.h"
#include "modules/xslt/src/xslt_namespacecollection.h"

class XSLT_AttributeValueTemplate;
class XSLT_UseAttributeSets;
class TempBuffer;

class XSLT_LiteralResultElement
  : public XSLT_TemplateContent
{
protected:
  class Attribute
  {
  protected:
    XMLCompleteName name;
    XSLT_String value;
    XMLNamespaceDeclaration::Reference nsdeclaration;

  public:
    void SetNameL (const XMLCompleteNameN &completename);
    void SetValueL (XSLT_StylesheetParserImpl *parser, const uni_char *value, unsigned value_length);
    void SetNamespaceDeclaration (XSLT_StylesheetParserImpl *parser);

    const XMLCompleteName &GetName ();

    void CompileL (XSLT_Compiler *compiler, XSLT_Element *element);
  };

  XMLCompleteName name;
  Attribute **attributes;
  unsigned attributes_count, attributes_total;
  XSLT_UseAttributeSets *use_attribute_sets;
  XSLT_NamespaceCollection excluded_namespaces, extension_namespaces;

public:
  XSLT_LiteralResultElement ();
  virtual ~XSLT_LiteralResultElement ();

  void SetNameL (const XMLCompleteNameN &name);
  const XMLCompleteName GetName () { return name; }

  XSLT_NamespaceCollection *GetExcludedNamespaces () { return &excluded_namespaces; }
  XSLT_NamespaceCollection *GetExtensionNamespaces () { return &extension_namespaces; }

  virtual void CompileL (XSLT_Compiler *compiler);

  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

class XSLT_LiteralResultTextNode
  : public XSLT_TemplateContent
{
protected:
  uni_char *text;

public:
  XSLT_LiteralResultTextNode ();
  virtual ~XSLT_LiteralResultTextNode ();

  void SetTextL (const uni_char *text, unsigned text_length);
  const uni_char *GetText () { return text; }

  virtual void CompileL (XSLT_Compiler *compiler);
};

#endif // XSLT_SUPPORT
#endif // XSLT_LITERALRESULTELEMENT_H
