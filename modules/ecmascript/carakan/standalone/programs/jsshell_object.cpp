/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "programs/jsshell_object.h"
#include <stdio.h>
#include "modules/util/opstring.h"

static void AddFunctionToObject(ES_Runtime* runtime, ES_Object *this_object, Function *function, const uni_char *name, const char *format_list, unsigned flags = 0)
{
    ES_Value value;
    value.type = VALUE_OBJECT;

    BuiltinFunction *fn = new BuiltinFunction(function);
    fn->SetFunctionRuntimeL(runtime, name, NULL, format_list);
    runtime->Protect(*fn);
    value.value.object = *fn;
    runtime->PutName(this_object, name, value, flags);
    runtime->Unprotect(*fn);
}

/* virtual */ int
BuiltinFunction::Call(ES_Object* this_native_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    BuiltinObject *this_object = (BuiltinObject *)ES_Runtime::GetHostObject(this_native_object);

    if (this_object && !this_object->SecurityCheck(origining_runtime))
        return ES_EXCEPT_SECURITY;

    return function(this_object, argv, argc, return_value, origining_runtime);
}

/* virtual */
JSShell_Object::~JSShell_Object()
{

}

void
JSShell_Object::AddFunction(Function *function, const uni_char *name, const char *format_list, unsigned flags)
{
    AddFunctionToObject(GetRuntime(), GetNativeObject(), function, name, format_list, flags);
}

/* virtual  */ void
JSShell_Object::InitializeL()
{
    ES_Value value;
    value.type = VALUE_OBJECT;

    AddFunction(JSShell_Object::write, UNI_L("write"), "s", PROP_DONT_ENUM);
    AddFunction(JSShell_Object::writeln, UNI_L("writeln"), "s", PROP_DONT_ENUM);
    AddFunction(JSShell_Object::writeln, UNI_L("print"), "s", PROP_DONT_ENUM);
    AddFunction(JSShell_Object::writeln, UNI_L("alert"), "s", PROP_DONT_ENUM);
    AddFunction(JSShell_Object::getPropertyNames, UNI_L("__getPropertyNames__"), "-", PROP_DONT_ENUM);

    AddFunction(JSShell_Object::quit, UNI_L("quit"), "-", PROP_DONT_ENUM | PROP_DONT_DELETE);
    AddFunction(JSShell_Object::load, UNI_L("load"), "s", PROP_DONT_ENUM);
    AddFunction(JSShell_Object::readline, UNI_L("readline"), "-", PROP_DONT_ENUM);

    AddFunction(JSShell_Object::loadFileIntoString, UNI_L("loadFileIntoString"), "s", PROP_DONT_ENUM);
    AddFunction(JSShell_Object::saveStringAsFile, UNI_L("saveStringAsFile"), "ss", PROP_DONT_ENUM);
    AddFunction(JSShell_Object::appendStringToFile, UNI_L("appendStringToFile"), "ss", PROP_DONT_ENUM);
#ifdef ES_DISASSEMBLER_SUPPORT
    AddFunction(JSShell_Object::disassemble, UNI_L("disassemble"), "o", PROP_DONT_ENUM);
#endif
#ifdef DEBUG_ENABLE_OPASSERT
#ifdef ES_DEBUG_COMPACT_OBJECTS
    AddFunction(JSShell_Object::printClass, UNI_L("printClass"), "o", PROP_DONT_ENUM);
    AddFunction(JSShell_Object::printRootClass, UNI_L("printRootClass"), "o", PROP_DONT_ENUM);
#endif
    AddFunction(JSShell_Object::trap, UNI_L("trap"), "b", PROP_DONT_ENUM);
#endif // DEBUG_ENABLE_OPASSERT

    HostArray_Constructor *HostArray = OP_NEW(HostArray_Constructor, ());
    HostArray->SetFunctionRuntimeL(GetRuntime(), UNI_L("HostArray"), "Function", NULL);
    value.value.object = *HostArray;
    GetRuntime()->PutName(GetNativeObject(), UNI_L("HostArray"), value, PROP_DONT_ENUM);

    Phase2_Constructor *Phase2 = OP_NEW(Phase2_Constructor, ());
    Phase2->SetFunctionRuntimeL(GetRuntime(), UNI_L("Phase2"), "Function", "n-");
    value.value.object = *Phase2;
    GetRuntime()->PutName(GetNativeObject(), UNI_L("Phase2"), value, PROP_DONT_ENUM);

    HostObject_Constructor *Host = OP_NEW(HostObject_Constructor, ());
    Host->SetFunctionRuntimeL(GetRuntime(), UNI_L("HostObject"), "Function", NULL);
    value.value.object = *Host;
    GetRuntime()->PutName(GetNativeObject(), UNI_L("HostObject"), value, PROP_DONT_ENUM);

}

/* static */ int
JSShell_Object::write(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    for (unsigned argi = 0; argi < (unsigned)argc; ++argi)
    {
        if (argv[argi].type != VALUE_STRING && argv[argi].type != VALUE_STRING_WITH_LENGTH)
            return ES_FAILED;

        OpString8 string8;
        if (argv[argi].type == VALUE_STRING)
            string8.Set(argv[argi].value.string);
        else
        {
            ES_ValueString *str = argv[argi].value.string_with_length;
            string8.Set(str->string, str->length);
        }

        printf("%s", string8.CStr());
    }

    return_value->type = VALUE_UNDEFINED;
    return_value->value.object = NULL;
    return ES_FAILED;
}

/* static */ int
JSShell_Object::writeln(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    for (unsigned argi = 0; argi < (unsigned)argc; ++argi)
    {
        if (argv[argi].type != VALUE_STRING && argv[argi].type != VALUE_STRING_WITH_LENGTH)
            return ES_FAILED;

        OpString8 string8;
        if (argv[argi].type == VALUE_STRING)
            string8.Set(argv[argi].value.string);
        else
        {
            ES_ValueString *str = argv[argi].value.string_with_length;
            string8.Set(str->string, str->length);
        }

        printf("%s\n", string8.CStr());
    }

    return_value->type = VALUE_UNDEFINED;
    return_value->value.object = NULL;
    return ES_FAILED;
}

/* static */ int
JSShell_Object::getPropertyNames(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    ES_Runtime *runtime = this_object->GetRuntime();

    if (argc < 1)
        return ES_FAILED;

    if (argv[0].type != VALUE_OBJECT)
        return ES_FAILED;

    ES_Object *object = argv[0].value.object;

    ES_Object *array;
    if (OpStatus::IsError(runtime->CreateNativeArrayObject(&array)))
        return ES_NO_MEMORY;

    return_value->type = VALUE_OBJECT;
    return_value->value.object = array;

    OpVector<OpString> names;

    if (OpStatus::IsError(runtime->GetPropertyNames(object, names)))
    {
        names.DeleteAll();
        return ES_NO_MEMORY;
    }

    ES_Value v;
    ES_ValueString string;
    v.type = VALUE_STRING_WITH_LENGTH;
    v.value.string_with_length = &string;
    for (int i = (int)names.GetCount(), j = 0; j < i; ++j)
    {
        OpString *name = names.Get(j);
        string.string = name->CStr();
        string.length = name->Length();

        if (OpStatus::IsError(runtime->PutIndex(array, j, v)))
        {
            names.DeleteAll();
            return ES_NO_MEMORY;
        }
    }

    names.DeleteAll();
    return ES_VALUE;
}

/* static */ int
JSShell_Object::quit(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    exit(0);
    return ES_FAILED;
}

/* static */ int
JSShell_Object::load(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    if (argc < 1)
        return ES_FAILED;

    ES_Eval_Status eval_result = ES_ERROR;
    return_value->type = VALUE_NULL;

    ES_ProgramText *text = NULL;
    TempBuffer *t = OP_NEWA(TempBuffer, argc);
    ES_Context *context;
    int iterations = 100;
    OP_STATUS ret;
    ES_ErrorInfo err(UNI_L("while parsing script"));
    ES_Program *program = NULL;
    ES_Runtime *runtime = this_object->GetRuntime();
    OpString8 string8;

    if (!t)
    {
        eval_result = ES_ERROR_NO_MEMORY;
        goto cleanup_return;
    }

    for (int i = 0; i < argc; i++)
    {
        if (argv[i].type == VALUE_STRING)
            string8.Set(argv[i].value.string);

        if (OpStatus::IsError(read_file(string8.CStr(), &t[i])))
        {
            eval_result = ES_THREW_EXCEPTION;
            goto cleanup_return;
        }
    }

    text = OP_NEWA(ES_ProgramText, argc);
    if (!text)
    {
        eval_result = ES_ERROR_NO_MEMORY;
        goto cleanup_return;
    }

    for (int i = 0; i < argc; i++)
    {
        text[i].program_text = t[i].GetStorage();
        text[i].program_text_length = t[i].Length();
    }

#if 0
    StopWatch sw;
    startTimer(&sw);
#endif

    ret = runtime->CompileProgram(text, argc, &program, ES_Runtime::CompileProgramOptions());

    if (OpStatus::IsError(ret))
    {
        eval_result = ES_THREW_EXCEPTION;
        goto cleanup_return;
    }

    context = runtime->CreateContext(NULL);
    runtime->PushProgram(context, program);

    do
    {
        eval_result = runtime->ExecuteContext(context, iterations);
    }
    while (eval_result == ES_SUSPENDED);

#if 0
    stopTimer(&sw);
    double dt = getElapsedTime(&sw);
    fprintf(stderr, "Parse + eval: %g\n", dt);
#endif

    runtime->DeleteContext(context);
    runtime->DeleteProgram(program);

cleanup_return:
    OP_DELETEA(text);
    OP_DELETEA(t);

    ES_Host_Call_Status status = ES_FAILED;
    switch(eval_result)
    {
        case ES_NORMAL: status = ES_FAILED; break;
        case ES_NORMAL_AFTER_VALUE: status = ES_VALUE; break;
        case ES_ERROR: status = ES_EXCEPTION; break;
        case ES_ERROR_NO_MEMORY: status = ES_NO_MEMORY; break;
        case ES_THREW_EXCEPTION: status = ES_EXCEPTION; break;
        default: OP_ASSERT(!"Help me! I'm trapped in an ecmascript engine"); break;
    }

    return status;
}

/* static */ int
JSShell_Object::readline(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    unsigned const max_line_length = 4000;
    ES_Host_Call_Status status = ES_FAILED;
    char *line = OP_NEWA(char, max_line_length);
    uni_char *uni_line;
    OpString16 string;

    size_t length;
    if (fgets(line, max_line_length, stdin) == NULL)
        goto cleanup_return;

    length = strlen(line);

    if (length >= max_line_length - 1 || OpStatus::IsError(string.Set(line)))
    {
        status = ES_NO_MEMORY;
        goto cleanup_return;
    }

    uni_line = uni_strdup(string.CStr());
    if (uni_line != NULL)
    {
        return_value->type = VALUE_STRING;
        uni_line[length-1] = '\0';
        return_value->value.string = uni_line;
        status = ES_VALUE;
    }
    else
        status = ES_NO_MEMORY;

cleanup_return:
    OP_DELETEA(line);

    return status;
}

int  /** loadFileIntoString('<file-name>') --> JString (each byte of the file contents is converted to uni_char) */
JSShell_Object::loadFileIntoString(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    if (argc != 1 || !(argv[0].type == VALUE_STRING || argv[0].type == VALUE_STRING_WITH_LENGTH))
        return ES_FAILED;

    OpString8 fname;
    if (argv[0].type == VALUE_STRING)
        fname.Set(argv[0].value.string);
    else
        fname.Set(argv[0].value.string_with_length->string, argv[0].value.string_with_length->length);

    FILE *f = fopen(fname.CStr(), "rb");
    if (!f)
        return ES_FAILED;

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return ES_FAILED;
    }

    unsigned length = ftell(f);
    rewind(f);

    ES_ValueString* vs = OP_NEW(ES_ValueString, ());
    uni_char* strbuf = OP_NEWA(uni_char, length + 1);
    vs->string = strbuf;
    unsigned i;
    for (i = 0; i < length; i++)
    {
        strbuf[i] = getc(f);
    }
    fclose(f);

    strbuf[i] = 0;
    vs->string = strbuf;
    vs->length = length;

    return_value->type = VALUE_STRING_WITH_LENGTH;
    return_value->value.string_with_length = vs;

    return ES_VALUE;
}

int  /** saveStringAsFile('<file-name>', '<str-content>') (each uni_char in the str should have codebase < 0xff and is converted to a byte in the file) */
JSShell_Object::saveStringAsFile(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    if (argc != 2 || !(argv[0].type == VALUE_STRING || argv[0].type == VALUE_STRING_WITH_LENGTH)
                  || !(argv[1].type == VALUE_STRING || argv[1].type == VALUE_STRING_WITH_LENGTH))
        return ES_FAILED;

    OpString8 fname, content;
    if (argv[0].type == VALUE_STRING)
        fname.Set(argv[0].value.string);
    else
        fname.Set(argv[0].value.string_with_length->string, argv[0].value.string_with_length->length);

    if (argv[1].type == VALUE_STRING)
        content.Set(argv[1].value.string);
    else
        content.Set(argv[1].value.string_with_length->string, argv[1].value.string_with_length->length);

    FILE* file = fopen(fname.CStr(), "wb");
    if (file)
    {
        fwrite(content.CStr(), content.Length(), 1, file);

        fclose(file);
    }
    return ES_FAILED;
}

int  /** appendStringToFile('<file-name>', '<str-to-append>') (each uni_char in the str should have codebase < 0xff and is converted to a byte in the file) */
JSShell_Object::appendStringToFile(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    if (argc != 2 || !(argv[0].type == VALUE_STRING || argv[0].type == VALUE_STRING_WITH_LENGTH)
                  || !(argv[1].type == VALUE_STRING || argv[1].type == VALUE_STRING_WITH_LENGTH))
        return ES_FAILED;

    OpString8 fname, content;
    if (argv[0].type == VALUE_STRING)
        fname.Set(argv[0].value.string);
    else
        fname.Set(argv[0].value.string_with_length->string, argv[0].value.string_with_length->length);

    if (argv[1].type == VALUE_STRING)
        content.Set(argv[1].value.string);
    else
        content.Set(argv[1].value.string_with_length->string, argv[1].value.string_with_length->length);

    FILE* file = fopen(fname.CStr(), "a+b");
    if (file)
    {
        if (0 == fseek(file, 0, SEEK_END))
            fwrite(content.CStr(), content.Length(), 1, file);
        fclose(file);
    }
    return ES_FAILED;
}

#ifdef ES_DISASSEMBLER_SUPPORT
int  /** disassemble('<function>') print disassembled function */
JSShell_Object::disassemble(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    if (argc != 1 || argv[0].type != VALUE_OBJECT || strcmp(ES_Runtime::GetClass(argv[0].value.object), "Function"))
        return ES_FAILED;

    ES_Context *context = origining_runtime->CreateContext(origining_runtime->GetGlobalObject());

    extern void ES_DisassembleFromHostCode(ES_Context *, ES_FunctionCode *);
    ES_DisassembleFromHostCode(context, static_cast<ES_Function*>(argv[0].value.object)->GetFunctionCode());

    origining_runtime->DeleteContext(context);

    return ES_FAILED;
}
#endif // ES_DISASSEMBLER_SUPPORT

#ifdef DEBUG_ENABLE_OPASSERT
# ifdef UNIX
#  include <signal.h>
# endif // UNIX
int  /** trap('<boolean>') break if argument is true*/
JSShell_Object::trap(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    if (argc > 1 || (argc == 1 && (argv[0].type != VALUE_BOOLEAN)))
        return ES_FAILED;

    BOOL brk = TRUE;

    if (argc == 1)
        brk = argv[0].value.boolean;

    if (brk)
# ifdef UNIX
        raise(SIGTRAP);
# else
        assert(0);
# endif // UNIX

    return ES_FAILED;
}

#endif // DEBUG_ENABLE_OPASSERT

#ifdef ES_DEBUG_COMPACT_OBJECTS
int  /** printClass('<object>') print object's class */
JSShell_Object::printClass(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    if (argc != 1 || argv[0].type != VALUE_OBJECT)
        return ES_FAILED;

    ES_Context *context = origining_runtime->CreateContext(origining_runtime->GetGlobalObject());

    extern void ES_PrintClassNonSuspended(ES_Context *, ES_Class *, unsigned);
    ES_Object *object = static_cast<ES_Object*>(argv[0].value.object);
    ES_PrintClassNonSuspended(context, object->Class(), 0);

    origining_runtime->DeleteContext(context);

    return ES_FAILED;
}

int  /** printRootClass('<object>') print object's class */
JSShell_Object::printRootClass(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    if (argc != 1 || argv[0].type != VALUE_OBJECT)
        return ES_FAILED;

    ES_Context *context = origining_runtime->CreateContext(origining_runtime->GetGlobalObject());

    extern void ES_PrintClassNonSuspended(ES_Context *, ES_Class *, unsigned);
    ES_Object *object = static_cast<ES_Object*>(argv[0].value.object);
    ES_PrintClassNonSuspended(context, object->Class()->GetRootClass(), 0);

    origining_runtime->DeleteContext(context);

    return ES_FAILED;
}
#endif // ES_DEBUG_COMPACT_OBJECTS

/* virtual */ void
JSShell_Object::GCTrace()
{
    // nothing
}

/* virtual */
HostArray::~HostArray()
{
    OP_DELETEA(values);
}

/* virtual */ ES_GetState
HostArray::GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
    if (property_index >= 0 && static_cast<unsigned>(property_index) < length)
    {
        if (value)
            *value = values[property_index];
        return GET_SUCCESS;
    }
    else
        return GET_FAILED;
}

/* virtual */ ES_GetState
HostArray::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
    if (uni_str_eq(property_name, "length"))
    {
        if (value)
        {
            value->type = VALUE_NUMBER;
            value->value.number = length;
        }
        return GET_SUCCESS;
    }
    else
        return GET_FAILED;
}

/* virtual */ ES_PutState
HostArray::PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
    if (property_index >= 0)
    {
        if (static_cast<unsigned>(property_index) < length)
            values[property_index] = *value;
        else if (static_cast<unsigned>(property_index) < UINT_MAX)
        {
            unsigned new_length = static_cast<unsigned>(property_index) + 1;
            ES_Value *new_values = OP_NEWA(ES_Value, new_length);
            if (!new_values)
                    return PUT_NO_MEMORY;
            if (length > 0)
                op_memcpy(new_values, values, length * sizeof(ES_Value));
            if (new_length > length + 1)
                for (unsigned i = 0; i < (new_length -  length) - 1; i++)
                    new_values[length + i].type = VALUE_UNDEFINED;

            new_values[new_length - 1] = *value;
            OP_DELETEA(values);
            values = new_values;
            length = new_length;
        }
    }

    return PUT_SUCCESS;
}

/* virtual */ ES_PutState
HostArray::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
    if (uni_str_eq(property_name, "length") && value && value->type == VALUE_NUMBER)
    {
        double d = value->value.number;
        if (d >= 0 && d <= UINT_MAX)
        {
            unsigned new_length = static_cast<unsigned>(d);
            ES_Value *new_values = NULL;
            if (new_length > 0)
            {
                new_values = OP_NEWA(ES_Value, new_length);
                if (!new_values)
                    return PUT_NO_MEMORY;
                if (length > 0)
                    op_memcpy(new_values, values, MIN(length, new_length) * sizeof(ES_Value));
                if (new_length > length)
                    for (unsigned i = 0; i < (new_length -  length); i++)
                        new_values[length + i].type = VALUE_UNDEFINED;
            }
            OP_DELETEA(values);
            values = new_values;
            length = new_length;
        }
        return PUT_SUCCESS;
    }
    else
        return PUT_FAILED;
}

/* virtual */ ES_GetState
HostArray::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
    enumerator->AddPropertiesL(0, length);
    return GET_SUCCESS;
}

/* virtual */ ES_GetState
HostArray::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
    count = length;
    return GET_SUCCESS;
}

/* virtual */ ES_DeleteStatus
HostArray::DeleteIndex(int property_index, ES_Runtime* origining_runtime)
{
    if (property_index >= 0 && static_cast<unsigned>(property_index) < length)
        values[property_index].type = VALUE_UNDEFINED;

    return DELETE_OK;
}

/* virtual */ void
HostArray::GCTrace()
{
    for (unsigned index = 0; index < length; ++index)
        if (values[index].type == VALUE_OBJECT)
            GetRuntime()->GCMark(values[index].value.object);
}

/* virtual */ int
HostArray_Constructor::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    return Construct(argv, argc, return_value, origining_runtime);
}

/* virtual */ int
HostArray_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    ES_Value *values;
    unsigned length;

    if (argc == 1 && argv[0].type == VALUE_NUMBER && op_isintegral(argv[0].value.number) && argv[0].value.number >= 0 && argv[0].value.number <= UINT_MAX)
    {
        length = static_cast<unsigned>(argv[0].value.number);
        values = length != 0 ? OP_NEWA(ES_Value, length) : NULL;
    }
    else
    {
        length = argc;
        if (length != 0)
        {
            values = OP_NEWA(ES_Value, length);
            op_memcpy(values, argv, length * sizeof(ES_Value));
        }
        else
            values = NULL;
    }

    HostArray *array = OP_NEW(HostArray, (values, length));

    array->SetObjectRuntime(GetRuntime(), GetRuntime()->GetArrayPrototype(), "HostArray");

    return_value->type = VALUE_OBJECT;
    return_value->value.object = *array;

    return ES_VALUE;
}

/* virtual */ int
Phase2_Constructor::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    return Construct(argv, argc, return_value, origining_runtime);
}

/* virtual */ int
Phase2_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    EcmaScript_Object *object = OP_NEW(EcmaScript_Object, ());

    if (OpStatus::IsError(object->SetObjectRuntime(origining_runtime, origining_runtime->GetObjectPrototype(), "Phase2", TRUE)))
        return ES_NO_MEMORY;

    return_value->type = VALUE_OBJECT;
    return_value->value.object = *object;

    return ES_VALUE;
}

/* virtual */ ES_GetState
HostObject::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
    /* The 'alpha' property may be read&written + getter/setters defined over it. */
    if (uni_str_eq(property_name, "alpha"))
    {
        if (value)
        {
            value->type = VALUE_STRING;
            value->value.string = UNI_L("alpha");
        }
        return GET_SUCCESS;
    }
    else if (uni_str_eq(property_name, "beta"))
    {
        if (value)
        {
            value->type = VALUE_STRING;
            value->value.string = UNI_L("beta");
        }
        return GET_SUCCESS;
    }
    else if (uni_str_eq(property_name, "gamma"))
    {
        if (value)
        {
            value->type = VALUE_STRING;
            value->value.string = UNI_L("gamma");
        }
        return GET_SUCCESS;
    }
    else if (uni_str_eq(property_name, "delta"))
    {
        if (value)
        {
            value->type = VALUE_STRING;
            value->value.string = UNI_L("delta");
        }
        return GET_SUCCESS;
    }
    else if (uni_str_eq(property_name, "zeta"))
    {
        if (value)
        {
            value->type = VALUE_STRING;
            value->value.string = UNI_L("zeta");
        }
        return GET_SUCCESS;
    }
    else
        return GET_FAILED;
}

/* virtual */ ES_PutState
HostObject::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
    if (uni_str_eq(property_name, "alpha"))
        return PUT_SUCCESS;
    else if (uni_str_eq(property_name, "beta"))
        return PUT_SUCCESS;
    else if (uni_str_eq(property_name, "gamma"))
        return PUT_SUCCESS;
    else if (uni_str_eq(property_name, "epsilon"))
        return PUT_SUCCESS;
    else if (uni_str_eq(property_name, "zeta"))
        return PUT_SUCCESS;
    else
        return PUT_FAILED;
}

/* virtual */BOOL
HostObject::AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op)
{
    if (uni_str_eq(property_name, "alpha"))
        return TRUE;
    else if (uni_str_eq(property_name, "beta"))
        return TRUE;
    else if (uni_str_eq(property_name, "gamma"))
        return (op == EcmaScript_Object::ALLOW_NATIVE_OVERRIDE);
    else if (uni_str_eq(property_name, "delta"))
        return (op == EcmaScript_Object::ALLOW_ACCESSORS);
    else if (uni_str_eq(property_name, "zeta"))
        return FALSE;

    return TRUE;
}

static int /** putPrivate('<number>', '<value>') write private property to object*/
putPrivate(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    if (argv[0].type != VALUE_NUMBER)
        return ES_FAILED;

    int number = DOUBLE2INT32(argv[0].value.number);

    if (OpStatus::IsError(this_object->PutPrivate(number, argv[1])))
        return ES_NO_MEMORY;

    return ES_FAILED;
}

static int /** getPrivate('<number>') read private property from object */
getPrivate(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    if (argv[0].type != VALUE_NUMBER)
        return ES_FAILED;

    int number = DOUBLE2INT32(argv[0].value.number);

    if (!this_object->GetPrivate(number, return_value))
        return ES_FAILED;

    return ES_VALUE;
}

static int /** deletePrivate('<number>') delete private property from object */
deletePrivate(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    if (argv[0].type != VALUE_NUMBER)
        return ES_FAILED;

    int number = DOUBLE2INT32(argv[0].value.number);

    if (OpStatus::IsError(this_object->DeletePrivate(number)))
        return ES_NO_MEMORY;

    return ES_VALUE;
}

/* virtual */ int
HostObject_Constructor::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    return Construct(argv, argc, return_value, origining_runtime);
}

/* virtual */ int
HostObject_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
    HostObject *object = OP_NEW(HostObject, ());

    if (prototype == NULL)
    {
        if (OpStatus::IsError(origining_runtime->CreateNativeObjectObject(&prototype)))
            return ES_NO_MEMORY;

        AddFunctionToObject(origining_runtime, prototype, putPrivate, UNI_L("putPrivate"), "n-", PROP_DONT_ENUM);
        AddFunctionToObject(origining_runtime, prototype, getPrivate, UNI_L("getPrivate"), "n-", PROP_DONT_ENUM);
        AddFunctionToObject(origining_runtime, prototype, deletePrivate, UNI_L("deletePrivate"), "n-", PROP_DONT_ENUM);
    }

    if (OpStatus::IsError(object->SetObjectRuntime(origining_runtime, prototype, "HostObject", TRUE)))
        return ES_NO_MEMORY;

    return_value->type = VALUE_OBJECT;
    return_value->value.object = *object;

    return ES_VALUE;
}

/* virtual */ void
HostObject_Constructor::GCTrace()
{
    GetRuntime()->GCMark(prototype);
}
