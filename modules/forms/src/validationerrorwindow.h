 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/**
 * @author Daniel Bratell
 */

#ifndef _VALIDATION_ERROR_WINDOW_H_
#define _VALIDATION_ERROR_WINDOW_H_

#include "modules/hardcore/timer/optimer.h"
#include "modules/widgets/OpBubbleHandler.h"

class FormObject;
class HTML_Element;
class OpMultilineEdit;

/**
 * This is the handler for things related to the message displayed for validation errors.
 */
class ValidationErrorWindow : private OpTimerListener
{
public:

	/**
	 * Displays a validation error message for a couple of seconds in connection
	 * to the form element |elm|. This method will swallow any oom errors.
	 *
	 * @param[in] elm The form element.
	 *
	 * @param[in] message The message to show.
	 */
	static void Display(HTML_Element* elm, const uni_char* message);

	/**
	 * And destroy it. Called indirectly from Close().
	 */
	virtual ~ValidationErrorWindow();

	/**
	 * Call to display the error message. It will show for a while and then
	 * self close and delete itself. Inline since there are only one caller.
	 */
	void Show();

	/**
	 * Close the error message.
	 */
	void Close();

	/**
	 * Controls flashing and closing.
	 */
	virtual void OnTimeOut(OpTimer* timer);

	/**
	 * Call this to move the window around, following the widget.
	 */
	void HandleNewWidgetPosition();

	/**
	 * Update placement and colors of the window.
	 */
	void UpdatePlacementAndColors();

	/**
	 * Create a error message. Call Show() to get it going.
	 */
	static OP_STATUS Create(ValidationErrorWindow*& window,
							FormObject* form_object,
							const uni_char* message);
private:
	/**
	 * Create a FormObject.
	 */
	ValidationErrorWindow(FormObject* form_object) :
	  m_form_object(form_object)
	  {
	  }

private:
	OpTimer m_timeout_timer;
	FormObject* m_form_object;
	/**
	 * The handler of the validation error message bubble.
	 */
	OpBubbleHandler m_validation_bubble;
};

#endif // _VALIDATION_ERROR_WINDOW_H_
