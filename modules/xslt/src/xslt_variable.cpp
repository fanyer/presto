/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_variable.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_template.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_template.h"
#include "modules/xslt/src/xslt_engine.h"
#include "modules/xpath/xpath.h"
#include "modules/util/str.h"
#include "modules/util/tempbuf.h"

XSLT_Variable::XSLT_Variable ()
  : has_name (FALSE),
    program (0)
{
}

XSLT_Variable::~XSLT_Variable ()
{
  OP_DELETE (program);
}

/* virtual */ BOOL
XSLT_Variable::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  if (parser)
    if (!has_name)
      SignalErrorL (parser, "missing required name argument");
    else if (GetType () == XSLTE_PARAM)
      {
        XSLT_TemplateContent *content = (XSLT_TemplateContent *) GetParent ();

        while (content && content->GetType () != XSLTE_TEMPLATE)
          content = (XSLT_TemplateContent *) content->GetParent ();

        if (content)
          static_cast<XSLT_Template *>(content)->AddParamL (name, this);
      }

  XSLT_Element *parent = GetParent ();

  if (parent->GetType () == XSLTE_STYLESHEET)
    if (parser)
      parser->GetStylesheet ()->AddVariable (this);
    else
      return TRUE;

  return FALSE;
}

/* virtual */ void
XSLT_Variable::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_NAME:
      parser->SetQNameAttributeL (value, value_length, FALSE, 0, &name);
      has_name = TRUE;
      break;

    case XSLTA_SELECT:
      parser->SetStringL (select, completename, value, value_length);
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, completename, value, value_length);
    }
}

/* virtual */ void
XSLT_Variable::CompileL (XSLT_Compiler *compiler)
{
  unsigned after_variable_setting = 0; // Silence compiler

  if (GetType () == XSLTE_PARAM)
    {
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_TEST_AND_SET_IF_PARAM_IS_PRESET, reinterpret_cast<UINTPTR> (this));
      after_variable_setting = XSLT_ADD_JUMP_INSTRUCTION (XSLT_Instruction::IC_JUMP_IF_TRUE);
    }

  // Compile a program that will create a variable value and bind that to a variable
  if (select.IsSpecified () || children_count == 0)
    // Value is in the select attribute
    if (select.IsSpecified ())
      {
        XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_EVALUATE_TO_VARIABLE_VALUE, compiler->AddExpressionL (select, GetXPathExtensions (), GetNamespaceDeclaration ()));
        XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (GetType () == XSLTE_WITH_PARAM ? XSLT_Instruction::IC_SET_WITH_PARAM_FROM_EVALUATE : XSLT_Instruction::IC_SET_VARIABLE_FROM_EVALUATE, reinterpret_cast<UINTPTR> (this));
      }
    else
      {
        // This is quite inefficient
        XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_SET_STRING, compiler->AddStringL (UNI_L("")));
        XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (GetType() == XSLTE_WITH_PARAM ? XSLT_Instruction::IC_SET_WITH_PARAM_FROM_STRING : XSLT_Instruction::IC_SET_VARIABLE_FROM_STRING, reinterpret_cast<UINTPTR> (this));
      }
  else
    {
      /* Variable value in element's children.  If the children are simple we
         could collect them compile time and make this into a cheaper
         operation. */
      XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_START_COLLECT_RESULTTREEFRAGMENT);

      XSLT_TemplateContent::CompileL (compiler);

      XSLT_ADD_INSTRUCTION (XSLT_Instruction::IC_END_COLLECT_RESULTTREEFRAGMENT);
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (GetType() == XSLTE_WITH_PARAM ? XSLT_Instruction::IC_SET_WITH_PARAM_FROM_COLLECTED : XSLT_Instruction::IC_SET_VARIABLE_FROM_COLLECTED, reinterpret_cast<UINTPTR> (this));
    }

  if (GetType () == XSLTE_PARAM)
    compiler->SetJumpDestination (after_variable_setting);
}

XSLT_Program *
XSLT_Variable::CompileProgramL (XSLT_StylesheetImpl *stylesheet, XSLT_MessageHandler *messagehandler)
{
  if (!program)
    {
      XSLT_Compiler compiler (stylesheet, messagehandler); ANCHOR (XSLT_Compiler, compiler);

      CompileL (&compiler);

      program = OP_NEW_L (XSLT_Program, (XSLT_Program::TYPE_TOP_LEVEL_VARIABLE));

      compiler.FinishL (program);

#ifdef XSLT_ERRORS
      program->variable = this;
#endif // XSLT_ERRORS
    }

  return program;
}

XSLT_VariableReference::XSLT_VariableReference (XSLT_Variable* variable_elm)
  : variable_elm(variable_elm),
    is_blocking (variable_elm->GetParent ()->GetType () == XSLTE_STYLESHEET)
{
}

/* virtual */ unsigned
XSLT_VariableReference::GetValueType ()
{
  /* FIXME: It would be great if we could say for sure here. */
  return XPathVariable::TYPE_ANY;
}

/* virtual */ unsigned
XSLT_VariableReference::GetFlags ()
{
#ifdef XSLT_DEBUG_MODE
  return FLAG_BLOCKING;
#else // XSLT_DEBUG_MODE
  return is_blocking ? FLAG_BLOCKING : 0;
#endif // XSLT_DEBUG_MODE
}

/* virtual */ XPathVariable::Result
XSLT_VariableReference::GetValue (XPathValue &value, XPathExtensions::Context *extensions_context, State *&state)
{
#ifdef XSLT_DEBUG_MODE
  if (!state)
    {
      /* For debugging: pause once every time a variable is read. */
      state = OP_NEW (State, ());
      return RESULT_BLOCKED;
    }
#endif // XSLT_DEBUG_MODE

  XSLT_VariableValue *variable_value = XSLT_Engine::GetVariableValue (extensions_context, variable_elm);

  OP_STATUS status;
  if (variable_value)
    {
      if (variable_value->NeedsCalculation ())
        if (OpStatus::IsMemoryError (XSLT_Engine::CalculateVariableValue (extensions_context, variable_elm, variable_value)))
          return RESULT_OOM;
        else
          return RESULT_BLOCKED;
      else if (variable_value->IsBeingCalculated ())
        {
#ifdef XSLT_ERRORS
          if (OpStatus::IsMemoryError (XSLT_Engine::ReportCircularVariables (extensions_context, variable_elm->GetName ())))
            return RESULT_OOM;
#endif // XSLT_ERRORS

          return RESULT_FAILED;
        }
      else
        status = variable_value->SetXPathValue (value);
    }
  else
    return RESULT_FAILED;

  if (OpStatus::IsError (status))
    return OpStatus::IsMemoryError (status) ? RESULT_OOM : RESULT_FAILED;

  return RESULT_FINISHED;
}

/* static */ XSLT_VariableValue *
XSLT_VariableValue::MakeL ()
{
  XSLT_VariableValue *value = OP_NEW_L (XSLT_VariableValue, ());
  value->type = NEEDS_CALCULATION;
  return value;
}

/* static */ XSLT_VariableValue *
XSLT_VariableValue::MakeL (const uni_char *string)
{
  XSLT_VariableValue *value = OP_NEW_L (XSLT_VariableValue, ());
  value->type = STRING;
  if (OpStatus::IsError (value->string.Set (string)))
    {
      OP_DELETE (value);
      LEAVE(OpStatus::ERR_NO_MEMORY);
    }
  return value;
}

/* static */ XSLT_VariableValue *
XSLT_VariableValue::MakeL (XSLT_NodeList *nodelist)
{
  XSLT_VariableValue *value = OP_NEW (XSLT_VariableValue, ());
  if (!value)
    {
      OP_DELETE (nodelist);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }
  value->type = NODESET;
  value->data.nodelist = nodelist;
  return value;
}

/* static */ XSLT_VariableValue *
XSLT_VariableValue::MakeL (double number_value)
{
  XSLT_VariableValue *value = OP_NEW_L (XSLT_VariableValue, ());
  value->type = NUMBER;
  value->data.number = number_value;
  return value;
}

/* static */ XSLT_VariableValue *
XSLT_VariableValue::MakeL (BOOL bool_value)
{
  XSLT_VariableValue *value = OP_NEW_L (XSLT_VariableValue, ());
  value->type = BOOLEAN;
  value->data.boolean = !!bool_value;
  return value;
}

/* static */ XSLT_VariableValue *
XSLT_VariableValue::MakeL (XSLT_Tree *tree_value)
{
  XSLT_VariableValue *value = OP_NEW_L (XSLT_VariableValue, ());
  value->type = TREE;
  value->data.tree = tree_value;
  return value;
}

XSLT_VariableValue::~XSLT_VariableValue ()
{
  if (type == TREE)
    OP_DELETE (data.tree);
  else if (type == NODESET)
    OP_DELETE (data.nodelist);
}

OP_STATUS
XSLT_VariableValue::SetXPathValue (XPathValue &xpath_value)
{
  OP_ASSERT (type != NEEDS_CALCULATION && type != IS_BEING_CALCULATED);

  if (type == STRING)
    return xpath_value.SetString (string.CStr ());
  else if (type == NUMBER)
    xpath_value.SetNumber(data.number);
  else if (type == BOOLEAN)
    xpath_value.SetBoolean(data.boolean);
  else
    {
      RETURN_IF_ERROR (xpath_value.SetNodeSet (TRUE, FALSE));

      XPathNode* node;

      if (type == TREE)
        {
          RETURN_IF_ERROR (XPathNode::Make (node, data.tree, data.tree->GetRoot ()));
          XPathValue::AddNodeStatus addnode_status;
          return xpath_value.AddNode (node, addnode_status);
        }
      else
        {
          XPathValue::AddNodeStatus addnode_status = XPathValue::ADDNODE_CONTINUE;
          for (unsigned index = 0; index < data.nodelist->GetCount() && addnode_status != XPathValue::ADDNODE_STOP; ++index)
            {
              RETURN_IF_ERROR (XPathNode::MakeCopy (node, data.nodelist->Get (index)));
              RETURN_IF_ERROR (xpath_value.AddNode (node, addnode_status));
            }
        }
    }

  return OpStatus::OK;
}

#endif // XSLT_SUPPORT
