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

#ifdef ARCHITECTURE_IA32

#include "modules/regexp/src/re_native.h"

static void *
ViolateFunctionPointer(int (RE_CALLING_CONVENTION *fn) (RE_Class *, int))
{
  union
  {
    int (RE_CALLING_CONVENTION *fn) (RE_Class *, int);
    void *ptr;
  } cvt;

  cvt.fn = fn;

  return cvt.ptr;
}

#ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS

RE_ArchitectureMixin::LoadedString *
RE_ArchitectureMixin::FindLoadedString (const uni_char *string)
{
  LoadedString *ls = first_loaded_string;

  while (ls)
    if (uni_str_eq (string, ls->string))
      return ls;
    else
      ls = ls->next;

  return 0;
}

#endif // RE_USE_PCMPxSTRx_INSTRUCTIONS

void
RE_Native::EmitGlobalPrologue ()
{
  stack_space_used = sizeof(void *);

#ifdef ARCHITECTURE_AMD64
#  ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
  cg.PUSH (ES_CodeGenerator::REG_BX);
  cg.MOV (REG_ARG_LENGTH, REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
#  endif // RE_USE_PCMPxSTRx_INSTRUCTIONS
# ifdef ARCHITECTURE_AMD64_WINDOWS
  cg.PUSH (ES_CodeGenerator::REG_DI);
  cg.PUSH (ES_CodeGenerator::REG_SI);

  stack_space_used += 2 * sizeof(void *);

  cg.MOV (REG_ARG_INDEX, REG_INDEX, ES_CodeGenerator::OPSIZE_32);
  cg.MOV (REG_ARG_LENGTH, REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
# endif // ARCHITECTURE_AMD64_WINDOWS
#else // ARCHITECTURE_AMD64
  cg.PUSH (ES_CodeGenerator::REG_BX);
  cg.PUSH (ES_CodeGenerator::REG_DI);
  cg.PUSH (ES_CodeGenerator::REG_SI);

  stack_space_used += 3 * sizeof(void *);

  cg.MOV (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, stack_space_used + sizeof (void *)), REG_STRING, ES_CodeGenerator::OPSIZE_POINTER);
  cg.MOV (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, stack_space_used + sizeof (void *) + sizeof (unsigned)), REG_INDEX, ES_CodeGenerator::OPSIZE_32);
  cg.MOV (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, stack_space_used + sizeof (void *) + 2 * sizeof (unsigned)), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64

#ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
  if (cg.SupportsSSE42 ())
    {
      const uni_char *const *strings1 = object->GetStrings ();
      const uni_char *const *strings2 = object->GetAlternativeStrings ();
      unsigned *string_lengths = object->GetStringLengths ();
      unsigned xmm = ES_CodeGenerator::REG_XMM1;

      if (case_insensitive)
        ++xmm;

      for (unsigned index = 0; index < object->GetStringsCount (); ++index)
        if (string_lengths[index] >= 4)
          {
            if (case_insensitive && strings2[index])
              {
                unsigned i;
                for (i = 0; i < string_lengths[index]; ++i)
                  if ((strings1[index][i] & ~32) != (strings2[index][i] & ~32))
                    break;
                if (i < string_lengths[index])
                  continue;
              }

            LoadedString *loaded_string = OP_NEWGRO_L (LoadedString, (), &arena);

            loaded_string->string = strings1[index];
            loaded_string->string_length = string_lengths[index];
            loaded_string->xmm = static_cast<ES_CodeGenerator::XMM> (xmm);
            loaded_string->blob = cg.NewBlob (16, 16);
            loaded_string->next = first_loaded_string;

            first_loaded_string = loaded_string;

            if (++xmm == ES_CodeGenerator::REG_XMM_MAXIMUM)
              break;
          }

      if (LoadedString *ls = first_loaded_string)
        {
          if (case_insensitive)
            {
              upcase_blob = cg.NewBlob (16, 16);
              cg.MOVDQA (ES_CodeGenerator::CONSTANT (upcase_blob), ES_CodeGenerator::REG_XMM1);
            }

          while (ls)
            {
              cg.MOVDQA (ES_CodeGenerator::CONSTANT (ls->blob), ls->xmm);
              ls = ls->next;
            }
        }
    }
#endif // RE_USE_PCMPxSTRx_INSTRUCTIONS

  jt_start = cg.ForwardJump();

  if (global_fixed_length)
    {
      cg.SUB (ES_CodeGenerator::IMMEDIATE (segment_length), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
      cg.Jump (jt_start, ES_NATIVE_CONDITION_GREATER_EQUAL, TRUE, TRUE);
    }
  else
    cg.Jump (jt_start);

  jt_failure = cg.Here (TRUE);
#ifdef ARCHITECTURE_AMD64
#  ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
  cg.POP (ES_CodeGenerator::REG_BX);
#  endif // RE_USE_PCMPxSTRx_INSTRUCTIONS
#  ifdef ARCHITECTURE_AMD64_WINDOWS
  cg.POP (ES_CodeGenerator::REG_SI);
  cg.POP (ES_CodeGenerator::REG_DI);
#  endif // ARCHITECTURE_AMD64_WINDOWS
#else // ARCHITECTURE_AMD64
  cg.POP (ES_CodeGenerator::REG_SI);
  cg.POP (ES_CodeGenerator::REG_DI);
  cg.POP (ES_CodeGenerator::REG_BX);
#endif // ARCHITECTURE_AMD64
  cg.XOR (ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
  cg.RET ();

  if (global_fixed_length)
    {
      jt_success = cg.Here (TRUE);

#ifndef ARCHITECTURE_AMD64
      cg.MOV (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, stack_space_used), REG_RESULT, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

      cg.MOV (REG_INDEX, ES_CodeGenerator::MEMORY (REG_RESULT), ES_CodeGenerator::OPSIZE_32);
      cg.MOV (ES_CodeGenerator::IMMEDIATE (segment_length), ES_CodeGenerator::MEMORY (REG_RESULT, sizeof (unsigned)), ES_CodeGenerator::OPSIZE_32);

      cg.MOV (ES_CodeGenerator::IMMEDIATE (1), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
#ifdef ARCHITECTURE_AMD64
#  ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
      cg.POP (ES_CodeGenerator::REG_BX);
#  endif // RE_USE_PCMPxSTRx_INSTRUCTIONS
#  ifdef ARCHITECTURE_AMD64_WINDOWS
      cg.POP (ES_CodeGenerator::REG_SI);
      cg.POP (ES_CodeGenerator::REG_DI);
#  endif // ARCHITECTURE_AMD64_WINDOWS
#else // ARCHITECTURE_AMD64
      cg.POP (ES_CodeGenerator::REG_SI);
      cg.POP (ES_CodeGenerator::REG_DI);
      cg.POP (ES_CodeGenerator::REG_BX);
#endif // ARCHITECTURE_AMD64
      cg.RET ();
    }
  else if (segment_variable_length)
    {
      jt_success = cg.Here (TRUE);

#ifndef ARCHITECTURE_AMD64
      cg.MOV (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, stack_space_used), REG_RESULT, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

      cg.MOV (REG_INDEX, ES_CodeGenerator::MEMORY (REG_RESULT), ES_CodeGenerator::OPSIZE_32);
      cg.MOV (REG_LENGTH, ES_CodeGenerator::MEMORY (REG_RESULT, sizeof (unsigned)), ES_CodeGenerator::OPSIZE_32);

      cg.MOV (ES_CodeGenerator::IMMEDIATE (1), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
#ifdef ARCHITECTURE_AMD64
#  ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
      cg.POP (ES_CodeGenerator::REG_BX);
#  endif // RE_USE_PCMPxSTRx_INSTRUCTIONS
#  ifdef ARCHITECTURE_AMD64_WINDOWS
      cg.POP (ES_CodeGenerator::REG_SI);
      cg.POP (ES_CodeGenerator::REG_DI);
#  endif // ARCHITECTURE_AMD64_WINDOWS
#else // ARCHITECTURE_AMD64
      cg.POP (ES_CodeGenerator::REG_SI);
      cg.POP (ES_CodeGenerator::REG_DI);
      cg.POP (ES_CodeGenerator::REG_BX);
#endif // ARCHITECTURE_AMD64
      cg.RET ();
    }
  else
    jt_success = 0;

  jt_search = cg.Here (TRUE);
  cg.ADD (ES_CodeGenerator::IMMEDIATE (1), REG_INDEX, ES_CodeGenerator::OPSIZE_32);
  cg.SUB (ES_CodeGenerator::IMMEDIATE (1), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
  cg.Jump (jt_failure, ES_NATIVE_CONDITION_CARRY, TRUE, FALSE);
}

void
RE_Native::EmitSegmentPrologue (unsigned segment_index)
{
  if (segment_index == 0)
    cg.SetJumpTarget (jt_start);
  else
    cg.SetJumpTarget (jt_segment_failure, TRUE);

  if (segment_index + 1 != object->GetBytecodeSegmentsCount ())
    jt_segment_failure = cg.ForwardJump ();
  else if (searching)
    jt_segment_failure = jt_search;
  else
    jt_segment_failure = jt_failure;

  if ((global_fixed_length || !segment_fixed_length) && object->GetCaptures () == 0)
    jt_segment_success = jt_success;
  else
    /* Signals that we mean to fall through to our epilogue. */
    jt_segment_success = cg.ForwardJump ();

  if (!global_fixed_length && segment_fixed_length)
    {
      cg.CMP (ES_CodeGenerator::IMMEDIATE(segment_length), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
      cg.Jump (jt_segment_failure, ES_NATIVE_CONDITION_BELOW, TRUE, FALSE);
    }
}

void
RE_Native::EmitSegmentSuccessEpilogue ()
{
  cg.SetJumpTarget (jt_segment_success);

#ifndef ARCHITECTURE_AMD64
  cg.MOV (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, stack_space_used), REG_RESULT, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

  cg.MOV (REG_INDEX, ES_CodeGenerator::MEMORY (REG_RESULT), ES_CodeGenerator::OPSIZE_32);

  if (segment_fixed_length)
    cg.MOV (ES_CodeGenerator::IMMEDIATE (segment_length), ES_CodeGenerator::MEMORY (REG_RESULT, sizeof (unsigned)), ES_CodeGenerator::OPSIZE_32);
  else
    cg.MOV (REG_LENGTH, ES_CodeGenerator::MEMORY (REG_RESULT, sizeof (unsigned)), ES_CodeGenerator::OPSIZE_32);

  cg.MOV (ES_CodeGenerator::IMMEDIATE (1), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
#ifdef ARCHITECTURE_AMD64
#  ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
  cg.POP (ES_CodeGenerator::REG_BX);
#  endif // RE_USE_PCMPxSTRx_INSTRUCTIONS
#  ifdef ARCHITECTURE_AMD64_WINDOWS
  cg.POP (ES_CodeGenerator::REG_SI);
  cg.POP (ES_CodeGenerator::REG_DI);
#  endif // ARCHITECTURE_AMD64_WINDOWS
#else // ARCHITECTURE_AMD64
  cg.POP (ES_CodeGenerator::REG_SI);
  cg.POP (ES_CodeGenerator::REG_DI);
  cg.POP (ES_CodeGenerator::REG_BX);
#endif // ARCHITECTURE_AMD64
  cg.RET ();
}

void
RE_Native::EmitCheckSegmentLength (unsigned length, ES_CodeGenerator::JumpTarget *failure)
{
  cg.CMP (ES_CodeGenerator::IMMEDIATE(length), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
  cg.Jump (failure, ES_NATIVE_CONDITION_BELOW);
}

void
RE_Native::EmitSetLengthAndJump (unsigned length, ES_CodeGenerator::JumpTarget *target)
{
  if (!segment_fixed_length)
    cg.MOV (ES_CodeGenerator::IMMEDIATE (length), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);

  if (target)
    cg.Jump (target);
}

#define MEMORY_CHARACTER(offset) ES_CodeGenerator::MEMORY (REG_STRING, REG_INDEX, ES_CodeGenerator::SCALE_2, (offset) * sizeof (uni_char))

static void
MatchTwoCharacters (ES_CodeGenerator &cg, const uni_char *characters1, const uni_char *characters2, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure, bool case_insensitive)
{
  if (case_insensitive && characters2 && (characters1[0] != characters2[0] || characters1[1] != characters2[1]))
    if (characters1[0] == characters2[0])
      {
        OP_ASSERT (characters1[1] != characters2[1]);
        OP_ASSERT ((characters1[1] ^ characters2[1]) == ('A' ^ 'a'));

        cg.AND (ES_CodeGenerator::IMMEDIATE (0xffdfffff), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
        cg.CMP (ES_CodeGenerator::IMMEDIATE ((characters1[1] & 0xffdf) << 16 | characters1[0]), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
      }
    else
      {
        OP_ASSERT ((characters1[0] ^ characters2[0]) == ('A' ^ 'a'));

        if (characters1[1] == characters2[1])
          {
            cg.AND (ES_CodeGenerator::IMMEDIATE (0xffffffdf), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
            cg.CMP (ES_CodeGenerator::IMMEDIATE (characters1[1] << 16 | characters1[0] & 0xffdf), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
          }
        else
          {
            OP_ASSERT ((characters1[1] ^ characters2[1]) == ('A' ^ 'a'));

            cg.AND (ES_CodeGenerator::IMMEDIATE (0xffdfffdf), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
            cg.CMP (ES_CodeGenerator::IMMEDIATE ((characters1[1] & 0xffdf) << 16 | characters1[0] & 0xffdf), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
          }
      }
  else
    cg.CMP (ES_CodeGenerator::IMMEDIATE (characters1[1] << 16 | characters1[0]), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);

  if (success)
    {
      cg.Jump (success, ES_NATIVE_CONDITION_EQUAL);
      if (failure)
        cg.Jump (failure);
    }
  else
    cg.Jump (failure, ES_NATIVE_CONDITION_NOT_EQUAL);
}

static void
MatchCharacter (ES_CodeGenerator &cg, const uni_char *character1, const uni_char *character2, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure, bool case_insensitive)
{
  if (case_insensitive && character2 && character1[0] != character2[0])
    if ((character1[0] ^ character2[0]) == ('A' ^ 'a'))
      {
        cg.AND (ES_CodeGenerator::IMMEDIATE (~('A' ^ 'a')), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
        cg.CMP (ES_CodeGenerator::IMMEDIATE (character1[0] & 0xffdf), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
      }
    else
      {
        ES_CodeGenerator::JumpTarget *use_success;

        if (success)
          use_success = success;
        else
          use_success = cg.ForwardJump ();

        cg.CMP (ES_CodeGenerator::IMMEDIATE (character1[0]), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
        cg.Jump (use_success, ES_NATIVE_CONDITION_EQUAL);
        cg.CMP (ES_CodeGenerator::IMMEDIATE (character2[0]), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);

        if (success)
          {
            cg.Jump (success, ES_NATIVE_CONDITION_EQUAL);
            if (failure)
              cg.Jump (failure);
          }
        else
          {
            cg.Jump (failure, ES_NATIVE_CONDITION_NOT_EQUAL);
            cg.SetJumpTarget (use_success);
          }

        return;
      }
  else
    cg.CMP (ES_CodeGenerator::IMMEDIATE (character1[0]), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);

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
RE_Native::EmitMatchPeriod (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  enum { LF = 10, CR = 13, LS = 0x2028, PS = 0x2029 };

  bool has_success = success != 0;
  bool has_failure = failure != 0;

  if (!success)
    success = cg.ForwardJump ();
  if (!failure)
    failure = cg.ForwardJump ();

  cg.MOVZX (MEMORY_CHARACTER (offset), REG_CHARACTER, ES_CodeGenerator::OPSIZE_16);
  cg.TEST (ES_CodeGenerator::IMMEDIATE (0xffff ^ (LF | CR | LS | PS)), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
  cg.Jump (success, ES_NATIVE_CONDITION_NOT_ZERO);

  cg.CMP (ES_CodeGenerator::IMMEDIATE (LF), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
  cg.Jump (failure, ES_NATIVE_CONDITION_EQUAL);
  cg.CMP (ES_CodeGenerator::IMMEDIATE (CR), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
  cg.Jump (failure, ES_NATIVE_CONDITION_EQUAL);

  cg.SUB (ES_CodeGenerator::IMMEDIATE (LS), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
  cg.TEST (ES_CodeGenerator::IMMEDIATE (~1), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);

  if (has_success)
    {
      cg.Jump (success, ES_NATIVE_CONDITION_NOT_ZERO);
      if (failure)
        cg.Jump (failure);
    }
  else
    cg.Jump (failure, ES_NATIVE_CONDITION_ZERO);

  if (!has_success)
    cg.SetJumpTarget (success);
  if (!has_failure)
    cg.SetJumpTarget (failure);
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

  cg.MOVZX (MEMORY_CHARACTER (offset), REG_CHARACTER, ES_CodeGenerator::OPSIZE_16);
  MatchCharacter (cg, &character1, cp2, success, failure, case_insensitive);
}

void
RE_Native::EmitMatchString (unsigned offset, const uni_char *string1, const uni_char *string2, unsigned string_length, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  unsigned index;

#ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
  if (string_length >= 4 && string_length <= 8)
    if (LoadedString *ls = FindLoadedString (string1))
      {
        cg.MOVDQU (MEMORY_CHARACTER (offset), ES_CodeGenerator::REG_XMM0);

        if (case_insensitive && string2)
          cg.ANDPD (ES_CodeGenerator::REG_XMM1, ES_CodeGenerator::REG_XMM0);

        cg.PCMPISTRM (ES_CodeGenerator::REG_XMM0, static_cast<ES_CodeGenerator::XMM> (ls->xmm), ES_CodeGenerator::SFMT_UNSIGNED_WORD, ES_CodeGenerator::AO_EQUAL_EACH, ES_CodeGenerator::POL_MASKED_NEGATIVE, ES_CodeGenerator::OUTPUT_LEAST_SIGNIFICANT);

        if (success)
          {
            cg.Jump (success, ES_NATIVE_CONDITION_NOT_CARRY);
            if (failure)
              cg.Jump (failure);
          }
        else
          cg.Jump (failure, ES_NATIVE_CONDITION_CARRY);

        return;
      }
#endif // RE_USE_PCMPxSTRx_INSTRUCTIONS

  for (index = 0; index < (string_length & ~1u); index += 2)
    {
      ES_CodeGenerator::JumpTarget *local_success;

      if (index + 2 == string_length)
        local_success = success;
      else
        local_success = 0;

      const uni_char *characters1 = string1 + index;
      const uni_char *characters2 = string2 ? string2 + index : 0;

      if (!case_insensitive || !string2 || (characters1[0] == characters2[0] || (characters1[0] ^ characters2[0]) == ('A' ^ 'a')) && (characters1[1] == characters2[1] ||  (characters1[1] ^ characters2[1]) == ('A' ^ 'a')))
        {
          cg.MOV (MEMORY_CHARACTER (offset + index), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
          MatchTwoCharacters (cg, characters1, characters2, local_success, failure, case_insensitive);
        }
      else
        {
          cg.MOVZX (MEMORY_CHARACTER (offset + index), REG_CHARACTER, ES_CodeGenerator::OPSIZE_16);
          MatchCharacter (cg, characters1, characters2, 0, failure, case_insensitive);
          cg.MOVZX (MEMORY_CHARACTER (offset + index + 1), REG_CHARACTER, ES_CodeGenerator::OPSIZE_16);
          MatchCharacter (cg, characters1 + 1, characters2 + 1, local_success, failure, case_insensitive);
        }
    }

  if (index != string_length)
    {
      cg.MOVZX (MEMORY_CHARACTER (offset + index), REG_CHARACTER, ES_CodeGenerator::OPSIZE_16);
      MatchCharacter (cg, string1 + index, string2 ? string2 + index : 0, success, failure, case_insensitive);
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

  cg.MOVZX (MEMORY_CHARACTER (offset), REG_CHARACTER, ES_CodeGenerator::OPSIZE_16);

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

#ifdef ARCHITECTURE_AMD64_UNIX
      cg.PUSH (ES_CodeGenerator::REG_DX);
      cg.PUSH (ES_CodeGenerator::REG_CX);
      cg.PUSH (ES_CodeGenerator::REG_DI);
      cg.PUSH (ES_CodeGenerator::REG_SI);
      cg.MovePointerIntoRegister (cls, ES_CodeGenerator::REG_DI);
      cg.MOV (REG_CHARACTER, ES_CodeGenerator::REG_SI, ES_CodeGenerator::OPSIZE_32);
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
      cg.PUSH (ES_CodeGenerator::REG_DX);
      cg.PUSH (ES_CodeGenerator::REG_CX);
      cg.PUSH (ES_CodeGenerator::REG_DI);
      cg.PUSH (ES_CodeGenerator::REG_SI);
      cg.MovePointerIntoRegister (cls, ES_CodeGenerator::REG_CX);
      cg.MOV (REG_CHARACTER, ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);
      if ((stack_space_used & 15) != 0)
        cg.SUB (ES_CodeGenerator::IMMEDIATE (8), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
#elif defined ES_USE_FASTCALL
      cg.PUSH (ES_CodeGenerator::REG_DX);
      cg.PUSH (ES_CodeGenerator::REG_CX);
      cg.MOV (ES_CodeGenerator::IMMEDIATE (cls), ES_CodeGenerator::REG_CX, ES_CodeGenerator::OPSIZE_POINTER);
      cg.MOV (REG_CHARACTER, ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);
#else // ES_USE_STDCALL
      cg.PUSH (ES_CodeGenerator::REG_DX);
      cg.PUSH (ES_CodeGenerator::REG_CX);
      cg.PUSH (REG_CHARACTER);
      cg.PUSH (ES_CodeGenerator::IMMEDIATE (cls));
#endif // ES_USE_FASTCALL

      cg.CALL (ViolateFunctionPointer (&RE_Class::ContainsCharacter));

#ifdef ARCHITECTURE_AMD64_UNIX
      cg.TEST (ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
      cg.POP (ES_CodeGenerator::REG_SI);
      cg.POP (ES_CodeGenerator::REG_DI);
      cg.POP (ES_CodeGenerator::REG_CX);
      cg.POP (ES_CodeGenerator::REG_DX);
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
      if ((stack_space_used & 15) != 0)
        cg.ADD (ES_CodeGenerator::IMMEDIATE (8), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
      cg.TEST (ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
      cg.POP (ES_CodeGenerator::REG_SI);
      cg.POP (ES_CodeGenerator::REG_DI);
      cg.POP (ES_CodeGenerator::REG_CX);
      cg.POP (ES_CodeGenerator::REG_DX);
#elif defined ES_USE_FASTCALL
      cg.TEST (ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
      cg.POP (ES_CodeGenerator::REG_CX);
      cg.POP (ES_CodeGenerator::REG_DX);
#else // ES_USE_STDCALL
      cg.ADD (ES_CodeGenerator::IMMEDIATE (2 * sizeof(void *)), ES_CodeGenerator::REG_SP);
      cg.TEST (ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
      cg.POP (ES_CodeGenerator::REG_CX);
      cg.POP (ES_CodeGenerator::REG_DX);
#endif // ES_USE_FASTCALL

      if (success)
        {
          cg.Jump (success, ES_NATIVE_CONDITION_NOT_ZERO);
          if (failure)
            {
              cg.Jump (failure);
              cg.EndOutOfOrderBlock (FALSE);
              slow_case = 0;
            }
          else
            cg.EndOutOfOrderBlock (TRUE);
        }
      else
        {
          cg.Jump (failure, ES_NATIVE_CONDITION_ZERO);
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
          cg.AND (ES_CodeGenerator::IMMEDIATE (~('A' ^ 'a')), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
          start = 'A', stop = 'Z' + 1;
        }
      else
        start = 0, stop = BITMAP_RANGE;

      unsigned character_offset = 0;

      for (index = start; index < stop; ++index)
        if (!bitmap[index] != !inverted)
          if (index + 1 < stop && !bitmap[index + 1] != !inverted)
            {
              unsigned range_length = 2;

              while (index + range_length < stop && !bitmap[index + range_length] != !inverted)
                ++range_length;

              if (index - character_offset != 0)
                cg.SUB (ES_CodeGenerator::IMMEDIATE (index - character_offset), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
              cg.CMP (ES_CodeGenerator::IMMEDIATE (range_length), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
              cg.Jump (use_success, ES_NATIVE_CONDITION_BELOW);

              character_offset = index;
              index += range_length - 1;
            }
          else
            {
              cg.CMP (ES_CodeGenerator::IMMEDIATE (index - character_offset), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
              cg.Jump (use_success, ES_NATIVE_CONDITION_EQUAL);
            }

      if (cls->HasHighCharacterMap ())
        {
          if (case_insensitive && has_alpha && !has_nonalpha)
            cg.MOVZX (MEMORY_CHARACTER (offset), REG_CHARACTER, ES_CodeGenerator::OPSIZE_16);
          cg.CMP (ES_CodeGenerator::IMMEDIATE (BITMAP_RANGE - character_offset), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
          cg.Jump (use_failure, ES_NATIVE_CONDITION_LESS);

          if (character_offset)
            cg.ADD (ES_CodeGenerator::IMMEDIATE (character_offset), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);

          cg.Jump (high_character);
          jump_to_failure = false;
        }
    }
  else
    {
      cg.TEST (ES_CodeGenerator::IMMEDIATE (~BITMAP_MASK), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
      cg.Jump (high_character, ES_NATIVE_CONDITION_NOT_ZERO);

#ifdef ARCHITECTURE_AMD64
      if (reinterpret_cast<UINTPTR> (cls->GetBitmap ()) > INT_MAX)
        {
          cg.MovePointerIntoRegister (cls->GetBitmap (), REG_SCRATCH1);
          cg.CMP (ES_CodeGenerator::IMMEDIATE (!!inverted), ES_CodeGenerator::MEMORY (REG_SCRATCH1, REG_CHARACTER, ES_CodeGenerator::SCALE_1), ES_CodeGenerator::OPSIZE_8);
        }
      else
#endif // ARCHITECTURE_AMD64
        cg.CMP (ES_CodeGenerator::IMMEDIATE (!!inverted), ES_CodeGenerator::MEMORY (REG_CHARACTER, reinterpret_cast<UINTPTR> (cls->GetBitmap ())), ES_CodeGenerator::OPSIZE_8);

      if (success)
        {
          cg.Jump (success, ES_NATIVE_CONDITION_NOT_ZERO);
          if (failure)
            cg.Jump (failure);
          else
            cg.SetJumpTarget (use_failure);
          if (slow_case)
            cg.SetOutOfOrderContinuationPoint (slow_case);
        }
      else
        {
          cg.Jump (failure, ES_NATIVE_CONDITION_ZERO);
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
MatchWordCharacter (ES_CodeGenerator &cg, int offset, ES_CodeGenerator::Register result)
{
  ES_CodeGenerator::JumpTarget *success = cg.ForwardJump ();

  cg.MOVZX (MEMORY_CHARACTER (offset), REG_CHARACTER, ES_CodeGenerator::OPSIZE_16);
  cg.MOV (ES_CodeGenerator::IMMEDIATE (1), result, ES_CodeGenerator::OPSIZE_32);

  cg.SUB (ES_CodeGenerator::IMMEDIATE ('0'), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
  cg.CMP (ES_CodeGenerator::IMMEDIATE ('9' - '0'), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
  cg.Jump (success, ES_NATIVE_CONDITION_BELOW_EQUAL);

  cg.CMP (ES_CodeGenerator::IMMEDIATE ('_' - '0'), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
  cg.Jump (success, ES_NATIVE_CONDITION_EQUAL);

  cg.ADD (ES_CodeGenerator::IMMEDIATE ('0'), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
  cg.AND (ES_CodeGenerator::IMMEDIATE (~('A' ^ 'a')), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
  cg.SUB (ES_CodeGenerator::IMMEDIATE ('A'), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
  cg.CMP (ES_CodeGenerator::IMMEDIATE ('Z' - 'A'), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
  cg.Jump (success, ES_NATIVE_CONDITION_BELOW_EQUAL);

  cg.XOR (result, result, ES_CodeGenerator::OPSIZE_32);
  cg.SetJumpTarget (success);
}

void
RE_Native::EmitAssertWordEdge (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  ES_CodeGenerator::JumpTarget *use_success;

  if (success)
    use_success = success;
  else
    use_success = cg.ForwardJump ();

  ES_CodeGenerator::JumpTarget *compare_b = cg.ForwardJump (), *check_characters = cg.ForwardJump ();

  /* Patterned on re_matcher logic for same: comparing characters at (index-1) and index, but
     avoid peeking beyond the end of either side of the input buffer. */

  cg.MOV (ES_CodeGenerator::ZERO (), REG_SCRATCH1, ES_CodeGenerator::OPSIZE_32);
  cg.MOV (ES_CodeGenerator::ZERO (), REG_SCRATCH2, ES_CodeGenerator::OPSIZE_32);
  if (offset == 0)
    {
      cg.CMP (ES_CodeGenerator::ZERO (), REG_INDEX, ES_CodeGenerator::OPSIZE_32);
      cg.Jump (compare_b, ES_NATIVE_CONDITION_EQUAL);
    }

  MatchWordCharacter (cg, static_cast<int> (offset) - 1, REG_SCRATCH1);

  cg.SetJumpTarget (compare_b);
  cg.CMP (ES_CodeGenerator::ZERO (), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
  cg.Jump (check_characters, ES_NATIVE_CONDITION_BELOW);

  MatchWordCharacter (cg, static_cast<int> (offset), REG_SCRATCH2);

  cg.SetJumpTarget (check_characters);
  cg.CMP (REG_SCRATCH1, REG_SCRATCH2, ES_CodeGenerator::OPSIZE_32);

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
          cg.TEST (REG_INDEX, REG_INDEX, ES_CodeGenerator::OPSIZE_32);

          if (success)
            {
              cg.Jump (success, ES_NATIVE_CONDITION_ZERO);
              if (failure && !multiline)
                cg.Jump (failure);
            }
          else if (multiline)
            cg.Jump (use_success, ES_NATIVE_CONDITION_ZERO);
          else
            cg.Jump (failure, ES_NATIVE_CONDITION_NOT_ZERO);
        }

      if (multiline)
        {
          enum { LF = 10, CR = 13, LS = 0x2028, PS = 0x2029 };

          cg.MOVZX (MEMORY_CHARACTER (static_cast<int> (offset) - 1), REG_CHARACTER, ES_CodeGenerator::OPSIZE_16);
          cg.CMP (ES_CodeGenerator::IMMEDIATE (LF), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
          cg.Jump (use_success, ES_NATIVE_CONDITION_EQUAL);
          cg.CMP (ES_CodeGenerator::IMMEDIATE (CR), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
          cg.Jump (use_success, ES_NATIVE_CONDITION_EQUAL);
          cg.SUB (ES_CodeGenerator::IMMEDIATE (LS), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
          cg.TEST (ES_CodeGenerator::IMMEDIATE (~1), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);

          if (success)
            {
              cg.Jump (success, ES_NATIVE_CONDITION_ZERO);
              if (failure)
                cg.Jump (failure);
            }
          else
            {
              cg.Jump (failure, ES_NATIVE_CONDITION_NOT_ZERO);
              if (use_success)
                cg.SetJumpTarget (use_success);
            }
        }
    }
  else if (failure)
    cg.Jump (failure);
}

void
RE_Native::EmitAssertLineEnd (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  if (global_fixed_length && offset != segment_length && !multiline)
    {
      /* This means we had a $ with something following it in a fixed-length
         non-multiline expression.  Such an expression cannot possibly match.
         We could obviously generate much simpler code for expressions that
         cannot match, but since they probably only appear in test suites, it
         doesn't really matter. */

      if (failure)
        cg.Jump (failure);
    }
  else
    {
      if (offset == 0 || global_fixed_length)
        cg.TEST (REG_LENGTH, REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
      else
        cg.CMP (ES_CodeGenerator::IMMEDIATE (offset), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);

      ES_CodeGenerator::Condition true_condition, false_condition;
      ES_CodeGenerator::JumpTarget *use_success = 0;

      if (multiline)
        {
          if (success)
            use_success = success;
          else
            use_success = cg.ForwardJump ();

          enum { LF = 10, CR = 13, LS = 0x2028, PS = 0x2029 };

          cg.Jump (use_success, ES_NATIVE_CONDITION_ZERO);
          cg.MOVZX (MEMORY_CHARACTER (offset), REG_CHARACTER, ES_CodeGenerator::OPSIZE_16);
          cg.CMP (ES_CodeGenerator::IMMEDIATE (LF), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
          cg.Jump (use_success, ES_NATIVE_CONDITION_EQUAL);
          cg.CMP (ES_CodeGenerator::IMMEDIATE (CR), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
          cg.Jump (use_success, ES_NATIVE_CONDITION_EQUAL);
          cg.SUB (ES_CodeGenerator::IMMEDIATE (LS), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);
          cg.TEST (ES_CodeGenerator::IMMEDIATE (~1), REG_CHARACTER, ES_CodeGenerator::OPSIZE_32);

          true_condition = ES_NATIVE_CONDITION_ZERO;
          false_condition = ES_NATIVE_CONDITION_NOT_ZERO;
        }
      else
        {
          true_condition = ES_NATIVE_CONDITION_ZERO;
          false_condition = ES_NATIVE_CONDITION_NOT_ZERO;
        }

      if (success)
        {
          cg.Jump (success, true_condition);
          if (failure)
            cg.Jump (failure);
        }
      else
        {
          cg.Jump (failure, false_condition);
          if (use_success)
            cg.SetJumpTarget (use_success);
        }
    }
}

void
RE_Native::EmitCaptureStart (unsigned capture_index, unsigned offset, bool initialize_end)
{
#ifndef ARCHITECTURE_AMD64
  cg.MOV (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, stack_space_used), REG_RESULT, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

  ES_CodeGenerator::Operand start (ES_CodeGenerator::MEMORY (REG_RESULT, (capture_index + 1) * 2 * sizeof (unsigned)));
  ES_CodeGenerator::Operand length (ES_CodeGenerator::MEMORY (REG_RESULT, (capture_index + 1) * 2 * sizeof (unsigned) + sizeof (unsigned)));

  if (offset == 0)
    cg.MOV (REG_INDEX, start, ES_CodeGenerator::OPSIZE_32);
  else
    {
      cg.LEA (ES_CodeGenerator::MEMORY (REG_INDEX, offset), REG_SCRATCH2, ES_CodeGenerator::OPSIZE_32);
      cg.MOV (REG_SCRATCH2, start, ES_CodeGenerator::OPSIZE_32);
    }

  if (initialize_end)
    cg.MOV (ES_CodeGenerator::IMMEDIATE (-1), length, ES_CodeGenerator::OPSIZE_32);
}

void
RE_Native::EmitCaptureEnd (unsigned capture_index, unsigned offset)
{
#ifndef ARCHITECTURE_AMD64
  cg.MOV (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, stack_space_used), REG_RESULT, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

  ES_CodeGenerator::Operand start (ES_CodeGenerator::MEMORY (REG_RESULT, (capture_index + 1) * 2 * sizeof (unsigned)));
  ES_CodeGenerator::Operand length (ES_CodeGenerator::MEMORY (REG_RESULT, (capture_index + 1) * 2 * sizeof (unsigned) + sizeof (unsigned)));

  if (offset == 0)
    cg.MOV (REG_INDEX, REG_SCRATCH2, ES_CodeGenerator::OPSIZE_32);
  else
    cg.LEA (ES_CodeGenerator::MEMORY (REG_INDEX, offset), REG_SCRATCH2, ES_CodeGenerator::OPSIZE_32);

  cg.SUB (start, REG_SCRATCH2, ES_CodeGenerator::OPSIZE_32);
  cg.MOV (REG_SCRATCH2, length, ES_CodeGenerator::OPSIZE_32);
}

void
RE_Native::EmitCaptureReset (unsigned capture_index)
{
#ifndef ARCHITECTURE_AMD64
  cg.MOV (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, stack_space_used), REG_RESULT, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

  ES_CodeGenerator::Operand length (ES_CodeGenerator::MEMORY (REG_RESULT, (capture_index + 1) * 2 * sizeof (unsigned) + sizeof (unsigned)));

  cg.MOV (ES_CodeGenerator::IMMEDIATE (-1), length, ES_CodeGenerator::OPSIZE_32);
}

void
RE_Native::EmitResetUnreachedCaptures (unsigned first_reached, unsigned last_reached, unsigned captures_count)
{
#ifndef ARCHITECTURE_AMD64
  cg.MOV (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, stack_space_used), REG_RESULT, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

  for (unsigned capture_index = 0; capture_index < captures_count; ++capture_index)
    if (capture_index < first_reached || capture_index > last_reached)
      {
        ES_CodeGenerator::Operand length (ES_CodeGenerator::MEMORY (REG_RESULT, (capture_index + 1) * 2 * sizeof (unsigned) + sizeof (unsigned)));
        cg.MOV (ES_CodeGenerator::IMMEDIATE (-1), length, ES_CodeGenerator::OPSIZE_32);
      }
}

void
RE_Native::EmitResetSkippedCaptures (unsigned first_skipped, unsigned last_skipped)
{
#ifndef ARCHITECTURE_AMD64
  cg.MOV (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, stack_space_used), REG_RESULT, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

  for (unsigned capture_index = first_skipped; capture_index <= last_skipped; ++capture_index)
    {
      ES_CodeGenerator::Operand length (ES_CodeGenerator::MEMORY (REG_RESULT, (capture_index + 1) * 2 * sizeof (unsigned) + sizeof (unsigned)));
      cg.MOV (ES_CodeGenerator::IMMEDIATE (-1), length, ES_CodeGenerator::OPSIZE_32);
    }
}

bool
RE_Native::EmitLoop (BOOL backtracking, unsigned min, unsigned max, unsigned offset, LoopBody &body, LoopTail &tail, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure)
{
  if (backtracking && max != 1)
    {
      /* FIXME: these are unnecessary restrictions. */
      if (min != 0 || max != UINT_MAX)
        return false;

      ES_CodeGenerator::JumpTarget *loop_start = cg.ForwardJump ();

      cg.PUSH (REG_LENGTH);
      cg.PUSH (REG_INDEX);

      stack_space_used += 2 * sizeof(void *);

      if (offset != 0)
        {
          cg.ADD (ES_CodeGenerator::IMMEDIATE (offset), REG_INDEX, ES_CodeGenerator::OPSIZE_32);
          cg.SUB (ES_CodeGenerator::IMMEDIATE (offset), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
          cg.PUSH (REG_INDEX);

          stack_space_used += 1 * sizeof(void *);
        }
      else
        cg.TEST (REG_LENGTH, REG_LENGTH, ES_CodeGenerator::OPSIZE_32);

      cg.Jump (loop_start, ES_NATIVE_CONDITION_NOT_ZERO);

      ES_CodeGenerator::JumpTarget *loop_break = cg.ForwardJump (), *loop_eos;

      if (failure == jt_search)
        {
          ES_CodeGenerator::OutOfOrderBlock *oob_eos = cg.StartOutOfOrderBlock ();

          cg.MOV (ES_CodeGenerator::IMMEDIATE (-1), ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP, (offset != 0 ? 2 : 1) * sizeof(void *)), ES_CodeGenerator::OPSIZE_POINTER);
          cg.Jump (loop_break);
          cg.EndOutOfOrderBlock (FALSE);

          loop_eos = oob_eos->GetJumpTarget ();
        }
      else
        loop_eos = loop_break;

      cg.Jump (loop_eos);

      ES_CodeGenerator::JumpTarget *loop_continue = cg.Here (TRUE);

      cg.ADD (ES_CodeGenerator::IMMEDIATE (1), REG_INDEX, ES_CodeGenerator::OPSIZE_32);
      cg.SUB (ES_CodeGenerator::IMMEDIATE (1), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
      cg.Jump (loop_eos, ES_NATIVE_CONDITION_ZERO);
      cg.SetJumpTarget (loop_start);

      GenerateLoopBody (body, 0, loop_continue, loop_break);

      ES_CodeGenerator::JumpTarget *loop_backtrack = cg.Here (), *loop_failure = cg.ForwardJump (), *loop_success = cg.ForwardJump ();

      cg.SUB (ES_CodeGenerator::IMMEDIATE (1), REG_INDEX, ES_CodeGenerator::OPSIZE_POINTER);
      cg.ADD (ES_CodeGenerator::IMMEDIATE (1), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
      cg.CMP (ES_CodeGenerator::MEMORY (ES_CodeGenerator::REG_SP), REG_INDEX, ES_CodeGenerator::OPSIZE_POINTER);
      cg.Jump (loop_failure, ES_NATIVE_CONDITION_LESS);
      cg.SetJumpTarget (loop_break);

      if (!GenerateLoopTail (tail, 0, loop_success, loop_backtrack, false))
        return false;

      cg.SetJumpTarget (loop_success);
      if (offset != 0)
        cg.POP (REG_SCRATCH2);
      cg.MOV (REG_INDEX, REG_SCRATCH1, ES_CodeGenerator::OPSIZE_32);
      cg.POP (REG_INDEX);
      cg.POP (REG_SCRATCH2); // original REG_LENGTH, but we don't care any more
      cg.SUB (REG_INDEX, REG_SCRATCH1, ES_CodeGenerator::OPSIZE_32); // calculate distance from start of match to end of loop
      cg.ADD (REG_SCRATCH1, REG_LENGTH, ES_CodeGenerator::OPSIZE_32); // and add to length matched after loop
      cg.Jump (success);

      cg.SetJumpTarget (loop_failure);
      if (offset != 0)
        cg.POP (REG_SCRATCH2);
      cg.POP (REG_INDEX);
      cg.POP (REG_LENGTH);
      cg.CMP (ES_CodeGenerator::ZERO (), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
      cg.Jump (jt_failure, ES_NATIVE_CONDITION_LESS);
      cg.Jump (failure);

      stack_space_used -= (offset != 0 ? 3 : 2) * sizeof (void *);

      return true;
    }

  if (min == 0 && max == 1)
    {
      ES_CodeGenerator::JumpTarget *local_failure = cg.ForwardJump ();

      OP_ASSERT (!segment_fixed_length);

      EmitCheckSegmentLength (offset + 1 + tail.length, local_failure);

      GenerateLoopBody (body, offset, 0, local_failure);

      if (!GenerateLoopTail (tail, offset + 1, success, local_failure, true))
        return false;

      cg.SetJumpTarget (local_failure, TRUE);

      if (!GenerateLoopTail (tail, offset, success, failure))
        return false;

      return true;
    }

  if (min != max && max != UINT_MAX)
    /* FIXME: Should support maximum properly, I suppose. */
    return false;

  if (!segment_fixed_length)
    {
      cg.PUSH (REG_INDEX);
      cg.PUSH (REG_LENGTH);

      stack_space_used += 2 * sizeof(void *);
    }

  ES_CodeGenerator::JumpTarget *loop_failure, *loop_end = cg.ForwardJump (), *loop_success, *loop_start = cg.ForwardJump ();

  if (segment_fixed_length)
    {
      loop_success = success;
      loop_failure = failure;
    }
  else
    {
      loop_success = cg.ForwardJump ();
      loop_failure = cg.ForwardJump ();
    }

  bool need_length_compare = true;

  if (offset != 0 && !segment_fixed_length)
    {
      need_length_compare = false;

      cg.ADD (ES_CodeGenerator::IMMEDIATE (offset), REG_INDEX, ES_CodeGenerator::OPSIZE_32);
      cg.SUB (ES_CodeGenerator::IMMEDIATE (offset), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);

      offset = 0;
    }

  if (min != 0)
    {
      if (!segment_fixed_length)
        {
          cg.CMP (ES_CodeGenerator::IMMEDIATE (min), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
          cg.Jump (loop_end, ES_NATIVE_CONDITION_LESS);
        }

      if (min <= 4)
        {
          unsigned loop_index;

          for (loop_index = 0; loop_index < min; ++loop_index)
            {
              ES_CodeGenerator::JumpTarget *use_success = 0;

              if (loop_index + 1 == max)
                if (segment_fixed_length && tail.is_empty)
                  use_success = loop_success;

              GenerateLoopBody (body, offset + loop_index, use_success, loop_failure);

              if (use_success)
                return true;
            }

          if (segment_fixed_length)
            offset += loop_index;
          else
            {
              cg.ADD (ES_CodeGenerator::IMMEDIATE (min), REG_INDEX, ES_CodeGenerator::OPSIZE_32);
              cg.SUB (ES_CodeGenerator::IMMEDIATE (min), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
            }
        }
      else
        /* FIXME: We could of course support this.  But how common is min!=1,
           really, and when it isn't 1, how often is it more than four? */
        return false;

      if (min < max)
        {
          cg.Jump (loop_start, ES_NATIVE_CONDITION_NOT_ZERO);
          cg.Jump (loop_end);
        }
    }
  else
    {
      if (need_length_compare)
        cg.TEST (REG_LENGTH, REG_LENGTH, ES_CodeGenerator::OPSIZE_32);

      cg.Jump (loop_start, ES_NATIVE_CONDITION_NOT_ZERO);
      cg.Jump (loop_end);
    }

  if (min != max)
    {
      OP_ASSERT (!segment_fixed_length);

      ES_CodeGenerator::JumpTarget *loop_continue = cg.Here (TRUE);

      cg.ADD (ES_CodeGenerator::IMMEDIATE (1), REG_INDEX, ES_CodeGenerator::OPSIZE_32);
      cg.SUB (ES_CodeGenerator::IMMEDIATE (1), REG_LENGTH, ES_CodeGenerator::OPSIZE_32);
      cg.Jump (loop_end, ES_NATIVE_CONDITION_ZERO);

      cg.SetJumpTarget (loop_start);

      GenerateLoopBody (body, 0, loop_continue, loop_end);
    }

  cg.SetJumpTarget (loop_end);

  if (!GenerateLoopTail (tail, offset, loop_success, loop_failure))
    return false;

  if (loop_success != success)
    {
      cg.SetJumpTarget (loop_success);
      if (!segment_fixed_length)
        {
          cg.POP (REG_SCRATCH1); // original REG_LENGTH, but we don't care any more
          cg.MOV (REG_INDEX, REG_SCRATCH1, ES_CodeGenerator::OPSIZE_32);
          cg.POP (REG_INDEX);
          cg.SUB (REG_INDEX, REG_SCRATCH1, ES_CodeGenerator::OPSIZE_32); // calculate distance from start of match to end of loop
          cg.ADD (REG_SCRATCH1, REG_LENGTH, ES_CodeGenerator::OPSIZE_32); // and add to length matched after loop
        }
      if (success)
        cg.Jump (success);
    }

  if (!segment_fixed_length)
    {
      stack_space_used -= 2 * sizeof (void *);

      if (!success)
        cg.StartOutOfOrderBlock ();

      cg.SetJumpTarget (loop_failure);
      cg.POP (REG_LENGTH);
      cg.POP (REG_INDEX);
      cg.Jump (failure);

      if (!success)
        cg.EndOutOfOrderBlock (FALSE);
    }

  return true;
}

void
RE_Native::Finish (void *base)
{
#ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
  if (upcase_blob)
    {
      uni_char *dst = static_cast<uni_char *> (cg.GetBlobStorage (base, upcase_blob));

      for (unsigned index = 0; index < 8; ++index)
        dst[index] = 0xffdf;
    }

  LoadedString *ls = first_loaded_string;

  while (ls)
    {
      uni_char *dst = static_cast<uni_char *> (cg.GetBlobStorage (base, ls->blob));
      const uni_char *src = ls->string;
      unsigned index;

      if (case_insensitive)
        for (index = 0; index < ls->string_length; ++index)
          dst[index] = src[index] & 0xffdf;
      else
        for (index = 0; index < ls->string_length; ++index)
          dst[index] = src[index];

      for (; index < 8; ++index)
        dst[index] = 0;

      ls = ls->next;
    }
#endif // RE_USE_PCMPxSTRx_INSTRUCTIONS
}

#endif // ARCHITECTURE_IA32
#endif // RE_FEATURE__MACHINE_CODED
