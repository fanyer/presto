/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_HANDLER_H
#define XSLT_HANDLER_H

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_parser.h"
# include "modules/xmlutils/xmltokenbackend.h"

# include "modules/util/opstring.h"
# include "modules/util/adt/opvector.h"

class XSLT_HandlerTokenHandler
  : public XMLTokenHandler,
    public XMLTokenBackend,
    public XSLT_StylesheetParser::Callback,
    public MessageObject
{
public:
  enum State { ANALYZING, PARSING_SOURCETREE, DISABLED };

private:
  class TransformationCallback;
  friend class TransformationCallback;

  class TransformationCallback
    : public XSLT_Stylesheet::Transformation::Callback
  {
  private:
    XSLT_HandlerTokenHandler *parent;

  public:
    TransformationCallback(XSLT_HandlerTokenHandler *parent)
      : parent(parent)
    {
    }

    virtual void ContinueTransformation (XSLT_Stylesheet::Transformation *transformation) { parent->ContinueTransformation (transformation); }
    virtual OP_STATUS LoadDocument (URL document_url, XMLTokenHandler *token_handler) { return parent->LoadDocument (document_url, token_handler); }
    virtual void CancelLoadDocument (XMLTokenHandler *token_handler) { parent->CancelLoadDocument (token_handler); }
#ifdef XSLT_ERRORS
    virtual OP_BOOLEAN HandleMessage (XSLT_Stylesheet::Transformation::Callback::MessageType type, const uni_char *message) { return parent->HandleMessage (type, message); }
#endif // XSLT_ERRORS
  } transformation_callback;

  /**
   * Sometimes we delay the decision if xslt should be invoked
   * or not while we're browsing through the document prologue.
   * and then we save the tokens we see in a list of these objects.
   */
  class QueuedToken
  {
  public:
    XMLToken::Type type;

    OpString name;
    OpString data;

    class Attribute
    {
    public:
      XMLCompleteName name;
      OpString value;
    };

    OpAutoVector<Attribute> attributes;
  };

  State state;
  XSLT_Handler *handler;
  XSLT_Handler::TreeCollector *tree_collector;
  BOOL tree_collector_finished;
  XMLParser *parser;

  OpAutoVector<QueuedToken> queued_tokens;
  unsigned queue_offset;

  XMLTokenHandler *token_handler;
  XSLT_Stylesheet::Transformation::StringDataCollector *string_data_collector;
  BOOL destroy_when_finished;

  XMLTokenHandler::SourceCallback *source_callback;

  XSLT_StylesheetParser *stylesheet_parser;
  BOOL stylesheet_parser_finished;
  XMLTokenHandler *stylesheet_parser_token_handler;

  XSLT_Stylesheet *stylesheet;
  XSLT_Stylesheet::Transformation *stylesheet_transformation;

  MessageHandler *mh;
  BOOL message_posted;

#ifdef SELFTEST
public:
  class PostMessageCallback
  {
  public:
    virtual ~PostMessageCallback () {}
    virtual BOOL PostMessage (BOOL start_transformation) = 0;
  };

private:
  PostMessageCallback *post_message_cb;
#endif // SELFTEST

  /* From XMLTokenHandler: */
  virtual Result HandleToken (XMLToken &token);
  virtual void SetSourceCallback (XMLTokenHandler::SourceCallback *source_callback);

  /* From XMLTokenBackend: */
  virtual BOOL GetLiteralIsWhitespace ();
  virtual const uni_char *GetLiteralSimpleValue ();
  virtual uni_char *GetLiteralAllocatedValue ();
  virtual unsigned GetLiteralLength ();
  virtual OP_STATUS GetLiteral (XMLToken::Literal &literal);
  virtual void ReleaseLiteralPart (unsigned index);
#ifdef XML_ERRORS
  virtual BOOL GetTokenRange (XMLRange &range);
  virtual BOOL GetAttributeRange (XMLRange &range, unsigned index);
#endif // XML_ERRORS

  /* From XSLT_StylesheetParser::Callback: */
  virtual OP_STATUS LoadOtherStylesheet (URL stylesheet_url, XMLTokenHandler *token_handler, BOOL is_import);
  virtual void CancelLoadOtherStylesheet (XMLTokenHandler *token_handler);
#ifdef XSLT_ERRORS
  virtual OP_BOOLEAN HandleMessage (XSLT_StylesheetParser::Callback::MessageType type, const uni_char *message);
#endif // XSLT_ERRORS
  virtual OP_STATUS ParsingFinished (XSLT_StylesheetParser *parser);

  /* From XSLT_Stylesheet::Transformation::Callback via TransformationCallback: */
  void ContinueTransformation (XSLT_Stylesheet::Transformation *transformation);
  OP_STATUS LoadDocument (URL document_url, XMLTokenHandler *token_handler);
  void CancelLoadDocument (XMLTokenHandler *token_handler);
#ifdef XSLT_ERRORS
  OP_BOOLEAN HandleMessage (XSLT_Stylesheet::Transformation::Callback::MessageType type, const uni_char *message);
#endif // XSLT_ERRORS

  class XMLTokenHandlerElm
    : public Link
  {
  public:
    XMLTokenHandlerElm ()
      : token_handler (0)
    {
    }

    virtual ~XMLTokenHandlerElm ()
    {
      OP_DELETE (token_handler);
    }

    XMLTokenHandler *token_handler;
  };
  Head token_handlers;

  void ContinueTransformation ();
  void AbortTransformation (BOOL oom);
  BOOL PostMessage (BOOL start_transformation);

public:
  XSLT_HandlerTokenHandler (XSLT_Handler *handler)
    : transformation_callback (this),
      state (ANALYZING),
      handler (handler),
      tree_collector (0),
      tree_collector_finished (FALSE),
      parser (0),
      queue_offset (0),
      token_handler (0),
      string_data_collector (0),
      destroy_when_finished (FALSE),
      source_callback (0),
      stylesheet_parser (0),
      stylesheet_parser_finished (FALSE),
      stylesheet_parser_token_handler (0),
      stylesheet (0),
      stylesheet_transformation (0),
      mh (0),
      message_posted (FALSE)
  {
#ifdef SELFTEST
    post_message_cb = 0;
#endif // SELFTEST
  }

  virtual ~XSLT_HandlerTokenHandler ();

  /* From MessageObject: */
  virtual void HandleCallback (OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

  State GetState () { return state; }
  XMLTokenHandler *GetTokenHandler () { return token_handler; }

  BOOL HasQueuedTokens () { return queue_offset != queued_tokens.GetCount (); }
  BOOL HasStylesheet () { return stylesheet != 0; }

  XMLTokenHandler::Result EnqueueToken (const XMLToken &token);
  XMLTokenHandler::Result FlushQueue ();
  XMLTokenHandler::Result StartParsingSourceTree (XMLToken &token, URL documen_url, const uni_char *stylesheet_url);
  XMLTokenHandler::Result Disable (XMLToken &token);

  void StartTransformation (BOOL from_handletoken);

#ifdef SELFTEST
  void SetPostMessageCallback (PostMessageCallback *cb) { post_message_cb = cb; }
#endif // SELFTEST
};

#endif // XSLT_SUPPORT
#endif // XSLT_HANDLER_H
