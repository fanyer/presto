/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/vm/es_frame.h"
#include "modules/ecmascript/carakan/src/compiler/es_native_arch.h"

/* Tempting as it would be to make the following an inline declared as part of
 * its class declaration, the ADS compiler (Brew) fails to generate code for the
 * ES_BlockHead<ES_VirtualStackFrame> if we put this there.  Here, it works
 * fine.
 */
ES_FrameStack::ES_FrameStack(size_t initial_capacity)
    : ES_BlockHead<ES_VirtualStackFrame>(initial_capacity), top_frame(NULL)
{
}

#ifdef ES_NATIVE_SUPPORT

void
ES_FrameStack::DropFrames(unsigned drop_count)
{
    /* Note: frames may span blocks, hence drop them one at a time. CARAKAN-1021 */
    while (drop_count--)
        Free();

    LastItem(top_frame);
}

#endif // ES_NATIVE_SUPPORT

ES_FrameStackIterator::ES_FrameStackIterator(ES_Execution_Context *context)
    : context(context)
    , frames(context->frame_stack)
    , before_first(TRUE)
    , at_first(FALSE)
    , virtual_frame(NULL)
    , virtual_iter_block(NULL)
    , virtual_iter_index(0)
#ifdef ES_NATIVE_SUPPORT
    , native_frame(NULL)
    , previous_native_frame(NULL)
    , native_iter(context->native_stack_frame)
    , native_ip(NULL)
#endif // ES_NATIVE_SUPPORT
{
    if ((virtual_iter_block = frames.GetLastUsedBlock()) != NULL)
        if ((virtual_iter_index = virtual_iter_block->Used()) == 0)
            virtual_iter_block = NULL;
#ifdef ES_NATIVE_SUPPORT
    if (native_iter)
        before_first = FALSE;
#endif // ES_NATIVE_SUPPORT
}

ES_FrameStackIterator::ES_FrameStackIterator(const ES_FrameStackIterator &other)
    : context(other.context)
    , frames(other.frames)
    , before_first(other.before_first)
    , at_first(other.at_first)
    , virtual_frame(other.virtual_frame)
    , virtual_iter_block(other.virtual_iter_block)
    , virtual_iter_index(other.virtual_iter_index)
#ifdef ES_NATIVE_SUPPORT
    , native_frame(other.native_frame)
    , previous_native_frame(other.previous_native_frame)
    , native_iter(other.native_iter)
    , native_ip(other.native_ip)
#endif // ES_NATIVE_SUPPORT
{
}

BOOL
ES_FrameStackIterator::Next()
{
#ifdef ES_NATIVE_SUPPORT
    if (native_iter)
    {
        virtual_frame = NULL;
        previous_native_frame = native_frame;
        native_frame = native_iter;
        native_iter = ES_NativeStackFrame::GetNextFrame(native_frame);
        if (!ES_NativeStackFrame::GetCode(native_iter))
            native_iter = NULL;
        return TRUE;
    }
#endif // ES_NATIVE_SUPPORT

    if (before_first)
    {
        before_first = FALSE;
        at_first = TRUE;
        return TRUE;
    }
    else if (virtual_iter_block)
    {
        at_first = FALSE;
#ifdef ES_NATIVE_SUPPORT
        native_frame = NULL;
#endif // ES_NATIVE_SUPPORT
        virtual_frame = virtual_iter_block->Storage() + --virtual_iter_index;
#ifdef ES_NATIVE_SUPPORT
        native_iter = virtual_frame->native_stack_frame;
        if (native_iter)
            native_ip = virtual_frame->true_ip ? virtual_frame->true_ip : virtual_frame->ip;
#endif // ES_NATIVE_SUPPORT
        if (virtual_iter_index == 0)
        {
            virtual_iter_block = static_cast<ES_Block<ES_VirtualStackFrame> *>(virtual_iter_block->Pred());
            if (virtual_iter_block)
                virtual_iter_index = virtual_iter_block->Used();
        }
        return TRUE;
    }
    else
        return FALSE;
}

BOOL
ES_FrameStackIterator::NextMajorFrame()
{
    unsigned frame_count = 0;

    ES_FrameStackIterator probe_frames(*this);

    while (probe_frames.Next())
    {
        frame_count++;

        ES_Code *code = probe_frames.GetCode();
        if (code && code->type != ES_Code::TYPE_EVAL_PLAIN)
        {
            /* found a notable frame, make this frame iterator point to it. */
            while (frame_count-- > 0)
                Next();

            return TRUE;
        }
    }

    return FALSE;
}

#ifdef _DEBUG
# define inline
# include "modules/ecmascript/carakan/src/vm/es_frame_inlines.h"
#endif // _DEBUG
