/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_FRAME_INLINES_H
#define ES_FRAME_INLINES_H

inline OP_STATUS ES_FrameStack::Push(ES_Execution_Context *context)
{
    RETURN_IF_ERROR(Allocate(context, top_frame));

    top_frame->ip = context->ip;
    top_frame->first_register = context->reg;
    top_frame->code = context->code;
    top_frame->variable_object = context->variable_object;
    context->variable_object = NULL;
    top_frame->arguments_object = context->arguments_object;
    context->arguments_object = NULL;
#ifdef ES_NATIVE_SUPPORT
    ES_NativeStackFrame *native_stack_frame = context->native_stack_frame;
    if (native_stack_frame)
    {
        IncrementTotalUsed(ES_NativeStackFrame::GetDepth(native_stack_frame));
        context->native_stack_frame = NULL;
    }
    top_frame->native_stack_frame = native_stack_frame;
    top_frame->true_ip = NULL;
#endif // ES_NATIVE_SUPPORT
    top_frame->register_overlap = context->overlap;
    top_frame->argc = context->argc;
    top_frame->in_constructor = !!context->in_constructor;
    top_frame->first_in_block = context->first_in_block;
    top_frame->frame_type = ES_VirtualStackFrame::NORMAL;

    return OpStatus::OK;
}

inline void ES_FrameStack::Pop(ES_Execution_Context *context)
{
    if (context->arguments_object)
        ES_Execution_Context::DetachArgumentsObject(context, context->arguments_object);

    if (context->variable_object)
        ES_Execution_Context::PopVariableObject(context, context->variable_object);

    context->ip = top_frame->ip;
    context->reg = top_frame->first_register;
    context->code = top_frame->code;
    context->variable_object = top_frame->variable_object;
    context->arguments_object = top_frame->arguments_object;
#ifdef ES_NATIVE_SUPPORT
    ES_NativeStackFrame *native_stack_frame = context->native_stack_frame = top_frame->native_stack_frame;
    if (native_stack_frame)
        DecrementTotalUsed(ES_NativeStackFrame::GetDepth(native_stack_frame));
#endif // ES_NATIVE_SUPPORT
    context->overlap = top_frame->register_overlap;
    context->argc = top_frame->argc;
    context->in_constructor = top_frame->in_constructor;
    context->first_in_block = top_frame->first_in_block;

    Free();
    LastItem(top_frame);
}

inline ES_Value_Internal *
ES_FrameStackIterator::GetRegisterFrame()
{
#ifdef ES_NATIVE_SUPPORT
    if (native_frame)
        return ES_NativeStackFrame::GetRegisterFrame(native_frame);
#endif // ES_NATIVE_SUPPORT

    if (at_first)
        return context->reg;
    else
        return virtual_frame->first_register;
}

inline ES_Code *&
ES_FrameStackIterator::GetCode()
{
#ifdef ES_NATIVE_SUPPORT
    if (native_frame)
        return ES_NativeStackFrame::GetCode(native_frame);
#endif // ES_NATIVE_SUPPORT

    if (at_first)
        return context->code;
    else
        return virtual_frame->code;
}

inline ES_CodeWord *
ES_FrameStackIterator::GetCodeWord()
{
#ifdef ES_NATIVE_SUPPORT
    if (native_frame)
    {
        ES_CodeWord *ip;

        if (native_frame == context->native_stack_frame)
            return context->ip;
        else if (previous_native_frame && (ip = ES_NativeStackFrame::GetCodeWord(native_frame, previous_native_frame)) != NULL)
            return ip;
        else if (native_ip)
        {
            ES_Code *code = GetCode();
            if (native_ip >= code->data->codewords && native_ip < code->data->codewords + code->data->codewords_count)
                return native_ip;
        }

        return NULL;
    }
#endif // ES_NATIVE_SUPPORT

    if (at_first)
        return context->ip;
    else
        return virtual_frame->ip;
}

inline ES_Arguments_Object *
ES_FrameStackIterator::GetArgumentsObject()
{
#ifdef ES_NATIVE_SUPPORT
    if (native_frame)
        return ES_NativeStackFrame::GetArgumentsObject(native_frame);
#endif // ES_NATIVE_SUPPORT

    if (at_first)
        return context->arguments_object;
    else
        return virtual_frame->arguments_object;
}

inline void
ES_FrameStackIterator::SetArgumentsObject(ES_Arguments_Object *arguments)
{
#ifdef ES_NATIVE_SUPPORT
    if (native_frame)
        return ES_NativeStackFrame::SetArgumentsObject(native_frame, arguments);
#endif // ES_NATIVE_SUPPORT

    if (at_first)
        context->arguments_object = arguments;
    else
        virtual_frame->arguments_object = arguments;
}

inline ES_Object *
ES_FrameStackIterator::GetVariableObject()
{
#ifdef ES_NATIVE_SUPPORT
    if (native_frame)
        return ES_NativeStackFrame::GetVariableObject(native_frame);
#endif // ES_NATIVE_SUPPORT

    if (at_first)
        return context->variable_object;
    else
        return virtual_frame->variable_object;
}

inline void
ES_FrameStackIterator::SetVariableObject(ES_Object *variables)
{
#ifdef ES_NATIVE_SUPPORT
    if (native_frame)
        return ES_NativeStackFrame::SetVariableObject(native_frame, variables);
#endif // ES_NATIVE_SUPPORT

    if (at_first)
        context->variable_object = variables;
    else
        virtual_frame->variable_object = variables;
}

inline unsigned
ES_FrameStackIterator::GetArgumentsCount()
{
#ifdef ES_NATIVE_SUPPORT
    if (native_frame)
        return ES_NativeStackFrame::GetArgumentsCount(native_frame);
#endif // ES_NATIVE_SUPPORT

    if (at_first)
        return context->argc;
    else
        return virtual_frame->argc;
}

inline BOOL
ES_FrameStackIterator::InConstructor()
{
    if (at_first)
        return !!context->in_constructor;
    else
    {
        OP_ASSERT(GetVirtualFrame());
        return GetVirtualFrame()->in_constructor;
    }
}

inline ES_Object *
ES_FrameStackIterator::GetThisObjectOrNull()
{
    ES_Value_Internal *registers = GetRegisterFrame();
    return registers[0].IsObject() ? registers[0].GetObject(context) : NULL;
}

inline ES_Function *
ES_FrameStackIterator::GetFunctionObject()
{
    if (ES_Code *code = GetCode())
        if (code->type == ES_Code::TYPE_FUNCTION)
        {
            OP_ASSERT(GetRegisterFrame()[1].GetObject(context)->IsFunctionObject());
            return static_cast<ES_Function *>(GetRegisterFrame()[1].GetObject(context));
        }

    return NULL;
}

inline BOOL
ES_FrameStackIterator::operator== (const ES_FrameStackIterator &other)
{
#ifdef ES_NATIVE_SUPPORT
    return at_first == other.at_first && virtual_frame == other.virtual_frame && native_frame == other.native_frame;
#else // ES_NATIVE_SUPPORT
    return at_first == other.at_first && virtual_frame == other.virtual_frame;
#endif // ES_NATIVE_SUPPORT
}

#endif // ES_FRAME_INLINES_H
