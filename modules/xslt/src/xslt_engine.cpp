/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_engine.h"

#include "modules/xslt/src/xslt_tree.h"
#include "modules/xslt/src/xslt_program.h"
#include "modules/xslt/src/xslt_nodelist.h"
#include "modules/xslt/src/xslt_instruction.h"
#include "modules/xslt/src/xslt_number.h"
#include "modules/xslt/src/xslt_sort.h"
#include "modules/xslt/src/xslt_variable.h"
#include "modules/xslt/src/xslt_transformation.h"
#include "modules/xslt/src/xslt_recordingoutputhandler.h"
#include "modules/xslt/src/xslt_applytemplates.h"
#include "modules/xslt/src/xslt_copyof.h"
#include "modules/xslt/src/xslt_key.h"

static BOOL
XSLT_IsTextNode (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode)
{
  XMLTreeAccessor::NodeType nodetype = tree->GetNodeType (treenode);
  return nodetype == XMLTreeAccessor::TYPE_TEXT || nodetype == XMLTreeAccessor::TYPE_CDATA_SECTION;
}

static BOOL
XSLT_IsNotANode (XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode)
{
  XMLTreeAccessor::NodeType nodetype = tree->GetNodeType (treenode);
  return nodetype == XMLTreeAccessor::TYPE_DOCTYPE;
}

/* ====== XSLT_VariableStore ===== */

static void XSLT_DeleteVariableValue(const void *key, void *data)
{
  XSLT_VariableValue::DecRef (static_cast<XSLT_VariableValue *> (data));
}

XSLT_VariableStore::~XSLT_VariableStore ()
{
  ForEach (XSLT_DeleteVariableValue);
  OP_DELETE (next);
}

void
XSLT_VariableStore::SetVariableValueL (const XSLT_Variable *variable_elm, XSLT_VariableValue *value)
{
  void *data;

  if (OpStatus::IsSuccess (Remove (variable_elm, &data))) // Can this happen? A variable is set only once, right?
    XSLT_VariableValue::DecRef (static_cast<XSLT_VariableValue *> (data));

  if (OpStatus::IsMemoryError (Add (variable_elm, value)))
    {
      XSLT_VariableValue::DecRef (value);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }
}

XSLT_VariableValue *
XSLT_VariableStore::GetVariableValue (const XSLT_Variable* variable_elm)
{
  void *data;

  if (OpStatus::IsSuccess (GetData (variable_elm, &data)))
    return static_cast<XSLT_VariableValue *> (data);

  OP_ASSERT(!"This means that we are trying to use a variable not yet evaluated. Either we have a cyclic dependancy or we evaluate variables in the wrong order. - see 11.4 in the XSLT spec)");
  return NULL;
}

XSLT_Variable *
XSLT_VariableStore::GetWithParamL (const XMLExpandedName& name)
{
  OpHashIterator *it = GetIterator ();
  if (!it)
    LEAVE(OpStatus::ERR_NO_MEMORY);

  OP_STATUS status = it->First ();
  while (status == OpStatus::OK)
    {
      void *key = const_cast<void *> (it->GetKey ());
      XSLT_Variable *with_param = static_cast<XSLT_Variable *> (key);
      OP_ASSERT (with_param);
      OP_ASSERT (with_param->GetType () == XSLTE_WITH_PARAM);

      if (static_cast<const XMLExpandedName &> (with_param->GetName ()) == name)
        {
          OP_DELETE (it);
          return with_param;
        }

      status = it->Next();
    }

  OP_DELETE (it);
  return NULL;
}

void XSLT_VariableStore::CopyValueFromOtherL (const XSLT_Variable *variable_to_set, XSLT_VariableStore *other_store, const XSLT_Variable *with_param_to_steal)
{
  void *value;

  OP_STATUS status = other_store->GetData (with_param_to_steal, &value);
  OP_ASSERT (OpStatus::IsSuccess (status));
  OpStatus::Ignore (status);

  LEAVE_IF_ERROR (Add (variable_to_set, value));

  XSLT_VariableValue::IncRef (static_cast<XSLT_VariableValue *> (value));
}

/* static */ void
XSLT_VariableStore::PushL (XSLT_VariableStore *&store)
{
  store = OP_NEW_L (XSLT_VariableStore, (store));
}

/* static */ void
XSLT_VariableStore::Pop (XSLT_VariableStore *&store)
{
  XSLT_VariableStore *previous = store;

  store = previous->next;
  previous->next = 0;

  OP_DELETE (previous);
}

XSLT_KeyValue *
XSLT_KeyValue::MakeL (const uni_char *value, XPathNode *node)
{
  XSLT_KeyValue *keyvalue = OP_NEW (XSLT_KeyValue, ());
  XPathNode *copy;

  if (OpStatus::IsMemoryError (keyvalue->value.Set (value)) ||
      OpStatus::IsMemoryError (XPathNode::MakeCopy (copy, node)) ||
      OpStatus::IsMemoryError (keyvalue->nodes.Add (copy)))
    {
      OP_DELETE (keyvalue);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  return keyvalue;
}

XSLT_KeyValue::~XSLT_KeyValue ()
{
  for (unsigned index = 0; index < nodes.GetCount (); ++index)
    XPathNode::Free (nodes.Get (index));
}

void
XSLT_KeyValue::AddNodeL (XPathNode *node)
{
  XPathNode *copy;

  LEAVE_IF_ERROR (XPathNode::MakeCopy (copy, node));

  if (OpStatus::IsMemoryError (nodes.Add (copy)))
    {
      XPathNode::Free (copy);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }
}

void
XSLT_KeyValue::FindNodesL (XPathValue *result)
{
  for (unsigned index = 0; index < nodes.GetCount (); ++index)
    {
      XPathNode *copy;
      LEAVE_IF_ERROR (XPathNode::MakeCopy (copy, nodes.Get (index)));
      XPathValue::AddNodeStatus status;
      LEAVE_IF_ERROR (result->AddNode (copy, status));
      if (status == XPathValue::ADDNODE_STOP)
        break;
    }
}

UINT32
XSLT_KeyValueTableBase::Hash () const
{
  XMLExpandedName::HashFunctions namehash;
  return namehash.Hash (&name) ^ static_cast<UINT32> (reinterpret_cast<UINTPTR> (tree));
}

BOOL
XSLT_KeyValueTableBase::Equals (const XSLT_KeyValueTableBase *other) const
{
  return tree == other->tree && name == other->name;
}

XSLT_KeyValueTable *
XSLT_KeyValueTable::MakeL (const XMLExpandedName &name, XMLTreeAccessor *tree)
{
  XSLT_KeyValueTable *keyvalue = OP_NEW_L (XSLT_KeyValueTable, (tree));
  if (OpStatus::IsMemoryError (keyvalue->name.Set (name)))
    {
      OP_DELETE (keyvalue);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }
  return keyvalue;
}

void
XSLT_KeyValueTable::AddNodeL (const uni_char *value, XPathNode *node)
{
  XSLT_KeyValue *keyvalue;

  if (GetData (value, &keyvalue) == OpStatus::OK)
    keyvalue->AddNodeL (node);
  else
    {
      XSLT_KeyValue *keyvalue = XSLT_KeyValue::MakeL (value, node);
      if (Add (keyvalue->GetValue (), keyvalue) == OpStatus::ERR_NO_MEMORY)
        {
          OP_DELETE (keyvalue);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }
    }
}

void
XSLT_KeyValueTable::FindNodesL (const uni_char *value, XPathValue *result)
{
  XSLT_KeyValue *keyvalue;

  if (GetData (value, &keyvalue) == OpStatus::OK)
    keyvalue->FindNodesL (result);
}

/* virtual */ void
XSLT_KeyTable::Delete (void *value)
{
  XSLT_KeyValueTable *table = static_cast<XSLT_KeyValueTable *> (value);
  OP_DELETE (table);
}

/* virtual */ UINT32
XSLT_KeyTable::Hash (const void *key)
{
  return static_cast<const XSLT_KeyValueTableBase *> (key)->Hash ();
}

/* virtual */ BOOL
XSLT_KeyTable::KeysAreEqual (const void *key1, const void *key2)
{
  return static_cast<const XSLT_KeyValueTableBase *> (key1)->Equals (static_cast<const XSLT_KeyValueTableBase *> (key2));
}

/* virtual */
XSLT_KeyTable::~XSLT_KeyTable ()
{
  DeleteAll ();
}

void
XSLT_KeyTable::AddValueL (const XMLExpandedName &name, const uni_char *value, XPathNode *node)
{
  XSLT_KeyValueTableBase base (name, node->GetTreeAccessor ());
  void *data;

  OpStatus::Ignore (GetData (&base, &data));

  static_cast<XSLT_KeyValueTable *> (data)->AddNodeL (value, node);
}

BOOL
XSLT_KeyTable::FindNodesL (XSLT_Engine *engine, const XMLExpandedName &name, XMLTreeAccessor *tree, const uni_char *string, XPathValue *result)
{
  XSLT_KeyValueTableBase base (name, tree);
  void *data;

  if (GetData (&base, &data) == OpStatus::OK)
    static_cast<XSLT_KeyValueTable *> (data)->FindNodesL (string, result);
  else if (XSLT_Key *key = engine->GetTransformation ()->GetStylesheet ()->FindKey (name))
    {
      XSLT_KeyValueTable *keyvaluetable = XSLT_KeyValueTable::MakeL (name, tree);
      if (Add (static_cast<XSLT_KeyValueTableBase *> (keyvaluetable), keyvaluetable) == OpStatus::ERR_NO_MEMORY)
        {
          OP_DELETE (keyvaluetable);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }
      OP_ASSERT (Contains (&base));
      engine->CalculateKeyValueL (key, tree);
      return FALSE;
    }

  return TRUE;
}

/* ====== XSLT_Engine ===== */

XSLT_Engine::ProgramState::ProgramState (XSLT_Program* program, ProgramState* previous_state)
  : program (program),
    instruction_index (0),
    instruction_states (0),
    current_number (0),
    current_boolean (FALSE),
    current_string (UNI_L ("")),
    evaluate (0),
    match (0),
    count (0),
    search (0),
#ifdef XSLT_ERRORS
    xpath_wrapper (0),
#endif // XSLT_ERRORS
    context_node (0),
    current_node (0),
    context_position (0),
    context_size (0),
    nodelist (0),
    tree (0),
    treenode (0),
    node_index (0),
    sortstate (0),
    owns_variables (FALSE),
    variables (0),
    caller_supplied_params (0),
    params_collected_for_call (0),
    previous_state (previous_state)
{
}

XSLT_Engine::ProgramState::~ProgramState()
{
  XPathExpression::Evaluate::Free (evaluate);
  XPathPattern::Match::Free (match);
  XPathPattern::Count::Free (count);
  if (search)
    search->Stop ();
  XPathNode::Free (context_node);
  XPathNode::Free (current_node);

  OP_DELETE (nodelist);
  OP_DELETE (tree);
  OP_DELETE (sortstate);
  if (owns_variables)
    OP_DELETE (variables);
  if (!previous_state || previous_state->params_collected_for_call != params_collected_for_call)
    OP_DELETE (params_collected_for_call);
}

XSLT_Engine::XSLT_Engine (XSLT_TransformationImpl *transformation)
  : current_state (0),
    recursion_depth (0),
    transformation (transformation),
    output_handler (0),
    instructions_left_in_slice (0),
    is_blocked (FALSE),
    is_program_interrupted (FALSE),
    uncertain_output_method (TRUE),
    need_output_handler_switch (FALSE),
    auto_detected_html (FALSE),
    root_output_handler (0),
    keytable (this),
    element_name_stack (0),
    element_name_stack_used (0),
    element_name_stack_total (0)
{
}

XSLT_Engine::~XSLT_Engine()
{
  while (current_state)
    {
      ProgramState* prev = current_state->previous_state;
      OP_DELETE (current_state);
      current_state = prev;
    }

  if (element_name_stack)
    {
      for (unsigned i = 0; i < element_name_stack_total; i++)
        OP_DELETE (element_name_stack[i]);

      OP_DELETEA (element_name_stack);
    }

  while (output_handler != root_output_handler)
    switch (static_cast<LocalOutputHandler *> (output_handler)->type)
      {
      case LocalOutputHandler::TYPE_COLLECTTEXT:
        output_handler = CollectText::Pop (output_handler);
        break;

      case LocalOutputHandler::TYPE_COLLECTRESULTTREEFRAGMENT:
        output_handler = CollectResultTreeFragment::Pop (output_handler, 0);
        break;
      }
}

void
XSLT_Engine::InitializeL (XSLT_Program *program, XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, XSLT_OutputHandler *new_output_handler, BOOL new_uncertain_output_method)
{
  OP_ASSERT(!current_state);
  OP_ASSERT(tree);
  OP_ASSERT(treenode);

  current_state = OP_NEW_L (ProgramState, (program, 0));
  root_output_handler = output_handler = new_output_handler;
  LEAVE_IF_ERROR (XPathNode::Make (current_state->context_node, tree, treenode));
  current_state->variables = &global_variables;
  OP_ASSERT (!current_state->owns_variables);
  uncertain_output_method = new_uncertain_output_method;

  XSLT_Variable *variable = transformation->GetStylesheet ()->GetLastVariable ();
  while (variable)
    {
      XSLT_VariableValue *value;
      if (variable->GetType () != XSLTE_PARAM || !transformation->CreateVariableValueFromParameterL (value, variable->GetName ()))
        value = XSLT_VariableValue::MakeL ();
      global_variables.SetVariableValueL (variable, value);
      variable = variable->GetPrevious ();
    }
}

/* static */ XSLT_OutputHandler *
XSLT_Engine::CollectText::PushL (XSLT_OutputHandler *previous_output_handler/*, TempBuffer *buffer*/)
{
  CollectText *collect_text = OP_NEW_L (CollectText, ());

  collect_text->previous_output_handler = previous_output_handler;

  return collect_text;
}

/* static */ XSLT_OutputHandler *
XSLT_Engine::CollectText::PopL (XSLT_OutputHandler *output_handler, TempBuffer& buffer_for_result)
{
  CollectText *collect_text = (CollectText *) output_handler;
  output_handler = collect_text->previous_output_handler;
  buffer_for_result.AppendL(collect_text->GetCollectedText());

  OP_DELETE (collect_text);
  return output_handler;
}

/* static */ XSLT_OutputHandler *
XSLT_Engine::CollectText::Pop (XSLT_OutputHandler *output_handler)
{
  CollectText *collect_text = static_cast<CollectText *> (output_handler);
  output_handler = collect_text->previous_output_handler;
  OP_DELETE (collect_text);

  return output_handler;
}

/* virtual */ void
XSLT_Engine::CollectText::StartElementL (const XMLCompleteName &name)
{
  /* FIXME: Signal warning? */
  ++ignore_level;
}

/* virtual */ void
XSLT_Engine::CollectText::AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified)
{
  /* FIXME: Signal warning? */
}

/* virtual */ void
XSLT_Engine::CollectText::AddTextL (const uni_char *data, BOOL disable_output_escaping)
{
  /* Signal warning if disable_output_escaping==TRUE? */
  if (ignore_level == 0)
    buffer.AppendL (data);
}

/* virtual */ void
XSLT_Engine::CollectText::AddCommentL (const uni_char *data)
{
  /* FIXME: Signal warning? */
}

/* virtual */ void
XSLT_Engine::CollectText::AddProcessingInstructionL (const uni_char *target, const uni_char *data)
{
  /* FIXME: Signal warning? */
}

/* virtual */ void
XSLT_Engine::CollectText::EndElementL (const XMLCompleteName &name)
{
  --ignore_level;
}

/* virtual */ void
XSLT_Engine::CollectText::EndOutputL ()
{
}

/* static */ XSLT_OutputHandler *
XSLT_Engine::CollectResultTreeFragment::PushL (XSLT_OutputHandler *previous_output_handler)
{
  CollectResultTreeFragment *collect_rtf = OP_NEW_L (CollectResultTreeFragment, ());
  OpStackAutoPtr<CollectResultTreeFragment> anchor (collect_rtf);

  collect_rtf->previous_output_handler = previous_output_handler;
  collect_rtf->tree = XSLT_Tree::MakeL ();

  anchor.release ();
  return collect_rtf;
}

/* static */ XSLT_OutputHandler *
XSLT_Engine::CollectResultTreeFragment::Pop (XSLT_OutputHandler *output_handler, XSLT_Tree **tree)
{
  CollectResultTreeFragment *collect_rtf = (CollectResultTreeFragment *) output_handler;
  output_handler = collect_rtf->previous_output_handler;

  if (tree)
    *tree = collect_rtf->tree;
  else
    OP_DELETE (collect_rtf->tree);
  OP_DELETE (collect_rtf);

  return output_handler;
}

/* virtual */ void
XSLT_Engine::CollectResultTreeFragment::StartElementL (const XMLCompleteName &name)
{
  tree->StartElementL (name);
}

/* virtual */ void
XSLT_Engine::CollectResultTreeFragment::AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified)
{
  tree->AddAttributeL (name, value, id, specified);
}

/* virtual */ void
XSLT_Engine::CollectResultTreeFragment::AddTextL (const uni_char *data, BOOL disable_output_escaping)
{
  /* Signal warning if disable_output_escaping==TRUE? */
  tree->AddTextL (data, FALSE);
}

/* virtual */ void
XSLT_Engine::CollectResultTreeFragment::AddCommentL (const uni_char *data)
{
  tree->AddCommentL (data);
}

/* virtual */ void
XSLT_Engine::CollectResultTreeFragment::AddProcessingInstructionL (const uni_char *target, const uni_char *data)
{
  tree->AddProcessingInstructionL (target, data);
}

/* virtual */ void
XSLT_Engine::CollectResultTreeFragment::EndElementL (const XMLCompleteName &name)
{
  tree->EndElementL (name);
}

/* virtual */ void
XSLT_Engine::CollectResultTreeFragment::EndOutputL ()
{
  tree->EndOutputL ();
}

UINTPTR &
XSLT_Engine::GetInstructionStateL ()
{
  if (!current_state->instruction_states)
    {
      current_state->instruction_states = OP_NEWA_L (UINTPTR, current_state->program->instructions_count);
      op_memset (current_state->instruction_states, 0, current_state->program->instructions_count * sizeof (UINTPTR));
    }

  return current_state->instruction_states[current_state->instruction_index];
}

void
XSLT_Engine::SetString (const uni_char *string)
{
  current_state->current_string = string;
}

void
XSLT_Engine::AppendStringL (const uni_char *string)
{
  current_state->string_buffer.AppendL (string);
  current_state->current_string = current_state->string_buffer.GetStorage ();
}

const uni_char *
XSLT_Engine::GetString ()
{
  return current_state->current_string;
}

void
XSLT_Engine::ClearString ()
{
  current_state->string_buffer.Clear ();
  current_state->current_string = UNI_L ("");
}

void
XSLT_Engine::SetName (const XMLCompleteName &name)
{
  current_state->current_name = name;
}

BOOL
XSLT_Engine::SetQNameL ()
{
  if (XMLUtils::IsValidQName (XMLVERSION_1_0, current_state->current_string))
    {
      XMLCompleteNameN temporary (current_state->current_string, uni_strlen (current_state->current_string));
      current_state->current_name.SetL (temporary);
      ClearString();
      return TRUE;
    }
  else
    {
      ClearString();
      return FALSE;
    }
}

void
XSLT_Engine::SetNameFromNode ()
{
  OP_ASSERT(current_state->context_node);
  current_state->context_node->GetNodeName(current_state->current_name);
}


void
XSLT_Engine::SetURIL ()
{
  if (current_state->current_string && *current_state->current_string)
    LEAVE_IF_ERROR (current_state->current_name.SetUri (current_state->current_string));
  else
    current_state->current_name.SetUri (0);

  ClearString();
}

void
XSLT_Engine::ResolveNameL (XMLNamespaceDeclaration *nsdeclaration, BOOL use_default, XSLT_Instruction *instruction)
{
  OP_BOOLEAN result = XMLNamespaceDeclaration::ResolveName (nsdeclaration, current_state->current_name, use_default);

  if (result == OpBoolean::IS_TRUE)
    return;
  else if (result == OpBoolean::IS_FALSE)
    /* Would be useful to have the actual value in the error message. */
    SignalErrorL (instruction, "unresolved prefix in QName");
  else
    LEAVE (OpStatus::ERR_NO_MEMORY);
}

const XMLCompleteName &
XSLT_Engine::GetName ()
{
  return current_state->current_name;
}

#define XSLT_INITIAL_ELEMENT_NAME_STACK 8

void
XSLT_Engine::PushElementNameL (const XMLCompleteName &name)
{
  if (element_name_stack_used == element_name_stack_total)
    {
      unsigned new_element_name_stack_total = element_name_stack_total == 0 ? XSLT_INITIAL_ELEMENT_NAME_STACK : element_name_stack_total + element_name_stack_total;
      XMLCompleteName **new_element_name_stack = OP_NEWA_L (XMLCompleteName*, new_element_name_stack_total);

      op_memcpy (new_element_name_stack, element_name_stack, element_name_stack_used * sizeof element_name_stack[0]);
      // Set the unused to 0 so that we don't think they contain
      // existing names we can reuse.
      op_memset (new_element_name_stack + element_name_stack_used, 0, (new_element_name_stack_total - element_name_stack_used) * sizeof element_name_stack[0]);

      OP_DELETEA (element_name_stack);
      element_name_stack = new_element_name_stack;
      element_name_stack_total = new_element_name_stack_total;
    }

  XMLCompleteName **ptr = &element_name_stack[element_name_stack_used];

  if (!*ptr)
    *ptr = OP_NEW_L (XMLCompleteName, ());

  if (OpStatus::IsMemoryError ((*ptr)->Set (name)))
    LEAVE (OpStatus::ERR_NO_MEMORY);

  ++element_name_stack_used;
}

void
XSLT_Engine::PopElementName ()
{
  OP_ASSERT(element_name_stack_used > 0);
  --element_name_stack_used;
}

const XMLCompleteName &
XSLT_Engine::GetCurrentElementName ()
{
  OP_ASSERT(element_name_stack[element_name_stack_used - 1]);
  return *element_name_stack[element_name_stack_used - 1];
}

void
XSLT_Engine::StartElementL (const XMLCompleteName &name, BOOL push)
{
  if (push)
    {
      PushElementNameL (name);
      output_handler->StartElementL (GetCurrentElementName ());
    }
  else
    output_handler->StartElementL (name);
}

void
XSLT_Engine::SuggestNamespaceDeclarationL (XSLT_Element *element)
{
  output_handler->SuggestNamespaceDeclarationL (element, element->GetNamespaceDeclaration (), TRUE);
}

void
XSLT_Engine::AddAttributeL (const XMLCompleteName &name)
{
  if (name.GetPrefix () && uni_strcmp (name.GetPrefix (), "xmlns") == 0)
    output_handler->AddAttributeL (XMLCompleteName (name.GetUri (), 0, name.GetLocalPart ()), GetString (), FALSE, TRUE);
  else
    output_handler->AddAttributeL (name, GetString (), FALSE, TRUE);

  ClearString ();
}

void
XSLT_Engine::AddTextL (XSLT_Instruction *instruction)
{
  if (GetString ())
    {
      output_handler->AddTextL (GetString (), !!instruction->argument);

      if (uncertain_output_method &&
          output_handler == root_output_handler &&
          !XMLUtils::IsWhitespace(GetString()))
        {
          uncertain_output_method = FALSE;
          need_output_handler_switch = TRUE;
          instructions_left_in_slice = 0;
        }

      ClearString();
    }
}

void
XSLT_Engine::AddCommentL ()
{
  /* FIXME: Make sure 'current_string' doesn't contain '--'. */
  output_handler->AddCommentL (GetString ());
  ClearString();
}

void
XSLT_Engine::AddProcessingInstructionL ()
{
  /* FIXME: Make sure 'current_string' doesn't contain '?>'. */
  output_handler->AddProcessingInstructionL (GetName ().GetLocalPart (), GetString ());
  ClearString();
}

void
XSLT_Engine::EndElementL (const XMLCompleteName *name)
{
  if (name)
    output_handler->EndElementL (*name);
  else
    {
      output_handler->EndElementL (GetCurrentElementName ());
      PopElementName ();
    }
}

BOOL // False = Not finished yet, TRUE = finished, next instruction please
XSLT_Engine::SortL (XSLT_Sort *sort)
{
  return sort->SortL (this, current_state->nodelist->GetCount (), current_state->nodelist->GetFlatArray (), current_state->sortstate, instructions_left_in_slice, this);
}

void
XSLT_Engine::SetSortParameterFromStringL (XSLT_Sort *sort, XSLT_Instruction::Code instr)
{
  sort->SetNextParameterL (GetString (), current_state->sortstate, instr, this);
  ClearString ();
}

BOOL // False = Not finished yet, TRUE = finished, next instruction please
XSLT_Engine::AddFormattedNumberL (XSLT_Number *number)
{
  OP_ASSERT(number);

  TempBuffer buffer; ANCHOR (TempBuffer, buffer);

  number->ConvertNumberToStringL (GetString (), current_state->current_number, buffer);

  if (buffer.GetStorage ())
    output_handler->AddTextL (buffer.GetStorage (), FALSE);

  ClearString();

  return TRUE;
}

void
XSLT_Engine::SendMessageL (XSLT_Instruction *instruction)
{
#ifdef XSLT_ERRORS
  instruction->origin->SignalMessageL (transformation, current_state->current_string, !!instruction->argument);
#else // XSLT_ERRORS
  if (instruction->argument)
    LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
  ClearString();
}

BOOL
XSLT_Engine::CalculateContextSizeL ()
{
  ProgramState *state = current_state->previous_state;
  unsigned context_size = current_state->context_position;

  if (state && state->treenode)
    {
      XMLTreeAccessor *tree = state->context_node->GetTreeAccessor ();
      XMLTreeAccessor::Node *treenode = state->treenode;

      while (treenode)
        {
          BOOL was_text = XSLT_IsTextNode (tree, treenode);

          treenode = tree->GetNextSibling (treenode);

          if (!treenode || was_text && XSLT_IsTextNode (tree, treenode) || XSLT_IsNotANode (tree, treenode))
            continue;

          ++context_size;
        }
    }
  else
    {
      if (state && state->program->type == XSLT_Program::TYPE_APPLY_TEMPLATES)
        state = state->previous_state;

      if (state && (state->program->type == XSLT_Program::TYPE_TEMPLATE || state->program->type == XSLT_Program::TYPE_FOR_EACH))
        if (state->evaluate)
          {
            if (!state->nodelist)
              state->nodelist = OP_NEW_L (XSLT_NodeList, ());

            XPathNode *node;

            while (TRUE)
              {
                OP_BOOLEAN is_finished = state->evaluate->GetNextNode (node);

                HandleXPathResultL (is_finished, state);

                if (is_finished == OpBoolean::IS_FALSE)
                  return FALSE;

                if (!node)
                  {
                    XPathExpression::Evaluate::Free (state->evaluate);
                    state->evaluate = 0;
#ifdef XSLT_ERRORS
                    state->xpath_wrapper = 0;
#endif // XSLT_ERRORS

                    break;
                  }

                state->nodelist->AddL (node);
              }

            context_size += state->nodelist->GetCount ();

            state->nodelist->SetOffset (state->node_index);
          }
    }

  current_state->context_size = context_size;
  return TRUE;
}

BOOL // TRUE == Next instruction, FALSE == come back later
XSLT_Engine::EvaluateExpressionL (int type, unsigned expression_index)
{
  ProgramState *state = current_state;
  XPathExpression::Evaluate *evaluate = state->evaluate;

  if (!evaluate)
    {
      XSLT_XPathExpression *wrapper = current_state->program->expressions[expression_index];
      XPathExpression *expression = wrapper->GetExpressionL (transformation);

      if (current_state->context_size == 0)
        {
          unsigned expression_flags = expression->GetExpressionFlags ();

          if ((expression_flags & XPathExpression::FLAG_NEEDS_CONTEXT_SIZE) != 0)
            if (!CalculateContextSizeL ())
              return FALSE;
        }

      LEAVE_IF_ERROR (XPathExpression::Evaluate::Make (current_state->evaluate, expression));

#ifdef XSLT_ERRORS
      current_state->xpath_wrapper = wrapper;
#endif // XSLT_ERRORS

      evaluate = current_state->evaluate;

      XPathNode *copy;
      LEAVE_IF_ERROR (XPathNode::MakeCopy (copy, current_state->context_node));

      evaluate->SetContext (copy, current_state->context_position, current_state->context_size);

      if (type == XPathExpression::Evaluate::TYPE_INVALID)
        /* Used via IC_EVALUATE_TO_VARIABLE_VALUE.  We don't want any
           conversion, and prefer snapshots for nodesets (we want all nodes
           anyway.) */
        evaluate->SetRequestedType (XPathExpression::Evaluate::PRIMITIVE_NUMBER,
                                    XPathExpression::Evaluate::PRIMITIVE_BOOLEAN,
                                    XPathExpression::Evaluate::PRIMITIVE_STRING,
                                    XPathExpression::Evaluate::NODESET_SNAPSHOT | XPathExpression::Evaluate::NODESET_FLAG_ORDERED);
      else if (type > XPathExpression::Evaluate::TYPE_INVALID)
        evaluate->SetRequestedType (type);

      evaluate->SetExtensionsContext (this);
    }

  evaluate->SetCostLimit (instructions_left_in_slice);

  OP_BOOLEAN result;

  if (type == XPathExpression::Evaluate::PRIMITIVE_NUMBER)
    result = evaluate->GetNumberResult (current_state->current_number);
  else if (type == XPathExpression::Evaluate::PRIMITIVE_BOOLEAN)
    result = evaluate->GetBooleanResult (current_state->current_boolean);
  else if (type == XPathExpression::Evaluate::PRIMITIVE_STRING)
    {
      const uni_char *string;

      result = evaluate->GetStringResult (string);

      if (result == OpBoolean::IS_TRUE)
        AppendStringL (string);
    }
  else if ((type & XPathExpression::Evaluate::NODESET_ITERATOR) != 0 ||
           type == XPathExpression::Evaluate::TYPE_INVALID ||
           type < 0)
    {
      // Nothing is really done here. It's done lazily. For instance CALL_PROGRAM_ON_EVALUATE will iterate over this
      current_state->node_index = 0;
      return TRUE;
    }
  else
    {
      OP_ASSERT((type & XPathExpression::Evaluate::NODESET_SNAPSHOT) != 0);
      unsigned count;

      result = evaluate->GetNodesCount (count);
      instructions_left_in_slice -= evaluate->GetLastOperationCost();

      if (result == OpBoolean::IS_TRUE)
        {
          OP_ASSERT (!current_state->nodelist);

          XSLT_NodeList *nodelist = current_state->nodelist = OP_NEW_L (XSLT_NodeList, ());

          // FIXME: Make the iteration interruptible
          for (unsigned index = 0; index < count; ++index)
            {
              XPathNode *node;
              LEAVE_IF_ERROR (evaluate->GetNode (node, index));
              instructions_left_in_slice -= evaluate->GetLastOperationCost();
              nodelist->AddL (node);
            }

          instructions_left_in_slice -= count;

          current_state->node_index = 0;
        }
      // To compensate for the subtraction after the branched code
      instructions_left_in_slice += evaluate->GetLastOperationCost();
    }

  instructions_left_in_slice -= evaluate->GetLastOperationCost();

  if (result == OpBoolean::IS_TRUE)
    {
      XPathExpression::Evaluate::Free (evaluate);
      current_state->evaluate = 0;
#ifdef XSLT_ERRORS
      current_state->xpath_wrapper = 0;
#endif // XSLT_ERRORS

      return TRUE;
    }
  else
    HandleXPathResultL (result, state);

  return FALSE;
}

BOOL
XSLT_Engine::MatchPatternL (unsigned pattern_index, unsigned patterns_count)
{
  OP_ASSERT(patterns_count > 0);
  OP_ASSERT(pattern_index < current_state->program->patterns_count);
  OP_ASSERT(pattern_index + patterns_count <= current_state->program->patterns_count);

  ProgramState *state = current_state;

  if (!state->match)
    {
      XPathNode *copy;

      LEAVE_IF_ERROR (XPathNode::MakeCopy (copy, state->context_node));
      LEAVE_IF_ERROR(XPathPattern::Match::Make (state->match, transformation->GetPatternContext (), copy, state->program->patterns + pattern_index, patterns_count));

      state->match->SetExtensionsContext (static_cast<XPathExtensions::Context*> (this));
    }

  XPathPattern::Match *match = state->match;

  XPathPattern *matched_pattern;

  OP_BOOLEAN result = match->GetMatchedPattern (matched_pattern);
  instructions_left_in_slice -= match->GetLastOperationCost ();

  if (result == OpBoolean::IS_TRUE)
    {
      XPathPattern::Match::Free (match);
      state->match = 0;

#ifdef XSLT_STATISTICS
      if (matched_pattern)
        {
          unsigned index;

          for (index = 0; index < patterns_count; ++index)
            if (state->program->patterns[pattern_index + index] == matched_pattern)
              break;

          transformation->patterns_tried_and_not_matched += index;
          transformation->patterns_tried_and_matched += 1;
        }
      else
        transformation->patterns_tried_and_not_matched += patterns_count;
#endif // XSLT_STATISTICS

      state->current_boolean = matched_pattern != 0;
      return TRUE;
    }
  else
    HandleXPathResultL (result, state);

  return FALSE;
}

void
XSLT_Engine::SearchPatternL (UINTPTR argument)
{
  XSLT_Key *key = reinterpret_cast<XSLT_Key *> (argument);

  XPathPattern::Search *search = current_state->search = key->GetSearchL (transformation->GetPatternContext ());
  XPathNode *copy;

  LEAVE_IF_ERROR (XPathNode::MakeCopy (copy, current_state->context_node));
  LEAVE_IF_ERROR (search->Start (copy));

  search->SetExtensionsContext (static_cast<XPathExtensions::Context *> (this));

  current_state->node_index = 0;
}

BOOL
XSLT_Engine::CountPatternsAndAddL (const XSLT_Number *number_elm)
{
  ProgramState *state = current_state;

  if (!state->count)
    {
      unsigned count_pattern_index;
      unsigned count_pattern_count;
      unsigned from_pattern_index;
      unsigned from_pattern_count;

      number_elm->GetCountPatternIndices (count_pattern_index, count_pattern_count);
      number_elm->GetFromPatternIndices (from_pattern_index, from_pattern_count);

      XPathPattern **count_pattern = state->program->patterns + count_pattern_index;
      XPathPattern **from_pattern = state->program->patterns + from_pattern_index;
      XPathNode *copy;

      LEAVE_IF_ERROR (XPathNode::MakeCopy (copy, state->context_node));
      LEAVE_IF_ERROR (XPathPattern::Count::Make (state->count, transformation->GetPatternContext (), number_elm->GetLevel (), copy, count_pattern, count_pattern_count, from_pattern, from_pattern_count));

      state->count->SetExtensionsContext (static_cast<XPathExtensions::Context *> (this));
    }

  XPathPattern::Count *count = state->count;

  count->SetCostLimit (instructions_left_in_slice);

  unsigned number_count = 1;
  unsigned *number_levelled = 0, number_any;

  OP_BOOLEAN result;

  if (number_elm->GetLevel () == XPathPattern::Count::LEVEL_ANY)
    result = count->GetResultAny (number_any);
  else
    result = count->GetResultLevelled (number_count, number_levelled);

  HandleXPathResultL (result, state);

  instructions_left_in_slice -= count->GetLastOperationCost ();

  if (result == OpBoolean::IS_FALSE)
    return FALSE;

  ANCHOR_ARRAY (unsigned, number_levelled);

  XPathPattern::Count::Free (count);
  state->count = 0;

  TempBuffer buffer; ANCHOR (TempBuffer, buffer);

  LEAVE_IF_ERROR (number_elm->ConvertNumbersToString (GetString (), number_levelled ? number_levelled : &number_any, number_count, buffer));

  if (buffer.Length ())
    output_handler->AddTextL (buffer.GetStorage (), FALSE);

  ClearString();
  return TRUE;
}

void
XSLT_Engine::CallProgramL (XPathNode *context_node, BOOL take_context_node, XSLT_Program *program, BOOL open_new_variable_scope)
{
  ProgramState *next_state = OP_NEW (ProgramState, (program, current_state));

  if (!next_state)
    {
      if (take_context_node)
        XPathNode::Free (context_node);

      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  if (take_context_node)
    next_state->context_node = context_node;
  else if (OpStatus::IsError (XPathNode::MakeCopy (next_state->context_node, context_node)))
    {
      OP_DELETE (next_state);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  if (open_new_variable_scope)
    {
      next_state->variables = OP_NEW (XSLT_VariableStore, ());

      if (!next_state->variables)
        {
          OP_DELETE (next_state);
          LEAVE(OpStatus::ERR_NO_MEMORY);
        }

      next_state->owns_variables = TRUE;
    }
  else
    next_state->variables = current_state->variables;

  if (program->type == XSLT_Program::TYPE_TEMPLATE)
    {
      next_state->caller_supplied_params = current_state->params_collected_for_call;

      if (current_state->program->type == XSLT_Program::TYPE_APPLY_TEMPLATES)
        {
          next_state->context_position = current_state->context_position;
          next_state->context_size = current_state->context_size;
        }
    }
  else if (program->type == XSLT_Program::TYPE_APPLY_TEMPLATES)
    next_state->params_collected_for_call = current_state->params_collected_for_call;

  ProgramState *previous_state = current_state;

  current_state = next_state;

  if (++recursion_depth == XSLT_MAX_RECURSION_DEPTH)
    SignalErrorL (previous_state->program->instructions + previous_state->instruction_index, "max recursion depth exceeded");
}

void
XSLT_Engine::CallProgramL (XPathNode *context_node, BOOL take_context_node, unsigned program_index, BOOL open_new_variable_scope)
{
  CallProgramL (context_node, take_context_node, current_state->program->programs[program_index], open_new_variable_scope);
}

BOOL
XSLT_Engine::CallProgramOnNodeL (unsigned program_index, BOOL open_new_variable_scope, BOOL copy_context)
{
  OP_ASSERT(current_state->node_index == 0 || current_state->node_index == 1);

  if (current_state->node_index == 0)
    {
      ProgramState *previous_state = current_state;

      previous_state->node_index = 1;
      CallProgramL (previous_state->context_node, FALSE, program_index, open_new_variable_scope);

      if (copy_context)
        {
          current_state->context_position = previous_state->context_position;
          current_state->context_size = previous_state->context_size;
        }

      return TRUE;
    }
  else
    {
      current_state->node_index = 0;
      return FALSE;
    }
}

BOOL
XSLT_Engine::GetNextNodeL (XPathNode *&node, BOOL &owns_node, unsigned &position, unsigned &size)
{
  if (XSLT_NodeList *nodelist = current_state->nodelist)
    if (current_state->node_index < nodelist->GetCount ())
      {
        node = nodelist->Steal (current_state->node_index);
        owns_node = TRUE;
        position = ++current_state->node_index;
        size = nodelist->GetCount ();
      }
    else
      {
        OP_DELETE (nodelist);
        current_state->nodelist = 0;
        current_state->node_index = 0;
        node = 0;
      }
  else if (XPathExpression::Evaluate *evaluate = current_state->evaluate)
    {
      ProgramState *state = current_state;

      evaluate->SetCostLimit (instructions_left_in_slice);

      OP_BOOLEAN result;

      HandleXPathResultL (result = evaluate->GetNextNode (node), state);

      instructions_left_in_slice -= evaluate->GetLastOperationCost ();

      if (result == OpBoolean::IS_FALSE)
        return FALSE;

      if (node)
        {
          owns_node = FALSE;
          position = ++current_state->node_index;
          size = 0;
        }
      else
        {
          XPathExpression::Evaluate::Free (evaluate);
          current_state->evaluate = 0;
          current_state->node_index = 0;
#ifdef XSLT_ERRORS
          current_state->xpath_wrapper = 0;
#endif // XSLT_ERRORS
        }
    }
  else if (XPathPattern::Search *search = current_state->search)
    {
      ProgramState *state = current_state;

      search->SetCostLimit (instructions_left_in_slice);

      OP_BOOLEAN result;

      HandleXPathResultL (result = search->GetNextNode (node), state);

      instructions_left_in_slice -= search->GetLastOperationCost ();

      if (result == OpBoolean::IS_FALSE)
        return FALSE;

      if (node)
        {
          owns_node = FALSE;
          position = 1;
          size = 1;
        }
      else
        {
          search->Stop ();

          current_state->search = 0;
        }
    }
  else
    node = 0;

  return TRUE;
}

int // 0 = block, 1 = call program, 2 = next instruction
XSLT_Engine::CallProgramOnNextNodeL (unsigned program_index, BOOL open_new_variable_scope)
{
  XPathNode *node;
  BOOL owns_node;
  unsigned position, size;

  if (!GetNextNodeL (node, owns_node, position, size))
    return 0;

  if (node)
    {
      CallProgramL (node, owns_node, program_index, open_new_variable_scope);

      current_state->context_position = position;
      current_state->context_size = size;

      return 1;
    }
  else
    return 2;
}

int
XSLT_Engine::ApplyTemplatesL (UINTPTR &argument)
{
  XSLT_StylesheetImpl *stylesheet = transformation->GetStylesheet ();
  XSLT_Program *program;
  const XMLExpandedName *mode;

  if ((argument & UINTPTR (1)) == 0)
    {
      XSLT_ApplyTemplates *apply_templates = reinterpret_cast<XSLT_ApplyTemplates *> (argument);
      mode = apply_templates->GetMode ();

#ifdef XSLT_USE_NODE_PROFILES
      XPathExpression *expression = current_state->program->expressions[apply_templates->GetSelectIndex ()]->GetExpressionL (transformation);
      XPathNodeProfile *nodeprofile;

      LEAVE_IF_ERROR (expression->GetNodeProfile (nodeprofile));

      argument = reinterpret_cast<UINTPTR> (program = stylesheet->GetApplyTemplatesProgramL (mode, 0, nodeprofile, TRUE)) | UINTPTR (1);
#else // XSLT_USE_NODE_PROFILES
      argument = reinterpret_cast<UINTPTR> (program = stylesheet->GetApplyTemplatesProgramL (mode, 0, 0, TRUE)) | UINTPTR (1);
#endif // XSLT_USE_NODE_PROFILES
    }
  else
    {
      program = reinterpret_cast<XSLT_Program *> (argument & ~UINTPTR (1));
      mode = program->mode;
    }

  XPathNode *node;
  BOOL owns_node;
  unsigned context_position, context_size;

  if (!GetNextNodeL (node, owns_node, context_position, context_size))
    return 0;

  if (node)
    {
#ifdef XSLT_USE_NODE_PROFILES
      XPathNode::Type nodetype = node->GetType ();

      if (program->type == XSLT_Program::TYPE_APPLY_TEMPLATES && (nodetype == XPathNode::TEXT_NODE || nodetype == XPathNode::COMMENT_NODE))
        /* These can be expected to show up reasonably often in cases where the
           point is really to apply templates to elements.  Especially
           whitespace-only text nodes can be really common in such cases.  By
           using a specialized program that only tests patterns that match such
           nodes we can reduce the number of pattern matches we do. */
        program = stylesheet->GetApplyTemplatesProgramL (mode, nodetype, TRUE);
#endif // XSLT_USE_NODE_PROFILES

      CallProgramL (node, owns_node, program, TRUE);

      current_state->context_position = context_position;
      current_state->context_size = context_size;

      return 1;
    }
  else
    return 2;
}

BOOL
XSLT_Engine::ApplyBuiltInTemplateL (UINTPTR &argument)
{
  XPathNode *node = current_state->context_node;
  XPathNode::Type nodetype = node->GetType ();

  if (nodetype == XPathNode::TEXT_NODE || nodetype == XPathNode::ATTRIBUTE_NODE)
    {
      TempBuffer buffer; ANCHOR (TempBuffer, buffer);

      LEAVE_IF_ERROR (node->GetStringValue (buffer));

      if (buffer.GetStorage ())
        output_handler->AddTextL (buffer.GetStorage (), FALSE);

      return FALSE;
    }
  else if (nodetype == XPathNode::ROOT_NODE || nodetype == XPathNode::ELEMENT_NODE)
    {
      ProgramState *state = current_state;

      XMLTreeAccessor *tree = node->GetTreeAccessor ();
      XMLTreeAccessor::Node *treenode = state->treenode;
      BOOL was_text;

      if (!treenode)
        if ((treenode = tree->GetFirstChild (node->GetNode ())) == 0)
          return FALSE;
        else
          {
            was_text = FALSE;
            current_state->node_index = 0;
          }
      else
        {
          was_text = XSLT_IsTextNode (tree, treenode);
          treenode = tree->GetNextSibling (treenode);
        }

      while (treenode && (was_text && XSLT_IsTextNode (tree, treenode) || XSLT_IsNotANode (tree, treenode)))
        treenode = tree->GetNextSibling (treenode);

      state->treenode = treenode;

      if (treenode)
        {
          XPathNode *node;

          LEAVE_IF_ERROR (XPathNode::Make (node, tree, treenode));

          XSLT_Program *program;

#ifdef XSLT_USE_NODE_PROFILES
          XPathNode::Type nodetype = node->GetType ();

          if (nodetype == XPathNode::TEXT_NODE || nodetype == XPathNode::COMMENT_NODE)
            /* These can be expected to show up reasonably often in cases where
               the point is really to apply templates to elements.  Especially
               whitespace-only text nodes can be really common in such cases.
               By using a specialized program that only tests patterns that
               match such nodes we can reduce the number of pattern matches we
               do. */
            program = transformation->GetStylesheet ()->GetApplyTemplatesProgramL (state->program->mode, nodetype, TRUE);
          else
#endif // XSLT_USE_NODE_PROFILES
            {
              if (argument == 0)
                argument = reinterpret_cast<UINTPTR> (program = transformation->GetStylesheet ()->GetApplyTemplatesProgramL (state->program->mode, 0, 0, TRUE));
              else
                program = reinterpret_cast<XSLT_Program *> (argument);
            }

          CallProgramL (node, TRUE, program, TRUE);

          current_state->context_position = ++state->node_index;
          current_state->context_size = 0;

          return TRUE;
        }

      state->node_index = 0;
    }

  return FALSE;
}

BOOL // FALSE = end of the whole program, TRUE = ended frame
XSLT_Engine::Return ()
{
  ProgramState *parent_state = current_state->previous_state;
  OP_DELETE (current_state);
  current_state = parent_state;
  --recursion_depth;
  return current_state != 0;
}

BOOL
XSLT_Engine::ProcessKeyL (XSLT_Key *key)
{
  ProgramState *state = current_state;
  XPathPattern::Search *search = state->search;
  OP_ASSERT(search);

  while (TRUE)
    {
      XPathExpression::Evaluate *evaluate = state->evaluate;

      if (!evaluate)
        {
          /* Get first/next node from search results and evaluate the
             'use' expression with it as the context node. */

          XPathNode *node;
          OP_BOOLEAN status;

          HandleXPathResultL (status = state->search->GetNextNode (node), state);

          if (status == OpBoolean::IS_FALSE)
            return FALSE;

          if (!node)
            {
              search->Stop ();

              state->search = 0;
              break;
            }

          XSLT_XPathExpression *wrapper = state->program->expressions[key->GetUseExpressionIndex ()];
          XPathExpression *expression = wrapper->GetExpressionL (transformation);

          LEAVE_IF_ERROR (XPathExpression::Evaluate::Make (state->evaluate, expression));

#ifdef XSLT_ERRORS
          state->xpath_wrapper = wrapper;
#endif // XSLT_ERRORS

          evaluate = state->evaluate;

          XPathNode *copy;
          LEAVE_IF_ERROR (XPathNode::MakeCopy (copy, node));
          LEAVE_IF_ERROR (XPathNode::MakeCopy (state->current_node, node));

          evaluate->SetContext (copy, 1, 1);
          evaluate->SetRequestedType (XPathExpression::Evaluate::PRIMITIVE_STRING,
                                      XPathExpression::Evaluate::PRIMITIVE_STRING,
                                      XPathExpression::Evaluate::PRIMITIVE_STRING,
                                      XPathExpression::Evaluate::NODESET_ITERATOR);
          evaluate->SetExtensionsContext (this);
        }

      evaluate->SetCostLimit (instructions_left_in_slice);

      OP_BOOLEAN status;
      unsigned type;

      HandleXPathResultL (status = evaluate->CheckResultType (type), state);

      instructions_left_in_slice -= evaluate->GetLastOperationCost ();

      if (instructions_left_in_slice < 0 || status == OpBoolean::IS_FALSE)
        return FALSE;

      evaluate->SetCostLimit (instructions_left_in_slice);

      if ((type & XPathExpression::Evaluate::NODESET_ITERATOR) != 0)
        {
          TempBuffer buffer; ANCHOR (TempBuffer, buffer);

          buffer.ExpandL (1);

          while (TRUE)
            {
              XPathNode *usenode;

              HandleXPathResultL (status = evaluate->GetNextNode (usenode), state);

              instructions_left_in_slice -= evaluate->GetLastOperationCost ();

              if (status == OpBoolean::IS_FALSE)
                return FALSE;

              if (!usenode)
                break;

              LEAVE_IF_ERROR (usenode->GetStringValue (buffer));
              keytable.AddValueL (key->GetName (), buffer.GetStorage (), state->current_node);
              buffer.Clear ();

              if (--instructions_left_in_slice <= 0)
                return FALSE;
            }
        }
      else
        {
          const uni_char *result;

          HandleXPathResultL (status = evaluate->GetStringResult (result), state);

          instructions_left_in_slice -= evaluate->GetLastOperationCost ();

          if (status == OpBoolean::IS_FALSE)
            return FALSE;

          keytable.AddValueL (key->GetName (), result, state->current_node);
        }

      XPathExpression::Evaluate::Free (evaluate);
      state->evaluate = 0;
#ifdef XSLT_ERRORS
      state->xpath_wrapper = 0;
#endif // XSLT_ERRORS

      XPathNode::Free (state->current_node);
      state->current_node = 0;
    }

  return TRUE;
}

void XSLT_Engine::TestAndSetIfParamIsPresetL(const XSLT_Variable *variable)
{
  if (variable->GetParent ()->GetType () == XSLTE_STYLESHEET)
    {
      XSLT_VariableValue *value;

      if (transformation->CreateVariableValueFromParameterL (value, variable->GetName ()))
        {
          current_state->variables->SetVariableValueL (variable, value);
          current_state->current_boolean = TRUE;
          return;
        }
    }
  else if (current_state->caller_supplied_params)
    {
      XSLT_Variable *with_param = current_state->caller_supplied_params->GetWithParamL (variable->GetName ());
      if (with_param)
        {
          current_state->variables->CopyValueFromOtherL (variable, current_state->caller_supplied_params, with_param);
          current_state->current_boolean = TRUE;
          return;
        }
    }
  current_state->current_boolean = FALSE;
}

BOOL
XSLT_Engine::CreateVariableValueFromEvaluateL (XSLT_VariableValue *&value)
{
  ProgramState *state = current_state;
  XPathExpression::Evaluate *evaluate = state->evaluate;

  OP_BOOLEAN status;
  unsigned type;

  HandleXPathResultL (status = evaluate->CheckResultType (type), state);

  if (status == OpBoolean::IS_FALSE)
    return FALSE;

  instructions_left_in_slice -= evaluate->GetLastOperationCost ();
  if (instructions_left_in_slice < 0)
    return FALSE;

  if (type == XPathExpression::Evaluate::PRIMITIVE_STRING ||
      type == XPathExpression::Evaluate::PRIMITIVE_BOOLEAN ||
      type == XPathExpression::Evaluate::PRIMITIVE_NUMBER)
    {
      // FIXME: Clean up code to avoid code duplication
      if (type == XPathExpression::Evaluate::PRIMITIVE_STRING)
        {
          const uni_char *result;

          HandleXPathResultL (status = evaluate->GetStringResult (result), state);

          instructions_left_in_slice -= evaluate->GetLastOperationCost ();
          if (status == OpBoolean::IS_FALSE)
            return FALSE;
          value = XSLT_VariableValue::MakeL (result);
        }
      else if (type == XPathExpression::Evaluate::PRIMITIVE_BOOLEAN)
        {
          BOOL result;

          HandleXPathResultL (status = evaluate->GetBooleanResult (result), state);

          instructions_left_in_slice -= evaluate->GetLastOperationCost();
          if (status == OpBoolean::IS_FALSE)
            return FALSE;
          value = XSLT_VariableValue::MakeL (result);
        }
      else
        {
          double result;

          HandleXPathResultL (status = evaluate->GetNumberResult (result), state);

          instructions_left_in_slice -= evaluate->GetLastOperationCost ();
          if (status == OpBoolean::IS_FALSE)
            return FALSE;
          value = XSLT_VariableValue::MakeL (result);
        }
    }
  else
    {
      OP_ASSERT ((type & XPathExpression::Evaluate::NODESET_SNAPSHOT) != 0);

      unsigned nodecount;

      HandleXPathResultL (status = evaluate->GetNodesCount (nodecount), state);

      instructions_left_in_slice -= evaluate->GetLastOperationCost ();
      if (status == OpBoolean::IS_FALSE)
        return FALSE;

      XSLT_NodeList *nodelist = OP_NEW_L (XSLT_NodeList, ());
      OpStackAutoPtr<XSLT_NodeList> anchor (nodelist);

      for (unsigned index = 0; index < nodecount; ++index)
        {
          XPathNode* node;
          LEAVE_IF_ERROR (evaluate->GetNode (node, index));
          nodelist->AddL (node);
        }

      instructions_left_in_slice -= nodecount;

      value = XSLT_VariableValue::MakeL (anchor.release());
    }

  return TRUE;
}

BOOL
XSLT_Engine::SetVariableL (XSLT_Instruction::Code instruction, const XSLT_Variable* variable)
{
  XSLT_VariableValue* value;
  if (instruction == XSLT_Instruction::IC_SET_VARIABLE_FROM_EVALUATE ||
      instruction == XSLT_Instruction::IC_SET_WITH_PARAM_FROM_EVALUATE)
    {
      OP_ASSERT (current_state->evaluate);
      if (!CreateVariableValueFromEvaluateL (value))
        return FALSE;
    }
  else if (instruction == XSLT_Instruction::IC_SET_VARIABLE_FROM_COLLECTED ||
           instruction == XSLT_Instruction::IC_SET_WITH_PARAM_FROM_COLLECTED)
    {
      OP_ASSERT (current_state->tree);
      value = XSLT_VariableValue::MakeL (current_state->tree);
      current_state->tree = 0; // Now owned by value
    }
  else
    value = XSLT_VariableValue::MakeL (current_state->current_string);

  OpStackAutoPtr<XSLT_VariableValue> anchor (value);

  XSLT_VariableStore* variable_store;

  if (instruction == XSLT_Instruction::IC_SET_WITH_PARAM_FROM_EVALUATE ||
      instruction == XSLT_Instruction::IC_SET_WITH_PARAM_FROM_COLLECTED ||
      instruction == XSLT_Instruction::IC_SET_WITH_PARAM_FROM_STRING)
    {
      /* An IC_START_COLLECT_PARAMS should have created it already. */
      OP_ASSERT (current_state->params_collected_for_call);

      variable_store = current_state->params_collected_for_call;
    }
  else
    variable_store = current_state->variables;

  variable_store->SetVariableValueL (variable, value);
  anchor.release ();

  if (instruction == XSLT_Instruction::IC_SET_VARIABLE_FROM_EVALUATE ||
      instruction == XSLT_Instruction::IC_SET_WITH_PARAM_FROM_EVALUATE)
    {
      OP_ASSERT (current_state->evaluate);
      XPathExpression::Evaluate::Free (current_state->evaluate);
      current_state->evaluate = 0;
#ifdef XSLT_ERRORS
      current_state->xpath_wrapper = 0;
#endif // XSLT_ERRORS
    }

  return TRUE; // We're finished setting the variable
}


void
XSLT_Engine::StartCollectTextL ()
{
  output_handler = CollectText::PushL (output_handler);
}

void
XSLT_Engine::EndCollectTextL ()
{
  output_handler->EndOutputL ();
  output_handler = CollectText::PopL (output_handler, current_state->string_buffer);
  current_state->current_string = current_state->string_buffer.GetStorage ();
  if (!current_state->current_string)
    current_state->current_string = UNI_L("");
}

void
XSLT_Engine::StartCollectResultTreeFragmentL ()
{
  output_handler = CollectResultTreeFragment::PushL (output_handler);
}

void
XSLT_Engine::EndCollectResultTreeFragment ()
{
  output_handler = CollectResultTreeFragment::Pop (output_handler, &current_state->tree);
}

XSLT_VariableValue *
XSLT_Engine::GetVariableValue (const XSLT_Variable *variable_elm)
{
  if (variable_elm->GetParent ()->GetType () == XSLTE_STYLESHEET)
    return global_variables.GetVariableValue (variable_elm);
  else
    return current_state->variables->GetVariableValue (variable_elm);
}

OP_BOOLEAN
XSLT_Engine::FindKeyedNodes (const XMLExpandedName &name, XMLTreeAccessor *tree, const uni_char *value, XPathValue *result)
{
  OP_MEMORY_VAR BOOL paused;
  TRAPD (status, paused = !keytable.FindNodesL (this, name, tree, value, result));
  return OpStatus::IsError (status) ? status : paused ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
}

void
XSLT_Engine::CalculateVariableValueL (XSLT_Variable *variable, XSLT_VariableValue *value)
{
  XSLT_Program *variable_program = variable->CompileProgramL (transformation->GetStylesheet (), transformation);

#ifdef XSLT_PROGRAM_DUMP_SUPPORT
  OpString8 variable_name;
  const XMLExpandedName& name = variable->GetName();
  variable_name.SetUTF8FromUTF16L(name.GetLocalPart());
  variable_program->program_description.AppendFormat("Evaluate variable %s", variable_name.CStr());
#endif // XSLT_PROGRAM_DUMP_SUPPORT

  ProgramState *state = current_state;
  while (state->previous_state)
    state = state->previous_state;

  CallProgramL (state->context_node, FALSE, variable_program, FALSE);
  current_state->context_position = 1;
  current_state->context_size = 1;
  current_state->variables = &global_variables;

  SetIsProgramInterrupted ();

  value->SetIsBeingCalculated ();
}

void
XSLT_Engine::SignalErrorL (XSLT_Instruction *instruction, const char *reason)
{
#ifdef XSLT_ERRORS
  if (instruction->origin)
    instruction->origin->SignalErrorL (transformation, reason);
  else
    XSLT_SignalErrorL (transformation, 0, reason);
#else // XSLT_ERRORS
  LEAVE (OpStatus::ERR);
#endif // XSLT_ERRORS
}

void
XSLT_Engine::HandleXPathResultL (OP_STATUS status, ProgramState *state)
{
#ifdef XSLT_ERRORS
  HandleXPathResultL (status, state, state->evaluate, state->xpath_wrapper);
#else // XSLT_ERRORS
  LEAVE_IF_ERROR (status);
#endif // XSLT_ERRORS
}

void
XSLT_Engine::HandleXPathResultL (OP_STATUS status, ProgramState *state, XPathExpression::Evaluate *evaluate, XSLT_XPathExpressionOrPattern *wrapper)
{
#ifdef XSLT_ERRORS
  if (status == OpStatus::ERR_NO_MEMORY)
    LEAVE (status);
  else if (status == OpStatus::ERR)
    {
      OpString error_message; ANCHOR (OpString, error_message);

      const uni_char *xpath_type, *xpath_error_message = 0, *xpath_source = 0;

      if (evaluate)
        {
          xpath_type = UNI_L ("evaluation of XPath expression");
          xpath_error_message = evaluate->GetErrorMessage ();
          xpath_source = wrapper ? wrapper->GetSource () : 0;
        }
      else
        {
          xpath_type = UNI_L ("matching XPath pattern");

          if (state->match)
            {
              xpath_error_message = state->match->GetErrorMessage ();
              xpath_source = state->match->GetFailedPatternSource ();
            }
          else if (state->count)
            {
              xpath_error_message = state->count->GetErrorMessage ();
              xpath_source = state->count->GetFailedPatternSource ();
            }
          else if (state->search)
            {
              xpath_error_message = state->search->GetErrorMessage ();
              xpath_source = state->search->GetFailedPatternSource ();
            }
          else
            {
              OP_ASSERT (FALSE);
              LEAVE (status);
            }
        }

      if (xpath_source)
        LEAVE_IF_ERROR (error_message.AppendFormat (UNI_L ("%s failed: %s\nDetails: %s"), xpath_type, xpath_source, xpath_error_message));
      else
        LEAVE_IF_ERROR (error_message.AppendFormat (UNI_L ("%s failed\nDetails: %s"), xpath_type, xpath_error_message));

      if (!error_detail.IsEmpty ())
        LEAVE_IF_ERROR (error_message.AppendFormat (UNI_L ("\nAdditional information: %s"), error_detail.CStr ()));

      if (wrapper)
        wrapper->GetString ().SignalErrorL (transformation, error_message.CStr ());
      else if (GetCurrentInstruction ()->origin)
        GetCurrentInstruction ()->origin->SignalErrorL (transformation, error_message.CStr ());
      else
        XSLT_SignalErrorL (transformation, 0, error_message.CStr ());
    }
#else // XSLT_ERRORS
  LEAVE_IF_ERROR (status);
#endif // XSLT_ERRORS
}

static void
XSLT_CopyOfL (XSLT_Element *copy_of, XSLT_OutputHandler *output_handler, XPathNode *node)
{
  XMLTreeAccessor *tree = node->GetTreeAccessor ();
  XMLTreeAccessor::Node *treenode = node->GetNode ();

  if (node->GetType () == XPathNode::ATTRIBUTE_NODE)
    {
      XMLCompleteName name;

      node->GetNodeName (name);

      output_handler->CopyOfL (copy_of, tree, treenode, name);
    }
  else
    output_handler->CopyOfL (copy_of, tree, treenode);
}

BOOL
XSLT_Engine::AddCopyOfEvaluateL (XSLT_CopyOf *copy_of)
{
  ProgramState *state = current_state;
  XPathExpression::Evaluate *evaluate = state->evaluate;

  evaluate->SetCostLimit (instructions_left_in_slice);

  unsigned type;
  OP_BOOLEAN finished;

  HandleXPathResultL (finished = evaluate->CheckResultType (type), state);

  instructions_left_in_slice -= evaluate->GetLastOperationCost ();

  if (finished == OpBoolean::IS_FALSE)
    return FALSE;

  if (instructions_left_in_slice < 0)
    return FALSE;

  evaluate->SetCostLimit (instructions_left_in_slice);

  if ((type & XPathExpression::Evaluate::NODESET_ITERATOR) != 0)
    {
      XPathNode *node;

      while (TRUE)
        {
          HandleXPathResultL (finished = evaluate->GetNextNode (node), state);

          instructions_left_in_slice -= evaluate->GetLastOperationCost ();

          if (finished == OpBoolean::IS_FALSE)
            return FALSE;

          if (!node)
            break;

          XSLT_CopyOfL (copy_of, output_handler, node);
        }
    }
  else if ((type & XPathExpression::Evaluate::NODESET_SNAPSHOT) != 0)
    {
      unsigned count;

      HandleXPathResultL (finished = evaluate->GetNodesCount (count), state);

      instructions_left_in_slice -= evaluate->GetLastOperationCost ();

      if (finished == OpBoolean::IS_FALSE)
        return FALSE;

      XPathNode *node;

      for (unsigned index = 0; index < count; ++index)
        {
          LEAVE_IF_ERROR (evaluate->GetNode (node, index));
          XSLT_CopyOfL (copy_of, output_handler, node);
        }
    }
  else
    {
      const uni_char *result;

      HandleXPathResultL (finished = evaluate->GetStringResult (result), state);

      instructions_left_in_slice -= evaluate->GetLastOperationCost ();

      if (finished == OpBoolean::IS_FALSE)
        return FALSE;

      output_handler->AddTextL (result, FALSE);
    }

  XPathExpression::Evaluate::Free (evaluate);
  state->evaluate = 0;
#ifdef XSLT_ERRORS
  state->xpath_wrapper = 0;
#endif // XSLT_ERRORS

  return TRUE;
}

XPathNode::Type XSLT_Engine::AddCopyL(XPathNode* node)
{
  XPathNode::Type node_type = node->GetType();
  switch (node_type)
    {
    case XPathNode::ATTRIBUTE_NODE:
      {
        XMLCompleteName attribute_name;
        node->GetNodeName(attribute_name);
        TempBuffer attribute_value;
        LEAVE_IF_ERROR(node->GetStringValue(attribute_value));
        output_handler->AddAttributeL (attribute_name,
          attribute_value.GetStorage() ? attribute_value.GetStorage() : UNI_L(""),
          FALSE /* id */, TRUE /* specified */);
        break;
      }
    case XPathNode::NAMESPACE_NODE:
      OP_ASSERT(!"Not implemented");
      //		AddCopyOfNamespaceNodeL();
      break;
    case XPathNode::TEXT_NODE:
      {
        TempBuffer text;
        LEAVE_IF_ERROR(node->GetStringValue(text));
        output_handler->AddTextL(text.GetStorage() ? text.GetStorage() : UNI_L(""), FALSE);
        break;
      }
    case XPathNode::COMMENT_NODE:
      {
        TempBuffer text;
        LEAVE_IF_ERROR(node->GetStringValue(text));
        output_handler->AddCommentL(text.GetStorage() ? text.GetStorage() : UNI_L(""));
        break;
      }
    case XPathNode::PI_NODE:
      {
        XMLExpandedName target;
        node->GetExpandedName(target);
        TempBuffer data;
        LEAVE_IF_ERROR(node->GetStringValue(data));
        output_handler->AddProcessingInstructionL(target.GetLocalPart(),
          data.GetStorage() ? data.GetStorage() : UNI_L(""));
        break;
      }
    case XPathNode::ELEMENT_NODE:
      {
        XMLCompleteName name;
        node->GetNodeName (name);
        StartElementL (name, TRUE);
        break;
      }
    default:
      OP_ASSERT(!"Unreachable (or missing case)");
    case XPathNode::ROOT_NODE:
      // nothing done here
      break;
    }

  return node_type;
}


void
XSLT_Engine::ExecuteProgramL ()
{
#ifdef XSLT_DEBUG_MODE
  instructions_left_in_slice = 10;
#else // XSLT_DEBUG_MODE
  instructions_left_in_slice = 10000;
#endif // XSLT_DEBUG_MODE

 new_program:
#ifdef XSLT_PROGRAM_DUMP_SUPPORT
  current_state->program->DumpProgram (current_state->instruction_index);
#endif // XSLT_PROGRAM_DUMP_SUPPORT

  uni_char **strings = current_state->program->strings;
  XMLCompleteName **names = current_state->program->names;
  XMLNamespaceDeclaration **nsdeclarations = current_state->program->nsdeclarations;

  while (instructions_left_in_slice > 0)
    {
      // Check that the instruction is inside the current program
      OP_ASSERT (current_state->instruction_index <= current_state->program->instructions_count);

      XSLT_Instruction *instruction = current_state->program->instructions + current_state->instruction_index;
      XSLT_Instruction::Code instruction_code = instruction->code;

#ifdef XSLT_STATISTICS
      ++transformation->instructions_executed;
#endif // XSLT_STATISTICS

#ifdef XSLT_PROGRAM_DUMP_SUPPORT
      current_state->program->DumpInstruction(*instruction);
#endif // XSLT_PROGRAM_DUMP_SUPPORT

      instructions_left_in_slice--;

      switch (instruction_code)
        {
        case XSLT_Instruction::IC_EVALUATE_TO_NUMBER:
          if (!EvaluateExpressionL (XPathExpression::Evaluate::PRIMITIVE_NUMBER, instruction->argument))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_EVALUATE_TO_BOOLEAN:
          if (!EvaluateExpressionL (XPathExpression::Evaluate::PRIMITIVE_BOOLEAN, instruction->argument))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_EVALUATE_TO_STRING:
          if (!EvaluateExpressionL (XPathExpression::Evaluate::PRIMITIVE_STRING, instruction->argument))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_EVALUATE_TO_NODESET_ITERATOR:
          if (!EvaluateExpressionL (XPathExpression::Evaluate::NODESET_ITERATOR | XPathExpression::Evaluate::NODESET_FLAG_ORDERED, instruction->argument))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_EVALUATE_TO_NODESET_SNAPSHOT:
          if (!EvaluateExpressionL (XPathExpression::Evaluate::NODESET_SNAPSHOT | XPathExpression::Evaluate::NODESET_FLAG_ORDERED, instruction->argument))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_EVALUATE_TO_VARIABLE_VALUE:
          if (!EvaluateExpressionL (XPathExpression::Evaluate::TYPE_INVALID, instruction->argument))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_MATCH_PATTERNS:
          if (!MatchPatternL (instruction->argument & 0xffffu, (instruction->argument >> 16) & 0xffffu))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_SEARCH_PATTERNS:
          SearchPatternL (instruction->argument);
          break;

        case XSLT_Instruction::IC_COUNT_PATTERNS_AND_ADD:
          if (!CountPatternsAndAddL(reinterpret_cast<const XSLT_Number*>(instruction->argument)))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_APPEND_STRING:
          AppendStringL (strings[instruction->argument]);
          break;

        case XSLT_Instruction::IC_SET_STRING:
          SetString (strings[instruction->argument]);
          break;

        case XSLT_Instruction::IC_SET_NAME:
          SetName (*names[instruction->argument]);
          break;

        case XSLT_Instruction::IC_SET_QNAME:
          if (!SetQNameL ())
            {
              current_state->instruction_index += instruction->argument;
              continue;
            }
          break;

        case XSLT_Instruction::IC_SET_URI:
          SetURIL ();
          break;

        case XSLT_Instruction::IC_RESOLVE_NAME:
          ResolveNameL (nsdeclarations[instruction->argument & 0x7fffffff], (instruction->argument & 0x80000000u) != 0, instruction);
          break;

        case XSLT_Instruction::IC_SET_NAME_FROM_NODE:
          SetNameFromNode();
          break;

        case XSLT_Instruction::IC_START_ELEMENT:
          if (instruction->argument == ~0u)
            StartElementL (GetName (), TRUE);
          else
            StartElementL (*names[instruction->argument], FALSE);
          break;

        case XSLT_Instruction::IC_SUGGEST_NSDECL:
          SuggestNamespaceDeclarationL (reinterpret_cast<XSLT_Element *> (instruction->argument));
          break;

        case XSLT_Instruction::IC_ADD_ATTRIBUTE:
          if (instruction->argument == ~0u)
            AddAttributeL (GetName ());
          else
            AddAttributeL (*names[instruction->argument]);
          break;

        case XSLT_Instruction::IC_ADD_TEXT:
          AddTextL (instruction);
          break;

        case XSLT_Instruction::IC_ADD_COMMENT:
          AddCommentL ();
          break;

        case XSLT_Instruction::IC_ADD_PROCESSING_INSTRUCTION:
          AddProcessingInstructionL ();
          break;

        case XSLT_Instruction::IC_END_ELEMENT:
          if (instruction->argument == ~0u)
            EndElementL (0);
          else
            EndElementL (names[instruction->argument]);
          break;

        case XSLT_Instruction::IC_ADD_COPY_OF_EVALUATE:
          if (!AddCopyOfEvaluateL (reinterpret_cast<XSLT_CopyOf *> (instruction->argument)))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_ADD_COPY_AND_JUMP_IF_NO_END_ELEMENT:
          {
            XPathNode::Type type = AddCopyL(current_state->context_node);
            if (type == XPathNode::ELEMENT_NODE)
              {
                current_state->instruction_index += 2;
                continue; // Skip past the other instruction update after the switch
              }
            else if (type != XPathNode::ROOT_NODE)
              {
                current_state->instruction_index += instruction->argument;
                continue; // Skip past the other instruction update after the switch
              }
            // else just go on
          }
          break;

        case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODE:
        case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODE_NO_SCOPE:
        case XSLT_Instruction::IC_APPLY_IMPORTS_ON_NODE:
          if (CallProgramOnNodeL (instruction->argument, instruction_code != XSLT_Instruction::IC_CALL_PROGRAM_ON_NODE_NO_SCOPE, instruction_code == XSLT_Instruction::IC_APPLY_IMPORTS_ON_NODE))
            goto new_program;
          break;

        case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODES:
        case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODES_NO_SCOPE:
          switch (CallProgramOnNextNodeL (instruction->argument, instruction_code == XSLT_Instruction::IC_CALL_PROGRAM_ON_NODES))
            {
            case 0:
              if (!IsProgramInterrupted ())
                return;
            case 1:
              goto new_program;
            }
          break;

        case XSLT_Instruction::IC_APPLY_TEMPLATES:
          switch (ApplyTemplatesL (instruction->argument))
            {
            case 0:
              if (!IsProgramInterrupted ())
                return;
              /* fall through */
            case 1:
              goto new_program;
            }
          break;

        case XSLT_Instruction::IC_APPLY_BUILTIN_TEMPLATE:
          if (ApplyBuiltInTemplateL (instruction->argument))
            goto new_program;
          break;

        case XSLT_Instruction::IC_RETURN:
          if (!Return())
            {
              /* End of program. */
              output_handler->EndOutputL ();
              return;
            }
          goto new_program;

        case XSLT_Instruction::IC_JUMP_IF_TRUE:
        case XSLT_Instruction::IC_JUMP_IF_FALSE:
          {
            BOOL jump_if = (instruction->code == XSLT_Instruction::IC_JUMP_IF_TRUE);
            if (current_state->current_boolean != jump_if)
              break;
          }
          // else fall-through

        case XSLT_Instruction::IC_JUMP:
          current_state->instruction_index += instruction->argument;
          continue; // Skip past the other instrucion update after the switch

        case XSLT_Instruction::IC_START_COLLECT_TEXT:
          StartCollectTextL ();
          break;

        case XSLT_Instruction::IC_END_COLLECT_TEXT:
          EndCollectTextL ();
          break;

        case XSLT_Instruction::IC_START_COLLECT_RESULTTREEFRAGMENT:
          StartCollectResultTreeFragmentL ();
          break;

        case XSLT_Instruction::IC_END_COLLECT_RESULTTREEFRAGMENT:
          EndCollectResultTreeFragment ();
          break;

        case XSLT_Instruction::IC_SORT:
          if (!SortL (reinterpret_cast<XSLT_Sort *> (instruction->argument)))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_SET_SORT_PARAMETER_ORDER:
        case XSLT_Instruction::IC_SET_SORT_PARAMETER_LANG:
        case XSLT_Instruction::IC_SET_SORT_PARAMETER_DATATYPE:
        case XSLT_Instruction::IC_SET_SORT_PARAMETER_CASEORDER:
          SetSortParameterFromStringL (reinterpret_cast<XSLT_Sort*> (instruction->argument), instruction_code);
          break;

        case XSLT_Instruction::IC_ADD_FORMATTED_NUMBER:
          if (!AddFormattedNumberL (reinterpret_cast<XSLT_Number *> (instruction->argument)))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_TEST_AND_SET_IF_PARAM_IS_PRESET:
          TestAndSetIfParamIsPresetL (reinterpret_cast<XSLT_Variable *> (instruction->argument));
          break;

        case XSLT_Instruction::IC_SET_WITH_PARAM_FROM_EVALUATE:
        case XSLT_Instruction::IC_SET_WITH_PARAM_FROM_STRING:
        case XSLT_Instruction::IC_SET_WITH_PARAM_FROM_COLLECTED:
        case XSLT_Instruction::IC_SET_VARIABLE_FROM_EVALUATE:
        case XSLT_Instruction::IC_SET_VARIABLE_FROM_STRING:
        case XSLT_Instruction::IC_SET_VARIABLE_FROM_COLLECTED:
          if (!SetVariableL (instruction->code, reinterpret_cast<XSLT_Variable* > (instruction->argument)))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        case XSLT_Instruction::IC_START_COLLECT_PARAMS:
          XSLT_VariableStore::PushL (current_state->params_collected_for_call);
          break;

        case XSLT_Instruction::IC_RESET_PARAMS_COLLECTED_FOR_CALL:
          XSLT_VariableStore::Pop (current_state->params_collected_for_call);
          break;

        case XSLT_Instruction::IC_SEND_MESSAGE:
          SendMessageL (instruction);
          break;

        case XSLT_Instruction::IC_ERROR:
          SignalErrorL (instruction, reinterpret_cast<const char *> (instruction->argument));
          break;

        case XSLT_Instruction::IC_PROCESS_KEY:
          if (!ProcessKeyL (reinterpret_cast<XSLT_Key *> (instruction->argument)))
            if (IsProgramInterrupted ())
              goto new_program;
            else
              return;
          break;

        default:
          OP_ASSERT (!"Unknown instruction in XSLT program");
        }

      current_state->instruction_index++;
    }
}

void
XSLT_Engine::SetDetectedOutputMethod (XSLT_Stylesheet::OutputSpecification::Method method)
{
  uncertain_output_method = FALSE;
  need_output_handler_switch = TRUE;
  instructions_left_in_slice = 0;
  auto_detected_html = method == XSLT_Stylesheet::OutputSpecification::METHOD_HTML;
}

OP_STATUS
XSLT_Engine::SwitchOutputHandler (XSLT_OutputHandler *new_root_outputhandler)
{
  OP_ASSERT (NeedsOutputHandlerSwitch ());

  XSLT_RecordingOutputHandler *old_output_handler = static_cast<XSLT_RecordingOutputHandler *> (output_handler);

  root_output_handler = output_handler = new_root_outputhandler;
  need_output_handler_switch = uncertain_output_method = FALSE;

  TRAPD (status, old_output_handler->ReplayCommandsL (output_handler));
  return status;
}

void
XSLT_Engine::HandleXPathResultL (OP_STATUS status, XPathExpression::Evaluate *evaluate, XSLT_XPathExpressionOrPattern *wrapper)
{
#ifdef XSLT_ERRORS
  HandleXPathResultL (status, current_state, evaluate, wrapper);
#else // XSLT_ERRORS
  LEAVE_IF_ERROR (status);
#endif // XSLT_ERRORS
}

void
XSLT_Engine::CalculateKeyValueL (XSLT_Key *key, XMLTreeAccessor *tree)
{
  XSLT_Program *key_program = key->CompileProgramL (transformation->GetStylesheet (), transformation);

#ifdef XSLT_PROGRAM_DUMP_SUPPORT
  OpString8 key_name;
  key_name.SetUTF8FromUTF16L (key->GetName ().GetLocalPart());
  key_program->program_description.AppendFormat ("Evaluate key %s", key_name.CStr ());
#endif // XSLT_PROGRAM_DUMP_SUPPORT

  XPathNode *context_node;

  LEAVE_IF_ERROR (XPathNode::Make (context_node, tree, tree->GetRoot ()));

  CallProgramL (context_node, TRUE, key_program, FALSE);
  current_state->context_position = 1;
  current_state->context_size = 1;

  SetIsProgramInterrupted ();
}

/* static */ XSLT_TransformationImpl *
XSLT_Engine::GetTransformation (XPathExtensions::Context *context)
{
  XSLT_Engine *engine = static_cast<XSLT_Engine *> (context);
  return engine->transformation;
}

/* static */ XPathNode *
XSLT_Engine::GetCurrentNode (XPathExtensions::Context *context)
{
  XSLT_Engine *engine = static_cast<XSLT_Engine *> (context);
  if (engine->current_state->program->type == XSLT_Program::TYPE_KEY)
    // When we're evaluating a key we have the "current" node in a different pointer than normally
    return engine->current_state->current_node;
  else
    return engine->current_state->context_node;
}

/* static */ XSLT_VariableValue *
XSLT_Engine::GetVariableValue(XPathExtensions::Context* context, const XSLT_Variable* variable_elm)
{
  OP_ASSERT (context);
  OP_ASSERT (variable_elm);

  XSLT_Engine* engine = static_cast<XSLT_Engine *> (context);
  return engine->GetVariableValue (variable_elm);
}

/* static */ OP_BOOLEAN
XSLT_Engine::FindKeyedNodes (XPathExtensions::Context *context, const XMLExpandedName &name, XMLTreeAccessor *tree, const uni_char *value, XPathValue *result)
{
  return static_cast<XSLT_Engine *> (context)->FindKeyedNodes (name, tree, value ? value : UNI_L (""), result);
}

/* static */ void
XSLT_Engine::SetBlocked (XPathExtensions::Context *context)
{
  XSLT_Engine *engine = static_cast<XSLT_Engine *> (context);
  engine->SetIsProgramBlocked ();
}

/* static */ OP_STATUS
XSLT_Engine::CalculateVariableValue (XPathExtensions::Context *context, XSLT_Variable *variable, XSLT_VariableValue *value)
{
  TRAPD (status, static_cast<XSLT_Engine *> (context)->CalculateVariableValueL (variable, value));
  return status;
}

#ifdef XSLT_ERRORS

static OP_STATUS
XSLT_AppendNameToString (OpString &string, const XMLCompleteName &name)
{
  if (name.GetPrefix ())
    return string.AppendFormat (UNI_L ("%s:%s"), name.GetPrefix (), name.GetLocalPart ());
  else
    return string.Append (name.GetLocalPart ());
}

/* static */ OP_STATUS
XSLT_Engine::ReportCircularVariables (XPathExtensions::Context *context, const XMLCompleteName &name)
{
  OpString &message = static_cast<XSLT_Engine *> (context)->error_detail;

  message.Empty ();

  RETURN_IF_ERROR (message.Set ("circular dependency between variables: "));
  RETURN_IF_ERROR (XSLT_AppendNameToString (message, name));

  XSLT_Engine::ProgramState *state = static_cast<XSLT_Engine *> (context)->current_state;
  while (state)
    {
      if (state->program->type == XSLT_Program::TYPE_TOP_LEVEL_VARIABLE)
        {
          RETURN_IF_ERROR (message.Append (" <= "));
          RETURN_IF_ERROR (XSLT_AppendNameToString (message, state->program->variable->GetName ()));

          if (state->program->variable->GetName () == name)
            break;
        }

      state = state->previous_state;
    }

  return OpStatus::OK;
}

#endif // XSLT_ERRORS
#endif // XSLT_SUPPORT
