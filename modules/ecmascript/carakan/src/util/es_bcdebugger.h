/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_BCDEBUGGER_H
#define ES_BCDEBUGGER_H

#ifdef ES_BYTECODE_DEBUGGER

class ES_BytecodeDebugger
{
public:
    static void AtInstruction(ES_Execution_Context *context, ES_Code *code, ES_CodeWord *previous, ES_CodeWord *next);
    static void AtFunction(ES_Execution_Context *context, ES_FunctionCode *code);
    static void AtError(ES_Execution_Context *context, ES_Code *code, ES_CodeWord *current, const char *error, const char *message);

private:
    friend class ES_BytecodeDebuggerData;

    static void StopHere(ES_Execution_Context *context, ES_Code *code, ES_CodeWord *current);
    static ES_FrameStack *GetFrameStack(ES_Execution_Context *context) { return &context->frame_stack; }
};

#endif // ES_BYTECODE_DEBUGGER
#endif // ES_BCDEBUGGER_H
