/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_COMPILER_H
#define RE_COMPILER_H

#include "modules/regexp/src/re_instructions.h"
#include "modules/regexp/src/re_class.h"
#include "modules/regexp/src/re_config.h"
#include "modules/regexp/src/re_object.h"
#include "modules/util/tempbuf.h"
//#include "modules/util/opstring.h"

class RE_Object;
class RE_Class;

class RE_ClassCleanupHelper
{
public:
  RE_ClassCleanupHelper (RE_Class **cs, int count) : cs(cs), count(count) {}
  ~RE_ClassCleanupHelper ()
  {
    if (cs)
      {
        for (int i = 0; i < count; i++)
          OP_DELETE(cs[i]);
        OP_DELETEA(cs);
      }
  }
  void release() { cs = NULL; }

private:
  RE_Class **cs;
  int count;
};

class RE_Compiler
{
public:
  RE_Compiler (bool case_insensitive, bool multiline, bool searching);
  ~RE_Compiler ();

  void SetExtended ();
  void SetLiteral ();

  RE_Object *CompileL (const uni_char *source, unsigned length = ~0u);

  unsigned GetEndIndex ();

#ifdef ES_FEATURE__ERROR_MESSAGES
  const char *GetErrorString ();
#endif /* ES_FEATURE__ERROR_MESSAGES */

#ifdef RE_FEATURE__DISASSEMBLER
  void Disassemble (OpString &target);
#endif /* RE_FEATURE__DISASSEMBLER */

protected:
  bool CompileClassL ();
  void CompileQuantifierL (unsigned min, unsigned max);

  unsigned ReadDecimalDigits ();

  friend class RE_Class;
  friend class RE_Searcher;

  static bool IsIdentifierPart (int character);
  static bool IsDigit (int character);
  static bool IsLetter (int character);
  static bool IsHexDigit (int character);
  static bool IsOctalDigit (int character);
  static int HexToCharacter (int h1);
  static int HexToCharacter (int h1, int h2);
  static int HexToCharacter (int h1, int h2, int h3, int h4);

  bool AddCharacter (uni_char ch);
  void AddString (const uni_char *string = 0, unsigned string_length = ~0u);

  bool FlushStringL (unsigned index_override = ~0u, BOOL skip_last = FALSE);
  bool FlushStringBeforeQuantifierL (unsigned index_override = ~0u);

  class Production
  {
  public:
    enum Type
      {
        DISJUNCTION,
        ALTERNATIVE,
        POSITIVE_LOOKAHEAD,
        NEGATIVE_LOOKAHEAD,
        CAPTURE
      };

    Type type;
    unsigned start_index, content_start_index, forward_jump_index, capture_index, captures;
    bool started_at_start_of_segment, might_match_empty;

    Production *previous;

	/* Use this instead of OP_DELETE(prod) to avoid potentially too deep recursion */
    static void Delete(Production* prod);
  };

  void PushProductionL (Production::Type type);
  void PopProduction ();
  void BeginProduction ();

  Production *top_production;
  Production *last_production;

  void EnsureBytecodeL (unsigned additional = 1);
  void InsertBytecodeL (unsigned index, unsigned length, bool adjust_jumps = false);
  void ResetLoops ();
  void ResetLoopsInsideLookahead ();
  void OptimizeJumps ();

  void WriteInstructionL (RE_Instructions::Instruction instruction, unsigned argument = 0, unsigned next_word = ~0u, const uni_char *string_data = NULL);
  void WriteUnsignedL (unsigned value);
  void SetForwardJump ();

#ifdef RE_FEATURE__EXTENDED_SYNTAX
  void SkipWhitespace ();
#endif // RE_FEATURE__EXTENDED_SYNTAX

  unsigned bytecode_index, bytecode_total;
  unsigned *bytecode;

  unsigned previous_instruction_index;

  const uni_char *source;
  unsigned index, length;

  bool extended;
  bool literal;
  bool case_insensitive;
  bool multiline;
  bool searching;

#ifdef RE_FEATURE__MACHINE_CODED
  unsigned loops_in_segment;
  bool can_machine_code;
#endif // RE_FEATURE__MACHINE_CODED

  RE_Searcher *searcher;
  bool at_start_of_segment;
  bool simple_segments;
  unsigned segment_index;
  unsigned determining_instr_index;

  unsigned captures;
  unsigned loops;

  ES_TempBuffer string_buffer;

  class CaptureElm
  {
  public:
    CaptureElm (unsigned capture_start, CaptureElm *previous)
      : capture_start (capture_start),
        capture_end (~0u),
        last_match_capture (~0u),
        next (0),
        previous (previous)
    {
    }

    unsigned GetIndex ()
    {
      if (previous)
        return previous->GetIndex () + 1;
      else
        return 0;
    }

    CaptureElm *GetIndexed (unsigned index)
    {
      if (index == 0)
        return this;
      else
        return next->GetIndexed (index - 1);
    }

    unsigned capture_start, capture_end;
    unsigned last_match_capture;

    CaptureElm *next, *previous;

    /* Use this instead of OP_DELETE(capture_elm) to avoid potentially too deep recursion */
	static void Delete(CaptureElm *capture_elm);

  private:
    ~CaptureElm () {;}
  };

  CaptureElm *first_capture, *last_capture;

  unsigned ActualCaptureStart (unsigned capture_start);

#ifdef RE_FEATURE__NAMED_CAPTURES
  class NamedCaptureElm
  {
  public:
    NamedCaptureElm (unsigned index)
      : index (index),
        next (0)
    {
    }

    unsigned index;
    OpString name;

    static bool Find (NamedCaptureElm *nce, const uni_char *name, unsigned name_length, unsigned &index);

    NamedCaptureElm *next;

	/* Use this instead of OP_DELETE(nce) to avoid potentially too deep recursion */
	static void Delete(NamedCaptureElm *nce);

  private:
    ~NamedCaptureElm () {;}
  };

  NamedCaptureElm *first_named_capture;
#endif // RE_FEATURE__NAMED_CAPTURES

  unsigned string_start;

  class StringElm
  {
  public:
    StringElm () : string(0), previous(0) {}

    unsigned length;
    uni_char *string;
    bool ci;

    StringElm *previous;

    void GetIndexed (unsigned index, uni_char *&string, unsigned &length) const;

	/* Use this instead of OP_DELETE(str_elm) to avoid potentially too deep recursion */
	static void Delete(StringElm *str_elm);

  private:
	~StringElm () { OP_DELETEA (string); }
  };

  unsigned strings_total;
  StringElm *strings;

  class ClassElm
  {
  public:
    ClassElm () : cls (0) {}

    RE_Class *cls;
    unsigned start_index, length;

    ClassElm *previous;

    RE_Class *GetIndexed (unsigned index) const;

    /* Use this instead of OP_DELETE(class_elm) to avoid potentially too deep recursion */
    static void Delete(ClassElm *class_elm);

  private:
    ~ClassElm () { OP_DELETE (cls); }
  };

  unsigned classes_total;
  ClassElm *classes;

  class BytecodeSegment
  {
  public:
    BytecodeSegment (unsigned start_index, BytecodeSegment *previous)
      : start_index (start_index),
        previous (previous)
    {
#ifdef RE_FEATURE__DISASSEMBLER
      next = 0;
      if (previous)
        previous->next = this;
#endif // RE_FEATURE__DISASSEMBLER
    }

    unsigned start_index;
    BytecodeSegment *previous;
#ifdef RE_FEATURE__DISASSEMBLER
    BytecodeSegment *next;
#endif // RE_FEATURE__DISASSEMBLER

	/* Use this instead of OP_DELETE(class_elm) to avoid potentially too deep recursion */
	static void Delete(BytecodeSegment *bc_seg);

  private:
	  ~BytecodeSegment () {;}
  };

  unsigned bytecode_segments_total;
  BytecodeSegment *bytecode_segments;

  void PushBytecodeSegmentL (unsigned start_index);

#ifdef ES_FEATURE__ERROR_MESSAGES
  const char *error_string;
#endif /* ES_FEATURE__ERROR_MESSAGES */

#ifdef RE_FEATURE__MACHINE_CODED
  RegExpNativeMatcher *CreateFastMatcher (const OpExecMemory *&block, RE_Object *object, unsigned segments_count);
#endif // RE_FEATURE__MACHINE_CODED
};

#endif /* RE_COMPILER_H */
