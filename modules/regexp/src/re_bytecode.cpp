/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/regexp/src/re_bytecode.h"
#include "modules/regexp/src/re_tree.h"

static void
RE_Disassemble (OpString &target, const uni_char *source, unsigned source_length, unsigned *bytecode, unsigned bytecode_length, const uni_char **strings, unsigned *string_lengths, RE_Class **classes)
{
#undef instruction_names

  static const uni_char *instruction_names[] =
    {
      UNI_L ("MATCH_PERIOD"),
      UNI_L ("MATCH_CHARACTER_CS"),
      UNI_L ("MATCH_CHARACTER_CI"),
      UNI_L ("MATCH_CHARACTER_CI_SLOW"),
      UNI_L ("MATCH_STRING_CS"),
      UNI_L ("MATCH_STRING_CI"),
      UNI_L ("MATCH_CLASS"),
      UNI_L ("MATCH_CAPTURE"),

      UNI_L ("ASSERT_LINE_START"),
      UNI_L ("ASSERT_LINE_END"),
      UNI_L ("ASSERT_WORD_EDGE"),
      UNI_L ("ASSERT_NOT_WORD_EDGE"),

      UNI_L ("PUSH_CHOICE"),
      UNI_L ("PUSH_CHOICE_MARK"),
      UNI_L ("POP_CHOICE_MARK"),

      UNI_L ("START_LOOP"),
      UNI_L ("LOOP_GREEDY"),
      UNI_L ("LOOP"),

      UNI_L ("LOOP_PERIOD"),
      UNI_L ("LOOP_CHARACTER_CS"),
      UNI_L ("LOOP_CHARACTER_CI"),
      UNI_L ("LOOP_CLASS"),

      UNI_L ("JUMP"),

      UNI_L ("CAPTURE_START"),
      UNI_L ("CAPTURE_END"),

      UNI_L ("RESET_LOOP"),

      UNI_L ("SUCCESS"),
      UNI_L ("FAILURE")
    };

  OpString sources;
  sources.Set (source, source_length);

  target.AppendFormat (UNI_L ("Source:\n  %s\n\n"), sources.CStr ());
  target.Append ("Program:\n");

#if 0
  BytecodeSegment *bcs = bytecode_segments;
  unsigned bcs_index = 1;
  if (bcs)
    while (BytecodeSegment *pbcs = bcs->previous)
      bcs = pbcs;
#endif // 0

  unsigned index = 0;

  while (index < bytecode_length)
    {
      unsigned argument = bytecode[index];
      RE_Instructions::Instruction instruction = (RE_Instructions::Instruction) (argument & 0xffu);
      argument >>= 8;

#if 0
      if (bcs && bcs->start_index == index)
        {
          target.AppendFormat (UNI_L ("segment_%u:\n"), bcs_index++);
          bcs = bcs->next;
        }
#endif // 0

      target.AppendFormat (UNI_L ("  %u: %s"), index, instruction_names[instruction]);

      ++index;

      int offset;
      unsigned loop_index;
      unsigned min;
      unsigned max;
      OpString string;

      switch (instruction)
        {
        case RE_Instructions::MATCH_PERIOD:
          break;

        case RE_Instructions::MATCH_CHARACTER_CS:
          target.AppendFormat (UNI_L (", character(%u)"), argument);

          if (argument >= 32 && argument <= 127)
            target.AppendFormat (UNI_L (" = '%c'"), argument);
          break;

        case RE_Instructions::MATCH_CHARACTER_CI:
          argument = bytecode[index] & 0xffffu;
          target.AppendFormat (UNI_L (", character(%u)"), argument);
          if (argument >= 32 && argument <= 127)
            target.AppendFormat (UNI_L (" = '%c'"), argument);

          argument = bytecode[index] >> 16;
          target.AppendFormat (UNI_L (", character(%u)"), argument);
          if (argument >= 32 && argument <= 127)
            target.AppendFormat (UNI_L (" = '%c'"), argument);

          ++index;
          break;

        case RE_Instructions::MATCH_CHARACTER_CI_SLOW:
          target.AppendFormat (UNI_L (", character(%u)"), argument);

          if (argument >= 32 && argument <= 127)
            target.AppendFormat (UNI_L (" = '%c'"), argument);
          break;

        case RE_Instructions::MATCH_STRING_CS:
        case RE_Instructions::MATCH_STRING_CI:
          string.Set (strings[argument], string_lengths[argument]);
          target.AppendFormat (UNI_L (", string(\"%s\")"), string.CStr ());
          break;

        case RE_Instructions::MATCH_CLASS:
          target.AppendFormat (UNI_L (", class(%u)"), argument);
          break;

        case RE_Instructions::MATCH_CAPTURE:
          target.AppendFormat (UNI_L (", capture(%u)"), argument);
          break;

        case RE_Instructions::ASSERT_LINE_START:
        case RE_Instructions::ASSERT_LINE_END:
        case RE_Instructions::ASSERT_WORD_EDGE:
        case RE_Instructions::ASSERT_NOT_WORD_EDGE:
          break;

        case RE_Instructions::POP_CHOICE_MARK:
          break;

        case RE_Instructions::PUSH_CHOICE:
        case RE_Instructions::PUSH_CHOICE_MARK:
        case RE_Instructions::JUMP:
          target.AppendFormat (UNI_L (", offset(%i) => %u"), (int) argument, (unsigned) ((int) index + (int) argument));
          break;

        case RE_Instructions::LOOP_GREEDY:
        case RE_Instructions::LOOP:
          offset = (int) bytecode[index++];

          target.AppendFormat (UNI_L (", index=%u, offset(%i) => %u"), argument, offset, (unsigned) ((int) index + offset));
          break;

        case RE_Instructions::START_LOOP:
          min = bytecode[index++];
          max = bytecode[index++];

          target.AppendFormat (UNI_L (", index=%u, min=%u"), argument, min);

          if (max == UINT_MAX)
            target.AppendFormat (UNI_L (", max=INFINITY"));
          else
            target.AppendFormat (UNI_L (", max=%u"), max);

          break;

        case RE_Instructions::LOOP_PERIOD:
          min = bytecode[index++];
          max = bytecode[index++];

          target.AppendFormat (UNI_L (", index=%u, min=%u"), argument, min);

          if (max == UINT_MAX)
            target.AppendFormat (UNI_L (", max=INFINITY"));
          else
            target.AppendFormat (UNI_L (", max=%u"), max);

          break;

        case RE_Instructions::LOOP_CHARACTER_CS:
        case RE_Instructions::LOOP_CHARACTER_CI:
          loop_index = bytecode[index++];
          min = bytecode[index++];
          max = bytecode[index++];

          target.AppendFormat (UNI_L (", character(%u)"), argument);

          if (argument >= 32 && argument <= 127)
            target.AppendFormat (UNI_L (" = '%c'"), argument);

          target.AppendFormat (UNI_L (", index=%u, min=%u"), loop_index, min);

          if (max == UINT_MAX)
            target.AppendFormat (UNI_L (", max=INFINITY"));
          else
            target.AppendFormat (UNI_L (", max=%u"), max);

          break;

        case RE_Instructions::LOOP_CLASS:
          loop_index = bytecode[index++];
          min = bytecode[index++];
          max = bytecode[index++];

          target.AppendFormat (UNI_L (", class(%u), index=%u, min=%u"), argument, loop_index, min);

          if (max == UINT_MAX)
            target.AppendFormat (UNI_L (", max=INFINITY"));
          else
            target.AppendFormat (UNI_L (", max=%u"), max);

          break;

        case RE_Instructions::CAPTURE_START:
          target.AppendFormat (UNI_L (", capture(%u)"), argument >> 1);
          if ((argument & 1) == 0)
            target.Append (UNI_L (", reset"));
          break;

        case RE_Instructions::CAPTURE_END:
          target.AppendFormat (UNI_L (", capture(%u)"), argument);
          break;

        case RE_Instructions::RESET_LOOP:
          target.AppendFormat (UNI_L (", index=%u"), argument);
          break;

        case RE_Instructions::SUCCESS:
        case RE_Instructions::FAILURE:
          break;
        }

      target.AppendFormat (UNI_L ("\n"));
    }

  target.AppendFormat (UNI_L ("\n"));
}

void
RE_BytecodeCompiler::Write (unsigned op)
{
  if (bytecode_used == bytecode_allocated)
    {
      unsigned new_bytecode_allocated = bytecode_allocated == 0 ? 64 : bytecode_allocated + bytecode_allocated;
      unsigned *new_bytecode = OP_NEWGROA_L (unsigned, new_bytecode_allocated, arena);

      op_memcpy (new_bytecode, bytecode, bytecode_used * sizeof *bytecode);

      bytecode = new_bytecode;
      bytecode_allocated = new_bytecode_allocated;
    }

  bytecode[bytecode_used++] = op;
}

RE_BytecodeCompiler::JumpTarget
RE_BytecodeCompiler::AddJumpTarget (unsigned target)
{
  if (targets_used == targets_allocated)
    {
      unsigned new_targets_allocated = targets_allocated == 0 ? 8 : targets_allocated + targets_allocated;
      unsigned *new_targets = OP_NEWGROA_L (unsigned, new_targets_allocated, arena);

      op_memcpy (new_targets, targets, targets_used * sizeof *targets);

      targets = new_targets;
      targets_allocated = new_targets_allocated;
    }

  unsigned *targetp = &targets[targets_used];

  *targetp = target;

  return JumpTarget (targets_used++, targetp);
}

RE_BytecodeCompiler::RE_BytecodeCompiler (OpMemGroup *arena)
  : arena (arena),
    bytecode (0),
    bytecode_used (0),
    bytecode_allocated (0),
    targets (0),
    targets_used (0),
    targets_allocated (0),
    strings (0),
    alternative_strings (0),
    string_lengths (0),
    strings_used (0),
    strings_allocated (0),
    classes (0),
    classes_used (0),
    classes_allocated (0)
{
}

void
RE_BytecodeCompiler::Instruction (RE_Instructions::Instruction instruction, unsigned argument)
{
  Write (static_cast<unsigned> (instruction) | (argument << 8));
}

void
RE_BytecodeCompiler::Operand (unsigned value)
{
  Write (value);
}

unsigned
RE_BytecodeCompiler::AddString (const uni_char *string, unsigned string_length, BOOL case_insensitive)
{
  if (strings_used == strings_allocated)
    {
      unsigned new_strings_allocated = strings_allocated == 0 ? 8 : strings_allocated + strings_allocated;
      const uni_char **new_strings = OP_NEWGROA_L (const uni_char *, new_strings_allocated, arena);
      unsigned *new_string_lengths = OP_NEWGROA_L (unsigned, new_strings_allocated, arena);

      op_memcpy (new_strings, strings, strings_used * sizeof *strings);
      op_memcpy (new_string_lengths, string_lengths, strings_used * sizeof *string_lengths);

      strings = new_strings;
      string_lengths = new_string_lengths;
      strings_allocated = new_strings_allocated;

      if (case_insensitive || alternative_strings)
        {
          const uni_char **new_alternative_strings = OP_NEWGROA_L (const uni_char *, new_strings_allocated, arena);

          if (alternative_strings)
            op_memcpy (new_alternative_strings, alternative_strings, strings_used * sizeof *alternative_strings);
          else
            op_memset (new_alternative_strings, 0, strings_used * sizeof *alternative_strings);

          alternative_strings = new_alternative_strings;
        }
    }

  if (case_insensitive)
    {
      uni_char *data = OP_NEWA_L (uni_char, string_length + string_length), *prime = data, *alternative = data + string_length;

      for (unsigned index = 0; index < string_length; ++index)
        {
          int ch = *prime++ = string[index], lower = uni_tolower (ch);
          if (lower != ch)
            *alternative++ = lower;
          else
            *alternative++ = uni_toupper (ch);
        }

      strings[strings_used] = data;
      alternative_strings[strings_used] = data + string_length;
    }
  else
    {
      uni_char *data = OP_NEWA_L (uni_char, string_length);
      op_memcpy (data, string, UNICODE_SIZE (string_length));
      strings[strings_used] = data;
      if (alternative_strings)
        alternative_strings[strings_used] = 0;
    }

  string_lengths[strings_used] = string_length;

  return strings_used++;
}

unsigned
RE_BytecodeCompiler::AddClass (RE_Class *cls)
{
  if (classes_used == classes_allocated)
    {
      unsigned new_classes_allocated = classes_allocated == 0 ? 8 : classes_allocated + classes_allocated;
      RE_Class **new_classes = OP_NEWGROA_L (RE_Class *, new_classes_allocated, arena);

      op_memcpy (new_classes, classes, classes_used * sizeof *classes);

      classes = new_classes;
      classes_allocated = new_classes_allocated;
    }

  unsigned index = classes_used++;
  classes[index] = cls;
  return index;
}


RE_BytecodeCompiler::JumpTarget
RE_BytecodeCompiler::Here ()
{
  return AddJumpTarget (bytecode_used);
}

RE_BytecodeCompiler::JumpTarget
RE_BytecodeCompiler::ForwardJump ()
{
  return AddJumpTarget ();
}

void
RE_BytecodeCompiler::Jump (const JumpTarget &target, RE_Instructions::Instruction instruction, unsigned argument)
{
  OP_ASSERT (instruction == RE_Instructions::PUSH_CHOICE || instruction == RE_Instructions::PUSH_CHOICE_MARK || instruction == RE_Instructions::JUMP || instruction == RE_Instructions::LOOP || instruction == RE_Instructions::LOOP_GREEDY);

  if (instruction == RE_Instructions::JUMP || instruction == RE_Instructions::PUSH_CHOICE || instruction == RE_Instructions::PUSH_CHOICE_MARK)
    Instruction (instruction, target.id);
  else
    {
      Instruction (instruction, argument);
      Operand (target.id);
    }
}

void
RE_BytecodeCompiler::Compile (RE_TreeNode *expression)
{
  expression->GenerateBytecode (*this);

  Write (RE_Instructions::SUCCESS);

#define INSTRUCTION(index) ((RE_Instructions::Instruction) (bytecode[index] & 0xffu))
#define ARGUMENT(index) (bytecode[index] >> 8)

  for (unsigned index = 0, instruction; index < bytecode_used; index += RE_InstructionLengths[instruction])
    {
      unsigned address, id;

      instruction = INSTRUCTION (index);

      if (instruction == RE_Instructions::JUMP || instruction == RE_Instructions::PUSH_CHOICE || instruction == RE_Instructions::PUSH_CHOICE_MARK)
        address = index, id = ARGUMENT (index);
      else if (instruction == RE_Instructions::LOOP || instruction == RE_Instructions::LOOP_GREEDY)
        address = index + 1, id = bytecode[address];
      else
        continue;

      if (instruction == RE_Instructions::JUMP && INSTRUCTION (targets[id]) == RE_Instructions::SUCCESS)
        bytecode[index] = RE_Instructions::SUCCESS;
      else
        {
          int offset = static_cast<int> (targets[id]) - static_cast<int> (index + RE_InstructionLengths[instruction]);

          if (address == index)
            bytecode[address] = instruction | (offset << 8);
          else
            bytecode[address] = offset;
        }
    }
}

#ifdef RE_FEATURE__DISASSEMBLER

void
RE_BytecodeCompiler::Disassemble (OpString &output, const uni_char *source, unsigned source_length)
{
  RE_Disassemble (output, source, source_length, bytecode, bytecode_used, strings, string_lengths, classes);
}

#endif // RE_FEATURE__DISASSEMBLER
