/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/desktop_pi_impl/ArgumentCreator.h"
#include "platforms/posix/posix_native_util.h"


/***********************************************************************************
 ** Constructor
 **
 ** ArgumentCreator::ArgumentCreator
 ***********************************************************************************/
ArgumentCreator::ArgumentCreator()
	: m_argv_array(0)
	, m_argv_count(0)
	, m_argv_size(0)
{
}


/***********************************************************************************
 ** Destructor
 **
 ** ArgumentCreator::~ArgumentCreator
 ***********************************************************************************/
ArgumentCreator::~ArgumentCreator()
{
	for (unsigned i = 0; i < m_argv_count; i++)
		OP_DELETEA(m_argv_array[i]);

	OP_DELETEA(m_argv_array);
}


/***********************************************************************************
 ** Initialization
 **
 ** ArgumentCreator::Init
 ***********************************************************************************/
OP_STATUS ArgumentCreator::Init(const OpStringC8& first_argument)
{
	if (first_argument.IsEmpty())
		return OpStatus::ERR;

	return AddArgument(first_argument);
}


/***********************************************************************************
 ** Add an argument
 **
 ** ArgumentCreator::AddArgument
 ***********************************************************************************/
OP_STATUS ArgumentCreator::AddArgument(const OpStringC8& argument)
{
	// Don't add empty arguments
	if (argument.IsEmpty())
		return OpStatus::OK;

	// Copy argument
	int argument_size = argument.Length() + 1;

	char* argument_copy = OP_NEWA(char, argument_size);
	if (!argument_copy)
		return OpStatus::ERR_NO_MEMORY;

	op_strlcpy(argument_copy, argument.CStr(), argument_size);

	// Add to list
	return AddArgumentInternal(argument_copy);
}


/***********************************************************************************
 ** Add an argument
 **
 ** ArgumentCreator::AddArgument
 ***********************************************************************************/
OP_STATUS ArgumentCreator::AddArgument(const OpStringC16& argument)
{
	// Don't add empty arguments
	if (argument.IsEmpty())
		return OpStatus::OK;

	// Convert the argument to native encoding and add to list
	PosixNativeUtil::NativeString native_string(argument.CStr());
	return AddArgument(native_string.get());
}


/***********************************************************************************
 ** Get a 0-terminated argument array for use in execve() style functions
 **
 ** ArgumentCreator::GetArgumentArray
 ***********************************************************************************/
char* const* ArgumentCreator::GetArgumentArray()
{
	return m_argv_array;
}


/***********************************************************************************
 ** Adds argument to internal array
 ** @param argument to add, will take ownership
 **
 ** ArgumentCreator::AddArgumentInternal
 ***********************************************************************************/
OP_STATUS ArgumentCreator::AddArgumentInternal(char* argument)
{
	// Don't add empty arguments
	if (!argument || !*argument)
	{
		OP_DELETEA(argument);
		return OpStatus::OK;
	}

	// Grow argument list if necessary
	if (m_argv_count + 1 >= m_argv_size)
	{
		// Grow by grow size
		char** new_argv_array = OP_NEWA(char*, m_argv_size + GrowSize);
		if (!new_argv_array)
		{
			OP_DELETEA(argument);
			return OpStatus::ERR_NO_MEMORY;
		}

		// Copy the original array, including NULL-termination
		if (m_argv_array)
			op_memcpy(new_argv_array, m_argv_array, m_argv_count + 1);

		OP_DELETEA(m_argv_array);
		m_argv_array =  new_argv_array;
		m_argv_size  += GrowSize;
	}

	// Add argument to list with NULL-termination
	m_argv_array[m_argv_count] = argument;
	m_argv_count++;
	m_argv_array[m_argv_count] = 0;

	return OpStatus::OK;
}


