/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_FUNCTIONS_H
#define XSLT_FUNCTIONS_H

#ifdef XSLT_SUPPORT

#include "modules/xpath/xpath.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/url/url2.h"

class XSLT_StylesheetImpl;
class XSLT_StylesheetParserImpl;

class XMLDoctype;

class XSLT_Function
  : public XPathFunction
{
public:
  XMLNamespaceDeclaration::Reference nsdeclaration;
};

class XSLT_Functions
{
public:
  class Current
    : public XSLT_Function
  {
  protected:
    virtual unsigned GetResultType(unsigned *arguments, unsigned arguments_count);
    virtual unsigned GetFlags();
    virtual Result Call(XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state);
  };

  class SystemProperty
    : public XSLT_Function
  {
  protected:
    virtual unsigned GetResultType(unsigned *arguments, unsigned arguments_count);
    virtual unsigned GetFlags();
    virtual Result Call(XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state);
  };

  class GenerateID
    : public XSLT_Function
  {
  protected:
    virtual unsigned GetResultType (unsigned *arguments, unsigned arguments_count);
    virtual unsigned GetFlags ();
    virtual Result Call (XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state);
  };

  class Key
    : public XSLT_Function
  {
  protected:
    class KeyState
      : public XPathFunction::State
    {
    public:
      enum StateCode { PROCESS_ARGUMENT0, PROCESS_ARGUMENT1, PROCESS_STRING, PROCESS_NODES };

      KeyState ()
        : state (PROCESS_ARGUMENT0),
          has_value (FALSE)
      {
      }

      StateCode state;
      XMLExpandedName keyname;
      BOOL has_value;
      OpString value;
    };

    XSLT_StylesheetImpl *stylesheet;

    virtual unsigned GetResultType (unsigned *arguments, unsigned arguments_count);
    virtual unsigned GetFlags ();
    virtual Result Call (XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state);

  public:
    Key (XSLT_StylesheetImpl *stylesheet);
  };

  class ElementOrFunctionAvailable
    : public XSLT_Function
  {
  protected:
    BOOL element_available;

    virtual unsigned GetResultType(unsigned *arguments, unsigned arguments_count);
    virtual unsigned GetFlags();
    virtual Result Call(XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state);

  public:
    ElementOrFunctionAvailable (/*XMLNamespaceDeclaration *nsdeclaration, */BOOL element_available);
  };

  class UnparsedEntityUri
    : public XSLT_Function
  {
  protected:
    virtual unsigned GetResultType(unsigned *arguments, unsigned arguments_count);
    virtual unsigned GetFlags();
    virtual Result Call(XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state);
  };

  class FormatNumber
    : public XSLT_Function
  {
  protected:
    XSLT_StylesheetImpl *stylesheet;

    virtual unsigned GetResultType(unsigned *arguments, unsigned arguments_count);
    virtual unsigned GetFlags();
    virtual Result Call(XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state);

  public:
    FormatNumber (XSLT_StylesheetImpl *stylesheet/*, XMLNamespaceDeclaration *nsdeclaration*/);
  };

  class NodeSet
    : public XSLT_Function
  {
  protected:
    virtual unsigned GetResultType(unsigned *arguments, unsigned arguments_count);
    virtual unsigned GetFlags();
    virtual Result Call(XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state);
  };

  class Document
    : public XSLT_Function
  {
  protected:
    class DocumentState
      : public XPathFunction::State
    {
    public:
      DocumentState (URL baseurl)
        : state (INITIAL),
          baseurl (baseurl)
      {
      }

      enum { INITIAL, SETUP_ARGUMENT2, PROCESS_ARGUMENT2, SETUP_ARGUMENT1, PROCESS_ARGUMENT1, LOAD_TREE_FROM_STRING, LOAD_TREES_FROM_NODE } state;
      URL url, baseurl;
    };

    URL baseurl;

    virtual unsigned GetResultType (unsigned *arguments, unsigned arguments_count);
    virtual unsigned GetFlags ();
    virtual Result Call (XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *context, State *&state);

  public:
    Document (URL baseurl)
      : baseurl (baseurl)
    {
    }
  };
};

#endif // XSLT_SUPPORT
#endif // XSLT_FUNCTIONS_H
