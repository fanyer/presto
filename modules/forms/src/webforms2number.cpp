/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
  *
  * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
  *
  * This file is part of the Opera web browser.	It may not be distributed
  * under any circumstances.
  */

/**
 * @author Daniel Bratell
 */
#include "core/pch.h"
#include "modules/forms/webforms2number.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/doc/frm_doc.h"
#include "modules/stdlib/include/double_format.h"

static void CutAwayAnnoyingDecimals(char* temp_buf)
{
	int str_len = op_strlen(temp_buf);
	char* dot_pos = op_strchr(temp_buf, '.');
	if (str_len < 10 || op_strchr(temp_buf, 'e') || !dot_pos)
	{
		// Good as it is
		return;
	}

	int high_count = 0;
	int low_count = 0;
	char* it = dot_pos+1;
	while (*it)
	{
		if (*it == '0')
		{
			low_count++;
		}
		else if (*it == '9')
		{
			high_count++;
		}
		it++;
	}

	if (low_count <= 7 && high_count <= 7)
	{
		// Good as it is
		return;
	}

	it = dot_pos + op_strlen(dot_pos) - 1;
	// First cut away garbage, defined as the last 4 chars
	it -= 3;
	*it-- = '\0';
	if (low_count > 7)
	{
		// then every zero left
		while (*it == '0')
		{
			it--;
		}
		if (*it != '.')
		{
			it++;
		}
            *it = '\0';
	}
	else
	{
		// then every nine left
		while (*it == '9')
		{
			it--;
		}
		if (*it != '.')
		{
			it++;
		}
		*it = '\0';

		// Now add one to the last position
		int carry = 1;
		while (carry && it != temp_buf)
		{
			it--;
			if (*it != '.')
			{
				int digit = *it - '0';
				digit++;
				if (digit == 10)
				{
					digit = 0;
				}
				else
				{
					carry = 0;
				}
				*it = digit + '0';
			}
		}

		if (carry)
		{
			// Need to add an initial 1. Since we've removed chars to the right we
			// now that we have room to move the string one step right.
			op_memmove(temp_buf+1, temp_buf, sizeof(uni_char)*(op_strlen(temp_buf)+1));
			temp_buf[0] = '1';
		}
	}
}

/* static */
OP_STATUS WebForms2Number::DoubleToString(double number, uni_char* str_out)
{
	char temp_buf[32]; // ARRAY OK 2011-11-07 sof - OpDoubleFormat says it needs a 32 byte buffer
	char* res = OpDoubleFormat::ToString(temp_buf, number);
	if (!res)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	OP_ASSERT(res == temp_buf);

	CutAwayAnnoyingDecimals(temp_buf);

	// Convert to unicode
	make_doublebyte_in_buffer(temp_buf, op_strlen(temp_buf), str_out, 32);
	return OpStatus::OK;
}

/* static */
BOOL WebForms2Number::StringToDouble(const uni_char* number_str, double& number_out)
{
	number_out = 0.0;
	BOOL negate = FALSE;
	OP_ASSERT(number_str);

	// OPTIONAL_MINUS
	if (*number_str == '-')
	{
		number_str++;
		negate = TRUE;
	}

	// Integer part
	if (*number_str < '0' || *number_str > '9')
	{
		return FALSE;
	}

	while (*number_str >= '0' && *number_str <= '9')
	{
		number_out = number_out * 10 + (*number_str - '0');
		number_str++;
	}

	// Optional fraction
	if (*number_str == '.')
	{
		number_str++;
		// Integer part
		double digit_value = 0.1;
		if (*number_str < '0' || *number_str > '9')
		{
			return FALSE;
		}

		while (*number_str >= '0' && *number_str <= '9')
		{
			number_out += digit_value * (*number_str - '0');
			digit_value /= 10;
			number_str++;
		}
	}

	// Optional exponent
	if (*number_str == 'e')
	{
		number_str++;

		BOOL negate_exponent = FALSE;
		// optional minus
		if (*number_str == '-')
		{
			number_str++;
			negate_exponent = TRUE;
		}
		else if (*number_str == '+')
		{
			number_str++;
		}
		// Integer part
		if (*number_str < '0' || *number_str > '9')
		{
			return FALSE;
		}

		long exponent = 0;
		while (*number_str >= '0' && *number_str <= '9')
		{
			exponent = exponent * 10 + (*number_str - '0');
			number_str++;
		}

		// XXX ugly
		if (exponent > 300)
		{
			return FALSE;
		}

		if (negate_exponent)
		{
			// XXX ugly
			while(exponent)
			{
				number_out /= 10;
				exponent--;
			}
		}
		else
		{
			// XXX ugly
			while(exponent)
			{
				number_out *= 10;
				exponent--;
			}
		}
	}

	if (negate)
	{
		number_out = -number_out;
	}
	return *number_str == '\0';
}

/*static */
void WebForms2Number::GetNumberRestrictions(HTML_Element* elm,
											double& out_min,
											double& out_max,
											double& out_step_base,
											double& out_step)
{
	OP_ASSERT(elm);
	OP_ASSERT(elm->GetInputType() == INPUT_NUMBER ||
			  elm->GetInputType() == INPUT_RANGE);
	if (elm->GetInputType() == INPUT_RANGE)
	{
		out_min = 0.0; // Default per wf2 spec
		out_max = 100.0; // Default per wf2 spec
		out_step = 1.0; // Default per wf2 spec
		out_step_base = out_min; // Default per wf2 spec
	}
	else
	{
		out_min = -DBL_MAX; // Default per wf2 spec
		out_max = DBL_MAX; // Default per wf2 spec
		out_step = 1.0; // Default per wf2 spec
		out_step_base = 0.0; // Default per wf2 spec
	}

	const uni_char* max_str = elm->GetStringAttr(ATTR_MAX);
	if (max_str)
	{
		double temp;
		if (WebForms2Number::StringToDouble(max_str, temp))
		{
			out_max = temp;
			out_step_base = out_max;
		}
	}

	const uni_char* min_str = elm->GetStringAttr(ATTR_MIN);
	if (min_str)
	{
		double temp;
		if (WebForms2Number::StringToDouble(min_str, temp))
		{
			out_min = MIN(out_max, temp);
			out_step_base = out_min; // Overrides max as step_base
		}
	}

	const uni_char* step_str = elm->GetStringAttr(ATTR_STEP);
	if (step_str)
	{
		if (uni_stri_eq(step_str, "any"))
		{
			out_step = 0.0;
		}
		else
		{
			double temp;
			if (WebForms2Number::StringToDouble(step_str, temp) && temp > 0.0)
			{
				out_step = temp;
			}
		}
	}
}

static BOOL IsInteger(double value)
{
	return op_ceil(value) == op_floor(value);
}

static double MakeToInteger(double value)
{
	double ceiling = op_ceil(value);
	double diff = op_fabs(value - ceiling);
	if (diff < 0.5)
	{
		return ceiling;
	}
	return op_floor(value);
}

/**
 * @param direction which way we've changed the number to get to the
 * "unsnapped" number
 */
/* static */
double WebForms2Number::SnapToStep(double unsnapped_number,
								   double step_base, double step,
								   int direction)
{
	OP_ASSERT(step != 0.0);
	OP_ASSERT(direction != 0);
	double step_count = (unsnapped_number - step_base) / step;
	if (op_fabs(op_floor(step_count+0.5) - step_count) > 0.0001)
	{
		// Not at an even step
		if (direction > 0)
		{
			step_count = op_floor(step_count);
		}
		else
		{
			step_count = op_ceil(step_count);
		}
		// FIXME: Far from the step_base (when step_count is large)
		// the error in precision in step will accumulate and be
		// visible.
		unsnapped_number = step_count * step + step_base;
	}

	if (IsInteger(step) && IsInteger(step_base))
	{
		unsnapped_number = MakeToInteger(unsnapped_number);
	}
	else
	{
		// FIXME Must keep step as a lossless number somehow
		// Hack to remove unwanted decimals.
		uni_char temp_buf[32]; // ARRAY OK bratell 2009-01-26 - DoubleToString says it needs a 32 byte buffer
		if (OpStatus::IsSuccess(DoubleToString(unsnapped_number, temp_buf)))
		{
			// Since we got the string from DoubleToString we knows that it's a correct string
			// and don't have to check the return value.
			(void)StringToDouble(temp_buf, unsnapped_number);
		}
	}

	return unsnapped_number;
}

/**
 * @return OpStatus::ERR_OUT_OF_RANGE if step operation moves
 *         value out of [min, max] range. OpStatus::OK otherwise.
 */
OP_STATUS WebForms2Number::StepNumber(double in_value,
									  double min_value, double max_value,
									  double step_base, double step_value,
									  int step_count, BOOL wrap_around,
									  BOOL fuzzy_snapping,
									  double& out_value)
{
	double delta = step_count * step_value;
	double old_number = in_value;
	in_value += delta;
	if (old_number == in_value)
	{
		// Delta had no impact; ignore and move along.
		out_value = in_value;
		return OpStatus::OK;
	}

	if (!fuzzy_snapping && (in_value > max_value || in_value < min_value))
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	BOOL wrapped_around = FALSE;

	if (wrap_around)
	{
		// only give them a single chance to wrap to simplify logic
		if (in_value > max_value)
		{
			in_value = min_value + in_value - max_value;
			wrapped_around = TRUE;
		}
		else if (in_value < min_value)
		{
			in_value = max_value - min_value + in_value;
			wrapped_around = TRUE;
		}
	}

	int direction = delta < 0 ? -1 : 1;
	double snapped_value = in_value;
	if (!fuzzy_snapping)
	{
		if (in_value > max_value)
		{
			in_value = max_value;
		}
		else if (in_value < min_value)
		{
			in_value = min_value;
		}

		snapped_value = SnapToStep(in_value, step_base, step_value, wrapped_around ? -direction: direction);
		if ((op_fabs(snapped_value-in_value) / op_fabs(step_value)) > 0.000001)
		{
			// If the snap moved the value more than a
			// millionth of a step, then the value wasn't
			// on a step and the stepUp/Down was illegal
			return OpStatus::ERR_OUT_OF_RANGE;
		}

		if (!wrapped_around &&
			((delta > 0 && snapped_value <= old_number) ||
			 (delta < 0 && snapped_value >= old_number)))
		{
			// The snapping brought the number in the
			// wrong direction or didn't move us at all. Give up.
			return OpStatus::ERR_OUT_OF_RANGE;
		}
	}

	if (snapped_value < min_value || snapped_value > max_value)
	{
		// Give up. We couldn't find a legal value within [min,max]
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	out_value = snapped_value;

	return OpStatus::OK;
}

/* static */
double WebForms2Number::GetProgressPosition(FramesDocument* doc, HTML_Element *he)
{
	if (!doc)
	{
		// Value doesn't matter
		return 0;
	}
	OP_ASSERT(he->Type() == HE_PROGRESS);

	if (he->HasAttr(ATTR_VALUE, NS_IDX_HTML))
	{
		BOOL absent;
		double val = he->DOMGetNumericAttribute(doc->GetDOMEnvironment(), ATTR_VALUE, NULL, NS_IDX_DEFAULT, absent);
		double max = he->DOMGetNumericAttribute(doc->GetDOMEnvironment(), ATTR_MAX, NULL, NS_IDX_DEFAULT, absent);
		if (max <= 0)
		{
			max = 1;
		}
		double position = val / max;
		position = MAX(position, 0.0);
		position = MIN(position, 1.0);
		return position;
	}
	return -1;
}

/* static */
void WebForms2Number::GetMeterValues(FramesDocument* doc, HTML_Element *he, double &val,
														double &min, double &max,
														double &low, double &high,
														double &optimum)
{
	if (!doc)
	{
		// Values doesn't matter
		return;
	}
	OP_ASSERT(he->Type() == HE_METER);

	DOM_Environment *environment = doc->GetDOMEnvironment();

	// Get limits
	BOOL absent;
	min = he->DOMGetNumericAttribute(environment, ATTR_MIN, NULL, NS_IDX_DEFAULT, absent);
	max = he->DOMGetNumericAttribute(environment, ATTR_MAX, NULL, NS_IDX_DEFAULT, absent);
	max = MAX(max, min);

	// Get other values
	val = he->DOMGetNumericAttribute(environment, ATTR_VALUE, NULL, NS_IDX_DEFAULT, absent);

	if (he->HasAttr(ATTR_LOW, NS_IDX_HTML))
		low = he->DOMGetNumericAttribute(environment, ATTR_LOW, NULL, NS_IDX_DEFAULT, absent);
	else
		low = min;

	if (he->HasAttr(ATTR_HIGH, NS_IDX_HTML))
		high = he->DOMGetNumericAttribute(environment, ATTR_HIGH, NULL, NS_IDX_DEFAULT, absent);
	else
		high = max;

	if (he->HasAttr(ATTR_OPTIMUM, NS_IDX_HTML))
		optimum = he->DOMGetNumericAttribute(environment, ATTR_OPTIMUM, NULL, NS_IDX_DEFAULT, absent);
	else
		optimum = min + (max - min) * 0.5;

	// Limit all values
	val = MAX(min, val);
	val = MIN(max, val);
	low = MAX(min, low);
	low = MIN(max, low);
	high = MAX(min, high);
	high = MIN(max, high);
	optimum = MAX(min, optimum);
	optimum = MIN(max, optimum);
}
