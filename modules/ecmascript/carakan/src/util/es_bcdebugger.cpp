/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#ifdef ES_BYTECODE_DEBUGGER

#include "modules/ecmascript/carakan/src/util/es_bcdebugger.h"
#include "modules/ecmascript/carakan/src/compiler/es_disassembler.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser.h"
#include "modules/util/opstring.h"

#ifdef UNIX
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#endif // UNIX

static void
OutputIdentifier (ES_Identifier *name)
{
    if (!name)
        fputs ("<anonymous>", stdout);
    else
    {
        const uni_char *ptr = name->name;
        while (*ptr)
            fputc (*ptr++, stdout);
    }
}

static void
OutputObject (ES_Object *object)
{
    switch (object->ClassID())
    {
	case CLASSID_BOOLEAN: fputs (" (Boolean)", stdout); break;
	case CLASSID_ARRAY: fputs (" (Array)", stdout); break;
	case CLASSID_DATE: fputs (" (Date)", stdout); break;
	case CLASSID_FUNCTION:
        fputs (" (Function; ", stdout);
        if (ES_FunctionCode *code = static_cast<ES_Function *>(object)->GetFunctionCode())
            OutputIdentifier (code->name);
        else
            fputs ("built-in", stdout);
        fputs (")", stdout);
        break;
	case CLASSID_STRING: fputs (" (String)", stdout); break;
	case CLASSID_NUMBER: fputs (" (Number)", stdout); break;
	case CLASSID_REGEX: fputs (" (RegExp)", stdout); break;
	case CLASSID_ERROR: fputs (" (Error)", stdout); break;
	case CLASSID_MATH: fputs (" (Math)", stdout); break;
	case CLASSID_ARGUMENTS: fputs (" (Arguments)", stdout); break;
	case CLASSID_OBJECT: fputs (" (Object)", stdout); break;
    case CLASSID_GLOBAL: fputs (" (GlobalObject)", stdout); break;
    }
}

static void
OutputString (JString *string)
{
    fputs ("\"", stdout);
    char buffer[80];

    unsigned length = Length (string), used_length = length < 80 ? length : 77, index;
    ES_Context context;
    const uni_char *storage = Storage (&context, string);

    for (index = 0; index < used_length; ++index)
        buffer[index] = storage[index];

    if (used_length != length)
    {
        buffer[index++] = '.';
        buffer[index++] = '.';
        buffer[index++] = '.';
    }

    buffer[index] = 0;

    fputs (buffer, stdout);
    fputs ("\"", stdout);
}

static void
OutputValue (const ES_Value_Internal &value)
{
    switch (value.Type())
    {
    case ESTYPE_UNDEFINED: fputs ("undefined", stdout); break;
    case ESTYPE_NULL: fputs ("null", stdout); break;
    case ESTYPE_BOOLEAN: fputs (value.GetBoolean() ? "boolean: true" : "boolean: false", stdout); break;
    case ESTYPE_INT32: fprintf (stdout, "int32: %d", value.GetInt32 ()); break;
    case ESTYPE_DOUBLE: fprintf (stdout, "double: %g", value.GetDouble ()); break;
    case ESTYPE_STRING:
        fputs ("string: ", stdout);
        OutputString (value.GetString ());
        break;
    case ESTYPE_OBJECT:
        fprintf (stdout, "object: %p", value.GetObject ());
        OutputObject (value.GetObject ());
        break;

    case ESTYPE_BOXED: fprintf(stdout, "boxed: %p", value.GetObject()); break;
    }
}

enum BCMode
{
    BCMODE_STEP,
    BCMODE_NEXT,
    BCMODE_CONTINUE,
    BCMODE_FINISH,
    BCMODE_UNTIL
};

enum BCCommand
{
    COMMAND_NONE,

    COMMAND_STEP,
    COMMAND_NEXT,
    COMMAND_CONTINUE,
    COMMAND_FINISH,
    COMMAND_UNTIL,
    COMMAND_UP,
    COMMAND_DOWN
};

class ES_BytecodeDebuggerData
{
public:
    ES_BytecodeDebuggerData(ES_Execution_Context *context)
        : context(context),
          mode(BCMODE_STEP),
          frame_depth(0),
          breakpoint_id_counter(0),
          function_breakpoints(NULL),
          first_code_element(NULL)
    {
    }

    ES_Execution_Context *context;
    BCMode mode;
    BCCommand command;
    unsigned call_depth;
    unsigned frame_depth;
    ES_Instruction until_instruction;
    ES_VirtualStackFrame *frame;

    class FunctionBreakpoint
    {
    public:
        unsigned id;
        ES_FunctionCode *code;
        OpString name;
        FunctionBreakpoint *next;
    };

    unsigned breakpoint_id_counter;
    FunctionBreakpoint *function_breakpoints;

    class CodeElement
    {
    public:
        ES_Code *code;
        unsigned *line_numbers;

        char filename[128];

        CodeElement *next;
    };

    CodeElement *first_code_element;

    CodeElement *GetCodeElement(ES_Code *code)
    {
        CodeElement **element = &first_code_element;

        while (*element)
        {
            if ((*element)->code == code)
                return *element;
            element = &(*element)->next;
        }

        if (mkdir(".bcdebugger", 0777) != 0)
        {
            switch (errno)
            {
            case EEXIST:
                break;

            default:
                fprintf(stderr, "failed to create directory ./.bcdebugger\n");
                exit(EXIT_FAILURE);
            }
        }

        CodeElement *new_element = *element = OP_NEW(CodeElement, ());
        new_element->code = code;
        sprintf(new_element->filename, ".bcdebugger/code-%p", code);
        new_element->next = NULL;

        FILE *file = fopen(new_element->filename, "wtc");

        if (file)
        {
            ES_Disassembler disassembler(context);

            if (code->type != ES_Code::TYPE_FUNCTION)
                disassembler.Disassemble(static_cast<ES_ProgramCode *>(code), TRUE);
            else
                disassembler.Disassemble(static_cast<ES_FunctionCode *>(code), TRUE);

            new_element->line_numbers = disassembler.GetInstructionLineNumbers();

            unsigned length = disassembler.GetLength();

            const uni_char *output = disassembler.GetOutput();
            char *buffer = OP_NEWA(char, length);

            for (unsigned index = 0; index < length; ++index)
                buffer[index] = static_cast<char>(output[index]);

            fwrite(buffer, 1, length, file);
            fclose(file);
        }

        return new_element;
    }

    BOOL SelectFrame(unsigned depth, ES_Code *code, ES_CodeWord *current)
    {
        ES_BlockItemIterator<ES_VirtualStackFrame> iter(ES_BytecodeDebugger::GetFrameStack(context));

        while (depth != 0)
            if (iter.Prev() && iter.GetItem())
            {
                code = iter.GetItem()->code;
                current = iter.GetItem()->ip;
                --depth;
            }
            else
                return FALSE;

        CodeElement *element = GetCodeElement(code);

        char format[32];
        char buffer[256];

        sprintf(format, "\x1a\x1a%%.%us:%%u:%%u:\n", (unsigned) op_strlen(element->filename));
        //sprintf(format, "%%.%us:%%u:%%u:\n", (unsigned) op_strlen(element->filename));
        sprintf(buffer, format, element->filename, element->line_numbers[current - code->codewords], 0);

        fputs(buffer, stdout);
        return TRUE;
    }

    void GetRegisterFrame(ES_Value_Internal *&registers, unsigned &frame_size)
    {
        ES_BlockItemIterator<ES_VirtualStackFrame> iter(ES_BytecodeDebugger::GetFrameStack(context));
        unsigned depth = frame_depth;

        while (depth-- != 0)
            iter.Prev();

        registers = iter.GetItem()->first_register;
        frame_size = iter.GetItem()->code->register_frame_size;
    }
};

static BOOL
IsCommand(const char *input, const char *command, const char **arguments = NULL)
{
    BOOL fuzzy_match = FALSE;

    while (*input && *input != ' ')
    {
        if (*command == '|')
        {
            fuzzy_match = TRUE;
            --input;
        }
        else if (!*command)
            return FALSE;
        else if (*input != *command)
            return FALSE;

        ++input;
        ++command;
    }

    if (fuzzy_match || !*command || *command == '|')
    {
        if (arguments)
        {
            while (*input == ' ')
                ++input;

            if (*input)
                *arguments = input;
        }

        return TRUE;
    }
    else
        return FALSE;
}

/* static */ void
ES_BytecodeDebugger::AtInstruction(ES_Execution_Context *context, ES_Code *code, ES_CodeWord *previous, ES_CodeWord *next)
{
    ES_BytecodeDebuggerData *data;

    if (!context->bytecode_debugger_data)
        data = context->bytecode_debugger_data = OP_NEW(ES_BytecodeDebuggerData, (context));
    else
        data = context->bytecode_debugger_data;

    ES_VirtualStackFrame *current_frame, *previous_frame = data->frame;

    context->frame_stack.LastItem(current_frame);

    data->frame = current_frame;

    if (next->instruction == ESI_EXIT_TO_BUILTIN || next->instruction == ESI_EXIT)
        return;

    if (data->mode == BCMODE_CONTINUE)
        return;
    else if (data->mode == BCMODE_FINISH)
    {
        if (previous)
            if (previous->instruction == ESI_RETURN_VALUE || previous->instruction == ESI_RETURN_NO_VALUE)
            {
                if (--data->call_depth == 0)
                {
                    data->mode = BCMODE_STEP;
                    goto stop_here;
                }
            }
            else if ((previous->instruction == ESI_CALL || previous->instruction == ESI_CONSTRUCT) && current_frame != previous_frame)
                ++data->call_depth;

        return;
    }
    else if (data->mode == BCMODE_NEXT)
    {
        if (previous)
            if ((previous->instruction == ESI_CALL || previous->instruction == ESI_CONSTRUCT) && current_frame != previous_frame)
            {
                data->mode = BCMODE_FINISH;
                ++data->call_depth;
                return;
            }
    }
    else if (data->mode == BCMODE_UNTIL)
        if (next->instruction == data->until_instruction)
            data->mode = BCMODE_STEP;
        else
            return;

stop_here:
    StopHere(context, code, next);
}

/* static */ void
ES_BytecodeDebugger::AtFunction(ES_Execution_Context *context, ES_FunctionCode *code)
{
    ES_BytecodeDebuggerData *data = context->bytecode_debugger_data;

    if (data)
    {
        ES_BytecodeDebuggerData::FunctionBreakpoint *function_breakpoint = data->function_breakpoints;

        while (function_breakpoint)
            if (function_breakpoint->code == code || !function_breakpoint->code && code->name && function_breakpoint->name.Compare(code->name->name) == 0)
            {
                OpString8 name;
                if (code->name)
                    name.Set(code->name->name);
                else
                    name.Set("<anonymous>");
                printf("Breakpoint %u, function %s (%p)\n", function_breakpoint->id, name.CStr(), code);
                data->mode = BCMODE_STEP;
                return;
            }
            else
                function_breakpoint = function_breakpoint->next;
    }
}

/* static */ void
ES_BytecodeDebugger::AtError(ES_Execution_Context *context, ES_Code *code, ES_CodeWord *current, const char *error, const char *message)
{
    fprintf(stdout, "%s: %s\n", error, message);

    StopHere(context, code, current);
}

/* static */ void
ES_BytecodeDebugger::StopHere(ES_Execution_Context *context, ES_Code *code, ES_CodeWord *current)
{
    ES_BytecodeDebuggerData *data;

    if (!context->bytecode_debugger_data)
        data = context->bytecode_debugger_data = OP_NEW(ES_BytecodeDebuggerData, (context));
    else
        data = context->bytecode_debugger_data;

    data->SelectFrame(0, code, current);

    char buffer[256];
    const char *arguments;

    BCCommand command = COMMAND_NONE;

    while (command == COMMAND_NONE || command == COMMAND_UP || command == COMMAND_DOWN)
    {
        fputs("(esdb) ", stdout);

        fgets (buffer, sizeof buffer, stdin);

        if (feof (stdin) || ferror (stdin))
            strcpy (buffer, "quit");

        while (op_strlen (buffer) != 0 && buffer[op_strlen (buffer) - 1] == '\n')
            buffer[op_strlen (buffer) - 1] = 0;

        if (!buffer[0])
            command = data->command;
        else if (buffer[0] == 'r' && buffer[1] == ' ' && buffer[2] >= '0' && buffer[2] <= '9')
        {
            char *index = buffer + 2, *end = index + 1;
            while (*end >= '0' && *end <= '9')
                ++end;
            *end = 0;

            unsigned r = op_strtoul(index, &end, 10), register_frame_size = code->register_frame_size;
            ES_Value_Internal *registers = context->reg;

            if (data->frame_depth != 0)
                data->GetRegisterFrame(registers, register_frame_size);

            if (r >= register_frame_size)
            {
                fprintf (stdout, "register index out of range; frame size is %u\n", code->register_frame_size);
                continue;
            }

            fprintf(stdout, "reg(%u) = ", r);
            OutputValue(registers[r]);
            fputs("\n", stdout);
            continue;
        }
        else if (IsCommand(buffer, "s|tep"))
        {
            command = COMMAND_STEP;
            break;
        }
        else if (IsCommand(buffer, "n|ext"))
        {
            command = COMMAND_NEXT;
            break;
        }
        else if (IsCommand(buffer, "c|ontinue"))
        {
            command = COMMAND_CONTINUE;
            break;
        }
        else if (IsCommand(buffer, "fi|nish"))
        {
            command = COMMAND_FINISH;
            break;
        }
        else if (IsCommand(buffer, "u|ntil", &arguments))
        {
            if (op_strcmp(arguments, "call") == 0)
                data->until_instruction = ESI_CALL;
            else if (op_strcmp(arguments, "construct") == 0)
                data->until_instruction = ESI_CONSTRUCT;
            else if (op_strcmp(arguments, "load_true") == 0)
                data->until_instruction = ESI_LOAD_TRUE;
            else
                fprintf (stdout, "sorry; can't wait for that instruction\n");

            command = COMMAND_UNTIL;
            break;
        }
        else if (IsCommand(buffer, "q|uit"))
            exit(0);
        else if (IsCommand(buffer, "p|rint", &arguments))
        {
            ES_CollectorLock gclock(context);
            ES_FunctionCode *code;

            OpString source;
            source.Set(arguments);

            if (ES_Parser::ParseDebugCode(context, code, context->GetGlobalObject(), source.CStr(), source.Length()))
            {
                data->mode = BCMODE_FINISH;
                data->call_depth = 1;

                ES_Function *function = ES_Function::Make(context, context->GetGlobalObject(), code, 0);
                ES_Value_Internal *registers = context->SetupFunctionCall(function, 0);
                registers[0].SetObject(context->GetGlobalObject());
                registers[1].SetObject(function);
                ES_Value_Internal return_value;
                context->CallFunction(registers, 0, &return_value);
                fputs("result = ", stdout);
                OutputValue(return_value);
                fputs("\n", stdout);
            }
            else
                fprintf(stdout, "parse error\n");
        }
        else if (IsCommand(buffer, "b|reak", &arguments))
        {
            ES_BytecodeDebuggerData::FunctionBreakpoint *function_breakpoint = OP_NEW(ES_BytecodeDebuggerData::FunctionBreakpoint, ());
            function_breakpoint->id = ++data->breakpoint_id_counter;
            function_breakpoint->code = NULL;
            function_breakpoint->name.Set(arguments);
            function_breakpoint->next = data->function_breakpoints;
            data->function_breakpoints = function_breakpoint;

            fprintf(stdout, "Breakpoint %u at 'function named %s'\n", function_breakpoint->id, arguments);
        }
        else if (IsCommand(buffer, "u|p"))
            data->command = command = COMMAND_UP;
        else if (IsCommand(buffer, "d|own"))
            data->command = command = COMMAND_DOWN;
        else
        {
            command = COMMAND_NONE;
            fprintf(stdout, "unrecognized input; please try again\n");
        }

        if (command == COMMAND_UP)
        {
            if (data->SelectFrame(data->frame_depth + 1, NULL, NULL))
                ++data->frame_depth;
            else
                fprintf(stdout, "Initial frame seleceted; you cannot go up.\n");
        }
        else if (command == COMMAND_DOWN)
        {
            if (data->frame_depth > 0 && data->SelectFrame(data->frame_depth - 1, code, current))
                --data->frame_depth;
            else
                fprintf(stdout, "Bottom (innermost) frame seleceted; you cannot go down.\n");
        }
    }

    if (command == COMMAND_STEP)
        data->mode = BCMODE_STEP;
    else if (command == COMMAND_NEXT)
    {
        data->mode = BCMODE_NEXT;
        data->call_depth = 0;
    }
    else if (command == COMMAND_FINISH)
    {
        data->mode = BCMODE_FINISH;
        data->call_depth = 1;
    }
    else if (command == COMMAND_CONTINUE)
        data->mode = BCMODE_CONTINUE;
    else if (command == COMMAND_UNTIL)
        data->mode = BCMODE_UNTIL;

    data->command = command;
}

#endif // ES_BYTECODE_DEBUGGER
