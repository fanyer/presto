/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/forms/formvaluewf2date.h"
#include "modules/forms/webforms2number.h"
#include "modules/forms/datetime.h"
#include "modules/forms/piforms.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/stdlib/util/opdate.h"

// FormValueDate

/* virtual */
FormValue* FormValueWF2DateTime::Clone(HTML_Element *he)
{
	FormValueWF2DateTime* clone = OP_NEW(FormValueWF2DateTime, (m_date_type));
	if (clone)
	{
		if (he && IsValueExternally())
		{
			OpString current_value;
			if (OpStatus::IsSuccess(GetFormObjectValue(he, current_value)))
			{
				// Rather ignore the value than destroying the whole FormValue/HTML_Element
				OpStatus::Ignore(SetInternalValueFromText(current_value.CStr()));
			}
		}
		clone->GetHasValueBool() = GetHasValueBool();
		clone->m_datetime = m_datetime;
		OP_ASSERT(!clone->IsValueExternally());
	}
	return clone;
}

/* virtual */
OP_STATUS FormValueWF2DateTime::ResetToDefault(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == this);

	ResetChangedFromOriginal();

	const uni_char* default_value = he->GetStringAttr(ATTR_VALUE);
	if (default_value)
	{
		DateTimeSpec date;
		WeekSpec week;
		MonthSpec month;
		if (m_date_type == DATE && !date.m_date.SetFromISO8601String(default_value) ||
			m_date_type == TIME && !date.m_time.SetFromISO8601String(default_value) ||
			m_date_type == WEEK && !week.SetFromISO8601String(default_value) ||
			m_date_type == MONTH && !month.SetFromISO8601String(default_value) ||
			m_date_type == DATETIME && !date.SetFromISO8601String(default_value, TRUE) ||
			m_date_type == DATETIME_LOCAL && !date.SetFromISO8601String(default_value, FALSE))
		{
			default_value = NULL;
		}
	}

	return SetValueFromText(he, default_value);
}

/* virtual */
OP_STATUS FormValueWF2DateTime::GetValueAsText(HTML_Element* he,
											   OpString& out_value) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueWF2DateTime*>(this));
	if(IsValueExternally())
	{
		return GetFormObjectValue(he, out_value);
	}

	if (!GetHasValueBool())
	{
		out_value.Empty();
	}
	else
	{
		return GetInternalValueAsText(out_value);
	}
	return OpStatus::OK;
}

/* static */
OP_STATUS FormValueWF2DateTime::ConvertDateTimeToText(ExactType type, DateTimeSpec datetime, OpString& out_value)
{
	uni_char* value_buf;
	unsigned int value_str_len;
	if (type == DATE)
	{
		value_str_len = datetime.m_date.GetISO8601StringLength();
	}
	else if (type == TIME)
	{
		value_str_len = datetime.m_time.GetISO8601StringLength();
	}
	else if (type == WEEK)
	{
		// A week is represented by the monday
		WeekSpec week/* = datetime.m_day.GetWeek();
		OP_ASSERT(week.GetFirstDay() == datetime.m_day)*/;
		value_str_len = week.GetISO8601StringLength();
	}
	else if (type == MONTH)
	{
		// A month is represented by the year/month part
		MonthSpec month/* = { datetime.m_day.m_year, datetime.m_day, m_month } */;
		value_str_len = month.GetISO8601StringLength();
	}
	else
	{
		OP_ASSERT(type == DATETIME || type == DATETIME_LOCAL);
		BOOL has_timezone = (type == DATETIME);
		value_str_len = datetime.GetISO8601StringLength(has_timezone);
	}

	value_buf = out_value.Reserve(value_str_len+1);
	if (!value_buf)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	if (type == DATE)
	{
		datetime.m_date.ToISO8601String(value_buf);
	}
	else if (type == TIME)
	{
		datetime.m_time.ToISO8601String(value_buf);
	}
	else if (type == WEEK)
	{
		// A week is represented by the monday
		WeekSpec week = datetime.m_date.GetWeek();
		OP_ASSERT(week.GetFirstDay().m_day == datetime.m_date.m_day);
		week.ToISO8601String(value_buf);
	}
	else if (type == MONTH)
	{
		// A month is represented by the year/month part
		MonthSpec month = { datetime.m_date.m_year, datetime.m_date.m_month };
		month.ToISO8601String(value_buf);
	}
	else
	{
		OP_ASSERT(type == DATETIME || type == DATETIME_LOCAL);
		BOOL has_timezone = (type == DATETIME);
		datetime.ToISO8601String(value_buf, has_timezone);
	}
	return OpStatus::OK;
}

OP_STATUS FormValueWF2DateTime::GetInternalValueAsText(OpString& out_value) const
{
	OP_ASSERT(GetHasValueBool());
	return ConvertDateTimeToText(m_date_type, m_datetime, out_value);
}

/* virtual */
OP_STATUS FormValueWF2DateTime::SetValueFromText(HTML_Element* he,
												 const uni_char* in_value)
{
	OP_ASSERT(he->GetFormValue() == this);

	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		OP_ASSERT(form_object); // If they said we was external, then we must have a form_object
		return form_object->SetFormWidgetValue(in_value);
	}

	return SetInternalValueFromText(in_value);
}

/* private */
OP_STATUS FormValueWF2DateTime::SetInternalValueFromText(const uni_char* in_value)
{
	if (!in_value || !*in_value)
	{
		GetHasValueBool() = FALSE;
	}
	else
	{
		BOOL result;
		DateTimeSpec date;
		date.Clear(); // Silence compiler

		if (m_date_type == DATE)
		{
			result = date.m_date.SetFromISO8601String(in_value);
		}
		else if (m_date_type == TIME)
		{
			result = date.m_time.SetFromISO8601String(in_value) > 0;
		}
		else if (m_date_type == WEEK)
		{
			WeekSpec week;
			result = week.SetFromISO8601String(in_value);
			if (result)
			{
                date.m_date = week.GetFirstDay();
			}
		}
		else if (m_date_type == MONTH)
		{
			MonthSpec month;
			result = month.SetFromISO8601String(in_value);
			if (result)
			{
                date.m_date.m_year = month.m_year;
				date.m_date.m_month = month.m_month;
			}
		}
		else
		{
			OP_ASSERT(m_date_type == DATETIME || m_date_type == DATETIME_LOCAL);
			BOOL has_timezone = (m_date_type == DATETIME);
			result = date.SetFromISO8601String(in_value, has_timezone) > 0;
		}
		if (!result)
		{
			return OpStatus::ERR;
		}
		m_datetime = date;
		GetHasValueBool() = TRUE;
	}

	return OpStatus::OK;
}

/* static */
OP_STATUS FormValueWF2DateTime::ConstructFormValueWF2DateTime(HTML_Element* he,
															  FormValue*& out_value)
{
	ExactType type;
	OP_ASSERT(he->IsMatchingType(HE_INPUT, NS_HTML));
	switch(he->GetInputType())
	{
	case INPUT_DATE:
		type = DATE;
		break;
	case INPUT_TIME:
		type = TIME;
		break;
	case INPUT_WEEK:
		type = WEEK;
		break;
	case INPUT_MONTH:
		type = MONTH;
		break;
	case INPUT_DATETIME:
		type = DATETIME;
		break;
	default:
		OP_ASSERT(FALSE);
		// fallthrough
	case INPUT_DATETIME_LOCAL:
		type = DATETIME_LOCAL;
		break;
	}

	FormValueWF2DateTime* date_value = OP_NEW(FormValueWF2DateTime, (type));
	if (!date_value)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	const uni_char* html_value = he->GetStringAttr(ATTR_VALUE);

	if (html_value)
	{
		OP_STATUS status = date_value->SetInternalValueFromText(html_value);
		if (OpStatus::IsMemoryError(status))
		{
			OP_DELETE(date_value);
			return status;
		}
	}

	out_value = date_value;
	return OpStatus::OK;
}

/* virtual */
BOOL FormValueWF2DateTime::Externalize(FormObject* form_object_to_seed)
{
	if (!FormValue::Externalize(form_object_to_seed))
	{
		return FALSE; // It was wrong to call Externalize
	}
	if (GetHasValueBool())
	{
		OpString val_as_text;
		// Widget will be empty on OOM, not much else to do
		OpStatus::Ignore(GetInternalValueAsText(val_as_text));
		form_object_to_seed->SetFormWidgetValue(val_as_text.CStr());
	}
	else
	{
		form_object_to_seed->SetFormWidgetValue(NULL);
	}
	return TRUE;
}

/* virtual */
void FormValueWF2DateTime::Unexternalize(FormObject* form_object_to_replace)
{
	if (IsValueExternally())
	{
		GetHasValueBool() = FALSE;
		OpString val_as_text;
		OP_STATUS status = form_object_to_replace->GetFormWidgetValue(val_as_text);
		if (OpStatus::IsSuccess(status))
		{
			SetInternalValueFromText(val_as_text.CStr());
		}
		FormValue::Unexternalize(form_object_to_replace);
	}
}

/* static */
void FormValueWF2DateTime::GetStepLimits(ExactType type,
						const uni_char* min_attr, const uni_char* max_attr,
						const uni_char* step_attr,
						double& min_value, double& max_value,
						double& step_base, double& step_value)
{
	step_base = 0.0;
	if (type == DATE)
	{
		// Default
		min_value = -DBL_MAX;
		max_value = DBL_MAX; // arbitrary
		step_value = 86400000.0; // one day in milliseconds

		if (max_attr)
		{
			DaySpec max_value_spec;
			if (max_value_spec.SetFromISO8601String(max_attr))
			{
				max_value = max_value_spec.AsDouble() * 86400000.0;
				step_base = max_value;
			}
		}
		if (min_attr)
		{
			DaySpec min_value_spec;
			if (min_value_spec.SetFromISO8601String(min_attr))
			{
				min_value = min_value_spec.AsDouble() * 86400000.0;
				step_base = min_value;
			}
		}

		if (step_attr)
		{
			// in days, to be converted to milliseconds
			double step_spec;
			BOOL legal_step = WebForms2Number::StringToDouble(step_attr, step_spec);
			if (legal_step && step_spec == static_cast<int>(step_spec) && step_spec >= 0.0)
			{
				step_value = step_spec * 86400000.0;
			}
			else if (uni_stri_eq(step_attr, "any"))
			{
				step_value = 0.0;
			}
		}
	}
	else if (type == TIME)
	{
		// Default
		min_value = 0;
		max_value = 86399999.0;
		step_value = 60000; // one minute

		if (max_attr)
		{
			TimeSpec max_value_spec;
			if (max_value_spec.SetFromISO8601String(max_attr))
			{
				max_value = max_value_spec.AsDouble() * 1000.0;
				step_base = max_value;
			}
		}
		if (min_attr)
		{
			TimeSpec min_value_spec;
			if (min_value_spec.SetFromISO8601String(min_attr))
			{
				min_value = min_value_spec.AsDouble() * 1000.0;
				step_base = min_value;
			}
		}

		if (step_attr)
		{
			// in seconds, to be converted to milliseconds
			double step_spec;
			BOOL legal_step = WebForms2Number::StringToDouble(step_attr, step_spec);
			if (legal_step && step_spec >= 0.0)
			{
				step_value = step_spec * 1000.0;
			}
			else if (uni_stri_eq(step_attr, "any"))
			{
				step_value = 0.0;
			}
		}
	}
	else if (type == WEEK)
	{
		// A week is represented by the monday that week
		// Default
		min_value = -DBL_MAX;
		max_value = DBL_MAX;
		step_value = 7*86400000; // one week

		if (max_attr)
		{
			WeekSpec max_value_spec;
			if (max_value_spec.SetFromISO8601String(max_attr))
			{
				max_value = max_value_spec.GetFirstDay().AsDouble()*86400000;
				step_base = max_value;
			}
		}
		if (min_attr)
		{
			WeekSpec min_value_spec;
			if (min_value_spec.SetFromISO8601String(min_attr))
			{
				min_value = min_value_spec.GetFirstDay().AsDouble()*86400000;
				step_base = min_value;
			}
		}

		if (step_attr)
		{
			// in weeks, to be converted to milliseconds
			double step_spec;
			BOOL legal_step = WebForms2Number::StringToDouble(step_attr, step_spec);
			if (legal_step && step_spec >= 0.0)
			{
				step_value = step_spec * (7 * 86400000);
			}
			else if (uni_stri_eq(step_attr, "any"))
			{
				step_value = 0.0;
			}
		}
	}
	else if (type == MONTH)
	{
		// A month is represented by the m_year and m_month part of DaySpec
		// Default
		min_value = -DBL_MAX;
		max_value = DBL_MAX;
		step_value = 1; // one week

		if (max_attr)
		{
			MonthSpec max_value_spec;
			if (max_value_spec.SetFromISO8601String(max_attr))
			{
				max_value = max_value_spec.AsDouble();
				step_base = max_value;
			}
		}
		if (min_attr)
		{
			MonthSpec min_value_spec;
			if (min_value_spec.SetFromISO8601String(min_attr))
			{
				min_value = min_value_spec.AsDouble();
				step_base = min_value;
			}
		}

		if (step_attr)
		{
			// in months
			double step_spec;
			BOOL legal_step = WebForms2Number::StringToDouble(step_attr, step_spec);
			if (legal_step && step_spec >= 0.0)
			{
				step_value = step_spec;
			}
			else if (uni_stri_eq(step_attr, "any"))
			{
				step_value = 0.0;
			}
		}
	}
	else
	{
		OP_ASSERT(type == DATETIME || type == DATETIME_LOCAL);
		BOOL has_timezone = (type == DATETIME);

		// Default
		min_value = -DBL_MAX;
		max_value = DBL_MAX;
		step_value = 60000; // one minute

		if (max_attr)
		{
			DateTimeSpec max_value_spec;
			if (max_value_spec.SetFromISO8601String(max_attr, has_timezone))
			{
				max_value = max_value_spec.AsDouble();
				step_base = max_value;
			}
		}
		if (min_attr)
		{
			DateTimeSpec min_value_spec;
			if (min_value_spec.SetFromISO8601String(min_attr, has_timezone))
			{
				min_value = min_value_spec.AsDouble();
				step_base = min_value;
			}
		}

		if (step_attr)
		{
			// in seconds, to be converted to milliseconds
			double step_spec;
			BOOL legal_step = WebForms2Number::StringToDouble(step_attr, step_spec);
			if (legal_step && step_spec >= 0.0)
			{
				step_value = step_spec * 1000.0;
			}
			else if (uni_stri_eq(step_attr, "any"))
			{
				step_value = 0.0;
			}
		}
	}
}

OP_STATUS FormValueWF2DateTime::SetValueFromNumber(HTML_Element* he, double value_as_number)
{
	OP_ASSERT(he->IsMatchingType(HE_INPUT, NS_HTML));
	DateTimeSpec resulting_date;
	resulting_date.Clear();
	if (m_date_type == MONTH)
	{
		int month_count = static_cast<int>(value_as_number);
		resulting_date.m_date.m_year = 1970 + month_count / 12;
		resulting_date.m_date.m_month = month_count % 12;
	}
	else if (m_date_type == TIME)
	{
		// Try to avoid the second and millisecond part.
		TimeSpec time;
		time.Clear();
		// This will keep the previous precision or increase it.
		time.SetFromNumber(static_cast<int>(op_fmod(value_as_number, 86400000)), 1000);
		resulting_date.m_time = time;
	}
	else
	{
		resulting_date.SetFromTime(value_as_number);
	}

	OpString as_text;
	RETURN_IF_ERROR(ConvertDateTimeToText(m_date_type, resulting_date, as_text));
	return SetValueFromText(he, as_text.CStr());
}

/* static */
double FormValueWF2DateTime::ConvertDateTimeToNumber(ExactType type, DateTimeSpec datetime)
{
	if (type == MONTH)
	{
		MonthSpec month = { datetime.m_date.m_year, datetime.m_date.m_month };
		return month.AsDouble();
	}

	if (type == TIME)
	{
		datetime.m_date.m_year = 1970;
		datetime.m_date.m_month = 0;
		datetime.m_date.m_day = 1;
	}
	else if (type == DATE || type == WEEK)
	{
		datetime.m_time.m_hour = 0;
		datetime.m_time.m_minute = 0;
		datetime.m_time.SetSecondUndefined();
		datetime.m_time.SetFractionUndefined();
	}

	return datetime.AsDouble();
}


/* virtual */
OP_STATUS FormValueWF2DateTime::StepUpDown(HTML_Element* he, int step_count)
{
	OP_ASSERT(he->GetFormValue() == this);
	OpString current_value;
	if (IsValueExternally())
	{
		RETURN_IF_ERROR(GetFormObjectValue(he, current_value));
		if (OpStatus::IsError(SetInternalValueFromText(current_value.CStr())))
		{
			// Someone has something strange in the widget
			return OpStatus::ERR_NOT_SUPPORTED;
		}
	}

	if (!GetHasValueBool())
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	double min_value;
	double max_value;
	double step_base;
	double step_value;

	GetStepLimits(m_date_type,
				he->GetStringAttr(ATTR_MIN),
				  he->GetStringAttr(ATTR_MAX),
				  he->GetStringAttr(ATTR_STEP),
				  min_value, max_value, step_base, step_value);
	if (step_value == 0.0)
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	double value = ConvertDateTimeToNumber(m_date_type, m_datetime);

	double out_value_num;
	OP_STATUS status = WebForms2Number::StepNumber(value, min_value, max_value,
		step_base, step_value, step_count, FALSE, /* wrap_around */
		TRUE, /* fuzzy_snapping */ out_value_num);
	RETURN_IF_ERROR(status);

/*	ValidationResult val_res = VALIDATE_OK;
	val_res.SetErrorsFrom(FormValidator::ValidateNumberForMinMaxStep(he, out_value_num));
	if (!val_res.IsOk())
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}
*/
	return SetValueFromNumber(he, out_value_num);
}

const uni_char* FormValueWF2DateTime::GetFormValueTypeString() const
{
	switch (m_date_type)
	{
	case DATE:
		return UNI_L("date");
	case TIME:
		return UNI_L("time");
	case DATETIME:
		return UNI_L("datetime");
	case DATETIME_LOCAL:
		return UNI_L("datetime-local");
	case MONTH:
		return UNI_L("month");
	default:
		OP_ASSERT(FALSE);
		// fallthrough
	case WEEK:
		return UNI_L("week");
	}
}
