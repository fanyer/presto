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
#include "modules/xmlparser/xmldoctype.h"

#define REWRITTEN_PUSHSTRING_2

void
XMLInternalParser::NormalizeReset ()
{
  normalize_buffer_count = 0;
  normalize_last_ch = 0;
  normalize_current_entity = 0;
}

void
XMLInternalParser::NormalizeGrow (unsigned added_count)
{
  if (normalize_buffer_count + added_count > normalize_buffer_total)
    {
      while (normalize_buffer_count + added_count > normalize_buffer_total)
        normalize_buffer_total = (normalize_buffer_total ? normalize_buffer_total + normalize_buffer_total : 256);

      uni_char *new_normalize_buffer = OP_NEWA_L(uni_char, normalize_buffer_total);

      op_memcpy (new_normalize_buffer, normalize_buffer, normalize_buffer_count * sizeof normalize_buffer[0]);

      OP_DELETEA(normalize_buffer);
      normalize_buffer = new_normalize_buffer;
    }
}

void
XMLInternalParser::NormalizeAddChar (uni_char ch)
{
  normalize_buffer[normalize_buffer_count++] = ch;
}

void
XMLInternalParser::NormalizePushChar (uni_char ch)
{
  if (!attribute_is_cdata && ch == 0x20 && (normalize_buffer_count == 0 || normalize_buffer[normalize_buffer_count - 1] == 0x20))
    {
      attribute_ws_normalized = TRUE;
      return;
    }

  NormalizeGrow (1);
  NormalizeAddChar (ch);
}

BOOL
XMLInternalParser::NormalizePushString (const uni_char *string, unsigned string_length, BOOL is_attribute, BOOL normalize_linebreaks)
{
  NormalizeGrow (string_length);

#ifdef REWRITTEN_PUSHSTRING_2
  if (string_length != 0)
    {
      BOOL is_xml11 = version == XMLVERSION_1_1, normalized = FALSE;

      uni_char ch, next_last_ch = string[string_length - 1];

      if (normalize_last_ch == 0x0d)
        {
          ch = string[0];

          if (ch == 0x0a || is_xml11 && ch == 0x85)
            {
              ++string;
              --string_length;
            }
        }

      if (string_length != 0)
        {
          uni_char *ptr_start = normalize_buffer + normalize_buffer_count, *ptr_end = ptr_start + string_length;

          uni_strncpy (ptr_start, string, string_length);
          normalize_buffer_count += string_length;

          if (normalize_linebreaks)
            {
              const uni_char *read = ptr_start;
              uni_char ch0 = normalize_last_ch, ch1;

              if (!is_xml11)
                while (((ch1 = *read++) != 0x0a || ch0 != 0x0d) && read != ptr_end) ch0 = ch1;
              else
                while (((ch1 = *read++) != 0x0a && ch1 != 0x85 || ch0 != 0x0d) && read != ptr_end) ch0 = ch1;

              if (ch0 == 0x0d && (ch1 == 0x0a || is_xml11 && ch1 == 0x85))
                {
                  normalized = TRUE;

                  ch0 = ch1;

                  --normalize_buffer_count;

                  uni_char *write = (uni_char *) (read - 1);
                  while (read != ptr_end)
                    {
                      ch1 = *read++;

                      if (ch0 != 0x0d || ch1 != 0x0a && (!is_xml11 || ch1 != 0x85))
                        *write++ = ch1;
                      else
                        --normalize_buffer_count;

                      ch0 = ch1;
                    }
                }
            }

          if (is_attribute)
            {
              uni_char *ptr = ptr_start;

              if (!normalize_linebreaks || !is_xml11)
                do if ((ch = *ptr) == 0x09 || ch == 0x0a || ch == 0x0d) normalized = TRUE, *ptr = 0x20; while (++ptr != ptr_end);
              else
                do if ((ch = *ptr) == 0x09 || ch == 0x0a || ch == 0x0d || ch == 0x85 || ch == 0x2028) normalized = TRUE, *ptr = 0x20; while (++ptr != ptr_end);

              if (!attribute_is_cdata)
                {
                  const uni_char *read = ptr_start;
                  uni_char ch0 = normalize_last_ch, ch1;

                  while (((ch1 = *read++) != 0x20 || ch0 != 0x20) && read != ptr_end) ch0 = ch1;

                  if (ch0 == 0x20 && ch1 == 0x20)
                    {
                      normalized = TRUE;

                      ch0 = ch1;

                      --normalize_buffer_count;

                      uni_char *write = (uni_char *) (read - 1);

                      while (read != ptr_end)
                        {
                          ch1 = *read++;

                          if (ch0 != 0x20 || ch1 != 0x20)
                            *write++ = ch1;
                          else
                            --normalize_buffer_count;

                          ch0 = ch1;
                        }
                    }
                }
            }
          else if (normalize_linebreaks)
            {
              uni_char *ptr = ptr_start;

              if (!is_xml11)
                do if ((ch = *ptr) == 0x0d) normalized = TRUE, *ptr = 0x0a; while (++ptr != ptr_end);
              else
                do if ((ch = *ptr) == 0x0d || ch == 0x85 || ch == 0x2028) normalized = TRUE, *ptr = 0x0a; while (++ptr != ptr_end);
            }
        }

      normalize_last_ch = next_last_ch;
      return normalized;
    }
  else
    return FALSE;
#else // REWRITTEN_PUSHSTRING_2
  unsigned normalize_buffer_count_before = normalize_buffer_count;

#ifdef XML_NORMALIZE_LINE_BREAKS
  if (normalize_linebreaks)
#ifdef REWRITTEN_PUSHSTRING_1
    {
      if (string_length != 0)
        {
          BOOL is_xml11 = version == XMLVERSION_1_1;

          unsigned offset = 0;
          uni_char ch;

          if (normalize_last_ch == 0x0d)
            {
              ch = string[0];

              if (ch == 0x0a || is_xml11 && ch == 0x85)
                ++offset;
            }

          const uni_char *ptr = string + offset, *ptr_end = string + string_length;

          if (offset < string_length)
            {
              do
                {
                  BOOL ws, linebreak = FALSE;
                  const uni_char *ptr_start = ptr;

                  do ch = *ptr++; while (!(ws = IsWhitespace (ch)) && !(linebreak = is_xml11 && (ch == 0x85 || ch == 0x2028)) && ptr != ptr_end);

                  unsigned plain = ptr - ptr_start - 1 + (!linebreak && !ws);
                  if (plain != 0)
                    {
                      uni_strncpy (normalize_buffer + normalize_buffer_count, ptr_start, plain);
                      normalize_buffer_count += plain;
                    }

                  if (linebreak)
                    ch = 0x0a;

                  if (linebreak || ws)
                    {
                      if (ch == 0x0d && ptr != ptr_end)
                        {
                          uni_char next = *ptr;

                          if (next == 0x0a || is_xml11 && ch == 0x85)
                            ++ptr;

                          ch = 0x0a;
                        }

                      if (is_attribute)
                        NormalizePushChar (0x20);
                      else
                        NormalizeAddChar (ch);
                    }
                }
              while (ptr != ptr_end);
            }

          normalize_last_ch = ch;
        }
    }
#else // REWRITTEN_PUSHSTRING_1
  for (unsigned index = 0; index < string_length; ++index)
    {
      uni_char last_ch = normalize_last_ch;
      uni_char ch = normalize_last_ch = string[index];

      if (last_ch != 0x0d || !(ch == 0x0a || version == XMLVERSION_1_1 && ch == 0x85))
        {
          if (ch == 0x0d || version == XMLVERSION_1_1 && (ch == 0x85 || ch == 0x2028))
            ch = 0x0a;

          if (is_attribute && IsWhitespace (ch))
            NormalizePushChar (0x20);
          else
            NormalizeAddChar (ch);
        }
    }
#endif // REWRITTEN_PUSHSTRING_1
  else
#endif // XML_NORMALIZE_LINE_BREAKS
    for (unsigned index = 0; index < string_length; ++index)
      {
        uni_char ch = string[index];

        if (is_attribute && IsWhitespace (ch))
          NormalizePushChar (0x20);
        else
          NormalizeAddChar (ch);
      }

  return normalize_buffer_count_before + string_length != normalize_buffer_count || op_memcmp (normalize_buffer + normalize_buffer_count_before, string, string_length * sizeof string[0]) != 0;
#endif // REWRITTEN_PUSHSTRING_2
}

BOOL
XMLInternalParser::NormalizeCharRef (const uni_char *value, unsigned start_index, unsigned end_index)
{
  unsigned ch_value;

  start_index += 2;

  BOOL is_hex;

  if (value[start_index] == 'x')
    {
      ++start_index;
      is_hex = TRUE;
    }
  else
    is_hex = FALSE;

  unsigned length = end_index - start_index;

  if (length != 0 && CalculateCharRef (value + start_index, length, ch_value, is_hex))
    {
      if (!IsValidChar (ch_value))
        return FALSE;
      else if (ch_value >= 0x10000)
        {
          unsigned adjusted_ch_value = ch_value - 0x10000;
          NormalizeGrow (2);
          NormalizeAddChar (0xd800 | (adjusted_ch_value >> 10));
          NormalizeAddChar (0xdc00 | (adjusted_ch_value & 0x03ff));
        }
      else
        NormalizePushChar (ch_value);

      normalize_last_ch = ch_value;
      return TRUE;
    }
  else
    return FALSE;
}

BOOL
XMLInternalParser::Normalize (const uni_char *value, unsigned value_length, BOOL is_attribute, BOOL normalize_linebreaks, unsigned error_index, unsigned error_length)
{
  OP_ASSERT (error_index != ~0u || value >= buffer && value < buffer + length);

  unsigned value_index = 0;
  BOOL indirect;

  if (error_index == ~0u)
    {
      error_index = value - buffer;
      indirect = FALSE;
    }
  else
    indirect = TRUE;

  unsigned error_index_start = error_index;

  while (value_index < value_length)
    {
      const uni_char *ptr = value + value_index, *ptr_end = value + value_length;
      unsigned start_index = value_index;

      if (is_attribute)
        {
          while (ptr != ptr_end && *ptr != '&' && *ptr != '<') ++ptr;

          if (*ptr == '<')
            HandleError (WELL_FORMEDNESS_ERROR_Invalid_AttValue, indirect ? error_index : ptr - buffer);
        }
      else
        while (ptr != ptr_end && *ptr != '&' && *ptr != '%') ++ptr;

      value_index = ptr - value;

      BOOL normalized = NormalizePushString (value + start_index, value_index - start_index, is_attribute, normalize_linebreaks);

      if (value_index == value_length)
        return normalized || start_index != 0;

      OP_ASSERT (value[value_index] == '&' || !is_attribute && value[value_index] == '%');

      if (!indirect)
        error_index = error_index_start + value_index;

      XMLDoctype::Entity::Type type = value[value_index] == '&' ? XMLDoctype::Entity::TYPE_General : XMLDoctype::Entity::TYPE_Parameter;
      unsigned semicolon_index = value_index + 1;

      if (semicolon_index == value_length)
        HandleError (WELL_FORMEDNESS_ERROR_Invalid_AttValue, error_index);

      if (type == XMLDoctype::Entity::TYPE_General && value[value_index + 1] == '#')
        {
          while (semicolon_index < value_length)
            if (value[semicolon_index] == ';')
              break;
            else
              ++semicolon_index;

          if (!indirect)
            error_length = semicolon_index - value_index + 1;

          if (semicolon_index == value_length || !NormalizeCharRef (value, value_index, semicolon_index))
            HandleError (WELL_FORMEDNESS_ERROR_Invalid_AttValue_CharRef, error_index, error_length);
        }
      else
        {
          BOOL valid = TRUE;

          if (!IsNameFirst (value[semicolon_index++]))
            valid = FALSE;
          else
            while (semicolon_index < value_length)
              if (value[semicolon_index] == ';')
                break;
              else if (!IsName (value[semicolon_index]))
                {
                  valid = FALSE;
                  break;
                }
              else
                ++semicolon_index;

          if (!indirect)
            error_length = semicolon_index - value_index + 1;

          if (!valid || semicolon_index == value_length)
            HandleError (!valid ? WELL_FORMEDNESS_ERROR_Invalid_AttValue_Reference : WELL_FORMEDNESS_ERROR_Invalid_AttValue, error_index, error_length);
          else if (!is_attribute && type == XMLDoctype::Entity::TYPE_General)
            NormalizePushString (value + value_index, semicolon_index - value_index + 1, FALSE, FALSE);
          else if (!is_attribute && current_context == PARSE_CONTEXT_INTERNAL_SUBSET)
            HandleError (WELL_FORMEDNESS_ERROR_PE_in_Internal_Subset, error_index, error_length);
          else
            {
              XMLDoctype::Entity *entity = doctype->GetEntity (type, value + value_index + 1, semicolon_index - value_index - 1);

              if ((!entity || entity->GetDeclaredInExternalSubset ()) &&
                  (!skipped_markup_declarations || standalone == XMLSTANDALONE_YES))
                HandleError (WELL_FORMEDNESS_ERROR_Undeclared_Entity, error_index, error_length);

              if (entity)
                {
                  if (type == XMLDoctype::Entity::TYPE_General && entity->GetValueType () != XMLDoctype::Entity::VALUE_TYPE_Internal)
                    HandleError (WELL_FORMEDNESS_ERROR_External_GE_in_AttValue, error_index, error_length);

                  XMLCurrentEntity *ce = normalize_current_entity;
                  while (ce)
                    if (ce->entity == entity)
                      HandleError (WELL_FORMEDNESS_ERROR_Recursive_Entity, error_index, error_length);
                    else
                      ce = ce->previous;

                  const uni_char *replacement_text = entity->GetValue ();

                  if (!replacement_text)
                    LoadEntity (entity, PARSE_CONTEXT_PARAMETER_ENTITY);
                  else
                    {
                      XMLCurrentEntity ce;
                      ce.entity = entity;
                      ce.previous = normalize_current_entity;
                      normalize_current_entity = &ce;

                      Normalize (replacement_text, uni_strlen (replacement_text), is_attribute, FALSE, error_index, error_length);

                      normalize_last_ch = 0;
                      normalize_current_entity = ce.previous;
                    }
                }
#ifdef XML_VALIDATING_ENTITY_DECLARED
              else if (xml_validating)
                HandleError (VALIDITY_ERROR_Undeclared_Entity, error_index, error_length);
#endif // XML_VALIDATING_ENTITY_DECLARED
            }
        }

      value_index = semicolon_index + 1;
    }

  return TRUE;
}

BOOL
XMLInternalParser::NormalizeAttributeValue (const uni_char *name, unsigned name_length, BOOL need_normalize)
{
#ifdef XML_STORE_ATTRIBUTES
  attribute_ws_normalized = FALSE;

  if (normalize_attribute)
    attribute_is_cdata = normalize_attribute->GetType () == XMLDoctype::Attribute::TYPE_String;
  else
    attribute_is_cdata = TRUE;
#else // XML_STORE_ATTRIBUTES
  attribute_is_cdata = TRUE;
#endif // XML_STORE_ATTRIBUTES

  if (name_length == 6 && op_memcmp (name, UNI_L ("xml:id"), 6 * sizeof name[0]) == 0)
    attribute_is_cdata = FALSE;

  if (!need_normalize && attribute_is_cdata)
    return FALSE;

  NormalizeReset ();

  if (!attribute_is_cdata)
    /* To skip leading spaces. */
    normalize_last_ch = 0x20;

  BOOL normalized = Normalize (literal, literal_length, TRUE, !current_buffer->IsInEntityReference ());

  if (!attribute_is_cdata)
    while (normalize_buffer_count != 0 && normalize_buffer[normalize_buffer_count - 1] == ' ')
      {
        --normalize_buffer_count;
        attribute_ws_normalized = normalized = TRUE;
      }

#ifdef XML_VALIDATING
  if (xml_validating && attribute_ws_normalized && normalize_attribute && normalize_attribute->GetDeclaredInExternalSubset () && standalone == XMLSTANDALONE_YES)
    HandleError (VALIDITY_ERROR_StandaloneDocumentDecl_AttrValueNormalized, literal_start);
#endif // XML_VALIDATING

  return normalized;
}

uni_char *
XMLInternalParser::GetNormalizedString ()
{
  return normalize_buffer;
}

unsigned
XMLInternalParser::GetNormalizedStringLength ()
{
  return normalize_buffer_count;
}
