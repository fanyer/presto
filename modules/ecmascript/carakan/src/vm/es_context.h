/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * @author Daniel Spang
 */

#ifndef ES_CONTEXT_H
#define ES_CONTEXT_H

#define ABORT_IF_MEMORY_ERROR(expr)                    \
   do                                                    \
   {                                                     \
       OP_STATUS RETURN_IF_ERROR_TMP = expr;             \
       if (OpStatus::IsMemoryError(RETURN_IF_ERROR_TMP)) \
       {                                                 \
           AbortOutOfMemory();                           \
       }                                                 \
   } while(0)

#define ABORT_CONTEXT_IF_MEMORY_ERROR(expr)            \
   do                                                    \
   {                                                     \
       OP_STATUS RETURN_IF_ERROR_TMP = expr;             \
       if (OpStatus::IsMemoryError(RETURN_IF_ERROR_TMP)) \
       {                                                 \
           context->AbortOutOfMemory();                  \
       }                                                 \
   } while(0)

class ES_Context
{
public:
    virtual ~ES_Context();

    ES_Eval_Status GetEvalStatus();
    /**< @return current eval status */

    void AbortOutOfMemory();
    /**< Yields execution and sets out of memory status in the
     * context. The context should not be used again. */

    void AbortError();
    /**< Yields execution and sets error status in the
     * context. The context should not be used again. */

    virtual ES_Execution_Context *GetExecutionContext() { return NULL; }

    virtual BOOL IsUsingStandardStack() { return TRUE; }
    /**< Returns TRUE if the context is running on the standard stack (compared
       to a separate (limited) stack). Used to determine if we should suspend
       before calling functions that we don't know the stack usage for.  */

    BOOL UseNativeDispatcher() { return use_native_dispatcher; }

    inline unsigned NewUnique() { return rt_data->unique++; }

    inline ES_Runtime *GetRuntime() const { return runtime; }

    ESRT_Data *rt_data;
    ES_Heap *heap;
    ES_Runtime *runtime;
    BOOL use_native_dispatcher;

protected:
    ES_Context(ESRT_Data *rt_data, ES_Heap *heap, ES_Runtime *runtime = NULL, BOOL use_native_dispatcher = FALSE);

    friend class ES_Host_Object;

    virtual inline void YieldImpl();
    /**< Provides a mechanism for yielding the context. */

    void SetNormal() { OP_ASSERT(eval_status != ES_NORMAL); eval_status = ES_NORMAL; }
    void SetOutOfMemory() { OP_ASSERT(eval_status == ES_NORMAL || eval_status == ES_ERROR_NO_MEMORY); eval_status = ES_ERROR_NO_MEMORY; }
    void SetError() { OP_ASSERT(eval_status == ES_NORMAL); eval_status = ES_ERROR; }

    ES_Eval_Status eval_status;
    /**< Current eval status. */

private:
    ES_Context();
};

void
ES_Context::YieldImpl()
{
    LEAVE(OpStatus::ERR_NO_MEMORY);
}
#endif // ES_CONTEXT_H
