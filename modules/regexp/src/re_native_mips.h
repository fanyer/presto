/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_NATIVE_MIPS_H
#define RE_NATIVE_MIPS_H

#ifdef ARCHITECTURE_MIPS

/*
  Register usage:

   Upon function entry:

    A0: pointer to RegExpMatchPointers array
    A1: pointer to string
    A2: match index (offset in string to match at)
    A3: string length - match index

   In function:

    S0: current character
    S1: pointer to RegExpMatchPointers array
    S2: pointer to string
    S3: pointer to current character
    S4: remaining characters
    S5: loop counter
*/

class RE_ArchitectureMixin
{
public:
    RE_ArchitectureMixin () : loop_level (0)
    {
    }

    unsigned loop_level;
};

#endif // ARCHITECTURE_MIPS
#endif // RE_NATIVE_MIPS_H
