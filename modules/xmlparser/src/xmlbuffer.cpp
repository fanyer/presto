/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlparser/xmlcommon.h"
#include "modules/xmlparser/xmlbuffer.h"
#include "modules/xmlparser/xmldatasource.h"
#include "modules/xmlparser/xmlinternalparser.h"

#define REWRITTEN_COPYTOLITERAL
#define REWRITTEN_UPDATELINE
#define SIMPLE_LINEBREAKS
#define CACHED_LOCATION
#define DELAYED_DATASOURCE_CONSUME

XMLBuffer::XMLBuffer (XMLDataSource *data_source, BOOL is_xml_1_1)
  : current_state (0),
    first_state (0),
    in_literal (FALSE),
    literal_buffers (0),
    literal_copied (0),
    literal_buffers_used (0),
    literal_buffers_count (0),
    literal_buffers_total (0),
    literal_buffer_offset (0),
    literal_buffer_size (0),
    parser_buffer (0),
    parser_index (0),
    parser_length (0),
    data_source (data_source),
    is_xml_1_1 (is_xml_1_1),
    at_end (FALSE),
#ifdef XML_VALIDATING
    is_validating (FALSE),
    line_buffer (0),
    line_buffer_offset (0),
    line_buffer_size (0),
    validity_report (0),
#endif // XML_VALIDATING
#ifdef XML_ERRORS
    line (0),
    column (0),
    character (0),
    previous_ch (0),
    cached_index (0),
    cached_line (0),
    cached_column (0),
#endif // XML_ERRORS
    copy_buffer (0),
    free_state (0)
{
  XML_OBJECT_CREATED (XMLBuffer);
}

XMLBuffer::~XMLBuffer ()
{
  DeleteStates (first_state);
  DeleteStates (free_state);

  for (unsigned index = 0; index < literal_buffers_count; ++index)
    OP_DELETEA(literal_buffers[index]);
  OP_DELETEA(literal_buffers);

#ifdef XML_VALIDATING
  OP_DELETEA(line_buffer);
#endif // XML_VALIDATING

  XML_OBJECT_DESTROYED (XMLBuffer);
}

OP_STATUS
XMLBuffer::Initialize (unsigned literalsize)
{
  RETURN_IF_ERROR (data_source->Initialize ());

  XML_OBJECT_CREATED (XMLBufferState);
  first_state = current_state = OP_NEW(State, ());
  if (!first_state)
    return OpStatus::ERR_NO_MEMORY;

  first_state->buffer = data_source->GetData ();
  first_state->index = 0;
  first_state->length = data_source->GetDataLength ();
  first_state->literal_start = ~0u;
  first_state->consumed = 0;
  first_state->consumed_before = 0;
  first_state->entity = 0;
  first_state->previous_state = 0;
  first_state->next_state = 0;

  literal_buffer_size = literalsize;

  return OpStatus::OK;
}

void
XMLBuffer::SetIsXML11 ()
{
  is_xml_1_1 = TRUE;
}

void
XMLBuffer::Consume (BOOL leave_state, BOOL no_linebreaks)
{
  CopyFromParserFields ();

  if (copy_buffer && current_state->index != current_state->consumed)
    copy_buffer->Append (current_state->buffer + current_state->consumed, current_state->index - current_state->consumed);

  current_state->consumed = current_state->index;

  if (in_literal)
    {
      FlushToLiteral ();
      current_state->literal_start = current_state->consumed;
    }

  if (!leave_state)
    {
      /* Discard any entity states whose replacement text has now been
         completely consumed. */

      while (current_state != first_state && current_state->index == current_state->length)
        {
          DiscardState ();
          current_state->consumed = current_state->index;
        }
    }

  if (current_state == first_state)
    {
#ifdef XML_ERRORS
#ifdef CACHED_LOCATION
      line = cached_line;
      column = cached_column;

      unsigned data_length = first_state->consumed - first_state->consumed_before - cached_index;
      if (data_length != 0)
        if (no_linebreaks)
          column += data_length;
        else
          UpdateLine (first_state->buffer + first_state->consumed_before + cached_index, data_length);

      cached_index = 0;
      cached_line = line;
      cached_column = column;
#else // CACHED_LOCATION
      UpdateLine (first_state->buffer, first_state->consumed - first_state->consumed_before);
#endif // CACHED_LOCATION
#endif // XML_ERRORS

      first_state->consumed_before = first_state->consumed;

#ifndef DELAYED_DATASOURCE_CONSUME
      unsigned consumed = data_source->Consume (first_state->consumed);

      first_state->buffer = data_source->GetData ();
      first_state->index -= consumed;
      first_state->length = data_source->GetDataLength ();
      first_state->consumed -= consumed;
      first_state->consumed_before -= consumed;
#endif // DELAYED_DATASOURCE_CONSUME

      if (in_literal)
        first_state->literal_start = first_state->index;
    }

  CopyToParserFields ();
}

void
XMLBuffer::ConsumeEntityReference (BOOL consume)
{
  OP_ASSERT (current_state->entity);
  OP_ASSERT (current_state->index == current_state->length);

  DiscardState ();

  if (consume)
	current_state->consumed = current_state->index;

  CopyToParserFields ();
}

void
XMLBuffer::DiscardState ()
{
  State *state = current_state;
  current_state = state->previous_state;
  current_state->next_state = 0;

  if (in_literal)
    current_state->literal_start = current_state->index;

  state->next_state = free_state;
  free_state = state;
}

void
XMLBuffer::DeleteStates (State *state)
{
  while (state)
    {
      State *discarded_state = state;
      state = state->next_state;
      OP_DELETE(discarded_state);
      XML_OBJECT_DESTROYED (XMLBufferState);
    }
}

void
XMLBuffer::ConsumeFromDataSource ()
{
#ifdef DELAYED_DATASOURCE_CONSUME
  unsigned consumed = data_source->Consume (first_state->consumed);

  if (consumed != 0)
    {
      first_state->index -= consumed;
      first_state->consumed -= consumed;
      first_state->consumed_before -= consumed;

      if (in_literal)
        first_state->literal_start = first_state->literal_start == ~0u ? first_state->index : first_state->literal_start - consumed;

      first_state->length = data_source->GetDataLength ();
    }
#endif // DELAYED_DATASOURCE_CONSUME
}

BOOL
XMLBuffer::GrowL (BOOL leave_state)
{
  CopyFromParserFields ();

  BOOL did_grow = FALSE;

  if (in_literal)
    FlushToLiteral ();

again:
  if (current_state == first_state)
    {
      unsigned length_before = first_state->length - first_state->consumed;

#ifdef DELAYED_DATASOURCE_CONSUME
      OP_ASSERT (first_state->consumed_before == first_state->consumed);

      ConsumeFromDataSource ();

      did_grow = length_before < first_state->length - first_state->consumed;

      if (!did_grow)
#endif // DELAYED_DATASOURCE_CONSUME
        {
          OP_BOOLEAN grow_result = data_source->Grow ();
          LEAVE_IF_ERROR (grow_result);
          if (grow_result == OpBoolean::IS_TRUE)
            {
              first_state->length = data_source->GetDataLength ();
              did_grow = length_before < first_state->length - first_state->consumed;
            }

          if (!did_grow)
            at_end = data_source->IsAtEnd ();
        }

      first_state->buffer = data_source->GetData ();
    }
  else if (!leave_state)
    {
      while (current_state && current_state->entity && current_state->index == current_state->length)
        DiscardState ();

      did_grow = current_state->index < current_state->length;

      if (!did_grow)
        goto again;
    }

  CopyToParserFields ();
  return did_grow;
}

BOOL
XMLBuffer::IsAtEnd ()
{
  return at_end;
}

BOOL
XMLBuffer::IsAllSeen ()
{
  return current_state == first_state && data_source->IsAllSeen ();
}

void
XMLBuffer::ExpandCharacterReference (unsigned length, unsigned character)
{
  CopyFromParserFields ();

  if (in_literal)
    {
      current_state->index -= length;
      FlushToLiteral ();
      current_state->index += length;
      current_state->literal_start = current_state->consumed = current_state->consumed_before = current_state->index;

#ifdef XML_ERRORS
      if (current_state == first_state)
        {
          column += length;

#ifdef CACHED_LOCATION
          cached_index = 0;
          cached_line = line;
          cached_column = column;
#endif // CACHED_LOCATION
        }
#endif // XML_ERRORS
    }

  uni_char string[2]; /* ARRAY OK jl 2008-02-07 */

  if (character >= 0x10000)
    {
      character -= 0x10000;
      string[0] = 0xd800 | (character >> 10);
      string[1] = 0xdc00 | (character & 0x3ff);
      length = 2;
    }
  else
    {
      string[0] = character;
      length = 1;
    }

  CopyToLiteral (string, length, FALSE);
  CopyToParserFields ();
}

void
XMLBuffer::ExpandEntityReference (unsigned length, XMLDoctype::Entity *entity)
{
  CopyFromParserFields ();

  if (in_literal)
    {
      current_state->index -= length;
      FlushToLiteral ();
      current_state->index += length;
      current_state->literal_start = current_state->consumed = current_state->consumed_before = current_state->index;

#ifdef XML_ERRORS
      if (current_state == first_state)
        {
          column += length;

#ifdef CACHED_LOCATION
          cached_index = 0;
          cached_line = line;
          cached_column = column;
#endif // CACHED_LOCATION
        }
#endif // XML_ERRORS
    }

  State *new_state = NewState ();

  new_state->buffer = entity->GetValue ();
  new_state->index = 0;
  new_state->length = entity->GetValueLength ();
  new_state->literal_start = in_literal ? 0 : ~0u;
  new_state->consumed = 0;
  new_state->consumed_before = 0;
  new_state->entity = entity;
  new_state->previous_state = current_state;
  new_state->next_state = 0;

#ifdef XML_ERRORS
  new_state->line = line;
  new_state->column = column;
#endif // XML_ERRORS

  current_state->next_state = new_state;
  current_state = new_state;

  CopyToParserFields ();
}

BOOL
XMLBuffer::IsInEntityReference (XMLDoctype::Entity *entity)
{
  State *state = current_state;

  while (state)
    if (state->entity == entity)
      return TRUE;
    else
      state = state->previous_state;

  return FALSE;
}

XMLDoctype::Entity *
XMLBuffer::GetCurrentEntity ()
{
  return current_state->entity;
}

void
XMLBuffer::SetParserFields (const uni_char *&pbuffer, unsigned &plength, unsigned &pindex)
{
  parser_buffer = &pbuffer;
  parser_length = &plength;
  parser_index = &pindex;

  current_state->index = current_state->consumed;

  first_state->buffer = data_source->GetData ();
  first_state->length = data_source->GetDataLength ();

  CopyToParserFields ();

#ifdef XML_ERRORS
#ifdef CACHED_LOCATION
  cached_index = 0;
  cached_line = line;
  cached_column = column;
#endif // CACHED_LOCATION
#endif // XML_ERRORS
}

void
XMLBuffer::LiteralStart (BOOL leave_state)
{
  if (!in_literal)
    {
      Consume (leave_state, TRUE);

      in_literal = TRUE;
      literal_buffers_used = 0;
      literal_buffer_offset = 0;
      literal_copied = 0;

      current_state->literal_start = current_state->index;
    }
}

void
XMLBuffer::LiteralEnd (BOOL leave_state)
{
  if (in_literal)
    {
      Consume (leave_state);

      in_literal = FALSE;

      FlushToLiteral ();
    }
}

static BOOL
IsWhitespace (const uni_char *ptr, unsigned length)
{
  const uni_char *ptr_end = ptr + length;

  while (ptr != ptr_end)
    if (!XMLInternalParser::IsWhitespace (*ptr++))
      return FALSE;

  return TRUE;
}

BOOL
XMLBuffer::GetLiteralIsWhitespace ()
{
  OP_ASSERT (!in_literal);

  if (literal_buffers_used != 0)
    {
      unsigned index = 0, count = literal_buffers_used - 1;

      for (; index < count; ++index)
        if (!IsWhitespace (literal_buffers[index], literal_buffer_size))
          return FALSE;

      if (!IsWhitespace (literal_buffers[count], literal_buffer_offset))
        return FALSE;
    }

  return TRUE;
}

BOOL
XMLBuffer::GetLiteralIsSimple ()
{
  return literal_buffers_used == 1 || literal_copied == 0;
}

uni_char *
XMLBuffer::GetLiteral (BOOL copy)
{
  if (!copy)
    {
      OP_ASSERT (GetLiteralIsSimple ());
      if (literal_copied == 0)
        return (uni_char *) UNI_L ("");
      else
        return (uni_char *) literal_buffers[0];
    }

  if (literal_copied == 0)
    {
      uni_char *literal = OP_NEWA(uni_char, 1);
      if (literal)
        literal[0] = 0;
      return literal;
    }
  else
    {
      uni_char *literal = OP_NEWA(uni_char, literal_copied + 1), *ptr = literal;
      if (literal)
        {
          unsigned index = 0, count = literal_buffers_used - 1, length = UNICODE_SIZE (literal_buffer_size);

          for (; index < count; ++index, ptr += literal_buffer_size)
            op_memcpy (ptr, literal_buffers[index], length);

          op_memcpy (ptr, literal_buffers[count], UNICODE_SIZE (literal_buffer_offset));

	      literal[literal_copied] = 0;
        }
      return literal;
    }
}

unsigned
XMLBuffer::GetLiteralLength ()
{
  return literal_copied;
}

void
XMLBuffer::GetLiteralPart (unsigned index, uni_char *&data, unsigned &data_length, BOOL &need_copy)
{
  data = literal_buffers[index];

  if (index == literal_buffers_used - 1)
    {
      data_length = literal_buffer_offset;
      need_copy = TRUE;
    }
  else
    {
      data_length = literal_buffer_size;
      need_copy = FALSE;
    }
}

void
XMLBuffer::ReleaseLiteralPart (unsigned index)
{
  literal_buffers[index] = 0;
}

unsigned
XMLBuffer::GetLiteralPartsCount ()
{
  return literal_buffers_used;
}

void
XMLBuffer::SetCopyBuffer (TempBuffer *buffer)
{
  copy_buffer = buffer;
}

void
XMLBuffer::NormalizeLinebreaks (uni_char *buffer, unsigned &length)
{
  uni_char *src = buffer, *src_end = buffer + length, *dest = buffer, ch;

  while (src != src_end)
    {
      if (!is_xml_1_1)
        while ((ch = *src) != 0x0d && (*dest++ = ch, ++src != src_end)) {}
      else
        while (((ch = *src) != 0x0d && ch != 0x85 && ch != 0x2028) && (*dest++ = ch, ++src != src_end)) {}

      if (src != src_end)
        {
          *dest++ = 0x0a;

          if (++src != src_end && ch == 0x0d)
            {
              ch = *src;
              if (ch == 0x0a || is_xml_1_1 && ch == 0x85)
                ++src;
            }
        }
    }

  length = dest - buffer;
}

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN

void
XMLBuffer::InsertCharacterData (const uni_char *data, unsigned data_length)
{
  OP_ASSERT (in_literal);

  FlushToLiteral ();
  CopyToLiteral (data, data_length, FALSE);
}

#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN

#ifdef XML_VALIDATING

void
XMLBuffer::SetValidityReport (XMLValidityReport *new_validity_report)
{
  is_validating = TRUE;
  validity_report = new_validity_report;
}

void
XMLBuffer::CopyToLineBuffer (const uni_char *data, unsigned data_length)
{
  if (line_buffer_offset + data_length > line_buffer_size)
    {
      unsigned new_line_buffer_size = line_buffer_size == 0 ? 256 : line_buffer_size << 1;

      while (line_buffer_offset + data_length > new_line_buffer_size)
        new_line_buffer_size <<= 1;

      uni_char *new_line_buffer = OP_NEWA_L(uni_char, new_line_buffer_size);
      op_memcpy (new_line_buffer, line_buffer, UNICODE_SIZE (line_buffer_offset));

      OP_DELETE(line_buffer);
      line_buffer = new_line_buffer;
      line_buffer_size = new_line_buffer_size;
    }

  op_memcpy (line_buffer + line_buffer_offset, data, UNICODE_SIZE (data_length));
  line_buffer_offset += data_length;
}

#endif // XML_VALIDATING

#ifdef XML_ERRORS

void
XMLBuffer::GetLocation (unsigned index, XMLPoint &point, BOOL no_linebreaks)
{
  if (index == 0)
    {
      point.line = line;
      point.column = column;
    }
  else
    {
      unsigned old_line = line, old_column = column, old_character = character;

      if (current_state != first_state)
        index = first_state->index - first_state->consumed;

      const uni_char *start = first_state->buffer + first_state->consumed;

#ifdef CACHED_LOCATION
      if (cached_index <= index)
        {
          start += cached_index;
          index -= cached_index;

          line = cached_line;
          column = cached_column;

          if (no_linebreaks)
            column += index;
          else
            UpdateLine (start, index);

          if (!in_literal)
            {
              cached_index += index;
              cached_line = line;
              cached_column = column;
            }
        }
#endif // CACHED_LOCATION
      else
        UpdateLine (start, index);

      point.line = line;
      point.column = column;

      line = old_line;
      column = old_column;
      character = old_character;
    }
}

void
XMLBuffer::UpdateLine (const uni_char *data, unsigned data_length)
{
#ifdef REWRITTEN_UPDATELINE
  if (data_length != 0)
    {
      character += data_length;

      const uni_char *ptr = data;
      uni_char ch = *ptr;

      if (previous_ch == 0x0d)
#ifdef SIMPLE_LINEBREAKS
        if (ch == 0x0a)
#else // SIMPLE_LINEBREAKS
        if (ch == 0x0a || is_xml_1_1 && ch == 0x85)
#endif // SIMPLE_LINEBREAKS
          ++ptr;

      const uni_char *ptr_start = ptr, *ptr_end = ptr + data_length;

      if (data_length > 1)
        {
          --ptr_end;

          do
            {
              BOOL found;

#ifdef SIMPLE_LINEBREAKS
              do ch = *ptr++; while (!(found = ch == 0x0a || ch == 0x0d) && ptr != ptr_end);
#else // SIMPLE_LINEBREAKS
              do ch = *ptr++; while (!(found = ch == 0x0a || ch == 0x0d || is_xml_1_1 && (ch == 0x85 || ch == 0x2028)) && ptr != ptr_end);
#endif // SIMPLE_LINEBREAKS

              if (found)
                {
                  ++line;

#ifdef SIMPLE_LINEBREAKS
                  if (ch == 0x0d && *ptr == 0x0a)
#else // SIMPLE_LINEBREAKS
                  if (ch == 0x0d && (*ptr == 0x0a || is_xml_1_1 && *ptr == 0x85))
#endif // SIMPLE_LINEBREAKS
                    ++ptr;

                  column = 0;
                  ptr_start = ptr;
                }
            }
          while (ptr < ptr_end);

          ++ptr_end;
        }

      if (ptr != ptr_end)
        {
          ch = *ptr++;

#ifdef SIMPLE_LINEBREAKS
          if (ch == 0x0a || ch == 0x0d)
#else // SIMPLE_LINEBREAKS
          if (ch == 0x0a || ch == 0x0d || is_xml_1_1 && (ch == 0x85 || ch == 0x2028))
#endif // SIMPLE_LINEBREAKS
            {
              ++line;
              column = 0;

              ptr_start = ptr;
            }
        }

      column += ptr - ptr_start;
      previous_ch = ch;
    }
#else // REWRITTEN_UPDATELINE
  const uni_char *ptr = data, *ptr_end = ptr + data_length;

  if (ptr != ptr_end)
    {
      character += data_length;

      if (previous_ch == 0x0d)
#ifdef SIMPLE_LINEBREAKS
        if (*ptr == 0x0a)
#else // SIMPLE_LINEBREAKS
        if (*ptr == 0x0a || is_xml_1_1 && *ptr == 0x85)
#endif // SIMPLE_LINEBREAKS
          ++ptr;

      while (ptr != ptr_end)
        {
          uni_char ch = *ptr++;

#ifdef SIMPLE_LINEBREAKS
          if (ch != 0x0a && ch != 0x0d)
#else // SIMPLE_LINEBREAKS
          if (ch != 0x0a && ch != 0x0d && (is_xml_1_1 || ch != 0x85 && ch != 0x2028))
#endif // SIMPLE_LINEBREAKS
            ++column;
          else
            {
#ifdef XML_VALIDATING
              if (is_validating && validity_report)
                {
                  CopyToLineBuffer (data, ptr - data - 1);
                  validity_report->SetLine (line_buffer, line_buffer_offset);
                  line_buffer_offset = 0;
                }
#endif // XML_VALIDATING

              ++line;
              column = 0;

#ifdef SIMPLE_LINEBREAKS
              if (ch == 0x0d && ptr != ptr_end && *ptr == 0x0a)
#else // SIMPLE_LINEBREAKS
              if (ch == 0x0d && ptr != ptr_end && (*ptr == 0x0a || is_xml_1_1 && *ptr == 0x85))
#endif // SIMPLE_LINEBREAKS
                ++ptr;

#ifdef XML_VALIDATING
              data = ptr;
#endif // XML_VALIDATING
            }
        }

#ifdef XML_VALIDATING
      if (is_validating && validity_report)
        CopyToLineBuffer (data, ptr - data);
#endif // XML_VALIDATING

      previous_ch = *(ptr - 1);
    }
#endif // REWRITTEN_UPDATELINE
}

#endif // XML_ERRORS

void
XMLBuffer::CopyFromParserFields ()
{
  current_state->index = current_state->consumed + *parser_index;
}

void
XMLBuffer::CopyToParserFields ()
{
  unsigned consumed = current_state->consumed;
  *parser_buffer = current_state->buffer + consumed;
  *parser_length = current_state->length - consumed;
  *parser_index = current_state->index - consumed;
}

void
XMLBuffer::FlushToLiteral ()
{
  if (current_state->literal_start != ~0u && current_state->literal_start < current_state->consumed)
    {
      CopyToLiteral (current_state->buffer + current_state->literal_start, current_state->consumed - current_state->literal_start, current_state->entity == NULL);
      current_state->literal_start = ~0u;
    }
}

void
XMLBuffer::CopyToLiteral (const uni_char *data, unsigned data_length, BOOL normalize_linebreaks)
{
#ifdef REWRITTEN_COPYTOLITERAL
  if (data_length > 0)
    {
      if (literal_buffers_used == 0)
        AddLiteralBuffer ();

      BOOL last_was_cr = literal_copied != 0 && last_was_carriage_return, xml_1_1 = is_xml_1_1;
      last_was_carriage_return = FALSE;

      const uni_char *src = data, *src_end = src + data_length;

      unsigned dest_length = literal_buffer_size - literal_buffer_offset;
      uni_char *dest = literal_buffers[literal_buffers_used - 1] + literal_buffer_offset;

#ifdef CACHED_LOCATION
      const uni_char *linestart = src;
      unsigned linebreaks = 0;
#endif // CACHED_LOCATION

      if (normalize_linebreaks)
        {
          uni_char ch = *src;

          if (last_was_cr && (ch == 0x0a || xml_1_1 && ch == 0x85))
            if (++src == src_end)
              goto done;

          while (TRUE)
            {
              if (dest_length == 0)
                {
                  dest_length = literal_buffer_offset = literal_buffer_size;
                  dest = AddLiteralBuffer ();
                }

              const uni_char *local_src_start = src, *local_src_end = MIN (src_end, src + dest_length);

              if (!xml_1_1)
#ifdef CACHED_LOCATION
                while ((ch = *src) != 0x0d && ((*dest++ = ch) == 0x0a ? (++linebreaks, linestart = ++src) : ++src) != local_src_end) {}
#else // CACHED_LOCATION
                while ((ch = *src) != 0x0d && (*dest++ = ch, ++src != local_src_end)) {}
#endif // CACHED_LOCATION
              else
#ifdef CACHED_LOCATION
                while (((ch = *src) != 0x0d && ch != 0x85 && ch != 0x2028) && ((*dest++ = ch) == 0x0a ? (++linebreaks, linestart = ++src) : ++src) != local_src_end) {}
#else // CACHED_LOCATION
                while (((ch = *src) != 0x0d && ch != 0x85 && ch != 0x2028) && (*dest++ = ch, ++src != local_src_end)) {}
#endif // CACHED_LOCATION

              unsigned copied = src - local_src_start;

              dest_length -= copied;

              if (src != local_src_end)
                {
                  *dest++ = 0x0a;
                  --dest_length;

                  if (++src != src_end && ch == 0x0d)
                    {
                      ch = *src;
                      if (ch == 0x0a || xml_1_1 && ch == 0x85)
                        ++src;
                    }

#ifdef CACHED_LOCATION
                  ++linebreaks;
                  linestart = src;
#endif // CACHED_LOCATION
                }

              if (src == src_end)
                break;
            }

          last_was_carriage_return = ch == 0x0d;

#ifdef XML_ERRORS
#ifdef CACHED_LOCATION
          cached_index += data_length;

          if (linebreaks != 0)
            {
              cached_line += linebreaks;
              cached_column = src_end - linestart;
            }
          else
            cached_column += src_end - linestart;
#endif // CACHED_LOCATION
#endif // XML_ERRORS
        }
      else
        {
          do
            {
              if (dest_length == 0)
                {
                  dest_length = literal_buffer_offset = literal_buffer_size;
                  dest = AddLiteralBuffer ();
                }

              unsigned copied = MIN ((unsigned) (src_end - src), dest_length);

              op_memcpy (dest, src, copied * sizeof (uni_char));

              dest += copied;
              dest_length -= copied;

              src += copied;
            }
          while (src != src_end);

          last_was_carriage_return = FALSE;
        }

    done:
      literal_buffer_offset = literal_buffer_size - dest_length;
      literal_copied = literal_buffer_size * (literal_buffers_used - 1) + literal_buffer_offset;
    }
#else // REWRITTEN_COPYTOLITERAL
  if (literal_buffers_used == 0)
    AddLiteralBuffer ();

  BOOL last_was_cr = literal_copied != 0 && last_was_carriage_return;
  last_was_carriage_return = FALSE;

  while (data_length > 0)
    {
      if (literal_buffer_offset == literal_buffer_size)
        AddLiteralBuffer ();

      const uni_char *dest_start = literal_buffers[literal_buffers_used - 1] + literal_buffer_offset;
      const uni_char *dest_end = dest_start + literal_buffer_size - literal_buffer_offset;
      const uni_char *src = data;
      const uni_char *src_end = data + data_length;
      uni_char *dest = (uni_char *) dest_start;

      if (normalize_linebreaks)
        {
          BOOL xml_1_1 = is_xml_1_1;

          if (last_was_cr)
            if (*src == 0x0a || *src == 0x85)
              ++src;

          while (dest != dest_end && src != src_end)
            {
              uni_char ch = *src++;

              if (ch == 0x0d)
                {
                  *dest++ = 0x0a;

                  if (src == src_end)
                    last_was_carriage_return = TRUE;
                  else if (*src == 0x0a || xml_1_1 && *src == 0x85)
                    ++src;
                }
              else if (xml_1_1 && (ch == 0x85 || ch == 0x2028))
                *dest++ = 0x0a;
              else
                *dest++ = ch;
            }
        }
      else
        while (dest != dest_end && src != src_end)
          *dest++ = *src++;

      unsigned copy_length = src - data;
      data += copy_length;
      data_length -= copy_length;
      literal_buffer_offset += dest - dest_start;
      literal_copied += dest - dest_start;
    }
#endif // REWRITTEN_COPYTOLITERAL
}

extern void **
XMLGrowArray (void **old_array, unsigned count, unsigned &total);

#define GROW_ARRAY(array, type) (array = (type *) XMLGrowArray ((void **) array, array##_count, array##_total))

uni_char *
XMLBuffer::AddLiteralBuffer ()
{
  if (literal_buffers_used == 0 && literal_buffers_count != 0)
    {
      unsigned index1 = 0, index2 = literal_buffers_count - 1;

      while (index1 < index2)
        if (!literal_buffers[index1])
          {
            literal_buffers[index1] = literal_buffers[index2--];
            --literal_buffers_count;
          }
        else
          ++index1;

      if (!literal_buffers[index2])
        --literal_buffers_count;
    }

  if (literal_buffers_used == literal_buffers_count)
    {
      GROW_ARRAY (literal_buffers, uni_char *);
      literal_buffers[literal_buffers_count++] = OP_NEWA_L(uni_char, literal_buffer_size);
    }

  literal_buffer_offset = 0;
  return literal_buffers[literal_buffers_used++];
}

XMLBuffer::State *
XMLBuffer::NewState ()
{
  if (free_state)
    {
      State *state;

      state = free_state;
      free_state = state->next_state;

      return state;
    }
  else
    {
      XML_OBJECT_CREATED (XMLBufferState);
      return OP_NEW_L(State, ());
    }
}
