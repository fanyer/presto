/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_NATIVE_H
#define RE_NATIVE_H

#include "modules/regexp/src/re_config.h"

#ifdef RE_FEATURE__MACHINE_CODED

#include "modules/regexp/src/re_object.h"
#include "modules/regexp/src/re_instructions.h"
#include "modules/regexp/src/re_class.h"
#include "modules/regexp/src/re_native_arch.h"

#include "modules/memory/src/memory_executable.h"

class RE_Native
  : public RE_ArchitectureMixin
{
public:
  RE_Native (RE_Object *object, OpExecMemoryManager *executable_memory);
  ~RE_Native ();

  bool CreateNativeMatcher (const OpExecMemory *&matcher);

private:
  friend class RE_ArchitectureMixin;

  void EmitGlobalPrologue ();
  void EmitSegmentPrologue (unsigned segment_index);
  void EmitSegmentSuccessEpilogue ();
  /**< Called for fixed length segments when there's no common fixed length. */

  void EmitCheckSegmentLength (unsigned length, ES_CodeGenerator::JumpTarget *failure);
  void EmitSetLengthAndJump (unsigned length, ES_CodeGenerator::JumpTarget *target);

  void EmitMatchPeriod (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure);
  void EmitMatchCharacter (unsigned offset, unsigned ch1, unsigned ch2, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure);
  void EmitMatchString (unsigned offset, const uni_char *string1, const uni_char *string2, unsigned string_length, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure);
  void EmitMatchClass (unsigned offset, RE_Class *cls, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure);
  void EmitAssertWordEdge (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure);
  void EmitAssertLineStart (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure);
  void EmitAssertLineEnd (unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure);

  void EmitCaptureStart (unsigned capture_index, unsigned offset, bool initialize_end);
  void EmitCaptureEnd (unsigned capture_index, unsigned offset);
  void EmitCaptureReset (unsigned capture_index);
  void EmitResetUnreachedCaptures (unsigned first_reached, unsigned last_reached, unsigned captures_count);
  void EmitResetSkippedCaptures (unsigned first_skipped, unsigned last_skipped);

  class LoopBody
  {
  public:
    enum Type { TYPE_PERIOD, TYPE_CHARACTER, TYPE_CLASS } type;

    unsigned ch1, ch2;
    RE_Class *cls;
  };

  class LoopTail
  {
  public:
    const unsigned *bytecode;
    unsigned index;

    bool is_empty, is_fixed_length, has_slow_instructions, has_loop, has_assertions;
    unsigned length, instructions_count;
  };

  bool EmitLoop (BOOL backtracking, unsigned min, unsigned max, unsigned offset, LoopBody &body, LoopTail &tail, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure);
  void GenerateLoopBody (const LoopBody &body, unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure);
  bool GenerateLoopTail (const LoopTail &tail, unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure, bool skip_length_check = false);

  bool GenerateBlock (const unsigned *bytecode, unsigned &index, unsigned &offset, unsigned loop_index, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure, bool skip_length_check = false);
  bool GenerateLoop (const unsigned *bytecode, unsigned &index, unsigned offset, ES_CodeGenerator::JumpTarget *success, ES_CodeGenerator::JumpTarget *failure, bool no_backtracking);

  void GenerateCaptureStart (unsigned capture_index, unsigned offset, bool reset);
  void GenerateResetUnreachedCaptures ();

  void Finish (void *base);

  RE_Object *object;
  OpExecMemoryManager *executable_memory;
  OpMemGroup arena;
  ES_CodeGenerator cg;

  bool case_insensitive;
  bool multiline;
  bool searching;

  bool global_fixed_length;
  /**< True if all segments are the same fixed length (which is then always
       stored in 'segment_length'.) */
  bool segment_variable_length;
  /**< True if at least one segment has a variable length. */
  bool segment_fixed_length;
  /**< True if the current segment has a fixed length.  During the processing of
       this segment 'segment_length' is set to its length. */
  unsigned segment_length;
  /**< Length of current segment if 'segment_fixed_length' is true, and if
       'global_fixed_length' is true, also the length of all segments.
       Otherwise a conservative minimum length of the segment; if fewer
       characters than this are available, the segment will not match, but other
       than that we don't know how many characters might be matched by the
       segment. */
  bool is_backtracking;
  /**< Currently compiling a sub-expression which, if it fails, will cause
       back-tracking.  In that case, further back-tracking will cause failure to
       compile the expression (which will be matched by the bytecode engine.) */

  ES_CodeGenerator::JumpTarget *jt_start, *jt_search, *jt_success, *jt_failure, *jt_segment_success, *jt_segment_failure;

  unsigned first_capture, last_capture;

  class ForwardJump
  {
  public:
    ES_CodeGenerator::JumpTarget *jt;
    unsigned index;

    ForwardJump *next;
  };

  ForwardJump *forward_jumps;

  ES_CodeGenerator::JumpTarget *AddForwardJump (unsigned target_index);
  ES_CodeGenerator::JumpTarget *SetForwardJump (unsigned index);

  unsigned stack_space_used;
  /**< The number of bytes that has currently been pushed
       onto the stack by the generated code */

  bool IsNearStackLimit ();
  /** Check if stack usage is close to maximum allowed, as controlled by
      RE_CONFIG_PARM_MAX_STACK. A return value of 'true' means that
      stack usage is close to maximum allowed and further nested calls
      should not be attempted. Only the compilation of ill-formed or
      pathological regexps will run close to or transgress this limit. */

  unsigned char *control_stack_base;
  /**< Stack pointer upon code generation start. */

};

#endif // RE_FEATURE__MACHINE_CODED
#endif // RE_NATIVE_H
