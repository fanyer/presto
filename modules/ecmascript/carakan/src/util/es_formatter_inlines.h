/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA 2012
 *
 * Ecmascript source formatter - inline methods.
 */
#ifndef ES_FORMATTER_INLINES_H
#define ES_FORMATTER_INLINES_H

inline bool
ES_Formatter::HandleLinebreak()
{
    while (allow_linebreak && token.type == ES_Token::LINEBREAK)
    {
        if (!NextToken())
            return false;

        linebreak_seen = true;
    }

    if (token.type != ES_Token::LINEBREAK)
        allow_linebreak = true;

    return token.type != ES_Token::INVALID;
}


inline bool
ES_Formatter::ParseIdentifier(JString *&i, bool opt, bool allow_reserved)
{
    if (!HandleLinebreak())
        return false;

    i = 0;

    if (token.type != ES_Token::IDENTIFIER)
        if (opt)
            return true;
        else if (allow_reserved && (token.type == ES_Token::RESERVED ||
                                    token.type == ES_Token::LITERAL && (token.value.IsBoolean() || token.value.IsNull()) ||
                                    token.type == ES_Token::KEYWORD))
        {
            lexer->RevertTokenIdentifier();
            token = lexer->GetToken();
        }
        else
            return false;

    i = token.identifier;
    AppendLiteral(lexer->GetTokenAsJString(false));

    if (!NextToken())
        return false;

    return true;
}


inline bool
ES_Formatter::ParsePunctuator(ES_Token::Punctuator p, bool opt)
{
    return ParsePunctuator1(p) == FOUND || opt;
}

inline ES_Formatter::ParseTokenResult
ES_Formatter::ParsePunctuator1(ES_Token::Punctuator p)
{
    if (!HandleLinebreak())
        return INVALID_TOKEN;

    if (token.type != ES_Token::PUNCTUATOR || token.punctuator != p)
        return NOT_FOUND;

    AppendPunctuator();

    return NextToken() ? FOUND : INVALID_TOKEN;
}


inline bool
ES_Formatter::ParseKeyword(ES_Token::Keyword k, bool opt)
{
    if (!HandleLinebreak())
        return false;

    if (token.type != ES_Token::KEYWORD || token.keyword != k)
        return opt;

    AppendKeyword();

    if (!NextToken())
        return false;

    return true;
}


inline void
ES_Formatter::AppendPunctuator(bool is_unary)
{
    OP_ASSERT(token.type == ES_Token::PUNCTUATOR);

    bool brace = false;
    bool break_before = false;
    bool break_after = false;

    const char *token_string;
    bool space_before;
    bool space_after;
    GetPunctuatorTokenData(token.punctuator, token_string, space_before, space_after);

    if (is_unary)
        space_before = space_after = false;

    switch (token.punctuator)
    {
    case ES_Token::LEFT_BRACE:
        brace = space_before = break_after = true;
        /* Opening brace of the block with control statement belongs on the same
           line. */
        if (state.context == IN_STATEMENT && !state.in_standalone_block)
            is_linebreak_scheduled = false;
        /* No space before braces inside parenthesis or brackets - [{}], ({}). */
        if (last_token.type == ES_Token::PUNCTUATOR &&
            last_token.punctuator == ES_Token::LEFT_BRACKET || last_token.punctuator == ES_Token::LEFT_PAREN)
            space_before = false;
        break;

    case ES_Token::RIGHT_BRACE:
        brace = break_before = true;
        /* Collapse empty code blocks. */
        if (last_token.type == ES_Token::PUNCTUATOR &&
            last_token.punctuator == ES_Token::LEFT_BRACE)
            is_linebreak_scheduled = break_before = false;
        break;

    case ES_Token::SEMICOLON:
        /* Consecutive semicolons (in empty statements, for loops). */
        if (last_token.type == ES_Token::PUNCTUATOR
            && last_token.punctuator == ES_Token::SEMICOLON)
            break;

        is_linebreak_scheduled = is_space_scheduled = false;
        break;

    case ES_Token::LEFT_PAREN:
        /* No space between specific keywords and left parenthesis. */
        if (last_token.type == ES_Token::KEYWORD
            && (last_token.keyword == ES_Token::KEYWORD_FUNCTION
                || last_token.keyword == ES_Token::KEYWORD_VOID
                || last_token.keyword == ES_Token::KEYWORD_TYPEOF))
            is_space_scheduled = false;
        break;

    case ES_Token::COLON:
        /* Space in ternary expression. */
        if (state.context == IN_TERNARY_EXPR)
            space_before = true;
        break;

    case ES_Token::PERIOD:
        /* Period token immediately following number indicates that period
           was not part of the literal thus must be separated.
           Lexer guards validity of that rule and it has to be mirrored
           in formatted script. */
        if (last_token.type == ES_Token::LITERAL && last_token.value.IsInt32())
            space_before = true;
        break;

    case ES_Token::ADD:
    case ES_Token::SUBTRACT:
        /* Last token being the same as current indicates that these two did not
           form an INCREMENT or DECREMENT token thus must be separated. */
        if (last_token.type == ES_Token::PUNCTUATOR && last_token.punctuator == token.punctuator)
            space_before = true;
        break;
    }

    if (space_before)
        ScheduleSpace();

    if (break_before)
        ScheduleLinebreak();

    AppendInternal(token_string, brace);

    if (space_after)
        ScheduleSpace();

    if (break_after)
        ScheduleLinebreak();
}


inline void
ES_Formatter::AppendKeyword()
{
    OP_ASSERT(token.type == ES_Token::KEYWORD);

    const char *token_string;
    bool space_before;
    bool space_after;
    GetKeywordTokenData(token.keyword, token_string, space_before, space_after);

    /* Avoid line break when 'else' followed by 'if'. */
    if (token.keyword == ES_Token::KEYWORD_IF &&
        last_token.type == ES_Token::KEYWORD &&
        last_token.keyword == ES_Token::KEYWORD_ELSE)
        is_linebreak_scheduled = false;

    if (space_before)
        ScheduleSpace();

    AppendInternal(token_string);

    if (space_after)
        ScheduleSpace();
}


inline void
ES_Formatter::AppendLiteral(JString *l)
{
    AppendInternal(l);
}


inline void
ES_Formatter::ScheduleLinebreak()
{
    if (can_append_linebreak && !last_char_is_linebreak)
        is_linebreak_scheduled = true;
}


inline void
ES_Formatter::ScheduleSpace()
{
    if (!last_char_is_linebreak)
        is_space_scheduled = true;
}


inline void
ES_Formatter::GetKeywordTokenData(ES_Token::Keyword keyword, const char *&string, bool &space_before, bool &space_after)
{
    switch (keyword)
    {
    case ES_Token::KEYWORD_BREAK:
        string = "break";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_CASE:
        string = "case";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_CATCH:
        string = "catch";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_CONTINUE:
        string = "continue";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_DEBUGGER:
        string = "debugger";
        space_before = false; space_after = false;
        break;

    case ES_Token::KEYWORD_DEFAULT:
        string = "default";
        space_before = false; space_after = false;
        break;

    case ES_Token::KEYWORD_DELETE:
        string = "delete";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_DO:
        string = "do";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_ELSE:
        string = "else";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_FALSE:
        string = "false";
        space_before = false; space_after = false;
        break;

    case ES_Token::KEYWORD_FINALLY:
        string = "finally";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_FOR:
        string = "for";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_FUNCTION:
        string = "function";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_IF:
        string = "if";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_IN:
        string = "in";
        space_before = true; space_after = true;
        break;

    case ES_Token::KEYWORD_INSTANCEOF:
        string = "instanceof";
        space_before = true; space_after = true;
        break;

    case ES_Token::KEYWORD_NEW:
        string = "new";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_NULL:
        string = "null";
        space_before = false; space_after = false;
        break;

    case ES_Token::KEYWORD_RETURN:
        string = "return";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_SWITCH:
        string = "switch";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_THIS:
        string = "this";
        space_before = false; space_after = false;
        break;

    case ES_Token::KEYWORD_THROW:
        string = "throw";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_TRUE:
        string = "true";
        space_before = false; space_after = false;
        break;

    case ES_Token::KEYWORD_TRY:
        string = "try";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_TYPEOF:
        string = "typeof";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_VAR:
        string = "var";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_VOID:
        string = "void";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_WHILE:
        string = "while";
        space_before = false; space_after = true;
        break;

    case ES_Token::KEYWORD_WITH:
        string = "with";
        space_before = false; space_after = true;
        break;

    default:
        OP_ASSERT(!"Unknown keyword!");
        break;
    }
}


inline void
ES_Formatter::GetPunctuatorTokenData(ES_Token::Punctuator punctuator, const char *&string, bool &space_before, bool &space_after)
{
    switch (punctuator)
    {
    case ES_Token::LEFT_BRACE:
        string = "{";
        space_before = false; space_after = false;
        break;

    case ES_Token::RIGHT_BRACE:
        string = "}";
        space_before = false; space_after = false;
        break;

    case ES_Token::LEFT_PAREN:
        string = "(";
        space_before = false; space_after = false;
        break;

    case ES_Token::RIGHT_PAREN:
        string = ")";
        space_before = false; space_after = false;
        break;

    case ES_Token::LEFT_BRACKET:
        string = "[";
        space_before = false; space_after = false;
        break;

    case ES_Token::RIGHT_BRACKET:
        string = "]";
        space_before = false; space_after = false;
        break;

    case ES_Token::PERIOD:
        string = ".";
        space_before = false; space_after = false;
        break;

    case ES_Token::SEMICOLON:
        string = ";";
        space_before = false; space_after = true;
        break;

    case ES_Token::COMMA:
        string = ",";
        space_before = false; space_after = true;
        break;

    case ES_Token::LESS_THAN:
        string = "<";
        space_before = true; space_after = true;
        break;

    case ES_Token::GREATER_THAN:
        string = ">";
        space_before = true; space_after = true;
        break;

    case ES_Token::LOGICAL_NOT:
        string = "!";
        space_before = true; space_after = false;
        break;

    case ES_Token::BITWISE_NOT:
        string = "~";
        space_before = true; space_after = false;
        break;

    case ES_Token::CONDITIONAL_TRUE:
        string = "?";
        space_before = true; space_after = true;
        break;

    case ES_Token::CONDITIONAL_FALSE:
        string = ":";
        space_before = false; space_after = true;
        break;

    case ES_Token::ASSIGN:
        string = "=";
        space_before = true; space_after = true;
        break;

    case ES_Token::ADD:
        string = "+";
        space_before = true; space_after = true;
        break;

    case ES_Token::SUBTRACT:
        string = "-";
        space_before = true; space_after = true;
        break;

    case ES_Token::MULTIPLY:
        string = "*";
        space_before = true; space_after = true;
        break;

    case ES_Token::DIVIDE:
        string = "/";
        space_before = true; space_after = true;
        break;

    case ES_Token::REMAINDER:
        string = "%";
        space_before = true; space_after = true;
        break;

    case ES_Token::BITWISE_AND:
        string = "&";
        space_before = true; space_after = true;
        break;

    case ES_Token::BITWISE_OR:
        string = "|";
        space_before = true; space_after = true;
        break;

    case ES_Token::BITWISE_XOR:
        string = "^";
        space_before = true; space_after = true;
        break;

    case ES_Token::LESS_THAN_OR_EQUAL:
        string = "<=";
        space_before = true; space_after = true;
        break;

    case ES_Token::GREATER_THAN_OR_EQUAL:
        string = ">=";
        space_before = true; space_after = true;
        break;

    case ES_Token::EQUAL:
        string = "==";
        space_before = true; space_after = true;
        break;

    case ES_Token::NOT_EQUAL:
        string = "!=";
        space_before = true; space_after = true;
        break;

    case ES_Token::INCREMENT:
        string = "++";
        space_before = false; space_after = false;
        break;

    case ES_Token::DECREMENT:
        string = "--";
        space_before = false; space_after = false;
        break;

    case ES_Token::LOGICAL_AND:
        string = "&&";
        space_before = true; space_after = true;
        break;

    case ES_Token::LOGICAL_OR:
        string = "||";
        space_before = true; space_after = true;
        break;

    case ES_Token::SHIFT_LEFT:
        string = "<<";
        space_before = true; space_after = true;
        break;

    case ES_Token::SHIFT_SIGNED_RIGHT:
        string = ">>";
        space_before = true; space_after = true;
        break;

    case ES_Token::ADD_ASSIGN:
        string = "+=";
        space_before = true; space_after = true;
        break;

    case ES_Token::SUBTRACT_ASSIGN:
        string = "-=";
        space_before = true; space_after = true;
        break;

    case ES_Token::MULTIPLY_ASSIGN:
        string = "*=";
        space_before = true; space_after = true;
        break;

    case ES_Token::DIVIDE_ASSIGN:
        string = "/=";
        space_before = true; space_after = true;
        break;

    case ES_Token::REMAINDER_ASSIGN:
        string = "%=";
        space_before = true; space_after = true;
        break;

    case ES_Token::BITWISE_AND_ASSIGN:
        string = "&=";
        space_before = true; space_after = true;
        break;

    case ES_Token::BITWISE_OR_ASSIGN:
        string = "|=";
        space_before = true; space_after = true;
        break;

    case ES_Token::BITWISE_XOR_ASSIGN:
        string = "^=";
        space_before = true; space_after = true;
        break;

    case ES_Token::STRICT_EQUAL:
        string = "===";
        space_before = true; space_after = true;
        break;

    case ES_Token::STRICT_NOT_EQUAL:
        string = "!==";
        space_before = true; space_after = true;
        break;

    case ES_Token::SHIFT_UNSIGNED_RIGHT:
        string = ">>>";
        space_before = true; space_after = true;
        break;

    case ES_Token::SHIFT_LEFT_ASSIGN:
        string = "<<=";
        space_before = true; space_after = true;
        break;

    case ES_Token::SHIFT_SIGNED_RIGHT_ASSIGN:
        string = ">>=";
        space_before = true; space_after = true;
        break;

    case ES_Token::SHIFT_UNSIGNED_RIGHT_ASSIGN:
        string = ">>>=";
        space_before = true; space_after = true;
        break;

    default:
        OP_ASSERT(!"Unknown punctuator!");
        break;
    }
}


template <typename T> inline void
ES_Formatter::AppendInternal(T string, bool brace)
{
    FlushScheduledInternal();
    IndentInternal(brace);
    appender->Append(string);
    last_char_is_linebreak = false;
    can_append_linebreak = GetAllowLinebreak();
}


inline void
ES_Formatter::LinebreakInternal()
{
    is_linebreak_scheduled = false;
    if (!can_append_linebreak)
        return;

    appender->Append('\n');
    is_space_scheduled = false;
    last_char_is_linebreak = true;
}


inline void
ES_Formatter::SpaceInternal()
{
    appender->Append(' ');
    is_space_scheduled = false;
    last_char_is_linebreak = false;
}


inline void
ES_Formatter::FlushScheduledInternal()
{
    if (is_linebreak_scheduled)
        LinebreakInternal();

    if (is_space_scheduled)
        SpaceInternal();
}


inline void
ES_Formatter::IndentInternal(bool is_brace)
{
    if (!last_char_is_linebreak)
        return;

    unsigned wanted_level = is_brace ? state.brace_indent_level : state.indent_level;
    while (wanted_level--)
        appender->Append('\t');
}


inline bool
ES_Formatter::IsNearStackLimit()
{
    unsigned char dummy;
    int stack_used = stack_base - &dummy;
    /* Having a SYSTEM_ setting for stack direction would be preferable. */
    if (stack_used < 0)
        stack_used = -stack_used;
    return stack_used >= (ES_CARAKAN_PARM_MAX_PARSER_STACK - ES_MINIMUM_STACK_REMAINING);
}


#endif /* ES_FORMATTER_INLINES_H */
