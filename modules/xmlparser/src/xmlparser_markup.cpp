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

#if 0
# include "modules/newdebug/src/profiler.h"
# define MARKUP_PROFILE_PARSER(id) OP_PROFILE(id)
#else
# define MARKUP_PROFILE_PARSER(id)
#endif

void
XMLInternalParser::ReadDocument ()
{
  MARKUP_PROFILE_PARSER ("MP1:Document");

  has_seen_linebreak = TRUE;

  if (length == 0)
    GrowBetweenMarkup ();

  if (in_text)
    goto read_text;

  if (in_comment)
    goto read_comment;

  if (in_cdata)
    goto read_cdata;

  while (1)
    {
      if (index == length)
        if (!GrowBetweenMarkup ())
          {
            expecting_eof = TRUE;
            LEAVE (PARSE_RESULT_END_OF_BUF);
          }

      if (buffer[index] == '<')
        {
          ConsumeChar ();

          if (buffer[index] == '?')
            {
              has_seen_linebreak = FALSE;
              ConsumeChar ();
              ReadPIToken ();
            }
          else if (buffer[index] == '!')
            {
              ConsumeChar ();

              if (Match (UNI_L ("--"), 2))
                read_comment: ReadCommentToken ();
              else if (Match (UNI_L ("[CDATA["), 7))
                read_cdata: ReadCDATAToken ();
              else if (Match (UNI_L ("DOCTYPE"), 7))
                if (!configuration->allow_doctype)
                  HandleError (PARSE_ERROR_NotAllowed_DOCTYPE);
                else if (has_doctype)
                  HandleError (PARSE_ERROR_Duplicate_DOCTYPE);
                else
                  {
                    ReadDOCTYPEToken ();
                    continue;
                  }
              else
                HandleError (PARSE_ERROR_Invalid_Syntax);
            }
          else
            {
              has_seen_linebreak = FALSE;

              if (buffer[index] == '/')
                {
                  ConsumeChar ();
                  ReadETagToken ();
                }
              else
                ReadSTagToken ();
            }
        }
      else read_text: if (!ReadTextToken ())
        continue;

#ifdef XML_ENTITY_REFERENCE_TOKENS
      if (entity_reference_tokens)
        do
          {
            BOOL block = !HandleToken ();

            current_buffer->Consume (TRUE);

            if (block)
              LEAVE (PARSE_RESULT_BLOCK);

            if (index == length && current_buffer->IsInEntityReference ())
              {
                token.SetType (XMLToken::TYPE_EntityReferenceEnd);
                token.SetEntity (current_buffer->GetCurrentEntity ());
                current_buffer->ConsumeEntityReference (TRUE);
              }
            else
              token.SetType (XMLToken::TYPE_Unknown);
          }
        while (token.GetType () == XMLToken::TYPE_EntityReferenceEnd);
      else
#endif // XML_ENTITY_REFERENCE_TOKENS
        {
          BOOL block = !HandleToken ();

          current_buffer->Consume (FALSE, !has_seen_linebreak);

          if (block)
            LEAVE (PARSE_RESULT_BLOCK);
        }

      has_seen_linebreak = TRUE;
    }
}

void
XMLInternalParser::ReadPIToken (BOOL in_doctype)
{
  MARKUP_PROFILE_PARSER ("MP2:PIToken");

  unsigned token_start = index - 2;

  if (!XML_READNCNAME ())
    HandleError (PARSE_ERROR_Invalid_PITarget);

  unsigned name_start = literal_start, name_length = literal_length;

  BOOL ws_after_name = ConsumeWhitespace ();

  unsigned data_start = index;

  if (literal_length == 3 && uni_strnicmp (literal, "xml", 3) == 0)
    if (uni_strncmp (literal, "xml", 3) == 0)
      {
        index = token_start;

        if (!configuration->allow_xml_declaration)
          HandleError (PARSE_ERROR_NotAllowed_XMLDeclaration);

        if (!ReadTextDecl (TRUE))
          HandleError (PARSE_ERROR_Invalid_XMLDecl);

        version = textdecl_version;
        standalone = textdecl_standalone;

        if (textdecl_encoding)
          CopyString (encoding, textdecl_encoding, textdecl_encoding_length);

        if (in_doctype)
          HandleError (WELL_FORMEDNESS_ERROR_OutOfPlace_XMLDecl);

        if (version == XMLVERSION_1_1)
          current_buffer->SetIsXML11 ();
      }
    else
      HandleError (PARSE_ERROR_Invalid_PITarget);
#ifdef XML_STYLESHEET
  else if (literal_length == 14 && ws_after_name && uni_strncmp (literal, "xml-stylesheet", 14) == 0)
    {
      /* ReadAttributes needs one whitespace character. */
      --index;

    again:
      unsigned buffer_id_before = buffer_id, index_before = index;

      /* FIXME: must make sure only predefined entities are used in the
         attribute values. */
      ReadAttributes ();

      if (buffer_id == buffer_id_before && !Match (UNI_L ("?>"), 2))
        HandleError (PARSE_ERROR_Invalid_XML_StyleSheet);

      if (buffer_id != buffer_id_before)
        {
          index = index_before;
          goto again;
        }
    }
#endif // XML_STYLESHEET
  else if (ws_after_name)
    {
      ScanFor (UNI_L ("?>"), 2);
      index += 2;
    }
  else if (!Match (UNI_L ("?>"), 2))
    HandleError (PARSE_ERROR_Invalid_PITarget);

  CheckValidChars (buffer + data_start, buffer + index - 2, TRUE);

  if (!in_doctype)
    {
      unsigned data_length = index - 2 - data_start;

      pi_data.Clear ();
      pi_data.AppendL (buffer + data_start, data_length);

      uni_char *data = pi_data.GetStorage ();

      if (data_length != 0)
        current_buffer->NormalizeLinebreaks (data, data_length);

      XMLCompleteNameN name (0, 0, 0, 0, buffer + name_start, name_length);

      token.SetType (XMLToken::TYPE_PI);
      token.SetName (name);
      token.SetData (data, data_length);
    }
}

void
XMLInternalParser::ReadCommentToken (BOOL in_doctype)
{
  MARKUP_PROFILE_PARSER ("MP3:Comment");

  if (!in_comment)
    {
      if (!in_doctype)
        {
          token.SetType (XMLToken::TYPE_Comment);
          current_buffer->LiteralStart (TRUE);

#ifdef XML_ERRORS
          current_buffer->GetLocation (0, token_start);
#endif // XML_ERRORS
        }
      else
        current_buffer->Consume ();

      in_comment = TRUE;
    }

  ScanFor (UNI_L ("--"), 2);
  current_buffer->LiteralEnd (TRUE);

  index += 2;

  if (index == length)
    GrowInMarkup ();

  if (buffer[index] != '>')
    HandleError (WELL_FORMEDNESS_ERROR_Invalid_Comment, index - 2, 2);

  ++index;
  in_comment = FALSE;

  current_buffer->Consume (entity_reference_tokens, TRUE);
}

void
XMLInternalParser::ReadCDATAToken ()
{
  MARKUP_PROFILE_PARSER ("MP4:CDATA");

  if (!in_cdata)
    {
      token.SetType (XMLToken::TYPE_CDATA);
      current_buffer->LiteralStart (TRUE);
      in_cdata = TRUE;

#ifdef XML_ERRORS
      current_buffer->GetLocation (0, token_start);
#endif // XML_ERRORS
    }

  ScanFor (UNI_L ("]]>"), 3);
  current_buffer->LiteralEnd (TRUE);
  in_cdata = FALSE;

  index += 3;
}

void
XMLInternalParser::ProcessAttribute (const uni_char *name, unsigned name_length, BOOL need_normalize, unsigned attrindex)
{
  XMLToken::Attribute *attr;
  LEAVE_IF_ERROR(token.AddAttribute (attr));

  const uni_char *value;
  unsigned value_length;
  BOOL need_copy;

#ifdef XML_STORE_ATTRIBUTES
  normalize_attribute = doctype->GetAttribute (token.GetName ().GetLocalPart (), token.GetName ().GetLocalPartLength (), name, name_length);
#endif // XML_STORE_ATTRIBUTES

  if (NormalizeAttributeValue (name, name_length, need_normalize))
    {
      uni_char *temporary;
      CopyString (temporary, normalize_buffer, normalize_buffer_count);

      value = temporary;
      value_length = normalize_buffer_count;
      need_copy = FALSE;
    }
  else
    {
      value = literal;
      value_length = literal_length;
      need_copy = TRUE;
    }

  XMLCompleteNameN attrname (name, name_length);

  attr->SetName (attrname);
  attr->SetValue (value, value_length, need_copy, TRUE);

#ifdef XML_STORE_ATTRIBUTES
  if (normalize_attribute && normalize_attribute->GetType () == XMLDoctype::Attribute::TYPE_Tokenized_ID)
    attr->SetId ();
#endif // XML_STORE_ATTRIBUTES
}

void
XMLInternalParser::ReadAttributes ()
{
  unsigned skip_attributes = token.GetAttributesCount (), attrindex = 0;
  unsigned buffer_id_before = buffer_id;

#ifdef XML_ERRORS
  while (ConsumeWhitespace () && (skip_attributes == 0 ? (StoreAttributeRange (attrindex, TRUE), 0) : 0, XML_READQNAME ()))
#else // XML_ERRORS
  while (ConsumeWhitespace () && XML_READQNAME ())
#endif // XML_ERRORS
    {
      const uni_char *attr_name = literal;
      unsigned attr_name_length = literal_length;

      ConsumeWhitespace ();

      if (buffer[index] != '=')
        HandleError (PARSE_ERROR_Invalid_STag_Attribute);

      ++index;

      ConsumeWhitespace ();

      BOOL unproblematic;

      if (!ReadQuotedLiteral (unproblematic))
        HandleError (PARSE_ERROR_Invalid_STag_AttValue);

#ifdef XML_ERRORS
      StoreAttributeRange (attrindex, FALSE);
#endif // XML_ERRORS

      if (buffer_id != buffer_id_before)
        return;

      if (skip_attributes == 0)
#ifdef XML_ERRORS
        ProcessAttribute (attr_name, attr_name_length, !unproblematic, attrindex);
#else // XML_ERRORS
        ProcessAttribute (attr_name, attr_name_length, !unproblematic);
#endif // XML_ERRORS
      else
        {
          XMLToken::Attribute *attr = token.GetAttributes () + attrindex;
          XMLCompleteNameN attrname (attr_name, attr_name_length);

          attr->SetName (attrname);

          if (!attr->OwnsValue())
            attr->SetValue (literal, literal_length, TRUE, TRUE);

          --skip_attributes;
        }

      ++attrindex;
    }
}

void
XMLInternalParser::ReadSTagToken ()
{
  MARKUP_PROFILE_PARSER ("MP5:STag");

  unsigned start = index;

again:
  if (!XML_READQNAME ())
    HandleError (PARSE_ERROR_Invalid_STag);

  XMLCompleteNameN elemname (literal, literal_length);

  token.SetType (XMLToken::TYPE_STag);
  token.SetName (elemname);

  unsigned buffer_id_before = buffer_id;

  ReadAttributes ();

  if (buffer[index] == '/')
    {
      ConsumeChar ();
      token.SetType (XMLToken::TYPE_EmptyElemTag);
    }

  if (buffer_id != buffer_id_before)
    {
      index = start;
      goto again;
    }

  if (buffer[index] != '>')
    HandleError (PARSE_ERROR_Invalid_STag);

  ++index;
}

void
XMLInternalParser::ReadETagToken ()
{
  MARKUP_PROFILE_PARSER ("MP6:ETag");

  unsigned start = index;

again:
  if (!XML_READQNAME ())
    HandleError (PARSE_ERROR_Invalid_ETag);

  XMLCompleteNameN elemname (literal, literal_length);

  token.SetType (XMLToken::TYPE_ETag);
  token.SetName (elemname);

  unsigned buffer_id_before = buffer_id;

  ConsumeWhitespace ();

  if (buffer[index] != '>')
    HandleError (PARSE_ERROR_Invalid_ETag);

  if (buffer_id != buffer_id_before)
    {
      index = start;
      goto again;
    }

  ++index;
}

#ifdef XML_ENTITY_REFERENCE_TOKENS
# define ENTITY_REFERENCE_TOKENS entity_reference_tokens
#else // XML_ENTITY_REFERENCE_TOKENS
# define ENTITY_REFERENCE_TOKENS
#endif // XML_ENTITY_REFERENCE_TOKENS

BOOL
XMLInternalParser::ReadTextToken ()
{
  MARKUP_PROFILE_PARSER ("MP7:Text");

  if (!in_text)
    {
      token.SetType (XMLToken::TYPE_Text);

      current_buffer->LiteralStart (entity_reference_tokens);
      in_text = TRUE;

#ifdef XML_ERRORS
      current_buffer->GetLocation (0, token_start);
#endif // XML_ERRORS
    }

  BOOL is_xml11 = version == XMLVERSION_1_1;

  while (1)
    {
      const uni_char *ptr_start = buffer + index, *ptr = ptr_start, *ptr_end = buffer + length;
      BOOL unproblematic = TRUE, in_entity = current_buffer->IsInEntityReference ();
      unsigned back = 0;

      if (in_entity)
        {
          while (ptr != ptr_end && *ptr != '<' && *ptr != '&' && *ptr != ']') ++ptr;
          goto done;
        }
      else if (!is_xml11)
        {
          while (ptr != ptr_end && *ptr != '<' && *ptr != '&' && *ptr != ']' && (*ptr >= 0x20 || *ptr == 0x0a || *ptr == 0x0d) && *ptr < 0x80) ++ptr;
          if (ptr == ptr_end || *ptr == '<' || *ptr == '&' || *ptr == ']')
            goto done;
        }

      unproblematic = in_entity;
      while (ptr != ptr_end && *ptr != '<' && *ptr != '&' && *ptr != ']') ++ptr;

    done:
      if (ptr != ptr_start)
        {
          index += ptr - ptr_start;

          if (!in_entity)
            {
              if (!unproblematic)
                CheckValidChars (ptr_start, ptr, ptr != ptr_end);

              if (ptr == ptr_end)
                {
                  /* If the last character might be the first of a surrogate
                     pair, leave it so that we can check the whole surrogate
                     pair later. */
                  uni_char last_ch = *(ptr - 1);
                  if (last_ch >= 0xd800 && last_ch <= 0xdbff)
                    {
                      --index;
                      ++back;
                    }
                }

              current_buffer->Consume (ENTITY_REFERENCE_TOKENS);

              if (ptr == ptr_end)
                {
                  index += back;

                  if (index == length)
                    goto grow;
                }
            }
          else
            {
              current_buffer->Consume (ENTITY_REFERENCE_TOKENS);
              in_entity = current_buffer->IsInEntityReference ();
            }

          /* Consuming characters might change buffer, index and length. */

          ptr = buffer + index;
          ptr_end = buffer + length;
        }

      XMLDoctype::Entity *entity;

      if (ptr != ptr_end)
        {
          if (*ptr == '<')
            goto end;
          else if (*ptr == '&')
            {
              if (index != 0)
                current_buffer->Consume (ENTITY_REFERENCE_TOKENS);

#ifdef XML_ENTITY_REFERENCE_TOKENS
              if (ReadReference ())
                {
                  if (!is_character_reference)
                    if (reference_entity && ++token_counter == max_tokens_per_call)
                      {
                        token_counter = 0;
                        is_paused = TRUE;
                        LEAVE (PARSE_RESULT_BLOCK);
                      }
                }
              else
                /* We will signal a ReferenceStart token, so this Text
                   token ends here. */
                goto end;
#else // XML_ENTITY_REFERENCE_TOKENS
              ReadReference ();

              if (!is_character_reference)
                if (reference_entity && ++token_counter == max_tokens_per_call)
                  {
                    token_counter = 0;
                    is_paused = TRUE;
                    LEAVE (PARSE_RESULT_BLOCK);
                  }
#endif // XML_ENTITY_REFERENCE_TOKENS
            }
          else if (*ptr == ']')
            {
              ++ptr;
              ++index;
              ++back;

              if (ptr == ptr_end)
                {
                  if (in_entity || current_buffer->IsAllSeen ())
                    {
                      current_buffer->Consume (ENTITY_REFERENCE_TOKENS);
                      back = 0;
                    }

                  goto grow;
                }
              else if (*ptr == ']')
                {
                  ++ptr;
                  ++index;
                  ++back;

                  if (ptr == ptr_end)
                    {
                      if (in_entity || current_buffer->IsAllSeen ())
                        {
                          current_buffer->Consume (ENTITY_REFERENCE_TOKENS);
                          back = 0;
                        }

                      goto grow;
                    }
                  else if (*ptr == '>')
                    HandleError (WELL_FORMEDNESS_ERROR_Invalid_CDATA_End);

                  --index;
                }
            }
        }
      else grow: if (entity = current_buffer->GetCurrentEntity (), !GrowBetweenMarkup ())
        break;
      else if (entity == current_buffer->GetCurrentEntity ())
        index -= back;
    }

end:
  in_text = FALSE;
  current_buffer->LiteralEnd (entity_reference_tokens);

#ifdef XML_ENTITY_REFERENCE_TOKENS
  return current_buffer->GetLiteralLength () != 0 || token.GetType () == XMLToken::TYPE_EntityReferenceStart;
#else // XML_ENTITY_REFERENCE_TOKENS
  return current_buffer->GetLiteralLength () != 0;
#endif // XML_ENTITY_REFERENCE_TOKENS
}

BOOL
XMLInternalParser::ReadReference ()
{
  MARKUP_PROFILE_PARSER ("MP8:ReadReference");

  ConsumeEntityReference ();

  if (!is_character_reference)
    {
#ifdef XML_ENTITY_REFERENCE_TOKENS
      if (entity_reference_tokens && last_token_type != XMLToken::TYPE_EntityReferenceStart)
        {
          index = reference_start;

          if (current_buffer->GetLiteralLength () == 0)
            {
              token.SetType (XMLToken::TYPE_EntityReferenceStart);
              token.SetEntity (reference_entity);
            }

          return FALSE;
        }

      if (last_token_type == XMLToken::TYPE_EntityReferenceStart)
        last_token_type = XMLToken::TYPE_Unknown;
#endif // XML_ENTITY_REFERENCE_TOKENS

      if (!reference_entity)
        {
#ifdef XML_VALIDATING_ENTITY_DECLARED
          if (xml_validating)
            HandleError (VALIDITY_ERROR_Undeclared_Entity, reference_start);
#endif // XML_VALIDATING_ENTITY_DECLARED
        }
      else if (!reference_entity->GetValue ())
        {
#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
          /* Will LEAVE if the entity is loaded. */
          LoadEntity (reference_entity, PARSE_CONTEXT_GENERAL_ENTITY);
#endif // XML_SUPPORT_EXTERNAL_ENTITIES

          /* If LoadEntity didn't LEAVE, skip the reference. */
        }
      else
        {
          if (current_buffer->IsInEntityReference () && nested_entity_references_allowed)
            if (!--nested_entity_references_allowed)
              HandleError (PARSE_ERROR_TooManyNestedEntityReferences, reference_start);

          current_buffer->ExpandEntityReference (reference_length, reference_entity);
        }
    }
  else
    current_buffer->ExpandCharacterReference (reference_length, reference_character_value);

  return TRUE;
}
