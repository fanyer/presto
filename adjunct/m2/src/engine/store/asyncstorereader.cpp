/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/store/asyncstorereader.h"
#include "adjunct/m2/src/MessageDatabase/MessageStore.h"
#include "modules/pi/OpSystemInfo.h"

namespace
{
	const double TargetTimeMS = 100.0; ///< try to spend this much time per execution
	const int MinBlockCount = 512;
};


AsyncStoreReader::AsyncStoreReader(MessageStore& store, OpFileLength startpos, int blockcount)
  : m_store(store)
  , m_startpos(startpos)
  , m_blockcount(blockcount)
{
}


void AsyncStoreReader::Execute()
{
	double starttime = g_op_time_info->GetRuntimeMS();

	OpFileLength pos = m_store.ReadData(m_startpos, m_blockcount);

	// Check if there might be more to read
	if (pos != 0)
	{
		double timespent = g_op_time_info->GetRuntimeMS() - starttime;
		AdaptBlockCount(timespent);

		AsyncStoreReader* next_command = OP_NEW(AsyncStoreReader, (m_store, pos, m_blockcount));
		GetQueue()->AddCommand(next_command);
	}
}


void AsyncStoreReader::AdaptBlockCount(double timespent)
{
	// Try to get closer to the target time by doubling or halving
	// the amount of blocks being read
	if (m_blockcount <= MinBlockCount)
		return;

	if (timespent > TargetTimeMS)
	{
		m_blockcount = max(m_blockcount / 2, MinBlockCount);
	}
	else if (timespent * 2 < TargetTimeMS)
	{
		m_blockcount = m_blockcount * 2;
	}
}

#endif // M2_SUPPORT
