/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#ifndef ES_ERROR_OBJECT_H
#define ES_ERROR_OBJECT_H

class ES_Error
    : public ES_Object
{
public:
    static ES_Error *Make(ES_Context *context, ES_Global_Object *global_object, ES_Class *prototype_class, BOOL via_constructor = FALSE, BOOL is_proto = FALSE);

    static ES_Object *MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance);
    static ES_Object *MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance, int id);

    static ES_Class *MakeErrorInstanceClass(ES_Context *context, ES_Class *klass);
    static ES_Object *MakeErrorPrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *error_prototype_prototype, const char *error_name);

    ES_StackTraceElement *GetStackTrace() { return stacktrace; }

    unsigned GetStackTraceLength() { return stacktrace_length; }
    void SetStackTraceLength(unsigned length) { stacktrace_length = length; }

    enum StackTraceFormat { FORMAT_READABLE, FORMAT_MOZILLA };

    JString *GetStackTraceString(ES_Execution_Context *context, ES_Execution_Context *error_context, StackTraceFormat format, unsigned prefix_linebreaks);

    static JString *GetErrorString(ES_Execution_Context *context, ES_Object *error_object, BOOL allow_getters);

    BOOL ViaConstructor() { return via_constructor != 0; }

    enum ErrorSpecialProperties
    {
        PROP_stacktrace,
        PROP_stack,
        PROP_message
    };

private:
    friend class ESMM;

    static void Initialize(ES_Error *self, ES_Class *prototype_class)
    {
        ES_Object::Initialize(self, prototype_class);

        self->ChangeGCTag(GCTAG_ES_Object_Error);
        self->stacktrace_length = 0;
        self->stacktrace_strings[0] = NULL;
        self->stacktrace_strings[1] = NULL;
    }

    ES_StackTraceElement stacktrace[ES_StackTraceElement::STACK_TRACE_ELEMENTS];
    unsigned stacktrace_length:31;
    unsigned via_constructor:1;
    JString *stacktrace_strings[2];
};

#endif // ES_ERROR_OBJECT_H
