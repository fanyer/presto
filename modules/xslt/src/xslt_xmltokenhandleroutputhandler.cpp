/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_xmltokenhandleroutputhandler.h"
# include "modules/xslt/src/xslt_transformation.h"
# include "modules/xslt/src/xslt_engine.h"

XSLT_XMLTokenHandlerOutputHandler::XSLT_XMLTokenHandlerOutputHandler (XMLTokenHandler *tokenhandler, XMLParser::ParseMode parsemode, XSLT_TransformationImpl *transformation)
  : XSLT_XMLOutputHandler (transformation->GetStylesheet ()),
    tokenhandler (tokenhandler),
    parsemode (parsemode),
    parser (0),
    token (0),
    transformation (transformation),
    literal (0)
{
  literal_buffer.SetExpansionPolicy (TempBuffer::AGGRESSIVE);
}

/* virtual */
XSLT_XMLTokenHandlerOutputHandler::~XSLT_XMLTokenHandlerOutputHandler ()
{
  OP_DELETE (parser);
  OP_DELETE (token);
}

OP_STATUS
XSLT_XMLTokenHandlerOutputHandler::Construct ()
{
  RETURN_IF_ERROR (XMLParser::Make (parser, this, static_cast<MessageHandler *> (0), tokenhandler, transformation->GetInputTree ()->GetDocumentURL ()));

  if (parsemode != XMLParser::PARSEMODE_DOCUMENT)
    {
      XMLParser::Configuration configuration;
      configuration.parse_mode = parsemode;
      parser->SetConfiguration (configuration);
    }

  token = OP_NEW (XMLToken, (parser, this));
  if (!token)
    return OpStatus::ERR_NO_MEMORY;

  return OpStatus::OK;
}

void
XSLT_XMLTokenHandlerOutputHandler::FlushCharacterDataL ()
{
  if (literal_buffer.GetStorage () && *literal_buffer.GetStorage ())
    {
      ProcessTokenL (UseCDATASections () ? XMLToken::TYPE_CDATA : XMLToken::TYPE_Text);
      literal_buffer.Clear ();
    }
}

/* virtual */ void
XSLT_XMLTokenHandlerOutputHandler::AddTextL (const uni_char *data, BOOL disable_output_escaping)
{
  XSLT_XMLOutputHandler::AddTextL (data, disable_output_escaping);

  if (disable_output_escaping)
    {
      FlushCharacterDataL ();

      LEAVE_IF_ERROR (parser->Parse (data, uni_strlen (data), TRUE));

      if (parser->IsFailed ())
        {
#ifdef XSLT_ERRORS
#ifdef XML_ERRORS
          const char *error, *uri, *fragment_id;
          parser->GetErrorDescription (error, uri, fragment_id);

          OpString8 error_message; ANCHOR (OpString8, error_message);
          LEAVE_IF_ERROR (error_message.AppendFormat ("invalid XML output: %s", error));

          XSLT_Instruction *instruction = transformation->GetEngine ()->GetCurrentInstruction ();
          XSLT_SignalErrorL (transformation, instruction ? instruction->origin : 0, error_message.CStr ());
#else // XML_ERRORS
          XSLT_Instruction *instruction = transformation->GetEngine ()->GetCurrentInstruction ();
          XSLT_SignalErrorL (transformation, instruction ? instruction->origin : 0, "invalid XML output");
#endif // XML_ERRORS
#else // XSLT_ERRORS
          LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
        }
    }
  else
    literal_buffer.AppendL (data);
}

/* virtual */ void
XSLT_XMLTokenHandlerOutputHandler::AddCommentL (const uni_char *data)
{
  XSLT_XMLOutputHandler::AddCommentL (data);

  FlushCharacterDataL ();

  literal = data;
  ProcessTokenL (XMLToken::TYPE_Comment);
  literal = 0;
}

/* virtual */ void
XSLT_XMLTokenHandlerOutputHandler::AddProcessingInstructionL (const uni_char *target, const uni_char *data)
{
  XSLT_XMLOutputHandler::AddProcessingInstructionL (target, data);

  FlushCharacterDataL ();

  token->SetName (XMLCompleteNameN (target, uni_strlen (target)));
  token->SetData (data, uni_strlen (data));

  ProcessTokenL (XMLToken::TYPE_PI);
}

/* virtual */ void
XSLT_XMLTokenHandlerOutputHandler::EndOutputL ()
{
  FlushCharacterDataL ();

  XSLT_XMLOutputHandler::EndOutputL ();

  ProcessTokenL (XMLToken::TYPE_Finished);
}

/* virtual */ void
XSLT_XMLTokenHandlerOutputHandler::OutputXMLDeclL (const XMLDocumentInformation &document_info)
{
  /* FIXME: According to XMLToken's documentation, this token should have
     attributes.  Don't think anyone uses them, so I'd like the documentation
     to be changed rather than to implement it.  Fetching information from the
     document information object is way more convenient. */

  token->SetName (XMLCompleteName (UNI_L ("xml")));
  token->SetDocumentInformation (&document_info);
  ProcessTokenL (XMLToken::TYPE_XMLDecl);
}

/* virtual */ void
XSLT_XMLTokenHandlerOutputHandler::OutputDocumentTypeDeclL (const XMLDocumentInformation &document_info)
{
  token->SetDocumentInformation (&document_info);
  ProcessTokenL (XMLToken::TYPE_DOCTYPE);
}

/* virtual */ void
XSLT_XMLTokenHandlerOutputHandler::OutputTagL (XMLToken::Type type, const XMLCompleteName &name)
{
  FlushCharacterDataL ();

  token->SetName (name);

  if (type != XMLToken::TYPE_ETag)
    for (unsigned index = 0; index < nsnormalizer.GetAttributesCount (); ++index)
      {
        XMLToken::Attribute *attribute;
        LEAVE_IF_ERROR (token->AddAttribute (attribute));

        attribute->SetName (nsnormalizer.GetAttributeName (index));

        const uni_char *value = nsnormalizer.GetAttributeValue (index);
        attribute->SetValue (value, uni_strlen (value), TRUE, TRUE);
      }

  ProcessTokenL (type);
}

/* virtual */ BOOL
XSLT_XMLTokenHandlerOutputHandler::GetLiteralIsWhitespace()
{
  return XMLUtils::IsWhitespace (XSLT_XMLTokenHandlerOutputHandler::GetLiteralSimpleValue ());
}

/* virtual */ const uni_char *
XSLT_XMLTokenHandlerOutputHandler::GetLiteralSimpleValue()
{
  if (literal)
    return literal;
  else if (literal_buffer.GetStorage ())
    return literal_buffer.GetStorage ();
  else
    return UNI_L ("");
}

/* virtual */ uni_char *
XSLT_XMLTokenHandlerOutputHandler::GetLiteralAllocatedValue()
{
  return UniSetNewStr (XSLT_XMLTokenHandlerOutputHandler::GetLiteralSimpleValue ());
}

/* virtual */ unsigned
XSLT_XMLTokenHandlerOutputHandler::GetLiteralLength()
{
  return uni_strlen (XSLT_XMLTokenHandlerOutputHandler::GetLiteralSimpleValue ());
}

/* virtual */ OP_STATUS
XSLT_XMLTokenHandlerOutputHandler::GetLiteral(XMLToken::Literal &literal0)
{
  RETURN_IF_ERROR (literal0.SetPartsCount (1));
  RETURN_IF_ERROR (literal0.SetPart (1, XSLT_XMLTokenHandlerOutputHandler::GetLiteralSimpleValue (), XSLT_XMLTokenHandlerOutputHandler::GetLiteralLength (), FALSE));
  return OpStatus::OK;
}

/* virtual */ void
XSLT_XMLTokenHandlerOutputHandler::ReleaseLiteralPart(unsigned index)
{
  OP_ASSERT (FALSE);
}

#ifdef XML_ERRORS

/* virtual */ BOOL
XSLT_XMLTokenHandlerOutputHandler::GetTokenRange(XMLRange &range)
{
  return FALSE;
}

/* virtual */ BOOL
XSLT_XMLTokenHandlerOutputHandler::GetAttributeRange(XMLRange &range, unsigned index)
{
  return FALSE;
}

#endif // XML_ERRORS

/* virtual */ void
XSLT_XMLTokenHandlerOutputHandler::Continue (XMLParser *parser)
{
  transformation->Continue ();
}

/* virtual */ void
XSLT_XMLTokenHandlerOutputHandler::Stopped (XMLParser *parser)
{
}

void
XSLT_XMLTokenHandlerOutputHandler::ProcessTokenL (XMLToken::Type type)
{
  token->SetType (type);

  LEAVE_IF_ERROR (parser->ProcessToken (*token));

  if (parser->IsFailed ())
    {
#ifdef XSLT_ERRORS
#ifdef XML_ERRORS
      const char *error, *uri, *fragment_id;
      parser->GetErrorDescription (error, uri, fragment_id);

      OpString8 error_message; ANCHOR (OpString8, error_message);
      LEAVE_IF_ERROR (error_message.AppendFormat ("invalid XML output: %s", error));

      XSLT_Instruction *instruction = transformation->GetEngine ()->GetCurrentInstruction ();
      XSLT_SignalErrorL (transformation, instruction ? instruction->origin : 0, error_message.CStr ());
#else // XML_ERRORS
      XSLT_Instruction *instruction = transformation->GetEngine ()->GetCurrentInstruction ();
      XSLT_SignalErrorL (transformation, instruction ? instruction->origin : 0, "invalid XML output");
#endif // XML_ERRORS
#else // XSLT_ERRORS
      LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
    }

  token->Initialize ();

  if (parser->IsBlockedByTokenHandler ())
    transformation->GetEngine ()->SetIsProgramBlocked ();
}

#endif // XSLT_SUPPORT
