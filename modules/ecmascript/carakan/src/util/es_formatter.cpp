/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA 2012
 *
 * ECMAScript source formatter.
 */
#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_expr.h"
#include "modules/ecmascript/carakan/src/util/es_formatter.h"
#include "modules/ecmascript/carakan/src/util/es_formatter_inlines.h"


ES_Formatter::ES_Formatter(ES_Lexer *lexer, ES_Context *context, bool is_eval)
    : expression_stack_used(0)
    , statement_stack_used(0)
    , identifier_stack_used(0)
    , context(context)
    , gclock(context)
    , out_text(NULL)
    , appender(NULL)
    , lexer(lexer)
    , program_lexer(context)
    , depth(0)
    , strings_table(NULL)
    , ident_arguments(context->rt_data->idents[ESID_arguments])
    , ident_eval(context->rt_data->idents[ESID_eval])
    , allow_return_in_program(FALSE)
    , allow_linebreak(true)
    , linebreak_seen(false)
    , is_eval(is_eval)
    , in_typeof(0)
    , in_function(0)
    , in_with(0)
    , last_char_is_linebreak(true)
    , can_append_linebreak(true)
    , is_linebreak_scheduled(false)
    , is_space_scheduled(false)
{
    state.indent_level = state.brace_indent_level = 0;
}


ES_Formatter::~ES_Formatter()
{
    OP_DELETE(appender);
}


void
ES_Formatter::Initialize()
{
    out_text = JString::Make(context);
    appender = OP_NEW_L(ES_JString_Appender, (context, out_text));

    lexer->SetEmitComments(TRUE);
    lexer->SetStringsTable(strings_table = ES_Identifier_List::Make(context, 256));

    JString **idents = context->rt_data->idents;
    unsigned index;

    /* The following ensures that any occurence of these identifiers (or even
       any occurence of them as string literals!) are shared, and thus
       identifiable by plain pointer comparisons.

       FIXME: Is it a problem that "random" string literals can use strings
              allocated on the shared rt_data heap? */

    strings_table->AppendL(context, ident_arguments, index);
    strings_table->AppendL(context, ident_eval, index);
    strings_table->AppendL(context, idents[ESID_length], index);
    strings_table->AppendL(context, idents[ESID_proto], index);

    stack_base = reinterpret_cast<unsigned char *>(&index);
    OP_ASSERT(ES_MINIMUM_STACK_REMAINING < ES_CARAKAN_PARM_MAX_PARSER_STACK);
}


BOOL
ES_Formatter::FormatProgram(uni_char *&program_text_out, unsigned &program_text_out_length, BOOL allow_top_level_function_expr)
{
    OP_ASSERT(lexer);

    Initialize();

    if (ParseSourceElements(false, !!allow_top_level_function_expr))
    {
        JString *formatted = appender->GetString();
        program_text_out = Storage(context, formatted);
        program_text_out_length = formatted->length;
        return TRUE;
    }

    return FALSE;
}


BOOL
ES_Formatter::FormatProgram(JString *&program_string)
{
    const uni_char *program = Storage(context, program_string);
    unsigned program_length = Length(program_string);

    ES_Fragments program_fragments;
    program_fragments.fragments = &program;
    program_fragments.fragment_lengths = &program_length;
    program_fragments.fragments_count = 1;

    program_lexer.SetSource(&program_fragments, program_string);

    lexer = &program_lexer;

    Initialize();

    if (ParseSourceElements(false))
    {
        program_string = appender->GetString();
        return TRUE;
    }

    return FALSE;
}


BOOL
ES_Formatter::FormatFunction(uni_char *&body_out, unsigned &body_out_length, const uni_char *body, unsigned body_length)
{
    ES_Fragments fbody(&body, &body_length);
    ES_Lexer lexer_body(context, NULL, &fbody);

    lexer = &lexer_body;
    lexer->SetStringsTable(strings_table);

    Initialize();

    ++in_function;

    if (ParseSourceElements(false) && token.type == ES_Token::END)
    {
        --in_function;
        body_out = Storage(context, appender->GetString());
        body_out_length = Length(appender->GetString());
        return TRUE;
    }

    return FALSE;
}


bool
ES_Formatter::ParseSourceElements(bool is_body, bool allow_top_level_function_expr)
{
    if (!(is_body ? ParsePunctuator(ES_Token::LEFT_BRACE) : NextToken()) || !HandleLinebreak())
        return false;

    unsigned depth_before = depth;

    while (token.type != ES_Token::END && !(is_body && token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::RIGHT_BRACE))
    {
        if (token.type == ES_Token::KEYWORD && token.keyword == ES_Token::KEYWORD_FUNCTION)
        {
            if (!ParseFunctionDecl(false, allow_top_level_function_expr))
                return false;
        }
        else if (!ParseStatement())
            return false;

        if (!HandleLinebreak())
            return false;
    }

    depth = depth_before;

    return true;
}


bool
ES_Formatter::ParseFunctionDecl(bool skip_function_kw, bool allow_top_level_function_expr)
{
    ScopedContext scoped_context(state, IN_FUNC_DECL);

    JString *function_name;

    ++in_function;

    if (skip_function_kw || ParseKeyword(ES_Token::KEYWORD_FUNCTION))
    {
        if (ParseIdentifier(function_name))
        {
            ScopedIndent scoped_indent(state);

            if (ParsePunctuator(ES_Token::LEFT_PAREN) &&
                ParseFormalParameterList() &&
                ParsePunctuator(ES_Token::RIGHT_PAREN) &&
                ParseSourceElements(true) &&
                ParsePunctuator(ES_Token::RIGHT_BRACE))
            {
                --in_function;

                ScheduleLinebreak();

                return true;
            }
        }
        else if (is_eval || allow_top_level_function_expr)
        {
            --in_function;

            if (!ParseFunctionExpr())
                return false;
            else
            {
                PopExpression();
                PushStatement();

                return true;
            }
        }
    }

    return false;
}

bool
ES_Formatter::ParseFunctionExpr()
{
    ScopedContext scoped_context(state, IN_FUNC_EXPR);

    JString *function_name;

    unsigned identifiers_before = identifier_stack_used;
    unsigned statements_before = statement_stack_used;

    if (IsNearStackLimit())
        return false;

    ++in_function;

    if (ParseKeyword(ES_Token::KEYWORD_FUNCTION, true))
    {
        if (ParseIdentifier(function_name, true))
        {
            ScopedIndent scoped_indent(state);

            if (ParsePunctuator(ES_Token::LEFT_PAREN) &&
                ParseFormalParameterList() &&
                ParsePunctuator(ES_Token::RIGHT_PAREN) &&
                ParseSourceElements(true) &&
                ParsePunctuator(ES_Token::RIGHT_BRACE))
            {
                unsigned parameter_names_count = identifier_stack_used - identifiers_before;
                unsigned statements_count = statement_stack_used - statements_before;

                PopIdentifiers(parameter_names_count);
                PopStatements(statements_count);

                --in_function;

                /* ES_FunctionExpr. */
                PushExpression(false);

                return true;
            }
        }
    }

    return false;
}


bool
ES_Formatter::ParseFormalParameterList()
{
    JString *parameter_name;

    if (!ParseIdentifier(parameter_name))
        return true;

    while (1)
    {
        PushIdentifier();

        if (!ParsePunctuator(ES_Token::COMMA))
            return true;

        if (!ParseIdentifier(parameter_name))
            return false;
    }
}


bool
ES_Formatter::ParseArguments(unsigned depth)
{
    bool first = true;

    while (1)
    {
        if (ParsePunctuator(ES_Token::RIGHT_PAREN))
            return true;

        if (!first && !ParsePunctuator(ES_Token::COMMA))
            return false;
        else if (!ParseExpression(depth, ES_Expression::PROD_ASSIGNMENT_EXPR, true, expression_stack_used))
            return false;

        first = false;
    }
}


bool
ES_Formatter::ParseVariableDeclList(unsigned depth, bool allow_in)
{
    bool first = true;

    while (1)
    {
        JString *identifier;

        ScopedIndent scoped_indent(state);

        if (!first && state.context != IN_FOR_DECL_LIST)
            ScheduleLinebreak();

        if (!ParseIdentifier(identifier))
            return !first;

        if (ParsePunctuator(ES_Token::ASSIGN))
        {
            if (!ParseExpression(depth, ES_Expression::PROD_ASSIGNMENT_EXPR, allow_in, expression_stack_used))
                return false;

            /* Pop "initializer". */
            PopExpression();
        }

        if (token.type == ES_Token::INVALID)
            return false;

        PushIdentifier();
        /* ES_AssignExpr. */
        PushExpression(false);

        if (!ParsePunctuator(ES_Token::COMMA))
            return true;

        first = false;
    }
}


bool
ES_Formatter::ParseProperty(JString *&i, bool opt)
{
    ES_Value_Internal v;
    bool regexp;

    if (!ParseIdentifier(i, opt, true))
        if (ParseLiteral(v, i, regexp, opt))
            if (regexp)
                return false;
            else
                switch (v.Type())
                {
                case ESTYPE_INT32:
                case ESTYPE_DOUBLE:
                case ESTYPE_STRING:
                    break;

                default:
                    return false;
                }
        else
            return false;

    return true;
}


bool
ES_Formatter::ParseLiteral(ES_Value_Internal &v, JString *&i, bool &regexp, bool opt)
{
    if (!HandleLinebreak())
        return false;

    v.SetUndefined();

    if (token.type != ES_Token::LITERAL)
    {
        lexer->RevertTokenRegexp();
        token = lexer->GetToken();

        if (token.type != ES_Token::LITERAL)
        {
            if (token.type == ES_Token::INVALID)
                return false;

            return opt;
        }
        else
            regexp = true;
    }
    else
        regexp = false;

    v = token.value;
    AppendLiteral(i = lexer->GetTokenAsJString(false));

    if (!NextToken())
        return false;

    return true;
}


bool
ES_Formatter::ParseSemicolon(bool opt)
{
    if (token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::SEMICOLON)
    {
        AppendPunctuator();
        return NextToken();
    }
    else if (opt || linebreak_seen || token.type == ES_Token::LINEBREAK || token.type == ES_Token::END || (token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::RIGHT_BRACE))
        return true;
    else
        return false;
}


bool
ES_Formatter::ParseStatement(unsigned depth, bool in_indented_scope)
{
    ScopedContext scoped_context(state, IN_STATEMENT);

    if (IsNearStackLimit() || ++depth > ES_MAXIMUM_SYNTAX_TREE_DEPTH)
        return false;

    if (!HandleLinebreak())
        return false;

    /* Only indent if the block statement has no control statement. Otherwise
       the control statement has indented already. */
    state.in_standalone_block = !in_indented_scope && token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::LEFT_BRACE;
    ScopedIndent scoped_indent(state.in_standalone_block ? state : fake_state);

    if (ParsePunctuator(ES_Token::LEFT_BRACE))
    {
        unsigned statements_before = statement_stack_used;

        while (1)
        {
            if (ParsePunctuator(ES_Token::RIGHT_BRACE))
                break;
            else if (!ParseStatement(depth))
                return false;
        }

        unsigned statements_count = statement_stack_used - statements_before;
        PopStatements(statements_count);
    }
    else if (ParseKeyword(ES_Token::KEYWORD_VAR))
    {
        unsigned identifiers_before = identifier_stack_used;

        if (!ParseVariableDeclList(depth, true) || !ParseSemicolon())
            return false;

        unsigned decls_count = identifier_stack_used - identifiers_before;

        /* Pop names. */
        PopIdentifiers(decls_count);
        /* Pop initializers. */
        PopExpressions(decls_count);
    }
    else if (ParsePunctuator(ES_Token::SEMICOLON))
    {
    }
    else if (ParseKeyword(ES_Token::KEYWORD_IF))
    {
        if (!ParsePunctuator(ES_Token::LEFT_PAREN) ||
            !ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
            !ParsePunctuator(ES_Token::RIGHT_PAREN))
            return false;

        ScheduleLinebreak();

        {
            ScopedIndent scoped_indent(state);

            if (!ParseStatement(depth, true))
                return false;
        }

        /* Pop condition. */
        PopExpression();
        /* Pop "ifstmt". */
        PopStatement();

        if (ParseKeyword(ES_Token::KEYWORD_ELSE))
        {
            ScheduleLinebreak();

            /* 'Else' followed by 'if', leave indenting to the latter. */
            if (token.type != ES_Token::KEYWORD || token.keyword != ES_Token::KEYWORD_IF)
            {
                ScopedIndent scoped_indent(state);

                if (!ParseStatement(depth, true))
                    return false;
            }
            else if (!ParseStatement(depth, false))
                return false;

            /* Pop "elsestmt". */
            PopStatement();
        }
    }
    else if (ParseKeyword(ES_Token::KEYWORD_DO))
    {
        ScheduleLinebreak();

        {
            ScopedIndent scoped_indent(state);

            if (!ParseStatement(depth, true) || !HandleLinebreak())
                return false;
        }

        if (!ParseKeyword(ES_Token::KEYWORD_WHILE) ||
            !ParsePunctuator(ES_Token::LEFT_PAREN) ||
            !ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
            !ParsePunctuator(ES_Token::RIGHT_PAREN))
            return false;

        if (!HandleLinebreak() || !ParseSemicolon(true))
            return false;

        /* Pop condition. */
        PopExpression();
        /* Pop "body". */
        PopStatement();
    }
    else if (ParseKeyword(ES_Token::KEYWORD_WHILE))
    {
        if (!ParsePunctuator(ES_Token::LEFT_PAREN) ||
            !ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
            !ParsePunctuator(ES_Token::RIGHT_PAREN))
            return false;

        ScheduleLinebreak();

        ScopedIndent scoped_indent(state);

        if (!ParseStatement(depth, true))
            return false;

        /* Pop condition. */
        PopExpression();
        /* Pop "body". */
        PopStatement();
    }
    else if (ParseKeyword(ES_Token::KEYWORD_FOR))
    {
        bool is_forin = false;

        if (!ParsePunctuator(ES_Token::LEFT_PAREN))
            return false;

        if (ParseKeyword(ES_Token::KEYWORD_VAR))
        {
            unsigned identifiers_before = identifier_stack_used;

            {
                ScopedContext scoped_context(state, IN_FOR_DECL_LIST);

                if (!ParseVariableDeclList(depth, false))
                    return false;
            }

            unsigned decls_count = identifier_stack_used - identifiers_before;

            if (decls_count == 1 && ParseKeyword(ES_Token::KEYWORD_IN))
                is_forin = true;

            /* Pop names. */
            PopIdentifiers(decls_count);
            /* Pop initializers. */
            PopExpressions(decls_count);
        }
        else
        {
            unsigned expression_stack_before = expression_stack_used;

            if (!ParseExpression(depth, ES_Expression::PROD_EXPRESSION, false, expression_stack_used, true))
                return false;

            if (expression_stack_before != expression_stack_used)
            {
                /* Pop "init_expr". */
                PopExpression();

                if (last_expr_is_lvalue && ParseKeyword(ES_Token::KEYWORD_IN))
                    is_forin = true;
            }
        }

        if (is_forin)
        {
            if (!ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used))
                return false;

            /* Pop source. */
            PopExpression();
        }
        else
        {
            if (!ParsePunctuator(ES_Token::SEMICOLON))
                return false;

            unsigned expression_stack_before = expression_stack_used;

            if (!ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used, true))
                return false;

            if (expression_stack_before != expression_stack_used)
                /* Pop "condition". */
                PopExpression();

            if (!ParsePunctuator(ES_Token::SEMICOLON))
                return false;

            expression_stack_before = expression_stack_used;

            if (!ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used, true))
                return false;

            if (expression_stack_before != expression_stack_used)
                PopExpression();
        }

        if (!ParsePunctuator(ES_Token::RIGHT_PAREN))
            return false;

        ScheduleLinebreak();

        ScopedIndent scoped_indent(state);

        if (!ParseStatement(depth, true))
            return false;

        /* Pop "body". */
        PopStatement();
    }
    else if (ParseKeyword(ES_Token::KEYWORD_CONTINUE))
    {
        JString *target;

        bool previous_allow_linebreak = SetAllowLinebreak(false);

        if (!ParseIdentifier(target, true))
            return false;

        SetAllowLinebreak(previous_allow_linebreak);

        if (!ParseSemicolon())
            return false;
    }
    else if (ParseKeyword(ES_Token::KEYWORD_BREAK))
    {
        JString *target;

        bool previous_allow_linebreak = SetAllowLinebreak(false);

        if (!ParseIdentifier(target, true))
            return false;

        SetAllowLinebreak(previous_allow_linebreak);

        if (!ParseSemicolon())
            return false;
    }
    else if (ParseKeyword(ES_Token::KEYWORD_RETURN))
    {
        if (!in_function && !allow_return_in_program)
            return false;

        bool previous_allow_linebreak = SetAllowLinebreak(false);

        unsigned expression_stack_before = expression_stack_used;

        if (!ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used, true))
            return false;

        if (expression_stack_before != expression_stack_used)
            PopExpression();

        SetAllowLinebreak(previous_allow_linebreak);

        if (!ParseSemicolon())
            return false;
    }
    else if (ParseKeyword(ES_Token::KEYWORD_WITH))
    {
        in_with++;

        if (!ParsePunctuator(ES_Token::LEFT_PAREN) ||
            !ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
            !ParsePunctuator(ES_Token::RIGHT_PAREN))
            return false;

        ScheduleLinebreak();

        ScopedIndent scoped_indent(state);

        if (!ParseStatement(depth, true))
            return false;

        PopExpression();
        /* Pop "body". */
        PopStatement();

        in_with--;
    }
    else if (ParseKeyword(ES_Token::KEYWORD_SWITCH))
    {
        if (!ParsePunctuator(ES_Token::LEFT_PAREN) ||
            !ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
            !ParsePunctuator(ES_Token::RIGHT_PAREN))
            return false;

        ScopedIndent scoped_indent(state);

        if (!ParsePunctuator(ES_Token::LEFT_BRACE))
            return false;

        PopExpression();

        unsigned statements_before = statement_stack_used;
        unsigned case_clauses_count = 0;
        bool seen_default = false;

        while (1)
        {
            if (ParsePunctuator(ES_Token::RIGHT_BRACE))
                break;
            else if (ParseKeyword(ES_Token::KEYWORD_CASE))
            {
                if (!ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
                    !ParsePunctuator(ES_Token::CONDITIONAL_FALSE))
                    return false;

                ScheduleLinebreak();

                PopExpression();

                ++case_clauses_count;
            }
            else if (ParseKeyword(ES_Token::KEYWORD_DEFAULT))
            {
                if (seen_default)
                    return false;

                seen_default = true;

                if (!ParsePunctuator(ES_Token::CONDITIONAL_FALSE))
                    return false;

                ScheduleLinebreak();

                ++case_clauses_count;
            }
            else
            {
                ScopedIndent scoped_indent(state);

                if (case_clauses_count == 0 || !ParseStatement(depth, true))
                    return false;
            }
        }

        unsigned statements_count = statement_stack_used - statements_before;
        PopStatements(statements_count);
    }
    else if (ParseKeyword(ES_Token::KEYWORD_THROW))
    {
        bool previous_allow_linebreak = SetAllowLinebreak(false);

        if (!ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used))
            return false;

        SetAllowLinebreak(previous_allow_linebreak);

        if (!ParseSemicolon())
            return false;

        PopExpression();
    }
    else if (ParseKeyword(ES_Token::KEYWORD_TRY))
    {
        JString *catch_identifier;
        bool has_catch_block = true;
        bool has_finally_block = true;

        {
            ScopedIndent scoped_indent(state);

            if (!ParseStatement(depth, true))
                return false;
        }

        /* Pop "try_block". */
        PopStatement();

        if (ParseKeyword(ES_Token::KEYWORD_CATCH))
        {
            ++in_with;

            if (!ParsePunctuator(ES_Token::LEFT_PAREN) ||
                !ParseIdentifier(catch_identifier) ||
                !ParsePunctuator(ES_Token::RIGHT_PAREN))
                return false;

            ScopedIndent scoped_indent(state);

            if (!ParseStatement(depth, true))
                return false;

            --in_with;

            /* Pop "catch_block". */
            PopStatement();
        }
        else
            has_catch_block = false;

        if (ParseKeyword(ES_Token::KEYWORD_FINALLY))
        {
            ScopedIndent scoped_indent(state);

            if (!ParseStatement(depth, true))
                return false;

            /* Pop "finally_block". */
            PopStatement();
        }
        else
            has_finally_block = false;

        if (!has_catch_block && !has_finally_block)
            return false;
    }
    else if (ParseKeyword(ES_Token::KEYWORD_FUNCTION))
    {
        if (!ParseFunctionDecl(true))
            return false;
    }
    else if (ParseKeyword(ES_Token::KEYWORD_DEBUGGER))
    {
        if (!ParseSemicolon())
            return false;
    }
    else
    {
        JString *identifier;

        unsigned expression_stack_base = expression_stack_used;

        if (ParseIdentifier(identifier))
        {
            if (ParsePunctuator(ES_Token::CONDITIONAL_FALSE))
            {
                if (!ParseStatement(depth))
                    return false;

                PopStatement();

                return true;
            }
            else
                 /* ES_IdentifierExpr. */
                 PushExpression(true);
        }

        if (!ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_base))
            return false;

        PopExpression();

        if (!ParseSemicolon())
            return false;
    }

    ScheduleLinebreak();

    PushStatement();
    return true;
}


bool
ES_Formatter::ParseAccessor(AccessorType type, JString *name)
{
    unsigned identifiers_before = identifier_stack_used;
    in_function++;

    ScopedIndent scoped_indent(state);

    if (!ParsePunctuator(ES_Token::LEFT_PAREN) ||
        type == ACCESSOR_SET && !ParseFormalParameterList() ||
        !ParsePunctuator(ES_Token::RIGHT_PAREN) ||
        !ParseSourceElements(true) ||
        !ParsePunctuator(ES_Token::RIGHT_BRACE))
      return false;

    in_function--;

    unsigned parameter_names_count = identifier_stack_used - identifiers_before;
    PopIdentifiers(parameter_names_count);

    return true;
}


bool
ES_Formatter::ParseExpression(unsigned depth, unsigned production, bool allow_in, unsigned expr_stack_base, bool opt)
{
    ScopedContext scoped_context(state, IN_EXPRESSION);

recurse:

    if (++depth > ES_MAXIMUM_SYNTAX_TREE_DEPTH)
        return false;

    unsigned expression_stack_length = expression_stack_used - expr_stack_base;

    JString *identifier;
    ES_Value_Internal value;

    /* These are always allowed and always unambigious. */
    if (expression_stack_length == 0)
    {
        if (ParseKeyword(ES_Token::KEYWORD_THIS))
        {
            /* ES_ThisExpr. */
            PushExpression(false);
            goto recurse;
        }

        if (ParseIdentifier(identifier))
        {
            /* ES_IdentifierExpr. */
            PushExpression(true);
            goto recurse;
        }

        bool regexp;

        if (ParseLiteral(value, identifier, regexp))
        {
            /* ES_RegExpLiteralExpr, ES_LiteralExpr. */
            PushExpression(false);
            goto recurse;
        }

        if (ParsePunctuator(ES_Token::LEFT_PAREN))
        {
            // parse grouped expression
            if (!ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) || !ParsePunctuator(ES_Token::RIGHT_PAREN))
                return false;

            goto recurse;
        }
    }

    ES_Expression::Production p = static_cast<ES_Expression::Production>(production);

    if (expression_stack_length == 0 && p < ES_Expression::PROD_UNARY_EXPR)
        p = ES_Expression::PROD_UNARY_EXPR;

    if (!HandleLinebreak())
        return false;

    switch (p)
    {
    case ES_Expression::PROD_EXPRESSION:
        if (ParsePunctuator(ES_Token::COMMA))
            if (ParseExpression(depth, ES_Expression::PROD_ASSIGNMENT_EXPR, allow_in, expression_stack_used))
            {
                /* Pop left and right. */
                PopExpressions(2);

                /* ES_CommaExpr. */
                PushExpression(false);
                goto recurse;
            }
            else
                return false;

    case ES_Expression::PROD_ASSIGNMENT_EXPR:
        if (token.type == ES_Token::PUNCTUATOR)
        {
            bool is_assign = true;
            bool is_compound_assign = true;

            switch (token.punctuator)
            {
            case ES_Token::ASSIGN:
                is_compound_assign = false;
                break;

            case ES_Token::ADD_ASSIGN:
            case ES_Token::MULTIPLY_ASSIGN:
            case ES_Token::DIVIDE_ASSIGN:
            case ES_Token::REMAINDER_ASSIGN:
            case ES_Token::SUBTRACT_ASSIGN:
            case ES_Token::SHIFT_LEFT_ASSIGN:
            case ES_Token::SHIFT_SIGNED_RIGHT_ASSIGN:
            case ES_Token::SHIFT_UNSIGNED_RIGHT_ASSIGN:
            case ES_Token::BITWISE_AND_ASSIGN:
            case ES_Token::BITWISE_OR_ASSIGN:
            case ES_Token::BITWISE_XOR_ASSIGN:
                break;

            default:
                is_assign = false;
            }

            if (is_assign)
            {
                AppendPunctuator();

                if (!NextToken())
                    return false;

                /* Pop target. */
                PopExpression();

                if (!ParseExpression(depth, ES_Expression::PROD_ASSIGNMENT_EXPR, allow_in, expression_stack_used))
                    return false;

                /* Pop source. */
                PopExpression();

                if (!is_compound_assign)
                {
                    /* ES_AssignExpr. */
                    PushExpression(false);
                    goto recurse;
                }

                /* ES_BinaryNumberExpr or ES_AddExpr or ES_ShiftExpr or ES_BitwiseExpr. */
                PushExpression(false);
                goto recurse;
            }
        }

    case ES_Expression::PROD_CONDITIONAL_EXPR:
        if (ParsePunctuator(ES_Token::CONDITIONAL_TRUE))
        {
            ScopedContext scoped_context(state, IN_TERNARY_EXPR);

            if (!ParseExpression(depth, ES_Expression::PROD_ASSIGNMENT_EXPR, true, expression_stack_used) ||
                !ParsePunctuator(ES_Token::CONDITIONAL_FALSE) ||
                !ParseExpression(depth, ES_Expression::PROD_ASSIGNMENT_EXPR, allow_in, expression_stack_used))
                return false;

            /* Pop "second", "first" and "condition". */
            PopExpressions(3);

            /* ES_ConditionalExpr. */
            PushExpression(false);
            goto recurse;
        }

    case ES_Expression::PROD_LOGICAL_OR_EXPR:
        if (ParsePunctuator(ES_Token::LOGICAL_OR))
            if (ParseExpression(depth, ES_Expression::PROD_LOGICAL_AND_EXPR, allow_in, expression_stack_used))
            {
                /* Pop left and right. */
                PopExpressions(2);

                /* ES_LogicalExpr. */
                PushExpression(false);

                goto recurse;
            }
            else
                return false;

    case ES_Expression::PROD_LOGICAL_AND_EXPR:
        if (ParsePunctuator(ES_Token::LOGICAL_AND))
            if (ParseExpression(depth, ES_Expression::PROD_BITWISE_OR_EXPR, allow_in, expression_stack_used))
            {
                /* Pop left and right. */
                PopExpressions(2);

                /* ES_LogicalExpr. */
                PushExpression(false);

                goto recurse;
            }
            else
                return false;

    case ES_Expression::PROD_BITWISE_OR_EXPR:
        if (ParsePunctuator(ES_Token::BITWISE_OR))
            if (ParseExpression(depth, ES_Expression::PROD_BITWISE_XOR_EXPR, allow_in, expression_stack_used))
            {
                /* Pop left and right. */
                PopExpressions(2);

                /* ES_BitwiseExpr. */
                PushExpression(false);

                goto recurse;
            }
            else
                return false;

    case ES_Expression::PROD_BITWISE_XOR_EXPR:
        if (ParsePunctuator(ES_Token::BITWISE_XOR))
            if (ParseExpression(depth, ES_Expression::PROD_BITWISE_AND_EXPR, allow_in, expression_stack_used))
            {
                /* Pop left and right. */
                PopExpressions(2);

                /* ES_BitwiseExpr. */
                PushExpression(false);

                goto recurse;
            }
            else
                return false;

    case ES_Expression::PROD_BITWISE_AND_EXPR:
        if (ParsePunctuator(ES_Token::BITWISE_AND))
            if (ParseExpression(depth, ES_Expression::PROD_EQUALITY_EXPR, allow_in, expression_stack_used))
            {
                /* Pop left and right. */
                PopExpressions(2);

                /* ES_BitwiseExpr. */
                PushExpression(false);

                goto recurse;
            }
            else
                return false;

    case ES_Expression::PROD_EQUALITY_EXPR:
        if (token.type == ES_Token::PUNCTUATOR)
        {
            bool is_equality = true;

            switch (token.punctuator)
            {
            case ES_Token::EQUAL:
            case ES_Token::NOT_EQUAL:
            case ES_Token::STRICT_EQUAL:
            case ES_Token::STRICT_NOT_EQUAL:
                break;

            default:
                is_equality = false;
            }

            if (is_equality)
            {
                AppendPunctuator();

                if (!NextToken())
                    return false;

                if (!ParseExpression(depth, ES_Expression::PROD_RELATIONAL_EXPR, allow_in, expression_stack_used))
                    return false;

                /* Pop "right" and "left". */
                PopExpressions(2);

                /* ES_EqualityExpr. */
                PushExpression(false);
                goto recurse;
            }
        }

    case ES_Expression::PROD_RELATIONAL_EXPR:
        if (token.type == ES_Token::PUNCTUATOR || token.type == ES_Token::KEYWORD)
        {
            bool is_relational = false;
            bool is_instanceof_or_in = false;

            if (token.type == ES_Token::PUNCTUATOR)
            {
                is_relational = true;

                switch (token.punctuator)
                {
                case ES_Token::LESS_THAN:
                case ES_Token::GREATER_THAN:
                case ES_Token::LESS_THAN_OR_EQUAL:
                case ES_Token::GREATER_THAN_OR_EQUAL:
                    break;

                default:
                    is_relational = false;
                }
            }
            else if (token.keyword == ES_Token::KEYWORD_INSTANCEOF
                     || token.keyword == ES_Token::KEYWORD_IN && allow_in)
                is_instanceof_or_in = true;

            if (is_relational || is_instanceof_or_in)
            {
                if (is_relational)
                    AppendPunctuator();
                else
                    AppendKeyword();

                if (!NextToken())
                    return false;

                if (!ParseExpression(depth, ES_Expression::PROD_SHIFT_EXPR, true, expression_stack_used))
                    return false;

                /* Pop "right" and "left". */
                PopExpressions(2);

                /* ES_RelationalExpr or ES_InstanceofOrInExpr. */
                PushExpression(false);

                goto recurse;
            }
        }

    case ES_Expression::PROD_SHIFT_EXPR:
        if (token.type == ES_Token::PUNCTUATOR)
        {
            bool is_shift = true;

            switch (token.punctuator)
            {
            case ES_Token::SHIFT_LEFT:
            case ES_Token::SHIFT_SIGNED_RIGHT:
            case ES_Token::SHIFT_UNSIGNED_RIGHT:
                break;

            default:
                is_shift = false;
            }

            if (is_shift)
            {
                AppendPunctuator();

                if (!NextToken())
                    return false;

                if (!ParseExpression(depth, ES_Expression::PROD_ADDITIVE_EXPR, true, expression_stack_used))
                    return false;

                /* Pop "right" and "left". */
                PopExpressions(2);

                /* ES_ShiftExpr. */
                PushExpression(false);

                goto recurse;
            }
        }

    case ES_Expression::PROD_ADDITIVE_EXPR:
        if (token.type == ES_Token::PUNCTUATOR)
        {
            bool is_add = false;
            bool is_subtract = false;

            if (token.punctuator == ES_Token::ADD)
                is_add = true;
            else if (token.punctuator == ES_Token::SUBTRACT)
                is_subtract = true;

            if (is_add || is_subtract)
            {
                AppendPunctuator();

                if (!NextToken())
                    return false;

                if (!ParseExpression(depth, ES_Expression::PROD_MULTIPLICATIVE_EXPR, true, expression_stack_used))
                    return false;

                /* Pop "right" and "left". */
                PopExpressions(2);

                /* ES_AddExpr or ES_BinaryNumberExpr. */
                PushExpression(false);

                goto recurse;
            }
        }

    case ES_Expression::PROD_MULTIPLICATIVE_EXPR:
        if (token.type == ES_Token::PUNCTUATOR)
        {
            bool is_multiplicative = true;

            switch (token.punctuator)
            {
            case ES_Token::MULTIPLY:
            case ES_Token::DIVIDE:
            case ES_Token::REMAINDER:
                break;

            default:
                is_multiplicative = false;
            }

            if (is_multiplicative)
            {
                AppendPunctuator();

                if (!NextToken())
                    return false;

                if (!ParseExpression(depth, ES_Expression::PROD_UNARY_EXPR, true, expression_stack_used))
                    return false;

                /* Pop "right" and "left". */
                PopExpressions(2);

                /* ES_BinaryNumberExpr. */
                PushExpression(false);

                goto recurse;
            }
        }

    case ES_Expression::PROD_UNARY_EXPR:
        if (expression_stack_length == 0)
        {
            if (ParseKeyword(ES_Token::KEYWORD_DELETE))
            {
                if (!ParseExpression(depth, ES_Expression::PROD_UNARY_EXPR, true, expression_stack_used))
                    return false;

                PopExpression();

                /* ES_DeleteExpr. */
                PushExpression(false);
                goto recurse;
            }

            bool is_unary = true;
            bool is_inc_or_dec = false;
            bool is_typeof = false;

            if (token.type == ES_Token::KEYWORD)
            {
                switch (token.keyword)
                {
                case ES_Token::KEYWORD_VOID:
                    break;

                case ES_Token::KEYWORD_TYPEOF:
                    is_typeof = true;
                    break;

                default:
                    is_unary = false;
                }

                if (is_unary)
                    AppendKeyword();
            }
            else if (token.type == ES_Token::PUNCTUATOR)
            {
                switch (token.punctuator)
                {
                case ES_Token::INCREMENT:
                case ES_Token::DECREMENT:
                    is_inc_or_dec = true;
                    is_unary = false;
                    break;

                case ES_Token::ADD:
                case ES_Token::SUBTRACT:
                case ES_Token::BITWISE_NOT:
                case ES_Token::LOGICAL_NOT:
                    break;

                default:
                    is_unary = false;
                }

                if (is_unary || is_inc_or_dec)
                    AppendPunctuator(true);
            }
            else
                is_unary = false;

            if (is_unary || is_inc_or_dec)
            {
                if (!NextToken())
                    return false;

                if (is_unary && is_typeof)
                    ++in_typeof;

                if (!ParseExpression(depth, ES_Expression::PROD_UNARY_EXPR, true, expression_stack_used))
                    return false;

                if (is_unary && is_typeof)
                    --in_typeof;

                PopExpression();

                /* ES_LiteralExpr or ES_UnaryExpr or ES_IncrementOrDecrementExpr. */
                PushExpression(false);

                goto recurse;
            }
        }

    case ES_Expression::PROD_POSTFIX_EXPR:
        if (expression_stack_length > 0)
        {
            bool is_postfix = true;
            bool previous_allow_linebreak = SetAllowLinebreak(false);

            if (last_token.type == ES_Token::LINEBREAK)
                is_postfix = false;
            else if (!ParsePunctuator(ES_Token::INCREMENT) && !ParsePunctuator(ES_Token::DECREMENT))
                is_postfix = false;

            SetAllowLinebreak(previous_allow_linebreak);

            if (is_postfix)
            {
                PopExpression();

                /* ES_IncrementOrDecrementExpr. */
                PushExpression(false);
                goto recurse;
            }
        }

    case ES_Expression::PROD_LEFT_HAND_SIDE_EXPR:
    case ES_Expression::PROD_CALL_EXPR:
        if (expression_stack_length > 0)
        {
            unsigned exprs_before = expression_stack_used;

            if (ParsePunctuator(ES_Token::LEFT_PAREN))
                if (ParseArguments(depth))
                {
                    unsigned args_count = expression_stack_used - exprs_before;
                    /* Pop arguments. */
                    PopExpressions(args_count);
                    /* Pop function. */
                    PopExpression();

                    /* ES_CallExpr. */
                    PushExpression(false);
                    goto recurse;
                }
                else
                    return false;
        }

    case ES_Expression::PROD_NEW_EXPR:
    case ES_Expression::PROD_MEMBER_EXPR:
        if (expression_stack_length > 0)
        {
            switch (ParsePunctuator1(ES_Token::LEFT_BRACKET))
            {
            case INVALID_TOKEN:
                return false;

            case FOUND:
                if (!ParseExpression(depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
                    !ParsePunctuator(ES_Token::RIGHT_BRACKET))
                    return false;

                /* Pop "index" and "base". */
                PopExpressions(2);

                /* ES_PropertyReferenceExpr or ES_ArrayReferenceExpr. */
                PushExpression(true);
                goto recurse;
            }

            switch (ParsePunctuator1(ES_Token::PERIOD))
            {
            case INVALID_TOKEN:
                return false;

            case FOUND:
                JString *name;

                if (!ParseIdentifier(name, false, true))
                    return false;

                /* Pop base. */
                PopExpression();

                /* ES_PropertyReferenceExpr. */
                PushExpression(true);
                goto recurse;
            }
        }

        if (expression_stack_length == 0)
            if (ParseKeyword(ES_Token::KEYWORD_NEW))
            {
                unsigned args_count;

                if (expression_stack_length > 0)
                    return false;

                if (!ParseExpression(depth, ES_Expression::PROD_NEW_EXPR, true, expression_stack_used))
                    return false;

                unsigned exprs_before = expression_stack_used;

                if (ParsePunctuator(ES_Token::LEFT_PAREN))
                    if (ParseArguments(depth))
                    {
                        args_count = expression_stack_used - exprs_before;
                        /* Pop arguments. */
                        PopExpressions(args_count);
                    }
                    else
                        return false;
                else if (production < ES_Expression::PROD_MEMBER_EXPR)
                {
                    args_count = 0;
                }
                else
                    return false;

                /* Pop constructor. */
                PopExpression();

                /* ES_NewExpr. */
                PushExpression(false);
                goto recurse;
            }
            else if (ParseKeyword(ES_Token::KEYWORD_FUNCTION))
            {
                if (!ParseFunctionExpr())
                    return false;

                goto recurse;
            }

    case ES_Expression::PROD_PRIMARY_EXPR:
        if (expression_stack_length == 0)
        {
            if (ParsePunctuator(ES_Token::LEFT_BRACKET))
            {
                unsigned exprs_count;
                unsigned expressions_before = expression_stack_used;

                while (!ParsePunctuator(ES_Token::RIGHT_BRACKET))
                    if (ParseExpression(depth, ES_Expression::PROD_ASSIGNMENT_EXPR, true, expression_stack_used))
                    {
                        if (!ParsePunctuator(ES_Token::COMMA))
                            if (ParsePunctuator(ES_Token::RIGHT_BRACKET))
                                break;
                            else
                                return false;
                    }
                    else if (ParsePunctuator(ES_Token::COMMA))
                        PushExpression(false);
                    else
                        return false;

                exprs_count = expression_stack_used - expressions_before;
                /* Pop expressions. */
                PopExpressions(exprs_count);

                /* ES_ArrayLiteralExpr. */
                PushExpression(false);
                goto recurse;
            }

            ScopedIndent scoped_indent(state);

            if (ParsePunctuator(ES_Token::LEFT_BRACE))
            {
                while (1)
                {
                    JString *name;

                    if (!ParseProperty(name, false))
                        if (ParsePunctuator(ES_Token::RIGHT_BRACE))
                            break;
                        else
                            return false;

                    if (name)
                    {
                        JString *actual_name;

                        if (name->Equals(UNI_L("get"), 3))
                        {
                            ScopedContext scoped_context(state, IN_ACCESSOR);
                            ScheduleSpace();
                            if (ParseProperty(actual_name, false))
                            {
                                if (!ParseAccessor(ACCESSOR_GET, actual_name))
                                    return false;
                            }
                            else
                                goto regular_property;
                        }
                        else if (name->Equals(UNI_L("set"), 3))
                        {
                            ScopedContext scoped_context(state, IN_ACCESSOR);
                            ScheduleSpace();
                            if (ParseProperty(actual_name, false))
                            {
                                if (!ParseAccessor(ACCESSOR_SET, actual_name))
                                    return false;
                            }
                            else
                                goto regular_property;
                        }
                        else
                        {
                        regular_property:

                            if (!ParsePunctuator(ES_Token::CONDITIONAL_FALSE) ||
                                !ParseExpression(depth, ES_Expression::PROD_ASSIGNMENT_EXPR, true, expression_stack_used))
                                return false;

                            PopExpression();
                        }
                    }

                    if (ParsePunctuator(ES_Token::COMMA))
                        ScheduleLinebreak();
                    else
                    {
                        if (ParsePunctuator(ES_Token::RIGHT_BRACE))
                            break;
                        else
                            return false;
                    }
                }

                /* ES_ObjectLiteralExpr. */
                PushExpression(false);
                goto recurse;
            }
        }

        if (expression_stack_length == 1 || (expression_stack_length == 0 && opt))
            return true;
        else
            return false;
    }

    /* Never reached. */
    return false;
}


bool
ES_Formatter::NextToken()
{
    last_token = token;

    lexer->NextToken();
    token = lexer->GetToken();

    while (token.type == ES_Token::PUNCTUATOR &&
           (token.punctuator == ES_Token::SINGLE_LINE_COMMENT || token.punctuator == ES_Token::MULTI_LINE_COMMENT))
    {
        bool is_single_line = token.punctuator == ES_Token::SINGLE_LINE_COMMENT;

        AppendLiteral(lexer->GetTokenAsJString(false));

        lexer->NextToken();
        token = lexer->GetToken();

        if (token.type == ES_Token::LINEBREAK)
            if (is_single_line)
                LinebreakInternal();
            else
                ScheduleLinebreak();
    }

    if (token.type == ES_Token::INVALID)
        return false;

    linebreak_seen = token.type == ES_Token::LINEBREAK;
    return true;
}


bool
ES_Formatter::SetAllowLinebreak(bool new_value)
{
    can_append_linebreak = new_value;
    bool old_value = allow_linebreak;
    allow_linebreak = new_value;
    return old_value;
}


bool
ES_Formatter::GetAllowLinebreak()
{
    return allow_linebreak;
}
