/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef GEOLOCATION_NEW_NETWORK_API_RESPONSE_PARSER_H
#define GEOLOCATION_NEW_NETWORK_API_RESPONSE_PARSER_H

#ifdef GEOLOCATION_SUPPORT

#include "modules/ecmascript/json_listener.h"

class Google2011NetworkApiResponseParser : public JSONListener
{
public:
	enum Status {
		NO_STATUS,          ///< There was no status in the response.
		OK,                 ///< Success, latitude and logitude should be present.
		REQUEST_DENIED,     ///< Request denied.
		INVALID_REQUEST,    ///< The request was invalid.
		ZERO_RESULTS,       ///< No results (I have no idea what that means).
		OVER_QUERY_LIMIT    ///< Query limit (per IP per day) has been reached.
	};

	Google2011NetworkApiResponseParser();

	/** Return status from the response.
	 */
	Status GetStatus() const { return m_response_status; }

	/** Return the accuracy.
	 *
	 * @return Accuracy in meters or NaN if there was no accuracy information in the response.
	 */
	double GetAccuracy() const { return m_accuracy; }

	/** Return the latitude.
	 *
	 * @return Latitude in degrees or NaN if there was no latitude data in the response.
	 */
	double GetLatitude() const { return m_latitude; }

	/** Return the longitude.
	 *
	 * @return Longitude in degrees or NaN if there was no longitude data in the response.
	 */
	double GetLongitude() const { return m_longitude; }
	const uni_char* GetAccessToken() const { return m_access_token.CStr(); }

	// From JSONListener
	virtual OP_STATUS EnterArray() { return PushState(UNKNOWN_ARRAY); }
	virtual OP_STATUS LeaveArray() { return LeaveObject(); }
	virtual OP_STATUS EnterObject();
	virtual OP_STATUS LeaveObject();
	virtual OP_STATUS AttributeName(const OpString& attribute);
	// Values
	virtual OP_STATUS String(const OpString& string);
	virtual OP_STATUS Number(double num);
	virtual OP_STATUS Bool(BOOL val) { return PopAttributeState(); }
	virtual OP_STATUS Null() { return PopAttributeState(); }

private:
	enum State {
		UNKNOWN_ARRAY,
		UNKNOWN_OBJECT,
		MAIN_OBJECT,
		LOCATION_OBJECT,

		// Attributes
		FIRST_ATTRIBUTE,
		UNKNOWN_ATTRIBUTE = FIRST_ATTRIBUTE,
		STATUS,
		ACCURACY,
		LOCATION,
		LAT,
		LNG,
		ACCESS_TOKEN
	};

	BOOL IsAttributeState(State s) const { return s >= FIRST_ATTRIBUTE; }

	enum {
		MAX_STATE_STACK_SIZE = 32
	};

	OP_STATUS PushState(State state);
	OP_STATUS PopState();
	OP_STATUS PopAttributeState();

	State m_state_stack[MAX_STATE_STACK_SIZE];
	unsigned m_state_stack_size;

	Status m_response_status;
	double m_accuracy;
	double m_latitude;
	double m_longitude;
	OpString m_access_token;
};

#endif // GEOLOCATION_SUPPORT

#endif // GEOLOCATION_NEW_NETWORK_API_RESPONSE_PARSER_H
