/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick/Application.h"
#include "adjunct/quick/application/ClassicGlobalUiContext.h"
#include "adjunct/quick/windows/DesktopGadget.h"
#include "adjunct/widgetruntime/GadgetHelpLoader.h"
#include "adjunct/widgetruntime/GadgetRemoteDebugDialog.h"
#include "adjunct/widgetruntime/GadgetRemoteDebugHandler.h"
#include "adjunct/widgetruntime/GadgetUiContext.h"
#include "modules/pi/system/OpPlatformViewers.h"

#ifdef _MACINTOSH_
#include "modules/gadgets/gadgets_module.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "platforms/mac/pi/desktop/MacGadgetAboutDialog.h"
#endif //_MACINTOSH_

GadgetUiContext::GadgetUiContext(Application& application)
	: m_global_context(OP_NEW(ClassicGlobalUiContext, (application)))
	, m_help_loader(OP_NEW(GadgetHelpLoader, ()))
	, m_debug_handler(OP_NEW(GadgetRemoteDebugHandler, ()))
{
}

GadgetUiContext::~GadgetUiContext()
{
	OP_DELETE(m_debug_handler);
	m_debug_handler = NULL;

	OP_DELETE(m_help_loader);
	m_help_loader = NULL;

	OP_DELETE(m_global_context);
	m_global_context = NULL;
}


void GadgetUiContext::SetEnabled(BOOL enabled)
{
	m_global_context->SetEnabled(enabled);
}

BOOL GadgetUiContext::OnInputAction(OpInputAction* action)
{
	BOOL handled = MaybeHandleGadgetAction(action);

	if (!handled)
	{
		handled = MaybeFilterBrowserAction(action);
	}

	if (!handled)
	{
		handled = m_global_context->OnInputAction(action);
	}

	return handled;
}


BOOL GadgetUiContext::MaybeHandleGadgetAction(OpInputAction* action)
{
	BOOL handled = FALSE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			// Action state handling goes here...
			break;
		}

		case OpInputAction::ACTION_SHOW_HELP:
		{
			const uni_char* const topic = action->GetActionDataString();
			if (NULL != topic)
			{
				OpStatus::Ignore(m_help_loader->Init(topic));
			}
			handled = TRUE;
			break;
		}

		case OpInputAction::ACTION_WIDGET_REMOTE_DEBUG:
		{
			OpAutoPtr<GadgetRemoteDebugDialog> dialog(
					OP_NEW(GadgetRemoteDebugDialog, ()));
			if (NULL != dialog.get())
			{
				if (OpStatus::IsSuccess(dialog->Init(*m_debug_handler)))
				{
					dialog.release();
				}
			}
			handled = TRUE;
			break;
		}
		case OpInputAction::ACTION_CLOSE_WINDOW:
		case OpInputAction::ACTION_EXIT:	
		case OpInputAction::ACTION_CLOSE_PAGE:
		{
			DesktopWindow *window = g_application->GetActiveDesktopWindow();
			if (window != NULL)
			{
				window->Close();
				handled = TRUE;
				break;
			}		   
			
		}
			
		case OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW:
		{
			OpString link;
			link.Set(action->GetActionDataString());
			link.Strip();
			if (link.HasContent())
			{
				g_op_platform_viewers->OpenInDefaultBrowser(link);
			}
			handled = TRUE;
			break;
		}

		case OpInputAction::ACTION_WIDGET_ABOUT_PAGE:
		{
#ifdef _MACINTOSH_	
			OpAutoPtr<MacGadgetAboutDialog> aboutDialog(OP_NEW(MacGadgetAboutDialog, (g_gadget_manager->GetGadget(0)->GetClass())));
			if (NULL != aboutDialog.get())
			{
				if (OpStatus::IsSuccess(aboutDialog->Init()))
				{
					aboutDialog.release();
				}
			}
#endif //_MACINTOSH_
			handled = TRUE;
			break;
		}

	    case OpInputAction::ACTION_WIDGET_PREFERENCE_PANE:
       	{
			DesktopWindow* window = g_application->GetActiveDesktopWindow();
			window->GetWindowCommander()->GetUserConsent(0);
			handled = TRUE;
			break;
		}

		case OpInputAction::ACTION_DESKTOP_GADGET_CALLBACK:
		{
			DesktopWindow* window = g_application->GetActiveDesktopWindow(TRUE);
			OP_ASSERT(NULL != window);
			if (NULL != window)
			{
				window->Activate();
			}
			GadgetNotificationCallbackWrapper* wrapper =
					reinterpret_cast<GadgetNotificationCallbackWrapper*>(
							action->GetActionData());
			OP_ASSERT(NULL != wrapper);
			if (NULL != wrapper)
			{
				wrapper->m_callback->OnShowNotificationFinished(
						OpDocumentListener::GadgetShowNotificationCallback
								::REPLY_ACKNOWLEDGED);
			}
			handled = TRUE;
			break;
		}

		case OpInputAction::ACTION_DESKTOP_GADGET_CANCEL_CALLBACK:
		{
			GadgetNotificationCallbackWrapper* wrapper =
					reinterpret_cast<GadgetNotificationCallbackWrapper*>(
							action->GetActionData());
			OP_ASSERT(NULL != wrapper);
			if (NULL != wrapper)
			{
				wrapper->m_callback->OnShowNotificationFinished(
						OpDocumentListener::GadgetShowNotificationCallback
								::REPLY_IGNORED);
			}
			handled = TRUE;
			break;
		}
	}
	return handled;
}


BOOL GadgetUiContext::MaybeFilterBrowserAction(OpInputAction* action)
{
	BOOL filtered = FALSE;

	switch (action->GetAction())
	{
		/* These actions should be ignored in gadget app.
		   TODO: There's more such actions. Most of them
		   will be included probably in later development. */
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction *child_action = action->GetChildAction();
			switch(child_action->GetAction())
			{
				case OpInputAction::ACTION_EXPORT_NEWSFEED_LIST:
				case OpInputAction::ACTION_GOTO_SPEEDDIAL:
				case OpInputAction::ACTION_EXPORT_CONTACTS:
				case OpInputAction::ACTION_SYNC_LOGIN:
					filtered = TRUE;
					break;
			}
			break;
		}
		case OpInputAction::ACTION_EXPORT_NEWSFEED_LIST:
		case OpInputAction::ACTION_GO_TO_NICKNAME:
		case OpInputAction::ACTION_GO_TO_PAGE:
		case OpInputAction::ACTION_NEW_PAGE:
		case OpInputAction::ACTION_CLOSE_PAGE:
		case OpInputAction::ACTION_OPEN_DOCUMENT:
		case OpInputAction::ACTION_SAVE_DOCUMENT_AS:
		case OpInputAction::ACTION_SAVE_DOCUMENT:
		case OpInputAction::ACTION_PRINT_DOCUMENT:
		case OpInputAction::ACTION_SHOW_PREFERENCES:
		case OpInputAction::ACTION_SHOW_PRINT_SETUP:
		case OpInputAction::ACTION_SHOW_PRINT_OPTIONS:
		case OpInputAction::ACTION_MANAGE:
		case OpInputAction::ACTION_PASTE_AND_GO:
		case OpInputAction::ACTION_NEW_BROWSER_WINDOW:
		case OpInputAction::ACTION_FOCUS_PANEL:
		case OpInputAction::ACTION_GO_TO_HOMEPAGE:
		case OpInputAction::ACTION_CUSTOMIZE_TOOLBARS:
		case OpInputAction::ACTION_GOTO_SPEEDDIAL:
			filtered = TRUE;
			break;
	}

	return filtered;
}

#endif // WIDGET_RUNTIME_SUPPORT
