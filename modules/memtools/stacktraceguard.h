/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifdef MEMTOOLS_ENABLE_STACKGUARD

class OpStackTraceGuard
{
public:
	OpStackTraceGuard(void) { parent = g_current_top_of_stack; g_current_top_of_stack = this; }
	~OpStackTraceGuard(void) { g_current_top_of_stack = parent; }

	static BOOL StackAddressIsOK(void* addr);

private:
	OpStackTraceGuard* parent;

	static OpStackTraceGuard* g_current_top_of_stack;
};

#endif // MEMTOOLS_ENABLE_STACKGUARD
