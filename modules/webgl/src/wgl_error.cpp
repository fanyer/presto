/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2010
 *
 * WebGL GLSL compiler -- error handling
 *
 */
#include "core/pch.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_error.h"
#include "modules/webgl/src/wgl_context.h"
#include "modules/webgl/src/wgl_printer.h"

/* static */ BOOL
WGL_Error::HaveErrors(WGL_Error *err)
{
    while (err)
    {
        if (err->severity == Error)
            return TRUE;
        err = err->Suc();
    }
    return FALSE;
}

/* static */ void
WGL_Error::ReportErrorsL(WGL_Printer *printer, WGL_Error *err)
{
    while (err)
    {
        err->ReportErrorL(printer);
        printer->Flush(TRUE);
        err = err->Suc();
    }
}

void
WGL_Error::SetMessageL(const uni_char *m)
{
    if (m)
    {
        message = OP_NEWGROA_L(uni_char, uni_strlen(m) + 1, context->Arena());
        uni_strcpy(const_cast<uni_char *>(message), m);
    }
    else
        message = NULL;
}

void
WGL_Error::SetMessageL(const uni_char *m, unsigned max_len, BOOL length_is_lines, const uni_char *end)
{
    if (m)
    {
        const uni_char *ptr = m;
        /* For now, restrict to ASCII. */
        while (*ptr && *ptr < 0x7f && (!end || ptr <= end) && max_len > 0)
        {
            if (!length_is_lines)
                max_len--;
            else if (*ptr == '\r' || *ptr == '\n')
            {
                max_len--;
                if (*ptr == '\r' && max_len && ptr[1] == '\n')
                    ptr++;
            }

            ptr++;
        }

        unsigned message_length = static_cast<unsigned>(ptr - m);
        uni_char *msg = OP_NEWGROA_L(uni_char, message_length + (length_is_lines ? 2 : 1), context->Arena());
        if (length_is_lines)
        {
            msg[0] = '\n';
            uni_strncpy(msg + 1 , m, message_length - 1);
            msg[message_length] = 0;
        }
        else
        {
            uni_strncpy(msg, m, message_length);
            msg[message_length] = 0;
        }

        message = msg;
    }
    else
        message = NULL;
}

void
WGL_Error::SetMessageL(const char *m)
{
    if (m)
    {
        uni_char *msg = OP_NEWGROA_L(uni_char, op_strlen(m) + 1, context->Arena());
        unsigned i = 0;
        while (*m)
            msg[i++] = *m++;

        msg[i] = 0;
        message = const_cast<const uni_char *>(msg);
    }
    else
        message = NULL;
}

void
WGL_Error::ReportErrorL(WGL_Printer *printer)
{
    static char kind_tags[] = "PLSLVX";

    if (location)
    {
        printer->OutString("\"");
        printer->OutString(location);
        printer->OutString("\":");
    }
    if (line_number > 0)
    {
        printer->OutInt(line_number);
        printer->OutString(": ");
    }

    Kind k = Other;
    unsigned id = 0;
    WGL_Error::GetErrorDetails(type, k, id);

    char short_msg[8]; // ARRAY OK 2011-01-13 sof

    OP_ASSERT(static_cast<unsigned>(k) < ARRAY_SIZE(kind_tags));
    short_msg[0] = kind_tags[k];
    if (id < 10)
    {
        short_msg[1] = short_msg[2] = short_msg[3] = '0';
        short_msg[4] = '0' + id;
        short_msg[5] = ':';
        short_msg[6] = ' ';
        short_msg[7] = 0;
    }
    else
    {
        OP_ASSERT(id < 100);
        short_msg[1] = short_msg[2] = '0';
        short_msg[3] = '0' + id / 10;
        short_msg[4] = '0' + id % 10;
        short_msg[5] = ':';
        short_msg[6] = ' ';
        short_msg[7] = 0;
    }
    printer->OutString(short_msg);

    BOOL has_message = TRUE;
    BOOL add_newline = FALSE;
    switch (type)
    {
    case CPP_SYNTAX_ERROR:
        printer->OutString("CPP syntax error");
        break;
    case CPP_ERROR_ERROR:
        printer->OutString("Error #error");
        break;
    case CPP_UNKNOWN_ID:
        printer->OutString("Unknown CPP identifier");
        break;
    case CPP_DUPLICATE_DEFINE:
        printer->OutString("Redefinition of macro");
        break;
    case CPP_MACRO_MISMATCH:
        printer->OutString("CPP macro applied to wrong number of arguments");
        break;
    case CPP_EXTENSION_NOT_SUPPORTED:
        printer->OutString("Unsupported extension");
        break;
    case CPP_EXTENSION_WARN_NOT_SUPPORTED:
        printer->OutString("Warning, extension not supported");
        break;
    case CPP_EXTENSION_DIRECTIVE_ERROR:
        printer->OutString("Extension directive not recognised");
        break;
#if 0
    case CPP_HIGH_PRECISION_NOT_SUPPORTED:
        printer->OutString("High Precision not supported");
        break;
#endif
    case CPP_VERSION_NOT_FIRST:
        printer->OutString("The #version directive must the first directive/statement");
        break;
    case CPP_VERSION_ILLEGAL:
        printer->OutString("Illformed '#version N' pragma");
        break;
    case CPP_VERSION_UNSUPPORTED:
        printer->OutString("Version not supported");
        break;

    case SYNTAX_ERROR:
        printer->OutString("Syntax error");
        break;
    case UNDEFINED_IDENTIFIER:
        printer->OutString("Unknown name/identifier");
        break;
    case RESERVED_WORD:
        printer->OutString("Illegal use of reserved word");
        break;
    case EXPECTED_STMT:
        printer->OutString("Expected a statement");
        break;
    case EXPECTED_EXPR:
        printer->OutString("Expected an expression");
        break;
    case EXPECTED_ID:
        printer->OutString("Expected an identifier");
        break;
    case EXPECTED_STRUCT:
        printer->OutString("Expected a struct");
        break;
    case EXPECTED_COMMA:
        printer->OutString("Expected a comma");
        break;
    case EXPECTED_INPUT:
        printer->OutString("Expected ");
        printer->OutString(message);
        has_message = FALSE;
        break;
    case PARSE_ERROR:
        printer->OutString("Parse error");
        add_newline = TRUE;
        break;
    case INVALID_STMT:
        printer->OutString("Invalid statement");
        break;

    case RESERVED_OPERATOR:
        printer->OutString("Illegal use of reserved operator");
        break;

    case ILLEGAL_TYPE_QUAL:
        printer->OutString("Illegal type qualifier");
        break;
    case ILLEGAL_TYPE_LAYOUT:
        printer->OutString("Illegal layout qualifier");
        break;
    case UNKNOWN_TYPE:
        printer->OutString("Unknown type");
        break;
    case UNKNOWN_VAR:
        printer->OutString("Unknown variable");
        break;
    case INVALID_CALL_EXPR:
        printer->OutString("Illegal function application");
        break;
    case INVALID_FUNCTION_DEFN:
        printer->OutString("Ill-formed function definition");
        break;

    case TYPE_MISMATCH:
        printer->OutString("Type mismatch");
        break;
    case MISMATCHED_FUNCTION_APPLICATION:
        printer->OutString("Function application cannot be resolved");
        break;
    case ARRAY_PARAM_MUST_BE_INT:
        printer->OutString("Array parameter must be an integer");
        break;
    case INVALID_IF_PRED:
        printer->OutString("Illegal predicate type in if-statement, expected a 'bool'");
        break;
    case INVALID_PREDICATE_TYPE:
        printer->OutString("Predicate not of type 'bool'");
        break;
    case UNSUPPORTED_ARGUMENT_COMBINATION:
        printer->OutString("Argument type mismatch");
        break;
    case INVALID_COND_EXPR:
        printer->OutString("Illegal predicate type in conditional expression, expected a 'bool'");
        break;
    case INVALID_COND_MISMATCH:
        printer->OutString("Mismatched types in two arms of a conditional expression");
        break;
    case WRONG_ARGUMENTS_CONSTRUCTOR:
        printer->OutString("Wrong arguments for constructor");
        break;
    case ARGUMENTS_UNUSED_CONSTRUCTOR:
        printer->OutString("Arguments unused in constructor");
        break;
    case TOO_FEW_ARGUMENTS_CONSTRUCTOR:
        printer->OutString("Too few arguments for constructor");
        break;
    case ILLEGAL_STRUCT:
        printer->OutString("Illegal struct declaration");
        break;
    case INVALID_TYPE_CONSTRUCTOR:
        printer->OutString("Illegal type constructor");
        break;
    case WRONG_ARGUMENT_ORDER_CONSTRUCTOR:
        printer->OutString("Arguments in wrong order for structure constructor");
        break;
    case INVALID_CONSTRUCTOR_ARG:
        printer->OutString("Illegal type constructor argument");
        break;
    case EXPRESSION_NOT_CONSTANT:
        printer->OutString("Expression must be a constant expression");
        break;
    case INITIALISER_CONST_NOT_CONST:
        printer->OutString("Initializer for constant variable must be a constant expression");
        break;
    case EXPRESSION_NOT_INTEGRAL_CONST:
        printer->OutString("Expression must be an integral constant expression");
        break;
    case ARRAY_SIZE_IS_ZERO:
        printer->OutString("Array size must be greater than zero");
        break;
    case INDEX_OUT_OF_BOUNDS_ARRAY:
        printer->OutString("Array index out of bounds");
        break;
    case INDEX_OUT_OF_BOUNDS_VECTOR:
        printer->OutString("Vector index out of bounds");
        break;
    case INDEX_ARRAY_NEGATIVE:
        printer->OutString("Array index is negative");
        break;
    case DUPLICATE_VAR_NAME:
        printer->OutString("Redefinition of variable in same scope");
        break;
    case DUPLICATE_FUN_NAME:
        printer->OutString("Redefinition of function in same scope");
        break;
    case DUPLICATE_NAME:
        printer->OutString("Redefinition of name in same scope");
        break;
    case INCOMPATIBLE_FIELD_SELECTOR:
        printer->OutString("Incompatible field selector use");
        break;
    case ILLEGAL_FIELD_SELECTOR:
        printer->OutString("Illegal field selector");
        break;
    case INVALID_LVALUE:
        printer->OutString("Target of assignment is not an l-value");
        break;
    case ILLEGAL_PRECISION_DECL_TYPE:
        printer->OutString("Precision used with type other than int, float or sampler");
        break;
    case ILLEGAL_MAIN_SIGNATURE:
        printer->OutString("Wrong signature for 'main'");
        break;
    case MISSING_CONST_INITIALISER:
        printer->OutString("const variable does not have an initializer");
        break;
    case MISSING_PRECISION_SCALAR:
        printer->OutString("Use of scalar without a precision qualifier nor a precision declaration in scope.");
        break;
    case MISSING_PRECISION_EXPRESSION:
        printer->OutString("Expression does not have an intrinsic precision nor a precision declaration in scope.");
        break;
    case MISSING_PRECISION_DECL:
        printer->OutString("Precision for declaration not specified nor defaulted.");
        break;
    case ILLEGAL_INVARIANT_DECL:
        printer->OutString("Variable cannot be declared invariant");
        break;
    case ILLEGAL_INVARIANT_USE_GLOBAL:
        printer->OutString("'invariant' use not at global scope");
        break;
    case DUPLICATE_LVALUE_COMPONENT:
        printer->OutString("Duplicate components in l-value");
        break;
    case MISSING_RETURN_VALUE:
        printer->OutString("Illegal 'void' return statement in function return a type");
        break;
    case ILLEGAL_RETURN_VALUE_VOID:
        printer->OutString("Value returned from 'void' function");
        break;
    case MISMATCH_RETURN_VALUES:
        printer->OutString("Not all paths return a value");
        break;
    case ILLEGAL_RETURN_TYPE_ARRAY:
        printer->OutString("Illegal to use array as return type");
        break;
    case ILLEGAL_RETURN_TYPE:
        printer->OutString("Illegal return type");
        break;
    case MISMATCH_RETURN_TYPE_DECL_DEFN:
        printer->OutString("Return type of function does not match its declaration");
        break;
    case MISMATCH_PARAM_DECL_DEFN:
        printer->OutString("Parameter qualifier/type does not match its declaration");
        break;

    case INVALID_ATTRIBUTE_DECL:
        printer->OutString("Illegal attribute declaration");
        break;
    case INVALID_LOCAL_ATTRIBUTE_DECL:
        printer->OutString("Illegal local 'attribute' declaration");
        break;
    case INVALID_LOCAL_UNIFORM_DECL:
        printer->OutString("Illegal local 'uniform' declaration");
        break;
    case INVALID_LOCAL_VARYING_DECL:
        printer->OutString("Illegal local 'varying' declaration");
        break;
    case INVALID_VARYING_DECL_TYPE:
        printer->OutString("Illegal type in 'varying' declaration");
        break;
    case INVALID_ATTRIBUTE_DECL_TYPE:
        printer->OutString("Illegal type in 'attribute' declaration");
        break;
    case INITIALIZER_FOR_ATTRIBUTE:
        printer->OutString("Initialisers not supported for 'attribute' declarations");
        break;
    case INITIALIZER_FOR_VARYING:
        printer->OutString("Initialisers not supported for 'varying' declarations");
        break;
    case INITIALIZER_FOR_UNIFORM:
        printer->OutString("Initialisers not supported for 'uniform' declarations");
        break;

    case ILLEGAL_NAME:
        printer->OutString("Illegal name");
        break;
    case ILLEGAL_TYPE_NAME:
        printer->OutString("Illegal type name");
        break;
    case ILLEGAL_LOOP:
        printer->OutString("Illegal loop construct");
        break;
    case ILLEGAL_TYPE_USE:
        printer->OutString("Illegal use of type");
        break;
    case ILLEGAL_TYPE:
        printer->OutString("Illegal type");
        break;
    case ILLEGAL_ARGUMENT_TYPE:
        printer->OutString("Ill-typed operator/function application");
        break;
    case INVALID_LOOP_HEADER:
        printer->OutString("Illegal 'for'-loop initialiser");
        break;
    case INVALID_LOOP_CONDITION:
        printer->OutString("Illegal 'for'-loop condition");
        break;
    case INVALID_LOOP_UPDATE:
        printer->OutString("Illegal 'for'-loop index update");
        break;
    case INVALID_LOOP_NONTERMINATION:
        printer->OutString("Illegal, non-terminating loop");
        break;
    case ILLEGAL_VAR_NOT_WRITEABLE:
        printer->OutString("Cannot assign to read-only/constant value");
        break;
    case ILLEGAL_FUNCTION_OVERLOAD_DUPLICATE:
        printer->OutString("Duplicate function overload");
        break;
    case ILLEGAL_FUNCTION_OVERLOAD_MISMATCH:
        printer->OutString("Conflicting function overload");
        break;
    case ILLEGAL_FUNCTION_OVERLOAD_BUILTIN_OVERLAP:
        printer->OutString("Cannot define function that conflicts with builtin");
        break;
    case ILLEGAL_REFERENCE_ARGUMENT:
        printer->OutString("Expression not permitted as a out/inout reference argument");
        break;
    case OVERLONG_IDENTIFIER:
        printer->OutString("Identifier exceeds maximum allowed length");
        break;
    case ILLEGAL_UNSIZED_ARRAY_DECL:
        printer->OutString("Illegal unsized array declaration");
        break;
    case ILLEGAL_NON_CONSTANT_ARRAY:
        printer->OutString("Array size is not constant and integral");
        break;
    case ILLEGAL_DISCARD_STMT:
        printer->OutString("The 'discard' statement is not permitted in a vertex shader");
        break;
    case ILLEGAL_VARYING_VAR_NOT_WRITEABLE:
        printer->OutString("Cannot assign to a 'varying' variable in a fragment shader");
        break;

    case GLOBAL_VARS_TYPE_MISMATCH:
        printer->OutString("Incompatible types for global variables");
        break;
    case TOO_MANY_ATTRIBUTE_VALUES:
        printer->OutString("Too many 'attribute' declarations used");
        break;
    case TOO_MANY_UNIFORM_VALUES:
        printer->OutString("Too many 'uniform' declarations used");
        break;
    case TOO_MANY_VARYING_VALUES:
        printer->OutString("Too many 'varying' declarations used");
        break;
    case UNDECLARED_FRAGMENT_VARYING:
        printer->OutString("'varying' declared in fragment shader not defined by vertex shader");
        break;
    case INCOMPATIBLE_VARYING_DECL:
        printer->OutString("Illegal 'varying' declaration");
        break;
    case MISSING_MAIN_FUNCTION:
        printer->OutString("Required 'main' function missing");
        break;
    case INCOMPATIBLE_UNIFORM_DECL:
        printer->OutString("Mismatched declaration of 'uniform'");
        break;
    case ILLEGAL_BUILTIN_OVERRIDE:
        printer->OutString("Overriding builtin function not allowed");
        break;
    case INCOMPATIBLE_ATTRIBUTE_DECL:
        printer->OutString("Mismatched declaration of 'attribute'");
        break;

    case EMPTY_STRUCT:
        printer->OutString("Illegal empty structure");
        break;
    case ANON_FIELD:
        printer->OutString("Illegal anonymous field");
        break;
    case UNKNOWN_VECTOR_SELECTOR:
        printer->OutString("Ill-formed vector component selector");
        break;
    case ILLEGAL_VECTOR_SELECTOR:
        printer->OutString("Ill-formed vector component selector");
        break;
    case UNKNOWN_FIELD:
        printer->OutString("Field not a member of struct");
        break;
    case ILLEGAL_PRECISION_DECL:
        printer->OutString("Illegal precision declaration");
        break;
    case INDEX_NON_CONSTANT_FRAGMENT:
        printer->OutString("Non-constant uniform array index used");
        break;
    case MISSING_LOOP_UPDATE:
        printer->OutString("Missing for-loop update expression");
        break;
    case ILLEGAL_RECURSIVE_CALL:
        printer->OutString("Illegal recursive function call");
        break;
    case ILLEGAL_NESTED_FUNCTION:
        printer->OutString("Illegal nested function definition");
        break;
    case ILLEGAL_NESTED_STRUCT:
        printer->OutString("Illegal nested structure declaration");
        break;
    case ILLEGAL_SELECT_EXPR:
        printer->OutString("Illegal l-value in field select expression");
        break;
    case ILLEGAL_INDEX_EXPR:
        printer->OutString("Illegal l-value in indexing expression");
        break;

    default:
        OP_ASSERT(0);
        printer->OutString("Unknown error");
        break;
    }

    if (has_message && message)
    {
        printer->OutString(": ");
        if (add_newline)
            printer->OutNewline();
        printer->OutString(message);
    }
    printer->OutNewline();
}

#define WGL_P_ERROR(tg, n) tg: kind = Preprocessor; value = n; return TRUE
#define WGL_L_ERROR(tg, n) tg: kind = Frontend; value = n; return TRUE
#define WGL_S_ERROR(tg, n) tg: kind = Semantic; value = n; return TRUE
#define WGL_LINK_ERROR(tg, n) tg: kind = Linker; value = n; return TRUE
#define WGL_VALID_ERROR(tg, n) tg: kind = Validation; value = n; return TRUE
#define WGL_OTHER_ERROR(tg, n) tg: kind = Other; value = n; return TRUE

/* static */ BOOL
WGL_Error::GetErrorDetails(Type t, Kind &kind, unsigned &value)
{
    /* Tracks the enum tag ordering in wgl_error.h */
    switch (t)
    {
    case WGL_P_ERROR(CPP_SYNTAX_ERROR, 1);
    case WGL_P_ERROR(CPP_ERROR_ERROR, 2);
    case WGL_P_ERROR(CPP_UNKNOWN_ID, 3);

    case CPP_DUPLICATE_DEFINE:
    case CPP_MACRO_MISMATCH:
        kind = Preprocessor;
        value = 1;
        return TRUE;

    case CPP_EXTENSION_NOT_SUPPORTED:
        kind = Preprocessor;
        value = 3;
        return TRUE;
    case CPP_EXTENSION_DIRECTIVE_ERROR:
        kind = Preprocessor;
        value = 4;
        return TRUE;
    case CPP_EXTENSION_WARN_NOT_SUPPORTED:
        kind = Preprocessor;
        value = 5;
        return TRUE;

#if 0
    case WGL_P_ERROR(CPP_HIGH_PRECISION_NOT_SUPPORTED, 4);
#endif
    case WGL_P_ERROR(CPP_VERSION_NOT_FIRST, 5);
    case WGL_P_ERROR(CPP_LINE_SYNTAX_ERROR, 6);
    case WGL_P_ERROR(CPP_VERSION_ILLEGAL, 7);
    case WGL_P_ERROR(CPP_VERSION_UNSUPPORTED, 7);

    case SYNTAX_ERROR:
    case EXPECTED_STMT:
    case EXPECTED_EXPR:
    case EXPECTED_ID:
    case EXPECTED_STRUCT:
    case EXPECTED_COMMA:
    case EXPECTED_INPUT:
    case PARSE_ERROR:
    case INVALID_STMT:
    case ILLEGAL_TYPE_QUAL:
    case ILLEGAL_TYPE_LAYOUT:
    case UNKNOWN_TYPE:
    case UNKNOWN_VAR:
    case INVALID_CALL_EXPR:
    case INVALID_FUNCTION_DEFN:
        kind = Frontend;
        value = 1;
        return TRUE;

    case WGL_L_ERROR(UNDEFINED_IDENTIFIER, 2);
    case WGL_L_ERROR(RESERVED_WORD, 3);
    case WGL_L_ERROR(RESERVED_OPERATOR, 3);

    case TYPE_MISMATCH:
    case MISMATCHED_FUNCTION_APPLICATION:
        kind = Semantic;
        value = 1;
        return TRUE;
    case WGL_S_ERROR(ARRAY_PARAM_MUST_BE_INT, 2);

    case INVALID_IF_PRED:
    case INVALID_PREDICATE_TYPE:
        kind = Semantic;
        value = 3;
        return TRUE;

    case WGL_S_ERROR(UNSUPPORTED_ARGUMENT_COMBINATION, 4);
    case WGL_S_ERROR(INVALID_COND_EXPR, 5);
    case WGL_S_ERROR(INVALID_COND_MISMATCH, 6);
    case WGL_S_ERROR(WRONG_ARGUMENTS_CONSTRUCTOR, 7);
    case WGL_S_ERROR(ARGUMENTS_UNUSED_CONSTRUCTOR, 8);
    case WGL_S_ERROR(TOO_FEW_ARGUMENTS_CONSTRUCTOR, 9);
    case WGL_S_ERROR(ILLEGAL_STRUCT, 9);
    case WGL_S_ERROR(INVALID_TYPE_CONSTRUCTOR, 10);
    case WGL_S_ERROR(WRONG_ARGUMENT_ORDER_CONSTRUCTOR, 11);
    case WGL_S_ERROR(INVALID_CONSTRUCTOR_ARG, 11);
    case WGL_S_ERROR(EXPRESSION_NOT_CONSTANT, 12);
    case WGL_S_ERROR(INITIALISER_CONST_NOT_CONST, 13);
    case WGL_S_ERROR(EXPRESSION_NOT_INTEGRAL_CONST, 15);
    case WGL_S_ERROR(ARRAY_SIZE_IS_ZERO, 17);
    case WGL_S_ERROR(INDEX_OUT_OF_BOUNDS_ARRAY, 20);
    case WGL_S_ERROR(INDEX_OUT_OF_BOUNDS_VECTOR, 20);
    case WGL_S_ERROR(INDEX_ARRAY_NEGATIVE, 21);
    case WGL_S_ERROR(DUPLICATE_VAR_NAME, 22);
    case WGL_S_ERROR(DUPLICATE_FUN_NAME, 23);
    case WGL_S_ERROR(DUPLICATE_NAME, 24);
    case WGL_S_ERROR(INCOMPATIBLE_FIELD_SELECTOR, 25);
    case WGL_S_ERROR(ILLEGAL_FIELD_SELECTOR, 26);
    case WGL_S_ERROR(INVALID_LVALUE, 27);
    case WGL_S_ERROR(ILLEGAL_PRECISION_DECL_TYPE, 28);
    case WGL_S_ERROR(ILLEGAL_MAIN_SIGNATURE, 29);
    case WGL_S_ERROR(MISSING_CONST_INITIALISER, 31);
    case WGL_S_ERROR(MISSING_PRECISION_SCALAR, 32);
    case WGL_S_ERROR(MISSING_PRECISION_EXPRESSION, 33);
    case WGL_S_ERROR(MISSING_PRECISION_DECL, 33);
    case WGL_S_ERROR(ILLEGAL_INVARIANT_DECL, 34);
    case WGL_S_ERROR(ILLEGAL_INVARIANT_USE_GLOBAL, 35);
    case WGL_S_ERROR(DUPLICATE_LVALUE_COMPONENT, 37);
    case WGL_S_ERROR(MISSING_RETURN_VALUE, 38);
    case WGL_S_ERROR(ILLEGAL_RETURN_VALUE_VOID, 39);
    case WGL_S_ERROR(MISMATCH_RETURN_VALUES, 40);
    case WGL_S_ERROR(ILLEGAL_RETURN_TYPE_ARRAY, 41);
    case WGL_S_ERROR(ILLEGAL_RETURN_TYPE, 41);
    case WGL_S_ERROR(MISMATCH_RETURN_TYPE_DECL_DEFN, 42);
    case WGL_S_ERROR(MISMATCH_PARAM_DECL_DEFN, 43);
    case WGL_S_ERROR(INVALID_ATTRIBUTE_DECL, 44);
    case WGL_S_ERROR(INVALID_LOCAL_ATTRIBUTE_DECL, 45);
    case WGL_S_ERROR(INVALID_LOCAL_UNIFORM_DECL, 46);
    case WGL_S_ERROR(INVALID_LOCAL_VARYING_DECL, 47);
    case WGL_S_ERROR(INVALID_VARYING_DECL_TYPE, 49);
    case WGL_S_ERROR(INVALID_ATTRIBUTE_DECL_TYPE, 49);
    case WGL_S_ERROR(INITIALIZER_FOR_ATTRIBUTE, 50);
    case WGL_S_ERROR(INITIALIZER_FOR_VARYING, 51);
    case WGL_S_ERROR(INITIALIZER_FOR_UNIFORM, 52);

    case WGL_VALID_ERROR(ILLEGAL_NAME, 1);
    case WGL_VALID_ERROR(ILLEGAL_TYPE_NAME, 2);
    case WGL_VALID_ERROR(ILLEGAL_LOOP, 3);
    case WGL_VALID_ERROR(ILLEGAL_TYPE_USE, 4);
    case WGL_VALID_ERROR(ILLEGAL_TYPE, 5);
    case WGL_VALID_ERROR(ILLEGAL_ARGUMENT_TYPE, 6);
    case WGL_VALID_ERROR(INVALID_LOOP_HEADER, 7);
    case WGL_VALID_ERROR(INVALID_LOOP_CONDITION, 8);
    case WGL_VALID_ERROR(INVALID_LOOP_UPDATE, 9);
    case WGL_VALID_ERROR(INVALID_LOOP_NONTERMINATION, 10);
    case WGL_VALID_ERROR(ILLEGAL_VAR_NOT_WRITEABLE, 11);
    case WGL_VALID_ERROR(ILLEGAL_FUNCTION_OVERLOAD_DUPLICATE, 12);
    case WGL_VALID_ERROR(ILLEGAL_FUNCTION_OVERLOAD_MISMATCH, 13);
    case WGL_VALID_ERROR(ILLEGAL_FUNCTION_OVERLOAD_BUILTIN_OVERLAP, 14);
    case WGL_VALID_ERROR(ILLEGAL_REFERENCE_ARGUMENT, 15);
    case WGL_VALID_ERROR(OVERLONG_IDENTIFIER, 16);
    case WGL_VALID_ERROR(ILLEGAL_UNSIZED_ARRAY_DECL, 17);
    case WGL_VALID_ERROR(ILLEGAL_NON_CONSTANT_ARRAY, 18);
    case WGL_VALID_ERROR(ILLEGAL_DISCARD_STMT, 19);
    case WGL_VALID_ERROR(ILLEGAL_VARYING_VAR_NOT_WRITEABLE, 20);

    case WGL_LINK_ERROR(GLOBAL_VARS_TYPE_MISMATCH, 1);
    case WGL_LINK_ERROR(TOO_MANY_ATTRIBUTE_VALUES, 4);
    case WGL_LINK_ERROR(TOO_MANY_UNIFORM_VALUES, 5);
    case WGL_LINK_ERROR(TOO_MANY_VARYING_VALUES, 6);
    case WGL_LINK_ERROR(UNDECLARED_FRAGMENT_VARYING, 7);
    case WGL_LINK_ERROR(INCOMPATIBLE_VARYING_DECL, 8);
    case WGL_LINK_ERROR(MISSING_MAIN_FUNCTION, 9);
    case WGL_LINK_ERROR(INCOMPATIBLE_UNIFORM_DECL, 10);
    case WGL_LINK_ERROR(ILLEGAL_BUILTIN_OVERRIDE, 11);

    case WGL_OTHER_ERROR(EMPTY_STRUCT, 1);
    case WGL_OTHER_ERROR(ANON_FIELD, 2);
    case WGL_OTHER_ERROR(UNKNOWN_VECTOR_SELECTOR, 3);
    case WGL_OTHER_ERROR(ILLEGAL_VECTOR_SELECTOR, 4);
    case WGL_OTHER_ERROR(UNKNOWN_FIELD, 5);
    case WGL_OTHER_ERROR(ILLEGAL_PRECISION_DECL, 6);
    case WGL_OTHER_ERROR(INDEX_NON_CONSTANT_FRAGMENT, 7);
    case WGL_OTHER_ERROR(MISSING_LOOP_UPDATE, 8);
    case WGL_OTHER_ERROR(ILLEGAL_RECURSIVE_CALL, 9);
    case WGL_OTHER_ERROR(ILLEGAL_NESTED_FUNCTION, 10);
    case WGL_OTHER_ERROR(ILLEGAL_NESTED_STRUCT, 11);
    case WGL_OTHER_ERROR(ILLEGAL_SELECT_EXPR, 12);
    case WGL_OTHER_ERROR(ILLEGAL_INDEX_EXPR, 13);

    default:
        OP_ASSERT(!"Missing kind/number mapping for type");
        return FALSE;
    }
}

/* static */ WGL_Error::Severity
WGL_Error::GetSeverity(Type t)
{
    switch (t)
    {
    case CPP_EXTENSION_WARN_NOT_SUPPORTED:
        return Warning;
    default:
        return Error;
    }
}

#endif // CANVAS3D_SUPPORT
