/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/* This file is included from vis_dev.cpp */

#ifdef SUPPORT_DATA_SYNC
#include "modules/sync/sync_coordinator.h"
#endif // SUPPORT_DATA_SYNC

extern BOOL CanScrollInDirection(FramesDocument* doc, OpInputAction::Action action);

BOOL VisualDevice::OnInputAction(OpInputAction* action)
{
	Window*			window		= GetDocumentManager()->GetWindow();
	FramesDocument*	document	= GetDocumentManager()->GetCurrentVisibleDoc();

#ifdef SVG_SUPPORT
	// FIXME: Find a better way to get what element was clicked, one that works for !quick.
	HTML_Element* clicked_elm = NULL;
#ifdef DISPLAY_CLICKINFO_SUPPORT
	clicked_elm = MouseListener::GetClickInfo().GetImageElement();
#endif // DISPLAY_CLICKINFO_SUPPORT
	if (!action->IsKeyboardInvoked() &&
		g_svg_manager->OnInputAction(action, clicked_elm, document))
		return TRUE;
#endif // SVG_SUPPORT

	if (g_cssManager)
	{
		BOOL handled;
		OP_STATUS err = g_cssManager->OnInputAction(action, handled);
		if (OpStatus::IsError(err))
		{
			window->RaiseCondition(err);
			return FALSE;
		}
		if (handled)
			return TRUE;
	}

	if (document)
	{
		OP_BOOLEAN handled = document->OnInputAction(action);
		if (OpStatus::IsError(handled))
		{
			window->RaiseCondition(handled);
			return FALSE;
		}
		else if (handled == OpBoolean::IS_TRUE)
			return TRUE;
	}

	switch (action->GetAction())
	{
#ifdef PAN_SUPPORT

# ifdef ACTION_COMPOSITE_PAN_ENABLED
		case OpInputAction::ACTION_COMPOSITE_PAN:
		{
			int16* delta = (int16*)action->GetActionData();

			return PanDocument(delta[0], delta[1]);
		}
# endif // ACTION_COMPOSITE_PAN_ENABLED

# ifdef ACTION_PAN_X_ENABLED
		case OpInputAction::ACTION_PAN_X:
			return PanDocument(action->GetActionData(), 0);
			break;
# endif // ACTION_PAN_X_ENABLED

# ifdef ACTION_PAN_Y_ENABLED
		case OpInputAction::ACTION_PAN_Y:
			return PanDocument(0, action->GetActionData());
			break;
# endif // ACTION_PAN_Y_ENABLED
#endif // PAN_SUPPORT

#ifdef ACTION_ENABLE_HANDHELD_MODE_ENABLED
		// A toggle action is specified in the input file as an or'ed command
		// sequence. It works by sending the commands one after another until
		// an OnInputAction() accepts it (returns TRUE). The HANDHELD on/off is
		// a toggle action so we must be sure we switch from one mode to another
		// when TRUE is returned (bug #172741) [espen 2005-08-05]

		case OpInputAction::ACTION_ENABLE_HANDHELD_MODE:
			if( window->GetWindowCommander()->GetLayoutMode() == OpWindowCommander::CSSR )
				return FALSE;
			window->GetWindowCommander()->SetLayoutMode(OpWindowCommander::CSSR);
			return TRUE;
#endif // ACTION_ENABLE_HANDHELD_MODE_ENABLED

#ifdef ACTION_DISABLE_HANDHELD_MODE_ENABLED
		case OpInputAction::ACTION_DISABLE_HANDHELD_MODE:
			if( window->GetWindowCommander()->GetLayoutMode() != OpWindowCommander::CSSR )
				return FALSE;
			window->GetWindowCommander()->SetLayoutMode(OpWindowCommander::NORMAL);
			return TRUE;
#endif // ACTION_DISABLE_HANDHELD_MODE_ENABLED

		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
#ifdef ACTION_ZOOM_TO_ENABLED
			case OpInputAction::ACTION_ZOOM_TO:
			{
				child_action->SetSelected(child_action->GetActionData()==window->GetScale());
				return TRUE;
			}
#endif // ACTION_ZOOM_TO_ENABLED
#if defined(WAND_SUPPORT) && defined(ACTION_WAND_ENABLED)
			case OpInputAction::ACTION_WAND:
			{
				child_action->SetEnabled(document && g_wand_manager->Usable(document));
				return TRUE;
			}
#endif // WAND_SUPPORT && ACTION_WAND_ENABLED

			case OpInputAction::ACTION_SCROLL_LEFT:
			case OpInputAction::ACTION_SCROLL_RIGHT:
			{
				child_action->SetEnabled(CanScrollInDirection(document, child_action->GetAction()));
				return TRUE;
			}

			default:
				return FALSE;
			}
		}

#ifdef _SPAT_NAV_SUPPORT_
		case OpInputAction::ACTION_NAVIGATE_RIGHT:
		case OpInputAction::ACTION_NAVIGATE_UP:
		case OpInputAction::ACTION_NAVIGATE_LEFT:
		case OpInputAction::ACTION_NAVIGATE_DOWN:
			return window->GetSnHandlerConstructIfNeeded() && window->GetSnHandlerConstructIfNeeded()->OnInputAction(action);
		case OpInputAction::ACTION_FOCUS_FORM:
		{
			if (window->GetSnHandler())
				return window->GetSnHandler()->FocusCurrentForm();
			return FALSE;
		}
		case OpInputAction::ACTION_UNFOCUS_FORM:
		{
			if (window->GetSnHandler())
				return window->GetSnHandler()->UnfocusCurrentForm();
			return FALSE;
		}
#if defined(LIBOPERA) && defined(SN_LEAVE_SUPPORT)
		case OpInputAction::ACTION_NAVIGATE_LEAVE_RIGHT:
		{
			window->LeaveInDirection(0);
			break;
		}
		case OpInputAction::ACTION_NAVIGATE_LEAVE_UP:
		{
			window->LeaveInDirection(90);
			break;
		}
		case OpInputAction::ACTION_NAVIGATE_LEAVE_LEFT:
		{
			window->LeaveInDirection(180);
			break;
		}
		case OpInputAction::ACTION_NAVIGATE_LEAVE_DOWN:
		{
			window->LeaveInDirection(270);
			break;
		}
#endif // LIBOPERA && SN_LEAVE_SUPPORT
#endif // _SPAT_NAV_SUPPORT_

		case OpInputAction::ACTION_OPEN_LINK:
#ifdef ACTION_OPEN_LINK_IN_NEW_PAGE_ENABLED
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
#endif // ACTION_OPEN_LINK_IN_NEW_PAGE_ENABLED
#ifdef ACTION_OPEN_LINK_IN_NEW_WINDOW_ENABLED
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
#endif // ACTION_OPEN_LINK_IN_NEW_WINDOW_ENABLED
#ifdef ACTION_OPEN_LINK_IN_BACKGROUND_PAGE_ENABLED
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
#endif // ACTION_OPEN_LINK_IN_BACKGROUND_PAGE_ENABLED
#ifdef ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW_ENABLED
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
#endif // ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW_ENABLED
		{
			BOOL open_link_in_new_page = FALSE;
			BOOL open_link_in_new_window = FALSE;
			BOOL open_link_in_background_page = FALSE;
			BOOL open_link_in_background_window = FALSE;
			switch (action->GetAction()) {
#ifdef ACTION_OPEN_LINK_IN_NEW_PAGE_ENABLED
			case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
				open_link_in_new_page = TRUE; break;
#endif // ACTION_OPEN_LINK_IN_NEW_PAGE_ENABLED
#ifdef ACTION_OPEN_LINK_IN_NEW_WINDOW_ENABLED
			case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
				open_link_in_new_window = TRUE; break;
#endif // ACTION_OPEN_LINK_IN_NEW_WINDOW_ENABLED
#ifdef ACTION_OPEN_LINK_IN_BACKGROUND_PAGE_ENABLED
			case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
				open_link_in_background_page = TRUE; break;
#endif // ACTION_OPEN_LINK_IN_BACKGROUND_PAGE_ENABLED
#ifdef ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW_ENABLED
			case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
				open_link_in_background_window = TRUE; break;
#endif // ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW_ENABLED
			}
			if (action->GetActionData() &&
				action->GetActionDataString() &&
				(uni_strnicmp(action->GetActionDataString(), UNI_L("urlinfo"), 7) != 0))
			{
				// if data is nonzero, it means open a bookmark etc.. let Application handle that
				// correction: if data is nonzero it could be that it has a PopupMenuInfo pointer or it's
				// a hotlistitem id, need to check the action data string to be sure of the context.
				// The context is defined by the entry in the *_menu.ini file. (rfz 20071003)
				return FALSE;
			}

			if (action->IsKeyboardInvoked())
			{
				if (document &&
					(action->GetAction() == OpInputAction::ACTION_OPEN_LINK
					 || open_link_in_new_page
					 || open_link_in_new_window
					 || open_link_in_background_page))
				{
					HTML_Document *html_doc = document->GetHtmlDocument();
					HTML_Element* focused_element = html_doc ? html_doc->GetFocusedElement() : NULL;
					// A formobject is focused so we want to submit it
					if (focused_element && ((focused_element->Type() == HE_INPUT &&
											(focused_element->GetInputType() == INPUT_TEXT ||
											focused_element->GetInputType() == INPUT_EMAIL ||
											focused_element->GetInputType() == INPUT_NUMBER ||
											focused_element->GetInputType() == INPUT_SEARCH ||
											focused_element->GetInputType() == INPUT_TEL ||
											focused_element->GetInputType() == INPUT_URI ||
											focused_element->GetInputType() == INPUT_PASSWORD)) ||
											focused_element->Type() == HE_SELECT))
					{
						// Dirty convert back to shiftkeys from action. (All submitcode from here use shiftkeys
						// to determine if the submit should open a new page or a backgroundpage.
						ShiftKeyState keystate = SHIFTKEY_NONE;
						if (open_link_in_new_page)
							keystate = SHIFTKEY_SHIFT;
						else if (open_link_in_background_page)
							keystate = SHIFTKEY_SHIFT | SHIFTKEY_CTRL;

						if (OpStatus::IsMemoryError(FormManager::SubmitFormFromInputField(document, focused_element, keystate)))
							window->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						return TRUE;
					}
				}
				FramesDocument* doc = window->GetCurrentDoc();
				if (doc)
				{
					int sub_win_id = -1;
					const uni_char* window_name = NULL;
					URL url = doc->GetCurrentURL(window_name, sub_win_id);

					if (!url.IsEmpty())
					{
						OP_STATUS stat =
							window->GetHighlightedURL(
								0,
								action->GetAction() != OpInputAction::ACTION_OPEN_LINK,
								open_link_in_background_page || open_link_in_background_window);

						if (OpStatus::IsMemoryError(stat))
							window->RaiseCondition(OpStatus::ERR_NO_MEMORY);

						return TRUE;
					}
				}
				return FALSE;
			}
#if defined(QUICK)
			else
			{
				// We handle it in the gui
			}
#else // !QUICK
			else if (action->GetAction() == OpInputAction::ACTION_OPEN_LINK)
			{
				BOOL3 open_in_new_window = YES;
				BOOL3 open_in_background = MAYBE;

				DocumentManager* doc_man = window->DocManager();
				if (!doc_man)
					return FALSE;

				//if the window is a mail window, open the document in another window
				window = window->IsMailOrNewsfeedWindow() ? window :
					g_windowManager->GetAWindow(FALSE, open_in_new_window, open_in_background);

				FramesDocument* doc = doc_man->GetCurrentDoc();
				URL curr_clicked_url = g_windowManager->GetCurrentClickedURL();

				if (doc && !curr_clicked_url.IsEmpty())
				{
					doc->GetTopDocument()->ClearSelection(TRUE);
					window->GetWindowCommander()->GetDocumentListener()->OnNoHover(window->GetWindowCommander());

					URL current_url = doc->GetURL();
					doc_man->StopLoading(TRUE);

					window->ResetProgressDisplay();

					window->OpenURL(curr_clicked_url, doc, TRUE, curr_clicked_url == current_url, -1, TRUE);
					return TRUE;
				}
			}
			else // action->GetAction() != OpInputAction::ACTION_OPEN_LINK
			{
				URL curr_clicked_url = g_windowManager->GetCurrentClickedURL();
				if (!curr_clicked_url.IsEmpty())
				{
					OP_ASSERT(open_link_in_new_page || open_link_in_new_window || open_link_in_background_page || open_link_in_background_window);
					g_windowManager->OpenURLNamedWindow
						(curr_clicked_url, window, window->GetCurrentDoc(),
						 -1, NULL, TRUE, TRUE,
						 /* Open in background: */
						 open_link_in_background_page || open_link_in_background_window,
						 TRUE,
						 /* Open in new page: */
						 open_link_in_new_page || open_link_in_background_page);
					return TRUE;
				}
				else
				{	// OOM
					OP_STATUS stat = window->GetHighlightedURL(0, action->GetAction() != OpInputAction::ACTION_OPEN_LINK,
															   open_link_in_background_page || open_link_in_background_window);

					if (OpStatus::IsMemoryError(stat))
						window->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				}
			}
#endif // QUICK
			return FALSE;
		}

#if defined(ACTION_ZOOM_POINT_ENABLED)
		case OpInputAction::ACTION_ZOOM_POINT:
		{
			OpRect rect;
			action->GetActionPosition(rect);
			double scale =
				MAX((short)20, GetScale() + action->GetActionData()) / 100.0;
			ViewportController* controller = window->GetViewportController();
			OpRect* priority_rect = 0;
			if (!rect.IsEmpty())
				priority_rect = &rect;
			controller->GetViewportRequestListener()->
				OnZoomLevelChangeRequest(controller, scale, priority_rect,
										 VIEWPORT_CHANGE_REASON_INPUT_ACTION);

			return TRUE;
		}
#endif // ACTION_ZOOM_POINT_ENABLED

#ifdef ACTION_ZOOM_IN_ENABLED
		case OpInputAction::ACTION_ZOOM_IN:
#endif // ACTION_ZOOM_IN_ENABLED
#ifdef ACTION_ZOOM_OUT_ENABLED
		case OpInputAction::ACTION_ZOOM_OUT:
#endif // ACTION_ZOOM_OUT_ENABLED
#if defined(ACTION_ZOOM_IN_ENABLED) || defined(ACTION_ZOOM_OUT_ENABLED)
		{
			INT32 delta = action->GetAction() == OpInputAction::ACTION_ZOOM_OUT ?
				-action->GetActionData() : action->GetActionData();
			window->GetMessageHandler()->PostMessage
				(WM_OPERA_SCALEDOC, 0, MAKELONG(delta, FALSE /* is delta */));
			return TRUE;
		}
#endif // ACTION_ZOOM_IN_ENABLED || ACTION_ZOOM_OUT_ENABLED
#ifdef ACTION_ZOOM_TO_ENABLED
		case OpInputAction::ACTION_ZOOM_TO:
		{
			window->GetMessageHandler()->PostMessage
				(WM_OPERA_SCALEDOC, 0,
				 MAKELONG(action->GetActionData(), TRUE /* is absolute */));
			return TRUE;
		}
#endif // ACTION_ZOOM_TO_ENABLED
#if defined(SUPPORT_DATA_SYNC) && defined(ACTION_SYNC_NOW_ENABLED)
		case OpInputAction::ACTION_SYNC_NOW:
		{
			return g_sync_coordinator->OnInputAction(action);
		}
#endif // SUPPORT_DATA_SYNC && ACTION_SYNC_NOW_ENABLED
	}
	int times = action->GetActionData();
	if (times < 1)
		times = 1;
	return ScrollDocument(document, action->GetAction(), times, action->GetActionMethod());
}

