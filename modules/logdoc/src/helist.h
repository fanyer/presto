/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef HELIST_H
#define HELIST_H

#include "modules/util/simset.h"

class LoadInlineElm;
class HEListElm;

/// List of references between LoadInlineElms and HTML elements.
class HEList : public List<HEListElm>
{
private:
	/// The LoadInlineElm this list belongs to.
	LoadInlineElm*	m_load_inline_element;

public:
					HEList(LoadInlineElm* lie) { m_load_inline_element = lie; }
	LoadInlineElm*	GetLoadInlineElm() { return m_load_inline_element; }
};



#endif // !HELIST_H

