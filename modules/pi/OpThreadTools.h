/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OP_THREAD_TOOLS_H
#define OP_THREAD_TOOLS_H

/**	@short Manager of thread-safe operations and communication with the main thread.
 *
 * Core supports multiple threads, but its concurrency design requires that
 * the thread-local variable g_component_manager is initialized and accessible
 * in each thread. When this is not viable, as is the case with threads spawned
 * by third party code, and for existing code that has not been ported to the
 * new design, this class may bridge the gap.
 *
 * This class provides means for safely posting messages to a Core or non-core
 * component from another thread than the thread in which a component manager
 * exists. Also, this class provides threadsafe memory allocation for environments
 * where the standard malloc implementation is not reentrant.
 */
class OpThreadTools
{
public:
	/** Create and return an OpThreadTools object.
	 *
	 * @param new_main_thread (output) Set to the object created
	 * @return OK or ERR_NO_MEMORY.
	 */
	static OP_STATUS Create(OpThreadTools** new_main_thread);

	virtual ~OpThreadTools() {}

	/** Allocate memory in a thread-safe manner.
	 *
	 * May be called from any thread. If the malloc implementation used on
	 * the platform is thread-safe, the implementation of this method should
	 * probably just call malloc().
	 *
	 * @param size Number of bytes to allocate
	 * @return A pointer to the allocated memory, which is suitably aligned for
	 * any kind of variable, or NULL if the request fails (typically out of
	 * memory). This memory should only be freed using the Free() method.
	 */
	virtual void* Allocate(size_t size) = 0;

	/** Free memory allocated by Allocate() in a thread-safe manner.
	 *
	 * May be called from any thread. If the malloc implementation used on
	 * the platform is thread-safe, the implementation of this method should
	 * probably just call free().
	 *
	 * @param memblock A pointer to the memory that is to be freed. This
	 * pointer must have been returned by a previous call to Allocate(), unless
	 * it is NULL, in which case no operation is performed.
	 */
	virtual void Free(void* memblock) = 0;

	/** Issue a call to g_main_message_handler->PostMessage() in the Opera main thread.
	 *
	 * This method is ambiguous in processes where multiple Opera components
	 * exist, be that several on one thread, or spread across threads. The
	 * method remains for compatibility purposes, and should only succeed
	 * in processes where the choice of destination component is unambigous.
	 *
	 * This method may be implemented by wrapping its arguments in an OpLegacyMessage
	 * delivered through SendMessageToMainThread().
	 *
	 * This method may be called from any thread (including the Opera main
	 * thread itself). This method must never block (will cause deadlocks). If
	 * this method is not called from the Opera main thread, it must wake up
	 * the main thread in some platform-specific manner and execute code there
	 * that calls g_main_message_handler->PostMessage().
	 *
	 * Aside from being passed to the g_main_message_handler->PostMessage(), the
	 * parameter values has no meaning on the platform side.
	 */
	virtual OP_STATUS PostMessageToMainThread(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay = 0) = 0;

	/** Send a message to an Opera messenger in this process.
	 *
	 * This method may be called from any thread, and must never block.
	 *
	 * @param message The message to send. Its source and destination parameters
	 *        are correctly initialized by the caller. The callee assumes ownership
	 *        of the message, regardless of return value.
	 *
	 * @return See OpStatus.
	 */
	virtual OP_STATUS SendMessageToMainThread(OpTypedMessage* message) = 0;
};

#endif // OP_THREAD_TOOLS_H
