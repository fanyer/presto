/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ARGUMENT_CREATOR_H
#define ARGUMENT_CREATOR_H

/** @brief A class to create an argv array for use in execve() style functions
  */
class ArgumentCreator
{
public:
	ArgumentCreator();
	~ArgumentCreator();

	/** Initialization - run this function and check error before checking others
	  * @param first_argument The first argument in the list, traditionally the executable name. Must be non-empty!
	  */
	OP_STATUS Init(const OpStringC8& first_argument);

	/** Adds an argument to the end of the list of arguments
	  * @param argument Argument to add, if empty no argument is added
	  */
	OP_STATUS AddArgument(const OpStringC8& argument);
	OP_STATUS AddArgument(const OpStringC16& argument);

	/** Get a 0-terminated argument array for use in execve()-style functions
	  * @return The array if successful, or NULL on error.
	  *         The pointer is owned by the ArgumentCreator, and is valid until
	  *         this object is destroyed.
	  */
	char* const* GetArgumentArray();

private:
	OP_STATUS AddArgumentInternal(char* argument);

	static const unsigned GrowSize = 16;

	char**   m_argv_array;	///< Array with command line arguments
	unsigned m_argv_count;	///< Number of arguments in m_argv_array
	unsigned m_argv_size;	///< Size of array m_argv_array
};

#endif // ARGUMENT_CREATOR_H
