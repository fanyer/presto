/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_string.h"
# include "modules/xslt/src/xslt_simple.h"

void
XSLT_String::SetStringL (const uni_char *string0, unsigned string_length0)
{
  uni_char *new_string = OP_NEWA_L (uni_char, string_length0 + 1);
  op_memcpy (new_string, string0, string_length0 * sizeof (uni_char));
  new_string[string_length0] = 0;
  OP_DELETEA (string);
  string = new_string;
}

#ifdef XSLT_ERRORS

void
#ifdef XML_ERRORS
XSLT_String::SetDebugInformationL (const XMLCompleteName &elementname, const XMLCompleteNameN &attributename, const XMLRange &range, URL url)
#else // XML_ERRORS
XSLT_String::SetDebugInformationL (const XMLCompleteName &elementname, const XMLCompleteNameN &attributename, URL url)
#endif // XML_ERRORS
{
  debuginfo = OP_NEW_L (DebugInformation, ());
  debuginfo->elementname.SetL (elementname);
  debuginfo->attributename.SetL (attributename);
#ifdef XML_ERRORS
  debuginfo->range = range;
#endif // XML_ERRORS
  debuginfo->url = url;
}

void
XSLT_String::GenerateContextStringL (OpString &context) const
{
  if (debuginfo->elementname.GetPrefix ())
    {
      context.AppendL (debuginfo->elementname.GetPrefix ());
      context.AppendL (":");
    }
  context.AppendL (debuginfo->elementname.GetLocalPart ());
  context.AppendL ("/@");
  if (debuginfo->attributename.GetPrefix ())
    {
      context.AppendL (debuginfo->attributename.GetPrefix ());
      context.AppendL (":");
    }
  context.AppendL (debuginfo->attributename.GetLocalPart ());

#ifdef XML_ERRORS
  if (debuginfo->range.start.IsValid ())
    {
      OP_STATUS status;

      if (debuginfo->range.end.IsValid () && (debuginfo->range.start.line != debuginfo->range.end.line || debuginfo->range.end.column - debuginfo->range.start.column > 2))
        if (debuginfo->range.start.line == debuginfo->range.end.line)
          status = context.AppendFormat (UNI_L (" at line %d, column %d-%d"), debuginfo->range.start.line + 1, debuginfo->range.start.column, debuginfo->range.end.column - 1);
        else
          status = context.AppendFormat (UNI_L (" at line %d, column %d to line %d, column %d"), debuginfo->range.start.line + 1, debuginfo->range.start.column, debuginfo->range.end.line + 1, debuginfo->range.end.column - 1);
      else
        status = context.AppendFormat (UNI_L (" at line %d, column %d"), debuginfo->range.start.line + 1, debuginfo->range.start.column);

      LEAVE_IF_ERROR (status);
    }
#endif // XML_ERRORS
}

void
XSLT_String::PrependContextL (OpString &message) const
{
  if (debuginfo->elementname.GetPrefix ())
    {
      message.AppendL (debuginfo->elementname.GetPrefix ());
      message.AppendL (":");
    }
  message.AppendL (debuginfo->elementname.GetLocalPart ());
  message.AppendL ("/@");
  if (debuginfo->attributename.GetPrefix ())
    {
      message.AppendL (debuginfo->attributename.GetPrefix ());
      message.AppendL (":");
    }
  message.AppendL (debuginfo->attributename.GetLocalPart ());
  message.AppendL (": ");
}

#endif // XSLT_ERRORS

XSLT_String::XSLT_String ()
  : string (0)
{
#ifdef XSLT_ERRORS
  debuginfo = 0;
#endif // XSLT_ERRORS
}

XSLT_String::~XSLT_String ()
{
  OP_DELETEA (string);

#ifdef XSLT_ERRORS
  OP_DELETE (debuginfo);
#endif // XSLT_ERRORS
}

void
XSLT_String::CopyL (const XSLT_String &other)
{
  SetSubstringOfL (other, 0, other.GetLength ());
}

void
XSLT_String::SetSubstringOfL (const XSLT_String &other, unsigned start, unsigned length)
{
  SetStringL (other.GetString () + start, length);

#ifdef XSLT_ERRORS
  if (other.debuginfo)
    {
      debuginfo = OP_NEW_L (DebugInformation, ());
      *debuginfo = *other.debuginfo;
    }
#endif // XSLT_ERRORS
}

void
XSLT_String::SignalErrorL (XSLT_MessageHandler *messagehandler, const char *reason, BOOL prepend_context) const
{
#ifdef XSLT_ERRORS
  OpString message; ANCHOR (OpString, message);
  if (prepend_context)
    PrependContextL (message);
  message.AppendL (reason);
  OpString context; ANCHOR (OpString, context);
  GenerateContextStringL (context);
  XSLT_SignalErrorL (messagehandler, message.CStr (), context.CStr (), &debuginfo->url);
#else // XSLT_ERRORS
  LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
}

void
XSLT_String::SignalErrorL (XSLT_MessageHandler *messagehandler, const uni_char *reason, BOOL prepend_context) const
{
#ifdef XSLT_ERRORS
  OpString message; ANCHOR (OpString, message);
  if (prepend_context)
    PrependContextL (message);
  message.AppendL (reason);
  OpString context; ANCHOR (OpString, context);
  GenerateContextStringL (context);
  XSLT_SignalErrorL (messagehandler, message.CStr (), context.CStr (), &debuginfo->url);
#else // XSLT_ERRORS
  LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
}

#endif // XSLT_SUPPORT
