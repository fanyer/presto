/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_PATTERN_SUPPORT

#include "modules/xpath/src/xppatternmachine.h"

XPath_PatternMachine::Instruction *
XPath_PatternMachine::AddInstructionL (Instruction::Code code, unsigned arg0, unsigned arg1, unsigned arg2, unsigned arg3)
{
  if (instruction_index == instructions_count)
    {
      unsigned new_instructions_count = instructions_count == 0 ? 16 : instructions_count + instructions_count;
      Instruction *new_instructions = OP_NEWA_L (Instruction, new_instructions_count);
      op_memcpy (new_instructions, instructions, instructions_count);
      OP_DELETEA (instructions);
      instructions = new_instructions;
      instructions_count = new_instructions_count;
    }

  Instruction *instruction = instructions[instruction_index++];
  instruction->code = code;
  op_memset (instruction->arguments, ~0u, sizeof instruction->arguments);
  return instruction;
}

static BOOL
XPath_IsMultipleOfTwo (unsigned number)
{
  return (number - 1) ^ number == (number - 1) + number;
}

unsigned
XPath_PatternMachine::AddPredicateL (XPath_Expression *expression)
{
  if (predicates_count == 0 || predicates_count >= 8 && XPath_IsMultipleOfTwo (predicates_count))
    {
      unsigned new_predicates_count = predicates_count == 0 ? 8 : predicates_count
      unsigned new_predicates = OP_NEWA_L (XPath_Expression *, predicates_count);
      op_memcpy (new_predicates, predicates, predicates_count);
      OP_DELETEA (predicates);
      predicates = new_predicates;
    }

  predicates[predicates_count++] = expression;
}

unsigned
XPath_PatternMachine::AddScanL (XPath_Scan *scan)
{
  if (scans_count == 0 || scans_count >= 8 && XPath_IsMultipleOfTwo (scans_count))
    {
      unsigned new_scans_count = scans_count == 0 ? 8 : scans_count
      unsigned new_scans = OP_NEWA_L (XPath_Expression *, scans_count);
      op_memcpy (new_scans, scans, scans_count);
      OP_DELETEA (scans);
      scans = new_scans;
    }

  scans[scans_count++] = scan;
}

unsigned
XPath_PatternMachine::AddFilterL (XPath_XMLTreeAccessorFilter *filter)
{
  if (filters_count == 0 || filters_count >= 8 && XPath_IsMultipleOfTwo (filters_count))
    {
      unsigned new_filters_count = filters_count == 0 ? 8 : filters_count
      unsigned new_filters = OP_NEWA_L (XPath_Expression *, filters_count);
      op_memcpy (new_filters, filters, filters_count);
      OP_DELETEA (filters);
      filters = new_filters;
    }

  filters[filters_count++] = filter;
}

void
XPath_PatternMachine::SetJumpTo (unsigned destination)
{
  instructions[instruction_index - 1]->arguments[4] = destination;
}

void
XPath_PatternMachine::SetJumpFrom (unsigned source)
{
  instructions[source]->arguments[4] = instruction_index - 1;
}

BOOL
XPath_PatternMachine::FilterL (XPath_XMLTreeAccessorFilter *filter, XPath_Node *node)
{
  filter->SetFilter (node->tree);
  BOOL result = node->tree->FilterNode (node->treenode);
  node->tree->ResetFilters ();
  return result;
}

BOOL
XPath_PatternMachine::EvaluateL (XPath_Expression *expression, XPath_Node *node, unsigned position0, unsigned size0)
{
  unsigned position, size;
  if ((position0 & 0x80000000u) == 0)
    position = numbers[position0];
  else
    position = nodelist_indeces[position0 & 0x7fffffffu];
  if ((size0 & 0x80000000u) == 0)
    size = numbers[size0];
  else
    size = nodelists[size0 & 0x7fffffffu].GetCount ();

  /* FIXME: Implement. */

  return TRUE;
}

BOOL
XPath_PatternMachine::GetParent (unsigned scan0, XPath_Node *child, XPath_Node *target)
{
  XMLTreeAccessor::Node *treenode;
  if (scan0 == ~0u)
    treenode = child->tree->GetParent (child->treenode);
  else
    {
      treenode = scans[scan0]->GetParent (child->tree, child->treenode);
      scans[scan0]->Reset ();
    }
  if (treenode)
    {
      target->Set (child->tree, treenode);
      return TRUE;
    }
  else
    return FALSE;
}

BOOL
XPath_PatternMachine::GetChild (XPath_Scan *scan, XPath_Node *parent, XPath_Node *target)
{
  if (XMLTreeAccessor::Node *treenode = scan->GetChild (parent->tree, parent->treenode))
    {
      target->Set (parent->tree, treenode);
      return TRUE;
    }
  else
    return FALSE;
}

BOOL
XPath_PatternMachine::GetAncestorOrSelf (XPath_Scan *scan, XPath_Node *origin, XPath_Node *target)
{
  if (XMLTreeAccessor::Node *treenode = scan->GetAncestorOrSelf (origin->tree, origin->treenode))
    {
      target->Set (origin->tree, treenode);
      return TRUE;
    }
  else
    return FALSE;
}

BOOL
XPath_PatternMachine::GetAncestor (XPath_Scan *scan, XPath_Node *descendant, XPath_Node *target)
{
  if (XMLTreeAccessor::Node *treenode = scan->GetAncestor (descendant->tree, descendant->treenode))
    {
      target->Set (descendant->tree, treenode);
      return TRUE;
    }
  else
    return FALSE;
}

BOOL
XPath_PatternMachine::ExecuteL (XPath_Node **yielded)
{
#define ARGUMENT(index) (instruction->arguments[index])
#define JUMP() instruction_index = instruction->arguments[4];

  while (TRUE)
    {
      Instruction *instruction = instructions[instruction_index++];

      switch (instruction->code)
        {
        case Instruction::CODE_RESET:
          scans[instruction->arguments[0]]->Reset ();
          break;

        case Instruction::CODE_GET_PARENT:
          if (!GetParent (ARGUMENT (0), &nodes[ARGUMENT (1)], &nodes[ARGUMENT (2)]))
            JUMP ();
          break;

        case Instruction::CODE_CHILD:
          if (!GetChild (scans[ARGUMENT (0)], &nodes[ARGUMENT (1)], &nodes[ARGUMENT (2)]))
            JUMP ();
          break;

        case Instruction::CODE_ANCESTOR_OR_SELF:
          if (!GetAncestorOrSelf (scans[ARGUMENT (0)], &nodes[ARGUMENT (1)], &nodes[ARGUMENT (2)]))
            JUMP ();
          break;

        case Instruction::CODE_ANCESTOR:
          if (!GetAncestor (scans[ARGUMENT (0)], &nodes[ARGUMENT (1)], &nodes[ARGUMENT (2)]))
            JUMP ();
          break;

        case Instruction::CODE_FILTER:
          if (!FilterL (filters[ARGUMENT (0)], &nodes[ARGUMENT (1)]))
            JUMP ();
          break;

        case Instruction::CODE_EVALUATE:
          if (!EvaluateL (predicates[ARGUMENT (0)], &nodes[ARGUMENT (1)], ARGUMENT (2), ARGUMENT (3)))
            JUMP ();
          break;

        case Instruction::CODE_EQUAL:
          if (XPath_Node::Equals (&nodes[ARGUMENT (0)], &nodes[ARGUMENT (1)]))
            JUMP ();
          break;

        case Instruction::CODE_ADD_NODE:
          nodelists[ARGUMENT (0)].AddL (context, &nodes[ARGUMENT (1)]);
          break;

        case Instruction::CODE_ITERATE_LIST:
          if (nodelist_indeces[ARGUMENT (0)] < nodelists[ARGUMENT (0)].GetCount ())
            nodes[ARGUMENT (1)].CopyL (nodelists[ARGUMENT (0)].Get (nodelist_indeces[ARGUMENT (0)]++));
          else
            {
              nodelist_indeces[ARGUMENT (0)] = 0;
              JUMP ();
            }
          break;

        case Instruction::CODE_ADD_NUMBER:
          /* FIXME: Implement. */
          break;

        case Instruction::CODE_INCREMENT_NUMBER:
          if ((ARGUMENT (0) & 0x80000000u) == 0)
            ++numbers[ARGUMENT (0)];
          else
            /* FIXME: Implement. */;
          break;

        case Instruction::CODE_RESET_NUMBER:
          if ((ARGUMENT (0) & 0x80000000u) == 0)
            numbers[ARGUMENT (0)] = 0;
          else
            /* FIXME: Implement. */;
          break;

        case Instruction::CODE_RETURN:
          return ARGUMENT (0) != 0;

        case Instruction::CODE_YIELD:
          *yielded = XPath_Node::IncRef (&nodes[ARGUMENT (0)]);
          return TRUE;
        }
    }
}

#endif // XPATH_PATTERN_SUPPORT
