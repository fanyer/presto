/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ST_IMAP4_H
#define ST_IMAP4_H

#include "adjunct/m2/src/backend/imap/imap-protocol.h"

class ST_IMAP4 : public IMAP4
{
protected:
	/** Initialize processor
	 */
	virtual OP_STATUS InitProcessor();

	/** Start authentication
	 */
	virtual OP_STATUS Authenticate();
};

#endif // ST_IMAP4_H