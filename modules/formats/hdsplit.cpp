/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/hardcore/mem/mem_man.h"

#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"

// hdsplit.cpp

// Splitting Head(er)s


HeaderList::HeaderList()
{
}

HeaderList::HeaderList(Prepared_KeywordIndexes index)
: Sequence_Splitter(index)
{
}

HeaderList::HeaderList(HeaderList &old_hdrs)
: Sequence_Splitter(old_hdrs)
{	
}

#if defined UNUSED_CODE || defined SELFTEST
HeaderList::HeaderList(const KeywordIndex *keys, int count)
: Sequence_Splitter(keys, count)
{
}
#endif // UNUSED_CODE

Sequence_Splitter *HeaderList::CreateSplitterL() const
{
	return OP_NEW_L(HeaderList, ());
}
