 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file webforms2number.h
 *
 * Help classes for the web forms 2 support. Numbers mostly
 *
 * @author Daniel Bratell
 */

#ifndef _WEBFORMS2NUMBER_H_
#define _WEBFORMS2NUMBER_H_

/**
 * Helper object with methods to handle the numbers in WF2.
 */
class WebForms2Number
{
private:
	WebForms2Number(); // Never instantiated

public:
	/**
	 * To convert a string to a double and also check if the number is
	 * legally formed, i.e. of the form:
	 *
	 * [-][0-9]+[.[0-9]+][e[-][0-9]+]
	 *
	 * and returns the value in the out parameter.
	 *
	 * @return TRUE if the string could be fully parsed, FALSE if it
	 * wasn't a legal number.
	 */
	static BOOL StringToDouble(const uni_char* number_str, double& number_out);

	/**
	 * Returns the number as a unicode string.
	 *
	 * @param number The number to convert.
	 * @param str_out A buffer, at least 32 bytes long (OpDoubleFormat requirement.)
	 * If it is to short we will crash so don't do that.
	 *
	 * @returns OpStatus::OK if success. OpStatus::ERR_NO_MEMORY if a
	 * memory error.
	 */
	static OP_STATUS DoubleToString(double number, uni_char* str_out);

	/**
	 * Moves the number one step up.
	 *
	 * @param number number to step.
	 * @param elm_with_restrictions The element with the step, min and max
	 * attributes for the number.
	 */
	static double StepUp(double number, HTML_Element* elm_with_restrictions);

	/**
	 * Moves the number one step down.
	 *
	 * @param number number to step.
	 * @param elm_with_restrictions The element with the step, min and max
	 * attributes for the number.
	 */
	static double StepDown(double number, HTML_Element* elm_with_restrictions);

	/**
	 * Sets the numbers if there are anything specified on the
	 * element, or to the default otherwise.
	 *
	 * @param elm_with_restrictions The element with the step, min and max
	 * attributes for the number.
	 * @param min will get the min value (-DBL_MAX if none specified)
	 * @param max will get the min value (DBL_MAX if none specified)
	 * @param step_base the base of the steps.
	 * @param step will get the value 0 if the step is specified as
	 * "any".
	 */
	static void GetNumberRestrictions(HTML_Element* elm_with_restrictions,
									  double& min,
									  double& max,
									  double& step_base,
									  double& step);

	/**
	 * This method tries to create a number on the exact step
	 * position. First it rounds to the next step position in the
	 * direction we're moving, unless we're already very close to a
	 * stap position. Then the number is corrected by taking errors in
	 * the floating point arithemetics into consideration.
	 *
	 * @param unsnapped_number The number to fix.
	 * @param step The step.
	 * @param step_base The base for all steps.
	 * @param direction which way we've changed the number to get to the
	 * "unsnapped" number
	 */
	static double SnapToStep(double unsnapped_number,
							 double step_base, double step,
							 int direction);
	/**
	 * @returns OpStatus::ERR_NOT_SUPPORTED If the step is 0
	 * ("any"). Returns OpStatus::ERR_OUT_OF_RANGE if the step brought
	 * us outside of the allowed range or, if fuzzy_snapping == FALSE,
	 * if the number wasn't on an allowed step value.
	 */
	static OP_STATUS StepNumber(double in_value,
								double min_value, double max_value,
								double step_base, double step_value,
								int step_count, BOOL wrap_around,
								BOOL fuzzy_snapping,
								double& out_value);

	/**
	 * @returns a number between 0 and 1 (based on the value and max attributes) if the progress bar
	 * is determinate. Otherwise it returns -1.
	 * @param doc The document owning the element.
	 * @param he The element. Must be element of type HE_PROGRESS.
	 */
	static double GetProgressPosition(FramesDocument* doc, HTML_Element *he);

	/**
	 * Get the value, min, max, low, high, optimum values from a meter element.
	 * @param doc The document owning the element.
	 * @param he The element. Must be element of type HE_METER.
	 * @param val The value after sanity checking.
	 * @param min The minimum value after sanity checking.
	 * @param max The maximum value after sanity checking.
	 * @param low The low boundary value after sanity checking.
	 * @param high The high boundary value after sanity checking.
	 * @param optimum The optimum value after sanity checking.
	 */
	static void GetMeterValues(FramesDocument* doc, HTML_Element *he, double &val,
														double &min, double &max,
														double &low, double &high,
														double &optimum);
};

#endif // _WEBFORMS2NUMBER_H_
