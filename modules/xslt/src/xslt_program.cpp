/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_program.h"

#include "modules/xmlutils/xmlnames.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_instruction.h"
#ifdef _DEBUG
#include "modules/xslt/src/xslt_variable.h"
#endif // _DEBUG

XSLT_Program::XSLT_Program (Type type)
  : type (type),
    mode (0),
#ifdef XSLT_ERRORS
    variable (0),
#endif // XSLT_ERRORS
    instructions (0),
    instructions_count (0),
    strings (0),
    strings_count (0),
    names (0),
    names_count (0),
    expressions (0),
    expressions_count (0),
    patterns (0),
    patterns_count (0),
    nsdeclarations (0),
    nsdeclarations_count (0),
    programs (NULL),
    programs_count (0)
{
#ifdef XSLT_PROGRAM_DUMP_SUPPORT
  program_has_been_dumped = FALSE;
#endif // XSLT_PROGRAM_DUMP_SUPPORT
}

XSLT_Program::~XSLT_Program ()
{
  unsigned index;

  OP_DELETE (mode);

  OP_DELETEA (instructions);

  for (index = 0; index < strings_count; ++index)
    OP_DELETEA (strings[index]);
  OP_DELETEA (strings);

  for (index = 0; index < names_count; ++index)
    OP_DELETE (names[index]);
  OP_DELETEA (names);

  for (index = 0; index < expressions_count; ++index)
    OP_DELETE (expressions[index]);
  OP_DELETEA (expressions);

  OP_DELETEA (patterns); // The actual objects owned by someone else

  OP_DELETEA (nsdeclarations); // The actual objects owned by someone else?

  OP_DELETEA (programs); // programs owned by someone else
}

OP_STATUS
XSLT_Program::SetMode (const XMLExpandedName &new_mode)
{
  if (!(mode = OP_NEW (XMLExpandedName, ())) || OpStatus::IsMemoryError (mode->Set (new_mode)))
    {
      OP_DELETE (mode);
      mode = 0;

      return OpStatus::ERR_NO_MEMORY;
    }

  return OpStatus::OK;
}

#ifdef XSLT_PROGRAM_DUMP_SUPPORT

const char *
InstructionToString (XSLT_Instruction::Code code)
{
  switch (code)
    {
    case XSLT_Instruction::IC_EVALUATE_TO_STRING: return "IC_EVALUATE_TO_STRING";
      /**< Evaluate an XPath expression to a string and add the result to the
         string buffer and make the string buffer the current string. */

    case XSLT_Instruction::IC_EVALUATE_TO_BOOLEAN: return "IC_EVALUATE_TO_BOOLEAN";
      /**< Evaluate an XPath expression to a boolean. */

    case XSLT_Instruction::IC_EVALUATE_TO_NUMBER: return "IC_EVALUATE_TO_NUMBER";
      /**< Evaluate an XPath expression to a number. */

    case XSLT_Instruction::IC_EVALUATE_TO_NODESET_SNAPSHOT: return "IC_EVALUATE_TO_NODESET_SNAPSHOT";
      /**< Evaluate an XPath expression to a node-set, snapshot style. */

    case XSLT_Instruction::IC_EVALUATE_TO_NODESET_ITERATOR: return "IC_EVALUATE_TO_NODESET_ITERATOR";
      /**< Evaluate an XPath expression to a node-set, iterator style. */

    case XSLT_Instruction::IC_EVALUATE_TO_VARIABLE_VALUE: return "IC_EVALUATE_TO_VARIABLE_VALUE";

    case XSLT_Instruction::IC_APPEND_STRING: return "IC_APPEND_STRING";
      /**< Append constant string to the string buffer and make the string
         buffer the current string. */

    case XSLT_Instruction::IC_SET_STRING: return "IC_SET_STRING";
      /**< Set current string to a constant string. */

    case XSLT_Instruction::IC_CLEAR_STRING: return "IC_CLEAR_STRING";
      /**< Clears the string buffer and sets the current string to an empty
         string. */

    case XSLT_Instruction::IC_SET_NAME: return "IC_SET_NAME";
      /**< Set the current name to a constant name. */

    case XSLT_Instruction::IC_SET_QNAME: return "IC_SET_QNAME";
      /**< Set the QName of the current name to the current string. */

    case XSLT_Instruction::IC_SET_URI: return "IC_SET_URI";
      /**< Set the URI of the current name to the current string. */

    case XSLT_Instruction::IC_RESOLVE_NAME: return "IC_RESOLVE_NAME";
      /**< Set the URI of the current name by resolving the current name's
         QName. */
    case XSLT_Instruction::IC_SET_NAME_FROM_NODE: return "IC_SET_NAME_FROM_NODE";

    case XSLT_Instruction::IC_START_ELEMENT: return "IC_START_ELEMENT";
      /**< Start an element using the current name. */

    case XSLT_Instruction::IC_SUGGEST_NSDECL: return "IC_SUGGEST_NSDECL";
    case XSLT_Instruction::IC_ADD_ATTRIBUTE: return "IC_ADD_ATTRIBUTE";
      /**< Add an attribute using the current name and the current string. */

    case XSLT_Instruction::IC_ADD_TEXT: return "IC_ADD_TEXT";
      /**< Add a text node using the current string. */

    case XSLT_Instruction::IC_ADD_COMMENT: return "IC_ADD_COMMENT";
      /**< Add a comment node using the current string. */

    case XSLT_Instruction::IC_ADD_PROCESSING_INSTRUCTION: return "IC_ADD_PROCESSING_INSTRUCTION";
      /**< Add a processing instruction node using the current name's
         localpart as the target and the current string as the data. */

    case XSLT_Instruction::IC_END_ELEMENT: return "IC_END_ELEMENT";
      /**< End the current element. */

    case XSLT_Instruction::IC_ADD_COPY_OF_EVALUATE: return "IC_ADD_COPY_OF_EVALUATE";
    case XSLT_Instruction::IC_ADD_COPY_AND_JUMP_IF_NO_END_ELEMENT: return "IC_ADD_COPY_AND_JUMP_IF_NO_END_ELEMENT";

    case XSLT_Instruction::IC_MATCH_PATTERNS: return "IC_MATCH_PATTERNS";
      /**< Match current node against patterns. The argument is
         consists of two parts, 16 bits (low bits) pattern index and
         16 bits (high bits) pattern count */

    case XSLT_Instruction::IC_SEARCH_PATTERNS: return "IC_SEARCH_PATTERNS";
    case XSLT_Instruction::IC_COUNT_PATTERNS_AND_ADD: return "IC_COUNT_PATTERNS_AND_ADD";

    case XSLT_Instruction::IC_PROCESS_KEY: return "IC_PROCESS_KEY";

    case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODE: return "IC_CALL_PROGRAM_ON_NODE";
    case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODE_NO_SCOPE: return "IC_CALL_PROGRAM_ON_NODE_NO_SCOPE";
    case XSLT_Instruction::IC_APPLY_IMPORTS_ON_NODE: return "IC_APPLY_IMPORTS_ON_NODE";
    case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODES: return "IC_CALL_PROGRAM_ON_NODES";
    case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODES_NO_SCOPE: return "IC_CALL_PROGRAM_ON_NODES_NO_SCOPE";

    case XSLT_Instruction::IC_APPLY_TEMPLATES: return "IC_APPLY_TEMPLATES";
    case XSLT_Instruction::IC_APPLY_BUILTIN_TEMPLATE: return "IC_APPLY_BUILTIN_TEMPLATE";

    case XSLT_Instruction::IC_JUMP: return "IC_JUMP";
      /**< Jump unconditionally. */

    case XSLT_Instruction::IC_JUMP_IF_TRUE: return "IC_JUMP_IF_TRUE";
      /**< Jump if the last pattern matched or the last expression evaluated
         to true. */

    case XSLT_Instruction::IC_JUMP_IF_FALSE: return "IC_JUMP_IF_FALSE";
      /**< Jump if the last pattern didn't match or the last expression
         evaluated to false. */

    case XSLT_Instruction::IC_START_COLLECT_TEXT: return "IC_START_COLLECT_TEXT";
      /**< Set an output handler that collects text into the string buffer. */

    case XSLT_Instruction::IC_END_COLLECT_TEXT: return "IC_END_COLLECT_TEXT";
      /**< Restore the previous output handler and make the string buffer the
         current string. */

    case XSLT_Instruction::IC_START_COLLECT_RESULTTREEFRAGMENT: return "IC_START_COLLECT_RESULTTREEFRAGMENT";
      /**< Set an output handler that collects a result tree fragment. */

    case XSLT_Instruction::IC_END_COLLECT_RESULTTREEFRAGMENT: return "IC_END_COLLECT_RESULTTREEFRAGMENT";
      /**< Restore the previous output handler. */

    case XSLT_Instruction::IC_SORT: return "IC_SORT";
      /**< Sort current node-set. */

    case XSLT_Instruction::IC_SET_SORT_PARAMETER_FROM_STRING: return "IC_SET_SORT_PARAMETER_FROM_STRING";

    case XSLT_Instruction::IC_ADD_FORMATTED_NUMBER: return "IC_ADD_FORMATTED_NUMBER";

    case XSLT_Instruction::IC_TEST_AND_SET_IF_PARAM_IS_PRESET: return "IC_TEST_AND_SET_IF_PARAM_IS_PRESET";

    case XSLT_Instruction::IC_SET_VARIABLE_FROM_EVALUATE: return "IC_SET_VARIABLE_FROM_EVALUATE";
      /**< Set the varaible to the contents of the current
         evaluate. Argument is a pointer to a XSLT_Variable. */

    case XSLT_Instruction::IC_SET_VARIABLE_FROM_STRING: return "IC_SET_VARIABLE_FROM_STRING";

    case XSLT_Instruction::IC_SET_WITH_PARAM_FROM_EVALUATE: return "IC_SET_WITH_PARAM_FROM_EVALUATE";

    case XSLT_Instruction::IC_SET_WITH_PARAM_FROM_STRING: return "IC_SET_WITH_PARAM_FROM_STRING";

    case XSLT_Instruction::IC_SET_VARIABLE_FROM_COLLECTED: return "IC_SET_VARIABLE_FROM_COLLECTED";

    case XSLT_Instruction::IC_SET_WITH_PARAM_FROM_COLLECTED: return "IC_SET_WITH_PARAM_FROM_COLLECTED";

    case XSLT_Instruction::IC_START_COLLECT_PARAMS: return "IC_START_COLLECT_PARAMS";

    case XSLT_Instruction::IC_RESET_PARAMS_COLLECTED_FOR_CALL: return "IC_RESET_PARAMS_COLLECTED_FOR_CALL";

    case XSLT_Instruction::IC_SEND_MESSAGE: return "IC_SEND_MESSAGE";

    case XSLT_Instruction::IC_ERROR: return "IC_ERROR";
      /**< Error message as argument. Should cause an error and abort
         if executed. */

    case XSLT_Instruction::IC_RETURN: return "IC_RETURN";
      /**< End program when this is encountered even if there are more instructions in it. */
    }


  OP_ASSERT(!"Should not be reachable");
  return "Unknown Instruction";
}

static void AppendName(OpString8& buf, XMLCompleteName name)
{
  buf.Append("name: '");
  OpString8 buf8;
  if (name.GetPrefix())
    {
      buf8.SetUTF8FromUTF16(name.GetPrefix());
      buf.AppendFormat("%s:", buf8.CStr());
    }
  buf8.SetUTF8FromUTF16(name.GetLocalPart());
  buf.AppendFormat("%s", buf8.CStr());
  if (name.GetUri())
    {
      buf8.SetUTF8FromUTF16(name.GetUri());
      buf.AppendFormat(" (%s)", buf8.CStr());
    }
  buf.Append("'");
}

const char* XSLT_Program::InstructionArgumentsToString(OpString8& buf, unsigned instruction_code, unsigned instruction_argument)
{
  buf.Empty();

  switch(instruction_code)
    {
    case XSLT_Instruction::IC_MATCH_PATTERNS:
    case XSLT_Instruction::IC_SEARCH_PATTERNS:
      {
        // FIXME: Display pattern source
        int first_pattern = static_cast<int>(instruction_argument & 0xffffu);
        int pattern_count = static_cast<int>((instruction_argument >> 16) & 0xffffu);
        OP_ASSERT(pattern_count > 0);
        OP_ASSERT(first_pattern >= 0);
        if (pattern_count == 1)
          buf.AppendFormat("pattern %d", first_pattern);
        else
          buf.AppendFormat("pattern %d-%d, pattern_count: %d", first_pattern, first_pattern+pattern_count-1,pattern_count);
      }
      break;

    case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODE:
    case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODE_NO_SCOPE:
    case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODES:
    case XSLT_Instruction::IC_CALL_PROGRAM_ON_NODES_NO_SCOPE:
      // FIXME: Display program summary
      buf.AppendFormat(" program %d, %s (0x%x)", instruction_argument, programs[instruction_argument]->program_description.CStr(), programs[instruction_argument]);
      break;

    case XSLT_Instruction::IC_EVALUATE_TO_NODESET_SNAPSHOT:
    case XSLT_Instruction::IC_EVALUATE_TO_NODESET_ITERATOR:
    case XSLT_Instruction::IC_EVALUATE_TO_NUMBER:
    case XSLT_Instruction::IC_EVALUATE_TO_BOOLEAN:
    case XSLT_Instruction::IC_EVALUATE_TO_STRING:
    case XSLT_Instruction::IC_EVALUATE_TO_VARIABLE_VALUE:
      {
        OpString8 buf8;
        buf8.SetUTF8FromUTF16(expressions[instruction_argument]->GetSource());
        const char* expression = buf8.IsEmpty() ? "" : buf8.CStr();
        // FIXME: Display Evaluate summary
        buf.AppendFormat("expression %d \"%s\"", instruction_argument, expression);
      }
      break;

    case XSLT_Instruction::IC_JUMP_IF_TRUE:
    case XSLT_Instruction::IC_JUMP_IF_FALSE:
    case XSLT_Instruction::IC_JUMP:
    case XSLT_Instruction::IC_ADD_COPY_AND_JUMP_IF_NO_END_ELEMENT:
      buf.AppendFormat("delta %d", instruction_argument);
      break;

    case XSLT_Instruction::IC_RETURN:
    case XSLT_Instruction::IC_ADD_TEXT:
    case XSLT_Instruction::IC_ADD_COMMENT:
    case XSLT_Instruction::IC_START_ELEMENT:
    case XSLT_Instruction::IC_END_ELEMENT:
    case XSLT_Instruction::IC_SEND_MESSAGE:
    case XSLT_Instruction::IC_START_COLLECT_RESULTTREEFRAGMENT:
    case XSLT_Instruction::IC_END_COLLECT_RESULTTREEFRAGMENT:
    case XSLT_Instruction::IC_ADD_COPY_OF_EVALUATE:
    case XSLT_Instruction::IC_PROCESS_KEY:
    case XSLT_Instruction::IC_START_COLLECT_PARAMS:
    case XSLT_Instruction::IC_RESET_PARAMS_COLLECTED_FOR_CALL:
      // No arguments
      break;

    case XSLT_Instruction::IC_SET_STRING:
    case XSLT_Instruction::IC_APPEND_STRING:
      {
        OpString8 buf8;
        buf8.SetUTF8FromUTF16(strings[instruction_argument]);
        buf.AppendFormat("\"%s\"", buf8.CStr());
      }
      break;

    case XSLT_Instruction::IC_ERROR:
      buf.AppendFormat ("\"%s\"", reinterpret_cast<const char *> (instruction_argument));
      break;

    case XSLT_Instruction::IC_TEST_AND_SET_IF_PARAM_IS_PRESET:
    case XSLT_Instruction::IC_SET_VARIABLE_FROM_EVALUATE:
    case XSLT_Instruction::IC_SET_VARIABLE_FROM_STRING:
    case XSLT_Instruction::IC_SET_WITH_PARAM_FROM_EVALUATE:
    case XSLT_Instruction::IC_SET_WITH_PARAM_FROM_STRING:
    case XSLT_Instruction::IC_SET_VARIABLE_FROM_COLLECTED:
    case XSLT_Instruction::IC_SET_WITH_PARAM_FROM_COLLECTED:
      {
        XSLT_Variable* var = reinterpret_cast<XSLT_Variable*>(instruction_argument);
        buf.AppendFormat("0x%x ", instruction_argument);
        AppendName(buf, var->GetName());
      }
      break;

    case XSLT_Instruction::IC_SET_NAME:
      {
        XMLCompleteName& name = *names[instruction_argument];
        AppendName(buf, name);
      }
      break;

    default:
      buf.AppendFormat("0x%x", instruction_argument);
    }

  if (buf.IsEmpty())
    return "";

  return buf.CStr();

}

void XSLT_Program::DumpInstruction(const XSLT_Instruction& instruction)
{
  OP_NEW_DBG("XSLT_Program::DumpInstruction", "xslt_program_trace");
  OpString8 buf;
  OP_DBG(("%3d %s: %s", static_cast<int>(&instruction - instructions), InstructionToString(instruction.code), InstructionArgumentsToString(buf, instruction.code, instruction.argument)));
}

void XSLT_Program::DumpProgram(unsigned marked_instruction_index)
{
  OP_ASSERT(marked_instruction_index == (unsigned)-1 || marked_instruction_index <= instructions_count);
  OP_NEW_DBG("XSLT_Program::DumpProgram", "xslt_program_dump");

  if (program_has_been_dumped)
    {
      OP_DBG(("Skipping dump of program at address 0x%x (%s)", this, program_description.CStr()));
      return;
    }

  OP_DBG(("Dumping program at address 0x%x (%s)", this, program_description.CStr()));
  OP_DBG(("-------------------------------------"));
  for (unsigned i = 0; i < instructions_count; i++)
    {
      OpString8 buf;
      OP_DBG(("%s%s: %s", (i == marked_instruction_index ? "--> " : "    "), InstructionToString(instructions[i].code), InstructionArgumentsToString(buf, instructions[i].code, instructions[i].argument)));
    }
  OP_DBG(("-------------------------------------"));

  program_has_been_dumped = TRUE;
}

#endif // XSLT_PROGRAM_DUMP_SUPPORT
#endif // XSLT_SUPPORT
