/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_TEXTOUTPUTHANDLER_H
#define XSLT_TEXTOUTPUTHANDLER_H

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_outputhandler.h"

class XSLT_OutputBuffer;

class XSLT_TextOutputHandler
  : public XSLT_OutputHandler
{
public:
  XSLT_TextOutputHandler (XSLT_OutputBuffer *buffer);

  virtual void StartElementL (const XMLCompleteName &name);
  virtual void AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified);
  virtual void AddTextL (const uni_char *data, BOOL disable_output_escaping);
  virtual void AddCommentL (const uni_char *data);
  virtual void AddProcessingInstructionL (const uni_char *target, const uni_char *data);
  virtual void EndElementL (const XMLCompleteName &name);
  virtual void EndOutputL ();

protected:
  XSLT_OutputBuffer *buffer;
};

#endif // XSLT_SUPPORT
#endif // XSLT_TEXTOUTPUTHANDLER_H
