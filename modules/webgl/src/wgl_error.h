/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL compiler -- frontend errors.
 *
 */
#ifndef WGL_ERROR_H
#define WGL_ERROR_H
#include "modules/webgl/src/wgl_base.h"
#include "modules/util/simset.h"

class WGL_Printer;

class WGL_Error
    : public ListElement<WGL_Error>
{
public:
    /** The different kind of errors that the frontend and validator
       might emit. Not individually documented, see ReportErrorL() and
       where reported from to determine meaning. (Numeric labels relate
       them to the list of errors Section 11 of the GLES 2.0 spec.) */
    enum Type
    {
        CPP_SYNTAX_ERROR = 1,
        // P0001
        CPP_ERROR_ERROR,
        // P0002
        CPP_UNKNOWN_ID,
        // P0001
        CPP_DUPLICATE_DEFINE,
        // P0001
        CPP_MACRO_MISMATCH,
        // P0001
        CPP_EXTENSION_NOT_SUPPORTED,
        // P0003
        CPP_EXTENSION_DIRECTIVE_ERROR,
        // P0004
        CPP_EXTENSION_WARN_NOT_SUPPORTED,
        // P0005

#if 0
        /* If any targets should transpire that won't have this
           level of support, enable (and report it during validation.) */
        CPP_HIGH_PRECISION_NOT_SUPPORTED,
        // P0004
#endif
        CPP_VERSION_NOT_FIRST,
        // P0005
        CPP_LINE_SYNTAX_ERROR,
        // P0006
        CPP_VERSION_ILLEGAL,
        // P0007
        CPP_VERSION_UNSUPPORTED,
        // P0007

        SYNTAX_ERROR,
        // L0001
        UNDEFINED_IDENTIFIER,
        // L0002
        RESERVED_WORD,
        // L0003
        EXPECTED_STMT,
        // L0001
        EXPECTED_EXPR,
        // L0001
        EXPECTED_ID,
        // L0001
        EXPECTED_STRUCT,
        // L0001
        EXPECTED_COMMA,
        // L0001
        EXPECTED_INPUT,
        // L0001
        PARSE_ERROR,
        // L0001
        INVALID_STMT,
        // L0001

        RESERVED_OPERATOR,
        // L0003

        ILLEGAL_TYPE_QUAL,
        // L0001
        ILLEGAL_TYPE_LAYOUT,
        // L0001
        UNKNOWN_TYPE,
        // L0001
        UNKNOWN_VAR,
        // L0001
        INVALID_CALL_EXPR,
        // L0001
        INVALID_FUNCTION_DEFN,
        // L0001

        TYPE_MISMATCH,
        // S0001
        MISMATCHED_FUNCTION_APPLICATION,
        // S0001
        ARRAY_PARAM_MUST_BE_INT,
        // S0002
        INVALID_IF_PRED,
        // S0003
        INVALID_PREDICATE_TYPE,
        // S0003
        UNSUPPORTED_ARGUMENT_COMBINATION,
        // S0004
        INVALID_COND_EXPR,
        // S0005
        INVALID_COND_MISMATCH,
        // S0006
        WRONG_ARGUMENTS_CONSTRUCTOR,
        // S0007
        ARGUMENTS_UNUSED_CONSTRUCTOR,
        // S0008
        TOO_FEW_ARGUMENTS_CONSTRUCTOR,
        // S0009
        ILLEGAL_STRUCT,
        // S0009 (not too precise a mapping..)
        INVALID_TYPE_CONSTRUCTOR,
        // S0010 [NOTE: an extension, spec has it unmapped.]
        WRONG_ARGUMENT_ORDER_CONSTRUCTOR,
        // S0011
        INVALID_CONSTRUCTOR_ARG,
        // S0011
        EXPRESSION_NOT_CONSTANT,
        // S0012
        INITIALISER_CONST_NOT_CONST,
        // S0013
        EXPRESSION_NOT_INTEGRAL_CONST,
        // S0015
        ARRAY_SIZE_IS_ZERO,
        // S0017

        INDEX_OUT_OF_BOUNDS_ARRAY,
        // S0020
        INDEX_OUT_OF_BOUNDS_VECTOR,
        // S0020
        INDEX_ARRAY_NEGATIVE,
        // S0021
        DUPLICATE_VAR_NAME,
        // S0022
        DUPLICATE_FUN_NAME,
        // S0023
        DUPLICATE_NAME,
        // S0024
        INCOMPATIBLE_FIELD_SELECTOR,
        // S0025
        ILLEGAL_FIELD_SELECTOR,
        // S0026
        INVALID_LVALUE,
        // S0027
        ILLEGAL_PRECISION_DECL_TYPE,
        // S0028
        ILLEGAL_MAIN_SIGNATURE,
        // S0029
        MISSING_CONST_INITIALISER,
        // S0031
        MISSING_PRECISION_SCALAR,
        // S0032
        MISSING_PRECISION_EXPRESSION,
        // S0033
        MISSING_PRECISION_DECL,
        // S0033
        ILLEGAL_INVARIANT_DECL,
        // S0034
        ILLEGAL_INVARIANT_USE_GLOBAL,
        // S0035
        DUPLICATE_LVALUE_COMPONENT,
        // S0037
        MISSING_RETURN_VALUE,
        // S0038
        ILLEGAL_RETURN_VALUE_VOID,
        // S0039
        MISMATCH_RETURN_VALUES,
        // S0040
        ILLEGAL_RETURN_TYPE_ARRAY,
        // S0041
        ILLEGAL_RETURN_TYPE,
        // S0041
        MISMATCH_RETURN_TYPE_DECL_DEFN,
        // S0042
        MISMATCH_PARAM_DECL_DEFN,
        // S0043
        INVALID_ATTRIBUTE_DECL,
        // S0044
        INVALID_LOCAL_ATTRIBUTE_DECL,
        // S0045
        INVALID_LOCAL_UNIFORM_DECL,
        // S0046
        INVALID_LOCAL_VARYING_DECL,
        // S0047
        INVALID_VARYING_DECL_TYPE,
        // S0049
        INVALID_ATTRIBUTE_DECL_TYPE,
        // S0049
        INITIALIZER_FOR_ATTRIBUTE,
        // S0050
        INITIALIZER_FOR_VARYING,
        // S0051
        INITIALIZER_FOR_UNIFORM,
        // S0052

        ILLEGAL_NAME,
        // V0001
        ILLEGAL_TYPE_NAME,
        // V0001
        ILLEGAL_LOOP,
        // V0003
        ILLEGAL_TYPE_USE,
        // V0004
        ILLEGAL_TYPE,
        // V0005
        ILLEGAL_ARGUMENT_TYPE,
        // V0006
        INVALID_LOOP_HEADER,
        // V0007
        INVALID_LOOP_CONDITION,
        // V0008
        INVALID_LOOP_UPDATE,
        // V0009
        INVALID_LOOP_NONTERMINATION,
        // V0010
        ILLEGAL_VAR_NOT_WRITEABLE,
        // V0011
        ILLEGAL_FUNCTION_OVERLOAD_DUPLICATE,
        // V0012
        ILLEGAL_FUNCTION_OVERLOAD_MISMATCH,
        // V0013
        ILLEGAL_FUNCTION_OVERLOAD_BUILTIN_OVERLAP,
        // V0014
        ILLEGAL_REFERENCE_ARGUMENT,
        // V0015
        OVERLONG_IDENTIFIER,
        // V0016
        ILLEGAL_UNSIZED_ARRAY_DECL,
        // V0017
        ILLEGAL_NON_CONSTANT_ARRAY,
        // V0018
        ILLEGAL_DISCARD_STMT,
        // V0019
        ILLEGAL_VARYING_VAR_NOT_WRITEABLE,
        // V0020

        // Linker errors:
        GLOBAL_VARS_TYPE_MISMATCH,
        // L0001
        TOO_MANY_ATTRIBUTE_VALUES,
        // L0004
        TOO_MANY_UNIFORM_VALUES,
        // L0005
        TOO_MANY_VARYING_VALUES,
        // L0006
        UNDECLARED_FRAGMENT_VARYING,
        // L0007
        INCOMPATIBLE_VARYING_DECL,
        // L0008
        MISSING_MAIN_FUNCTION,
        // L0009

        // NOTE: >L9 are linker error extensions.
        INCOMPATIBLE_UNIFORM_DECL,
        // L0010
        ILLEGAL_BUILTIN_OVERRIDE,
        // L0011
        INCOMPATIBLE_ATTRIBUTE_DECL,
        // L0012

        // Other errors (X - extension):
        EMPTY_STRUCT,
        // X0001
        ANON_FIELD,
        // X0002
        UNKNOWN_VECTOR_SELECTOR,
        // X0003
        ILLEGAL_VECTOR_SELECTOR,
        // X0004
        UNKNOWN_FIELD,
        // X0005
        ILLEGAL_PRECISION_DECL,
        // X0006
        INDEX_NON_CONSTANT_FRAGMENT,
        // X0007
        MISSING_LOOP_UPDATE,
        // X0008
        ILLEGAL_RECURSIVE_CALL,
        // X0009
        ILLEGAL_NESTED_FUNCTION,
        // X0010
        ILLEGAL_NESTED_STRUCT,
        // X0011
        ILLEGAL_SELECT_EXPR,
        // X0012
        ILLEGAL_INDEX_EXPR,
        // X0013

        ERROR_LAST_TAG
    };

    /** Sub-classification of errors. An error can
        can only belong to one category. */
    enum Kind
    {
        Preprocessor = 0,
        Frontend,
        Semantic,
        Linker,
        Validation,
        Other
    };

    /** Sub-classification of errors. An error can
        can only belong to one category. */
    enum Severity
    {
        Error = 0,
        Warning,
        Info
    };


    WGL_Error(WGL_Context *context, Type t, const uni_char *s, unsigned line = 0)
        : context(context)
        , type(t)
        , location(s)
        , line_number(line)
        , message(NULL)
        , severity(GetSeverity(t))
    {
    }

    ~WGL_Error()
    {
    }

    void SetMessageL(const uni_char *m);
    void SetMessageL(const uni_char *m, unsigned max_length, BOOL length_is_lines = FALSE, const uni_char *end = NULL);
    void SetMessageL(const char *m);

    static void ReportErrorsL(WGL_Printer *printer, WGL_Error *error);
    /**< Emit chain of error messages to 'printer'. If the printer
         encounters an error or an OOM condition, it may leave.
         ReportErrorsL() (and ReportErrorL()) will not catch these,
         but propagate. */

    void ReportErrorL(WGL_Printer *printer);
    /**< Emit an individual error to 'printer'. See ReportErrorsL()
         for how internal printer errors are handled. */

    static BOOL HaveErrors(WGL_Error *err);

    WGL_Context *context;

    Type type;
    const uni_char *location;
    unsigned line_number;

    const uni_char *message;
    Severity severity;

private:
    static BOOL GetErrorDetails(Type t, Kind &kind, unsigned &number);
    /**< Map from a specific error to its GLES 2.0 labelling (see Section 11.) */

    static Severity GetSeverity(Type t);
};

#endif // WGL_ERROR_H
