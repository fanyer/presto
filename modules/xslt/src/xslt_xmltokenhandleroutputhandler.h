/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_XMLTOKENHANDLEROUTPUTHANDLER_H
#define XSLT_XMLTOKENHANDLEROUTPUTHANDLER_H

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_xmloutputhandler.h"
# include "modules/xmlutils/xmltoken.h"
# include "modules/xmlutils/xmltokenbackend.h"
# include "modules/xmlutils/xmltokenhandler.h"
# include "modules/xmlutils/xmlnames.h"
# include "modules/xmlutils/xmlparser.h"

class XSLT_TransformationImpl;

class XSLT_XMLTokenHandlerOutputHandler
  : public XSLT_XMLOutputHandler,
    public XMLTokenBackend,
    public XMLParser::Listener
{
public:
  XSLT_XMLTokenHandlerOutputHandler (XMLTokenHandler *tokenhandler, XMLParser::ParseMode parsemode, XSLT_TransformationImpl *transformation);
  virtual ~XSLT_XMLTokenHandlerOutputHandler ();

  OP_STATUS Construct ();

private:
  XMLTokenHandler *tokenhandler;
  XMLParser::ParseMode parsemode;
  XMLParser *parser;
  XMLToken *token;
  XSLT_TransformationImpl *transformation;
  const uni_char *literal;
  TempBuffer literal_buffer;

  void FlushCharacterDataL ();

  /* From XSLT_OutputHandler via XSLT_XMLOutputHandler: */
  virtual void AddTextL (const uni_char *data, BOOL disable_output_escaping);
  virtual void AddCommentL (const uni_char *data);
  virtual void AddProcessingInstructionL (const uni_char *target, const uni_char *data);
  virtual void EndOutputL ();

  /* From XSLT_XMLOutputHandler: */
  virtual void OutputXMLDeclL (const XMLDocumentInformation &document_info);
  virtual void OutputDocumentTypeDeclL (const XMLDocumentInformation &document_info);
  virtual void OutputTagL (XMLToken::Type type, const XMLCompleteName &name);

  /* From XMLTokenBackend: */
  virtual BOOL GetLiteralIsWhitespace();
  virtual const uni_char *GetLiteralSimpleValue();
  virtual uni_char *GetLiteralAllocatedValue();
  virtual unsigned GetLiteralLength();
  virtual OP_STATUS GetLiteral(XMLToken::Literal &literal);
  virtual void ReleaseLiteralPart(unsigned index);
#ifdef XML_ERRORS
  virtual BOOL GetTokenRange(XMLRange &range);
  virtual BOOL GetAttributeRange(XMLRange &range, unsigned index);
#endif // XML_ERRORS

  /* From XMLParser::Listener: */
  virtual void Continue (XMLParser *parser);
  virtual void Stopped(XMLParser *parser);

  void ProcessTokenL (XMLToken::Type type);
};

#endif // XSLT_SUPPORT
#endif // XSLT_XMLTOKENHANDLEROUTPUTHANDLER_H
