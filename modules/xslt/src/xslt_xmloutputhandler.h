/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_XMLOUTPUTHANDLER_H
#define XSLT_XMLOUTPUTHANDLER_H

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_outputhandler.h"
# include "modules/xslt/src/xslt_stylesheet.h"
# include "modules/xmlutils/xmltoken.h"
# include "modules/xmlutils/xmltokenbackend.h"
# include "modules/xmlutils/xmltokenhandler.h"
# include "modules/xmlutils/xmlnames.h"
# include "modules/xmlutils/xmldocumentinfo.h"

# include "modules/util/adt/opvector.h"

class XSLT_XMLOutputHandler
  : public XSLT_OutputHandler
{
protected:
  XSLT_XMLOutputHandler (XSLT_StylesheetImpl *stylesheet)
    : nsnormalizer (TRUE, TRUE),
      stylesheet (stylesheet),
      output_specification (0),
      in_starttag (FALSE),
      output_xmldecl (TRUE),
      output_doctype (TRUE),
      level (0),
      suggested_ns_level (0)
  {
    if (stylesheet)
      output_specification = &stylesheet->GetOutputSpecificationInternal ();
  }

  /* From XSLT_OutputHandler: */
  virtual void StartElementL (const XMLCompleteName &name);
  virtual void SuggestNamespaceDeclarationL (XSLT_Element *element, XMLNamespaceDeclaration *nsdeclaration, BOOL skip_excluded_namespaces);
  virtual void AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified);
  virtual void AddTextL (const uni_char *data, BOOL disable_output_escaping);
  virtual void AddCommentL (const uni_char *data);
  virtual void AddProcessingInstructionL (const uni_char *target, const uni_char *data);
  virtual void EndElementL (const XMLCompleteName &name);
  virtual void EndOutputL ();

  /* New: */
  virtual void OutputXMLDeclL (const XMLDocumentInformation &document_info) = 0;
  virtual void OutputDocumentTypeDeclL (const XMLDocumentInformation &document_info) = 0;
  virtual void OutputTagL (XMLToken::Type type, const XMLCompleteName &name) = 0;

  XMLNamespaceNormalizer nsnormalizer;
  XMLCompleteName root_element_name;
  XSLT_StylesheetImpl *stylesheet;
  const XSLT_OutputSpecificationInternal *output_specification;

  BOOL UseCDATASections ();

private:
  void CallOutputTagL (XMLToken::Type type, const XMLCompleteName *name = 0);

  OpINT32Vector use_cdata_sections;
  BOOL in_starttag, output_xmldecl, output_doctype;

  XMLNamespaceDeclaration::Reference suggested_ns;
  unsigned level, suggested_ns_level;
};

#endif // XSLT_SUPPORT
#endif // XSLT_XMLOUTPUTHANDLER_H
