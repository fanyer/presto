/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPPATTERNMACHINE_H
#define XPPATTERNMACHINE_H

#ifdef XPATH_PATTERN_SUPPORT

class XPath_PatternMachine
{
private:
  /* Registers: */
  XPath_Expression **predicates;
  XPath_Scan **scans;
  XPath_XMLTreeAccessorFilter **filters;
  XPath_Node *nodes;
  XPath_NodeList *nodelists;
  unsigned nodelist_indeces;
  unsigned *numbers;

  unsigned predicates_count;
  unsigned scans_count;
  unsigned filters_count;
  unsigned nodes_count;
  unsigned nodelists_count;
  unsigned numbers_count;

  class Instruction
  {
  public:
    enum Code
      {
        CODE_RESET,
        CODE_GET_PARENT,
        CODE_CHILD,
        CODE_ANCESTOR_OR_SELF,
        CODE_ANCESTOR,
        CODE_FILTER,
        CODE_EVALUATE,
        CODE_EQUAL,
        CODE_ADD_NODE,
        CODE_ITERATE_LIST,
        CODE_ADD_NUMBER,
        CODE_INCREMENT_NUMBER,
        CODE_RESET_NUMBER,
        CODE_RETURN,
        CODE_YIELD
      };

    Code code;
    unsigned arguments[5];
    /**< Arguments.  Typically indeces into arrays.  High bits set usually
         indicate alternative meanings (that is, mask away and index some
         other array.)  Which array (and which alternatives) depends on the
         instruction.  The target instruction for instructions that do or
         might jump is always index 4, even if it takes less than 4 other
         arguments.  Unused arguments should be ~0u. */
  };

  Instruction *instructions;
  unsigned instruction_index, instructions_count;

  void AddInstructionL (Instruction::Code code, unsigned arg0 = ~0u, unsigned arg1 = ~0u, unsigned arg2 = ~0u, unsigned arg3 = ~0u);
  unsigned AddPredicateL (XPath_Expression *expression);
  unsigned AddScanL (XPath_Scan *scan);
  unsigned AddFilterL (XPath_XMLTreeAccessorFilter *filter);
  unsigned AddNode () { return nodes_count++; }
  unsigned AddNodeList () { return nodelists_count++; }
  unsigned AddNumber () { return numbers_count++; }

  void SetJumpTo (unsigned destination);
  /**< Set the jump target of the current (last added) instruction to be the
       destination:th instruction. */
  void SetJumpFrom (Instruction *source);
  /**< Set the jump target of the source:th instruction to the current (last
       added) instruction. */

public:
  static XPath_PatternMachine *MakeMatchL (XPath_Node *node, XPath_Pattern *pattern);
  /**< Construct a pattern machine for matching 'node' against a single
       pattern. */

  static XPath_PatternMachine *MakeSearchL (XPath_Node *root, XPath_Pattern **patterns, unsigned patterns_count);
  /**< Construct a pattern machine for searching a tree for nodes matching at
       least one of a list of patterns. */

  static XPath_PatternMachine *MakeCountL (XPathPattern::Count::Level level, XPath_Node *origin, XPath_Pattern **count_patterns, unsigned count_patterns_count, XPathPattern **from_patterns, unsigned from_patterns_count);
  /**< Construct a pattern machine for counting nodes matching at least one of
       the patterns in 'count_patterns', according to 'level' and limited by
       the patterns in 'from_patterns'. */

  void SetMatchKey (XPathExtensions::MatchKey *match_key);
};

#endif // XPPATTERNMACHINE_H
#endif // XPATH_PATTERN_SUPPORT
