/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OP_UNIQUE_H
#define OP_UNIQUE_H

#include "modules/util/opfile/opfile.h"

/**
 * Helps generate unique names.
 *
 * Usage example:
 * @code
 * 		OpString name;
 * 		RETURN_IF_ERROR(name.Set(UNI_L("Base name")));
 * 		for (OpUnique unique(name); Exists(name); )
 *			RETURN_IF_ERROR(unique.Next());
 * @endcode
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class OpUnique
{
public:
	/**
	 * @param name the name to work on.  Should be set to some initial value
	 * 		by the caller.  It is then modified by Next().
	 * @param suffix_format the suffix format used to generate unique names
	 */
	OpUnique(OpString& name, const OpStringC& suffix_format = DEFAULT_SUFFIX_FORMAT);

	/**
	 * Suffixes the original name to generate another, possibly unique name.
	 */
	OP_STATUS Next();

	/**
	 * The suffix format used to generate unique names by default.
	 */
	static const uni_char DEFAULT_SUFFIX_FORMAT[];

private:
	OpString m_original_name;
	OpString* const m_name;
	OpStringC m_suffix_format;
	int m_counter;
};

#endif // OP_UNIQUE_H
