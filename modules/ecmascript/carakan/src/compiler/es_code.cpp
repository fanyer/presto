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
#include "modules/ecmascript/carakan/src/compiler/es_code.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"
#include "modules/ecmascript/carakan/src/object/es_function.h"
#include "modules/ecmascript/carakan/src/compiler/es_native.h"
#include "modules/ecmascript/carakan/src/compiler/es_analyzer.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler.h"
#include "modules/ecmascript/carakan/src/compiler/es_instruction_data.h"
#include "modules/ecmascript/carakan/src/es_program_cache.h"
#include "modules/regexp/include/regexp_advanced_api.h"

ES_CodeStatic::ConstantArrayLiteral::~ConstantArrayLiteral()
{
    OP_DELETEA(indeces);
    OP_DELETEA(values);
}

ES_CodeStatic::ES_CodeStatic(Type type)
    : codewords(NULL),
      instruction_offsets(NULL),
      parent_code(NULL),
      source_storage_owner(NULL),
      source_storage(NULL),
      string_storage_owner(NULL),
      string_storage(NULL),
      strings(NULL),
      doubles(NULL),
      functions(NULL),
      function_declarations(NULL),
      object_literal_classes(NULL),
      constant_array_literals(NULL),
      regexps(NULL),
      global_accesses(NULL),
      switch_tables(NULL),
#ifdef ES_NATIVE_SUPPORT
      loop_data(NULL),
#endif // ES_NATIVE_SUPPORT
      exception_handlers(NULL),
      inner_scopes(NULL),
      compressed_debug_records(NULL),
      debug_records(NULL),
#ifdef ES_NATIVE_SUPPORT
      first_variable_range_limit_span(NULL),
      optimization_data(NULL),
      profile_data(NULL),
#endif // ES_NATIVE_SUPPORT
      ref_count(0),
      cache_ref_count(0),
      type(type),
      privilege_level(ES_Runtime::PRIV_LVL_UNSPEC),
      prepared_for_sharing(FALSE),
      is_external(FALSE),
      is_strict_mode(FALSE),
#ifdef ECMASCRIPT_DEBUGGER
      has_debug_code(FALSE),
#endif // ECMASCRIPT_DEBUGGER
      codewords_count(0),
      instruction_count(0),
      register_frame_size(0),
      first_temporary_register(0),
      strings_count(0),
      doubles_count(0),
      functions_count(0),
      function_declarations_count(0),
      object_literal_classes_count(0),
      constant_array_literals_count(0),
      regexps_count(0),
      global_accesses_count(0),
      property_get_caches_count(0),
      property_put_caches_count(0),
#ifdef ES_NATIVE_SUPPORT
      loop_data_count(0),
#endif // ES_NATIVE_SUPPORT
      format_string_caches_count(0),
      eval_caches_count(0),
      switch_tables_count(0),
      exception_handlers_count(0),
      inner_scopes_count(0),
      debug_records_count(0)
{
}

ES_CodeStatic::~ES_CodeStatic()
{
    OP_ASSERT(ref_count == 0);

    unsigned index;

#ifndef _STANDALONE
    MemoryManager::DecDocMemoryCount(sizeof(ES_CodeWord) * codewords_count);
#endif // !_STANDALONE
    OP_DELETEA(codewords);
    OP_DELETEA(instruction_offsets);

    if (!parent_code)
    {
        ES_StaticStringData::DecRef(source_storage_owner);
        ES_StaticStringData::DecRef(string_storage_owner);

        OP_DELETEA(strings);
        OP_DELETEA(doubles);

        for (index = 0; index < functions_count; ++index)
            DecRef(functions[index]);

        OP_DELETEA(functions);
        OP_DELETEA(function_declarations);
        OP_DELETEA(object_literal_classes);
        OP_DELETEA(constant_array_literals);

        for (index = 0; index < regexps_count; ++index)
            regexps[index].regexp->DecRef();

        OP_DELETEA(regexps);
        OP_DELETEA(global_accesses);
#ifdef ES_NATIVE_SUPPORT
        OP_DELETEA(loop_data);
#endif // ES_NATIVE_SUPPORT
        OP_DELETEA(switch_tables);
        OP_DELETEA(exception_handlers);
        OP_DELETEA(inner_scopes);
    }

    OP_DELETEA(compressed_debug_records);
    OP_DELETEA(debug_records);

#ifdef ES_NATIVE_SUPPORT
    VariableRangeLimitSpan *vrls = first_variable_range_limit_span;
    while (vrls)
    {
        VariableRangeLimitSpan *next = vrls->next;
        OP_DELETE(vrls);
        vrls = next;
    }

    OP_DELETE(optimization_data);
    OP_DELETEA(profile_data);
#endif // ES_NATIVE_SUPPORT
}

void
ES_CodeStatic::SetParentCode(ES_CodeStatic *new_parent_code)
{
    parent_code = new_parent_code;

#define COPY(what) what = new_parent_code->what, what##_count = new_parent_code->what##_count
#define COPYN(what) what##_count = new_parent_code->what##_count

    COPY(strings);
    COPY(doubles);
    COPY(object_literal_classes);
    COPY(constant_array_literals);
    COPY(functions);
    COPY(regexps);
    COPYN(property_get_caches);
    COPYN(property_put_caches);
    COPYN(global_accesses);
    COPY(inner_scopes);
    COPYN(format_string_caches);
    COPYN(eval_caches);
    COPY(switch_tables);

#undef COPYN
#undef COPY
}

void
ES_Code::DiscardCacheRecord(ES_Execution_Context *context, BOOL get_cache, unsigned index, PropertyCache *record)
{
    PropertyCache *discard;

    if (record->next)
    {
        discard = record->next;
        *record = *discard;
    }
    else
    {
        PropertyCache *first = &(get_cache ? property_get_caches : property_put_caches)[index];

        if (record == first)
        {
            *record = PropertyCache();
            return;
        }
        else
        {
            while (first->next != record)
                first = first->next;

            first->next = NULL;
            discard = record;
        }
    }

    if (context->IsUsingStandardStack())
        OP_DELETE(discard);
    else
    {
        ES_Suspended_DELETE<ES_Code::PropertyCache> suspended(discard);
        context->SuspendedCall(&suspended);
    }
}

void
ES_Code::RemoveCacheRecord(ES_Execution_Context *context, BOOL get_cache, unsigned index, PropertyCache *record)
{
    PropertyCache *discard;

    if (record->next)
    {
        discard = record->next;
        *record = *discard;
    }
    else
    {
        PropertyCache *first = &(get_cache ? property_get_caches : property_put_caches)[index];

        if (record == first)
        {
            *record = PropertyCache();
            return;
        }
        else
        {
            while (first->next != record)
                first = first->next;

            first->next = NULL;
            discard = record;
        }
    }

    context->CachePropertyCache(discard);
}

/* static */ BOOL
ES_Code::IsSimplePropertyCache(PropertyCache *cache, BOOL is_get_cache)
{
    if (is_get_cache)
        return !ES_Class::HasHashTableProperties(cache->class_id) || (!cache->IsNegativeCache() && !cache->data.prototype_object);
    else
        return !ES_Class::HasHashTableProperties(cache->class_id) || !cache->data.new_class;
}

/* static */ void
ES_Code::Initialize(ES_Code *code, ES_CodeStatic *data, ES_Global_Object *global_object, Type type)
{
    op_memset(reinterpret_cast<char *>(code) + sizeof(ES_Header), 0, sizeof(*code) - sizeof(ES_Header));

    code->data = ES_CodeStatic::IncRef(data);
    code->global_object = global_object;
    code->type = type;
}

static void
ES_DeleteCaches(ES_Code::PropertyCache *caches, unsigned count)
{
    if (caches)
    {
        for (unsigned index = 0; index < count; ++index)
            if (ES_Code::PropertyCache *cache = caches[index].next)
            {
                ES_Code::PropertyCache *next;
                do
                {
                    next = cache->next;
                    OP_DELETE(cache);
                }
                while ((cache = next) != NULL);
            }
        OP_DELETEA(caches);
    }
}

/* static */ void
ES_Code::Destroy(ES_Code *code)
{
    if (!code->parent_code)
    {
        OP_DELETEA(code->strings);
        OP_DELETEA(code->regexps);
        OP_DELETEA(code->object_literal_classes);
        /* The GC releases the object_literal_prototype_classes object. */
        code->object_literal_prototype_classes = NULL;
        OP_DELETEA(code->constant_array_literals);
        OP_DELETEA(code->functions);

        ES_DeleteCaches(code->property_get_caches, code->data->property_get_caches_count);
        ES_DeleteCaches(code->property_put_caches, code->data->property_put_caches_count);

        OP_DELETEA(code->global_caches);
        code->global_caches = NULL;

#ifdef ES_NATIVE_SUPPORT
        if (code->loop_dispatcher_codes)
        {
            for (unsigned index = 0; index < code->data->loop_data_count; ++index)
                if (code->loop_dispatcher_codes[index])
                    OP_DELETE(code->loop_dispatcher_codes[index]->native_code_block);
            OP_DELETEA(code->loop_dispatcher_codes);
        }

        OP_DELETEA(code->loop_counters);
#endif // ES_NATIVE_SUPPORT

        OP_DELETEA(code->format_string_caches);
        OP_DELETEA(code->eval_caches);
#ifdef ES_NATIVE_SUPPORT
        OP_DELETEA(code->switch_tables);
#endif // ES_NATIVE_SUPPORT
        OP_DELETEA(code->scope_chain);
    }

#ifdef ES_NATIVE_SUPPORT
    if (code->native_code_block)
        OP_DELETE(code->native_code_block);

    OP_DELETEA(code->codeword_from_return_address);

    while (LoopIO *loop_io = code->first_loop_io)
    {
        code->first_loop_io = loop_io->next;
        OP_DELETE(loop_io);
    }
#endif // ES_NATIVE_SUPPORT

    ES_CodeStatic::DecRef(code->data);
}

BOOL
ES_Code::CanHaveVariableObject()
{
    if (type == TYPE_FUNCTION)
        return static_cast<ES_FunctionCode *>(this)->GetData()->CanHaveVariableObject();
    else if (type == TYPE_EVAL_PLAIN)
        return TRUE;
    else
        return FALSE;
}

void
ES_Code::PrepareForExecution(ES_Context *context)
{
    OptimizeGlobalAccessors(context);
}

/*static */ BOOL
ES_CodeStatic::DecRef(ES_CodeStatic *data)
{
    if (data)
    {
        --data->ref_count;
        if (data->ref_count + data->cache_ref_count == 0)
        {
            if (data->type == TYPE_PROGRAM)
            {
                ES_ProgramCodeStatic* program = static_cast<ES_ProgramCodeStatic*>(data);
                if (program->program_cache)
                {
                    program->program_cache->TouchProgram(program);
                    if (data->cache_ref_count != 0)
                        return TRUE;
                }
            }
            OP_DELETE(data);
            return FALSE;
        }
    }
    return TRUE;
}


/* static */ BOOL
ES_CodeStatic::DecCacheRef(ES_CodeStatic *data)
{
    OP_ASSERT(data->cache_ref_count == 1);
    --data->cache_ref_count;
    if (data->cache_ref_count + data->ref_count == 0)
    {
        OP_DELETE(data);
        return FALSE;
    }
    return TRUE;
}

void
ES_CodeStatic::SetIsExternal(BOOL value)
{
    is_external = value;

    for (unsigned index = 0; index < functions_count; ++index)
        functions[index]->SetIsExternal(value);
}

void
ES_CodeStatic::SetPrivilegeLevel(ES_Runtime::PrivilegeLevel value)
{
    privilege_level = value;

    for (unsigned index = 0; index < functions_count; ++index)
        functions[index]->SetPrivilegeLevel(value);
}

void
ES_CodeStatic::FindInstructionOffsets(ES_Execution_Context *context)
{
    if (!instruction_offsets)
    {
        ES_CodeWord *iter;
        unsigned count = 1, index = 0;

        for (iter = codewords; ES_Analyzer::NextInstruction(this, iter); ++count)
            ;

        if (context)
        {
            if ((instruction_offsets = Suspended_NEWA<unsigned>(context, count + 1)) == NULL)
                context->AbortOutOfMemory();
        }
        else if ((instruction_offsets = OP_NEWA(unsigned, count + 1)) == NULL)
            LEAVE(OpStatus::ERR_NO_MEMORY);

        instruction_count = count;

        iter = codewords;
        do
            instruction_offsets[index++] = iter - codewords;
        while (ES_Analyzer::NextInstruction(this, iter));

        instruction_offsets[count] = codewords_count;
    }
}

void
ES_Code::SetParentCode(ES_Code *new_parent_code)
{
    data->SetParentCode(new_parent_code->data);

    global_object = new_parent_code->global_object;
    parent_code = new_parent_code;

#define COPY(what) what = new_parent_code->what

    COPY(strings);
    COPY(regexps);
    COPY(object_literal_classes);
    COPY(object_literal_prototype_classes);
    COPY(constant_array_literals);
    COPY(functions);
    COPY(property_get_caches);
    COPY(property_put_caches);
    COPY(global_caches);
    COPY(format_string_caches);
    COPY(eval_caches);

    scope_chain = new_parent_code->scope_chain;
    scope_chain_length = new_parent_code->scope_chain_length;

#undef COPY
}

void
ES_Code::SetIsExternal(BOOL value)
{
    data->SetIsExternal(value);
}

void
ES_Code::SetPrivilegeLevel(ES_Runtime::PrivilegeLevel value)
{
    data->SetPrivilegeLevel(value);
}

void
ES_Code::OptimizeGlobalAccessors(ES_Context *context)
{
    BOOL with_eval_type = type == TYPE_EVAL_PLAIN || type == TYPE_EVAL_ODD;
    if (type == TYPE_PROGRAM || with_eval_type)
    {
        ES_ProgramCode *program = static_cast<ES_ProgramCode *>(this);
        ES_ProgramCodeStatic *data = program->GetData();

        global_object->PrepareForOptimizeGlobalAccessors(context, data->variable_declarations_count);

        for (unsigned variable_declaration_index = 0; variable_declaration_index < data->variable_declarations_count; ++variable_declaration_index)
        {
            unsigned index = data->variable_declarations[variable_declaration_index];
            BOOL is_function = FALSE;
#ifdef ES_NATIVE_SUPPORT
            if ((0x80000000u  & index) != 0)
            {
                index &= 0x7fffffffu;
                is_function = TRUE;
            }
#endif

            JString *name = GetString(index);

            if (!global_object->HasOwnNativeProperty(name) && global_object->GetHostObject()->AllowOperationOnProperty(StorageZ(context, name), EcmaScript_Object::ALLOW_NATIVE_DECLARED_OVERRIDE))
                global_object->AddVariable(context, name, is_function, with_eval_type);
        }
    }

    for (unsigned cache_index = 0; cache_index < data->global_accesses_count; ++cache_index)
    {
        unsigned val = data->global_accesses[cache_index];
        JString *name = GetString((val & 0xc0000000u) | ((val & ~0xc0000000u) >> 3));
        unsigned global_index;
        if (global_object->GetVariableIndex(global_index, name) && (val & ES_CodeStatic::GetScope) == 0
#ifdef ES_NATIVE_SUPPORT
            && ((val & (ES_CodeStatic::PutGlobal | ES_CodeStatic::GetGlobal)) == ES_CodeStatic::GetGlobal || !global_object->GetVariableIsFunction(global_index))
#endif // ES_NATIVE_SUPPORT
            )
        {
            global_caches[cache_index].class_id = ES_Class::GLOBAL_IMMEDIATE_CLASS_ID;
            global_caches[cache_index].cached_index = global_index;
        }
    }

    for (unsigned function_index = 0; function_index < data->functions_count; ++function_index)
        functions[function_index]->OptimizeGlobalAccessors(context);
}

BOOL
ES_CodeStatic::FindExceptionHandler(unsigned current_ip, unsigned &handler_ip, ES_CodeStatic::ExceptionHandler::Type &handler_type)
{
    BOOL found = FALSE;

    ExceptionHandler *handlers = exception_handlers;
    unsigned count = exception_handlers_count;

    while (handlers)
    {
        int low = 0, high = count - 1;

        while (high >= low)
        {
            unsigned index = (low + high) / 2;

            if (handlers[index].start < current_ip && current_ip <= handlers[index].end)
            {
                found = TRUE;
                handler_ip = handlers[index].handler_ip;
                handler_type = handlers[index].type;

                count = handlers[index].nested_handlers_count;
                handlers = handlers[index].nested_handlers;

                break;
            }
            else if (current_ip <= handlers[index].start)
                high = index - 1;
            else
                low = index + 1;
        }

        if (high < low)
            break;
    }

    return found;
}

BOOL
ES_CodeStatic::FindFinallyHandler(unsigned current_ip, unsigned target, unsigned &handler_ip)
{
    BOOL found = FALSE;

    ExceptionHandler *handlers = exception_handlers;
    unsigned count = exception_handlers_count;

    while (handlers)
    {
        int low = 0, high = count - 1;

        while (high >= low)
        {
            unsigned index = (low + high) / 2;

            if (handlers[index].start < current_ip &&
                current_ip <= handlers[index].end &&
                (target > handlers[index].end || target < handlers[index].start) &&
                handlers[index].type == ExceptionHandler::TYPE_FINALLY)
            {
                found = TRUE;
                handler_ip = handlers[index].handler_ip;

                count = handlers[index].nested_handlers_count;
                handlers = handlers[index].nested_handlers;

                break;
            }
            else if (current_ip <= handlers[index].start)
                high = index - 1;
            else
                low = index + 1;
        }

        if (high < low)
            break;
    }

    return found;
}

ES_Class *
ES_Code::GetObjectLiteralClass(ES_Context *context, unsigned index, ES_CodeWord *values, ES_Value_Internal *reg)
{
    ES_Class *klass = object_literal_classes[index];
    if (klass)
        return klass;
    else
    {
        BOOL unique;
        ES_CodeStatic::ObjectLiteralClass &olc = data->object_literal_classes[index];

        if (type == TYPE_FUNCTION && data->object_literal_classes_count < ES_Code_ClassTable::MAX_OBJECT_LITERAL_COMPACT_CLASSES)
        {
            klass = ES_Class::MakeCompactRoot(context, global_object->GetObjectPrototype(), "Object", context->rt_data->idents[ESID_Object], TRUE, olc.properties_count);
            unique = TRUE;
        }
        else
        {
            klass = global_object->GetObjectClass();
            unique = FALSE;
        }

        ES_CollectorLock gclock(context);

        for (unsigned property_index = 0; property_index < olc.properties_count; ++property_index)
        {
            ES_Property_Info info;
            ES_StorageType type = values ? reg[values[property_index].index].GetStorageType() : ES_STORAGE_WHATEVER;

            unsigned string_index = olc.properties[property_index];

            if ((string_index & 0x80000000u) != 0)
            {
                info.SetIsFunction();
                string_index &= 0x7fffffffu;
            }

            klass = ES_Class::ExtendWithL(context, klass, GetString(string_index), info, type, ES_LayoutIndex(property_index));
        }

        if (unique)
        {
            if (!object_literal_prototype_classes)
            {
                if (ES_Execution_Context *exec_context = context->GetExecutionContext())
                {
                    ES_Suspended_NEW<ES_Code_ClassTable> allocate;
                    exec_context->SuspendedCall(&allocate);
                    object_literal_prototype_classes = allocate.storage;
                }
                else
                    object_literal_prototype_classes = OP_NEW(ES_Code_ClassTable, ());

                if (!object_literal_prototype_classes)
                    context->AbortOutOfMemory();

                context->heap->AddClassTable(object_literal_prototype_classes);
            }

            for (unsigned i = 0; i < ES_Code_ClassTable::MAX_OBJECT_LITERAL_COMPACT_CLASSES; i++)
                if (object_literal_prototype_classes->table[i] == NULL)
                {
                    object_literal_prototype_classes->table[i] = klass;
                    break;
                }
        }

        if (olc.properties_count > 0)
            klass = ES_Class::Branch(context, klass, ES_LayoutIndex(klass->GetPropertyInfoAtIndex(ES_PropertyIndex(olc.properties_count - 1)).Index() + 1));

        return object_literal_classes[index] = klass;
    }
}

void
ES_Code::SetObjectLiteralClass(ES_Context *context, unsigned index, ES_Class *klass)
{
    ES_CodeStatic::ObjectLiteralClass &olc = data->object_literal_classes[index];
    object_literal_classes[index] = klass;

    if (olc.properties_count > 0)
        object_literal_classes[index] = ES_Class::Branch(context, klass, ES_LayoutIndex(klass->GetPropertyInfoAtIndex(ES_PropertyIndex(olc.properties_count - 1)).Index() + 1));
}

unsigned
ES_Code::GetObjectLiteralClassCount(unsigned index)
{
    return data->object_literal_classes[index].properties_count;
}

OP_STATUS
ES_Code::SetScopeChain(ES_Object **objects, unsigned length)
{
    scope_chain = OP_NEWA(ES_Object *, length);
    if (!scope_chain)
        return OpStatus::ERR_NO_MEMORY;
    op_memcpy(scope_chain, objects, length * sizeof scope_chain[0]);
    scope_chain_length = length;
    for (unsigned i = 0; i < data->functions_count; i++)
        functions[i]->SetScopeChain(objects, length);
    return OpStatus::OK;
}

ES_Code_ClassTable::ES_Code_ClassTable()
    : next(NULL)
{
    op_memset(table, 0, MAX_OBJECT_LITERAL_COMPACT_CLASSES * sizeof(ES_Class *));
}

static unsigned
CompressedDelta(unsigned previous, unsigned next)
{
    if (previous <= next)
        return (next - previous) << 1;
    else
        return ((previous - next) << 1) | 1;
}

static unsigned
CompressedLength(unsigned value)
{
    if (value < (1 << 7))
        return 1;
    else if (value < (1 << 14))
        return 2;
    else if (value < (1 << 21))
        return 3;
    else if (value < (1 << 28))
        return 4;
    else
        return 5;
}

static unsigned char *
CompressedWrite(unsigned char *write, unsigned value)
{
    if (value & (((1 << 4) - 1) << 28))
        *write++ = static_cast<unsigned char>(0x80 | (value >> 28) & 0x7f);
    if (value & (((1 << 7) - 1) << 21))
        *write++ = static_cast<unsigned char>(0x80 | (value >> 21) & 0x7f);
    if (value & (((1 << 14) - 1) << 14))
        *write++ = static_cast<unsigned char>(0x80 | (value >> 14) & 0x7f);
    if (value & (((1 << 21) - 1) << 7))
        *write++ = static_cast<unsigned char>(0x80 | (value >> 7) & 0x7f);
    *write++ = static_cast<unsigned char>(value & 0x7f);

    return write;
}

static unsigned
CompressedRead(const unsigned char *&read)
{
    unsigned value = 0;
    while (*read & 0x80)
        value = (value << 7) | *read++ & 0x7f;
    return (value << 7) | *read++;
}

static unsigned
DecompressDelta(unsigned previous, unsigned value)
{
    if (value & 1)
        return previous - (value >> 1);
    else
        return previous + (value >> 1);
}

/* static */ const unsigned char *
ES_CodeStatic::DebugRecord::Compress(DebugRecord *records, unsigned count)
{
    OP_ASSERT(count > 0);
    unsigned length = 0;

    unsigned previous_cw_index = 0;
    unsigned previous_index = 0;
    unsigned previous_line = 0;

    ES_CodeStatic::DebugRecord *iter = records, *stop = iter + count;

    while (iter != stop)
    {
        unsigned cw_index = (iter->codeword_index << 1) | iter->type;
        unsigned index = iter->location.Index();
        unsigned line = iter->location.Line();

        length += (CompressedLength(CompressedDelta(previous_cw_index, cw_index)) +
                   CompressedLength(CompressedDelta(previous_index, index)) +
                   CompressedLength(CompressedDelta(previous_line, line)) +
                   CompressedLength(iter->location.Length()));

        previous_cw_index = cw_index;
        previous_index = index;
        previous_line = line;

        ++iter;
    }

    unsigned char *compressed = OP_NEWA_L(unsigned char, length), *write = compressed;

    previous_cw_index = 0;
    previous_index = 0;
    previous_line = 0;

    iter = records;
    stop = iter + count;

    while (iter != stop)
    {
        unsigned cw_index = (iter->codeword_index << 1) | iter->type;
        unsigned index = iter->location.Index();
        unsigned line = iter->location.Line();

        write = CompressedWrite(write, CompressedDelta(previous_cw_index, cw_index));
        write = CompressedWrite(write, CompressedDelta(previous_index, index));
        write = CompressedWrite(write, CompressedDelta(previous_line, line));
        write = CompressedWrite(write, iter->location.Length());

        previous_cw_index = cw_index;
        previous_index = index;
        previous_line = line;

        ++iter;
    }

    OP_ASSERT(write == compressed + length);

#if defined _DEBUG && defined DEBUG_ENABLE_OPASSERT
    ANCHOR_ARRAY(unsigned char, compressed);
    ES_CodeStatic::DebugRecord *reverse = Decompress(compressed, count);
    OP_ASSERT(op_memcmp(reverse, records, sizeof *records * count) == 0);
    OP_DELETEA(reverse);
    ANCHOR_ARRAY_RELEASE(compressed);
#endif // _DEBUG && DEBUG_ENABLE_OPASSERT

    return compressed;
}

/* static */ ES_CodeStatic::DebugRecord *
ES_CodeStatic::DebugRecord::Decompress(const unsigned char *data, unsigned count, ES_Execution_Context *context)
{
    OP_ASSERT(count > 0);
    DebugRecord *records;

    if (context)
    {
        ES_Suspended_NEWA<ES_CodeStatic::DebugRecord> allocate(count);
        context->SuspendedCall(&allocate);
        if (!allocate.storage)
            context->AbortOutOfMemory();
        records = allocate.storage;
    }
    else
        records = OP_NEWA_L(ES_CodeStatic::DebugRecord, count);

    ES_CodeStatic::DebugRecord *write = records, *stop = write + count;
    const unsigned char *read = data;

    unsigned previous_cw_index = 0;
    unsigned previous_index = 0;
    unsigned previous_line = 0;

    while (write != stop)
    {
        unsigned cw_index = DecompressDelta(previous_cw_index, CompressedRead(read));

        write->codeword_index = cw_index >> 1;
        write->type = static_cast<ES_CodeStatic::DebugRecord::Type>(cw_index & 1);

        unsigned index = DecompressDelta(previous_index, CompressedRead(read));
        unsigned line = DecompressDelta(previous_line, CompressedRead(read));
        unsigned length = CompressedRead(read);

        write->location.Set(index, line, length);

        previous_cw_index = cw_index;
        previous_index = index;
        previous_line = line;

        ++write;
    }

    return records;
}

ES_CodeStatic::DebugRecord *
ES_CodeStatic::FindDebugRecord(ES_Execution_Context *context, DebugRecord::Type type, ES_CodeWord *codeword)
{
    if (compressed_debug_records && !debug_records)
    {
        debug_records = DebugRecord::Decompress(compressed_debug_records, debug_records_count, context);

        ES_Suspended_DELETEA<unsigned char> delete_compressed_records(const_cast<unsigned char*>(compressed_debug_records));
        context->SuspendedCall(&delete_compressed_records);
        compressed_debug_records = NULL;
    }

    if (debug_records)
    {
        unsigned codeword_index = codeword - codewords;
        unsigned stop = debug_records_count - 1;
        unsigned index = 0;
        int low, high;

        if (codeword_index < debug_records[0].codeword_index)
            return NULL;
        else if (codeword_index >= debug_records[stop].codeword_index)
        {
            index = stop;
            goto found;
        }
        else
        {
            low = 0;
            high = stop;

            while (high >= low)
            {
                index = (low + high) / 2;

                if (debug_records[index].codeword_index <= codeword_index && (index == stop || debug_records[index + 1].codeword_index > codeword_index))
                {
                found:
                    if (type == DebugRecord::TYPE_EXTENT_INFORMATION)
                    {
                        while (type != static_cast<DebugRecord::Type>(debug_records[index].type))
                            if (index-- == 0)
                                return NULL;

                        return &debug_records[index];
                    }
                    else
                    {
                        while (index < stop && debug_records[index + 1].codeword_index <= codeword_index)
                            ++index;

                        do
                            if (type == static_cast<DebugRecord::Type>(debug_records[index].type))
                                return &debug_records[index];
                        while (index != 0 && debug_records[--index].codeword_index <= codeword_index);
                    }

                    return NULL;
                }
                else if (codeword_index < debug_records[index].codeword_index)
                    high = index - 1;
                else
                    low = index + 1;
            }
        }
    }

    return NULL;
}

/* virtual */
ES_ProgramCodeStatic::~ES_ProgramCodeStatic()
{
    OP_DELETEA(variable_declarations);

    if (program_cache)
        program_cache->RemoveProgram(this);
}

static ES_FunctionCode *
AllocateFunctionCode(ES_Context *context, ES_FunctionCodeStatic *data, ES_Global_Object *global_object, ES_ProgramCodeStaticReaper *program_reaper)
{
    ES_FunctionCode *function_code;
    GC_ALLOCATE(context, function_code, ES_FunctionCode, (function_code, data, global_object, program_reaper));
    return function_code;
}

/* static */ void
ES_Code::InitializeFromStatic(ES_Context *context, ES_Global_Object *global_object, ES_Code *code, ES_Identifier_List *strings_table, ES_ProgramCodeStaticReaper *program_reaper, JString *url_str)
{
    JStringStorage *source_storage = code->data->source_storage;
    JStringStorage *string_storage = code->data->string_storage;
    ES_CodeStatic::String *strings = code->data->strings;
    unsigned index, source_storage_length = source_storage ? source_storage->length : 0;

    code->url = url_str;

    code->strings = OP_NEWA_L(JString *, code->data->strings_count);
    for (index = 0; index < code->data->strings_count; ++index)
    {
        JStringStorage *storage;
        unsigned offset;

        if (strings[index].offset < source_storage_length)
            storage = source_storage, offset = strings[index].offset;
        else if ((strings[index].offset & 0x80000000u) != 0)
        {
            code->strings[index] = ES_Lexer::ProcessStringLiteral(context, source_storage->storage + (strings[index].offset & 0x7fffffffu), strings[index].length, code->data->is_strict_mode);
            continue;
        }
        else
            storage = string_storage, offset = strings[index].offset - source_storage_length;

        JString *string;
        JTemporaryString temporary(storage->storage + offset, strings[index].length);
        unsigned string_index;

        if (strings_table->IndexOf(temporary, string_index))
            strings_table->Lookup(string_index, string);
        else
        {
            string = temporary.Allocate(context, storage);
            string->hash = static_cast<JString *>(temporary)->hash;
            strings_table->AppendAtIndexL(context, string, string_index, string_index);
        }

        code->strings[index] = string;
    }

    if (code->data->regexps_count)
    {
        code->regexps = OP_NEWA_L(ES_RegExp_Object *, code->data->regexps_count);
        op_memset(code->regexps, 0, code->data->regexps_count * sizeof(ES_RegExp_Object *));
    }

    if (code->data->object_literal_classes_count)
    {
        code->object_literal_classes = OP_NEWA_L(ES_Class *, code->data->object_literal_classes_count);
        op_memset(code->object_literal_classes, 0, code->data->object_literal_classes_count * sizeof(ES_Class *));
    }

    if (code->data->constant_array_literals_count)
    {
        code->constant_array_literals = OP_NEWA_L(ES_Compact_Indexed_Properties *, code->data->constant_array_literals_count);
        op_memset(code->constant_array_literals, 0, code->data->constant_array_literals_count * sizeof(ES_Compact_Indexed_Properties *));
    }

    code->functions = OP_NEWA_L(ES_FunctionCode *, code->data->functions_count);
    for (index = 0; index < code->data->functions_count; ++index)
    {
        ES_FunctionCode *function_code = AllocateFunctionCode(context, code->data->functions[index], global_object, program_reaper);
        InitializeFromStatic(context, global_object, code->functions[index] = function_code, strings_table, program_reaper, url_str);
    }

    code->property_get_caches = OP_NEWA_L(ES_Code::PropertyCache, code->data->property_get_caches_count);
    code->property_put_caches = OP_NEWA_L(ES_Code::PropertyCache, code->data->property_put_caches_count);
    code->global_caches = OP_NEWA_L(ES_Code::GlobalCache, code->data->global_accesses_count);

#ifdef ES_PROPERTY_CACHE_PROFILING
    context->rt_data->pcache_allocated_property += 8 + sizeof(ES_Code::PropertyCache) * (code->data->property_get_caches_count + code->data->property_put_caches_count);
    context->rt_data->pcache_allocated_global += 8 + sizeof(ES_Code::GlobalCache) * code->data->global_accesses_count;
#endif // ES_PROPERTY_CACHE_PROFILING

#ifdef ES_NATIVE_SUPPORT
    code->loop_counters = OP_NEWA_L(unsigned, code->data->loop_data_count);
    op_memset(code->loop_counters, 0, code->data->loop_data_count * sizeof(unsigned));

    code->loop_dispatcher_codes = OP_NEWA_L(ES_Code *, code->data->loop_data_count);
    op_memset(code->loop_dispatcher_codes, 0, code->data->loop_data_count * sizeof(ES_Code *));
#endif // ES_NATIVE_SUPPORT

    code->format_string_caches = OP_NEWA_L(ES_Code::FormatStringCache, code->data->format_string_caches_count);
    code->eval_caches = OP_NEWA_L(ES_Code::EvalCache, code->data->eval_caches_count);

#ifdef ES_NATIVE_SUPPORT
    code->switch_tables = OP_NEWA_L(ES_Code::SwitchTable, code->data->switch_tables_count);
#endif // ES_NATIVE_SUPPORT
}

/* static */ ES_ProgramCode *
ES_ProgramCode::Make(ES_Context *context, ES_Global_Object *global_object, ES_ProgramCodeStatic *data, BOOL initialize_from_static, JString *url_str)
{
    OpStackAutoPtr<ES_ProgramCodeStatic> data_anchor;
    if (!data->IsShared())
        data_anchor.reset(data);

    ES_ProgramCode *program;
    GC_ALLOCATE(context, program, ES_ProgramCode, (program, data, global_object));

    data_anchor.release();

    if (initialize_from_static)
    {
        if (data->string_storage_owner && !context->heap->AddStaticStringData(context, data->string_storage_owner))
            LEAVE(OpStatus::ERR_NO_MEMORY);

        if (data->source_storage_owner && !context->heap->AddStaticStringData(context, data->source_storage_owner))
            LEAVE(OpStatus::ERR_NO_MEMORY);

        ES_Identifier_List *strings_table = ES_Identifier_List::Make(context, 256);

        ES_ProgramCodeStaticReaper *program_reaper = ES_ProgramCodeStaticReaper::IncRef(OP_NEW_L(ES_ProgramCodeStaticReaper, ()));
        program_reaper->Initialize(data);

        TRAPD(status, InitializeFromStatic(context, global_object, program, strings_table, program_reaper, url_str));

        ES_ProgramCodeStaticReaper::DecRef(program_reaper);

        if (OpStatus::IsMemoryError(status))
            LEAVE(status);
    }

    return program;
}

static unsigned
CountStrings(ES_Code *code)
{
    unsigned count = code->data->strings_count;

    for (unsigned index = 0; index < code->data->functions_count; ++index)
        count += CountStrings(code->functions[index]);

    return count;
}

/* static */ void
ES_Code::CalculateStringOffsets(ES_Context *context, ES_Code *code, ES_Identifier_Hash_Table *long_string_literal_table, ES_Identifier_List *strings_table, unsigned *offsets, unsigned &current_offset)
{
    ES_CodeStatic::String *strings = code->data->strings = OP_NEWA_L(ES_CodeStatic::String, code->data->strings_count);
    unsigned index;

    for (index = 0; index < code->data->strings_count; ++index)
    {
        JString *string = code->strings[index];

        if (string->value == code->data->source_storage)
            strings->offset = string->offset;
        else
        {
            if (long_string_literal_table && Length(string) > 1024)
            {
                unsigned string_literal_index;

                if (long_string_literal_table->Find(string, string_literal_index))
                {
                    OP_ASSERT(code->data->source_storage && (code->data->source_storage->storage[string_literal_index] == '\'' || code->data->source_storage->storage[string_literal_index] == '"'));
                    strings->offset = string_literal_index | 0x80000000u;
                    goto next_string;
                }
            }

            unsigned position;
            if (strings_table->IndexOf(string, position))
                strings->offset = offsets[position];
            else
            {
                strings_table->AppendAtIndexL(context, string, position, position);
                strings->offset = offsets[position] = current_offset;
                current_offset += Length(string) + 1;
            }
        }

    next_string:
        strings++->length = Length(string);
    }

    for (index = 0; index < code->data->functions_count; ++index)
        CalculateStringOffsets(context, code->functions[index], long_string_literal_table, strings_table, offsets, current_offset);
}


/* static */ unsigned
ES_Code::CopyStrings(ES_Context *context, ES_Code *code, ES_StaticStringData *data, unsigned write_offset)
{
    code->data->string_storage_owner = ES_StaticStringData::IncRef(data);
    code->data->string_storage = data->storage;

    uni_char *storage = data->storage->storage;

    ES_CodeStatic::String *strings = code->data->strings;
    unsigned index, write_offset_adjust = code->data->source_storage ? code->data->source_storage->length : 0;

    for (index = 0; index < code->data->strings_count; ++index)
    {
        JString *string = code->strings[index];

        if (string->value != code->data->source_storage && (strings[index].offset & 0x80000000u) == 0)
        {
            OP_ASSERT(Length(string) == strings[index].length);
            OP_ASSERT(strings[index].offset <= write_offset);

            if (strings[index].offset == write_offset)
            {
                unsigned length = Length(string), local_write_offset = write_offset - write_offset_adjust;
                op_memmove(storage + local_write_offset, Storage(context, string), length * sizeof(uni_char));
                storage[local_write_offset + length] = 0;
                write_offset += length + 1;
            }
            else
            {
                OP_ASSERT(op_memcmp(storage + strings[index].offset - write_offset_adjust, Storage(context, string), Length(string) * sizeof(uni_char)) == 0);
                OP_ASSERT(storage[strings[index].offset - write_offset_adjust + strings[index].length] == 0);
            }
        }
    }

    for (index = 0; index < code->data->functions_count; ++index)
        write_offset = CopyStrings(context, code->functions[index], data, write_offset);

    return write_offset;
}

void
ES_ProgramCode::PrepareStaticForSharing(ES_Context *context)
{
    if (data->prepared_for_sharing)
        return;

    OP_ASSERT(!data->string_storage);
    OP_ASSERT(!data->strings);

    unsigned count = CountStrings(this);

    if (count != 0)
    {
        unsigned *offsets = OP_NEWA_L(unsigned, count);
        ANCHOR_ARRAY(unsigned, offsets);

        ES_Identifier_List *strings_table = ES_Identifier_List::Make(context, 2 * count);

        unsigned current_offset = 0;

        if (data->source_storage)
            current_offset += data->source_storage->length;

        CalculateStringOffsets(context, this, long_string_literal_table, strings_table, offsets, current_offset);

        if (data->source_storage)
            current_offset -= data->source_storage->length;

        ES_StaticStringData *string_data = OP_NEW_L(ES_StaticStringData, ());

        TRAPD(status, string_data->storage = JStringStorage::MakeStatic(context, current_offset));

        if (OpStatus::IsSuccess(status))
        {
            unsigned write_offset = 0;

            if (data->source_storage)
                write_offset += data->source_storage->length;

            CopyStrings(context, this, string_data, write_offset);
        }

        ES_StaticStringData::DecRef(string_data);

        context->heap->Free(strings_table);

        LEAVE_IF_ERROR(status);
    }

    data->prepared_for_sharing = TRUE;
}

void
ES_ProgramCode::StartExecution(ES_Runtime *runtime, ES_Context *context)
{
    PrepareForExecution(context);
}

/* virtual */
ES_FunctionCodeStatic::~ES_FunctionCodeStatic()
{
    OP_DELETEA(formals_and_locals);
}

static ES_Class *
PruneConstructorPropertyCache(ES_Execution_Context *context, ES_FunctionCode *code, ES_CodeWord *word, ES_Class *klass, BOOL first)
{
    ES_Code::PropertyCache *cache = &code->property_put_caches[word[4].index];
    ES_Class *next_klass = NULL;
    unsigned class_id = klass->Id();

    if (first && (cache->next != NULL || cache->data.new_class == NULL))
        return NULL;

    while (cache != NULL)
    {
        if (cache->class_id == class_id)
        {
            if (next_klass != NULL)
                return NULL;
            else
                next_klass = cache->data.new_class;
            cache = cache->next;
        }
        else
        {
            BOOL is_last = cache->next == NULL;
            code->RemoveCacheRecord(context, FALSE, word[4].index, cache);
            if (is_last)
                break;
        }
    }

    return next_klass;
}

ES_Class *
ES_FunctionCode::OptimizeConstructorPropertyCaches(ES_Execution_Context *context, ES_Class *root_class, unsigned &property_count)
{
    ES_Class *final_class = root_class;

    OP_ASSERT(context->GetExecutionContext() != NULL);

    property_count = 0;
    for (unsigned index = 0, count = data->instruction_count; final_class && index < count; ++index)
    {
        unsigned cw_index = data->instruction_offsets[index];
        ES_CodeWord *word = &data->codewords[cw_index];

        switch (word->instruction)
        {
        case ESI_PUTN_IMM:
        case ESI_INIT_PROPERTY:
            if (word[1].index == 0)
                final_class = PruneConstructorPropertyCache(context->GetExecutionContext(), this, word, final_class, property_count++ == 0);
            break;

        case ESI_JUMP:
        case ESI_JUMP_IF_TRUE:
        case ESI_JUMP_IF_FALSE:
        case ESI_TABLE_SWITCH:
        case ESI_CONSTRUCT:
            final_class = NULL;
            break;

#ifdef ES_COMBINED_ADD_SUPPORT
        case ESI_ADDN:
            {
                unsigned index = cw_index + 2, stop = cw_index + 2 + word[2].index;

                while (index != stop)
                    if (word[index++].index == 0)
                    {
                        final_class = NULL;
                        break;
                    }
            }
            break;
#endif // ES_COMBINED_ADD_SUPPORT

        case ESI_CONSTRUCT_OBJECT:
            {
                unsigned index = 3, stop = 3 + data->object_literal_classes[word[2].index].properties_count;

                while (index != stop)
                    if (word[index++].index == 0)
                    {
                        final_class = NULL;
                        break;
                    }
            }
            break;

        default:
            unsigned operand_count = g_instruction_operand_count[word->instruction];
            unsigned short register_i = g_instruction_operand_register_io[word->instruction] >> 8;

            for (unsigned index = 0; index < operand_count; ++index)
                if ((register_i & (1 << index)) != 0)
                    if (word[1 + index].index == 0)
                    {
                        final_class = NULL;
                        break;
                    }
        }
    }

    if (final_class && final_class != root_class)
        final_class = ES_Class::Branch(context, final_class, ES_LayoutIndex(final_class->GetPropertyInfoAtIndex(ES_PropertyIndex(property_count - 1)).Index() + 1));

    return final_class;
}

void
ES_FunctionCode::ConstructClass(ES_Context *context)
{
    if (!klass)
    {
        ES_FunctionCodeStatic *data = GetData();

        klass = ES_Class::MakeCompactRoot(context, NULL, "Variable", context->rt_data->idents[ESID_Variable]);
        klass->SetIsCompact();

        ES_Property_Info info(DD);

        unsigned index, locals_count = data->LocalsCount();

        if (data->formals_count + locals_count != 0)
        {
            unsigned count = 0;
            for (index = 0; index < data->formals_count; ++index)
                klass = klass->ExtendWithL(context, GetString(data->formals_and_locals[index]), info, ES_STORAGE_WHATEVER, ES_LayoutIndex(count++), TRUE);

            for (index = 0; index < locals_count; ++index)
                klass = klass->ExtendWithL(context, GetString(data->formals_and_locals[data->formals_count + index]), info, ES_STORAGE_WHATEVER, ES_LayoutIndex(count++), FALSE);

            klass = klass->Branch(context, ES_LayoutIndex(count));
        }
    }
}

BOOL
ES_FunctionCodeStatic::CanHaveVariableObject()
{
    return uses_get_scope_ref || uses_eval || functions_count != 0 && functions_count != static_functions_count;
}

BOOL
ES_FunctionCodeStatic::CanInline()
{
#define INLINE_CODEWORDS_LIMIT 64

    return !exception_handlers && !CanHaveVariableObject() && codewords_count < INLINE_CODEWORDS_LIMIT && !uses_arguments;
}

ES_ProgramCodeStaticReaper::ES_ProgramCodeStaticReaper()
    : ref_count(0),
      data(NULL),
      functions_count(0),
      deleted_functions(0)
{
}

ES_ProgramCodeStaticReaper::~ES_ProgramCodeStaticReaper()
{
    if (data)
        ES_ProgramCodeStatic::DecRef(data);
}

void
ES_ProgramCodeStaticReaper::Initialize(ES_ProgramCodeStatic *data)
{
    OP_ASSERT(this->data == NULL);

    this->data = data;

    if (data)
        ES_ProgramCodeStatic::IncRef(data);
}

void
ES_ProgramCodeStaticReaper::FunctionCreatedInternal()
{
    functions_count++;
}

void
ES_ProgramCodeStaticReaper::FunctionDeletedInternal()
{
    deleted_functions++;

    if (data)
        if (functions_count > 16 && deleted_functions * 4 > functions_count || deleted_functions == functions_count)
        {
            ES_ProgramCodeStatic::DecRef(data);
            data = NULL;
        }
}

/* static */ void
ES_ProgramCodeStaticReaper::FunctionCreated(ES_ProgramCodeStaticReaper *reaper)
{
    if (reaper)
        reaper->FunctionCreatedInternal();
}

/* static */ void
ES_ProgramCodeStaticReaper::FunctionDeleted(ES_ProgramCodeStaticReaper *reaper)
{
    if (reaper)
        reaper->FunctionDeletedInternal();
}

/* static */ ES_ProgramCodeStaticReaper *
ES_ProgramCodeStaticReaper::IncRef(ES_ProgramCodeStaticReaper *reaper)
{
    if (reaper)
        ++reaper->ref_count;

    return reaper;
}

/* static */ void
ES_ProgramCodeStaticReaper::DecRef(ES_ProgramCodeStaticReaper *reaper)
{
    if (reaper)
        if (--reaper->ref_count == 0)
            OP_DELETE(reaper);
}

/* virtual */ void
ES_Program::GCTrace()
{
    if (program)
        heap->Mark(program);
}

/* virtual */ void
ES_Program::NewHeap(ES_Heap *heap)
{
    this->heap = heap;
}

ES_CodeOptimizationData::~ES_CodeOptimizationData()
{
    if (register_accesses)
    {
        for (unsigned index = 0; index < code->register_frame_size; ++index)
            OP_DELETEA(register_accesses[index]);

        OP_DELETEA(register_accesses);
    }
    OP_DELETEA(register_access_counts);

    JumpTarget *jt = first_jump_target;

    while (jt)
    {
        JumpTarget *next = jt->next;
        OP_DELETE(jt);
        jt = next;
    }
}

BOOL
ES_CodeOptimizationData::RegisterAccess::GetValue(ES_Code *code, ES_Value_Internal &value) const
{
    if (IsValueKnown())
    {
        ES_CodeWord *ip = &code->data->codewords[data >> SHIFT_WRITE_CW_INDEX];

        switch (ip->instruction)
        {
        case ESI_LOAD_INT32:
            value.SetInt32(ip[2].immediate);
            return TRUE;

        case ESI_LOAD_DOUBLE:
            value.SetDouble(code->data->doubles[ip[2].index]);
            return TRUE;

        case ESI_LOAD_STRING:
            value.SetString(code->GetString(ip[2].index));
            return TRUE;

        case ESI_LOAD_NULL:
            value.SetNull();
            return TRUE;

        case ESI_LOAD_UNDEFINED:
            value.SetUndefined();
            return TRUE;

        case ESI_LOAD_TRUE:
            value.SetTrue();
            return TRUE;

        case ESI_LOAD_FALSE:
            value.SetFalse();
            return TRUE;

        case ESI_LOAD_GLOBAL_OBJECT:
            value.SetObject(code->global_object);
            return TRUE;
        }

        OP_ASSERT(FALSE);
    }

    return FALSE;
}

unsigned
ES_CodeOptimizationData::RegisterAccess::GetLastReadIndex(BOOL ignore_implicit_writes) const
{
    OP_ASSERT(IsWrite());

    const RegisterAccess *access = GetNextRead(ignore_implicit_writes), *next;

    if (access)
        while (TRUE)
            if ((next = access->GetNextRead(ignore_implicit_writes)) != NULL)
                access = next;
            else
                return access->cw_index;
    else
        return UINT_MAX;
}

unsigned
ES_CodeOptimizationData::RegisterAccess::GetNumberOfReads(BOOL ignore_implicit_writes) const
{
    OP_ASSERT(IsWrite());

    const RegisterAccess *access = GetNextRead(ignore_implicit_writes);
    unsigned count = 0;

    while (access)
    {
        ++count;
        access = access->GetNextRead(ignore_implicit_writes);
    }

    return count;
}

const ES_CodeOptimizationData::RegisterAccess *
ES_CodeOptimizationData::RegisterAccess::GetNextRead(BOOL ignore_implicit_writes) const
{
    const RegisterAccess *access = this + 1;

    while (!access->IsTerminator())
        if (access->IsRead())
            return access;
        else if (ignore_implicit_writes && access->IsImplicit())
            ++access;
        else
            break;

    return NULL;
}

const ES_CodeOptimizationData::RegisterAccess *
ES_CodeOptimizationData::RegisterAccess::GetNextWrite() const
{
    const RegisterAccess *access = this + 1;

    while (access->IsRead())
        ++access;

    return access;
}

BOOL
ES_CodeOptimizationData::RegisterAccess::IsWrittenValueRead() const
{
    OP_ASSERT(IsWrite());
    return GetNextRead(TRUE) != NULL;
}

static unsigned
FindRegisterAccessIndex(unsigned cw_index, const ES_CodeOptimizationData::RegisterAccess *accesses, unsigned count)
{
    int low = 0, high = count - 1;

    OP_ASSERT(count != 0 && cw_index <= accesses[high].cw_index);

    while (TRUE)
    {
        unsigned index = (low + high) / 2;

        if (cw_index <= accesses[index].cw_index)
        {
            if (index == 0 || cw_index > accesses[index - 1].cw_index)
            {
                while (index + 1 < count && cw_index == accesses[index + 1].cw_index)
                    ++index;

                return index;
            }

            high = index - 1;
        }
        else
            low = index + 1;
    }
}

const ES_CodeOptimizationData::RegisterAccess *
ES_CodeOptimizationData::FindWriteAt(unsigned cw_index, unsigned register_index)
{
    const ES_CodeOptimizationData::RegisterAccess *accesses = register_accesses[register_index];
    unsigned count = register_access_counts[register_index];
    unsigned index = FindRegisterAccessIndex(cw_index, accesses, count);

    OP_ASSERT(accesses[index].cw_index == cw_index);

    if (accesses[index].IsImplicitRead())
        --index;

    OP_ASSERT(accesses[index].cw_index == cw_index && accesses[index].IsWrite());

    return &accesses[index];
}

const ES_CodeOptimizationData::RegisterAccess *
ES_CodeOptimizationData::FindCorrespondingImplicitWrite(unsigned register_index, const RegisterAccess *implicit_read)
{
    ES_CodeWord *word = &code->codewords[implicit_read->cw_index];

    switch (word->instruction)
    {
    default:
        return NULL;

    case ESI_JUMP:
    case ESI_JUMP_IF_TRUE:
    case ESI_JUMP_IF_FALSE:
        const ES_CodeOptimizationData::RegisterAccess *accesses = register_accesses[register_index];
        unsigned count = register_access_counts[register_index];
        unsigned cw_index = implicit_read->cw_index + 1 + word[1].offset;
        unsigned index = FindRegisterAccessIndex(cw_index, accesses, count);

        OP_ASSERT(accesses[index].cw_index == cw_index);

        if (accesses[index].IsImplicitRead())
            --index;
        if (accesses[index].IsExplicitWrite())
            --index;
        if (accesses[index].IsExplicitRead())
            --index;

        OP_ASSERT(accesses[index].cw_index == cw_index && accesses[index].IsImplicitWrite());

        return &accesses[index];
    }
}

const ES_CodeOptimizationData::RegisterAccess *
ES_CodeOptimizationData::FindReadAt(unsigned cw_index, unsigned register_index)
{
    const ES_CodeOptimizationData::RegisterAccess *accesses = register_accesses[register_index];
    unsigned count = register_access_counts[register_index];
    unsigned index = FindRegisterAccessIndex(cw_index, accesses, count);

    OP_ASSERT(accesses[index].cw_index == cw_index);

    if (accesses[index].IsImplicitRead())
        --index;

    if (accesses[index].IsExplicitWrite())
        --index;

    OP_ASSERT(accesses[index].cw_index == cw_index && accesses[index].IsExplicitRead());

    return &accesses[index];
}

const ES_CodeOptimizationData::RegisterAccess *
ES_CodeOptimizationData::FindWriteReadAt(unsigned cw_index, unsigned register_index)
{
    const ES_CodeOptimizationData::RegisterAccess *accesses = register_accesses[register_index];
    unsigned count = register_access_counts[register_index];
    unsigned index = FindRegisterAccessIndex(cw_index, accesses, count);

    /* We don't care about things happening after 'cw_index'. */
    while (index != 0 && cw_index < accesses[index].cw_index)
        --index;

    /* We also don't care about things happening at 'cw_index' unless it happens
       to be an implicit write. */
    while (index != 0 && cw_index == accesses[index].cw_index && !accesses[index].IsImplicitWrite())
        --index;

    /* And we don't care about reads at all. */
    if (accesses[index].IsRead() && index != 0)
        if (accesses[--index].IsRead() && index != 0)
            if (accesses[--index].IsRead())
            {
                /* We seem to be looking at a possibly long list of reads.  Find
                   the write with a binary search using the write index recorded
                   in each read access. */

                unsigned write_cw_index = accesses[index].GetWriteIndex();

                index = FindRegisterAccessIndex(write_cw_index, accesses, count);

                OP_ASSERT(accesses[index].cw_index == write_cw_index || write_cw_index == 0);

                while (index != 0 && accesses[index].IsRead())
                    --index;

                OP_ASSERT(accesses[index].cw_index == write_cw_index || write_cw_index == 0);
            }

    /* Now, if we've found a write occur before 'cw_index' or an implicit write
       occuring at 'cw_index', return it, otherwise return NULL. */
    if (accesses[index].IsWrite() && (accesses[index].cw_index < cw_index || accesses[index].IsImplicit()))
        return &accesses[index];
    else
        return NULL;
}
