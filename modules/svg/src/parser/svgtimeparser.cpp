/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/parser/svgtimeparser.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/dom/domenvironment.h"

OP_STATUS
SVGTimeParser::ParseTimeList(const uni_char *input_string,
							 unsigned str_len, SVGVector *vector)
{
    tokenizer.Reset(input_string, str_len);
    this->vector = vector;
    this->status = OpStatus::OK;

    SVGTimeObject *time_value;
    while (GetNextTimeValue(time_value))
    {
		OP_ASSERT(OpStatus::IsSuccess(status));
		status = vector->Append(time_value);
		if(OpStatus::IsError(status))
		{
			OP_DELETE(time_value);
		}
    }

	return tokenizer.ReturnStatus(status);
}

OP_STATUS
SVGTimeParser::ParseAnimationTime(const uni_char *input_string, unsigned str_len,
								  SVG_ANIMATION_TIME &animation_time)
{
    tokenizer.Reset(input_string, str_len);
    this->status = OpStatus::OK;

	animation_time = ParseAnimationTime();

	return tokenizer.ReturnStatus(status);
}

BOOL
SVGTimeParser::GetNextTimeValue(SVGTimeObject *&time_value)
{
    if (OpStatus::IsError(status))
		return FALSE;

	time_value = NULL;

	tokenizer.EatWsp();

	if (tokenizer.IsEnd())
	{
		return FALSE;
	}

	unsigned first_char = tokenizer.CurrentCharacter();

	if (first_char == '+' || first_char == '-' || XMLUtils::IsDecDigit(first_char))
	{
		ParseOffsetValue(time_value);
	}
	else if (tokenizer.Scan("indefinite"))
	{
		time_value = OP_NEW(SVGTimeObject, (SVGTIME_INDEFINITE));
		if (!time_value)
			status = OpStatus::ERR_NO_MEMORY;
	}
	else if (tokenizer.Scan("wallclock("))
	{
		status = OpStatus::ERR;
	}
	else if (tokenizer.Scan("accessKey("))
	{
		ParseAccessKeyValue(time_value);
	}
	else
	{
		const uni_char *id_value = NULL;
		unsigned id_value_len = 0;

		FindIdReference(id_value, id_value_len);

		if (tokenizer.Scan("repeat("))
		{
			time_value = OP_NEW(SVGTimeObject, (SVGTIME_REPEAT));
			if (!time_value)
				status = OpStatus::ERR_NO_MEMORY;
			else
			{
				SVG_ANIMATION_TIME repeat_number = GetDigits();
				time_value->SetRepetition((UINT32)repeat_number);

				if (tokenizer.Scan(')'))
				{
					time_value->SetOffset(GetOffset(TRUE, FALSE));

					if (id_value && id_value_len)
					{
						status = time_value->SetElementID(id_value, id_value_len);
					}
				}
				else
				{
					status = OpStatus::ERR;
				}
			}
		}
		else
		{
			BOOL begin_syncbase = FALSE;
			BOOL end_syncbase = FALSE;

			// event or syncbase
			if (id_value && id_value_len > 0)
			{
				SVGTokenizer::State saved_state = tokenizer.state;
				begin_syncbase = tokenizer.Scan("begin");
				if (!begin_syncbase)
				{
					end_syncbase = tokenizer.Scan("end");
				}

				if (Unicode::IsAlphaOrDigit(tokenizer.CurrentCharacter()))
				{
					// false alarm, possibly part of a event-name, revert!
					tokenizer.state = saved_state;
					begin_syncbase = end_syncbase = FALSE;
				}
			}

			if (begin_syncbase || end_syncbase)
			{
				// syncbase
				time_value = OP_NEW(SVGTimeObject, (SVGTIME_SYNCBASE));
				if (time_value)
				{
					if (begin_syncbase)
					{
						time_value->SetSyncbaseType(SVGSYNCBASE_BEGIN);
					}
					else if (end_syncbase)
					{
						time_value->SetSyncbaseType(SVGSYNCBASE_END);
					}

					time_value->SetOffset(GetOffset(TRUE, FALSE));

					if (id_value && id_value_len)
					{
						status = time_value->SetElementID(id_value, id_value_len);
					}
				}
				else
					status = OpStatus::ERR_NO_MEMORY;
			}
			else
			{
				// event type (or error)
				ParseEventValue(time_value, id_value, id_value_len);
			}
		}
	}

	if (OpStatus::IsError(status))
	{
		OP_DELETE(time_value);
		time_value = NULL;
	}
	else
	{
		tokenizer.EatWspSeparatorWsp(';');
	}

	return (time_value != NULL);
}

BOOL
SVGTimeParser::ScanUnicode(unsigned& uc)
{
	// Probably overly cautious
	SVGTokenizer::State saved_state = tokenizer.state;

	if (tokenizer.Scan("U+"))
	{
		uc = 0;
		if (tokenizer.ScanHexDigits(uc) == 4)
			return TRUE;

		tokenizer.state = saved_state;
		status = OpStatus::ERR;
	}
	return FALSE;
}

void
SVGTimeParser::ParseAccessKeyValue(SVGTimeObject *&time_value)
{
	time_value = OP_NEW(SVGTimeObject, (SVGTIME_ACCESSKEY));
	if (!time_value)
	{
		status = OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		unsigned tok;
		if (!ScanUnicode(tok))
		{
			tok = tokenizer.CurrentCharacter();
			tokenizer.Shift();
		}
		time_value->SetAccessKey(tok);

		if (tokenizer.Scan(')'))
		{
			time_value->SetOffset(GetOffset(TRUE, FALSE));
		}
		else
		{
			status = OpStatus::ERR;
		}
	}
}

void
SVGTimeParser::FindIdReference(const uni_char *& name, unsigned &name_len)
{
	name_len = 0;

	SVGTokenizer::State original_state = tokenizer.state;

	name = original_state.CurrentString();
	if (XMLUtils::IsNameFirst(XMLVERSION_1_0, tokenizer.CurrentCharacter()))
	{
		name_len = 1;
		unsigned c = tokenizer.Shift();
		while (c)
		{
			if (c == '.' || c == '-')
				break;

			if (c == '\\')
			{
				name_len++;
				c = tokenizer.Shift();
			}

			if (!XMLUtils::IsName(XMLVERSION_1_0, c))
				break;

			c = tokenizer.Shift();
			name_len++;
		}
	}

	if (name_len > 0 && !tokenizer.Scan('.'))
	{
		name = NULL;
		name_len = 0;
		tokenizer.state = original_state;
	}
}

unsigned
SVGTimeParser::ScanEventType()
{
	unsigned len = 0;

	// All known event types are alphabetic
	while (tokenizer.ScanAlphabetic())
	{
		len++;
	}
	return len;
}

void
SVGTimeParser::ParseEventValue(SVGTimeObject *&time_value, const uni_char *id_value, unsigned id_value_len)
{
	const uni_char *prefix_event = NULL;
	unsigned prefix_event_length = 0;

	const uni_char *event_name_start = tokenizer.CurrentString();
	unsigned event_name_length = ScanEventType();

	if (tokenizer.Scan(':'))
	{
		// A colon is a indicator that this is a custom event.
		prefix_event_length = event_name_length;
		prefix_event = event_name_start;

		event_name_start = tokenizer.CurrentString();
		event_name_length = ScanEventType();
	}

	if (event_name_length == 0)
	{
		status = OpStatus::ERR;
	}
	else
	{
		time_value = OP_NEW(SVGTimeObject, (SVGTIME_EVENT));
		if (!time_value ||
			OpStatus::IsMemoryError(time_value->SetEventName(event_name_start, event_name_length,
															 prefix_event, prefix_event_length)))
		{
			status = OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			if (time_value->GetEventType() == DOM_EVENT_NONE)
			{
				status = OpStatus::ERR;
			}
			else
			{
				time_value->SetOffset(GetOffset(TRUE, FALSE));
				if (id_value && id_value_len)
				{
					status = time_value->SetElementID(id_value, id_value_len);
				}
			}
		}
	}
}

void
SVGTimeParser::ParseOffsetValue(SVGTimeObject *&time_value)
{
	time_value = OP_NEW(SVGTimeObject, (SVGTIME_OFFSET));
	if (!time_value)
	{
		status = OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		SVG_ANIMATION_TIME clock_value = GetOffset(FALSE, TRUE);
		time_value->SetOffset(clock_value);
	}
}

SVG_ANIMATION_TIME
SVGTimeParser::ParseAnimationTime()
{
	tokenizer.EatWsp();

	SVG_ANIMATION_TIME arg1 = GetDigits();
	SVG_ANIMATION_TIME animation_time = SVGAnimationTime::Indefinite();

	if (tokenizer.Scan(':'))
	{
		// Full or partial clockvalue
		SVG_ANIMATION_TIME seconds;
		SVG_ANIMATION_TIME minutes;
		SVG_ANIMATION_TIME hours;

		SVG_ANIMATION_TIME arg2 = GetDigits();
		if (tokenizer.Scan(':'))
		{
			// Full clockvalue
			seconds = GetDigits();
			minutes = arg2;
			hours = arg1;
		}
		else
		{
			seconds = arg2;
			minutes = arg1;
			hours = 0;
		}

		if (minutes < 60 && seconds < 60)
		{
			/* Compute maximum hour such that the calculation that
			   follows can't overflow */

			const SVG_ANIMATION_TIME max_hour = (SVGAnimationTime::Latest() - (59 * 60000 + 59 * 1000)) / 3600000;

			if (hours <= max_hour)
			{
				double q = GetOptionalFraction();

				animation_time =
					hours * 3600000 +
					minutes * 60000 +
					seconds * 1000;

				if (q != 0)
					animation_time += (SVG_ANIMATION_TIME)((1000 * q) + 0.5);
			}
			else
				animation_time = SVGAnimationTime::Latest();
		}
	}
	else
	{
		double q = GetOptionalFraction();
		SVG_ANIMATION_TIME metric_numerator = GetOptionalMetric();

		if (arg1 < (SVGAnimationTime::Latest() / metric_numerator))
			animation_time = (SVG_ANIMATION_TIME)((metric_numerator * ((double)arg1 + q)) + 0.5);
		else
			animation_time = SVGAnimationTime::Latest();
	}

	tokenizer.EatWsp();

	if (animation_time == SVGAnimationTime::Indefinite())
	{
		status = OpStatus::ERR;
	}

	return animation_time;
}

SVG_ANIMATION_TIME
SVGTimeParser::GetDigits()
{
	SVG_ANIMATION_TIME r = 0;
	unsigned n = 0;
	while (XMLUtils::IsDecDigit(tokenizer.CurrentCharacter()))
	{
		char c = (char)tokenizer.CurrentCharacter();
		tokenizer.Shift();
		int num = c - '0';
		r = (r * 10) + num;
		n++;
	}

	if (n == 0)
		status = OpStatus::ERR;

	return r;
}

double
SVGTimeParser::GetOptionalFraction()
{
	double p = 0;
	SVG_ANIMATION_TIME q = 1;
	if (tokenizer.Scan('.'))
	{
		while (XMLUtils::IsDecDigit(tokenizer.CurrentCharacter()))
		{
			char c = (char)tokenizer.CurrentCharacter();
			tokenizer.Shift();
			int num = c - '0';
			q *= 10;
			p += num / (double)q;
		}
	}
	return p;
}

SVG_ANIMATION_TIME
SVGTimeParser::GetOptionalMetric()
{
	if (tokenizer.Scan('s'))
	{
		return 1000;
	}
	else if (tokenizer.Scan('h'))
	{
		return 3600000;
	}
	else if (tokenizer.Scan("min"))
	{
		return 60000;
	}
	else if (tokenizer.Scan("ms"))
	{
		return 1;
	}
	else
	{
		// default
		return 1000;
	}
}

SVG_ANIMATION_TIME
SVGTimeParser::GetOffset(BOOL optional, BOOL optional_sign)
{
	SVGTokenizer::State saved_state = tokenizer.state;

	tokenizer.EatWsp();

	SVG_ANIMATION_TIME sign = 1;
	if (tokenizer.Scan('-'))
	{
		sign = -1;
	}
	else if (!tokenizer.Scan('+') && !optional_sign && !optional)
	{
		status = OpStatus::ERR;
		return 0;
	}

	tokenizer.EatWsp();
	if (!optional || XMLUtils::IsDecDigit(tokenizer.CurrentCharacter()))
	{
		SVG_ANIMATION_TIME animation_time = ParseAnimationTime();
		animation_time *= sign;
		return animation_time;
	}
	else
	{
		tokenizer.state = saved_state;
		return 0;
	}
}

#endif // SVG_SUPPORT
