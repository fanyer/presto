/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/regexp/src/re_config.h"
#include "modules/regexp/src/re_searcher.h"
#include "modules/regexp/src/re_class.h"

void
RE_Searcher::Add (RE_Class *cls, unsigned segment)
{
  for (int ch = 0; ch < BITMAP_RANGE; ++ch)
    if (cls->bitmap[ch])
      Add (ch, segment);

  /* Ranges and sets are used to represent characters outside the
     bitmap range only, so we don't need to inspect them closer than
     this.

     If the class is inverted it most likely matches at least one
     character outside the bitmap range.  Sure, it could have a range
     that includes everything outside the bitmap range, but who writes
     a class like that? */
  if (cls->map || cls->inverted)
    all |= 1 << segment;
}

bool
RE_Searcher::ForgetSegments ()
{
  /* When we don't use segments at all, we'll say that any potential
     matches are for segment 1.  Segment 1 in the RE_Object will be
     the whole expression. */

  bool matches_all = true;

  for (unsigned index = 0; index < BITMAP_RANGE; ++index)
    if (bitmap[index])
      bitmap[index] = 1;
    else
      matches_all = false;

  if (matches_all && all)
    return false;
  else if (all)
    all = 1;

  return true;
}

void
RE_Searcher::MatchAnything (unsigned segment)
{
  for (unsigned index = 0; index < BITMAP_RANGE; ++index)
    bitmap[index] |= 1 << segment;
  all |= 1 << segment;
}
