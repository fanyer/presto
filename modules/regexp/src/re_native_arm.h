/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_NATIVE_ARM_H
#define RE_NATIVE_ARM_H

#ifdef ARCHITECTURE_ARM

#include "modules/ecmascript/carakan/src/util/es_codegenerator_arm.h"

/*

  Register usage:

   Upon function entry:

    R0: pointer to RegExpMatchPointers array
    R1: pointer to string
    R2: match index (offset in string to match at)
    R3: string length - match index

   In function:

    R0: scratch 1
    R1: current character
    R2: scratch 2
    R3: scratch 3 (scratched by LoadCharacter() and other primitives)

    R4: pointer to RegExpMatchPointers array
    R5: pointer to string
    R6: pointer to current character
    R7: remaining characters
    R8: loop counter

 */

#define REG_SCRATCH1  ES_CodeGenerator::REG_R0
#define REG_CHARACTER ES_CodeGenerator::REG_R1
#define REG_SCRATCH2  ES_CodeGenerator::REG_R2
#define REG_SCRATCH3  ES_CodeGenerator::REG_R3
#define REG_RESULT    ES_CodeGenerator::REG_R4
#define REG_STRING    ES_CodeGenerator::REG_R5
#define REG_INDEX     ES_CodeGenerator::REG_R6
#define REG_LENGTH    ES_CodeGenerator::REG_R7
#define REG_COUNT     ES_CodeGenerator::REG_R8

class RE_ArchitectureMixin
{
public:
  RE_ArchitectureMixin ()
    : c_ContainsCharacter (0),
      loop_level (0)
  {
  }

  void LoadCharacter (int offset);

  ES_CodeGenerator::Constant *c_ContainsCharacter;
  unsigned loop_level;
};

#endif // ARCHITECTURE_ARM
#endif // RE_NATIVE_IA32_H
