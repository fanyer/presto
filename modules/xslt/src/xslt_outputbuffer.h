/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_OUTPUTBUFFER_H
#define XSLT_OUTPUTBUFFER_H

#ifdef XSLT_SUPPORT
# include "modules/xslt/xslt.h"

class XSLT_OutputBuffer
{
private:
  XSLT_Stylesheet::Transformation::StringDataCollector *collector;
#ifdef XSLT_COPY_TO_FILE_SUPPORT
  OpFile *file;
#endif // XSLT_COPY_TO_FILE_SUPPORT

  uni_char *buffer;
  unsigned buffer_used, buffer_capacity;
  BOOL eof;

  uni_char *AllocateSpaceL (unsigned length);
  void FlushL ();

public:
  XSLT_OutputBuffer (XSLT_Stylesheet::Transformation::StringDataCollector *collector);
#ifdef XSLT_COPY_TO_FILE_SUPPORT
  XSLT_OutputBuffer (OpFile *file);
#endif // XSLT_COPY_TO_FILE_SUPPORT
  ~XSLT_OutputBuffer ();

  void WriteL (const char *data);
  void WriteL (const char *data, unsigned length);
  void WriteL (const uni_char *data);
  void WriteL (const uni_char *data, unsigned length);
  void WriteL (const XMLCompleteName &name);
  void WriteLowerL (const uni_char *data);
  void WriteUpperL (const uni_char *data);
  void EndL ();
};

#endif // XSLT_SUPPORT
#endif // XSLT_OUTPUTBUFFER_H
