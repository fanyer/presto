/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_HANDLER_SUPPORT
# include "modules/xslt/src/xslt_handler.h"
# include "modules/xslt/src/xslt_parser.h"
# include "modules/xslt/src/xslt_stylesheet.h"
# include "modules/logdoc/htm_elm.h"
# include "modules/logdoc/logdoc.h"

/* static */ OP_STATUS
XSLT_Handler::MakeTokenHandler (XMLTokenHandler *&token_handler, XSLT_Handler *handler)
{
  token_handler = OP_NEW (XSLT_HandlerTokenHandler, (handler));
  return token_handler ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/* virtual */
XSLT_Handler::~XSLT_Handler ()
{
}

/* virtual */ OP_STATUS
XSLT_Handler::OnXMLOutput (XMLTokenHandler *&tokenhandler, BOOL &destroy_when_finished)
{
  return OpStatus::ERR;
}

/* virtual */ OP_STATUS
XSLT_Handler::OnHTMLOutput (XSLT_Stylesheet::Transformation::StringDataCollector *&collector, BOOL &destroy_when_finished)
{
  return OpStatus::ERR;
}

/* virtual */ OP_STATUS
XSLT_Handler::OnTextOutput (XSLT_Stylesheet::Transformation::StringDataCollector *&collector, BOOL &destroy_when_finished)
{
  return OpStatus::ERR;
}

#ifdef XSLT_ERRORS

/* virtual */ OP_BOOLEAN
XSLT_Handler::HandleMessage (XSLT_Handler::MessageType type, const uni_char *message)
{
  return OpBoolean::IS_FALSE;
}

#endif // XSLT_ERRORS

/* virtual */ XMLTokenHandler::Result
XSLT_HandlerTokenHandler::HandleToken (XMLToken &token)
{
  if (!parser)
    parser = token.GetParser ();

  if (GetState () == XSLT_HandlerTokenHandler::ANALYZING)
    {
      switch (token.GetType ())
        {
        case XMLToken::TYPE_PI:
          if (token.GetName () == XMLExpandedName (UNI_L ("xml-stylesheet")))
          {
            XMLToken::Attribute *attributes = token.GetAttributes ();
            unsigned attributes_count = token.GetAttributesCount ();

            BOOL is_xslt_stylesheet = FALSE;
            OpString href;

            for (unsigned index = 0; index < attributes_count; ++index)
              if (attributes[index].GetName () == XMLExpandedName (UNI_L ("type")))
                {
                  const uni_char *value = attributes[index].GetValue ();
                  unsigned value_length = attributes[index].GetValueLength ();

                  if (uni_strncmp (value, "text/xml", value_length) == 0 ||
                      uni_strncmp (value, "text/xsl", value_length) == 0 ||
                      uni_strncmp (value, "application/xml", value_length) == 0)
                    is_xslt_stylesheet = TRUE;
                }
              else if (attributes[index].GetName () == XMLExpandedName (UNI_L ("href")))
                {
                  if (href.Set (attributes[index].GetValue (), attributes[index].GetValueLength ()) == OpStatus::ERR_NO_MEMORY)
                    return XMLTokenHandler::RESULT_OOM;
                }

            if (is_xslt_stylesheet)
              return StartParsingSourceTree (token, handler->GetDocumentURL (), href.CStr ());

            /* Otherwise enqueue and continue analyzing. */
          }

          if (OpStatus::IsMemoryError (EnqueueToken (token)))
            return XMLTokenHandler::RESULT_OOM;
          else
            return XMLTokenHandler::RESULT_OK;

        case XMLToken::TYPE_STag:
        case XMLToken::TYPE_EmptyElemTag:
          return Disable (token);

        default:
          OP_ASSERT (token.GetType () == XMLToken::TYPE_XMLDecl || token.GetType () == XMLToken::TYPE_DOCTYPE || token.GetType () == XMLToken::TYPE_Comment);

          if (OpStatus::IsMemoryError (EnqueueToken (token)))
            return XMLTokenHandler::RESULT_OOM;
          else
            return XMLTokenHandler::RESULT_OK;
        }
    }
  else if (GetState () == XSLT_HandlerTokenHandler::PARSING_SOURCETREE)
    {
      XMLTokenHandler::Result result = token_handler->HandleToken (token);
      if (result != XMLTokenHandler::RESULT_OK)
        return result;

      if (token.GetType () == XMLToken::TYPE_Finished)
        {
          tree_collector_finished = TRUE;
          token_handler = 0;

          if (stylesheet_parser_finished)
            StartTransformation (TRUE);

          return XMLTokenHandler::RESULT_BLOCK;
        }
      else
        return XMLTokenHandler::RESULT_OK;
    }
  else if (XMLTokenHandler *token_handler = GetTokenHandler ())
    return token_handler->HandleToken (token);
  else
    return XMLTokenHandler::RESULT_ERROR;
}

/* virtual */ void
XSLT_HandlerTokenHandler::SetSourceCallback (XMLTokenHandler::SourceCallback *new_source_callback)
{
  if (state == XSLT_HandlerTokenHandler::DISABLED)
    token_handler->SetSourceCallback (new_source_callback);
  else
    source_callback = new_source_callback;
}

/* virtual */ BOOL
XSLT_HandlerTokenHandler::GetLiteralIsWhitespace ()
{
  return FALSE;
}

/* virtual */ const uni_char *
XSLT_HandlerTokenHandler::GetLiteralSimpleValue ()
{
  return queued_tokens.Get (queue_offset)->data.CStr ();
}

/* virtual */ uni_char *
XSLT_HandlerTokenHandler::GetLiteralAllocatedValue ()
{
  return UniSetNewStr (GetLiteralSimpleValue ());
}

/* virtual */ unsigned
XSLT_HandlerTokenHandler::GetLiteralLength ()
{
  return queued_tokens.Get (queue_offset)->data.Length ();
}

/* virtual */ OP_STATUS
XSLT_HandlerTokenHandler::GetLiteral (XMLToken::Literal &literal)
{
  RETURN_IF_ERROR (literal.SetPartsCount (1));
  RETURN_IF_ERROR (literal.SetPart (0, GetLiteralSimpleValue (), GetLiteralLength (), TRUE));
  return OpStatus::OK;
}

/* virtual */ void
XSLT_HandlerTokenHandler::ReleaseLiteralPart (unsigned index)
{
}

#ifdef XML_ERRORS

/* virtual */ BOOL
XSLT_HandlerTokenHandler::GetTokenRange (XMLRange &range)
{
  return FALSE;
}

/* virtual */ BOOL
XSLT_HandlerTokenHandler::GetAttributeRange (XMLRange &range, unsigned index)
{
  return FALSE;
}

#endif // XML_ERRORS

/* virtual */ OP_STATUS
XSLT_HandlerTokenHandler::LoadOtherStylesheet (URL stylesheet_url, XMLTokenHandler *token_handler, BOOL is_import)
{
  return handler->LoadResource (is_import ? XSLT_Handler::RESOURCE_IMPORTED_STYLESHEET : XSLT_Handler::RESOURCE_INCLUDED_STYLESHEET, stylesheet_url, token_handler);
}

/* virtual */ void
XSLT_HandlerTokenHandler::CancelLoadOtherStylesheet (XMLTokenHandler *token_handler)
{
  handler->CancelLoadResource (token_handler);
}

#ifdef XSLT_ERRORS

/* virtual */ OP_BOOLEAN
XSLT_HandlerTokenHandler::HandleMessage (XSLT_StylesheetParser::Callback::MessageType type, const uni_char *message)
{
  return handler->HandleMessage ((XSLT_Handler::MessageType) type, message);
}

#endif // XSLT_ERRORS

/* virtual */ OP_STATUS
XSLT_HandlerTokenHandler::ParsingFinished (XSLT_StylesheetParser *parser)
{
  stylesheet_parser_finished = TRUE;

  if (tree_collector_finished)
    StartTransformation (TRUE);

  return OpStatus::OK;
}

/* virtual */ void
XSLT_HandlerTokenHandler::ContinueTransformation (XSLT_Stylesheet::Transformation *transformation)
{
  if (!message_posted)
    message_posted = PostMessage (FALSE);
}

/* virtual */ OP_STATUS
XSLT_HandlerTokenHandler::LoadDocument (URL document_url, XMLTokenHandler *token_handler)
{
  return handler->LoadResource (XSLT_Handler::RESOURCE_LOADED_DOCUMENT, document_url, token_handler);
}

/* virtual */ void
XSLT_HandlerTokenHandler::CancelLoadDocument (XMLTokenHandler *token_handler)
{
  handler->CancelLoadResource (token_handler);
}

#ifdef XSLT_ERRORS

/* virtual */ OP_BOOLEAN
XSLT_HandlerTokenHandler::HandleMessage (XSLT_Stylesheet::Transformation::Callback::MessageType type, const uni_char *message)
{
  /* A static_cast should be valid for converting between enumerated types, but
     apparently GCC 2.95 doesn't think so. */
  return handler->HandleMessage ((XSLT_Handler::MessageType) type, message);
}

#endif // XSLT_ERRORS

void
XSLT_HandlerTokenHandler::ContinueTransformation ()
{
 again:
  XSLT_Stylesheet::Transformation::Status status = stylesheet_transformation->Transform ();

  if (status == XSLT_Stylesheet::Transformation::TRANSFORM_PAUSED)
    {
      if (!message_posted)
        message_posted = PostMessage (FALSE);
    }
  else if (status == XSLT_Stylesheet::Transformation::TRANSFORM_BLOCKED)
    /* No need to do anything. */
    return;
  else if (status == XSLT_Stylesheet::Transformation::TRANSFORM_NEEDS_OUTPUTHANDLER)
    {
      XSLT_Stylesheet::OutputSpecification::Method output_method = stylesheet_transformation->GetOutputMethod ();
      OP_STATUS status;

      if (output_method == XSLT_Stylesheet::OutputSpecification::METHOD_XML)
        {
          if (OpStatus::IsSuccess (status = handler->OnXMLOutput (token_handler, destroy_when_finished)))
            {
              stylesheet_transformation->SetDelayedOutputForm (XSLT_Stylesheet::OUTPUT_XMLTOKENHANDLER);
              stylesheet_transformation->SetXMLTokenHandler (token_handler, FALSE);
            }
        }
      else
        {
          OP_ASSERT (output_method == XSLT_Stylesheet::OutputSpecification::METHOD_HTML);

          if (OpStatus::IsSuccess (status = handler->OnHTMLOutput (string_data_collector, destroy_when_finished)))
            {
              stylesheet_transformation->SetDelayedOutputForm (XSLT_Stylesheet::OUTPUT_STRINGDATA);
              stylesheet_transformation->SetStringDataCollector (string_data_collector, FALSE);
            }
        }

      if (OpStatus::IsError (status))
        AbortTransformation (OpStatus::IsMemoryError (status));

      goto again;
    }
  else if (status == XSLT_Stylesheet::Transformation::TRANSFORM_FINISHED)
    {
      XSLT_Stylesheet::StopTransformation (stylesheet_transformation);
      stylesheet_transformation = 0;

      if (source_callback)
        {
          source_callback->Continue (XMLTokenHandler::SourceCallback::STATUS_CONTINUE);
          source_callback = 0;
        }

      OP_DELETE (tree_collector);
      tree_collector = 0;

      mh->UnsetCallBacks(this);

      handler->OnFinished ();
    }
  else if (status == XSLT_Stylesheet::Transformation::TRANSFORM_FAILED ||
           status == XSLT_Stylesheet::Transformation::TRANSFORM_NO_MEMORY)
    AbortTransformation (status == XSLT_Stylesheet::Transformation::TRANSFORM_NO_MEMORY);
}

void
XSLT_HandlerTokenHandler::AbortTransformation (BOOL oom)
{
  XSLT_Stylesheet::StopTransformation (stylesheet_transformation);
  stylesheet_transformation = 0;

  if (source_callback)
    {
      source_callback->Continue (oom ? XMLTokenHandler::SourceCallback::STATUS_ABORT_OOM : XMLTokenHandler::SourceCallback::STATUS_ABORT_FAILED);
      source_callback = 0;
    }

  OP_DELETE (tree_collector);
  tree_collector = 0;

  if (mh)
    mh->UnsetCallBacks(this);

  handler->OnAborted ();
}

/* virtual */ BOOL
XSLT_HandlerTokenHandler::PostMessage (BOOL start_transformation)
{
#ifdef SELFTEST
  if (post_message_cb)
    return post_message_cb->PostMessage (start_transformation);
  else
#endif // SELFTEST
    return mh->PostMessage (MSG_XSLT_CONTINUE_TRANSFORMATION, 0, start_transformation);
}

XSLT_HandlerTokenHandler::~XSLT_HandlerTokenHandler ()
{
  queued_tokens.DeleteAll ();

  if (destroy_when_finished)
    {
      OP_DELETE (token_handler);
      OP_DELETE (string_data_collector);
    }

  handler->CancelLoadResource (stylesheet_parser_token_handler);

  OP_DELETE (stylesheet_parser_token_handler);
  XSLT_StylesheetParser::Free (stylesheet_parser);

  if (stylesheet_transformation)
    XSLT_Stylesheet::StopTransformation (stylesheet_transformation);
  XSLT_Stylesheet::Free (stylesheet);

  OP_DELETE (tree_collector);
  OP_DELETE (mh);
}

/* virtual */ void
XSLT_HandlerTokenHandler::HandleCallback (OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 start_transformation)
{
  if (message_posted)
    {
      message_posted = FALSE;

      if (start_transformation)
        StartTransformation (FALSE);
      else
        ContinueTransformation ();
    }
}

XMLTokenHandler::Result
XSLT_HandlerTokenHandler::EnqueueToken (const XMLToken &token)
{
  QueuedToken *qt = OP_NEW (QueuedToken, ());
  if (!qt)
    return XMLTokenHandler::RESULT_OOM;
  OpAutoPtr<QueuedToken> qt_anchor (qt);

  qt->type = token.GetType ();

#define RETURN_IF_OOM(expr) do { if ((expr) == OpStatus::ERR_NO_MEMORY) return XMLTokenHandler::RESULT_OOM; } while (0)

  if (qt->type != XMLToken::TYPE_Comment)
    RETURN_IF_OOM (qt->name.Set (token.GetName ().GetLocalPart (), token.GetName ().GetLocalPartLength ()));

  if (qt->type == XMLToken::TYPE_PI)
    {
      RETURN_IF_OOM (qt->data.Set (token.GetData (), token.GetDataLength ()));
      unsigned int attribute_count = token.GetAttributesCount();
      if (attribute_count)
        {
          XMLToken::Attribute* token_attributes = token.GetAttributes();
          qt->attributes.SetAllocationStepSize(attribute_count);
          for (unsigned i = 0; i < attribute_count; i++)
            {
              OpAutoPtr<QueuedToken::Attribute> queue_attr(OP_NEW (QueuedToken::Attribute, ()));
              if (!queue_attr.get())
                return XMLTokenHandler::RESULT_OOM;
              RETURN_IF_OOM (queue_attr->name.Set(token_attributes[i].GetName()));
              RETURN_IF_OOM (queue_attr->value.Append(token_attributes[i].GetValue(), token_attributes[i].GetValueLength()));
              RETURN_IF_OOM (qt->attributes.Add(queue_attr.get()));
              queue_attr.release();
            }
        }
    }
  else if (qt->type == XMLToken::TYPE_Comment)
    {
      const uni_char *token_data = token.GetLiteralSimpleValue ();
      if (token_data)
        RETURN_IF_OOM (qt->data.Set (token_data, token.GetLiteralLength ()));
      else
        {
          uni_char *data = token.GetLiteralAllocatedValue (); ANCHOR_ARRAY (uni_char, data);
          RETURN_IF_OOM (qt->data.Set (data, token.GetLiteralLength ()));
        }
    }

  RETURN_IF_OOM (queued_tokens.Add (qt));

#undef RETURN_IF_OOM

  qt_anchor.release ();

  return XMLTokenHandler::RESULT_OK;
}

XMLTokenHandler::Result
XSLT_HandlerTokenHandler::FlushQueue ()
{
  while (queue_offset != queued_tokens.GetCount ())
    {
      QueuedToken *qt = queued_tokens.Get (queue_offset);
      XMLToken token (parser, this);

      token.SetType (qt->type);

      if (qt->type != XMLToken::TYPE_Comment)
        token.SetName (XMLCompleteName (qt->name.CStr ()));

      if (qt->type == XMLToken::TYPE_PI)
      {
        token.SetData (qt->data.CStr (), qt->data.Length ());
        for (UINT32 i = 0; i < qt->attributes.GetCount(); i++)
          {
            XMLToken::Attribute* token_attribute;
            if (OpStatus::IsError(token.AddAttribute(token_attribute)))
              return XMLTokenHandler::RESULT_OOM;
            QueuedToken::Attribute* queue_attr = qt->attributes.Get(i);
            token_attribute->SetName(queue_attr->name);
            token_attribute->SetValue(queue_attr->value.CStr(), queue_attr->value.Length(), TRUE, TRUE);
          }
      }

      XMLTokenHandler::Result result = token_handler->HandleToken (token);

      if (result != XMLTokenHandler::RESULT_OOM)
        ++queue_offset;

      /* This we don't like.  Supporting it is possible, but then we
         need to be capable of queueing any kind of token. */
      OP_ASSERT (result != XMLTokenHandler::RESULT_BLOCK);

      if (result != XMLTokenHandler::RESULT_OK)
        return result;
    }

  queued_tokens.DeleteAll ();
  queue_offset = 0;

  return XMLTokenHandler::RESULT_OK;
}

XMLTokenHandler::Result
XSLT_HandlerTokenHandler::StartParsingSourceTree (XMLToken &token, URL document_url, const uni_char *stylesheet_url_string)
{
  URL stylesheet_url = g_url_api->GetURL (document_url, stylesheet_url_string);

  if (OpStatus::IsMemoryError (XSLT_StylesheetParser::Make (stylesheet_parser, this)) ||
      OpStatus::IsMemoryError (XMLLanguageParser::MakeTokenHandler (stylesheet_parser_token_handler, stylesheet_parser, stylesheet_url.UniRelName ())))
    return XMLTokenHandler::RESULT_OOM;

  OP_STATUS status = handler->LoadResource (XSLT_Handler::RESOURCE_LINKED_STYLESHEET, stylesheet_url, stylesheet_parser_token_handler);

  if (OpStatus::IsMemoryError (status))
    return XMLTokenHandler::RESULT_OOM;
  else if (OpStatus::IsError (status))
    {
      OP_DELETE (stylesheet_parser_token_handler);
      stylesheet_parser_token_handler = 0;

      XSLT_StylesheetParser::Free (stylesheet_parser);
      stylesheet_parser = 0;

      return Disable (token);
    }

  if (OpStatus::IsMemoryError (handler->StartCollectingSourceTree (tree_collector)))
    return XMLTokenHandler::RESULT_OOM;

  token_handler = tree_collector->GetTokenHandler ();
  destroy_when_finished = FALSE;

  XMLTokenHandler::Result result = FlushQueue ();

  if (result != XMLTokenHandler::RESULT_OK)
    return result;

  state = PARSING_SOURCETREE;

  return token_handler->HandleToken (token);
}

XMLTokenHandler::Result
XSLT_HandlerTokenHandler::Disable (XMLToken &token)
{
  state = DISABLED;

  OP_STATUS status = handler->OnXMLOutput(token_handler, destroy_when_finished);

  if (status == OpStatus::ERR_NO_MEMORY)
    return XMLTokenHandler::RESULT_OOM;
  else if (status == OpStatus::ERR)
    {
      token_handler = 0;

      queued_tokens.DeleteAll ();
      queue_offset = 0;

      return XMLTokenHandler::RESULT_ERROR;
    }

  XMLTokenHandler::Result result = FlushQueue ();

  if (result != XMLTokenHandler::RESULT_OK)
    return result;

  return token_handler->HandleToken (token);
}

void
XSLT_HandlerTokenHandler::StartTransformation (BOOL postpone)
{
  enum UseMode { UNKNOWN, USE_XML, USE_HTML, USE_TEXT };

  OP_STATUS status = OpStatus::OK;

  if (!mh)
    {
      mh = OP_NEW (MessageHandler, (0, 0));

      if (!mh || mh->SetCallBack (this, MSG_XSLT_CONTINUE_TRANSFORMATION, 0) == OpStatus::ERR_NO_MEMORY)
        {
          status = OpStatus::ERR_NO_MEMORY;
          goto handle_error;
        }
    }

  if (postpone)
    message_posted = PostMessage (TRUE);
  else
    {
#define HANDLE_ERROR(expr) do { status = (expr); if (OpStatus::IsError (status)) goto handle_error; } while (0)

      HANDLE_ERROR (stylesheet_parser->GetStylesheet (stylesheet));

      if (static_cast<XSLT_StylesheetImpl *> (stylesheet)->HasStripSpaceElements ())
        tree_collector->StripWhitespace (stylesheet);

      XSLT_Stylesheet::Input input;

      HANDLE_ERROR (tree_collector->GetTreeAccessor (input.tree));
      input.node = input.tree->GetRoot ();

      const XSLT_Stylesheet::OutputSpecification *output_specification = stylesheet->GetOutputSpecification ();

      XSLT_Stylesheet::OutputForm output_form = XSLT_Stylesheet::OUTPUT_DELAYED_DECISION;
      UseMode use_mode = UNKNOWN;

      if (output_specification->method == XSLT_Stylesheet::OutputSpecification::METHOD_XML)
        {
          output_form = XSLT_Stylesheet::OUTPUT_XMLTOKENHANDLER;
          use_mode = USE_XML;
        }
      else if (output_specification->method == XSLT_Stylesheet::OutputSpecification::METHOD_HTML)
        {
          output_form = XSLT_Stylesheet::OUTPUT_STRINGDATA;
          use_mode = USE_HTML;
        }
      else if (output_specification->method == XSLT_Stylesheet::OutputSpecification::METHOD_TEXT)
        {
          output_form = XSLT_Stylesheet::OUTPUT_STRINGDATA;
          use_mode = USE_TEXT;
        }

      if (output_specification->media_type)
        if (uni_str_eq (output_specification->media_type, "text/plain"))
          {
            output_form = XSLT_Stylesheet::OUTPUT_STRINGDATA;
            use_mode = USE_TEXT;
          }
        else if (uni_str_eq (output_specification->media_type, "text/html"))
          {
            output_form = XSLT_Stylesheet::OUTPUT_STRINGDATA;
            use_mode = USE_HTML;
          }

      HANDLE_ERROR (stylesheet->StartTransformation (stylesheet_transformation, input, output_form));

      stylesheet_transformation->SetCallback (&transformation_callback);

      if (use_mode == USE_XML)
        {
          if (OpStatus::IsSuccess (status = handler->OnXMLOutput (token_handler, destroy_when_finished)))
            stylesheet_transformation->SetXMLTokenHandler (token_handler, FALSE);
        }
      else if (use_mode == USE_HTML || use_mode == USE_TEXT)
        {
          if (use_mode == USE_HTML)
            status = handler->OnHTMLOutput (string_data_collector, destroy_when_finished);
          else
            status = handler->OnTextOutput (string_data_collector, destroy_when_finished);

          if (OpStatus::IsSuccess (status))
            stylesheet_transformation->SetStringDataCollector (string_data_collector, FALSE);
        }
      else
        status = OpStatus::OK;

      if (status == OpStatus::OK)
        message_posted = PostMessage (FALSE);

      if (status == OpStatus::ERR)
        {
          status = XSLT_Stylesheet::StopTransformation (stylesheet_transformation);
          stylesheet_transformation = 0;
        }

#undef HANDLE_ERROR
    }

 handle_error:
  if (OpStatus::IsError (status))
    AbortTransformation (OpStatus::IsMemoryError (status));
}

#endif // XSLT_HANDLER_SUPPORT
