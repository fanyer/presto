/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/regexp/src/re_config.h"
#include "modules/regexp/src/re_compiler.h"
#include "modules/regexp/src/re_object.h"
#include "modules/regexp/src/re_class.h"
#include "modules/regexp/src/re_searcher.h"
#include "modules/regexp/src/re_matcher.h"
#include "modules/util/adt/opvector.h"

#ifdef RE_FEATURE__MACHINE_CODED
#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/util/es_codegenerator_ia32.h"
#include "modules/regexp/src/re_native.h"
#endif // RE_FEATURE__MACHINE_CODED

#define TAG_FORWARD_JUMP 0xffffffu
#define RE_ERROR(string) (literal ? "in regexp literal: " string : string)

#define INSTRUCTION(index) ((RE_Instructions::Instruction) (bytecode[index] & 0xffu))
#define ARGUMENT(index) (bytecode[index] >> 8)

const unsigned RE_InstructionLengths[] =
  {
    1, // MATCH_PERIOD, -
    1, // MATCH_CHARACTER_CS, ushort: character
    2, // MATCH_CHARACTER_CI, ulong: alternative << 16 | character
    1, // MATCH_CHARACTER_CI_SLOW, ushort: character
    1, // MATCH_STRING_CS, ushort: string index
    1, // MATCH_STRING_CI, ushort: string index
    1, // MATCH_CLASS, ushort: class index
    1, // MATCH_CAPTURE, ushort: capture index

    1, // ASSERT_LINE_START, -
    1, // ASSERT_LINE_END, -
    1, // ASSERT_WORD_EDGE, -
    1, // ASSERT_NOT_WORD_EDGE, -

    1, // PUSH_CHOICE, long: offset
    1, // PUSH_CHOICE_MARK, long: offset
    1, // POP_CHOICE_MARK, -

    3, // START_LOOP, ushort: loop index, ulong: min, max
    2, // LOOP_GREEDY, ushort: loop index, long: offset
    2, // LOOP, ushort: loop index, long: offset

    3, // LOOP_PERIOD, ushort: loop index, ulong: min, max
    4, // LOOP_CHARACTER_CS, ushort: character, ushort: loop index, ulong: min, max
    4, // LOOP_CHARACTER_CI, ushort: character, ushort: loop index, ulong: min, max
    4, // LOOP_CLASS, ushort: class index, ushort: loop index, ulong: min, max

    1, // JUMP, long: offset

    1, // CAPTURE_START, ushort: (capture index << 1) | (1 if actual start, 0 if reset)
    1, // CAPTURE_END, ushort: capture index

    1, // RESET_LOOP, ushort: loop index

    1, // SUCCESS, -
    1 // FAILURE, -
  };

static bool
RE_IsCaseSensitive (uni_char ch)
{
  uni_char lower = uni_tolower (ch), upper = uni_toupper (ch);
  if (lower != ch || upper != ch && !(upper < 128 && ch >= 128))
    return true;
  else
    return false;
}

bool
RE_IsComplicated (uni_char ch)
{
  switch (ch)
    {
    /* These character triplets have one thing in common: the first is
       an upper-case character and the two other are lower-case
       characters whose upper-case character is the first.  Our fast
       case insensitive matching where we simply know both the
       upper-case and the lower-case variant and compare the input to
       both without actually converting the input to either doesn't
       work for these. */
    case 914: case 946: case 976:
    case 917: case 949: case 1013:
    case 920: case 952: case 977:
    case 921: case 953: case 8126:
    case 922: case 954: case 1008:
    case 924: case 181: case 956:
    case 928: case 960: case 982:
    case 929: case 961: case 1009:
    case 931: case 962: case 963:
    case 934: case 966: case 981:
    case 7776: case 7777: case 7835:
      return true;

    default:
      return false;
    }
}

static uni_char
RE_GetAlternativeChar (uni_char ch)
{
  return uni_tolower (ch) != ch ? uni_tolower (ch) : uni_toupper (ch);
}

#ifdef RE_FEATURE__EXTENDED_SYNTAX
#include "modules/regexp/src/regexp_private.h"
#endif // RE_FEATURE__EXTENDED_SYNTAX

RE_Compiler::RE_Compiler (bool case_insensitive, bool multiline, bool searching)
  : top_production (0),
    last_production (0),
    bytecode (0),
    extended (false),
    literal (false),
    case_insensitive (case_insensitive),
    multiline (multiline),
    searching (searching),
    searcher (0),
    determining_instr_index (~0u),
    first_capture (0),
    last_capture (0),
#ifdef RE_FEATURE__NAMED_CAPTURES
    first_named_capture (0),
#endif // RE_FEATURE__NAMED_CAPTURES
    strings (0),
    classes (0),
    bytecode_segments (0)
{
}

void  RE_Compiler::Production::Delete(Production* that)
{
  while (that != NULL)
    {
      Production* del;
      that = (del = that)->previous;
      OP_DELETE (del);
    }
}

void  RE_Compiler::CaptureElm::Delete(CaptureElm* that)
{
  while (that != NULL)
    {
      CaptureElm* del;
      that = (del = that)->next;
      OP_DELETE (del);
    }
}

#ifdef RE_FEATURE__NAMED_CAPTURES

void  RE_Compiler::NamedCaptureElm::Delete(NamedCaptureElm* that)
{
  while (that != NULL)
    {
      NamedCaptureElm* del;
      that = (del = that)->next;
      OP_DELETE (del);
    }
}

#endif // RE_FEATURE__NAMED_CAPTURES

void  RE_Compiler::StringElm::Delete(StringElm* that)
{
  while (that != NULL)
    {
      StringElm* del;
      that = (del = that)->previous;
      OP_DELETE (del);
    }
}

void  RE_Compiler::ClassElm::Delete(ClassElm* that)
{
  while (that != NULL)
    {
      ClassElm* del;
      that = (del = that)->previous;
      OP_DELETE (del);
    }
}

void  RE_Compiler::BytecodeSegment::Delete(BytecodeSegment* that)
{
  while (that != NULL)
    {
      BytecodeSegment* del;
      that = (del = that)->previous;
      OP_DELETE (del);
    }
}

RE_Compiler::~RE_Compiler ()
{
  Production::Delete(top_production);
  Production::Delete(last_production);
  CaptureElm::Delete(first_capture);
  StringElm ::Delete(strings);
  ClassElm  ::Delete(classes);
  OP_DELETEA (bytecode);
  BytecodeSegment::Delete(bytecode_segments);
  OP_DELETE (searcher);
#ifdef RE_FEATURE__NAMED_CAPTURES
  NamedCaptureElm::Delete(first_named_capture);
#endif // RE_FEATURE__NAMED_CAPTURES
}

#ifdef RE_FEATURE__EXTENDED_SYNTAX

void
RE_Compiler::SetExtended ()
{
  extended = true;
}

#endif // RE_FEATURE__EXTENDED_SYNTAX

void
RE_Compiler::SetLiteral ()
{
  literal = true;
}

class StringOwner
{
private:
  OpVector<uni_char> strings;
  StringOwner(const StringOwner&); // To prevent accidental use
public:
  StringOwner() {}
  void AddL(uni_char* str) { LEAVE_IF_ERROR(strings.Add(str)); }
  void ReleaseAll() { strings.Clear(); }
  ~StringOwner()
    {
      for (unsigned i = 0; i < strings.GetCount(); i++)
        OP_DELETEA(strings.Get(i));
    }
};


RE_Object *
RE_Compiler::CompileL (const uni_char *s, unsigned l)
{
  source = s;
  length = l == ~0u ? uni_strlen (s) : l;

  captures = 0;
  loops = 0;

  string_start = UINT_MAX;

  strings_total = 0;
  classes_total = 0;

  bytecode_index = bytecode_total = 0;
  previous_instruction_index = ~0u;
  bytecode_segments_total = 0;

  OP_ASSERT(!last_production);
  Production *p = last_production = OP_NEW_L (Production, ());
  p->type = Production::DISJUNCTION;
  p->start_index = 0;
  p->forward_jump_index = ~0u;
  p->previous = 0;

  PushProductionL (Production::DISJUNCTION);

  bool slash_found = false;
  bool last_instruction_popped = false;

  searcher = OP_NEW_L (RE_Searcher, ());
  at_start_of_segment = true;
  simple_segments = true;
  segment_index = 0;
  bool forget_segments = false;

#ifdef RE_FEATURE__MACHINE_CODED
  loops_in_segment = 0;
#endif // RE_FEATURE__MACHINE_CODED

#ifdef ES_FEATURE__ERROR_MESSAGES
  RE_Class::literal = literal;
#endif /* ES_FEATURE__ERROR_MESSAGES */

  unsigned max_captures = 0;

  index = 0;

  while (index < length)
    {
      if (s[index] == '\\')
        ++index;
      else if (s[index] == '(' && index + 1 < length && s[index + 1] != '?')
        ++max_captures;
#ifdef RE_FEATURE__NAMED_CAPTURES
      else if (s[index] == '(' && index + 3 < length && s[index + 2] == 'P' && s[index + 3] == '<')
        {
          index += 4;

#ifdef RE_FEATURE__EXTENDED_SYNTAX
          SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX

          unsigned name_start = index;

          while (index < length && s[index] != '>' && !RE_IsIgnorableSpace (s[index]))
            ++index;

#ifdef RE_FEATURE__EXTENDED_SYNTAX
          SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX

          if (index == length || s[index] != '>')
            return 0;

          unsigned dummy;

          if (NamedCaptureElm::Find (first_named_capture, source + name_start, index - name_start, dummy))
            return 0;

          NamedCaptureElm *nce = OP_NEW_L (NamedCaptureElm, (max_captures++));

          nce->next = first_named_capture;
          first_named_capture = nce;

          nce->name.SetL (s + name_start, index - name_start);
        }
#endif // RE_FEATURE__NAMED_CAPTURES
#ifdef RE_FEATURE__COMMENTS
      else if (s[index] == '(' && index + 2 < length && s[index + 2] == '#')
        do
          ++index;
        while (index < length && s[index] != ')');
#ifdef RE_FEATURE__EXTENDED_SYNTAX
      else if (extended && s[index] == '#')
        do
          ++index;
        while (index < length && !RE_Matcher::IsLineTerminator (s[index]));
#endif // RE_FEATURE__EXTENDED_SYNTAX
#endif // RE_FEATURE__COMMENTS
      else if (s[index] == '[')
        while (++index < length)
          if (s[index] == '\\')
            ++index;
          else if (s[index] == ']')
            break;

      ++index;
    }

  index = 0;

#ifdef RE_FEATURE__EXTENDED_SYNTAX
  SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX

  bool last_was_quantifier = false;

  while (index < length && !slash_found)
    {
      unsigned min, max;
      int character, ch1 = 0, ch2 = 0, ch3 = 0;
      unsigned capture;
      unsigned old_bytecode_index;
      unsigned start_index;
      unsigned index_before;
      CaptureElm *c;

      unsigned rest = length - index - 1, index0 = index;

      switch (rest)
        {
        default:
          /* fall through */

        case 3:
          ch3 = source[index + 3];
          /* fall through */

        case 2:
          ch2 = source[index + 2];
          /* fall through */

        case 1:
          ch1 = source[index + 1];
          /* fall through */

        case 0:
          character = source[index];
        }

      bool previous_was_quantifier = last_was_quantifier;
      last_was_quantifier = false;

      bool previous_pop = last_instruction_popped;
      last_instruction_popped = false;

      switch (character)
        {
        case '[':
          if (!FlushStringL ())
            return 0;

          BeginProduction ();

          if (!CompileClassL ())
            return 0;

          --index;
          break;

        case '(':
          if (!FlushStringL ())
            return 0;

          if (rest >= 2 && ch1 == '?')
            {
              index += 2;

              if (ch2 == '=' || ch2 == '!')
                {
                  if (ch2 == '=')
                    PushProductionL (Production::POSITIVE_LOOKAHEAD);
                  else
                    PushProductionL (Production::NEGATIVE_LOOKAHEAD);

                  WriteInstructionL (RE_Instructions::PUSH_CHOICE_MARK, TAG_FORWARD_JUMP);
                  top_production->content_start_index = bytecode_index;
                }
              else if (ch2 == ':')
                PushProductionL (Production::DISJUNCTION);
#ifdef RE_FEATURE__NAMED_CAPTURES
              else if (ch2 == 'P')
                if (ch3 == '=' || ch3 == '<')
                  {
                    index += 2;

#ifdef RE_FEATURE__EXTENDED_SYNTAX
                    SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX

                    unsigned name_start = index;
                    int stop;

                    if (ch3 == '=')
                      stop = ')';
                    else
                      stop = '>';

                    while (index < length && s[index] != stop && !RE_IsIgnorableSpace (s[index]))
                      ++index;

#ifdef RE_FEATURE__EXTENDED_SYNTAX
                    SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX

                    if (index == length || s[index] != stop)
                      return 0;

                    if (ch3 == '=')
                      if (NamedCaptureElm::Find (first_named_capture, source + name_start, index - name_start, capture))
                        goto match_capture;
                      else
                        return 0;
                    else
                      goto start_capture;
                  }
                else
                  return 0;
#endif // RE_FEATURE__NAMED_CAPTURES
#ifdef RE_FEATURE__COMMENTS
              else if (ch2 == '#')
                {
                  BeginProduction ();
                  while (index < length && source[index++] != ')');
                  continue;
                }
#endif // RE_FEATURE__COMMENTS
              else
                {
#ifdef ES_FEATURE__ERROR_MESSAGES
                  error_string = RE_ERROR ("expected ':', '=' or '!'.");
#endif /* ES_FEATURE__ERROR_MESSAGES */

                  return 0;
                }

              break;
            }

#ifdef RE_FEATURE__NAMED_CAPTURES
        start_capture:
#endif // RE_FEATURE__NAMED_CAPTURES
          PushProductionL (Production::CAPTURE);

          {
            CaptureElm *ce = OP_NEW_L(CaptureElm, (bytecode_index, last_capture));
            if (last_capture)
              last_capture = last_capture->next = ce;
            else
              first_capture = last_capture = ce;
          }

          WriteInstructionL (RE_Instructions::CAPTURE_START, (top_production->capture_index << 1) | 1);
          top_production->content_start_index = bytecode_index;
          break;

        case ')':
          if (!FlushStringL ())
            return 0;

#ifdef ES_FEATURE__ERROR_MESSAGES
          error_string = RE_ERROR ("unexpected ')'.");
#endif /* ES_FEATURE__ERROR_MESSAGES */

        again:
          switch (top_production->type)
            {
            case Production::DISJUNCTION:
              PopProduction ();
              last_instruction_popped = true;
              break;

            case Production::POSITIVE_LOOKAHEAD:
              ResetLoopsInsideLookahead ();
              WriteInstructionL (RE_Instructions::POP_CHOICE_MARK);
              WriteInstructionL (RE_Instructions::JUMP, 1);
              SetForwardJump ();
              WriteInstructionL (RE_Instructions::FAILURE);
              PopProduction ();
              break;

            case Production::NEGATIVE_LOOKAHEAD:
              ResetLoopsInsideLookahead ();
              WriteInstructionL (RE_Instructions::POP_CHOICE_MARK);
              WriteInstructionL (RE_Instructions::FAILURE);
              SetForwardJump ();
              PopProduction ();
              break;

            case Production::CAPTURE:
              c = last_capture;
              while (c->capture_end != ~0u)
                c = c->previous;
              c->capture_end = bytecode_index;

              WriteInstructionL (RE_Instructions::CAPTURE_END, top_production->capture_index);
              PopProduction ();
              break;

            default:
              PopProduction ();
              last_instruction_popped = true;

              if (top_production->previous == 0)
                return 0;

              goto again;
            }

          if (!top_production)
            return 0;

          break;

        case '*':
        case '+':
        case '?':
          if (previous_was_quantifier)
            return 0;

          last_was_quantifier = true;

          if (!FlushStringBeforeQuantifierL ())
            return 0;

          if (top_production->content_start_index == bytecode_index && !previous_pop)
            return 0;

          min = character == '+' ? 1 : 0;
          max = character == '?' ? 1 : UINT_MAX;

          CompileQuantifierL (min, max);
          break;

        case '^':
          if (!FlushStringL ())
            return 0;
          BeginProduction ();
          WriteInstructionL (RE_Instructions::ASSERT_LINE_START);
          break;

        case '$':
          if (!FlushStringL ())
            return 0;
          BeginProduction ();
          WriteInstructionL (RE_Instructions::ASSERT_LINE_END);
          break;

        case '\\':
          AddString ();

          if (rest == 0)
            return 0;

          ++index;

          switch (ch1)
            {
            case 'd':
            case 'D':
            case 's':
            case 'S':
            case 'w':
            case 'W':
              --index;
              if (!FlushStringL ())
                return 0;
              BeginProduction ();
              if (!CompileClassL ())
                return 0;
              continue;

            case 'b':
              if (!FlushStringL ())
                return 0;
              character = -1;
              if (previous_instruction_index != ~0u)
                if (INSTRUCTION (previous_instruction_index) == RE_Instructions::LOOP_CLASS && index + 1 == length)
                  if (classes->GetIndexed (ARGUMENT (previous_instruction_index))->IsWordCharacter () && bytecode[previous_instruction_index + 2] != 0 && bytecode[previous_instruction_index + 3] == ~0u)
                    break;
              BeginProduction ();
              WriteInstructionL (RE_Instructions::ASSERT_WORD_EDGE);
              break;

            case 'B':
              if (!FlushStringL ())
                return 0;
              BeginProduction ();
              WriteInstructionL (RE_Instructions::ASSERT_NOT_WORD_EDGE);
              character = -1;
              break;

            case '0':
              if (rest >= 2 && IsOctalDigit (ch2))
                {
                octal_escape:
                  character = 0;
                  while (++index < length && IsOctalDigit (source[index]))
                    {
                      int next_character = character * 8 + (source[index] - '0');
                      if (next_character > 255)
                        break;
                      character = next_character;
                    }
                  --index;
                }
              else
                character = 0;
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

            case 'c':
              if (rest < 2 || !IsLetter (ch2))
                {
                  character = -1;
                  string_start = index - 1;
                  break;
                }

              ++index;

              character = ch2 % 32;
              break;

            case 'x':
            case 'u':
              character = 0;

#ifdef RE_FEATURE__BRACED_HEXADECIMAL_ESCAPES
              if (ch2 == '{' && IsHexDigit (ch3))
                {
                  unsigned index0 = index + 2;

                  while (index0 < length && source[index0] != '}' && IsHexDigit (source[index0]))
                    character = (character << 4) | HexToCharacter (source[index0++]);

                  if (index0 < length && source[index0] == '}')
                    {
                      index = index0;
                      break;
                    }
                }
#endif // RE_FEATURE__BRACED_HEXADECIMAL_ESCAPES

              if (ch1 == 'x')
                rest = MIN(rest, 3);
              else
                rest = MIN(rest, 5);

              --rest;

              while (rest--)
                if (!IsHexDigit (source[++index]))
                  {
                    character = -1;
                    string_start = index0 + 1;
                    --index;
                    break;
                  }
                else
                  character = (character << 4) | HexToCharacter (source[index]);

              break;

            default:
              if (ch1 >= '1' && ch1 <= '9')
                {
                  index_before = index;

                  capture = ReadDecimalDigits () - 1;
                  --index;

                  if (capture < max_captures)
                    {
                      if (!FlushStringL ())
                        return 0;

#ifdef RE_FEATURE__NAMED_CAPTURES
                    match_capture:
#endif // RE_FEATURE__NAMED_CAPTURES
                      BeginProduction ();

                      if (capture < captures)
                        first_capture->GetIndexed (capture)->last_match_capture = bytecode_index;

                      WriteInstructionL (RE_Instructions::MATCH_CAPTURE, capture);

                      character = -1;
                      break;
                    }

                  index = index_before;
                }

              if (IsOctalDigit (ch1))
                {
                  --index;
                  goto octal_escape;
                }
              else if (ch1 == '8' || ch1 == '9' || ch1 == 'c')
                {
                  character = -1;
                  string_start = index - 1;
                }
              else
                character = ch1;
            }

          if (character != -1)
            if (!AddCharacter (character & 0xffff))
              return 0;

          break;

        case '|':
          if (!FlushStringL ())
            return 0;

          start_index = top_production->content_start_index;
          old_bytecode_index = bytecode_index + 1;

          InsertBytecodeL (start_index, 1);
          WriteInstructionL (RE_Instructions::PUSH_CHOICE, old_bytecode_index - start_index);

          bytecode_index = old_bytecode_index;

          if (!top_production->previous)
            {
              PushBytecodeSegmentL (start_index + 1);

              WriteInstructionL (RE_Instructions::SUCCESS);

              at_start_of_segment = true;
              if (segment_index == 7)
                {
                  forget_segments = true;
                  simple_segments = false;
                  segment_index = 0;
                }
              else
                {
                  ++segment_index;

#ifdef RE_FEATURE__MACHINE_CODED
                  loops_in_segment = 0;
#endif // RE_FEATURE__MACHINE_CODED
                }
            }
          else
            {
              PushProductionL (Production::ALTERNATIVE);
              WriteInstructionL (RE_Instructions::JUMP, TAG_FORWARD_JUMP);
            }

          top_production->start_index = top_production->content_start_index = bytecode_index;
          break;

        case '{':
          if (++index != length && IsDigit (ch1))
            {
#ifdef RE_FEATURE__EXTENDED_SYNTAX
              SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX

              min = max = ReadDecimalDigits ();

#ifdef RE_FEATURE__EXTENDED_SYNTAX
              SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX

              if (index != length)
                {
                  character = source[index];

                  if (character == ',')
                    {
                      ++index;

#ifdef RE_FEATURE__EXTENDED_SYNTAX
                      SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX

                      if (index == length)
                        {
                          index = index0;
                          goto string_start;
                        }

                      character = source[index];

                      if (IsDigit (character))
                        {
                          max = ReadDecimalDigits ();

#ifdef RE_FEATURE__EXTENDED_SYNTAX
                          SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX
                        }
                      else
                        max = UINT_MAX;

                      if (min > max || index == length)
                        return 0;

                      character = source[index];
                    }

                  if (character == '}')
                    {
                      if (previous_was_quantifier)
                        return 0;

                      last_was_quantifier = true;

                      if (!FlushStringBeforeQuantifierL (index0))
                        return 0;

                      if (top_production->content_start_index == bytecode_index && !previous_pop)
                        return 0;

                      CompileQuantifierL (min, max);
                      break;
                    }
                }
            }

          index = index0;
          goto string_start;

        case '.':
          if (!FlushStringL ())
            return 0;

          BeginProduction ();
          WriteInstructionL (RE_Instructions::MATCH_PERIOD);
          break;

        case '/':
          if (literal)
            {
              slash_found = true;
              continue;
            }
          /* fall through */

        default:
        string_start:
          if (RE_IsComplicated (character))
            AddCharacter (character);
          else if (string_start == UINT_MAX)
            string_start = index;
        }

      ++index;

#ifdef RE_FEATURE__EXTENDED_SYNTAX
      SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX

#ifdef RE_FEATURE__DISASSEMBLER
#if 0
      OpString disassembly; Disassemble (disassembly);
      OpString8 disassembly8; disassembly8.Set (disassembly);
      printf ("%s\n", disassembly8.CStr());
#endif // 0
#endif // RE_FEATURE__DISASSEMBLER
    }

  if (!FlushStringL ())
    return 0;

  if (slash_found)
    ++index;

  while (top_production->type == Production::ALTERNATIVE)
    PopProduction ();

  if (!top_production->previous && (bytecode_segments || simple_segments && bytecode_segments_total == 0))
    {
      PushBytecodeSegmentL (top_production->content_start_index);
      WriteInstructionL (RE_Instructions::SUCCESS);
    }

  if (top_production->previous != 0 || (literal && !slash_found))
    {
#ifdef ES_FEATURE__ERROR_MESSAGES
      error_string = RE_ERROR ("unexpected end of file.");
#endif /* ES_FEATURE__ERROR_MESSAGES */

      return 0;
    }
  else
    {
      ResetLoops ();

      WriteInstructionL (RE_Instructions::SUCCESS);

      OptimizeJumps ();

      unsigned **bcs = 0;
      if (bytecode_segments)
        {
          bcs = OP_NEWA_L(unsigned *, bytecode_segments_total);
          BytecodeSegment *bs = bytecode_segments;
          for (unsigned i = bytecode_segments_total; i != 0; --i, bs = bs->previous)
            bcs[i - 1] = bytecode + bs->start_index;
        }
      ANCHOR_ARRAY(unsigned*, bcs);

      unsigned i;

      StringElm *se;
      unsigned st = strings_total;
      unsigned *sls = OP_NEWA_L(unsigned, st);
      ANCHOR_ARRAY(unsigned, sls);
      uni_char **ss = OP_NEWA_L(uni_char *, st);
      ANCHOR_ARRAY(uni_char*, ss);
      uni_char **altss = OP_NEWA_L(uni_char *, st);
      ANCHOR_ARRAY(uni_char*, altss);

      StringOwner string_owner; // Keep track of strings in case we Leave
      ANCHOR(StringOwner, string_owner);

      for (se = strings, i = st - 1; se; --i, se = se->previous)
        {
          sls[i] = se->length;
          ss[i] = se->string;

          if (se->ci)
            {
              altss[i] = OP_NEWA_L(uni_char, se->length);
              string_owner.AddL(altss[i]);
              for (unsigned index = 0; index < se->length; ++index)
                altss[i][index] = RE_GetAlternativeChar (se->string[index]);
            }
          else
            altss[i] = 0;
        }

      ClassElm *ce;
      unsigned ct = classes_total;
      RE_Class **cs = OP_NEWA_L(RE_Class *, ct);

      RE_ClassCleanupHelper cs_cleanup(cs, ct);
      ANCHOR(RE_ClassCleanupHelper, cs_cleanup);

      for (ce = classes, i = ct - 1; ce; --i, ce = ce->previous)
        {
          cs[i] = ce->cls;
          ce->cls = 0;
        }

      if (slash_found)
        length = index - 1;

      uni_char* src = OP_NEWA_L (uni_char, length + 1);
      op_memcpy (src, source, UNICODE_SIZE (length));
      src[length] = 0;
      ANCHOR_ARRAY(uni_char, src);

      if (searcher && forget_segments)
        if (!searcher->ForgetSegments ())
          {
            OP_DELETE (searcher);
            searcher = 0;
          }

      RE_Object *object = OP_NEW (RE_Object, (bytecode, bytecode_index, bcs, bytecode_segments_total, captures, loops, st, sls, ss, altss, ct, cs, src, searcher));
      if (!object)
        LEAVE(OpStatus::ERR_NO_MEMORY);

#ifdef RE_FEATURE__DISASSEMBLER
      Disassemble (object->GetDisassembly ());

#if 0
      OpString8 disassembly8; disassembly8.Set (object->GetDisassembly ());
      printf ("%s\n", disassembly8.CStr());
#endif // 0
#endif // RE_FEATURE__DISASSEMBLER

      // All these are owned by object now
      ANCHOR_ARRAY_RELEASE(bcs);
      ANCHOR_ARRAY_RELEASE(sls);
      ANCHOR_ARRAY_RELEASE(ss);
      ANCHOR_ARRAY_RELEASE(altss);
      cs_cleanup.release();
      ANCHOR_ARRAY_RELEASE(src);
      string_owner.ReleaseAll();
      bytecode = 0;
      searcher = 0;
      for (se = strings; se; se = se->previous) // pointers to these are in the ss/altss arrays now owned by the object
        se->string = 0;

#ifdef RE_FEATURE__NAMED_CAPTURES
      NamedCaptureElm *nce = first_named_capture;
      if (nce)
        {
          object->capture_names = OP_NEWA (OpString, captures);

          if (!object->capture_names)
            {
              OP_DELETE (object);
              LEAVE (OpStatus::ERR_NO_MEMORY);
            }

          while (nce)
            {
              OpStatus::Ignore (object->capture_names[nce->index].TakeOver (nce->name));
              nce = nce->next;
            }
        }
#endif // RE_FEATURE__NAMED_CAPTURES

#ifdef RE_FEATURE__MACHINE_CODED
      object->case_insensitive = case_insensitive;
      object->multiline = multiline;
      object->searching = searching;
#endif // RE_FEATURE__MACHINE_CODED

      return object;
    }
}

unsigned
RE_Compiler::GetEndIndex ()
{
  return index;
}

#ifdef ES_FEATURE__ERROR_MESSAGES

const char *
RE_Compiler::GetErrorString ()
{
  return error_string;
}

#endif /* ES_FEATURE__ERROR_MESSAGES */

#ifdef RE_FEATURE__DISASSEMBLER

void
RE_Compiler::Disassemble (OpString &target)
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
  sources.Set (source, length);

  target.AppendFormat (UNI_L ("Source:\n  %s\n\n"), sources.CStr ());
  target.AppendFormat (UNI_L ("Program:\n"));

  BytecodeSegment *bcs = bytecode_segments;
  unsigned bcs_index = 1;
  if (bcs)
    while (BytecodeSegment *pbcs = bcs->previous)
      bcs = pbcs;

  unsigned index = 0;

  while (index < bytecode_index)
    {
      unsigned argument = bytecode[index];
      RE_Instructions::Instruction instruction = (RE_Instructions::Instruction) (argument & 0xffu);
      argument >>= 8;

      if (bcs && bcs->start_index == index)
        {
          target.AppendFormat (UNI_L ("segment_%u:\n"), bcs_index++);
          bcs = bcs->next;
        }

      target.AppendFormat (UNI_L ("  %u: %s"), index, instruction_names[instruction]);

      ++index;

      int offset;
      unsigned loop_index;
      unsigned min;
      unsigned max;
      OpString string;
      uni_char *string_data;
      unsigned string_length;

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
          strings->GetIndexed (argument, string_data, string_length);
          string.Set (string_data, string_length);
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

          target.AppendFormat (UNI_L (", index=%u, min=%u, greedy=%d"), argument >> 1, min, argument & 1);

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

          target.AppendFormat (UNI_L (", argument(%u)"), argument);

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
          target.AppendFormat (UNI_L (", index=%u"), argument >> 1, (argument & 1) == 1 ? " [loop min reset]" : "");
          break;

        case RE_Instructions::SUCCESS:
        case RE_Instructions::FAILURE:
          break;
        }

      target.AppendFormat (UNI_L ("\n"));
    }

  target.AppendFormat (UNI_L ("\n"));
}

#endif /* RE_FEATURE__DISASSEMBLER */

bool
RE_Compiler::CompileClassL ()
{
  unsigned start_index = index, cls_index;

  ClassElm *ce;

  /* Disabled once we hit 16 classes since the search below scales
     poorly.  If we had a hash table instead, we wouldn't need to
     bother. */
  if (classes_total < 16)
    {
      ce = classes;
      while (ce)
        if (ce->length <= (length - index) && op_memcmp (source + index, source + ce->start_index, UNICODE_SIZE (ce->length)) == 0)
          break;
        else
          ce = ce->previous;
    }
  else
    ce = 0;

  if (ce)
    {
      index += ce->length;

      cls_index = 0;
      while (ce->previous)
        {
          ++cls_index;
          ce = ce->previous;
        }
    }
  else
    {
      /* Maximum number of classes we can handle. */
      if (classes_total == 0xffffffu)
        return false;

      ce = OP_NEW_L (ClassElm, ());
      ce->previous = classes;
      classes = ce;

      bool full = source[index] == '[';

      if (full)
        ++index;

      ce->cls = OP_NEW_L (RE_Class, (case_insensitive));

      if (!ce->cls->ConstructL (source, index, length, full, extended))
        {
#ifdef ES_FEATURE__ERROR_MESSAGES
          error_string = RE_Class::error_string;
#endif /* ES_FEATURE__ERROR_MESSAGES */

          return false;
        }

      if (full)
        ++index;

      ce->start_index = start_index;
      ce->length = index - start_index;

      cls_index = classes_total++;
    }

  WriteInstructionL (RE_Instructions::MATCH_CLASS, cls_index);
  return true;
}

void
RE_Compiler::CompileQuantifierL (unsigned min, unsigned max)
{
  bool greedy = true;

#ifdef RE_FEATURE__EXTENDED_SYNTAX
  SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX

  if (index < length - 1 && source[index + 1] == '?')
    {
      greedy = false;
      ++index;
    }

  if (searcher && last_production->started_at_start_of_segment)
    {
      if (min == 0)
        at_start_of_segment = true;

      simple_segments = false;
    }

  unsigned start_index = last_production->start_index;
  unsigned old_bytecode_index = bytecode_index;
  unsigned delta = 3;
  unsigned loop_index = loops++;

  if (start_index == bytecode_index)
    return;

  if (greedy)
    {
      bool optimized = false, write_loop_index = true;

      if (start_index == bytecode_index - RE_InstructionLengths[INSTRUCTION (start_index)])
        {
          if (max == 0)
            {
              bytecode_index = start_index;
              return;
            }

          RE_Instructions::Instruction new_instruction = RE_Instructions::FAILURE;
          unsigned argument = (bytecode[start_index] & 0xffffff00u);

          switch (INSTRUCTION (start_index))
            {
            case RE_Instructions::MATCH_PERIOD:
              bytecode[start_index] = RE_Instructions::LOOP_PERIOD | (loop_index << 8);
              optimized = true;
              write_loop_index = false;
              break;

            case RE_Instructions::MATCH_CHARACTER_CS:
              new_instruction = RE_Instructions::LOOP_CHARACTER_CS;
              break;

            case RE_Instructions::MATCH_CHARACTER_CI:
              new_instruction = RE_Instructions::LOOP_CHARACTER_CI;
              argument = (bytecode[start_index + 1] & 0xffffu) << 8;
              --bytecode_index;
              break;

            case RE_Instructions::MATCH_CLASS:
              new_instruction = RE_Instructions::LOOP_CLASS;
              break;
            }

          if (new_instruction != RE_Instructions::FAILURE)
            {
              bytecode[start_index] = new_instruction | argument;
              optimized = true;
            }
        }

      if (optimized)
        {
          if (searcher && min == 0 && determining_instr_index == start_index)
            at_start_of_segment = true;

          if (start_index != 0)
            {
              unsigned index = 0;

              while (index + RE_InstructionLengths[INSTRUCTION (index)] != start_index)
                index += RE_InstructionLengths[INSTRUCTION (index)];

              unsigned min_index = 0;
              bool can_join;

              switch (INSTRUCTION (index))
                {
                case RE_Instructions::LOOP_PERIOD:
                  can_join = INSTRUCTION (start_index) == RE_Instructions::LOOP_PERIOD;
                  min_index = index + 1;
                  break;

                case RE_Instructions::LOOP_CHARACTER_CS:
                case RE_Instructions::LOOP_CHARACTER_CI:
                case RE_Instructions::LOOP_CLASS:
                  can_join = bytecode[start_index] == bytecode[index];
                  min_index = index + 2;
                  break;

                default:
                  can_join = false;
                }

              if (can_join && index >= top_production->start_index)
                {
                  --loops;

                  bytecode[min_index] += min;

                  unsigned max_index = min_index + 1;

                  if (max == ~0u)
                    bytecode[max_index] = ~0u;
                  else if (bytecode[max_index] != ~0u)
                    bytecode[max_index] += max;

                  bytecode_index = index + RE_InstructionLengths[INSTRUCTION (index)];
                  return;
                }
            }

          if (write_loop_index)
            WriteUnsignedL (loop_index);

          WriteUnsignedL (min);
          WriteUnsignedL (max);

          return;
        }
    }

  unsigned capture_reset_count1 = 0, capture_reset_count2 = 0;

  if (last_production->captures)
    {
      CaptureElm *c1 = last_capture;
      unsigned actual_start_index = ActualCaptureStart (start_index);

      /* We're interested in captures that are inside the quantified part of the
         expression.  Strictly speaking, this means any capture whose start
         instruction (CaptureElm::capture_start) is at start_index or later.
         But if it is exactly at start_index, there is no point in placing an
         extra CAPTURE_START instruction right in front of it, resetting the
         capture. */
      while (c1 && ActualCaptureStart (c1->capture_start) > actual_start_index && capture_reset_count1 < last_production->captures)
        {
          ++capture_reset_count1;
          c1 = c1->previous;
        }

      while (c1 && ActualCaptureStart (c1->capture_start) >= actual_start_index && capture_reset_count1 + capture_reset_count2 < last_production->captures)
        {
          ++capture_reset_count2;
          c1 = c1->previous;
        }

      InsertBytecodeL (start_index, capture_reset_count1);

      CaptureElm *c2 = last_capture;

      for (unsigned index = 0; index < capture_reset_count1; ++index, c2 = c2->previous)
        WriteInstructionL (RE_Instructions::CAPTURE_START, c2->GetIndex () << 1);

      bytecode_index = old_bytecode_index += capture_reset_count1;
    }

  /* The commented out check on 'last_production->captures' is supposedly
     necessary (according to the original checkin comment) to handle empty
     matches correctly.  I cannot currently think of, or reproduce, any such
     problems however, so I'm removing it since it makes the generated bytecode
     easier to analyze. */
  if (min == 0 && max == 1 /* && last_production->captures == 0 */)
    {
      unsigned delta;

      if (capture_reset_count1 + capture_reset_count2)
        delta = 1;
      else
        delta = 0;

      if (greedy)
        {
          InsertBytecodeL (start_index, RE_InstructionLengths[RE_Instructions::PUSH_CHOICE]);
          WriteInstructionL (RE_Instructions::PUSH_CHOICE, ++old_bytecode_index + delta - (start_index + RE_InstructionLengths[RE_Instructions::PUSH_CHOICE]));
        }
      else
        {
          InsertBytecodeL (start_index, RE_InstructionLengths[RE_Instructions::PUSH_CHOICE] + RE_InstructionLengths[RE_Instructions::JUMP]);
          WriteInstructionL (RE_Instructions::PUSH_CHOICE, RE_InstructionLengths[RE_Instructions::PUSH_CHOICE]);
          WriteInstructionL (RE_Instructions::JUMP, (old_bytecode_index += 2) + delta - (start_index + RE_InstructionLengths[RE_Instructions::PUSH_CHOICE] + RE_InstructionLengths[RE_Instructions::JUMP]));
        }

      bytecode_index = old_bytecode_index;

      if (capture_reset_count1 + capture_reset_count2)
        {
          WriteInstructionL (RE_Instructions::JUMP, capture_reset_count1 + capture_reset_count2);

          CaptureElm *c = last_capture;

          for (unsigned index = 0; index < capture_reset_count1 + capture_reset_count2; ++index, c = c->previous)
            WriteInstructionL (RE_Instructions::CAPTURE_START, c->GetIndex () << 1);
        }
    }
  else
    {
      OpINT32Vector reset_loops; ANCHOR (OpINT32Vector, reset_loops);

      if (min != 0)
        {
          unsigned index1 = start_index;
          while (index1 < bytecode_index)
            {
              RE_Instructions::Instruction instruction = (RE_Instructions::Instruction) (INSTRUCTION (index1));
              unsigned instruction_length = RE_InstructionLengths[instruction];
              unsigned loop_index = ARGUMENT (index1);

              switch (instruction)
                {
                default:
                  break;

                case RE_Instructions::LOOP_CHARACTER_CI:
                case RE_Instructions::LOOP_CHARACTER_CS:
                case RE_Instructions::LOOP_CLASS:
                  loop_index = bytecode[index1 + 1];

                case RE_Instructions::LOOP_PERIOD:
                  LEAVE_IF_ERROR (reset_loops.Add (loop_index));
                  break;
                }

              index1 += instruction_length;
            }
        }

      ++delta;

      InsertBytecodeL (start_index, 1 + reset_loops.GetCount ());
      WriteInstructionL (RE_Instructions::JUMP, static_cast<unsigned> (static_cast<int> (old_bytecode_index + reset_loops.GetCount ()) - static_cast<int> (start_index)));

      for (unsigned reset_loop_index = 0; reset_loop_index < reset_loops.GetCount (); ++reset_loop_index)
        WriteInstructionL (RE_Instructions::RESET_LOOP, (reset_loops.Get (reset_loop_index) << 1) | 1);

      bytecode_index = old_bytecode_index += 1 + reset_loops.GetCount ();

      InsertBytecodeL (start_index, 3);
      WriteInstructionL (RE_Instructions::START_LOOP, (loop_index << 1) | (greedy ? 1 : 0));
      WriteUnsignedL (min);
      WriteUnsignedL (max);

      bytecode_index = old_bytecode_index + 3;

      WriteInstructionL (greedy ? RE_Instructions::LOOP_GREEDY : RE_Instructions::LOOP, loop_index, (unsigned) (((int) start_index + (int) delta) - ((int) bytecode_index + 2)));
    }
}

unsigned
RE_Compiler::ReadDecimalDigits ()
{
  unsigned value = 0;

  while (index < length)
    {
      int character = source[index];

      if (!IsDigit (character))
        break;

      value = value * 10 + character - '0';
      ++index;
    }

  return value;
}

bool
RE_Compiler::IsIdentifierPart (int character)
{
  return (IsDigit (character) || IsLetter (character) || character == '_' || character == '$');
}

bool
RE_Compiler::IsDigit (int character)
{
  return character >= '0' && character <= '9';
}

bool
RE_Compiler::IsLetter (int character)
{
  return ((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z'));
}

bool
RE_Compiler::IsHexDigit (int character)
{
  return IsDigit (character) || character >= 'A' && character <= 'F' || character >= 'a' && character <= 'f';
}

bool
RE_Compiler::IsOctalDigit (int character)
{
  return character >= '0' && character <= '7';
}

int
RE_Compiler::HexToCharacter (int h1)
{
  if (IsDigit (h1))
    return h1 - '0';
  else
    return 10 + (uni_toupper (h1) - 'A');
}

int
RE_Compiler::HexToCharacter (int h1, int h2)
{
  return HexToCharacter (h1) + (HexToCharacter (h2) << 4);
}

int
RE_Compiler::HexToCharacter (int h1, int h2, int h3, int h4)
{
  return HexToCharacter (h1, h2) + (HexToCharacter (h3, h4) << 8);
}

bool
RE_Compiler::AddCharacter (uni_char ch)
{
  if (RE_IsComplicated (ch))
    {
      if (!FlushStringL ())
        return false;
      string_buffer.Append (ch);
      if (!FlushStringL ())
        return false;
    }
  else
    string_buffer.Append (ch);

  return true;
}

void
RE_Compiler::AddString (const uni_char *string, unsigned string_length)
{
  if (string == 0)
    if (string_start != UINT_MAX)
      {
        string = source + string_start;
        string_length = index - string_start;
        string_start = UINT_MAX;
      }
    else
      return;

  unsigned added = 0, before = string_buffer.Length ();

  while (added != string_length)
    {
      string_buffer.AppendL (string + added, string_length - added);

      added += string_buffer.Length () - before;

      if (added != string_length)
        {
          string_buffer.Append (static_cast<int> ('\0'));
          ++added;
        }

      before = string_buffer.Length ();
    }
}

bool
RE_Compiler::FlushStringL (unsigned index_override, BOOL skip_last)
{
  if (string_buffer.Length () <= (skip_last ? 1U : 0U) && string_start == UINT_MAX)
    return true;

  if (string_start != UINT_MAX)
    {
      if (index_override == ~0u)
        index_override = index;

      AddString (source + string_start, index_override - string_start);

      string_start = UINT_MAX;
    }

  unsigned string_length = string_buffer.Length (), string_offset = 0;

  if (string_length <= (skip_last ? 1U : 0U))
    return true;
  else if (skip_last)
    string_length -= 1;

#ifndef RE_FEATURE__MACHINE_CODED
  if (searcher && at_start_of_segment && simple_segments && string_length > 1)
    {
      uni_char ch = string_buffer.GetStorage ()[0];

      if (ch < BITMAP_RANGE)
        {
          if (!case_insensitive || !RE_IsCaseSensitive (ch))
            WriteInstructionL (RE_Instructions::MATCH_CHARACTER_CS, ch);
          else if (!RE_IsComplicated (ch))
            WriteInstructionL (RE_Instructions::MATCH_CHARACTER_CI, 0, ((uni_toupper (ch) != ch ? uni_toupper (ch) : uni_tolower (ch)) << 16) | ch);
          else
            WriteInstructionL (RE_Instructions::MATCH_CHARACTER_CI_SLOW, ch);

          string_length -= 1;
          string_offset = 1;
        }
    }
#endif // RE_FEATURE__MACHINE_CODED

  if (string_length != 0)
    {
      BeginProduction ();

      if (string_length == 1)
        {
          uni_char ch = string_buffer.GetStorage ()[string_offset];

          if (!case_insensitive || !RE_IsCaseSensitive (ch))
            WriteInstructionL (RE_Instructions::MATCH_CHARACTER_CS, ch);
          else if (!RE_IsComplicated (ch))
            WriteInstructionL (RE_Instructions::MATCH_CHARACTER_CI, 0, ((uni_toupper (ch) != ch ? uni_toupper (ch) : uni_tolower (ch)) << 16) | ch);
          else
            WriteInstructionL (RE_Instructions::MATCH_CHARACTER_CI_SLOW, ch);
        }
      else
        {
          /* Maximum number of strings we can handle. */
          if (strings_total == 0xffffffu)
            return false;

          uni_char *s = OP_NEWA(uni_char, string_length + 1);
          StringElm *se = OP_NEW(StringElm, ());
          if (!s || !se)
            {
              OP_DELETEA(s);
              StringElm::Delete(se);
              LEAVE(OpStatus::ERR_NO_MEMORY);
            }
          op_memcpy (s, string_buffer.GetStorage () + string_offset, UNICODE_SIZE (string_length));
          s[string_length] = 0;

          se->length = string_length;
          se->string = s;
          se->ci = false;
          if (case_insensitive)
            for (unsigned i = 0; i < string_length; ++i)
              if (RE_IsCaseSensitive (s[i]))
                {
                  se->ci = true;
                  break;
                }

          se->previous = strings;
          strings = se;

          WriteInstructionL (!se->ci ? RE_Instructions::MATCH_STRING_CS : RE_Instructions::MATCH_STRING_CI, strings_total++, ~0u, s);
        }
    }

  uni_char ch = string_buffer.GetStorage ()[string_offset + string_length];

  string_buffer.Clear ();

  if (skip_last)
    string_buffer.AppendL (ch);

  return true;
}

bool
RE_Compiler::FlushStringBeforeQuantifierL (unsigned index_override)
{
  if (string_start != UINT_MAX)
    {
      if (index_override == ~0u)
        index_override = index;

      AddString (source + string_start, index_override - string_start);

      string_start = UINT_MAX;
    }

  if (string_buffer.Length () > 1)
    if (!FlushStringL (~0u, TRUE))
      return false;

  if (string_buffer.Length () != 0)
    {
      BeginProduction ();

      if (!FlushStringL (~0u))
        return false;
    }

  return true;
}

void
RE_Compiler::PushProductionL (Production::Type type)
{
  Production *p;

  if (last_production->previous)
    {
      p = last_production;
      last_production = p->previous;
    }
  else
    p = OP_NEW_L (Production, ());

  p->type = type;
  p->start_index = p->content_start_index = bytecode_index;
  p->forward_jump_index = ~0u;
  p->captures = 0;
  p->might_match_empty = false;
  p->previous = top_production;

  if (type == Production::CAPTURE)
    {
      p->capture_index = captures++;

      Production *q = p;
      while (q->previous)
        {
          ++q->captures;
          q = q->previous;
        }
    }
  else if (type == Production::ALTERNATIVE && top_production->started_at_start_of_segment)
    if (at_start_of_segment)
      {
        Production *q = top_production;
        while (q->type == Production::ALTERNATIVE)
          q = q->previous;
        q->might_match_empty = true;
      }
    else
      at_start_of_segment = true;

  p->started_at_start_of_segment = at_start_of_segment;

  top_production = p;
}

void
RE_Compiler::PopProduction ()
{
  Production *p;

  if (top_production->might_match_empty)
    at_start_of_segment = true;

  if (top_production->type == Production::ALTERNATIVE)
    {
      SetForwardJump ();

      p = top_production;
      top_production = top_production->previous;

      p->previous = 0;
      Production::Delete(p);
    }
  else
    {
      OP_ASSERT (top_production->forward_jump_index == ~0u);

      p = last_production;
      top_production = (last_production = top_production)->previous;
      last_production->previous = p;
    }
}

void
RE_Compiler::BeginProduction ()
{
  last_production->start_index = bytecode_index;
  last_production->started_at_start_of_segment = at_start_of_segment;
}

void
RE_Compiler::EnsureBytecodeL (unsigned additional)
{
  if (bytecode_index + additional >= bytecode_total)
    {
      bytecode_total = bytecode_total > 0 ? bytecode_total << 1 : 64;

      unsigned *new_bytecode = OP_NEWA_L(unsigned, bytecode_total);

      op_memcpy (new_bytecode, bytecode, bytecode_index * sizeof bytecode[0]);

      OP_DELETEA (bytecode);
      bytecode = new_bytecode;
    }
}

void
RE_Compiler::InsertBytecodeL (unsigned index, unsigned length, bool adjust_jumps)
{
  EnsureBytecodeL (length);
  unsigned *target = bytecode + index;
  op_memmove (target + length, target, (bytecode_index - index) * sizeof bytecode[0]);

  unsigned bytecode_length = bytecode_index + length;
  bytecode_index = index;

  Production *p = top_production;
  while (p)
    {
      if (p->start_index > index)
        p->start_index += length;
      if (p->content_start_index > index)
        p->content_start_index += length;
      if (p->forward_jump_index >= index && p->forward_jump_index != ~0u)
        p->forward_jump_index += length;

      p = p->previous;
    }

  if (~determining_instr_index && determining_instr_index >= index)
    determining_instr_index += length;

  CaptureElm *c = first_capture;
  while (c)
    {
      if (c->capture_start >= index)
        c->capture_start += length;
      if (c->capture_end != ~0u && c->capture_end >= index)
        c->capture_end += length;
      if (c->last_match_capture != ~0u && c->last_match_capture >= index)
        c->last_match_capture += length;

      c = c->next;
    }

  BytecodeSegment *bs = bytecode_segments;
  while (bs && bs->start_index >= index)
    {
      bs->start_index += length;
      bs = bs->previous;
    }

  if (adjust_jumps)
    {
      unsigned index1;

      /* First adjust forwards jumps before the inserted block that jumps to
         after the inserted block. */
      index1 = 0;
      while (index1 < index)
        {
          RE_Instructions::Instruction instruction = (RE_Instructions::Instruction) (INSTRUCTION (index1));
          unsigned instruction_length = RE_InstructionLengths[instruction];
          int offset;

          switch (instruction)
            {
            case RE_Instructions::LOOP_GREEDY:
            case RE_Instructions::LOOP:
              offset = (int) bytecode[index1 + 1];
              if (offset > 0)
                if ((int) (index1 + instruction_length) + offset >= (int) index)
                  bytecode[index1 + 1] += (int) length;
              break;

            case RE_Instructions::PUSH_CHOICE:
            case RE_Instructions::PUSH_CHOICE_MARK:
            case RE_Instructions::JUMP:
              offset = (int) (bytecode[index1] >> 8);

              if (offset > 0)
                if ((int) (index1 + instruction_length) + offset >= (int) index)
                  bytecode[index1] = (INSTRUCTION (index1)) | ((unsigned) (offset + (int) length) << 8);
            }

          index1 += instruction_length;
        }

      /* Then adjust backwards jumps after the inserted block that jumps to
         before the inserted block. */
      index1 = index + length;
      while (index1 < bytecode_length)
        {
          RE_Instructions::Instruction instruction = (RE_Instructions::Instruction) (INSTRUCTION (index1));
          unsigned instruction_length = RE_InstructionLengths[instruction];
          int offset;

          switch (instruction)
            {
            case RE_Instructions::LOOP_GREEDY:
            case RE_Instructions::LOOP:
              offset = (int) bytecode[index1 + 1];
              if (offset < 0)
                // Offset from original index
                if ((int) ((index1 - length) + instruction_length) + offset <= (int) index)
                  bytecode[index1 + 1] -= (int) length;
              break;

            case RE_Instructions::PUSH_CHOICE:
            case RE_Instructions::PUSH_CHOICE_MARK:
            case RE_Instructions::JUMP:
              offset = (int) (bytecode[index1] >> 8);

              if (offset < 0)
                // Offset from original index
                if ((int) ((index1 - length) + instruction_length) + offset <= (int) index)
                  bytecode[index1] = (INSTRUCTION (index1)) | ((unsigned) (offset - (int) length) << 8);
            }

          index1 += instruction_length;
        }
    }
}

void
RE_Compiler::ResetLoops ()
{
  CaptureElm *c = first_capture;
  while (c)
    {
      if (c->last_match_capture != ~0u)
        {
          unsigned loops = 0;

          unsigned index = c->capture_end + RE_InstructionLengths[RE_Instructions::CAPTURE_END], index0 = index;
          while (index < c->last_match_capture)
            {
              RE_Instructions::Instruction instruction = (RE_Instructions::Instruction) (INSTRUCTION (index));
              index += RE_InstructionLengths[instruction];
              OP_ASSERT ((INSTRUCTION (index)) <= RE_Instructions::FAILURE);

              switch (instruction)
                {
                case RE_Instructions::LOOP_PERIOD:
                case RE_Instructions::LOOP_CHARACTER_CS:
                case RE_Instructions::LOOP_CHARACTER_CI:
                case RE_Instructions::LOOP_CLASS:
                case RE_Instructions::START_LOOP:
                  ++loops;
                  break;
                }
            }

          if (loops)
            {
              unsigned old_bytecode_index = bytecode_index;
              unsigned inserted_length = loops * (RE_InstructionLengths[RE_Instructions::RESET_LOOP]);

              InsertBytecodeL (index0, inserted_length, true);

              index = bytecode_index + inserted_length;
              while (loops)
                {
                  RE_Instructions::Instruction instruction = (RE_Instructions::Instruction) (INSTRUCTION (index));
                  unsigned argument = ARGUMENT (index);

                  switch (instruction)
                    {
                    case RE_Instructions::START_LOOP:
                      argument >>= 1;
                      /* fall through */

                    case RE_Instructions::LOOP_PERIOD:
                      WriteInstructionL (RE_Instructions::RESET_LOOP, argument << 1);
                      --loops;
                      break;

                    case RE_Instructions::LOOP_CHARACTER_CS:
                    case RE_Instructions::LOOP_CHARACTER_CI:
                    case RE_Instructions::LOOP_CLASS:
                      WriteInstructionL (RE_Instructions::RESET_LOOP, bytecode[index + 1] << 1);
                      --loops;
                    }

                  index += RE_InstructionLengths[instruction];
                }

              bytecode_index = old_bytecode_index + inserted_length;
            }
        }

      c = c->next;
    }
}

void
RE_Compiler::ResetLoopsInsideLookahead ()
{
  unsigned index = top_production->start_index, stop = bytecode_index;

  while (index < stop)
    {
      RE_Instructions::Instruction instruction = (RE_Instructions::Instruction) (INSTRUCTION (index));
      unsigned argument = ARGUMENT (index);

      switch (instruction)
        {
        case RE_Instructions::START_LOOP:
          argument >>= 1;
          /* fall-through */
        case RE_Instructions::LOOP_PERIOD:
          WriteInstructionL (RE_Instructions::RESET_LOOP, argument << 1);
          break;

        case RE_Instructions::LOOP_CHARACTER_CS:
        case RE_Instructions::LOOP_CHARACTER_CI:
        case RE_Instructions::LOOP_CLASS:
          WriteInstructionL (RE_Instructions::RESET_LOOP, bytecode[index + 1] << 1);
        }

      index += RE_InstructionLengths[instruction];
    }
}

void
RE_Compiler::OptimizeJumps ()
{
  unsigned index = 0;

  while (index < bytecode_index)
    {
      RE_Instructions::Instruction instruction = INSTRUCTION (index);
      unsigned argument = bytecode[index] >> 8;

      if (instruction == RE_Instructions::JUMP)
        {
          unsigned initial_target_index = (unsigned) ((int) (index + RE_InstructionLengths[instruction]) + (int) argument), target_index = initial_target_index;

          while (INSTRUCTION (target_index) == RE_Instructions::JUMP)
            target_index = (unsigned) ((int) (target_index + RE_InstructionLengths[RE_Instructions::JUMP]) + (int) (bytecode[target_index] >> 8));

          if (target_index != initial_target_index)
            bytecode[index] = instruction | (((int) target_index - (int) (index + RE_InstructionLengths[instruction])) << 8);

          RE_Instructions::Instruction target_instruction = INSTRUCTION (target_index);

          switch (target_instruction)
            {
            case RE_Instructions::SUCCESS:
            case RE_Instructions::FAILURE:
              bytecode[index] = target_instruction;
            }
        }

      index += RE_InstructionLengths[instruction];
    }
}

void
RE_Compiler::WriteInstructionL (RE_Instructions::Instruction instr, unsigned argument, unsigned next_word, const uni_char *string_data)
{
  OP_ASSERT ((argument & 0xffffff) == argument);

  unsigned bytecode_index_before = bytecode_index;

  EnsureBytecodeL ();

  previous_instruction_index = bytecode_index;

  if (argument == TAG_FORWARD_JUMP)
    {
      bytecode[bytecode_index] = instr;

      OP_ASSERT (top_production->forward_jump_index == ~0u);
      top_production->forward_jump_index = bytecode_index++;
    }
  else
    bytecode[bytecode_index++] = instr | (argument << 8);

  if (next_word != ~0u)
    bytecode[bytecode_index++] = next_word;

  if (searcher && at_start_of_segment)
    {
      bool determining_instr = false;
      bool non_simple_segment = true;

      RE_Class *cls;

      switch (instr)
        {
        case RE_Instructions::MATCH_PERIOD:
          searcher->MatchAnything (segment_index);
          determining_instr = true;
          non_simple_segment = false;
          break;

        case RE_Instructions::MATCH_CHARACTER_CS:
          searcher->Add (argument, segment_index);
          determining_instr = true;
          non_simple_segment = argument >= BITMAP_RANGE;
          break;

        case RE_Instructions::MATCH_CHARACTER_CI:
          searcher->Add (next_word & 0xffffu, segment_index);
          searcher->Add (next_word >> 16, segment_index);
          determining_instr = true;
          non_simple_segment = (next_word & 0xffffu) >= BITMAP_RANGE || (next_word >> 16) >= BITMAP_RANGE;
          break;

        case RE_Instructions::MATCH_CHARACTER_CI_SLOW:
          /* MATCH_CHARACTER_CI_SLOW only matches a few select
             characters outside the bitmap range.  It doesn't matter
             which character we tell the searcher to match, as long as
             it is outside the bitmap range. */
          searcher->Add (BITMAP_RANGE, segment_index);
          determining_instr = true;
          break;

        case RE_Instructions::MATCH_STRING_CS:
        case RE_Instructions::MATCH_STRING_CI:
          OP_ASSERT (string_data != NULL);
          searcher->Add (*string_data, segment_index);
          if (instr == RE_Instructions::MATCH_STRING_CI)
            searcher->Add (RE_GetAlternativeChar (*string_data), segment_index);
          determining_instr = true;
          break;

        case RE_Instructions::MATCH_CLASS:
          cls = classes->GetIndexed (argument);
          searcher->Add (cls, segment_index);
          determining_instr = true;
          non_simple_segment = cls->map != 0;
          break;

        case RE_Instructions::MATCH_CAPTURE:
          /* At the beginning of a segment all captures are empty, so
             this matches the empty string.  Ignore, and let next
             instruction determine what actually starts this segment. */
          break;

        case RE_Instructions::ASSERT_LINE_START:
        case RE_Instructions::ASSERT_LINE_END:
        case RE_Instructions::ASSERT_WORD_EDGE:
        case RE_Instructions::ASSERT_NOT_WORD_EDGE:
          /* "(^|x)y" at the beginning of a segment is a bit tricky.
             We'll ignore the ASSERT_LINE_START since it only limits
             where we can match, but it does mean that the capture can
             match empty, and thus that 'y' can also start the match.
             This is handled in PushProduction: if a new alternative
             (at the '|' character) starts while the old alternative
             ("^" in this case) is still in the "start of segment"
             state, the whole capture production is marked as "might
             match empty" and we go back into "start of segment" state
             when that production ends. */
          break;

        case RE_Instructions::PUSH_CHOICE:
          /* Ignore this.  Typically written when a '|' is
             encountered, but where that matters, it is handled
             elsewhere. */
          break;

        case RE_Instructions::PUSH_CHOICE_MARK:
        case RE_Instructions::POP_CHOICE_MARK:
          /* Segment starting with lookahead.  Negative lookahead
             would be fine, really, since it only eliminates possible
             matches.  Positive lookahead would be more work to
             handle.  I won't bother. */
          OP_DELETE (searcher);
          searcher = 0;
          break;

        case RE_Instructions::START_LOOP:
        case RE_Instructions::LOOP_GREEDY:
        case RE_Instructions::LOOP:
          /* Quantifiers are handled in CompileQuantifier(). */
          break;

        case RE_Instructions::LOOP_PERIOD:
        case RE_Instructions::LOOP_CHARACTER_CS:
        case RE_Instructions::LOOP_CHARACTER_CI:
        case RE_Instructions::LOOP_CLASS:
          /* Whatever the loop repeats will have been handled as a
             corresponding MATCH_* already (that was then replaced by
             this instruction,) and as stated above, quantifiers are
             handled in CompileQuantifier().  (Besides, these
             instructions are written in-place, never via this
             function, so we will actually never see them.) */
          break;

        case RE_Instructions::JUMP:
          /* Only occurs in some specific cases (positive lookahead
             and loops with min=0) that doesn't affect us. */
          break;

        case RE_Instructions::CAPTURE_START:
        case RE_Instructions::CAPTURE_END:
          /* CAPTURE_END won't start a segment of course; but starting
             a capture at the beginning of a segment ought to be fine.
             Just apply our calculations to the contents of it and to
             whatever follows it, if appropriate. */
          break;

        case RE_Instructions::RESET_LOOP:
          /* Uhm.  Special case.  Ought not affect searching. */
          break;

        case RE_Instructions::SUCCESS:
          /* Pattern can match empty. */
          OP_DELETE (searcher);
          searcher = 0;
          break;

        case RE_Instructions::FAILURE:
          /* No need to handle these here. */
          break;
        }

      if (determining_instr)
        {
          at_start_of_segment = false;
          determining_instr_index = bytecode_index_before;
        }

      if (non_simple_segment)
        simple_segments = false;
    }
}

void
RE_Compiler::WriteUnsignedL (unsigned value)
{
  EnsureBytecodeL ();
  bytecode[bytecode_index++] = value;
}

void
RE_Compiler::SetForwardJump ()
{
  unsigned index = top_production->forward_jump_index;
  top_production->forward_jump_index = ~0u;

  bytecode[index] = (INSTRUCTION (index)) | (((unsigned) ((int) bytecode_index - (int) index - 1)) << 8);

  OP_ASSERT ((INSTRUCTION (index)) < RE_Instructions::FAILURE);
}

#ifdef RE_FEATURE__EXTENDED_SYNTAX

void
RE_Compiler::SkipWhitespace ()
{
  if (extended)
    {
      unsigned start = index;

      while (index < length)
        {
          int ch = source[index];

          if (!RE_IsIgnorableSpace (ch))
            {
#ifdef RE_FEATURE__COMMENTS
              if (ch == '#')
                {
                  if (start == index)
                    AddString ();

                  ++index;
                  while (index < length && !RE_Matcher::IsLineTerminator (source[index]))
                    ++index;
                  continue;
                }
#endif // RE_FEATURE__COMMENTS

              break;
            }
          else
            {
              if (start == index)
                AddString ();

              ++index;
            }
        }
    }
}

#endif // RE_FEATURE__EXTENDED_SYNTAX

unsigned
RE_Compiler::ActualCaptureStart (unsigned capture_start)
{
  unsigned index = capture_start;

  while (TRUE)
    switch (INSTRUCTION (index))
      {
      case RE_Instructions::CAPTURE_START:
        index += RE_InstructionLengths[INSTRUCTION (index)];
        break;

      default:
        return index;
      }
}

#ifdef RE_FEATURE__NAMED_CAPTURES

/* static */ bool
RE_Compiler::NamedCaptureElm::Find (NamedCaptureElm *nce, const uni_char *name, unsigned name_length, unsigned &index)
{
  while (nce)
    if ((unsigned) nce->name.Length () == name_length && op_memcmp (name, nce->name.CStr (), name_length) == 0)
      {
        index = nce->index;
        return true;
      }
    else
      nce = nce->next;
  return false;
}

#endif // RE_FEATURE__NAMED_CAPTURES

void
RE_Compiler::StringElm::GetIndexed (unsigned index, uni_char *&string, unsigned &length) const
{
  const StringElm *s;
  unsigned count = 1;

  s = this;
  while (s->previous)
    {
      ++count;
      s = s->previous;
    }

  index = count - index;

  s = this;
  while (--index != 0)
    s = s->previous;

  string = s->string;
  length = s->length;
}

RE_Class *
RE_Compiler::ClassElm::GetIndexed (unsigned index) const
{
  const ClassElm *c;
  unsigned count = 1;

  c = this;
  while (c->previous)
    {
      ++count;
      c = c->previous;
    }

  index = count - index;

  c = this;
  while (--index != 0)
    c = c->previous;

  return c->cls;
}

void
RE_Compiler::PushBytecodeSegmentL (unsigned start_index)
{
  ++bytecode_segments_total;

  if (bytecode_segments_total <= 8)
    bytecode_segments = OP_NEW_L(BytecodeSegment, (start_index, bytecode_segments));
  else
    {
      BytecodeSegment::Delete(bytecode_segments);
      bytecode_segments = 0;
    }
}
