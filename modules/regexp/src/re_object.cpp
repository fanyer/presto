/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/regexp/src/re_config.h"
#include "modules/regexp/src/re_object.h"
#include "modules/regexp/src/re_class.h"
#include "modules/regexp/src/re_searcher.h"

RE_Object::RE_Object (unsigned *bytecode,
                      unsigned bytecode_length,
                      unsigned **bytecode_segments,
                      unsigned bytecode_segments_count,
                      unsigned captures,
                      unsigned loops,
                      unsigned strings_count,
                      unsigned *string_lengths,
                      uni_char **strings,
                      uni_char **alternative_strings,
                      unsigned classes_count,
                      RE_Class **classes,
                      uni_char *source,
                      RE_Searcher *searcher)
  : bytecode (bytecode)
  , bytecode_length (bytecode_length)
  , bytecode_segments (bytecode_segments)
  , bytecode_segments_count (bytecode_segments_count)
  , captures (captures)
  , loops (loops)
  , match_offset (0)
  , strings_count (strings_count)
  , string_lengths (string_lengths)
  , strings (strings)
  , alternative_strings (alternative_strings)
  , classes_count (classes_count)
  , classes (classes)
#ifdef RE_FEATURE__NAMED_CAPTURES
  , capture_names (0)
#endif // RE_FEATURE__NAMED_CAPTURES
  , source (source)
  , searcher (searcher)
#ifdef RE_FEATURE__MACHINE_CODED
  , native_failed (false)
  , fast_matcher (0)
  , fast_matcher_block (0)
#endif // RE_FEATURE__MACHINE_CODED
{
  if (!bytecode_segments || bytecode_segments_count == 0)
    {
      this->bytecode_segments = &this->bytecode;
      this->bytecode_segments_count = 1;
    }
}

RE_Object::~RE_Object ()
{
  OP_DELETEA (bytecode);
  if (bytecode_segments != &bytecode)
    OP_DELETEA (bytecode_segments);
  OP_DELETEA (string_lengths);
  unsigned index;
  for (index = 0; index < strings_count; ++index)
    {
      OP_DELETEA (strings[index]);
      OP_DELETEA (alternative_strings[index]);
    }
  OP_DELETEA (strings);
  OP_DELETEA (alternative_strings);
  for (index = 0; index < classes_count; ++index)
    OP_DELETE (classes[index]);
  OP_DELETEA (classes);
#ifdef RE_FEATURE__NAMED_CAPTURES
  OP_DELETEA (capture_names);
#endif // RE_FEATURE__NAMED_CAPTURES
  OP_DELETEA (source);
  OP_DELETE (searcher);
#ifdef RE_FEATURE__MACHINE_CODED
  if (fast_matcher_block)
    OpExecMemoryManager::Free(fast_matcher_block);
#endif // RE_FEATURE__MACHINE_CODED
}

#ifdef RE_FEATURE__NAMED_CAPTURES

unsigned
RE_Object::CountNamedCaptures ()
{
  unsigned count = 0;
  if (capture_names)
    for (unsigned index = 0; index < captures; ++index)
      if (capture_names[index].HasContent ())
        ++count;
  return count;
}

#endif // RE_FEATURE__NAMED_CAPTURES
