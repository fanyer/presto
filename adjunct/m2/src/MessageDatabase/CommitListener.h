/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef COMMIT_LISTENER_H
#define COMMIT_LISTENER_H

class CommitListener
{
public:
	virtual ~CommitListener() {}

	/** Function called when a database has committed data to disk
	  */
	virtual void OnCommitted() = 0;
};

#endif // COMMIT_LISTENER_H
