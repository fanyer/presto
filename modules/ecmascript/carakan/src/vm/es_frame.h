/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_FRAME_H
#define ES_FRAME_H

#include "modules/ecmascript/carakan/src/compiler/es_code.h"
#include "modules/ecmascript/carakan/src/kernel/es_value.h"
#include "modules/ecmascript/carakan/src/vm/es_block.h"

#ifdef _DEBUG
# define inline
#endif // _DEBUG

class ES_FrameStack
    : public ES_BlockHead<ES_VirtualStackFrame>
{
public:
    ES_FrameStack(size_t initial_capacity);

    inline OP_STATUS Push(ES_Execution_Context *context);
    inline void Pop(ES_Execution_Context *context);

    BOOL IsEmpty() { return top_frame == NULL; }

    ES_VirtualStackFrame *TopFrame() { return top_frame; }

#ifdef ES_NATIVE_SUPPORT
    void DropFrames(unsigned count);
#endif // ES_NATIVE_SUPPORT

private:
    ES_VirtualStackFrame *top_frame;
};

class ES_FrameStackIterator
{
public:
    ES_FrameStackIterator(ES_Execution_Context *context);
    ES_FrameStackIterator(const ES_FrameStackIterator &other);

    BOOL Next();

    ES_VirtualStackFrame *GetVirtualFrame() { return virtual_frame; }
#ifdef ES_NATIVE_SUPPORT
    ES_NativeStackFrame *GetNativeFrame() { return native_frame; }

    inline BOOL IsFollowedByNative() { return virtual_frame && virtual_frame->native_stack_frame; }
#endif // ES_NATIVE_SUPPORT

    inline ES_Value_Internal *GetRegisterFrame();
    inline ES_Code *&GetCode();

    inline ES_Arguments_Object *GetArgumentsObject();
    inline void SetArgumentsObject(ES_Arguments_Object *arguments);

    inline ES_Object *GetVariableObject();
    inline void SetVariableObject(ES_Object *variables);

    inline ES_Object *GetThisObjectOrNull();

    inline ES_Function *GetFunctionObject();

    inline unsigned GetArgumentsCount();

    inline BOOL InConstructor();

    inline ES_CodeWord *GetCodeWord();
    inline ES_VirtualStackFrame::FrameType GetFrameType() { return virtual_frame ? virtual_frame->frame_type : ES_VirtualStackFrame::NORMAL; }

    inline BOOL operator== (const ES_FrameStackIterator &other);

    BOOL NextMajorFrame();

private:
    ES_Execution_Context *context;
    ES_FrameStack &frames;

    BOOL before_first, at_first;

    ES_VirtualStackFrame *virtual_frame;
    ES_Block<ES_VirtualStackFrame> *virtual_iter_block;
    unsigned virtual_iter_index;

#ifdef ES_NATIVE_SUPPORT
    ES_NativeStackFrame *native_frame;
    ES_NativeStackFrame *previous_native_frame;
    ES_NativeStackFrame *native_iter;
    ES_CodeWord *native_ip;
#endif // ES_NATIVE_SUPPORT
};

#ifdef _DEBUG
# undef inline
#endif // _DEBUG

#endif // ES_FRAME_H
