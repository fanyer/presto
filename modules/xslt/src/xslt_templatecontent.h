/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_TEMPLATECONTENT_H
#define XSLT_TEMPLATECONTENT_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_types.h"
#include "modules/xslt/src/xslt_parser.h"

class XSLT_StylesheetImpl;
class XSLT_StylesheetParserImpl;
class XSLT_ExtendedXPathValue;
class XSLT_Import;
class XSLT_Compiler;
class XSLT_XPathExtensions;
class XSLT_Variable;

class XSLT_TemplateContent
  : public XSLT_Element
{
protected:
  XSLT_TemplateContent **children;
  unsigned children_count, children_total;
  unsigned variables_count;

  XSLT_Variable *CalculatePreviousVariable (XSLT_StylesheetImpl *stylesheet);

public:
  XSLT_TemplateContent ();
  virtual ~XSLT_TemplateContent ();

  virtual void CompileL (XSLT_Compiler *compiler);

  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element);
  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddCharacterDataL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetParser::CharacterDataType type, const uni_char *value, unsigned value_length);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);

  virtual BOOL IsTemplateContent () { return TRUE; }

  void AddChildL (XSLT_TemplateContent *child);
};

class XSLT_EmptyTemplateContent
  : public XSLT_TemplateContent
{
public:
  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element);
  virtual void AddCharacterDataL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetParser::CharacterDataType type, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_TEMPLATECONTENT_H
