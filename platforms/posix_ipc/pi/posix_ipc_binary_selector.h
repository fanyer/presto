/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef POSIX_IPC_BINARY_SELECTOR_H
#define POSIX_IPC_BINARY_SELECTOR_H

class PosixIpcBinarySelector
{
public:
	/**
	 * Destructor.
	 */
	virtual ~PosixIpcBinarySelector() {}

	/** Gets the component type that should be used within a newly created process
 	  *
 	  * This allows for creating processes where the 'real' component in the
	  * new process is not the same as the component passed to PosixIpcBinarySelector
	  * in @ref Create, a hack have both 32-bit and 64-bit plugin wrappers that contain
	  * exactly the same code. Both internally use COMPONENT_PLUGIN, but they
	  * are started by specifying different component types.
	  *
	  * @example mapping on Linux, COMPONENT_PLUGIN_LINUX_IA32 maps to COMPONENT_PLUGIN
	  *
 	  * @param type Type to map
 	  * @return mapped type
 	  */
	static OpComponentType GetMappedComponentType(OpComponentType type);

	/** Create a PosixIpcBinarySelector for a given component type
	  * @param component Component type to start
	  * @param token Serialized @ref PosixIpcProcessToken that the new process can deserialize
	  *  to get the data to start a new component
	  * @param logfolder Path to folder that should be used for crash logging. If the binary
	  *  is capable of generating crash logs, it should place them in this directory.
	  * @return New PosixIpcBinarySelector object, caller takes ownership, or NULL on error
	  */
	static PosixIpcBinarySelector* Create(OpComponentType component, const char* token, const char* logfolder);

	/** Get the path to the binary that should be executed for the component given in
	  * Create()
	  * @return A full path to a binary executable file
	  */
	virtual const char* GetBinaryPath() = 0;

	/** Get an array of string arguments to use for executing the binary
	  * @return argument array that can be passed to execv
	  * @note The argument array returned must be valid as long as this object exists.
	  * The first item in the array is traditionally the binary name (ie. same as GetBinaryPath())
	  * The array MUST be terminated by a NULL value
	  */
	virtual char *const * GetArgsArray() = 0;
};

#endif // POSIX_IPC_BINARY_SELECTOR_H
