/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2003 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

/**
 * @author jb@opera.com
 *
 * The Stackometer is a very simple stack usage meter. It measures the
 * stack usage by comparing the first frame pointer to the current frame
 * pointer at the start of every function. When a new max depth is found
 * a message is printed to stderr.
 *
 * It uses gcc's instrument functions capability, and therefore only
 * works with gcc and only with functions that have been compiled with
 * -finstrument-functions.
 *
 * Note that the stack meassurement does not give a 100% correct view of
 * the stack usage:
 *
 *   +  The system libraries (e.g. qt) are usually not compiled with
 *      -finstrument-functions, and therefore are not accounted for. 
 *   +  Any stack usage by the system before the first frame is not
 *      accounted for.
 *   +  Local peaks of stack usage within a function is not accounted
 *      for. The stack usage is only calculated when the function is
 *      entered.
 *
 * The code to find the first frame (in get_stack_start()) probably
 * only works for x86. Other platforms should redefine GET_FRAME()
 * and PREV_FRAME(), or find the start of the stack in some other way.
 * Note that for multithreaded environments the stack start must be
 * determined per thread since every thread have their own stack.
 *
 * Defines:
 * =======
 * ALWAYS_CALCULATE_STACK_START
 *   Instead of storing the start of the stack in a global (or thread
 *   local) variable the stack start is recalculated every time it is
 *   used. This is useful for multithreaded environments since setting
 *   and getting thread local variables might have a high overhead, and
 *   (it seems) call other system functions, possibly causing loops in the
 *   stackometer. (This happens for instance with pthreads if LTH malloc
 *   is used and is compiled with -finstrument-functions).
 *
 * _NO_THREADS_
 *   If ALWAYS_CALCULATE_STACK_START is not defined, the start of the stack,
 *   once calculated, is stored in a global variable.
 *
 * _PTHREADS_
 *   If neither _NO_THREADS_ nor ALWAYS_CALCULATE_STACK_START is defined
 *   the stackometer will store the stack start in a thread local variable
 *   (using the pthreads API).
 *
 * Threads
 * =======
 * The stackometer is not 100% thread safe, the max_stack and max_fn
 * variables are not guarded in any way, this could cause race
 * conditions with incorrect data as a result. However, in practice it
 * isn't a big problem.
 */

#include <stdlib.h>
#include <stdio.h>

#if !defined(_NO_THREADS_) && defined(_PTHREADS_)
#include <pthread.h>
#endif

#if !defined(_NO_THREADS_) && !defined(_PTHREADS_)
#define ALWAYS_CALCULATE_STACK_START
#endif

#ifdef STACK_TRACE_SUPPORT
/* C++ name mangling... */
#define show_stack_trace show_stack_trace__Fii
extern void show_stack_trace(int levels, int start_level);
#endif

static int max_depth = 0;
static void *max_fn = 0;

static char initialized = 0;

#if defined(ALWAYS_CALCULATE_STACK_START)
# define GET_STACK_START() NULL
# define SET_STACK_START(start)
#elif defined(_NO_THREADS_)
  static void *stack_start = 0;
# define GET_STACK_START() stack_start
# define SET_STACK_START(start) stack_start = start
#elif defined(_PTHREADS_)
  static pthread_key_t stack_start_key;
# define GET_STACK_START() pthread_getspecific(stack_start_key)
# define SET_STACK_START(start) pthread_setspecific(stack_start_key, start)
#else
#error "Don't know how to get/set the stack start!"
#endif

#define GET_FRAME() __builtin_frame_address(0)
#define PREV_FRAME(frame) (*((void **)(frame)))

/**
 * Once this is called the stackometer will start ticking.
 */
int __attribute__((__no_instrument_function__))
init_stackometer()
{
	int retval = 0;

	if (initialized == 1)
		return 0;

#if !defined(_NO_THREADS_) && !defined(ALWAYS_CALCULATE_STACK_START) && defined(_PTHREADS_)
	retval = pthread_key_create(&stack_start_key, NULL);
#endif

	initialized = 1;

	return retval;
}

/**
 * Gets the start of the stack.
 * If the start of the stack is not know it is calculated
 * by traversing the frames until the first frame is
 * found.
 */
static void *__attribute__((__no_instrument_function__))
get_stack_start()
{
	void *start = GET_STACK_START();

	if (start == NULL)
	{
		start = GET_FRAME();

		while ( PREV_FRAME(start) != NULL )
		{
			start = PREV_FRAME(start);
		}

		SET_STACK_START(start);
	}

	return start;
}

/**
 * Prints information about the stack usage to stderr.
 */
static void __attribute__((__no_instrument_function__))
show_stack_info(void *this_fn, void *call_site, void *frame, int depth)
{
	char *unit = "";
	double ddepth = depth / 1024.0;

	if (ddepth > 1)
		unit = "K";
	else
		ddepth = depth;
	
	fprintf(stderr, "MAX_STACK : this_fn = %p, call_site = %p, frame = %p, stack_depth = %.2f%s\n", this_fn, call_site, frame, ddepth, unit);
	
#ifdef STACK_TRACE_SUPPORT
	show_stack_trace(2,2);
#endif
}

/**
 * Called when a function (compiled with -finstrument-functions) is entered.
 * It is this function that determines the stack depth and sets the max depth.
 */
void __attribute__((__no_instrument_function__))
__cyg_profile_func_enter(void *this_fn, void *call_site)
{
	void *start;
	void *frame = GET_FRAME();
	int depth;

	if (initialized == 0)
		return;

	start = get_stack_start();

	depth = start - frame;

	if (depth > max_depth)
	{
		max_depth = depth;
		max_fn = this_fn;
		show_stack_info(this_fn, call_site, frame, depth);
	}
}

/**
 * Called when a function (compiled with -finstrument-functions) returns.
 * This function is intentionally left blank. 
 */
void __attribute__((__no_instrument_function__))
__cyg_profile_func_exit(void *this_fn, void *call_site)
{
}
