/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_TREE_H
#define RE_TREE_H

#include "modules/util/adt/opvector.h"

class RE_Class;
class RE_BytecodeCompiler;

#ifdef RE_FEATURE__MACHINE_CODED
class RE_NativeCompiler;
#endif // RE_FEATURE__MACHINE_CODED

class RE_TreeNode
{
public:
  enum NodeType
    {
      TYPE_ALTERNATIVES,
      TYPE_SEQUENCE,
      TYPE_QUANTIFIER,
      TYPE_CAPTURE,
      TYPE_LOOKAHEAD,
      TYPE_CHARACTERS,
      TYPE_CHARACTER_CLASS,
      TYPE_SIMPLE_ATOM
    };

  virtual ~RE_TreeNode () {}

  virtual NodeType GetNodeType () = 0;

  virtual void GenerateBytecode (RE_BytecodeCompiler &compiler) = 0;
#ifdef RE_FEATURE__MACHINE_CODED
  virtual void GenerateNative (RE_NativeCompiler &compiler) = 0;
#endif // RE_FEATURE__MACHINE_CODED

  static void GetFollowers (RE_TreeNode *from, OpVector<RE_TreeNode> &followers);

  RE_TreeNode *parent;
  unsigned index;

  BOOL mark;
  BOOL in_negative_lookahead;
};

#define NODETYPE(type) \
  virtual NodeType GetNodeType () { return TYPE_##type; }

#ifdef RE_FEATURE__MACHINE_CODED
#define GENERATE() \
  virtual void GenerateBytecode (RE_BytecodeCompiler &compiler); \
  virtual void GenerateNative (RE_NativeCompiler &compiler);
#else // RE_FEATURE__MACHINE_CODED
#define GENERATE() \
  virtual void GenerateBytecode (RE_BytecodeCompiler &compiler);
#endif // RE_FEATURE__MACHINE_CODED

class RE_Alternatives
  : public RE_TreeNode
{
private:
  RE_TreeNode **alternatives;
  unsigned alternatives_count;

public:
  RE_Alternatives (RE_TreeNode **alternatives, unsigned alternatives_count)
    : alternatives (alternatives), alternatives_count (alternatives_count)
  {
  }

  RE_TreeNode **GetNodes () { return alternatives; }
  unsigned GetNodesCount () { return alternatives_count; }

  NODETYPE (ALTERNATIVES)
  GENERATE ()
};

class RE_Sequence
  : public RE_TreeNode
{
private:
  RE_TreeNode **nodes;
  unsigned nodes_count;

public:
  RE_Sequence (RE_TreeNode **nodes, unsigned nodes_count)
    : nodes (nodes), nodes_count (nodes_count)
  {
    for (unsigned index = 0; index < nodes_count; ++index)
      nodes[index]->index = index;
  }

  RE_TreeNode **GetNodes () { return nodes; }
  unsigned GetNodesCount () { return nodes_count; }

  NODETYPE (SEQUENCE)
  GENERATE ()
};

class RE_Quantifier
  : public RE_TreeNode
{
private:
  RE_TreeNode *subject;
  unsigned index, min, max;
  BOOL greedy;

public:
  RE_Quantifier (RE_TreeNode *subject, unsigned index, unsigned min, unsigned max, BOOL greedy)
    : subject (subject), index (index), min (min), max (max), greedy (greedy)
  {
  }

  RE_TreeNode *GetSubject () { return subject; }
  unsigned GetMinimum () { return min; }
  unsigned GetMaximum () { return max; }

  NODETYPE (QUANTIFIER)
  GENERATE ()
};

class RE_Capture
  : public RE_TreeNode
{
private:
  RE_TreeNode *contents;
  unsigned index;

public:
  RE_Capture (RE_TreeNode *contents, unsigned index)
    : contents (contents), index (index)
  {
  }

  RE_TreeNode *GetContents () { return contents; }

  NODETYPE (CAPTURE)
  GENERATE ()
};

class RE_Lookahead
  : public RE_TreeNode
{
private:
  RE_TreeNode *contents;
  BOOL positive;

public:
  RE_Lookahead (RE_TreeNode *contents, BOOL positive)
    : contents (contents), positive (positive)
  {
  }

  RE_TreeNode *GetContents () { return contents; }
  BOOL IsPositive () { return positive; }

  NODETYPE (LOOKAHEAD)
  GENERATE ()
};

class RE_Characters
  : public RE_TreeNode
{
private:
  const uni_char *characters;
  unsigned characters_count;
  BOOL case_insensitive, complicated;

  void GenerateFragment (RE_BytecodeCompiler &compiler, unsigned start, unsigned length);

public:
  RE_Characters (const uni_char *characters, unsigned characters_count, BOOL case_insensitive);

  BOOL IsCaseInsensitive () { return case_insensitive; }
  BOOL IsComplicated () { return complicated; }

  const uni_char *GetCharacters () { return characters; }
  unsigned GetCharactersCount () { return characters_count; }

  NODETYPE (CHARACTERS)
  GENERATE ()
};

class RE_CharacterClass
  : public RE_TreeNode
{
private:
  RE_Class *cls;

public:
  RE_CharacterClass (RE_Class *cls)
    : cls (cls)
  {
  }

  RE_Class *GetClass () { return cls; }

  NODETYPE (CHARACTER_CLASS)
  GENERATE ()
};

class RE_SimpleAtom
  : public RE_TreeNode
{
public:
  enum Type
    {
      TYPE_PERIOD,
      TYPE_ASSERT_LINE_START,
      TYPE_ASSERT_LINE_END,
      TYPE_ASSERT_WORD_EDGE,
      TYPE_ASSERT_NOT_WORD_EDGE
    };

private:
  Type type;

public:
  RE_SimpleAtom (Type type)
    : type (type)
  {
  }

  Type GetType () { return type; }

  NODETYPE (SIMPLE_ATOM)
  GENERATE ()
};

#endif // RE_TREE_H
