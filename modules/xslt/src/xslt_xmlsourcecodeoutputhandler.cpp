/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_xmlsourcecodeoutputhandler.h"
# include "modules/xslt/src/xslt_outputbuffer.h"

XSLT_XMLSourceCodeOutputHandler::XSLT_XMLSourceCodeOutputHandler (XSLT_OutputBuffer *buffer, XSLT_StylesheetImpl *stylesheet, BOOL expand_empty_elements /* = FALSE */)
  : XSLT_XMLOutputHandler (stylesheet),
    buffer (buffer),
	expand_empty_elements(expand_empty_elements),
    in_cdata_section (FALSE)
{
}

static void
XSLT_XMLWriteCDATASection (XSLT_OutputBuffer *buffer, const uni_char *data, BOOL &in_cdata_section)
{
  while (*data)
    {
      const uni_char *start = data;

      while (*data && *data != ']' && XMLUtils::IsChar10 (*data))
        ++data;

      if (data != start)
        {
          if (!in_cdata_section)
            {
              buffer->WriteL ("<![CDATA[");
              in_cdata_section = TRUE;
            }

          buffer->WriteL (start, data - start);
        }

      if (*data == ']' && data[1] == ']' && data[2] == '>')
        {
          if (!in_cdata_section)
            {
              buffer->WriteL ("<![CDATA[");
              in_cdata_section = TRUE;
            }

          buffer->WriteL ("]]]]><![CDATA[>");

          data += 3;
        }
      else if (*data)
        /* FIXME: Signal error? */
        ++data;
    }
}

static void
XSLT_XMLWriteEscapedL (XSLT_OutputBuffer *buffer, const uni_char *data, BOOL is_attribute_value)
{
  while (*data)
    {
      const uni_char *start = data;

    continue_scan:
      while (*data && *data != '<' && *data != '>' && *data != '&' && *data != '"' && XMLUtils::IsChar10 (*data))
        ++data;

      if (data != start)
        buffer->WriteL (start, data - start);

      switch (*data)
        {
        case 0:
          return;

        case '<':
          buffer->WriteL ("&lt;");
          break;

        case '>':
          if (!is_attribute_value && data - start >= 2 && data[-1] == ']' && data[-2] == ']')
            buffer->WriteL ("&gt;");
          else
            {
              start = data;
              ++data;
              goto continue_scan;
            }
          break;

        case '&':
          buffer->WriteL ("&amp;");
          break;

        case '"':
          if (is_attribute_value)
            buffer->WriteL ("&quot;");
          else
            {
              start = data;
              ++data;
              goto continue_scan;
            }
        }

      ++data;
    }
}

/* virtual */ void
XSLT_XMLSourceCodeOutputHandler::AddTextL (const uni_char *data, BOOL disable_output_escaping)
{
  XSLT_XMLOutputHandler::AddTextL (data, disable_output_escaping);

  /* XSLT_HTMLSourceCodeOutputHandler uses AddTextL (0, FALSE) to "close" XML
     start tags. */
  if (data)
    if (disable_output_escaping)
      buffer->WriteL (data);
    else if (UseCDATASections ())
      XSLT_XMLWriteCDATASection (buffer, data, in_cdata_section);
    else
      XSLT_XMLWriteEscapedL (buffer, data, FALSE);
}

/* virtual */ void
XSLT_XMLSourceCodeOutputHandler::AddCommentL (const uni_char *data)
{
  XSLT_XMLOutputHandler::AddCommentL (data);
  CloseCDATASectionL ();

  buffer->WriteL ("<!--");
  buffer->WriteL (data);
  buffer->WriteL ("-->");
}

/* virtual */ void
XSLT_XMLSourceCodeOutputHandler::AddProcessingInstructionL (const uni_char *target, const uni_char *data)
{
  XSLT_XMLOutputHandler::AddProcessingInstructionL (target, data);
  CloseCDATASectionL ();

  buffer->WriteL ("<?");
  buffer->WriteL (target);

  if (data && *data)
    {
      buffer->WriteL (" ");
      buffer->WriteL (data);
    }

  buffer->WriteL ("?>");
}

/* virtual */ void
XSLT_XMLSourceCodeOutputHandler::EndOutputL ()
{
  XSLT_XMLOutputHandler::EndOutputL ();
  buffer->EndL ();
}

/* virtual */ void
XSLT_XMLSourceCodeOutputHandler::OutputXMLDeclL (const XMLDocumentInformation &document_info)
{
  buffer->WriteL ("<?xml version=\"1.");
  switch (document_info.GetVersion ())
    {
    case XMLVERSION_1_1:
      buffer->WriteL ("1\"");
      break;

    default:
      buffer->WriteL ("0\"");
    }

  if (document_info.GetEncoding ())
    {
      buffer->WriteL (" encoding=\"");
      buffer->WriteL (document_info.GetEncoding ());
      buffer->WriteL ("\"");
    }

  if (document_info.GetStandalone () != XMLSTANDALONE_NONE)
    {
      buffer->WriteL (" standalone=\"");
      if (document_info.GetStandalone () == XMLSTANDALONE_YES)
        buffer->WriteL ("yes\"");
      else
        buffer->WriteL ("no\"");
    }

  buffer->WriteL ("?>\n");
}

/* virtual */ void
XSLT_XMLSourceCodeOutputHandler::OutputDocumentTypeDeclL (const XMLDocumentInformation &document_info)
{
  buffer->WriteL ("<!DOCTYPE ");
  buffer->WriteL (document_info.GetDoctypeName ());

  if (document_info.GetPublicId ())
    {
      buffer->WriteL (" PUBLIC \"");
      buffer->WriteL (document_info.GetPublicId ());

      if (document_info.GetSystemId ())
        {
          buffer->WriteL ("\" \"");
          buffer->WriteL (document_info.GetSystemId ());
        }
    }
  else if (document_info.GetSystemId ())
    {
      buffer->WriteL (" SYSTEM \"");
      buffer->WriteL (document_info.GetSystemId ());
    }

  buffer->WriteL ("\">\n");
}

/* virtual */ void
XSLT_XMLSourceCodeOutputHandler::OutputTagL (XMLToken::Type type, const XMLCompleteName &name)
{
  CloseCDATASectionL ();

  if (type != XMLToken::TYPE_ETag)
    {
      buffer->WriteL ("<");
      buffer->WriteL (name);

      for (unsigned index = 0; index < nsnormalizer.GetAttributesCount (); ++index)
        {
          buffer->WriteL (" ");
          buffer->WriteL (nsnormalizer.GetAttributeName (index));
          buffer->WriteL ("=\"");
          XSLT_XMLWriteEscapedL (buffer, nsnormalizer.GetAttributeValue (index), TRUE);
          buffer->WriteL ("\"");
        }

      if (type == XMLToken::TYPE_STag || expand_empty_elements)
        buffer->WriteL (">");
      else
        buffer->WriteL ("/>");
    }

  if (type == XMLToken::TYPE_ETag ||
      type == XMLToken::TYPE_EmptyElemTag && expand_empty_elements)
    {
      buffer->WriteL ("</");
      buffer->WriteL (name);
      buffer->WriteL (">");
    }
}

void
XSLT_XMLSourceCodeOutputHandler::CloseCDATASectionL ()
{
  if (in_cdata_section)
    {
      buffer->WriteL ("]]>");
      in_cdata_section = FALSE;
    }
}

#endif // XSLT_SUPPORT
