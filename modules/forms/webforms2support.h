/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file webforms2support.h
 *
 * Help classes for the web forms 2 support. Dates mostly.
 *
 * @author Daniel Bratell
 */

#ifndef _WEBFORMS2SUPPORT_H_
#define _WEBFORMS2SUPPORT_H_

#include "modules/dom/domeventtypes.h"
#include "modules/forms/formmanager.h"

class TempBuffer;

/**
 * These values come from the standard and are used in the DOM interface.
 * Each represents a specific validation error (except VALIDATE_OK).
 */
enum ValidationResultFlag
{
	VALIDATE_OK = 0x0, // Not a flag...
	/**
	 * The data entered does not match the type of the control.  For
	 * example, if the UA allows uninterpreted arbitrary text entry
	 * for expdate fields, and the user has entered SEP02, then this
	 * error code would be used. This code is also used when the
	 * selected file in a file upload control does not have an
	 * appropriate MIME type. If the control is empty, this flag will
	 * not be set.
	 */
	VALIDATE_ERROR_TYPE_MISMATCH = 0x1,
	/**
	 * The numeric, date, or time value of a field with a min
	 * attribute is lower than the minimum, or a file upload control
	 * has fewer files selected than the minimum. If the control is
	 * empty, this flag will not be set.
	 */
	VALIDATE_ERROR_RANGE_UNDERFLOW = 0x2,
	/**
	 * The numeric, date, or time value of a field with a max
	 * attribute is higher than the maximum, or a file upload control
	 * has more files selected than the maximum. If the control is
	 * empty, this flag will not be set.
	 */
	VALIDATE_ERROR_RANGE_OVERFLOW = 0x4,
	/**
	 */
	VALIDATE_ERROR_STEP_MISMATCH = 0x8,
	/**
	 * The value of a field with a maxlength attribute is longer than
	 * the attribute allows.
	 */
	VALIDATE_ERROR_TOO_LONG = 0x10,
	/**
	 * The value of the field with a pattern attribute doesn't match
	 * the pattern. If the control is empty, this flag will not be
	 * set.
	 */
	VALIDATE_ERROR_PATTERN_MISMATCH = 0x20,
	/**
	 * The field has the required attribute set but has no value.
	 */
	VALIDATE_ERROR_REQUIRED = 0x40,
	/**
	 * The field was marked invalid from script. See the definition of
	 * the setCustomValidity() method.
	 */
	VALIDATE_ERROR_CUSTOM = 0x8000
};

/**
 * Information about validation. Many different validation error flags can be
 * set. This class is small and simple and should be treated by value.
 *
 * @author Daniel Bratell
 */
class ValidationResult
{
private:
	/**
	 * The bitset with flags.
	 */
	int m_flags;
public:
	/**
	 * Constructs a validation result given an initial flag to set (or
	 * VALIDATE_OK if none should be set).
	 *
	 * @param init_flag The initial value.
	 */
	ValidationResult(ValidationResultFlag init_flag) : m_flags(init_flag) {}
	/**
	 * If the validation is ok.
	 *
	 * @returns TRUE if the ValidationResult contains no validation errors.
	 */
	BOOL IsOk() const { return m_flags == 0; }
	/**
	 * Checks a specific error.
	 *
	 * @param error The specific error to look for. VALIDATE_OK is not a
	 * legal input here.
	 *
	 * @returns TRUE if the result contains the specified validation error.
	 */
	BOOL HasError(ValidationResultFlag error) const { return (error & m_flags) != 0; }
	/**
	 * Set a specific error in this ValidationResult.
	 *
	 * @param error The error set.
	 */
	void SetError(ValidationResultFlag error) { m_flags |= error; }
	/**
	 * Clears a specific error in this ValidationResult.
	 *
	 * @param error The error to clear.
	 */
	void ClearError(ValidationResultFlag error) { m_flags &= ~error; }

	/**
	 * Copies all set errors from another ValidationResult and keeps the
	 * errors already set in this ValidationResult.
	 *
	 * @param other The other ValidationResult.
	 */
	void SetErrorsFrom(ValidationResult other) { m_flags |= other.m_flags; }
};

class FramesDocument;
class HTML_Element;
class ES_Thread;

/**
 * Small (a couple of bytes) helper object that contains methods validating
 * forms and taking the appropriate actions.
 */
class FormValidator
{
private:
	/**
	 * The frames document for the form. Set in the constructor.
	 */
	FramesDocument*		m_frames_doc;

	/**
	 * The thread that blocks the invalid event threads or NULL if there are no invalid event threads.
	 */
	ES_Thread*			m_invalid_event_blocker;

	/**
	 * If any invalid events were sent through DOM.
	 */
	BOOL				m_invalid_events_sent_to_dom;

	/**
	 * As ValidateSingleControlInternal but doesn't check if the control should be checked at all.
	 */
	ValidationResult ValidateSingleControlInternal(HTML_Element* form_control_element, BOOL use_custom_value, const uni_char* custom_value) const;

	/**
	 * Does validation specific for URI fields. This doesn't include
	 * pattern and max length but might include min/max/format/precision.
	 *
	 * @param fhe The form control element.
	 * @param value The current value.
	 */
	static ValidationResult	ValidateUri(HTML_Element* fhe,
										const uni_char* value);
	/**
	 * Does validation specific for date fields. This doesn't include
	 * pattern and max length but might include min/max/format/precision.
	 *
	 * @param fhe The form control element.
	 * @param value The current value.
	 */
	static ValidationResult	ValidateDate(HTML_Element* fhe,
										 const uni_char* value);
	/**
	 * Does validation specific for week fields. This doesn't include
	 * pattern and max length but might include min/max/format/precision.
	 *
	 * @param fhe The form control element.
	 * @param value The current value.
	 */
	static ValidationResult	ValidateWeek(HTML_Element* fhe,
										 const uni_char* value);
	/**
	 * Does validation specific for time fields. This doesn't include
	 * pattern and max length but might include min/max/format/precision.
	 *
	 * @param fhe The form control element.
	 * @param value The current value.
	 */
	static ValidationResult	ValidateTime(HTML_Element* fhe,
										 const uni_char* value);
	/**
	 * Does validation specific for email fields. This doesn't include
	 * pattern and max length but might include min/max/format/precision.
	 *
	 * @param fhe The form control element.
	 * @param value The current value.
	 * @param multiple TRUE if multiple addresses separated by comma should be valid.
	 */
	static ValidationResult	ValidateEmail(const uni_char* value, BOOL multiple);
	/**
	 * Does validation specific for number and range fields. This
	 * doesn't include pattern and max length but might include
	 * min/max/format/precision.
	 *
	 * @param fhe The form control element.
	 * @param value The current value.
	 */
	static ValidationResult	ValidateNumber(HTML_Element* fhe,
										   const uni_char* value);

	/**
	 * Does validation specific for month fields. This doesn't
	 * include pattern and max length but might include
	 * min/max/format/precision.
	 *
	 * @param fhe The form control element.
	 * @param value The current value.
	 */
	static ValidationResult	ValidateMonth(HTML_Element* fhe,
											const uni_char* value);
	/**
	 * Does validation specific for datetime fields. This doesn't include
	 * pattern and max length but might include min/max/format/precision.
	 * It can handle both INPUT_DATETIME and INPUT_DATETIME_LOCAL.
	 *
	 * @param fhe The form control element.
	 * @param value The current value.
	 */
	static ValidationResult	ValidateDateTime(HTML_Element* fhe,
											 const uni_char* value);
#if 0
	/**
	 * Does validation specific for location fields. This doesn't include
	 * pattern and max length but might include min/max/format/precision.
	 *
	 * @param fhe The form control element.
	 * @param value The current value.
	 */
	static ValidationResult	ValidateLocation(HTML_Element* fhe,
											 const uni_char* value);
#endif
	/**
	 * Does validation specific for file upload fields.
	 *
	 * @param fhe The form control element.
	 * @param value The current value.
	 */
	static ValidationResult ValidateFile(HTML_Element* fhe,
										 const uni_char* value);

	/**
	 * @todo Replace with help class in datetime.h
	 */
	static int ParseMonth(const uni_char* value);
	/**
	 * @todo Replace with help class in datetime.h
	 */
	static int ParseWeek(const uni_char* value);

	/**
	 * Checks the length of the text in a text element.
	 */
	static ValidationResult CheckTextMaxLength(HTML_Element* form_control_element,
											   const uni_char* value);

	/**
	 * Checks an element agains the specified pattern.
	 */
	static ValidationResult CheckPattern(HTML_Element* form_control_element,
										 const uni_char* value);
	/**
	 * Helper method for sending an event
	 *
	 * @param event The event to send.
	 * @param form_control_element The element to send the event to.
	 * @param thread The thread that this comes from.
	 */
//	void SendEvent(DOM_EventType event,
//				   HTML_Element* form_control_element,
//				   ES_Thread* thread) const;

	/**
	 * Check if a string matches a regular expression fully.
	 *
	 * @param reg_exp The regular expression.
	 * @param a_string The string to match against the regular expression.
	 */
	static BOOL MatchesRegExp(const uni_char* reg_exp,
							  const uni_char* a_string);

	/**
	 * Checks the step attribute.
	 *
	 * @param elm A element to look for the step attribute at.
	 * @param step_base Where the steps should originate
	 * @param value The value to check.
	 * @param require_integer_step If steps must be integers.
	 * @param require_positive_step If step must be positive to be accepted.
	 * @param default_step The step to use if nothing else is found. 0.0 if no step checking should be done when the step isn't specified.
	 */
	static BOOL CheckStep(HTML_Element* elm, double step_base, double value, BOOL require_integer_step, BOOL require_positive_step, double default_step);

	/**
	 * Validates a form and creates optional oninvalid
	 * events for the non validating
	 * controls.
	 *
	 * @param form_element The form to validate
	 * @param send_invalid_event TRUE if failing controls
	 * should result in an invalid event. FALSE if no events should be sent.
	 * @param alert_as_default_action The oninvalid event should
	 * display some kind of UI message as default action and focus the control.
	 * If FALSE nothing at all will happen.
	 *
	 * @param thread The thread any events sent should interrupt.
	 */
	ValidationResult ValidateFullFormInternal(const HTML_Element* form_element,
												BOOL send_invalid_event,
												BOOL alert_as_default_action,
												ES_Thread* thread
												);

	OP_STATUS DisplayValidationErrorMessage(HTML_Element* form_control_element,
		const uni_char* message) const;

public:
	/**
	 * Constructor. Cheap. This object can very well be created on the stack.
	 *
	 * @param frames_doc The document with the form.
	 */
	FormValidator(FramesDocument* frames_doc) : m_frames_doc(frames_doc), m_invalid_event_blocker(NULL), m_invalid_events_sent_to_dom(FALSE) {};

	/**
	 * Validates a form and creates oninvalid events for the non validating
	 * controls.
	 *
	 * @param form_element The form (HE_FORM) to validate.
	 * @param alert_as_default_action The oninvalid event should
	 * display some kind of UI message as default action and focus the control.
	 * If FALSE nothing at all will happen.
	 * @param thread The thread this validation's invalid events interrupt.
	 */
	ValidationResult	ValidateForm(const HTML_Element* form_element,
									 BOOL alert_as_default_action,
									 ES_Thread* thread = NULL)
	{
		return ValidateFullFormInternal(form_element,
			TRUE /* send event */,
			alert_as_default_action,
			thread
			);
	}

	/**
	 * Validates a form control.
	 */
	ValidationResult	ValidateSingleControl(HTML_Element* form_control) const;

	/**
	 * Validates a form control.
	 */
	ValidationResult	ValidateSingleControlAgainstValue(HTML_Element* form_control, const uni_char* value) const;

	/**
	 * Fetches the validation message into TempBuffer.
	 * If there is none set, OpStatus::ERR will be returned.
	 */
	static OP_STATUS GetCustomValidationMessage(HTML_Element* form_control, TempBuffer* result_buffer);

	/**
	 * Returns the thread that's blocked by all the invalid
	 * events if there were invalid events sent to DOM. NULL otherwise.
	 */
	ES_Thread* GetInvalidEventsBlocker() { return m_invalid_events_sent_to_dom ? m_invalid_event_blocker : NULL; }

	/**
	 * Determines if the control will validate, which is not the same as
	 * if it will be included in a submit. For instance, all submit buttons
	 * have willValidate=TRUE but only the one used to submit will
	 * be submitted. See documentation of the WF2 property willValidate.
	 *
	 * @param form_control_element The element to check
	 */
	BOOL WillValidate(HTML_Element* form_control_element) const;

	/**
	 * Determines if the control can validate or if it's excluded from
	 * validation.
	 *
	 */
	BOOL CanValidate(HTML_Element* form_control_element) const;

	/**
	 * Displays an error message for the first uncaught invalidation
	 * on a form. The rest are ignored.
	 *
	 * @param form_control_element The element receiving the validation error.
	 *
	 * @returns OpStatus::OK if everything was ok or any error locale
	 * data fetching can return otherwise.
	 */
	OP_STATUS MaybeDisplayErrorMessage(HTML_Element* form_control_element) const;

	/**
	 * Do the validation without messing with the document.
	 */
	ValidationResult ValidateFormWithoutEvent(const HTML_Element* form_element)
	{
		return ValidateFullFormInternal(form_element,
			FALSE /* send event */,
			FALSE /* alert as default action */,
			NULL
			);
	}

	/**
	 * Checks the specified number agains the supplied controls
	 * max/man and step.
	 *
	 * @param fhe The form control element.
	 *
	 * @param value_number The number to check against min, max and
	 * step. This may very well be different than the current form
	 * value.
	 */
	static ValidationResult	ValidateNumberForMinMaxStep(HTML_Element* fhe,
														double value_number);

	/**
	 * Sends an OnInvalid event.
	 */
	OP_STATUS SendOnInvalidEvent(HTML_Element* form_control_element, ES_Thread* thread = NULL);

	/**
	 * Sends an OnFormInput event.
	 */
	void SendOnFormInputEvent(HTML_Element* form_control_element,
							  ES_Thread* thread) const;

	/**
	 * Sends an OnFormChange event.
	 */
	void SendOnFormChangeEvent(HTML_Element* form_control_element,
							   ES_Thread* thread) const;

	/**
	 * Sends an ONFORMCHANGE to all elements in the form.
	 *
	 * @param form_element The form element.
	 */
	void DispatchFormChange(HTML_Element* form_element) const;

	/**
	 * Sends an ONFORMINPUT to all elements in the form.
	 *
	 * @param form_element The form element.
	 */
	void DispatchFormInput(HTML_Element* form_element) const;

	/**
	 * Determines if a form element is disabled by taking disabled
	 * fieldset parents into consideration.
	 *
	 * @param form_control_element The form element that might be disabled.
	 *
	 * @returns TRUE if the element is disabled. FALSE if it's not.
	 */
	static BOOL IsInheritedDisabled(const HTML_Element* form_control_element) { return FormManager::IsInheritedDisabled(form_control_element); }

	/**
	 * Checks if a string (not NULL) matches the definition of an
	 * email address in RFC2822. Not that this method is quite slow
	 * and should not be called unless it's very likely that the
	 * string actually is an email address.  This is used by WebForms2
	 * validation of email addresses.
	 *
	 * @param string The string to check. Must not be NULL.
	 * @returns TRUE if the string matches the RFC2822 definition of an email address, FALSE otherwise.
	 */
	static BOOL IsValidEmailAddress(const uni_char* string);

	/**
	 * Returns TRUE if form_control_element is a child to a HE_DATALIST element.
	 */
	static BOOL IsInDataList(HTML_Element* form_control_element);
};

#endif // _WEBFORMS2SUPPORT_H_
