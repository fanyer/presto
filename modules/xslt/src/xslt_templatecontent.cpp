/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"

#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_applyimports.h"
#include "modules/xslt/src/xslt_applytemplates.h"
#include "modules/xslt/src/xslt_calltemplate.h"
#include "modules/xslt/src/xslt_choose.h"
#include "modules/xslt/src/xslt_comment.h"
#include "modules/xslt/src/xslt_copy.h"
#include "modules/xslt/src/xslt_copyof.h"
#include "modules/xslt/src/xslt_elemorattr.h"
#include "modules/xslt/src/xslt_fallback.h"
#include "modules/xslt/src/xslt_foreach.h"
#include "modules/xslt/src/xslt_iforwhen.h"
#include "modules/xslt/src/xslt_key.h"
#include "modules/xslt/src/xslt_message.h"
#include "modules/xslt/src/xslt_number.h"
#include "modules/xslt/src/xslt_processinginstruction.h"
#include "modules/xslt/src/xslt_text.h"
#include "modules/xslt/src/xslt_template.h"
#include "modules/xslt/src/xslt_valueof.h"
#include "modules/xslt/src/xslt_variable.h"
#include "modules/xslt/src/xslt_unsupported.h"
#include "modules/xslt/src/xslt_literalresultelement.h"
#include "modules/xslt/src/xslt_compiler.h"

XSLT_TemplateContent::XSLT_TemplateContent ()
  : children (0),
    children_count (0),
    children_total (0),
    variables_count (0)
{
  SetParent (parent);
}

/* virtual */
XSLT_TemplateContent::~XSLT_TemplateContent ()
{
  for (unsigned index = 0; index < children_count; ++index)
    OP_DELETE (children[index]);

  OP_DELETEA (children);
}

XSLT_Variable *
XSLT_TemplateContent::CalculatePreviousVariable (XSLT_StylesheetImpl *stylesheet)
{
  XSLT_Variable *previous_variable = 0;

  if (parent->IsTemplateContent ())
    {
      XSLT_TemplateContent *parent_template_content = static_cast<XSLT_TemplateContent *> (parent);
      unsigned index = parent_template_content->children_count;

      while (parent_template_content->children[--index] != this) {}

      while (index != 0)
        {
          XSLT_TemplateContent *previous_sibling = parent_template_content->children[--index];

          if (previous_sibling->GetType () == XSLTE_PARAM || previous_sibling->GetType () == XSLTE_VARIABLE)
            {
              previous_variable = static_cast<XSLT_Variable *> (previous_sibling);
              break;
            }
          else if (previous_sibling->extensions.GetPreviousVariable ())
            {
              previous_variable = previous_sibling->extensions.GetPreviousVariable ();
              break;
            }
        }

      if (!previous_variable)
        {
          if (!parent_template_content->extensions.GetPreviousVariable ())
            parent_template_content->CalculatePreviousVariable (stylesheet);

          previous_variable = parent_template_content->extensions.GetPreviousVariable ();
        }
    }

  return previous_variable;
}

/* virtual */ XSLT_Element *
XSLT_TemplateContent::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  XSLT_TemplateContent *child = 0;

  switch (type)
    {
    default:
      OP_ASSERT(!"Unknown element type in StartElementL");
      /* fall through */
    case XSLTE_UNSUPPORTED:
    case XSLTE_UNRECOGNIZED:
      child = OP_NEW_L (XSLT_Unsupported, ());
      break;

    case XSLTE_ATTRIBUTE_SET:
    case XSLTE_DECIMAL_FORMAT:
    case XSLTE_IMPORT:
    case XSLTE_INCLUDE:
    case XSLTE_KEY:
    case XSLTE_NAMESPACE_ALIAS:
    case XSLTE_OUTPUT:
    case XSLTE_PRESERVE_SPACE:
    case XSLTE_STRIP_SPACE:
    case XSLTE_TEMPLATE:
      parser->SignalErrorL ("top-level element in xsl:template");
      ignore_element = TRUE;
      return this;

    case XSLTE_STYLESHEET:
    case XSLTE_TRANSFORM:
      parser->SignalErrorL ("root element in xsl:template");
      ignore_element = TRUE;
      return this;

    case XSLTE_SORT:
      parser->SignalErrorL ("misplaced xsl:sort");
      ignore_element = TRUE;
      return this;

    case XSLTE_APPLY_IMPORTS:
      child = OP_NEW_L (XSLT_ApplyImports, ());
      break;

    case XSLTE_APPLY_TEMPLATES:
      child = OP_NEW_L (XSLT_ApplyTemplates, ());
      break;

    case XSLTE_ATTRIBUTE:
    case XSLTE_ELEMENT:
      child = OP_NEW_L (XSLT_ElementOrAttribute, (type == XSLTE_ATTRIBUTE, parser->GetCurrentXMLVersion ()));
      break;

    case XSLTE_CALL_TEMPLATE:
      child = OP_NEW_L (XSLT_CallTemplate, ());
      break;

    case XSLTE_CHOOSE:
      child = OP_NEW_L (XSLT_Choose, ());
      break;

    case XSLTE_COMMENT:
      child = OP_NEW_L (XSLT_Comment, ());
      break;

    case XSLTE_COPY:
      child = OP_NEW_L (XSLT_Copy, ());
      break;

    case XSLTE_COPY_OF:
      child = OP_NEW_L (XSLT_CopyOf, ());
      break;

    case XSLTE_FALLBACK:
      child = OP_NEW_L (XSLT_Fallback, (GetType () == XSLTE_UNSUPPORTED || GetType () == XSLTE_UNRECOGNIZED));
      break;

    case XSLTE_FOR_EACH:
      child = OP_NEW_L (XSLT_ForEach, ());
      break;

    case XSLTE_IF:
    case XSLTE_WHEN:
      child = OP_NEW_L (XSLT_IfOrWhen, ());
      break;

    case XSLTE_MESSAGE:
      child = OP_NEW_L (XSLT_Message, ());
      break;

    case XSLTE_NUMBER:
      child = OP_NEW_L (XSLT_Number, ());
      break;

    case XSLTE_OTHERWISE:
      child = OP_NEW_L (XSLT_TemplateContent, ());
      break;

    case XSLTE_PROCESSING_INSTRUCTION:
      child = OP_NEW_L (XSLT_ProcessingInstruction, (parser->GetCurrentXMLVersion ()));
      break;

    case XSLTE_TEXT:
      child = OP_NEW_L (XSLT_Text, ());
      break;

    case XSLTE_VALUE_OF:
      child = OP_NEW_L (XSLT_ValueOf, ());
      break;

    case XSLTE_PARAM:
    case XSLTE_VARIABLE:
    case XSLTE_WITH_PARAM:
      child = OP_NEW_L (XSLT_Variable, ());
      break;

    case XSLTE_OTHER:
      child = OP_NEW_L (XSLT_LiteralResultElement, ());
    }

  /* Add child adds the child (which anchors it) or deletes it. */
  AddChildL (child);

  child->SetType (type);
  child->SetParent (this);
  child->GetXPathExtensions ()->SetPreviousVariable (child->CalculatePreviousVariable (parser->GetStylesheet ()));

#if defined XSLT_ERRORS && defined XML_ERRORS
  child->SetLocation (parser->GetCurrentLocation ());
#endif // XSLT_ERRORS && XML_ERRORS

  if (type == XSLTE_OTHER)
    ((XSLT_LiteralResultElement *) child)->SetNameL (name);

  return child;
}

/* virtual */ BOOL
XSLT_TemplateContent::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  return FALSE;
}

/* virtual */ void
XSLT_TemplateContent::AddCharacterDataL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetParser::CharacterDataType type, const uni_char *value, unsigned value_length)
{
  XSLT_LiteralResultTextNode *child = OP_NEW_L (XSLT_LiteralResultTextNode, ());

  AddChildL (child);

  child->SetTextL (value, value_length);
}

/* virtual */ void
XSLT_TemplateContent::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_OTHER:
    case XSLTA_NO_MORE_ATTRIBUTES:
      break;

    default:
      XSLT_Element::AddAttributeL (parser, type, name, value, value_length);
    }
}

void
XSLT_TemplateContent::AddChildL (XSLT_TemplateContent *child)
{
  if (children_count == children_total)
    {
      OpStackAutoPtr<XSLT_TemplateContent> anchor (child);

      unsigned new_children_total = children_total == 0 ? 8 : children_total + children_total;
      XSLT_TemplateContent **new_children = OP_NEWA_L (XSLT_TemplateContent *, new_children_total);

      op_memcpy (new_children, children, children_count * sizeof children[0]);
      OP_DELETEA (children);

      children = new_children;
      children_total = new_children_total;

      anchor.release ();
    }

  children[children_count++] = child;
}

/* virtual */ void
XSLT_TemplateContent::CompileL (XSLT_Compiler *compiler)
{
  for (unsigned index = 0; index < children_count; ++index)
    children[index]->CompileL (compiler);
}

/* virtual */ XSLT_Element *
XSLT_EmptyTemplateContent::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  parser->SignalErrorL ("unexpected element");
  ignore_element = TRUE;
  return this;
}

/* virtual */ void
XSLT_EmptyTemplateContent::AddCharacterDataL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetParser::CharacterDataType type, const uni_char *value, unsigned value_length)
{
  XSLT_Element::AddCharacterDataL (parser, type, value, value_length);
}

#endif // XSLT_SUPPORT
