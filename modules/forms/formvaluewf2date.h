/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMVALUEWF2DATE_H
#define FORMVALUEWF2DATE_H

#include "modules/forms/formvalue.h"
#include "modules/logdoc/logdocenum.h" // For InputType


/**
 * Represents an input of type time/date/datetime/datetime-local.
 *
 * @author Daniel Bratell
 */
class FormValueWF2DateTime : public FormValue
{
public:
	// Use base class documentation
	virtual FormValue* Clone(HTML_Element *he);

	/**
	 * "Cast" the FormValue to this class. This asserts in debug
	 * builds if done badly. This should be inlined and be as
	 * cheap as a regular cast. If it turns out it's not we
	 * might have to redo this design.
	 *
	 * @param val The FormValue pointer that really is a pointer to
	 * an object of this class.
	 */
	static FormValueWF2DateTime* GetAs(FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_DATETIMERELATED);
		return static_cast<FormValueWF2DateTime*>(val);
	}
	static const FormValueWF2DateTime* GetAs(const FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_DATETIMERELATED);
		return static_cast<const FormValueWF2DateTime*>(val);
	}

	// Use base class documentation
	virtual OP_STATUS ResetToDefault(HTML_Element* he);

	// Use base class documentation
	virtual OP_STATUS GetValueAsText(HTML_Element* he, OpString& out_value) const;

	static OP_STATUS ConstructFormValueWF2DateTime(HTML_Element* he,
											FormValue*& out_value);
	// Use base class documentation
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
									   const uni_char* in_value);
	// Use base class documentation
	virtual BOOL Externalize(FormObject* form_object_to_seed);
	// Use base class documentation
	virtual void Unexternalize(FormObject* form_object_to_replace);
	// Use base class documentation
	virtual OP_STATUS StepUpDown(HTML_Element* he, int step_count);

	/**
	 * Returns the string that would suit this if used in &lt;input type="...."&gt;
	 * Needed when we get OOM during a type change to restore the old type.
	 */
	const uni_char* GetFormValueTypeString() const;

private:
	const enum ExactType
	{
		DATETIME, // Full m_datetime
		DATE, // m_datetime.m_date,
		WEEK, // m_datetime.m_date, but only mondays are allowed
		MONTH, // Only part of m_datetime.m_date (year and month)
		TIME, // m_datetime.m_time
		DATETIME_LOCAL // Full m_datetime
	} m_date_type; // FIXME, put in the extra bits in reserved char 1.
	FormValueWF2DateTime(ExactType type) : FormValue(VALUE_DATETIMERELATED), m_date_type(type) {};
	/**
	 * Sets the internal value in this FormValue by parsing the
	 * given string.
	 *
	 * @param in_value The value to set this FormValue to.
	 */
	OP_STATUS SetInternalValueFromText(const uni_char* in_value);
	OP_STATUS GetInternalValueAsText(OpString& out_value) const;
	static void GetStepLimits(ExactType type,
						const uni_char* min_attr, const uni_char* max_attr,
						const uni_char* step_attr,
						double& min_value, double& max_value,
						double& step_base, double& step_value);
	OP_STATUS SetValueFromNumber(HTML_Element* he, double value_as_number);
	/** Returns the date as the number of milliseconds since 1970-01-01 00:00 */
	static double ConvertDateTimeToNumber(ExactType type, DateTimeSpec datetime);
	static OP_STATUS ConvertDateTimeToText(ExactType type, DateTimeSpec datetime, OpString& out_value);

	unsigned char& GetHasValueBool() { return GetReservedChar1(); }
	const unsigned char& GetHasValueBool() const { return GetReservedChar1(); }
	DateTimeSpec m_datetime;
	// Selection?
};

#endif // FORMVALUEWF2DATE_H
