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

#include "modules/ecmascript/carakan/src/compiler/es_compiler.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_stmt.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_expr.h"
#include "modules/ecmascript/carakan/src/compiler/es_analyzer.h"
#include "modules/ecmascript/carakan/src/compiler/es_instruction_data.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"

#define WRITE1(arg1) do { SetExtentInformation(arg1); EnsureBytecodeAllocation(1); *bytecode++ = arg1; } while (0)
#define WRITE2(arg1, arg2) do { SetExtentInformation(arg1); EnsureBytecodeAllocation(2); *bytecode++ = arg1; *bytecode++ = arg2; } while (0)
#define WRITE3(arg1, arg2, arg3) do { SetExtentInformation(arg1); EnsureBytecodeAllocation(3); *bytecode++ = arg1; *bytecode++ = arg2; *bytecode++ = arg3; } while (0)
#define WRITE4(arg1, arg2, arg3, arg4) do { SetExtentInformation(arg1); EnsureBytecodeAllocation(4); *bytecode++ = arg1; *bytecode++ = arg2; *bytecode++ = arg3; *bytecode++ = arg4; } while (0)
#define WRITE5(arg1, arg2, arg3, arg4, arg5) do { SetExtentInformation(arg1); EnsureBytecodeAllocation(5); *bytecode++ = arg1; *bytecode++ = arg2; *bytecode++ = arg3; *bytecode++ = arg4; *bytecode++ = arg5; } while (0)
#define WRITE6(arg1, arg2, arg3, arg4, arg5, arg6) do { SetExtentInformation(arg1); EnsureBytecodeAllocation(6); *bytecode++ = arg1; *bytecode++ = arg2; *bytecode++ = arg3; *bytecode++ = arg4; *bytecode++ = arg5; *bytecode++ = arg6; } while (0)
#define REGISTER(reg) static_cast<ES_CodeWord>(reg)

ES_Compiler::JumpOffset *
ES_Compiler::AllocateJumpOffset()
{
    if (unused_jump_offsets)
    {
        JumpOffset *offset = unused_jump_offsets;
        unused_jump_offsets = offset->next;
        return offset;
    }
    else
        return OP_NEWGRO_L(JumpOffset, (), Arena());
}

void
ES_Compiler::FreeJumpOffset(JumpOffset *offset)
{
    offset->next = unused_jump_offsets;
    unused_jump_offsets = offset;
}

ES_Compiler::Jump *
ES_Compiler::AllocateJump()
{
    if (unused_jumps)
    {
        Jump *jump = unused_jumps;
        unused_jumps = jump->next;
        return jump;
    }
    else
        return OP_NEWGRO_L(Jump, (*this), Arena());
}

void
ES_Compiler::FreeJump(Jump *jump)
{
    jump->next = unused_jumps;
    unused_jumps = jump;
}

ES_Compiler::ES_Compiler(ES_Runtime *runtime, ES_Context *context, ES_Global_Object *global_object, CodeType codetype, unsigned register_frame_offset, ES_FunctionCode **lexical_scopes, unsigned lexical_scopes_count)
    : unused_jump_offsets(NULL)
    , unused_jumps(NULL)
    , runtime(runtime)
    , context(context)
    , global_object(global_object)
    , codetype(codetype)
    , ident_length(context->rt_data->idents[ESID_length])
    , ident_eval(context->rt_data->idents[ESID_eval])
    , ident_apply(context->rt_data->idents[ESID_apply])
    , uses_arguments(FALSE)
    , uses_eval(FALSE)
    , uses_get_scope_ref(FALSE)
    , uses_lexical_scope(FALSE)
    , has_outer_scope_chain(FALSE)
    , is_function_expr(FALSE)
    , is_strict_mode(FALSE)
    , redirected_call(NULL)
    , formals_count(0)
    , lexical_scopes(lexical_scopes)
    , lexical_scopes_count(lexical_scopes_count)
    , closures(NULL)
    , closures_count(0)
    , register_frame_offset(register_frame_offset)
    , bytecode_base(NULL)
    , bytecode(NULL)
    , bytecode_allocated(0)
    , maximum_jump_target(0)
    , first_jump(NULL)
    , object_literal_classes_used(0)
    , object_literal_classes(NULL)
    , property_get_caches_used(0)
    , property_put_caches_used(0)
    , first_unused_temporary(NULL)
    , last_unused_temporary(NULL)
    , high_unused_temporaries(NULL)
    , highest_used_temporary_index(es_maxu(register_frame_offset, 1) - 1)
    , highest_ever_temporary_index(es_maxu(register_frame_offset, 1) - 1)
    , forward_jump_id_counter(0)
    , known_local_value(NULL)
    , local_value_known(NULL)
    , local_value_snapshot(1)
    , local_value_generation(NULL)
    , local_value_stored(NULL)
    , suspend_value_tracking(0)
    , doubles(NULL)
    , doubles_count(0)
    , global_accesses(NULL)
    , global_accesses_count(0)
    , current_rethrow_target(NULL)
    , current_finally_target(NULL)
    , current_continuation_target(NULL)
    , current_unused_label_set(NULL)
    , current_exception_handler(NULL)
    , toplevel_exception_handlers(NULL)
    , caught_exception(NULL)
#ifdef ES_NATIVE_SUPPORT
    , current_simple_loop_variable(NULL)
    , variable_range_limit_span(NULL)
#endif // ES_NATIVE_SUPPORT
    , current_inner_scopes(NULL)
    , current_inner_scopes_count(0)
    , current_inner_scopes_allocated(0)
    , final_inner_scopes(NULL)
    , final_inner_scopes_count(0)
    , first_loop(NULL)
    , last_loop(NULL)
    , loops_count(0)
    , format_string_caches_used(0)
    , eval_caches_used(0)
    , first_switch_table(NULL)
    , switch_tables_used(0)
    , want_object(FALSE)
    , emit_debug_information(TRUE)
    , current_source_location(NULL)
    , debug_records(NULL)
    , debug_records_count(0)
    , debug_records_allocated(0)
{
}

ES_Compiler::~ES_Compiler()
{
    OP_DELETEA(bytecode_base);
    OP_DELETEA(doubles);
    OP_DELETEA(global_accesses);
    OP_DELETEA(current_inner_scopes);
    OP_DELETEA(final_inner_scopes);

    for (unsigned i = 0; i < regexps.GetCount(); i++)
    {
        ES_RegExp_Information *re = regexps.Get(i);
        ES_RegExp_Information::Destroy(re);
        OP_DELETE(re);
    }
    regexps.Clear();

    for (ObjectLiteralClass *olc = object_literal_classes; olc; olc = olc->next)
        OP_DELETEA(olc->klass.properties);

    /* Cleanup since we return before pop on compile errors in labelled code. */
    while (current_rethrow_target)
        PopRethrowTarget();

    while (current_finally_target)
        PopFinallyTarget();

    while (current_continuation_target)
        PopContinuationTargets(1);

    while (caught_exception)
        PopCaughtException();

    OP_DELETEA(debug_records);

    int count = constant_array_literals.GetCount();
    for (int i = 0; i < count; i++)
    {
        ES_CodeStatic::ConstantArrayLiteral *cal = constant_array_literals.Get(i);
        OP_DELETEA(cal->indeces);
        OP_DELETEA(cal->values);
    }

    CodeExceptionHandlerElement *handler_element = toplevel_exception_handlers;
    while (handler_element)
    {
        OP_DELETEA(handler_element->code_handler.nested_handlers);
        handler_element = handler_element->previous;
    }

    ExceptionHandler *handler = current_exception_handler;
    while (handler)
    {
        if (handler->nested)
            OP_DELETEA(handler->nested->code_handler.nested_handlers);
        handler = handler->previous;
    }
}

void
ES_Compiler::SetParser(ES_Parser *new_parser)
{
    parser = new_parser;
#ifdef ECMASCRIPT_DEBUGGER
    if (parser->GetIsDebugging())
        suspend_value_tracking = 1;
#endif // ECMASCRIPT_DEBUGGER
}

void
ES_Compiler::SetInnerScope(unsigned *inner_scopes, unsigned inner_scopes_count)
{
    if (inner_scopes_count)
        for (unsigned index = 0; index < inner_scopes_count; ++index)
            PushInnerScope(Register(inner_scopes[index]));
}

void
ExtractExceptionHandlers(ES_CodeStatic::ExceptionHandler *&handlers, unsigned &handlers_count, ES_Compiler::CodeExceptionHandlerElement *elements)
{
    handlers_count = 0;

    if (elements)
    {
        ES_Compiler::CodeExceptionHandlerElement *iter;

        iter = elements;
        while (iter)
        {
            ++handlers_count;
            iter = iter->previous;
        }

        handlers = OP_NEWA_L(ES_CodeStatic::ExceptionHandler, handlers_count);

        unsigned index = handlers_count - 1;

        iter = elements;
        while (iter)
        {
            ES_Compiler::CodeExceptionHandlerElement *element = iter;
            iter = iter->previous;

            handlers[index--] = element->code_handler;
            element->code_handler.nested_handlers = NULL;
        }
    }
    else
        handlers = NULL;
}

void Initialize(ES_ProgramCode *code, ES_ProgramCodeStatic *data, ES_Global_Object *global_object)
{
    ES_ProgramCode::Initialize(code, data, global_object);
}

static void
ExtractStrings(JString **&strings, unsigned &count, ES_Identifier_List *list, unsigned skip = 0)
{
    count = list->GetCount() - skip;

    if (count)
    {
        strings = OP_NEWA_L(JString *, count);
        op_memcpy(strings, list->GetIdentifiers() + skip, count * sizeof strings[0]);
    }
    else
        strings = NULL;
}

static void
ExtractStringIndeces(unsigned *strings, unsigned &count, ES_Identifier_List *list, ES_Identifier_List *strings_table, unsigned skip = 0)
{
    count = list->GetCount() - skip;

    if (count)
    {
        JString *string = NULL;

        for (unsigned index = 0; index < count; ++index)
        {
            list->Lookup(index + skip, string);

            if (string->StaticIndex(strings[index]))
                strings[index] |= 0x40000000u;
            else
            {
#ifdef _DEBUG
                BOOL found = strings_table->IndexOf(string, strings[index]);
                OP_ASSERT(found);
#else // _DEBUG
                strings_table->IndexOf(string, strings[index]);
#endif // _DEBUG
            }
        }
    }
    else
        strings = NULL;
}

BOOL
ES_Compiler::CompileProgram(unsigned functions_count, ES_FunctionCode **&functions,
                            unsigned statements_count, ES_Statement **statements,
                            BOOL generate_result, ES_ProgramCode *&code)
{
    ANCHOR_ARRAY(ES_FunctionCode *, functions);

    this->functions = functions;
    this->function_data = NULL;
    this->functions_count = functions_count;
    this->functions_offset = 0;

    this->statements = statements;
    this->statements_count = statements_count;

    unsigned stmt_index;

    locals_table = NULL;
    strings_table = ES_Identifier_List::Make(context, 8);
    globals_table = ES_Identifier_List::Make(context, 8);
    property_get_caches_table = ES_Identifier_List::Make(context, 8);
    property_put_caches_table = ES_Identifier_List::Make(context, 8);
    global_caches_table = ES_Identifier_Hash_Table::Make(context, 8);

    for (stmt_index = 0; stmt_index < statements_count; ++stmt_index)
        statements[stmt_index]->Prepare(*this);

    unsigned function_index;

#ifdef ECMASCRIPT_DEBUGGER
    if (GetParser()->GetIsDebugging())
        EmitInstruction(ESI_DEBUGGER_STOP, ES_DebugListener::ESEV_NEWSCRIPT);
#endif // ECMASCRIPT_DEBUGGER

    for (function_index = 0; function_index < functions_count; ++function_index)
    {
        ES_FunctionCode *function = functions[function_index];
        ES_FunctionCodeStatic *data = function->GetData();

        if (!data->is_function_expr)
            AddGlobalDeclaration(function->GetName());
    }

    if (generate_result || codetype == CODETYPE_EVAL)
    {
        global_result_value = Temporary();
        EmitInstruction(ESI_LOAD_UNDEFINED, global_result_value);
    }

    if (codetype == CODETYPE_EVAL && closures_count == 0)
        for (unsigned global_index = 0; global_index < globals_table->GetCount(); ++global_index)
        {
            JString *name;
            globals_table->Lookup(global_index, name);
            EmitInstruction(ESI_DECLARE_GLOBAL, Identifier(name));
        }

    for (function_index = 0; function_index < functions_count; ++function_index)
        if (!functions[function_index]->GetData()->is_function_expr)
        {
            Register fn(Temporary());
            EmitInstruction(ESI_NEW_FUNCTION, fn, function_index, GetInnerScopeIndex());

            if (HasUnknownScope())
                EmitInstruction(ESI_PUT_SCOPE, Identifier(functions[function_index]->GetName()), fn, GetInnerScopeIndex(), ~0u);
            else
                EmitGlobalAccessor(ESI_PUT_GLOBAL, functions[function_index]->GetName(), fn);
        }

    SuspendValueTracking();

    for (stmt_index = 0; stmt_index < statements_count; ++stmt_index)
        if (!statements[stmt_index]->Compile(*this, global_result_value))
            return FALSE;

    if (codetype == CODETYPE_EVAL)
        EmitInstruction(ESI_RETURN_FROM_EVAL, global_result_value);
    else if (generate_result)
        EmitInstruction(ESI_RETURN_VALUE, global_result_value);
    else
        EmitInstruction(ESI_RETURN_NO_VALUE);

    ES_ProgramCodeStatic *data = OP_NEW_L(ES_ProgramCodeStatic, ());
    OpStackAutoPtr<ES_ProgramCodeStatic> data_anchor(data);

    GC_ALLOCATE(context, code, ES_ProgramCode, (code, data, global_object));

    data_anchor.release();

    data->register_frame_size = highest_ever_temporary_index + 1;
    data->first_temporary_register = 1;

    data->functions = OP_NEWA_L(ES_FunctionCodeStatic *, functions_count);
    data->functions_count = functions_count;

    for (function_index = 0; function_index < functions_count; ++function_index)
        data->functions[function_index] = ES_FunctionCodeStatic::IncRef(functions[function_index]->GetData());

    code->functions = functions;
    /* Ownership now resides with 'code'. */
    ANCHOR_ARRAY_RELEASE(functions);

    InitializeCode(code);

    ExtractStrings(code->strings, data->strings_count, strings_table);

    if (globals_table->GetCount() != 0)
    {
        data->variable_declarations = OP_NEWA_L(unsigned, globals_table->GetCount());
        ExtractStringIndeces(data->variable_declarations, data->variable_declarations_count, globals_table, strings_table);
    }

#ifdef ES_NATIVE_SUPPORT
    for (function_index = 0; function_index < functions_count; ++function_index)
    {
        if (!code->functions[function_index]->GetData()->is_function_expr)
        {
            unsigned index;
            globals_table->IndexOf(code->functions[function_index]->GetName(), index);
            data->variable_declarations[index] |= 0x80000000u;
        }
    }
#endif

    data->generate_result = generate_result;

#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!data->optimization_data);
    OP_ASSERT(!code->native_dispatcher);
#endif // ES_NATIVE_SUPPORT

    ES_Identifier_List::Free(context, strings_table);
    ES_Identifier_List::Free(context, globals_table);
    ES_Identifier_List::Free(context, property_get_caches_table);
    ES_Identifier_List::Free(context, property_put_caches_table);
    ES_Identifier_Hash_Table::Free(context, global_caches_table);

    return TRUE;
}

void
Initialize(ES_FunctionCode *code, ES_FunctionCodeStatic *data, ES_Global_Object *global_object, ES_ProgramCodeStaticReaper *program_reaper)
{
    ES_FunctionCode::Initialize(code, data, global_object, program_reaper);
}

static ES_CallExpr *
DetectRedirectedCall(ES_Statement *statement)
{
    /* Checks that 'statement' is a function call on the form

         this.initialize.apply(this, arguments)

       or

         return this.initialize.apply(this, arguments)
     */

    ES_Expression *expression;

    if (statement->GetType() == ES_Statement::TYPE_EXPRESSION)
        expression = static_cast<ES_ExpressionStmt *>(statement)->GetExpression();
    else if (statement->GetType() == ES_Statement::TYPE_RETURN)
        expression = static_cast<ES_ReturnStmt *>(statement)->GetReturnValue();
    else
        return NULL;

    if (!expression || expression->GetType() != ES_Expression::TYPE_CALL)
        return NULL;

    ES_CallExpr *call = static_cast<ES_CallExpr *>(expression);
    ES_Expression *function = call->GetFunction();

    if (function->GetType() != ES_Expression::TYPE_PROPERTY_REFERENCE)
        return NULL;

    JString *name = static_cast<ES_PropertyReferenceExpr *>(function)->GetName();

    if (!name->Equals(UNI_L("apply"), 5))
        return NULL;

    function = static_cast<ES_PropertyReferenceExpr *>(function)->GetBase();

    if (function->GetType() == ES_Expression::TYPE_IDENTIFIER)
    {
        name = static_cast<ES_IdentifierExpr *>(function)->GetName();

        if (name->Equals(UNI_L("arguments"), 9))
            return NULL;
    }
    else
    {
        if (function->GetType() != ES_Expression::TYPE_PROPERTY_REFERENCE)
            return NULL;

        ES_Expression *base = static_cast<ES_PropertyReferenceExpr *>(function)->GetBase();

        if (base->GetType() != ES_Expression::TYPE_THIS)
        {
            if (base->GetType() != ES_Expression::TYPE_IDENTIFIER)
                return NULL;

            name = static_cast<ES_IdentifierExpr *>(base)->GetName();

            if (name->Equals(UNI_L("arguments"), 9))
                return NULL;
        }
    }

    unsigned argc = call->GetArgumentsCount();
    ES_Expression **argv = call->GetArguments();

    if (argc != 2 || argv[0]->GetType() != ES_Expression::TYPE_THIS || argv[1]->GetType() != ES_Expression::TYPE_IDENTIFIER)
        return NULL;

    name = static_cast<ES_IdentifierExpr *>(argv[1])->GetName();

    if (!name->Equals(UNI_L("arguments"), 9))
        return NULL;

    return call;
}

class ES_DetectRedirectedCalls
    : public ES_StatementVisitor
{
public:
    ES_DetectRedirectedCalls()
        : custom_arguments_use(FALSE),
          complex_tail(FALSE),
          has_variables(FALSE),
          redirect_found(NULL)
    {
    }

    virtual BOOL Visit(ES_Expression *expression);
    virtual BOOL Enter(ES_Statement *statement, BOOL &skip);
    virtual BOOL Leave(ES_Statement *statement);

    BOOL custom_arguments_use, complex_tail, has_variables;
    ES_CallExpr *redirect_found;

    BOOL Valid() { return !custom_arguments_use && !complex_tail && !has_variables; }
};

/* virtual */ BOOL
ES_DetectRedirectedCalls::Visit(ES_Expression *expression)
{
    if (expression->GetType() == ES_Expression::TYPE_IDENTIFIER)
    {
        JString *name = static_cast<ES_IdentifierExpr *>(expression)->GetName();

        if (name->Equals(UNI_L("arguments"), 9))
            custom_arguments_use = TRUE;
    }

    return Valid();
}

/* virtual */ BOOL
ES_DetectRedirectedCalls::Enter(ES_Statement *statement, BOOL &skip)
{
    if (statement->GetType() == ES_Statement::TYPE_VARIABLEDECL)
        has_variables = TRUE;
    else if (!redirect_found && (redirect_found = DetectRedirectedCall(statement)))
        skip = TRUE;
    else if (redirect_found)
        if (statement->GetType() == ES_Statement::TYPE_RETURN)
        {
            ES_Expression *value = static_cast<ES_ReturnStmt *>(statement)->GetReturnValue();

            if (value)
                complex_tail = TRUE;
        }
        else if (statement->GetType() != ES_Statement::TYPE_EMPTY)
            complex_tail = TRUE;

    return Valid();
}

/* virtual */ BOOL
ES_DetectRedirectedCalls::Leave(ES_Statement *statement)
{
    return Valid();
}

static unsigned
CountStaticFunctions(ES_FunctionCodeStatic **functions, unsigned functions_count)
{
    unsigned static_functions_count = 0;
    for (unsigned index = 0; index < functions_count; ++index)
        if (functions[index]->is_static_function)
            ++static_functions_count;
    return static_functions_count;
}

class ES_NestedScopeAnalyzer
    : public ES_StatementVisitor
{
public:
    ES_NestedScopeAnalyzer(ES_Compiler &compiler, const ES_Parser::FunctionData &data)
        : compiler(compiler),
          context(compiler.GetContext()),
          data(data),
          skip_read(NULL)
    {
    }

    ~ES_NestedScopeAnalyzer()
    {
    }

    void Process();

    virtual BOOL Visit(ES_Expression *expression);
    virtual BOOL Enter(ES_Statement *statement, BOOL &skip);
    virtual BOOL Leave(ES_Statement *statement);

private:
    ES_Identifier_List *locals_table;

    ES_Compiler &compiler;
    ES_Context *context;
    const ES_Parser::FunctionData &data;

    ES_Expression *skip_read;
    enum { COLLECT_LOCALS, PROCESS_NESTED_ACCESSES } pass;

    void HandleRead(ES_IdentifierExpr *source);
    void HandleWrite(ES_IdentifierExpr *target);
};

void
ES_NestedScopeAnalyzer::Process()
{
    unsigned index, position;

    pass = COLLECT_LOCALS;
    locals_table = ES_Identifier_List::Make(context, 16);

    for (unsigned index = 0; index < data.parameters_count; ++index)
        locals_table->AppendL(context, data.parameters[index], position);

    for (index = 0; index < data.statements_count; ++index)
        data.statements[index]->CallVisitor(this);

    pass = PROCESS_NESTED_ACCESSES;

    for (index = 0; index < data.statements_count; ++index)
        data.statements[index]->CallVisitor(this);

    ES_Identifier_List::Free(context, locals_table);
}

/* virtual */ BOOL
ES_NestedScopeAnalyzer::Visit(ES_Expression *expression)
{
    if (pass == PROCESS_NESTED_ACCESSES)
    {
        switch (expression->GetType())
        {
        case ES_Expression::TYPE_ASSIGN:
            {
                ES_Expression *target = static_cast<ES_AssignExpr *>(expression)->GetTarget();

                if (!target)
                {
                    ES_Expression *source = static_cast<ES_AssignExpr *>(expression)->GetSource();
                    target = static_cast<ES_BinaryExpr *>(source)->GetLeft();
                }

                if (target->GetType() == ES_Expression::TYPE_IDENTIFIER)
                {
                    HandleWrite(static_cast<ES_IdentifierExpr *>(target));
                    skip_read = target;
                }
            }
            break;

        case ES_Expression::TYPE_INCREMENT_OR_DECREMENT:
            {
                ES_Expression *target = static_cast<ES_IncrementOrDecrementExpr *>(expression)->GetExpression();

                if (target->GetType() == ES_Expression::TYPE_IDENTIFIER)
                    HandleWrite(static_cast<ES_IdentifierExpr *>(target));
            }
            break;

        case ES_Expression::TYPE_IDENTIFIER:
            if (skip_read == expression)
                skip_read = NULL;
            else
                HandleRead(static_cast<ES_IdentifierExpr *>(expression));
        }
    }

    return TRUE;
}

/* virtual */ BOOL
ES_NestedScopeAnalyzer::Enter(ES_Statement *statement, BOOL &skip)
{
    if (pass == COLLECT_LOCALS)
    {
        if (statement->GetType() == ES_Statement::TYPE_VARIABLEDECL)
        {
            ES_VariableDeclStmt *variable_decl = static_cast<ES_VariableDeclStmt *>(statement);
            unsigned count = variable_decl->GetCount();

            for (unsigned index = 0, position; index < count; ++index)
                locals_table->AppendL(context, variable_decl->GetName(index), position);
        }
    }

    return TRUE;
}

/* virtual */ BOOL
ES_NestedScopeAnalyzer::Leave(ES_Statement *statement)
{
    return TRUE;
}

void
ES_NestedScopeAnalyzer::HandleRead(ES_IdentifierExpr *source)
{
    JString *name = source->GetName();
    unsigned index;

    if (!locals_table->IndexOf(name, index))
        compiler.RecordNestedRead(name);
}

void
ES_NestedScopeAnalyzer::HandleWrite(ES_IdentifierExpr *target)
{
    JString *name = target->GetName();
    unsigned index;

    if (!locals_table->IndexOf(name, index))
        compiler.RecordNestedWrite(name);
}

BOOL
ES_Compiler::CompileFunction(JString *name, JString *debug_name,
                             unsigned formals_count, JString **formals,
                             unsigned functions_count, ES_Parser::FunctionData *function_data,
                             unsigned statements_count, ES_Statement **statements,
                             ES_FunctionCode *&code)
{
    this->current_function_name = name;
    this->functions = NULL;
    this->function_data = function_data;
    this->functions_count = functions_count;
    this->functions_offset = 0;
    this->formals_count = formals_count;

    this->statements = statements;
    this->statements_count = statements_count;

    locals_table = static_cast<ES_Identifier_Mutable_List *>(ES_Identifier_List::Make(context, 8));
    strings_table = ES_Identifier_List::Make(context, 8);
    property_get_caches_table = ES_Identifier_List::Make(context, 8);
    property_put_caches_table = ES_Identifier_List::Make(context, 8);
    global_caches_table = ES_Identifier_Hash_Table::Make(context, 8);

    unsigned formal_index;
    for (formal_index = 0; formal_index < formals_count; ++formal_index)
    {
        if (formals[formal_index] == context->rt_data->idents[ESID_arguments])
            /* A formal parameter named 'arguments'. Later references to
               'arguments' should be to this formal parameter, not the arguments
               object. */
            uses_arguments = FALSE;
        AddVariable(formals[formal_index], TRUE);
    }

#ifdef ECMASCRIPT_DEBUGGER
    if (GetParser()->GetIsDebugging())
        switch (GetParser()->GetScriptType())
        {
        case SCRIPT_TYPE_EVENT_HANDLER:
        case SCRIPT_TYPE_EVAL:
            EmitInstruction(ESI_DEBUGGER_STOP, ES_DebugListener::ESEV_NEWSCRIPT);
            break;

        default:
            break;
        }
#endif // ECMASCRIPT_DEBUGGER

    if (codetype == CODETYPE_STRICT_EVAL)
        uses_arguments = FALSE;

    BOOL redirect_candidate = uses_arguments && formals_count == 0 && functions_count == 0;
#ifdef ECMASCRIPT_DEBUGGER
    if (GetParser()->GetIsDebugging())
        redirect_candidate = FALSE;
#endif // ECMASCRIPT_DEBUGGER

    if (redirect_candidate)
    {
        ES_DetectRedirectedCalls checker;

        ES_Statement::VisitStatements(statements, statements_count, &checker);

        if (checker.Valid())
            redirected_call = checker.redirect_found;
    }

    is_void_function = TRUE;

    if (redirected_call)
        uses_arguments = FALSE;

#ifdef ECMASCRIPT_DEBUGGER
    // If the script is being debugged we always create the 'arguments'
    // variable. This makes it possible for Evals (ES_AsyncInterface::Eval)
    // from the debugger to access it in the external scope list.
    if (parser->GetIsDebugging())
        uses_arguments = TRUE;
#endif // ECMASCRIPT_DEBUGGER

    if (uses_arguments)
        AddVariable(context->rt_data->idents[ESID_arguments], TRUE);

    unsigned function_index;

    for (function_index = 0; function_index < functions_count; ++function_index)
        if (!function_data[function_index].is_function_expr)
        {
            AddVariable(function_data[function_index].name, FALSE);
            EmitInstruction(ESI_NEW_FUNCTION, Local(function_data[function_index].name), function_index, UINT_MAX);
        }

    unsigned stmt_index;

    for (stmt_index = 0; stmt_index < statements_count; ++stmt_index)
        statements[stmt_index]->Prepare(*this);

    unsigned locals_count = locals_table->GetCount();

    if (!suspend_value_tracking)
    {
        BOOL enabled = locals_count != 0 && !uses_arguments && !uses_eval;

        if (locals_count < functions_count)
            /* If there are more nested functions than there are locals in this
               function, then the gain of tracking locals probably doesn't
               motivate the cost of analyzing all the nested functions. */
            enabled = FALSE;

        for (unsigned index = 0; enabled && index < functions_count; ++index)
        {
            ES_Parser::FunctionData &data = function_data[index];

            /* Disable tracking if the nested function uses eval() or if it has
               nested functions itself (since in that case we would need to
               analyze those nested functions as well, and their nested
               functions, and so on.)  Use of 'arguments' in the nested function
               doesn't affect us, however. */
            if (data.uses_eval || data.functions_count != 0)
                enabled = FALSE;
        }

        if (enabled)
        {
            known_local_value = OP_NEWGROA_L(ES_Value_Internal, locals_count, Arena());
            local_value_known = OP_NEWGROA_L(BOOL, locals_count, Arena());
            local_value_generation = OP_NEWGROA_L(unsigned, locals_count, Arena());
            local_value_stored = OP_NEWGROA_L(unsigned, locals_count, Arena());
            local_value_nested_read = OP_NEWGROA_L(unsigned, (locals_count + 31) / 32, Arena());
            local_value_nested_write = OP_NEWGROA_L(unsigned, (locals_count + 31) / 32, Arena());

            op_memset(local_value_known, 0, locals_count * sizeof *local_value_known);
            op_memset(local_value_nested_read, 0, ((locals_count + 31) / 32) * sizeof *local_value_nested_read);
            op_memset(local_value_nested_write, 0, ((locals_count + 31) / 32) * sizeof *local_value_nested_write);

            for (unsigned index = 0; enabled && index < functions_count; ++index)
                ES_NestedScopeAnalyzer(*this, function_data[index]).Process();
        }
        else
            SuspendValueTracking();
    }

    if (redirected_call)
    {
        OP_ASSERT(locals_count == 0);
        highest_used_temporary_index = 2 + ES_REDIRECTED_CALL_MAXIMUM_ARGUMENTS;
    }
    else
        highest_used_temporary_index = 2 + locals_count - 1;

    highest_ever_temporary_index = highest_used_temporary_index;

    function_return_value = ForwardJump();

    if (codetype == CODETYPE_STRICT_EVAL)
    {
        global_result_value = Temporary();
        EmitInstruction(ESI_LOAD_UNDEFINED, global_result_value);
    }
    else
        global_result_value = Invalid();

    for (stmt_index = 0; stmt_index < statements_count; ++stmt_index)
    {
        if (!statements[stmt_index]->Compile(*this, global_result_value))
            return FALSE;

        if (statements[stmt_index]->NoExit())
            break;
    }

    if (statements_count == 0 || !statements[statements_count - 1]->NoExit())
    {
        if (codetype == CODETYPE_STRICT_EVAL)
            EmitInstruction(ESI_RETURN_VALUE, global_result_value);
        else
            EmitInstruction(ESI_RETURN_NO_VALUE);
    }

    if (try_block_return_value.IsValid())
    {
        SetJumpTarget(function_return_value);
        EmitInstruction(ESI_RETURN_VALUE, try_block_return_value);
    }

    ES_FunctionCodeStatic *data = OP_NEW_L(ES_FunctionCodeStatic, ());
    OpStackAutoPtr<ES_FunctionCodeStatic> data_anchor(data);

    GC_ALLOCATE(context, code, ES_FunctionCode, (code, data, global_object, parser->program_reaper));

    data_anchor.release();

    data->register_frame_size = highest_ever_temporary_index + 1;
    data->first_temporary_register = 2 + locals_count;

    data->formals_and_locals = OP_NEWA_L(unsigned, locals_table->GetCount());
    data->formals_count = formals_count;

    data->is_function_expr = is_function_expr;
    data->has_redirected_call = redirected_call != NULL;

    for (unsigned formal_index = 0; formal_index < formals_count; ++formal_index)
        data->formals_and_locals[formal_index] = String(formals[formal_index]);

    if (name)
        data->name = String(name);
    else if (debug_name)
        data->name = String(debug_name) | 0x80000000u;
    else
        data->name = UINT_MAX;

    ExtractStrings(code->strings, data->strings_count, strings_table);
    ExtractStringIndeces(data->formals_and_locals + formals_count, locals_count, locals_table, strings_table, formals_count);
    OP_ASSERT(2 + data->formals_count + locals_count == data->first_temporary_register);

    if (functions_count != 0)
    {
        BOOL need_lexical_scope = FALSE;
        unsigned index;

        for (index = 0; index < functions_count; ++index)
            if (!function_data[index].inside_inner_scope)
            {
                need_lexical_scope = TRUE;
                break;
            }

        if (need_lexical_scope)
        {
            code->ConstructClass(context);

            for (index = 0; index < functions_count; ++index)
                if (function_data[index].inside_inner_scope)
                {
                    if (!parser->CompileFunction(function_data[index], NULL, 0, TRUE))
                        return FALSE;
                }
                else
                {
                    BOOL is_named_function_expr = function_data[index].is_function_expr && function_data[index].name;
                    unsigned scope_chain_length = is_named_function_expr ? 0 : uses_eval ? 1 : lexical_scopes_count + 1;

                    ES_FunctionCode **scope_chain;

                    if (scope_chain_length)
                    {
                        scope_chain = OP_NEWGROA_L(ES_FunctionCode *, scope_chain_length, Arena());
                        scope_chain[0] = code;

                        if (scope_chain_length > 1)
                            op_memcpy(scope_chain + 1, lexical_scopes, lexical_scopes_count * sizeof(ES_Class *));
                    }
                    else
                        scope_chain = NULL;

                    if (!parser->CompileFunction(function_data[index], scope_chain, scope_chain_length, has_outer_scope_chain || uses_eval || is_named_function_expr))
                        return FALSE;
                }
        }
        else
            for (index = 0; index < functions_count; ++index)
                if (!parser->CompileFunction(function_data[index], NULL, 0, TRUE))
                    return FALSE;

        ES_FunctionCode **functions = code->functions = parser->PopFunctions(functions_count);

        data->functions = OP_NEWA_L(ES_FunctionCodeStatic *, functions_count);
        data->functions_count = functions_count;

        for (index = 0; index < functions_count; ++index)
            data->functions[index] = ES_FunctionCodeStatic::IncRef(functions[index]->GetData());

        data->static_functions_count = CountStaticFunctions(data->functions, data->functions_count);
    }
    else
    {
        data->functions = NULL;
        data->functions_count = 0;
        data->static_functions_count = 0;
        code->functions = NULL;
    }

    InitializeCode(code);

    if (uses_arguments)
    {
        unsigned arguments_index;
        locals_table->IndexOf(context->rt_data->idents[ESID_arguments], arguments_index);
        data->arguments_index = arguments_index + 2;
        data->has_created_arguments_array = TRUE;
    }
    else
    {
        data->arguments_index = ES_FunctionCodeStatic::ARGUMENTS_NOT_USED;
        data->has_created_arguments_array = FALSE;
    }

    data->uses_eval = uses_eval;
    data->uses_arguments = uses_eval || uses_arguments;
    data->uses_get_scope_ref = uses_get_scope_ref;
    data->is_void_function = is_void_function;
    data->is_static_function = !uses_eval && !has_outer_scope_chain && !uses_lexical_scope && (functions_count == 0 || functions_count == data->static_functions_count);

    // Set by the caller.
    data->is_function_expr = FALSE;

#ifdef ES_NATIVE_SUPPORT
    if (!uses_eval && functions_count == 0)
        data->first_variable_range_limit_span = variable_range_limit_span;
    else
    {
        ES_CodeStatic::VariableRangeLimitSpan *vrls = variable_range_limit_span;
        while (vrls)
        {
            ES_CodeStatic::VariableRangeLimitSpan *next = vrls->next;
            OP_DELETE(vrls);
            vrls = next;
        }
    }
#endif // ES_NATIVE_SUPPORT

    ES_Identifier_List::Free(context, locals_table);
    ES_Identifier_List::Free(context, strings_table);

    return TRUE;
}

void
ES_Compiler::InitializeCode(ES_Code *code)
{
    ES_CodeStatic *data = code->data;

    unsigned codewords_count = bytecode - bytecode_base;

    data->codewords = OP_NEWA_L(ES_CodeWord, codewords_count);
    data->codewords_count = codewords_count;
#ifndef _STANDALONE
    MemoryManager::IncDocMemoryCount(sizeof(ES_CodeWord) * codewords_count, FALSE);
#endif // !_STANDALONE

    op_memcpy(data->codewords, bytecode_base, codewords_count * sizeof(ES_CodeWord));

    code->global_object = global_object;

    data->doubles = doubles;
    data->doubles_count = doubles_count;

    doubles = NULL;
    doubles_count = 0;

    data->is_strict_mode = is_strict_mode ? 1 : 0;

    if (object_literal_classes_used != 0)
    {
        data->object_literal_classes = OP_NEWA_L(ES_CodeStatic::ObjectLiteralClass, object_literal_classes_used);
        data->object_literal_classes_count = object_literal_classes_used;

        code->object_literal_classes = OP_NEWA_L(ES_Class *, object_literal_classes_used);
        op_memset(code->object_literal_classes, 0, object_literal_classes_used * sizeof(ES_Class *));

        ObjectLiteralClass *olc = object_literal_classes;

        while (olc)
        {
            data->object_literal_classes[olc->index] = olc->klass;
            olc = olc->next;
        }

        object_literal_classes = NULL;
        object_literal_classes_used = 0;
    }
    else
    {
        data->object_literal_classes = NULL;
        data->object_literal_classes_count = 0;

        code->object_literal_classes = NULL;
    }

    unsigned cal_count = constant_array_literals.GetCount();
    if (cal_count != 0)
    {
        data->constant_array_literals = OP_NEWA_L(ES_CodeStatic::ConstantArrayLiteral, cal_count);
        for (unsigned index = 0; index < cal_count; ++index)
        {
            ES_CodeStatic::ConstantArrayLiteral *cal = constant_array_literals.Get(index);
            data->constant_array_literals[index] = *cal;
            cal->indeces = NULL;
            cal->values = NULL;
        }
        constant_array_literals.Empty();
        data->constant_array_literals_count = cal_count;
        code->constant_array_literals = OP_NEWA_L(ES_Compact_Indexed_Properties *, cal_count);
        op_memset(code->constant_array_literals, 0, cal_count * sizeof(ES_Compact_Indexed_Properties *));
    }
    else
    {
        data->constant_array_literals = NULL;
        data->constant_array_literals_count = 0;

        code->constant_array_literals = NULL;
    }

    data->regexps = OP_NEWA_L(ES_RegExp_Information, regexps.GetCount());
    for (unsigned regexp_index = 0; regexp_index < regexps.GetCount(); ++regexp_index)
    {
        data->regexps[regexp_index] = *regexps.Get(regexp_index);
        data->regexps[regexp_index].regexp->IncRef();
    }
    data->regexps_count = regexps.GetCount();
    code->regexps = OP_NEWA_L(ES_RegExp_Object *, regexps.GetCount());
    op_memset(code->regexps, 0, regexps.GetCount() * sizeof *code->regexps);

    code->property_get_caches = OP_NEWA_L(ES_Code::PropertyCache, property_get_caches_used);
    data->property_get_caches_count = property_get_caches_used;

    code->property_put_caches = OP_NEWA_L(ES_Code::PropertyCache, property_put_caches_used);
    data->property_put_caches_count = property_put_caches_used;

    code->global_caches = OP_NEWA_L(ES_Code::GlobalCache, global_accesses_count);
    data->global_accesses_count = global_accesses_count;

#ifdef ES_PROPERTY_CACHE_PROFILING
    context->rt_data->pcache_allocated_property += 8 + sizeof(ES_Code::PropertyCache) * (property_get_caches_used + property_put_caches_used);
    context->rt_data->pcache_allocated_global += 8 + sizeof(ES_Code::GlobalCache) * global_accesses_count;
#endif // ES_PROPERTY_CACHE_PROFILING

    if (functions_count != 0)
    {
        unsigned function_declarations_count = 0, function_index;
        for (function_index = 0; function_index < functions_count; ++function_index)
            if (!code->functions[function_index]->GetData()->is_function_expr)
                ++function_declarations_count;
        ES_CodeStatic::FunctionDeclaration *fndecl = data->function_declarations = OP_NEWA_L(ES_CodeStatic::FunctionDeclaration, function_declarations_count);
        for (function_index = 0; function_index < functions_count; ++function_index)
        {
            ES_FunctionCode *fn = code->functions[function_index];
            if (!fn->GetData()->is_function_expr)
            {
                fndecl->function = function_index;
                ++fndecl;
            }
        }
        data->function_declarations_count = function_declarations_count;
    }
    else
    {
        data->function_declarations = NULL;
        data->function_declarations_count = 0;
    }

    data->global_accesses = global_accesses;
    data->global_accesses_count = global_accesses_count;
    global_accesses = NULL;
    global_accesses_count = 0;

    ExtractExceptionHandlers(data->exception_handlers, data->exception_handlers_count, toplevel_exception_handlers);
    toplevel_exception_handlers = NULL;

    data->inner_scopes = final_inner_scopes;
    data->inner_scopes_count = final_inner_scopes_count;
    final_inner_scopes = NULL;

    if (emit_debug_information && debug_records_count > 0)
    {
#ifdef ECMASCRIPT_DEBUGGER
        if (parser->GetIsDebugging())
        {
            data->debug_records = OP_NEWA_L(ES_CodeStatic::DebugRecord, debug_records_count);
            op_memcpy(data->debug_records, debug_records, sizeof *debug_records * debug_records_count);
        }
        else
#endif // ECMASCRIPT_DEBUGGER
            data->compressed_debug_records = ES_CodeStatic::DebugRecord::Compress(debug_records, debug_records_count);

        data->debug_records_count = debug_records_count;

        OP_DELETEA(debug_records);
        debug_records = NULL;
    }

#ifdef ES_NATIVE_SUPPORT
    if (loops_count == 0)
        data->loop_data = NULL;
    else
    {
        ES_CodeStatic::LoopData *loop_data = data->loop_data = OP_NEWA_L(ES_CodeStatic::LoopData, loops_count);
        data->loop_data_count = loops_count;

        Loop *loop = first_loop;
        for (unsigned index = 0; index < loops_count; ++index)
        {
            loop_data[index].start = loop->start;
            loop_data[index].jump = loop->jump;

            loop = loop->next;
        }

        code->loop_counters = OP_NEWA_L(unsigned, loops_count);
        op_memset(code->loop_counters, 0, loops_count * sizeof(code->loop_counters[0]));

        code->loop_dispatcher_codes = OP_NEWA_L(ES_Code *, loops_count);
        op_memset(code->loop_dispatcher_codes, 0, loops_count * sizeof(code->loop_dispatcher_codes[0]));
    }
#endif // ES_NATIVE_SUPPORT

    if (format_string_caches_used == 0)
        data->format_string_caches_count = 0;
    else
    {
        ES_Code::FormatStringCache *caches = code->format_string_caches = OP_NEWA_L(ES_Code::FormatStringCache, format_string_caches_used);
        data->format_string_caches_count = format_string_caches_used;

        op_memset(caches, 0, format_string_caches_used * sizeof caches[0]);
    }

    if (eval_caches_used == 0)
        data->eval_caches_count = 0;
    else
    {
        ES_Code::EvalCache *caches = code->eval_caches = OP_NEWA_L(ES_Code::EvalCache, eval_caches_used);
        data->eval_caches_count = eval_caches_used;

        op_memset(caches, 0, eval_caches_used * sizeof caches[0]);
    }

    if (switch_tables_used == 0)
        data->switch_tables_count = 0;
    else
    {
        ES_CodeStatic::SwitchTable *tables = data->switch_tables = OP_NEWA_L(ES_CodeStatic::SwitchTable, switch_tables_used);

        SwitchTable *switch_table = first_switch_table;
        while (switch_table)
        {
            ES_CodeStatic::SwitchTable &table = tables[switch_table->index];

            unsigned count = switch_table->switch_table->maximum - switch_table->switch_table->minimum + 1;
            table.codeword_indeces = OP_NEWA_L(unsigned, count);
            table.minimum = switch_table->switch_table->minimum;
            table.maximum = switch_table->switch_table->maximum;

            op_memcpy(table.codeword_indeces, switch_table->switch_table->codeword_indeces + table.minimum, count * sizeof(unsigned));
            table.codeword_indeces -= table.minimum;
            table.default_codeword_index = switch_table->switch_table->default_codeword_index;

            ++code->data->switch_tables_count;

            switch_table = switch_table->next;
        }

#ifdef ES_NATIVE_SUPPORT
        code->switch_tables = OP_NEWA_L(ES_Code::SwitchTable, switch_tables_used);
#endif // ES_NATIVE_SUPPORT
    }

    // Might be populated based on code->strings at some point.
    data->strings = NULL;
}

void
ES_Compiler::EmitInstruction(ES_Instruction instruction)
{
    if (instruction == ESI_RETURN_NO_VALUE && codetype == CODETYPE_EVAL)
        EmitInstruction(ESI_RETURN_FROM_EVAL, UINT_MAX);
    else
        WRITE1(instruction);
}

void
ES_Compiler::EmitInstruction(ES_Instruction instruction, const Register &reg1)
{
    if (instruction == ESI_RETURN_VALUE)
    {
        if (codetype == CODETYPE_EVAL)
            instruction = ESI_RETURN_FROM_EVAL;

        is_void_function = FALSE;
    }

    WRITE2(instruction, REGISTER(reg1));
}

void
ES_Compiler::EmitInstruction(ES_Instruction instruction, const Register &reg1, ES_CodeWord word1)
{
    if (instruction == ESI_COPY)
        OP_ASSERT(reg1.index != word1.index);

    WRITE3(instruction, REGISTER(reg1), word1);
}

void
ES_Compiler::EmitInstruction(ES_Instruction instruction, const Register &reg1, ES_CodeWord word1, ES_CodeWord word2)
{
    WRITE4(instruction, REGISTER(reg1), word1, word2);

    /* Should use EmitPropertyAccessor() instead. */
    OP_ASSERT(instruction != ESI_GETN_IMM && instruction != ESI_PUTN_IMM && instruction != ESI_INIT_PROPERTY);
}

void
ES_Compiler::EmitInstruction(ES_Instruction instruction, const Register &reg1, ES_CodeWord word1, ES_CodeWord word2, ES_CodeWord word3)
{
    WRITE5(instruction, REGISTER(reg1), word1, word2, word3);

    /* Should use EmitPropertyAccessor() instead. */
    OP_ASSERT(instruction != ESI_GETN_IMM && instruction != ESI_PUTN_IMM && instruction != ESI_INIT_PROPERTY);
}

void
ES_Compiler::EmitInstruction(ES_Instruction instruction, const Register &reg1, ES_CodeWord word1, ES_CodeWord word2, ES_CodeWord word3, ES_CodeWord word4)
{
    WRITE6(instruction, REGISTER(reg1), word1, word2, word3, word4);
}

#ifdef ES_COMBINED_ADD_SUPPORT

void
ES_Compiler::EmitADDN(const Register &dst, unsigned count, const Register *srcs)
{
    EnsureBytecodeAllocation(3 + count);

    *bytecode++ = ESI_ADDN;
    *bytecode++ = REGISTER(dst);
    *bytecode++ = count;

    for (unsigned index = 0; index < count; ++index)
        *bytecode++ = REGISTER(srcs[index]);
}

#endif // ES_COMBINED_ADD_SUPPORT

void
ES_Compiler::EmitCONSTRUCT_OBJECT(const Register &dst, unsigned class_index, unsigned count, const Register *values)
{
    EnsureBytecodeAllocation(3 + count);

    *bytecode++ = ESI_CONSTRUCT_OBJECT;
    *bytecode++ = REGISTER(dst);
    *bytecode++ = class_index;

    for (unsigned index = 0; index < count; ++index)
        *bytecode++ = REGISTER(values[index]);
}

void
ES_Compiler::EmitInstruction(ES_Instruction instruction, ES_CodeWord word1, ES_CodeWord word2)
{
    WRITE3(instruction, word1, word2);
}

void
ES_Compiler::EmitInstruction(ES_Instruction instruction, const Register &reg1, const JumpTarget &jt)
{
    unsigned location = BytecodeUsed() + 2;
    WRITE3(instruction, REGISTER(reg1), ES_CodeWord(jt.Address(location)));
}

void
ES_Compiler::EmitJump(const Register *condition_on, ES_Instruction instruction, const JumpTarget &jt)
{
    if (condition_on)
        EmitInstruction(ESI_CONDITION, *condition_on);

    unsigned location = BytecodeUsed() + 1;
    WRITE2(instruction, ES_CodeWord(jt.Offset(location, condition_on != NULL)));
}

void
ES_Compiler::EmitJump(const Register *condition_on, ES_Instruction instruction, const JumpTarget &jt, ES_CodeWord word2)
{
    if (condition_on)
        EmitInstruction(ESI_CONDITION, *condition_on);

    if (word2.index == loops_count - 1 && last_loop && last_loop->jump == UINT_MAX)
        last_loop->jump = BytecodeUsed();

    unsigned location = BytecodeUsed() + 1;
    WRITE3(instruction, ES_CodeWord(jt.Offset(location, condition_on != NULL)), word2);
}

void
ES_Compiler::EmitInstruction(ES_Instruction instruction, ES_CodeWord word1)
{
    WRITE2(instruction, word1);
}

void
ES_Compiler::EmitInstruction(ES_Instruction instruction, ES_CodeWord word1, ES_CodeWord word2, ES_CodeWord word3)
{
    WRITE4(instruction, word1, word2, word3);
}

void
ES_Compiler::EmitInstruction(ES_Instruction instruction, ES_CodeWord word1, ES_CodeWord word2, ES_CodeWord word3, ES_CodeWord word4)
{
    WRITE5(instruction, word1, word2, word3, word4);
}

void
ES_Compiler::EmitPropertyAccessor(ES_Instruction instruction, JString *name, const Register &obj, const Register &value)
{
    Register use_value(value);

    if (!use_value.IsValid())
        use_value = Temporary();

    ES_Instruction use_instruction;

    if (name == ident_length)
        if (instruction == ESI_GETN_IMM)
            use_instruction = ESI_GET_LENGTH;
        else
            use_instruction = ESI_PUT_LENGTH;
    else
        use_instruction = instruction;

    if (instruction == ESI_GETN_IMM)
        WRITE5(use_instruction, use_value, obj, Identifier(name), GetPropertyGetCacheIndex(name));
    else
        WRITE5(use_instruction, obj, Identifier(name), use_value, GetPropertyPutCacheIndex(name));
}

void
ES_Compiler::EmitPropertyAccessor(ES_Instruction instruction, const Register &name, const Register &obj, const Register &value)
{
    if (instruction == ESI_GET)
    {
        unsigned value_index = value.index;

        if (want_object)
            value_index |= 0x80000000u;

        WRITE4(instruction, value_index, obj, name);
    }
    else
        WRITE4(instruction, obj, name, value);
}

unsigned
ES_Compiler::RecordGlobalCacheAccess(JString *name, ES_CodeStatic::GlobalCacheAccess kind)
{
    if (kind != ES_CodeStatic::GetScope)
    {
        unsigned index;
        if (global_caches_table->Find(name, index))
        {
            global_accesses[index] |= kind;
            return index;
        }
        else
            global_caches_table->AddL(context, name, global_accesses_count);
    }

    ES_CodeWord::Index name_index = Identifier(name);

    /* Just a sanity check if your word is wider than 32-bits. */
    OP_ASSERT((name_index & ~0xc0000000) < (1 << 29));

    /* The two highest bits are flags, and we keep them as the two
       highest bits after shifting the value 3 bits to the left. */
    name_index = (name_index & 0xc0000000) | (name_index << 3);

    if (global_accesses_count == 0 || (global_accesses_count & (global_accesses_count - 1)) == 0 && (global_accesses_count & 7) == 0)
    {
        unsigned new_size = global_accesses_count == 0 ? 8 : global_accesses_count + global_accesses_count;
        unsigned *new_array = OP_NEWA_L(unsigned, new_size);

        op_memcpy(new_array, global_accesses, global_accesses_count * sizeof global_accesses[0]);
        OP_DELETEA(global_accesses);
        global_accesses = new_array;
    }

    unsigned cache_index = global_accesses_count++;
    global_accesses[cache_index] = name_index | kind;

    return cache_index;
}

void
ES_Compiler::EmitGlobalAccessor(ES_Instruction instruction, JString *name, const Register &reg, BOOL quiet)
{
    BOOL is_get = instruction == ESI_GET_GLOBAL;
    ES_CodeWord::Index name_index = Identifier(name);
    unsigned cache_index = RecordGlobalCacheAccess(name, is_get ? ES_CodeStatic::GetGlobal : ES_CodeStatic::PutGlobal);
    if (is_get)
    {
        Register actual_dst(reg.IsValid() ? reg : Temporary());
        unsigned dst_index = actual_dst.index;
        if (quiet)
            dst_index |= 0x80000000u;
        WRITE4(instruction, dst_index, name_index, cache_index);
    }
    else
        WRITE4(instruction, name_index, REGISTER(reg), cache_index);
}

void
ES_Compiler::EmitScopedAccessor(ES_Instruction instruction, JString *name, const Register *reg1, const Register *reg2, BOOL for_call_or_typeof)
{
    if (instruction == ESI_GET_SCOPE_REF)
    {
        EnsureBytecodeAllocation(6);
        uses_get_scope_ref = TRUE;
    }
    else if (instruction == ESI_DELETE_SCOPE)
        EnsureBytecodeAllocation(4);
    else
        EnsureBytecodeAllocation(5);

    *bytecode++ = instruction;

    unsigned dst_index;

    switch (instruction)
    {
    case ESI_GET_SCOPE:
        dst_index = reg1->index;
        if (for_call_or_typeof)
            dst_index |= 0x80000000u;
        *bytecode++ = dst_index;
        *bytecode++ = Identifier(name);
        break;

    case ESI_GET_SCOPE_REF:
        dst_index = reg1->index;
        if (for_call_or_typeof)
            dst_index |= 0x80000000u;
        *bytecode++ = dst_index;
        *bytecode++ = reg2->index;
        *bytecode++ = Identifier(name);
        break;

    case ESI_PUT_SCOPE:
        *bytecode++ = Identifier(name);
        *bytecode++ = reg1->index;
        break;

    case ESI_DELETE_SCOPE:
        *bytecode++ = Identifier(name);
        break;
    }

    *bytecode++ = GetInnerScopeIndex();

    Register local(Local(name));

    if (local.IsValid())
    {
        /* This would only happen within a with-statement, and value tracking
           should be disabled entirely in functions containing with-statements. */
        OP_ASSERT(suspend_value_tracking);

        *bytecode++ = local.index;
    }
    else
        *bytecode++ = UINT_MAX;

    if (instruction == ESI_GET_SCOPE || instruction == ESI_GET_SCOPE_REF)
        *bytecode++ = RecordGlobalCacheAccess(name, ES_CodeStatic::GetScope);

    uses_lexical_scope = TRUE;
}

void
ES_Compiler::EnsureBytecodeAllocation(unsigned nwords)
{
    unsigned bytecode_used = BytecodeUsed();
    if (BytecodeUsed() + nwords >= bytecode_allocated)
    {
        unsigned new_bytecode_allocated = bytecode_allocated == 0 ? 256 : bytecode_allocated * 2;
        ES_CodeWord *new_bytecode = OP_NEWA_L(ES_CodeWord, new_bytecode_allocated);

        op_memcpy(new_bytecode, bytecode_base, bytecode_used * sizeof bytecode[0]);
        OP_DELETEA(bytecode_base);

        bytecode_base = new_bytecode;
        bytecode = bytecode_base + bytecode_used;
        bytecode_allocated = new_bytecode_allocated;
    }
}

BOOL
ES_Compiler::SafeAsThisRegister(const Register &reg)
{
    if (reg.IsTemporary() && (!current_exception_handler || reg != global_result_value))
    {
        CaughtException *ce = caught_exception;
        while (ce)
            if (ce->value == reg)
                return FALSE;
            else
                ce = ce->previous;
        return TRUE;
    }
    else
        return FALSE;
}


void
ES_Compiler::StoreRegisterValue(const ES_Compiler::Register &dst, const ES_Value_Internal &value)
{
    OP_ASSERT(!value.IsObject());

    if (value.IsInt32())
        EmitInstruction(ESI_LOAD_INT32, dst, value.GetNumAsInt32());
    else if (value.IsDouble())
        EmitInstruction(ESI_LOAD_DOUBLE, dst, Double(value.GetNumAsDouble()));
    else if (value.IsString())
        EmitInstruction(ESI_LOAD_STRING, dst, String(value.GetString()));
    else if (value.IsBoolean())
        EmitInstruction(value.GetBoolean() ? ESI_LOAD_TRUE : ESI_LOAD_FALSE, dst);
    else if (value.IsNull())
        EmitInstruction(ESI_LOAD_NULL, dst);
    else
        EmitInstruction(ESI_LOAD_UNDEFINED, dst);

    if (ValueTracked(dst))
        SetLocalValue(dst, value, TRUE);
}

BOOL
ES_Compiler::ValueTracked(const Register &reg)
{
    return !suspend_value_tracking && IsLocal(reg) && !IsFormal(reg) && !HasNestedWrite(reg);
}

void
ES_Compiler::SetLocalValue(const Register &reg, const ES_Value_Internal &value, BOOL value_stored)
{
    if (ValueTracked(reg))
    {
        unsigned index = reg.index - 2;

        if (local_value_known[index] && ES_Value_Internal::IsSameValue(context, known_local_value[index], value))
        {
            if (!local_value_stored[index] && value_stored)
                local_value_stored[index] = local_value_snapshot;
        }
        else
        {
            known_local_value[index] = value;
            local_value_known[index] = TRUE;
            local_value_stored[index] = value_stored ? local_value_snapshot : 0;
            local_value_generation[index] = local_value_snapshot;

            if (!value_stored && (IsFormal(reg) || HasNestedRead(reg)))
                StoreRegisterValue(reg, value);
        }
    }
    else
        StoreRegisterValue(reg, value);
}

BOOL
ES_Compiler::GetLocalValue(ES_Value_Internal &value, const Register &reg, BOOL &is_stored)
{
    if (ValueTracked(reg))
    {
        unsigned index = reg.index - 2;

        if (IsLocal(reg) && local_value_known[index])
        {
            value = known_local_value[index];
            is_stored = local_value_stored[index] != 0;
            return TRUE;
        }
    }

    return FALSE;
}

void
ES_Compiler::ResetLocalValue(const Register &reg, BOOL flush)
{
    if (ValueTracked(reg))
    {
        if (flush)
            FlushLocalValue(reg);

        local_value_known[reg.index - 2] = FALSE;
    }
}

void
ES_Compiler::FlushLocalValue(const Register &reg)
{
    if (ValueTracked(reg))
    {
        unsigned index = reg.index - 2;

        if (local_value_known[index] && !local_value_stored[index])
            StoreRegisterValue(reg, known_local_value[index]);
    }
}

void
ES_Compiler::ResetLocalValues(unsigned snapshot, BOOL flush)
{
    if (!suspend_value_tracking)
        for (unsigned index = 0; index < locals_table->GetCount(); ++index)
            if (local_value_known[index])
                if (local_value_generation[index] > snapshot)
                    ResetLocalValue(Register(2 + index), flush);
                else if (local_value_stored[index] > snapshot)
                    local_value_stored[index] = 0;
}

void
ES_Compiler::FlushLocalValues(unsigned snapshot)
{
    if (!suspend_value_tracking)
    {
        /* We don't want our StoreRegisterValue() calls to track the stored
           values further. */
        SuspendValueTracking();

        for (unsigned index = 0; index < locals_table->GetCount(); ++index)
            if (local_value_known[index] && local_value_generation[index] > snapshot && !local_value_stored[index])
                StoreRegisterValue(Register(2 + index), known_local_value[index]);

        ResumeValueTracking();
    }
}

void *
ES_Compiler::BackupLocalValues()
{
    if (!suspend_value_tracking)
    {
        unsigned locals_count = locals_table->GetCount(), known_count = 0;

        for (unsigned index = 0; index < locals_count; ++index)
            if (local_value_known[index])
                ++known_count;

        if (known_count != 0)
        {
            void *data = OP_NEWGROA_L(char, sizeof(ES_Value_Internal) + known_count * (sizeof(ES_Value_Internal) + sizeof(unsigned) + sizeof(unsigned) + sizeof(unsigned)), Arena());

            ES_Value_Internal *values = static_cast<ES_Value_Internal *>(data);
            unsigned *indexes = reinterpret_cast<unsigned *>(values + known_count + 1);
            unsigned *generations = indexes + known_count;
            unsigned *stored = generations + known_count;

            values[0].SetInt32(static_cast<int>(known_count));
            ++values;

            for (unsigned index = 0, value_index = 0; index < locals_count; ++index)
                if (local_value_known[index])
                {
                    values[value_index] = known_local_value[index];
                    indexes[value_index] = index;
                    generations[value_index] = local_value_generation[index];
                    stored[value_index] = local_value_stored[index];

                    ++value_index;
                }

            return data;
        }
    }

    return NULL;
}

void
ES_Compiler::RestoreLocalValues(void *data)
{
    if (!suspend_value_tracking)
    {
        unsigned locals_count = locals_table->GetCount();

        op_memset(local_value_known, 0, locals_count * sizeof *local_value_known);

        if (data)
        {
            ES_Value_Internal *values = static_cast<ES_Value_Internal *>(data);

            unsigned known_count = values++->GetInt32();

            unsigned *indexes = reinterpret_cast<unsigned *>(values + known_count);
            unsigned *generations = indexes + known_count;
            unsigned *stored = generations + known_count;

            for (unsigned value_index = 0; value_index < known_count; ++value_index)
            {
                unsigned index = indexes[value_index];

                local_value_known[index] = TRUE;
                known_local_value[index] = values[value_index];
                local_value_generation[index] = generations[value_index];
                local_value_stored[index] = stored[value_index];
            }
        }
    }
}

void
ES_Compiler::RecordNestedRead(JString *name)
{
    unsigned index;
    if (locals_table && locals_table->IndexOf(name, index))
        local_value_nested_read[index >> 5] |= 1 << (index & 0x1f);
}

void
ES_Compiler::RecordNestedWrite(JString *name)
{
    unsigned index;
    if (locals_table && locals_table->IndexOf(name, index))
        local_value_nested_write[index >> 5] |= 1 << (index & 0x1f);
}

BOOL
ES_Compiler::HasNestedRead(const Register &reg)
{
    OP_ASSERT(IsLocal(reg));

    unsigned index = reg.index - 2;
    return (local_value_nested_read[index >> 5] & (1 << (index & 0x1f))) != 0;
}

BOOL
ES_Compiler::HasNestedWrite(const Register &reg)
{
    unsigned index = reg.index - 2;
    return (local_value_nested_write[index >> 5] & (1 << (index & 0x1f))) != 0;
}

JString *
ES_Compiler::GetLocal(const Register &reg)
{
    OP_ASSERT(IsLocal(reg));

    JString *name;

    if (closures_count != 0)
    {
        ES_Property_Info info;
        closures[0]->klass->Lookup(ES_PropertyIndex(reg.index - 2), name, info);
    }
    else
        locals_table->Lookup(ES_PropertyIndex(reg.index - 2), name);

    return name;
}

ES_Compiler::Register
ES_Compiler::Local(JString *name, BOOL for_assignment/* = FALSE*/)
{
    CaughtException *ce = caught_exception;
    while (ce)
        if (ce->name == name)
            return ce->value;
        else
            ce = ce->previous;

    if (closures_count != 0)
    {
        ES_FunctionCode *code = closures[0];
        code->ConstructClass(context);

        ES_PropertyIndex index;
        if (code->klass->Find(name, index, code->klass->Count()))
            return FixedRegister(2 + index);
    }

    unsigned index;

    if (locals_table && locals_table->IndexOf(name, index))
        return FixedRegister(index + 2);
    else if (is_function_expr && !HasUnknownScope() && name == current_function_name)
        if (for_assignment)
        {
            if (is_strict_mode)
                EmitInstruction(ESI_THROW_BUILTIN, ES_CodeStatic::TYPEERROR_INVALID_ASSIGNMENT);

            return Temporary(FALSE, NULL);
        }
        else
            return FixedRegister(1);
    else
        return Register();
}

ES_Compiler::Register
ES_Compiler::Temporary(BOOL for_call_arguments, const Register *available_temporary)
{
    if (available_temporary && available_temporary->index == highest_used_temporary_index)
        return *available_temporary;
    else
    {
        TemporaryRegister *temporary;

        /* Our list of unused temporary registers contains only registers that
           are followed by used temporary registers (at higher indeces.)  So for
           call arguments, where we want a list of registers at the end of the
           active register frame, we will just never reuse an unused temporary
           register below highest_used_temporary_index. */

        if (for_call_arguments || !first_unused_temporary)
            if (high_unused_temporaries)
            {
                temporary = high_unused_temporaries;
                high_unused_temporaries = temporary->next;
                temporary->next = NULL;
                ++highest_used_temporary_index;
                OP_ASSERT(temporary->index == highest_used_temporary_index);
            }
            else
                temporary = OP_NEWGRO_L(TemporaryRegister, (this), parser->Arena());
        else
        {
            temporary = first_unused_temporary;
            OP_ASSERT(temporary->prev == NULL);
            first_unused_temporary = temporary->next;
            temporary->next = NULL;
            if (!first_unused_temporary)
                last_unused_temporary = NULL;
            else
                first_unused_temporary->prev = NULL;
        }

        return Register(temporary);
    }
}

ES_Compiler::JumpTarget
ES_Compiler::ForwardJump()
{
    Jump *jump = AllocateJump();

    jump->target_address = ~0u - ++forward_jump_id_counter;

    return JumpTarget(jump);
}

ES_Compiler::JumpTarget
ES_Compiler::BackwardJump()
{
    Jump *jump = AllocateJump();

    jump->SetPointer(&first_jump);
    first_jump = jump;

    jump->target_address = maximum_jump_target = BytecodeUsed();

    return JumpTarget(jump);
}

void
ES_Compiler::SetJumpTarget(const JumpTarget &jt)
{
    if (JumpOffset *jump_offset = jt.jump->first_offset)
    {
        BOOL has_deleted = FALSE;

        do
        {
            ES_CodeWord::Offset offset = BytecodeUsed() - (jump_offset->bytecode_index);
            unsigned operands = g_instruction_operand_count[bytecode_base[jump_offset->bytecode_index - 1].instruction];

            if (offset == static_cast<ES_CodeWord::Offset>(operands) && maximum_jump_target < BytecodeUsed())
            {
                /* Delete jump instruction instead.  This will occur rather
                   frequently because of how the compiler compiles expressions
                   as conditions with two explicit jump targets, one of which
                   will usually be right after the condition. */
                bytecode -= 1 + operands + (jump_offset->has_condition ? 2 : 0);
                has_deleted = TRUE;
            }
            else
            {
                bytecode_base[jump_offset->bytecode_index].offset = offset;
                maximum_jump_target = BytecodeUsed();
            }

            jump_offset = jump_offset->next;
        }
        while (jump_offset);

        if (has_deleted)
        {
            unsigned bytecode_used = BytecodeUsed();
            Jump *jump = first_jump;

            while (jump && jump->target_address > bytecode_used)
            {
                jump->target_address = bytecode_used;
                jump = jump->previous;
            }
        }
    }

    if (jt.jump->used_in_table_switch)
        maximum_jump_target = BytecodeUsed();

    if (JumpOffset *jump_address = jt.jump->first_address)
    {
        do
        {
            bytecode_base[jump_address->bytecode_index].immediate = BytecodeUsed();
            maximum_jump_target = BytecodeUsed();

            jump_address = jump_address->next;
        }
        while (jump_address);
    }

    jt.jump->target_address = BytecodeUsed();

    if (current_exception_handler && current_exception_handler->handler_target == jt)
    {
        ExceptionHandler *handler = current_exception_handler;
        current_exception_handler = handler->previous;

        CodeExceptionHandlerElement *code_handler_element = OP_NEWGRO_L(CodeExceptionHandlerElement, (), Arena());
        ES_CodeStatic::ExceptionHandler &code_handler = code_handler_element->code_handler;

        code_handler.type = handler->is_finally ? ES_CodeStatic::ExceptionHandler::TYPE_FINALLY : ES_CodeStatic::ExceptionHandler::TYPE_CATCH;
        code_handler.start = handler->start_index;
        code_handler.end = handler->end_index;
        code_handler.handler_ip = BytecodeUsed();

        ExtractExceptionHandlers(code_handler.nested_handlers, code_handler.nested_handlers_count, handler->nested);
        handler->nested = NULL;

        if (current_exception_handler)
        {
            code_handler_element->previous = current_exception_handler->nested;
            current_exception_handler->nested = code_handler_element;
        }
        else
        {
            code_handler_element->previous = toplevel_exception_handlers;
            toplevel_exception_handlers = code_handler_element;
        }
    }
}

void
ES_Compiler::PushRethrowTarget(const Register &reg)
{
    RethrowTarget *rethrow_target = OP_NEW_L(RethrowTarget, ());
    rethrow_target->target = reg;
    rethrow_target->previous = current_rethrow_target;
    current_rethrow_target = rethrow_target;
}

void
ES_Compiler::PopRethrowTarget()
{
    RethrowTarget *rethrow_target = current_rethrow_target;
    current_rethrow_target = rethrow_target->previous;
    OP_DELETE(rethrow_target);
}

void
ES_Compiler::PushFinallyTarget(const JumpTarget &target)
{
    FinallyTarget *finally = OP_NEW_L(FinallyTarget, ());
    finally->jump_target = target;
    finally->previous = current_finally_target;
    current_finally_target = finally;

    PushContinuationTarget(target, TARGET_FINALLY, NULL);
}

void
ES_Compiler::PopFinallyTarget()
{
    PopContinuationTargets(1);

    FinallyTarget *finally = current_finally_target;
    current_finally_target = current_finally_target->previous;
    OP_DELETE(finally);
}

const ES_Compiler::JumpTarget *
ES_Compiler::GetFinallyTarget()
{
    return current_finally_target ? &current_finally_target->jump_target : NULL;
}

void
ES_Compiler::PushContinuationTarget(const JumpTarget &target, ContinuationTargetType t, JString *label)
{
    ContinuationTarget *new_target = OP_NEW_L(ContinuationTarget, ());

    new_target->type = t;
    new_target->jump_target = target;
    new_target->label = label;
    new_target->previous = current_continuation_target;
    current_continuation_target = new_target;
}

void
ES_Compiler::PopContinuationTargets(unsigned c)
{
    while (c-- != 0)
    {
        ContinuationTarget *target = current_continuation_target;
        current_continuation_target = target->previous;
        OP_DELETE(target);
    }
}

const ES_Compiler::JumpTarget *
ES_Compiler::FindBreakTarget(BOOL &crosses_finally, JString *label)
{
    ContinuationTarget *target = current_continuation_target;

    while (target)
        if (target->type == TARGET_BREAK && target->label == label)
            return &target->jump_target;
        else
        {
            if (target->type == TARGET_FINALLY)
                crosses_finally = TRUE;
            target = target->previous;
        }

    return NULL;
}

const ES_Compiler::JumpTarget *
ES_Compiler::FindContinueTarget(BOOL &crosses_finally, JString *label)
{
    ContinuationTarget *target = current_continuation_target;

    while (target)
        if (target->type == TARGET_CONTINUE && target->label == label)
            return &target->jump_target;
        else
        {
            if (target->type == TARGET_FINALLY)
                crosses_finally = TRUE;
            target = target->previous;
        }

    return NULL;
}

BOOL
ES_Compiler::IsDuplicateLabel(JString *label)
{
    OP_ASSERT(label);

    for (ContinuationTarget *bset = current_continuation_target; bset; bset = bset->previous)
        if (label == bset->label)
            return TRUE;

    for (LabelSet *lset = current_unused_label_set; lset; lset = lset->previous)
        if (label == lset->label)
            return TRUE;

    return FALSE;
}

void
ES_Compiler::PushLabel(JString *label)
{
    LabelSet *set = OP_NEWGRO_L(LabelSet, (), Arena());

    set->label = label;
    set->previous = current_unused_label_set;
    current_unused_label_set = set;
}

JString *
ES_Compiler::PopLabel()
{
    JString *label = NULL;
    if (current_unused_label_set)
    {
        LabelSet *set = current_unused_label_set;
        label = set->label;
        current_unused_label_set = current_unused_label_set->previous;
    }
    return label;
}

BOOL
ES_Compiler::GetGlobalIndex(unsigned &index, JString *name)
{
    return global_object->GetVariableIndex(index, name);
}

ES_CodeWord::Index
ES_Compiler::Identifier(JString *name)
{
    return String(name);
}

ES_CodeWord::Index
ES_Compiler::String(JString *string)
{
    unsigned index;

    if (string->StaticIndex(index))
        return index | 0x40000000u;
    else if (!strings_table->IndexOf(string, index))
        strings_table->AppendL(context, string, index);

    return index;
}

JString *
ES_Compiler::String(ES_CodeWord::Index index)
{
    if (index & 0x40000000u)
        return context->rt_data->strings[index & 0x3fffffffu];
    else
    {
        JString *string;
        strings_table->Lookup(index, string);
        return string;
    }
}

ES_CodeWord::Index
ES_Compiler::Double(double value)
{
    if (doubles_count == 0 || (doubles_count & (doubles_count - 1)) == 0)
    {
        unsigned new_size = doubles_count == 0 ? 8 : doubles_count + doubles_count;
        double *new_array = OP_NEWA_L(double, new_size);

        op_memcpy(new_array, doubles, doubles_count * sizeof doubles[0]);
        OP_DELETEA(doubles);

        doubles = new_array;
    }
    doubles[doubles_count] = value;

    return doubles_count++;
}

ES_CodeWord::Index
ES_Compiler::RegExp(ES_RegExp_Information *regexp)
{
    /* regexp info is parser-allocated and not guaranteed to survive past its lifetime; copy it,
       but share the embedded regexp. */
    ES_RegExp_Information *re = OP_NEW_L(ES_RegExp_Information, ());
    *re = *regexp;

    if (OpStatus::IsError(regexps.Add(re)))
    {
        OP_DELETE(re);
        LEAVE(OpStatus::ERR_NO_MEMORY);
    }
    if (re->regexp)
        re->regexp->IncRef();

    return regexps.GetCount() - 1;
}

ES_CodeWord::Index
ES_Compiler::Function(ES_FunctionCode *function_code)
{
    for (unsigned index = functions_offset; index < functions_count; ++index)
        if (functions[index] == function_code)
        {
            functions_offset = index + 1;
            return index;
        }
    if (functions_offset != 0)
        for (unsigned index = 0; index < functions_offset; ++index)
            if (functions[index] == function_code)
            {
                functions_offset = index + 1;
                return index;
            }
    OP_ASSERT(FALSE);
    return ~0u;
}

void
ES_Compiler::AddConstantArrayLiteral(ES_CodeStatic::ConstantArrayLiteral *&cal, unsigned &cal_index, unsigned elements_count, unsigned array_length)
{
    cal = OP_NEWGRO_L(ES_CodeStatic::ConstantArrayLiteral, (), parser->Arena());
    cal_index = constant_array_literals.GetCount();

    cal->elements_count = elements_count;
    cal->array_length = array_length;
    cal->indeces = OP_NEWA(unsigned, elements_count);
    cal->values = OP_NEWA(ES_CodeStatic::ConstantArrayLiteral::Value, elements_count);

    if (!cal->indeces || !cal->values || OpStatus::IsMemoryError(constant_array_literals.Add(cal)))
    {
        OP_DELETEA(cal->indeces);
        OP_DELETEA(cal->values);
        LEAVE(OpStatus::ERR_NO_MEMORY);
    }
}

BOOL
ES_Compiler::GetLexical(ES_CodeWord::Index &scope_index, ES_CodeWord::Index &variable_index, JString *name, BOOL &is_read_only)
{
    if (codetype != CODETYPE_FUNCTION || HasInnerScopes() || UsesEval())
        return FALSE;

    ES_PropertyIndex property_index;

    scope_index = 0;

    if (is_function_expr && current_function_name)
        if (current_function_name->Equals(name))
        {
            variable_index = 0;
            is_read_only = TRUE;
            uses_lexical_scope = TRUE;

            return TRUE;
        }
        else
            ++scope_index;

    for (unsigned index = 0; index < lexical_scopes_count; ++index, ++scope_index)
        if (lexical_scopes[index]->klass->Find(name, property_index, lexical_scopes[index]->klass->Count()))
        {
            variable_index = property_index;
            is_read_only = FALSE;
            uses_lexical_scope = TRUE;

            return TRUE;
        }
        else if (lexical_scopes[index]->GetData()->is_function_expr)
            if (JString *function_name = lexical_scopes[index]->GetName())
            {
                ++scope_index;

                if (function_name->Equals(name))
                {
                    variable_index = 0;
                    is_read_only = TRUE;
                    uses_lexical_scope = TRUE;

                    return TRUE;
                }
            }

    return FALSE;
}

void
ES_Compiler::AddVariable(JString *name, BOOL is_argument)
{
    if (locals_table)
    {
        unsigned index;

        if (locals_table->AppendL(context, name, index, is_argument))
        {
            String(name);

            if (!is_argument && name == context->rt_data->idents[ESID_arguments])
                uses_arguments = TRUE;
        }
    }
    else
        AddGlobalDeclaration(name);
}

void
ES_Compiler::AddGlobalDeclaration(JString *name)
{
    unsigned index;
    globals_table->AppendL(context, name, index);
    String(name);
}

void
ES_Compiler::PushInnerScope(const Register &robject)
{
    if (current_inner_scopes_count == current_inner_scopes_allocated)
    {
        unsigned new_allocated = current_inner_scopes_allocated == 0 ? 8 : current_inner_scopes_allocated * 2;
        unsigned *new_scopes = OP_NEWA_L(unsigned, new_allocated);

        op_memcpy(new_scopes, current_inner_scopes, current_inner_scopes_allocated * sizeof(unsigned));
        OP_DELETEA(current_inner_scopes);

        current_inner_scopes = new_scopes;
        current_inner_scopes_allocated = new_allocated;
    }

    current_inner_scopes[current_inner_scopes_count++] = robject.index;
    cached_inner_scopes_index = ~0u;
}

void
ES_Compiler::PopInnerScope()
{
    --current_inner_scopes_count;
    cached_inner_scopes_index = ~0u;
}

unsigned
ES_Compiler::GetInnerScopeIndex()
{
    if (current_inner_scopes_count == 0)
        return ~0u;
    else
    {
        if (cached_inner_scopes_index != ~0u)
            return cached_inner_scopes_index;
        else
            for (unsigned i = 0, j; i < final_inner_scopes_count; ++i)
            {
                ES_CodeStatic::InnerScope &final_inner_scope = final_inner_scopes[i];

                if (final_inner_scope.registers_count == current_inner_scopes_count)
                {
                    for (j = 0; j < current_inner_scopes_count; ++j)
                        if (final_inner_scope.registers[j] != current_inner_scopes[j])
                            break;

                    if (j == current_inner_scopes_count)
                        return cached_inner_scopes_index = i;
                }
            }

        ES_CodeStatic::InnerScope *new_scopes = OP_NEWA_L(ES_CodeStatic::InnerScope, final_inner_scopes_count + 1);

        op_memcpy(new_scopes, final_inner_scopes, final_inner_scopes_count * sizeof(ES_CodeStatic::InnerScope));
        op_memset(final_inner_scopes, 0, final_inner_scopes_count * sizeof(ES_CodeStatic::InnerScope));
        OP_DELETEA(final_inner_scopes);

        final_inner_scopes = new_scopes;
        ES_CodeWord::Index *registers = final_inner_scopes[final_inner_scopes_count].registers = OP_NEWA_L(ES_CodeWord::Index, current_inner_scopes_count);
        final_inner_scopes[final_inner_scopes_count].registers_count = current_inner_scopes_count;

        for (unsigned index = 0; index < current_inner_scopes_count; ++index)
            registers[index] = current_inner_scopes[index];

        return cached_inner_scopes_index = final_inner_scopes_count++;
    }
}

unsigned
ES_Compiler::AddObjectLiteralClass(ES_CodeStatic::ObjectLiteralClass *&klass)
{
    ObjectLiteralClass *olc = OP_NEWGRO_L(ObjectLiteralClass, (), Arena());

    olc->index = object_literal_classes_used++;
    olc->next = object_literal_classes;

    object_literal_classes = olc;

    klass = &olc->klass;
    return olc->index;
}

void
ES_Compiler::StartExceptionHandler(const JumpTarget &handler_target, BOOL is_finally)
{
    ExceptionHandler *handler = OP_NEWGRO_L(ExceptionHandler, (), Arena());

    handler->handler_target = handler_target;
    handler->is_finally = is_finally;
    handler->start_index = BytecodeUsed();
    handler->end_index = ~0u;
    handler->nested = NULL;

    handler->previous = current_exception_handler;
    current_exception_handler = handler;
}

void
ES_Compiler::EndExceptionHandler()
{
    OP_ASSERT(current_exception_handler->end_index == ~0u);
    current_exception_handler->end_index = BytecodeUsed();
}

void
ES_Compiler::PushCaughtException(JString *name, const Register &value)
{
    CaughtException *pushed = OP_NEW_L(CaughtException, ());
    pushed->name = name;
    pushed->value = value;
    pushed->previous = caught_exception;
    caught_exception = pushed;
}

void
ES_Compiler::PopCaughtException()
{
    OP_ASSERT(caught_exception != NULL);

    CaughtException *popped = caught_exception;
    caught_exception = popped->previous;
    OP_DELETE(popped);
}

BOOL
ES_Compiler::IsInTryBlock()
{
    ExceptionHandler *handler = current_exception_handler;
    while (handler)
        if (handler->end_index == ~0u)
            return TRUE;
        else
            handler = handler->previous;
    return FALSE;
}

void
ES_Compiler::AssignedVariable(JString *name)
{
#ifdef ES_NATIVE_SUPPORT
    for (CurrentSimpleLoopVariable *cslv = current_simple_loop_variable; cslv; cslv = cslv->previous)
        if (cslv->name == name)
            cslv->tainted = TRUE;
#endif // ES_NATIVE_SUPPORT
}

void
ES_Compiler::StartSimpleLoopVariable(JString *name, int lower_bound, int upper_bound)
{
#ifdef ES_NATIVE_SUPPORT
    if (codetype == CODETYPE_FUNCTION)
    {
        CurrentSimpleLoopVariable *cslv = OP_NEW_L(CurrentSimpleLoopVariable, ());
        cslv->name = name;
        cslv->lower_bound = lower_bound;
        cslv->upper_bound = upper_bound;
        cslv->start_index = BytecodeUsed();
        cslv->end_index = UINT_MAX;
        cslv->tainted = FALSE;
        cslv->previous = current_simple_loop_variable;
        current_simple_loop_variable = cslv;
    }
#endif // ES_NATIVE_SUPPORT
}

void
ES_Compiler::EndSimpleLoopVariable(JString *name)
{
#ifdef ES_NATIVE_SUPPORT
    if (codetype == CODETYPE_FUNCTION)
    {
        OP_ASSERT(current_simple_loop_variable->name == name);

        CurrentSimpleLoopVariable *cslv = current_simple_loop_variable;
        current_simple_loop_variable = cslv->previous;

        if (!cslv->tainted)
        {
            ES_CodeStatic::VariableRangeLimitSpan *vrls = OP_NEW(ES_CodeStatic::VariableRangeLimitSpan, ());
            if (!vrls)
            {
                OP_DELETE(cslv);
                LEAVE(OpStatus::ERR_NO_MEMORY);
            }

            locals_table->IndexOf(name, vrls->index);

            vrls->index += 2;
            vrls->lower_bound = cslv->lower_bound;
            vrls->upper_bound = cslv->upper_bound;
            vrls->start = cslv->start_index;
            vrls->end = BytecodeUsed();
            vrls->next = NULL;

            ES_CodeStatic::VariableRangeLimitSpan **ptr = &variable_range_limit_span;
            while (*ptr)
                ptr = &(*ptr)->next;
            *ptr = vrls;
        }

        OP_DELETE(cslv);
    }
#endif // ES_NATIVE_SUPPORT
}

unsigned
ES_Compiler::AddLoop(unsigned start)
{
    if (codetype != CODETYPE_FUNCTION)
    {
        Loop *loop = OP_NEWGRO_L(Loop, (), Arena());

        loop->start = start;
        loop->jump = UINT_MAX;
        loop->next = NULL;

        if (!first_loop)
            first_loop = last_loop = loop;
        else
            last_loop = last_loop->next = loop;

        return loops_count++;
    }
    else
        return UINT_MAX;
}

ES_CodeStatic::SwitchTable *
ES_Compiler::AddSwitchTable(unsigned &index, int minimum, int maximum)
{
    SwitchTable *switch_table = OP_NEWGRO_L(SwitchTable, (), Arena());

    index = switch_table->index = switch_tables_used++;
    switch_table->switch_table = OP_NEWGRO_L(ES_CodeStatic::SwitchTable, (minimum, maximum), Arena());
    switch_table->switch_table->codeword_indeces = OP_NEWGROA_L(unsigned, maximum - minimum + 1, Arena()) - minimum;
    switch_table->next = first_switch_table;

    first_switch_table = switch_table;
    return switch_table->switch_table;
}


class ES_SingleSimpleEval
    : public ES_StatementVisitor
{
public:
    ES_SingleSimpleEval(ES_Compiler &compiler)
        : result(TRUE),
          compiler(compiler),
          found(FALSE),
          flag(0)
    {
    }

    virtual BOOL Visit(ES_Expression *expression);
    virtual BOOL Enter(ES_Statement *statement, BOOL &skip);
    virtual BOOL Leave(ES_Statement *statement);

    BOOL result;

private:
    ES_Compiler &compiler;
    BOOL found;
    unsigned flag;
};

/* virtual */ BOOL
ES_SingleSimpleEval::Visit(ES_Expression *expression)
{
    if (expression->GetType() == ES_Expression::TYPE_CALL)
    {
        ES_CallExpr *call = static_cast<ES_CallExpr *>(expression);

        if (call->IsEvalCall(compiler))
            if (found || flag != 0)
                return result = FALSE;
            else
                found = TRUE;
    }

    return result;
}

/* virtual */ BOOL
ES_SingleSimpleEval::Enter(ES_Statement *statement, BOOL &skip)
{
    switch (statement->GetType())
    {
    case ES_Statement::TYPE_WHILE:
    case ES_Statement::TYPE_FOR:
    case ES_Statement::TYPE_FORIN:
        ++flag;
    }

    return result;
}

/* virtual */ BOOL
ES_SingleSimpleEval::Leave(ES_Statement *statement)
{
    switch (statement->GetType())
    {
    case ES_Statement::TYPE_WHILE:
    case ES_Statement::TYPE_FOR:
    case ES_Statement::TYPE_FORIN:
        --flag;
    }

    return result;
}

BOOL
ES_Compiler::HasSingleEvalCall()
{
    ES_SingleSimpleEval checker(*this);

    for (unsigned index = 0; checker.result && index < statements_count; ++index)
        statements[index]->CallVisitor(&checker);

    return checker.result;
}

#define ES_SHARED_PROPERTY_CACHES

unsigned
ES_Compiler::GetPropertyGetCacheIndex(JString *name)
{
#ifdef ES_SHARED_PROPERTY_CACHES
    unsigned index;

    property_get_caches_table->AppendL(context, name, index);
    property_get_caches_used = es_maxu(property_get_caches_used, index + 1);

    return index;
#else // ES_SHARED_PROPERTY_CACHES
    return property_get_caches_used++;
#endif // ES_SHARED_PROPERTY_CACHES
}

unsigned
ES_Compiler::GetPropertyPutCacheIndex(JString *name)
{
#ifdef ES_SHARED_PROPERTY_CACHES
    unsigned index;

    property_put_caches_table->AppendL(context, name, index);
    property_put_caches_used = es_maxu(property_put_caches_used, index + 1);

    return index;
#else // ES_SHARED_PROPERTY_CACHES
    return property_put_caches_used++;
#endif // ES_SHARED_PROPERTY_CACHES
}

void
ES_Compiler::AddDebugRecord(ES_CodeStatic::DebugRecord::Type type, const ES_SourceLocation &location)
{
    if (emit_debug_information)
    {
        if (debug_records_count == debug_records_allocated)
        {
            unsigned new_allocated = debug_records_allocated == 0 ? 64 : debug_records_allocated + debug_records_allocated;

            ES_CodeStatic::DebugRecord *new_records = OP_NEWA_L(ES_CodeStatic::DebugRecord, new_allocated);
            op_memcpy(new_records, debug_records, debug_records_count * sizeof debug_records[0]);
            OP_DELETEA(debug_records);

            debug_records = new_records;
            debug_records_allocated = new_allocated;
        }

        ES_CodeStatic::DebugRecord &new_record = debug_records[debug_records_count++];

        new_record.codeword_index = BytecodeUsed();
        new_record.type = type;
        new_record.location = location;
    }
}

BOOL
ES_Compiler::HasSetExtentInformation()
{
    if (debug_records_count != 0)
    {
        int index = debug_records_count - 1;

        while (index >= 0 && static_cast<ES_CodeStatic::DebugRecord::Type>(debug_records[index].type) != ES_CodeStatic::DebugRecord::TYPE_EXTENT_INFORMATION)
            --index;

        if (index >= 0 && static_cast<ES_CodeStatic::DebugRecord::Type>(debug_records[index].type) == ES_CodeStatic::DebugRecord::TYPE_EXTENT_INFORMATION && debug_records[index].location == *current_source_location)
            return TRUE;
    }

    return FALSE;
}

void
ES_Compiler::SetExtentInformation(ES_Instruction instruction)
{
    if (emit_debug_information)
        if (current_source_location && !current_source_location_used)
        {
            if (HasSetExtentInformation())
                return;

            if (g_instruction_is_trivial[instruction])
                return;

            AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_EXTENT_INFORMATION, *current_source_location);

            current_source_location_used = TRUE;
        }
}

OpMemGroup *
ES_Compiler::Arena()
{
    return parser->Arena();
}

#undef WRITE1
#undef WRITE2
#undef WRITE3
#undef WRITE4
#undef WRITE5
#undef WRITE6
#undef REGISTER
