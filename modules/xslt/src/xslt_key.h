/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_KEY_H
#define XSLT_KEY_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/util/OpHashTable.h"

class XPathPattern;
class XPathNode;
class XSLT_Compiler;
class XSLT_Program;

class XSLT_Key
  : public XSLT_Element
{
protected:
  XMLExpandedName name;
  BOOL has_name;
  XSLT_XPathPattern match;
  XSLT_String use;
  XSLT_Key *next;
  XSLT_XPathExtensions extensions;
  XSLT_Program *program;
  unsigned use_index;
  XPathPattern::Search *search;

public:
  XSLT_Key ();
  ~XSLT_Key ();

  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element);
  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length);

  XPathPattern **GetPatterns () { return match.GetPatterns (); }
  unsigned GetPatternsCount () { return match.GetPatternsCount (); }

  XPathPattern::Search *GetSearchL (XPathPatternContext *pattern_context);

  const XMLExpandedName &GetName () { return name; }

  XSLT_Key *GetNext () { return next; }
  XSLT_Key *Find (const XMLExpandedName &name);
  XSLT_Key *FindNext (const XMLExpandedName &name) { return next ? next->Find (name) : 0; }

  static void Push (XSLT_Key *&existing, XSLT_Key *key);

  void CompileL (XSLT_Compiler *compiler);
  XSLT_Program *CompileProgramL (XSLT_StylesheetImpl *stylesheet, XSLT_MessageHandler *messagehandler);
  unsigned GetUseExpressionIndex () { return use_index; }
};

#endif // XSLT_SUPPORT
#endif // XSLT_KEY_H
