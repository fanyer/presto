/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ASYNC_STORE_READER
#define ASYNC_STORE_READER

#include "adjunct/desktop_util/async_queue/AsyncCommand.h"

class MessageStore;

class AsyncStoreReader : public AsyncCommand
{
public:
	AsyncStoreReader(MessageStore& store, OpFileLength startpos, int blockcount);
	virtual void Execute();

private:
	void AdaptBlockCount(double timespent);

	MessageStore& m_store;
	OpFileLength m_startpos;
	int m_blockcount;
};

#endif // ASYNC_STORE_READER
