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
#include "core/pch.h"

#include "modules/forms/src/validationerrorwindow.h"
#include "modules/forms/piforms.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/WidgetWindow.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/logdoc/htm_elm.h"

#define VALIDATION_ERROR_AUTOHIDE_DELAY 9000

/* static */
void ValidationErrorWindow::Display(HTML_Element* form_control_element, const uni_char* message)
{
	OP_ASSERT(form_control_element);
	OP_ASSERT(message);

	FormObject* form_object = form_control_element->GetFormObject();
	if (!form_object)
	{
		// Hmmm
		OP_ASSERT(!"Don't call this without a form object and a widget you cluns");
		return;
	}
	ValidationErrorWindow* previous_error_window = form_object->GetValidationError();
	if (previous_error_window)
	{
		previous_error_window->Close();
		OP_ASSERT(!form_object->GetValidationError()); // The Close should have deleted the previous_error_window and unregistered it
	}

	ValidationErrorWindow* window;
	if (OpStatus::IsSuccess(ValidationErrorWindow::Create(window, form_object, message)))
	{
		form_object->SetValidationError(window);
		window->Show();
	}
}

ValidationErrorWindow::~ValidationErrorWindow()
{
	if (m_form_object)
	{
		m_form_object->SetValidationError(NULL);
	}
	m_timeout_timer.Stop();
}

void ValidationErrorWindow::Show()
{
	m_validation_bubble.Show();
	m_timeout_timer.SetTimerListener(this);
	m_timeout_timer.Start(VALIDATION_ERROR_AUTOHIDE_DELAY);
}

void ValidationErrorWindow::Close()
{
	m_validation_bubble.Close();
	// Emulate window behaviour and delete this since FormObject expects it.
	OP_DELETE(this);
}

/*virtual */
void ValidationErrorWindow::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer && timer == &m_timeout_timer);
	Close();
}

void ValidationErrorWindow::HandleNewWidgetPosition()
{
	UpdatePlacementAndColors();
}

void ValidationErrorWindow::UpdatePlacementAndColors()
{
	// Let the error message follow the widget
	m_validation_bubble.UpdateFontAndColors(m_form_object->GetWidget());
	m_validation_bubble.UpdatePlacement(m_form_object->GetWidget()->GetScreenRect());
}

/* static */
OP_STATUS ValidationErrorWindow::Create(ValidationErrorWindow*& window, FormObject* form_object, const uni_char* message)
{
	OP_ASSERT(form_object);
	OP_ASSERT(message);

	window = OP_NEW(ValidationErrorWindow, (form_object));
	if (!window)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OpWidget* widget = form_object->GetWidget();
	OpWindow* parent_window = widget->GetParentOpWindow();
	OP_STATUS status = window->m_validation_bubble.CreateBubble(parent_window, message);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(window);
		return status;
	}

	window->UpdatePlacementAndColors();
	return OpStatus::OK;
}
