/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPDLL_H
#define OPDLL_H

/** @file OpDLL.h
 *
 * Contains the OpDLL interface.
 *
 * @author Markus Johansson
 *
 */

/** @short OpDLL provides functions for loading and unloading a dynamically linked library (DLL). */
class OpDLL
{
public:
	/** Create a new OpDLL.
	 *
	 * @param new_opdll *new_opdll shall be set to a new OpDLL object.
	 *
	 * @return OK or ERR_NO_MEMORY.
	 */
	static OP_STATUS Create(OpDLL** new_opdll);

	virtual ~OpDLL() {};

	/** Load a DLL.
	 *
	 * If a DLL is currently loaded, it is first unloaded.
	 *
	 * @param dll_name Name of the DLL.
	 *
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	virtual OP_STATUS Load(const uni_char* dll_name) = 0;

	/** Check if the DLL has been successfully loaded.
	 *
	 * @return TRUE if the DLL has been successfully loaded.
	 */
	virtual BOOL IsLoaded() const = 0;

	/** Unload the DLL.
	 *
	 * If the DLL is not loaded this function does nothing.
	 *
	 * @return OK, ERR_NO_MEMORY or ERR.
	 */
	virtual OP_STATUS Unload() = 0;

#ifdef DLL_NAME_LOOKUP_SUPPORT
	/** Return the address for a symbol in the library.
	 *
	 * This can be used to get the address for functions and
	 * variables. If this function is called when the DLL has not been
	 * successfully loaded (IsLoaded() return FALSE) this function
	 * will return NULL.
	 *
	 * @param symbol_name Name of the symbol to lookup. Only available
	 * if DLL_NAME_LOOKUP_SUPPORT is defined.
	 *
	 * @return A pointer to the symbol. NULL if the symbol was not
	 * found or if the DLL was not successfully loaded.
	 */
	virtual void* GetSymbolAddress(const char* symbol_name) const = 0;
#else
	/** Return the address for a symbol in the library.
	 *
	 * This can be used to get the address for functions and
	 * variables. If this function is called when the DLL has not been
	 * successfully loaded (IsLoaded() return FALSE) this function
	 * will return NULL.
	 *
	 * @param symbol_id Id for the symbol to lookup if
	 * DLL_NAME_LOOKUP_SUPPORT is not defined.
	 *
	 * @return A pointer to the symbol. NULL if the symbol was not
	 * found or if the DLL was not successfully loaded.
	 */
	virtual void* GetSymbolAddress(int symbol_id) const = 0;
#endif // DLL_NAME_LOOKUP_SUPPORT
};

#endif // OPDLL_H
