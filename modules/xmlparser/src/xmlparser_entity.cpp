/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlparser/xmlcommon.h"
#include "modules/xmlparser/xmlinternalparser.h"
#include "modules/xmlparser/xmlbuffer.h"
#include "modules/xmlparser/xmldatasource.h"
#include "modules/xmlparser/xmldoctype.h"
#include "modules/xmlparser/src/xmlcheckingtokenhandler.h"

void
XMLInternalParser::LoadEntity (XMLDoctype::Entity *entity, ParseContext context)
{
  XMLDataSource *source;

  if (!datasource_handler)
    {
      skip_remaining_doctype = TRUE;
      return;
    }

  if (entity->GetValue ())
    LEAVE_IF_ERROR (datasource_handler->CreateInternalDataSource (source, entity->GetValue (), entity->GetValueLength ()));
  else
    {
      source = 0;

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
      if (load_external_entities)
        LEAVE_IF_ERROR (datasource_handler->CreateExternalDataSource (source, entity->GetPubid (), entity->GetSystem (), entity->GetBaseURL ()));

      if (!source)
#endif // XML_SUPPORT_EXTERNAL_ENTITIES
        {
          skip_remaining_doctype = TRUE;
          return;
        }
    }

  OpStackAutoPtr<XMLDataSource> anchor (source);

  if (source)
    {
      XMLBuffer *buffer = OP_NEW_L(XMLBuffer, (source, version == XMLVERSION_1_1));
      buffer->Initialize (32768);
      source->SetBuffer (buffer);

      XMLInternalParserState *state = OP_NEW_L(XMLInternalParserState, ());
      state->context = context;
      state->entity = entity;
      source->SetParserState (state);
      source->SetNextSource (current_source);

      blocking_source = source;

      anchor.release ();
      LEAVE (PARSE_RESULT_BLOCK);
    }
}

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES

void
XMLInternalParser::ReadParameterEntity ()
{
  current_buffer->LiteralStart (TRUE);

  while (1)
    {
      index = length;
      current_buffer->Consume ();

      if (!current_buffer->GrowL (TRUE))
        if (current_buffer->IsAllSeen ())
          break;
        else
          LEAVE (PARSE_RESULT_END_OF_BUF);
    }

  index = length;
  current_buffer->Consume ();

  current_buffer->LiteralEnd (TRUE);

  BOOL copy = !current_buffer->GetLiteralIsSimple ();
  uni_char *value = current_buffer->GetLiteral (copy);

  if (!value)
    LEAVE (PARSE_RESULT_OOM);

  current_related_entity->SetValue (value, current_buffer->GetLiteralLength (), copy);

  expecting_eof = TRUE;
  LEAVE (PARSE_RESULT_END_OF_BUF);
}

void
XMLInternalParser::ReadGeneralEntity ()
{
  current_related_entity->SetValue (UNI_L (""), 0, FALSE);

  if (index == 0)
    if (ReadTextDecl ())
      {
        current_source->SetEncoding (textdecl_encoding, textdecl_encoding_length);
        current_buffer->Consume ();
      }

  ReadParameterEntity ();
}

#endif // XML_SUPPORT_EXTERNAL_ENTITIES

BOOL
XMLInternalParser::ReadTextDecl (BOOL xmldecl)
{
  unsigned start_index = index;

  textdecl_encoding_start = textdecl_encoding_length = 0;
  expecting_eof = TRUE;

  BOOL has_start = Match (UNI_L ("<?xml"), 5), unproblematic;

  expecting_eof = FALSE;

  ParseError error = xmldecl ? PARSE_ERROR_Invalid_XMLDecl : PARSE_ERROR_Invalid_TextDecl;

  if (has_start && ConsumeWhitespace ())
    {
      textdecl_version = XMLVERSION_1_0;
      textdecl_standalone = XMLSTANDALONE_NONE;
      textdecl_encoding = 0;
      textdecl_encoding_length = 0;

      if (Match (UNI_L ("version"), 7))
        {
          ConsumeWhitespace ();

          if (!Match (UNI_L ("="), 1))
            HandleError (error);

          ConsumeWhitespace ();

          if (!ReadQuotedLiteral (unproblematic) || index == length || literal_length != 3 || literal[0] != '1' || literal[1] != '.' || literal[2] != '0' && literal[2] != '1')
            HandleError ((ParseError) (error + 1));

          if (literal[2] == '0')
            textdecl_version = XMLVERSION_1_0;
          else
            textdecl_version = XMLVERSION_1_1;

          if (current_context != PARSE_CONTEXT_DOCUMENT && version < textdecl_version)
            HandleError (WELL_FORMEDNESS_ERROR_ExternalEntityHasLaterVersion);

          if (!ConsumeWhitespace ())
            if (!xmldecl)
              HandleError (error);
            else
              goto end;
        }
      else if (xmldecl)
        HandleError (error);

      if (Match (UNI_L ("encoding"), 8))
        {
          ConsumeWhitespace ();

          if (!Match (UNI_L ("="), 1))
            HandleError (error);

          ConsumeWhitespace ();

          if (!ReadQuotedLiteral (unproblematic) || index == length || literal_length == 0)
            HandleError (error);

          uni_char ch = literal[0];

          if ((ch < 'A' || ch > 'Z') && (ch < 'a' || ch > 'z'))
            HandleError ((ParseError) (error + 2));

          for (const uni_char *ptr = literal, *ptr_end = ptr + literal_length; ptr != ptr_end; ++ptr)
            {
              uni_char ch = *ptr;
              if ((ch < 'A' || ch > 'Z') && (ch < 'a' || ch > 'z') && (ch < '0' || ch > '9') && ch != '.' && ch != '_' && ch != '-')
                HandleError ((ParseError) (error + 2));
            }

          textdecl_encoding_start = literal_start;
          textdecl_encoding_length = literal_length;

          if (!ConsumeWhitespace ())
            goto end;
        }
      else if (!xmldecl)
        HandleError (error);

      unsigned index_before_sddecl = index;

      if (Match (UNI_L ("standalone"), 10))
        {
          ConsumeWhitespace ();

          if (!Match (UNI_L ("="), 1))
            HandleError (error);

          ConsumeWhitespace ();

          if (!ReadQuotedLiteral (unproblematic) || index == length)
            HandleError (error);

          if (!xmldecl)
            HandleError (error, index_before_sddecl, index - index_before_sddecl);

          if (literal_length == 2 && literal[0] == 'n' && literal[1] == 'o')
            textdecl_standalone = XMLSTANDALONE_NO;
          else if (literal_length == 3 && literal[0] == 'y' && literal[1] == 'e' && literal[2] == 's')
            textdecl_standalone = XMLSTANDALONE_YES;
          else
            HandleError ((ParseError) (error + 3));

          ConsumeWhitespace ();
        }
    }
  else
    {
      index = start_index;
      return FALSE;
    }

end:
  if (!Match (UNI_L ("?>"), 2))
    HandleError (error);

  textdecl_encoding = textdecl_encoding_start != 0 ? buffer + textdecl_encoding_start : 0;
  return TRUE;
}
