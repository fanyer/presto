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

#include "modules/regexp/src/re_native_arch.h"

#ifdef ARCHITECTURE_ARM

#include "modules/regexp/src/re_native.h"

static void
AddImmediateToRegister(ES_CodeGenerator &cg, ES_CodeGenerator::Register source, int value, ES_CodeGenerator::Register target, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS, BOOL move_first = FALSE)
{
    if (ES_CodeGenerator::Operand::EncodeImmediate(value))
        cg.ADD(source, value, target, ES_CodeGenerator::UNSET_CONDITION_CODES, condition);
    else if (ES_CodeGenerator::Operand::EncodeImmediate(-value))
        cg.SUB(source, -value, target, ES_CodeGenerator::UNSET_CONDITION_CODES, condition);
    else
    {
        ES_CodeGenerator::Register scratch;

        if (source == REG_SCRATCH1 || target == REG_SCRATCH1)
            scratch = REG_SCRATCH2;
        else
            scratch = REG_SCRATCH1;

        OP_ASSERT(source != scratch && target != scratch);

        BOOL subtract = FALSE;

        if (ES_CodeGenerator::NotOperand::EncodeImmediate(value))
            cg.MOV(value, scratch, ES_CodeGenerator::UNSET_CONDITION_CODES, condition);
        else if (ES_CodeGenerator::NotOperand::EncodeImmediate(-value))
        {
            cg.MOV(value, scratch, ES_CodeGenerator::UNSET_CONDITION_CODES, condition);
            subtract = TRUE;
        }
        else
        {
            ES_CodeGenerator::Constant *constant = cg.NewConstant(value);
            cg.LDR(constant, scratch, condition);
        }

        if (subtract)
            cg.SUB(source, scratch, target, ES_CodeGenerator::UNSET_CONDITION_CODES, condition);
        else
            cg.ADD(source, scratch, target, ES_CodeGenerator::UNSET_CONDITION_CODES, condition);
    }
}

static void
MoveImmediateToRegister (ES_CodeGenerator &cg, int immediate, ES_CodeGenerator::Register target, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS)
{
    if (ES_CodeGenerator::NotOperand::EncodeImmediate (immediate))
        cg.MOV (immediate, target, ES_CodeGenerator::UNSET_CONDITION_CODES, condition);
    else
    {
        ES_CodeGenerator::Constant *constant = cg.NewConstant (immediate);
        cg.LDR (constant, target, condition);
    }
}

static void
CompareRegisterToImmediate(ES_CodeGenerator &cg, ES_CodeGenerator::Register lhs, int rhs, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS)
{
    if (ES_CodeGenerator::NegOperand::EncodeImmediate (rhs))
        cg.CMP (lhs, rhs, condition);
    else
    {
        MoveImmediateToRegister (cg, rhs, REG_SCRATCH3, condition);
        cg.CMP (lhs, REG_SCRATCH3, condition);
    }
}

void
RE_ArchitectureMixin::LoadCharacter (int offset)
{
  RE_Native *self = static_cast<RE_Native *>(this);

  offset *= sizeof (uni_char);

  if (offset < 256)
    self->cg.LDRH (REG_INDEX, offset, REG_CHARACTER);
  else
    {
      MoveImmediateToRegister (self->cg, offset, REG_SCRATCH3);
      self->cg.LDRH (REG_INDEX, REG_SCRATCH3, TRUE, REG_CHARACTER);
    }
}

#define RESULT_ELEMENT_START(index) REG_RESULT, (index) * 2 * sizeof (unsigned)
#define RESULT_ELEMENT_LENGTH(index) REG_RESULT, (index) * 2 * sizeof (unsigned) + sizeof (unsigned)

void
RE_Native::EmitGlobalPrologue ()
{
  unsigned registers = (1 << ES_CodeGenerator::REG_R4 |
                        1 << ES_CodeGenerator::REG_R5 |
                        1 << ES_CodeGenerator::REG_R6 |
                        1 << ES_CodeGenerator::REG_R7 |
                        1 << ES_CodeGenerator::REG_R8);

  cg.PUSH (registers | 1 << ES_CodeGenerator::REG_LR);
  cg.MOV (ES_CodeGenerator::REG_R0, REG_RESULT);
  cg.MOV (ES_CodeGenerator::REG_R1, REG_STRING);
  cg.ADD (REG_STRING, ES_CodeGenerator::Operand (ES_CodeGenerator::REG_R2, 1), REG_INDEX);
  cg.MOV (ES_CodeGenerator::REG_R3, REG_LENGTH);

  jt_start = cg.ForwardJump ();

  if (global_fixed_length)
    {
      if (ES_CodeGenerator::Operand::EncodeImmediate (segment_length))
        cg.SUB (REG_LENGTH, segment_length, REG_LENGTH, ES_CodeGenerator::SET_CONDITION_CODES);
      else
        {
          MoveImmediateToRegister (cg, segment_length, REG_SCRATCH1);
          cg.SUB (REG_LENGTH, REG_SCRATCH1, REG_LENGTH, ES_CodeGenerator::SET_CONDITION_CODES);
        }

      cg.Jump (jt_start, ES_NATIVE_CONDITION_HIGHER_OR_SAME);
    }
  else
    cg.Jump (jt_start);

  jt_failure = cg.Here ();

  cg.MOV (0, ES_CodeGenerator::REG_R0);
  cg.POP (registers | 1 << ES_CodeGenerator::REG_PC);

  if (global_fixed_length)
    {
      jt_success = cg.Here ();

      cg.SUB (REG_INDEX, REG_STRING, REG_SCRATCH1);
      cg.MOV (ES_CodeGenerator::Operand (REG_SCRATCH1, 1, ES_CodeGenerator::SHIFT_LOGICAL_RIGHT), REG_SCRATCH1);
      MoveImmediateToRegister (cg, segment_length, REG_SCRATCH2);
      cg.STR (REG_SCRATCH1, RESULT_ELEMENT_START (0));
      cg.STR (REG_SCRATCH2, RESULT_ELEMENT_LENGTH (0));

      cg.MOV (1, ES_CodeGenerator::REG_R0);
      cg.POP (registers | 1 << ES_CodeGenerator::REG_PC);
    }
  else if (segment_variable_length)
    {
      jt_success = cg.Here ();

      cg.SUB (REG_INDEX, REG_STRING, REG_SCRATCH1);
      cg.MOV (ES_CodeGenerator::Operand (REG_SCRATCH1, 1, ES_CodeGenerator::SHIFT_LOGICAL_RIGHT), REG_SCRATCH1);
      cg.STR (REG_SCRATCH1, RESULT_ELEMENT_START (0));
      cg.STR (REG_LENGTH, RESULT_ELEMENT_LENGTH (0));

      cg.MOV (1, ES_CodeGenerator::REG_R0);
      cg.POP (registers | 1 << ES_CodeGenerator::REG_PC);
    }
  else
    jt_success = 0;

  if (searching)
    {
      jt_search = cg.Here ();

      cg.ADD (REG_INDEX, 2, REG_INDEX);
      cg.SUB (REG_LENGTH, 1, REG_LENGTH, ES_CodeGenerator::SET_CONDITION_CODES);
      cg.Jump (jt_failure, ES_NATIVE_CONDITION_NEGATIVE);
    }
}

void
RE_Native::EmitSegmentPrologue (unsigned segment_index)
{
  if (segment_index == 0)
    cg.SetJumpTarget (jt_start);
  else
    cg.SetJumpTarget (jt_segment_failure);

  if (segment_index + 1 != object->GetBytecodeSegmentsCount ())
    jt_segment_failure = cg.ForwardJump ();
  else if (searching)
    jt_segment_failure = jt_search;
  else
    jt_segment_failure = jt_failure;

  if ((global_fixed_length || !segment_fixed_length) && object->GetCaptures () == 0)
    jt_segment_success = jt_success;
  else
    jt_segment_success = cg.ForwardJump ();

  if (!global_fixed_length && segment_fixed_length)
    {
      CompareRegisterToImmediate (cg, REG_LENGTH, segment_length);
      cg.Jump (jt_segment_failure, ES_NATIVE_CONDITION_LOWER);
    }
}

void
RE_Native::EmitSegmentSuccessEpilogue ()
{
  cg.SetJumpTarget (jt_segment_success);

  cg.SUB (REG_INDEX, REG_STRING, REG_SCRATCH1);
  cg.MOV (ES_CodeGenerator::Operand (REG_SCRATCH1, 1, ES_CodeGenerator::SHIFT_LOGICAL_RIGHT), REG_SCRATCH1);
  cg.STR (REG_SCRATCH1, RESULT_ELEMENT_START (0));

  if (segment_fixed_length)
    {
      MoveImmediateToRegister (cg, segment_length, REG_SCRATCH1);
      cg.STR (REG_SCRATCH1, RESULT_ELEMENT_LENGTH (0));
    }
  else
    cg.STR (REG_LENGTH, RESULT_ELEMENT_LENGTH (0));

  unsigned registers = (1 << ES_CodeGenerator::REG_R4 |
                        1 << ES_CodeGenerator::REG_R5 |
                        1 << ES_CodeGenerator::REG_R6 |
                        1 << ES_CodeGenerator::REG_R7 |
                        1 << ES_CodeGenerator::REG_R8);

  cg.MOV (1, ES_CodeGenerator::REG_R0);
  cg.POP (registers | 1 << ES_CodeGenerator::REG_PC);
}

void
RE_Native::EmitCheckSegmentLength (unsigned length, ES_CodeGenerator::JumpTarget *failure)
{
  CompareRegisterToImmediate (cg, REG_LENGTH, length);
  cg.Jump (failure, ES_NATIVE_CONDITION_LOWER);
}

void
RE_Native::EmitSetLengthAndJump (unsigned length, ES_CodeGenerator::JumpTarget *target)
{
  if (!segment_fixed_length)
    MoveImmediateToRegister (cg, length, REG_LENGTH);

  if (target)
    cg.Jump (target);
}

static void
MatchLineBreak (ES_CodeGenerator &cg, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  enum { LF = 10, CR = 13, LS = 0x2028, PS = 0x2029 };

  bool has_success = success != 0;
  bool has_failure = failure != 0;

  if (!success)
    success = cg.ForwardJump ();
  if (!failure)
    failure = cg.ForwardJump ();

  /* 0xfd0 are some bits that aren't set in any line break character.  If any of
     those bits are set in the character, the character is definitely not a line
     break.  0xdfd0 is used in x86, but we have limited immediates, so 0xfd0 is
     the best we can do.  */
  cg.TST (REG_CHARACTER, 0xfd0);
  cg.Jump (failure, ES_NATIVE_CONDITION_NOT_EQUAL);

  cg.CMP (REG_CHARACTER, LF);
  cg.CMP (REG_CHARACTER, CR, ES_NATIVE_CONDITION_NOT_EQUAL);
  cg.Jump (success, ES_NATIVE_CONDITION_EQUAL);

  cg.SUB (REG_CHARACTER, 0x2000, REG_SCRATCH3);
  cg.CMP (REG_SCRATCH3, 0x28);
  cg.CMP (REG_SCRATCH3, 0x29, ES_NATIVE_CONDITION_NOT_EQUAL);

  if (has_success)
    {
      cg.Jump (success, ES_NATIVE_CONDITION_EQUAL);
      if (has_failure)
        cg.Jump (failure);
    }
  else
    cg.Jump (failure, ES_NATIVE_CONDITION_NOT_EQUAL);

  if (!has_success)
    cg.SetJumpTarget (success);
  if (!has_failure)
    cg.SetJumpTarget (failure);
}

void
RE_Native::EmitMatchPeriod (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  LoadCharacter (offset);
  MatchLineBreak (cg, failure, success);
}

static void
MatchCharacter (ES_CodeGenerator &cg, const uni_char *character1, const uni_char *character2, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure, bool case_insensitive)
{
  if (case_insensitive && character2 && character1[0] != character2[0])
    if ((character1[0] ^ character2[0]) == ('A' ^ 'a'))
      {
        cg.BIC (REG_CHARACTER, 'A' ^ 'a', REG_CHARACTER);
        CompareRegisterToImmediate (cg, REG_CHARACTER, character1[0] & 0xffdf);
      }
    else
      {
        CompareRegisterToImmediate (cg, REG_CHARACTER, character1[0]);
        CompareRegisterToImmediate (cg, REG_CHARACTER, character2[0], ES_NATIVE_CONDITION_NOT_EQUAL);
      }
  else
    CompareRegisterToImmediate (cg, REG_CHARACTER, character1[0]);

  if (success)
    {
      cg.Jump (success, ES_NATIVE_CONDITION_EQUAL);
      if (failure)
        cg.Jump (failure);
    }
  else
    cg.Jump (failure, ES_NATIVE_CONDITION_NOT_EQUAL);
}

void
RE_Native::EmitMatchCharacter (unsigned offset, unsigned ch1, unsigned ch2, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  uni_char character1 = ch1, character2, *cp2;

  if (ch2 != 0 && ch2 != ~0u)
    {
      character2 = ch2;
      cp2 = &character2;
    }
  else
    cp2 = 0;

  LoadCharacter (offset);
  MatchCharacter (cg, &character1, cp2, success, failure, case_insensitive);
}

void
RE_Native::EmitMatchString (unsigned offset, const uni_char *string1, const uni_char *string2, unsigned string_length, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  for (unsigned index = 0; index < string_length; ++index)
    {
      ES_CodeGenerator::JumpTarget *local_success;

      if (index + 1 == string_length)
        local_success = success;
      else
        local_success = 0;

      const uni_char *character1 = string1 + index;
      const uni_char *character2 = string2 ? string2 + index : 0;

      LoadCharacter (offset + index);
      MatchCharacter (cg, character1, character2, local_success, failure, case_insensitive);
    }
}

void
RE_Native::EmitMatchClass (unsigned offset, RE_Class *cls, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  unsigned index, count, segments;
  unsigned char *bitmap = cls->bitmap;
  bool has_alpha = false, has_nonalpha = false, is_alpha = false;

  for (index = 0, count = 0, segments = 0; index < BITMAP_RANGE; ++index)
    {
      if (index == 'A' || index == 'a')
        is_alpha = true;
      else if (index == 'Z' + 1 || index == 'z' + 1)
        is_alpha = false;

      if (bitmap[index])
        {
          ++count;

          if (index == 0 || !bitmap[index - 1])
            ++segments;

          if (is_alpha)
            has_alpha = true;
          else
            has_nonalpha = true;
        }
    }

  bool inverted = cls->IsInverted ();

  if (inverted)
    {
      ES_CodeGenerator::JumpTarget *temporary = success;
      success = failure;
      failure = temporary;
    }

  LoadCharacter (offset);

  ES_CodeGenerator::JumpTarget *use_success;

  if (success)
    use_success = success;
  else
    use_success = cg.ForwardJump ();

  ES_CodeGenerator::OutOfOrderBlock *slow_case = 0;
  ES_CodeGenerator::JumpTarget *high_character, *use_failure;

  if (failure)
    use_failure = failure;
  else
    use_failure = cg.ForwardJump ();

  if (cls->HasHighCharacterMap ())
    {
      slow_case = cg.StartOutOfOrderBlock ();
      high_character = slow_case->GetJumpTarget ();

      ES_CodeGenerator::Constant *c_cls = cg.NewConstant (cls);

      if (!c_ContainsCharacter)
        c_ContainsCharacter = cg.NewFunction (reinterpret_cast<void (*)()>(&RE_Class::ContainsCharacter));

      cg.LDR (c_cls, ES_CodeGenerator::REG_R0);
      cg.MOV (ES_CodeGenerator::REG_PC, ES_CodeGenerator::REG_LR);
      cg.LDR (c_ContainsCharacter, ES_CodeGenerator::REG_PC);
      cg.TEQ (ES_CodeGenerator::REG_R0, 0);

      if (success)
        {
          cg.Jump (success, ES_NATIVE_CONDITION_NOT_EQUAL);
          if (failure)
            {
              cg.Jump (failure);
              cg.EndOutOfOrderBlock (FALSE);
            }
          else
            cg.EndOutOfOrderBlock (TRUE);
        }
      else
        {
          cg.Jump (failure, ES_NATIVE_CONDITION_EQUAL);
          cg.EndOutOfOrderBlock (TRUE);
        }
    }
  else
    high_character = use_failure;

  bool jump_to_failure = true;

  if (segments < 8)
    {
      unsigned start, stop;

      if (case_insensitive && has_alpha && !has_nonalpha)
        {
          cg.BIC (REG_CHARACTER, 'A' ^ 'a', REG_CHARACTER);
          start = 'A', stop = 'Z' + 1;
        }
      else
        start = 0, stop = BITMAP_RANGE;

      for (index = start; index < stop; ++index)
        if (!bitmap[index] != !inverted)
          if (index + 1 < stop && !bitmap[index + 1] != !inverted)
            {
              unsigned range_length = 2;

              while (index + range_length < stop && !bitmap[index + range_length] != !inverted)
                ++range_length;

              if (index != 0)
                {
                  cg.SUB (REG_CHARACTER, index, REG_SCRATCH1);
                  cg.CMP (REG_SCRATCH1, range_length);
                }
              else
                cg.CMP (REG_CHARACTER, range_length);

              cg.Jump (use_success, ES_NATIVE_CONDITION_LOWER);

              index += range_length - 1;
            }
          else
            {
              cg.CMP (REG_CHARACTER, index);
              cg.Jump (use_success, ES_NATIVE_CONDITION_EQUAL);
            }

      if (cls->HasHighCharacterMap ())
        {
          if (case_insensitive && has_alpha && !has_nonalpha)
            LoadCharacter (offset);
          cg.CMP (REG_CHARACTER, BITMAP_RANGE);
          cg.Jump (use_failure, ES_NATIVE_CONDITION_LOWER);
          cg.Jump (high_character);
          jump_to_failure = false;
        }
    }
  else
    {
      cg.CMP (REG_CHARACTER, BITMAP_RANGE);
      cg.Jump (high_character, ES_NATIVE_CONDITION_HIGHER_OR_SAME);

      ES_CodeGenerator::Constant *c_bitmap = cg.NewConstant (cls->GetBitmap ());

      cg.LDR (c_bitmap, REG_SCRATCH1);
      cg.LDRB (REG_SCRATCH1, REG_CHARACTER, ES_CodeGenerator::SHIFT_LOGICAL_LEFT, 0, TRUE, REG_SCRATCH1);
      cg.TEQ (REG_SCRATCH1, !!inverted);

      if (success)
        {
          cg.Jump (success, ES_NATIVE_CONDITION_NOT_EQUAL);
          if (failure)
            cg.Jump (failure);
          else
            cg.SetJumpTarget (use_failure);
          if (slow_case)
            cg.SetOutOfOrderContinuationPoint (slow_case);
        }
      else
        {
          cg.Jump (failure, ES_NATIVE_CONDITION_EQUAL);
          if (slow_case)
            cg.SetOutOfOrderContinuationPoint (slow_case);
        }

      return;
    }

  if (failure)
    {
      if (jump_to_failure)
        cg.Jump (failure);
      if (!success)
        {
          cg.SetJumpTarget (use_success);
          if (slow_case)
            cg.SetOutOfOrderContinuationPoint (slow_case);
        }
    }
  else
    {
      cg.SetJumpTarget (use_failure);
      if (slow_case)
        cg.SetOutOfOrderContinuationPoint (slow_case);
    }
}

static void
MatchWordCharacter (ES_CodeGenerator &cg, ES_CodeGenerator::Register result)
{
  ES_CodeGenerator::JumpTarget *success = cg.ForwardJump ();

  cg.MOV (1, result);

  cg.SUB (REG_CHARACTER, '0', REG_SCRATCH3);
  cg.CMP (REG_SCRATCH3, '9' - '0');
  cg.Jump (success, ES_NATIVE_CONDITION_LOWER_OR_SAME);

  cg.CMP (REG_CHARACTER, '_');
  cg.Jump (success, ES_NATIVE_CONDITION_EQUAL);

  cg.BIC (REG_CHARACTER, 'A' ^ 'a', REG_SCRATCH3);
  cg.SUB (REG_SCRATCH3, 'A', REG_SCRATCH3);
  cg.CMP (REG_SCRATCH3, 'Z' - 'A');
  cg.MOV (0, result, ES_CodeGenerator::UNSET_CONDITION_CODES, ES_NATIVE_CONDITION_HIGHER);

  cg.SetJumpTarget (success);
}

void
RE_Native::EmitAssertWordEdge (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  ES_CodeGenerator::Register left(REG_SCRATCH1), right(REG_SCRATCH2);
  ES_CodeGenerator::JumpTarget *use_success, *skip_left = NULL, *skip_right = NULL;

  if (success)
    use_success = success;
  else
    use_success = cg.ForwardJump ();

  if (offset == 0)
    {
      skip_left = cg.ForwardJump ();

      cg.CMP (REG_STRING, REG_INDEX);
      cg.MOV (0, left, ES_CodeGenerator::UNSET_CONDITION_CODES, ES_NATIVE_CONDITION_EQUAL);
      cg.Jump (skip_left, ES_NATIVE_CONDITION_EQUAL);
    }

  LoadCharacter (static_cast<int> (offset) - 1);
  MatchWordCharacter (cg, left);

  if (skip_left)
    cg.SetJumpTarget (skip_left);

  if (!global_fixed_length && !segment_fixed_length || offset == segment_length)
    {
      skip_right = cg.ForwardJump ();

      if (global_fixed_length)
        cg.TEQ (REG_LENGTH, 0);
      else
        cg.TEQ (REG_LENGTH, offset);

      cg.MOV (0, right, ES_CodeGenerator::UNSET_CONDITION_CODES, ES_NATIVE_CONDITION_EQUAL);
      cg.Jump (skip_right, ES_NATIVE_CONDITION_EQUAL);
    }

  LoadCharacter (offset);
  MatchWordCharacter (cg, right);

  if (skip_right)
    cg.SetJumpTarget (skip_right);

  cg.TEQ (left, right);

  if (success)
    {
      cg.Jump (success, ES_NATIVE_CONDITION_NOT_EQUAL);
      if (failure)
        cg.Jump (failure);
    }
  else
    cg.Jump (failure, ES_NATIVE_CONDITION_EQUAL);

  if (!success)
    cg.SetJumpTarget (use_success);
}

void
RE_Native::EmitAssertLineStart (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  if (offset == 0 || multiline)
    {
      ES_CodeGenerator::JumpTarget *use_success = 0;

      if (multiline)
        if (success)
          use_success = success;
        else
          use_success = cg.ForwardJump ();

      if (offset == 0)
        {
          cg.TEQ (REG_STRING, REG_INDEX);

          if (success)
            {
              cg.Jump (success, ES_NATIVE_CONDITION_EQUAL);
              if (failure && !multiline)
                cg.Jump (failure);
            }
          else if (multiline)
            cg.Jump (use_success, ES_NATIVE_CONDITION_EQUAL);
          else
            cg.Jump (failure, ES_NATIVE_CONDITION_NOT_EQUAL);
        }

      if (multiline)
        {
          /* Either offset is non-zero, or we just emitted a check for whether
             we're at the beginning of the string, so we don't need to worry
             about reading before the beginning of the string here, even if the
             effective offset becomes -1.  */
          LoadCharacter (static_cast<int> (offset) - 1);
          MatchLineBreak (cg, success, failure);

          if (!success)
            cg.SetJumpTarget (use_success);
        }
    }
  else if (failure)
    cg.Jump (failure);
}

void
RE_Native::EmitAssertLineEnd (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  if (offset == 0 || global_fixed_length)
    cg.TEQ (REG_LENGTH, 0);
  else
    CompareRegisterToImmediate (cg, REG_LENGTH, offset);

  if (multiline)
    {
      ES_CodeGenerator::JumpTarget *use_success;

      if (success)
        use_success = success;
      else
        use_success = cg.ForwardJump ();

      /* End of string, from comparison above. */
      cg.Jump (use_success, ES_NATIVE_CONDITION_EQUAL);

      LoadCharacter (offset);
      MatchLineBreak (cg, use_success, failure);

      if (!success)
        cg.SetJumpTarget (use_success);
    }
  else if (success)
    {
      cg.Jump (success, ES_NATIVE_CONDITION_EQUAL);
      if (failure)
        cg.Jump (failure);
    }
  else
    cg.Jump (failure, ES_NATIVE_CONDITION_NOT_EQUAL);
}

void
RE_Native::EmitCaptureStart (unsigned capture_index, unsigned offset, bool initialize_end)
{
  cg.SUB (REG_INDEX, REG_STRING, REG_SCRATCH1);
  cg.MOV (ES_CodeGenerator::Operand (REG_SCRATCH1, 1, ES_CodeGenerator::SHIFT_LOGICAL_RIGHT), REG_SCRATCH1);

  if (offset != 0)
    AddImmediateToRegister (cg, REG_SCRATCH1, offset, REG_SCRATCH1);

  cg.STR (REG_SCRATCH1, RESULT_ELEMENT_START (capture_index + 1));

  if (initialize_end)
    {
      cg.MOV (-1, REG_SCRATCH1);
      cg.STR (REG_SCRATCH1, RESULT_ELEMENT_LENGTH (capture_index + 1));
    }
}

void
RE_Native::EmitCaptureEnd (unsigned capture_index, unsigned offset)
{
  cg.LDR (RESULT_ELEMENT_START (capture_index + 1), REG_SCRATCH1);
  cg.SUB (REG_INDEX, REG_STRING, REG_SCRATCH2);
  cg.MOV (ES_CodeGenerator::Operand (REG_SCRATCH2, 1, ES_CodeGenerator::SHIFT_LOGICAL_RIGHT), REG_SCRATCH2);

  if (offset != 0)
    AddImmediateToRegister (cg, REG_SCRATCH2, offset, REG_SCRATCH2);

  cg.SUB (REG_SCRATCH2, REG_SCRATCH1, REG_SCRATCH2);
  cg.STR (REG_SCRATCH2, RESULT_ELEMENT_LENGTH (capture_index + 1));
}

void
RE_Native::EmitCaptureReset (unsigned capture_index)
{
  cg.MOV (-1, REG_SCRATCH1);
  cg.STR (REG_SCRATCH1, RESULT_ELEMENT_LENGTH (capture_index + 1));
}

void
RE_Native::EmitResetUnreachedCaptures (unsigned first_reached, unsigned last_reached, unsigned captures_count)
{
  cg.MOV (-1, REG_SCRATCH1);

  for (unsigned capture_index = 0; capture_index < captures_count; ++capture_index)
    if (capture_index < first_reached || capture_index > last_reached)
      cg.STR (REG_SCRATCH1, RESULT_ELEMENT_LENGTH (capture_index + 1));
}

void
RE_Native::EmitResetSkippedCaptures (unsigned first_skipped, unsigned last_skipped)
{
  cg.MOV (-1, REG_SCRATCH1);

  for (unsigned capture_index = first_skipped; capture_index <= last_skipped; ++capture_index)
    cg.STR (REG_SCRATCH1, RESULT_ELEMENT_LENGTH (capture_index + 1));
}

bool
RE_Native::EmitLoop (BOOL backtracking, unsigned min, unsigned max, unsigned offset, LoopBody &body, LoopTail &tail, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  if (min == max)
    backtracking = FALSE;

  ES_CodeGenerator::JumpTarget *loop_start = cg.ForwardJump (), *loop_continue = NULL, *loop_break = NULL, *loop_success = cg.ForwardJump (), *loop_failure = cg.ForwardJump (), *loop_backtrack = backtracking ? cg.ForwardJump () : loop_failure;

  unsigned registers = 1 << REG_INDEX;

  if (!global_fixed_length)
    registers |= 1 << REG_LENGTH;

  if (loop_level++ != 0)
    registers |= 1 << REG_COUNT;

  cg.PUSH (registers);
  cg.MOV (0, REG_COUNT);

  if (offset != 0)
    {
      AddImmediateToRegister (cg, REG_INDEX, offset * sizeof (uni_char), REG_INDEX);

      if (!global_fixed_length)
        AddImmediateToRegister (cg, REG_LENGTH, -static_cast<int> (offset), REG_LENGTH);
    }

  if (!global_fixed_length && !segment_fixed_length)
    {
      cg.TEQ (REG_LENGTH, 0);
      cg.Jump (loop_start, ES_NATIVE_CONDITION_NOT_EQUAL);
    }
  else
    cg.Jump (loop_start);

  loop_break = cg.Here ();

  if (min != 0)
    {
      CompareRegisterToImmediate (cg, REG_COUNT, min);
      cg.Jump (loop_failure, ES_NATIVE_CONDITION_LOWER);
    }

  unsigned tail_offset = 0;

  if (global_fixed_length)
    {
      OP_ASSERT(min == max);

      tail_offset = offset + min;
      cg.SUB (REG_INDEX, tail_offset * sizeof (uni_char), REG_INDEX);
    }

  if (!GenerateLoopTail (tail, tail_offset, loop_success, loop_backtrack))
    return false;

  if (backtracking)
    {
      cg.SetJumpTarget (loop_backtrack);

      cg.SUB (REG_INDEX, 2, REG_INDEX);
      cg.ADD (REG_LENGTH, 1, REG_LENGTH);
      cg.SUB (REG_COUNT, 1, REG_COUNT, ES_CodeGenerator::SET_CONDITION_CODES);
      cg.Jump (loop_break, ES_NATIVE_CONDITION_POSITIVE_OR_ZERO);
    }

  cg.SetJumpTarget (loop_failure);
  cg.POP (registers);
  cg.Jump (failure);
  cg.SetJumpTarget (loop_success);

  cg.MOV (REG_INDEX, REG_SCRATCH1);

  if (!global_fixed_length)
    {
      cg.POP (REG_INDEX);
      cg.ADD (ES_CodeGenerator::REG_SP, sizeof(unsigned), ES_CodeGenerator::REG_SP);

      if (loop_level > 1)
        cg.POP (REG_COUNT);
    }
  else if (loop_level > 1)
    cg.POP (1 << REG_INDEX | 1 << REG_COUNT);
  else
    cg.POP (REG_INDEX);

  cg.SUB (REG_SCRATCH1, REG_INDEX, REG_SCRATCH1);
  cg.MOV (ES_CodeGenerator::Operand (REG_SCRATCH1, 1, ES_CodeGenerator::SHIFT_LOGICAL_RIGHT), REG_SCRATCH1);
  cg.ADD (REG_LENGTH, REG_SCRATCH1, REG_LENGTH);
  cg.Jump (success);

  loop_continue = cg.Here ();

  cg.ADD (REG_INDEX, 2, REG_INDEX);
  cg.ADD (REG_COUNT, 1, REG_COUNT);

  if (!global_fixed_length)
    if (!segment_fixed_length)
      {
        cg.SUB (REG_LENGTH, 1, REG_LENGTH, ES_CodeGenerator::SET_CONDITION_CODES);
        cg.Jump (loop_break, ES_NATIVE_CONDITION_EQUAL);
      }
    else
      cg.SUB (REG_LENGTH, 1, REG_LENGTH);

  if (max != UINT_MAX)
    {
      CompareRegisterToImmediate (cg, REG_COUNT, max);
      cg.Jump (loop_break, ES_NATIVE_CONDITION_HIGHER_OR_SAME);
    }

  cg.SetJumpTarget (loop_start);

  GenerateLoopBody (body, 0, loop_continue, loop_break);

  --loop_level;

  return true;
}

void
RE_Native::Finish (void *base)
{
}

#endif // ARCHITECTURE_ARM
#endif // RE_FEATURE__MACHINE_CODED
