/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#ifndef HISTORY_LOCKS_H
#define HISTORY_LOCKS_H

#ifdef DIRECT_HISTORY_SUPPORT

/**
 * @brief A "lock" to hinder sync'ing with link
 * @author Patricia Aas
 *
 * This "lock" is absolutely NOT thread safe. It is for convenience only.
 *
 * Declaration: OpHistorySyncLock lock;
 */
class OpTypedHistorySyncLock
{
public:
	OpTypedHistorySyncLock();
	~OpTypedHistorySyncLock();

	static BOOL IsLocked();
};

/**
 * @brief A "lock" to hinder saving
 * @author Patricia Aas
 *
 * This "lock" is absolutely NOT thread safe. It is for convenience only.
 *
 * Declaration: OpHistorySaveLock lock;
 */
class OpTypedHistorySaveLock
{
public:
	OpTypedHistorySaveLock();
	~OpTypedHistorySaveLock();

	static BOOL IsLocked();
};
#endif // DIRECT_HISTORY_SUPPORT

#endif // HISTORY_LOCKS_H
