/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_importorinclude.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/util/str.h"

XSLT_ImportOrInclude::XSLT_ImportOrInclude ()
  : href (0)
{
}

XSLT_ImportOrInclude::~XSLT_ImportOrInclude ()
{
  OP_DELETEA (href);
}

/* virtual */ XSLT_Element *
XSLT_ImportOrInclude::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  parser->SignalErrorL (GetType () == XSLTE_IMPORT ? "unexpected element in xsl:import" : "unexpected element in xsl:include");
  ignore_element = TRUE;
  return this;
}

/* virtual */ BOOL
XSLT_ImportOrInclude::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  if (parser)
    parser->HandleImportOrIncludeL (href, type);

  return TRUE;
}

/* virtual */ void
XSLT_ImportOrInclude::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_OTHER:
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!href)
        parser->SignalErrorL (GetType () == XSLTE_IMPORT ? "xsl:import missing required href attribute" : "xsl:include missing required href attribute");
      break;

    case XSLTA_HREF:
      href = UniSetNewStrN (value, value_length);
      if (!href)
        LEAVE (OpStatus::ERR_NO_MEMORY);
      break;

    default:
      XSLT_Element::AddAttributeL (parser, type, name, value, value_length);
    }
}

#endif // XSLT_SUPPORT
