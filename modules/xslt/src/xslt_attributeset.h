/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_ATTRIBUTESET_H
#define XSLT_ATTRIBUTESET_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_parser.h"
#include "modules/xmlutils/xmlnames.h"

class XSLT_ElementOrAttribute;
class XSLT_Compiler;
class XSLT_UseAttributeSets;

class XSLT_AttributeSet
  : public XSLT_Element
{
protected:
  XMLCompleteName name;
  BOOL has_name;

  XSLT_ElementOrAttribute **attributes;
  unsigned attributes_count, attributes_total;

  XSLT_AttributeSet *next, *next_used;

  XSLT_UseAttributeSets *use_attribute_sets;

  void AddAttributeL (XSLT_ElementOrAttribute *attribute);

public:
  XSLT_AttributeSet ();
  ~XSLT_AttributeSet ();

  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element);
  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);

  void CompileL (XSLT_Compiler *compiler);

  const XMLCompleteName &GetName () { return name; }

  XSLT_AttributeSet *Find (const XMLExpandedName &name);
  XSLT_AttributeSet *FindNext ();

  static void Push (XSLT_AttributeSet *&existing, XSLT_AttributeSet *key);
};

class XSLT_UseAttributeSets
{
protected:
  OpString value;
  XMLVersion xmlversion;

  XMLExpandedName *names;
  unsigned names_count;

  XSLT_UseAttributeSets ();

public:
  ~XSLT_UseAttributeSets ();

  static XSLT_UseAttributeSets *MakeL (XSLT_StylesheetParserImpl *parser, const uni_char *value, unsigned value_length);

  void FinishL (XSLT_StylesheetParserImpl *parser, XSLT_Element *element);

  XSLT_AttributeSet *GetAttributeSet (XSLT_StylesheetImpl *stylesheet, unsigned index);
  unsigned GetAttributeSetsCount () { return names_count; }

  void CompileL (XSLT_Compiler *compiler);
};

#endif // XSLT_SUPPORT
#endif // XSLT_ATTRIBUTESET_H
