/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_COPYTOFILEOUTPUTHANDLER_H
#define XSLT_COPYTOFILEOUTPUTHANDLER_H

#ifdef XSLT_COPY_TO_FILE_SUPPORT
# include "modules/xslt/src/xslt_outputhandler.h"
# include "modules/util/opfile/opfile.h"

class XSLT_OutputBuffer;
class XSLT_StylesheetImpl;

class XSLT_CopyToFileOutputHandler
  : public XSLT_OutputHandler
{
public:
  XSLT_CopyToFileOutputHandler (const uni_char *path, XSLT_OutputHandler *real, XSLT_OutputMethod method, XSLT_StylesheetImpl *stylesheet);
  virtual ~XSLT_CopyToFileOutputHandler ();

private:
  void InitializeL ();

  /* From XSLT_OutputHandler: */
  virtual void StartElementL (const XMLCompleteName &name);
  virtual void AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified);
  virtual void AddTextL (const uni_char *data, BOOL disable_output_escaping);
  virtual void AddCommentL (const uni_char *data);
  virtual void AddProcessingInstructionL (const uni_char *target, const uni_char *data);
  virtual void EndElementL (const XMLCompleteName &name);
  virtual void EndOutputL ();

  const uni_char *path;
  OpFile file;
  XSLT_OutputHandler *real, *copy;
  XSLT_OutputBuffer *buffer;
  XSLT_OutputMethod method;
  XSLT_StylesheetImpl *stylesheet;
};

#endif // XSLT_COPY_TO_FILE_SUPPORT
#endif // XSLT_COPYTOFILEOUTPUTHANDLER_H
