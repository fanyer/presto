/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_CLASS_H
#define RE_CLASS_H

#define BITMAP_RANGE 256
#define BITMAP_MASK 0xff

#ifdef RE_COMPACT_CLASS_BITMAP
# define BITMAP_ELEMENTS (BITMAP_RANGE / 32)
#else // RE_COMPACT_CLASS_BITMAP
# define BITMAP_ELEMENTS BITMAP_RANGE
#endif // RE_COMPACT_CLASS_BITMAP

class RE_Searcher;
class RE_ExcludeIncludeRange;

class RE_Class
{
public:
  RE_Class (bool case_insensitive)
    : case_insensitive (case_insensitive),
      map (0),
      map_size (0)
#ifdef RE_FEATURE__CHARACTER_CLASS_INTERSECTION
    , intersect_with (0)
    , intersect_next (0)
#endif // RE_FEATURE__CHARACTER_CLASS_INTERSECTION
#ifdef RE_FEATURE__MACHINE_CODED
    , ch1 (-1)
    , ch2 (-1)
    , ch3 (-1)
    , ch4 (-1)
    , is_range1 (false)
    , is_range2 (false)
    , can_machine_code (true)
#endif // RE_FEATURE__MACHINE_CODED
    , builtin_class (BUILTIN_EMPTY)
  {
  }

  ~RE_Class ()
  {
    OP_DELETEA(map);
#ifdef RE_FEATURE__CHARACTER_CLASS_INTERSECTION
    OP_DELETE(intersect_with);
    OP_DELETE(intersect_next);
#endif // RE_FEATURE__CHARACTER_CLASS_INTERSECTION
  }

  bool ConstructL (unsigned depth, const uni_char *source, unsigned &index, unsigned length, bool extended);
  void ConstructL (int predefined);

  bool ConstructL (const uni_char *source, unsigned &index, unsigned length, bool full, bool extended);
  bool Match (int character);

  bool IsWordCharacter () { return builtin_class == BUILTIN_WORD_CHARACTER; }

#ifdef ES_FEATURE__ERROR_MESSAGES
  static bool literal;
  static const char *error_string;
#endif /* ES_FEATURE__ERROR_MESSAGES */

#ifdef RE_FEATURE__MACHINE_CODED
  bool CanMachineCode () { return can_machine_code; }
  void GetCharacters (int &och1, int &och2, int &och3, int &och4, bool &ois_range1, bool &ois_range2, bool &oinverted)
  {
    och1 = ch1;
    och2 = ch2;
    och3 = ch3;
    och4 = ch4;
    ois_range1 = is_range1;
    ois_range2 = is_range2;
    oinverted = inverted;
  }

  bool HasHighCharacterMap () { return map != 0; }
  bool IsInverted ()  { return inverted; }
  unsigned char *GetBitmap () { return bitmap; }
#endif // RE_FEATURE__MACHINE_CODED


protected:
  friend class RE_Searcher;
  friend class RE_Compiler;
  friend class RE_Native;

  void SetCharacter (int character);
  void SetRange (int first, int last);

  static int RE_CALLING_CONVENTION ContainsCharacter (RE_Class *self, int character);
  /**< Returns true if this set locally (disregarding intersections) contains
       the character.  Note: this function will only return true for characters
       outside the bitmap range. */
  bool SlowMatch (int character);
  /**< Returns true if this character class matches the character, taking into
       account complementation (invertion) and intersections.  Note: this
       function can only be called for characters outside the bitmap range. */

  void ConstructMap (RE_ExcludeIncludeRange *eir);

  bool inverted, case_insensitive;

  int *map;
  unsigned map_size;

#ifdef RE_FEATURE__CHARACTER_CLASS_INTERSECTION
  RE_Class *intersect_with, *intersect_next;
#endif // RE_FEATURE__CHARACTER_CLASS_INTERSECTION

#ifdef RE_COMPACT_CLASS_BITMAP
  unsigned bitmap[BITMAP_ELEMENTS];
#else // RE_COMPACT_CLASS_BITMAP
  unsigned char bitmap[BITMAP_ELEMENTS]; // ARRAY OK 2009-05-26 stanislavj
#endif // RE_COMPACT_CLASS_BITMAP

#ifdef RE_FEATURE__MACHINE_CODED
  void SetCharacterX (int ch);
  void SetRangeX (int ch1, int ch2);

  int ch1, ch2, ch3, ch4;
  bool is_range1, is_range2, can_machine_code;
#endif // RE_FEATURE__MACHINE_CODED

  enum BuiltInClass
    {
      BUILTIN_EMPTY,
      BUILTIN_DIGIT,
      BUILTIN_NON_DIGIT,
      BUILTIN_WHITESPACE,
      BUILTIN_NON_WHITESPACE,
      BUILTIN_WORD_CHARACTER,
      BUILTIN_NON_WORD_CHARACTER,
      BUILTIN_NONE
    };

  BuiltInClass builtin_class;
};

inline bool
RE_Class::Match (int character)
{
  if (character < BITMAP_RANGE)
    {
#ifdef RE_COMPACT_CLASS_BITMAP
      unsigned a = (unsigned) character >> 5;
      unsigned b = (unsigned) character & 31;

      return bitmap[a] & (1 << b);
#else // RE_COMPACT_CLASS_BITMAP
      return bitmap[character] != 0;
#endif // RE_COMPACT_CLASS_BITMAP
    }
  else
    return map ? SlowMatch (character) : inverted;
}

#endif /* RE_CLASS_H */
