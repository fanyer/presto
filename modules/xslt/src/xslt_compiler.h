/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_COMPILER_H
#define XSLT_COMPILER_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_instruction.h"

class XSLT_StylesheetImpl;
class XSLT_String;
class XSLT_Program;
class XMLCompleteName;
class XMLCompleteNameN;
class XSLT_XPathPattern;
class XMLNamespaceDeclaration;
class XSLT_XPathExpression;
class XSLT_Element;
class XPathPattern;
class XPathExtensions;

#ifdef XSLT_ERRORS
# define XSLT_ADD_INSTRUCTION(code) (compiler->AddInstructionL (code, 0, this))
# define XSLT_ADD_INSTRUCTION_WITH_ARGUMENT(code, argument) (compiler->AddInstructionL (code, argument, this))
# define XSLT_ADD_INSTRUCTION_EXTERNAL(code, origin) (compiler->AddInstructionL (code, 0, origin))
# define XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL(code, argument, origin) (compiler->AddInstructionL (code, argument, origin))
# define XSLT_ADD_JUMP_INSTRUCTION(code) (compiler->AddJumpInstructionL (code, this))
# define XSLT_ADD_JUMP_INSTRUCTION_EXTERNAL(code, origin) (compiler->AddJumpInstructionL (code, origin))
#else // XSLT_ERRORS
# define XSLT_ADD_INSTRUCTION(code) (compiler->AddInstructionL (code, 0))
# define XSLT_ADD_INSTRUCTION_WITH_ARGUMENT(code, argument) (compiler->AddInstructionL (code, argument))
# define XSLT_ADD_INSTRUCTION_EXTERNAL(code, origin) (compiler->AddInstructionL (code, 0))
# define XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL(code, argument, origin) (compiler->AddInstructionL (code, argument))
# define XSLT_ADD_JUMP_INSTRUCTION(code) (compiler->AddJumpInstructionL (code))
# define XSLT_ADD_JUMP_INSTRUCTION_EXTERNAL(code, origin) (compiler->AddJumpInstructionL (code))
#endif // XSLT_ERRORS

class XSLT_MessageHandler;

class XSLT_Compiler
{
protected:
  XSLT_StylesheetImpl *stylesheet;
  XSLT_MessageHandler *messagehandler;

  XSLT_Instruction *instructions;
  unsigned instructions_used, instructions_total;

  uni_char **strings;
  unsigned strings_used, strings_total;

  XMLCompleteName **names;
  unsigned names_used, names_total;

  XSLT_XPathExpression **expressions;
  unsigned expressions_used, expressions_total;

  XPathPattern **patterns;
  unsigned patterns_used, patterns_total;

  XMLNamespaceDeclaration **nsdeclarations;
  unsigned nsdeclarations_used, nsdeclarations_total;

  XSLT_Program **programs;
  unsigned programs_used, programs_total;

#ifdef XSLT_ERRORS
  void AddInstructionL (XSLT_Instruction::Code code, UINTPTR *&argument_ptr, XSLT_Element *origin);
#else // XSLT_ERRORS
  void AddInstructionL (XSLT_Instruction::Code code, UINTPTR *&argument_ptr);
#endif // XSLT_ERRORS

public:
  XSLT_Compiler (XSLT_StylesheetImpl *stylesheet, XSLT_MessageHandler *messagehandler);
  ~XSLT_Compiler ();

  XSLT_StylesheetImpl *GetStylesheet () { return stylesheet; }
  XSLT_MessageHandler *GetMessageHandler () { return messagehandler; }

  XSLT_Compiler *GetNewCompilerL () { return OP_NEW_L (XSLT_Compiler, (stylesheet, messagehandler)); }

#ifdef XSLT_ERRORS
  void AddInstructionL (XSLT_Instruction::Code code, UINTPTR argument, XSLT_Element *origin);
  unsigned AddJumpInstructionL (XSLT_Instruction::Code code, XSLT_Element *origin);
  /**< Returns the handle to use in SetJumpDestination. */
#else // XSLT_ERRORS
  void AddInstructionL (XSLT_Instruction::Code code, UINTPTR argument);
  unsigned AddJumpInstructionL (XSLT_Instruction::Code code);
  /**< Returns the handle to use in SetJumpDestination. */
#endif // XSLT_ERRORS

  unsigned AddStringL (const uni_char *string);
  unsigned AddNameL (const XMLCompleteNameN &name, BOOL process_namespace_aliases);
  unsigned AddExpressionL (const XSLT_String &source, XPathExtensions* extension, XMLNamespaceDeclaration *nsdeclaration);
  unsigned AddPatternsL (XPathPattern **patterns, unsigned patterns_count);
  unsigned AddNamespaceDeclarationL (XMLNamespaceDeclaration *nsdeclaration);

  unsigned AddProgramL (XSLT_Program *program);
  /**< Adds a pointer to a XSLT_Program to this program for use in a CALL
       instruction. The program is still owned by the caller and must be alive
       until this program has finished. */

  void SetJumpDestination (unsigned jump_instruction_offset);

  void FinishL (XSLT_Program *program);
};

#endif // XSLT_SUPPORT
#endif // XSLT_COMPILER_H
