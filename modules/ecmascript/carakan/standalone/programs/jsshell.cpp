/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999-2005
 *
 * Standalone shell for ECMAScript engine
 */

#include "core/pch.h"

#ifdef UNIX
# include <sys/time.h>
# include <sys/resource.h>
# include <sys/errno.h>
#endif // UNIX

#include <signal.h>
extern "C"
void Debug_OpAssert(const char* expression, const char* file, int line)
{
    printf("ASSERT FAILED: OP_ASSERT(%s) %s:%d\n",expression, file, line);
#if defined(UNIX)
    raise(SIGABRT);
#else
    assert(0);
#endif
}

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript/carakan/src/ecmascript_manager_impl.h"
#define TIME_SLICE_SIZE 50 // ms

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_disassembler.h"
#include "modules/ecmascript/carakan/src/util/es_instruction_string.h"
#include "programs/jsshell_object.h"

#include "programs/stopwatch.h"

#include "modules/util/opstring.h"

Opera* g_opera;
OpSystemInfo* g_op_system_info;
OpTimeInfo* g_op_time_info;
StopWatch g_slice_stop_watch;

#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
BOOL g_debug_native_behaviour = FALSE;
BOOL g_disassemble_loop_dispatcher = FALSE;
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

#ifndef ES_STANDALONE_VERSION
# define ES_STANDALONE_VERSION "unknown version"
#endif // ES_STANDALONE_VERSION

#ifndef ES_STANDALONE_BUILDTIME
# define ES_STANDALONE_BUILDTIME "now or never"
#endif // ES_STANDALONE_BUILDTIME

OP_STATUS
ESCB_SignalExecutionEvent(unsigned int ev, ES_Runtime* rt)
{
    return OpStatus::OK;
}

BOOL TimeOut()
{
    g_slice_stop_watch.Stop();
    if (g_slice_stop_watch.GetElapsedTime() > TIME_SLICE_SIZE)
    {
        g_slice_stop_watch.Start(); // restart the timer
        return TRUE;
    }
    else
        return FALSE;
}

void AddHostObject(ES_Context *context, ES_Runtime *runtime, EcmaScript_Object *o, const uni_char *name)
{
    ES_CollectorLock gclock(context);
    o->SetObjectRuntime(runtime, NULL, NULL, FALSE);
    runtime->PutInGlobalObject(o, name);
}

#ifdef NATIVE_DISASSEMBLER_SUPPORT
FILE *g_native_disassembler_file = NULL;
enum DisassemblerFormat
{
    DEFAULT,
    INTEL,
    ATT
};
int g_native_disassembler_format = DEFAULT;
#endif // NATIVE_DISASSEMBLER_SUPPORT

#ifdef ES_NATIVE_SUPPORT
BOOL g_force_fpmode_x87 = FALSE;
#endif // ES_NATIVE_SUPPORT

struct JsShellOptions
{
    BOOL use_expr;
    BOOL quiet;
    BOOL reformat;
#if defined(ES_BYTECODE_DEBUGGER) || defined(_DEBUG)
    BOOL debug;
#endif
#ifdef ES_BYTECODE_LOGGER
    BOOL log;
#endif // ES_BYTECODE_LOGGER
    BOOL benchmark;
    BOOL disassembleBefore, disassembleAfter;
#ifdef ES_NATIVE_SUPPORT
    BOOL native_dispatcher;
#endif // ES_NATIVE_SUPPORT
    BOOL force_cached;
#ifdef ECMASCRIPT_DEBUGGER
    BOOL es_debug;
#endif // ECMASCRIPT_DEBUGGER
};

BOOL g_disassemble_eval = FALSE;
BOOL g_slow_case_summary = FALSE;

uni_char **g_focused_functions = NULL;
BOOL g_native_dispatcher = FALSE;

#ifdef ES_DEBUG_COMPACT_OBJECTS
BOOL g_debug_type_changes = FALSE;
#endif // ES_DEBUG_COMPACT_OBJECTS

#ifdef ACOVEA
unsigned g_class_linear_growth = 1;
unsigned g_class_growth_rate = 2;
unsigned g_max_property_cache_size = 10;
#endif

static OP_STATUS
HandleCompilationError (const uni_char *message, const uni_char *full_message, const ES_SourcePosition &position, void *user_data)
{
    unsigned length = uni_strlen(full_message);
    char *ascii = OP_NEWA_L(char, (length + 2));

    for (unsigned index = 0; index < length; ++index)
        ascii[index] = (char) full_message[index];
    ascii[length] = '\n';
    ascii[length + 1] = '\0';
    fputs(ascii, stdout);

    OP_DELETEA(ascii);
    return OpStatus::OK;
}

static BOOL runRepl(const JsShellOptions &opt, EcmaScript_Object *global_object_shadow)
{
    ES_Runtime runtime;
    runtime.Construct(global_object_shadow, "global", g_native_dispatcher);
    ES_Eval_Status eval_result;

    static_cast<JSShell_Object *>(global_object_shadow)->InitializeL();

    while (true)
    {
        BOOL brk = FALSE;
        TempBuffer t;

        BOOL previous_was_newline = TRUE;
        // read
        int c;
        // fprintf(stdout, ">");
        while ((c = fgetc(stdin)) != EOF)
        {
            t.AppendL((uni_char)c);
            if (previous_was_newline && (c == '\n' || c == '\r'))
                break;
            else if (c == '\n' || c == '\r')
            {
                // fprintf(stdout, ">");
                previous_was_newline = TRUE;
            }
            else
                previous_was_newline = FALSE;
        }

        ES_ProgramText program_text[1];
        program_text[0].program_text = t.GetStorage();
        program_text[0].program_text_length = t.Length();
        ES_Program *program = NULL;

        ES_Runtime::CompileProgramOptions options;
        options.generate_result = TRUE;
        options.reformat_source = opt.reformat;
        options.error_callback = HandleCompilationError;

        OP_STATUS ret = runtime.CompileProgram(program_text, 1, &program, options);

        if (OpStatus::IsError(ret))
        {
            fprintf(stdout, "Error: no parse\n\n");
            continue;
        }

        ES_Context *context = runtime.CreateContext(NULL);
        runtime.PushProgram(context, program);

        // eval
        do
        {
            eval_result = runtime.ExecuteContext(context, TRUE);
        }
        while (eval_result == ES_SUSPENDED);

        fprintf(stdout, "==> ");
        // print
        switch(eval_result)
        {
            case ES_NORMAL:
            case ES_NORMAL_AFTER_VALUE:
                {
                    ES_Value return_value;
                    runtime.GetReturnValue(context, &return_value);
                    switch(return_value.type)
                    {
                        case VALUE_UNDEFINED: break;
                        case VALUE_NULL: fprintf(stdout, "null"); break;
                        case VALUE_BOOLEAN: fprintf(stdout, return_value.value.boolean ? "true" : "false"); break;
                        case VALUE_NUMBER: fprintf(stdout, "%g", return_value.value.number); break;
                        case VALUE_STRING:
                        case VALUE_STRING_WITH_LENGTH:
                            {
                                OpString8 string8;
                                if (return_value.type == VALUE_STRING)
                                    string8.Set(return_value.value.string);
                                else
                                {
                                    ES_ValueString *str = return_value.value.string_with_length;
                                    string8.Set(str->string, str->length);
                                }
                                fprintf(stdout, "%s", string8.CStr());
                            }
                            break;
                        case VALUE_OBJECT: fprintf(stdout, "[Object]"); break;
                    }
                    break;
                }
            case ES_ERROR_NO_MEMORY: fprintf(stdout, "Out of memory\n"); brk = TRUE; break;
            case ES_BLOCKED: fprintf(stdout, "Blocked!\n"); brk = TRUE; break;
            case ES_ERROR:
            case ES_THREW_EXCEPTION: fprintf(stdout, "\n"); break;
        }
        fprintf(stdout, "\n");
        runtime.DeleteContext(context);
        runtime.DeleteProgram(program);

        if ( brk )
            break;
    }
    runtime.Detach();
    if ( eval_result != ES_NORMAL )
        return FALSE;
    return TRUE;
}

static BOOL runOnce(const JsShellOptions &opt, TempBuffer *t, int num_input_files, const char **input_args, BOOL *is_expr, EcmaScript_Object *global_object_shadow)
{
    ES_ProgramText *program_text = new ES_ProgramText[num_input_files];
    for (int i = 0; i < num_input_files; i++)
    {
        program_text[i].program_text = t[i].GetStorage();
        program_text[i].program_text_length = t[i].Length();
    }
    ES_Program *program = NULL;
    ES_Runtime *runtime = OP_NEW(ES_Runtime, ());
    runtime->Construct(global_object_shadow, "global", g_native_dispatcher);

    static_cast<JSShell_Object *>(global_object_shadow)->InitializeL();

    StopWatch compilation_stop_watch;
    compilation_stop_watch.Start();

    ES_Runtime::CompileProgramOptions options;
    options.reformat_source = opt.reformat;
    options.error_callback = HandleCompilationError;

    OP_STATUS ret = runtime->CompileProgram(program_text, num_input_files, &program, options);
    compilation_stop_watch.Stop();

    delete[] program_text;

    if (OpStatus::IsError(ret))
    {
        runtime->Detach();
        return 1;
    }

    if (opt.force_cached)
    {
        ES_Static_Program *static_program;

        runtime->ExtractStaticProgram(static_program, program);
        runtime->DeleteProgram(program);
        runtime->CreateProgramFromStatic(program, static_program);
        runtime->DeleteStaticProgram(static_program);
    }

    ES_Context *context = runtime->CreateContext(NULL);

    runtime->PushProgram(context, program);

#ifdef ES_BYTECODE_DEBUGGER
    if (opt.debug)
        static_cast<ES_Execution_Context *> (context)->EnableBytecodeDebugger();
#endif // ES_BYTECODE_DEBUGGER

    if (opt.disassembleBefore)
    {
#ifdef ES_DISASSEMBLER_SUPPORT
        ES_Disassembler disassembler(context);

        disassembler.Disassemble(program->program);

        unsigned length = disassembler.GetLength();

        if (length != 0)
        {
            const uni_char *unicode = disassembler.GetOutput();
            char *ascii = new char[length + 1];

            for (unsigned index = 0; index < length + 1; ++index)
                ascii[index] = (char) unicode[index];

            fprintf(stdout, "--------------------------------------------------------------------------------\n%s--------------------------------------------------------------------------------\n", ascii);
            delete[] ascii;
        }
#endif // ES_DISASSEMBLER_SUPPORT
    }

    StopWatch execution_stop_watch;
    execution_stop_watch.Start();

    ES_Eval_Status eval_result;
    do
    {
        eval_result = runtime->ExecuteContext(context);
    }
    while (eval_result == ES_SUSPENDED);

    execution_stop_watch.Stop();

    runtime->GetHeap()->ForceCollect(context, GC_REASON_SHUTDOWN);

    if (opt.disassembleAfter)
    {
#ifdef ES_DISASSEMBLER_SUPPORT
        ES_Disassembler disassembler(context);

        disassembler.Disassemble(program->program);

        unsigned length = disassembler.GetLength();

        if (length != 0)
        {
            const uni_char *unicode = disassembler.GetOutput();
            char *ascii = new char[length + 1];

            for (unsigned index = 0; index < length + 1; ++index)
                ascii[index] = (char) unicode[index];

            fprintf(stdout, "--------------------------------------------------------------------------------\n%s--------------------------------------------------------------------------------\n", ascii);
            delete[] ascii;
        }
#endif // ES_DISASSEMBLER_SUPPORT
    }

#ifdef ES_SLOW_CASE_PROFILING
    if (g_slow_case_summary)
    {
        for (unsigned i = 0; i < ESI_LAST_INSTRUCTION; ++i)
            if (context->rt_data->slow_case_calls[i] != 0)
                printf("%s: %d\n", g_instruction_strings[i], context->rt_data->slow_case_calls[i]);
    }
#endif // ES_SLOW_CASE_PROFILING

    if (!opt.quiet && !opt.benchmark || opt.disassembleAfter)
        fprintf(stdout, "--------------------------------------------------------------------------------\n");
    if (!opt.quiet)
    {
        double time = compilation_stop_watch.GetElapsedTime();
        fprintf( stdout, "Compilation time: %f ms\n", time);
        time = execution_stop_watch.GetElapsedTime();
        fprintf( stdout, "Execution time: %f ms\n", time);

        if (!opt.benchmark)
        {
            fprintf( stdout, "Major collection      : %.1fms (%.1f%%)\n", GC_DDATA.ms_major_collecting, GC_DDATA.ms_major_collecting * 100.0 / time);
            fprintf( stdout, "  Tracing             : %.1fms (%.1f%%)\n", GC_DDATA.ms_major_tracing, GC_DDATA.ms_major_tracing * 100.0 / time);
            fprintf( stdout, "  Sweeping            : %.1fms (%.1f%%)\n", GC_DDATA.ms_major_sweeping, GC_DDATA.ms_major_sweeping * 100.0 / time);
            fprintf( stdout, "  Number              : %lu\n", GC_DDATA.num_major_collections );
            fprintf( stdout, "Minor collection      : %.1fms (%.1f%%)\n", GC_DDATA.ms_minor_collecting, GC_DDATA.ms_minor_collecting * 100.0 / time);
            fprintf( stdout, "  Tracing             : %.1fms (%.1f%%)\n", GC_DDATA.ms_minor_tracing, GC_DDATA.ms_minor_tracing * 100.0 / time);
            fprintf( stdout, "  Sweeping            : %.1fms (%.1f%%)\n", GC_DDATA.ms_minor_sweeping, GC_DDATA.ms_minor_sweeping * 100.0 / time);
            fprintf( stdout, "  Number              : %lu\n", GC_DDATA.num_minor_collections );
            fprintf( stdout, "Total in collection   : %.1fms (%.1f%%)\n", GC_DDATA.ms_major_collecting + GC_DDATA.ms_minor_collecting, (GC_DDATA.ms_major_collecting + GC_DDATA.ms_minor_collecting) * 100.0 / time);
            fprintf( stdout, "Total bytes allocated : %lu\n", GC_DDATA.bytes_allocated + GC_DDATA.bytes_allocated_external );
            fprintf( stdout, "Peak bytes allocated  : %lu\n", GC_DDATA.bytes_in_heap_peak );
            fprintf( stdout, "Total external bytes  : %lu\n", GC_DDATA.bytes_allocated_external );
        }
#ifdef ES_RECORD_PEAK_STACK
        fprintf( stdout, "Peak stack size       : %i bytes\n", static_cast<OpPseudoThread*>(static_cast<ES_Execution_Context*>(context))->GetPeakStackSize() );
        fprintf( stdout, "Total stack size      : %i bytes\n", static_cast<OpPseudoThread*>(static_cast<ES_Execution_Context*>(context))->GetStackSize() );
#endif

#ifdef _DEBUG
        extern unsigned g_instructions_executed;
        fprintf( stdout, "Instructions executed : %u\n", g_instructions_executed );

#ifdef ES_NATIVE_SUPPORT
#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
        if (opt.native_dispatcher)
        {
            extern unsigned g_total_slow_case_calls;
            extern unsigned g_total_native_recompiles;

            fprintf( stdout, "Slow case calls       : %u\nRecompiled dispatchers: %u\n", g_total_slow_case_calls, g_total_native_recompiles );
        }
#endif // ES_DEBUG_NATIVE_BEHAVIOUR
#endif // ES_NATIVE_SUPPORT
#endif // _DEBUG
    }

#ifdef ES_BYTECODE_LOGGER
    if (opt.log)
        static_cast<ES_Execution_Context *> (context)->GetLogger()->PrintFrequentInstructionSequences(20);
#endif // ES_BYTECODE_LOGGER

    if (!opt.quiet && !opt.benchmark)
        fprintf(stdout, "done!\n");

    runtime->DeleteContext(context);
    runtime->DeleteProgram(program);
    runtime->Detach();
    return eval_result == ES_ERROR;
}

#ifdef ECMASCRIPT_DEBUGGER
class JsShellDebugListener
    : public ES_DebugListener
{
public:
    ~JsShellDebugListener()
    {
    }

    virtual BOOL EnableDebugging(ES_Runtime *runtime) { return TRUE; }
    virtual void NewScript(ES_Runtime *runtime, ES_DebugScriptData *data) {}
    virtual void NewContext(ES_Runtime *runtime, ES_Context *context) {}
    virtual void EnterContext(ES_Runtime *runtime, ES_Context *context) {}
    virtual void LeaveContext(ES_Runtime *runtime, ES_Context *context) {}
    virtual void DestroyContext(ES_Runtime *runtime, ES_Context *context) {}
    virtual void DestroyRuntime(ES_Runtime *runtime) {}
    virtual void DestroyObject(ES_Object *object) {}
    virtual EventAction HandleEvent(EventType type, unsigned int script_guid, unsigned int line_no) { return ESACT_CONTINUE; }
    virtual void RuntimeCreated(ES_Runtime *runtime) {}
};
#endif // ECMASCRIPT_DEBUGGER

int main(int argc, char**argv)
{
    Opera opera;
    op_memset(&opera, 0, sizeof(Opera));
    g_opera = &opera;
    g_opera->InitL();
    g_op_system_info = new OpSystemInfo;
    g_op_time_info = new OpTimeInfo;

    JsShellOptions opt;

    BOOL run_in_sequence = FALSE;
    BOOL repl = FALSE;

    opt.quiet = FALSE;
    opt.reformat = FALSE;
#if defined(ES_BYTECODE_DEBUGGER) || defined(_DEBUG)
    opt.debug = FALSE;
#endif // ES_BYTECODE_DEBUGGER
#if defined(ECMASCRIPT_DEBUGGER)
    opt.es_debug = FALSE;
#endif // ES_BYTECODE_DEBUGGER
#ifdef ES_BYTECODE_LOGGER
    opt.log = FALSE;
#endif // ES_BYTECODE_LOGGER
    opt.benchmark = FALSE;
    opt.disassembleBefore = opt.disassembleAfter = FALSE;
#ifdef ES_NATIVE_SUPPORT
    opt.native_dispatcher = FALSE;
#endif // ES_NATIVE_SUPPORT
    opt.force_cached = FALSE;
    int num_input_files = 0;
    const char **input_files = new const char*[argc];
    BOOL *is_expr = new BOOL[argc], next_is_expr = FALSE;
    char *focused_functions = NULL;
    int mem_limit = 512;

    int arg=1;
    while ( arg < argc )
    {
        if (op_strcmp( argv[arg], "-r" ) == 0)
        {
            repl = TRUE;
            ++arg;
            continue;
        }

        if (op_strcmp( argv[arg], "-e" ) == 0)
        {
            next_is_expr = TRUE;
            ++arg;
            continue;
        }

        if (op_strcmp( argv[arg], "-q" ) == 0)
        {
            opt.quiet = TRUE;
            ++arg;
            continue;
        }

        if (op_strcmp( argv[arg], "-benchmark" ) == 0 || op_strcmp( argv[arg], "-b" ) == 0)
        {
            opt.benchmark = TRUE;
            ++arg;
            continue;
        }

        if (op_strcmp( argv[arg], "-s" ) == 0)
        {
            run_in_sequence = TRUE;
            ++arg;
            continue;
        }

        if (op_strcmp( argv[arg], "-reformat" ) == 0 || op_strcmp( argv[arg], "-f" ) == 0)
        {
            opt.reformat = TRUE;
            ++arg;
            continue;
        }

#ifdef ACOVEA
        if (op_strcmp( argv[arg], "-class-linear-growth" ) == 0)
        {
            g_class_linear_growth = op_atoi(argv[arg + 1]);
            arg += 2;
            continue;
        }

        if (op_strcmp( argv[arg], "-class-growth-rate" ) == 0)
        {
            g_class_growth_rate = op_atoi(argv[arg + 1]);
            arg += 2;
            continue;
        }

        if (op_strcmp( argv[arg], "-property-cache-size" ) == 0)
        {
            g_max_property_cache_size = op_atoi(argv[arg + 1]);
            arg += 2;
            continue;
        }
#endif

#ifdef ES_BYTECODE_DEBUGGER
        if (op_strcmp( argv[arg], "-debug" ) == 0 || op_strcmp( argv[arg], "-g" ) == 0)
        {
            opt.debug = TRUE;
            ++arg;
            continue;
        }
#endif // ES_BYTECODE_DEBUGGER

#ifdef ES_BYTECODE_LOGGER
        if (op_strcmp( argv[arg], "-log" ) == 0)
        {
            opt.log = TRUE;
            ++arg;
            continue;
        }
#endif // ES_BYTECODE_LOGGER

#ifdef ES_HARDCORE_GC_MODE
        if (op_strcmp( argv[arg], "-hardcore-gc" ) == 0 || op_strcmp( argv[arg], "-gc" ) == 0)
        {
            g_hardcode_gc_mode = TRUE;
            ++arg;
            continue;
        }
#endif // ES_HARDCORE_GC_MODE

#ifdef ECMASCRIPT_DEBUGGER
        if (op_strcmp( argv[arg], "-es-debug" ) == 0)
        {
            opt.es_debug = TRUE;
            ++arg;
            continue;
        }
#endif // ECMASCRIPT_DEBUGGER

        if (op_strcmp( argv[arg], "-disassembleBefore" ) == 0 || op_strcmp( argv[arg], "-D" ) == 0)
        {
            opt.disassembleBefore = TRUE;
            ++arg;
            continue;
        }

        if (op_strcmp( argv[arg], "-disassemble" ) == 0 || op_strcmp( argv[arg], "-d" ) == 0)
        {
            opt.disassembleAfter = TRUE;
            ++arg;
            continue;
        }

        if (op_strcmp( argv[arg], "-disassembleEval" ) == 0 || op_strcmp( argv[arg], "-de" ) == 0)
        {
            g_disassemble_eval = TRUE;
            ++arg;
            continue;
        }

        if (op_strcmp( argv[arg], "-countSlowCases" ) == 0 || op_strcmp( argv[arg], "-csc" ) == 0)
        {
            g_slow_case_summary = TRUE;
            ++arg;
            continue;
        }

#ifdef NATIVE_DISASSEMBLER_SUPPORT
        if (op_strncmp( argv[arg], "-format-intel", op_strlen("-format-intel") ) == 0)
        {
            if (g_native_disassembler_format == ATT)
                goto usage;
            g_native_disassembler_format = INTEL;
            ++arg;
            continue;
        }

        if (op_strncmp( argv[arg], "-format-att", op_strlen("-format-att") ) == 0)
        {
            if (g_native_disassembler_format == INTEL)
                goto usage;
            g_native_disassembler_format = ATT;
            ++arg;
            continue;
        }


        if (op_strncmp( argv[arg], "-native-disassemble", op_strlen("-native-disassemble") ) == 0
            || op_strncmp( argv[arg], "-nd", op_strlen("-nd") ) == 0)
        {
            if (op_strncmp( argv[arg], "-native-disassemble=", op_strlen("-native-disassemble=")) == 0)
                g_native_disassembler_file = fopen(argv[arg] + op_strlen("-native-disassemble="), "wct");
            else if (op_strncmp( argv[arg], "-nd=", op_strlen("-nd=")) == 0)
                g_native_disassembler_file = fopen(argv[arg] + op_strlen("-nd="), "wct");
            else
                g_native_disassembler_file = stdout;
            ++arg;
            continue;
        }

#endif // NATIVE_DISASSEMBLER_SUPPORT

#ifdef ES_NATIVE_SUPPORT
        if (op_strcmp( argv[arg], "-native-dispatcher" ) == 0
            || op_strcmp( argv[arg], "-np" ) == 0)
        {
            opt.native_dispatcher = TRUE;
            g_native_dispatcher = TRUE;
            ++arg;
            continue;
        }

#ifdef ES_DEBUG_COMPACT_OBJECTS
        if (op_strcmp( argv[arg], "-debug-compact-objects" ) == 0
            || op_strcmp( argv[arg], "-dco" ) == 0)
        {
            g_debug_type_changes = TRUE;
            ++arg;
            continue;
        }
#endif // ES_DEBUG_COMPACT_OBJECTS

#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
        if (op_strcmp( argv[arg], "-debug-native-behaviour" ) == 0
            || op_strcmp( argv[arg], "-dnb" ) == 0)
        {
            g_debug_native_behaviour = TRUE;
            ++arg;
            continue;
        }

        if (op_strcmp( argv[arg], "-disassemble-loop-dispatcher" ) == 0 || op_strcmp( argv[arg], "-dld" ) == 0)
        {
            g_disassemble_loop_dispatcher = TRUE;
            ++arg;
            continue;
        }
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

        if (op_strcmp( argv[arg], "-fpmode" ) == 0)
        {
            if (op_strcmp( argv[arg + 1], "x87") == 0)
                g_force_fpmode_x87 = TRUE;
            else if (op_strcmp( argv[arg + 1], "sse2") != 0)
            {
                fprintf( stderr, "-fpmode: possible values are 'x87' and 'sse2'\n");
                return 1;
            }
            arg += 2;
            continue;
        }
#endif // ES_NATIVE_SUPPORT

        if (op_strcmp( argv[arg], "-F") == 0 || op_strcmp( argv[arg], "-focus") == 0)
        {
            focused_functions = argv[arg + 1];
            arg += 2;
            continue;
        }

        if (op_strcmp( argv[arg], "-mem" ) == 0)
        {
            mem_limit = op_atoi(argv[arg + 1]);
            arg += 2;
            continue;
        }

        if (op_strcmp( argv[arg], "-force-cached" ) == 0
            || op_strcmp( argv[arg], "-fc" ) == 0)
        {
            opt.force_cached = TRUE;
            ++arg;
            continue;
        }

        is_expr[num_input_files] = next_is_expr;
        next_is_expr = FALSE;

        input_files[num_input_files++] = argv[arg++];
    }

    if (!opt.quiet && !opt.benchmark)
    {
        fputs("Opera JavaScript shell (" ES_STANDALONE_VERSION ", built " ES_STANDALONE_BUILDTIME ")\n", stdout);
        fflush(stdout);
    }

    if (!repl && num_input_files <= 0)
    {
usage:
        fprintf( stderr, "Usage: jsshell [options] filename [filenames]\n" );
        fprintf( stderr, "       jsshell [options] \"expression\"\n" );
        fprintf( stderr, "       jsshell -r\n\n" );
        fprintf( stderr, "Options:\n" );
        fprintf( stderr, "   -r                Start the read-eval-print loop.\n" );
        fprintf( stderr, "   -e                Use expression instead of file.\n" );
        fprintf( stderr, "   -q                Suppress banners etc.\n" );
        fprintf( stderr, "   -s                Run given files in sequence.\n" );
        fprintf( stderr, "   -b -benchmark     Like -q but display execution time.\n" );
        fprintf( stderr, "   -f -reformat      Enable source reformatting.\n" );
#ifdef ES_BYTECODE_DEBUGGER
        fprintf( stderr, "   -g -debug         Enable debugger.\n" );
#endif // ES_BYTECODE_DEBUGGER
#ifdef ES_BYTECODE_LOGGER
        fprintf( stderr, "   -log              Log bytecode bigrams.\n" );
#endif // ES_BYTECODE_LOGGER
#ifdef ES_HARDCORE_GC_MODE
        fprintf( stderr, "   -gc -hardcore-gc  Garbage collect at each allocation.\n" );
#endif // ES_HARDCORE_GC_MODE
        fprintf( stderr, "   -d -disassemble   Show disassembled bytecode.\n" );
#ifdef NATIVE_DISASSEMBLER_SUPPORT
        fprintf( stderr, "   -nd[=file] -native-disassemble[=file]\n" );
#endif // NATIVE_DISASSEMBLER_SUPPORT
#ifdef ES_NATIVE_SUPPORT
        fprintf( stderr, "   -np -native-dispatcher\n" );
#endif // ES_NATIVE_SUPPORT
        return 1;
    }

#ifdef UNIX
    struct rlimit data_limit;

    if (mem_limit > 0 && getrlimit(RLIMIT_AS, &data_limit) == 0)
    {
        data_limit.rlim_cur = 1024 * 1024 * mem_limit;
        if (setrlimit(RLIMIT_AS, &data_limit) != 0)
        {
            if (!opt.quiet)
                perror("setrlimit failed");
        }
        else if (!opt.quiet)
            fprintf( stdout, "Setting address space limit to %d MB\n", mem_limit );
    }
#endif // UNIX

    if (!opt.quiet)
        fprintf( stdout, "\n" );

    if (focused_functions)
    {
        unsigned count = 0;
        char *s = focused_functions, *e = s;

        while (*e)
        {
            while (*e == ',') ++e;
            ++count;
            while (*e && *e != ',') ++e;
        }

        g_focused_functions = OP_NEWA(uni_char *, count + 1);

        unsigned index = 0;
        e = s;
        while (index < count)
        {
            while (*e && *e != ',') ++e;
            if (*e == ',') *e = 0;
            g_focused_functions[index++] = uni_up_strdup(s);
            s = e + 1;
        }

        g_focused_functions[count] = NULL;
    }

    BOOL ret_val = FALSE;

    EcmaScript_Object *global_object_shadow = new JSShell_Object();

#ifdef ECMASCRIPT_DEBUGGER
    JsShellDebugListener debug_listener;
    if (opt.es_debug && !opt.native_dispatcher)
        g_ecmaManager->SetDebugListener(&debug_listener);
#endif // ECMASCRIPT_DEBUGGER

    if (repl)
    {
        ret_val = runRepl(opt, global_object_shadow);
    }
    else if (run_in_sequence)
    {
        // What to do?
        // Loop over the input_files.
        for (int i = 0; i < num_input_files; i++)
        {
            if (!opt.quiet)
                fprintf( stdout, "Running: %s\n", input_files[i] );

            TempBuffer t;
            OP_STATUS res;
            if (is_expr[i])
                t.Append(input_files[i]);
            else if (OpStatus::IsError(res = read_file(input_files[i], &t)))
            {
                if (res == OpStatus::ERR_FILE_NOT_FOUND)
                    fprintf( stderr, "Can't open %s\n", input_files[i] );
                fprintf( stderr, "ERROR: Can't  read file\n" );
                goto usage;
            }
            ret_val = runOnce(opt, &t, 1, input_files, is_expr, global_object_shadow) || ret_val;
        }
    }
    else
    {
        TempBuffer *t = new TempBuffer[num_input_files];
        for (int i = 0; i < num_input_files; i++)
        {
            OP_STATUS res;
            if (is_expr[i])
                t[i].Append(input_files[i]);
            else if (OpStatus::IsError(res = read_file(input_files[i], &t[i])))
            {
                if (res == OpStatus::ERR_FILE_NOT_FOUND)
                    fprintf( stderr, "Can't open %s\n", input_files[i] );
                fprintf( stderr, "ERROR: Can't  read file\n" );
                delete [] t;
                goto usage;
            }
        }

        ret_val = runOnce(opt, t, num_input_files, input_files, is_expr, global_object_shadow);
        delete [] t;
    }

    delete global_object_shadow;
    delete[] is_expr;
    delete[] input_files;
    delete g_op_system_info;
    g_op_system_info = NULL;
    g_opera->Destroy();

    return ret_val ? 1 : 0;
}

#ifdef WINGOGI
void *
WINGOGI_VirtualAlloc(size_t s, unsigned long t, unsigned long p)
{
    return VirtualAlloc(NULL, s, t, p);
}

void
WINGOGI_VirtualFree(void *p, size_t s)
{
    VirtualFree(p, s, MEM_RELEASE);
}

void
WINGOGI_VirtualProtect(void *m, size_t s, unsigned long p, unsigned long *op)
{
	VirtualProtect(m, s, p, op);
}
#endif//WINGOGI
