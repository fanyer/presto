/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_htmlsourcecodeoutputhandler.h"
# include "modules/xslt/src/xslt_xmlsourcecodeoutputhandler.h"
# include "modules/xslt/src/xslt_outputbuffer.h"
# include "modules/xslt/src/xslt_stylesheet.h"
# include "modules/logdoc/html.h"
# include "modules/logdoc/html_att.h"
# include "modules/logdoc/htm_lex.h"

XSLT_HTMLSourceCodeOutputHandler::XSLT_HTMLSourceCodeOutputHandler (XSLT_OutputBuffer *buffer, XSLT_StylesheetImpl *stylesheet)
  : in_html_starttag (FALSE),
    in_xml_starttag (FALSE),
    in_cdata_element (0),
    first_used_attribute (0),
    first_free_attribute (0),
    buffer (buffer),
    output_specification (stylesheet->GetOutputSpecification ()),
    output_doctype (TRUE),
    xml_backup (0)
{
}

XSLT_HTMLSourceCodeOutputHandler::~XSLT_HTMLSourceCodeOutputHandler ()
{
  OP_DELETE (first_used_attribute);
  OP_DELETE (first_free_attribute);
  OP_DELETE (xml_backup);
}

/* virtual */ void
XSLT_HTMLSourceCodeOutputHandler::StartElementL (const XMLCompleteName &name)
{
  OutputTagL (FALSE);

  if (!name.GetUri ())
    {
      element_name.SetL (name.GetLocalPart ());
      in_html_starttag = TRUE;
    }
  else
    {
      ConstructXMLBackupL ();
      xml_backup->StartElementL (name);
      in_xml_starttag = TRUE;
    }

  // Counting depth into a cdata element
  if (in_cdata_element)
    ++in_cdata_element;
  else if (!name.GetUri ())
    {
      HTML_ElementType element_type = static_cast<HTML_ElementType> (HTM_Lex::GetElementType (element_name.CStr (), NS_HTML));
      if (element_type == HE_STYLE || element_type == HE_SCRIPT)
        in_cdata_element = 1;
    }
}

/* virtual */ void
XSLT_HTMLSourceCodeOutputHandler::AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified)
{
  if (in_html_starttag)
    {
      if (!name.GetUri ())
        {
          Attribute *attribute;

          if (first_free_attribute)
            {
              attribute = first_free_attribute;
              first_free_attribute = attribute->next;
            }
          else
            attribute = OP_NEW_L (Attribute, ());

          attribute->next = first_used_attribute;
          first_used_attribute = attribute;

          attribute->name.SetL (name.GetLocalPart ());
          attribute->value.SetL (value);
        }
    }
  else if (in_xml_starttag)
    xml_backup->AddAttributeL (name, value, id, specified);
}

static char
XSLT_ToHexDigit (unsigned number)
{
  if (number < 10)
    return '0' + number;
  else
    return 'A' + (number - 10);
}

static void
XSLT_HTMLWriteEscapedL (XSLT_OutputBuffer *buffer, const uni_char *data, BOOL is_attribute_value, BOOL is_uri_attribute_value)
{
  while (*data)
    {
      const uni_char *start = data;

    continue_scan:
      while (*data && *data != '<' && *data != '>' && *data != '&' && *data != '"' && XMLUtils::IsChar10 (*data) && *data < 128)
        ++data;

      if (data != start)
        buffer->WriteL (start, data - start);

      switch (*data)
        {
        case 0:
          return;

        case '<':
          if (!is_attribute_value)
            buffer->WriteL ("&lt;");
          else
            {
              start = data;
              ++data;
              goto continue_scan;
            }
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
          break;

        default:
          if (is_uri_attribute_value && *data >= 128)
            {
              OpString8 utf8;
              utf8.SetUTF8FromUTF16L (data, 1);
              for (unsigned index = 0, length = utf8.Length (); index < length; ++index)
                {
                  uni_char escaped[4]; /* ARRAY OK jl 2007-09-22 */
                  escaped[0] = '%';
                  escaped[1] = XSLT_ToHexDigit ((utf8.CStr ()[index] >> 4) & 0xf);
                  escaped[2] = XSLT_ToHexDigit (utf8.CStr ()[index] & 0xf);
                  escaped[3] = 0;
                  buffer->WriteL (escaped, 3);
                }
            }
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
XSLT_HTMLSourceCodeOutputHandler::AddTextL (const uni_char *data, BOOL disable_output_escaping)
{
  OutputTagL (FALSE);

  if (!disable_output_escaping && !in_cdata_element)
    XSLT_HTMLWriteEscapedL (buffer, data, FALSE, FALSE);
  else
    buffer->WriteL (data);
}

/* virtual */ void
XSLT_HTMLSourceCodeOutputHandler::AddCommentL (const uni_char *data)
{
  OutputTagL (FALSE);

  buffer->WriteL ("<!--");
  buffer->WriteL (data);
  buffer->WriteL ("-->");
}

/* virtual */ void
XSLT_HTMLSourceCodeOutputHandler::AddProcessingInstructionL (const uni_char *target, const uni_char *data)
{
  OutputTagL (FALSE);

  buffer->WriteL ("<?");
  buffer->WriteL (target);

  if (data && *data)
    {
      buffer->WriteL (" ");
      buffer->WriteL (data);
    }

  buffer->WriteL (">");
}

/* virtual */ void
XSLT_HTMLSourceCodeOutputHandler::EndElementL (const XMLCompleteName &name)
{
  if (!name.GetUri ())
    {
      if (!OutputTagL (TRUE))
        {
          buffer->WriteL ("</");
          buffer->WriteUpperL (name.GetLocalPart ());
          buffer->WriteL (">");
        }
    }
  else
    {
      xml_backup->EndElementL (name);
      in_xml_starttag = FALSE;
    }

  // Counting depth into a CDATA element
  if (in_cdata_element)
    --in_cdata_element;
}

/* virtual */ void
XSLT_HTMLSourceCodeOutputHandler::EndOutputL ()
{
  buffer->WriteL ("\n");
  buffer->EndL ();
}

BOOL
XSLT_HTMLSourceCodeOutputHandler::OutputTagL (BOOL end_element)
{
  if (output_doctype && (in_html_starttag || in_xml_starttag))
    {
      if (output_specification->doctype_public_id || output_specification->doctype_system_id)
        {
          buffer->WriteL ("<!DOCTYPE HTML ");
          if (output_specification->doctype_public_id)
            {
              buffer->WriteL ("PUBLIC \"");
              buffer->WriteL (output_specification->doctype_public_id);

              if (output_specification->doctype_system_id)
                {
                  buffer->WriteL ("\" \"");
                  buffer->WriteL (output_specification->doctype_system_id);
                }
            }
          else
            {
              buffer->WriteL ("SYSTEM \"");
              buffer->WriteL (output_specification->doctype_system_id);
            }

          buffer->WriteL ("\">\n");
        }
      output_doctype = FALSE;
    }

  if (in_html_starttag)
    {
      HTML_ElementType element_type = static_cast<HTML_ElementType> (HTM_Lex::GetElementType (element_name.CStr (), NS_HTML));

      buffer->WriteL ("<");

      if (element_type == HE_UNKNOWN)
        buffer->WriteL (element_name.CStr ());
      else
        buffer->WriteUpperL (element_name.CStr ());

      if (Attribute *attribute = first_used_attribute)
        while (TRUE)
          {
            BOOL is_uri_attribute_value = FALSE, is_boolean_attribute = FALSE;

            if (element_type != HE_UNKNOWN)
              switch (HTM_Lex::GetAttrType (attribute->name.CStr (), NS_HTML))
                {
                case ATTR_FOR:
                  if (element_type == HE_SCRIPT)
                    is_uri_attribute_value = TRUE;
                  break;

                case ATTR_SRC:
                  if (element_type == HE_IMG || element_type == HE_INPUT || element_type == HE_SCRIPT)
                    is_uri_attribute_value = TRUE;
                  break;

                case ATTR_CITE:
                  if (element_type == HE_DEL || element_type == HE_INS || element_type == HE_Q || element_type == HE_BLOCKQUOTE)
                    is_uri_attribute_value = TRUE;
                  break;

                case ATTR_HREF:
                  if (element_type == HE_A || element_type == HE_AREA || element_type == HE_BASE || element_type == HE_LINK)
                    is_uri_attribute_value = TRUE;
                  break;

                case ATTR_ISMAP:
                  if (element_type == HE_IMG || element_type == HE_INPUT)
                    is_boolean_attribute = TRUE;
                  break;

                case ATTR_DEFER:
                  if (element_type == HE_SCRIPT)
                    is_boolean_attribute = TRUE;
                  break;

                case ATTR_NOHREF:
                  if (element_type == HE_AREA)
                    is_boolean_attribute = TRUE;
                  break;

                case ATTR_ACTION:
                  if (element_type == HE_FORM)
                    is_uri_attribute_value = TRUE;
                  break;

                case ATTR_USEMAP:
                  if (element_type == HE_IMG || element_type == HE_INPUT || element_type == HE_OBJECT)
                    is_uri_attribute_value = TRUE;
                  break;

                case ATTR_CHECKED:
                  if (element_type == HE_INPUT)
                    is_boolean_attribute = TRUE;
                  break;

                case ATTR_PROFILE:
                  if (element_type == HE_HEAD)
                    is_uri_attribute_value = TRUE;
                  break;

                case ATTR_DISABLED:
                  switch (element_type)
                    {
                    case HE_INPUT:
                    case HE_SELECT:
                    case HE_TEXTAREA:
                    case HE_OPTGROUP:
                    case HE_BUTTON:
                      is_boolean_attribute = TRUE;
                    }
                  break;

                case ATTR_READONLY:
                  if (element_type == HE_INPUT || element_type == HE_TEXTAREA)
                    is_boolean_attribute = TRUE;
                  break;

                case ATTR_MULTIPLE:
                  if (element_type == HE_SELECT)
                    is_boolean_attribute = TRUE;
                  break;

                case ATTR_LONGDESC:
                  if (element_type == HE_IMG)
                    is_uri_attribute_value = TRUE;
                  break;

                case ATTR_SELECTED:
                  if (element_type == HE_OPTION)
                    is_boolean_attribute = TRUE;
                  break;

                case ATTR_CLASSID:
                case ATTR_CODEBASE:
                case ATTR_DATA:
                  if (element_type == HE_OBJECT)
                    is_uri_attribute_value = TRUE;
                  break;
                }

            buffer->WriteL (" ");
            buffer->WriteL (attribute->name.CStr ());

            if (!is_boolean_attribute)
              {
                buffer->WriteL ("=\"");
                XSLT_HTMLWriteEscapedL (buffer, attribute->value.CStr (), TRUE, is_uri_attribute_value);
                buffer->WriteL ("\"");
              }

            if (attribute->next)
              attribute = attribute->next;
            else
              {
                attribute->next = first_free_attribute;
                first_free_attribute = first_used_attribute;
                first_used_attribute = 0;
                break;
              }
          } // end attributes while(TRUE)

      buffer->WriteL (">");

      BOOL empty_element;

      switch (element_type)
        {
        case HE_AREA:
        case HE_BASE:
        case HE_BASEFONT:
        case HE_BR:
        case HE_COL:
        case HE_FRAME:
        case HE_HR:
        case HE_IMG:
        case HE_INPUT:
        case HE_ISINDEX:
        case HE_LINK:
        case HE_META:
        case HE_PARAM:
          empty_element = TRUE;
          break;

        default:
          empty_element = FALSE;
        }

      element_name.Empty ();
      in_html_starttag = FALSE;

      return empty_element;
    }
  else if (in_xml_starttag)
    {
      if (!end_element)
        xml_backup->AddTextL (0, FALSE);

      in_xml_starttag = FALSE;
    }

  return FALSE;
}

void
XSLT_HTMLSourceCodeOutputHandler::ConstructXMLBackupL ()
{
  if (!xml_backup)
    xml_backup = OP_NEW_L (XSLT_XMLSourceCodeOutputHandler, (buffer, 0, TRUE));
}

#endif // XSLT_SUPPORT
