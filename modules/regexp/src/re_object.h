/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_OBJECT_H
#define RE_OBJECT_H

#include "modules/util/opstring.h"
#include "modules/regexp/include/regexp_advanced_api.h"


class RE_Class;
class RE_Searcher;
class RE_Compiler;
struct RegExpMatch2;


class RE_Object
{
public:
  RE_Object (unsigned *bytecode, unsigned bytecode_length, unsigned **bytecode_segments, unsigned bytecode_segments_count,
             unsigned captures, unsigned loops, unsigned strings_count,
             unsigned *string_lengths, uni_char **strings, uni_char **alternative_strings, unsigned classes_count,
             RE_Class **classes, uni_char *source, RE_Searcher *searcher);
  ~RE_Object ();

  const unsigned *GetBytecode ();
  unsigned GetBytecodeLength () { return bytecode_length; }

  const unsigned *const *GetBytecodeSegments ();
  unsigned GetBytecodeSegmentsCount ();
  unsigned GetCaptures ();
  unsigned GetLoops ();

  unsigned GetMatchOffset () { return match_offset; }

  unsigned GetStringsCount ();
  unsigned *GetStringLengths ();
  const uni_char *const *GetStrings ();
  const uni_char *const *GetAlternativeStrings ();

  unsigned GetClassesCount ();
  RE_Class **GetClasses ();

#ifdef RE_FEATURE__NAMED_CAPTURES
  bool HasNamedCaptures () { return capture_names != 0; }
  unsigned CountNamedCaptures ();
  const uni_char *GetCaptureName (unsigned index) { return capture_names[index].CStr(); }
#endif // RE_FEATURE__NAMED_CAPTURES

  const uni_char *GetSource ();
  RE_Searcher *GetSearcher ();

#ifdef RE_FEATURE__DISASSEMBLER
  OpString &GetDisassembly () { return disassembly; }
#endif // RE_FEATURE__DISASSEMBLER

#ifdef RE_FEATURE__MACHINE_CODED
  bool GetNativeFailed () { return native_failed; }
  bool IsCaseInsensitive () { return case_insensitive; }
  bool IsMultiline () { return multiline; }
  bool IsSearching () { return searching; }

  RegExpNativeMatcher *GetNativeMatcher () { return fast_matcher; }
  void SetNativeMatcher (RegExpNativeMatcher *fm, const OpExecMemory *fmb) { fast_matcher = fm; fast_matcher_block = fmb; }
  void SetNativeFailed () { native_failed = true; }
#endif // RE_FEATURE__JIT

protected:
  friend class RE_Compiler;

  unsigned *bytecode;
  unsigned bytecode_length;
  unsigned **bytecode_segments;
  unsigned bytecode_segments_count;

  unsigned captures;
  unsigned loops;

  unsigned match_offset;

  unsigned strings_count;
  unsigned *string_lengths;
  uni_char **strings, **alternative_strings;

  unsigned classes_count;
  RE_Class **classes;

#ifdef RE_FEATURE__NAMED_CAPTURES
  OpString *capture_names;
#endif // RE_FEATURE__NAMED_CAPTURES

  uni_char *source;
#ifdef RE_FEATURE__DISASSEMBLER
  OpString disassembly;
#endif // RE_FEATURE__DISASSEMBLER

  RE_Searcher *searcher;

#ifdef RE_FEATURE__MACHINE_CODED
  bool native_failed, case_insensitive, multiline, searching;
  RegExpNativeMatcher *fast_matcher;
  const OpExecMemory *fast_matcher_block;
#endif // RE_FEATURE__JIT
};


inline const unsigned *
RE_Object::GetBytecode ()
{
  return bytecode;
}


inline const unsigned *const *
RE_Object::GetBytecodeSegments ()
{
  return bytecode_segments;
}


inline unsigned
RE_Object::GetBytecodeSegmentsCount ()
{
  return bytecode_segments_count;
}


inline unsigned
RE_Object::GetCaptures ()
{
  return captures;
}


inline unsigned
RE_Object::GetLoops ()
{
  return loops;
}


inline unsigned
RE_Object::GetStringsCount ()
{
  return strings_count;
}


inline unsigned *
RE_Object::GetStringLengths ()
{
  return string_lengths;
}


inline const uni_char *const *
RE_Object::GetStrings ()
{
  return strings;
}


inline const uni_char *const *
RE_Object::GetAlternativeStrings ()
{
  return alternative_strings;
}


inline unsigned
RE_Object::GetClassesCount ()
{
  return classes_count;
}


inline RE_Class **
RE_Object::GetClasses ()
{
  return classes;
}


inline const uni_char *
RE_Object::GetSource ()
{
  return source;
}


inline RE_Searcher *
RE_Object::GetSearcher ()
{
  return searcher;
}

#endif /* RE_OBJECT_H */
