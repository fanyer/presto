/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_namespacecollection.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/xmlutils/xmlnames.h"

void
XSLT_NamespaceCollection::FinishL (XSLT_StylesheetParserImpl *parser, XSLT_Element *element)
{
  if (IsSpecified ())
    {
      const uni_char *value = GetString ();
      unsigned value_length = uni_strlen (value);
      XMLVersion xmlversion = element->GetXMLVersion ();

      while (value_length != 0)
        {
          const uni_char *prefix = value;

          if (!XMLUtils::IsSpace (XMLUtils::GetNextCharacter (value, value_length)))
            {
              unsigned prefix_length = 1;

              while (value_length != 0 && !XMLUtils::IsSpace (XMLUtils::GetNextCharacter (value, value_length)))
                ++prefix_length;

              const uni_char *uri;

              if (prefix_length == 8 && uni_strncmp (prefix, UNI_L ("#default"), prefix_length) == 0)
                uri = XMLNamespaceDeclaration::FindDefaultUri (element->GetNamespaceDeclaration ());
              else
                {
                  if (!XMLUtils::IsValidNCName (xmlversion, prefix, prefix_length))
                    {
#ifdef XSLT_ERRORS
                      OpString reason; ANCHOR (OpString, reason);
                      reason.SetL ("invalid NCName: ");
                      reason.AppendL (prefix, prefix_length);
                      SignalErrorL (parser, reason.CStr (), TRUE);
#else // XSLT_ERRORS
                      LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
                    }

                  uri = XMLNamespaceDeclaration::FindUri (element->GetNamespaceDeclaration (), prefix, prefix_length);
                }

              if (!uri)
                {
#ifdef XSLT_ERRORS
                  OpString reason; ANCHOR (OpString, reason);
                  reason.SetL ("undeclared prefix: ");
                  reason.AppendL (prefix, prefix_length);
                  SignalErrorL (parser, reason.CStr (), TRUE);
#else // XSLT_ERRORS
                  LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
                }

              OpString *string = OP_NEW_L (OpString, ());
              OP_STATUS status;

              if (OpStatus::IsMemoryError (status = string->Set (uri)) || OpStatus::IsError (status = uris.Add (string->CStr (), string)))
                {
                  OP_DELETE (string);
                  if (OpStatus::IsMemoryError (status))
                    LEAVE (status);
                }
            }
        }
    }
}

#endif // XSLT_SUPPORT
