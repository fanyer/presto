/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2010
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */
#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_stmt.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_expr.h"
#include "modules/ecmascript/carakan/src/compiler/es_native.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser_inlines.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"
#include "modules/util/opstring.h"

#define PARSE_FAILED(code) do { automatic_error_code = code; return FALSE; } while (0)

ES_Parser::ES_Parser(ES_Lexer *lex, ES_Runtime *runtime, ES_Context *context, ES_Global_Object *global_object, bool is_eval)
    : current_arena(&main_arena),
      runtime(runtime),
      context(context),
      gclock(context),
      global_object(global_object),
      strings_table(NULL),
      long_string_literal_table(NULL),
      ident_arguments(context->rt_data->idents[ESID_arguments]),
      ident_eval(context->rt_data->idents[ESID_eval]),
      current_debug_name(NULL),
      url(NULL),
      current_debug_name_expr(NULL),
      script_type(SCRIPT_TYPE_UNKNOWN),
      is_global_scope(true),
      generate_result(false),
      allow_return_in_program(false),
      allow_in_expr(true),
      allow_linebreak(true),
      linebreak_seen(false),
      uses_arguments(false),
      uses_eval(false),
      is_eval(!!is_eval),
      is_simple_eval(false),
      is_strict_eval(false),
      is_strict_mode(false),
      is_function_eval(false),
      in_function(0),
      in_with(0),
      register_frame_offset(0),
      closures(NULL),
      closures_count(0),
      inner_scopes(NULL),
      inner_scopes_count(0),
      expression_stack_used(0),
      expression_stack_total(0),
      expression_stack(0),
      statement_stack_used(0),
      statement_stack_total(0),
      statement_stack(0),
      function_stack_used(0),
      function_stack_total(0),
      function_stack(0),
      function_data_stack_used(0),
      function_data_stack_total(0),
      function_data_index(0),
      function_nesting_depth(0),
      function_data_stack(0),
      identifier_stack_used(0),
      identifier_stack_total(0),
      identifier_stack(0),
      property_stack_used(0),
      property_stack_total(0),
      property_stack(0),
      lexer(lex),
      program_lexer(context, &main_arena),
      in_typeof(0),
      source_string_owner(NULL),
      source_string(NULL),
      error_code(NO_ERROR),
      automatic_error_code(NO_ERROR),
      error_message(NULL),
      error_identifier(NULL),
      error_index(0),
      error_line(1),
      global_index_offset(0)
#ifdef ECMASCRIPT_DEBUGGER
    , is_debugging(FALSE)
#endif // ECMASCRIPT_DEBUGGER
    , script_guid(ESRT::GetGlobalUniqueScriptId(g_esrt))
    , program_reaper(NULL)
    , stack_base(NULL)
{
    if (lexer)
    {
        lexer->SetArena(current_arena);
        lexer->SetParser(this);
    }
}


ES_Parser::ES_Parser(ES_Execution_Context *context, ES_Global_Object *global_object, BOOL is_eval)
    : current_arena(&main_arena),
      runtime(NULL), // FIXME
      context(context),
      gclock(context),
      global_object(global_object),
      strings_table(NULL),
      long_string_literal_table(NULL),
      ident_arguments(context->rt_data->idents[ESID_arguments]),
      ident_eval(context->rt_data->idents[ESID_eval]),
      current_debug_name(NULL),
      url(NULL),
      current_debug_name_expr(NULL),
      script_type(SCRIPT_TYPE_UNKNOWN),
      is_global_scope(true),
      generate_result(false),
      allow_return_in_program(false),
      allow_in_expr(true),
      allow_linebreak(true),
      linebreak_seen(false),
      uses_arguments(false),
      uses_eval(false),
      is_eval(!!is_eval),
      is_simple_eval(false),
      is_strict_eval(false),
      is_strict_mode(false),
      is_function_eval(!!is_eval),
      in_function(0),
      in_with(0),
      register_frame_offset(0),
      closures(NULL),
      closures_count(0),
      inner_scopes(NULL),
      inner_scopes_count(0),
      expression_stack_used(0),
      expression_stack_total(0),
      expression_stack(0),
      statement_stack_used(0),
      statement_stack_total(0),
      statement_stack(0),
      function_stack_used(0),
      function_stack_total(0),
      function_stack(0),
      function_data_stack_used(0),
      function_data_stack_total(0),
      function_data_index(0),
      function_nesting_depth(0),
      function_data_stack(0),
      identifier_stack_used(0),
      identifier_stack_total(0),
      identifier_stack(0),
      property_stack_used(0),
      property_stack_total(0),
      property_stack(0),
      lexer(NULL),
      program_lexer(context, &main_arena),
      in_typeof(0),
      source_string_owner(NULL),
      source_string(NULL),
      error_code(NO_ERROR),
      automatic_error_code(NO_ERROR),
      error_message(NULL),
      error_index(0),
      error_line(1),
      global_index_offset(0)
#ifdef ECMASCRIPT_DEBUGGER
    , is_debugging(FALSE)
#endif // ECMASCRIPT_DEBUGGER
    , script_guid(ESRT::GetGlobalUniqueScriptId(g_esrt))
    , program_reaper(NULL)
    , stack_base(NULL)
{
    program_lexer.SetParser(this);
}


ES_Parser::~ES_Parser()
{
    OP_DELETEA(expression_stack);
    OP_DELETEA(statement_stack);
    OP_DELETEA(function_stack);
    OP_DELETEA(function_data_stack);
    OP_DELETEA(identifier_stack);
    OP_DELETEA(static_cast<ES_ObjectLiteralExpr::Property *>(property_stack));

    ReleaseRegExpLiterals();

    ES_StaticStringData::DecRef(source_string_owner);
    ES_ProgramCodeStaticReaper::DecRef(program_reaper);
}


BOOL
ES_Parser::ParseProgram(ES_ProgramCode *&code, bool allow_top_level_function_expr)
{
    Initialize(TRUE);

    source_string_owner = OP_NEW_L(ES_StaticSourceData, ());
    source_string = source_string_owner->storage = lexer->GetSourceStringStorage();
    source_string_owner->document_line = lexer->GetDocumentLine();
    source_string_owner->document_column = lexer->GetDocumentColumn();

    if (!context->heap->AddStaticStringData(context, source_string_owner))
        LEAVE(OpStatus::ERR_NO_MEMORY);

    if (ParseSourceElements(false, NULL, allow_top_level_function_expr))
    {
        ES_Compiler compiler(runtime, context, global_object, ES_Compiler::CODETYPE_GLOBAL, register_frame_offset);
        ANCHOR(ES_Compiler, compiler);

        compiler.SetParser(this);

        unsigned functions_count = function_stack_used;
        ES_FunctionCode **functions = PopFunctions ();

        unsigned statements_count = statement_stack_used;
        ES_Statement **statements = PopStatements ();

        ES_ProgramCode *program_code;

        if (!is_global_scope)
            compiler.SetHasOuterScopeChain(TRUE);

        if (is_strict_mode)
            compiler.SetIsStrictMode();

        BOOL success = compiler.CompileProgram(functions_count, functions, statements_count, statements, generate_result, program_code);
        if (!success)
            goto compile_error;

#ifdef ECMASCRIPT_DEBUGGER
        program_code->data->has_debug_code = GetIsDebugging();
#endif // ECMASCRIPT_DEBUGGER

        program_code->url = url;
        program_code->data->source.Set(source_string, 0, 1, 0, UINT_MAX, script_guid);

        program_code->data->source_storage_owner = ES_StaticSourceData::IncRef(source_string_owner);
        program_code->data->source_storage = source_string;

        if (long_string_literal_table && long_string_literal_table->GetCount() != 0)
            program_code->long_string_literal_table = long_string_literal_table;

        OP_ASSERT(functions_count == program_code->GetData()->functions_count);
        program_reaper->Initialize(program_code->GetData());

        code = program_code;
        return TRUE;
    }

    // both parse_error and compile_error conclude here:
compile_error:
    return FALSE;
}


class ES_DeclaresNoLocals
    : public ES_StatementVisitor
{
public:
    ES_DeclaresNoLocals()
        : result(TRUE)
    {
    }

    virtual BOOL Visit(ES_Expression *expression) { return result; }
    virtual BOOL Enter(ES_Statement *statement, BOOL &skip)
    {
        if (statement->GetType() == ES_Statement::TYPE_VARIABLEDECL)
            result = FALSE;
        return result;
    }
    virtual BOOL Leave(ES_Statement *statement) { return result; }

    BOOL result;
};


class ES_StoreFunctionCode
    : public ES_StatementVisitor
{
public:
    ES_StoreFunctionCode(unsigned function_index, ES_FunctionCode *function_code)
        : function_index(function_index),
          function_code(function_code),
          found(FALSE)
    {
    }

    virtual BOOL Visit(ES_Expression *expression)
    {
        if (expression->GetType() == ES_Expression::TYPE_FUNCTION)
        {
            ES_FunctionExpr *fnexpr = static_cast<ES_FunctionExpr *>(expression);
            if (fnexpr->GetFunctionIndex() == function_index)
            {
                fnexpr->SetFunctionCode(function_code);
                found = TRUE;
            }
        }
        return !found;
    }
    virtual BOOL Enter(ES_Statement *statement, BOOL &skip) { return !found; }
    virtual BOOL Leave(ES_Statement *statement) { return !found; }

    unsigned function_index;
    ES_FunctionCode *function_code;
    BOOL found;
};


BOOL
ES_Parser::ParseProgram(ES_Code *&code, JString *program_string, ES_Value_Internal *value, bool strict_mode)
{
    source_string = JStringStorage::Make(context, program_string);

    if (source_string != program_string->value)
        program_string = JString::Make(context, source_string, 0, Length(program_string));

    const uni_char *program = Storage(context, program_string);
    unsigned program_length = Length(program_string);

    program_text = program;
    program_text_length = program_length;
    program_fragments.fragments = &program_text;
    program_fragments.fragment_lengths = &program_text_length;
    program_fragments.fragments_count = 1;

    program_lexer.SetSource(&program_fragments, program_string);

    lexer = &program_lexer;

    Initialize();

    if (strict_mode)
        SetIsStrictMode(strict_mode);

    if (value)
    {
        program_lexer.SetSkipLinebreaks(TRUE);

        if (NextToken() && ParseLiteral(value))
            return TRUE;
        else
        {
            program_lexer.SetSkipLinebreaks(FALSE);
            program_lexer.SetSource(&program_fragments, program_string);
        }
    }

    if (ParseSourceElements(false))
    {
        unsigned statements_count = statement_stack_used;
        ES_Statement **statements = PopStatements ();

        unsigned functions_count = function_data_stack_used;
        FunctionData *functions = PopFunctionData (functions_count);

        if (is_strict_eval)
        {
            ES_Compiler compiler(runtime, context, global_object, ES_Compiler::CODETYPE_STRICT_EVAL, 0);
            ANCHOR(ES_Compiler, compiler);

            compiler.SetParser(this);

            if (inner_scopes_count != 0 || closures_count != 0)
                compiler.SetHasOuterScopeChain(TRUE);

            ES_FunctionCode *function_code;

            compiler.SetIsStrictMode();

            if (!compiler.CompileFunction(NULL, NULL, 0, NULL, functions_count, functions, statements_count, statements, function_code))
                return false;

            code = function_code;
        }
        else
        {
            ES_Compiler compiler(runtime, context, global_object, is_eval ? ES_Compiler::CODETYPE_EVAL : ES_Compiler::CODETYPE_GLOBAL, register_frame_offset);
            ANCHOR(ES_Compiler, compiler);

            compiler.SetParser(this);
            compiler.SetClosures(closures, closures_count);
            compiler.SetInnerScope(inner_scopes, inner_scopes_count);

            if (functions_count)
            {
                OP_ASSERT(is_simple_eval);

                for (unsigned function_index = 0; function_index < functions_count; ++function_index)
                    if (!functions[function_index].is_function_expr)
                    {
                        is_simple_eval = FALSE;
                        break;
                    }

                if (is_simple_eval)
                {
                    ES_DeclaresNoLocals checker;
                    ES_Statement::VisitStatements(statements, statements_count, &checker);
                    is_simple_eval = !!checker.result;
                }

                for (unsigned index = 0; index < functions_count; ++index)
                {
                    if (!CompileFunction (functions[index]))
                        goto compile_error;

                    if (functions[index].is_function_expr)
                    {
                        ES_StoreFunctionCode store_function_code(index, function_stack[function_stack_used - 1]);
                        ES_Statement::VisitStatements(statements, statements_count, &store_function_code);

                        OP_ASSERT(store_function_code.found);
                    }
                }
            }

            unsigned functions_count = function_stack_used;
            ES_FunctionCode **functions = PopFunctions ();

            ES_ProgramCode *program_code;

            if (!is_global_scope)
                compiler.SetHasOuterScopeChain(TRUE);

            if (is_strict_mode)
                compiler.SetIsStrictMode();

            BOOL success = compiler.CompileProgram(functions_count, functions, statements_count, statements, generate_result, program_code);
            if (!success)
                goto compile_error;

            code = program_code;
        }

        code->url = url;
        code->data->source.Set(source_string, 0, 1, 0, UINT_MAX, script_guid);

        code->data->source_storage_owner = ES_StaticSourceData::IncRef(source_string_owner);
        code->data->source_storage = source_string;

#ifdef ECMASCRIPT_DEBUGGER
        code->data->has_debug_code = GetIsDebugging();
#endif // ECMASCRIPT_DEBUGGER

        return TRUE;
    }

// both parse_error and compile_error conclude here:
compile_error:
    return FALSE;
}


BOOL
ES_Parser::ParseFunction(ES_FunctionCode *&code, const uni_char *formals, unsigned formals_length, const uni_char *body, unsigned body_length, ES_ParseErrorInfo *error_info)
{
    ES_Fragments fformals(&formals, &formals_length);
    ES_Lexer lformals(context, current_arena, &fformals);
    ANCHOR(ES_Lexer, lformals);

    lexer = &lformals;

    lexer->SetParser(this);

    Initialize();

    unsigned source_length = 25 + formals_length + body_length;

    source_string = JStringStorage::Make(context, static_cast<const char *>(NULL), source_length + 1, source_length);

    uni_char *storage = source_string->storage;

    uni_strcpy(storage, UNI_L("function anonymous("));
    uni_strncat(storage, formals, formals_length);
    uni_strcat(storage, UNI_L(") {\n"));

    global_index_offset = uni_strlen(storage);

    uni_strncat(storage, body, body_length);
    uni_strcat(storage, UNI_L("\n}"));

    if (!NextToken() || !ParseFormalParameterList() || token.type != ES_Token::END)
    {
        if (error_info)
            GetError(*error_info);
        return FALSE;
    }

    ES_Fragments fbody(&body, &body_length);
    ES_Lexer lbody(context, current_arena, &fbody);
    ANCHOR(ES_Lexer, lbody);

    ++in_function;

    lexer = &lbody;

    lexer->SetStringsTable(strings_table);
    lexer->SetParser(this);

    if (ParseSourceElements(false) && token.type == ES_Token::END)
    {
        --in_function;

        if (CompileFunction(NULL, identifier_stack_used, function_data_stack_used, statement_stack_used, 0, 0, 0, source_length, ES_SourceLocation(0, 1), ES_SourceLocation(), NULL, FALSE, NULL, 0, !is_global_scope))
        {
            code = PopFunction();
            code->PrepareForExecution(context);
            return TRUE;
        }
    }

    if (error_info)
        GetError(*error_info);
    return FALSE;
}

BOOL
ES_Parser::ParseFunction(ES_FunctionCode *&code, const uni_char** formals, unsigned nformals, unsigned *formals_length, const uni_char *body, unsigned body_length, ES_ParseErrorInfo *error_info)
{
    unsigned total_length = nformals == 0 ? 0 : (nformals - 1) * 2, i;

    for (i = 0; i < nformals; ++i)
        total_length += formals_length[i];

    uni_char *all_formals = OP_NEWA_L(uni_char, total_length + 1), *ptr = all_formals;
    ANCHOR_ARRAY(uni_char, all_formals);

    for (i = 0; i < nformals; ++i)
    {
        uni_strncpy(ptr, formals[i], formals_length[i]);
        ptr += formals_length[i];

        if (i + 1 < nformals)
        {
            uni_strcpy(ptr, UNI_L(", "));
            ptr += 2;
        }
    }

    return ParseFunction(code, all_formals, total_length, body, body_length, error_info);
}

#ifdef ES_BYTECODE_DEBUGGER

/* static */ BOOL
ES_Parser::ParseDebugCode(ES_Context *context, ES_FunctionCode *&code, ES_Object *global_object, const uni_char *source, unsigned source_length)
{
    ES_Fragments fragments(&source, &source_length);
    ES_Lexer lexer(context, &fragments);
    ES_Parser parser(&lexer, NULL, context, static_cast<ES_Global_Object *> (global_object));

    if (parser.ParseSourceElements(false) && parser.CompileFunction(NULL, 0, parser.function_stack_used, parser.statement_stack_used, UINT_MAX, UINT_MAX, UINT_MAX, NULL, NULL, 0, TRUE, TRUE))
    {
        code = parser.PopFunction();
        return TRUE;
    }

    return FALSE;
}

#endif // ES_BYTECODE_DEBUGGER


bool
ES_Parser::ParseFunction ()
{
    if (NextToken () && ParseFunctionExpr (false))
        if (token.type == ES_Token::END)
            return true;
        else
            error_code = UNEXPECTED_TOKEN;

    return false;
}


void
ES_Parser::GetError (ES_ParseErrorInfo &info)
{
    if (error_code == NO_ERROR)
        error_code = automatic_error_code;

    if (error_code == CUSTOM_ERROR)
        info.message = error_message;
    else
    {
        const char *message8;
        BOOL append_token = TRUE, append_identifier = FALSE;

        switch (error_code)
        {
        default:
        case NO_ERROR:
            OP_ASSERT(FALSE);

        case GENERIC_ERROR:
            message8 = "syntax error";
            append_token = FALSE;
            break;

        case EXPECTED_LVALUE:
            message8 = "invalid left-hand side in assignment";
            append_token = FALSE;
            break;

        case EXPECTED_SEMICOLON:
            message8 = "expected ';', got ";
            break;

        case EXPECTED_RIGHT_BRACE:
            message8 = "expected '}', got ";
            break;

        case EXPECTED_RIGHT_BRACKET:
            message8 = "expected ']', got ";
            break;

        case EXPECTED_RIGHT_PAREN:
            message8 = "expected ')', got ";
            break;

        case EXPECTED_IDENTIFIER:
            message8 = "expected identifier, got ";
            break;

        case EXPECTED_EXPRESSION:
            message8 = "expected expression, got ";
            break;

        case UNEXPECTED_TOKEN:
            message8 = "unexpected token ";
            break;

        case INVALID_RETURN:
            message8 = "return outside function definition";
            append_token = FALSE;
            break;

        case ILLEGAL_BREAK:
            message8 = "illegal 'break' outside a switch or iteration statement";
            append_token = FALSE;
            break;

        case UNBOUND_BREAK:
            message8 = "unknown label in 'break' statement";
            append_token = FALSE;
            break;

        case ILLEGAL_CONTINUE:
            message8 = "illegal 'continue' outside an iteration statement";
            append_token = FALSE;
            break;

        case UNBOUND_CONTINUE:
            message8 = "unknown label in 'continue' statement";
            append_token = FALSE;
            break;

        case TRY_WITHOUT_CATCH_OR_FINALLY:
            message8 = "missing catch or finally clause in try statement";
            append_token = FALSE;
            break;

        case DUPLICATE_LABEL:
            message8 = "duplicate label";
            append_token = FALSE;
            break;

        case BOTH_GETTER_AND_PROPERTY:
            message8 = "both property and its getter defined";
            append_token = FALSE;
            break;

        case BOTH_SETTER_AND_PROPERTY:
            message8 = "both property and its setter defined";
            append_token = FALSE;
            break;

        case DUPLICATE_DATA_PROPERTY:
            message8 = "multiple definitions of data property";
            append_token = FALSE;
            break;

        case DUPLICATE_GETTER:
            message8 = "redefinition of getter";
            append_token = FALSE;
            break;

        case DUPLICATE_SETTER:
            message8 = "redefinition of setter";
            append_token = FALSE;
            break;

        case DUPLICATE_DEFAULT:
            message8 = "multiple default cases in switch statement";
            append_token = FALSE;
            break;

        case INPUT_TOO_COMPLEX:
            message8 = "input too deeply nested";
            append_token = FALSE;
            break;

        case WITH_IN_STRICT_MODE:
            message8 = "with statement not allowed in strict mode";
            append_token = FALSE;
            break;

        case INVALID_USE_OF_EVAL:
            message8 = "invalid use of identifier 'eval' in strict mode";
            append_token = FALSE;
            break;

        case INVALID_USE_OF_ARGUMENTS:
            message8 = "invalid use of identifier 'arguments' in strict mode";
            append_token = FALSE;
            break;

        case INVALID_USE_OF_DELETE:
            message8 = "invalid use of 'delete' in strict mode";
            append_token = FALSE;
            break;

        case DUPLICATE_FORMAL_PARAMETER:
            message8 = "identifier occurs more than once in formal parameter list: ";
            append_identifier = TRUE;
            break;

        case FUNCTION_DECL_IN_STATEMENT:
            message8 = "FunctionDeclaration in Statement context";
            append_token = FALSE;
            break;
        }

        const char *token8;

        if (append_token && (token8 = lexer->GetTokenAsCString()) != NULL)
        {
            OpString8 message_string; ANCHOR(OpString8, message_string);

            message_string.SetL(message8);
            message_string.AppendL(token8);

            info.message = JString::Make(context, message_string.CStr());
        }
        else
        {
            info.message = JString::Make(context, message8);

            if (append_token)
                Append(context, info.message, lexer->GetTokenAsJString());
            else if (append_identifier)
                Append(context, info.message, error_identifier);
        }
    }

    info.index = error_index;
    info.line = error_line;
}

void
ES_Parser::SetError(ErrorCode code, const ES_SourceLocation &loc)
{
    if (error_code == NO_ERROR)
    {
        error_code = code;
        error_index = loc.Index();
        error_line = loc.Line();
    }
}

bool
ES_Parser::ValidateIdentifier(JString *identifier, const ES_SourceLocation *location)
{
    if (is_strict_mode)
    {
        ErrorCode code;

        if (identifier->Equals("eval", 4))
            code = INVALID_USE_OF_EVAL;
        else if (identifier->Equals("arguments", 9))
            code = INVALID_USE_OF_ARGUMENTS;
        else
            return true;

        SetError (code, location ? *location : last_token.GetSourceLocation ());
        return false;
    }
    else
        return true;
}

void
ES_Parser::GenerateErrorSourceViewL(OpString &buffer)
{
    // How do we visualize source location? Generally we try to visualize the whole line if it is not too long.
    // lbeg will point to begining of pre-error-context, lend - to the end of post-error-context
    uni_char *lbeg, *lend, *lerr;

    // Some errors are reported few lines later, when an unexpected token is encountered
    // In such case, we should skip back all whitespace and assume this is the right err pos
    lerr = source_string->storage + global_index_offset + error_index;
    if (lerr >= source_string->storage + source_string->length)
        lerr = source_string->storage + source_string->length;
    while (uni_isspace(*lerr))
    {
        if (source_string->storage < lerr)
            lerr--;
        else
        {   // we've skipped back to the very begining of the source;
            // hmmm, this can't be right so get back to original err pos;
            lerr = source_string->storage + error_index;
            break;
        }
    }

    lbeg = lend = lerr;

    unsigned ctx_len = 20;

    for (unsigned i = 0; i < ctx_len; ++i) // no more than 20 preceding chars
        if (lbeg == source_string->storage || lbeg[-1] == '\n')
            break;
        else
            --lbeg;

    for (unsigned i = 0; i < ctx_len; ++i) // no more than 20 succeeding chars
        if (lend == source_string->storage + source_string->length || *lend == '\r' || *lend == '\n')
            break;
        else
            ++lend;

    unsigned nprint = lend - lbeg;

    /* Reserve storage for two lines of 'nprint' characters and two
       line breaks and an arrow marker. The second line is at most 'nprint'
       characters long, but can be shorter. */
    unsigned buffer_length = buffer.Length();
    uni_char *write = buffer.ReserveL(buffer_length + nprint + 1 + nprint + 2) + buffer_length;

    uni_strncpy(write, lbeg, nprint);
    write += nprint;

    *write++ = '\n';

    // This is a bit complicated - to keep alignment in sync with the
    // upper line substitute non-space chars with ' ' while copying
    // space chars verbatim.
    for (; lbeg < lerr; lbeg++)
        *write++ = uni_isspace(*lbeg) ? *lbeg : ' ';

    for (uni_char *t = write - 1; *t == ' '; --t)
        *t = '-';

    *write++ = '^';
    *write++ = '\n';
    *write = 0;
    OP_ASSERT(write < buffer.CStr() + buffer.Capacity() * sizeof(uni_char));
}


ES_SourcePosition
ES_Parser::GetErrorPosition(unsigned index, unsigned line)
{
    unsigned column = ES_Lexer::GetColumn(source_string->storage, global_index_offset + index);
    unsigned doc_line = source_string_owner ? source_string_owner->document_line : 1;
    unsigned doc_column = source_string_owner ? source_string_owner->document_column : 0;

    return ES_SourcePosition(index, line, column, doc_line, doc_column);
}


void
ES_Parser::Initialize(BOOL parsing_program)
{
#ifdef ES_NATIVE_SUPPORT
#ifdef USE_CUSTOM_DOUBLE_OPS
    AddDoubles = ES_Native::CreateAddDoubles();
    SubtractDoubles = ES_Native::CreateSubtractDoubles();
    MultiplyDoubles = ES_Native::CreateMultiplyDoubles();
    DivideDoubles = ES_Native::CreateDivideDoubles();
#endif // USE_CUSTOM_DOUBLE_OPS
#endif // ES_NATIVE_SUPPORT

    lexer->SetStringsTable(strings_table = ES_Identifier_List::Make(context, 256));

    if (parsing_program && lexer->GetSource()->length > 1024)
        lexer->SetLongStringLiteralTable(long_string_literal_table = ES_Identifier_Hash_Table::Make(context, 16));

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

    if (parsing_program)
        program_reaper = ES_ProgramCodeStaticReaper::IncRef(OP_NEW_L(ES_ProgramCodeStaticReaper, ()));

    stack_base = reinterpret_cast<unsigned char *>(&index);
    OP_ASSERT(ES_MINIMUM_STACK_REMAINING < ES_CARAKAN_PARM_MAX_PARSER_STACK);
}


bool
ES_Parser::NextToken ()
{
    last_token = token;

    lexer->NextToken ();
    token = lexer->GetToken ();

    error_index = token.start;
    error_line = token.line;

    if (token.type == ES_Token::INVALID)
    {
        error_code = CUSTOM_ERROR;
        error_message = token.value.GetString ();

        return FALSE;
    }

    linebreak_seen = token.type == ES_Token::LINEBREAK;
    return true;
}


bool
ES_Parser::ParseProperty (JString *&i, bool opt)
{
    ES_Value_Internal v;
    bool regexp;

    if (!ParseIdentifier (i, opt, true))
        if (ParseLiteral (v, regexp, opt))
            if (regexp)
                PARSE_FAILED (GENERIC_ERROR);
            else
                switch (v.Type ())
                {
                case ESTYPE_INT32:
                    i = tostring (context, v.GetNumAsDouble ());
                    break;

                case ESTYPE_DOUBLE:
                    i = lexer->MakeSharedString (tostring (context, v.GetNumAsDouble ()));
                    break;

                case ESTYPE_STRING:
                    i = v.GetString ();
                    break;

                default:
                    PARSE_FAILED (GENERIC_ERROR);
                }
        else
            PARSE_FAILED (GENERIC_ERROR);

    return true;
}


bool
ES_Parser::ParseLiteral(ES_Value_Internal &v, bool &regexp, bool opt)
{
    if (!HandleLinebreak ())
        return false;

    v.SetUndefined ();

    if (token.type != ES_Token::LITERAL)
    {
        lexer->RevertTokenRegexp ();
        token = lexer->GetToken ();

        if (token.type != ES_Token::LITERAL)
        {
            if (token.type == ES_Token::INVALID)
            {
                error_index = token.start;
                error_line = token.line;

                error_code = CUSTOM_ERROR;
                error_message = token.value.GetString ();

                return false;
            }

            return opt;
        }
        else
            regexp = true;
    }
    else
        regexp = false;

    v = token.value;

    if (!NextToken ())
        return false;

    return true;
}


bool
ES_Parser::ParseSemicolon (bool opt)
{
    if (token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::SEMICOLON)
        return NextToken ();
    else if (opt || linebreak_seen || token.type == ES_Token::LINEBREAK || token.type == ES_Token::END || (token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::RIGHT_BRACE))
        return true;
    else
    {
        automatic_error_code = EXPECTED_SEMICOLON;

        return false;
    }
}


bool
ES_Parser::ParseSourceElements(bool is_body, ES_SourceLocation *lbrace_location, bool allow_top_level_function_expr)
{
    if (!is_strict_mode)
    {
        ES_Token saved_token(token);
        ES_Lexer::State saved_state(lexer);

        if (!(is_body ? ParsePunctuator(ES_Token::LEFT_BRACE, *lbrace_location) : NextToken()) || !HandleLinebreak())
            return false;

        while (token.type == ES_Token::LITERAL && token.value.IsString())
        {
            bool contained_escapes = token.contained_escapes;
            JString *string = token.value.GetString();

            if (!NextToken() || !HandleLinebreak())
                break;

            /* A string literal is only a part of a directive prologue if it is
               followed by a semicolon, possibly an automatically inserted one. */
            if (token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::SEMICOLON)
            {
                if (!NextToken() || !HandleLinebreak())
                    break;
            }
            else if (linebreak_seen)
            {
                /* A semicolon is inserted after a linebreak, unless followed by
                   something that combines with the string literal to form an
                   expression (possibly an invalid one.) */
                if (token.type == ES_Token::PUNCTUATOR)
                    switch (token.punctuator)
                    {
                    case ES_Token::LEFT_BRACE:
                    case ES_Token::RIGHT_BRACE:
                    case ES_Token::LOGICAL_NOT:
                    case ES_Token::BITWISE_NOT:
                    case ES_Token::INCREMENT:
                    case ES_Token::DECREMENT:
                    case ES_Token::SINGLE_LINE_COMMENT:
                    case ES_Token::MULTI_LINE_COMMENT:
                        break;

                    default:
                        goto finished;
                    }
            }
            /* In a function body a semicolon would be inserted before the
               ending right brace, otherwise no semicolon would be inserted,
               and the string literal is not a valid directive. */
            else if (!is_body || token.type != ES_Token::PUNCTUATOR || token.punctuator != ES_Token::RIGHT_BRACE)
                break;

            if (!contained_escapes && string->Equals("use strict", 10))
                SetIsStrictMode(true);
        }

    finished:
        saved_state.Restore();
        token = saved_token;
    }

    if (!(is_body ? ParsePunctuator(ES_Token::LEFT_BRACE, *lbrace_location) : NextToken()) || !HandleLinebreak())
        return false;

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

    return true;
}


class LiteralParseStack
{
public:
    LiteralParseStack()
        : object(NULL),
          array(NULL),
          previous_class(NULL),
          name(NULL),
          property_index(0),
          array_index(0),
          previous(NULL)
    {
    }

    ES_Object *object;
    ES_Object *array;
    ES_Class  *previous_class;
    JString *name;
    unsigned property_index, array_index;

    LiteralParseStack *previous;
};

bool
ES_Parser::ParseLiteral(ES_Value_Internal *value)
{
#define NEXTC() (NextToken())
#define NEXT() do { if (!NEXTC()) return false; } while (0)
#define POP() do { if (stack) { name = stack->name; property_index = stack->property_index; array_index = stack->array_index; previous_class = stack->previous_class; if (stack->object) { in_object = TRUE; current = stack->object; } else { in_object = FALSE; current = stack->array; } LiteralParseStack *previous = stack->previous; stack->previous = free_list; free_list = stack; stack = previous; goto put_property; } else goto finalize; } while (0)

    BOOL has_outer_paren = token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::LEFT_PAREN;
    ES_Class *object_class = global_object->GetObjectClass(), *previous_class = NULL;
    ES_Object *current = NULL, *empty_object = NULL;

    LiteralParseStack stack_block[16];
    LiteralParseStack *free_list = NULL;

    for (unsigned i = 0; i < sizeof(stack_block) / sizeof(stack_block[0]); i++)
    {
        stack_block[i].previous = free_list;
        free_list = &stack_block[i];
    }

    if (has_outer_paren && !NEXTC())
        return false;

    BOOL in_object;

    if (has_outer_paren && token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::LEFT_BRACE)
    {
        NEXT();
        in_object = TRUE;
        current = ES_Object::Make(context, object_class);
    }
    else if (token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::LEFT_BRACKET)
    {
        NEXT();
        in_object = FALSE;
        current = ES_Array::Make(context, global_object);
    }
    else if (token.type == ES_Token::LITERAL)
    {
        NEXT();

        if (has_outer_paren)
            if (token.type != ES_Token::PUNCTUATOR || token.punctuator != ES_Token::RIGHT_PAREN || !NEXTC())
                return false;

        if (token.type != ES_Token::END)
            return false;

        *value = token.value;
        return true;
    }
    else
        return false;

    LiteralParseStack *stack = NULL;
    JString *name = NULL;
    unsigned property_index = 0, array_index = 0;
    BOOL first = TRUE;

    while (true)
    {
        if (in_object)
        {
            if (!first)
                if (token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::COMMA)
                    NEXT();
                else if (token.type != ES_Token::PUNCTUATOR || token.punctuator != ES_Token::RIGHT_BRACE)
                    return false;

            first = false;

            if (token.type == ES_Token::IDENTIFIER)
                name = token.identifier;
            else if (token.type == ES_Token::LITERAL)
                if (token.value.IsInt32() && token.value.GetNumAsInt32() >= 0)
                {
                    array_index = token.value.GetNumAsInt32();
                    name = NULL;
                }
                else if (token.value.IsString())
                {
                    name = token.value.GetString ();

                    if (convertindex(Storage(context, name), Length(name), array_index))
                        name = NULL;
                }
                else
                    return false;
            else if (token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::RIGHT_BRACE)
            {
                value->SetObject(current);

                if (current->Class() != object_class)
                    previous_class = current->Class();

                NEXT();
                POP();
            }
            else
                return false;

            NEXT();

            if (token.type != ES_Token::PUNCTUATOR || token.punctuator != ES_Token::COLON)
                return false;

            NEXT();

            if (token.type == ES_Token::PUNCTUATOR)
                if (token.punctuator == ES_Token::LEFT_BRACE || token.punctuator == ES_Token::LEFT_BRACKET)
                {
                    if (!free_list)
                        return false;
                    LiteralParseStack *stack_previous = stack;
                    stack = free_list;
                    free_list = free_list->previous;
                    stack->previous = stack_previous;
                    stack->object = current;
                    stack->previous_class = previous_class;
                    stack->name = name;
                    stack->property_index = property_index;
                    stack->array_index = array_index;

                    name = NULL;
                    property_index = 0;
                    array_index = 0;
                    previous_class = NULL;

                    if (token.punctuator == ES_Token::LEFT_BRACE)
                    {
                        if (empty_object)
                        {
                            current = empty_object;
                            empty_object = NULL;
                        }
                        else
                            current = ES_Object::Make(context, object_class);
                    }
                    else
                    {
                        current = ES_Array::Make(context, global_object);
                        in_object = FALSE;
                    }

                    NEXT();

                    first = TRUE;
                    continue;
                }
                else
                    return false;
            else if (token.type == ES_Token::LITERAL)
            {
                *value = token.value;
                NEXT();
            }
            else
                return false;
        }
        else
        {
            if (!first)
                if (token.type == ES_Token::PUNCTUATOR && token.punctuator == ES_Token::COMMA)
                {
                    NEXT();
                    ++array_index;
                }
                else if (token.type != ES_Token::PUNCTUATOR || token.punctuator != ES_Token::RIGHT_BRACKET)
                    return false;

            if (token.type == ES_Token::PUNCTUATOR)
                if (token.punctuator == ES_Token::LEFT_BRACE || token.punctuator == ES_Token::LEFT_BRACKET)
                {
                    if (!free_list)
                        return false;
                    LiteralParseStack *stack_previous = stack;
                    stack = free_list;
                    free_list = free_list->previous;
                    stack->previous = stack_previous;
                    stack->array = current;
                    stack->previous_class = previous_class;
                    stack->name = name;
                    stack->property_index = property_index;
                    stack->array_index = array_index;

                    name = NULL;
                    property_index = 0;
                    array_index = 0;
                    previous_class = NULL;

                    if (token.punctuator == ES_Token::LEFT_BRACE)
                    {
                        if (empty_object)
                        {
                            current = empty_object;
                            empty_object = NULL;
                        }
                        else
                            current = ES_Object::Make(context, object_class);

                        in_object = TRUE;
                    }
                    else
                        current = ES_Array::Make(context, global_object);

                    NEXT();

                    first = TRUE;
                    continue;
                }
                else if (token.punctuator == ES_Token::RIGHT_BRACKET)
                {
                    ES_Array::SetLength(context, current, first ? 0 : array_index + 1);
                    first = FALSE;
                    value->SetObject(current);

                    NEXT();
                    POP();
                }
                else
                    return false;
            else if (token.type == ES_Token::LITERAL)
            {
                *value = token.value;
                NEXT();
            }
            else
                return false;

            first = FALSE;
        }

    put_property:
        if (name)
        {
            if (previous_class && property_index != UINT_MAX)
            {
                if (property_index < previous_class->CountProperties() && ((name == previous_class->GetNameAtIndex(ES_PropertyIndex(property_index))) || name->Equals(previous_class->GetNameAtIndex(ES_PropertyIndex(property_index)))))
                {
                    if (property_index == 0)
                    {
                        empty_object = current;
                        current = ES_Object::Make(context, previous_class);
                    }

                    current->PutCachedAtIndex(ES_PropertyIndex(property_index++), *value);
                    continue;
                }
                else if (property_index != 0)
                {
                    ES_Class *klass = current->Class();
                    if (property_index != klass->Count())
                    {
                        do
                            klass = klass->GetParent();
                        while (klass->Count() > property_index);

                        current->SetClass(klass);

                        property_index = UINT_MAX;
                    }
                }
                else
                    property_index = UINT_MAX;
            }

            if (is_strict_mode && current->HasProperty(context, name))
                return FALSE;

            current->PutNativeL(context, name, 0, *value);
        }
        else
        {
            if (is_strict_mode && current->HasProperty(context, array_index))
                return FALSE;

            ES_Indexed_Properties::PutL(context, current, array_index, *value, current);
        }
    }

finalize:
    if (has_outer_paren)
    {
        if (token.type != ES_Token::PUNCTUATOR || token.punctuator != ES_Token::RIGHT_PAREN)
            return FALSE;
        NEXT();
    }

    return token.type == ES_Token::END;
#undef NEXTC
#undef NEXT
#undef POP
}


bool
ES_Parser::CompileFunction(JString *function_name, unsigned parameter_names_count, unsigned functions_count, unsigned statements_count, unsigned start_index, unsigned start_line, unsigned start_column, unsigned end_index, ES_SourceLocation start_location, ES_SourceLocation end_location, const FunctionData *data, bool is_function_expr, ES_FunctionCode **scope_chain, unsigned scope_chain_length, BOOL has_outer_scope_chain, bool implicit_return)
{
    JString **parameter_names = data ? data->parameters : PopIdentifiers(parameter_names_count);
    ES_Statement **statements = data ? data->statements : PopStatements(statements_count);

    if (is_strict_mode)
    {
        ES_Identifier_List *table = parameter_names_count > 1 ? ES_Identifier_List::Make(context, parameter_names_count) : NULL;
        ES_SourceLocation location(start_index, start_line);
        unsigned position;

        for (unsigned index = 0; index < parameter_names_count; ++index)
        {
            JString *parameter_name = parameter_names[index];

            if (!ValidateIdentifier(parameter_name, &location))
            {
                error_index = start_index;
                error_line = start_line;

                return false;
            }
            else if (table && !table->AppendL(context, parameter_name, position))
            {
                error_code = DUPLICATE_FORMAL_PARAMETER;
                error_index = start_index;
                error_line = start_line;
                error_identifier = parameter_name;

                return false;
            }
        }

        if (table)
            ES_Identifier_List::Free(context, table);
    }

    if (in_function == 0 && (!is_simple_eval && !is_strict_eval || data))
    {
        FunctionData *functions = data ? data->functions : PopFunctionData (functions_count);

        if (implicit_return && statements_count != 0 && statements[statements_count - 1]->GetType () == ES_Statement::TYPE_EXPRESSION)
        {
            ES_ExpressionStmt *exprstmt = static_cast<ES_ExpressionStmt *> (statements[statements_count - 1]);
            statements[statements_count - 1] = OP_NEWGRO_L(ES_ReturnStmt, (exprstmt->GetExpression()), Arena());
        }

        ES_Compiler compiler(runtime, context, global_object, ES_Compiler::CODETYPE_FUNCTION, register_frame_offset, scope_chain, scope_chain_length);
        ANCHOR(ES_Compiler, compiler);

        compiler.SetParser(this);

        if (inner_scopes_count != 0 || has_outer_scope_chain || (data ? data->inside_inner_scope : in_with))
            compiler.SetHasOuterScopeChain(TRUE);
        else if (function_nesting_depth == 0 && closures_count != 0)
            if (is_simple_eval)
            {
                for (unsigned index = 0; index < closures_count; ++index)
                    closures[index]->ConstructClass(context);

                compiler.SetLexicalScopes(closures, closures_count);
            }
            else
                compiler.SetHasOuterScopeChain(TRUE);
        else if (is_eval && closures_count != 0)
            compiler.SetHasOuterScopeChain(TRUE);
        else if (!is_global_scope)
            compiler.SetHasOuterScopeChain(TRUE);

        compiler.SetUsesArguments(data ? data->uses_arguments || data->uses_eval : uses_arguments || uses_eval); /* eval might use arguments */
        compiler.SetUsesEval(data ? data->uses_eval : uses_eval);

        compiler.SetIsFunctionExpr(is_function_expr);

        ES_FunctionCode *function_code;

        if (++function_nesting_depth > ES_MAXIMUM_FUNCTION_NESTING)
            return false;

        JString *debug_name = NULL;

#if 1
        BOOL emit_debug_information = TRUE;
#else // 1
        /* Skip debug information for large eval:ed programs; it's probably just
           an evil test in the mozilla regression test suite.  :-) */
        BOOL emit_debug_information = !is_eval || lexer->GetSource()->length < 16384;
#endif // 1

        compiler.SetEmitDebugInformation(emit_debug_information);

        if (data)
        {
            start_index = data->start_index;
            start_line = data->start_line;
            start_column = data->start_column;
            end_index = data->end_index;
            start_location = data->start_location;
            end_location = data->end_location;
        }

        if (!function_name)
            if (data)
            {
                if (data->debug_name)
                    debug_name = data->debug_name;
                else if (data->debug_name_expr)
                    debug_name = data->debug_name_expr->AsDebugName(context);
            }
            else if (current_debug_name)
                debug_name = current_debug_name;
            else if (current_debug_name_expr)
                debug_name = current_debug_name_expr->AsDebugName(context);

        if (data ? data->is_strict_mode : is_strict_mode)
            compiler.SetIsStrictMode();

        if (!compiler.CompileFunction(function_name, debug_name, parameter_names_count, parameter_names, functions_count, functions, statements_count, statements, function_code))
            return false;

        function_code->url = url;

        if (emit_debug_information)
            if (start_index != UINT_MAX)
            {
                /* For nested functions, adjust starting index/column by buffer offset. */
                unsigned initial_offset = function_nesting_depth > 1 ? global_index_offset : 0;
                function_code->data->source.Set(source_string, initial_offset + start_index, start_line, start_column, end_index - start_index, script_guid);

                function_code->data->source.SetLocationOffset(global_index_offset);

                function_code->data->source_storage_owner = ES_StaticSourceData::IncRef(source_string_owner);
                function_code->data->source_storage = source_string;

                function_code->data->start_location = start_location;
                function_code->data->end_location = end_location;
            }

#ifdef ECMASCRIPT_DEBUGGER
        function_code->data->has_debug_code = GetIsDebugging();
#endif // ECMASCRIPT_DEBUGGER

        --function_nesting_depth;

        function_code->GetData()->is_function_expr = is_function_expr;

        PushFunction (function_code);
    }
    else
    {
        FunctionData data;

        OP_ASSERT(function_name || is_function_expr);

        data.name = function_name;
        data.debug_name = NULL;
        data.debug_name_expr = NULL;
        if (!function_name)
            if (current_debug_name)
                data.debug_name = current_debug_name;
            else if (current_debug_name_expr)
                data.debug_name_expr = current_debug_name_expr;
        data.parameters = parameter_names;
        data.functions = PopFunctionData (functions_count);
        data.statements = statements;
        data.parameters_count = parameter_names_count;
        data.functions_count = functions_count;
        data.statements_count = statements_count;
        data.start_index = start_index;
        data.start_line = start_line;
        data.start_column = start_column;
        data.end_index = end_index;
        data.start_location = start_location;
        data.end_location = end_location;
        data.is_function_expr = is_function_expr;
        data.uses_arguments = uses_arguments;
        data.uses_eval = uses_eval;
        data.inside_inner_scope = in_with > 0;
        data.is_strict_mode = is_strict_mode;

        ++function_data_index;

        PushFunctionData (data);
    }

    return true;
}


bool
ES_Parser::CompileFunction(const FunctionData &data, ES_FunctionCode **scope_chain, unsigned scope_chain_length, BOOL has_outer_scope_chain)
{
    return CompileFunction(data.name, data.parameters_count, data.functions_count, data.statements_count, 0, 0, 0, 0, ES_SourceLocation(), ES_SourceLocation(), &data, data.is_function_expr, scope_chain, scope_chain_length, has_outer_scope_chain);
}


bool
ES_Parser::CompileFunctions(unsigned functions_count)
{
    FunctionData *data = PopFunctionData (functions_count);

    for (unsigned index = 0; index < functions_count; ++index)
        if (!CompileFunction (data[index]))
            return false;

    return true;
}


bool
ES_Parser::ParseFunctionDecl(bool skip_function_kw, bool allow_top_level_function_expr)
{
    JString *function_name;
    OpMemGroup local_arena, *previous_arena = current_arena;

    ANCHOR(OpMemGroup, local_arena);

    if (!in_function && !is_simple_eval && !is_strict_eval)
        current_arena = &local_arena;

    ++in_function;

    bool old_uses_arguments = uses_arguments;
    bool old_uses_eval = uses_eval;
    bool old_is_strict_mode = is_strict_mode;

    uses_arguments = FALSE;
    uses_eval = FALSE;

    unsigned old_function_data_index = function_data_index;
    function_data_index = 0;

    if (skip_function_kw || ParseKeyword (ES_Token::KEYWORD_FUNCTION))
    {
        unsigned start_index = last_token.start;
        unsigned start_line = last_token.line;
        unsigned start_column = last_token.column;

        if (ParseIdentifier(function_name))
        {
            if (!ValidateIdentifier(function_name))
                return false;

            unsigned identifiers_before = identifier_stack_used;
            unsigned functions_before = function_stack_used;
            unsigned function_data_before = function_data_stack_used;
            unsigned statements_before = statement_stack_used;
            ES_SourceLocation start_location, end_location, function_name_location(last_token.GetSourceLocation());

            if (ParsePunctuator (ES_Token::LEFT_PAREN) &&
                ParseFormalParameterList () &&
                ParsePunctuator (ES_Token::RIGHT_PAREN) &&
                ParseSourceElements (true, &start_location) &&
                ParsePunctuator (ES_Token::RIGHT_BRACE, end_location))
            {
                unsigned parameter_names_count = identifier_stack_used - identifiers_before;
                unsigned functions_count = function_stack_used - functions_before;
                unsigned function_data_count = function_data_stack_used - function_data_before;
                unsigned statements_count = statement_stack_used - statements_before;

                if (!ValidateIdentifier(function_name, &function_name_location))
                    return false;

                OP_ASSERT (functions_count == 0);

                --in_function;

                function_data_index = old_function_data_index;

                if (!CompileFunction (function_name, parameter_names_count, es_maxu (functions_count, function_data_count), statements_count, start_index, start_line, start_column, last_token.end, start_location, end_location, NULL, false))
                    return false;

                uses_arguments = old_uses_arguments;
                uses_eval = old_uses_eval;

                RestoreStrictMode(old_is_strict_mode);

                current_arena = previous_arena;

                return true;
            }
        }
        else if (is_eval || allow_top_level_function_expr)
        {
            --in_function;

            uses_arguments = old_uses_arguments;
            uses_eval = old_uses_eval;

            RestoreStrictMode(old_is_strict_mode);

            current_arena = previous_arena;

            if (!ParseFunctionExpr())
                return false;
            else
            {
                PushStatement(OP_NEWGRO_L(ES_ExpressionStmt, (PopExpression()), Arena()));
                return true;
            }
        }
        else
            error_code = EXPECTED_IDENTIFIER;
    }
    else
        error_code = EXPECTED_IDENTIFIER;

    current_arena = previous_arena;
    function_data_index = old_function_data_index;

    return false;
}


bool
ES_Parser::ParseFunctionExpr(bool push_expression)
{
    JString *function_name;

    unsigned identifiers_before = identifier_stack_used;
    unsigned functions_before = function_stack_used;
    unsigned function_data_before = function_data_stack_used;
    unsigned statements_before = statement_stack_used;

    if (IsNearStackLimit())
        PARSE_FAILED(INPUT_TOO_COMPLEX);

    ++in_function;

    bool old_uses_arguments = uses_arguments;
    bool old_uses_eval = uses_eval;
    bool old_is_strict_mode = is_strict_mode;

    uses_arguments = FALSE;
    uses_eval = FALSE;

    unsigned old_function_data_index = function_data_index;
    function_data_index = 0;

    if (ParseKeyword (ES_Token::KEYWORD_FUNCTION, true))
    {
        unsigned start_index = last_token.start;
        unsigned start_line = last_token.line;
        unsigned start_column = last_token.column;
        ES_SourceLocation start_location, end_location;

        if (ParseIdentifier (function_name, true))
        {
            if (function_name && !ValidateIdentifier (function_name))
                return false;

            ES_SourceLocation function_name_location (last_token.GetSourceLocation ());

            if (ParsePunctuator (ES_Token::LEFT_PAREN) &&
                ParseFormalParameterList () &&
                ParsePunctuator (ES_Token::RIGHT_PAREN) &&
                ParseSourceElements (true, &start_location) &&
                ParsePunctuator (ES_Token::RIGHT_BRACE, end_location))
            {
                unsigned parameter_names_count = identifier_stack_used - identifiers_before;
                unsigned functions_count = function_stack_used - functions_before;
                unsigned function_data_count = function_data_stack_used - function_data_before;
                unsigned statements_count = statement_stack_used - statements_before;

                if (function_name && !ValidateIdentifier (function_name, &function_name_location))
                    return false;

                --in_function;

                function_data_index = old_function_data_index;

                if (!CompileFunction(function_name, parameter_names_count, es_maxu (functions_count, function_data_count), statements_count, start_index, start_line, start_column, last_token.end, start_location, end_location, NULL, true, NULL, 0, in_with != 0))
                    return false;

                if (push_expression)
                    if (in_function || is_simple_eval || is_strict_eval)
                        PushExpression(OP_NEWGRO_L(ES_FunctionExpr, (function_data_index - 1), Arena()));
                    else
                        PushExpression(OP_NEWGRO_L(ES_FunctionExpr, (function_stack[function_stack_used - 1]), Arena()));

                RestoreStrictMode(old_is_strict_mode);

                uses_arguments = old_uses_arguments;
                uses_eval = old_uses_eval;

                return true;
            }
        }
    }

    function_data_index = old_function_data_index;
    return false;
}


bool
ES_Parser::ParseFormalParameterList ()
{
    JString *parameter_name;

    if (!ParseIdentifier (parameter_name))
        return true;

    while (1)
    {
        if (!ValidateIdentifier (parameter_name))
            return false;

        PushIdentifier (parameter_name);

        if (!ParsePunctuator (ES_Token::COMMA))
            return true;

        if (!ParseIdentifier (parameter_name))
            return false;
    }
}


bool
ES_Parser::ParseArguments (unsigned depth)
{
    bool first = true;

    while (1)
    {
        if (ParsePunctuator (ES_Token::RIGHT_PAREN))
            return true;
        else if (!first && !ParsePunctuator (ES_Token::COMMA))
        {
            error_code = EXPECTED_RIGHT_PAREN;

            return false;
        }
        else if (!ParseExpression (depth, ES_Expression::PROD_ASSIGNMENT_EXPR, true, expression_stack_used))
        {
            OP_ASSERT(error_code != NO_ERROR || automatic_error_code != NO_ERROR);
            return false;
        }

        first = false;
    }
}


bool
ES_Parser::ParseVariableDeclList (unsigned depth, bool allow_in)
{
    bool first = true;

    while (1)
    {
        JString *identifier;
        ES_Expression *initializer = 0;

        if (!ParseIdentifier (identifier))
            return !first;

        if (!ValidateIdentifier (identifier))
            return false;

        if (ParsePunctuator (ES_Token::ASSIGN))
        {
            if (!ParseExpression (depth, ES_Expression::PROD_ASSIGNMENT_EXPR, allow_in, expression_stack_used))
                return false;

            initializer = PopExpression ();
            // replace <init-expr> with the assignment <ident> = <init-expr>
            ES_IdentifierExpr *ident = OP_NEWGRO_L (ES_IdentifierExpr, (identifier), Arena ());
            initializer = OP_NEWGRO_L (ES_AssignExpr, (ident, initializer), Arena ());
        }

        if (token.type == ES_Token::INVALID)
            return false;

        PushIdentifier (identifier);
        PushExpression (initializer);

        if (!ParsePunctuator (ES_Token::COMMA))
            return true;
    }
}


bool
ES_Parser::SetAllowLinebreak (bool new_value)
{
    bool old_value = allow_linebreak;
    allow_linebreak = new_value;
    return old_value;
}


bool
ES_Parser::GetAllowLinebreak ()
{
    return allow_linebreak;
}


static void *
GrowGeneric (void *data, unsigned &used, unsigned &total)
{
    if (used == total)
    {
        if (total == 0)
            total = 8;
        else
            total *= 2;

        void **old_data = (void **) data;
        void **new_data = OP_NEWA_L(void *, total);

        for (unsigned index = 0; index < used; ++index)
            new_data[index] = old_data[index];

        OP_DELETEA(old_data);
        return new_data;
    }
    else
        return data;
}


void **
PopGeneric (unsigned &used, void **array, unsigned count, OpMemGroup *arena)
{
    OP_ASSERT (count == ~0u || used >= count);

    if (count == ~0u)
        if (used == 0)
            return 0;
        else
            count = used;
    else if (count == 0)
        return 0;

    void **result = arena ? OP_NEWGROA_L(void *, count, arena) : OP_NEWA_L(void *, count);

    op_memcpy (result, array + used - count, sizeof (void *) * count);

    used -= count;

    return result;
}


void
ES_Parser::PushExpression (ES_Expression *expression, const ES_SourceLocation &location)
{
    if (expression)
        expression->SetSourceLocation(location);

    expression_stack = (ES_Expression **) GrowGeneric (expression_stack, expression_stack_used, expression_stack_total);

    expression_stack[expression_stack_used++] = expression;
}

void
ES_Parser::PushExpression (ES_Expression *expression)
{
    if (expression && !expression->HasSourceLocation())
        expression->SetSourceLocation(ES_SourceLocation(last_token.start, last_token.line, last_token.end - last_token.start));

    expression_stack = (ES_Expression **) GrowGeneric (expression_stack, expression_stack_used, expression_stack_total);

    expression_stack[expression_stack_used++] = expression;
}


ES_Expression *
ES_Parser::PopExpression ()
{
    OP_ASSERT (expression_stack_used > 0);

    return expression_stack[--expression_stack_used];
}


ES_Expression **
ES_Parser::PopExpressions (unsigned count)
{
    return (ES_Expression **) PopGeneric (expression_stack_used, (void **) expression_stack, count, Arena());
}


void
ES_Parser::PushStatement (ES_Statement *statement)
{
    statement_stack = (ES_Statement **) GrowGeneric (statement_stack, statement_stack_used, statement_stack_total);
    statement_stack[statement_stack_used++] = statement;
}


ES_Statement *
ES_Parser::PopStatement ()
{
    OP_ASSERT (statement_stack_used > 0);

    return statement_stack[--statement_stack_used];
}


ES_Statement **
ES_Parser::PopStatements (unsigned count)
{
    return (ES_Statement **) PopGeneric (statement_stack_used, (void **) statement_stack, count, Arena());
}


void
ES_Parser::PushFunction (ES_FunctionCode *function)
{
    function_stack = (ES_FunctionCode **) GrowGeneric (function_stack, function_stack_used, function_stack_total);
    function_stack[function_stack_used++] = function;
}


ES_FunctionCode *
ES_Parser::PopFunction ()
{
    OP_ASSERT (function_stack_used > 0);

    return function_stack[--function_stack_used];
}


ES_FunctionCode **
ES_Parser::PopFunctions (unsigned count)
{
    return (ES_FunctionCode **) PopGeneric (function_stack_used, (void **) function_stack, count, NULL);
}


void
ES_Parser::PushFunctionData (const FunctionData &data)
{
    if (function_data_stack_used == function_data_stack_total)
    {
        if (function_data_stack_total == 0)
            function_data_stack_total = 8;
        else
            function_data_stack_total *= 2;

        FunctionData *old_stack = function_data_stack;
        FunctionData *new_stack = OP_NEWA_L(FunctionData, function_data_stack_total);

        op_memcpy(new_stack, old_stack, function_data_stack_used * sizeof (FunctionData));

        OP_DELETEA(old_stack);
        function_data_stack = new_stack;
    }

    function_data_stack[function_data_stack_used++] = data;
}


ES_Parser::FunctionData *
ES_Parser::PopFunctionData (unsigned count)
{
    if (count == ~0u)
        if (function_data_stack_used == 0)
            return 0;
        else
            count = function_data_stack_used;
    else if (count == 0)
        return 0;

    OP_ASSERT(count <= function_data_stack_used);

    FunctionData *result = OP_NEWGROA_L(FunctionData, count, Arena());

    op_memcpy (result, function_data_stack + function_data_stack_used - count, count * sizeof (FunctionData));

    function_data_stack_used -= count;

    return result;
}


void
ES_Parser::PushIdentifier (JString *identifier)
{
    identifier_stack = (JString **) GrowGeneric (identifier_stack, identifier_stack_used, identifier_stack_total);
    identifier_stack[identifier_stack_used++] = identifier;
}


JString *
ES_Parser::PopIdentifier ()
{
    OP_ASSERT (identifier_stack_used > 0);

    return identifier_stack[--identifier_stack_used];
}


JString **
ES_Parser::PopIdentifiers (unsigned count, BOOL use_arena)
{
    return (JString **) PopGeneric (identifier_stack_used, (void **) identifier_stack, count, use_arena ? Arena() : NULL);
}


BOOL
ES_Parser::PushProperty (unsigned check_count,
                         JString *name,
#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
                         ES_SourceLocation name_loc,
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
                         ES_Expression *expression,
                         BOOL getter,
                         BOOL setter)
{
    OP_ASSERT(!(getter && setter));

    if (check_count)
    {
        ES_ObjectLiteralExpr::Property *stop = static_cast<ES_ObjectLiteralExpr::Property *>(property_stack) + property_stack_used, *existing = stop - check_count;

        do
            if (existing->name == name)
            {
                if (getter)
                {
                    if (existing->expression)
                    {
                        SetError(BOTH_GETTER_AND_PROPERTY, name_loc);
                        return FALSE;
                    }
                    else if (existing->getter)
                    {
                        SetError(DUPLICATE_GETTER, name_loc);
                        return FALSE;
                    }

                    existing->getter = expression;
                }
                else if (setter)
                {
                    if (existing->expression)
                    {
                        SetError(BOTH_SETTER_AND_PROPERTY, name_loc);
                        return FALSE;
                    }
                    else if (existing->setter)
                    {
                        SetError(DUPLICATE_SETTER, name_loc);
                        return FALSE;
                    }

                    existing->setter = expression;
                }
                else // the current one is neither getter nor setter; hence - a property
                {
                    if (existing->getter)
                    {
                        SetError(BOTH_GETTER_AND_PROPERTY, name_loc);
                        return FALSE;
                    }
                    else if (existing->setter)
                    {
                        SetError(BOTH_SETTER_AND_PROPERTY, name_loc);
                        return FALSE;
                    }
                    else if (is_strict_mode)
                    {
                        SetError(DUPLICATE_DATA_PROPERTY, name_loc);
                        return FALSE;
                    }

                    existing->expression = expression;
                }

                return TRUE;
            }
        while (++existing != stop);
    }

    if (property_stack_used == property_stack_total)
    {
        if (property_stack_total == 0)
            property_stack_total = 8;
        else
            property_stack_total *= 2;

        ES_ObjectLiteralExpr::Property *old_stack = static_cast<ES_ObjectLiteralExpr::Property *>(property_stack);
        ES_ObjectLiteralExpr::Property *new_stack = OP_NEWA_L(ES_ObjectLiteralExpr::Property, property_stack_total);

        for (unsigned index = 0; index < property_stack_used; ++index)
            new_stack[index] = old_stack[index];

        OP_DELETEA(old_stack);
        property_stack = new_stack;
    }

    ES_ObjectLiteralExpr::Property &property = static_cast<ES_ObjectLiteralExpr::Property *>(property_stack)[property_stack_used++];

    property.name = name;
    property.expression = getter || setter ? NULL : expression;
    property.getter = getter ? expression : NULL;
    property.setter = setter ? expression : NULL;

    return TRUE;
}


void *
ES_Parser::PopProperties (unsigned count)
{
    if (count == ~0u)
        if (property_stack_used == 0)
            return 0;
        else
            count = property_stack_used;
    else if (count == 0)
        return 0;

    ES_ObjectLiteralExpr::Property *result = OP_NEWGROA_L(ES_ObjectLiteralExpr::Property, count, Arena());

    op_memcpy (result, static_cast<ES_ObjectLiteralExpr::Property *>(property_stack) + property_stack_used - count, sizeof (ES_ObjectLiteralExpr::Property) * count);

    property_stack_used -= count;

    return result;
}


#ifdef ECMASCRIPT_DEBUGGER

void
ES_Parser::DebuggerSignalNewScript(ES_Context *context/* = NULL*/)
{
    if (GetIsDebugging())
        DebuggerSignalNewScript(StorageZ(context, lexer->GetSourceString()), context);
}

void
ES_Parser::DebuggerSignalNewScript(const uni_char *source, ES_Context *context/* = NULL*/)
{
    if (GetIsDebugging())
        if (g_ecmaManager->GetDebugListener())
        {
            ES_DebugScriptData data;
            data.source = source;
            data.url = url ? StorageZ(context, url) : UNI_L("");
            data.type = script_type;
            data.script_guid = script_guid;
            g_ecmaManager->GetDebugListener()->NewScript(context ? context->GetRuntime() : runtime, &data);
        }
        else
            SetIsDebugging(FALSE);
}

void
ES_Parser::DebuggerSignalParseError(ES_ErrorInfo *err)
{
    if (GetIsDebugging())
        if (g_ecmaManager->GetDebugListener())
        {
            OP_ASSERT(err && err->script_guid);
            g_ecmaManager->GetDebugListener()->ParseError(runtime, err);
        }
        else
            SetIsDebugging(FALSE);
}

#endif // ECMASCRIPT_DEBUGGER

OP_STATUS
ES_Parser::RegisterRegExpLiteral(ES_RegExp_Information *regexp)
{
    if (regexp)
        return regexp_literals.Add(regexp);
    return OpStatus::ERR;
}

void
ES_Parser::ReleaseRegExpLiterals()
{
    for (unsigned index = 0; index < regexp_literals.GetCount(); ++index)
    {
        /* The struct is arena allocated, regexp is shared. */
        ES_RegExp_Information *regexp = regexp_literals.Get(index);
        if (regexp->regexp)
            regexp->regexp->DecRef();
    }

    regexp_literals.Clear();
}
