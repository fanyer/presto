/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_elemorattr.h"

#include "modules/xslt/src/xslt_attributevaluetemplate.h"
#include "modules/xslt/src/xslt_attributeset.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_xpathextensions.h"
#include "modules/xslt/src/xslt_literalresultelement.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/util/tempbuf.h"

XSLT_ElementOrAttribute::XSLT_ElementOrAttribute (BOOL is_attribute, XMLVersion xmlversion)
  : is_attribute (is_attribute),
    use_attribute_sets (0),
    xmlversion (xmlversion)
{
}

XSLT_ElementOrAttribute::~XSLT_ElementOrAttribute ()
{
  OP_DELETE (use_attribute_sets);
}

/* virtual */ void
XSLT_ElementOrAttribute::CompileL (XSLT_Compiler *compiler)
{
  unsigned invalid_qname_jump = 0, valid_qname_jump = 0, name_index = ~0u;
  BOOL3 valid_qname;

  if (XSLT_AttributeValueTemplate::NeedsCompiledCode (name) || ns.IsSpecified () && XSLT_AttributeValueTemplate::NeedsCompiledCode (ns))
    {
      valid_qname = MAYBE;

      XSLT_AttributeValueTemplate::CompileL (compiler, this, name);

      invalid_qname_jump = XSLT_ADD_JUMP_INSTRUCTION (XSLT_Instruction::IC_SET_QNAME);

      if (ns.IsSpecified ())
        {
          XSLT_AttributeValueTemplate::CompileL (compiler, this, ns);
          XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_SET_URI);
        }
      else
        {
          unsigned argument = compiler->AddNamespaceDeclarationL (GetNamespaceDeclaration ());
          if (GetType () == XSLTE_ELEMENT)
            argument |= 0x80000000u;
          XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_RESOLVE_NAME, argument);
        }
    }
  else
    {
      TempBuffer name_buffer; ANCHOR (TempBuffer, name_buffer);
      TempBuffer ns_buffer; ANCHOR (TempBuffer, ns_buffer);
      const uni_char *qname = XSLT_AttributeValueTemplate::UnescapeL (name, name_buffer);

      if (XMLUtils::IsValidQName (GetXMLVersion (), qname))
        {
          valid_qname = YES;

          XMLCompleteNameN completename (qname, uni_strlen (qname));

          if (!ns.IsSpecified ())
            {
              if (!XMLNamespaceDeclaration::ResolveName (GetNamespaceDeclaration (), completename, GetType () == XSLTE_ELEMENT))
                {
                  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_ERROR, reinterpret_cast<UINTPTR> (GetType () == XSLTE_ELEMENT ? "unresolved prefix in xsl:element/@name" : "unresolved prefix in xsl:attribute/@name"));
                  return;
                }
            }
          else if (*ns.GetString ())
            {
              const uni_char *uri = XSLT_AttributeValueTemplate::UnescapeL (ns, ns_buffer);
              completename.SetUri (uri, uni_strlen (uri));
            }

          name_index = compiler->AddNameL (completename, FALSE);
        }
      else
        valid_qname = NO;
    }

  if (valid_qname != NO)
    if (GetType () == XSLTE_ELEMENT)
      {
        XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_START_ELEMENT, name_index);

        if (use_attribute_sets)
          use_attribute_sets->CompileL (compiler);

        XSLT_TemplateContent::CompileL (compiler);

        XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_END_ELEMENT, name_index);

        if (valid_qname != YES)
          valid_qname_jump = XSLT_ADD_JUMP_INSTRUCTION (XSLT_Instruction::IC_JUMP);
      }
    else
      {
        if (children_count == 1 && children[0]->GetType () == XSLTE_LITERAL_RESULT_TEXT_NODE)
          XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_SET_STRING, compiler->AddStringL (static_cast<XSLT_LiteralResultTextNode *> (children[0])->GetText ()));
        else
          {
            XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_START_COLLECT_TEXT);
            XSLT_TemplateContent::CompileL (compiler);
            XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_END_COLLECT_TEXT);
          }

        XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_ADD_ATTRIBUTE, name_index);

        if (valid_qname != YES)
          compiler->SetJumpDestination (invalid_qname_jump);
      }

  if (valid_qname != YES && GetType () == XSLTE_ELEMENT)
    {
      if (valid_qname == MAYBE)
        compiler->SetJumpDestination (invalid_qname_jump);

      XSLT_TemplateContent::CompileL (compiler);
    }

  if (valid_qname == MAYBE && GetType () == XSLTE_ELEMENT)
    compiler->SetJumpDestination (valid_qname_jump);
}

/* virtual */ void
XSLT_ElementOrAttribute::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_NAME:
      parser->SetStringL (name, completename, value, value_length);
      break;

    case XSLTA_NAMESPACE:
      parser->SetStringL (ns, completename, value, value_length);
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (!name.IsSpecified())
        SignalErrorL (parser, is_attribute ? "xsl:attribute missing required name attribute" : "xsl:element missing required name attribute");
      if (use_attribute_sets)
        use_attribute_sets->FinishL (parser, this);
      break;

    case XSLTA_USE_ATTRIBUTE_SET:
      if (!is_attribute)
        {
          use_attribute_sets = XSLT_UseAttributeSets::MakeL (parser, value, value_length);
          break;
        }

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, completename, value, value_length);
    }
}

#endif // XSLT_SUPPORT
