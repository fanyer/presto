/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_NATIVE_IA32_H
#define RE_NATIVE_IA32_H

#ifdef ARCHITECTURE_IA32

#include "modules/ecmascript/carakan/src/util/es_codegenerator_ia32.h"

/*

  Upon function entry,

  Registers:

                    x86 | AMD64_UNIX | AMD64_WINDOWS
   -------------------------------------------------
   REG_RESULT       EDI          RDI             RCX
   REG_STRING       ESI          RSI             RDX
   REG_INDEX        EDX          RDX              R8
   REG_LENGTH       ECX          RCX              R9

  Other common:

                    x86 | AMD64_UNIX | AMD64_WINDOWS
    ------------------------------------------------
    REG_ARG_SOURCE  ESI          RSI             RDX
    REG_ARG_INDEX   EDX          RDX              R8
    REG_ARG_LENGTH  ECX          RCX              R9
    REG_CHARACTER   EAX          RAX             RAX
    REG_SCRATCH1    EDI           R8              R8
    REG_SCRATCH2    EBX           R9              R9

   REG_RESULT      Pointer to RegExpMatchPointers array.
   REG_STRING      Pointer to input string.
   REG_INDEX       Match index (offset in string to match at)
   REG_LENGTH      String length - match index

   REG_ARG_SOURCE  Pointer to string.
   REG_ARG_INDEX   Match index.
   REG_ARG_LENGTH  String length - match index
                   (decremented during searching and
                    set to the length of the actual match
                    before jumping to the  generic success epilogue)
   REG_CHARACTER   Current character.

 */

#define REG_CHARACTER ES_CodeGenerator::REG_AX

#ifdef ARCHITECTURE_AMD64_UNIX
# ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
#  define REG_LENGTH    ES_CodeGenerator::REG_BX
# else // RE_USE_PCMPxSTRx_INSTRUCTIONS
#  define REG_LENGTH    ES_CodeGenerator::REG_CX
# endif // RE_USE_PCMPxSTRx_INSTRUCTIONS

# define REG_RESULT     ES_CodeGenerator::REG_DI
# define REG_STRING     ES_CodeGenerator::REG_SI
# define REG_INDEX      ES_CodeGenerator::REG_DX

# define REG_SCRATCH1   ES_CodeGenerator::REG_R8
# define REG_SCRATCH2   ES_CodeGenerator::REG_R9

# define REG_ARG_RESULT ES_CodeGenerator::REG_DI
# define REG_ARG_SOURCE ES_CodeGenerator::REG_SI
# define REG_ARG_INDEX  ES_CodeGenerator::REG_DX
# define REG_ARG_LENGTH ES_CodeGenerator::REG_CX
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
# ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
#  define REG_LENGTH    ES_CodeGenerator::REG_BX
# else // RE_USE_PCMPxSTRx_INSTRUCTIONS
#  define REG_LENGTH    ES_CodeGenerator::REG_DI
# endif // RE_USE_PCMPxSTRx_INSTRUCTIONS

# define REG_RESULT     ES_CodeGenerator::REG_CX
# define REG_STRING     ES_CodeGenerator::REG_DX
# define REG_INDEX      ES_CodeGenerator::REG_SI

# define REG_SCRATCH1   ES_CodeGenerator::REG_R8
# define REG_SCRATCH2   ES_CodeGenerator::REG_R9

# define REG_ARG_RESULT ES_CodeGenerator::REG_CX
# define REG_ARG_SOURCE ES_CodeGenerator::REG_DX
# define REG_ARG_INDEX  ES_CodeGenerator::REG_R8
# define REG_ARG_LENGTH ES_CodeGenerator::REG_R9
#else
# ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
#  define REG_LENGTH    ES_CodeGenerator::REG_BX
# else // RE_USE_PCMPxSTRx_INSTRUCTIONS
#  define REG_LENGTH    ES_CodeGenerator::REG_CX
# endif // RE_USE_PCMPxSTRx_INSTRUCTIONS

# define REG_RESULT     ES_CodeGenerator::REG_DI
# define REG_STRING     ES_CodeGenerator::REG_SI
# define REG_INDEX      ES_CodeGenerator::REG_DX

# define REG_SCRATCH1   ES_CodeGenerator::REG_DI
# ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
#   define REG_SCRATCH2 ES_CodeGenerator::REG_CX
# else // RE_USE_PCMPxSTRx_INSTRUCTIONS
#   define REG_SCRATCH2 ES_CodeGenerator::REG_BX
# endif // RE_USE_PCMPxSTRx_INSTRUCTIONS
#endif // ARCHITECTURE_AMD64_UNIX

class RE_ArchitectureMixin
{
public:
#ifdef RE_USE_PCMPxSTRx_INSTRUCTIONS
  RE_ArchitectureMixin ()
    : upcase_blob (0),
      first_loaded_string (0)
  {
  }

  ES_CodeGenerator::Constant *upcase_blob;

  class LoadedString
  {
  public:
    const uni_char *string;
    unsigned string_length;
    ES_CodeGenerator::XMM xmm;
    ES_CodeGenerator::Constant *blob;

    LoadedString *next;
  };

  LoadedString *first_loaded_string;
  LoadedString *FindLoadedString (const uni_char *string);
#endif // RE_USE_PCMPxSTRx_INSTRUCTIONS
};

#endif // ARCHITECTURE_IA32
#endif // RE_NATIVE_IA32_H
