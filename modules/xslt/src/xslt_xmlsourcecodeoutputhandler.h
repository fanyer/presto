/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_XMLSOURCECODEOUTPUTHANDLER_H
#define XSLT_XMLSOURCECODEOUTPUTHANDLER_H

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_xmloutputhandler.h"

class XSLT_OutputBuffer;

class XSLT_XMLSourceCodeOutputHandler
  : public XSLT_XMLOutputHandler
{
public:
  XSLT_XMLSourceCodeOutputHandler (XSLT_OutputBuffer *buffer, XSLT_StylesheetImpl *stylesheet, BOOL expand_empty_elements = FALSE);

private:
  /* From XSLT_OutputHandler via XSLT_XMLOutputHandler: */
  virtual void AddTextL (const uni_char *data, BOOL disable_output_escaping);
  virtual void AddCommentL (const uni_char *data);
  virtual void AddProcessingInstructionL (const uni_char *target, const uni_char *data);
  virtual void EndOutputL ();

  /* From XSLT_XMLOutputHandler: */
  virtual void OutputXMLDeclL (const XMLDocumentInformation &document_info);
  virtual void OutputDocumentTypeDeclL (const XMLDocumentInformation &document_info);
  virtual void OutputTagL (XMLToken::Type type, const XMLCompleteName &name);

  void CloseCDATASectionL ();

  XSLT_OutputBuffer *buffer;
  BOOL expand_empty_elements;
  BOOL in_cdata_section;
};

#endif // XSLT_SUPPORT
#endif // XSLT_XMLSOURCECODEOUTPUTHANDLER_H
