/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_namespacealias.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/util/str.h"
#include "modules/xmlutils/xmlutils.h"

XSLT_NamespaceAlias::XSLT_NamespaceAlias (XMLVersion xmlversion)
  : stylesheet_prefix (0),
    result_prefix (0),
    xmlversion (xmlversion)
{
}

XSLT_NamespaceAlias::~XSLT_NamespaceAlias ()
{
  OP_DELETEA (stylesheet_prefix);
  OP_DELETEA (result_prefix);
}

/* virtual */ XSLT_Element *
XSLT_NamespaceAlias::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  parser->SignalErrorL ("unexpected element in xsl:namespace-alias");
  ignore_element = TRUE;
  return this;
}

/* virtual */ BOOL
XSLT_NamespaceAlias::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  return TRUE; // delete me
}

static void
XSLT_SetPrefix (uni_char *&prefix, XSLT_StylesheetParserImpl *parser, XMLVersion xmlversion, const uni_char *value, unsigned value_length)
{
  if (!XSLT_CompareStrings (value, value_length, "#default") && !XMLUtils::IsValidNCName (xmlversion, value, value_length))
    parser->SignalErrorL ("invalid prefix in xsl:namespace-alias");
  else
    LEAVE_IF_ERROR (UniSetStrN (prefix, value, value_length));
}

/* virtual */ void
XSLT_NamespaceAlias::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_STYLESHEET_PREFIX:
      XSLT_SetPrefix (stylesheet_prefix, parser, xmlversion, value, value_length);
      break;

    case XSLTA_RESULT_PREFIX:
      XSLT_SetPrefix (result_prefix, parser, xmlversion, value, value_length);
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!stylesheet_prefix)
        parser->SignalErrorL ("xsl:namespace-alias missing required stylesheet-prefix attribute");
      else if (!result_prefix)
        parser->SignalErrorL ("xsl:namespace-alias missing required result-prefix attribute");
      else
        parser->AddNamespaceAliasL (stylesheet_prefix, result_prefix);
      break;

    case XSLTA_OTHER:
      break;

    default:
      XSLT_Element::AddAttributeL (parser, type, name, value, value_length);
    }
}

#endif // XSLT_SUPPORT
