/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMLIBRARY_H
#define DOM_DOMLIBRARY_H

#ifdef DOM_LIBRARY_FUNCTIONS

class DOM_LibraryFunction
{
public:
	class Binding
	{
	public:
		void (*impl)();
		int data;
	};

	const Binding *bindings;
	const char *const *text;
};

extern const DOM_LibraryFunction DOM_Node_normalize;

#endif // DOM_LIBRARY_FUNCTIONS
#endif // DOM_DOMLIBRARY_H
