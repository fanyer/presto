/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2010
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */
#ifndef ES_PARSER_H
#define ES_PARSER_H


#include "modules/ecmascript/carakan/src/compiler/es_lexer.h"
#include "modules/ecmascript/carakan/src/compiler/es_source_location.h"


#undef NO_ERROR


#define ES_MAXIMUM_FUNCTION_NESTING 256
#define ES_MAXIMUM_SYNTAX_TREE_DEPTH 1024


class ES_Program;
class ES_FunctionCode;
class ES_Environment;
class ES_Arena;
class ES_Runtime;
class ES_SourceRecord;
class ES_Statement;
class ES_Expression;
class ES_ParseErrorInfo;

class ES_Parser
{
public:
    ES_Parser(ES_Lexer *lexer, ES_Runtime *runtime, ES_Context *context, ES_Global_Object *global_object, bool is_eval = false);
    /**< When used to compile program. */

    ES_Parser(ES_Execution_Context *context, ES_Global_Object *global_object, BOOL is_eval);
    /**< When used from Function constructor or eval. */

    ~ES_Parser();

    void SetGlobalScope(BOOL value) { is_global_scope = !!value; }
    void SetGenerateResult(BOOL value) { generate_result = !!value; }
    void SetAllowReturnInProgram(BOOL value) { allow_return_in_program = !!value; }

    void SetURL(JString *new_url) { url = new_url; }

#ifdef ECMASCRIPT_DEBUGGER
    void SetIsDebugging(BOOL value) { is_debugging = value; }
    BOOL GetIsDebugging() { return is_debugging; }
    void SetScriptType(ES_ScriptType value) { script_type = value; }
    ES_ScriptType GetScriptType() const { return script_type; }
#else // ECMASCRIPT_DEBUGGER
    BOOL GetIsDebugging() { return FALSE; }
#endif // ECMASCRIPT_DEBUGGER

    unsigned GetScriptGuid() { return script_guid; }

    void SetClosures(ES_FunctionCode **new_closures, unsigned new_closures_count) { closures = new_closures; closures_count = new_closures_count; }
    //void SetScope(ES_Object **new_scope, unsigned new_scopes_count) { scopes = new_scopes; scopes_count = new_scopes_count; }

    void SetRegisterFrameOffset(unsigned offset) { register_frame_offset = offset; }
    /**< Offset to first register available to allocate as temporary register. */

    void SetInnerScope(unsigned *new_inner_scopes, unsigned new_inner_scopes_count) { inner_scopes = new_inner_scopes; inner_scopes_count = new_inner_scopes_count; }

    void SetIsSimpleEval() { is_simple_eval = TRUE; }

    enum ErrorCode
    {
        NO_ERROR,
        CUSTOM_ERROR,
        GENERIC_ERROR,

        EXPECTED_LVALUE,
        EXPECTED_SEMICOLON,
        EXPECTED_RIGHT_BRACE,
        EXPECTED_RIGHT_BRACKET,
        EXPECTED_RIGHT_PAREN,
        EXPECTED_IDENTIFIER,
        EXPECTED_EXPRESSION,
        UNEXPECTED_TOKEN,
        INVALID_RETURN,
        ILLEGAL_BREAK,
        UNBOUND_BREAK,
        ILLEGAL_CONTINUE,
        UNBOUND_CONTINUE,
        TRY_WITHOUT_CATCH_OR_FINALLY,
        DUPLICATE_LABEL,
        BOTH_GETTER_AND_PROPERTY,
        BOTH_SETTER_AND_PROPERTY,
        DUPLICATE_DATA_PROPERTY,
        DUPLICATE_GETTER,
        DUPLICATE_SETTER,
        DUPLICATE_DEFAULT,
        INPUT_TOO_COMPLEX,
        WITH_IN_STRICT_MODE,
        INVALID_USE_OF_EVAL,
        INVALID_USE_OF_ARGUMENTS,
        INVALID_USE_OF_DELETE,
        DUPLICATE_FORMAL_PARAMETER,
        FUNCTION_DECL_IN_STATEMENT,

        ERROR_CODES_COUNT
    };

    void GetError(ES_ParseErrorInfo &info);
    void SetError(ErrorCode code, const ES_SourceLocation &loc);
    BOOL HasError() { return error_code != NO_ERROR; }

    /** Generates an excerpt from the source code around the error location. */
    void GenerateErrorSourceViewL(OpString &buffer);

    ES_SourcePosition GetErrorPosition(unsigned index, unsigned line);
    /**< Get the ES_SourcePosition for an error at the given @c index and
         @c line.

         This function will calculate the column of the error based on the
         @c index parameter.

         @param index The character offset of the error (relative to start of
                      script source).
         @param line The line where the error occurred (relative to start of
                     script source).
         @return A new ES_SourcePosition for the error. */

    BOOL ParseProgram(ES_ProgramCode *&program_code, bool allow_top_level_function_expr = false);
    BOOL ParseProgram(ES_Code *&program_code, JString *program_string, ES_Value_Internal *value = NULL, bool is_strict_mode = false);

    BOOL ParseFunction(ES_FunctionCode *&code, const uni_char *formals, unsigned formals_length, const uni_char *body, unsigned body_length, ES_ParseErrorInfo *error_info = NULL);
    BOOL ParseFunction(ES_FunctionCode *&code, const uni_char** formals, unsigned nformals, unsigned *formals_length, const uni_char *body, unsigned body_length, ES_ParseErrorInfo *error_info = NULL);

#ifdef ES_BYTECODE_DEBUGGER
    static BOOL ParseDebugCode(ES_Context *context, ES_FunctionCode *&code, ES_Object *global_object, const uni_char *source, unsigned source_length);
#endif // ES_BYTECODE_DEBUGGER

    bool ParseFunction();

    static void Test(ES_Object *global_object, const uni_char *source);

    class FunctionData
    {
    public:
        JString *name, *debug_name;
        ES_Expression *debug_name_expr;

        JString **parameters;
        FunctionData *functions;
        ES_Statement **statements;

        unsigned parameters_count;
        unsigned functions_count;
        unsigned statements_count;

        unsigned start_index;
        unsigned start_line;
        unsigned start_column;
        unsigned end_index;

        ES_SourceLocation start_location;
        ES_SourceLocation end_location;

        bool is_function_expr;
        bool uses_arguments;
        bool uses_eval;
        bool inside_inner_scope;
        bool is_strict_mode;
    };

    OpMemGroup *Arena() { return current_arena; }

    ES_Context *GetContext() { return context; }

#ifdef ECMASCRIPT_DEBUGGER
    void DebuggerSignalNewScript(ES_Context *context = NULL);
    void DebuggerSignalNewScript(const uni_char *source, ES_Context *context = NULL);
    void DebuggerSignalParseError(ES_ErrorInfo *err);
#endif // ECMASCRIPT_DEBUGGER

    OP_STATUS RegisterRegExpLiteral(ES_RegExp_Information *regexp);

private:
    OpMemGroup main_arena, *current_arena;

    enum Context
    {
        EXPRESSION_STMT,
        FUNCTION_EXPR
    };

    void Initialize(BOOL parsing_program = FALSE);

    bool NextToken();
    inline bool HandleLinebreak();
    void SetIsStrictMode(bool value) { is_strict_mode = value; is_strict_eval = in_function == 0 && value && is_function_eval; lexer->SetIsStrictMode(value); }
    void RestoreStrictMode(bool value) { is_strict_mode = value; lexer->SetIsStrictMode(value); }

    enum ParseTokenResult { NOT_FOUND = 0, FOUND = 1, INVALID_TOKEN = -1 };

    inline bool ParseIdentifier(JString *&i, bool opt = false, bool allow_reserved = false);
    bool ParseProperty (JString *&i, bool opt = false);
    inline bool ParsePunctuator(ES_Token::Punctuator p, bool opt = false);
    inline bool ParsePunctuator(ES_Token::Punctuator p, ES_SourceLocation &location);
    inline ParseTokenResult ParsePunctuator1(ES_Token::Punctuator p);
    inline bool ParseKeyword(ES_Token::Keyword k, bool opt = false);
    bool ParseLiteral(ES_Value_Internal &v, bool &regexp, bool opt = false);
    bool ParseRegexpLiteral(ES_Value_Internal &v, bool opt = false);
    bool ParseSemicolon(bool opt = false);

    friend class ES_Compiler;

    bool ParseSourceElements(bool is_body, ES_SourceLocation *lbrace_location = NULL, bool allow_top_level_function_expr = false);
    bool ParseLiteral(ES_Value_Internal *value);
    bool CompileFunction(JString *function_name, unsigned parameter_names_count, unsigned functions_count, unsigned statements_count, unsigned start_index, unsigned start_line, unsigned start_column, unsigned end_index, ES_SourceLocation start_location, ES_SourceLocation end_location, const FunctionData *data, bool is_function_expr, ES_FunctionCode **scope_chain = NULL, unsigned scope_chain_length = 0, BOOL has_outer_scope_chain = FALSE, bool implicit_return = FALSE);
    bool CompileFunction(const FunctionData &data, ES_FunctionCode **scope_chain = NULL, unsigned scope_chain_length = 0, BOOL has_outer_scope_chain = FALSE);
    bool CompileFunctions(unsigned functions_count);
    bool ParseFunctionDecl(bool skip_function_kw = false, bool allow_top_level_function_expr = false);
    bool ParseFunctionExpr(bool push_expression = true);
    bool ParseFormalParameterList();
    bool ParseArguments(unsigned depth);
    bool ParseVariableDeclList(unsigned depth, bool allow_in);

    bool EvaluateConstantBinaryExpression(unsigned type, ES_Expression *left, ES_Expression *right);

    bool ParseLogicalExpr(unsigned &depth, unsigned rhs_production, bool allow_in, unsigned type);
    bool ParseBitwiseExpr(unsigned &depth, unsigned rhs_production, bool allow_in, unsigned type);

    bool ParseStatement(unsigned depth = 0);
    bool ParseExpression(unsigned depth, unsigned production, bool allow_in, unsigned expression_stack_base, bool opt = false);

    enum AccessorType { ACCESSOR_GET, ACCESSOR_SET };
    bool ParseAccessor(AccessorType type, JString *name, unsigned check_count, unsigned index, unsigned line, unsigned column);

    bool SetAllowLinebreak(bool new_value);
    bool GetAllowLinebreak();

    bool ValidateIdentifier(JString *identifier, const ES_SourceLocation *location = NULL);
    /**< "Validate" identifier.  Returns false (and calls SetError()) if we're
         in strict mode and the identifier is 'eval' or 'arguments'.  Returns
         true otherwise.  To be used when parsing constructs where strict mode
         disallows 'eval' and 'arguments'. */

    ES_Runtime *runtime;
    ES_Context *context;
    ES_CollectorLock gclock;
    ES_Global_Object *global_object;

    ES_Identifier_List *strings_table;
    ES_Identifier_Hash_Table *long_string_literal_table;

    JString *ident_arguments, *ident_eval, *current_debug_name, *url;
    ES_Expression *current_debug_name_expr;
    ES_ScriptType script_type;

    bool is_global_scope, generate_result, allow_return_in_program, allow_in_expr, allow_linebreak, linebreak_seen, uses_arguments, uses_eval, is_eval, is_simple_eval, is_strict_eval, is_strict_mode, is_function_eval;
    unsigned in_function, in_with, register_frame_offset;

    ES_FunctionCode **closures;
    unsigned closures_count;
    //ES_Object **scopes;
    //unsigned scopes_count;

    unsigned *inner_scopes, inner_scopes_count;

    void PushExpression(ES_Expression *expression, const ES_SourceLocation &location);
    void PushExpression(ES_Expression *expression);

    ES_Expression *PopExpression();
    ES_Expression **PopExpressions(unsigned count = UINT_MAX);

    unsigned expression_stack_used, expression_stack_total;
    ES_Expression **expression_stack;

    void PushStatement(ES_Statement *statement);
    ES_Statement *PopStatement();
    ES_Statement **PopStatements(unsigned count = UINT_MAX);

    unsigned statement_stack_used, statement_stack_total;
    ES_Statement **statement_stack;

    void PushFunction(ES_FunctionCode *function_code);
    ES_FunctionCode *PopFunction();
    ES_FunctionCode **PopFunctions(unsigned count = UINT_MAX);

    unsigned function_stack_used, function_stack_total;
    ES_FunctionCode **function_stack;

    void PushFunctionData(const FunctionData &data);
    FunctionData *PopFunctionData(unsigned count = UINT_MAX);

    unsigned function_data_stack_used, function_data_stack_total, function_data_index, function_nesting_depth;
    FunctionData *function_data_stack;

    void PushIdentifier(JString *identifier);
    JString *PopIdentifier();
    JString **PopIdentifiers(unsigned count, BOOL use_arena = TRUE);

    unsigned identifier_stack_used, identifier_stack_total;
    JString **identifier_stack;

    BOOL PushProperty(unsigned check_count,
                      JString *name,
#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
                      ES_SourceLocation name_loc,
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
                      ES_Expression *expression,
                      BOOL getter = FALSE,
                      BOOL setter = FALSE);
    void *PopProperties(unsigned count);

    unsigned property_stack_used, property_stack_total;
    void *property_stack;

    ES_Lexer *lexer;
    ES_Token token, last_token;

    const uni_char *program_text;
    unsigned program_text_length;

    ES_Fragments program_fragments;
    ES_Lexer program_lexer;

    unsigned in_typeof;

    ES_StaticSourceData *source_string_owner;
    JStringStorage *source_string;

    ErrorCode error_code, automatic_error_code;
    JString *error_message, *error_identifier;

    unsigned error_index, error_line, global_index_offset;

#ifdef ES_NATIVE_SUPPORT
#ifdef USE_CUSTOM_DOUBLE_OPS
    double (*AddDoubles)(double lhs, double rhs);
    double (*SubtractDoubles)(double lhs, double rhs);
    double (*MultiplyDoubles)(double lhs, double rhs);
    double (*DivideDoubles)(double lhs, double rhs);
#endif // USE_CUSTOM_DOUBLE_OPS
#endif // ES_NATIVE_SUPPORT

#ifdef ECMASCRIPT_DEBUGGER
    BOOL is_debugging;
#endif // ECMASCRIPT_DEBUGGER

    unsigned script_guid;

    OpVector<ES_RegExp_Information> regexp_literals;

    void ReleaseRegExpLiterals();
    ES_ProgramCodeStaticReaper *program_reaper;

    /** Check if stack usage is close to maximum allowed, as controlled by
        ES_CARAKAN_PARM_MAX_PARSER_STACK. The test uses the
        value of ES_MINIMUM_STACK_REMAINING as the size of the 'red
        zone'. A return value of TRUE means that stack usage is close to
        maximum allowed and further nested calls should not be attempted. */
    inline bool IsNearStackLimit();

    /** (Approximate) value of stack pointer when the parser was invoked. */
    unsigned char *stack_base;

};

/** Combining the parse error message along with location. */
class ES_ParseErrorInfo
{
public:
    ES_ParseErrorInfo()
        : message(NULL)
        , index(0)
        , line(0)
    {
    }

    JString *message;
    unsigned index;
    unsigned line;
};

#endif /* ES_PARSER_H */
