/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWSOPDLL_H
#define WINDOWSOPDLL_H

#include "modules/pi/OpDLL.h"

class WindowsOpDLL : public OpDLL
{
public:
	WindowsOpDLL();
	~WindowsOpDLL();

	OP_STATUS Load(const uni_char* dll_name);

	BOOL IsLoaded() const;

	OP_STATUS Unload();

	void* GetSymbolAddress(const char* symbol_name) const;

	HMODULE GetHandle() const { return dll; };

private:
	BOOL loaded;
	HMODULE dll;
};

#endif // WINDOWSOPDLL_H
