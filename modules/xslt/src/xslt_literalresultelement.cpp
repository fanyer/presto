/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_literalresultelement.h"
#include "modules/xslt/src/xslt_attributevaluetemplate.h"
#include "modules/xslt/src/xslt_attributeset.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_xpathextensions.h"
#include "modules/util/str.h"

void
XSLT_LiteralResultElement::Attribute::SetNameL (const XMLCompleteNameN &completename)
{
  name.SetL (completename);
}

void
XSLT_LiteralResultElement::Attribute::SetValueL (XSLT_StylesheetParserImpl *parser, const uni_char *string, unsigned string_length)
{
  parser->SetStringL(value, name, string, string_length);
}

void
XSLT_LiteralResultElement::Attribute::SetNamespaceDeclaration (XSLT_StylesheetParserImpl *parser)
{
  nsdeclaration = parser->GetNamespaceDeclaration ();
}

const XMLCompleteName &
XSLT_LiteralResultElement::Attribute::GetName ()
{
  return name;
}

void
XSLT_LiteralResultElement::Attribute::CompileL (XSLT_Compiler *compiler, XSLT_Element *element)
{
  unsigned name_index = compiler->AddNameL (name, TRUE);

  XSLT_AttributeValueTemplate::CompileL (compiler, element, value);

  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL (XSLT_Instruction::IC_ADD_ATTRIBUTE, name_index, element);
}

XSLT_LiteralResultElement::XSLT_LiteralResultElement ()
  : attributes (0),
    attributes_count (0),
    attributes_total (0),
    use_attribute_sets (0),
    excluded_namespaces (XSLT_NamespaceCollection::TYPE_EXCLUDED_RESULT_PREFIXES),
    extension_namespaces (XSLT_NamespaceCollection::TYPE_EXTENSION_ELEMENT_PREFIXES)
{
}

XSLT_LiteralResultElement::~XSLT_LiteralResultElement ()
{
  for (unsigned index = 0; index < attributes_count; ++index)
    OP_DELETE (attributes[index]);

  OP_DELETEA (attributes);
  OP_DELETE (use_attribute_sets);
}

void
XSLT_LiteralResultElement::SetNameL (const XMLCompleteNameN &completename)
{
  name.SetL (completename);
}

/* virtual */ void
XSLT_LiteralResultElement::CompileL (XSLT_Compiler *compiler)
{
  unsigned name_index = compiler->AddNameL (name, TRUE);

  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_START_ELEMENT, name_index);
  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_SUGGEST_NSDECL, reinterpret_cast<UINTPTR> (static_cast<XSLT_Element *> (this)));

  if (use_attribute_sets)
    use_attribute_sets->CompileL (compiler);

  for (unsigned index = 0; index < attributes_count; ++index)
    attributes[index]->CompileL (compiler, this);

  XSLT_TemplateContent::CompileL (compiler);

  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_END_ELEMENT, name_index);
}

/* virtual */ BOOL
XSLT_LiteralResultElement::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  if (parser)
    for (unsigned index = 0; index < attributes_count; ++index)
      attributes[index]->SetNamespaceDeclaration (parser);

  return FALSE;
}

void
XSLT_LiteralResultElement::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_NO_MORE_ATTRIBUTES:
      excluded_namespaces.FinishL (parser, this);
      extension_namespaces.FinishL (parser, this);
      if (use_attribute_sets)
        use_attribute_sets->FinishL (parser, this);
      break;

    case XSLTA_EXTENSION_ELEMENT_PREFIXES:
    case XSLTA_VERSION:
      if (parent->GetType () == XSLTE_TEMPLATE && parent->GetIsInserted ())
        parent->GetParent ()->AddAttributeL (parser, type, completename, value, value_length);
      else if (type == XSLTA_EXTENSION_ELEMENT_PREFIXES)
        parser->SetStringL (extension_namespaces, completename, value, value_length);
      break;

    case XSLTA_EXCLUDE_RESULT_PREFIXES:
      parser->SetStringL (excluded_namespaces, completename, value, value_length);
      break;

    case XSLTA_USE_ATTRIBUTE_SET:
      use_attribute_sets = XSLT_UseAttributeSets::MakeL (parser, value, value_length);
      break;

    case XSLTA_OTHER:
      OpStackAutoPtr<Attribute> attribute (OP_NEW_L (Attribute, ()));

      if (completename.IsXMLNamespaces())
        return;

      attribute->SetNameL (completename);
      attribute->SetValueL (parser, value, value_length);

      if (attributes_count == attributes_total)
        {
          unsigned new_attributes_total = attributes_total == 0 ? 8 : attributes_total + attributes_total;
          Attribute **new_attributes = OP_NEWA_L (Attribute *, new_attributes_total);

          op_memcpy (new_attributes, attributes, attributes_count * sizeof attributes[0]);
          OP_DELETEA (attributes);

          attributes = new_attributes;
          attributes_total = new_attributes_total;
        }

      attributes[attributes_count++] = attribute.release ();
    }
}

XSLT_LiteralResultTextNode::XSLT_LiteralResultTextNode ()
  : text (0)
{
  SetType (XSLTE_LITERAL_RESULT_TEXT_NODE);
}

XSLT_LiteralResultTextNode::~XSLT_LiteralResultTextNode ()
{
  OP_DELETEA (text);
}

void
XSLT_LiteralResultTextNode::SetTextL (const uni_char *new_text, unsigned new_text_length)
{
  LEAVE_IF_ERROR (UniSetStrN (text, new_text, new_text_length));
}

void
XSLT_LiteralResultTextNode::CompileL (XSLT_Compiler *compiler)
{
  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_SET_STRING, compiler->AddStringL (text));
  XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_ADD_TEXT);
}

#endif // XSLT_SUPPORT
