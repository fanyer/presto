/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JSON_LISTENER_H
#define JSON_LISTENER_H

class JSONListener
{
public:
	virtual OP_STATUS EnterArray() = 0;
	virtual OP_STATUS LeaveArray() = 0;
	virtual OP_STATUS EnterObject() = 0;
	virtual OP_STATUS LeaveObject() = 0;
	virtual OP_STATUS AttributeName(const OpString&) = 0;

	// Values
	virtual OP_STATUS String(const OpString&) = 0;
	virtual OP_STATUS Number(double num) = 0;
	virtual OP_STATUS Bool(BOOL val) = 0;
	virtual OP_STATUS Null() = 0;
};

#endif // JSON_LISTENER_H
