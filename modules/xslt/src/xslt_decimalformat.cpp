/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_decimalformat.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/util/str.h"

XSLT_DecimalFormat::XSLT_DecimalFormat ()
  : df (0)
{
}

/* virtual */ XSLT_Element *
XSLT_DecimalFormat::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  parser->SignalErrorL ("unexpected element in xsl:decimal-format");
  ignore_element = TRUE;
  return this;
}

/* virtual */ BOOL
XSLT_DecimalFormat::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  return TRUE; // deletable
}

static void
XSLT_SetCharacter (unsigned &character, const uni_char *value, unsigned value_length)
{
  if (value_length > 0)
    character = XMLUtils::GetNextCharacter (value, value_length);
}

static void
XSLT_SetStringL (uni_char *&string, const uni_char *value, unsigned value_length)
{
  LEAVE_IF_ERROR (UniSetStrN (string, value, value_length));
}


/* virtual */ void
XSLT_DecimalFormat::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
  if (!df)
    {
      df = parser->GetStylesheet ()->AddDecimalFormat ();
      df->SetDefaultsL ();
    }

  switch (type)
    {
    case XSLTA_DECIMAL_SEPARATOR:
      XSLT_SetCharacter (df->decimal_separator, value, value_length);
      break;

    case XSLTA_DIGIT:
      XSLT_SetCharacter (df->digit, value, value_length);
      break;

    case XSLTA_GROUPING_SEPARATOR:
      XSLT_SetCharacter (df->grouping_separator, value, value_length);
      break;

    case XSLTA_INFINITY:
      XSLT_SetStringL (df->infinity, value, value_length);
      break;

    case XSLTA_MINUS_SIGN:
      XSLT_SetCharacter (df->minus_sign, value, value_length);
      break;

    case XSLTA_NAME:
      parser->SetQNameAttributeL (value, value_length, FALSE, &df->name, 0);
      break;

    case XSLTA_NAN:
      XSLT_SetStringL (df->nan, value, value_length);
      break;

    case XSLTA_PATTERN_SEPARATOR:
      XSLT_SetCharacter (df->pattern_separator, value, value_length);
      break;

    case XSLTA_PERCENT:
      XSLT_SetCharacter (df->percent, value, value_length);
      break;

    case XSLTA_PER_MILLE:
      XSLT_SetCharacter (df->per_mille, value, value_length);
      break;

    case XSLTA_ZERO_DIGIT:
      XSLT_SetCharacter (df->zero_digit, value, value_length);
    }
}

#endif // XSLT_SUPPORT
