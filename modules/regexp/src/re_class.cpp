/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/regexp/src/re_config.h"
#include "modules/regexp/src/re_class.h"
#include "modules/regexp/src/re_compiler.h"

#ifdef ES_FEATURE__ERROR_MESSAGES
#define ES_ERROR(string) (literal ? "in regexp literal: " string : string)
#endif

#ifdef RE_FEATURE__EXTENDED_SYNTAX
#include "modules/regexp/src/re_matcher.h"
#include "modules/regexp/src/regexp_private.h"

static void
RE_SkipWhitespace (const uni_char *source, unsigned &index, unsigned length)
{
  while (index < length)
    {
      int ch = source[index];

      if (!RE_IsIgnorableSpace (ch))
        {
#ifdef RE_FEATURE__COMMENTS
          if (ch == '#')
            {
              ++index;
              while (index < length && !RE_Matcher::IsLineTerminator (source[index]))
                ++index;
              continue;
            }
#endif // RE_FEATURE__COMMENTS

          break;
        }
      else
        ++index;
    }
}

#define RE_SKIP_WHITESPACE() do { if (extended) RE_SkipWhitespace (source, index, length); } while (0)
#else // RE_FEATURE__EXTENDED_SYNTAX
#define RE_SKIP_WHITESPACE()
#endif // RE_FEATURE__EXTENDED_SYNTAX

class RE_ExcludeIncludeRange
{
public:
  RE_ExcludeIncludeRange (bool include, int from, int to)
    : include (include),
      from (from),
      to (to),
      previous (0),
      next (0)
  {
  }

  ~RE_ExcludeIncludeRange ()
  {
    /* Deleting any element in the list deletes the whole list. */
    if (previous)
      {
        previous->next = 0;
        Delete (previous);
      }
    if (next)
      {
        next->previous = 0;
        Delete (next);
      }
  }

  static void Delete (RE_ExcludeIncludeRange *rei);

  bool include;
  int from;
  int to;

  RE_ExcludeIncludeRange *previous, *next;

  RE_ExcludeIncludeRange *Find (int ch)
  {
    if (from <= ch && ch <= to)
      return this;
    else if (ch < from)
      return previous->Find (ch);
    else
      return next->Find (ch);
  }

  static RE_ExcludeIncludeRange *Include (RE_ExcludeIncludeRange *eir, int from, int to);

#ifdef DEBUG_ENABLE_OPASSERT
  void CheckIntegrity ()
  {
    RE_ExcludeIncludeRange *eir = this;

    while (eir->previous)
      eir = eir->previous;

    bool include = false;

    while (eir)
      {
        /* First and last should be exclude and no two included or
           excluded ranges should be adjacent. */
        OP_ASSERT (eir->include == include);
        include = !include;

        OP_ASSERT (eir->from == 0 || eir->previous);
        OP_ASSERT (eir->to == INT_MAX || eir->next);

        OP_ASSERT (!eir->previous || eir->previous->to == eir->from - 1);
        OP_ASSERT (!eir->next || eir->next->from == eir->to + 1);

        eir = eir->next;
      }

    OP_ASSERT (include);
  }
#endif // DEBUG_ENABLE_OPASSERT
};

/* static */ void
RE_ExcludeIncludeRange::Delete (RE_ExcludeIncludeRange *rei)
{
  if (rei)
    {
      for (RE_ExcludeIncludeRange *previous = rei->previous, *pp; previous; previous = pp)
        {
          pp = previous->previous;
          previous->previous = previous->next = 0;
          OP_DELETE (previous);
        }
      for (RE_ExcludeIncludeRange *next = rei, *nn; next; next = nn)
        {
          nn = next->next;
          next->previous = next->next = 0;
          OP_DELETE (next);
        }
    }
}

/* static */ RE_ExcludeIncludeRange *
RE_ExcludeIncludeRange::Include (RE_ExcludeIncludeRange *eir, int from, int to)
{
  RE_ExcludeIncludeRange *existing = eir->Find (from);

  if (existing->from == from && existing->to == to)
    {
      /* Exact match existed already; return if it's an included
         range, remove it and merge its siblings if it's an excluded
         range and then return the merged range. */

      if (!existing->include)
        {
          existing->previous->to = existing->next->to;

          RE_ExcludeIncludeRange *obsolete_exclude = existing;
          RE_ExcludeIncludeRange *obsolete_include = existing->next;

          existing = existing->previous;
          existing->next = obsolete_include->next;
          existing->next->previous = existing;

          obsolete_exclude->previous = obsolete_exclude->next = 0;
          Delete (obsolete_exclude);
          obsolete_include->previous = obsolete_include->next = 0;
          Delete (obsolete_include);
        }

      return existing;
    }
  else if (existing->include || existing->from == from && to > existing->to)
    {
      if (!existing->include)
        existing = existing->previous;

      if (to > existing->to)
        {
          existing->to = to;

          while (to > existing->next->to)
            {
              RE_ExcludeIncludeRange *obsolete = existing->next;
              existing->next = obsolete->next;
              obsolete->previous = obsolete->next = 0;
              Delete (obsolete);
            }

          if (existing->next->include)
            {
              existing->to = existing->next->to;

              RE_ExcludeIncludeRange *obsolete = existing->next;
              existing->next = obsolete->next;
              obsolete->previous = obsolete->next = 0;
              Delete (obsolete);
            }

          existing->next->previous = existing;
        }

      return existing;
    }
  else if (existing->from < from && to < existing->to)
    {
      /* New included range entirely inside existing excluded range. */

      RE_ExcludeIncludeRange *new_excluded = OP_NEW (RE_ExcludeIncludeRange, (false, to + 1, existing->to));
      RE_ExcludeIncludeRange *new_included = OP_NEW (RE_ExcludeIncludeRange, (true, from, to));

      if (!new_excluded || !new_included)
        {
          Delete (new_excluded);
          Delete (new_included);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      new_excluded->previous = new_included;
      new_excluded->next = existing->next;

      new_included->previous = existing;
      new_included->next = new_excluded;

      existing->next = new_included;
      if (new_excluded->next)
        new_excluded->next->previous = new_excluded;

      existing->to = from - 1;

      return new_included;
    }
  else if (existing->from == from && to < existing->to)
    {
      /* New included range at the beginning of existing excluded range. */

      existing->previous->to = to;
      existing->from = to + 1;

      return existing->previous;
    }
  else
    {
      /* New included range at the end of existing excluded range. */

      OP_ASSERT (!existing->include && existing->from < from && existing->to <= to);

      RE_ExcludeIncludeRange *next_range = existing->next;
      OP_ASSERT(next_range->include && next_range->from >= from);
      next_range->from = from;
      /* Adjust range to encompass 'to' */
      if (next_range->to < to)
        {
          next_range->to = to;
          /* Succeeding ranges are either contained or need
             to be adjusted with new lower bound, (to + 1). */
          next_range = next_range->next;
          while (next_range && next_range->from <= to)
            {
              if (next_range->to <= to)
                {
                  /* Subsumed by previous; delete. */
                  RE_ExcludeIncludeRange *previous = next_range->previous;
                  RE_ExcludeIncludeRange *successor = next_range->next;
                  next_range->previous = next_range->next = 0;
                  Delete (next_range);
                  if (successor)
                    successor->previous = previous;
                  previous->next = successor;
                  next_range = successor;
                }
              else if (next_range->include)
                {
                  /* The range that extends beyond the inserted range (existing->next) is
                     also an inclusive range; subsume and remove. */
                  existing->next->to = next_range->to;
                  existing->next->next = next_range->next;
                  if (next_range->next)
                    next_range->next->previous = existing->next;
                  next_range->previous = next_range->next = 0;
                  Delete (next_range);
                  break;
                }
              else
                {
                  next_range->from = to + 1;
                  break;
                }
            }
        }
      existing->to = from - 1;

      return existing->next;
    }
}

bool
RE_Class::ConstructL (unsigned depth, const uni_char *source, unsigned &index, unsigned length, bool extended)
{
  int range_first = -1;

  for (unsigned i = 0; i < BITMAP_ELEMENTS; ++i)
    bitmap[i] = 0;

  if (source[index] == '^')
    {
      inverted = true;
      ++index;
    }
  else
    inverted = false;

#ifdef RE_FEATURE__CHARACTER_CLASS_INTERSECTION
  if (depth++ >= RE_FEATURE__CHARACTER_CLASS_INTERSECTION_MAX_DEPTH)
    return false;
#endif // RE_FEATURE__CHARACTER_CLASS_INTERSECTION

#ifdef ES_FEATURE__ERROR_MESSAGES
  unsigned range_first_index, range_second_index;
#endif /* ES_FEATURE__ERROR_MESSAGES */

  RE_SKIP_WHITESPACE ();

  RE_ExcludeIncludeRange *eir = OP_NEW_L (RE_ExcludeIncludeRange, (false, 0, INT_MAX));
  OpStackAutoPtr<RE_ExcludeIncludeRange> eir_anchor (eir);

#define SetCharacter(ch) do { SetCharacter (ch); if (!(ch < BITMAP_RANGE)) eir = RE_ExcludeIncludeRange::Include (eir, ch, ch); } while (0)
#define SetRange(first, last) do { SetRange (first, last); if (last >= BITMAP_RANGE) eir = RE_ExcludeIncludeRange::Include (eir, MAX (first, BITMAP_RANGE), last); } while (0)

  while (index < length)
    {
#ifdef ES_FEATURE__ERROR_MESSAGES
      if (range_first == -1)
        range_first_index = index;
#endif /* ES_FEATURE__ERROR_MESSAGES */

      int character = source[index++];

      if (character == ']')
        {
          if (range_first >= 0)
            {
              SetCharacter (range_first);
              SetCharacter ('-');
            }

          --index;
          break;
        }
      else if (character == '\\')
        {
          if (index == length)
            return false;

          character = -2;

          bool builtin_class_handled = false;

          switch (source[index++])
            {
            case '0':
              if (index == length || !RE_Compiler::IsOctalDigit (source[index]))
                {
                  character = 0;
                  break;
                }
              ++index;

            case '1': case '2': case '3': case '4': case '5': case'6': case '7':
              character = 0;
              --index;
              while (index < length && RE_Compiler::IsOctalDigit (source[index]))
                {
                  int next_character = character * 8 + (source[index] - '0');
                  if (next_character > 255)
                    break;
                  character = next_character;
                  ++index;
                }
              break;

            case 'b':
              character = 8;
              break;

            case 't':
              character = 9;
              break;

            case 'n':
              character = 10;
              break;

            case 'v':
              character = 11;
              break;

            case 'f':
              character = 12;
              break;

            case 'r':
              character = 13;
              break;

            case 'd':
              if (builtin_class == BUILTIN_EMPTY || builtin_class == BUILTIN_DIGIT)
                {
                  builtin_class = BUILTIN_DIGIT;
                  builtin_class_handled = true;
                }

              SetRange ('0', '9');
              break;

            case 'D':
              if (builtin_class == BUILTIN_EMPTY || builtin_class == BUILTIN_NON_DIGIT)
                {
                  builtin_class = BUILTIN_NON_DIGIT;
                  builtin_class_handled = true;
                }

              SetRange (0, '0' - 1);
              SetRange ('9' + 1, INT_MAX - 1);
              break;

            case 's':
              if (builtin_class == BUILTIN_EMPTY || builtin_class == BUILTIN_WHITESPACE)
                {
                  builtin_class = BUILTIN_WHITESPACE;
                  builtin_class_handled = true;
                }

              SetRange (9, 13);
              SetCharacter (32);
              SetCharacter (160);
              SetCharacter (0x1680);
              SetCharacter (0x180e);
              SetRange (0x2000, 0x200b);
              SetCharacter (0x2028);
              SetCharacter (0x2029);
              SetCharacter (0x202f);
              SetCharacter (0x205f);
              SetCharacter (0x3000);
              SetCharacter (0xfeff);
              break;

            case 'S':
              if (builtin_class == BUILTIN_EMPTY || builtin_class == BUILTIN_NON_WHITESPACE)
                {
                  builtin_class = BUILTIN_NON_WHITESPACE;
                  builtin_class_handled = true;
                }

              SetRange (0, 8);
              SetRange (14, 31);
              SetRange (33, 159);
              SetRange (161, 0x1680 - 1);
              SetRange (0x1680 + 1, 0x180e - 1);
              SetRange (0x180e + 1, 0x2000 - 1);
              SetRange (0x200b + 1, 0x2028 - 1);
              SetRange (0x2029 + 1, 0x202f - 1);
              SetRange (0x202f + 1, 0x205f - 1);
              SetRange (0x205f + 1, 0x3000 - 1);
              SetRange (0x3000 + 1, 0xfeff - 1);
              SetRange (0xfeff + 1, INT_MAX - 1);
              break;

            case 'w':
              if (builtin_class == BUILTIN_EMPTY || builtin_class == BUILTIN_WORD_CHARACTER)
                {
                  builtin_class = BUILTIN_WORD_CHARACTER;
                  builtin_class_handled = true;
                }

              SetRange ('0', '9');
              SetRange ('A', 'Z');
              SetCharacter ('_');
              SetRange ('a', 'z');
              break;

            case 'W':
              if (builtin_class == BUILTIN_EMPTY || builtin_class == BUILTIN_NON_WORD_CHARACTER)
                {
                  builtin_class = BUILTIN_NON_WORD_CHARACTER;
                  builtin_class_handled = true;
                }

              SetRange (0, '0' - 1);
              SetRange ('9' + 1, 'A' - 1);
              SetRange ('Z' + 1, '_' - 1);
              SetCharacter ('_' + 1);
              SetRange ('z' + 1, INT_MAX - 1);
              break;

            case 'c':
              if (index - length < 2)
                continue;

              if (!RE_Compiler::IsLetter (source[index]))
                continue;

              character = source[index++] % 32;
              break;

            case 'x':
            case 'u':
              if (index - length < 3)
                continue;

#ifdef RE_FEATURE__BRACED_HEXADECIMAL_ESCAPES
              if (source[index] == '{' && RE_Compiler::IsHexDigit (source[index + 1]))
                {
                  unsigned index0 = index + 1;
                  character = 0;

                  while (index0 < length && source[index0] != '}' && RE_Compiler::IsHexDigit (source[index0]))
                    character = (character << 4) | RE_Compiler::HexToCharacter (source[index0++]);

                  if (index0 < length && source[index0] == '}')
                    {
                      index = index0 + 1;
                      break;
                    }

                  character = -2;
                }
#endif // RE_FEATURE__BRACED_HEXADECIMAL_ESCAPES

              if (!RE_Compiler::IsHexDigit (source[index]) || !RE_Compiler::IsHexDigit (source[index + 1]))
                continue;

              if (source[index - 1] == 'x')
                {
                  character = RE_Compiler::HexToCharacter (source[index + 1], source[index]);
                  index += 2;
                }
              else
                {
                  if (index - length < 5)
                    continue;

                  if (!RE_Compiler::IsHexDigit (source[index + 2]) || !RE_Compiler::IsHexDigit (source[index + 3]))
                    continue;

                  character = RE_Compiler::HexToCharacter (source[index + 3], source[index + 2], source[index + 1], source[index]);
                  index += 4;
                }
              break;

            default:
              character = source[index - 1];
              break;

#if 0
#ifdef ES_FEATURE__ERROR_MESSAGES
              error_string = ES_ERROR ("invalid escape sequence.");
              --index;
#endif /* ES_FEATURE__ERROR_MESSAGES */

              return false;
#endif // 0
            }

          if (!builtin_class_handled)
            builtin_class = BUILTIN_NONE;
        }
#ifdef RE_FEATURE__CHARACTER_CLASS_INTERSECTION
      else if (character == '&' && length - index >= 3 && source[index] == '&' && source[index + 1] == '[')
        {
          unsigned index0 = index + 2;

          RE_Class *iw = OP_NEW_L (RE_Class, (case_insensitive));

          iw->intersect_next = intersect_with;
          intersect_with = iw;

          if (!iw->ConstructL (depth, source, index0, length, extended))
            return false;

          index = index0 + 1;
        }
#endif // RE_FEATURE__CHARACTER_CLASS_INTERSECTION

      if (character == -2)
        range_first = -1;
      else if (range_first == -1)
        {
          RE_SKIP_WHITESPACE ();

          if (index < length)
            {
              int dash = source[index];

              if (dash == '-')
                {
                  range_first = character;
                  ++index;

#ifdef ES_FEATURE__ERROR_MESSAGES
                  range_second_index = index;
#endif /* ES_FEATURE__ERROR_MESSAGES */

                  continue;
                }
            }

          if (character != -2)
            {
              SetCharacter (character);
              builtin_class = BUILTIN_NONE;
            }
        }
      else if (range_first == -2 || range_first > character)
        {
#ifdef ES_FEATURE__ERROR_MESSAGES
          if (range_first == -2 || character == -2)
            {
              error_string = ES_ERROR ("invalid component in range.");

              if (range_first == -2)
                index = range_first_index;
              else
                index = range_second_index;
            }
          else
            {
              error_string = ES_ERROR ("negative range.");
              index = range_first_index;
            }
#endif /* ES_FEATURE__ERROR_MESSAGES */

          return false;
        }
      else
        {
          SetRange (range_first, character);
          builtin_class = BUILTIN_NONE;

          range_first = -1;
        }

      RE_SKIP_WHITESPACE ();
    }

  if (index == length)
    return false;

#ifdef RE_FEATURE__CHARACTER_CLASS_INTERSECTION
  RE_Class **iwp = &intersect_with;

  while (RE_Class *iw = *iwp)
    {
      for (unsigned i = 0; i < BITMAP_ELEMENTS; ++i)
#ifdef RE_COMPACT_CLASS_BITMAP
        bitmap[i] &= iw->bitmap[i];
#else // RE_COMPACT_CLASS_BITMAP
        bitmap[i] &= iw->bitmap[i];
#endif // RE_COMPACT_CLASS_BITMAP

      if (!iw->map)
        {
          /* Discard ranges that only use the bitmap; they are fully
             merged with the main set in the step above. */
          *iwp = iw->intersect_next;
          OP_DELETE(iw);
        }
      else
        iwp = &iw->intersect_next;
    }
#endif // RE_FEATURE__CHARACTER_CLASS_INTERSECTION

  if (case_insensitive)
    {
#ifdef RE_COMPACT_CLASS_BITMAP
      bitmap[3] |= bitmap[2] & 0x7ffffe;
      bitmap[2] |= bitmap[3] & 0x7ffffe;
#else // RE_COMPACT_CLASS_BITMAP
      unsigned b;

      for (b = 'A'; b <= 'Z'; ++b)
        if (bitmap[b])
          bitmap[b + 32] = 1;
        else if (bitmap[b + 32])
          bitmap[b] = 1;
#endif // RE_COMPACT_CLASS_BITMAP

      for (int character = 128; character < 256; ++character)
        if (Match (character))
          if (uni_islower (character))
            SetCharacter (uni_toupper (character));
          else if (uni_isupper (character))
            SetCharacter (uni_tolower (character));
    }

  if (inverted)
    for (unsigned idx = 0; idx < BITMAP_ELEMENTS; ++idx)
#ifdef RE_COMPACT_CLASS_BITMAP
      bitmap[idx] ^= ~0u;
#else // RE_COMPACT_CLASS_BITMAP
      bitmap[idx] = !bitmap[idx];
#endif // RE_COMPACT_CLASS_BITMAP

#undef SetCharacter
#undef SetRange

  ConstructMap (eir);

  return true;
}

void
RE_Class::ConstructL (int predefined)
{
  uni_char source[4];

  source[0] = '\\';
  source[1] = predefined;
  source[2] = ']';

  unsigned index = 0;

  ConstructL (0, source, index, 3, false);
}

bool
RE_Class::ConstructL (const uni_char *source, unsigned &index, unsigned length, bool full, bool extended)
{
  if (!full)
    {
      ConstructL (source[index + 1]);
      index += 2;
      return true;
    }
  else
    return ConstructL (0, source, index, length, extended);
}

void
RE_Class::SetCharacter (int character)
{
  if (character < BITMAP_RANGE)
    {
#ifdef RE_COMPACT_CLASS_BITMAP
      unsigned a = (unsigned) character >> 5;
      unsigned b = (unsigned) character & 31;

      bitmap[a] |= (1 << b);
#else // RE_COMPACT_CLASS_BITMAP
      bitmap[character] = 1;
#endif // RE_COMPACT_CLASS_BITMAP
    }

#ifdef RE_FEATURE__MACHINE_CODED
  extern bool RE_IsComplicated (uni_char ch);

  if (case_insensitive && RE_IsComplicated (character))
    can_machine_code = false;

  if (ch1 == -1)
    ch1 = character;
  else if (ch2 == -1)
    ch2 = character;
  else if (ch3 == -1)
    ch3 = character;
  else if (ch4 == -1)
    ch4 = character;
  else
    can_machine_code = false;
#endif // RE_FEATURE__MACHINE_CODED
}

void
RE_Class::SetRange (int first, int last)
{
  if (first < BITMAP_RANGE)
    for (int ch = first, last_handled = MIN (last, BITMAP_RANGE - 1); ch <= last_handled; ++ch)
      {
#ifdef RE_COMPACT_CLASS_BITMAP
        unsigned a = (unsigned) ch >> 5;
        unsigned b = (unsigned) ch & 31;

        bitmap[a] |= (1 << b);
#else // RE_COMPACT_CLASS_BITMAP
        bitmap[ch] = 1;
#endif // RE_COMPACT_CLASS_BITMAP
      }

#ifdef RE_FEATURE__MACHINE_CODED
  if (case_insensitive)
    can_machine_code = false;
  else if (ch1 == -1)
    {
      ch1 = first;
      ch2 = last;
      is_range1 = true;
    }
  else if (ch3 == -1)
    {
      ch3 = first;
      ch4 = last;
      is_range2 = true;
    }
  else
    can_machine_code = false;
#endif // RE_FEATURE__MACHINE_CODED
}

/* static */ int RE_CALLING_CONVENTION
RE_Class::ContainsCharacter (RE_Class *self, int character)
{
  bool recursive = false;

 again:
  unsigned lo = 0, hi = self->map_size;
  int *m = self->map;

  while (true)
    {
      unsigned i = (lo + hi) / 2;

      if (m[i] <= character)
        if (character < m[i + 1])
          if ((i & 1) == 1)
            return true;
          else
            break;
        else
          lo = i + 1;
      else
        hi = i - 1;
    }

  if (self->case_insensitive && !recursive)
    {
      int upper = uni_toupper (character);
      if (upper != character)
        {
          recursive = true;
          character = upper;
          goto again;
        }

      int lower = uni_tolower (character);
      if (lower != character)
        {
          recursive = true;
          character = lower;
          goto again;
        }
    }

  return false;
}

bool
RE_Class::SlowMatch (int character)
{
  if (ContainsCharacter (this, character))
    {
#ifdef RE_FEATURE__CHARACTER_CLASS_INTERSECTION
      RE_Class *iw = intersect_with;

      while (iw)
        {
          if (!iw->SlowMatch (character))
            return inverted;
          iw = iw->intersect_next;
        }
#endif // RE_FEATURE__CHARACTER_CLASS_INTERSECTION

      return !inverted;
    }
  else
    return inverted;
}

void
RE_Class::ConstructMap (RE_ExcludeIncludeRange *eir)
{
  RE_ExcludeIncludeRange *first_eir = eir;
  unsigned count = 1;

  while (first_eir->previous)
    {
      ++count;
      first_eir = first_eir->previous;
    }

  while (eir->next)
    {
      ++count;
      eir = eir->next;
    }

  OP_ASSERT ((count & 1) == 1);

  if (count > 1)
    {
      map_size = count + 1;
      map = OP_NEWA_L (int, map_size);

      unsigned index = 0;

      eir = first_eir;

      while (eir)
        {
          map[index++] = eir->from;
          eir = eir->next;
        }

      map[index++] = INT_MAX;
    }
  else
    map = 0;
}

#ifdef ES_FEATURE__ERROR_MESSAGES
bool RE_Class::literal;
const char *RE_Class::error_string;
#endif /* ES_FEATURE__ERROR_MESSAGES */
