/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_PARSER_H
#define RE_PARSER_H

class RE_TreeNode;

class RE_Parser
{
private:
  OpMemGroup *arena;

  RE_TreeNode **stack;
  unsigned stack_used, stack_allocated;

  void Push (RE_TreeNode *node);

  RE_TreeNode *&Top () { return stack[stack_used - 1]; }
  RE_TreeNode **Pop (unsigned count);

  class Production
  {
  public:
    enum Type
      {
        TYPE_SEQUENCE,                  // plain sequence
        TYPE_ALTERNATIVES,              // x | y | ...
        TYPE_CAPTURE,                   // ( ... )
        TYPE_GROUP,                     // (?: ... )
        TYPE_LOOKAHEAD_POSITIVE,        // (?= ... )
        TYPE_LOOKAHEAD_NEGATIVE,        // (?! ... )

        TYPE_CLOSED_BY_PAREN = TYPE_CAPTURE
      };

    Type type;
    unsigned start;
    unsigned index;                     // capture index
    unsigned name_offset, name_length;  // capture name
    Production *next;
  };

  Production *current, *free_production;

  void Open (Production::Type type);
  void Close ();

  unsigned capture_count, loop_count;

  uni_char *characters;
  unsigned characters_used;

  BOOL case_insensitive;

public:
  RE_Parser (BOOL case_insensitive, OpMemGroup *arena);

  RE_TreeNode *Parse (const uni_char *source, unsigned source_length);
};

#endif // RE_PARSER_H
