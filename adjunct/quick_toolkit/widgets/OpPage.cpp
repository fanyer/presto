/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpPage.h"

#include "modules/pi/OpBitmap.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/viewers/viewers.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/windowcommander/src/TransferManagerDownload.h"

#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/desktop_util/sound/SoundUtils.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/controller/WebHandlerPermissionController.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/managers/DesktopSecurityManager.h"
#include "adjunct/quick/managers/DesktopCookieManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/SecurityUIManager.h"
#include "adjunct/quick/managers/WebmailManager.h"
#include "adjunct/quick/models/ServerWhiteList.h"
#include "adjunct/quick/dialogs/MidClickDialog.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/usagereport/UsageReport.h"
#include "adjunct/quick/dialogs/SetupApplyDialog.h"
#include "adjunct/quick/application/BrowserAuthenticationListener.h"

OpPage::OpPage(OpWindowCommander* window_commander) :
	m_window_commander(window_commander),
	m_current_popup_callback(NULL),
	m_play_loaded_sound(false),
	m_any_pending_authentication(false)
{
	SetSupportsSmallScreen(FALSE);
	SetSupportsNavigation(FALSE);
	SetStoppedByUser(FALSE);
}

OpPage::~OpPage()
{
	BroadcastPageDestroyed();

	if (m_window_commander)					// might not exist due to OOM in init
	{
		m_window_commander->SetMenuListener(NULL);
 		m_window_commander->SetLoadingListener(NULL);
 		m_window_commander->SetDocumentListener(NULL);

//		m_window_commander->OnUiWindowClosing();
		g_windowCommanderManager->ReleaseWindowCommander(m_window_commander);
	}
}

OP_STATUS OpPage::Init()
{
	if (!m_window_commander)
		RETURN_IF_ERROR(g_windowCommanderManager->GetWindowCommander(&m_window_commander));

	m_window_commander->SetMenuListener(this);
 	m_window_commander->SetLoadingListener(this);
 	m_window_commander->SetDocumentListener(this);

	if (g_security_ui_manager)
		m_window_commander->SetSSLListener(g_security_ui_manager);

	// As long as Quick don't use OpErrorListener API, the usage report system hooks directly into it.
	if (g_usage_report_manager)
		m_window_commander->SetErrorListener(g_usage_report_manager->GetErrorReport());

#ifdef _ASK_COOKIE
	m_window_commander->SetCookieListener(g_desktop_cookie_manager);
#endif //_ASK_COOKIE

	return OpStatus::OK;
}

OP_STATUS OpPage::AddListener(OpPageListener* listener)
{
	RETURN_IF_ERROR(m_listeners.Add(listener));
	listener->SetPage(this);
	return OpStatus::OK;
}

OP_STATUS OpPage::RemoveListener(OpPageListener* listener)
{
	listener->SetPage(0);
	return m_listeners.Remove(listener);
}

OP_STATUS OpPage::AddAdvancedListener(OpPageAdvancedListener* listener)
{
	RETURN_IF_ERROR(AddListener(listener));
	return m_advanced_listeners.Add(listener);
}

OP_STATUS OpPage::RemoveAdvancedListener(OpPageAdvancedListener* listener)
{
	RETURN_IF_ERROR(RemoveListener(listener));
	return m_advanced_listeners.Remove(listener);
}

OP_STATUS OpPage::SetOpWindow(OpWindow* op_window)
{
	if(op_window)
	{
		RETURN_IF_ERROR(m_window_commander->OnUiWindowCreated(op_window));
		return OpStatus::OK;
	}

	m_window_commander->OnUiWindowClosing();

	return OpStatus::OK;
}

OpWindowCommander* OpPage::GetWindowCommander()
{
	return m_window_commander;
}

BOOL OpPage::HasCurrentElement()
{
	int history_min, history_max;
	GetWindowCommander()->GetHistoryRange(&history_min, &history_max);
	return history_max >= history_min;
}

OP_STATUS OpPage::GoToPage(const OpStringC& url)
{
	return WindowCommanderProxy::OpenURL(GetWindowCommander(), url.CStr());
}

OP_STATUS OpPage::Reload()
{
	GetWindowCommander()->Reload();
	return OpStatus::OK;
}

BOOL OpPage::IsFullScreen()
{
	return m_window_commander->GetDocumentFullscreenState() != OpWindowCommander::FULLSCREEN_NONE;
}

BOOL OpPage::IsHighSecurity()
{
	return (m_window_commander && g_desktop_security_manager->IsHighSecurity(m_window_commander->GetSecurityMode()));
}

OpDocumentListener::SecurityMode OpPage::GetSecurityMode()
{
	return m_window_commander->GetSecurityMode();
}

BOOL OpPage::ContinuePopup(OpDocumentListener::JSPopupCallback::Action action, OpDocumentListener::JSPopupOpener **opener)
{
	if (!m_current_popup_callback)
		return FALSE;

	m_current_popup_callback->Continue(action, opener);
	m_current_popup_callback = NULL;

	return TRUE;
}

BOOL OpPage::IsActionSupported(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_ENABLE_HANDHELD_MODE:
		case OpInputAction::ACTION_DISABLE_HANDHELD_MODE:
		{
			return GetSupportsSmallScreen();
		}

		case OpInputAction::ACTION_STOP:
		case OpInputAction::ACTION_RELOAD:
		case OpInputAction::ACTION_FORCE_RELOAD:
		case OpInputAction::ACTION_RELOAD_FRAME:
		{
			return GetSupportsNavigation();
		}
	}

	return TRUE;
}

void OpPage::RefreshSecurityInformation(SecurityMode mode)
{
#ifdef SECURITY_INFORMATION_PARSER
	if (m_window_commander)
		m_window_commander->GetSecurityInformation(m_security_information, mode == EXTENDED_SECURITY || mode == SOME_EXTENDED_SECURITY);
#endif // SECURITY_INFORMATION_PARSER
}

OP_STATUS OpPage::GetSecurityInformation(OpString& text)
{
	return text.Set(m_security_information.CStr());
}

void OpPage::SaveDocumentIcon()
{
	OpString icon_url_str;
	const uni_char* tmp;
	URL iconurl;

	OP_STATUS status = GetWindowCommander()->GetDocumentIconURL(&tmp);
	if(OpStatus::IsSuccess(status))
	{
		icon_url_str.Set(tmp);
		iconurl = urlManager->GetURL(icon_url_str.CStr());
	}

	if( OpStatus::IsSuccess(status) && iconurl.GetContentLoaded())
	{
		OpString document_url_str;
		document_url_str.Set(GetWindowCommander()->GetCurrentURL(TRUE));
		g_favicon_manager->Add(document_url_str.CStr(), icon_url_str.CStr());
	}
}

void OpPage::UpdateWindowImage(BOOL force)
{
	if (!force && GetDocumentIcon()->HasBitmapImage())
		return;

	Image img;

	if ((g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AlwaysLoadFavIcon) != 0) && !GetWindowCommander()->GetPrivacyMode())
	{
		OpBitmap* bitmap = NULL;
		GetWindowCommander()->GetDocumentIcon(&bitmap);

		if (bitmap)
		{
			img = imgManager->GetImage(bitmap);

			SaveDocumentIcon();

		}
		else if (!GetWindowCommander()->IsLoading() && WindowCommanderProxy::HasCurrentDoc(GetWindowCommander()))
		{
			/* Use an already saved favicon if available. This is to prevent
			   favicon flickering when core doesn't have favicon temporarily
			   before or right after page loaded. */
			OpString document_url;
			document_url.Set(GetWindowCommander()->GetCurrentURL(TRUE));
			img = g_favicon_manager->Get(document_url.CStr());

			if (!img.IsEmpty())
			{
				/* If favicon manager has favicon, store a copy of it. Manager
				   can get rid of the favicon without notifying OpPage and
				   subsequently also OpBrowserView and DocumentDesktopWindow
				   which might still be using it which would result in favicon
				   dispappearing if we wouldn't copy it. */
				OpBitmap* src_bmp = img.GetBitmap(null_image_listener);
				if (src_bmp)
				{
					UINT32 src_width = src_bmp->Width();
					UINT32 src_height = src_bmp->Height();

					if (OpStatus::IsSuccess(OpBitmap::Create(&bitmap, src_width, src_height, src_bmp->IsTransparent(), src_bmp->HasAlpha(), 0, 0, FALSE)))
					{
						UINT32 lineCount = MIN(src_height, img.GetLastDecodedLine());
						UINT32* data = OP_NEWA(UINT32, src_width);

						if (data)
						{
							for (UINT32 i = 0 ; i < lineCount ; i++)
							{
								if (src_bmp->GetLineData(data, i) == TRUE)
									bitmap->AddLine(data, i);
							}

							OP_DELETEA(data);
						}
						else
						{
							OP_DELETE(bitmap);
							bitmap = NULL;
							g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						}
					}
					img.ReleaseBitmap();
				}
				if (bitmap)
					img = imgManager->GetImage(bitmap);
			}
		}
	}

	if (img.IsEmpty())
	{
		if (g_security_ui_manager && !g_security_ui_manager->HasCertReqProcessed(GetWindowCommander())
		    || m_any_pending_authentication)
		{
			m_document_icon.SetImage("Window Pending For Authentication");
		}
		else if (GetWindowCommander()->IsLoading())
			m_document_icon.SetImage("Window Document Loading Icon");
		else if (GetWindowCommander()->GetPrivacyMode())
			m_document_icon.SetImage("Window Private Icon");
		else
			m_document_icon.SetImage("Window Document Icon");
	}
	else if (!img.IsEmpty())
	{
		m_document_icon.SetBitmapImage(img, FALSE);
	}
}

void OpPage::BroadcastPageDestroyed()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageDestroyed(this);
	}
}

void OpPage::SelectionChanged()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageSelectionChanged(m_window_commander);
	}
}

// Implementing OpLoadingListener
//
// NOTE: This will be removed from OpLoadingListener soon (CORE-35579), don't add anything here!
void OpPage::OnUrlChanged(OpWindowCommander* commander, URL& url)
{
	OpString name;
	if (OpStatus::IsSuccess(url.GetAttribute(URL::KUniName, name)))
	{
		OnUrlChanged(commander, name.CStr());
	}
}

// Implementing OpLoadingListener
void OpPage::OnUrlChanged(OpWindowCommander* commander, const uni_char* url)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageUrlChanged(commander, url);
	}
}

// Implementing OpLoadingListener
void OpPage::OnStartLoading(OpWindowCommander* commander)
{
	OP_NEW_DBG("OpPage::OnStartLoading()", "quick.page");
	OP_DBG(("commander = ") << commander);

	m_play_loaded_sound = true;
	m_security_information.Empty();

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageStartLoading(commander);
	}
}

// Implementing OpLoadingListener
void OpPage::OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageLoadingProgress(commander, info);
	}
}

// Implementing OpLoadingListener
void OpPage::OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status)
{
	OP_NEW_DBG("OpPage::OnLoadingFinished", "quick.page");

	if (m_play_loaded_sound)
	{
		OP_DBG(("Playing the PrefsCollectionUI::LoadedSound."));
		OpStatus::Ignore(SoundUtils::SoundIt(g_pcui->GetStringPref(PrefsCollectionUI::LoadedSound), FALSE));
		m_play_loaded_sound = false;
	}

	if(IsHighSecurity() && m_security_information.IsEmpty())
		RefreshSecurityInformation(m_window_commander->GetSecurityMode());

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageLoadingFinished(commander, status, WasStoppedByUser());
	}

	SetStoppedByUser(FALSE);
}

BOOL OpPage::OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url)
{
	OP_NEW_DBG("OpPage::OnLoadingFailed", "quick.page");
	OP_DBG(("Playing the PrefsCollectionUI::FailureSound."));
	OpStatus::Ignore(SoundUtils::SoundIt(g_pcui->GetStringPref(PrefsCollectionUI::FailureSound), FALSE));

	return FALSE;
}

// Implementing OpLoadingListener
void OpPage::OnAuthenticationRequired(OpWindowCommander* commander, 
		OpAuthenticationCallback* callback)
{
	URL current_url = m_window_commander->GetWindow()->GetCurrentURL();
	URL newurl = g_url_api->GetURL(callback->Url());

	// if authentication dialog's URL is different, replace address field with that address.
	if (current_url.Id() != newurl.Id())
	{
		// URL module uses a fixed workbuffer, so we need a copy of the url.
		// Otherwise, OnUrlChanged may modify the buffer and we end up with
		// the wrong URL.
		uni_char* actual_url = uni_strdup(callback->Url());
		OnUrlChanged(commander, actual_url);
		op_free(actual_url);
	}

	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageAuthenticationRequired(commander, callback);
	}
}

// Implementing OpLoadingListener
void OpPage::OnStartUploading(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageStartUploading(commander);
	}
}

// Implementing OpLoadingListener
void OpPage::OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageUploadingFinished(commander, status);
	}
}

void OpPage::OnCancelDialog(OpWindowCommander* commander, DialogCallback* callback)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageCancelDialog(commander, callback);
	}
}

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
// Implementing OpLoadingListener
void OpPage::OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnPageSearchSuggestionsRequested(commander, url, callback);
}
#endif

void OpPage::OnGeolocationAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnGeolocationAccess(commander, hosts);
}

void OpPage::OnCameraAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnCameraAccess(commander, hosts);
}

OP_STATUS OpPage::OnJSFullScreenRequest(OpWindowCommander* commander, BOOL enable_fullscreen)
{
	OP_STATUS status = OpStatus::ERR_NOT_SUPPORTED;
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		status = m_listeners.GetNext(iterator)->OnJSFullScreenRequest(commander, enable_fullscreen);
		if (OpStatus::IsError(status))
			break;
	}
	return status;
}

#ifdef EMBROWSER_SUPPORT
// Implementing OpLoadingListener
void OpPage::OnUndisplay(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageUndisplay(commander);
	}
}

// Implementing OpLoadingListener
void OpPage::OnLoadingCreated(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageLoadingCreated(commander);
	}
}
#endif // EMBROWSER_SUPPORT

void OpPage::OnWebHandler(OpWindowCommander* commander, WebHandlerCallback* cb)
{
	DesktopWindow* dw = NULL;
	if (commander)
		dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(commander->GetOpWindow());

	WebHandlerPermissionController::Add(cb, dw);
}

void OpPage::OnWebHandlerCancelled(OpWindowCommander* commander, WebHandlerCallback* cb)
{
	WebHandlerPermissionController::Remove(cb);
}

BOOL OpPage::OnMailToWebHandlerRegister(OpWindowCommander* commander, const uni_char* url, const uni_char* description)
{
	OP_STATUS rc = WebmailManager::GetInstance()->OnWebHandlerAdded(url, description, TRUE);
	// Return FALSE on success to let core update ini file (handlers.ini)
	return OpStatus::IsSuccess(rc) ? FALSE : TRUE;
}

void OpPage::OnMailToWebHandlerUnregister(OpWindowCommander* commander, const uni_char* url)
{
	WebmailManager::GetInstance()->OnWebHandlerRemoved(url);
}

void OpPage::IsMailToWebHandlerRegistered(OpWindowCommander* commander, MailtoWebHandlerCheckCallback* cb)
{
	BOOL is_registered = WebmailManager::GetInstance()->IsWebHandlerRegistered(cb->GetURL());
	cb->OnCompleted(is_registered);
}

#ifdef PLUGIN_AUTO_INSTALL
// Implementing OpDocumentListener
void OpPage::NotifyPluginDetected(OpWindowCommander* commander, const OpStringC& mime_type)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPagePluginDetected(commander, mime_type);
	}
}

void OpPage::NotifyPluginMissing(OpWindowCommander* commander, const OpStringC& mime_type)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageNotifyPluginMissing(commander, mime_type);
	}
}

void OpPage::RequestPluginInfo(OpWindowCommander* commander, const OpStringC& mime_type, OpString& plugin_name, BOOL& available)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageRequestPluginInfo(commander, mime_type, plugin_name, available);
	}
}

void OpPage::RequestPluginInstallDialog(OpWindowCommander* commander, const PluginInstallInformation& information)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageRequestPluginInstallDialog(commander, information);
	}
}

#endif // PLUGIN_AUTO_INSTALL

#if defined SUPPORT_DATABASE_INTERNAL
void OpPage::OnIncreaseQuota(OpWindowCommander* commander, const uni_char* db_name, const uni_char* db_domain, const uni_char* db_type, OpFileLength current_quota_size, OpFileLength quota_hint, QuotaCallback *callback)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageIncreaseQuota(commander, db_name, db_domain, db_type, current_quota_size, quota_hint, callback);
	}
}
#endif // SUPPORT_DATABASE_INTERNAL


// Implementing OpMenuListener
void OpPage::OnPopupMenu(OpWindowCommander* commander, OpDocumentContext& context, const OpPopupMenuRequestMessage* message)
{
	OpListenersIterator iterator(m_listeners);
	while (m_listeners.HasNext(iterator))
		if (m_listeners.GetNext(iterator)->OnPagePopupMenu(commander, context))
			return;

	OpAutoPtr<DesktopMenuContext> menu_context(OP_NEW(DesktopMenuContext, ()));
	if (menu_context.get() == NULL)
		return;

	RETURN_VOID_IF_ERROR(menu_context->Init(commander, &context, message));
	const uni_char* menu_name  = NULL;
	OpRect rect;
	OpPoint point = context.GetScreenPosition();
	ResolveContextMenu(point, menu_context.get(), menu_name, rect, WindowCommanderProxy::GetType(GetWindowCommander()));
	if (InvokeContextMenuAction(point, menu_context.get(), menu_name, rect))
		menu_context.release();
}

# ifdef WIC_MIDCLICK_SUPPORT
void OpPage::OnMidClick(OpWindowCommander * commander, OpDocumentContext& context,
						BOOL mousedown,	ShiftKeyState modifiers)
{
	MidClickManager::OnMidClick(commander, context, this, mousedown, modifiers, FALSE);
}
# endif // WIC_MIDCLICK_SUPPORT

// Implementing OpDocumentListener
void OpPage::OnSecurityModeChanged(OpWindowCommander* commander, SecurityMode mode, BOOL inline_elt)
{
	if (g_desktop_security_manager->IsHighSecurity(mode))
		RefreshSecurityInformation(mode);

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageSecurityModeChanged(commander, mode, inline_elt);
	}
}

#ifdef TRUST_RATING
// Implementing OpDocumentListener
void OpPage::OnTrustRatingChanged(OpWindowCommander* commander, TrustRating new_rating)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageTrustRatingChanged(commander, new_rating);
	}
}
#endif // TRUST_RATING

// Implementing OpDocumentListener
void OpPage::OnTitleChanged(OpWindowCommander* commander, const uni_char* title)
{
	RETURN_VOID_IF_ERROR(m_title.Set(title));

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageTitleChanged(commander, title);
	}
}
// Implementing OpDocumentListener
void OpPage::OnLinkClicked(OpWindowCommander* commander)
{
	OP_NEW_DBG("OpPage::OnLinkClicked", "quick.page");
	OP_DBG(("Playing the PrefsCollectionUI::ClickedSound."));

	OpStatus::Ignore(SoundUtils::SoundIt(g_pcui->GetStringPref(PrefsCollectionUI::ClickedSound), FALSE));
}

#ifdef SUPPORT_VISUAL_ADBLOCK
// Implementing OpDocumentListener
void OpPage::OnContentBlocked(OpWindowCommander* commander, const uni_char *image_url)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageContentBlocked(commander, image_url);
	}
}

// Implementing OpDocumentListener
void OpPage::OnContentUnblocked(OpWindowCommander* commander, const uni_char *image_url)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageContentUnblocked(commander, image_url);
	}
}
#endif // SUPPORT_VISUAL_ADBLOCK

// Implementing OpDocumentListener
void OpPage::OnAccessKeyUsed(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageAccessKeyUsed(commander);
	}
}

// Implementing OpDocumentListener
void OpPage::OnHover(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageHover(commander, url, link_title, is_image);
	}
}

// Implementing OpDocumentListener
void OpPage::OnNoHover(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageNoHover(commander);
	}
}

void OpPage::OnTabFocusChanged(OpWindowCommander* commander, OpDocumentContext& context)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnTabFocusChanged(commander, context);
	}
}

// Implementing OpDocumentListener
void OpPage::OnSearchReset(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageSearchReset(commander);
	}
}

// Implementing OpDocumentListener
void OpPage::OnSearchHit(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageSearchHit(commander);
	}
}

// Implementing OpDocumentListener
void OpPage::OnDocumentIconAdded(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageDocumentIconAdded(commander);
	}
}

// Implementing OpDocumentListener
void OpPage::OnScaleChanged(OpWindowCommander* commander, UINT32 newScale)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageScaleChanged(commander, newScale);
	}
}

// Implementing OpDocumentListener
void OpPage::OnInnerSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageInnerSizeRequest(commander, width, height);
	}
}

// Implementing OpDocumentListener
void OpPage::OnGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageGetInnerSize(commander, width, height);
	}
}

// Implementing OpDocumentListener
void OpPage::OnOuterSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageOuterSizeRequest(commander, width, height);
	}
}

// Implementing OpDocumentListener
void OpPage::OnGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageGetOuterSize(commander, width, height);
	}
}

// Implementing OpDocumentListener
void OpPage::OnMoveRequest(OpWindowCommander* commander, INT32 x, INT32 y)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageMoveRequest(commander, x, y);
	}
}

// Implementing OpDocumentListener
void OpPage::OnGetPosition(OpWindowCommander* commander, INT32* x, INT32* y)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageGetPosition(commander, x, y);
	}
}

// Implementing OpDocumentListener
void OpPage::OnWindowAttachRequest(OpWindowCommander* commander, BOOL attached)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageWindowAttachRequest(commander, attached);
	}
}

// Implementing OpDocumentListener
void OpPage::OnGetWindowAttached(OpWindowCommander* commander, BOOL* attached)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageGetWindowAttached(commander, attached);
	}
}

// Implementing OpDocumentListener
void OpPage::OnClose(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageClose(commander);
	}
}

// Implementing OpDocumentListener
void OpPage::OnRaiseRequest(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageRaiseRequest(commander);
	}
}

// Implementing OpDocumentListener
void OpPage::OnLowerRequest(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageLowerRequest(commander);
	}
}

// Implementing OpDocumentListener
void OpPage::OnDrag(OpWindowCommander* commander, const OpRect rect, const uni_char* link_url, const uni_char* link_title, const uni_char* image_url)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageDrag(commander, rect, link_url, link_title, image_url);
	}
}

#ifdef DND_DRAGOBJECT_SUPPORT
void OpPage::OnDragLeave(OpWindowCommander* commander, class DesktopDragObject* drag_object)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnDragLeave(commander, drag_object);
	}
}

void OpPage::OnDragMove(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnDragMove(commander, drag_object, point);
	}
}

void OpPage::OnDragDrop(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnDragDrop(commander, drag_object, point);
	}
}
#endif // DND_DRAGOBJECT_SUPPORT

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
void OpPage::OnSearchSuggestionsUsed(OpWindowCommander* commander, BOOL suggestions)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnPageSearchSuggestionsUsed(commander, suggestions);
}
#endif // DOC_SEARCH_SUGGESTIONS_SUPPORT

#ifdef GADGET_SUPPORT
// Implementing OpDocumentListener
void OpPage::OnGadgetInstall(OpWindowCommander* commander, URLInformation* url_info)
{
	OpStringC8 mime_type(url_info->MIMEType());

	if (mime_type.Compare("application/x-opera-extension") == 0)
	{
		g_desktop_extensions_manager->InstallFromExternalURL(url_info->URLName());
		// url_info object is no longer needed, because extensions manager handles
		// downloading and installation of extension
		url_info->URL_Information_Done();
	}
}
#endif // GADGET_SUPPORT

// Implementing OpDocumentListener
void OpPage::OnStatusText(OpWindowCommander* commander, const uni_char* text)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageStatusText(commander, text);
	}
}

// Implementing OpDocumentListener
void OpPage::OnConfirm(OpWindowCommander* commander, const uni_char *message, OpDocumentListener::DialogCallback *callback)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageConfirm(commander, message, callback);
	}
}

// Implementing OpDocumentListener
void OpPage::OnGenericError(OpWindowCommander* commander, const uni_char *title, const uni_char *message, const uni_char *additional)
{
	OpStatus::Ignore(SimpleDialog::ShowDialog(WINDOW_NAME_MESSAGE_DIALOG, NULL, message, title, Dialog::TYPE_OK, Dialog::IMAGE_ERROR));
}

// Implementing OpDocumentListener
void OpPage::OnJSAlert(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageJSAlert(commander, hostname, message, callback);
	}
}

// Implementing OpDocumentListener
void OpPage::OnJSConfirm(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageJSConfirm(commander, hostname, message, callback);
	}
}

// Implementing OpDocumentListener
void OpPage::OnJSPrompt(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, const uni_char *default_value, OpDocumentListener::JSDialogCallback *callback)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageJSPrompt(commander, hostname, message, default_value, callback);
	}
}

// Implementing OpDocumentListener
void OpPage::OnJSPopup(OpWindowCommander* commander,	const uni_char *url, const uni_char *name, int left, int top, int width, int height, BOOL3 scrollbars, BOOL3 location, BOOL refuse, BOOL unrequested, OpDocumentListener::JSPopupCallback *callback)
{
	m_current_popup_callback = callback;

	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageJSPopup(commander, url, name, left, top, width, height, scrollbars, location, refuse, unrequested, callback);
	}

	ContinuePopup();
}

#if defined WIC_CAP_JAVASCRIPT_PRINT && defined _PRINT_SUPPORT_
// Implementing OpDocumentListener
void OpPage::OnJSPrint(OpWindowCommander * commander, PrintCallback * callback)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageJSPrint(commander, callback);
	}
}
#endif // WIC_CAP_JAVASCRIPT_PRINT && _PRINT_SUPPORT_

#if defined WIC_CAP_JAVASCRIPT_PRINT && defined _PRINT_SUPPORT_
// Implementing OpDocumentListener
void OpPage::OnUserPrint(OpWindowCommander * commander, PrintCallback * callback)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageUserPrint(commander, callback);
	}
}
#endif // WIC_CAP_JAVASCRIPT_PRINT && _PRINT_SUPPORT_

#if defined (DOM_GADGET_FILE_API_SUPPORT) && defined (WIC_CAP_HAS_MOUNTPOINT_API)
// Implementing OpDocumentListener
void OpPage::OnMountpointFolderRequest(OpWindowCommander* commander, MountPointFolderCallback* callback, MountPointFolderCallback::FolderType folder_type)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageMountpointFolderRequest(commander, callback, folder_type);
	}
}

// Implementing OpDocumentListener
void OpPage::OnMountpointFolderCancel(OpWindowCommander* commander, MountPointFolderCallback* callback)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPageMountpointFolderCancel(commander, callback);
	}
}
#endif // DOM_GADGET_FILE_API_SUPPORT && WIC_CAP_HAS_MOUNTPOINT_API

// Implementing OpDocumentListener
void OpPage::OnFormSubmit(OpWindowCommander* commander, OpDocumentListener::FormSubmitCallback *callback)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageFormSubmit(commander, callback);
	}
}

// Implementing OpDocumentListener
void OpPage::OnPluginPost(OpWindowCommander* commander, OpDocumentListener::PluginPostCallback *callback)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPagePluginPost(commander, callback);
	}
}

// Implementing OpDocumentListener
void OpPage::OnDownloadRequest(OpWindowCommander* commander, OpDocumentListener::DownloadCallback * callback)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageDownloadRequest(commander, callback);
	}
}

// Implementing OpDocumentListener
void OpPage::OnSubscribeFeed(OpWindowCommander* commander, const uni_char* url)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageSubscribeFeed(commander, url);
	}
}

// Implementing OpDocumentListener
void OpPage::OnAskAboutUrlWithUserName(OpWindowCommander* commander, const uni_char* url, const uni_char* hostname, const uni_char* username, OpDocumentListener::DialogCallback* callback)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageAskAboutUrlWithUserName(commander, url, hostname, username, callback);
	}
}

// Implementing OpDocumentListener
void OpPage::OnAskAboutFormRedirect(OpWindowCommander* commander, const uni_char* source_url, const uni_char* target_url, OpDocumentListener::DialogCallback* callback)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageAskAboutFormRedirect(commander, source_url, target_url, callback);
	}
}

// Implementing OpDocumentListener
void OpPage::OnFormRequestTimeout(OpWindowCommander* commander, const uni_char* url)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageFormRequestTimeout(commander, url);
	}
}

// Implementing OpDocumentListener
BOOL OpPage::OnAnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		if(m_advanced_listeners.GetNext(iterator)->OnPageAnchorSpecial(commander, info))
			return TRUE;
	}

	return FALSE;
}

// Implementing OpDocumentListener
void OpPage::OnAskPluginDownload(OpWindowCommander * commander)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageAskPluginDownload(commander);
	}
}

// Implementing OpDocumentListener
void OpPage::OnVisibleRectChanged(OpWindowCommander * commander, const OpRect& visible_rect)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageVisibleRectChanged(commander, visible_rect);
	}
}

void OpPage::OnOnDemandStateChangeNotify(OpWindowCommander* commander, BOOL has_placeholders)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageOnDemandStateChangeNotify(commander, has_placeholders);
	}
}

// Implementing OpDocumentListener
void OpPage::OnMailTo(OpWindowCommander* commander, const OpStringC8& raw_mailto, BOOL mailto_from_dde, BOOL mailto_from_form, const OpStringC8& servername)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageMailTo(commander, raw_mailto, mailto_from_dde, mailto_from_form, servername);
	}
}

// Implementing OpDocumentListener
void OpPage::OnQueryGoOnline(OpWindowCommander* commander, const uni_char *message, DialogCallback *callback)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageQueryGoOnline(commander, message, callback);
	}
}

#ifdef DRAG_SUPPORT
void OpPage::OnFileDropConfirm(OpWindowCommander* commander, const uni_char *server_name, DialogCallback *callback)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnFileDropConfirm(commander, server_name, callback);
	}
}
#endif // DRAG_SUPPORT

void OpPage::OnDocumentScroll(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnDocumentScroll(commander);
	}
}

void OpPage::OnUnexpectedComponentGone(OpWindowCommander* commander, OpComponentType type, const OpMessageAddress& address, const OpStringC& information)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnUnexpectedComponentGone(commander,type, address, information);
	}
}

void OpPage::OnSetupInstall(OpWindowCommander* commander, const uni_char* url, URLContentType content_type)
{
	for (OpListenersIterator iterator(m_advanced_listeners); m_advanced_listeners.HasNext(iterator);)
	{
		m_advanced_listeners.GetNext(iterator)->OnPageSetupInstall(commander, url, content_type);
	}
}
