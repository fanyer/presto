/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_TEMPLATE_H
#define XSLT_TEMPLATE_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_program.h"

#include "modules/url/url2.h"

class XSLT_StylesheetParserImpl;
class XSLT_Import;
class XSLT_Program;

class XPathPattern;

class XSLT_Template
  : public XSLT_TemplateContent
{
protected:
  /** Pointer is only valid during parsing/compiling */
  XSLT_Import *import;
  URL import_url;
  unsigned import_precedence;

  XSLT_XPathPattern match;

  BOOL has_name, has_mode;
  XMLCompleteName name, mode;

  BOOL has_priority;
  float priority;

  class Param
  {
  public:
    XMLExpandedName name;
    XSLT_Variable* variable;
    Param *next;
  } *params;

  XSLT_Program program;

public:
  XSLT_Template (XSLT_Import *import, unsigned import_precedence);
  ~XSLT_Template ();
  void AddParamL (const XMLExpandedName &name, XSLT_Variable* param_elm);

  XSLT_Import *GetImport () { return import; }

  URL GetImportURL () { return import_url; }
  unsigned GetImportPrecedence () { return import_precedence; }
  void SetImportPrecedence (unsigned index) { import_precedence = index; }

  const XSLT_XPathPattern &GetMatch () { return match; }
  XPathPattern **GetPatterns () { return match.GetPatterns (); }
  unsigned GetPatternsCount () { return match.GetPatternsCount (); }
  XMLNamespaceDeclaration *GetNamespaceDeclaration () { return match.GetNamespaceDeclaration (); }

  BOOL HasName () { return has_name; }
  const XMLCompleteName &GetName () { return name; }

  BOOL HasMode () { return has_mode; }
  const XMLCompleteName &GetMode () { return mode; }

  BOOL HasPriority () { return has_priority; }
  float GetPriority () { return priority; }
  float GetPatternPriority (unsigned index);

  BOOL HasParams () { return params != 0; }

  XSLT_Program *GetProgram () { return &program; }
  void CompileTemplateL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetImpl *stylesheet);

  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);
};

#endif // XSLT_SUPPORT
#endif // XSLT_TEMPLATE_H
