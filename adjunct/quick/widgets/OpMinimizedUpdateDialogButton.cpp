/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 *
 */

#include "core/pch.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/widgets/OpMinimizedUpdateDialogButton.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/dragdrop/dragdrop_manager.h"

#ifdef AUTO_UPDATE_SUPPORT

/***********************************************************************************
** static OpMinimizedUpdateDialogButton::Construct
**
************************************************************************************/
DEFINE_CONSTRUCT(OpMinimizedUpdateDialogButton)


/***********************************************************************************
**	Constructor
**  Initializes members, creates progress bar and hides it.
**
**  OpMinimizedUpdateDialogButton::OpMinimizedUpdateDialogButton
**
***********************************************************************************/
OpMinimizedUpdateDialogButton::OpMinimizedUpdateDialogButton() :
	m_minimized(FALSE),
	m_has_progress(FALSE),
	m_in_customizing_mode(FALSE)
{
	OpProgressBar::Construct(&m_progress);
	if (m_progress)
	{
		m_progress->SetVisibility(m_has_progress);
		AddChild(m_progress, TRUE);
		m_progress->SetListener(this);
	}
}


/***********************************************************************************
**  Adds the button to AutoUpdateManager's set of listeners, makes sure it is
**  not shown by default and that, if shown, it shows with image and text on the right.
**
**	OpMinimizedUpdateDialogButton::OnAdded
**  @see OpWidget::OnAdded
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnAdded()
{
	if (g_autoupdate_manager)
	{
		g_autoupdate_manager->AddListener(this);
		SetButtonTypeAndStyle(TYPE_TOOLBAR, STYLE_IMAGE_AND_TEXT_ON_RIGHT);
		SetUpdateMinimized(g_autoupdate_manager->IsUpdateMinimized());
		g_autoupdate_manager->RequestCallback(this);
	}
}


/***********************************************************************************
**  Removes the button from AutoUpdateManager's set of listeners.
**
**	OpMinimizedUpdateDialogButton::OnRemoved
**  @see OpWidget::OnRemoved
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnRemoved()
{
	if (g_autoupdate_manager)
		g_autoupdate_manager->RemoveListener(this);
}


/***********************************************************************************
**	Layouts the progress bar, if visible, and needs to take care of the button's
**  presentation when in customizing mode. There's no customizing state change it
**  can listen to (bug/task DSK-238401).
**
**  OpMinimizedUpdateDialogButton::OnLayout
**  @see OpWidget::OnLayout
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnLayout()
{
	if (!m_in_customizing_mode)
	{
		if (m_has_progress && m_progress)
		{
			OpRect rect = GetBounds();
			m_progress->SetRect(rect);
		}
	}
}

/***********************************************************************************
 ** Called before OnLayout
 **
 **	OpMinimizedUpdateDialogButton::OnBeforePaint
 **
 ***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnBeforePaint()
{
	HandleCustomizingMode();
}


/***********************************************************************************
**  No information is presented if up-to-date.
**
**	OpMinimizedUpdateDialogButton::OnUpToDate
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnUpToDate()
{
	Clear();
	UpdateText(UNI_L(""));
}


/***********************************************************************************
**  Checking for updates is silent, no information is presented to the users.
**
**	OpMinimizedUpdateDialogButton::OnChecking
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnChecking()
{
}


/***********************************************************************************
**	Updates the text to inform about an available update (only shown when minimized).
**
**	OpMinimizedUpdateDialogButton::OnUpdateAvailable
**  @param	context	Information about the available update
**  @see	AvailableUpdateContext
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnUpdateAvailable(AvailableUpdateContext* context)
{	
	UpdateText(Str::D_MINIMIZED_UPDATE_UPDATE_AVAILABLE);
}

/***********************************************************************************
**  Shows a progress bar to inform about the status of the download. The downloading
**  status is set as text on the progress bar.
**
**	OpMinimizedUpdateDialogButton::OnDownloading
**  @param	context	Information about the downloading progress
**  @see	UpdateProgressContext
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnDownloading(UpdateProgressContext* context)
{
	if(context && context->type == UpdateProgressContext::BrowserUpdate)
	{
		if (m_progress && m_minimized)
		{
			SetHasProgress(TRUE);
			m_progress->SetProgress(context->downloaded_size, context->total_size);
		}
		UpdateText(Str::D_MINIMIZED_UPDATE_DOWNLOADING);
	}
}

/***********************************************************************************
**  
**
**	OpMinimizedUpdateDialogButton::OnPreparing
**  @param	context	Information about the ongoing update
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnPreparing(UpdateProgressContext* context)
{
	SetHasProgress(FALSE);
	UpdateText(Str::D_MINIMIZED_UPDATE_PREPARING);
}

/***********************************************************************************
**  Hides the progress bar and sets 'Ready to install' text.
**
**	OpMinimizedUpdateDialogButton::OnReadyToInstall
**  @param	version	The Opera version that has been downloaded
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnReadyToInstall(UpdateProgressContext* context)
{
	SetHasProgress(FALSE);
	UpdateText(Str::D_MINIMIZED_UPDATE_READY);
}

/***********************************************************************************
**  Hides the progress bar, sets error or resuming text.
**
**	OpMinimizedUpdateDialogButton::OnError
**  @param	context
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnError(UpdateErrorContext* context)
{
	OP_ASSERT(context);

	SetHasProgress(FALSE);
	if (context->seconds_until_retry == NO_RETRY_TIMER) // set error string
	{
		if(context->error != AUConnectionError)
		{
			UpdateText(Str::D_MINIMIZED_UPDATE_ERROR);
		}
	}
	else // set resuming string
	{
		OpString resuming_string, tmp;
		g_languageManager->GetString(Str::D_MINIMIZED_UPDATE_RESUMING, resuming_string);
		tmp.AppendFormat(resuming_string, context->seconds_until_retry);
		UpdateText(tmp.CStr());
	}
}

/***********************************************************************************
**  Sets the minimized state of the update (button is visible if minimized).
**
**	OpMinimizedUpdateDialogButton::OnMinimizedStateChanged
**  @param	minimized	If auto-update dialogs are minimized or not
**  @see	OpMinimizedUpdateDialogButton::SetUpdateMinimized
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnMinimizedStateChanged(BOOL minimized)
{
	SetUpdateMinimized(minimized);
}

/***********************************************************************************
 ** Overridden version of OnMouseEvent
 **
 **	OpMinimizedUpdateDialogButton::OnMouseEvent
 **
 ***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if(widget == m_progress && !down && button == MOUSE_BUTTON_1)
	{
		if(GetAction())
		{
			g_input_manager->InvokeAction(GetAction());
		}
	}
}

/***********************************************************************************
**  This is a workaround to using OpWidget::SetVisibility(), as this method is called
**  in OpToolbar::OnLayout() and overrides local changes.
**  If minimized, the Button is shown by setting text, action and showing the progress
**  bar, if necessary.
**  If not minimized, all this information is cleared.
**
**	OpMinimizedUpdateDialogButton::SetUpdateMinimized
**  @param	minimized	If auto-update dialogs are minimized or not
**  @see	OpMinimizedUpdateDialogButton::Clear
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::SetUpdateMinimized(BOOL minimized) 
{
	m_minimized = minimized;
	if (!m_minimized)
	{
		Clear();
	}
	else
	{
		SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_RESTORE_AUTO_UPDATE_DIALOG)));
		UpdateText();
	}
}

/***********************************************************************************
**  Clears all information set in the button, information is saved for later in 
**  member variables. Text/Action are emptied, progress bar is hidden, if necessary.
**
**	OpMinimizedUpdateDialogButton::Clear
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::Clear()
{
	SetText(UNI_L(""));
	SetAction(NULL);
	if (m_progress)
	{
		m_progress->SetVisibility(FALSE);
	}
}

/***********************************************************************************
**  Sets if it has progress or not. Sets according member variable and hides
**  progress bar.
**
**	OpMinimizedUpdateDialogButton::SetHasProgress
**  @param has_progress
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::SetHasProgress(BOOL has_progress)
{
	m_has_progress = has_progress;
	if (m_progress)
	{
		m_progress->SetVisibility(m_has_progress);
	}
}

/***********************************************************************************
**  Sets the text in the member variable (if text is set). If minimized/shown, the
**  text is drawn either on the button directly or on the progress bar.
**
**	OpMinimizedUpdateDialogButton::UpdateText
**  @param	text	The status text to be written on the button
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::UpdateText(const uni_char* text)
{
	if (text != NULL)
	{
		m_text_str.Set(text);
	}
	if (m_minimized)
	{
		if (m_has_progress && m_progress)
		{
			m_progress->SetText(m_text_str.CStr());
		}
		SetText(m_text_str.CStr()); // set anyway, to get the right size in OnLayout
	}
}

/***********************************************************************************
**  Gets a language string before it updates the text.
**
**	OpMinimizedUpdateDialogButton::UpdateText
**  @param	str	The local string of the text to be written on the button
**  @see	OpMinimizedUpdateDialogButton::UpdateText(const uni_char*)
**
***********************************************************************************/
void OpMinimizedUpdateDialogButton::UpdateText(Str::LocaleString str)
{
	OpString tmp;
	g_languageManager->GetString(str, tmp);
	UpdateText(tmp.CStr());
}

/***********************************************************************************
**  If in customizing state, it will reset the button and draw a basic 'customizing'
**  string on it. The first time it after it has been in customizing mode, it will 
**  set everything back to the actuall strings/actions/progress bars.
**
**	OpMinimizedUpdateDialogButton::HandleCustomizingMode
**  @return	If toolbar is in customizing mode or not
**
***********************************************************************************/
BOOL OpMinimizedUpdateDialogButton::HandleCustomizingMode()
{
	if(g_application->IsCustomizingToolbars())
	{
		m_in_customizing_mode = TRUE;
		
		OpString prev_str, str;
		GetText(prev_str);
		g_languageManager->GetString(Str::D_MINIMIZED_UPDATE_DEFAULT, str);
		if (prev_str.Compare(str) == 0)
			return TRUE;

		Clear();
		SetText(str.CStr()); // don't override m_text

		GetParent()->OnContentSizeChanged();

		return TRUE;
	}
	else if (m_in_customizing_mode) // first time after in customizing mode
	{
		m_in_customizing_mode = FALSE;
		SetUpdateMinimized(m_minimized); // refresh text/action
		
		GetParent()->OnContentSizeChanged();
	}
	return FALSE;
}

/***********************************************************************************
 **
 **	OpMinimizedUpdateDialogButton::GetPreferedSize
 **
 ***********************************************************************************/
void OpMinimizedUpdateDialogButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	string.NeedUpdate();
	UpdateActionStateIfNeeded();
	
	if(string.GetWidth() > 0)
	{
		GetInfo()->GetPreferedSize(this, WIDGET_TYPE_BUTTON, w, h, cols, rows);
	}
	else
	{
		*w = 0;
		*h = 0;
	}
}

/***********************************************************************************
 **
 **	OpMinimizedUpdateDialogButton::OnDragStart
 **
 ***********************************************************************************/
void OpMinimizedUpdateDialogButton::OnDragStart(const OpPoint& point)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;
	
	DesktopDragObject* drag_object = GetDragObject(WIDGET_TYPE_MINIMIZED_AUTOUPDATE_STATUS_BUTTON, point.x, point.y);
	if (drag_object)
	{
		drag_object->SetObject(this);
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

#endif // AUTO_UPDATE_SUPPORT
