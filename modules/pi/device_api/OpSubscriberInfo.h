/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef PI_DEVICE_API_OPSUBSCRIBERINFO_H
#define PI_DEVICE_API_OPSUBSCRIBERINFO_H

#ifdef PI_SUBSCRIBERINFO

/** @short Provides information about user's subscription.
 */
class OpSubscriberInfo
{
public:
	virtual ~OpSubscriberInfo() { }

	static OP_STATUS Create(OpSubscriberInfo** new_subscriber_info);

	/** Gets the name of the operator hosting the user's account.
	 *
	 * @param result (out) - set to the name of the operator.
	 *
	 * @return
	 *  - OK - success,
	 *  - ERR_NOT_SUPPORTED - when the device has no such capability (e.g. a PC)
	 *  - ERR_NO_SUCH_RESOURCE - when the device cannot currently provide such
	 *    a value (e.g. SIM card not inserted)
	 */
	virtual OP_STATUS GetOperatorName(OpString* result) = 0;

	/** Provides the mobile phone number associated with the subscriber account.
	 *
	 * This corresponds to the dialed mobile number and not necessarily to the
	 * phone's internal Mobile Identification Number (MIN).
	 *
	 * @param result (out) - set to MSISDN.
	 *
	 * @return
	 *  - OK - success,
	 *  - ERR_NOT_SUPPORTED - when the device has no such capability (e.g. a PC)
	 *  - ERR_NO_SUCH_RESOURCE - when the device cannot currently provide such
	 *    a value (e.g. SIM card not inserted)
	 */
	virtual OP_STATUS GetSubscriberMSISDN(OpString8* result) = 0;

	/** Provides the IMSI value associated with the subscriber account.
	 *
	 * @param result (out) Set to IMSI.
	 *
	 * @return
	 *  - OK - success,
	 *  - ERR_NOT_SUPPORTED - when the device has no such capability (e.g. a PC)
	 *  - ERR_NO_SUCH_RESOURCE - when the device cannot currently provide such
	 *    a value (e.g. SIM card not inserted)
	 */
	virtual OP_STATUS GetSubscriberIMSI(OpString8* result) = 0;
};

#endif // PI_SUBSCRIBERINFO

#endif // PI_DEVICE_API_OPSUBSCRIBERINFO_H
