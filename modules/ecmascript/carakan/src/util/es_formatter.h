/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA 2012
 *
 * Ecmascript source formatter.
 */
#ifndef ES_FORMATTER_H
#define ES_FORMATTER_H

#include "modules/ecmascript/carakan/src/compiler/es_lexer.h"
#include "modules/ecmascript/carakan/src/kernel/es_string_appender.h"

class ES_Formatter
{
public:
    ES_Formatter(ES_Lexer *lexer, ES_Context *context, bool is_eval);
    /**< Formatter constructor.

         @param[in] lexer The instance of the lexer. Must be non-NULL when used
                          to format source of regular scripts.
         @param[in] context The context in which original string is used.
         @param[in] is_eval Set when script is an eval source. */

    ~ES_Formatter();

    BOOL FormatProgram(uni_char *&program_source, unsigned &program_source_length, BOOL allow_top_level_function_expr);
    /**< Format regular script source.

         Used to format regular scripts loaded through the script tag. Lexer
         instance must be passed to the class constructor to use this method.

         @param[out] program_source The formatted source of the program on
                                    success.
         @param[out] program_source_length The length, in characters, of the
                                           formatted source.
         @param[in] allow_top_level_function_expr True if top level function
                                                  expressions should be allowed.
         @return True if the program source was formatted successfully. */

    BOOL FormatProgram(JString *&program_source);
    /**< Format eval source.

         Used to format provided fragment source.

         @param[in/out] program_string The source of the program that is to be
                                       formatted. On success, the argument is
                                       set to a new, formatted source.
         @return True if program source was formatted successfully. */

    BOOL FormatFunction(uni_char *&body_out, unsigned &body_length_out, const uni_char *body, unsigned body_length);
    /**< Format event handler source.

         Used to format source of the event handler.

         @param[out] body_out The formatted source of the program on success.
         @param[out] body_lengh_out The length, in characters, of the formatted
                                    source.
         @param[in] body The source of the function that is to be formatted.
         @param[in] body_length The length of the source that is to be formatted.
         @return True if program source was formatted successfully. */

    void SetAllowReturnInProgram(BOOL value) { allow_return_in_program = value; }

    /* Public due to brew - C3032E. */
    enum InContext { IN_STATEMENT, IN_EXPRESSION, IN_FUNC_DECL, IN_FUNC_EXPR, IN_TERNARY_EXPR, IN_ACCESSOR, IN_FOR_DECL_LIST };

    struct State
    {
        unsigned brace_indent_level;  /**< Current indentation level of the
                                           braces. */
        unsigned indent_level;        /**< Current indentation level of the
                                           code. */
        bool in_standalone_block;     /**< True when inside a code block that has
                                           no control statement. Only meaningful
                                           when in IN_STATEMENT context. */
        InContext context;            /**< The type of the context most recently
                                           seen on the stack. */
    };
private:
    void Initialize();

    inline void PushExpression(bool is_lvalue) { ++expression_stack_used; last_expr_is_lvalue = is_lvalue; }
    inline void PopExpression() { --expression_stack_used; }
    inline void PopExpressions(unsigned count) { expression_stack_used -= count; }

    unsigned expression_stack_used;
    bool last_expr_is_lvalue;
    /**< In one case it's necessary to check if the last expression was
         an l-value. As formatter doesn't track all expressions like
         ES_Parser does, we keep track of this specific property only. */

    inline void PushStatement() { ++statement_stack_used; }
    inline void PopStatement() { --statement_stack_used; }
    inline void PopStatements(unsigned count) { statement_stack_used -= count; }

    unsigned statement_stack_used;

    inline void PushIdentifier() { identifier_stack_used++; }
    inline void PopIdentifiers(unsigned count) { identifier_stack_used -= count; }

    unsigned identifier_stack_used;

    bool ParseSourceElements(bool is_body, bool allow_top_level_function_expr = false);
    bool ParseFunctionDecl(bool skip_function_kw = false, bool allow_top_level_function_expr = false);
    bool ParseFunctionExpr();
    bool ParseFormalParameterList();
    bool ParseArguments(unsigned depth);
    bool ParseVariableDeclList(unsigned depth, bool allow_in);

    enum ParseTokenResult { NOT_FOUND = 0, FOUND = 1, INVALID_TOKEN = -1 };

    inline bool ParseIdentifier(JString *&i, bool opt = false, bool allow_reserved = false);
    bool ParseProperty(JString *&i, bool opt = false);
    inline bool ParsePunctuator(ES_Token::Punctuator p, bool opt = false);
    inline ParseTokenResult ParsePunctuator1(ES_Token::Punctuator p);
    inline bool ParseKeyword(ES_Token::Keyword k, bool opt = false);
    bool ParseLiteral(ES_Value_Internal &v, JString *&i, bool &regexp, bool opt = false);
    bool ParseSemicolon(bool opt = false);

    bool ParseStatement(unsigned depth = 0, bool in_indented_scope = false);
    bool ParseExpression(unsigned depth, unsigned production, bool allow_in, unsigned expression_stack_base, bool opt = false);

    enum AccessorType { ACCESSOR_GET, ACCESSOR_SET };
    bool ParseAccessor(AccessorType type, JString *name);

    bool NextToken();
    inline bool HandleLinebreak();

    bool SetAllowLinebreak(bool new_value);
    bool GetAllowLinebreak();

    ES_Context *context;
    ES_CollectorLock gclock;
    JString *out_text;
    ES_JString_Appender *appender;

    ES_Lexer *lexer;
    ES_Token token, last_token;

    ES_Lexer program_lexer;

    unsigned depth;

    ES_Identifier_List *strings_table;
    JString *ident_arguments, *ident_eval;

    BOOL allow_return_in_program;
    bool allow_linebreak, linebreak_seen, is_eval;
    unsigned in_typeof, in_function, in_with;

    class ScopedContext
    {
    public:
        ScopedContext(State &state, InContext new_context)
            : state(state)
        {
            original_context = state.context;
            state.context = new_context;
        }

        ~ScopedContext()
        {
            state.context = original_context;
        }

    private:
        State &state;
        InContext original_context;
    };
    /**< Set and restore context of the given State instance. */

    class ScopedIndent
    {
    public:
        ScopedIndent(State &state)
            : state(state)
        {
            original_indent_level = state.indent_level;
            original_brace_indent_level = state.brace_indent_level;
            state.brace_indent_level = state.indent_level++;
        }

        ~ScopedIndent()
        {
            state.indent_level = original_indent_level;
            state.brace_indent_level = original_brace_indent_level;
        }

    private:
        State &state;
        unsigned original_indent_level;
        unsigned original_brace_indent_level;
    };
    /**< Scope specific indentation level change.

         Increases indentation level in the current scope. Previous indentation
         level is restored when instance goes out of the scope. */

    State state;
    /**< State of the formatter. */
    State fake_state;
    /**< Fake state, to be passed on when real state is not supposed to be modified. */

    bool last_char_is_linebreak;
    /**< The most recently appended content was a line break. */
    bool can_append_linebreak;
    /**< Indicates whether line break is allowed for the next append. */
    bool is_linebreak_scheduled;
    /**< The line break is scheduled for appending. Will be canceled if
         original code already contains line break. */
    bool is_space_scheduled;
    /**< The space is scheduled for appending. Can be canceled by line break
         or some other specific characters. */

    inline void AppendPunctuator(bool is_unary = false);
    /**< Append a punctuator.
         @param is_unary The token is an unary operator and should not be
                         surrounded by spaces. */
    inline void AppendKeyword();
    /**< Append a keyword. */
    inline void AppendLiteral(JString *l);
    /**< Append a string literal.
         @param l The literal string to append. */
    inline void ScheduleLinebreak();
    /**< Schedule line break for appending. */
    inline void ScheduleSpace();
    /**< Schedule a space for appending. */

    /**@{*/
    inline void GetKeywordTokenData(ES_Token::Keyword keyword, const char *&string, bool &space_before, bool &space_after);
    inline void GetPunctuatorTokenData(ES_Token::Punctuator punctuator, const char *&string, bool &space_before, bool &space_after);
    /**< Get formatter data related to the specified token.
         @param[in] The token for which data should be retrieved.
         @param[out] Set to the literal string that represents the token.
         @param[out] Set to true if token is to be preceded by a space.
         @param[out] Set to true if token is to be followed by a space. */
    /**}@*/


    template <typename T> inline void AppendInternal(T string, bool brace = false);
    inline void LinebreakInternal();
    inline void SpaceInternal();
    inline void FlushScheduledInternal();
    inline void IndentInternal(bool is_brace = false);

    inline bool IsNearStackLimit();

    /** (Approximate) value of stack pointer when the parser was invoked. */
    unsigned char *stack_base;
};

#endif // ES_FORMATTER_H
