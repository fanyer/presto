/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/regexp/src/re_config.h"
#include "modules/regexp/src/re_matcher.h"
#include "modules/regexp/src/re_object.h"
#include "modules/regexp/src/re_class.h"
#include "modules/regexp/include/regexp_advanced_api.h"

RE_Matcher::RE_Matcher ()
  : suspend (0),
    choice (0),
    free_choice (0),
    captures (0),
    free_capture (0),
    loops_count (0),
    loops (0),
    serial (0),
    object (0),
    loop_with_min_repeated (false)
{
}

RE_Matcher::~RE_Matcher ()
{
  Cleanup ();
}

void
RE_Matcher::SetObjectL (RE_Object *o, bool case_insensitive_, bool multiline_)
{
  object = o;

  case_insensitive = case_insensitive_;
  multiline = multiline_;

  bytecode = o->GetBytecode ();
  bytecode_segments = o->GetBytecodeSegments ();

  if (!bytecode_segments)
    bytecode_segments = &bytecode;

  string_lengths = o->GetStringLengths ();
  strings = o->GetStrings ();
  alternative_strings = o->GetAlternativeStrings ();
  classes = o->GetClasses ();

  captures_count = o->GetCaptures ();
  if (captures_count)
    {
      if (suspend)
        {
          captures = static_cast<Capture **> (suspend->AllocateL (sizeof (Capture *) * captures_count));
          AllocateCapturesL ();
        }
      else
        captures = OP_NEWA_L (Capture *, captures_count);

      for (unsigned i = 0; i < captures_count; ++i)
        {
          Capture *c;

          if (suspend)
            {
              OP_ASSERT (free_capture);

              c = free_capture;
              free_capture = c->previous;
            }
          else
            c = OP_NEW(Capture, ());

          captures[i] = c;
          if (c)
            {
              c->start = UINT_MAX;
              c->end = c->start_serial = c->end_serial = 0;
              c->previous = 0;
            }
          else
            {
              // Fill the rest of the pointers with null to avoid crashes in the cleanup
              i++;
              while (i < captures_count)
                captures[i++] = 0;
              LEAVE(OpStatus::ERR_NO_MEMORY);
            }
        }
    }

  loops_count = o->GetLoops ();

  if (suspend)
    loops = static_cast<Loop *> (suspend->AllocateL (sizeof (Loop) * loops_count));
  else
    loops = OP_NEWA_L (Loop, loops_count);

  op_memset (loops, 0, sizeof (Loop) * loops_count);
}

void
RE_Matcher::Cleanup ()
{
  if (!suspend)
    {
      Choice *c;

      c = choice;
      while (c)
        {
          Choice *d = c->previous;
          OP_DELETE(c);
          c = d;
        }

      c = free_choice;
      while (c)
        {
          Choice *d = c->previous;
          OP_DELETE(c);
          c = d;
        }

      if (captures)
        {
          for (unsigned i = 0; i < captures_count; ++i)
            {
              Capture *d = captures[i];
              while (d)
                {
                  Capture *previous = d->previous;
                  OP_DELETE(d);
                  d = previous;
                }
            }

          OP_DELETEA(captures);
        }

      Capture *d = free_capture;
      while (d)
        {
          Capture *c = d->previous;
          OP_DELETE(d);
          d = c;
        }

      OP_DELETEA(loops);
    }

  choice = 0;
  free_choice = 0;
  captures = 0;
  free_capture = 0;
  loops = 0;
}

RE_Matcher::ExecuteResult
RE_Matcher::ExecuteL (const unsigned *address_, unsigned index_, unsigned length_)
{
  unsigned maximum_loops_taken = 0xffffu;

  unsigned index = index_, length = length_;
  const unsigned *address = address_;

#ifdef RE_FEATURE__DISASSEMBLER
#if 0
  OpString8 disassembly8; disassembly8.SetUTF8FromUTF16 (object->GetDisassembly ());
  printf ("%s\n", disassembly8.CStr());
#endif // 0
#endif // RE_FEATURE__DISASSEMBLER

  while (1)
    {
      unsigned code = *address++;
      RE_Instructions::Instruction instruction = (RE_Instructions::Instruction) (code & 0xffu);
      unsigned argument = code >> 8;

#define ReadUnsigned() (*address++)
#define ReadSigned() ((int) *address++)

      int offset;
      unsigned loop_index;
      unsigned min;
      unsigned max;
      bool end = index == length;

      switch (instruction)
        {
        case RE_Instructions::MATCH_PERIOD:
          if (end || IsLineTerminator (string[index]))
            goto failure;
          ++index;
          continue;

        case RE_Instructions::MATCH_CHARACTER_CS:
          if (!end && argument == string[index])
            {
              ++index;
              continue;
            }
          else if (choice && choice->address == address - 1 && choice->count != 0 && choice->index + (1 + choice->count) * choice->additional == index)
            if (Backtrack (argument, -1, 0, index))
              continue;
          goto failure;

        case RE_Instructions::MATCH_CHARACTER_CI:
          argument = ReadUnsigned ();
          if (!end && MatchCharacterCI (argument, string[index]))
            {
              ++index;
              continue;
            }
          else if (choice && choice->address == address - 2 && choice->count != 0 && choice->index + (1 + choice->count) * choice->additional == index)
            if (Backtrack (argument & 0xffffu, argument >> 16, 0, index))
              continue;
          goto failure;

        case RE_Instructions::MATCH_CHARACTER_CI_SLOW:
          if (!end && MatchCharacterCISlow (argument, string[index]))
            {
              ++index;
              continue;
            }
          goto failure;

        case RE_Instructions::MATCH_STRING_CS:
          if (!end && MatchStringCS (argument, index))
            continue;
          goto failure;

        case RE_Instructions::MATCH_STRING_CI:
          if (!end && MatchStringCI (argument, index))
            continue;
          goto failure;

        case RE_Instructions::MATCH_CLASS:
          if (!end && MatchClass (classes[argument], string[index]))
            {
              ++index;
              continue;
            }
          else if (choice && choice->address == address - 1 && choice->count != 0 && choice->index + (choice->count + 1) * choice->additional == index)
            if (Backtrack (-1, -1, classes[argument], index))
              continue;
          goto failure;

        case RE_Instructions::MATCH_CAPTURE:
          if (!MatchCapture (argument, index))
            goto failure;
          continue;

        case RE_Instructions::ASSERT_LINE_START:
          if (index != 0 && (!multiline || !IsLineTerminator (string[index - 1])))
            goto failure;
          continue;

        case RE_Instructions::ASSERT_LINE_END:
          if (!end && (!multiline || !IsLineTerminator (string[index])))
            goto failure;
          continue;

        case RE_Instructions::ASSERT_WORD_EDGE:
        case RE_Instructions::ASSERT_NOT_WORD_EDGE:
          {
            bool a = index > 0 && IsWordChar (string[index - 1]);
            bool b = index < length && IsWordChar (string[index]);

            if ((instruction == RE_Instructions::ASSERT_WORD_EDGE) == (a == b))
              goto failure;
          }
          continue;

        case RE_Instructions::PUSH_CHOICE:
        case RE_Instructions::PUSH_CHOICE_MARK:
          PushChoiceL (address + (int) argument, ~0u, index, instruction == RE_Instructions::PUSH_CHOICE_MARK);
          continue;

        case RE_Instructions::POP_CHOICE_MARK:
          index = PopChoiceMark ();
          continue;

        case RE_Instructions::START_LOOP:
          min = ReadUnsigned ();
          max = ReadUnsigned ();
          StartLoop (argument >> 1, index, min, max, argument & 1);
          continue;

        case RE_Instructions::LOOP_GREEDY:
        case RE_Instructions::LOOP:
          offset = ReadSigned ();
          if (!ContinueLoopL (argument, index, address, offset, instruction != RE_Instructions::LOOP))
            goto failure;
          continue;

        case RE_Instructions::LOOP_PERIOD:
          min = ReadUnsigned ();
          max = ReadUnsigned ();
          if (!LoopPeriod (argument, min, max, index, address))
            goto failure;
          continue;

        case RE_Instructions::LOOP_CHARACTER_CS:
        case RE_Instructions::LOOP_CHARACTER_CI:
          loop_index = ReadUnsigned ();
          min = ReadUnsigned ();
          max = ReadUnsigned ();
          if (!LoopCharacter (loop_index, instruction == RE_Instructions::LOOP_CHARACTER_CI, argument, min, max, index, address))
            goto failure;
          continue;

        case RE_Instructions::LOOP_CLASS:
          loop_index = ReadUnsigned ();
          min = ReadUnsigned ();
          max = ReadUnsigned ();
          if (!LoopClass (argument, loop_index, min, max, index, address))
            goto failure;
          continue;

        case RE_Instructions::JUMP:
          address += (int) argument;
          continue;

        case RE_Instructions::CAPTURE_START:
          CaptureStartL (argument >> 1, (argument & 1) ? index : UINT_MAX);
          continue;

        case RE_Instructions::CAPTURE_END:
          captures[argument]->end = index;
          captures[argument]->end_serial = serial;
          continue;

        case RE_Instructions::RESET_LOOP:
          loop_index = argument >> 1;
          if ((argument & 1) == 1 && !loop_with_min_repeated)
            continue;
          loops[loop_index].previous_start = length + 1;
          loops[loop_index].pcp_count = 0;
          if (loops[loop_index].last_choice)
            loops[loop_index].last_choice->allow_loop_repeat = true;
          continue;

        case RE_Instructions::SUCCESS:
          end_index = index;
          return EXECUTE_SUCCESS;

        case RE_Instructions::FAILURE:
          goto failure;
        }

    failure:
      if (choice && choice->loop_values)
        PopChoice ();

      if (choice)
        {
          address = choice->address;
          index = choice->GetIndex ();

          if (captures_count)
            RewindCaptures ();

          PopChoice ();

          if (!--maximum_loops_taken)
            if (suspend)
              {
                suspend->Yield ();
                maximum_loops_taken = 0xffffu;
              }
            else
              return EXECUTE_FAILURE;
        }
      else
        return EXECUTE_FAILURE;
    }
}

void
RE_Matcher::GetCapture (unsigned index, unsigned &start, unsigned &length)
{
  Capture *c = captures[index];

  if (c->start != UINT_MAX)
    {
      start = c->start;
      length = c->end - c->start;
    }
  else
    length = UINT_MAX;
}

unsigned
RE_Matcher::PopChoiceMark ()
{
  unsigned serial_before = serial;

  Choice *c = choice;
  while (!c->mark)
    {
      Choice *d = c->previous;
      unsigned loop_index = c->loop_index;

      if (loop_index != ~0u)
        {
          if (loops[loop_index].last_choice == c)
            loops[loop_index].last_choice = c->previous_choice_for_loop;
          if (c->loop_values)
            loops[loop_index].count = c->loop_count;
        }

      serial -= 1 + c->count;

      c->previous = free_choice;
      free_choice = c;

      c = d;
    }

  choice = c;

  unsigned index = c->index;

  PopChoice ();

  Capture **cp = captures, **cpe = captures + captures_count;
  while (cp != cpe)
    {
      Capture *c = *cp;

      if (c->start_serial <= serial_before && c->start_serial > serial)
        c->start_serial = c->end_serial = serial;

      ++cp;
    }

  return index;
}

void
RE_Matcher::CaptureStartL (unsigned capture_index, unsigned index)
{
  Capture *c = captures[capture_index];

  if (serial > c->start_serial)
    {
      Capture *d;

      if (!free_capture)
        AllocateCapturesL ();

      d = free_capture;
      free_capture = d->previous;

      captures[capture_index] = d;

      d->previous = c;
      c = d;
    }

  c->start = index;
  c->end = 0;
  c->start_serial = serial;
  c->end_serial = serial;
}

void
RE_Matcher::RewindCaptures ()
{
  Capture **cp = captures, **cpe = captures + captures_count;
  while (cp != cpe)
    {
      Capture *c = *cp;

      while (c->start_serial >= serial)
        {
          Capture *d = c->previous;

          c->previous = free_capture;
          free_capture = c;

          c = d;
        }

      if (c && c->end_serial >= serial)
        {
          c->end = 0;
          c->end_serial = c->start_serial;
        }

      *cp++ = c;
    }
}

static unsigned
RE_GetAlternativeCharacter (unsigned character)
{
  unsigned alternative = uni_tolower (character);
  if (alternative == character)
    alternative = uni_toupper (character);
  return alternative;
}

bool
RE_Matcher::LoopPeriod (unsigned loop_index, unsigned min, unsigned max, unsigned &index, const unsigned *&address)
{
  unsigned i = index, len = length;

  Loop *l = loops + loop_index;
  Choice *c = l->last_choice;

  unsigned ps = l->previous_start;

  if (c)
    {
      if (i == c->GetIndex () + 1)
        if (max == ~0u || c->count < max - min - 1)
          if (!c->allow_loop_repeat)
            return false;
          else if (i + min < len)
            len = i + min;
    }
  else if (i == ps)
    return false;
  else if (i < ps)
    len = ps - 1;

  unsigned avail = len - index;
  const uni_char *s = string + i;

  if (min != 0)
    {
      if (avail < min)
        return false;

      const uni_char *sm = s + min;

      while (s != sm)
        if (!IsLineTerminator (*s))
          ++s;
        else
          return false;

      if (min == max)
        {
          index = s - string;
          return true;
        }
    }

  l->previous_start = i + min;

  bool limited_by_max = false;

  if (max != ~0u && max < avail)
    {
      avail = max;
      limited_by_max = true;
    }

  const uni_char *se = s + (avail - min);
  bool push_choice = true;

  if (s != se && !IsLineTerminator (*s))
    {
      int character = -1, alternative = -1;
      bool match_character = false;

      switch (*address & 0xffu)
        {
        case RE_Instructions::LOOP_CHARACTER_CS:
        case RE_Instructions::LOOP_CHARACTER_CI:
          if (address[2] == 0)
            break;

        case RE_Instructions::MATCH_CHARACTER_CS:
          match_character = true;
          character = *address >> 8;
          if ((*address & 0xffu) == RE_Instructions::LOOP_CHARACTER_CI)
            alternative = RE_GetAlternativeCharacter (character);
          break;

        case RE_Instructions::MATCH_CHARACTER_CI:
          match_character = true;
          character = address[1] & 0xffffu;
          alternative = address[1] >> 16;
          break;

        case RE_Instructions::MATCH_STRING_CI:
          alternative = alternative_strings[*address >> 8][0];

        case RE_Instructions::MATCH_STRING_CS:
          character = strings[*address >> 8][0];
          break;
        }

      const uni_char *s1 = s;

      if (character != -1)
        {
          if (match_character && (address[RE_InstructionLengths[*address & 0xffu]] & 0xffu) == RE_Instructions::SUCCESS)
            push_choice = false;

          if (*s == character || *s == alternative)
            goto found_follower1;

          ++s;

          if (alternative == -1)
            while (s != se)
              if (*s == character)
                goto found_follower2;
              else if (!IsLineTerminator (*s))
                ++s;
              else
                break;
          else
            while (s != se)
              if (*s == character || *s == alternative)
                goto found_follower2;
              else if (!IsLineTerminator (*s))
                ++s;
              else
                break;

          if (s == string + length || *s != character && *s != alternative)
            {
              l->previous_end = s - string;
              return false;
            }
        }
      else
        {
        found_follower1:
          ++s;

        found_follower2:
          while (s != se)
            if (!IsLineTerminator (*s))
              ++s;
            else
              break;

          if (!push_choice && (s == se || *s != character && *s != alternative))
            push_choice = true;
        }

      if (push_choice)
        {
          Choice *c;

          if (!free_choice)
            AllocateChoicesL ();

          c = free_choice;
          free_choice = c->previous;

          c->address = address;
          c->index = s1 - string;
          c->count = (s - string) - c->index - 1;
          c->additional = 1;
          c->loop_index = loop_index;
          c->previous_choice_for_loop = loops[loop_index].last_choice;
          c->mark = false;
          c->allow_loop_repeat = false;
          c->previous = choice;
          c->loop_choice = true;
          c->loop_values = false;
          choice = l->last_choice = c;

          serial += 1 + c->count;
        }
      else
        {
          address += RE_InstructionLengths[*address & 0xffu];
          ++s;
        }
    }

  index = s - string;

  if (!limited_by_max || s != se)
    l->previous_end = index;

  return true;
}

bool
RE_Matcher::LoopCharacter (unsigned loop_index, bool case_insensitive, unsigned short character, unsigned min, unsigned max, unsigned &index, const unsigned *address)
{
  unsigned i = index, len = length;

  Loop *l = loops + loop_index;
  Choice *c = l->last_choice;

  unsigned ps = l->previous_start;

  if (c)
    {
      if (i == c->GetIndex () + 1)
        if (max == ~0u || c->count < max - min - 1)
          if (!c->allow_loop_repeat)
            return false;
          else if (i + min < len)
            len = i + min;
    }
  else if (i == ps)
    return false;
  else if (i < ps)
    len = ps - 1;

  unsigned avail = len - index;
  const uni_char *s = string + i;
  uni_char alternative = static_cast<uni_char>(-1);

  if (case_insensitive)
    alternative = RE_GetAlternativeCharacter (character);

  if (min != 0)
    {
      if (avail < min)
        return false;

      const uni_char *sm = s + min;

      if (!case_insensitive)
        while (s != sm)
          if (character == *s)
            ++s;
          else
            return false;
      else
        while (s != sm)
          if (character == *s || alternative == *s)
            ++s;
          else
            return false;

      if (min == max)
        {
          index = s - string;
          return true;
        }
    }

  l->previous_start = i + min;

  if (max != ~0u && max < avail)
    avail = max;

  const uni_char *se = s + (avail - min);

  if (s != se && (character == *s || case_insensitive && alternative == *s))
    {
      const uni_char *s1 = s;

      ++s;

      if (!case_insensitive)
        while (s != se)
          if (character == *s)
            ++s;
          else
            break;
      else
        while (s != se)
          if (character == *s || alternative == *s)
            ++s;
          else
            break;

      if (*address != RE_Instructions::SUCCESS)
        {
          Choice *c;

          if (!free_choice)
            AllocateChoicesL ();

          c = free_choice;
          free_choice = c->previous;

          c->address = address;
          c->index = s1 - string;
          c->count = (s - string) - c->index - 1;
          c->additional = 1;
          c->loop_index = loop_index;
          c->previous_choice_for_loop = loops[loop_index].last_choice;
          c->mark = false;
          c->allow_loop_repeat = false;
          c->previous = choice;
          c->loop_choice = true;
          c->loop_values = false;
          choice = l->last_choice = c;

          serial += 1 + c->count;
        }
    }

  index = s - string;
  return true;
}

bool
RE_Matcher::LoopClass (unsigned class_index, unsigned loop_index, unsigned min, unsigned max, unsigned &index, const unsigned *address)
{
  unsigned i = index, len = length;

  Loop *l = loops + loop_index;
  Choice *c = l->last_choice;

  unsigned ps = l->previous_start;

  if (c)
    {
      if (i == c->GetIndex () + 1)
        if (max == ~0u || c->count < max - min - 1)
          if (!c->allow_loop_repeat)
            return false;
          else if (i + min < len)
            len = i + min;
    }
  else if (i == ps)
    return false;
  else if (i < ps)
    len = ps - 1;

  unsigned avail = len - index;
  const uni_char *s = string + i;
  RE_Class *cls = classes[class_index];

  if (min != 0)
    {
      if (avail < min)
        return false;

      const uni_char *sm = s + min;

      while (s != sm)
        if (cls->Match (*s))
          ++s;
        else
          return false;

      if (min == max)
        {
          index = s - string;
          return true;
        }
    }

  l->previous_start = i + min;

  if (max != ~0u && max < avail)
    avail = max;

  const uni_char *se = s + (avail - min);

  if (s != se && cls->Match (*s))
    {
      const uni_char *s1 = s;

      ++s;

      while (s != se)
        if (cls->Match (*s))
          ++s;
        else
          break;

      if (*address != RE_Instructions::SUCCESS)
        {
          Choice *c;

          if (!free_choice)
            AllocateChoicesL ();

          c = free_choice;
          free_choice = c->previous;

          c->address = address;
          c->index = s1 - string;
          c->count = (s - string) - c->index - 1;
          c->additional = 1;
          c->loop_index = loop_index;
          c->previous_choice_for_loop = loops[loop_index].last_choice;
          c->mark = false;
          c->allow_loop_repeat = false;
          c->previous = choice;
          c->loop_choice = true;
          c->loop_values = false;
          choice = l->last_choice = c;

          serial += 1 + c->count;
        }
    }

  index = s - string;
  return true;
}

bool
RE_Matcher::Backtrack (int character, int alternative, RE_Class *cls, unsigned &index)
{
  if (choice->loop_values)
    {
      PopChoice ();
      if (!choice)
        return false;
    }

  Choice *c = choice, *d;
  const uni_char *s = string + index, *se = string + c->index;
  unsigned count = c->count, loop_index;

  OP_ASSERT (c->index + (c->count + 1) * c->additional == index);

  if (!cls)
    if (character != -1)
      if (alternative != -1)
        do
          if (*--s == character || *s == alternative)
            goto matched;
        while (s != se);
      else
        do
          if (*--s == character)
            goto matched;
        while (s != se);
    else
      do
        if (IsLineTerminator (*--s))
          goto matched;
      while (s != se);
  else
    do
      if (cls->Match (*--s))
        goto matched;
    while (s != se);

  serial -= 1 + count;

  d = choice->previous;
  loop_index = choice->loop_index;

  if (loop_index != ~0u)
    loops[loop_index].last_choice = c->previous_choice_for_loop;

  c->previous = free_choice;
  free_choice = c;

  choice = d;
  return false;

 matched:
  unsigned i = s - string, new_count = (i - c->index) / c->additional;

  index = i + 1;
  serial -= c->count - new_count;
  c->count = new_count;

  RewindCaptures ();
  PopChoice ();

  return true;
}

bool
RE_Matcher::MatchCapture (unsigned capture_index, unsigned &index)
{
  Capture *c = captures[capture_index];

  if (c->start != UINT_MAX)
    if (c->end <= c->start)
      return true;
    else
      {
        unsigned capture_length = c->end - c->start;

        if (length - index >= capture_length)
          {
            const uni_char *capture = string + c->start;
            const uni_char *input = string + index, *input_end = input + capture_length;

            if (!case_insensitive)
              while (input != input_end && *input == *capture)
                ++input, ++capture;
            else
              while (input != input_end && (uni_tolower (*input) == *capture || uni_toupper (*input) == *capture))
                ++input, ++capture;

            if (input == input_end)
              {
                index += capture_length;
                return true;
              }
          }

        return false;
      }
  else
    return true;
}

void
RE_Matcher::ResetSlow ()
{
  for (unsigned i = 0; i < captures_count; ++i)
    {
      Capture *c = captures[i];

      if (Capture *d = c->previous)
        {
          Capture *e = d;
          while (e->previous)
            e = e->previous;
          e->previous = free_capture;
          free_capture = d;
          c->previous = 0;
        }

      c->start = UINT_MAX;
      c->start_serial = 0;
    }

  for (unsigned j = 0; j < loops_count; ++j)
    {
      loops[j].previous_start = length + 1;
      loops[j].pcp_count = 0;

      OP_ASSERT (loops[j].last_choice == 0);
    }

  serial = 0;
}

void
RE_Matcher::AllocateChoicesL ()
{
  if (suspend)
    {
      unsigned count = 8;

      Choice *c = free_choice = static_cast<Choice *> (suspend->AllocateL (sizeof (Choice) * count));

      for (unsigned index = 0; index < count - 1; ++index)
        c[index].previous = &c[index + 1];

      c[count - 1].previous = 0;
    }
  else
    {
      free_choice = OP_NEW_L (Choice, ());
      free_choice->previous = 0;
    }
}

void
RE_Matcher::AllocateCapturesL ()
{
  if (suspend)
    {
      unsigned count = captures_count > 16 ? captures_count : 16;

      Capture *c = free_capture = static_cast<Capture *> (suspend->AllocateL (sizeof (Capture) * count));

      for (unsigned index = 0; index < count - 1; ++index)
        c[index].previous = &c[index + 1];

      c[count - 1].previous = 0;
    }
  else
    {
      free_capture = OP_NEW_L (Capture, ());
      free_capture->previous = 0;
    }
}

void *
RE_Matcher::Allocate (unsigned bytes)
{
  if (suspend)
    return suspend->AllocateL (bytes);
  else
    return OP_NEWA_L (char, bytes);
}
