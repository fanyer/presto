/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_output.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_stylesheet.h"

/* virtual */ XSLT_Element *
XSLT_Output::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  parser->SignalErrorL ("unexpected element in xsl:output");
  ignore_element = TRUE;
  return this;
}

/* virtual */ BOOL
XSLT_Output::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  return TRUE; // delete me
}

static void
XSLT_ReplaceSpecificationValueL (const uni_char *&spec_value, BOOL& owns_spec_value, const uni_char *value, unsigned value_length)
{
  uni_char *copy = UniSetNewStrN (value, value_length);
  if (!copy)
    LEAVE (OpStatus::ERR_NO_MEMORY);
  if (owns_spec_value)
    {
      uni_char *current = const_cast<uni_char *> (spec_value);
      OP_DELETEA (current);
    }
  spec_value = copy;
  owns_spec_value = TRUE;
}

/* virtual */ void
XSLT_Output::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  XSLT_OutputMethod method;
  XSLT_OutputSpecificationInternal &output_specification = parser->GetStylesheet ()->GetOutputSpecificationInternal ();

  switch (type)
    {
    case XSLTA_METHOD:
      method = XSLT_OUTPUT_DEFAULT;

      if (value_length == 3 && uni_strncmp (value, "xml", 3) == 0)
        method = XSLT_OUTPUT_XML;
      else if (value_length == 4)
        if (uni_strncmp (value, "html", 4) == 0)
          method = XSLT_OUTPUT_HTML;
        else if (uni_strncmp (value, "text", 4) == 0)
          method = XSLT_OUTPUT_TEXT;

      if (method != XSLT_OUTPUT_DEFAULT)
        parser->GetStylesheet ()->SetOutputMethod (method);
      break;

    case XSLTA_VERSION:
      XSLT_ReplaceSpecificationValueL (output_specification.version, output_specification.owns_version, value, value_length);
      break;

    case XSLTA_ENCODING:
      XSLT_ReplaceSpecificationValueL (output_specification.encoding, output_specification.owns_encoding, value, value_length);
      break;

    case XSLTA_OMIT_XML_DECLARATION:
      if (value_length == 3 && uni_strncmp (value, "yes", 3) == 0)
        output_specification.omit_xml_declaration = TRUE;
      break;

    case XSLTA_STANDALONE:
      if (value_length == 3 && uni_strncmp (value, "yes", 3) == 0)
        output_specification.standalone = XMLSTANDALONE_YES;
      else if (value_length == 2 && uni_strncmp (value, "no", 2) == 0)
        output_specification.standalone = XMLSTANDALONE_NO;
      break;

    case XSLTA_DOCTYPE_PUBLIC:
      XSLT_ReplaceSpecificationValueL (output_specification.doctype_public_id, output_specification.owns_doctype_public_id, value, value_length);
      break;

    case XSLTA_DOCTYPE_SYSTEM:
      XSLT_ReplaceSpecificationValueL (output_specification.doctype_system_id, output_specification.owns_doctype_system_id, value, value_length);
      break;

    case XSLTA_CDATA_SECTION_ELEMENTS:
      parser->AddCDATASectionElementsL (value, value_length);
      break;

    case XSLTA_INDENT:
      if (value_length == 3 && uni_strncmp (value, "yes", 3) == 0)
        output_specification.indent = TRUE;
      break;

    case XSLTA_MEDIA_TYPE:
      XSLT_ReplaceSpecificationValueL (output_specification.media_type, output_specification.owns_media_type, value, value_length);
      break;

#ifdef XSLT_COPY_TO_FILE_SUPPORT
    case XSLTA_COPY_TO_FILE:
      parser->GetStylesheet ()->SetCopyToFileL (value, value_length);
      break;
#endif // XSLT_COPY_TO_FILE_SUPPORT

    case XSLTA_OTHER:
    case XSLTA_NO_MORE_ATTRIBUTES:
      break;

    default:
      XSLT_Element::AddAttributeL (parser, type, name, value, value_length);
    }
}

#endif // XSLT_SUPPORT
