/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_INSTRUCTION_H
#define XSLT_INSTRUCTION_H

#ifdef XSLT_SUPPORT

#ifdef XSLT_ERRORS
class XSLT_Element;
#endif // XSLT_ERRORS

class XSLT_Instruction
{
public:
  enum Code
    {
      IC_EVALUATE_TO_STRING,
      /**< Evaluate an XPath expression to a string and add the result to the
           string buffer and make the string buffer the current string. */

      IC_EVALUATE_TO_BOOLEAN,
      /**< Evaluate an XPath expression to a boolean. */

      IC_EVALUATE_TO_NUMBER,
      /**< Evaluate an XPath expression to a number. */

      IC_EVALUATE_TO_NODESET_SNAPSHOT,
      /**< Evaluate an XPath expression to a node-set, snapshot style. */

      IC_EVALUATE_TO_NODESET_ITERATOR,
      /**< Evaluate an XPath expression to a node-set, iterator style. */

      IC_EVALUATE_TO_VARIABLE_VALUE,
      /**< Evaluate an XPath expression to something that can be set as a
           variable value. Basically it doesn't force a type. */

      IC_APPEND_STRING,
      /**< Append constant string to the string buffer and make the string
           buffer the current string. */

      IC_SET_STRING,
      /**< Set current string to a constant string. */

      IC_CLEAR_STRING,
      /**< Clears the string buffer and sets the current string to an empty
         string. */

      IC_SET_NAME,
      /**< Set the current name to a constant name. */

      IC_SET_QNAME,
      /**< Set the QName of the current name to the current string. */

      IC_SET_URI,
      /**< Set the URI of the current name to the current string. */

      IC_RESOLVE_NAME,
      /**< Set the URI of the current name by resolving the current name's
           QName. */

      IC_SET_NAME_FROM_NODE,
      /**< Sets the current name to the name of the current node */

      IC_START_ELEMENT,
      /**< Start an element using the current name, or, if the argument is not
           zero, the name the argument points at. */

      IC_SUGGEST_NSDECL,
      /**< Suggest namespace declarations that might be useful to declare
           although not strictly necessary. */

      IC_ADD_ATTRIBUTE,
      /**< Add an attribute using the current name and the current
           string. Clears the current string. */

      IC_ADD_TEXT,
      /**< Add a text node using the current string. Clears the current
           string. */

      IC_ADD_COMMENT,
      /**< Add a comment node using the current string. Clears the current
           string. */

      IC_ADD_PROCESSING_INSTRUCTION,
      /**< Add a processing instruction node using the current name's localpart
           as the target and the current string as the data. */

      IC_END_ELEMENT,
      /**< End the current element. */

      IC_ADD_COPY_OF_EVALUATE,
      /**< Output whatever is in evaluate as in a copy-of instruction. The
           evaluate should have been evaluated to a varaible value to preserve
           trees. */

      IC_ADD_COPY_AND_JUMP_IF_NO_END_ELEMENT,
      /**< Output a copy of the current node and jump if the node doesn't
           require child processing and an end element. Directly after this
           instruction should be an instruction to jump to the code for root
           elements. That instruction will be skipped for elements. */

      IC_MATCH_PATTERNS,
      /**< Match current node against patterns. The argument consists of two
           parts, 16 bits (low bits) pattern index and 16 bits (high bits)
           pattern count */

      IC_SEARCH_PATTERNS,
      /**< Search patterns for matching nodes. The argument consists of two
           parts, 16 bits (low bits) pattern index and 16 bits (high bits)
           pattern count */

      IC_COUNT_PATTERNS_AND_ADD,
      /**< Run a pattern match count. The argument is a pointer to a XSLT_Number
           element which has information about the actual patterns. The result
           will be added to the output. */

      IC_PROCESS_KEY,
      /**< Run the key process operation on a node. */

      IC_CALL_PROGRAM_ON_NODE,
      /**< Call program unconditionally.  Opening a new scope.  Consuming the
           collected with-params.  Argument is a program number returned from
           XSLT_Program::AddProgramL. */

      IC_CALL_PROGRAM_ON_NODE_NO_SCOPE,
      /**< Call program unconditionally.  Not opening a new scope.  Argument is
           a program number returned from XSLT_Program::AddProgramL. */

      IC_APPLY_IMPORTS_ON_NODE,
      /**< Like IC_CALL_PROGRAM_ON_NODE, but suitable for xsl:apply-imports.
           This primarily means that context position and size is copied from
           the current program state to the called.  Argument is a program
           number returned from XSLT_Program::AddProgramL. */

      IC_CALL_PROGRAM_ON_NODES,
      /**< Call program unconditionally.  Opening a new scope.  Argument is a
           program number returned from XSLT_Program::AddProgramL. */

      IC_CALL_PROGRAM_ON_NODES_NO_SCOPE,
      /**< Call program unconditionally.  Not opening a new scope.  Argument is a
           program number returned from XSLT_Program::AddProgramL. */

      IC_APPLY_TEMPLATES,
      /**< Like IC_CALL_PROGRAM_ON_NODES for an apply templates program, except
           the argument is a pointer to the XSLT_ApplyTemplates element. */

      IC_APPLY_BUILTIN_TEMPLATE,
      /**< Apply built-in template rules on the current context node.  Used only
           in apply-templates programs. */

      IC_JUMP,
      /**< Jump unconditionally. */

      IC_JUMP_IF_TRUE,
      /**< Jump if the last pattern matched or the last expression evaluated to
           true. */

      IC_JUMP_IF_FALSE,
      /**< Jump if the last pattern didn't match or the last expression
           evaluated to false. */

      IC_START_COLLECT_TEXT,
      /**< Set an output handler that collects text into the string buffer. */

      IC_END_COLLECT_TEXT,
      /**< Restore the previous output handler and make the string buffer the
           current string. */

      IC_START_COLLECT_RESULTTREEFRAGMENT,
      /**< Set an output handler that collects a result tree fragment. */

      IC_END_COLLECT_RESULTTREEFRAGMENT,
      /**< Restore the previous output handler. */

      IC_SORT,
      /**< Sort current node-set. */

      IC_SET_SORT_PARAMETER_ORDER,
      IC_SET_SORT_PARAMETER_LANG,
      IC_SET_SORT_PARAMETER_DATATYPE,
      IC_SET_SORT_PARAMETER_CASEORDER,
      /**< Used to calculate the parameters for a sort. A sequence of these must
           be followed by a IC_SORT. */

      IC_ADD_FORMATTED_NUMBER,
      /**< Take a XSLT_Number (as argument) and an Evaluate (in the current
           state) and output something. */

      IC_TEST_AND_SET_IF_PARAM_IS_PRESET,
      /**< Set current_boolean to TRUE if there is a with-param that has already
           set the param value, FALSE otherwise. Argument is a pointer to the
           XSLTE_PARAM element (type XSLT_Variable).  If this returns TRUE the
           variable will also be set. */

      IC_SET_VARIABLE_FROM_EVALUATE,
      /**< Set the variable to the contents of the current evaluate. Argument is
           a pointer to a XSLT_Variable. */

      IC_SET_VARIABLE_FROM_STRING,
      /**< Set the variable to the contents of the current string. Argument is a
           pointer to a XSLT_Variable. */

      IC_SET_VARIABLE_FROM_COLLECTED,
      /**< Set the variable from the collected tree (see RESULTTREEFRAGMENT
           operations */

      IC_SET_WITH_PARAM_FROM_EVALUATE,
      /**< Set the variable to the contents of the current evaluate. Argument is
           a pointer to a XSLT_Variable. */

      IC_SET_WITH_PARAM_FROM_STRING,
      /**< Set the variable to the contents of the current string. Argument is a
           pointer to a XSLT_Variable. */

      IC_SET_WITH_PARAM_FROM_COLLECTED,
      /**< Set the variable from the collected tree (see RESULTTREEFRAGMENT
           operations */

      IC_START_COLLECT_PARAMS,
      /**< Start collecting parameter values for a call-template instruction. */

      IC_RESET_PARAMS_COLLECTED_FOR_CALL,
      /**< Reset the collection of parameter values collected within an
           apply-templates or call-template element. */

      IC_SEND_MESSAGE,
      /**< Sends the current string as a message. */

      IC_ERROR,
      /**< Error message as argument. Should cause an error and abort if
           executed. */

      IC_RETURN,
      /**< End program when this is encountered even if there are more
           instructions in it. */

      IC_LAST_INSTRUCTION
    };

  Code code;
  UINTPTR argument;

#ifdef XSLT_ERRORS
  XSLT_Element *origin;
#endif // XSLT_ERRORS
};

#endif // XSLT_SUPPORT
#endif // XSLT_INSTRUCTION_H
