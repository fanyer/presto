/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_COPY_TO_FILE_SUPPORT
# include "modules/xslt/src/xslt_copytofileoutputhandler.h"
# include "modules/xslt/src/xslt_xmlsourcecodeoutputhandler.h"
# include "modules/xslt/src/xslt_htmlsourcecodeoutputhandler.h"
# include "modules/xslt/src/xslt_textoutputhandler.h"
# include "modules/xslt/src/xslt_outputbuffer.h"

XSLT_CopyToFileOutputHandler::XSLT_CopyToFileOutputHandler (const uni_char *path, XSLT_OutputHandler *real, XSLT_OutputMethod method, XSLT_StylesheetImpl *stylesheet)
  : path (path),
    real (real),
    copy (0),
    buffer (0),
    method (method),
    stylesheet (stylesheet)
{
}

XSLT_CopyToFileOutputHandler::~XSLT_CopyToFileOutputHandler ()
{
  OP_DELETE (real);
  OP_DELETE (copy);
  OP_DELETE (buffer);
}

void
XSLT_CopyToFileOutputHandler::InitializeL ()
{
  if (!file.IsOpen ())
    {
      LEAVE_IF_ERROR (file.Construct (path));
      LEAVE_IF_ERROR (file.Open (OPFILE_WRITE));
    }

  if (!buffer)
    buffer = OP_NEW_L (XSLT_OutputBuffer, (&file));

  if (!copy)
    switch (method)
      {
      default:
        copy = OP_NEW_L (XSLT_XMLSourceCodeOutputHandler, (buffer, stylesheet));
        break;

      case XSLT_OUTPUT_HTML:
        copy = OP_NEW_L (XSLT_HTMLSourceCodeOutputHandler, (buffer, stylesheet));
        break;

      case XSLT_OUTPUT_TEXT:
        copy = OP_NEW_L (XSLT_TextOutputHandler, (buffer));
      }
}

/* virtual */ void
XSLT_CopyToFileOutputHandler::StartElementL (const XMLCompleteName &name)
{
  InitializeL ();
  real->StartElementL (name);
  copy->StartElementL (name);
}

/* virtual */ void
XSLT_CopyToFileOutputHandler::AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified)
{
  real->AddAttributeL (name, value, id, specified);
  copy->AddAttributeL (name, value, id, specified);
}

/* virtual */ void
XSLT_CopyToFileOutputHandler::AddTextL (const uni_char *data, BOOL disable_output_escaping)
{
  InitializeL ();
  real->AddTextL (data, disable_output_escaping);
  copy->AddTextL (data, disable_output_escaping);
}

/* virtual */ void
XSLT_CopyToFileOutputHandler::AddCommentL (const uni_char *data)
{
  InitializeL ();
  real->AddCommentL (data);
  copy->AddCommentL (data);
}

/* virtual */ void
XSLT_CopyToFileOutputHandler::AddProcessingInstructionL (const uni_char *target, const uni_char *data)
{
  InitializeL ();
  real->AddProcessingInstructionL (target, data);
  copy->AddProcessingInstructionL (target, data);
}

/* virtual */ void
XSLT_CopyToFileOutputHandler::EndElementL (const XMLCompleteName &name)
{
  real->EndElementL (name);
  copy->EndElementL (name);
}

/* virtual */ void
XSLT_CopyToFileOutputHandler::EndOutputL ()
{
  InitializeL ();
  real->EndOutputL ();
  copy->EndOutputL ();
}

#endif // XSLT_COPY_TO_FILE_SUPPORT
