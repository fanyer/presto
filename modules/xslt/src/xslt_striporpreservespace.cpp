/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_striporpreservespace.h"
#include "modules/xslt/src/xslt_parser.h"

XSLT_StripOrPreserveSpace::XSLT_StripOrPreserveSpace ()
  : has_elements (FALSE)
{
}

/* virtual */ XSLT_Element *
XSLT_StripOrPreserveSpace::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  parser->SignalErrorL (GetType () == XSLTE_STRIP_SPACE ? "unexpected element in xsl:strip-space" : "unexpected element in xsl:preserve-space");
  ignore_element = TRUE;
  return this;
}

/* virtual */ BOOL
XSLT_StripOrPreserveSpace::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  return TRUE; // delete me!
}

/* virtual */ void
XSLT_StripOrPreserveSpace::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_ELEMENTS:
      parser->AddWhiteSpaceControlL (value, value_length, GetType () == XSLTE_STRIP_SPACE);
      has_elements = TRUE;
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!has_elements)
        parser->SignalErrorL ("missing required elements attribute");
      break;

    case XSLTA_OTHER:
      break;

    default:
      XSLT_Element::AddAttributeL (parser, type, name, value, value_length);
    }
}

#endif // XSLT_SUPPORT
