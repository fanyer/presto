/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLPARSER_XMLCHECKINGTOKENHANDLER_H
#define XMLPARSER_XMLCHECKINGTOKENHANDLER_H

#include "modules/xmlparser/xmlinternalparser.h"
#include "modules/xmlutils/xmltokenhandler.h"

class XMLCheckingTokenHandler
{
public:
  XMLCheckingTokenHandler (XMLInternalParser *parser, XMLTokenHandler *secondary, BOOL is_fragment, XMLNamespaceDeclaration *nsdeclaration);
  ~XMLCheckingTokenHandler ();

  void SetSecondary (XMLTokenHandler *new_secondary) { secondary = new_secondary; }

  XMLTokenHandler::Result HandleToken (XMLToken &token, BOOL from_processtoken = FALSE);

  BOOL IsReferenceAllowed () { return current != 0 || is_fragment; }

protected:
  XMLTokenHandler::Result CallSecondary (XMLToken &token);
  XMLTokenHandler::Result HandleSTagToken (XMLToken &token, BOOL from_processtoken);
  XMLTokenHandler::Result HandleETagToken (XMLToken &token, BOOL from_processtoken);
  XMLTokenHandler::Result SetError (XMLInternalParser::ParseError error);
  XMLTokenHandler::Result SetAttributeError (XMLInternalParser::ParseError error, unsigned attrindex, unsigned attrindex_related = ~0u);

  class Element
  {
  public:
    Element ()
      : qname (0)
#if defined XML_ERRORS && defined OPERA_CONSOLE
      , firstnsdecl (0)
#endif // XML_ERRORS && OPERA_CONSOLE
    {}

    ~Element ();

    OP_STATUS SetQName (const uni_char *qname, unsigned qname_length);

    Element *parent;
    XMLDoctype::Entity *containing_entity;
    uni_char *qname;
    unsigned qname_length, qname_free;
    BOOL owns_qname;
    const uni_char *uri;
    unsigned uri_length;
#ifdef XML_ERRORS
    XMLRange range;

#ifdef OPERA_CONSOLE
    /** Used to keep track of namespace declarations stemming from a
        "known default attribute" (tainted) and declarations that
        override such declarations (non-tainted.)  If a tainted
        namespace declaration is used, we issue a warning in the
        console. */
    class NamespaceDeclaration
    {
    public:
      ~NamespaceDeclaration ();

      BOOL tainted, reported;
      uni_char *prefix, *uri;
      NamespaceDeclaration *nextnsdecl;
    } *firstnsdecl;
#endif // OPERA_CONSOLE
#endif // XML_ERRORS
  };

#if defined XML_ERRORS && defined OPERA_CONSOLE
  XMLTokenHandler::Result AddNamespaceDeclaration (BOOL tainted, const uni_char *prefix, unsigned prefix_length, const uni_char *uri, unsigned uri_length);
  BOOL FindTaintedNamespaceDeclaration (const uni_char *prefix, unsigned prefix_length, Element *&ownerelement, Element::NamespaceDeclaration *&nsdecl);
  BOOL has_tainted_nsdecls;
#endif // XML_ERRORS && OPERA_CONSOLE

  Element *NewElement (), *current, *old;
  void FreeElement (Element *element);

  BOOL has_xmldecl, has_doctype, has_misc, has_whitespace, has_root, is_fragment;
  XMLInternalParser *parser;
  XMLDoctype *doctype;
  XMLTokenHandler *secondary;
  XMLNamespaceDeclaration::Reference nsdeclaration;
  unsigned level;
};

#endif // XMLPARSER_XMLCHECKINGTOKENHANDLER_H
