/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_outputbuffer.h"

#ifdef XSLT_COPY_TO_FILE_SUPPORT
# include "modules/util/opfile/opfile.h"
#endif // XSLT_COPY_TO_FILE_SUPPORT

# define XSLT_INITIAL_BUFFER_CAPACITY 8192

uni_char *
XSLT_OutputBuffer::AllocateSpaceL (unsigned length)
{
  if (buffer_used + length > buffer_capacity)
    {
      FlushL ();

      if (length > buffer_capacity)
        {
          unsigned new_buffer_capacity = buffer_capacity == 0 ? XSLT_INITIAL_BUFFER_CAPACITY : buffer_capacity;
          do
            new_buffer_capacity += new_buffer_capacity;
          while (length > new_buffer_capacity);

          uni_char *new_buffer = OP_NEWA_L (uni_char, new_buffer_capacity);
          OP_DELETEA (buffer);

          buffer = new_buffer;
          buffer_capacity = new_buffer_capacity;
        }
    }

  return buffer + buffer_used;
}

void
XSLT_OutputBuffer::FlushL ()
{
  if (buffer_used != 0)
    {
#ifdef XSLT_COPY_TO_FILE_SUPPORT
      if (file)
        {
          OpString8 utf8; ANCHOR (OpString8, utf8);
          LEAVE_IF_ERROR (utf8.SetUTF8FromUTF16 (buffer, buffer_used));
          LEAVE_IF_ERROR (file->Write (utf8.CStr (), utf8.Length ()));
        }
      else
#endif // XSLT_COPY_TO_FILE_SUPPORT
        LEAVE_IF_ERROR (collector->CollectStringData (buffer, buffer_used));

      buffer_used = 0;
    }
}

XSLT_OutputBuffer::XSLT_OutputBuffer (XSLT_Stylesheet::Transformation::StringDataCollector *collector)
  : collector (collector),
#ifdef XSLT_COPY_TO_FILE_SUPPORT
    file (0),
#endif // XSLT_COPY_TO_FILE_SUPPORT
    buffer (0),
    buffer_used (0),
    buffer_capacity (0),
    eof (FALSE)
{
}

#ifdef XSLT_COPY_TO_FILE_SUPPORT

XSLT_OutputBuffer::XSLT_OutputBuffer (OpFile *file)
  : collector (0),
    file (file),
    buffer (0),
    buffer_used (0),
    buffer_capacity (0),
    eof (FALSE)
{
}

#endif // XSLT_COPY_TO_FILE_SUPPORT

XSLT_OutputBuffer::~XSLT_OutputBuffer ()
{
  OP_DELETEA (buffer);
}

void
XSLT_OutputBuffer::WriteL (const char *data)
{
  WriteL (data, op_strlen (data));
}

void
XSLT_OutputBuffer::WriteL (const char *data, unsigned length)
{
  uni_char *space = AllocateSpaceL (length);
  const char *end = data + length;
  while (data != end)
    *space++ = *data++;
  buffer_used += length;
}

void
XSLT_OutputBuffer::WriteL (const uni_char *data)
{
  WriteL (data, uni_strlen (data));
}

void
XSLT_OutputBuffer::WriteL (const uni_char *data, unsigned length)
{
  uni_char *space = AllocateSpaceL (length);
  op_memcpy (space, data, length * sizeof (uni_char));
  buffer_used += length;
}

void
XSLT_OutputBuffer::WriteL (const XMLCompleteName &name)
{
  if (name.GetPrefix ())
    {
      WriteL (name.GetPrefix ());
      WriteL (":");
    }
  WriteL (name.GetLocalPart ());
}

void
XSLT_OutputBuffer::WriteLowerL (const uni_char *data)
{
  unsigned length = uni_strlen (data);
  uni_char *space = AllocateSpaceL (length);
  const uni_char *end = data + length;
  while (data != end)
    *space++ = op_tolower (*data++);
  buffer_used += length;
}

void
XSLT_OutputBuffer::WriteUpperL (const uni_char *data)
{
  unsigned length = uni_strlen (data);
  uni_char *space = AllocateSpaceL (length);
  const uni_char *end = data + length;
  while (data != end)
    *space++ = op_toupper (*data++);
  buffer_used += length;
}

void
XSLT_OutputBuffer::EndL ()
{
  FlushL ();

#ifdef XSLT_COPY_TO_FILE_SUPPORT
  if (!file)
#endif // XSLT_COPY_TO_FILE_SUPPORT
    LEAVE_IF_ERROR (collector->StringDataFinished ());
}

#endif // XSLT_SUPPORT
