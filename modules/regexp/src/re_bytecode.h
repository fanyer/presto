/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_BYTECODE_H
#define RE_BYTECODE_H

#include "modules/regexp/src/re_instructions.h"

class RE_TreeNode;
class RE_Class;

class RE_BytecodeCompiler
{
public:
  class JumpTarget;

private:
  friend class JumpTarget;

  OpMemGroup *arena;

  unsigned *bytecode, bytecode_used, bytecode_allocated;
  void Write (unsigned op);

  unsigned *targets, targets_used, targets_allocated;
  JumpTarget AddJumpTarget (unsigned target = UINT_MAX);

  const uni_char **strings, **alternative_strings;
  unsigned *string_lengths, strings_used, strings_allocated;

  RE_Class **classes;
  unsigned classes_used, classes_allocated;

public:
  RE_BytecodeCompiler (OpMemGroup *arena);

  class JumpTarget
  {
  public:
    JumpTarget () : id (UINT_MAX) {}

    BOOL IsValid () { return id != UINT_MAX; }

  private:
    friend class RE_BytecodeCompiler;

    JumpTarget (unsigned id, unsigned *target) : id (id), target (target) {}

    unsigned id;
    unsigned *target;
  };

  void Instruction (RE_Instructions::Instruction instruction, unsigned argument = 0);
  void Operand (unsigned value);

  unsigned AddString (const uni_char *string, unsigned string_length, BOOL case_insensitive);
  unsigned AddClass (RE_Class *cls);

  JumpTarget Here ();
  JumpTarget ForwardJump ();

  void Jump (const JumpTarget &target, RE_Instructions::Instruction instruction, unsigned argument = 0);
  void SetJumpTarget (const JumpTarget &target) { *target.target = bytecode_used; }

  void Compile (RE_TreeNode *expression);

#ifdef RE_FEATURE__DISASSEMBLER
  void Disassemble (OpString &output, const uni_char *source, unsigned source_length);
#endif // RE_FEATURE__DISASSEMBLER
};

#endif // RE_BYTECODE_H
