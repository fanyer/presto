/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_compiler.h"

#include "modules/xslt/src/xslt_program.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_sort.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_utils.h"

#ifdef XSLT_ERRORS
void
XSLT_Compiler::AddInstructionL (XSLT_Instruction::Code code, UINTPTR *&argument_ptr, XSLT_Element *origin)
#else // XSLT_ERRORS
void
XSLT_Compiler::AddInstructionL (XSLT_Instruction::Code code, UINTPTR *&argument_ptr)
#endif // XSLT_ERRORS
{
  if (instructions_used == instructions_total)
    {
      unsigned new_instructions_total = instructions_total == 0 ? 64 : instructions_total + instructions_total;
      XSLT_Instruction *new_instructions = OP_NEWA_L (XSLT_Instruction, new_instructions_total);

      op_memcpy (new_instructions, instructions, instructions_used * sizeof instructions[0]);
      OP_DELETEA (instructions);

      instructions = new_instructions;
      instructions_total = new_instructions_total;
    }

  instructions[instructions_used].code = code;
  instructions[instructions_used].argument = instructions_used;
#ifdef XSLT_ERRORS
  instructions[instructions_used].origin = origin;
#endif // XSLT_ERRORS

  argument_ptr = &instructions[instructions_used].argument;
  ++instructions_used;
}

XSLT_Compiler::XSLT_Compiler (XSLT_StylesheetImpl *stylesheet, XSLT_MessageHandler *messagehandler)
  : stylesheet (stylesheet),
    messagehandler (messagehandler),
    instructions (0),
    instructions_used (0),
    instructions_total (0),
    strings (0),
    strings_used (0),
    strings_total (0),
    names (0),
    names_used (0),
    names_total (0),
    expressions (0),
    expressions_used (0),
    expressions_total (0),
    patterns (0),
    patterns_used (0),
    patterns_total (0),
    nsdeclarations (0),
    nsdeclarations_used (0),
    nsdeclarations_total (0),
    programs (0),
    programs_used (0),
    programs_total (0)
{
}

XSLT_Compiler::~XSLT_Compiler ()
{
  /* This is actually only used if the compilation was aborted.  Otherwise all
     objects should have been transferred to a XSLT_Program. */
  unsigned index;

  OP_DELETEA (instructions);

  for (index = 0; index < strings_used; ++index)
    OP_DELETEA (strings[index]);
  OP_DELETEA (strings);

  for (index = 0; index < names_used; ++index)
    OP_DELETE (names[index]);
  OP_DELETEA (names);

  for (index = 0; index < expressions_used; ++index)
    OP_DELETE (expressions[index]);
  OP_DELETEA (expressions);

  OP_DELETEA (patterns);
  OP_DELETEA (nsdeclarations);
  OP_DELETEA (programs);
}

#ifdef XSLT_ERRORS

void
XSLT_Compiler::AddInstructionL (XSLT_Instruction::Code code, UINTPTR argument, XSLT_Element *origin)
{
  UINTPTR *argument_ptr;
  AddInstructionL (code, argument_ptr, origin);
  *argument_ptr = argument;
}

unsigned
XSLT_Compiler::AddJumpInstructionL (XSLT_Instruction::Code code, XSLT_Element *origin)
{
  UINTPTR *dummy_argument_ptr;
  AddInstructionL (code, dummy_argument_ptr, origin);
  return instructions_used - 1;
}

#else // XSLT_ERRORS

void
XSLT_Compiler::AddInstructionL (XSLT_Instruction::Code code, UINTPTR argument)
{
  UINTPTR *argument_ptr;
  AddInstructionL (code, argument_ptr);
  *argument_ptr = argument;
}

unsigned
XSLT_Compiler::AddJumpInstructionL (XSLT_Instruction::Code code)
{
  UINTPTR *dummy_argument_ptr;
  AddInstructionL (code, dummy_argument_ptr);
  return instructions_used - 1;
}

#endif // XSLT_ERRORS

unsigned
XSLT_Compiler::AddStringL (const uni_char *string)
{
  for (unsigned index = 0; index < strings_used; ++index)
    if (uni_strcmp (string, strings[index]) == 0)
      return index;

  void **strings0 = reinterpret_cast<void **> (strings);
  XSLT_Utils::GrowArrayL (&strings0, strings_used, strings_used+1, strings_total);
  strings = reinterpret_cast<uni_char **> (strings0);

  if (!(strings[strings_used] = UniSetNewStr (string)))
    LEAVE (OpStatus::ERR_NO_MEMORY);

  return strings_used++;
}

unsigned
XSLT_Compiler::AddNameL (const XMLCompleteNameN &name, BOOL process_namespace_aliases)
{
  XMLCompleteName *new_name = OP_NEW_L (XMLCompleteName, ());
  OpStackAutoPtr<XMLCompleteName> new_name_anchor (new_name);

  new_name->SetL (name);

  if (process_namespace_aliases)
    stylesheet->ProcessNamespaceAliasesL (*new_name);

  for (unsigned index = 0; index < names_used; ++index)
    if (*new_name == *names[index])
      return index;

  void **names0 = reinterpret_cast<void **> (names);
  XSLT_Utils::GrowArrayL (&names0, names_used, names_used + 1, names_total);
  names = reinterpret_cast<XMLCompleteName **> (names0);

  names[names_used] = new_name;
  new_name_anchor.release ();

  return names_used++;
}

unsigned
XSLT_Compiler::AddExpressionL (const XSLT_String& source, XPathExtensions* extensions, XMLNamespaceDeclaration *nsdeclaration)
{
  void **expressions0 = reinterpret_cast<void **> (expressions);
  XSLT_Utils::GrowArrayL (&expressions0, expressions_used, expressions_used+1, expressions_total);
  expressions = reinterpret_cast<XSLT_XPathExpression **> (expressions0);

  OpStackAutoPtr<XSLT_XPathExpression> expression (OP_NEW_L (XSLT_XPathExpression, (extensions, nsdeclaration)));
  expression->SetSourceL (source);
  expressions[expressions_used] = expression.release ();
  return expressions_used++;
}

unsigned
XSLT_Compiler::AddPatternsL (XPathPattern **new_patterns, unsigned patterns_count)
{
  OP_ASSERT(patterns_count > 0);

  void **patterns0 = reinterpret_cast<void **> (patterns);
  XSLT_Utils::GrowArrayL (&patterns0, patterns_used, patterns_used + patterns_count, patterns_total);
  patterns = reinterpret_cast<XPathPattern **> (patterns0);

  op_memcpy (patterns + patterns_used, new_patterns, patterns_count * sizeof patterns[0]);

#ifdef _DEBUG
  for (unsigned check_index = patterns_used; check_index < patterns_used + patterns_count; check_index++)
    {
      OP_ASSERT(patterns[check_index]);
      OP_ASSERT(patterns[check_index]); // Catch bad data
    }
#endif // _DEBUG

  patterns_used += patterns_count;
  // Create a Pattern argument which consists of two 16 bits halves
  return (patterns_used - patterns_count) | (patterns_count << 16);
}

unsigned
XSLT_Compiler::AddNamespaceDeclarationL (XMLNamespaceDeclaration *nsdeclaration)
{
  for (unsigned index = 0; index < nsdeclarations_used; ++index)
    if (nsdeclaration == nsdeclarations[index])
      return index;

  void **nsdeclarations0 = reinterpret_cast<void **> (nsdeclarations);
  XSLT_Utils::GrowArrayL (&nsdeclarations0, nsdeclarations_used, nsdeclarations_used+1, nsdeclarations_total);
  nsdeclarations = reinterpret_cast<XMLNamespaceDeclaration **> (nsdeclarations0);

  nsdeclarations[nsdeclarations_used] = nsdeclaration;
  return nsdeclarations_used++;
}

unsigned
XSLT_Compiler::AddProgramL (XSLT_Program *program)
{
#ifdef XSLT_PROGRAM_DUMP_SUPPORT
  OP_ASSERT(!program->program_description.IsEmpty());
#endif // XSLT_PROGRAM_DUMP_SUPPORT

  void **programs0 = reinterpret_cast<void **> (programs);
  XSLT_Utils::GrowArrayL (&programs0, programs_used, programs_used+1, programs_total);
  programs = reinterpret_cast<XSLT_Program **> (programs0);

  programs[programs_used] = program;
  return programs_used++;
}

void
XSLT_Compiler::SetJumpDestination (unsigned jump_instruction_offset)
{
  instructions[jump_instruction_offset].argument = instructions_used - jump_instruction_offset;
}

void XSLT_Compiler::FinishL (XSLT_Program *program)
{
  OP_ASSERT(!program->instructions);

#ifdef XSLT_ERRORS
  AddInstructionL (XSLT_Instruction::IC_RETURN, 0, 0);
#else // XSLT_ERRORS
  AddInstructionL (XSLT_Instruction::IC_RETURN, 0);
#endif // XSLT_ERRORS

  program->instructions = OP_NEWA_L (XSLT_Instruction, instructions_used);
  program->instructions_count = instructions_used;
  op_memcpy (program->instructions, instructions, instructions_used * sizeof instructions[0]);
  instructions_used = 0;

  OP_ASSERT(!program->strings);
  if (strings_used)
    {
      program->strings = OP_NEWA_L (uni_char *, strings_used);
      program->strings_count = strings_used;
      op_memcpy (program->strings, strings, strings_used * sizeof strings[0]);
      strings_used = 0;
    }

  OP_ASSERT(!program->names);
  if (names_used)
    {
      program->names = OP_NEWA_L (XMLCompleteName *, names_used);
      program->names_count = names_used;
      op_memcpy (program->names, names, names_used * sizeof names[0]);
      names_used = 0;
    }

  OP_ASSERT(!program->expressions);
  if (expressions_used)
    {
      program->expressions = OP_NEWA_L (XSLT_XPathExpression *, expressions_used);
      program->expressions_count = expressions_used;
      op_memcpy (program->expressions, expressions, expressions_used * sizeof expressions[0]);
      expressions_used = 0;
    }

  OP_ASSERT(!program->patterns);
  if (patterns_used)
    {
      program->patterns = OP_NEWA_L (XPathPattern *, patterns_used);
      program->patterns_count = patterns_used;
      op_memcpy (program->patterns, patterns, patterns_used * sizeof patterns[0]);
      patterns_used = 0;
    }

  OP_ASSERT(!program->nsdeclarations);
  if (nsdeclarations_used)
    {
      program->nsdeclarations = OP_NEWA_L (XMLNamespaceDeclaration *, nsdeclarations_used);
      program->nsdeclarations_count = nsdeclarations_used;
      op_memcpy (program->nsdeclarations, nsdeclarations, nsdeclarations_used * sizeof nsdeclarations[0]);
      nsdeclarations_used = 0;
    }

  OP_ASSERT(!program->programs);
  if (programs_used)
    {
      program->programs = OP_NEWA_L (XSLT_Program *, programs_used);
      program->programs_count = programs_used;
      op_memcpy (program->programs, programs, programs_used * sizeof programs[0]);
      programs_used = 0;
    }
}

#endif // XSLT_SUPPORT
