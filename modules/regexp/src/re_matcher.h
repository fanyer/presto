/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef RE_EXECUTE_H
#define RE_EXECUTE_H

#include "modules/regexp/src/re_instructions.h"
#include "modules/regexp/src/re_class.h"
#include "modules/regexp/src/re_object.h"

class RE_Object;
class RE_Class;
class RegExpSuspension;

class RE_Matcher
{
public:
  RE_Matcher ();
  ~RE_Matcher ();

  void SetObjectL (RE_Object *object, bool case_insensitive, bool multiline);
  void SetString (const uni_char *string, unsigned length);
  void SetSuspend (RegExpSuspension *suspend);

  void Reset ();
  void Cleanup ();

  enum ExecuteResult { EXECUTE_SUCCESS, EXECUTE_FAILURE, EXECUTE_SUSPEND };

  ExecuteResult ExecuteL (const unsigned *address, unsigned index, unsigned length);
  bool GetNextAttemptIndex (unsigned &index);

  unsigned GetEndIndex ();
  void GetCapture (unsigned index, unsigned &start, unsigned &length);

  const unsigned *GetFull () { return bytecode; }
  const unsigned *GetSegment (unsigned index) { return bytecode_segments[index]; }

  unsigned GetSuspendAtIndex () { return suspend_at_index; }
  const unsigned *GetSuspendAtAddress () { return suspend_at_address; }

  static bool IsLineTerminator (int character);
  static bool IsWordChar (int character);

protected:
  inline bool PushChoiceL (const unsigned *address, unsigned loop_index, unsigned index, bool mark);
  enum RepeatStatus { REPEAT_FIRST, REPEAT_REPEATED, REPEAT_ENDED };
  inline RepeatStatus RepeatChoiceL (const unsigned *address, unsigned loop_index, unsigned index);
  void PopChoice ();
  unsigned PopChoiceMark ();

  void CaptureStartL (unsigned capture, unsigned index);
  void RewindCaptures ();

  void StartLoop (unsigned loop_index, unsigned index, unsigned min, unsigned max, bool greedy);
  bool ContinueLoopL (unsigned loop_index, unsigned index, const unsigned *&address, int offset, bool greedy);
  bool LoopPeriod (unsigned loop_index, unsigned min, unsigned max, unsigned &index, const unsigned *&address);
  bool LoopCharacter (unsigned loop_index, bool case_insensitive, unsigned short character, unsigned min, unsigned max, unsigned &index, const unsigned *address);
  bool LoopClass (unsigned loop_index, unsigned class_index, unsigned min, unsigned max, unsigned &index, const unsigned *address);
  bool Backtrack (int character, int alternative, RE_Class *cls, unsigned &index);

  bool MatchCharacterCI (unsigned chars, uni_char ch);
  bool MatchCharacterCISlow (uni_char patternch, uni_char inputch);
  bool MatchStringCS (unsigned string_index, unsigned &index);
  bool MatchStringCI (unsigned string_index, unsigned &index);
  bool MatchClass (RE_Class *cls, int character);
  bool MatchCapture (unsigned capture_index, unsigned &index);

  void ResetSlow ();

  RegExpSuspension *suspend;

  class Choice
  {
  public:
    const unsigned *address;
    unsigned index;
    unsigned count;
    unsigned additional;
    unsigned loop_index;
    unsigned loop_count;
    bool loop_choice;
    bool loop_values;
    bool mark;
    bool allow_loop_repeat;

    unsigned GetIndex () { return index + count * additional; }

    Choice *previous;
    Choice *previous_choice_for_loop;
  };

  Choice *choice, *free_choice;

  void AllocateChoicesL ();

  class Capture
  {
  public:
    unsigned start, end;
    unsigned start_serial, end_serial;

    Capture *previous;
  };

  unsigned captures_count;
  Capture **captures, *free_capture;

  void AllocateCapturesL ();

  class Loop;
  friend class Loop;

  class Loop
  {
  public:
    Loop () : last_choice (0) {}

    unsigned min, max, count;
    unsigned previous_start, previous_end;
    unsigned *pcp, pcp_count, pcp_capacity;
    Choice *last_choice;
  };

  unsigned loops_count;
  Loop *loops;

  unsigned serial;

  RE_Object *object;

  const unsigned *bytecode;
  const unsigned *const *bytecode_segments;

  unsigned *string_lengths;
  const uni_char *const *strings, *const *alternative_strings;

  RE_Class **classes;

  const uni_char *string;
  unsigned length, end_index;

  bool case_insensitive, multiline, loop_with_min_repeated;

  unsigned suspend_at_index;
  const unsigned *suspend_at_address;

  void *Allocate (unsigned bytes);
};

inline void
RE_Matcher::SetString (const uni_char *s, unsigned l)
{
  string = s;
  length = l;

  for (unsigned li = 0; li < loops_count; ++li)
    loops[li].previous_start = l + 1;
}

inline void
RE_Matcher::SetSuspend (RegExpSuspension *s)
{
  suspend = s;
}

inline unsigned
RE_Matcher::GetEndIndex ()
{
  return end_index;
}

inline bool
RE_Matcher::MatchCharacterCI (unsigned chars, uni_char ch)
{
  return ch == (chars & 0xffff) || ch == (chars >> 16);
}

inline bool
RE_Matcher::MatchCharacterCISlow (uni_char patternch, uni_char inputch)
{
  return uni_toupper (patternch) == uni_toupper (inputch);
}

inline bool
RE_Matcher::MatchStringCS (unsigned string_index, unsigned &index)
{
  unsigned string_length = string_lengths[string_index];
  if (length - index >= string_length && op_memcmp (string + index, strings[string_index], UNICODE_SIZE (string_length)) == 0)
    {
      index += string_length;
      return true;
    }
  else
    return false;
}

inline bool
RE_Matcher::MatchStringCI (unsigned string_index, unsigned &index)
{
  unsigned string_length = string_lengths[string_index];

  if (length - index >= string_length)
    {
      const uni_char *input = string + index, *input_end = input + string_length;
      const uni_char *normal_string = strings[string_index];
      const uni_char *alternative_string = alternative_strings[string_index];

      while (input != input_end && (*input == *normal_string || *input == *alternative_string))
        ++input, ++normal_string, ++alternative_string;

      if (input == input_end)
        {
          index += string_length;
          return true;
        }
    }

  return false;
}

inline bool
RE_Matcher::MatchClass (RE_Class *cls, int character)
{
  if (cls->Match (character))
    return true;
  else
    return false;
}

inline void
RE_Matcher::PopChoice ()
{
  unsigned loop_index = choice->loop_index;

  if (choice->loop_values)
    {
      while (choice && choice->loop_values)
        {
          loops[choice->loop_index].count = choice->loop_count;

          Choice *c = choice->previous;
          choice->previous = free_choice;
          free_choice = choice;
          choice = c;

          --serial;
        }

      return;
    }

  if (loop_index != ~0u)
    loops[loop_index].previous_start = choice->index;

  if (choice->count > 0)
    --choice->count;
  else
    {
      Choice *c = choice->previous;

      if (loop_index != ~0u)
        {
          Loop &l = loops[loop_index];

          if (l.last_choice == choice)
            {
              l.last_choice = choice->previous_choice_for_loop;
              if (l.pcp)
                {
                  if (l.pcp_count == l.pcp_capacity)
                    {
                      l.pcp_capacity += l.pcp_capacity;

                      unsigned *new_pcp = static_cast<unsigned *> (Allocate (l.pcp_capacity * sizeof (unsigned)));

                      op_memcpy (new_pcp, l.pcp, l.pcp_count * sizeof (unsigned));

                      if (!suspend)
                        OP_DELETEA (l.pcp);

                      l.pcp = new_pcp;
                    }

                  l.pcp[l.pcp_count++] = choice->index;
                }
              else if (++l.pcp_count == RE_CONFIG__LOOP_BACKTRACKING_LIMIT)
                {
                  l.pcp_capacity = 64;
                  l.pcp = static_cast<unsigned *> (Allocate (64 * sizeof (unsigned)));
                  l.pcp_count = 1;
                  l.pcp[0] = choice->index;
                }
            }

          l.count = choice->loop_count;
        }

      choice->previous = free_choice;
      free_choice = choice;

      choice = c;
    }

  --serial;
}

inline void
RE_Matcher::StartLoop (unsigned loop_index, unsigned index, unsigned min, unsigned max, bool greedy)
{
  Loop &l = loops[loop_index];
  l.min = min;
  l.max = max;
  l.count = 0;
  if (l.previous_start == length + 1 || !greedy)
    l.previous_start = index;
}

inline bool
RE_Matcher::ContinueLoopL (unsigned loop_index, unsigned index, const unsigned *&address, int offset, bool greedy)
{
  Loop &l = loops[loop_index];

  if (l.count == l.max)
    return true;

  if (l.min != 0 || l.max != UINT_MAX)
    if (PushChoiceL (0, loop_index, index, false))
      {
        choice->loop_values = true;
        choice->loop_count = l.count;
      }

  ++l.count;

  if (l.count <= l.min)
    {
      address += offset;
      loop_with_min_repeated = true;
    }
  else
    {
      loop_with_min_repeated = false;

      if (greedy)
        {
          if (l.pcp && l.pcp_count)
            {
              int i = l.pcp_count - 1;
              while (i >= 0 && l.pcp[i] < index)
                --i;
              l.pcp_count = i + 1;
              if (i >= 0 && l.pcp[i] == index)
                return false;
            }

          switch (RepeatChoiceL (address, loop_index, index))
            {
            case REPEAT_REPEATED:
              if (l.previous_start == index)
                {
                  l.previous_start = choice->index;
                  return false;
                }
              else if (l.previous_start < index)
                l.previous_start = length + 1;
              break;

            case REPEAT_ENDED:
              return false;
            }

          address += offset;
        }
      else if (l.count != 1 && index == l.previous_start)
        return true;
      else if (!PushChoiceL (address + offset, loop_index, index, false))
        return false;
    }

  return true;
}

/* static */ inline bool
RE_Matcher::IsLineTerminator (int character)
{
#ifdef RE_FEATURE__NEL_IS_LINE_TERMINATOR
  if ((character & (0xffff ^ (0xa | 0xd | 0x85 | 0x2028 | 0x2029))) != 0)
    return false;
  else
    return character == 0xa || character == 0xd || character == 0x85 || character == 0x2028 || character == 0x2029;
#else // RE_FEATURE__NEL_IS_LINE_TERMINATOR
  if ((character & (0xffff ^ (0xa | 0xd | 0x2028 | 0x2029))) != 0)
    return false;
  else
    return character == 10 || character == 13 || character == 0x2028 || character == 0x2029;
#endif // RE_FEATURE__NEL_IS_LINE_TERMINATOR
}

/* static */ inline bool
RE_Matcher::IsWordChar (int character)
{
  return character >= '0' && character <= '9' || character >= 'A' && character <= 'Z' || character >= 'a' && character <= 'z' || character == '_';
}

inline bool
RE_Matcher::PushChoiceL (const unsigned *address, unsigned loop_index, unsigned index, bool mark)
{
  if (address && loop_index != ~0u && loops[loop_index].last_choice && loops[loop_index].last_choice->GetIndex() == index && loops[loop_index].count >= loops[loop_index].last_choice->loop_count)
    return false;

  Choice *c = choice, *d;

  if (!free_choice)
    AllocateChoicesL ();

  d = choice = free_choice;
  free_choice = d->previous;

  d->address = address;
  d->index = index;
  d->count = 0;
  d->additional = 0;
  d->loop_index = loop_index;
  if (loop_index != ~0u)
    d->loop_count = loops[loop_index].count;
  d->mark = mark;
  d->loop_values = false;
  d->previous = c;

  if (address && loop_index != ~0u)
    {
      d->previous_choice_for_loop = loops[loop_index].last_choice;
      loops[loop_index].last_choice = d;
      d->loop_choice = true;
    }
  else
    d->loop_choice = false;

  ++serial;
  return true;
}

inline RE_Matcher::RepeatStatus
RE_Matcher::RepeatChoiceL (const unsigned *address, unsigned loop_index, unsigned index)
{
  Choice *c = choice;

  if (c && !c->mark && !c->loop_values && c->address == address && c->loop_index == loop_index)
    {
      unsigned actual = c->GetIndex ();

      if (index > actual)
        if (c->count == 0)
          {
            c->count = 1;
            c->additional = index - c->index;
            ++serial;
            return REPEAT_REPEATED;
          }
        else if (index - actual == c->additional)
          {
            ++c->count;
            ++serial;
            return REPEAT_REPEATED;
          }
    }

  return PushChoiceL (address, loop_index, index, false) ? REPEAT_FIRST : REPEAT_ENDED;
}

static inline unsigned
ToLowerCase(unsigned ch)
{
  if (ch >= 'A' && ch <= 'Z')
    return ch + 32;
  else if (ch < 127)
    return ch;
  else
    return uni_tolower (ch);
}

inline void
RE_Matcher::Reset ()
{
  OP_ASSERT (!choice);

  if (captures_count || loops_count)
    ResetSlow ();

  loop_with_min_repeated = false;
}

#define INSTRUCTION(index) ((RE_Instructions::Instruction) (bytecode[index] & 0xffu))
#define ARGUMENT(index) (bytecode[index] >> 8)

inline bool
RE_Matcher::GetNextAttemptIndex (unsigned &index)
{
  const unsigned *bytecode = object->GetBytecode ();
  unsigned bytecode_index = 0;

  while (true)
    {
      switch (bytecode[bytecode_index] & 0xffu)
        {
        case RE_Instructions::ASSERT_LINE_START:
          if (!multiline)
            {
              index = length + 1;
              return true;
            }
          return false;

        case RE_Instructions::LOOP_PERIOD:
          {
            unsigned previous_end = loops[(bytecode[bytecode_index] & 0xffffff00u) >> 8].previous_end;
            if (previous_end < length && IsLineTerminator (string[previous_end]))
              {
                index = loops[(bytecode[bytecode_index] & 0xffffff00u) >> 8].previous_end + 1;
                return true;
              }
            else if (previous_end == length && bytecode[bytecode_index + 2] == ~0u)
              {
                index = length + 1;
                return true;
              }
            else
              return false;
          }

        case RE_Instructions::CAPTURE_START:
          {
            unsigned capture_index = ARGUMENT (bytecode_index) >> 1;

            for (unsigned bytecode_index2 = bytecode_index; bytecode_index2 < object->GetBytecodeLength (); bytecode_index2 += RE_InstructionLengths[INSTRUCTION (bytecode_index2)])
              if (INSTRUCTION (bytecode_index2) == RE_Instructions::MATCH_CAPTURE && ARGUMENT (bytecode_index2) == capture_index)
                return false;

            ++bytecode_index;
          }
          break;

        default:
          return false;
        }
    }
}

#endif /* RE_EXECUTE_H */
