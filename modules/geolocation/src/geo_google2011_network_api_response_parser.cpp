/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef GEOLOCATION_SUPPORT

#include "modules/geolocation/src/geo_google2011_network_api_response_parser.h"

Google2011NetworkApiResponseParser::Google2011NetworkApiResponseParser()
	: m_state_stack_size(0)
	, m_response_status(NO_STATUS)
	, m_accuracy(op_nan(NULL))
	, m_latitude(op_nan(NULL))
	, m_longitude(op_nan(NULL))
{
}

/* virtual */ OP_STATUS
Google2011NetworkApiResponseParser::AttributeName(const OpString& attribute)
{
	if (m_state_stack_size == 1 && m_state_stack[0] == MAIN_OBJECT)
	{
		if (uni_str_eq(attribute, "status"))
			return PushState(STATUS);
		else if (uni_str_eq(attribute, "accuracy"))
			return PushState(ACCURACY);
		else if (uni_str_eq(attribute, "location"))
			return PushState(LOCATION);
		else if (uni_str_eq(attribute, "access_token"))
			return PushState(ACCESS_TOKEN);
	}
	else if (m_state_stack_size == 3 && m_state_stack[2] == LOCATION_OBJECT)
	{
		if (uni_str_eq(attribute, "lat"))
			return PushState(LAT);
		if (uni_str_eq(attribute, "lng"))
			return PushState(LNG);
	}

	return PushState(UNKNOWN_ATTRIBUTE);
}

/* virtual */ OP_STATUS
Google2011NetworkApiResponseParser::EnterObject()
{
	if (m_state_stack_size == 0)
		return PushState(MAIN_OBJECT);
	else if (m_state_stack_size == 2 && m_state_stack[1] == LOCATION)
		return PushState(LOCATION_OBJECT);
	else
		return PushState(UNKNOWN_OBJECT);
}

/* virtual */ OP_STATUS
Google2011NetworkApiResponseParser::LeaveObject()
{
	RETURN_IF_ERROR(PopState());

	// PopAttributeState returns ERR if there is nothing left on the state stack.
	// Here is it a valid situation if we're leaving the top-level object.
	OpStatus::Ignore(PopAttributeState());
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
Google2011NetworkApiResponseParser::String(const OpString& string)
{
	if (m_state_stack_size == 2)
	{
		switch (m_state_stack[1])
		{
		case STATUS:
			if (uni_str_eq(string, "OK"))
				m_response_status = OK;
			else if (uni_str_eq(string, "REQUEST_DENIED"))
				m_response_status = REQUEST_DENIED;
			else if (uni_str_eq(string, "INVALID_REQUEST"))
				m_response_status = INVALID_REQUEST;
			else if (uni_str_eq(string, "ZERO_RESULTS"))
				m_response_status = ZERO_RESULTS;
			else if (uni_str_eq(string, "OVER_QUERY_LIMIT"))
				m_response_status = OVER_QUERY_LIMIT;
			else
				return OpStatus::ERR;
			break;
		case ACCESS_TOKEN:
			RETURN_IF_ERROR(m_access_token.Set(string));
			break;
		}
	}

	return PopAttributeState();
}

/* virtual */ OP_STATUS
Google2011NetworkApiResponseParser::Number(double num)
{
	if (m_state_stack_size == 2 && m_state_stack[1] == ACCURACY)
		m_accuracy = num;
	else if (m_state_stack_size == 4 && m_state_stack[2] == LOCATION_OBJECT)
	{
		switch (m_state_stack[3])
		{
		case LAT: m_latitude = num; break;
		case LNG: m_longitude = num; break;
		}
	}

	return PopAttributeState();
}


OP_STATUS
Google2011NetworkApiResponseParser::PushState(Google2011NetworkApiResponseParser::State state)
{
	if (m_state_stack_size < MAX_STATE_STACK_SIZE)
	{
		m_state_stack[m_state_stack_size++] = state;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
Google2011NetworkApiResponseParser::PopState()
{
	if (m_state_stack_size > 0)
	{
		--m_state_stack_size;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
Google2011NetworkApiResponseParser::PopAttributeState()
{
	if (m_state_stack_size == 0)
		return OpStatus::ERR;

	if (IsAttributeState(m_state_stack[m_state_stack_size-1]))
		return PopState();
	else
		return OpStatus::OK;
}

#endif // GEOLOCATION_SUPPORT
