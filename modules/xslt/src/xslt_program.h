/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_PROGRAM_H
#define XSLT_PROGRAM_H

#ifdef XSLT_SUPPORT

class XSLT_Instruction;
class XMLExpandedName;
class XMLCompleteName;
class XSLT_XPathExpression;
class XPathPattern;
class XMLNamespaceDeclaration;
class XSLT_Variable;

class XSLT_Program
{
public:
  enum Type
    {
      TYPE_TEMPLATE,
      TYPE_APPLY_TEMPLATES,
      TYPE_FOR_EACH,
      /**< Content of for-each element; called once per selected node. */
      TYPE_TOP_LEVEL_VARIABLE,
      TYPE_KEY,
      TYPE_OTHER
    };

  XSLT_Program (Type type = TYPE_OTHER);
  ~XSLT_Program ();

  OP_STATUS SetMode (const XMLExpandedName &mode);

#ifdef XSLT_PROGRAM_DUMP_SUPPORT
  void DumpProgram(unsigned marked_instruction_index);
  void DumpInstruction(const XSLT_Instruction& instruction);
  const char* InstructionArgumentsToString(OpString8& buf, unsigned instruction_code, unsigned instruction_argument);
  BOOL program_has_been_dumped;
  OpString8 program_description;
#endif // XSLT_PROGRAM_DUMP_SUPPORT

  Type type;
  XMLExpandedName *mode;

#ifdef XSLT_ERRORS
  XSLT_Variable *variable;
#endif // XSLT_ERRORS

  XSLT_Instruction *instructions;
  unsigned instructions_count;

  uni_char **strings;
  unsigned strings_count;

  XMLCompleteName **names;
  unsigned names_count;

  XSLT_XPathExpression **expressions;
  unsigned expressions_count;

  XPathPattern **patterns;
  unsigned patterns_count;

  XMLNamespaceDeclaration **nsdeclarations;
  unsigned nsdeclarations_count;

  XSLT_Program **programs;
  unsigned programs_count;
};

#endif // XSLT_SUPPORT
#endif // XSLT_PROGRAM_H
