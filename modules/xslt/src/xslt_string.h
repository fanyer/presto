/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_STRING_H
#define XSLT_STRING_H

#ifdef XSLT_SUPPORT
# ifdef XSLT_ERRORS
#  include "modules/xmlutils/xmlnames.h"
#  include "modules/xmlutils/xmltypes.h"
#  include "modules/url/url2.h"
# endif // XSLT_ERRORS

class XSLT_StylesheetParserImpl;
class XSLT_MessageHandler;

class XSLT_String
{
private:
  uni_char *string;

#ifdef XSLT_ERRORS
  class DebugInformation
  {
  public:
    XMLCompleteName elementname;
    XMLCompleteName attributename;
#ifdef XML_ERRORS
    XMLRange range;
#endif // XML_ERRORS
    URL url;
  } *debuginfo;
#endif // XSLT_ERRORS

  friend class XSLT_StylesheetParserImpl;

  /* XSLT_StylesheetParserImpl::SetStringL() should be used to set the string's
     value.  It also sets debug information, if available. */
  void SetStringL (const uni_char *string0, unsigned string_length0);
#ifdef XSLT_ERRORS
#ifdef XML_ERRORS
  void SetDebugInformationL (const XMLCompleteName &elementname, const XMLCompleteNameN &attributename, const XMLRange &range, URL url);
#else // XML_ERRORS
  void SetDebugInformationL (const XMLCompleteName &elementname, const XMLCompleteNameN &attributename, URL url);
#endif // XML_ERRORS

  void GenerateContextStringL (OpString &context) const;
  void PrependContextL (OpString &message) const;
#endif // XSLT_ERRORS

public:
  XSLT_String ();
  ~XSLT_String ();

  void CopyL (const XSLT_String& other);
  void SetSubstringOfL (const XSLT_String &other, unsigned start, unsigned length);

  BOOL  IsSpecified () const { return string != 0; }
  const uni_char *GetString () const { return string; }
  unsigned GetLength () const { return uni_strlen (string); }

#ifdef XSLT_ERRORS
  BOOL HasDebugInformation () const { return debuginfo != 0; }
  const XMLCompleteName &GetElementName () const { return debuginfo->elementname; }
  const XMLCompleteName &GetAttributeName () const { return debuginfo->attributename; }
#ifdef XML_ERRORS
  const XMLRange &GetRange () const { return debuginfo->range; }
#endif // XML_ERRORS
#endif // XSLT_ERRORS

  void SignalErrorL (XSLT_MessageHandler *messagehandler, const char *reason, BOOL prepend_context = FALSE) const;
  void SignalErrorL (XSLT_MessageHandler *messagehandler, const uni_char *reason, BOOL prepend_context = FALSE) const;
};

# endif // XSLT_SUPPORT
#endif // XSLT_STRING_H
