/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/regexp/src/re_tree.h"
#include "modules/regexp/src/re_bytecode.h"

static bool
RE_IsCaseSensitive (uni_char ch)
{
  uni_char lower = uni_tolower (ch), upper = uni_toupper (ch);
  if (lower != ch || upper != ch && !(upper < 128 && ch >= 128))
    return true;
  else
    return false;
}

static bool
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

RE_Characters::RE_Characters (const uni_char *characters, unsigned characters_count, BOOL case_insensitive0)
  : characters (characters),
    characters_count (characters_count),
    case_insensitive (FALSE),
    complicated (FALSE)
{
  if (case_insensitive0)
    for (unsigned index = 0; index < characters_count; ++index)
      if (RE_IsCaseSensitive (characters[index]))
        {
          case_insensitive = TRUE;
          if (RE_IsComplicated (characters[index]))
            complicated = TRUE;
        }
}

/* virtual */ void
RE_Alternatives::GenerateBytecode (RE_BytecodeCompiler &compiler)
{
  RE_BytecodeCompiler::JumpTarget finished (compiler.ForwardJump ());

  for (unsigned index = 0; index < alternatives_count - 1; ++index)
    {
      RE_BytecodeCompiler::JumpTarget next (compiler.ForwardJump ());
      compiler.Jump (next, RE_Instructions::PUSH_CHOICE);
      alternatives[index]->GenerateBytecode (compiler);
      compiler.Jump (finished, RE_Instructions::JUMP);
      compiler.SetJumpTarget (next);
    }

  alternatives[alternatives_count - 1]->GenerateBytecode (compiler);
  compiler.SetJumpTarget (finished);
}

/* virtual */ void
RE_Sequence::GenerateBytecode (RE_BytecodeCompiler &compiler)
{
  for (unsigned index = 0; index < nodes_count; ++index)
    nodes[index]->GenerateBytecode (compiler);
}

/* virtual */ void
RE_Quantifier::GenerateBytecode (RE_BytecodeCompiler &compiler)
{
  BOOL full_loop = FALSE;

  if (min == 1 && max == 1)
    {
      subject->GenerateBytecode (compiler);
      return;
    }
  else if (greedy && subject->GetNodeType () == TYPE_SIMPLE_ATOM && static_cast<RE_SimpleAtom *> (subject)->GetType () == RE_SimpleAtom::TYPE_PERIOD)
    compiler.Instruction (RE_Instructions::LOOP_PERIOD, index);
  else if (greedy && subject->GetNodeType () == TYPE_CHARACTERS && static_cast<RE_Characters *> (subject)->GetCharactersCount () == 1 && !static_cast<RE_Characters *> (subject)->IsComplicated ())
    compiler.Instruction (static_cast<RE_Characters *> (subject)->IsCaseInsensitive () ? RE_Instructions::LOOP_CHARACTER_CI : RE_Instructions::LOOP_CHARACTER_CS, static_cast<RE_Characters *> (subject)->GetCharacters ()[0]);
  else if (greedy && subject->GetNodeType () == TYPE_CHARACTER_CLASS)
    compiler.Instruction (RE_Instructions::LOOP_CLASS, compiler.AddClass (static_cast<RE_CharacterClass *> (subject)->GetClass ()));
  else
    {
      compiler.Instruction (RE_Instructions::START_LOOP, index);
      full_loop = TRUE;
    }

  compiler.Operand (min);
  compiler.Operand (max);

  if (full_loop)
    {
      RE_BytecodeCompiler::JumpTarget target (compiler.Here ());
      subject->GenerateBytecode (compiler);
      compiler.Jump (target, greedy ? RE_Instructions::LOOP_GREEDY : RE_Instructions::LOOP, index);
    }
}

/* virtual */ void
RE_Capture::GenerateBytecode (RE_BytecodeCompiler &compiler)
{
  compiler.Instruction (RE_Instructions::CAPTURE_START, 1 | (index << 1));
  contents->GenerateBytecode (compiler);
  compiler.Instruction (RE_Instructions::CAPTURE_END);
}

/* virtual */ void
RE_Lookahead::GenerateBytecode (RE_BytecodeCompiler &compiler)
{
  RE_BytecodeCompiler::JumpTarget target (compiler.ForwardJump ()), finished;

  compiler.Jump (target, RE_Instructions::PUSH_CHOICE_MARK);
  contents->GenerateBytecode (compiler);
  compiler.Instruction (RE_Instructions::POP_CHOICE_MARK);

  if (positive)
    {
      finished = compiler.ForwardJump ();
      compiler.Jump (finished, RE_Instructions::JUMP);
      compiler.SetJumpTarget (target);
    }

  compiler.Instruction (RE_Instructions::FAILURE);

  if (positive)
    compiler.SetJumpTarget (finished);
  else
    compiler.SetJumpTarget (target);
}

void
RE_Characters::GenerateFragment (RE_BytecodeCompiler &compiler, unsigned start, unsigned length)
{
  if (length == 1)
    if (case_insensitive && RE_IsCaseSensitive (characters[start]))
      if (RE_IsComplicated (characters[start]))
        compiler.Instruction (RE_Instructions::MATCH_CHARACTER_CI_SLOW, characters[start]);
      else
        {
          compiler.Instruction (RE_Instructions::MATCH_CHARACTER_CI);
          compiler.Operand (characters[start] | (RE_GetAlternativeChar (characters[start]) << 16));
        }
    else
      compiler.Instruction (RE_Instructions::MATCH_CHARACTER_CS, characters[start]);
  else
    {
      BOOL fragment_ci = FALSE;

      if (case_insensitive)
        for (unsigned index = 0; index < length; ++index)
          if (RE_IsCaseSensitive (characters[start + index]))
            {
              fragment_ci = TRUE;
              break;
            }

      unsigned argument = compiler.AddString (characters + start, length, fragment_ci);

      if (fragment_ci)
        compiler.Instruction (RE_Instructions::MATCH_STRING_CI, argument);
      else
        compiler.Instruction (RE_Instructions::MATCH_STRING_CS, argument);
    }
}

/* virtual */ void
RE_Characters::GenerateBytecode (RE_BytecodeCompiler &compiler)
{
  if (!complicated)
    GenerateFragment (compiler, 0, characters_count);
  else
    {
      unsigned index = 0;

      while (index < characters_count)
        if (RE_IsComplicated (characters[index]))
          GenerateFragment (compiler, index++, 1);
        else
          {
            unsigned start = index;

            do
              ++index;
            while (index < characters_count && !RE_IsComplicated (characters[index]));

            GenerateFragment (compiler, start, index - start);
          }
    }
}

/* virtual */ void
RE_CharacterClass::GenerateBytecode (RE_BytecodeCompiler &compiler)
{
  compiler.Instruction (RE_Instructions::MATCH_CLASS, compiler.AddClass (cls));
}

/* virtual */ void
RE_SimpleAtom::GenerateBytecode (RE_BytecodeCompiler &compiler)
{
  switch (type)
    {
    case TYPE_PERIOD:
      compiler.Instruction (RE_Instructions::MATCH_PERIOD);
      break;

    case TYPE_ASSERT_LINE_START:
      compiler.Instruction (RE_Instructions::ASSERT_LINE_START);
      break;

    case TYPE_ASSERT_LINE_END:
      compiler.Instruction (RE_Instructions::ASSERT_LINE_END);
      break;

    case TYPE_ASSERT_WORD_EDGE:
      compiler.Instruction (RE_Instructions::ASSERT_WORD_EDGE);
      break;

    case TYPE_ASSERT_NOT_WORD_EDGE:
      compiler.Instruction (RE_Instructions::ASSERT_NOT_WORD_EDGE);
      break;
    }
}

#ifdef RE_FEATURE__MACHINE_CODED

/* virtual */ void
RE_Alternatives::GenerateNative (RE_NativeCompiler &compiler)
{
}

/* virtual */ void
RE_Sequence::GenerateNative (RE_NativeCompiler &compiler)
{
}

/* virtual */ void
RE_Quantifier::GenerateNative (RE_NativeCompiler &compiler)
{
}

/* virtual */ void
RE_Capture::GenerateNative (RE_NativeCompiler &compiler)
{
}

/* virtual */ void
RE_Lookahead::GenerateNative (RE_NativeCompiler &compiler)
{
}

/* virtual */ void
RE_Characters::GenerateNative (RE_NativeCompiler &compiler)
{
}

/* virtual */ void
RE_CharacterClass::GenerateNative (RE_NativeCompiler &compiler)
{
}

/* virtual */ void
RE_SimpleAtom::GenerateNative (RE_NativeCompiler &compiler)
{
}

#endif // RE_FEATURE__MACHINE_CODED

static BOOL
AddFollower (OpVector<RE_TreeNode> &followers, RE_TreeNode *node, BOOL in_negative_lookahead)
{
  switch (node->GetNodeType ())
    {
    case RE_TreeNode::TYPE_ALTERNATIVES:
      {
        RE_Alternatives *alternatives = static_cast<RE_Alternatives *> (node);
        RE_TreeNode **nodes = alternatives->GetNodes ();
        unsigned nodes_count = alternatives->GetNodesCount ();
        BOOL result = FALSE;

        for (unsigned index = 0; index < nodes_count; ++index)
          result = AddFollower (followers, nodes[index], in_negative_lookahead) || result;

        return result;
      }

    case RE_TreeNode::TYPE_SEQUENCE:
      {
        RE_Sequence *sequence = static_cast<RE_Sequence *> (node);
        RE_TreeNode **nodes = sequence->GetNodes ();
        unsigned nodes_count = sequence->GetNodesCount ();

        for (unsigned index = 0; index < nodes_count; ++index)
          if (!AddFollower (followers, nodes[index], in_negative_lookahead))
            return FALSE;

        return TRUE;
      }

    case RE_TreeNode::TYPE_QUANTIFIER:
      {
        RE_Quantifier *quantifier = static_cast<RE_Quantifier *> (node);

        return AddFollower (followers, quantifier->GetSubject (), in_negative_lookahead) || quantifier->GetMinimum () == 0;
      }

    case RE_TreeNode::TYPE_CAPTURE:
      {
        RE_Capture *capture = static_cast<RE_Capture *> (node);

        return AddFollower (followers, capture->GetContents (), in_negative_lookahead);
      }

    case RE_TreeNode::TYPE_LOOKAHEAD:
      {
        RE_Lookahead *lookahead = static_cast<RE_Lookahead *> (node);

        if (!lookahead->IsPositive ())
          in_negative_lookahead = !in_negative_lookahead;

        AddFollower (followers, lookahead->GetContents (), in_negative_lookahead);

        return TRUE;
      }

    case RE_TreeNode::TYPE_SIMPLE_ATOM:
      {
        RE_SimpleAtom *simple_atom = static_cast<RE_SimpleAtom *> (node);

        if (simple_atom->GetType () != RE_SimpleAtom::TYPE_PERIOD)
          return TRUE;
      }
    }

  if (!node->mark)
    {
      LEAVE_IF_ERROR (followers.Add (node));

      node->mark = TRUE;
      node->in_negative_lookahead = in_negative_lookahead;
    }

  return FALSE;
}

/* static */ void
RE_TreeNode::GetFollowers (RE_TreeNode *from, OpVector<RE_TreeNode> &followers)
{
  RE_TreeNode *child = from;

  while (RE_TreeNode *parent = child->parent)
    {
      switch (parent->GetNodeType ())
        {
        case TYPE_ALTERNATIVES:
          break;

        case TYPE_SEQUENCE:
          {
            RE_Sequence *sequence = static_cast<RE_Sequence *> (parent);
            RE_TreeNode **nodes = sequence->GetNodes ();
            unsigned nodes_count = sequence->GetNodesCount ();

            for (unsigned index = child->index; index < nodes_count; ++index)
              if (!AddFollower (followers, nodes[index], FALSE))
                return;
          }
          break;

        case TYPE_QUANTIFIER:
          {
            RE_Quantifier *quantifier = static_cast<RE_Quantifier *> (parent);

            AddFollower (followers, quantifier->GetSubject (), FALSE);
          }
          break;

        case TYPE_LOOKAHEAD:
          return;
        }

      child = parent;
    }
}
