/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/regexp/src/re_config.h"

#ifdef RE_FEATURE__MACHINE_CODED

#include "modules/regexp/src/re_native.h"
#include "modules/regexp/src/re_matcher.h"

#include "modules/memory/src/memory_executable.h"

RE_Native::RE_Native (RE_Object *object, OpExecMemoryManager *executable_memory)
  : object (object)
  , executable_memory (executable_memory)
  , cg (&arena)
  , case_insensitive (object->IsCaseInsensitive ())
  , multiline (object->IsMultiline ())
  , searching (object->IsSearching ())
  , is_backtracking (false)
  , forward_jumps (0)
{
}

RE_Native::~RE_Native ()
{
}

#define INSTRUCTION(index) ((RE_Instructions::Instruction) (bytecode[index] & 0xffu))
#define ARGUMENT(index) (bytecode[index] >> 8)

#define MAX_RECURSION_LEVEL 16

static bool
IsFixedLength (bool &result, RE_Object *object, const unsigned *bytecode, unsigned &index, unsigned &length, unsigned &instructions_count, bool &has_slow_instructions, bool &has_loop, bool &has_assertions, unsigned level = 0, unsigned loop_index = ~0u)
{
  result = true;

  length = 0;
  instructions_count = 0;
  has_slow_instructions = false;
  has_loop = false;

  if (++level == MAX_RECURSION_LEVEL)
    return false;

  while (INSTRUCTION (index) != RE_Instructions::SUCCESS)
    {
      unsigned argument = ARGUMENT (index), instruction_length = RE_InstructionLengths[INSTRUCTION (index)];

      ++instructions_count;

      switch (INSTRUCTION (index))
        {
        case RE_Instructions::MATCH_PERIOD:
        case RE_Instructions::MATCH_CHARACTER_CS:
        case RE_Instructions::MATCH_CHARACTER_CI:
        case RE_Instructions::MATCH_CHARACTER_CI_SLOW:
          length += 1;
          break;

        case RE_Instructions::MATCH_CLASS:
          has_slow_instructions = true;
          length += 1;
          break;

        case RE_Instructions::MATCH_STRING_CS:
        case RE_Instructions::MATCH_STRING_CI:
          length += object->GetStringLengths ()[argument];
          break;

        case RE_Instructions::MATCH_CAPTURE:
          result = false;
          break;

        case RE_Instructions::ASSERT_LINE_START:
        case RE_Instructions::ASSERT_LINE_END:
        case RE_Instructions::ASSERT_WORD_EDGE:
        case RE_Instructions::ASSERT_NOT_WORD_EDGE:
          has_assertions = true;
          break;

        case RE_Instructions::START_LOOP:
          {
            unsigned min = bytecode[index + 1], max = bytecode[index + 2], quantified_length, quantified_instructions_count;

            if (min != max)
              result = false;

            index += instruction_length;

            bool quantified_slow_instructions, quantified_loop, quantified_fixed_length;

            argument >>= 1;

            if (!IsFixedLength (quantified_fixed_length, object, bytecode, index, quantified_length, quantified_instructions_count, quantified_slow_instructions, quantified_loop, has_assertions, level, argument))
              return false;

            if (!quantified_fixed_length)
              result = false;

            length += quantified_length * min;

            if (quantified_slow_instructions)
              has_slow_instructions = true;
          }
          continue;

        case RE_Instructions::LOOP_GREEDY:
        case RE_Instructions::LOOP:
          if (argument == loop_index)
            {
              index += instruction_length;
              return true;
            }
          break;

        case RE_Instructions::LOOP_PERIOD:
          has_loop = true;
          if (bytecode[index + 1] != bytecode[index + 2])
            result = false;
          length += bytecode[index + 1];
          break;

        case RE_Instructions::LOOP_CHARACTER_CS:
        case RE_Instructions::LOOP_CHARACTER_CI:
        case RE_Instructions::LOOP_CLASS:
          has_loop = true;
          if (bytecode[index + 2] != bytecode[index + 3])
            result = false;
          length += bytecode[index + 2];
          break;

        case RE_Instructions::PUSH_CHOICE:
          {
            unsigned alternative1_index = index + instruction_length + argument, alternative1_length, alternative1_instructions_count;
            bool alternative1_slow_instructions, alternative1_loop, alternative1_fixed_length;

            if (!IsFixedLength (alternative1_fixed_length, object, bytecode, alternative1_index, alternative1_length, alternative1_instructions_count, alternative1_slow_instructions, alternative1_loop, has_assertions, level))
              return false;

            if (!alternative1_fixed_length)
              result = false;

            unsigned alternative2_index = index + instruction_length, alternative2_length, alternative2_instructions_count;
            bool alternative2_slow_instructions, alternative2_loop, alternative2_fixed_length;

            if (!IsFixedLength (alternative2_fixed_length, object, bytecode, alternative2_index, alternative2_length, alternative2_instructions_count, alternative2_slow_instructions, alternative2_loop, has_assertions, level))
              return false;

            if (!alternative2_fixed_length)
              result = false;

            if (result && alternative1_length != alternative2_length)
              result = false;

            length += alternative1_length < alternative2_length ? alternative1_length : alternative2_length;

            has_loop = true;
          }
          return true;

        case RE_Instructions::PUSH_CHOICE_MARK:
        case RE_Instructions::POP_CHOICE_MARK:
          /* While a lookahead doesn't affect the actual length of the match, it
             does complicate things; for instance, the actual length of the
             match might be shorter than the string that was actually used to
             determine if there was a match, and so on. */
          return false;

        case RE_Instructions::JUMP:
          index += argument;
          break;
        }

      index += instruction_length;
    }

  return true;
}

static bool
IsFixedLengthSegment (bool &result, RE_Object *object, unsigned segment_index, unsigned &length)
{
  unsigned index = 0, instructions_count;
  bool has_slow_instructions;
  bool has_loop;
  bool has_assertions = false;

  return IsFixedLength (result, object, object->GetBytecodeSegments ()[segment_index], index, length, instructions_count, has_slow_instructions, has_loop, has_assertions);
}

bool
RE_Native::CreateNativeMatcher (const OpExecMemory *&matcher)
{
  const unsigned *bytecode = object->GetBytecode ();

  if (!multiline && searching && INSTRUCTION (0) == RE_Instructions::ASSERT_LINE_START)
    searching = false;

  unsigned segments_count = object->GetBytecodeSegmentsCount (), segment_index;

  global_fixed_length = true;

  segment_variable_length = false;

  unsigned global_length = 0;

  stack_space_used = 0;

  control_stack_base = reinterpret_cast<unsigned char *>(&global_length);

  for (segment_index = 0; segment_index < segments_count; ++segment_index)
    {
      unsigned length;
      bool is_fixed_length;

      if (!IsFixedLengthSegment (is_fixed_length, object, segment_index, length))
        return false;

      if (!is_fixed_length)
        {
          global_fixed_length = false;
          segment_variable_length = true;
          break;
        }

      if (segment_index == 0)
        global_length = length;
      else if (length != global_length)
        global_fixed_length = false;
    }

  if (global_fixed_length)
    segment_length = global_length;

  EmitGlobalPrologue ();

  for (segment_index = 0; segment_index < segments_count; ++segment_index)
    {
      unsigned index = 0, offset = 0;
      bool is_fixed_length;

      if (global_fixed_length)
        is_fixed_length = true;
      else if (!IsFixedLengthSegment (is_fixed_length, object, segment_index, segment_length))
        return false;

      segment_fixed_length = global_fixed_length || is_fixed_length;
      first_capture = last_capture = UINT_MAX;

      EmitSegmentPrologue (segment_index);

      if (!GenerateBlock (object->GetBytecodeSegments ()[segment_index], index, offset, ~0u, jt_segment_success, jt_segment_failure))
        return false;

      if ((segment_fixed_length && !global_fixed_length) || object->GetCaptures () != 0)
        EmitSegmentSuccessEpilogue ();
    }

#ifdef NATIVE_DISASSEMBLER_SUPPORT
  cg.EnableDisassemble ();
#endif // NATIVE_DISASSEMBLER_SUPPORT

  matcher = cg.GetCode (executable_memory);

  Finish (matcher->address);

  cg.Finalize (executable_memory, matcher);

#if 0
  OpString8 s;
  s.Set (object->GetSource ());

  printf ("%p -> %p: /%s/\n", matcher->memory, static_cast<char *> (matcher->memory) + matcher->size, s.CStr ());
#endif // 0

  return true;
}

static bool
AntiMatchesPeriod (const unsigned *bytecode, const uni_char *const *strings1, RE_Class **classes, bool multiline)
{
  unsigned index = 0;

  while (TRUE)
    {
      unsigned argument = ARGUMENT (index);

      switch (INSTRUCTION (index))
        {
        case RE_Instructions::MATCH_CHARACTER_CS:
          return RE_Matcher::IsLineTerminator (argument);

        case RE_Instructions::MATCH_STRING_CS:
        case RE_Instructions::MATCH_STRING_CI:
          return RE_Matcher::IsLineTerminator (strings1[argument][0]);

        case RE_Instructions::MATCH_CLASS:
          {
            RE_Class *cls = classes[argument];

            if (!cls->HasHighCharacterMap () && !cls->IsInverted ())
              {
                for (unsigned ch = 0; ch < BITMAP_RANGE; ++ch)
                  if (cls->Match (ch) && ch != 10 && ch != 13)
                    return false;
                return true;
              }
          }
          return false;

        case RE_Instructions::ASSERT_LINE_END:
          return multiline;

        case RE_Instructions::SUCCESS:
          return true;

        case RE_Instructions::CAPTURE_START:
        case RE_Instructions::CAPTURE_END:
          break;

        default:
          return false;
        }

      index += RE_InstructionLengths[INSTRUCTION (index)];
    }
}

static uni_char
RE_GetAlternativeUniChar (uni_char ch)
{
  return uni_tolower (ch) != ch ? uni_tolower (ch) : uni_toupper (ch);
}

static bool
CharactersAntiMatch (uni_char ch1a, uni_char ch1b, uni_char ch2a, uni_char ch2b)
{
  if (ch1a == ch2a)
    return false;

  if (ch1b && ch2b && ch1a == ch2b || ch1b == ch2a || ch1b == ch2b)
    return false;

  return true;
}

static bool
AntiMatchesCharacter (unsigned ch, const unsigned *bytecode, const uni_char *const *strings1, const uni_char *const *strings2, RE_Class **classes)
{
  unsigned index = 0;

  while (TRUE)
    {
      unsigned argument = ARGUMENT (index);
      uni_char ch1 = ch & 0xffffu, ch2 = ch >> 16;

      switch (INSTRUCTION (index))
        {
        case RE_Instructions::MATCH_PERIOD:
          return ch1 == 10 || ch1 == 13 || ch1 == 0x2028 || ch1 == 0x2029;

        case RE_Instructions::MATCH_CHARACTER_CS:
          return ch1 != argument && (ch2 == 0 || ch2 != argument);

        case RE_Instructions::MATCH_CHARACTER_CI:
          return CharactersAntiMatch (ch1, ch2, bytecode[index + 1] & 0xffffu, bytecode[index + 1] >> 16);

        case RE_Instructions::MATCH_STRING_CS:
          return CharactersAntiMatch (ch1, ch2, strings1[argument][0], 0);

        case RE_Instructions::MATCH_STRING_CI:
          return CharactersAntiMatch (ch1, ch2, strings1[argument][0], strings2[argument][0]);

        case RE_Instructions::MATCH_CLASS:
          return !classes[argument]->Match (ch1) && (ch2 == 0 || !classes[argument]->Match (ch2));

        case RE_Instructions::LOOP_CHARACTER_CS:
          if (argument != ch1 && (ch2 == 0 || argument != ch2))
            return bytecode[index + 2] > 0;
          else
            return false;

        case RE_Instructions::LOOP_CHARACTER_CI:
          if (CharactersAntiMatch (ch1, ch2, argument, RE_GetAlternativeUniChar (argument)))
            return bytecode[index + 2] > 0;
          else
            return false;

        case RE_Instructions::LOOP_CLASS:
          if (!classes[argument]->Match (ch1) && (ch2 == 0 || !classes[argument]->Match (ch2)))
            return bytecode[index + 2] > 0;
          else
            return false;

        case RE_Instructions::SUCCESS:
          return true;

        case RE_Instructions::CAPTURE_START:
        case RE_Instructions::CAPTURE_END:
          break;

        default:
          return false;
        }

      index += RE_InstructionLengths[INSTRUCTION (index)];
    }
}

static bool
ClassesAntiMatch (RE_Class *a, RE_Class *b)
{
  if ((a->HasHighCharacterMap() || a->IsInverted ()) && (b->HasHighCharacterMap () || b->IsInverted ()))
    return false;

  for (unsigned ch = 0; ch < BITMAP_RANGE; ++ch)
    if (a->Match (ch) && b->Match (ch))
      return false;

  return true;
}

static bool
AntiMatchesClass (RE_Class *cls, const unsigned *bytecode, const uni_char *const *strings1, const uni_char *const *strings2, RE_Class **classes)
{
  unsigned index = 0;

  while (TRUE)
    {
      unsigned argument = ARGUMENT (index);

      switch (INSTRUCTION (index))
        {
        case RE_Instructions::MATCH_PERIOD:
          {
            if (cls->HasHighCharacterMap () || cls->IsInverted ())
              return false;
            else
              for (unsigned ch = 0; ch < BITMAP_RANGE; ++ch)
                if (ch != 10 && ch != 13 && cls->Match (ch))
                  return false;

            return true;
          }

        case RE_Instructions::MATCH_CHARACTER_CS:
          return !cls->Match (argument);

        case RE_Instructions::MATCH_CHARACTER_CI:
          return !cls->Match (bytecode[index + 1] & 0xffffu) && !cls->Match (bytecode[index + 1] >> 16);

        case RE_Instructions::MATCH_STRING_CS:
          return !cls->Match (strings1[argument][0]);

        case RE_Instructions::MATCH_STRING_CI:
          return !cls->Match (strings1[argument][0]) && !cls->Match (strings2[argument][0]);

        case RE_Instructions::MATCH_CLASS:
          return ClassesAntiMatch (cls, classes[argument]);

        case RE_Instructions::LOOP_CHARACTER_CS:
          if (!cls->Match (argument))
            return bytecode[index + 2] > 0;
          else
            return false;

        case RE_Instructions::LOOP_CHARACTER_CI:
          if (!cls->Match (argument) && !cls->Match (RE_GetAlternativeUniChar (argument)))
            return bytecode[index + 2] > 0;
          else
            return false;

        case RE_Instructions::LOOP_CLASS:
          if (ClassesAntiMatch (cls, classes[argument]))
            return bytecode[index + 2] > 0;
          else
            return false;

        case RE_Instructions::PUSH_CHOICE:
          {
            if (!AntiMatchesClass (cls, bytecode + index + 1 + argument, strings1, strings2, classes))
              return false;
          }
          break;

        case RE_Instructions::SUCCESS:
          return true;

        case RE_Instructions::CAPTURE_START:
        case RE_Instructions::CAPTURE_END:
          break;

        default:
          return false;
        }

      index += RE_InstructionLengths[INSTRUCTION (index)];
    }
}

bool
RE_Native::GenerateBlock (const unsigned *bytecode, unsigned &index, unsigned &offset, unsigned loop_index, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure, bool skip_length_check)
{
  if (IsNearStackLimit ())
    return false;

  const uni_char *const *strings1 = object->GetStrings ();
  const uni_char *const *strings2 = object->GetAlternativeStrings ();
  unsigned *string_lengths = object->GetStringLengths ();
  RE_Class **classes = object->GetClasses ();

#define GET_OFFSET() (offset == ~0u ? 0 : offset)
#define ADD_OFFSET(amount) do { if (offset != ~0u) offset += (amount); } while (0)

  unsigned local_index = index, block_length, instructions_count, handled_instructions = 0;
  bool has_slow_instructions, has_loop, only_fast_instructions = false, only_slow_instructions = false, has_assertions = false;
  bool is_fixed_length;

  if (!IsFixedLength (is_fixed_length, object, bytecode, local_index, block_length, instructions_count, has_slow_instructions, has_loop, has_assertions, 0, loop_index))
    return false;

  while (INSTRUCTION (index) == RE_Instructions::CAPTURE_START)
    {
      unsigned argument = ARGUMENT (index);

      if ((argument & 1) != 0)
        GenerateCaptureStart (argument >> 1, GET_OFFSET (), failure != jt_segment_failure);
      else
        EmitCaptureReset (argument >> 1);

      ++index;
    }

  unsigned stored_index = index, stored_offset = offset;

  if (offset != ~0u && is_fixed_length && has_slow_instructions && !has_loop)
    only_fast_instructions = true;

  if (!skip_length_check && !segment_fixed_length && INSTRUCTION (index) != RE_Instructions::SUCCESS)
    EmitCheckSegmentLength (GET_OFFSET () + block_length, failure);

  bool emit_success_jump = true;

  for (unsigned pass = 0, passes = only_fast_instructions ? 2 : 1; pass < passes; ++pass)
    {
      while (INSTRUCTION (index) != RE_Instructions::SUCCESS)
        {
          unsigned argument = ARGUMENT (index), instruction_length = RE_InstructionLengths[INSTRUCTION (index)];

          ES_CodeGenerator::JumpTarget *local_success;
          bool instruction_skipped = false;

          if (segment_fixed_length && handled_instructions + 1 == instructions_count && !has_loop &&
              (object->GetCaptures () == 0 || first_capture == 0 && last_capture == object->GetCaptures () - 1))
            local_success = success;
          else
            local_success = 0;

          SetForwardJump (index);

          switch (INSTRUCTION (index))
            {
            case RE_Instructions::MATCH_PERIOD:
              if (!only_slow_instructions)
                {
                  EmitMatchPeriod (GET_OFFSET (), local_success, failure);
                  ++handled_instructions;
                }
              else
                instruction_skipped = true;

              ADD_OFFSET (1);
              break;

            case RE_Instructions::MATCH_CHARACTER_CS:
              if (!only_slow_instructions)
                {
                  EmitMatchCharacter (GET_OFFSET (), argument, 0, local_success, failure);
                  ++handled_instructions;
                }
              else
                instruction_skipped = true;

              ADD_OFFSET (1);
              break;

            case RE_Instructions::MATCH_CHARACTER_CI:
              if (!only_slow_instructions)
                {
                  EmitMatchCharacter (GET_OFFSET (), bytecode[index + 1] & 0xffffu, bytecode[index + 1] >> 16, local_success, failure);
                  ++handled_instructions;
                }
              else
                instruction_skipped = true;

              ADD_OFFSET (1);
              break;

            case RE_Instructions::MATCH_CHARACTER_CI_SLOW:
              if (!only_slow_instructions)
                {
                  EmitMatchCharacter (GET_OFFSET (), argument, ~0u, local_success, failure);
                  ++handled_instructions;
                }
              else
                instruction_skipped = true;

              ADD_OFFSET (1);
              break;

            case RE_Instructions::MATCH_STRING_CS:
            case RE_Instructions::MATCH_STRING_CI:
              if (!only_slow_instructions)
                {
                  EmitMatchString (GET_OFFSET (), strings1[argument], case_insensitive ? strings2[argument] : 0, string_lengths[argument], local_success, failure);
                  ++handled_instructions;
                }
              else
                instruction_skipped = true;

              ADD_OFFSET (string_lengths[argument]);
              break;

            case RE_Instructions::MATCH_CLASS:
              if (!only_fast_instructions)
                {
                  EmitMatchClass (GET_OFFSET (), classes[argument], local_success, failure);
                  ++handled_instructions;
                }
              else
                instruction_skipped = true;

              ADD_OFFSET (1);
              break;

            case RE_Instructions::PUSH_CHOICE:
              {
                unsigned target_index = index + instruction_length + argument, stored_offset = offset;
                ES_CodeGenerator::JumpTarget *local_failure = cg.ForwardJump ();

                index += instruction_length;

                unsigned stored_first_capture = first_capture, stored_last_capture = last_capture;

                if (!GenerateBlock (bytecode, index, offset, ~0u, success, local_failure))
                  return false;

                cg.SetJumpTarget (local_failure, TRUE);

                if (stored_last_capture < last_capture)
                  EmitResetSkippedCaptures (stored_last_capture + 1, last_capture);

                first_capture = stored_first_capture;
                last_capture = stored_last_capture;

                index = target_index;
                offset = stored_offset;

                /* emit segment length check if right hand side differs. */
                if (!segment_fixed_length)
                  {
                    bool is_fixed_length_right, has_slow_instructions_right, has_loop_right, has_assertions_right;
                    unsigned block_length_right, instructions_count_right;

                    if (!IsFixedLength (is_fixed_length_right, object, bytecode, target_index, block_length_right, instructions_count_right, has_slow_instructions_right, has_loop_right, has_assertions_right, 0, loop_index))
                      return false;

                    EmitCheckSegmentLength (GET_OFFSET () + block_length_right, failure);
                  }
              }
              continue;

            case RE_Instructions::LOOP_GREEDY:
            case RE_Instructions::LOOP:
              if (argument == loop_index)
                {
                  index += instruction_length;
                  return true;
                }
              break;

            case RE_Instructions::LOOP_PERIOD:
              return GenerateLoop (bytecode, index, GET_OFFSET (), success, failure, AntiMatchesPeriod (bytecode + index + instruction_length, strings1, classes, multiline));

            case RE_Instructions::LOOP_CHARACTER_CS:
              return GenerateLoop (bytecode, index, GET_OFFSET (), success, failure, AntiMatchesCharacter (argument, bytecode + index + instruction_length, strings1, strings2, classes));

            case RE_Instructions::LOOP_CHARACTER_CI:
              return GenerateLoop (bytecode, index, GET_OFFSET (), success, failure, AntiMatchesCharacter (argument | (RE_GetAlternativeUniChar (argument) << 16), bytecode + index + instruction_length, strings1, strings2, classes));

            case RE_Instructions::LOOP_CLASS:
              return GenerateLoop (bytecode, index, GET_OFFSET (), success, failure, AntiMatchesClass (classes[argument], bytecode + index + instruction_length, strings1, strings2, classes));

            case RE_Instructions::ASSERT_WORD_EDGE:
              if (!only_fast_instructions)
                {
                  EmitAssertWordEdge (GET_OFFSET (), local_success, failure);
                  ++handled_instructions;
                }
              else
                instruction_skipped = true;
              break;

            case RE_Instructions::ASSERT_NOT_WORD_EDGE:
              if (!only_fast_instructions)
                {
                  EmitAssertWordEdge (GET_OFFSET (), failure, local_success);
                  ++handled_instructions;
                }
              else
                instruction_skipped = true;
              break;

            case RE_Instructions::ASSERT_LINE_START:
              if (!only_fast_instructions)
                {
                  EmitAssertLineStart (GET_OFFSET (), local_success, failure);
                  ++handled_instructions;
                }
              else
                instruction_skipped = true;
              break;

            case RE_Instructions::ASSERT_LINE_END:
              if (!only_fast_instructions)
                {
                  EmitAssertLineEnd (GET_OFFSET (), local_success, failure);
                  ++handled_instructions;
                }
              else
                instruction_skipped = true;
              break;

            case RE_Instructions::CAPTURE_START:
              if ((argument & 1) != 0)
                GenerateCaptureStart (argument >> 1, GET_OFFSET (), failure != jt_segment_failure);
              else
                EmitCaptureReset (argument >> 1);
              instruction_skipped = true;
              break;

            case RE_Instructions::CAPTURE_END:
              if (!only_fast_instructions)
                {
                  EmitCaptureEnd (argument, GET_OFFSET ());
                  ++handled_instructions;
                }
              instruction_skipped = true;
              break;

            case RE_Instructions::JUMP:
              {
                unsigned local_index = index += instruction_length;

                index += argument;

                while (local_index != index)
                  {
                    unsigned instruction_length = RE_InstructionLengths[INSTRUCTION (local_index)];

                    if (INSTRUCTION (local_index) == RE_Instructions::CAPTURE_START && (ARGUMENT (local_index) & 1) == 1)
                      EmitCaptureReset (ARGUMENT (local_index) >> 1);

                    local_index += instruction_length;
                  }

                index -= instruction_length;
              }
              break;

            case RE_Instructions::FAILURE:
              cg.Jump (failure);
              break;

            default:
              return false;
            }

          if (success && local_success == success && !instruction_skipped)
            emit_success_jump = false;

          index += instruction_length;
        }

      if (only_fast_instructions)
        {
          only_fast_instructions = false;
          only_slow_instructions = true;

          index = stored_index;
          offset = stored_offset;
        }
    }

  if (object->GetCaptures ())
    GenerateResetUnreachedCaptures ();

  if (emit_success_jump)
    EmitSetLengthAndJump (offset, success);

  return true;
}

static unsigned
GetAlternativeCharacter (unsigned character)
{
  unsigned alternative = uni_tolower (character);
  if (alternative == character)
    alternative = uni_toupper (character);
  return alternative;
}

void
RE_Native::GenerateLoopBody (const LoopBody &body, unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  switch (body.type)
    {
    case LoopBody::TYPE_PERIOD:
      EmitMatchPeriod (offset, success, failure);
      break;

    case LoopBody::TYPE_CHARACTER:
      EmitMatchCharacter (offset, body.ch1, body.ch2, success, failure);
      break;

    case LoopBody::TYPE_CLASS:
      EmitMatchClass (offset, body.cls, success, failure);
    }
}

bool
RE_Native::GenerateLoopTail (const LoopTail &tail, unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure, bool skip_length_check)
{
  unsigned index = tail.index, stored_first_capture = first_capture, stored_last_capture = last_capture;

  if (!GenerateBlock (tail.bytecode, index, offset, ~0u, success, failure, skip_length_check))
    return false;

  first_capture = stored_first_capture;
  last_capture = stored_last_capture;

  return true;
}

bool
RE_Native::GenerateLoop (const unsigned *bytecode, unsigned &index, unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure, bool no_backtracking)
{
  unsigned argument = ARGUMENT (index), instruction_length = RE_InstructionLengths[INSTRUCTION (index)];

  if (!no_backtracking)
    {
      unsigned local_index = index + instruction_length, tail_length, tail_instructions_count;
      bool has_slow_instructions, has_loop, is_fixed_length, has_assertions = false;

      if (!IsFixedLength (is_fixed_length, object, bytecode, local_index, tail_length, tail_instructions_count, has_slow_instructions, has_loop, has_assertions))
        return false;

      if (tail_length == 0 && !has_assertions)
        /* Tail matches the empty string, so cannot fail, and we will never need
           to backtrack.

           However, assertions may appear to match empty, but could still fail (e.g., [a ]*\b),
           so if any assertions have been encountered for seemingly empty matches, consider them backtrackable. */
        no_backtracking = true;
    }

  unsigned loop_index, min, max;

  LoopBody body;

  switch (INSTRUCTION (index))
    {
    case RE_Instructions::LOOP_PERIOD:
      loop_index = argument;
      min = bytecode[index + 1];
      max = bytecode[index + 2];
      body.type = LoopBody::TYPE_PERIOD;
      break;

    case RE_Instructions::LOOP_CHARACTER_CS:
    case RE_Instructions::LOOP_CHARACTER_CI:
    case RE_Instructions::LOOP_CLASS:
      loop_index = bytecode[index + 1];
      min = bytecode[index + 2];
      max = bytecode[index + 3];

      if (INSTRUCTION (index) == RE_Instructions::LOOP_CLASS)
        {
          body.type = LoopBody::TYPE_CLASS;
          body.cls = object->GetClasses ()[argument];
        }
      else
        {
          body.type = LoopBody::TYPE_CHARACTER;
          body.ch1 = argument;

          if (INSTRUCTION (index) == RE_Instructions::LOOP_CHARACTER_CI)
            body.ch2 = GetAlternativeCharacter (body.ch1);
          else
            body.ch2 = 0;
        }
      break;

    default:
      return false;
    }

  index += instruction_length;

  LoopTail tail;

  tail.bytecode = bytecode;
  tail.index = index;

  if (is_backtracking && !no_backtracking)
    return false;

  tail.is_empty = INSTRUCTION (index) == RE_Instructions::SUCCESS;

  unsigned local_index = index;

  if (!IsFixedLength (tail.is_fixed_length, object, bytecode, local_index, tail.length, tail.instructions_count, tail.has_slow_instructions, tail.has_loop, tail.has_assertions, 0, loop_index))
    return false;

  if (!no_backtracking)
    is_backtracking = true;

  if (!EmitLoop (!no_backtracking, min, max, offset, body, tail, success, failure))
    return false;

  if (!no_backtracking)
    is_backtracking = false;

  return true;
}

void
RE_Native::GenerateCaptureStart (unsigned capture_index, unsigned offset, bool initialize_end)
{
  if (capture_index < first_capture)
    first_capture = capture_index;
  if (last_capture == UINT_MAX || capture_index > last_capture)
    last_capture = capture_index;

  EmitCaptureStart (capture_index, offset, initialize_end);
}

void
RE_Native::GenerateResetUnreachedCaptures ()
{
  if (first_capture != 0 || last_capture != object->GetCaptures () - 1)
    {
      if (first_capture == UINT_MAX)
        EmitResetUnreachedCaptures (UINT_MAX, UINT_MAX, object->GetCaptures ());
      else
        EmitResetUnreachedCaptures (first_capture, last_capture, object->GetCaptures ());
    }
}

ES_CodeGenerator::JumpTarget *
RE_Native::AddForwardJump (unsigned target_index)
{
  ForwardJump **ptr = &forward_jumps;

  while (*ptr && (*ptr)->index < target_index)
    ptr = &(*ptr)->next;

  if (!*ptr || (*ptr)->index != target_index)
    {
      ForwardJump *fj = OP_NEW_L (ForwardJump, ());
      fj->jt = cg.ForwardJump ();
      fj->index = target_index;
      fj->next = *ptr;
      *ptr = fj;
    }

  return (*ptr)->jt;
}

ES_CodeGenerator::JumpTarget *
RE_Native::SetForwardJump (unsigned index)
{
  if (forward_jumps && forward_jumps->index == index)
    {
      ForwardJump *fj = forward_jumps;
      forward_jumps = fj->next;

      ES_CodeGenerator::JumpTarget *jt = fj->jt;
      OP_DELETE (fj);

      cg.SetJumpTarget (jt, TRUE);
      return jt;
    }

  return 0;
}

bool
RE_Native::IsNearStackLimit ()
{
    unsigned char dummy;
    int stack_used = control_stack_base - &dummy;
    /* Having a SYSTEM_ setting for stack direction would be preferable. */
    if (stack_used < 0)
        stack_used = -stack_used;
    return stack_used >= RE_CONFIG_PARM_MAX_STACK;
}

#endif // RE_FEATURE__MACHINE_CODED
