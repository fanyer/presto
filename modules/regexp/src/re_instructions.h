/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_INSTRUCTIONS_H
#define RE_INSTRUCTIONS_H

#undef SUCCESS
#undef FAILURE

#include "modules/regexp/src/re_config.h"

class RE_Instructions
{
public:
  enum Instruction
    {
      MATCH_PERIOD,             // -
      MATCH_CHARACTER_CS,       // ushort: character
      MATCH_CHARACTER_CI,       // ulong: upper << 16 | upper
      MATCH_CHARACTER_CI_SLOW,  // ushort: character
      MATCH_STRING_CS,          // ushort: string index
      MATCH_STRING_CI,          // ushort: string index
      MATCH_CLASS,              // ushort: class index
      MATCH_CAPTURE,            // ushort: capture index

      ASSERT_LINE_START,        // -
      ASSERT_LINE_END,          // -
      ASSERT_WORD_EDGE,         // -
      ASSERT_NOT_WORD_EDGE,     // -

      PUSH_CHOICE,              // long: offset
      PUSH_CHOICE_MARK,         // long: offset
      POP_CHOICE_MARK,          // -

      START_LOOP,               // ushort: loop index, ulong: min, max
      LOOP_GREEDY,              // ushort: loop index, long: offset
      LOOP,                     // ushort: loop index, long: offset

      LOOP_PERIOD,		// ushort: loop index, ulong: min, max
      LOOP_CHARACTER_CS,	// ushort: character, ushort: loop index, ulong: min, max
      LOOP_CHARACTER_CI,	// ushort: character, ushort: loop index, ulong: min, max
      LOOP_CLASS,		// ushort: class index, ushort: loop index, ulong: min, max

      JUMP,                     // long: offset

      CAPTURE_START,            // ushort: capture index
      CAPTURE_END,              // ushort: capture index

      RESET_LOOP,               // ushort: loop index

      SUCCESS,                  // -
      FAILURE                   // -
    };
};

extern const unsigned RE_InstructionLengths[];

#endif /* RE_INSTRUCTIONS_H */
