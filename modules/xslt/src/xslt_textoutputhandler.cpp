/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_textoutputhandler.h"
# include "modules/xslt/src/xslt_outputbuffer.h"

XSLT_TextOutputHandler::XSLT_TextOutputHandler (XSLT_OutputBuffer *buffer)
  : buffer (buffer)
{
}

/* virtual */ void
XSLT_TextOutputHandler::StartElementL (const XMLCompleteName &name)
{
}

/* virtual */ void
XSLT_TextOutputHandler::AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified)
{
}

/* virtual */ void
XSLT_TextOutputHandler::AddTextL (const uni_char *data, BOOL disable_output_escaping)
{
  buffer->WriteL (data);
}

/* virtual */ void
XSLT_TextOutputHandler::AddCommentL (const uni_char *data)
{
}

/* virtual */ void
XSLT_TextOutputHandler::AddProcessingInstructionL (const uni_char *target, const uni_char *data)
{
}

/* virtual */ void
XSLT_TextOutputHandler::EndElementL (const XMLCompleteName &name)
{
}

/* virtual */ void
XSLT_TextOutputHandler::EndOutputL ()
{
  buffer->EndL ();
}

#endif // XSLT_SUPPORT
