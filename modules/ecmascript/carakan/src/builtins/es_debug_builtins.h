/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_DEBUG_BUILTINS_H
#define ES_DEBUG_BUILTINS_H

#ifdef ES_DEBUG_BUILTINS
class ES_DebugBuiltins
{
public:
#ifdef ES_HEAP_DEBUGGER
    static BOOL ES_CALLING_CONVENTION getHeapInformation(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getObjectDemographics(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
#endif // ES_HEAP_DEBUGGER

    static void PopulateDebug(ES_Context *context, ES_Global_Object *global_object, ES_Object *);
    static void PopulateDebugClass(ES_Context *context, ES_Class_Singleton *debug_class);

    enum
    {
        ES_DebugBuiltins_getHeapInformation,
        ES_DebugBuiltins_getObjectDemographics,

        ES_DebugBuiltinsCount,
        ES_DebugBuiltins_LAST_PROPERTY = ES_DebugBuiltins_getObjectDemographics
    };

};
#endif // ES_DEBUG_BUILTINS

#endif // ES_DEBUG_BUILTINS_H
