/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/compiler/es_native.h"
#include "modules/ecmascript/carakan/src/compiler/es_instruction_data.h"
#include "modules/ecmascript/carakan/src/compiler/es_analyzer.h"
#include "modules/ecmascript/carakan/src/compiler/es_disassembler.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler.h"
#include "modules/ecmascript/carakan/src/builtins/es_math_builtins_support.h"
#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
#include "modules/util/opstring.h"
#endif // ES_DEBUG_NATIVE_BEHAVIOUR
#ifdef VALGRIND_INTEGRATION
#include "valgrind.h"
#endif // VALGRIND_INTEGRATION

#ifdef ES_NATIVE_SUPPORT
#ifdef NATIVE_DISASSEMBLER_SUPPORT

static void
EnableNativeDisassemble(ES_CodeGenerator &cg, ES_Code *code)
{
    BOOL disassemble = FALSE;

#ifdef _STANDALONE
    extern uni_char **g_focused_functions;

    if (g_focused_functions)
    {
        if (code->type == ES_Code::TYPE_FUNCTION)
        {
            ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);
            JString *name = fncode->GetName();
            if (!name)
                name = fncode->GetDebugName();
            if (name)
            {
                uni_char **focused = g_focused_functions;
                while (*focused)
                    if (name->Equals(*focused, uni_strlen(*focused)))
                    {
                        disassemble = TRUE;
                        break;
                    }
                    else
                        ++focused;
            }
        }
    }
    else
#endif // _STANDALONE
        disassemble = TRUE;

    if (disassemble)
        cg.EnableDisassemble();
}

#endif // NATIVE_DISASSEMBLER_SUPPORT

ES_NativeCodeBlock::ES_NativeCodeBlock(ES_Context *context, Type type)
    : type(type),
      block(NULL)
#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
    , data_section(NULL)
    , base_register_offset(0)
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION
{
    data.function_code = NULL;
    context->heap->AddNativeDispatcher(this);
}

ES_NativeCodeBlock::~ES_NativeCodeBlock()
{
    Out();
    Reset();

    if (block)
        OpExecMemoryManager::Free(block);

#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
    op_free(data_section);
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION
}

void
ES_NativeCodeBlock::SetFunctionCode(ES_FunctionCode *function_code)
{
    OP_ASSERT(type == TYPE_FUNCTION);
    data.function_code = function_code;
}

void
ES_NativeCodeBlock::SetFunctionObject(ES_Function *function_object)
{
    OP_ASSERT(type == TYPE_CONSTRUCTOR);
    data.function_object = function_object;
}

void
ES_NativeCodeBlock::SetLoop(ES_Code *code, unsigned loop_index, ES_Object *global_object)
{
    OP_ASSERT(type == TYPE_LOOP);
    data.loop.code = code;
    data.loop.loop_index = loop_index;
    data.loop.global_object_class_id = global_object ? global_object->Class()->Id() : ES_Class::NOT_CACHED_CLASS_ID;
}

void
ES_NativeCodeBlock::Reset()
{
    if (type == TYPE_FUNCTION)
    {
        if (data.function_code)
        {
            data.function_code->native_dispatcher = NULL;
            data.function_code->native_code_block = NULL;
        }
    }
    else if (type == TYPE_CONSTRUCTOR)
    {
        if (data.function_object)
            data.function_object->ResetNativeConstructorDispatcher();
    }
    else if (type == TYPE_LOOP)
        if (ES_Code *code = data.loop.code)
        {
            unsigned loop_index = data.loop.loop_index;

            code->loop_counters[loop_index] = 0;
            code->loop_dispatcher_codes[loop_index]->native_dispatcher = NULL;
            code->loop_dispatcher_codes[loop_index]->native_code_block = NULL;
            code->loop_dispatcher_codes[loop_index] = NULL;
        }
}

BOOL
ES_NativeCodeBlock::IsLoopValid(ES_Object *global_object)
{
    return data.loop.global_object_class_id == ES_Class::NOT_CACHED_CLASS_ID || data.loop.global_object_class_id == global_object->Class()->Id();
}

ES_Native::ES_Native(ES_Execution_Context *context, ES_Code *code, OpMemGroup *arena0, ES_CodeGenerator *alternate_cg)
    : ES_ArchitecureMixin(code),
      arena(arena0 ? arena0 : &local_arena),
      context(context),
      code(code),
      fncode(code->type == ES_Code::TYPE_FUNCTION ? static_cast<ES_FunctionCode *>(code) : NULL),
      optimization_data(code->data->optimization_data),
      is_trivial(TRUE),
      is_light_weight(FALSE),
      eliminate_integer_overflows(FALSE),
      allow_inlined_calls(FALSE),
      cw_index(0),
      primary_cg(arena),
      cg(alternate_cg ? *alternate_cg : primary_cg),
#ifdef NATIVE_DISASSEMBLER_SUPPORT
      annotator(context),
#endif // NATIVE_DISASSEMBLER_SUPPORT
#ifdef _DEBUG
      current_disassembly(NULL),
#endif // _DEBUG
      native_registers(NULL),
      virtual_registers(NULL),
      jump_targets(NULL),
      next_jump_target(NULL),
      jump_targets_count(0),
      first_switch_table(NULL),
      last_switch_table(NULL),
      entry_point_cw(NULL),
      entry_point_cw_index(UINT_MAX),
      prologue_entry_point(FALSE),
      entry_point_jump_target(NULL),
      constructor(FALSE),
      constructor_final_class(NULL),
      constructor_final_class_count(0),
      is_inlined_function_call(FALSE),
      loop_dispatcher(FALSE),
      first_loop_io(NULL),
      owns_loop_io(FALSE),
      global_object_class(NULL),
      first_function_call(NULL),
      function_call_count(0),
      arithmetic_block_count(0),
      first_arithmetic_block(NULL),
      last_arithmetic_block(NULL),
      current_loop(NULL),
      enumeration_object_register(NULL),
      enumeration_name_register(NULL),
      inlined_function_call(NULL),
      property_get_caches(NULL),
      property_put_caches(NULL),
      first_stack_frame_storage(NULL),
      unused_stack_frame_storage(NULL),
      property_value_write_vr(NULL),
      property_value_read_vr(NULL),
      property_value_nr(NULL),
      property_value_needs_type_check(TRUE),
      property_value_fail(NULL),
      epilogue_jump_target(NULL),
      failure_jump_target(NULL),
      current_slow_case(NULL),
      light_weight_failure(NULL),
      current_true_target(NULL),
      current_false_target(NULL),
      has_called_slow_case(FALSE)
{
#ifdef NATIVE_DISASSEMBLER_SUPPORT
    EnableNativeDisassemble(cg, code);
#endif // NATIVE_DISASSEMBLER_SUPPORT
}

ES_Native::~ES_Native()
{
    ReleaseLoopIOChain();
}

void
ES_Native::ReleaseLoopIOChain()
{
    if (owns_loop_io)
        while (ES_Code::LoopIO *loop_io = first_loop_io)
        {
            first_loop_io = loop_io->next;
            OP_DELETE(loop_io);
        }
}

ES_NativeCodeBlock *
ES_Native::CreateNativeDispatcher(ES_CodeWord *entry_point_cw0, void **entry_point_address, BOOL prologue_entry_point0, BOOL light_weight_allowed)
{
    if (!code->data->CanHaveNativeDispatcher() || !CheckLimits())
        return NULL;

#ifdef _DEBUG
#if 0
    //if (code->codewords_count == 95)
    {
        ES_Disassembler disassembler(context);

        if (code->type != ES_Code::TYPE_FUNCTION)
            disassembler.Disassemble(static_cast<ES_ProgramCode *>(code), TRUE);
        else
            disassembler.Disassemble(static_cast<ES_FunctionCode *>(code));

        unsigned length = disassembler.GetLength();

        if (length)
        {
            const uni_char *unicode = disassembler.GetOutput();
            current_disassembly = OP_NEWA(char, length + 1);
            if (!current_disassembly)
                context->AbortOutOfMemory();

            for (unsigned index = 0; index < length + 1; ++index)
                current_disassembly[index] = (char) unicode[index];

            //printf("%s\n", current_disassembly);
        }
    }
#endif // 0
#endif // _DEBUG

    unsigned *instruction_offsets = code->data->instruction_offsets;

    is_light_weight = !loop_dispatcher && light_weight_allowed && CheckIsLightWeight(code, &light_weight_accesses_cached_globals, NULL);
    allow_inlined_calls = TRUE;

    entry_point_cw = entry_point_cw0;
    entry_point_cw_index = entry_point_cw - code->data->codewords;
    prologue_entry_point = prologue_entry_point0;

    if (loop_dispatcher)
    {
        first_loop_io = code->first_loop_io;
        loop_io_start = code->loop_io_start;
        loop_io_end = code->loop_io_end;
    }

try_again:
    TRAPD(status, {
            cw_index = instruction_offsets[code->data->instruction_count - 1];

            InitializeJumpTargets();
            InitializeVirtualRegisters();
            InitializeNativeRegisters();

            epilogue_jump_target = cg.ForwardJump();
            eliminate_integer_overflows = EliminateIntegerOverflows();

            AllocateRegisters();
            GenerateCode();
        });

    if (OpStatus::IsMemoryError(status))
        LEAVE(status);
    else if (OpStatus::IsError(status))
    {
        if (is_light_weight)
            is_light_weight = FALSE;
        else
            allow_inlined_calls = FALSE;

        arithmetic_block_count = 0;
        first_arithmetic_block = last_arithmetic_block = NULL;
        entry_point_jump_target = NULL;

        cg.Reset();

        goto try_again;
    }

    GeneratePrologue();
    GenerateEpilogue();

    ES_NativeCodeBlock *ncblock = OP_NEW_L(ES_NativeCodeBlock, (context, loop_dispatcher ? ES_NativeCodeBlock::TYPE_LOOP : ES_NativeCodeBlock::TYPE_FUNCTION));

    OpStackAutoPtr<ES_NativeCodeBlock> ncblock_anchor(ncblock);

    if (OpStatus::IsError(Finish(ncblock)))
        return NULL;

#if 0
    ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);
    JString *fnname = fncode->GetName() ? fncode->GetName() : fncode->GetDebugName();
    OpString8 fnname_string;

    if (fnname)
        fnname_string.Set(Storage(NULL, fnname), Length(fnname));
    else
        fnname_string.Set("<anonymous function>");

    printf("%p - %p => %s\n", block->memory, (char *) block->memory + block->size, fnname_string.CStr());
#endif // 0

    if (entry_point_cw)
    {
        OP_ASSERT(entry_point_jump_target != NULL);
        *entry_point_address = entry_point_jump_target->GetAddress(ncblock->GetAddress());
    }

    OP_DELETEA(code->codeword_from_return_address);
    code->codeword_from_return_address = NULL;

    code->codeword_from_return_address = OP_NEWA_L(ES_Code::CodeWordFromReturnAddress, function_call_count + 1);

    FunctionCall *fc = first_function_call;
    for (unsigned index = 0; index < function_call_count; ++index)
    {
        code->codeword_from_return_address[index].return_address = fc->return_target->GetAddress(ncblock->GetAddress());
        code->codeword_from_return_address[index].codeword = fc->codeword;

        fc = fc->next;
    }

    code->codeword_from_return_address[function_call_count].return_address = NULL;

#ifdef _DEBUG
    OP_DELETEA(current_disassembly);
#endif // _DEBUG

    if (loop_dispatcher)
        ncblock->SetLoop(code->parent_code, loop_index, first_loop_io ? code->global_object : NULL);

    unsigned char *profiling = code->data->profile_data;
    if (profiling)
    {
        unsigned *instruction_offsets = code->data->instruction_offsets;
        for (unsigned index = 0, count = code->data->instruction_count; index < count; ++index)
            profiling[instruction_offsets[index]] = 0;
    }

#ifdef VALGRIND_JITFN_REGISTER
    if (RUNNING_ON_VALGRIND)
    {
        const char *fnname = "<unknown>";

        TempBuffer buffer;
        OpString8 buffer8;

        if (code->type == ES_Code::TYPE_FUNCTION)
        {
            extern void AppendFunctionNameL(TempBuffer &, ES_FunctionCode *);

            AppendFunctionNameL(buffer, static_cast<ES_FunctionCode *>(code));
            buffer8.SetL(buffer.GetStorage());
            fnname = buffer8.CStr();
        }
        else
            fnname = "<program>";

        VALGRIND_JITFN_REGISTER(reinterpret_cast<UINTPTR>(block->address), reinterpret_cast<UINTPTR>(block->address) + block->size, fnname);
    }
#endif // VALGRIND_JITFN_REGISTER

    ncblock_anchor.release();
    return ncblock;
}

ES_NativeCodeBlock *
ES_Native::CreateNativeConstructorDispatcher(ES_Object *prototype, ES_Class *klass, ES_CodeWord *entry_point_cw0, void **entry_point_address)
{
    if (!code->data->CanHaveNativeDispatcher() || !CheckLimits())
        return NULL;

    cw_index = code->data->instruction_offsets[code->data->instruction_count - 1];

    entry_point_cw = entry_point_cw0;
    entry_point_cw_index = entry_point_cw ? entry_point_cw - code->data->codewords : 0;

    constructor = TRUE;
    constructor_prototype = prototype ? ES_Value_Internal(prototype).GetObject(context) : NULL;
    constructor_class = klass;

    is_trivial = FALSE;
    is_light_weight = FALSE;

    constructor_final_class = static_cast<ES_FunctionCode *>(code)->OptimizeConstructorPropertyCaches(context, klass, constructor_final_class_count);

    InitializeJumpTargets();
    InitializeVirtualRegisters();
    InitializeNativeRegisters();

    epilogue_jump_target = cg.ForwardJump();

    AllocateRegisters();
    GenerateCode();
    GeneratePrologue();
    GenerateEpilogue();

    ES_NativeCodeBlock *ncblock = OP_NEW_L(ES_NativeCodeBlock, (context, ES_NativeCodeBlock::TYPE_CONSTRUCTOR));

    OpStackAutoPtr<ES_NativeCodeBlock> ncblock_anchor(ncblock);

    if (OpStatus::IsError(Finish(ncblock)))
        return NULL;

    if (entry_point_address)
    {
        OP_ASSERT(entry_point_jump_target != NULL);
        *entry_point_address = entry_point_jump_target->GetAddress(ncblock->GetAddress());
    }

    unsigned char *profiling = code->data->profile_data;
    if (profiling)
    {
        unsigned *instruction_offsets = code->data->instruction_offsets;
        for (unsigned index = 0, count = code->data->instruction_count; index < count; ++index)
            profiling[instruction_offsets[index]] = 0;
    }

#ifdef VALGRIND_JITFN_REGISTER
    if (RUNNING_ON_VALGRIND)
    {
        const char *fnname = "<unknown>";

        TempBuffer buffer;
        OpString8 buffer8;

        extern void AppendFunctionNameL(TempBuffer &, ES_FunctionCode *);

        AppendFunctionNameL(buffer, static_cast<ES_FunctionCode *>(code));
        buffer8.SetL(buffer.GetStorage());
        buffer8.InsertL(0, "CTOR: ");
        fnname = buffer8.CStr();

        VALGRIND_JITFN_REGISTER(reinterpret_cast<UINTPTR>(block->address), reinterpret_cast<UINTPTR>(block->address) + block->size, fnname);
    }
#endif // VALGRIND_JITFN_REGISTER

    ncblock_anchor.release();
    return ncblock;
}

ES_Code::LoopIO *
ES_Native::AddLoopIO(ES_Code::LoopIO::Type type, JString *name, unsigned index, BOOL input, BOOL output)
{
    ES_Code::LoopIO *loop_io = first_loop_io, **ptr = &first_loop_io;

    while (loop_io)
        if (loop_io->name == name)
            break;
        else
        {
            ptr = &loop_io->next;
            loop_io = *ptr;
        }

    if (loop_io)
    {
        OP_ASSERT(loop_io->type == type);
        OP_ASSERT(loop_io->index == index);

        if (input)
            loop_io->input = TRUE;
        if (output)
            loop_io->output = TRUE;
    }
    else
    {
        loop_io = *ptr = OP_NEW_L(ES_Code::LoopIO, ());
        loop_io->type = type;
        loop_io->name = name;
        loop_io->index = index;
        loop_io->input = input;
        loop_io->output = output;
    }

    return loop_io;
}

ES_Code::LoopIO *
ES_Native::FindLoopIO(JString *name)
{
    ES_Code::LoopIO *loop_io = first_loop_io;

    while (loop_io)
        if (loop_io->name == name)
            break;
        else
            loop_io = loop_io->next;

    return loop_io;
}

#ifdef SIXTY_FOUR_BIT
# define GET_CLASS_ARG code
# define GET_IDENTIFIER_ARG code
#else // SIXTY_FOUR_BIT
# define GET_CLASS_ARG
# define GET_IDENTIFIER_ARG
#endif // SIXTY_FOUR_BIT

BOOL
ES_Native::CreateNativeLoopDispatcher(unsigned loop_index0)
{
    if (!CheckLimits())
        return FALSE;

    OP_ASSERT(!code->is_strict_eval);

    code->data->FindInstructionOffsets();

    loop_index = loop_index0;

    ES_CodeStatic::LoopData *loop_data = &code->data->loop_data[loop_index];
    ES_Global_Object *global_object = code->global_object;
    BOOL use_alias_registers = TRUE;

    global_object_class = global_object->Class();

    for (unsigned eh_index = 0; eh_index < code->data->exception_handlers_count; ++eh_index)
        if (code->data->exception_handlers[eh_index].start <= loop_data->jump && code->data->exception_handlers[eh_index].end > loop_data->start)
            return FALSE;

    if (code->data->switch_tables_count != 0)
        /* FIXME: This is not really a problem, we just need to make sure the
           switches are either outside the loop or, if they are within it,
           translate all the code word indeces in the jump tables to the
           corresponding indeces in the loop bytecode. */
        return FALSE;

    owns_loop_io = TRUE;

    for (unsigned instruction_index = 0;; ++instruction_index)
    {
        cw_index = code->data->instruction_offsets[instruction_index];

        if (cw_index < loop_data->start)
            continue;

        ES_CodeWord *word = &code->data->codewords[cw_index];
        ES_Code::GlobalCache *cache;

        switch (word->instruction)
        {
        case ESI_GET_GLOBAL:
            cache = &code->global_caches[word[3].index];
            if (cache->class_id != ES_Class::GLOBAL_IMMEDIATE_CLASS_ID && cache->class_id != global_object_class->Id())
                use_alias_registers = FALSE;
            else if (use_alias_registers)
            {
                ES_Code::LoopIO::Type type;
                if (cache->class_id == ES_Class::GLOBAL_IMMEDIATE_CLASS_ID)
                    type = ES_Code::LoopIO::TYPE_GLOBAL_IMM;
                else
                    type = ES_Code::LoopIO::TYPE_GLOBAL_CACHED;
                AddLoopIO(type, code->GetString(word[2].index), cache->cached_index, TRUE, FALSE);
            }
            break;

        case ESI_PUT_GLOBAL:
            cache = &code->global_caches[word[3].index];
            if (cache->class_id != ES_Class::GLOBAL_IMMEDIATE_CLASS_ID && cache->class_id != global_object_class->Id())
                use_alias_registers = FALSE;
            else if (use_alias_registers)
            {
                ES_Code::LoopIO::Type type;
                if (cache->class_id == ES_Class::GLOBAL_IMMEDIATE_CLASS_ID)
                    type = ES_Code::LoopIO::TYPE_GLOBAL_IMM;
                else
                    type = ES_Code::LoopIO::TYPE_GLOBAL_CACHED;
                AddLoopIO(type, code->GetString(word[1].index), cache->cached_index, FALSE, TRUE);
            }
            break;

        case ESI_COPYN:
#ifdef ES_COMBINED_ADD_SUPPORT
        case ESI_ADDN:
#endif // ES_COMBINED_ADD_SUPPORT
        case ESI_CONSTRUCT_OBJECT:
            /* These aren't really that bad, but we need to support them
               specificly in the code below to let them through. */
            use_alias_registers = FALSE;
            break;

        case ESI_GET:
        case ESI_GETN_IMM:
        case ESI_GETI_IMM:
        case ESI_GET_LENGTH:
        case ESI_GET_SCOPE:
        case ESI_GET_SCOPE_REF:
        case ESI_PUT:
        case ESI_PUTN_IMM:
        case ESI_INIT_PROPERTY:
        case ESI_PUTI_IMM:
        case ESI_PUT_LENGTH:
        case ESI_PUT_SCOPE:
        case ESI_EVAL:
        case ESI_CALL:
        case ESI_CONSTRUCT:
        case ESI_DELETE:
        case ESI_DELETEI_IMM:
        case ESI_DELETEN_IMM:
        case ESI_DELETE_SCOPE:
            use_alias_registers = FALSE;
            break;

        case ESI_JUMP:
        case ESI_JUMP_IF_TRUE:
        case ESI_JUMP_IF_FALSE:
            {
                unsigned target_cw_index = cw_index + 1 + word[1].offset;
                if (target_cw_index < loop_data->start || target_cw_index > loop_data->jump)
                    return FALSE;
                else if (target_cw_index != loop_data->start)
                    use_alias_registers = FALSE;
            }
            break;
        }

        if (cw_index == loop_data->jump)
            break;
    }

    ES_ProgramCodeStatic *data = OP_NEW_L(ES_ProgramCodeStatic, ());
    OpStackAutoPtr<ES_ProgramCodeStatic> data_anchor(data);

    ES_ProgramCode *new_code;

    GC_ALLOCATE(context, new_code, ES_ProgramCode, (new_code, data, global_object));

    new_code->type = code->type;

    data_anchor.release();
    //ES_CodeStatic::IncRef(data);

    ES_CollectorLock gclock(context);

    new_code->SetParentCode(code);

    if (use_alias_registers)
    {
        if (!optimization_data)
        {
            ES_Analyzer analyzer(context, code);
            optimization_data = code->data->optimization_data = analyzer.AnalyzeL();
        }

        unsigned alias_register_index = code->data->register_frame_size;
        ES_Code::LoopIO *loop_io = first_loop_io;

        while (loop_io)
        {
            loop_io->alias_register = alias_register_index++;
            loop_io = loop_io->next;
        }

        unsigned temporary_offset = alias_register_index - code->data->register_frame_size;
        unsigned *register_translations = OP_NEWGROA_L(unsigned, code->data->register_frame_size, Arena());
        unsigned *register_translations_start = OP_NEWGROA_L(unsigned, code->data->register_frame_size, Arena());
        unsigned codewords_count = loop_data->jump + 2 - loop_data->start;
        ES_CodeWord *codewords = OP_NEWA_L(ES_CodeWord, codewords_count);
        unsigned char *profile_data = code->data->profile_data ? OP_NEWA(unsigned char, codewords_count) : NULL;
        op_memset(profile_data, 0, codewords_count);
#ifndef _STANDALONE
        MemoryManager::IncDocMemoryCount(sizeof(ES_CodeWord) * codewords_count);
#endif // !_STANDALONE
        ES_CodeWord *write = codewords;
        unsigned char *write_pd = profile_data;

        op_memset(register_translations + code->data->first_temporary_register, 0, (code->data->register_frame_size - code->data->first_temporary_register) * sizeof(unsigned));
        op_memset(register_translations_start + code->data->first_temporary_register, 0, (code->data->register_frame_size - code->data->first_temporary_register) * sizeof(unsigned));

        BOOL skip_next_instruction = FALSE;

        for (unsigned instruction_index = 0;; ++instruction_index)
        {
            cw_index = code->data->instruction_offsets[instruction_index];

            if (cw_index < loop_data->start)
                continue;

            for (unsigned register_index = code->data->first_temporary_register; register_index < code->data->register_frame_size; ++register_index)
                if (register_translations[register_index] != 0 && optimization_data->FindWriteAt(register_translations_start[register_index], register_index)->GetLastReadIndex() < cw_index)
                    register_translations[register_index] = 0;

            if (skip_next_instruction)
            {
                skip_next_instruction = FALSE;
                continue;
            }

            ES_CodeWord *word = &code->data->codewords[cw_index];

            unsigned write_length = 0;

            switch (word->instruction)
            {
            case ESI_GET_GLOBAL:
                loop_io = FindLoopIO(code->GetString(word[2].index));

                if (word[1].index >= code->data->first_temporary_register)
                {
                    register_translations[word[1].index] = loop_io->alias_register;
                    register_translations_start[word[1].index] = cw_index;
                }
                else
                {
                    *write++ = ES_CodeWord(ESI_COPY);
                    *write++ = ES_CodeWord(word[1].index);
                    *write++ = ES_CodeWord(loop_io->alias_register);

                    if (write_pd)
                        write_pd += 3;
                }
                break;

            case ESI_PUT_GLOBAL:
                {
                    loop_io = FindLoopIO(code->GetString(word[1].index));

                    unsigned register_index = word[2].index;

                    if (register_translations[register_index] != 0)
                        register_index = register_translations[register_index];

                    /* FIXME: we could probably have eliminated this copy. */
                    *write++ = ES_CodeWord(ESI_COPY);
                    *write++ = ES_CodeWord(loop_io->alias_register);
                    *write++ = ES_CodeWord(register_index);

                    if (write_pd)
                        write_pd += 3;
                }
                break;

            case ESI_COPYN:
#ifdef ES_COMBINED_ADD_SUPPORT
            case ESI_ADDN:
#endif // ES_COMBINED_ADD_SUPPORT
            case ESI_CONSTRUCT_OBJECT:
                OP_ASSERT(FALSE);
                break;

            case ESI_INC:
            case ESI_DEC:
                {
                    unsigned register_index = word[1].index;

                    if (register_translations[register_index] != 0)
                    {
                        register_translations_start[register_index] = cw_index;
                        register_index = register_translations[register_index];
                        switch (word[2].instruction)
                        {
                        case ESI_PUT_GLOBAL:
                            skip_next_instruction = TRUE;
                        }
                    }

                    *write++ = *word;
                    *write++ = ES_CodeWord(register_index);

                    write_length = 2;
                }
                break;

            case ESI_JUMP:
                *write++ = ES_CodeWord(word->instruction);
                *write = ES_CodeWord(static_cast<int>(codewords - write)), ++write;

                write_length = 2;
                break;

            case ESI_JUMP_IF_FALSE:
            case ESI_JUMP_IF_TRUE:
                *write++ = ES_CodeWord(word->instruction);
                *write = ES_CodeWord(static_cast<int>(codewords - write)), ++write;
                *write++ = word[2];

                write_length = 3;
                break;

            default:
                unsigned operand_count = g_instruction_operand_count[word->instruction];
                unsigned short register_o = g_instruction_operand_register_io[word->instruction];
                unsigned short register_i = register_o >> 8;
                unsigned target_register_index = UINT_MAX, target_register_translation = 0, target_register_translation_start = 0;

                *write++ = *word;

                for (unsigned index = 0; index < operand_count; ++index)
                    if (((register_o | register_i) & (1 << index)) != 0)
                    {
                        unsigned register_index = word[1 + index].index;

                        if (register_index != UINT_MAX)
                        {
                            register_index = register_index & 0x7fffffffu;

                            OP_ASSERT((register_o & (1 << index)) == 0 || (register_i & (1 << index)) == 0);

                            if ((register_o & (1 << index)) != 0)
                            {
                                if (register_index >= code->data->first_temporary_register)
                                    switch (word[1 + operand_count].instruction)
                                    {
                                    case ESI_PUT_GLOBAL:
                                        loop_io = FindLoopIO(code->GetString(word[1 + operand_count + 1].index));

                                        if (loop_io)
                                        {
                                            target_register_index = register_index;
                                            target_register_translation = register_index = loop_io->alias_register;
                                            target_register_translation_start = cw_index;
                                            skip_next_instruction = TRUE;
                                            break;
                                        }
                                    }
                            }
                            else
                            {
                                if (register_translations[register_index] != 0)
                                    register_index = register_translations[register_index];
                            }
                        }

                        *write++ = ES_CodeWord(register_index);
                    }
                    else
                        *write++ = word[1 + index];

                if (target_register_index != UINT_MAX)
                {
                    register_translations[target_register_index] = target_register_translation;
                    register_translations_start[target_register_index] = target_register_translation_start;
                }

                write_length = operand_count + 1;
                break;
            }

            if (write_pd)
            {
                op_memcpy(write_pd, code->data->profile_data + cw_index, write_length);
                write_pd += write_length;
            }

            OP_ASSERT(write <= codewords + codewords_count);

            if (cw_index == loop_data->jump)
                break;
        }

        new_code->data->codewords = codewords;
        new_code->data->codewords_count = write - codewords;
#ifndef _STANDALONE
        MemoryManager::DecDocMemoryCount(sizeof(ES_CodeWord) * codewords_count);
        MemoryManager::IncDocMemoryCount(sizeof(ES_CodeWord) * new_code->data->codewords_count, FALSE); // Approximation but the slack at the end will have to be forgotten to balance the DecDocMemoryUsage call.
#endif // !_STANDALONE

        new_code->data->register_frame_size = code->data->register_frame_size + temporary_offset;
        new_code->data->first_temporary_register = code->data->first_temporary_register;
        new_code->data->profile_data = profile_data;

        owns_loop_io = FALSE;
        loop_io_start = code->data->register_frame_size;
        loop_io_end = loop_io_start + temporary_offset;

        new_code->first_loop_io = first_loop_io;
        new_code->loop_io_start = loop_io_start;
        new_code->loop_io_end = loop_io_end;
    }
    else
    {
        ReleaseLoopIOChain();

        new_code->data->codewords_count = loop_data->jump + 3 - loop_data->start;
        new_code->data->codewords = OP_NEWA_L(ES_CodeWord, new_code->data->codewords_count);
        if (code->data->profile_data)
            new_code->data->profile_data = OP_NEWA(unsigned char, data->codewords_count);
#ifndef _STANDALONE
        MemoryManager::IncDocMemoryCount(sizeof(ES_CodeWord) * new_code->data->codewords_count, FALSE);
#endif // !_STANDALONE
        new_code->data->register_frame_size = code->data->register_frame_size;
        new_code->data->first_temporary_register = code->data->first_temporary_register;

        op_memcpy(new_code->data->codewords, code->data->codewords + loop_data->start, new_code->data->codewords_count * sizeof(ES_CodeWord));
        if (new_code->data->profile_data)
            op_memcpy(new_code->data->profile_data, code->data->profile_data + loop_data->start, new_code->data->codewords_count);

        loop_io_start = UINT_MAX;
        loop_io_end = UINT_MAX;
    }

    ES_Code *parent_code = code;

    UpdateCode(new_code);

    if (!CheckLimits())
        return FALSE;

    parent_code->loop_dispatcher_codes[loop_index] = new_code;
    code->data->FindInstructionOffsets();

    ES_Analyzer analyzer(context, code, TRUE);

    if (first_loop_io)
        analyzer.SetLoopIORange(loop_io_start, loop_io_end);

    OP_ASSERT(!code->data->optimization_data);

    if (!code->data->optimization_data)
        code->data->optimization_data = analyzer.AnalyzeL();

    optimization_data = code->data->optimization_data;

#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
    extern BOOL g_debug_native_behaviour;
    if (g_debug_native_behaviour)
    {
        const char *fnname = "<unknown>";

        TempBuffer buffer;
        OpString8 buffer8;

        if (code->type == ES_Code::TYPE_FUNCTION)
        {
            extern void AppendFunctionNameL(TempBuffer &, ES_FunctionCode *);

            AppendFunctionNameL(buffer, static_cast<ES_FunctionCode *>(code));
            buffer8.SetL(buffer.GetStorage());
            fnname = buffer8.CStr();
        }
        else
            fnname = "<program>";

        printf("compiling native loop dispatcher: %s (start: %u, jump: %u, codewords: %u, loop io: %s)\n", fnname, loop_data->start, loop_data->jump, code->data->codewords_count, first_loop_io ? "yes" : "no");
    }

    extern unsigned g_total_native_recompiles;
    ++g_total_native_recompiles;

# ifdef _STANDALONE
    extern BOOL g_disassemble_loop_dispatcher;
    if (g_disassemble_loop_dispatcher)
    {
        ES_Disassembler disassembler(context);

        disassembler.Disassemble(static_cast<ES_ProgramCode *>(code), TRUE);

        unsigned length = disassembler.GetLength();
        const uni_char *unicode = disassembler.GetOutput();
        char *ascii = OP_NEWA(char, length + 1);
        if (!ascii)
            context->AbortOutOfMemory();

        for (unsigned index = 0; index < length + 1; ++index)
            ascii[index] = (char) unicode[index];

        fprintf(stdout, "--------------------------------------------------------------------------------\n%s--------------------------------------------------------------------------------\n", ascii);

        OP_DELETEA(ascii);
    }
# endif // _STANDALONE
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

    cw_index = code->data->instruction_offsets[code->data->instruction_count - 1];
    loop_dispatcher = TRUE;

    InitializeJumpTargets();
    InitializeVirtualRegisters();
    InitializeNativeRegisters();

    epilogue_jump_target = cg.ForwardJump();

    AllocateRegisters();
    GenerateCode();

    if (JumpTarget *jump_target = FindJumpTarget(code->data->codewords_count, TRUE))
        if (jump_target->forward_jump)
            cg.SetJumpTarget(jump_target->forward_jump);

    GeneratePrologue();
    GenerateEpilogue();

    ES_NativeCodeBlock *ncblock = OP_NEW_L(ES_NativeCodeBlock, (context, ES_NativeCodeBlock::TYPE_LOOP));

    OpStackAutoPtr<ES_NativeCodeBlock> ncblock_anchor(ncblock);

    if (OpStatus::IsError(Finish(ncblock)))
        return FALSE;

    ncblock->SetLoop(code->parent_code, loop_index, first_loop_io ? code->global_object : NULL);

    code->native_code_block = ncblock;
    code->native_dispatcher = ncblock->GetAddress();

    ncblock_anchor.release();
    return TRUE;
}

BOOL
ES_Native::StartInlinedFunctionCall(ES_Class *klass, unsigned argc)
{
    is_inlined_function_call = TRUE;
    this_object_class = klass;
    inlined_call_argc = argc;

    OP_ASSERT(CheckIsLightWeight(code, NULL, NULL));

    is_light_weight = TRUE;
    light_weight_accesses_cached_globals = FALSE;

    TRAPD(status, {
            InitializeJumpTargets();
            InitializeVirtualRegisters();
            InitializeNativeRegisters();

            AllocateRegisters();
        });

    if (OpStatus::IsMemoryError(status))
        LEAVE(status);

    return OpStatus::IsSuccess(status);
}

void
ES_Native::GenerateInlinedFunctionCall(ES_CodeGenerator::OutOfOrderBlock *failure)
{
    light_weight_failure = failure;

    ES_CodeGenerator::Block *previous_prologue_point = cg.SetProloguePoint();
    ES_CodeGenerator::JumpTarget *previous_epilogue_jump_target = epilogue_jump_target;

    epilogue_jump_target = cg.ForwardJump();

    GenerateCode();
    GeneratePrologue();
    GenerateEpilogue();

    epilogue_jump_target = previous_epilogue_jump_target;
    cg.ResetProloguePoint(previous_prologue_point);
}

void
ES_Native::InitializeJumpTargets()
{
    if (optimization_data->jump_targets_count != 0)
    {
        ES_CodeOptimizationData::JumpTarget *data = optimization_data->first_jump_target;

        jump_targets = OP_NEWGROA_L(JumpTarget, optimization_data->jump_targets_count, Arena());
        jump_targets_count = optimization_data->jump_targets_count;

        op_memset(jump_targets, 0, sizeof(JumpTarget) * jump_targets_count);

        for (unsigned index = 0; index < optimization_data->jump_targets_count; ++index, data = data->next)
        {
            JumpTarget &jt = jump_targets[index];

            jt.data = data;
            jt.forward_jump = NULL;
            jt.here = NULL;
        }

        next_jump_target = &jump_targets[0];
    }
}

ES_Native::JumpTarget *
ES_Native::FindJumpTarget(unsigned target_index, BOOL optional)
{
    int low = 0, high = jump_targets_count - 1;

    while (low <= high)
    {
        unsigned index = (low + high) / 2;

        if (target_index < jump_targets[index].data->index)
            high = index - 1;
        else if (target_index > jump_targets[index].data->index)
            low = index + 1;
        else
            return &jump_targets[index];
    }

    OP_ASSERT(optional);

#if 0
    printf("Jump target not found: %u\nAvailable targets: ", target_index);
    for (unsigned index = 0; index < jump_targets_count; ++index)
        printf("%s%u", index > 0 ? ", " : "", jump_targets[index].data->index);
    printf("\n");
#endif // 0

    return NULL;
}

void
ES_Native::InitializeVirtualRegisters()
{
    virtual_registers = OP_NEWGROA_L(VirtualRegister, code->data->register_frame_size + -(VR_PSEUDO_COUNT + 1), Arena()) + -(VR_PSEUDO_COUNT + 1);
    virtual_registers_count = code->data->register_frame_size;

    for (unsigned index = 0; index < code->data->register_frame_size; ++index)
    {
        VirtualRegister &virtual_register = virtual_registers[index];

        virtual_register.index = index;
        virtual_register.is_temporary = index >= code->data->first_temporary_register;
        virtual_register.first_allocation = virtual_register.last_allocation = virtual_register.current_allocation = NULL;
        virtual_register.deferred_copy_from = UINT_MAX;
        virtual_register.number_of_uses = 0;
        virtual_register.integer_no_overflow = FALSE;
        virtual_register.stack_frame_offset = INT_MAX;
    }

    /* Pseudo-virtual registers for context pointer, register frame pointer, and
       one for collecting scratch register allocations for instructions. */
    for (int pseudo_index = VR_IMMEDIATE; pseudo_index > VR_PSEUDO_COUNT; --pseudo_index)
    {
        VirtualRegister &virtual_register = virtual_registers[pseudo_index];

        virtual_register.index = pseudo_index == VR_IMMEDIATE ? code->data->register_frame_size : UINT_MAX + pseudo_index;
        virtual_register.is_temporary = FALSE;
        virtual_register.first_allocation = virtual_register.last_allocation = virtual_register.current_allocation = NULL;
        virtual_register.deferred_copy_from = UINT_MAX;
        virtual_register.number_of_uses = 0;
        virtual_register.stack_frame_offset = INT_MAX;
    }
}

#ifdef ES_VALUES_AS_NANS
BOOL
ES_Native::FlushToVirtualRegisters(ArithmeticBlock *arithmetic_block, BOOL only_integers, BOOL keep_states)
#else // ES_VALUES_AS_NANS
void
ES_Native::FlushToVirtualRegisters(ArithmeticBlock *arithmetic_block)
#endif // ES_VALUES_AS_NANS
{
#ifdef ES_VALUES_AS_NANS
    BOOL anything_left_to_flush = FALSE;
#endif // ES_VALUES_AS_NANS

    for (unsigned index = 0; index < virtual_registers_count; ++index)
    {
        VirtualRegister *vr = VR(index);

        if (vr->value_needs_flush)
        {
            FreeStackFrameStorage(vr);
            EmitSetRegisterValue(vr, vr->value_known_in_arithmetic_block, TRUE);
            continue;
        }

        if (RegisterAllocation *allocation = vr->current_allocation)
        {
            /* Find an allocation that ends at the end of the arithmetic_block,
               if there is one. */
            while (allocation->end < arithmetic_block->end_cw_index && allocation->virtual_next)
                allocation = allocation->virtual_next;
            if (allocation->end != arithmetic_block->end_cw_index || allocation->start == allocation->end)
                continue;

            if (allocation->intermediate_write)
                continue;

            /* The value in the native register was read (and/or converted) from
               the virtual register, so it's already in the virtual register. */
            if ((allocation->type == RegisterAllocation::TYPE_READ || allocation->type == RegisterAllocation::TYPE_CONVERT) /* && !value*/)
                continue;

            /* The value in the native register was copied from another native
               register.  If that value, in turn, was read (and/or converted)
               from the virtual register, there is no need to write a value
               back.  If the copied value was calculated into that native
               register, then the value in this native register needs to be
               written to the virtual register. */
            while (allocation->type == RegisterAllocation::TYPE_COPY)
            {
                RegisterAllocation *previous = allocation->virtual_previous;
                if (!previous)
                    break;
                else if (previous->type == RegisterAllocation::TYPE_READ || previous->type == RegisterAllocation::TYPE_CONVERT)
                {
                    allocation = NULL;
                    break;
                }
                else
                    allocation = previous;
            }

            if (allocation)
            {
#ifdef ES_VALUES_AS_NANS
                if (only_integers && allocation->native_register->type == NativeRegister::TYPE_DOUBLE)
                {
                    anything_left_to_flush = TRUE;
                    continue;
                }
#endif // ES_VALUES_AS_NANS

                FreeStackFrameStorage(vr);
                EmitStoreRegisterValue(vr, allocation->native_register);

#ifdef ES_VALUES_AS_NANS
                if (!keep_states)
#endif // ES_VALUES_AS_NANS
                    allocation->type = RegisterAllocation::TYPE_READ;
            }
        }
    }

    unsigned stored_cw_index = cw_index;

#ifdef ES_VALUES_AS_NANS
    ArithmeticBlock::DelayedCopy *unprocessed = NULL;
#endif // ES_VALUES_AS_NANS

    if (ArithmeticBlock::DelayedCopy *copy = arithmetic_block->first_delayed_copy)
    {
        ArithmeticBlock::DelayedCopy *next;

        do
        {
            next = copy->next;

#ifdef ES_VALUES_AS_NANS
            if (only_integers && copy->source->stack_frame_offset != INT_MAX && copy->source->stack_frame_type == ESTYPE_DOUBLE)
            {
                copy->next = unprocessed;
                unprocessed = copy;

                anything_left_to_flush = TRUE;
            }
            else
#endif // ES_VALUES_AS_NANS
            {
                cw_index = copy->cw_index;

                if (copy->target->stack_frame_offset != INT_MAX)
                    copy->target->stack_frame_offset = INT_MAX;

                EmitRegisterCopy(copy->source, copy->target);
            }
        }
        while ((copy = next) != NULL);
    }

    cw_index = stored_cw_index;

#ifdef ES_VALUES_AS_NANS
    arithmetic_block->first_delayed_copy = unprocessed;
#else // ES_VALUES_AS_NANS
    arithmetic_block->first_delayed_copy = NULL;
#endif // ES_VALUES_AS_NANS

    for (unsigned index = 0, stop = virtual_registers_count; index < stop; ++index)
    {
        VirtualRegister *vr = VR(index);

        if (vr->stack_frame_offset != INT_MAX)
#ifdef ES_VALUES_AS_NANS
            if (only_integers && vr->stack_frame_type == ESTYPE_DOUBLE)
                anything_left_to_flush = TRUE;
            else
#endif // ES_VALUES_AS_NANS
            {
                int stored_stack_frame_offset = vr->stack_frame_offset;

                EmitSetRegisterValueFromStackFrameStorage(vr);

                vr->stack_frame_offset = stored_stack_frame_offset;

#ifdef ES_VALUES_AS_NANS
                if (!keep_states)
#endif // ES_VALUES_AS_NANS
                    FreeStackFrameStorage(vr);
            }
    }

#ifdef ES_VALUES_AS_NANS
    return anything_left_to_flush;
#endif // ES_VALUES_AS_NANS
}

BOOL
ES_Native::IsReadAllocated(VirtualRegister *vr)
{
    if (RegisterAllocation *allocation = vr->current_allocation)
    {
        if (allocation->type == RegisterAllocation::TYPE_READ)
            return allocation->start <= cw_index;
        else if (allocation->type == RegisterAllocation::TYPE_WRITE)
            return allocation->start < cw_index || allocation->initial_argument_value;
    }

    return FALSE;
}

BOOL
ES_Native::IsWriteAllocated(VirtualRegister *vr)
{
    if (RegisterAllocation *allocation = vr->current_allocation)
    {
        while (allocation && allocation->start < cw_index)
            allocation = allocation->virtual_next;

        while (allocation && allocation->start == cw_index)
            if (allocation->type == RegisterAllocation::TYPE_WRITE)
                return TRUE;
            else
                allocation = allocation->virtual_next;
    }

    return FALSE;
}

ES_Native::NativeRegister *
ES_Native::ReadNR(VirtualRegister *vr)
{
    if (RegisterAllocation *allocation = vr->current_allocation)
    {
        if (allocation->type == RegisterAllocation::TYPE_READ)
        {
            if (allocation->start <= cw_index)
                return allocation->native_register;
        }
        else if (allocation->type == RegisterAllocation::TYPE_WRITE)
        {
            if (allocation->start < cw_index || allocation->initial_argument_value)
                return allocation->native_register;
        }
    }

    return NULL;
}

ES_Native::NativeRegister *
ES_Native::WriteNR(VirtualRegister *vr)
{
    if (RegisterAllocation *allocation = vr->current_allocation)
    {
        while (allocation && allocation->start < cw_index)
            allocation = allocation->virtual_next;

        while (allocation && allocation->start == cw_index)
            if (allocation->type == RegisterAllocation::TYPE_WRITE)
                return allocation->native_register;
            else
                allocation = allocation->virtual_next;
    }

    return NULL;
}

ES_Native::NativeRegister *
ES_Native::GetScratchNR()
{
    VirtualRegister *vr = ScratchAllocationsVR();
    RegisterAllocation *allocation = vr->current_allocation;

    OP_ASSERT(allocation != NULL);
    OP_ASSERT(allocation->start <= cw_index);

    while (allocation->start < cw_index)
        allocation = allocation->virtual_next;

    NativeRegister *nr = allocation->native_register;

    vr->current_allocation = allocation->virtual_next;

    return nr;
}

BOOL
ES_Native::IsType(VirtualRegister *vr, ES_ValueType type)
{
    ES_ValueType known_type;

    if (vr == property_value_read_vr && !property_value_needs_type_check && type == ESTYPE_OBJECT)
        return TRUE;
    else if (GetType(vr, known_type))
        return type == ESTYPE_INT32_OR_DOUBLE ? known_type == ESTYPE_INT32 || known_type == ESTYPE_DOUBLE : known_type == type;
    else
        return FALSE;
}

BOOL
ES_Native::IsProfiledType(unsigned operand_index, unsigned mask)
{
    unsigned char *profile_data = code->data->profile_data;
    if (profile_data && profile_data[cw_index] != 0 && (profile_data[cw_index + operand_index] & mask) != 0)
        return TRUE;
    else
        return FALSE;
}

BOOL
ES_Native::IsTrampled(ES_Native::VirtualRegister *vr)
{
    if (!vr->is_temporary && code->type == ES_Code::TYPE_FUNCTION)
    {
        unsigned formals_count = static_cast<ES_FunctionCode *>(code)->GetData()->formals_count;

        if (vr->index >= 2 && vr->index < 2 + formals_count)
            return TRUE;
        else if (optimization_data->trampled_variables && vr->index >= 2 + formals_count)
            return TRUE;
    }

    return FALSE;
}

BOOL
ES_Native::GetType(VirtualRegister *vr, ES_ValueType &type)
{
    if (IsTrampled(vr))
        return FALSE;

    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteReadAt(cw_index, vr->index);

    if (access->IsTypeKnown())
    {
        if (access->IsSingleType())
            type = access->GetSingleType();
        else if (access->IsNumberType())
            type = ESTYPE_INT32_OR_DOUBLE;
        else
            return FALSE;

        return TRUE;
    }
    else
        return FALSE;
}

BOOL
ES_Native::IsImmediate(VirtualRegister *vr, ES_Value_Internal &value, BOOL allow_double)
{
    if (!IsTrampled(vr))
    {
        const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteReadAt(cw_index, vr->index);

        if (access->GetValue(code, value) && (allow_double || value.Type() != ESTYPE_DOUBLE))
            return TRUE;
    }

    return FALSE;
}

static BOOL
FindCurrentRegisterTypeBits(ES_Native::VirtualRegister *vr, ES_Native::ArithmeticInstructionProfile *start, ES_Native::ArithmeticInstructionProfile *end, unsigned &type)
{
    for (ES_Native::ArithmeticInstructionProfile *ip = end - 1; ip >= start; --ip)
        if (ip->target_vr == vr)
        {
            type = ES_Value_Internal::TypeBits(ip->target_type);
            return TRUE;
        }
        else if (ip->lhs_vr == vr)
        {
            type = ip->lhs_handled;
            return TRUE;
        }
        else if (ip->rhs_vr == vr)
        {
            type = ip->rhs_handled;
            return TRUE;
        }

    return FALSE;
}

static BOOL
IsConstant(ES_Native *nc, ES_Native::VirtualRegister *vr, ES_Native::ArithmeticInstructionProfile *start, ES_Native::ArithmeticInstructionProfile *end, unsigned cw_index, int *immediate = NULL)
{
    if (nc->IsTrampled(vr))
        return FALSE;

    for (ES_Native::ArithmeticInstructionProfile *ip = end - 1; ip >= start; --ip)
        if (ip->target_vr == vr)
        {
            if (immediate && ip->target_constant)
                *immediate = ip->target_immediate;
            return ip->target_constant != 0;
        }

    unsigned old_cw_index = nc->cw_index;
    nc->cw_index = cw_index;

    ES_Value_Internal value;
    BOOL is_immediate = nc->IsImmediate(vr, value, TRUE);

    if (immediate && is_immediate && value.IsInt32())
        *immediate = value.GetInt32();

    nc->cw_index = old_cw_index;
    return is_immediate;
}

static unsigned
GetStaticTypeBits(ES_Native *nc, ES_Native::VirtualRegister *vr, unsigned cw_index)
{
    if (!nc->IsTrampled(vr))
    {
        const ES_CodeOptimizationData::RegisterAccess *access = nc->optimization_data->FindWriteReadAt(cw_index, vr->index);

        if (access->IsTypeKnown())
            return access->GetType();
    }

    return 0xffu;
}

/** Returns TRUE if the instruction's target is a temporary that's later
    read at the given type, once. */
static BOOL
IsTargetReadAtSingleType(ES_CodeOptimizationData *optimization_data, ES_Native::ArithmeticInstructionProfile *ip, unsigned cw_index, ES_ValueType type)
{
    if (ip->target_vr && ip->target_vr->is_temporary)
    {
        const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteAt(cw_index, ip->target_vr->index)->GetNextRead(TRUE);
        if (access && access->IsExplicitRead() && access->IsSingleType() && access->GetSingleType() == type && !access->GetNextRead(TRUE))
            return TRUE;
    }

    return FALSE;
}

static BOOL
IsDivOrRemInstruction(ES_Instruction instr)
{
    return instr == ESI_DIV || instr == ESI_DIV_IMM || instr == ESI_REM || instr == ESI_REM_IMM;
}

void
ES_Native::ProfileArithmeticInstruction(unsigned start_instruction_index, unsigned instruction_index, BOOL &uncertain_instruction)
{
    ArithmeticInstructionProfile *start_ip = instruction_profiles + start_instruction_index;
    ArithmeticInstructionProfile *ip = instruction_profiles + instruction_index;

    unsigned cw_index = code->data->instruction_offsets[instruction_index];
    unsigned lhs_type, rhs_type, lhs_operand_index = 2, rhs_operand_index = 3;
    BOOL has_rhs = FALSE;

    ES_CodeWord *word = &code->data->codewords[cw_index];

    unsigned char empty_profile_data[4] = { 0, 0, 0, 0 }, *profiling = code->data->profile_data ? &code->data->profile_data[cw_index] : empty_profile_data;

    uncertain_instruction = FALSE;

    switch (word->instruction)
    {
    case ESI_LOAD_STRING:
        ip->target_vr = VR(word[1].index);
        ip->target_type = ESTYPE_STRING;
        ip->target_constant = 1;
        break;

    case ESI_LOAD_DOUBLE:
        if (ARCHITECTURE_HAS_FPU())
        {
            ip->target_vr = VR(word[1].index);
            ip->target_type = ESTYPE_DOUBLE;
            ip->target_constant = 1;
        }
        else
            ip->target_type = ESTYPE_UNDEFINED;
        break;

    case ESI_LOAD_INT32:
        ip->target_vr = VR(word[1].index);
        ip->target_type = ESTYPE_INT32;
        ip->target_constant = 1;
        ip->target_immediate = word[2].immediate;
        break;

    case ESI_COPY:
        ip->target_vr = VR(word[1].index);
        ip->lhs_vr = VR(word[2].index);
        if (FindCurrentRegisterTypeBits(ip->lhs_vr, start_ip, ip, lhs_type))
            ip->lhs_possible = lhs_type;
        else
        {
            ip->lhs_possible = GetStaticTypeBits(this, ip->lhs_vr, cw_index);
            if (ip->lhs_possible != ESTYPE_BITS_INT32 && ip->lhs_possible != ESTYPE_BITS_DOUBLE)
                ip->lhs_possible = 0xffu;
        }
        ip->target_constant = ip->lhs_constant = IsConstant(this, ip->lhs_vr, start_ip, ip, cw_index, &ip->lhs_immediate) ? 1 : 0;
        if (ip->lhs_possible == ESTYPE_BITS_INT32)
            ip->target_type = ESTYPE_INT32;
        else if (ARCHITECTURE_HAS_FPU() && ip->lhs_possible == ESTYPE_BITS_DOUBLE)
            ip->target_type = ESTYPE_DOUBLE;
        else
            ip->target_type = ESTYPE_UNDEFINED;
        ip->lhs_handled = ES_Value_Internal::TypeBits(ip->target_type);
        ip->rhs_possible = ip->rhs_handled = ip->rhs_constant = 0;
        break;

    case ESI_LSHIFT:
    case ESI_RSHIFT_SIGNED:
    case ESI_RSHIFT_UNSIGNED:
    case ESI_BITAND:
    case ESI_BITOR:
    case ESI_BITXOR:
        has_rhs = TRUE;

    case ESI_LSHIFT_IMM:
    case ESI_RSHIFT_SIGNED_IMM:
    case ESI_RSHIFT_UNSIGNED_IMM:
    case ESI_BITAND_IMM:
    case ESI_BITOR_IMM:
    case ESI_BITXOR_IMM:
    case ESI_COMPL:
        {
            ip->target_vr = VR(word[1].index);
            ip->lhs_vr = VR(word[2].index);
            ip->rhs_vr = has_rhs ? VR(word[3].index) : NULL;
            ip->target_constant = 0;
            ip->truncate_to_int32 = 1;

            /* If the result of a uint32-producing right shift is converted to an int32,
               only then do the truncation right away. */
            if (word->instruction == ESI_RSHIFT_UNSIGNED || word->instruction == ESI_RSHIFT_UNSIGNED_IMM)
                ip->truncate_to_int32 = IsTargetReadAtSingleType(optimization_data, ip, cw_index, ESTYPE_INT32);
            else
                ip->truncate_to_int32 = 1;

            ip->target_type = ESTYPE_INT32;
            if (FindCurrentRegisterTypeBits(ip->lhs_vr, start_ip, ip, lhs_type))
            {
                ip->lhs_possible = lhs_type;
                if ((ip->lhs_handled = ip->lhs_possible & ESTYPE_BITS_SUPPORTED_NUMBER_BITWISE) == 0)
                    ip->target_type = ESTYPE_UNDEFINED;
            }
            else
            {
                ip->lhs_possible = GetStaticTypeBits(this, ip->lhs_vr, cw_index);
                if (profiling[2])
                {
                    ip->lhs_handled = ip->lhs_possible & profiling[2] & ESTYPE_BITS_SUPPORTED_NUMBER_BITWISE;
                    if (!ARCHITECTURE_HAS_FPU())
                        ip->lhs_handled &= ~ESTYPE_BITS_DOUBLE;
                    if ((ip->lhs_handled & ESTYPE_BITS_SUPPORTED_NUMBER) == 0)
                        ip->target_type = ESTYPE_UNDEFINED;
                }
                else if ((ip->lhs_possible & ESTYPE_BITS_INT32) != 0)
                    ip->lhs_handled = ESTYPE_BITS_INT32;
                else if (ARCHITECTURE_HAS_FPU())
                    ip->lhs_handled = ip->lhs_possible & ESTYPE_BITS_DOUBLE;
                else
                    ip->lhs_handled = 0;

                if (ip->lhs_handled == 0)
                    ip->target_type = ESTYPE_UNDEFINED;
            }
            if ((ip->lhs_handled & ESTYPE_BITS_DOUBLE) != 0)
                uncertain_instruction = TRUE;
            ip->lhs_constant = IsConstant(this, ip->lhs_vr, start_ip, ip, cw_index, &ip->lhs_immediate) ? 1 : 0;

            if (has_rhs)
            {
                if (FindCurrentRegisterTypeBits(ip->rhs_vr, start_ip, ip, rhs_type))
                {
                    ip->rhs_possible = rhs_type;
                    if ((ip->rhs_handled = ip->rhs_possible & ESTYPE_BITS_SUPPORTED_NUMBER_BITWISE) == 0)
                        ip->target_type = ESTYPE_UNDEFINED;
                }
                else
                {
                    ip->rhs_possible = GetStaticTypeBits(this, ip->rhs_vr, cw_index);
                    if (profiling[3])
                    {
                        ip->rhs_handled = ip->rhs_possible & profiling[3] & ESTYPE_BITS_SUPPORTED_NUMBER_BITWISE;
                        if (!ARCHITECTURE_HAS_FPU())
                            ip->rhs_handled &= ~ESTYPE_BITS_DOUBLE;
                        if ((ip->rhs_handled & ESTYPE_BITS_SUPPORTED_NUMBER) == 0)
                            ip->target_type = ESTYPE_UNDEFINED;
                    }
                    else if ((ip->rhs_possible & ESTYPE_BITS_INT32) != 0)
                        ip->rhs_handled = ESTYPE_BITS_INT32;
                    else if (ARCHITECTURE_HAS_FPU())
                        ip->rhs_handled = ip->rhs_possible & ESTYPE_BITS_DOUBLE;
                    else
                    	ip->rhs_handled = 0;

                    if (ip->rhs_handled == 0)
                        ip->target_type = ESTYPE_UNDEFINED;
                }
                if ((ip->rhs_handled & ESTYPE_BITS_DOUBLE) != 0)
                    uncertain_instruction = TRUE;
                ip->rhs_constant = IsConstant(this, ip->rhs_vr, start_ip, ip, cw_index, &ip->rhs_immediate) ? 1 : 0;
                if (word->instruction == ESI_RSHIFT_UNSIGNED && (!ip->rhs_constant || ip->rhs_immediate == 0))
                    /* If the shift count in an unsigned right shift is zero and
                       the left-hand side is negative, the result is in the
                       range (INT_MAX, UINT_MAX], meaning it "overflows" and
                       becomes a double.  We handle that (relatively rare) case
                       by jumping to the slow case, meaning the instruction is
                       an uncertain one. */
                    uncertain_instruction = TRUE;
            }
            else
            {
                if (word->instruction != ESI_COMPL)
                {
                    ip->rhs_possible = ip->rhs_handled = ESTYPE_BITS_INT32;
                    ip->rhs_constant = 1;
                    ip->rhs_immediate = word[3].immediate;

                    if (word->instruction == ESI_RSHIFT_UNSIGNED_IMM && ip->rhs_immediate == 0)
                        uncertain_instruction = TRUE;
                }
                else
                    ip->rhs_possible = ip->rhs_handled = ip->rhs_constant = 0;
            }
        }
        break;

    case ESI_ADD:
    case ESI_SUB:
    case ESI_MUL:
    case ESI_DIV:
    case ESI_REM:
    case ESI_EQ:
    case ESI_NEQ:
    case ESI_EQ_STRICT:
    case ESI_NEQ_STRICT:
    case ESI_LT:
    case ESI_LTE:
    case ESI_GT:
    case ESI_GTE:
        has_rhs = TRUE;

    case ESI_ADD_IMM:
    case ESI_SUB_IMM:
    case ESI_MUL_IMM:
    case ESI_DIV_IMM:
    case ESI_REM_IMM:
    case ESI_EQ_IMM:
    case ESI_NEQ_IMM:
    case ESI_EQ_STRICT_IMM:
    case ESI_NEQ_STRICT_IMM:
    case ESI_LT_IMM:
    case ESI_LTE_IMM:
    case ESI_GT_IMM:
    case ESI_GTE_IMM:
    case ESI_NEG:
    case ESI_INC:
    case ESI_DEC:
        {
            if (word->instruction >= ESI_EQ && word->instruction <= ESI_GTE_IMM)
            {
                lhs_operand_index = 1;
                rhs_operand_index = 2;
            }
            else
            {
                if (word->instruction == ESI_INC || word->instruction == ESI_DEC)
                    lhs_operand_index = 1;

                ip->target_vr = VR(word[1].index);
            }

            BOOL is_division = word->instruction == ESI_DIV || word->instruction == ESI_DIV_IMM;

            ip->lhs_vr = VR(word[lhs_operand_index].index);
            ip->target_constant = 0;
            ip->truncate_to_int32 = 0;

            ip->target_type = is_division && profiling[1] != ESTYPE_BITS_INT32 ? ESTYPE_DOUBLE : ESTYPE_INT32;
            if (FindCurrentRegisterTypeBits(ip->lhs_vr, start_ip, ip, lhs_type))
                ip->lhs_handled = ip->lhs_possible = lhs_type;
            else
            {
                ip->lhs_handled = 0;
                ip->lhs_possible = GetStaticTypeBits(this, ip->lhs_vr, cw_index);
            }
            ip->lhs_constant = IsConstant(this, ip->lhs_vr, start_ip, ip, cw_index, &ip->lhs_immediate) ? 1 : 0;

            if (has_rhs)
            {
                ip->rhs_vr = VR(word[rhs_operand_index].index);
                if (FindCurrentRegisterTypeBits(ip->rhs_vr, start_ip, ip, rhs_type))
                    ip->rhs_handled = ip->rhs_possible = rhs_type;
                else
                {
                    ip->rhs_handled = 0;
                    ip->rhs_possible = GetStaticTypeBits(this, ip->rhs_vr, cw_index);
                }
                ip->rhs_constant = IsConstant(this, ip->rhs_vr, start_ip, ip, cw_index, &ip->rhs_immediate) ? 1 : 0;
            }
            else
            {
                switch (word->instruction)
                {
                case ESI_ADD_IMM:
                case ESI_SUB_IMM:
                case ESI_MUL_IMM:
                case ESI_DIV_IMM:
                case ESI_REM_IMM:
                case ESI_EQ_IMM:
                case ESI_NEQ_IMM:
                case ESI_EQ_STRICT_IMM:
                case ESI_NEQ_STRICT_IMM:
                case ESI_LT_IMM:
                case ESI_LTE_IMM:
                case ESI_GT_IMM:
                case ESI_GTE_IMM:
                    ip->rhs_possible = ip->rhs_handled = ESTYPE_BITS_INT32;
                    ip->rhs_constant = 1;
                    ip->rhs_immediate = word[rhs_operand_index].immediate;
                    break;

                default:
                    ip->rhs_possible = ip->rhs_handled = ip->rhs_constant = 0;
                }
            }

            if (eliminate_integer_overflows)
            {
                if (ip->lhs_vr->integer_no_overflow)
                    ip->lhs_handled = ip->lhs_possible = ESTYPE_BITS_INT32;

                if (has_rhs && ip->rhs_vr->integer_no_overflow)
                    ip->rhs_handled = ip->rhs_possible = ESTYPE_BITS_INT32;
            }

            if (profiling[lhs_operand_index] != 0)
            {
                OP_ASSERT(!has_rhs || profiling[rhs_operand_index] != 0);

                if (ip->lhs_handled == 0)
                    if ((profiling[lhs_operand_index] & ip->lhs_possible) != 0)
                        ip->lhs_handled = profiling[lhs_operand_index] & ip->lhs_possible;
                    else
                        ip->lhs_handled = ip->lhs_possible;
                if (ip->rhs_handled == 0 && has_rhs)
                    if ((profiling[rhs_operand_index] & ip->rhs_possible) != 0)
                        ip->rhs_handled = profiling[rhs_operand_index] & ip->rhs_possible;
                    else
                        ip->rhs_handled = ip->rhs_possible;

                if (is_light_weight && ((profiling[lhs_operand_index] | profiling[rhs_operand_index]) & ~(ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE)) != 0)
                    LEAVE(OpStatus::ERR);

                OP_ASSERT(ip->lhs_handled != 0);
            }

            /* Strategy: if LHS/RHS should be handled inline as a double, or if
               either cannot be an integer, we use double arithmetics. */
            if (            (ip->lhs_handled & ESTYPE_BITS_DOUBLE) != 0 || (ip->lhs_possible & ESTYPE_BITS_INT32) == 0 ||
                has_rhs && ((ip->rhs_handled & ESTYPE_BITS_DOUBLE) != 0 || (ip->rhs_possible & ESTYPE_BITS_INT32) == 0))
                ip->target_type = ESTYPE_DOUBLE;

            /* Strategy: if the result has ever been a double, we use double
               arithmetics, unless the result is stored in a temporary register
               and is only read by instructions that convert the result to an
               int32. */
            if ((profiling[1] & ESTYPE_BITS_DOUBLE) != 0)
            {
                if (IsTargetReadAtSingleType(optimization_data, ip, cw_index, ESTYPE_INT32))
                    ip->truncate_to_int32 = 1;
                else
                    ip->target_type = ESTYPE_DOUBLE;
            }

            /* Strategy: if we have runtime type profiling information, handle
               all profiled types that are at all possible.  Otherwise, if the
               "native" type of the operation (integer or double) is possible,
               handle only that, otherwise handle both integer or double (unless
               only one of them is possible.)

               NOTE: both the first and the last case can in theory end up with
               no handled types, for instance if the register has only been
               profiled to contain a string or if the register is statically
               known to contain a string.  This results in an unconditional
               failure to the slow path, in which case the code generator can
               stop generating fast path code. */

            if (profiling[lhs_operand_index] == 0)
            {
                if (ip->lhs_handled == 0)
                    if ((ip->lhs_possible & ES_Value_Internal::TypeBits(ip->target_type)) != 0)
                        ip->lhs_handled = ES_Value_Internal::TypeBits(ip->target_type);
                    else
                        ip->lhs_handled = ip->lhs_possible & (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE);

                if (ip->rhs_handled == 0 && has_rhs)
                    if ((ip->rhs_possible & ES_Value_Internal::TypeBits(ip->target_type)) != 0)
                        ip->rhs_handled = ES_Value_Internal::TypeBits(ip->target_type);
                    else
                        ip->rhs_handled = ip->rhs_possible & (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE);
            }

            unsigned supported_types = ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE;

            if (ip->target_type == ESTYPE_DOUBLE)
                switch (word->instruction)
                {
                case ESI_EQ:
                case ESI_EQ_IMM:
                case ESI_NEQ:
                case ESI_NEQ_IMM:
                case ESI_EQ_STRICT:
                case ESI_EQ_STRICT_IMM:
                case ESI_NEQ_STRICT:
                case ESI_NEQ_STRICT_IMM:
                    break;

                default:
                    /* For all instructions except equality comparisons we can
                       quite easily handle undefined in the double case, since
                       undefined simply becomes NaN. */
                    supported_types |= ESTYPE_BITS_UNDEFINED;
                }

            if ((ip->lhs_handled & ~supported_types) != 0 || ip->lhs_handled == ESTYPE_BITS_UNDEFINED ||
                has_rhs && ((ip->rhs_handled & ~supported_types) != 0 || ip->rhs_handled == ESTYPE_BITS_UNDEFINED))
                /* Signal to ContinuesArithmeticBlock() that this instruction
                   shouldn't be part of the arithmetic block after all: */
                ip->target_type = ESTYPE_UNDEFINED;

            if (ip->lhs_handled == 0 || has_rhs && ip->rhs_handled == 0)
                ip->target_type = ESTYPE_UNDEFINED;

#ifndef ARCHITECTURE_CAP_INTEGER_DIVISION
            if (ip->target_type == ESTYPE_INT32 && IsDivOrRemInstruction(word->instruction))
                /* If CPU lacks integer division support, do double division
                   instead, unless we can reduce the operation to a simple shift
                   or mask. */
                if (!ip->rhs_constant || ip->rhs_immediate <= 0 || (ip->rhs_immediate & (ip->rhs_immediate - 1)) != 0)
                    if (ARCHITECTURE_HAS_FPU())
                        ip->target_type = ESTYPE_DOUBLE;
                    else
                        ip->target_type = ESTYPE_UNDEFINED;
#endif // ARCHITECTURE_CAP_INTEGER_DIVISION

            if (!ARCHITECTURE_HAS_FPU() && ip->target_type == ESTYPE_DOUBLE)
                /* If we're compiling for a CPU without a (usable) FPU, then
                   emit int32-only instructions if it seems plausible that all
                   of LHS, RHS and result are int32.  If we believe that either
                   LHS, RHS or the result will for sure be a double, then say
                   this is an arithmetic we can't handle at all, by setting the
                   target type to ESTYPE_UNDEFINED. */
                if ((ip->lhs_handled & ESTYPE_BITS_INT32) != 0 && (!has_rhs || (ip->rhs_handled & ESTYPE_BITS_INT32) != 0) &&
                    !IsDivOrRemInstruction(word->instruction) && !is_light_weight)
                {
                    ip->lhs_handled &= ~ESTYPE_BITS_DOUBLE;
                    if (has_rhs)
                        ip->rhs_handled &= ~ESTYPE_BITS_DOUBLE;
                    ip->target_type = ESTYPE_INT32;
                }
                else
                    ip->target_type = ESTYPE_UNDEFINED;

            if (ip->target_type == ESTYPE_INT32)
                switch (word->instruction)
                {
                case ESI_ADD:
                case ESI_ADD_IMM:
                case ESI_SUB:
                case ESI_SUB_IMM:
                case ESI_MUL:
                case ESI_MUL_IMM:
                case ESI_DIV:
                case ESI_DIV_IMM:
                case ESI_REM:
                case ESI_REM_IMM:
                case ESI_NEG:
                    /* Result may turn out to be a double. */
                    uncertain_instruction = TRUE;
                    break;

                case ESI_INC:
                case ESI_DEC:
                    if (ip->target_vr->is_temporary || IsTrampled(ip->target_vr))
                        uncertain_instruction = TRUE;
                    else
                    {
                        const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteAt(cw_index, ip->target_vr->index);

                        if (!access->IsTypeKnown() || access->GetType() != ESTYPE_BITS_INT32)
                            uncertain_instruction = TRUE;
                    }
                }
        }
        break;

    case ESI_STORE_BOOLEAN:
        ip->target_vr = VR(word[1].index);
        ip->target_type = ESTYPE_BOOLEAN;
        break;

    case ESI_CONDITION:
    case ESI_JUMP_IF_TRUE:
    case ESI_JUMP_IF_FALSE:
        break;

    case ESI_RETURN_VALUE:
        ip->target_vr = NULL;
        ip->lhs_vr = VR(word[1].index);

        if (FindCurrentRegisterTypeBits(ip->lhs_vr, start_ip, ip, lhs_type) && (lhs_type & (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE)) == lhs_type)
        {
            ip->lhs_handled = ip->lhs_possible = lhs_type;
            ip->lhs_constant = IsConstant(this, ip->lhs_vr, start_ip, ip, cw_index, &ip->lhs_immediate) ? 1 : 0;

            OP_ASSERT(ARCHITECTURE_HAS_FPU() || ip->lhs_handled == ESTYPE_BITS_INT32);

            if ((ip->lhs_handled & ESTYPE_BITS_DOUBLE) != 0)
                ip->target_type = ESTYPE_DOUBLE;
            else
                ip->target_type = ESTYPE_INT32;
        }
        else
            ip->target_type = ESTYPE_UNDEFINED;
        break;
    }

    if (ip->target_type != ESTYPE_UNDEFINED)
        if (ip->lhs_vr && ip->lhs_handled != ip->lhs_possible)
            uncertain_instruction = TRUE;
        else if (ip->rhs_vr && ip->rhs_handled != ip->rhs_possible)
            uncertain_instruction = TRUE;
}

void
ES_Native::SetFunctionCallReturnTarget(ES_CodeGenerator::JumpTarget *return_target)
{
    FunctionCall *fc = OP_NEWGRO_L(FunctionCall, (), Arena());

    fc->return_target = return_target;
    fc->codeword = CurrentCodeWord();
    fc->next = first_function_call;

    first_function_call = fc;
    ++function_call_count;
}

BOOL
ES_Native::StartsArithmeticBlock(unsigned instruction_index, unsigned &failing_index, BOOL &uncertain_instruction)
{
    OP_ASSERT(!current_jump_target || current_jump_target->index != code->data->instruction_offsets[instruction_index]);
    return ContinuesArithmeticBlock(instruction_index, instruction_index, failing_index, uncertain_instruction);
}

BOOL
ES_Native::ContinuesArithmeticBlock(unsigned start_instruction_index, unsigned instruction_index, unsigned &failing_index, BOOL &uncertain_instruction)
{
    if (start_instruction_index != instruction_index)
        switch (code->data->codewords[code->data->instruction_offsets[instruction_index - 1]].instruction)
        {
        case ESI_RETURN_VALUE:
        case ESI_JUMP_IF_TRUE:
        case ESI_JUMP_IF_FALSE:
            return FALSE;
        }

    failing_index = instruction_index;
again:
    unsigned cw_index = code->data->instruction_offsets[instruction_index];

    ES_CodeWord *previous = start_instruction_index != instruction_index ? &code->data->codewords[code->data->instruction_offsets[instruction_index - 1]] : NULL;
    ES_CodeWord *word = &code->data->codewords[cw_index];

    ArithmeticInstructionProfile *ip = instruction_profiles + instruction_index;

    /* If instruction is a following jump target or the entry point, it does not extend the arithmetic block. */
    if (previous)
        if (current_jump_target && current_jump_target->index == cw_index)
            return FALSE;
        else if (entry_point_cw_index == cw_index)
            switch (word->instruction)
            {
            case ESI_RETURN_VALUE:
            case ESI_JUMP_IF_TRUE:
            case ESI_JUMP_IF_FALSE:
                break;

            default:
                return FALSE;
            }

    ProfileArithmeticInstruction(start_instruction_index, instruction_index, uncertain_instruction);

#ifndef ARCHITECTURE_CAP_FPU
    OP_ASSERT(ip->target_type != ESTYPE_DOUBLE);
#endif // ARCHITECTURE_CAP_FPU

    switch (word->instruction)
    {
#ifdef ARCHITECTURE_CAP_FPU
    case ESI_LOAD_DOUBLE:
        if (!ARCHITECTURE_HAS_FPU())
            return FALSE;
#endif // ARCHITECTURE_CAP_FPU
    case ESI_LOAD_INT32:
        failing_index = instruction_index++;
        goto again;

    case ESI_COPY:
        if (ip->target_type == ESTYPE_UNDEFINED)
            return FALSE;
        failing_index = instruction_index++;
        goto again;

    case ESI_REM:
    case ESI_REM_IMM:
        if (ip->lhs_handled != ESTYPE_BITS_INT32 || ip->rhs_handled != ESTYPE_BITS_INT32 || ip->target_type != ESTYPE_INT32)
            if (is_light_weight)
                LEAVE(OpStatus::ERR);
            else
                return FALSE;
        return TRUE;

    case ESI_LSHIFT:
    case ESI_LSHIFT_IMM:
    case ESI_RSHIFT_SIGNED:
    case ESI_RSHIFT_SIGNED_IMM:
    case ESI_RSHIFT_UNSIGNED:
    case ESI_RSHIFT_UNSIGNED_IMM:
    case ESI_ADD:
    case ESI_ADD_IMM:
    case ESI_SUB:
    case ESI_SUB_IMM:
    case ESI_MUL:
    case ESI_MUL_IMM:
    case ESI_DIV:
    case ESI_DIV_IMM:
    case ESI_BITAND:
    case ESI_BITAND_IMM:
    case ESI_BITOR:
    case ESI_BITOR_IMM:
    case ESI_BITXOR:
    case ESI_BITXOR_IMM:
    case ESI_COMPL:
    case ESI_NEG:
    case ESI_INC:
    case ESI_DEC:
    case ESI_EQ:
    case ESI_EQ_IMM:
    case ESI_NEQ:
    case ESI_NEQ_IMM:
    case ESI_EQ_STRICT:
    case ESI_EQ_STRICT_IMM:
    case ESI_NEQ_STRICT:
    case ESI_NEQ_STRICT_IMM:
    case ESI_LT:
    case ESI_LT_IMM:
    case ESI_LTE:
    case ESI_LTE_IMM:
    case ESI_GT:
    case ESI_GT_IMM:
    case ESI_GTE:
    case ESI_GTE_IMM:
        if (ip->target_type == ESTYPE_UNDEFINED)
            if (is_light_weight)
                LEAVE(OpStatus::ERR);
            else
                return FALSE;
        return TRUE;

    case ESI_CONDITION:
        if (previous != NULL)
            switch (previous->instruction)
            {
            case ESI_LOAD_DOUBLE:
            case ESI_LOAD_INT32:
                return FALSE;

            case ESI_LSHIFT:
            case ESI_LSHIFT_IMM:
            case ESI_RSHIFT_SIGNED:
            case ESI_RSHIFT_SIGNED_IMM:
            case ESI_RSHIFT_UNSIGNED:
            case ESI_RSHIFT_UNSIGNED_IMM:
            case ESI_ADD:
            case ESI_ADD_IMM:
            case ESI_SUB:
            case ESI_SUB_IMM:
            case ESI_MUL:
            case ESI_MUL_IMM:
            case ESI_DIV:
            case ESI_DIV_IMM:
            case ESI_BITAND:
            case ESI_BITAND_IMM:
            case ESI_BITOR:
            case ESI_BITOR_IMM:
            case ESI_BITXOR:
            case ESI_BITXOR_IMM:
            case ESI_COMPL:
            case ESI_NEG:
            case ESI_INC:
            case ESI_DEC:
                return previous[1].index == word[1].index;
            }
        return FALSE;

    case ESI_RETURN_VALUE:
        return ip->target_type == ESTYPE_INT32 || ip->target_type == ESTYPE_DOUBLE;

    case ESI_STORE_BOOLEAN:
    case ESI_JUMP_IF_TRUE:
    case ESI_JUMP_IF_FALSE:
        if (previous != NULL)
            switch (previous->instruction)
            {
            case ESI_STORE_BOOLEAN:
                OP_ASSERT(word->instruction == ESI_JUMP_IF_TRUE || word->instruction == ESI_JUMP_IF_FALSE);

            case ESI_EQ:
            case ESI_EQ_IMM:
            case ESI_NEQ:
            case ESI_NEQ_IMM:
            case ESI_EQ_STRICT:
            case ESI_EQ_STRICT_IMM:
            case ESI_NEQ_STRICT:
            case ESI_NEQ_STRICT_IMM:
            case ESI_LT:
            case ESI_LT_IMM:
            case ESI_LTE:
            case ESI_LTE_IMM:
            case ESI_GT:
            case ESI_GT_IMM:
            case ESI_GTE:
            case ESI_GTE_IMM:
            case ESI_CONDITION:
                return TRUE;
            }
        return FALSE;

    default:
        return FALSE;
    }
}

void
ES_Native::ArithmeticBlock::AddDelayedCopy(unsigned cw_index, VirtualRegister *source, VirtualRegister *target, OpMemGroup *arena)
{
    first_delayed_copy = OP_NEWGRO_L(DelayedCopy, (cw_index, source, target, first_delayed_copy), arena);
}

BOOL
ES_Native::GetConditionalTargets(unsigned cw_index, Operand &value_target, ES_CodeGenerator::JumpTarget *&true_target, ES_CodeGenerator::JumpTarget *&false_target)
{
    ES_CodeWord *word = &code->data->codewords[cw_index];

    switch (word->instruction)
    {
    case ESI_JUMP_IF_TRUE:
    case ESI_JUMP_IF_FALSE:
        {
        handle_jump:
            unsigned jump_target_cw_index = cw_index + 1 + word[1].offset;
            JumpTarget *jump_target = FindJumpTarget(jump_target_cw_index);
            ES_CodeGenerator::JumpTarget *cg_target;

            if (jump_target_cw_index < cw_index)
                cg_target = jump_target->here;
            else
            {
                if (!jump_target->forward_jump)
                    jump_target->forward_jump = cg.ForwardJump();

                cg_target = jump_target->forward_jump;
            }

            if (word->instruction == ESI_JUMP_IF_TRUE)
                true_target = cg_target;
            else
                false_target = cg_target;
        }
        return TRUE;

    case ESI_STORE_BOOLEAN:
        value_target = Operand(VR(word[1].index));

        word += 2;
        if (word->instruction == ESI_JUMP_IF_TRUE || word->instruction == ESI_JUMP_IF_FALSE)
        {
            cw_index += 2;
            goto handle_jump;
        }

        return TRUE;

    default:
        return FALSE;
    }
}

extern unsigned CountValidPropertyCaches(ES_Code*, ES_CodeWord *, BOOL);

/* static */ BOOL
ES_Native::IsHandledInline(ES_Code *code, ES_CodeWord *word)
{
    switch (word->instruction)
    {
    case ESI_LOAD_STRING:
    case ESI_LOAD_DOUBLE:
    case ESI_LOAD_INT32:
    case ESI_LOAD_TRUE:
    case ESI_LOAD_FALSE:
    case ESI_LOAD_NULL:
    case ESI_LOAD_UNDEFINED:
    case ESI_LOAD_GLOBAL_OBJECT:
    case ESI_COPY:
    case ESI_TONUMBER:
    case ESI_TOPRIMITIVE:
    case ESI_TOPRIMITIVE1:
    case ESI_SUB:
    case ESI_SUB_IMM:
    case ESI_MUL:
    case ESI_MUL_IMM:
    case ESI_DIV:
    case ESI_DIV_IMM:
    case ESI_REM:
    case ESI_REM_IMM:
    case ESI_NEG:
    case ESI_INC:
    case ESI_DEC:
    case ESI_LSHIFT:
    case ESI_LSHIFT_IMM:
    case ESI_RSHIFT_SIGNED:
    case ESI_RSHIFT_SIGNED_IMM:
    case ESI_RSHIFT_UNSIGNED:
    case ESI_RSHIFT_UNSIGNED_IMM:
    case ESI_BITAND:
    case ESI_BITAND_IMM:
    case ESI_BITOR:
    case ESI_BITOR_IMM:
    case ESI_BITXOR:
    case ESI_BITXOR_IMM:
    case ESI_COMPL:
    case ESI_EQ:
    case ESI_EQ_IMM:
    case ESI_NEQ:
    case ESI_NEQ_IMM:
    case ESI_EQ_STRICT:
    case ESI_EQ_STRICT_IMM:
    case ESI_NEQ_STRICT:
    case ESI_NEQ_STRICT_IMM:
    case ESI_LT:
    case ESI_LT_IMM:
    case ESI_LTE:
    case ESI_LTE_IMM:
    case ESI_GT:
    case ESI_GT_IMM:
    case ESI_GTE:
    case ESI_GTE_IMM:
    case ESI_CONDITION:
    case ESI_STORE_BOOLEAN:
    case ESI_IS_NULL_OR_UNDEFINED:
    case ESI_IS_NOT_NULL_OR_UNDEFINED:
    case ESI_GET:
    case ESI_GETI_IMM:
    case ESI_GET_LEXICAL:
    case ESI_JUMP:
    case ESI_JUMP_IF_TRUE:
    case ESI_JUMP_IF_FALSE:
    case ESI_RETURN_VALUE:
    case ESI_RETURN_NO_VALUE:
        return TRUE;

    case ESI_ADD:
        if (code->data->profile_data && (code->data->profile_data[(word - code->data->codewords) + 1] & ESTYPE_BITS_STRING) != 0)
            return FALSE;
        return TRUE;

    case ESI_GET_GLOBAL:
    case ESI_PUT_GLOBAL:
        return code->global_caches[word[3].index].class_id != ES_Class::NOT_CACHED_CLASS_ID;

    case ESI_GETN_IMM:
    case ESI_PUTN_IMM:
    case ESI_INIT_PROPERTY:
        return CountValidPropertyCaches(code, word, word->instruction == ESI_GETN_IMM) != 0;

    case ESI_PUT:
    case ESI_PUTI_IMM:
        if (code->data->profile_data && ES_Indexed_Properties::ToTypeBits(code->data->profile_data[(word - code->data->codewords) + 1]) != ES_Indexed_Properties::TYPE_BITS_TYPE_ARRAY)
            return TRUE;
        return code->data->profile_data && (code->data->profile_data[(word - code->data->codewords) + 3] & (ESTYPE_BITS_DOUBLE | ESTYPE_BITS_INT32));
    default:
        return FALSE;
    }
}

/* static */ BOOL
ES_Native::HasIntrinsicSideEffects(ES_Code *code, ES_CodeWord *word)
{
    unsigned formal_limit = static_cast<ES_FunctionCode *>(code)->GetData()->formals_count + 2;

    switch (word->instruction)
    {
    case ESI_PUT:
    case ESI_PUTI_IMM:
    case ESI_PUTN_IMM:
    case ESI_INIT_PROPERTY:
    case ESI_PUT_GLOBAL:
    case ESI_PUT_SCOPE:
    case ESI_DELETE:
    case ESI_DELETEI_IMM:
    case ESI_DELETEN_IMM:
    case ESI_DELETE_SCOPE:
        return TRUE;

#ifdef ES_COMBINED_ADD_SUPPORT
    case ESI_ADDN:
        return word[1].index < formal_limit;
#endif // ES_COMBINED_ADD_SUPPORT

    case ESI_CONSTRUCT_OBJECT:
        return word[1].index < formal_limit;

    default:
        unsigned operand_count = g_instruction_operand_count[word->instruction];
        unsigned short register_o = g_instruction_operand_register_io[word->instruction] & 0xff;

        for (unsigned index = 0; index < operand_count; ++index)
            if ((register_o & (1 << index)) != 0)
                if (word[1 + index].index < formal_limit)
                    return TRUE;

        return FALSE;
    }
}

/* static */ BOOL
ES_Native::CanAllocateObject(ES_Class *klass, unsigned nindexed)
{
    unsigned size = klass->Count();

    unsigned mainbytes = GC_ALIGN(sizeof(ES_Object));
    unsigned namedbytes = GC_ALIGN(ES_Object::AllocationSize(klass, size, size));
    unsigned indexedbytes = nindexed ? GC_ALIGN(ES_Compact_Indexed_Properties::AllocationSize(nindexed)) : 0;

    return mainbytes + namedbytes + indexedbytes < LARGE_OBJECT_LIMIT;
}

/* static */ void
ES_Native::GetObjectAllocationData(ObjectAllocationData &data, ES_Class *actual_klass, ES_Class *final_klass, unsigned property_count, unsigned *nindexed, ES_Compact_Indexed_Properties *representation)
{
    if (!final_klass)
        final_klass = actual_klass->GetMainClass();

    data.main_bytes = GC_ALIGN(sizeof(ES_Object));

    if (final_klass)
        data.named_bytes = GC_ALIGN(ES_Object::AllocationSize(final_klass, property_count, property_count));
    else if (!nindexed)
        data.named_bytes = GC_ALIGN(ES_Class::MaxSize(actual_klass));

    if (nindexed)
    {
        data.length = *nindexed;
        *nindexed = ES_Compact_Indexed_Properties::AppropriateCapacity(data.length);
    }

    data.indexed_bytes = !nindexed || representation ? 0 : GC_ALIGN(ES_Compact_Indexed_Properties::AllocationSize(*nindexed));

    if (data.main_bytes + data.named_bytes + data.indexed_bytes >= LARGE_OBJECT_LIMIT)
        data.named_bytes = GC_ALIGN(LARGE_OBJECT_LIMIT - data.main_bytes - data.indexed_bytes - GC_ALIGN(sizeof(ES_Box)));

    if (nindexed)
        data.gctag = GCTAG_ES_Object_Array;
    else
        data.gctag = GCTAG_ES_Object;

    if (nindexed)
    {
        ES_StorageType type = actual_klass->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(0)).GetStorageType();
        if (representation || type != ES_STORAGE_INT32)
            data.object_bits = ES_Object::MASK_SIMPLE_COMPACT_INDEXED;
        else
            data.object_bits = ES_Object::MASK_SIMPLE_COMPACT_INDEXED | ES_Object::MASK_MUTABLE_COMPACT_INDEXED;
    }
    else
        data.object_bits = 0;
}

BOOL
ES_Native::EliminateIntegerOverflows()
{
    if (code->has_integer_overflows != 0 || code->type != ES_Code::TYPE_FUNCTION || code->CanHaveVariableObject() || optimization_data->trampled_variables)
    {
        code->has_integer_overflows = 2;
        return FALSE;
    }

    ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);
    ES_FunctionCodeStatic *data = fncode->GetData();
    BOOL do_eliminate = FALSE;
    unsigned locals_count = data->LocalsCount();

    for (unsigned index = 0; index < locals_count; ++index)
    {
        unsigned register_index = 2 + data->formals_count + index;

        const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->register_accesses[register_index];

        while (access->cw_index < data->codewords_count)
        {
            if (!access->IsTypeKnown() || !access->IsNumberType() || (access->GetType() & ESTYPE_BITS_INT32) == 0)
                break;
            else
            {
                ES_CodeWord *word = &data->codewords[access->cw_index];

                if (!(word->instruction == ESI_LOAD_INT32 || word->instruction == ESI_INC || word->instruction == ESI_DEC))
                    break;
            }

            ++access;
        }

        if (access->cw_index == data->codewords_count)
            do_eliminate = VR(register_index)->integer_no_overflow = TRUE;
    }

    return do_eliminate;
}

void
ES_Native::StartLoop(unsigned start_cw_index, unsigned end_cw_index)
{
#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
    extern BOOL g_debug_native_behaviour;
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

    if (entry_point_cw)
    {
        unsigned entry_point_cw_index = entry_point_cw - code->data->codewords;

        if (entry_point_cw_index >= start_cw_index && entry_point_cw_index < end_cw_index)
            return;
    }

    if (code->has_failed_preconditions != 0)
    {
#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
        if (g_debug_native_behaviour && code->has_failed_preconditions == 1)
        {
            ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);

            JString *fnname = fncode->GetName() ? fncode->GetName() : fncode->GetDebugName();
            OpString8 fnname_string;

            if (fnname)
                fnname_string.Set(Storage(NULL, fnname), Length(fnname));
            else
                fnname_string.Set("<anonymous function>");

            printf("Pre-conditions have failed in %s\n", fnname_string.CStr());
        }
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

        code->has_failed_preconditions = 2;
    }
    else if (!code->CanHaveVariableObject() && code->type == ES_Code::TYPE_FUNCTION)
    {
        ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);
        ES_FunctionCodeStatic *data = fncode->GetData();
        ES_CodeWord *codewords = data->codewords;
        unsigned char *profile_data = data->profile_data;

        for (unsigned index = 2, stop = index + data->formals_count + data->LocalsCount(); index < stop; ++index)
        {
            const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteReadAt(start_cw_index, index);
            const ES_CodeOptimizationData::RegisterAccess *read = access->GetNextRead();

            while (read && read->cw_index < start_cw_index)
                read = read->GetNextRead();

            if (!read)
                access = access->GetNextWrite();

            BOOL has_type = FALSE, is_read = FALSE;
            ES_ValueType type = ESTYPE_UNDEFINED;

            while (access && access->cw_index < end_cw_index)
            {
                ES_ValueType read_type = ESTYPE_UNDEFINED;

                if (!access->GetNextRead())
                {
                    if (has_type)
                        read_type = type;
                }
                else
                {
                    if (profile_data)
                    {
                        unsigned type_bits = 0;

                        read = access->GetNextRead();

                        while (read)
                        {
                            ES_CodeWord *word = &codewords[read->cw_index];

                            unsigned operand_count = g_instruction_operand_count[word->instruction];
                            unsigned short register_i = g_instruction_operand_register_io[word->instruction] >> 8;

                            if (operand_count != UINT_MAX)
                                for (unsigned index = 0; index < operand_count; ++index)
                                    if ((register_i & (1 << index)) != 0)
                                        if (word[1 + index].index == index)
                                        {
                                            type_bits |= profile_data[read->cw_index + 1 + index];
                                            break;
                                        }

                            read = read->GetNextRead();
                        }

                        if (type_bits)
                        {
                            if ((type_bits & (type_bits - 1)) != 0)
                                goto not_handled;

                            read_type = ES_Value_Internal::TypeFromBits(type_bits);
                        }
                    }

                    if (read_type == ESTYPE_UNDEFINED)
                        if (access->IsTypeKnown())
                            if (access->IsSingleType())
                                read_type = access->GetSingleType();
                            else if (access->IsNumberType())
                                read_type = ESTYPE_INT32;
                }

                if (access->cw_index >= start_cw_index && access->IsExplicit())
                    switch (codewords[access->cw_index].instruction)
                    {
                    case ESI_INC:
                    case ESI_DEC:
                        if ((has_type ? type == ESTYPE_INT32 : read_type == ESTYPE_INT32) && (!access->IsTypeKnown() || access->GetType() == (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE)))
                            break;

                    default:
                        goto not_handled;
                    }

                if ((access->cw_index < start_cw_index && access->IsTypeKnown() && access->GetType() != (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE)) || has_type && type != read_type)
                    goto not_handled;

                if (!has_type && access->GetNextRead() && read_type != ESTYPE_UNDEFINED)
                {
                    has_type = TRUE;
                    type = read_type;

                    if (type == ESTYPE_STRING)
                        goto not_handled;
                }

                if (!is_read)
                {
                    read = access->GetNextRead();

                    while (read)
                    {
                        if (read->cw_index >= start_cw_index && read->cw_index < end_cw_index)
                        {
                            is_read = TRUE;
                            break;
                        }

                        read = read->GetNextRead();
                    }
                }

                access = access->GetNextWrite();
            }

            if (is_read && has_type)
            {
                if (!current_loop)
                    current_loop = OP_NEWGRO_L(TopLevelLoop, (start_cw_index, end_cw_index), Arena());

                current_loop->preconditions = OP_NEWGRO_L(TopLevelLoop::PreCondition, (index, type, current_loop->preconditions), Arena());
            }

        not_handled: ;
        }

        if (current_loop)
        {
            ES_CodeGenerator::JumpTarget *failure = EmitPreConditionsStart(start_cw_index + 1 + g_instruction_operand_count[ESI_START_LOOP]);
            TopLevelLoop::PreCondition *pc = current_loop->preconditions;

#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
            if (g_debug_native_behaviour)
            {
                JString *fnname = fncode->GetName() ? fncode->GetName() : fncode->GetDebugName();
                OpString8 fnname_string;

                if (fnname)
                    fnname_string.Set(Storage(NULL, fnname), Length(fnname));
                else
                    fnname_string.Set("<anonymous function>");

                printf("Pre-conditions for loop [%u, %u) in %s:\n", start_cw_index, end_cw_index, fnname_string.CStr());
            }
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

            do
            {
#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
                if (g_debug_native_behaviour)
                {
                    ES_FunctionCodeStatic *data = fncode->GetData();

                    JString *name = code->GetString(data->formals_and_locals[pc->vr_index - 2]);
                    OpString8 name_string;

                    name_string.Set(Storage(NULL, name), Length(name));
                    printf("  %s: ", name_string.CStr());

                    switch (pc->checked_type)
                    {
                    case ESTYPE_NULL: printf("null\n"); break;
                    case ESTYPE_UNDEFINED: printf("undefined\n"); break;
                    case ESTYPE_BOOLEAN: printf("boolean\n"); break;
                    case ESTYPE_INT32: printf("int32\n"); break;
                    case ESTYPE_DOUBLE: printf("double\n"); break;
                    case ESTYPE_OBJECT: printf("object\n"); break;
                    case ESTYPE_STRING: printf("string\n"); break;
                    default: printf("unhandled!\n"); break;
                    }
                }
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

                EmitRegisterTypeCheck(VR(pc->vr_index), pc->checked_type, failure);
            }
            while ((pc = pc->next) != NULL);
        }
    }
}

BOOL
ES_Native::IsPreConditioned(VirtualRegister *vr, ES_ValueType type)
{
    if (current_loop)
    {
        if (vr->index >= 2 && !vr->is_temporary)
        {
            TopLevelLoop::PreCondition *pc = current_loop->preconditions;

            do
                if (pc->vr_index == vr->index)
                    return pc->checked_type == type;
            while ((pc = pc->next) != NULL);
        }
    }

    return FALSE;
}

BOOL
ES_Native::CheckInlineFunction(unsigned get_cw_index, VirtualRegister *vr_function, BOOL *needs_class_check_on_entry)
{
    if (code->total_inline_failures > 10 || inlined_function_call || !allow_inlined_calls)
        return FALSE;

    ES_CodeWord *word = &code->data->codewords[get_cw_index], *next_word;
    ES_Value_Internal function_value;
    ES_Class *this_object_class;

    if (!vr_function->is_temporary)
        return FALSE;

    BOOL already_failed_inlining = FALSE;

    if (word->instruction == ESI_GETN_IMM)
    {
        unsigned char *profile_data = code->data->profile_data;
        /* If the instruction hasn't recorded a cache hit of the property,
           do not inline. Avoids accidentally inlining a function with the
           same name that was cached elsewhere. */
        if (profile_data && (*(profile_data + cw_index + 3) & ES_CodeStatic::GET_CACHE_HIT) == 0)
            return FALSE;

        already_failed_inlining = profile_data && (*(profile_data + get_cw_index + 3) & ES_CodeStatic::GET_FAILED_INLINE) != 0;

        ES_Code::PropertyCache *cache = &code->property_get_caches[word[4].index];
        if (cache->class_id == ES_Class::NOT_CACHED_CLASS_ID || cache->IsNegativeCache() || cache->data.prototype_object == NULL || cache->next != NULL || cache->data.prototype_object->GetSubObjectClass() == NULL || cache->object_class->Id() != cache->class_id)
            return FALSE;

        this_object_class = cache->object_class;

        ES_Object *prototype = cache->data.prototype_object;
        ES_Class *prototype_class = prototype->Class();
        ES_PropertyIndex index = cache->data.prototype_object->Class()->IndexOf(code->GetString(word[3].index));
        OP_ASSERT(code->GetString(word[3].index)->Equals(prototype_class->GetNameAtIndex(index)));
        OP_ASSERT(index < prototype->Count());

        ES_Property_Info info = prototype_class->GetPropertyInfoAtIndex(index);

        if (!info.IsFunction())
            return FALSE;

        OP_ASSERT(prototype_class->GetLayoutInfo(info).CheckType(ES_STORAGE_OBJECT));

        prototype->GetCached(prototype_class->GetLayoutInfo(info), function_value);

        next_word = word + 5;
    }
    else
    {
        OP_ASSERT(word->instruction == ESI_GET_GLOBAL && code->global_caches[word[3].index].class_id == ES_Class::GLOBAL_IMMEDIATE_CLASS_ID);

        function_value = code->global_object->GetVariableValue(code->global_caches[word[3].index].cached_index);
        this_object_class = code->global_object->Class();

        ES_Property_Info info;

        this_object_class->Find(code->GetString(word[2].index), info, code->global_object->Count());

        if (!info.IsFunction())
            return FALSE;

        next_word = word + 4;
    }

    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteAt(get_cw_index, vr_function->index);

    if (access->GetNumberOfReads() != 1)
        return FALSE;

    unsigned call_cw_index = access->GetNextRead()->cw_index;

    if (code->data->codewords[call_cw_index].instruction != ESI_CALL || (code->data->codewords[call_cw_index + 1].index + 1) != code->data->codewords[access->cw_index + 1].index)
        return FALSE;

    if (!function_value.IsObject() || !function_value.GetObject()->IsFunctionObject() || function_value.GetObject()->IsHostObject())
        return FALSE;

    /* Instruction has already failed an inlining attempt, do not repeat. */
    if (already_failed_inlining)
        return FALSE;

    ES_Function *function = static_cast<ES_Function *>(function_value.GetObject());
    ES_FunctionCode *fncode = function->GetFunctionCode();

    if (!fncode)
        return FALSE;

    BOOL do_inline = TRUE, accesses_cached_globals = FALSE, accesses_lexical_scope = TRUE;

    if (code->has_failed_inlined_function && (entry_point_cw == next_word || entry_point_cw == &code->data->codewords[call_cw_index]))
    {
        do_inline = FALSE;
        goto process;
    }

    OP_ASSERT(word->instruction != ESI_GET_GLOBAL || !fncode->GetData()->is_function_expr);

    fncode->data->FindInstructionOffsets();

    if (!fncode->GetData()->CanInline() || !CheckIsLightWeight(fncode, &accesses_cached_globals, &accesses_lexical_scope) || accesses_cached_globals || (code->data->codewords[call_cw_index + 2].index & 0x7fffffffu) > fncode->GetData()->formals_count)
    {
        do_inline = FALSE;
        goto process;
    }

    /* If a function call is inlined and we return to somewhere in the
       middle of that inlining, we've bypassed the inlined function
       validation check at the start. Hence, also need to inject
       a check on the re-entry code path that performs the same
       validation check. */
    if (needs_class_check_on_entry && entry_point_cw >= next_word && entry_point_cw <= &code->data->codewords[call_cw_index])
        *needs_class_check_on_entry = TRUE;

#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
    extern BOOL g_debug_native_behaviour;

    if (g_debug_native_behaviour)
    {
        JString *fnname = fncode->GetName() ? fncode->GetName() : fncode->GetDebugName();
        OpString8 fnname_string;

        if (fnname)
            fnname_string.Set(Storage(NULL, fnname), Length(fnname));
        else
            fnname_string.Set("<anonymous function>");

        OpString8 this_fnname_string;

        if (code->type == ES_Code::TYPE_FUNCTION)
        {
            ES_FunctionCode *this_fncode = static_cast<ES_FunctionCode *>(code);
            JString *this_fnname = this_fncode->GetName() ? this_fncode->GetName() : this_fncode->GetDebugName();

            if (this_fnname)
                this_fnname_string.Set(Storage(NULL, this_fnname), Length(this_fnname));
            else
                this_fnname_string.Set("<anonymous function>");
        }
        else
            this_fnname_string.Set("<program>");

        printf("Inlining function call to %s from %s:\n", fnname_string.CStr(), this_fnname_string.CStr());
    }
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

    if (!fncode->data->optimization_data)
    {
        ES_Analyzer analyzer(context, fncode);

        fncode->data->optimization_data = analyzer.AnalyzeL();

        if (!fncode->data->optimization_data)
            return FALSE;
    }

process:
    if (!do_inline)
        return FALSE;

    ES_Native *native = do_inline ? OP_NEWGRO_L(ES_Native, (context, fncode, Arena(), &cg), Arena()) : NULL;
    InlinedFunctionCall *call = OP_NEWGRO_L(InlinedFunctionCall, (native), Arena());

    call->get_cw_index = get_cw_index;
    call->call_cw_index = call_cw_index;
    /* Not fetching the inlined function into its register if it isn't used by the
       inlined function is tempting. However, if the inlining function proceeds
       to fail and regeneration is forced, the code for the non-inlined function
       that follows will use it to initiate a call. Hence we must ensure that the
       function's target register has been updated when inlining -- in preparation
       for this subsequent cause of events. */
    call->function_fetched = TRUE;
    call->this_object_class = this_object_class;
    call->function = function;
    call->next = inlined_function_call;

    if (!call->native->StartInlinedFunctionCall(this_object_class, code->data->codewords[call_cw_index + 2].index))
        return FALSE;

    for (unsigned instruction_index = 0; instruction_index < fncode->data->instruction_count && !call->uses_this_object; ++instruction_index)
    {
        ES_CodeWord *word = &fncode->data->codewords[fncode->data->instruction_offsets[instruction_index]];

        unsigned operand_count = g_instruction_operand_count[word->instruction];
        unsigned short register_i = g_instruction_operand_register_io[word->instruction] >> 8;

        for (unsigned index = 0; index < operand_count; ++index)
            if ((register_i & (1 << index)) != 0 && word[1 + index].index == 0)
            {
               call->uses_this_object = TRUE;
               break;
            }
    }

    inlined_function_call = call;

    function->SetHasBeenInlined();
    return TRUE;
}

BOOL
ES_Native::CheckIsLightWeight(ES_Code *code, BOOL *accesses_cached_globals, BOOL *accesses_lexical_scope)
{
    if (code->type != ES_Code::TYPE_FUNCTION || code->data->register_frame_size >= ES_Native::LIGHT_WEIGHT_MAX_REGISTERS || static_cast<ES_FunctionCode *>(code)->GetData()->arguments_index != ES_FunctionCodeStatic::ARGUMENTS_NOT_USED)
        return FALSE;

    if (accesses_cached_globals)
        *accesses_cached_globals = FALSE;

    if (accesses_lexical_scope)
        *accesses_lexical_scope = FALSE;

    BOOL has_had_side_effect = FALSE;

    for (unsigned instruction_index = 0; instruction_index < code->data->instruction_count; ++instruction_index)
    {
        ES_CodeWord *word = &code->data->codewords[code->data->instruction_offsets[instruction_index]];

        /* Perhaps temporary limitation, perhaps not.  A side-effect free
           light-weight dispatcher if more "perfectly" light-weight (that is,
           lighter weight,) and perhaps it's just as well not to compile
           side-effectful dispatchers as light-weight at all.  But primarily,
           handling the case of a light-weight dispatcher "failing" (that is,
           turning out not to be so light-weight) when its register frame is
           allocated inside the extra capacity registers of a register block,
           moving its register frame to a new register block is a lot more
           complicated.  For a side-effect free dispatcher, which is simply
           restarted when it fails, the special case can be handled by using
           exactly the same mechanism as other cases when a native dispatcher's
           register frame doesn't fit inside the register block. */

        if (!IsHandledInline(code, word))
            return FALSE;

        if (has_had_side_effect && !g_instruction_is_trivial[word->instruction])
            return FALSE;

        if (HasIntrinsicSideEffects(code, &code->data->codewords[code->data->instruction_offsets[instruction_index]]))
            has_had_side_effect = TRUE;

        if (accesses_cached_globals && (word->instruction == ESI_GET_GLOBAL || word->instruction == ESI_PUT_GLOBAL))
        {
            ES_Code::GlobalCache &cache = code->global_caches[word[3].index];

            if (cache.class_id != ES_Class::NOT_CACHED_CLASS_ID && cache.class_id != ES_Class::GLOBAL_IMMEDIATE_CLASS_ID)
                *accesses_cached_globals = TRUE;
        }

        if (accesses_lexical_scope && (word->instruction == ESI_GET_LEXICAL || word->instruction == ESI_PUT_LEXICAL))
            *accesses_lexical_scope = TRUE;
    }

    return TRUE;
}

BOOL
ES_Native::CheckPropertyValueTransfer(VirtualRegister *property_value_vr)
{
    if (!property_value_vr->is_temporary)
        return FALSE;

    unsigned next_cw_index = code->data->instruction_offsets[instruction_index + 1];
    ES_CodeWord *word = &code->data->codewords[next_cw_index];

    if (word == entry_point_cw)
        return FALSE;

    if (next_jump_target && next_jump_target->data->index == next_cw_index)
        return FALSE;

    unsigned base_operand_index;

    switch (word->instruction)
    {
    case ESI_GETN_IMM:
    case ESI_PUTN_IMM:
    case ESI_INIT_PROPERTY:
        if (CountValidPropertyCaches(code, word, word->instruction == ESI_GETN_IMM) == 0)
            return FALSE;

    case ESI_GETI_IMM:
    case ESI_PUTI_IMM:
    case ESI_GET:
    case ESI_PUT:
        switch (word->instruction)
        {
        case ESI_GETN_IMM:
        case ESI_GETI_IMM:
        case ESI_GET:
            base_operand_index = 2;
            break;

        default:
            base_operand_index = 1;
        }

        if (word[base_operand_index].index == property_value_vr->index)
        {
            if (code->data->profile_data)
            {
                /* If the next instruction uses it at a non-object type,
                   do not set up the transfer. */
                unsigned profiled_object_type = code->data->profile_data[next_cw_index + base_operand_index];
                if (profiled_object_type == ESTYPE_BITS_STRING || profiled_object_type && (profiled_object_type & (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE)) == profiled_object_type)
                    return FALSE;
            }
            break;
        }

    default:
        return FALSE;
    }

    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteAt(cw_index, property_value_vr->index);

    if (access->GetNextRead(TRUE)->GetNextRead(TRUE))
        return FALSE;

    return TRUE;
}

void
ES_Native::SetPropertyValueTransferRegister(NativeRegister *property_value_transfer_nr, unsigned offset, BOOL needs_type_check)
{
    property_value_nr = property_value_transfer_nr;
    property_value_offset = offset;
    property_value_needs_type_check = needs_type_check;
}

void
ES_Native::ClearPropertyValueTransferRegister()
{
    OP_ASSERT(property_value_fail == NULL);
    property_value_nr = NULL;
    property_value_needs_type_check = FALSE;
}

void
ES_Native::SetPropertyValueTransferFailJumpTarget(ES_CodeGenerator::JumpTarget *continuation_target)
{
    if (property_value_fail)
    {
        cg.StartOutOfOrderBlock();
        cg.SetJumpTarget(property_value_fail);
        cg.Jump(continuation_target);
        cg.EndOutOfOrderBlock(FALSE);
        property_value_fail = NULL;
    }
}

static void
AnalyzePropertyCache(BOOL is_get_cache, ES_Native::PropertyCache &cache_data, ES_Code::PropertyCache *cache)
{
    unsigned ncaches = 0;
    unsigned handled = 0;

    if (cache->class_id != ES_Class::NOT_CACHED_CLASS_ID)
        while (cache)
        {
            if (ES_Code::IsSimplePropertyCache(cache, is_get_cache))
            {
                ES_StorageType type = cache->GetStorageType();
                handled |= ES_Value_Internal::ConvertToValueTypeBits(type);
                ncaches++;
            }

            cache = cache->next;
        }

    cache_data.size = ncaches;
    cache_data.handled = handled;
}

void
ES_Native::AnalyzePropertyCaches(ES_Code *code)
{
    if (!property_get_caches && code->data->property_get_caches_count > 0)
        property_get_caches = OP_NEWGROA_L(PropertyCache, (code->data->property_get_caches_count), Arena());

    if (!property_put_caches && code->data->property_put_caches_count > 0)
        property_put_caches = OP_NEWGROA_L(PropertyCache, (code->data->property_put_caches_count), Arena());

    for (unsigned index = 0; index < code->data->property_get_caches_count; ++index)
        AnalyzePropertyCache(TRUE, property_get_caches[index], &code->property_get_caches[index]);

    for (unsigned index = 0; index < code->data->property_put_caches_count; ++index)
        AnalyzePropertyCache(FALSE, property_put_caches[index], &code->property_put_caches[index]);
}

unsigned
ES_Native::PropertyCacheSize(unsigned index, BOOL is_get_cache) const
{
    PropertyCache *cache = is_get_cache ? property_get_caches : property_put_caches;

    if (cache)
        return cache[index].size;
    else
        return 0;
}

unsigned
ES_Native::PropertyCacheType(unsigned index, BOOL is_get_cache) const
{
    return (is_get_cache ? property_get_caches[index] : property_put_caches[index]).handled;
}

void
ES_Native::AllocateStackFrameStorage(ArithmeticBlock *arithmetic_block, VirtualRegister *vr, ES_ValueType type)
{
    int offset = 0;
    unsigned size = type == ESTYPE_DOUBLE ? 8 : 4;
    StackFrameStorage **ptr = &first_stack_frame_storage;

#ifdef ARCHITECTURE_IA32
    unsigned index = type != ESTYPE_DOUBLE ? 0 : integer_registers_count;
    unsigned stop = type != ESTYPE_DOUBLE ? integer_registers_count : native_registers_count;

    while (index != stop)
    {
        NativeRegister *nr = NR(index);
        RegisterAllocation *allocation = nr->current_allocation;

        while (allocation && allocation->end <= arithmetic_block->start_cw_index)
            allocation = allocation->native_next;

        if (!allocation || allocation->start >= arithmetic_block->end_cw_index)
        {
            BOOL already_used = FALSE;

            for (unsigned vr_index = 0; !already_used && vr_index < virtual_registers_count; ++vr_index)
                if (VR(vr_index)->stack_frame_offset == INT_MAX - 1 && VR(vr_index)->stack_frame_register == nr)
                    already_used = TRUE;

            if (!already_used)
            {
                vr->stack_frame_offset = INT_MAX - 1;
                vr->stack_frame_register = nr;
                vr->stack_frame_type = type;

                return;
            }
        }

        ++index;
    }
#endif // ARCHITECTURE_IA32

    if (StackFrameStorage *current = first_stack_frame_storage)
    {
        do
        {
            if (static_cast<unsigned>(current->offset - offset) > size)
                goto allocate;

            offset = current->offset + current->size;

            unsigned misalignment = offset % size;

            if (misalignment)
                offset += size - misalignment;

            current = *(ptr = &current->next);
        }
        while (current);
    }

allocate:
    StackFrameStorage *storage;

    if (unused_stack_frame_storage)
    {
        storage = unused_stack_frame_storage;
        unused_stack_frame_storage = storage->next;
    }
    else
        storage = OP_NEWGRO_L(StackFrameStorage, (), Arena());

    storage->offset = offset;
    storage->size = size;
    storage->next = *ptr;

    *ptr = storage;

    vr->stack_frame_offset = offset;
    vr->stack_frame_register = NULL;
    vr->stack_frame_type = type;
}

void
ES_Native::ConvertStackFrameStorage(ArithmeticBlock *arithmetic_block, VirtualRegister *vr, ES_ValueType type)
{
    if (vr->stack_frame_offset < INT_MAX)
        if (vr->stack_frame_type == ESTYPE_DOUBLE ? type != ESTYPE_DOUBLE : type == ESTYPE_DOUBLE)
        {
            FreeStackFrameStorage(vr);
            AllocateStackFrameStorage(arithmetic_block, vr, type);
        }
}

void
ES_Native::FreeStackFrameStorage(VirtualRegister *vr)
{
    int offset = vr->stack_frame_offset;

    if (offset < INT_MAX - 1)
    {
        StackFrameStorage *storage = first_stack_frame_storage, **ptr = &first_stack_frame_storage;

        while (storage->offset != offset)
            storage = *(ptr = &storage->next);

        OP_ASSERT(storage->size == ((vr->stack_frame_type == ESTYPE_DOUBLE) ? 8u : 4u));

        *ptr = storage->next;

        storage->next = unused_stack_frame_storage;
        unused_stack_frame_storage = storage;
    }

    vr->stack_frame_offset = INT_MAX;
}

void
ES_Native::UseInPlace(VirtualRegister *vr)
{
    if (!vr->value_stored)
    {
        EmitSetRegisterValue(vr, vr->value_known_in_arithmetic_block, TRUE);

        vr->value_stored = TRUE;
        vr->value_needs_flush = FALSE;
    }
}

static BOOL
CanConvert(ES_StorageType from, ES_StorageType to)
{
    if (from == ES_STORAGE_INT32_OR_DOUBLE && to != ES_STORAGE_INT32)
        from = ES_STORAGE_INT32;
    return ES_Layout_Info::IsSubType(from, to) || ES_Layout_Info::IsNullable(to) && from == ES_STORAGE_NULL;
}

BOOL
ES_Native::EmitTypeConversionCheck(ES_StorageType type, ES_StorageType source_type, ES_Native::VirtualRegister *source_vr, ES_CodeGenerator::JumpTarget *slow_case, ES_CodeGenerator::JumpTarget *&null_target, ES_CodeGenerator::JumpTarget *&int_to_double_target)
{
    if (CanConvert(source_type, type))
    {
        if (type == ES_STORAGE_DOUBLE)
        {
            if (!int_to_double_target)
                int_to_double_target = cg.ForwardJump();

            OP_ASSERT(source_type == ES_STORAGE_INT32_OR_DOUBLE || source_type == ES_STORAGE_INT32);

            if (source_type == ES_STORAGE_INT32_OR_DOUBLE)
                EmitRegisterTypeCheck(source_vr, ESTYPE_INT32, int_to_double_target, TRUE);
            else
            {
                cg.Jump(int_to_double_target);
                return TRUE;
            }
        }
        else if (source_type == ES_STORAGE_NULL)
        {
            if (!null_target)
                null_target = cg.ForwardJump();

            cg.Jump(null_target);
            return TRUE;
        }
    }
    else if (source_type == ES_STORAGE_WHATEVER || source_type == ES_STORAGE_INT32_OR_DOUBLE)
    {
        ES_CodeGenerator::JumpTarget *other_slow_case_target = slow_case;

        if (type == ES_STORAGE_DOUBLE || ES_Layout_Info::IsNullable(type) && source_type != ES_STORAGE_INT32_OR_DOUBLE)
        {
            ES_CodeGenerator::OutOfOrderBlock *other_slow_case = cg.StartOutOfOrderBlock();
            if (type == ES_STORAGE_DOUBLE)
            {
                if (!int_to_double_target)
                    int_to_double_target = cg.ForwardJump();

                EmitRegisterTypeCheck(source_vr, ESTYPE_INT32, int_to_double_target, TRUE);
            }
            else
            {
                if (!null_target)
                    null_target = cg.ForwardJump();

                EmitRegisterTypeCheck(source_vr, ESTYPE_NULL, null_target, TRUE);
            }

            cg.Jump(slow_case);
            cg.EndOutOfOrderBlock(FALSE);
            other_slow_case_target = other_slow_case->GetJumpTarget();
        }

        EmitRegisterTypeCheck(source_vr, ES_Value_Internal::ConvertToValueType(type), other_slow_case_target);
    }
    else
    {
        cg.Jump(slow_case);
        return TRUE;
    }

    return FALSE;
}

void
ES_Native::EmitSlowInstructionCall()
{
    if (property_value_read_vr || property_value_fail)
    {
        if (!current_slow_case)
            EmitSlowCaseCall();
        cg.Jump(current_slow_case->GetJumpTarget());
    }
    else
        EmitInstructionHandlerCall();
}

/* static */ int ES_CALLING_CONVENTION
ES_Native::CompareShiftyObjects(ES_Execution_Context *context, ES_Object *object1, ES_Object *object2)
{
    ES_Value_Internal value1(object1), value2(object2);
    return value1.GetObject(context) == value2.GetObject(context);
}

/* static */ int ES_CALLING_CONVENTION
ES_Native::GetDoubleAsSwitchValue(ES_Execution_Context *context, ES_Value_Internal *value)
{
    ES_Value_Internal value2(value->GetNumAsDouble());
    /* Signal a non-integral/out-of-range Double via implicit_bool. */
    if (value2.IsInt32())
    {
        context->implicit_bool = 0;
        return value2.GetInt32();
    }
    else
    {
        context->implicit_bool = value->GetNumAsDouble() != 0.0;
        return 0;
    }
}

/* static */ int ES_CALLING_CONVENTION
ES_Native::InstanceOf(ES_Execution_Context *context, ES_Object *object, ES_Object *constructor)
{
    return object->InstanceOf(context, constructor);
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::MathSin(ES_Value_Internal *reg)
{
    reg[0].SetNumber(op_sin(reg[2].GetNumAsDouble()));
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::MathCos(ES_Value_Internal *reg)
{
    reg[0].SetNumber(op_cos(reg[2].GetNumAsDouble()));
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::MathTan(ES_Value_Internal *reg)
{
    reg[0].SetNumber(op_tan(reg[2].GetNumAsDouble()));
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::MathSqrt(ES_Value_Internal *reg)
{
    reg[0].SetNumber(op_sqrt(reg[2].GetNumAsDouble()));
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::MathFloor(ES_Value_Internal *reg)
{
    if (reg[2].IsInt32())
        reg[0] = reg[2];
    else
        reg[0].SetNumber(op_floor(reg[2].GetNumAsDouble()));
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::MathCeil(ES_Value_Internal *reg)
{
    if (reg[2].IsInt32())
        reg[0] = reg[2];
    else
        reg[0].SetNumber(op_ceil(reg[2].GetNumAsDouble()));
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::MathAbs(ES_Value_Internal *reg)
{
    if (reg[2].IsInt32())
    {
        int value = reg[2].GetNumAsInt32();

        if (value == INT_MIN)
            reg[0].SetDouble((double)INT_MAX + 1);
        else
            reg[0].SetInt32(op_abs(value));
    }
    else
        reg[0].SetNumber(op_fabs(reg[2].GetNumAsDouble()));
}

/* static */ int ES_CALLING_CONVENTION
ES_Native::MathPow(ES_Value_Internal *reg)
{
    if (!reg[2].IsNumber() || !reg[3].IsNumber())
        return 0;
    else
    {
#ifndef _NO_GLOBALS_
        static BOOL first = TRUE;
        static double previous_number, previous_exp, previous_result;
#endif // _NO_GLOBALS_

        double number = reg[2].GetNumAsDouble();
        double exp = reg[3].GetNumAsDouble();
        double result;

#ifndef _NO_GLOBALS_
        if (!first && number == previous_number)
            if (exp == previous_exp)
            {
                reg[0].SetNumber(previous_result);
                return 1;
            }
            else if (exp == previous_exp + 1)
            {
                result = number * previous_result;
                goto finished;
            }
#endif // _NO_GLOBALS_

        if (op_isnan(exp) || (op_fabs(number) == 1.0 && op_isinf(exp)))
        {
            reg[0].SetNan();
            return 1;
        }
        else
        {
            result = op_pow(number, exp);

            if (op_isnan(result))
                result = ES_MathBuiltinSupport::HandlePowSpecial(result, number, exp);
        }

#ifndef _NO_GLOBALS_
    finished:
        first = FALSE;
        previous_number = number;
        previous_exp = exp;
        previous_result = result;
#endif // _NO_GLOBALS_

        reg[0].SetNumber(result);
        return 1;
    }
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::PrepareThisObject(ES_Execution_Context *context, ES_Value_Internal *this_vr)
{
    if (this_vr->IsNullOrUndefined())
        this_vr->SetObject(ES_NativeStackFrame::GetCode(ES_NativeStackFrame::GetNextFrame(context->native_stack_frame))->global_object);
    else
    {
        OP_ASSERT(this_vr->IsNumber() || this_vr->IsBoolean() || this_vr->IsString());
        this_vr->ToObject(context, FALSE);
    }
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::PrepareThisObjectLightWeight(ES_Execution_Context *context, ES_Value_Internal *this_vr)
{
    if (this_vr->IsNullOrUndefined())
        /* When called from a light-weight dispatcher, the current native stack
           frame referenced by the context is the calling function's, since a
           light-weight dispatcher doesn't set up a stack frame of its own. */
        this_vr->SetObject(ES_NativeStackFrame::GetCode(context->native_stack_frame)->global_object);
    else
    {
        /* Since a light-weight dispatcer doesn't have a native stack frame, it
           is unsafe to run the GC during the call.  But this function is called
           by the prologue, so the only registers containing interesting values
           are the ones provided by the caller, and those are protected via the
           caller's register frame. */
        OP_ASSERT(this_vr->IsNumber() || this_vr->IsBoolean() || this_vr->IsString());
        this_vr->ToObject(context, FALSE);
    }
}

/* static */ int ES_CALLING_CONVENTION
ES_Native::GetDoubleAsInt(ES_Value_Internal *value)
{
    OP_ASSERT(value->IsDouble());
    return value->GetNumAsInt32();
}

/* static */ int ES_CALLING_CONVENTION
ES_Native::GetDoubleAsIntRound(ES_Value_Internal *value)
{
    OP_ASSERT(value->IsDouble());
    return DOUBLE2INT32(value->GetNumAsIntegerRound());
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::StoreDoubleAsFloat(double *src, float *dst)
{
    *dst = static_cast<float>(*src);
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::StoreFloatAsDouble(float *src, double *dst)
{
    *dst = static_cast<double>(*src);
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::StoreIntAsDouble(int src, double *dst)
{
    *dst = static_cast<double>(src);
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::StoreIntAsFloat(int src, float *dst)
{
    *dst = static_cast<float>(src);
}

/* static */ void ES_CALLING_CONVENTION
ES_Native::StoreUIntAsDouble(unsigned src, double *dst)
{
    *dst = static_cast<double>(src);
}

#endif // ES_NATIVE_SUPPORT
