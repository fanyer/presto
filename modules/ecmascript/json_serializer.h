/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JSON_SERIALIZER_H
#define JSON_SERIALIZER_H

class JSONSerializer
{
	TempBuffer &buffer;
	BOOL add_comma;
	OP_STATUS AddComma(); // if needed
public:
	JSONSerializer(TempBuffer &out);
	virtual OP_STATUS EnterArray();
	virtual OP_STATUS LeaveArray();
	virtual OP_STATUS EnterObject();
	virtual OP_STATUS LeaveObject();
	virtual OP_STATUS AttributeName(const OpString& str);
	OP_STATUS AttributeName(const uni_char *str);

	// Values
	virtual OP_STATUS PlainString(const OpString& str); // Don't JSONify.
	OP_STATUS PlainString(const uni_char *str); // Don't JSONify.
	virtual OP_STATUS String(const OpString& str);
	OP_STATUS String(const uni_char *str);
	virtual OP_STATUS Number(double num);
	virtual OP_STATUS Int(int num);
	virtual OP_STATUS UnsignedInt(unsigned int num);
	virtual OP_STATUS Bool(BOOL val);
	virtual OP_STATUS Null();
};

#endif // JSON_SERIALIZER_H
