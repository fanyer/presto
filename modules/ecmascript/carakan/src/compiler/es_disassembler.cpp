/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#include "core/pch.h"

#ifdef ES_DISASSEMBLER_SUPPORT

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_disassembler.h"
#include "modules/ecmascript/carakan/src/compiler/es_instruction_data.h"
#include "modules/ecmascript/carakan/src/util/es_instruction_string.h"
#include "modules/util/opstring.h"

ES_Disassembler::ES_Disassembler(ES_Context *context)
    : primary(NULL),
      line_numbers(0),
      context(context),
      optimization_data(NULL)
{
    output.SetExpansionPolicy(TempBuffer::AGGRESSIVE);
    output.SetCachedLengthPolicy(TempBuffer::TRUSTED);
}

static BOOL
IncludeFunction(ES_FunctionCode *code)
{
#ifdef _STANDALONE
    extern uni_char **g_focused_functions;

    if (!g_focused_functions)
        return TRUE;
    else if (!code || !code->GetName() && !code->GetDebugName())
        return FALSE;
    else
    {
        JString *name = code->GetName() ? code->GetName() : code->GetDebugName();
        uni_char **focused = g_focused_functions;
        while (*focused)
            if (name->Equals(*focused, uni_strlen(*focused)))
                return TRUE;
            else
                ++focused;
        return FALSE;
    }
#else // _STANDALONE
    return TRUE;
#endif // _STANDALONE
}

static void
OutputExceptionHandlers(TempBuffer &output, ES_CodeStatic::ExceptionHandler *handlers, unsigned handlers_count, unsigned depth = 1)
{
    if (handlers_count != 0)
    {
        if (depth == 1)
            output.AppendL("Exception handlers:\n");

        for (unsigned index = 0; index < handlers_count; ++index)
        {
            ES_CodeStatic::ExceptionHandler &handler = handlers[index];

            for (unsigned i = 0; i < depth; ++i) output.AppendL("  ");
            output.AppendL("Type:   ");
            if (handler.type == ES_CodeStatic::ExceptionHandler::TYPE_CATCH)
                output.AppendL("CATCH\n");
            else
                output.AppendL("FINALLY\n");

            for (unsigned i = 0; i < depth; ++i) output.AppendL("  ");
            output.AppendL("Range:  [");
            output.AppendUnsignedLongL(handler.start);
            output.AppendL(", ");
            output.AppendUnsignedLongL(handler.end);
            output.AppendL(")\n");

            for (unsigned i = 0; i < depth; ++i) output.AppendL("  ");
            output.AppendL("Target: ");
            output.AppendUnsignedLongL(handler.handler_ip);
            output.AppendL("\n");

            OutputExceptionHandlers(output, handler.nested_handlers, handler.nested_handlers_count, depth + 1);
        }
    }
}

static void
OutputInnerScopes(TempBuffer &output, ES_CodeStatic::InnerScope *scopes, unsigned scopes_count)
{
    if (scopes_count != 0)
    {
        output.AppendL("Inner scopes:\n");

        for (unsigned index = 0; index < scopes_count; ++index)
        {
            ES_CodeStatic::InnerScope &scope = scopes[index];
            OpString s; ANCHOR(OpString, s);
            LEAVE_IF_ERROR(s.AppendFormat(UNI_L("  %u: "), index));

            for (unsigned i = 0; i < scope.registers_count; ++i)
                LEAVE_IF_ERROR(s.AppendFormat(UNI_L("%sreg(%u)"), i > 0 ? UNI_L(", ") : UNI_L(""), scope.registers[i]));

            output.AppendL(s.CStr());
            output.AppendL("\n");
        }
    }
}

static void
AppendTypes(OpString &string, ES_Code *code, const ES_CodeOptimizationData::RegisterAccess *access)
{
    if (access->IsTypeKnown())
    {
        BOOL first = TRUE;

#define CHECK(TYPE, type) if (access->GetType() & ESTYPE_BITS_##TYPE) { string.Append(first ? #type : "|" #type); first = FALSE; }

        CHECK(UNDEFINED, undefined);
        CHECK(NULL, null);
        CHECK(DOUBLE, double);
        CHECK(INT32, int32);
        CHECK(BOOLEAN, boolean);
        CHECK(STRING, string);
        CHECK(OBJECT, object);

        ES_Value_Internal value;
        if (access->GetValue(code, value))
        {
            if (value.IsInt32())
                string.AppendFormat(UNI_L("=%d"), value.GetInt32());
            else if (value.IsDouble())
                string.AppendFormat(UNI_L("=%g"), value.GetDouble());
            else if (value.IsBoolean())
                string.Append(value.GetBoolean() ? "=true" : "=false");
            else
                string.Append("=known");
        }
    }
    else
        string.Append("unknown");
}

static void
AppendAccesses(OpString &string, ES_Code *code, const ES_CodeOptimizationData::RegisterAccess *access, unsigned end_cw_index, unsigned indent)
{
    BOOL first = TRUE;
    unsigned skip = 0;

    for (; access->cw_index < end_cw_index; ++access)
    {
        OpString s0;

        if (access->IsWrite())
        {
            const uni_char *format;

            if (access->IsImplicit())
                format = UNI_L("(W:%u)=<%s>");
            else
                format = UNI_L("W:%u=<%s>");

            OpString ts;
            AppendTypes(ts, code, access);
            s0.AppendFormat(format, access->cw_index, ts.CStr());
        }
        else
        {
            const uni_char *format;

            if (access->IsImplicit())
                format = UNI_L("(R:%u)");
            else
                format = UNI_L("R:%u");

            s0.AppendFormat(format, access->cw_index);

            if (access->IsTypeKnown())
            {
                OpString ts;
                AppendTypes(ts, code, access);
                s0.AppendFormat(access->IsReadTypeHint() ? UNI_L("=(%s)") : UNI_L("=<%s>"), ts.CStr());
            }
        }

        if (first)
            first = FALSE;
        else
        {
            string.Append(",");

            if (string.Length() + 1u + s0.Length() - skip > 80u - indent)
            {
                string.Append("\n");
                string.Append("                                " + (32 - indent));

                skip = string.Length();
            }
            else
                string.Append(" ");
        }

        string.Append(s0.CStr());
    }
}

static void
OutputOptimizationData(TempBuffer &output, ES_Code *code, ES_CodeOptimizationData *data)
{
    unsigned index;

    output.AppendL("  Non-temporaries:\n");

    for (index = 0; index < code->data->first_temporary_register; ++index)
    {
        OpString s;
        s.AppendFormat(UNI_L("    reg(%u): "), index);

        OpString as;
        AppendAccesses(as, code, data->register_accesses[index], code->data->codewords_count, s.Length());
        s.Append(as.CStr());

        s.AppendL("\n");
        output.AppendL(s.CStr());
    }

    if (index != code->data->register_frame_size)
    {
        output.AppendL("  Temporaries:\n");

        for (; index < code->data->register_frame_size; ++index)
        {
            OpString s;
            s.AppendFormat(UNI_L("    reg(%u): "), index);

            OpString as;
            AppendAccesses(as, code, data->register_accesses[index], code->data->codewords_count, s.Length());
            s.Append(as.CStr());

            s.AppendL("\n");
            output.AppendL(s.CStr());
        }
    }
}

static void
OutputSwitchTables(TempBuffer &output, ES_Code *code)
{
    if (code->data->switch_tables_count != 0)
    {
        output.AppendL("  Switch tables:\n");

        for (unsigned index = 0; index < code->data->switch_tables_count; ++index)
        {
            ES_CodeStatic::SwitchTable &table = code->data->switch_tables[index];

            for (int value = table.minimum; value <= table.maximum; ++value)
            {
                OpString line;
                line.AppendFormat(UNI_L("    %d: codeword %u%s\n"), value, table.codeword_indeces[value], table.codeword_indeces[value] == table.default_codeword_index ? UNI_L(" (default)") : UNI_L(""));
                output.AppendL(line.CStr());
            }

            OpString line;
            line.AppendFormat(UNI_L("    default: codeword %u\n"), table.default_codeword_index);
            output.AppendL(line.CStr());
        }
    }
}

/*

static void
OutputDebugRecords(TempBuffer &output, ES_Code *code)
{
    if (code->data->debug_records)
    {
        output.AppendL("Debug records:\n");

        for (unsigned index = 0; index < code->data->debug_records_count; ++index)
        {
            ES_CodeStatic::DebugRecord &record = code->data->debug_records[index];
            const uni_char *type;

            if (static_cast<ES_CodeStatic::DebugRecord::Type>(record.type) == ES_CodeStatic::DebugRecord::TYPE_EXTENT_INFORMATION)
                type = UNI_L("extent");
            else
                type = UNI_L("base");

            OpString line;

            line.AppendFormat(UNI_L("  %4u: type=%s, index=%u, line=%u, length=%u\n"), record.codeword_index, type, record.location.Index(), record.location.Line(), record.location.Length());
            output.AppendL(line);
        }
    }
}

*/

void
ES_Disassembler::Disassemble(ES_ProgramCode *program, BOOL local_only)
{
    if (!primary)
        primary = program;

    if (!local_only)
        OutputFunctions(program);

#ifdef _STANDALONE
    if (IncludeFunction(NULL))
#endif // _STANDALONE
    {
        output.AppendL("Program:\n");
        output.AppendL("  reg(0) = this\n");

        if (program->data->register_frame_size > 1)
        {
            OpString temps; ANCHOR(OpString, temps);
            LEAVE_IF_ERROR(temps.AppendFormat(UNI_L("  reg(%u-%u) = temporaries\n"), program->data->first_temporary_register, program->data->register_frame_size - 1));
            output.AppendL(temps.CStr());
        }

        OutputExceptionHandlers(output, program->data->exception_handlers, program->data->exception_handlers_count);
        OutputInnerScopes(output, program->data->inner_scopes, program->data->inner_scopes_count);
        //OutputDebugRecords(output, program);

#ifdef ES_NATIVE_SUPPORT
        if (program->data->optimization_data)
            OutputOptimizationData(output, program, program->data->optimization_data);
#endif // ES_NATIVE_SUPPORT

        OutputSwitchTables(output, program);

        DisassembleCode(program, program->data->codewords, program->data->codewords_count);
    }
}

void
ES_Disassembler::Disassemble(ES_FunctionCode *function, BOOL local_only, BOOL force_include)
{
    if (!primary)
        primary = function;

    if (!local_only)
        OutputFunctions(function, IncludeFunction(function));

    if (!force_include && !IncludeFunction(function))
        return;

    ES_FunctionCodeStatic *data = function->GetData();

    OpString parameters; ANCHOR(OpString, parameters);
    OpString parameter_regs; ANCHOR(OpString, parameter_regs);
    for (unsigned formal_index = 0; formal_index < data->formals_count; ++formal_index)
    {
        if (formal_index > 0)
            parameters.AppendL(", ");

        JString *formal = function->GetString(data->formals_and_locals[formal_index]);
        parameters.AppendL(Storage(context, formal), Length(formal));
        LEAVE_IF_ERROR(parameter_regs.AppendFormat(UNI_L("  reg(%u) = formal '%s'\n"), formal_index + 2, StorageZ(context, formal)));
    }

    OpString header; ANCHOR(OpString, header);
    if (function->GetName())
        LEAVE_IF_ERROR(header.AppendFormat(UNI_L("Function %s(%s):\n"), StorageZ(context, function->GetName()), parameters.HasContent() ? parameters.CStr() : UNI_L("")));
    else
        LEAVE_IF_ERROR(header.AppendFormat(UNI_L("Anonymous function (%s):\n"), parameters.HasContent() ? parameters.CStr() : UNI_L("")));

    output.AppendL(header.CStr());
    output.AppendL("  reg(0) = this\n  reg(1) = arguments.callee\n");
    if (parameter_regs.CStr())
        output.AppendL(parameter_regs.CStr());

    unsigned local_index;
    for (local_index = 0; local_index < data->LocalsCount(); ++local_index)
    {
        OpString local; ANCHOR(OpString, local);
        unsigned formals_count = data->formals_count;
        LEAVE_IF_ERROR(local.AppendFormat(UNI_L("  reg(%u) = local '%s'\n"), local_index + 2 + formals_count, StorageZ(context, function->GetString(data->formals_and_locals[formals_count + local_index]))));
        output.AppendL(local);
    }

    if (2 + local_index < function->data->register_frame_size)
    {
        OpString temps; ANCHOR(OpString, temps);
        LEAVE_IF_ERROR(temps.AppendFormat(UNI_L("  reg(%u-%u) = temporaries\n"), function->data->first_temporary_register, function->data->register_frame_size - 1));
        output.AppendL(temps);
    }

    OutputExceptionHandlers(output, function->data->exception_handlers, function->data->exception_handlers_count);
    OutputInnerScopes(output, function->data->inner_scopes, function->data->inner_scopes_count);
    //OutputDebugRecords(output, function);

    if (primary == function && optimization_data)
        OutputOptimizationData(output, function, optimization_data);
#ifdef ES_NATIVE_SUPPORT
    else if (function->data->optimization_data)
        OutputOptimizationData(output, function, function->data->optimization_data);
#endif // ES_NATIVE_SUPPORT

    OutputSwitchTables(output, function);

    DisassembleCode(function, function->data->codewords, function->data->codewords_count);
}

void
ES_Disassembler::Disassemble(ES_Code *code, ES_CodeWord *codewords, unsigned codewords_count)
{
    DisassembleCode(code, codewords, codewords_count);
}

#if defined UNIX || defined LINGOGI || defined LINUXSDK || defined _STANDALONE
# include <stdio.h>

class ES_SuspendedDisassemble
    : public ES_SuspendedCall
{
public:
    ES_SuspendedDisassemble(ES_Execution_Context *context, ES_Code *code)
        : disassembler(context),
          code(code)
    {
        context->SuspendedCall(this);
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        if (code->type == ES_Code::TYPE_FUNCTION)
            disassembler.Disassemble(static_cast<ES_FunctionCode *>(code), TRUE);
        else
            disassembler.Disassemble(static_cast<ES_ProgramCode *>(code), TRUE);

        OpString8 result;

        result.Set(disassembler.GetOutput(), disassembler.GetLength());

        puts(result.CStr());
    }

    ES_Disassembler disassembler;
    ES_Code *code;
};

void
ES_DisassembleFromDebugger(ES_Execution_Context *context, ES_Code *code)
{
    ES_SuspendedDisassemble(context, code);
}

void
ES_DisassembleFromHostCode(ES_Context *context, ES_FunctionCode *code)
{
    ES_Disassembler disassembler(context->GetExecutionContext());
    disassembler.Disassemble(code, TRUE);
    OpString8 result;

    result.Set(disassembler.GetOutput(), disassembler.GetLength());

    puts(result.CStr());
}

#endif // UNIX || LINGOGI || LINUXSDK || _STANDALONE

static void
AppendPadding(TempBuffer &output, unsigned amount)
{
    OP_ASSERT(amount <= 32);
    output.AppendL("                                " + (32 - amount));
}

TempBuffer &
ES_Disassembler::DisassembleInstruction(ES_Code *code, ES_CodeWord *word)
{
    output.Clear();
    output.AppendL("  ");

    unsigned cw_index = word - code->data->codewords;
    if (cw_index < 10) AppendPadding(output, 3);
    else if (cw_index < 100) AppendPadding(output, 2);
    else if (cw_index < 1000) AppendPadding(output, 1);
    output.AppendUnsignedLongL(cw_index);
    output.AppendL(": ");

    const char *name = g_instruction_strings[word->instruction];
    OutputInstruction(name);
    if (op_strlen(name) < 32)
        AppendPadding(output, 32 - op_strlen(name));

    operand_index = 0;

    this->word = word;
    DisassembleInstruction(code, word, code->data->codewords);

    output.AppendL("\n");
    return output;
}

const char *
ES_Disassembler::GetInstructionName(ES_Instruction instruction)
{
    return g_instruction_strings[instruction];
}

void
ES_Disassembler::DisassembleCode(ES_Code *code_, ES_CodeWord *codewords, unsigned codewords_count)
{
    code = code_;
    word = codewords;

    ES_CodeWord *stop = codewords + codewords_count;

    unsigned base_line_number = 2;
    const uni_char *storage = output.GetStorage(), *storage_end = storage + output.Length();
    while (storage != storage_end)
        if (*storage++ == '\n')
            ++base_line_number;
    unsigned line_number = base_line_number;

    output.AppendL("Code:\n");

    if (code == primary)
        line_numbers = OP_NEWA_L(unsigned, codewords_count);

    while (word != stop)
    {
        if (code == primary)
            line_numbers[word - codewords] = line_number;
        ++line_number;

        output.AppendL("  ");
        unsigned cw_index = word - codewords;
        if (cw_index < 10) AppendPadding(output, 3);
        else if (cw_index < 100) AppendPadding(output, 2);
        else if (cw_index < 1000) AppendPadding(output, 1);
        output.AppendUnsignedLongL(cw_index);
        output.AppendL(": ");

        const char *name = g_instruction_strings[word->instruction];
        OutputInstruction(name);
        if (op_strlen(name) < 32)
            AppendPadding(output, 32 - op_strlen(name));

        operand_index = 0;

        ES_CodeWord *word_next = word;
        DisassembleInstruction(code, word_next, codewords);
#ifdef ES_NATIVE_SUPPORT
        OutputProfilingData(code, word, cw_index);
#endif // ES_NATIVE_SUPPORT
        word = word_next;

        output.AppendL("\n");
    }
}

void
ES_Disassembler::OutputFunctions(ES_Code *code, BOOL force_include)
{
    if (code->data->function_declarations_count != 0)
    {
#ifdef _STANDALONE
        if (IncludeFunction(NULL))
#endif // _STANDALONE
            output.AppendL("Function declarations:\n\n");

        for (unsigned function_declaration_index = 0; function_declaration_index < code->data->function_declarations_count; ++function_declaration_index)
            if (ES_FunctionCode *function = code->functions[code->data->function_declarations[function_declaration_index].function])
                if (force_include || IncludeFunction(function))
                {
                    Disassemble(function, FALSE, force_include);
                    output.AppendL("\n");
                }
                else
                    OutputFunctions(function, force_include);
    }

    if (code->data->functions_count > code->data->function_declarations_count)
    {
#ifdef _STANDALONE
        if (IncludeFunction(NULL))
#endif // _STANDALONE
            output.AppendL("Other functions:\n\n");

        for (unsigned function_index = 0; function_index < code->data->functions_count; ++function_index)
            if (force_include || IncludeFunction(code->functions[function_index]))
            {
                if (code->data->functions[function_index]->is_function_expr)
                {
                    Disassemble(code->functions[function_index], FALSE, force_include);
                    output.AppendL("\n");
                }
            }
            else
                OutputFunctions(code->functions[function_index], force_include);
    }
}

void
ES_Disassembler::OutputInstruction(const char *string)
{
    output.AppendL(string);
}

void
ES_Disassembler::OutputRegister(unsigned index, const char *name)
{
    if (operand_index++ != 0)
        output.AppendL(", ");
    output.AppendL("reg(");
#if 0
    if (name)
    {
        output.AppendL(name);
        output.AppendL(":");
    }
#endif // 0
    if (index != ~0u)
        output.AppendUnsignedLongL(index);
    else
        output.AppendL("N/A");
    output.AppendL(")");
}

void
ES_Disassembler::OutputRegisters(unsigned count, ES_CodeWord *&word)
{
    while (count--)
        OutputRegister(word++->index);
}

void
ES_Disassembler::OutputImmediate(int immediate, const uni_char *key)
{
    if (operand_index++ != 0)
        output.AppendL(", ");
    OpString string; ANCHOR(OpString, string);
    LEAVE_IF_ERROR(string.AppendFormat(UNI_L("%s(%d)"), key ? key : UNI_L("imm"), immediate));
    output.AppendL(string.CStr());
}

void
ES_Disassembler::OutputOffset(int offset, unsigned base)
{
    if (operand_index++ != 0)
        output.AppendL(", ");
    OpString string; ANCHOR(OpString, string);
    LEAVE_IF_ERROR(string.AppendFormat(UNI_L("offset(%d) => %u"), offset, base + offset));
    output.AppendL(string.CStr());
}

void
ES_Disassembler::OutputIdentifier(ES_Code *code, ES_CodeWord &word)
{
    if (operand_index++ != 0)
        output.AppendL(", ");

    OpString string; ANCHOR(OpString, string);
    JString *identifier;

    identifier = word.index != ~0u ? code->GetString(word.index) : NULL;

    const uni_char *name = identifier ? StorageZ(context, identifier) : UNI_L("<not implemented>");

    LEAVE_IF_ERROR(string.AppendFormat(UNI_L("ident('%s')"), name));
    output.AppendL(string.CStr());
}

void
ES_Disassembler::OutputString(ES_Context *context, ES_Code *code, unsigned index)
{
    if (operand_index++ != 0)
        output.AppendL(", ");

    OpString string; ANCHOR(OpString, string);
    if (index == ~0u)
        string.AppendL(UNI_L("const(no string)"));
    else
    {
        OpString as_string; ANCHOR(OpString, as_string);
        as_string.Set(Storage(context, code->GetString(index)), es_minu(Length(code->GetString(index)), 100));

        for (int i = 0; i < as_string.Length(); i += 2)
            switch (as_string[i])
            {
            case '"':
            case '\\':
                as_string.Insert(i, "\\");
                break;

            case '\n':
                as_string.Delete(i, 1);
                as_string.Insert(i, "\\n");
                break;

            case '\r':
                as_string.Delete(i, 1);
                as_string.Insert(i, "\\r");
                break;

            case '\t':
                as_string.Delete(i, 1);
                as_string.Insert(i, "\\t");
                break;

            default:
                --i;
            }

        as_string.Insert(0, "string(\"");
        as_string.Append("\")");

        LEAVE_IF_ERROR(string.AppendFormat(UNI_L("const(%u)=%s"), index, as_string.CStr()));
    }
    output.AppendL(string.CStr());
}

void
ES_Disassembler::OutputDouble(ES_Context *context, ES_Code *code, unsigned index)
{
    if (operand_index++ != 0)
        output.AppendL(", ");

    OpString string; ANCHOR(OpString, string);
    if (index == ~0u)
        string.AppendL("const(<not implemented>)");
    else
        string.AppendFormat(UNI_L("const(%u)=double(%f)"), index, code->data->doubles[index]);
    output.AppendL(string.CStr());
}

void
ES_Disassembler::OutputFunction(ES_Code *code, ES_CodeWord &word)
{
    if (operand_index++ != 0)
        output.AppendL(", ");
    output.AppendL("<function>");
}

void
ES_Disassembler::OutputPropertyOffset(unsigned index)
{
    if (operand_index++ != 0)
        output.AppendL(", ");
    OpString string; ANCHOR(OpString, string);
    LEAVE_IF_ERROR(string.AppendFormat(UNI_L("property(%u)"), index));
    output.AppendL(string.CStr());
}

static const uni_char *
OutputCachedType(unsigned type_bits)
{
    switch (ES_Value_Internal::StorageTypeFromCachedTypeBits(type_bits))
    {
        case ES_STORAGE_WHATEVER: return UNI_L("value");
        case ES_STORAGE_NULL: return UNI_L("null");
        case ES_STORAGE_UNDEFINED: return UNI_L("undefined");
        case ES_STORAGE_BOOLEAN: return UNI_L("bool");
        case ES_STORAGE_INT32: return UNI_L("int");
        case ES_STORAGE_DOUBLE: return UNI_L("double");
        case ES_STORAGE_STRING: return UNI_L("string");
        case ES_STORAGE_STRING_OR_NULL: return UNI_L("string?");
        case ES_STORAGE_OBJECT: return UNI_L("object");
        case ES_STORAGE_OBJECT_OR_NULL: return UNI_L("object?");
        case ES_STORAGE_BOXED: return UNI_L("boxed");
        default: return UNI_L("");
    }
}

void
ES_Disassembler::OutputPropertyCache(ES_Code *code, unsigned index)
{
    if (operand_index++ != 0)
        output.AppendL(", ");
    ES_Code::PropertyCache *cache;
    switch (word->instruction)
    {
    case ESI_GETN_IMM:
    case ESI_GET_LENGTH:
        cache = code->property_get_caches;
        break;

    case ESI_PUTN_IMM:
    case ESI_INIT_PROPERTY:
    case ESI_PUT_LENGTH:
        cache = code->property_put_caches;
        break;
    }
    cache += index;
    OpString string; ANCHOR(OpString, string);
    LEAVE_IF_ERROR(string.AppendFormat(UNI_L("cache(%u)"), index));
    if (cache->class_id != ES_Class::NOT_CACHED_CLASS_ID)
    {
        LEAVE_IF_ERROR(string.Append("={"));
        while (cache)
        {
            if (cache->IsNegativeCache())
                LEAVE_IF_ERROR(string.AppendFormat(UNI_L("class(%u), limit(%d) => N/A"), cache->class_id, int(cache->GetLimit())));
            else
            {
                unsigned offset = cache->GetOffset();
                LEAVE_IF_ERROR(string.AppendFormat(UNI_L("class(%u), limit(%d) => offset(%u) :: %s"), cache->class_id, int(cache->GetLimit()), offset, OutputCachedType(cache->cached_type)));
                switch (word->instruction)
                {
                case ESI_GETN_IMM:
                case ESI_GET_LENGTH:
                case ESI_GET_GLOBAL:
                    if (cache->data.prototype_object)
                        LEAVE_IF_ERROR(string.Append(" in prototype"));
                    break;

                default:
                    if (cache->data.new_class)
                        LEAVE_IF_ERROR(string.AppendFormat(UNI_L(" and class(%u)"), cache->data.new_class->Id()));
                }
            }
            if (cache->next)
                string.Append(", ");
            cache = cache->next;
        }
        LEAVE_IF_ERROR(string.Append("}"));
    }
    output.AppendL(string.CStr());
}

void
ES_Disassembler::OutputGlobalCache(ES_Code *code, unsigned index)
{
    if (operand_index++ != 0)
        output.AppendL(", ");
    ES_Code::GlobalCache *cache = &code->global_caches[index];
    OpString string; ANCHOR(OpString, string);
    LEAVE_IF_ERROR(string.AppendFormat(UNI_L("cache(%u)"), index));
    if (cache->class_id == ES_Class::GLOBAL_IMMEDIATE_CLASS_ID)
        LEAVE_IF_ERROR(string.AppendFormat(UNI_L("={global immediate: %u}"), cache->cached_index));
    else if (cache->class_id != ES_Class::NOT_CACHED_CLASS_ID)
        LEAVE_IF_ERROR(string.AppendFormat(UNI_L("={class(%u) => index(%u)}"), cache->class_id, cache->cached_index));
    output.AppendL(string.CStr());
}

#ifdef ES_NATIVE_SUPPORT
void
ES_Disassembler::OutputProfilingData(ES_Code *code, ES_CodeWord *word, unsigned index)
{
    if (code->data->profile_data && code->data->profile_data[index] != 0)
    {
        if (operand_index++ != 0)
            output.AppendL(", ");

        LEAVE_IF_ERROR(output.AppendFormat(UNI_L("profile={"), index, g_instruction_operand_count[word->instruction]));
        if (g_instruction_operand_count[word->instruction] > 0)
        {
            index++;
            output.AppendUnsignedLongL(code->data->profile_data[index]);
            for (unsigned l = 1; l < g_instruction_operand_count[word->instruction]; ++l)
            {
                output.AppendL(", ");
                output.AppendUnsignedLongL(code->data->profile_data[index + l]);
            }
        }
        output.AppendL("}");
    }
}
#endif // ES_NATIVE_SUPPORT

#endif // ES_DISASSEMBLER_SUPPORT
