/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/regexp/src/re_config.h"

#ifdef RE_FEATURE__MACHINE_CODED
#ifdef ARCHITECTURE_MIPS

#include "modules/regexp/src/re_native.h"

typedef ES_CodeGenerator::Register Register;

#define REG_CHARACTER ES_CodeGenerator::REG_S0
#define REG_RESULT    ES_CodeGenerator::REG_S1
#define REG_STRING    ES_CodeGenerator::REG_S2
#define REG_INDEX     ES_CodeGenerator::REG_S3
#define REG_LENGTH    ES_CodeGenerator::REG_S4
#define REG_COUNT     ES_CodeGenerator::REG_S5

#define REG_Z  ES_CodeGenerator::REG_ZERO
#define REG_AT ES_CodeGenerator::REG_AT // Scratch register shared with ES_CodeGenerator.
#define REG_V0 ES_CodeGenerator::REG_V0
#define REG_V1 ES_CodeGenerator::REG_V1
#define REG_A0 ES_CodeGenerator::REG_A0
#define REG_A1 ES_CodeGenerator::REG_A1
#define REG_A2 ES_CodeGenerator::REG_A2
#define REG_A3 ES_CodeGenerator::REG_A3
#define REG_T0 ES_CodeGenerator::REG_T0
#define REG_T1 ES_CodeGenerator::REG_T1
#define REG_T2 ES_CodeGenerator::REG_T2
#define REG_T3 ES_CodeGenerator::REG_T3
#define REG_T4 ES_CodeGenerator::REG_T4
#define REG_T5 ES_CodeGenerator::REG_T5
#define REG_T6 ES_CodeGenerator::REG_T6
#define REG_T7 ES_CodeGenerator::REG_T7
#define REG_S0 ES_CodeGenerator::REG_S0
#define REG_S1 ES_CodeGenerator::REG_S1
#define REG_S2 ES_CodeGenerator::REG_S2
#define REG_S3 ES_CodeGenerator::REG_S3
#define REG_S4 ES_CodeGenerator::REG_S4
#define REG_S5 ES_CodeGenerator::REG_S5
#define REG_S6 ES_CodeGenerator::REG_S6
#define REG_S7 ES_CodeGenerator::REG_S7
#define REG_T8 ES_CodeGenerator::REG_T8
#define REG_T9 ES_CodeGenerator::REG_T9
#define REG_SP ES_CodeGenerator::REG_SP
#define REG_RA ES_CodeGenerator::REG_RA

#define ELEMENT_START_OFFSET(index) ((index) * 2 * sizeof (unsigned))
#define ELEMENT_LENGTH_OFFSET(index) ((index) * 2 * sizeof (unsigned) + sizeof (unsigned))

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
#define ANNOTATE(msg) cg.Annotate (UNI_L(msg "\n"))
#else
#define ANNOTATE(msg)
#endif

static void
LoadCharacter (ES_CodeGenerator &cg, int offset)
{
  offset *= sizeof (uni_char);

  if (offset >= SHRT_MIN && offset <= SHRT_MAX)
    cg.LHU (REG_CHARACTER, offset, REG_INDEX);
  else
    {
      cg.Add (REG_AT, REG_INDEX, offset);
      cg.LHU (REG_CHARACTER, 0, REG_AT);
    }
}

static void
PopAndReturn (ES_CodeGenerator &cg)
{
  cg.LW (REG_S0, 16, REG_SP);
  cg.LW (REG_S1, 20, REG_SP);
  cg.LW (REG_S2, 24, REG_SP);
  cg.LW (REG_S3, 28, REG_SP);
  cg.LW (REG_S4, 32, REG_SP);
  cg.LW (REG_S5, 36, REG_SP);
  cg.LW (REG_RA, 40, REG_SP);

  cg.JR ();
  cg.Add (REG_SP, 48);
}

static void
MatchLineBreak (ES_CodeGenerator &cg, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  ANNOTATE ("MatchLineBreak");

  enum { LF = 10, CR = 13, LS = 0x2028, PS = 0x2029 };

  bool has_success = success != 0;
  bool has_failure = failure != 0;

  if (!success)
    success = cg.ForwardJump ();

  if (!failure)
    failure = cg.ForwardJump ();

  /* Test for bits that aren't set in any line break character. */
  cg.ANDI (REG_T0, REG_CHARACTER, 0xffff ^ (LF | CR | LS | PS));
  cg.Jump (failure, ES_NATIVE_CONDITION_NEZ (REG_T0));

  cg.Load (REG_T0, LF);
  cg.JumpEQ (success, REG_CHARACTER, REG_T0);

  cg.Load (REG_T0, CR);
  cg.JumpEQ (success, REG_CHARACTER, REG_T0);

  cg.Load (REG_T0, LS);
  cg.JumpEQ (success, REG_CHARACTER, REG_T0);

  cg.Load (REG_T0, PS);
  cg.JumpEQ (success, REG_CHARACTER, REG_T0);

  if (has_failure)
    cg.Jump (failure);

  if (!has_success)
    cg.SetJumpTarget (success);

  if (!has_failure)
    cg.SetJumpTarget (failure);
}

static void
MatchCharacter (ES_CodeGenerator &cg, const uni_char *character1, const uni_char *character2, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure, bool case_insensitive)
{
  ANNOTATE ("MatchCharacter");

  if (case_insensitive && character2 && character1[0] != character2[0])
    {
      if ((character1[0] ^ character2[0]) == ('A' ^ 'a'))
        {
          cg.ANDI (REG_T1, REG_CHARACTER, ~('A' ^ 'a'));
          cg.XORI (REG_T0, REG_T1, character1[0] & ~('A' ^ 'a'));
        }
      else
        {
          cg.XORI (REG_T1, REG_CHARACTER, character1[0]);
          cg.XORI (REG_T2, REG_CHARACTER, character2[0]);
          cg.AND (REG_T0, REG_T1, REG_T2);
        }
    }
  else
    cg.XORI (REG_T0, REG_CHARACTER, character1[0]);

  if (success)
    {
      cg.Jump (success, ES_NATIVE_CONDITION_EQZ (REG_T0));

      if (failure)
        cg.Jump (failure);
    }
  else
    cg.Jump (failure, ES_NATIVE_CONDITION_NEZ (REG_T0));
}

static void
MatchWordCharacter (ES_CodeGenerator &cg, ES_CodeGenerator::Register result)
{
  ANNOTATE ("MatchWordCharacter");

  ES_CodeGenerator::JumpTarget *success = cg.ForwardJump ();

  cg.Load (result, 1);

  cg.Sub (REG_T3, REG_CHARACTER, '0');
  cg.JumpLEU (success, REG_T3, '9' - '0');

  cg.Load (REG_T3, '_');
  cg.JumpEQ (success, REG_CHARACTER, REG_T3);

  cg.ANDI (REG_T3, REG_CHARACTER, ~('A' ^ 'a'));
  cg.Sub (REG_T3, 'A');
  cg.JumpLEU (success, REG_T3, 'Z' - 'A');

  cg.Load (result, 0);

  cg.SetJumpTarget (success);
}


void
RE_Native::EmitGlobalPrologue ()
{
  ANNOTATE ("EmitGlobalPrologue");

  OP_ASSERT (sizeof (uni_char) == 2); // This is set in stone, right?

  cg.Sub (REG_SP, 48);

  cg.SW (REG_S0, 16, REG_SP);
  cg.SW (REG_S1, 20, REG_SP);
  cg.SW (REG_S2, 24, REG_SP);
  cg.SW (REG_S3, 28, REG_SP);
  cg.SW (REG_S4, 32, REG_SP);
  cg.SW (REG_S5, 36, REG_SP);
  cg.SW (REG_RA, 40, REG_SP);

  cg.Move (REG_RESULT, REG_A0);
  cg.Move (REG_STRING, REG_A1);

  cg.SLL (REG_T0, REG_A2, 1);
  cg.ADDU (REG_INDEX, REG_STRING, REG_T0);

  cg.Move (REG_LENGTH, REG_A3);

  jt_start = cg.ForwardJump ();

  if (global_fixed_length)
    {
      cg.Sub (REG_LENGTH, segment_length);
      cg.Jump (jt_start, ES_NATIVE_CONDITION_GEZ (REG_LENGTH));
    }
  else
    cg.Jump (jt_start);

  jt_failure = cg.Here ();

  ANNOTATE ("EmitGlobalPrologue, failure");

  cg.Load (REG_V0, 0);
  PopAndReturn (cg);

  if (global_fixed_length)
    {
      jt_success = cg.Here ();

      ANNOTATE ("EmitGlobalPrologue, success");

      cg.SUBU (REG_T0, REG_INDEX, REG_STRING);
      cg.SRL (REG_T0, REG_T0, 1);
      cg.Load (REG_T1, segment_length);

      cg.SW (REG_T0, ELEMENT_START_OFFSET (0), REG_RESULT);
      cg.SW (REG_T1, ELEMENT_LENGTH_OFFSET (0), REG_RESULT);

      cg.Load (REG_V0, 1);
      PopAndReturn (cg);
    }
  else if (segment_variable_length)
    {
      jt_success = cg.Here ();

      ANNOTATE ("EmitGlobalPrologue, success");

      cg.SUBU (REG_T0, REG_INDEX, REG_STRING);
      cg.SRL (REG_T0, REG_T0, 1);

      cg.SW (REG_T0, ELEMENT_START_OFFSET (0), REG_RESULT);
      cg.SW (REG_LENGTH, ELEMENT_LENGTH_OFFSET (0), REG_RESULT);

      cg.Load (REG_V0, 1);
      PopAndReturn (cg);
    }
  else
    jt_success = NULL;

  if (searching)
    {
      jt_search = cg.Here ();

      ANNOTATE ("EmitGlobalPrologue, search");

      cg.Add (REG_INDEX, 2);
      cg.Sub (REG_LENGTH, 1);
      cg.Jump (jt_failure, ES_NATIVE_CONDITION_LTZ (REG_LENGTH));
    }
}

void
RE_Native::EmitSegmentPrologue (unsigned segment_index)
{
  ANNOTATE ("EmitSegmentPrologue");

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
    cg.JumpLT (jt_segment_failure, REG_LENGTH, segment_length);
}

/* Called for fixed length segments when there's no common fixed length. */
void
RE_Native::EmitSegmentSuccessEpilogue ()
{
  ANNOTATE ("EmitSegmentSuccessEpilogue");

  cg.SetJumpTarget (jt_segment_success);

  cg.SUBU (REG_T0, REG_INDEX, REG_STRING);
  cg.SRL (REG_T0, REG_T0, 1);
  cg.SW (REG_T0, ELEMENT_START_OFFSET (0), REG_RESULT);

  if (segment_fixed_length)
    {
      cg.Load (REG_T1, segment_length);
      cg.SW (REG_T1, ELEMENT_LENGTH_OFFSET (0), REG_RESULT);
    }
  else
    cg.SW (REG_LENGTH, ELEMENT_LENGTH_OFFSET (0), REG_RESULT);

  cg.Load (REG_V0, 1);
  PopAndReturn (cg);
}

void
RE_Native::EmitCheckSegmentLength (unsigned length, ES_CodeGenerator::JumpTarget *failure)
{
  ANNOTATE ("EmitCheckSegmentLength");

  cg.JumpLT (failure, REG_LENGTH, length);
}

void
RE_Native::EmitSetLengthAndJump (unsigned length, ES_CodeGenerator::JumpTarget *target)
{
  ANNOTATE ("EmitSetLengthAndJump");

  if (!segment_fixed_length)
    cg.Load (REG_LENGTH, length);

  if (target)
    cg.Jump (target);
}

void
RE_Native::EmitMatchPeriod (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  ANNOTATE ("EmitMatchPeriod");

  LoadCharacter (cg, offset);
  MatchLineBreak (cg, failure, success);
}

void
RE_Native::EmitMatchCharacter (unsigned offset, unsigned ch1, unsigned ch2, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  ANNOTATE ("EmitMatchCharacter");

  uni_char character1 = ch1, character2, *cp2;

  if (ch2 != 0 && ch2 != ~0u)
    {
      character2 = ch2;
      cp2 = &character2;
    }
  else
    cp2 = 0;

  LoadCharacter (cg, offset);
  MatchCharacter (cg, &character1, cp2, success, failure, case_insensitive);
}

void
RE_Native::EmitMatchString (unsigned offset, const uni_char *string1, const uni_char *string2, unsigned string_length, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  ANNOTATE ("EmitMatchString");

  OP_ASSERT (string_length);

  if (string_length <= 16)
    {
      for (unsigned index = 0; index < string_length; index++)
        {
          ES_CodeGenerator::JumpTarget *local_success;

          if (index + 1 == string_length)
            local_success = success;
          else
            local_success = 0;

          const uni_char *character1 = string1 + index;
          const uni_char *character2 = string2 ? string2 + index : 0;

          LoadCharacter (cg, offset + index);
          MatchCharacter (cg, character1, character2, local_success, failure, case_insensitive);
        }
    }
  else
    {
      // TODO: Test 2 chars at a time and unroll loop a bit to elimitate adds.

      OP_ASSERT (failure);

      const Register reg_source = REG_T4;
      const Register reg_string1 = REG_T5;
      const Register reg_string2 = REG_T6;
      const Register reg_count = REG_T7;

      cg.Add (reg_source, REG_INDEX, offset * sizeof (uni_char));
      cg.Load (reg_string1, reinterpret_cast<INTPTR> (string1));

      if (string2)
        cg.Load (reg_string2, reinterpret_cast<INTPTR> (string2));

      cg.Load (reg_count, string_length);

      ES_CodeGenerator::JumpTarget *jt_loop = cg.Here ();

      cg.LHU (REG_CHARACTER, 0, reg_source);

      cg.LHU (REG_T8, 0, reg_string1);
      cg.XOR (REG_T0, REG_CHARACTER, REG_T8);

      if (string2)
        {
          cg.LHU (REG_T9, 0, reg_string2);
          cg.XOR (REG_T1, REG_CHARACTER, REG_T9);
          cg.AND (REG_T0, REG_T0, REG_T1);
        }

      cg.Jump (failure, ES_NATIVE_CONDITION_NEZ (REG_T0));

      cg.Add (reg_source, 2);
      cg.Add (reg_string1, 2);

      if (string2)
        cg.Add (reg_string2, 2);

      cg.Sub (reg_count, 1);
      cg.Jump (jt_loop, ES_NATIVE_CONDITION_NEZ (reg_count));

      if (success)
        cg.Jump (success);
    }
}

void
RE_Native::EmitMatchClass (unsigned offset, RE_Class *cls, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  ANNOTATE ("EmitMatchClass");

  unsigned index, count, segments;
  unsigned char *bitmap = cls->bitmap;
  bool has_alpha = false, has_nonalpha = false, is_alpha = false;

  for (index = 0, count = 0, segments = 0; index < BITMAP_RANGE; index++)
    {
      if (index == 'A' || index == 'a')
        is_alpha = true;
      else if (index == 'Z' + 1 || index == 'z' + 1)
        is_alpha = false;

      if (bitmap[index])
        {
          count++;

          if (index == 0 || !bitmap[index - 1])
            segments++;

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

  LoadCharacter (cg, offset);

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

      ANNOTATE ("EmitMatchClass, high character map slow case");

      cg.Load (REG_A0, cls);
      cg.Load (REG_T9, reinterpret_cast<INTPTR> (&RE_Class::ContainsCharacter));
      cg.JALR (REG_T9);
      cg.Move (REG_A1, REG_CHARACTER);

      if (success)
        {
          cg.Jump (success, ES_NATIVE_CONDITION_NEZ (REG_V0));
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
          cg.Jump (failure, ES_NATIVE_CONDITION_EQZ (REG_V0));
          cg.EndOutOfOrderBlock (TRUE);
        }
    }
  else
    high_character = use_failure;

  bool jump_to_failure = true;

  if (segments < 8)
    {
      unsigned start, stop;

      ANNOTATE ("EmitMatchClass, segments < 8");

      if (case_insensitive && has_alpha && !has_nonalpha)
        {
          cg.ANDI (REG_CHARACTER, REG_CHARACTER, ~('A' ^ 'a'));
          start = 'A', stop = 'Z' + 1;
        }
      else
        start = 0, stop = BITMAP_RANGE;

      for (index = start; index < stop; index++)
        {
          if (!bitmap[index] != !inverted)
            {
              if (index + 1 < stop && !bitmap[index + 1] != !inverted)
                {
                  unsigned range_length = 2;

                  while (index + range_length < stop && !bitmap[index + range_length] != !inverted)
                    range_length++;

                  if (index != 0)
                    {
                      cg.Sub (REG_T0, REG_CHARACTER, index);
                      cg.JumpLTU (use_success, REG_T0, range_length);
                    }
                  else
                    cg.JumpLT (use_success, REG_CHARACTER, range_length);

                  index += range_length - 1;
                }
              else
                {
                  cg.Load (REG_T0, index);
                  cg.JumpEQ (use_success, REG_CHARACTER, REG_T0);
                }
            }
        }

      if (cls->HasHighCharacterMap ())
        {
          if (case_insensitive && has_alpha && !has_nonalpha)
            LoadCharacter (cg, offset);
          cg.JumpLT (use_failure, REG_CHARACTER, BITMAP_RANGE);
          cg.Jump (high_character);
          jump_to_failure = false;
        }
    }
  else
    {
      ANNOTATE ("EmitMatchClass, segments > 8");

      cg.JumpGE (high_character, REG_CHARACTER, BITMAP_RANGE);

      cg.Load (REG_T0, cls->GetBitmap ());
      cg.ADDU (REG_T1, REG_T0, REG_CHARACTER);
      cg.LBU (REG_T2, 0, REG_T1);

      cg.Load (REG_T3, !!inverted);

      if (success)
        {
          cg.JumpNE (success, REG_T2, REG_T3);
          if (failure)
            cg.Jump (failure);
          else
            cg.SetJumpTarget (use_failure);
          if (slow_case)
            cg.SetOutOfOrderContinuationPoint (slow_case);
        }
      else
        {
          cg.JumpEQ (failure, REG_T2, REG_T3);
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

void
RE_Native::EmitAssertWordEdge (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  ANNOTATE ("EmitAssertWordEdge");

  const Register reg_left = REG_T4;
  const Register reg_right = REG_T5;

  ES_CodeGenerator::JumpTarget *use_success, *skip_left = NULL, *skip_right = NULL;

  if (success)
    use_success = success;
  else
    use_success = cg.ForwardJump ();

  if (offset == 0)
    {
      skip_left = cg.ForwardJump ();

      cg.Load (reg_left, 0);
      cg.JumpEQ (skip_left, REG_STRING, REG_INDEX);
    }

  LoadCharacter (cg, static_cast<int> (offset) - 1);
  MatchWordCharacter (cg, reg_left);

  if (skip_left)
    cg.SetJumpTarget (skip_left);

  if (!global_fixed_length && !segment_fixed_length || offset == segment_length)
    {
      skip_right = cg.ForwardJump ();

      cg.Load (reg_right, 0);

      if (global_fixed_length)
        cg.JumpEQ (skip_right, REG_LENGTH, REG_Z);
      else
        {
          cg.Load (REG_T2, offset);
          cg.JumpEQ (skip_right, REG_LENGTH, REG_T2);
        }
    }

  LoadCharacter (cg, offset);
  MatchWordCharacter (cg, reg_right);

  if (skip_right)
    cg.SetJumpTarget (skip_right);

  if (success)
    {
      cg.JumpNE (success, reg_left, reg_right);
      if (failure)
        cg.Jump (failure);
    }
  else
    cg.JumpEQ (failure, reg_left, reg_right);

  if (!success)
    cg.SetJumpTarget (use_success);
}

void
RE_Native::EmitAssertLineStart (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  ANNOTATE ("EmitAssertLineStart");

  if (offset == 0 || multiline)
    {
      ES_CodeGenerator::JumpTarget *use_success = 0;

      if (multiline)
        {
          if (success)
            use_success = success;
          else
            use_success = cg.ForwardJump ();
        }

      if (offset == 0)
        {
          if (success)
            {
              cg.JumpEQ (success, REG_STRING, REG_INDEX);
              if (failure && !multiline)
                cg.Jump (failure);
            }
          else if (multiline)
            cg.JumpEQ (use_success, REG_STRING, REG_INDEX);
          else
            cg.JumpNE (failure, REG_STRING, REG_INDEX);
        }

      if (multiline)
        {
          /* Either offset is non-zero, or we just emitted a check for whether
             we're at the beginning of the string, so we don't need to worry
             about reading before the beginning of the string here, even if the
             effective offset becomes -1.  */
          LoadCharacter (cg, static_cast<int>(offset) - 1);
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
  ANNOTATE ("EmitAssertLineEnd");

  if (global_fixed_length)
    cg.XORI (REG_T0, REG_LENGTH, 0);
  else
    cg.XORI (REG_T0, REG_LENGTH, offset);

  if (multiline)
    {
      ES_CodeGenerator::JumpTarget *use_success;

      if (success)
        use_success = success;
      else
        use_success = cg.ForwardJump ();

      /* End of string, from XOR comparison above. */
      cg.Jump (use_success, ES_NATIVE_CONDITION_EQZ (REG_T0));

      LoadCharacter (cg, offset);
      MatchLineBreak (cg, use_success, failure);

      if (!success)
        cg.SetJumpTarget (use_success);
    }
  else if (success)
    {
      cg.Jump (success, ES_NATIVE_CONDITION_EQZ (REG_T0));
      if (failure)
        cg.Jump (failure);
    }
  else
    cg.Jump (failure, ES_NATIVE_CONDITION_NEZ (REG_T0));
}

void
RE_Native::EmitCaptureStart (unsigned capture_index, unsigned offset, bool initialize_end)
{
  ANNOTATE ("EmitCaptureStart");

  cg.SUBU (REG_T0, REG_INDEX, REG_STRING);
  cg.SRL (REG_T0, REG_T0, 1);

  if (offset)
    cg.Add (REG_T0, offset);

  cg.SW (REG_T0, ELEMENT_START_OFFSET (capture_index + 1), REG_RESULT);

  if (initialize_end)
    {
      cg.Load (REG_T1, -1);
      cg.SW (REG_T1, ELEMENT_LENGTH_OFFSET (capture_index + 1), REG_RESULT);
    }
}

void
RE_Native::EmitCaptureEnd (unsigned capture_index, unsigned offset)
{
  ANNOTATE ("EmitCaptureEnd");

  cg.LW (REG_T0, ELEMENT_START_OFFSET (capture_index + 1), REG_RESULT);

  cg.SUBU (REG_T1, REG_INDEX, REG_STRING);
  cg.SRL (REG_T1, REG_T1, 1);

  if (offset)
    cg.Add (REG_T1, offset);

  cg.SUBU (REG_T2, REG_T1, REG_T0);
  cg.SW (REG_T2, ELEMENT_LENGTH_OFFSET (capture_index + 1), REG_RESULT);
}

void
RE_Native::EmitCaptureReset (unsigned capture_index)
{
  ANNOTATE ("EmitCaptureReset");

  cg.Load (REG_T0, -1);
  cg.SW (REG_T0, ELEMENT_LENGTH_OFFSET (capture_index + 1), REG_RESULT);
}

void
RE_Native::EmitResetUnreachedCaptures (unsigned first_reached, unsigned last_reached, unsigned captures_count)
{
  ANNOTATE ("EmitResetUnreachedCaptures");

  cg.Load (REG_T0, -1);

  for (unsigned capture_index = 0; capture_index < captures_count; capture_index++)
    {
      if (capture_index < first_reached || capture_index > last_reached)
        cg.SW (REG_T0, ELEMENT_LENGTH_OFFSET (capture_index + 1), REG_RESULT);
    }
}

void
RE_Native::EmitResetSkippedCaptures (unsigned first_skipped, unsigned last_skipped)
{
  ANNOTATE ("EmitResetSkippedCaptures");

  cg.Load (REG_T0, -1);

  for (unsigned capture_index = first_skipped; capture_index <= last_skipped; capture_index++)
    cg.SW (REG_T0, ELEMENT_LENGTH_OFFSET (capture_index + 1), REG_RESULT);
}

bool
RE_Native::EmitLoop (BOOL backtracking, unsigned min, unsigned max, unsigned offset, LoopBody &body, LoopTail &tail, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  ANNOTATE ("EmitLoop");

  BOOL length_stored = FALSE, count_stored = FALSE;

  if (min == max)
    backtracking = FALSE;

  ES_CodeGenerator::JumpTarget *loop_start = cg.ForwardJump (), *loop_continue = NULL, *loop_break = NULL, *loop_success = cg.ForwardJump (), *loop_failure = cg.ForwardJump (), *loop_backtrack = backtracking ? cg.ForwardJump () : loop_failure;

  cg.Sub (REG_SP, 16);

  cg.SW (REG_INDEX, 16, REG_SP);

  if (!global_fixed_length)
    {
      length_stored = TRUE;
      cg.SW (REG_LENGTH, 20, REG_SP);
    }

  if (loop_level++ != 0)
    {
      count_stored = TRUE;
      cg.SW (REG_COUNT, 24, REG_SP);
    }

  cg.Load (REG_COUNT, 0);

  if (offset != 0)
    {
      cg.Add (REG_INDEX, offset * sizeof (uni_char));

      if (!global_fixed_length)
        cg.Sub (REG_LENGTH, offset);
    }

  if (!global_fixed_length && !segment_fixed_length)
    cg.JumpNE (loop_start, REG_LENGTH, REG_Z);
  else
    cg.Jump (loop_start);

  loop_break = cg.Here ();

  ANNOTATE ("EmitLoop, break");

  if (min != 0)
    cg.JumpLT (loop_failure, REG_COUNT, min);

  unsigned tail_offset = 0;

  if (global_fixed_length)
    {
      OP_ASSERT (min == max);

      tail_offset = offset + min;
      cg.Sub (REG_INDEX, tail_offset * sizeof (uni_char));
    }

  ANNOTATE ("EmitLoop, loop tail");

  if (!GenerateLoopTail (tail, tail_offset, loop_success, loop_backtrack))
    return false;

  if (backtracking)
    {
      cg.SetJumpTarget (loop_backtrack);

      ANNOTATE ("EmitLoop, backtrack");

      cg.Sub (REG_INDEX, 2);
      cg.Add (REG_LENGTH, 1);
      cg.Sub (REG_COUNT, 1);
      cg.Jump (loop_break, ES_NATIVE_CONDITION_GEZ (REG_COUNT));
    }

  cg.SetJumpTarget (loop_failure);

  ANNOTATE ("EmitLoop, failure");

  cg.LW (REG_INDEX, 16, REG_SP);

  if (length_stored)
    cg.LW (REG_LENGTH, 20, REG_SP);

  if (count_stored)
    cg.LW (REG_COUNT, 24, REG_SP);

  cg.Add (REG_SP, 16);

  cg.Jump (failure);

  cg.SetJumpTarget (loop_success);

  ANNOTATE ("EmitLoop, success");

  cg.Move (REG_T0, REG_INDEX);

  cg.LW (REG_INDEX, 16, REG_SP);
  if (loop_level > 1)
    cg.LW (REG_COUNT, 24, REG_SP);
  cg.Add (REG_SP, 16);

  cg.SUBU (REG_T1, REG_T0, REG_INDEX);

  cg.SRL (REG_T1, REG_T1, 1);
  cg.Add (REG_LENGTH, REG_T1);
  cg.Jump (success);

  loop_continue = cg.Here ();

  ANNOTATE ("EmitLoop, continue");

  cg.Add (REG_INDEX, 2);
  cg.Add (REG_COUNT, 1);

  if (!global_fixed_length)
    {
      cg.Sub (REG_LENGTH, 1);

      if (!segment_fixed_length)
        cg.Jump (loop_break, ES_NATIVE_CONDITION_EQZ (REG_LENGTH));
    }

  if (max != UINT_MAX)
    cg.JumpGE (loop_break, REG_COUNT, max);

  cg.SetJumpTarget (loop_start);

  ANNOTATE ("EmitLoop, loop body");

  GenerateLoopBody (body, 0, loop_continue, loop_break);

  loop_level--;

  return true;
}

void
RE_Native::Finish (void *base)
{
}

#undef REG_CHARACTER
#undef REG_RESULT
#undef REG_STRING
#undef REG_INDEX
#undef REG_LENGTH
#undef REG_COUNT

#undef REG_Z
#undef REG_AT
#undef REG_V0
#undef REG_V1
#undef REG_A0
#undef REG_A1
#undef REG_A2
#undef REG_A3
#undef REG_T0
#undef REG_T1
#undef REG_T2
#undef REG_T3
#undef REG_T4
#undef REG_T5
#undef REG_T6
#undef REG_T7
#undef REG_S0
#undef REG_S1
#undef REG_S2
#undef REG_S3
#undef REG_S4
#undef REG_S5
#undef REG_S6
#undef REG_S7
#undef REG_T8
#undef REG_T9
#undef REG_SP
#undef REG_RA

#undef ELEMENT_START_OFFSET
#undef ELEMENT_LENGTH_OFFSET

#undef ANNOTATE

#endif // ARCHITECTURE_MIPS
#endif // RE_FEATURE__MACHINE_CODED
