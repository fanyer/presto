/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_OPDATA_OPDATABASICS_H
#define MODULES_OPDATA_OPDATABASICS_H

/** @file
 *
 * @brief Foundational declarations needed by OpData and UniString
 *
 * This file is an internal/private part of the opdata module. Please do not
 * \#include this file from outside modules/opdata.
 *
 * This file contains some basic declarations needed by most other classes in
 * opdata module.
 *
 * See OpData.h and UniString.h for more extensive design notes on OpData and
 * UniString, respectively.
 */


/**
 * @name Convenient length constants/values.
 *
 * @{
 */

/**
 * Maximum number of bytes that can be held by an OpData object.
 */
const size_t OpDataMaxLength = ~static_cast<size_t>(0);

/**
 * Sentinel value indicating that op_strlen() determines actual length.
 *
 * This value is used to indicate that the length of the associated data chunk
 * is unknown, but guarantees that it is a valid C-style '\0'-terminated string,
 * such that its actual length can be determined by calling op_strlen().
 */
const size_t OpDataUnknownLength = OpDataMaxLength;

/**
 * Sentinel value indicating the full length available in the current context.
 *
 * This value is used to indicate that the actual value will be set to the
 * maximum length available in the current context. For example, in the context
 * of specifying a length within a buffer, this value indicates that the
 * maximum/entire length of the buffer will be used.
 */
const size_t OpDataFullLength = OpDataMaxLength;

/**
 * Sentinel value indicating a failed search.
 *
 * This value is used as a return value from search methods to indicate that
 * the search was unsuccessful.
 */
const size_t OpDataNotFound = OpDataMaxLength;

/** @} */

/**
 * @name Deallocation strategies.
 *
 * Strategies for deallocating a piece of memory.
 */
enum OpDataDeallocStrategy {
	/**
	 * Do not deallocate at all. Suitable for static memory, or
	 * constant memory managed somewhere else (must be guaranteed
	 * to outlive the object holding this reference).
	 * Memory is considered read-only.
	 */
	OPDATA_DEALLOC_NONE = 0,

	/**
	 * Deallocate by passing the pointer to op_free().
	 * Suitable for memory allocated with op_malloc().
	 * Memory is considered read/writable.
	 */
	OPDATA_DEALLOC_OP_FREE = 1,

	/**
	 * Deallocate by passing the pointer to OP_DELETEA.
	 * Suitable for memory allocated by OP_NEWA.
	 * Memory is considered read/writable.
	 */
	OPDATA_DEALLOC_OP_DELETEA = 2,

	/*
	 * IMPORTANT NOTE:
	 * If you need to insert a new deallocation strategy that is viable
	 * across API boundaries, you should add it directly above this comment.
	 *
	 * Also make sure to adjust the values below this comment accordingly.
	 */

	/**
	 * Deallocation is defined within the context where this value is used.
	 * It is strictly forbidden to use this value in official APIs, it is
	 * only to be used in special-purpose code (e.g. within the OpData
	 * implementation).
	 */
	OPDATA_DEALLOC_USER_DEFINED = 3,

	/*
	 * IMPORTANT NOTE:
	 * If you need to insert a new deallocation strategy that is NOT viable
	 * across API boundaries, you should add it directly above this comment.
	 *
	 * Also make sure to adjust the values below this comment accordingly.
	 */

	/**
	 * @name Meta values
	 *
	 * IMPORTANT: Please update the following values when adding/removing
	 * deallocation strategies above.
	 *
	 * @{
	 */

	/**
	 * Number of available deallocation strategies.
	 */
	OPDATA_DEALLOC_COUNT = 4,

	/**
	 * Number of bits needed to represent available deallocation
	 * strategies (not counting meta values). This should be equal to
	 * ceil(log2(OPDATA_DEALLOC_COUNT)).
	 */
	OPDATA_DEALLOC_NUMBITS = 2

	/** @} */
};

/**
 * Generic function for deallocating data with a given deallocation strategy.
 *
 * IMPORTANT: The given deallocation strategy can NOT be
 * #OPDATA_DEALLOC_USER_DEFINED, as that strategy depends on special knowledge
 * from the domain where it is used.
 *
 * @param ptr Pointer to piece of memory that should be deallocated.
 * @param ds Deallocation strategy determining how given data is deallocated.
 */
op_force_inline void OpDataDealloc(char *ptr, OpDataDeallocStrategy ds)
{
	switch (ds)
	{
	case OPDATA_DEALLOC_NONE:
		/* do nothing */
		break;
	case OPDATA_DEALLOC_OP_FREE:
		op_free(ptr);
		break;
	case OPDATA_DEALLOC_OP_DELETEA:
		OP_DELETEA(ptr);
		break;
	case OPDATA_DEALLOC_USER_DEFINED:
		OP_ASSERT(!"OpDataDealloc() cannot handle OP_DATA_DEALLOC_USER_DEFINED data!");
		break;
	default:
		OP_ASSERT(!"OpDataDealloc() called with invalid parameter!");
		break;
	}
}

/**
 * Return true if the given strategy is valid.
 *
 * This function is meant to be used for checking a given deallocation strategy
 * for validity across API boundaries. Therefore, for the purposes of this
 * function, #OPDATA_DEALLOC_USER_DEFINED is considered INVALID (as it should
 * not be used across API boundaries).
 *
 * @param ds Deallocation strategy to be checked for validity
 * @returns true if given deallocation strategy is valid, false otherwise
 */
op_force_inline bool OpDataDeallocStrategyIsValid(OpDataDeallocStrategy ds)
{
	return static_cast<int>(ds) >= 0 && ds < OPDATA_DEALLOC_USER_DEFINED;
}

#endif // MODULES_OPDATA_OPDATABASICS_H
