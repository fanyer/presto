/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef PI_DEVICE_API_OPADDRESSBOOKSTATUS_H
#define PI_DEVICE_API_OPADDRESSBOOKSTATUS_H

typedef OP_STATUS OP_ADDRESSBOOKSTATUS;

/** @short Additional error status codes useful for reporting address book related errors.
 */
class OpAddressBookStatus : public OpStatus
{
public:

	enum
	{
		/** Item hasn't been found */
		ERR_ITEM_NOT_FOUND = USER_ERROR + 1,

		/** Field at this index isn't supported */
		ERR_FIELD_INDEX_OUT_OF_RANGE,

		/** Value at this index isn't supported */
		ERR_VALUE_INDEX_OUT_OF_RANGE,

		/** Item has not yet been registered with underlying store */
		ERR_NOT_YET_REGISTERED,

		/** No space left in the underlying store */
		ERR_NO_SPACE_LEFT
	};
};

#endif // PI_DEVICE_API_OPADDRESSBOOKSTATUS_H
