/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_SEARCHER_H
#define RE_SEARCHER_H

#include "modules/regexp/src/re_class.h"

class RE_Compiler;

class RE_Searcher
{
public:
  RE_Searcher ()
    : all (0)
  {
    op_memset (bitmap, 0, BITMAP_RANGE);
  }

  unsigned char Search (const uni_char *input, unsigned input_length, unsigned offset, unsigned &match)
  {
    const uni_char *ptr = input + offset, *ptr_end = input + input_length;
    while (ptr != ptr_end)
      {
        if (*ptr > 255)
          {
            if (all != 0)
              {
                match = ptr - input;
                return all;
              }
          }
        else if (bitmap[*ptr] != 0)
          {
            match = ptr - input;
            return bitmap[*ptr];
          }

        ++ptr;
      }

    return 0;
  }

private:
  friend class RE_Compiler;

  void Add (RE_Class *cls, unsigned segment);

  bool ForgetSegments ();
  /**< Forget about segment specific things; the expression had more
       than eight segments.  Returns false if the searcher is useless
       (will skip no characters.) */

  void MatchAnything (unsigned segment);

  void Add (int character, unsigned segment)
  {
    if (character < BITMAP_RANGE)
      bitmap[character] |= 1 << segment;
    else
      all |= 1 << segment;
  }

  unsigned char bitmap[BITMAP_RANGE], all;
};

#endif /* RE_SEARCHER_H */
