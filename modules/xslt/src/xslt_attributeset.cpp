/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_attributeset.h"

#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_elemorattr.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_utils.h"
#include "modules/xmlutils/xmlutils.h"

void
XSLT_AttributeSet::AddAttributeL (XSLT_ElementOrAttribute *attribute)
{
  OpStackAutoPtr<XSLT_ElementOrAttribute> anchor (attribute);
  void **attributes0 = reinterpret_cast<void **> (attributes);
  XSLT_Utils::GrowArrayL (&attributes0, attributes_count, attributes_count + 1, attributes_total);
  attributes = reinterpret_cast<XSLT_ElementOrAttribute **> (attributes0);

  attributes[attributes_count++] = anchor.release();
}

XSLT_AttributeSet::XSLT_AttributeSet ()
  : has_name (FALSE),
    attributes (0),
    attributes_count (0),
    attributes_total (0),
    next (0),
    next_used (0),
    use_attribute_sets(0)
{
}

XSLT_AttributeSet::~XSLT_AttributeSet ()
{
  for (unsigned index = 0; index < attributes_count; ++index)
    OP_DELETE (attributes[index]);
  OP_DELETEA (attributes);

  OP_DELETE (use_attribute_sets);

  OP_DELETE (next);
}

/* virtual */ XSLT_Element *
XSLT_AttributeSet::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  if (type == XSLTE_ATTRIBUTE)
    {
      XSLT_ElementOrAttribute *attribute = OP_NEW_L (XSLT_ElementOrAttribute, (TRUE, parser->GetCurrentXMLVersion ()));

      AddAttributeL (attribute); // will free attribute if it fails

      attribute->SetType (XSLTE_ATTRIBUTE);
      attribute->SetParent (this);

#if defined XSLT_ERRORS && defined XML_ERRORS
      attribute->SetLocation (parser->GetCurrentLocation ());
#endif // XSLT_ERRORS && XML_ERRORS

      return attribute;
    }

  parser->SignalErrorL ("unexpected element in xsl:output");
  ignore_element = TRUE;
  return this;
}

/* virtual */ BOOL
XSLT_AttributeSet::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  if (parser)
    {
      parser->GetStylesheet ()->AddAttributeSet (this);
      return FALSE;
    }
  else
    return TRUE;
}

/* virtual */ void
XSLT_AttributeSet::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_NAME:
      parser->SetQNameAttributeL (value, value_length, FALSE, 0, &name);
      has_name = TRUE;
      break;

    case XSLTA_USE_ATTRIBUTE_SET:
      OP_ASSERT(!use_attribute_sets);
      use_attribute_sets = XSLT_UseAttributeSets::MakeL (parser, value, value_length);
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!has_name)
        parser->SignalErrorL ("xsl:attribute-set missing required name attribute");
      if (use_attribute_sets)
        use_attribute_sets->FinishL (parser, this);

    case XSLTA_OTHER:
      break;

    default:
      XSLT_Element::AddAttributeL (parser, type, name, value, value_length);
    }
}

void
XSLT_AttributeSet::CompileL (XSLT_Compiler *compiler)
{
  XSLT_StylesheetImpl *stylesheet = compiler->GetStylesheet ();
  XSLT_AttributeSet *attribute_set = next_used = stylesheet->GetUsedAttributeSet ();

  while (attribute_set)
    if (attribute_set == this)
      {
        XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_ERROR, reinterpret_cast<UINTPTR> ("recursively used attribute-set"));
        return;
      }
    else
      attribute_set = attribute_set->next_used;

  stylesheet->SetUsedAttributeSet (this);

  if (use_attribute_sets)
    use_attribute_sets->CompileL(compiler);

  for (unsigned index = 0; index < attributes_count; ++index)
    attributes[index]->CompileL (compiler);

  stylesheet->SetUsedAttributeSet (next_used);
}

XSLT_AttributeSet *
XSLT_AttributeSet::Find (const XMLExpandedName &name)
{
  XSLT_AttributeSet *attribute_set = this;

  while (attribute_set)
    if (static_cast<const XMLExpandedName &> (attribute_set->name) == name)
      return attribute_set;
    else
      attribute_set = attribute_set->next;

  return 0;
}

XSLT_AttributeSet *
XSLT_AttributeSet::FindNext ()
{
  if (next)
    return next->Find (name);
  else
    return 0;
}

/* static */ void
XSLT_AttributeSet::Push (XSLT_AttributeSet *&existing, XSLT_AttributeSet *attribute_set)
{
  XSLT_AttributeSet **pointer = &existing;

  while (*pointer)
    pointer = &(*pointer)->next;

  *pointer = attribute_set;
}

XSLT_UseAttributeSets::XSLT_UseAttributeSets ()
  : names (0),
    names_count (0)
{
}

XSLT_UseAttributeSets::~XSLT_UseAttributeSets ()
{
  OP_DELETEA (names);
}

/* static */ XSLT_UseAttributeSets *
XSLT_UseAttributeSets::MakeL (XSLT_StylesheetParserImpl *parser, const uni_char *value, unsigned value_length)
{
  XSLT_UseAttributeSets *use_attribute_sets = OP_NEW_L (XSLT_UseAttributeSets, ());

  if (OpStatus::IsMemoryError (use_attribute_sets->value.Set (value, value_length)))
    {
      OP_DELETE (use_attribute_sets);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  use_attribute_sets->xmlversion = parser->GetCurrentXMLVersion ();
  return use_attribute_sets;
}

void
XSLT_UseAttributeSets::FinishL (XSLT_StylesheetParserImpl *parser, XSLT_Element *element)
{
 again:
  const uni_char *string = value.CStr ();
  unsigned string_length = value.Length ();

  names_count = 0;

  while (string_length != 0)
    {
      const uni_char *qname = string;
      unsigned ch = 0;

      while (string_length != 0 && XMLUtils::IsSpace (ch = XMLUtils::GetNextCharacter (string, string_length))) qname = string;

      if (string_length != 0)
        {
          const uni_char *qname_end = string;

          while (!XMLUtils::IsSpace (ch))
            {
              qname_end = string;
              if (string_length == 0)
                break;
              ch = XMLUtils::GetNextCharacter (string, string_length);
            }

          unsigned qname_length = qname_end - qname;

          if (!XMLUtils::IsValidQName (xmlversion, qname, qname_length))
            element->SignalErrorL (parser, "invalid QName in attribute value");

          if (!names)
            ++names_count;
          else
            {
              XMLCompleteNameN name (qname, qname_length);

              if (!XMLNamespaceDeclaration::ResolveName (element->GetNamespaceDeclaration (), name, FALSE))
                element->SignalErrorL (parser, "undeclared prefix in attribute value");

              /* Skip duplicates. */
              for (unsigned index = 0; index < names_count; ++index)
                if (names[index] == static_cast<const XMLExpandedNameN &> (name))
                  continue;

              if (OpStatus::IsMemoryError (names[names_count++].Set (name)))
                LEAVE (OpStatus::ERR_NO_MEMORY);
            }
        }
    }

  if (names_count != 0 && !names)
    {
      names = OP_NEWA_L (XMLExpandedName, names_count);
      goto again;
    }
}

XSLT_AttributeSet *
XSLT_UseAttributeSets::GetAttributeSet (XSLT_StylesheetImpl *stylesheet, unsigned index)
{
  return stylesheet->FindAttributeSet (names[index]);
}

void
XSLT_UseAttributeSets::CompileL (XSLT_Compiler *compiler)
{
  XSLT_StylesheetImpl *stylesheet = compiler->GetStylesheet ();

  for (unsigned index = 0; index < names_count; ++index)
    {
      XSLT_AttributeSet *attribute_set = GetAttributeSet (stylesheet, index);

      while (attribute_set)
        {
          attribute_set->CompileL (compiler);
          attribute_set = attribute_set->FindNext ();
        }
    }
}

#endif // XSLT_SUPPORT
