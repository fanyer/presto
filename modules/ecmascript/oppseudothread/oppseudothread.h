/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef OPPSEUDOTHREAD_H
#define OPPSEUDOTHREAD_H

#ifdef ES_OPPSEUDOTHREAD

#include "modules/pi/system/OpMemory.h"

#if !defined OPPSEUDOTHREAD_STACK_SWAPPING && !defined OPPSEUDOTHREAD_THREADED
# define OPPSEUDOTHREAD_STACK_SWAPPING
//# define OPPSEUDOTHREAD_THREADED
#endif // !OPPSEUDOTHREAD_STACK_SWAPPING && !OPPSEUDOTHREAD_THREADED

#ifdef OPPSEUDOTHREAD_THREADED
# ifdef LINGOGI
#  define OPPSEUDOTHREAD_THREADED_PTHREADS
# elif WINGOGI
#  define OPPSEUDOTHREAD_THREADED_WIN32
# else // LINGOGI
#  error "Threaded OpPseudoThread not supported on your platform!"
# endif // LINGOGI

class OpSystemMutex;
class OpSystemCondition;
class OpSystemThread;
#endif // OPPSEUDOTHREAD_THREADED

#undef Yield

#ifdef CRASHLOG_CAP_PRESENT
# define OPPSEUDOTHREAD_CRASHLOG_SUPPORT
#endif // CRASHLOG_CAP_PRESENT

/**
 * Pseudo thread implementation.
 *
 * A pseudo thread is a thread of execution that can be suspended and resumed
 * without actually unwinding the stack.  The primary way of doing this is to
 * allocate a separate stack which the thread uses, and suspend the thread's
 * execution by restoring the normal stack and returning.  Alternative ways to
 * accomplish the same functionality exists, for instance one could use the
 * normal stack all along, and suspend by copying the thread's part of it into
 * a separate location and "popping" it all from the normal stack.  That type
 * of suspension is obviously more expensive, but might be more portable since
 * nothing particularly abnormal really happens; we would only be copying data
 * to and from the stack, and adjust the stack pointer.
 *
 * A pseudo thread's implementation should take be care about using certain
 * mechanisms.  TRAP/LEAVE "inside" a pseudo thread probably works, whereas
 * leaving out of the thread (across a call to Start() or Resume()) might not
 * be a good idea.  But really, the safest is probably to use Yield() in
 * exceptional situations, and translate it to LEAVE afterwards.  Also, relying
 * on stack unwinding obviously has the same issues as with TRAP/LEAVE; if
 * there is any chance that a yielded thread is destroyed rather than resumed
 * (which seems inevitable) all data referenced from the stack must be possible
 * to clean up externally.  But since available stack space is also fixed (at
 * thread creation time,) conservative usage of the stack is wise disregarding
 * the unwind issues.
 *
 * Issues (found so far)
 *
 * Register ownership with Visual C++: the calling convention documentation
 * claims that ESI, EDI, EBX and EBP needs to be preserved by a called
 * function.  Some functions in this implementation are such that they seem
 * to do very little, in particular don't call any other functions, or write
 * into any registers (except to preserve their values.)  When calling such
 * a function, Visual C++ apparently assumes that registers not used by the
 * called function are also preserved, and thus sometimes relies on them
 * actually being preserved across the call.  Our simple implementations do
 * one crucial thing though: they swap stacks.  So in practice, they "call"
 * other code by returning to it, and then the call ends when the stacks are
 * swapped back and someone else returns.  In the end, we have to preserve
 * all registers (hopefully except EAX, and if not, we can let these
 * functions return something, in which case EAX is obviously trampled.)
 *
 */
class OpPseudoThread
{
public:
	OpPseudoThread();

	virtual ~OpPseudoThread();

	OP_STATUS Initialize(unsigned stacksize);
	/**< Initialize thread and allocate its stack to be 'stacksize' bytes large. */

    typedef void (*Callback)(OpPseudoThread *thread);

	BOOL Start(Callback callback);
	/**< Start thread by switching stack and calling 'callback'.  Returns TRUE
	     if the thread finished (OnRun() returned,) and FALSE if Yield() was
	     called. */

	BOOL Resume();
	/**< Resume a thread that has yielded.  Calling this on a thread that has
	     not been started, or which has finished, would be bad.  Same return
	     value as Start(). */

	void Yield();
	/**< Suspend the thread by restoring the original stack and then return from
	     the call to Start() or Resume(). */

	void Suspend(Callback callback);
	/**< Call 'callback' with the original stack.  In other words, use of the
	     thread local stack is suspended, and yielding becomes impossible.  When
	     OnSuspend() returns, the thread local stack is restored and yielding
	     becomes possible again. */

    OP_STATUS Reserve(Callback callback, unsigned stacksize);
    /**< Call 'callback' on the thread's stack, but ensure that there's at least
         'stacksize' bytes remaining.  If there is not, allocate a new memory
         block twice as large as the previous, switch to it, and then call
         'callback'. */

	unsigned StackSpaceRemaining();

    unsigned char *StackLimit();
    unsigned char *StackTop();

    BOOL IsActive() { return !is_using_standard_stack; }

#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
    static OpPseudoThread *current_thread;
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT

#ifndef CONSTANT_DATA_IS_EXECUTABLE
    static void InitCodeVectors();
    /**< Allocates & initializes the code blocks used by the platform-dependant, assembler-coded
         implementations of the meta-functions Start, Resume, Yield, Suspend, Reserve, StackSpaceRemaining */
#endif // CONSTANT_DATA_IS_EXECUTABLE

#ifdef _DEBUG
    class MemoryRange
    {
    public:
        void *start;
        unsigned length;
    };

    void DetectPointersIntoRanges(MemoryRange *ranges, unsigned ranges_count, void (*callback)(void *, void *), void *callback_data);
    /**< Scan all used stack memory for pointers into any of the specified
         memory ranges.  For each such pointer that is found, the specified
         callback is called with the two arguments 'callback_data' and the found
         pointer.  Note that there is of course no guarantee that a found
         pointer isn't just some unused garbage value that the compiler left on
         the stack at some point. */
#endif // _DEBUG

#if defined(ES_RECORD_PEAK_STACK)
    unsigned GetPeakStackSize() { return max_peak; }
    unsigned GetStackSize() { return max_stack_size; }
#endif

protected:
    BOOL is_using_standard_stack;

private:
#ifdef OPPSEUDOTHREAD_THREADED
    static void ThreadRun(void *);

    class Thread
    {
    public:
        Thread(Thread *previous = NULL)
            : callback(NULL),
              status(OpStatus::OK),
              thread(NULL),
              condition(NULL),
              previous(previous)
        {
        }

        ~Thread();

        Callback callback;
        OP_STATUS status;

        OpSystemThread *thread;
        OpSystemCondition *condition;

        Thread *previous;
    };

    OpSystemMutex *mutex;
    OpSystemCondition *condition;

    BOOL aborted, finished;
    Thread *current_thread;

    Callback suspend;
#else // OPPSEUDOTHREAD_THREADED
#ifdef _DEBUG
    void DetectPointersIntoRangesInternal(unsigned stack_space_remaining, MemoryRange *ranges, unsigned ranges_count, void (*callback)(void *, void *), void *callback_data);
#endif // _DEBUG

    OP_STATUS DoReserve(Callback callback, unsigned stacksize);

	static void AllocateNewBlock(OpPseudoThread *thread);

	unsigned char *original_stack, *thread_stack_ptr;

    class MemoryBlock
    {
    public:
        MemoryBlock(MemoryBlock *previous = NULL)
            : next(NULL),
              previous(previous),
              segment(NULL),
              memory(NULL)
        {
        }

        MemoryBlock *next, *previous;
        const OpMemory::OpMemSegment *segment;
        unsigned char *memory, *stack_ptr;
        unsigned actual_size, usable_size;
#ifdef _DEBUG
        unsigned stack_space_unused;
#endif // _DEBUG
#if defined(ES_RECORD_PEAK_STACK)
        unsigned previous_peak;
        unsigned char *previoius_stack_pointer;
#endif // _DEBUG

        void InitializeTail(void *previous_stack_ptr, void *original_stack_ptr);
    };

#if defined(ES_RECORD_PEAK_STACK)
    static void ComputeStackUsage(OpPseudoThread *thread);
    static void SetPattern(OpPseudoThread *thread);
    unsigned max_peak, max_stack_size, previous_reserved_stacksize;
#endif

    MemoryBlock *first, *current;
	unsigned reserved_stacksize;

    Callback suspend_callback;
    static void SuspendTrampoline(OpPseudoThread *thread);

    MemoryBlock *reserve_previous;
    Callback reserve_callback;
    static void ReserveTrampoline(OpPseudoThread *thread);

#ifdef OPPSEUDOTHREAD_CRASHLOG_SUPPORT
    unsigned char *crashlog_trampoline_stack_ptr;
    Callback crashlog_trampoline_callback;

    static void CrashlogTrampoline(OpPseudoThread *thread);
#endif // OPPSEUDOTHREAD_CRASHLOG_SUPPORT
#endif // OPPSEUDOTHREAD_THREADED
};

#endif // ES_OPPSEUDOTHREAD
#endif // OPPSEUDOTHREAD_H
