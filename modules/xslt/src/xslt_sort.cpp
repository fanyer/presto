/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_sort.h"

#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_applytemplates.h"
#include "modules/xslt/src/xslt_foreach.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_attributevaluetemplate.h"
#include "modules/xslt/src/xslt_transformation.h"
#include "modules/xslt/src/xslt_engine.h"
#include "modules/xpath/xpath.h"
#include "modules/util/tempbuf.h"
#include "modules/hardcore/unicode/unicode.h"


/* static */ int
XSLT_Sort::CompareText (const OpString &text1, const OpString &text2, CaseOrder caseorder)
{
  const uni_char *string1 = text1.CStr (), *string2 = text2.CStr ();
  unsigned length1 = text1.Length (), length2 = text2.Length ();
  BOOL different_case = FALSE;

  for (unsigned index = 0, length = MAX (length1, length2); index < length; ++index, ++string1, ++string2)
    if (*string1 != *string2)
      {
        uni_char upper1 = uni_toupper (*string1), upper2 = uni_toupper (*string2);

        if (upper1 == upper2)
          different_case = TRUE;
        else
          return upper1 < upper2 ? -1 : 1;
      }

  if (length1 == length2 && different_case)
    {
      string1 = text1.CStr ();
      string2 = text2.CStr ();

      for (unsigned index = 0, length = MAX (length1, length2); index < length; ++index, ++string1, ++string2)
        if (*string1 != *string2)
          {
            uni_char upper1 = uni_toupper (*string1), upper2 = uni_toupper (*string2);

            if (upper1 == upper2)
              if (uni_isupper (*string1))
                return caseorder == CASEORDER_UPPERFIRST ? -1 : 1;
              else
                return caseorder == CASEORDER_UPPERFIRST ? 1 : -1;
          }
    }

  return length1 < length2 ? -1 : length1 == length2 ? 0 : 1;
}

/* static */ int
XSLT_Sort::CompareNumber (double number1, double number2)
{
  if (op_isnan (number1) && op_isnan (number2))
    return 0;
  else if (op_isnan (number1))
    return -1;
  else if (op_isnan (number2))
    return 1;
  else if (number1 < number2)
    return -1;
  else if (number1 > number2)
    return 1;
  else
    return 0;
}

/* static */ int
XSLT_Sort::Compare (unsigned index1, unsigned index2, const OpString *textkeys, double *numberkeys, XSLT_SortState* sortstate)
{
  int result;

  if (textkeys) // (datatype == DATATYPE_TEXT)
    result = CompareText (textkeys[index1], textkeys[index2], sortstate->caseorder );
  else
    result = CompareNumber (numberkeys[index1], numberkeys[index2]);

  if (result == 0)
    sortstate->has_equal = TRUE;

  if (sortstate->order == ORDER_ASCENDING)
    return result;
  else
    return -result;
}

/* static */void
XSLT_Sort::MergeSortL (unsigned count, unsigned *indeces, const OpString * const textkeys, double * const numberkeys, XSLT_SortState* sortstate)
{
  if (count == 2)
    {
      if (Compare (indeces[0], indeces[1], textkeys, numberkeys, sortstate) > 0)
        {
          unsigned temporary = indeces[0];
          indeces[0] = indeces[1];
          indeces[1] = temporary;
        }
    }
  else if (count > 2)
    {
      unsigned count1 = count / 2, count2 = count - count1;
      unsigned *sorted1 = OP_NEWA_L (unsigned, count), *sorted2 = sorted1 + count1;

      ANCHOR_ARRAY (unsigned, sorted1);

      op_memcpy (sorted1, indeces, count * sizeof indeces[0]);

      MergeSortL (count1, sorted1, textkeys, numberkeys, sortstate);
      MergeSortL (count2, sorted2, textkeys, numberkeys, sortstate);

      unsigned index = 0, index1 = 0, index2 = 0;

      while (index1 < count1 && index2 < count2)
        {
          unsigned smallest;

          if (Compare (sorted1[index1], sorted2[index2], textkeys, numberkeys, sortstate) <= 0)
            smallest = sorted1[index1++];
          else
            smallest = sorted2[index2++];

          indeces[index++] = smallest;
        }

      while (index1 < count1)
        indeces[index++] = sorted1[index1++];

      while (index2 < count2)
        indeces[index++] = sorted2[index2++];
    }
}

BOOL
XSLT_Sort::SortL (XSLT_Engine *engine, unsigned count, XPathNode **nodes, XSLT_SortState *&sortstate, int &instructions_left, XPathExtensions::Context *extensionscontext)
{
  OP_ASSERT(!is_secondary || sortstate);
  if (!is_secondary)
    SetRemainingParametersL (sortstate, engine);

  // FIXME: Move this code to XSLT_Engine (more logical) or into SortState (makes for easier code to read)?
  if (sortstate->unsortednodes == NULL) // Init the sort
    {
      sortstate->unsortednodes = OP_NEWA_L (XPathNode *, count);

      op_memcpy (sortstate->unsortednodes, nodes, count * sizeof nodes[0]);
      if (sortstate->datatype == DATATYPE_TEXT)
        {
          sortstate->resulttype = XPathExpression::Evaluate::PRIMITIVE_STRING;
          sortstate->textkeys = OP_NEWA_L (OpString, count);
        }
      else
        {
          sortstate->resulttype = XPathExpression::Evaluate::PRIMITIVE_NUMBER;
          sortstate->numberkeys = OP_NEWA_L (double, count);
        }

      sortstate->indeces = OP_NEWA_L (unsigned, count);
      sortstate->index = 0;
    }

  OP_ASSERT(sortstate);
  OP_ASSERT(sortstate->unsortednodes);
  OP_ASSERT(sortstate->textkeys || sortstate->numberkeys);
  OP_ASSERT(!(sortstate->textkeys && sortstate->numberkeys));

  if (!sortstate->has_calculated_sortvalues)
    {
      //  unsigned index;
      while (sortstate->index < count)
        {
          sortstate->indeces[sortstate->index] = sortstate->index;

          if (!sortstate->evaluate)
            {
              if (instructions_left <= 0)
                return FALSE;

              // Start evaluation of select for this node
              LEAVE_IF_ERROR (XPathExpression::Evaluate::Make (sortstate->evaluate, select.GetExpressionL (engine->GetTransformation ())));

              OP_ASSERT(sortstate->evaluate);
              sortstate->evaluate->SetRequestedType(sortstate->resulttype);
              sortstate->evaluate->SetExtensionsContext(extensionscontext);
              XPathNode *copy;
              LEAVE_IF_ERROR (XPathNode::MakeCopy (copy, sortstate->unsortednodes[sortstate->index]));
              sortstate->evaluate->SetContext(copy, sortstate->index + 1, count);
              sortstate->evaluate->SetCostLimit(instructions_left);
            }

          if (sortstate->datatype == DATATYPE_TEXT)
            {
              OP_BOOLEAN result;
              const uni_char *text;

              engine->HandleXPathResultL (result = sortstate->evaluate->GetStringResult (text), sortstate->evaluate, &select);

              instructions_left -= sortstate->evaluate->GetLastOperationCost();
              if (result == OpBoolean::IS_FALSE)
                return FALSE;
              LEAVE_IF_ERROR(sortstate->textkeys[sortstate->index].Set (text));
            }
          else
            {
              OP_BOOLEAN result;
              double number;

              engine->HandleXPathResultL (result = sortstate->evaluate->GetNumberResult (number), sortstate->evaluate, &select);

              instructions_left -= sortstate->evaluate->GetLastOperationCost();
              if (result == OpBoolean::IS_FALSE)
                return FALSE;
              sortstate->numberkeys[sortstate->index] = number;
            }

          XPathExpression::Evaluate::Free(sortstate->evaluate);
          sortstate->evaluate = NULL;

          sortstate->index++;

          if (instructions_left <= 0)
            return FALSE;
        }
      sortstate->has_calculated_sortvalues = TRUE;
    } // end if (!has_calculated_sortvalues)

  sortstate->has_equal = FALSE;

  MergeSortL (count, sortstate->indeces, sortstate->textkeys, sortstate->numberkeys, sortstate);

  instructions_left -= count*count; // very conservative estimate

  for (unsigned index = 0; index < count; ++index)
    nodes[index] = sortstate->unsortednodes[sortstate->indeces[index]];

  if (next && sortstate->has_equal)
    {
      unsigned index = 0;

      while (index < count - 1)
        {
          instructions_left--;
          if (Compare (sortstate->indeces[index], sortstate->indeces[index + 1], sortstate->textkeys, sortstate->numberkeys, sortstate) == 0)
            {
              unsigned equalcount = 2;

              while (index + equalcount < count && Compare (sortstate->indeces[index], sortstate->indeces[index + equalcount], sortstate->textkeys, sortstate->numberkeys, sortstate) == 0)
                ++equalcount;

              OP_ASSERT(!sortstate->next->unsortednodes); // Has to be an "empty" sortstate
              BOOL finished;
              do
                {
                  int many_instructions_left = 100000000;
                  finished = next->SortL (engine, equalcount, nodes + index, sortstate->next, many_instructions_left, extensionscontext);
                  OP_ASSERT(finished || sortstate->next);
                  if (!finished && engine->IsProgramInterrupted ())
                    {
                      /* Need to calculate a variable's value (most likely) before
                         restarting the computation of the sorted result. */
                      engine->SetIsProgramInterrupted ();
                      /* Removing the sort data is a conservative, albeit blunt measure
                         to get back to a state where sorting can be restarted.
                         FIXME: allow partial sorting to be resumed. */
                      sortstate->next->ClearSortData ();
                      return FALSE;
                    }
                  instructions_left -= (100000000 - many_instructions_left);
                }
              while (!finished);

              OP_ASSERT(!sortstate->next->unsortednodes);

              index += equalcount;
            }
          else
            ++index;
        } // end while
    }

  if (is_secondary)
    {
      sortstate->ClearSortData();
    }
  else
    {
      OP_DELETE (sortstate);
      sortstate = NULL;
    }

  return TRUE;
}

XSLT_Sort::XSLT_Sort (XSLT_Element *parent, XSLT_Variable* previous_variable)
  : select(GetXPathExtensions(), NULL), // nsdeclaration set in EndElement
//    order (ORDER_ASCENDING),
//    datatype (DATATYPE_TEXT),
//    caseorder (CASEORDER_UPPERFIRST),
    next (0),
	is_secondary(FALSE)
{
  SetType (XSLTE_SORT);
  SetParent (parent);
  extensions.SetPreviousVariable(previous_variable);
}

/* virtual */
XSLT_Sort::~XSLT_Sort ()
{
  OP_DELETE (next);
}

/* virtual */ XSLT_Element *
XSLT_Sort::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  parser->SignalErrorL ("unexpected element in xsl:sort");
  ignore_element = TRUE;
  return this;
}

/* virtual */ BOOL
XSLT_Sort::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  if (parser)
    {
      if (!select.IsSpecified ())
        select.SetSourceL (parser, XMLCompleteName (0, 0, UNI_L ("select")), UNI_L ("."), 1);

      select.SetNamespaceDeclaration (parser);
    }

  return FALSE;
}

/* virtual */ void
XSLT_Sort::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_ORDER:
      parser->SetStringL (order, name, value, value_length);
      break;

    case XSLTA_LANG:
      parser->SetStringL (lang, name, value, value_length);
      break;

    case XSLTA_DATA_TYPE:
      parser->SetStringL (datatype, name, value, value_length);
      break;

    case XSLTA_CASE_ORDER:
      parser->SetStringL (caseorder, name, value, value_length);
      break;

    case XSLTA_SELECT:
      select.SetSourceL (parser, name, value, value_length);
    }
}

/* static */ void
XSLT_Sort::Push (XSLT_Sort *&first, XSLT_Sort *next)
{
  if (first)
    next->SetIsSecondary();

  XSLT_Sort **pointer = &first;

  while (*pointer)
    pointer = &(*pointer)->next;

  *pointer = next;
}

static void
XSLT_Sort_CompileParamL (XSLT_Compiler *compiler, XSLT_Sort *sort, XSLT_String &param, XSLT_Instruction::Code instr)
{
  if (XSLT_AttributeValueTemplate::NeedsCompiledCode (param))
    {
      XSLT_AttributeValueTemplate::CompileL (compiler, sort, param);
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL (instr, reinterpret_cast<UINTPTR> (sort), sort);
    }
}

void
XSLT_Sort::CompileL (XSLT_Compiler* compiler)
{
  XSLT_Sort *sort = this;
  while (sort)
    {
      XSLT_Sort_CompileParamL (compiler, sort, sort->order, XSLT_Instruction::IC_SET_SORT_PARAMETER_ORDER);
      XSLT_Sort_CompileParamL (compiler, sort, sort->lang,  XSLT_Instruction::IC_SET_SORT_PARAMETER_LANG);
      XSLT_Sort_CompileParamL (compiler, sort, sort->datatype, XSLT_Instruction::IC_SET_SORT_PARAMETER_DATATYPE);
      XSLT_Sort_CompileParamL (compiler, sort, sort->caseorder, XSLT_Instruction::IC_SET_SORT_PARAMETER_CASEORDER);
      sort = sort->next;
    }
  XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_SORT, reinterpret_cast<UINTPTR> (this));
}


void
XSLT_Sort::InitSortStateL (XSLT_SortState *&sortstate, XSLT_Engine *engine)
{
  if (!sortstate)
    sortstate = OP_NEW_L (XSLT_SortState, (engine));
}

/* static */ void
XSLT_Sort::SetParamValue (XSLT_Sort::Attr param_attr, const uni_char *param_value, XSLT_SortState *sortstate)
{
  OP_ASSERT((param_attr < XSLT_Sort::Attr_LAST));
  OP_ASSERT(sortstate);

  switch (param_attr)
    {
    case XSLT_Sort::Attr_Order:
      if (param_value && uni_str_eq (param_value, "descending"))
        sortstate->order = ORDER_DESCENDING;
      else
        sortstate->order = ORDER_ASCENDING;
      break;
	case XSLT_Sort::Attr_Lang:
      // Ignored
      break;
    case XSLT_Sort::Attr_Datatype:
      if (param_value && uni_str_eq (param_value, "number"))
        sortstate->datatype = DATATYPE_NUMBER;
      else
        sortstate->datatype = DATATYPE_TEXT;
      break;
    default:
      OP_ASSERT(!"Not reachable");
      // Fall through
    case XSLT_Sort::Attr_Caseorder:
      if (param_value && uni_str_eq (param_value, "lower-first"))
        sortstate->caseorder = CASEORDER_LOWERFIRST;
      else
        sortstate->caseorder = CASEORDER_UPPERFIRST;
      break;
    }
}

void
XSLT_Sort::SetNextParameterL (const uni_char* param_value, XSLT_SortState *&in_sortstate, XSLT_Instruction::Code instr, XSLT_Engine *engine) const
{
  InitSortStateL (in_sortstate, engine);

  const XSLT_Sort* sort = this;
  OP_ASSERT(in_sortstate);
  XSLT_SortState* sortstate = in_sortstate;
  OP_ASSERT(sortstate->parameters_set <= 4);

  XSLT_Sort::Attr index;
  switch(instr)
    {
    case XSLT_Instruction::IC_SET_SORT_PARAMETER_ORDER:
  	  index = XSLT_Sort::Attr_Order;
  	  break;
    case XSLT_Instruction::IC_SET_SORT_PARAMETER_LANG:
  	  index = XSLT_Sort::Attr_Lang;
  	  break;
    case XSLT_Instruction::IC_SET_SORT_PARAMETER_DATATYPE:
  	  index = XSLT_Sort::Attr_Datatype;
  	  break;
    case XSLT_Instruction::IC_SET_SORT_PARAMETER_CASEORDER:
  	  index = XSLT_Sort::Attr_Caseorder;
  	  break;
    default:
  	  OP_ASSERT(!"Unexpected instruction");
  	  return;
    }

  while (sortstate->sort_node != sort)
    {
      if (sortstate->next)
  	    sortstate = sortstate->next;
      else if (sortstate->sort_node)
  	    {
  	      InitSortStateL (sortstate->next, engine);
  	      sortstate = sortstate->next;
  	      sortstate->sort_node = sort;
        }
  	  else
  	    sortstate->sort_node = sort;
    }
  SetParamValue(index, param_value, sortstate);
  if (!(sortstate->parameter_set_mask & (0x1 << static_cast<int>(index))))
    {
      sortstate->parameters_set++;
      sortstate->parameter_set_mask |= (0x1 << static_cast<int>(index));
    }
}

/* static  */ void
XSLT_Sort::SetRemainingParametersL (XSLT_SortState*& in_sortstate, XSLT_Engine *engine) const
{
  OP_ASSERT (!is_secondary);

  InitSortStateL (in_sortstate, engine);

  const XSLT_Sort* sort = this;
  OP_ASSERT(in_sortstate);
  XSLT_SortState* sortstate = in_sortstate;
  OP_ASSERT(sortstate->parameters_set <= 4);

  while (sortstate->parameters_set == 4)
    {
      sort = sort->next;
      if (!sort)
        return; // Everything set
      InitSortStateL (sortstate->next, engine);
      sortstate = sortstate->next;
    }

  while (sort)
    {
      for (unsigned int param_offset = 0; param_offset < static_cast<unsigned int>(XSLT_Sort::Attr_LAST); param_offset++)
        {
          if (!(sortstate->parameter_set_mask & (0x1 << param_offset)))
            {
              TempBuffer buffer; ANCHOR (TempBuffer, buffer);

              switch (static_cast<XSLT_Sort::Attr>(param_offset))
                {
                case XSLT_Sort::Attr_Order:
                  if (sort->order.IsSpecified ())
                    SetParamValue(static_cast<XSLT_Sort::Attr>(param_offset), XSLT_AttributeValueTemplate::UnescapeL(sort->order, buffer), sortstate);
                  else
                    sortstate->order = SORT_PARAM_DEFAULT_ORDER;
                  break;
                case XSLT_Sort::Attr_Lang:
                  // ignored.
                  break;
                case XSLT_Sort::Attr_Datatype:
                  if (sort->datatype.IsSpecified ())
                    SetParamValue(static_cast<XSLT_Sort::Attr>(param_offset), XSLT_AttributeValueTemplate::UnescapeL(sort->datatype, buffer), sortstate);
                  else
                    sortstate->datatype = SORT_PARAM_DEFAULT_DATATYPE;
                  break;
                case XSLT_Sort::Attr_Caseorder:
                  if (sort->caseorder.IsSpecified ())
                    SetParamValue(static_cast<XSLT_Sort::Attr>(param_offset), XSLT_AttributeValueTemplate::UnescapeL(sort->caseorder, buffer), sortstate);
                  else
                    sortstate->caseorder = SORT_PARAM_DEFAULT_CASEORDER;
                  break;
                }
              sortstate->parameter_set_mask |= (0x1 << param_offset);
              sortstate->parameters_set++;
            }
        }
      OP_ASSERT(sortstate->parameters_set == 4);
      sort = sort->next;
      if (!sort)
        break; // Everything set.
      InitSortStateL (sortstate->next, engine);
      sortstate = sortstate->next;
    }
}


void XSLT_SortState::ClearSortData()
{
  if (evaluate)
    XPathExpression::Evaluate::Free(evaluate);
  evaluate = NULL;
  OP_DELETEA (indeces);
  indeces = NULL;
  OP_DELETEA (unsortednodes);
  unsortednodes = NULL;
  OP_DELETEA (textkeys);
  textkeys = NULL;
  OP_DELETEA (numberkeys);
  numberkeys = NULL;
  has_calculated_sortvalues = FALSE;
}

XSLT_SortState::~XSLT_SortState()
{
  if (evaluate)
    XPathExpression::Evaluate::Free(evaluate);
  OP_DELETEA (unsortednodes);
  OP_DELETEA (textkeys);
  OP_DELETEA (numberkeys);
  OP_DELETEA (indeces);

  OP_DELETE (next);
}

#endif // XSLT_SUPPORT
