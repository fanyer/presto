/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef FOLDER_RECURSE_H
#define FOLDER_RECURSE_H

#include "modules/util/opfile/opfile.h"

class FolderRecursor
{
		OpAutoVector <OpFolderLister>	lister_stack;
		const INT32						max_stack_depth; 
public:
		/**
		 * @param max_depth The maximum recursion depth.  @c 0 means listing the
		 *		root folder only.  Negative values mean no depth limit.
		 */
		explicit FolderRecursor(INT32 max_depth = -1);
		~FolderRecursor();
		
		OP_STATUS SetRootFolder(const OpString& root);
		OP_STATUS GetNextFile(OpFile*& file);
};

#endif // FOLDER_RECURSE_H
