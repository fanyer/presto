/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/desktop_pi/DesktopColorChooser.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/desktop_util/mail/mailcompose.h"
#include "adjunct/desktop_util/mail/mailto.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/controller/DownloadSelectionController.h"
#include "adjunct/quick/dialogs/AskPluginDownloadDialog.h"
#include "adjunct/quick/dialogs/DownloadDialog.h"
#include "adjunct/quick/dialogs/PasswordDialog.h"
#if defined _PRINT_SUPPORT_ && defined GENERIC_PRINTING
# include "adjunct/quick/dialogs/PrintProgressDialog.h"
#endif // _PRINT_SUPPORT_ && defined GENERIC_PRINTING
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/dialogs/SecurityInformationDialog.h"
#include "adjunct/quick/dialogs/ServerPropertiesDialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/dialogs/jsdialogs.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/dialogs/WebStorageQuotaDialog.h"
#include "adjunct/quick/widgets/OpSpeedDialView.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpPage.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#ifdef _BITTORRENT_SUPPORT_
# include "adjunct/quick/dialogs/BTSelectionDialog.h"
#endif // _BITTORRENT_SUPPORT_
#include "adjunct/quick/dialogs/SetupApplyDialog.h"

#include "modules/display/VisDevListeners.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/logdoc_util.h"

#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/thumbnails/thumbnailgenerator.h"
#include "modules/dochand/winman.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/dragdrop/dragdrop_manager.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegastencil.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/skin/OpSkinManager.h"
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
# include "adjunct/desktop_util/search/search_net.h"
# include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES

#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/managers/DownloadManagerManager.h"

#ifdef DESKTOP_PRINTDIALOG
# include "adjunct/desktop_pi/desktop_printdialog.h"
#endif

#ifdef _MACINTOSH_
#include "adjunct/quick/widgets/OpPagebar.h"
#endif

#ifdef _PRINT_SUPPORT_
# include "modules/display/prn_info.h"
#endif // _PRINT_SUPPORT_

#ifdef EMBROWSER_SUPPORT
extern long gVendorDataID;
#endif

#ifdef M2_SUPPORT
#include "adjunct/m2/src/engine/engine.h"
#endif // M2_SUPPORT

#ifdef ENABLE_USAGE_REPORT
#include "adjunct/quick/usagereport/UsageReport.h"
#endif

#include "adjunct/quick/application/BrowserAuthenticationListener.h"

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

#ifdef _MACINTOSH_
#define DEFAULT_DIMMER_OPACITY	0
#else
#define DEFAULT_DIMMER_OPACITY	128
#endif

#include "modules/selftest/optestsuite.h"

/***********************************************************************************
**
**	InsecureFormSubmitDialog
**
***********************************************************************************/

class OnFormSubmitDialog : public SimpleDialog
#ifdef WIC_CAP_FORM_SUBMIT_CALLBACK_HAS_LISTENER
	, public OpDocumentListener::FormSubmitCallbackListener
#endif //WIC_CAP_FORM_SUBMIT_CALLBACK_HAS_LISTENER
{
	public:

		BOOL Init(DesktopWindow* parent_window, OpDocumentListener::FormSubmitCallback * callback, OpBrowserView *parent_page_view)
		{
			m_callback = callback;
			OpString title, message;
			dont_repeat = FALSE;

#ifdef WIC_CAP_FORM_SUBMIT_CALLBACK_HAS_LISTENER
			m_callback->AddListener(this);
#endif //WIC_CAP_FORM_SUBMIT_CALLBACK_HAS_LISTENER
			g_languageManager->GetString(Str::D_SECURITY_WARNING, title);

			OpDocumentListener::SecurityMode referrerSecurity = callback->ReferrerSecurityMode();
			BOOL https_http_submit =	callback->FormServerIsHTTP()
									 && referrerSecurity != OpDocumentListener::UNKNOWN_SECURITY
									 && referrerSecurity != OpDocumentListener::NO_SECURITY;
			if (https_http_submit)
			{
				OpString name;
				OpString message_format;

				g_languageManager->GetString(Str::D_SUBMIT_FORM_HTTPS_HTTP, message_format);

				name.Set(m_callback->FormServerName());

				message.AppendFormat(message_format.CStr(), name.CStr());
			}
			else if (callback->FormServerIsHTTP() && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::WarnInsecureFormSubmit))
			{
				g_languageManager->GetString(Str::D_SUBMIT_FORM_HTTP_HTTP, message);
			}
			else
			{
				// This dialog shall not be shown, default behaviour is to accept submit.
				m_callback->Continue(TRUE);
				OP_DELETE(this);
				return FALSE;
			}


			SimpleDialog::Init(WINDOW_NAME_ON_FORM_SUBMIT,	title, message, parent_window, TYPE_OK_CANCEL, https_http_submit ? IMAGE_WARNING : IMAGE_INFO,
								FALSE, (INT32 *)NULL, https_http_submit ? NULL : &dont_repeat, (const char *) NULL, 1, parent_page_view);
			return TRUE;
		}

		Str::LocaleString GetOkTextID()
		{
			return Str::SI_IDFORM_SUBMIT; // "Submit"
		}

		UINT32 OnOk()
		{
			if (dont_repeat == TRUE)
			{
				TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::WarnInsecureFormSubmit, FALSE));
			}

			if (m_callback)
				m_callback->Continue(TRUE);
			return 1;
		}

		void OnCancel()
		{
			if (m_callback)
				m_callback->Continue(FALSE);
		}

		void OnClose(BOOL user_initiated)
		{
			SimpleDialog::OnClose(user_initiated);
		}

#ifdef WIC_CAP_FORM_SUBMIT_CALLBACK_HAS_LISTENER
		void CallbackDone()
		{
			m_callback = NULL;
			this->Close(TRUE, FALSE);
		}
#endif //WIC_CAP_FORM_SUBMIT_CALLBACK_HAS_LISTENER

#ifdef VEGA_OPPAINTER_SUPPORT
		virtual BOOL			GetModality() {return FALSE;}
		virtual BOOL			GetOverlayed() {return TRUE;}
		virtual BOOL			GetDimPage() {return TRUE;}
#endif
	private:

		OpDocumentListener::FormSubmitCallback*	m_callback;
		BOOL dont_repeat;
};

/***********************************************************************************
**
**  PluginPostDialog
**
***********************************************************************************/

class OnPluginPostDialog : public SimpleDialog
{
    public:

        BOOL Init(DesktopWindow* parent_window, OpDocumentListener::PluginPostCallback * callback)
        {
            m_callback = callback;
            OpString title, message;

            g_languageManager->GetString(Str::D_SECURITY_WARNING, title);

            OpDocumentListener::SecurityMode referrerSecurity = callback->ReferrerSecurityMode();
            BOOL https_http_submit = callback->PluginServerIsHTTP()
                                     && referrerSecurity != OpDocumentListener::UNKNOWN_SECURITY
                                     && referrerSecurity != OpDocumentListener::NO_SECURITY;
            if (https_http_submit)
            {
                OpString name;
                OpString message_format;

                name.Set(m_callback->PluginServerName());

                if (name.IsEmpty()) // javascript url
                    g_languageManager->GetString(Str::D_SUBMIT_PLUGIN_HTTPS_HTTP, message);
                else
                {
                    g_languageManager->GetString(Str::D_SUBMIT_FORM_HTTPS_HTTP, message_format);
                    message.AppendFormat(message_format.CStr(), name.CStr());
                }
            }
            else
            {
                // This dialog shall not be shown, default behaviour is to post plugin data
                m_callback->Continue(TRUE);
                OP_DELETE(this);
                return FALSE;
            }

            SimpleDialog::Init(WINDOW_NAME_ON_PLUGIN_POST, title, message, parent_window, TYPE_OK_CANCEL, IMAGE_WARNING, FALSE, NULL, NULL, NULL, 1);
            return TRUE;
        }

        Str::LocaleString GetOkTextID()
        {
            return Str::SI_IDFORM_SUBMIT; // "Submit"
        }

        UINT32 OnOk()
        {
            m_callback->Continue(TRUE);
            return 1;
        }

        void OnCancel()
        {
            m_callback->Continue(FALSE);
        }

        void OnClose(BOOL user_initiated)
        {
            SimpleDialog::OnClose(user_initiated);
        }

		BOOL GetOverlayed() {return TRUE;}

		BOOL GetDimPage() {return TRUE;}
    private:

        OpDocumentListener::PluginPostCallback* m_callback;
};


class ConfirmDialogController : public SimpleDialogController
{
public:
	ConfirmDialogController(const uni_char* message, const uni_char* title, OpDocumentListener::DialogCallback* callback):
			SimpleDialogController(SimpleDialogController::TYPE_YES_NO, SimpleDialogController::IMAGE_QUESTION, WINDOW_NAME_CONFIRM, message, title),
			m_callback(callback)
	{
	}

private:
	virtual void OnOk()
	{
		m_callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_YES);
		g_application->UpdateOfflineMode(OpDocumentListener::DialogCallback::REPLY_YES);
	}

	virtual void OnCancel()
	{
		m_callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_NO);
		g_application->UpdateOfflineMode(OpDocumentListener::DialogCallback::REPLY_NO);
	}

private:
	OpDocumentListener::DialogCallback* m_callback;
};

/***********************************************************************************
**
**	CallbackColorChooserListener
**    - Listener for color chooser request from core
**
***********************************************************************************/

#ifdef WIC_COLORSELECTION_LISTENER
class CallbackColorChooserListener : public OpColorChooserListener
{
public:
	COLORREF selected_color;
	BOOL is_color_selected;

	CallbackColorChooserListener() : is_color_selected(FALSE) { }
	virtual void OnColorSelected(COLORREF color)
	{
		// We don't get any alpha back from the windows color picker
		selected_color = OP_RGB(GetRValue(color), GetGValue(color), GetBValue(color));
		is_color_selected = TRUE;
	}
};
#endif // WIC_COLORSELECTION_LISTENER


#ifdef VEGA_OPPAINTER_SUPPORT

/***********************************************************************************
**
**	DimmerAnimation is a transparent layer that will act as a dimmer for the page.
**  It supports fading in and out, and can stop or let through mouse input.
**  It also make holes for any search matches that are in view in the visible document.
**
***********************************************************************************/

class OpBrowserView::DimmerAnimation : public QuickAnimationWindow
{
public:
	DimmerAnimation(OpBrowserView* page_view, int opacity = DEFAULT_DIMMER_OPACITY)
		: m_fade_in(true)
		, m_skin_inset(5)
		, m_opacity(opacity)
		, m_curr_opacity(m_opacity)
		, m_page_view(page_view)
	{
	}

	virtual OP_STATUS Init(OpWindow* parent_window)
	{
		RETURN_IF_ERROR(QuickAnimationWindow::Init(m_page_view->GetParentDesktopWindow(), parent_window));

		animateIgnoreMouseInput(false, false);

		MDE_OpWindow* win = (MDE_OpWindow*) GetOpWindow();
		win->SetAllowAsActive(FALSE);
		win->GetMDEWidget()->SetFullyTransparent(true);
		win->GetMDEWidget()->SetScrollingTransparent(true);
		win->GetMDEWidget()->SetScrollInvalidationExtra(m_skin_inset);
		GetWidgetContainer()->SetEraseBg(TRUE);

		return OpStatus::OK;
	}

	void PrepareFadeOut()
	{
		m_fade_in = false;
		GetWidgetContainer()->SetEraseBg(TRUE);
	}

	virtual BOOL OnInputAction(OpInputAction* action)
	{
		switch (action->GetAction())
		{
		case OpInputAction::ACTION_CLOSE_PAGE:
			{
				DesktopWindow* win = g_application->GetDesktopWindowCollection().GetDocumentWindowFromCommander(
					m_page_view->GetWindowCommander());
				if (win)
					return win->OnInputAction(action);
			}
		}

		return QuickAnimationWindow::OnInputAction(action);
	}

	virtual void OnAnimationUpdate(double position)
	{
		QuickAnimationWindow::OnAnimationUpdate(position);
		if (m_fade_in)
			m_curr_opacity = (unsigned int)(m_opacity * position);
		else
			m_curr_opacity = m_opacity - (unsigned int)(m_opacity * position);
		GetWidgetContainer()->GetRoot()->SetBackgroundColor(OP_RGBA(0, 0, 0, m_curr_opacity));
		GetWidgetContainer()->GetRoot()->InvalidateAll();
	}

	virtual void OnAnimationComplete()
	{
		QuickAnimationWindow::OnAnimationComplete();
		if (m_fade_in)
			GetWidgetContainer()->GetRoot()->SetBackgroundColor(OP_RGBA(0, 0, 0, m_opacity));
		else
		{
			Show(FALSE);
			Close(FALSE);
		}
	}

protected:
	bool m_fade_in;
	INT32 m_skin_inset;
	int m_opacity;
	int m_curr_opacity;
	OpBrowserView* m_page_view;
};


class OpBrowserView::SearchDimmerAnimation : public DimmerAnimation
{
public:
	SearchDimmerAnimation(OpBrowserView* page_view)
		: DimmerAnimation(page_view, g_pcui->GetIntegerPref(PrefsCollectionUI::DimSearchOpacity))
		, m_posted_undim_message(false)
	{
		if (OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Search Hit Active Skin"))
			skin_elm->GetMargin(&m_skin_inset, &m_skin_inset, &m_skin_inset, &m_skin_inset, 0);
	}

	virtual OP_STATUS Init(OpWindow* parent_window)
	{
		RETURN_IF_ERROR(DimmerAnimation::Init(parent_window));
		animateIgnoreMouseInput(true, true);
		GetWidgetContainer()->SetEraseBg(FALSE);
		return OpStatus::OK;
	}

	// Implementing OpWidgetListener
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect)
	{
#ifdef WIC_CAP_HAS_SEARCH_MATCHES
		if (m_fade_in)
		{
			VisualDevice *vd = widget->GetVisualDevice();
			VEGAOpPainter *vp = (VEGAOpPainter *) vd->painter;

			// Create path over entire dimview
			int x = 0;
			int y = 0;
			int w = widget->GetBounds().width;
			int h = widget->GetBounds().height;

			VEGAPath dim_path;
			dim_path.moveTo(x, y);
			dim_path.lineTo(x + w, y);
			dim_path.lineTo(x + w, y + h);
			dim_path.lineTo(x, y + h);

			// Create holes in path for every search match
			OpAutoVector<OpRect> all_rects;
			OpAutoVector<OpRect> active_rects;
			if (!m_posted_undim_message && m_page_view->GetWindowCommander()->GetSearchMatchRectangles(all_rects, active_rects))
			{
				for (unsigned int i = 0; i < all_rects.GetCount(); i++)
				{
					OpRect rect = *all_rects.Get(i);

					rect.x += x;
					rect.y += y;
					dim_path.moveTo(rect.x, rect.y);
					dim_path.lineTo(rect.x, rect.y + rect.height);
					dim_path.lineTo(rect.x + rect.width, rect.y + rect.height);
					dim_path.lineTo(rect.x + rect.width, rect.y);
				}
			}
			else if (!m_posted_undim_message)
			{
				// We had no search matches so post a message to undim page.
				g_main_message_handler->PostMessage(MSG_DIM_PAGEVIEW, (MH_PARAM_1)m_page_view, (MH_PARAM_2)false);

				if(WindowCommanderProxy::IsVisualDeviceFocused(m_page_view->GetWindowCommander(), FALSE)
					|| WindowCommanderProxy::IsVisualDeviceFocused(m_page_view->GetWindowCommander(), TRUE))
				{
					// The page is focused, so post a message to hide the findtext bar automatically.
					if (OpInputAction *new_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_FINDTEXT)))
						g_main_message_handler->PostDelayedMessage(MSG_QUICK_DELAYED_ACTION, (MH_PARAM_1)new_action, 0, 0);
				}
				m_posted_undim_message = true;
			}

			dim_path.close(true);
			if (OpStatus::IsSuccess(vp->BeginOpacity(OpRect(x, y, w, h), m_curr_opacity)))
			{
				vp->SetColor(0, 0, 0, 255);
				vp->FillPath(dim_path);
				vp->EndOpacity();
			}

			// Draw skin on active rectangles
			for (unsigned int i = 0; i < active_rects.GetCount(); i++)
			{
				OpRect rect = *active_rects.Get(i);
				g_skin_manager->DrawElement(vd, "Search Hit Active Skin", rect.InsetBy(-m_skin_inset));
			}
		}
#endif // WIC_CAP_HAS_SEARCH_MATCHES
	}

private:
	bool m_posted_undim_message;
};

#endif // VEGA_OPPAINTER_SUPPORT


/***********************************************************************************
**
**	OpBrowserView
**
***********************************************************************************/

OP_STATUS OpBrowserView::Construct(OpBrowserView** obj, BOOL border, OpWindowCommander* window_commander)
{
	if (!obj)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPtr<OpBrowserView> browser_view(OP_NEW(OpBrowserView, (border, window_commander)));
	RETURN_OOM_IF_NULL(browser_view.get());
	RETURN_IF_ERROR(browser_view->init_status);

	*obj = browser_view.release();
	return OpStatus::OK;
}

void SetListeners(OpWindowCommander* commander, OpBrowserView* listener)
{
#ifdef _PRINT_SUPPORT_
	commander->SetPrintingListener(listener);
#endif // _PRINT_SUPPORT_
#ifdef WIC_FILESELECTION_LISTENER
	commander->SetFileSelectionListener(listener);
#endif // WIC_FILESELECTION_LISTENER
#ifdef WIC_COLORSELECTION_LISTENER
	commander->SetColorSelectionListener(listener);
#endif // WIC_COLORSELECTION_LISTENER
#ifdef APPLICATION_CACHE_SUPPORT
	if (!listener)
		commander->SetApplicationCacheListener(NULL);
	else if (g_application->GetApplicationCacheListener())
		commander->SetApplicationCacheListener(g_application->GetApplicationCacheListener());
#endif
}

OpBrowserView::~OpBrowserView()
{
	OP_DELETE(m_page);
	OP_DELETE(m_chooser);
#if defined _PRINT_SUPPORT_ && defined DESKTOP_PRINTDIALOG
	OP_DELETE(m_desktop_printdialog);
#endif

#if defined QUICK_TOOLKIT_PRINT_PROGRESS_DIALOG
	OP_DELETE(m_print_progress_dialog);
#endif

	g_main_message_handler->UnsetCallBack(this, MSG_QUICK_ASK_PLUGIN_DOWNLOAD);
	g_main_message_handler->UnsetCallBack(this, MSG_DIM_PAGEVIEW);
}

OpWindowCommander* OpBrowserView::GetWindowCommander()
{
	return m_page ? m_page->GetWindowCommander() : NULL;
}

OpBrowserView::OpBrowserView(BOOL border, OpWindowCommander* window_commander)
	: m_op_window(NULL)
	, m_page(OP_NEW(OpPage, (window_commander)))
	, m_border(border)
	, m_own_auth(FALSE)
	, m_dim_window(NULL)
	, m_dim_count(0)
	, m_search_dim_window(NULL)
	, m_suppress_dialogs(FALSE)
	, m_suppress_focus_events(FALSE)
	, m_embedded_in_dialog(FALSE)
	, m_hidden(FALSE)
	, m_consume_scrolls(FALSE)
	, m_prefered_width(-1)
	, m_prefered_height(-1)
	, m_last_post_dialog(NULL)
#if defined _PRINT_SUPPORT_ && defined DESKTOP_PRINTDIALOG
	, m_desktop_printdialog(NULL)
#endif
#if defined QUICK_TOOLKIT_PRINT_PROGRESS_DIALOG
	, m_print_progress_dialog(NULL)
#endif
#if defined (WIC_CAP_UPLOAD_FILE_CALLBACK)  || defined (WIC_FILESELECTION_LISTENER)
	, m_chooser(NULL)
# if defined (WIC_CAP_UPLOAD_FILE_CALLBACK)
	, m_upload_callback(NULL)
# elif defined (WIC_FILESELECTION_LISTENER)
	, m_selection_callback(NULL)
# endif // WIC_FILESELECTION_LISTENER
#endif // WIC_CAP_UPLOAD_FILE_CALLBACK || WIC_FILESELECTION_LISTENER
#if defined SUPPORT_DATABASE_INTERNAL
	, m_quota_callback(NULL)
#endif
#ifdef WIC_COLORSELECTION_LISTENER
	, m_color_selection_callback(NULL)
#endif
{
	if (!m_page)
		init_status = OpStatus::ERR_NO_MEMORY;

	if (m_border)
	{
		GetBorderSkin()->SetImage("Browser Skin");
		SetSkinned(TRUE);
	}

#ifdef PREFS_CAP_DEFAULTSEARCHTYPE_STRINGS
	/**
	 * m_search_guid is initially the default search.
	 * It changes each time a search is done with 'Search with' in the right-click menu
	 */
	SearchTemplate* search = g_searchEngineManager->GetDefaultSearch();
	if (search)
		m_search_guid.Set(search->GetUniqueGUID());
#endif

	SetOnMoveWanted(TRUE);
	SetTabStop(TRUE);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilitySkipsMe();
#endif

	g_main_message_handler->SetCallBack(this, MSG_QUICK_DELAYED_COLOR_SELECTOR, 0);
}

void OpBrowserView::OnPageDestroyed(OpPage* page)
{
	OnPageNoHover(GetWindowCommander());
	m_page = NULL;
	WindowCommanderProxy::SetParentInputContext(page->GetWindowCommander(), 0);
	page->SetOpWindow(NULL);
	OpPageListener::OnPageDestroyed(page);
	SetListeners(page->GetWindowCommander(), NULL);
}

void OpBrowserView::OnDeleted()
{
	OpStatus::Ignore(SetPage(NULL));

	OP_DELETE(m_op_window);
	m_op_window = NULL;

	g_main_message_handler->UnsetCallBack(this, MSG_QUICK_DELAYED_COLOR_SELECTOR);

	OpWidget::OnDeleted();
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS OpBrowserView::Init()
{
	RETURN_IF_ERROR(init_status);

	OP_ASSERT(m_op_window == NULL);
	RETURN_IF_ERROR(DesktopOpWindow::Create(&m_op_window));
	OP_ASSERT(m_op_window != NULL);

	RETURN_IF_ERROR(m_op_window->Init(OpWindow::STYLE_CHILD, OpTypedObject::WINDOW_TYPE_UNKNOWN, NULL, GetWidgetContainer()->GetOpView()));
	m_op_window->SetBrowserView(this);

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_DIM_PAGEVIEW, (MH_PARAM_1)this));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_QUICK_ASK_PLUGIN_DOWNLOAD, (MH_PARAM_1)this));

	RETURN_IF_ERROR(m_page->Init());
	RETURN_IF_ERROR(AttachToPage(*m_page));

	return OpStatus::OK;
}

OP_STATUS OpBrowserView::SetPage(OpPage* page)
{
	OpAutoPtr<OpPage> page_holder(page);

	// Will call OnPageDestroyed().
	OP_DELETE(m_page);

	if (page != NULL)
		RETURN_IF_ERROR(AttachToPage(*page));

	m_page = page_holder.release();
	return OpStatus::OK;
}

OP_STATUS OpBrowserView::AttachToPage(OpPage& page)
{
	RETURN_IF_ERROR(page.SetOpWindow(m_op_window));
	RETURN_IF_ERROR(page.AddAdvancedListener(this));
	WindowCommanderProxy::SetParentInputContext(page.GetWindowCommander(), this);
	SetListeners(page.GetWindowCommander(), this);

	return OpStatus::OK;
}

/***********************************************************************************
**
**	BeginWrite
**
***********************************************************************************/

URL	OpBrowserView::BeginWrite()
{
	URL url = g_url_api->GetNewOperaURL();

	url.Unload();
	url.ForceStatus(URL_LOADING);
#ifndef USE_ABOUT_FRAMEWORK
	url.SetAttribute(URL::KMIME_ForceContentType, "text/html");
#endif

	return url;
}

/***********************************************************************************
**
**	GetThumbnailImage
**
***********************************************************************************/

Image OpBrowserView::GetThumbnailImage(long width, long height, BOOL high_quality)
{
	OpAutoPtr<ThumbnailGenerator> generator (CreateThumbnailGenerator(width, height, high_quality));
	if (!generator.get())
		return Image();

	return generator->GenerateThumbnail();
}


Image OpBrowserView::GetThumbnailImage(long width, long height, BOOL high_quality, OpPoint& logo_start)
{
	OpAutoPtr<ThumbnailGenerator> generator (CreateThumbnailGenerator(width, height, high_quality));
	if (!generator.get())
		return Image();

	Image thumbnail = generator->GenerateThumbnail();
	logo_start = generator->FindLogo();

	return thumbnail;
}

// same as the thumbnail methods except it takes a full sized snapshot instead
Image OpBrowserView::GetSnapshotImage()
{
	long width = 0, height = 0;
	OpRect rect = WindowCommanderProxy::GetVisualDeviceVisibleRect(GetWindowCommander());

	width = rect.width;
	height = rect.height;

	OpAutoPtr<ThumbnailGenerator> generator (CreateThumbnailGenerator(width, height, TRUE));
	if (!generator.get())
		return Image();

	Image thumbnail = generator->GenerateSnapshot();

	return thumbnail;
}

ThumbnailGenerator* OpBrowserView::CreateThumbnailGenerator(long width, long height, BOOL high_quality)
{
	OP_ASSERT(width > 0 && height > 0);

	ThumbnailGenerator* generator = GetWindowCommander()->CreateThumbnailGenerator();
	if (!generator)
		return 0;

	generator->SetThumbnailSize(width, height);
	generator->SetHighQuality(high_quality);

	return generator;
}

/***********************************************************************************
**
**	EndWrite
**
***********************************************************************************/

void OpBrowserView::EndWrite(URL& url)
{
#ifndef USE_ABOUT_FRAMEWORK
	url.WriteDocumentDataFinished();
#endif

	URL dummy;

	WindowCommanderProxy::OpenURL(m_page->GetWindowCommander(), url, dummy, FALSE,FALSE,-1);

	url.ForceStatus(URL_LOADED);
}

void OpBrowserView::SetSupportsNavigation(BOOL supports)
{
	m_page->SetSupportsNavigation(supports);
}

void OpBrowserView::SetSupportsSmallScreen(BOOL supports)
{
	m_page->SetSupportsSmallScreen(supports);
}

/***********************************************************************************
**
**	OnAdded
**
***********************************************************************************/

void OpBrowserView::OnAdded()
{
	if (m_op_window)
	{
		m_op_window->SetParent(NULL, GetWidgetContainer()->GetOpView());
	}
	else
	{
		init_status = Init();
	}
}

/***********************************************************************************
**
**	FocusDocument
**
***********************************************************************************/

void OpBrowserView::FocusDocument(FOCUS_REASON reason)
{
	if(!WindowCommanderProxy::FocusDocument(GetWindowCommander(), reason))
	{
		WindowCommanderProxy::SetFocusToVisualDevice(GetWindowCommander(), reason);
	}
}

/***********************************************************************************
**
**	OnFocus
**
***********************************************************************************/

void OpBrowserView::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	// Fix for bug #251071. We should fix this elsewhere (context code)
	if (GetParent() == NULL)
		return;

	if (focus)
	{
		FocusDocument(reason);
	}
}

/***********************************************************************************
**
**	OnShow
**
***********************************************************************************/

void OpBrowserView::OnShow(BOOL show)
{
	if (OpStatus::IsSuccess(init_status))
		m_op_window->Show(show);
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void OpBrowserView::OnLayout()
{
	OpRect rect = GetRect(TRUE);

	INT32 left = 0;
	INT32 top = 0;
	INT32 right = 0;
	INT32 bottom = 0;

	if (m_border && !m_page->IsFullScreen())
	{
		GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);

#ifdef _MACINTOSH_
		if(gVendorDataID == 'OPRA')
		{
			AddPadding(rect);

			OpPagebar* pagebar = NULL;
			if(GetParentDesktopWindow())
			{
				pagebar = (OpPagebar*)GetParentDesktopWindow()->GetWidgetByType(OpTypedObject::WIDGET_TYPE_PAGEBAR);
			}
			if(pagebar)
			{
				// If the pagebar is closest neighbor to the browserview then we don't
				// want the border there since the pagebar has a blue separator line there already.

				OpRect pagebarRect = pagebar->GetScreenRect();
				OpRect browserRect = GetScreenRect();

				switch(pagebar->GetAlignment())
				{
				case OpBar::ALIGNMENT_TOP:
					// Skip top border
					if((pagebarRect.y+pagebarRect.height) == browserRect.y)
					{
						rect.y--;
						rect.height++;
					}
					break;
				case OpBar::ALIGNMENT_BOTTOM:
					// Skip bottom border
					if(pagebarRect.y == (browserRect.y+browserRect.height))
					{
						rect.y++;
					}
					break;
				}
			}
		}
#endif
	}

	m_op_window->SetOuterPos(rect.x + left, rect.y + top);
	rect.width -= left + right;
	rect.width = MAX(rect.width, 0);
	rect.height -= top + bottom;
	rect.height = MAX(rect.height, 0);

	/* Because the OpWindow is the child of an OpView, a change in maximum size might not have propagated.
	 * This would stop the SetOuterSize() call from executing correctly, So set maximum size explicitly here.
	 * Note: maximum size is unnecessary for internal windows anyway - it should probably be removed.
	 */
	m_op_window->SetMaximumInnerSize(rect.width, rect.height);

	// Now set the size, without being limited by the maximum
	m_op_window->SetOuterSize(rect.width, rect.height);

	if (m_dim_window)
		m_dim_window->GetOpWindow()->SetOuterSize(rect.width, rect.height);
}

Window* OpBrowserView::GetWindow()
{
	return m_page->GetWindowCommander() ? m_page->GetWindowCommander()->GetWindow() : NULL;
}

/***********************************************************************************
**
**	AddListener
**
***********************************************************************************/
OP_STATUS OpBrowserView::AddListener(OpPageListener* listener)
{
	if (m_page)
	{
		return m_page->AddListener(listener);
	}
	return OpStatus::ERR;
}

/***********************************************************************************
**
**	RemoveListener
**
***********************************************************************************/
OP_STATUS OpBrowserView::RemoveListener(OpPageListener* listener)
{
	if (m_page)
	{
		return m_page->RemoveListener(listener);
	}
	return OpStatus::ERR;
}

/***********************************************************************************
**
**	AddAdvancedListener
**
***********************************************************************************/
OP_STATUS OpBrowserView::AddAdvancedListener(OpPageAdvancedListener* listener)
{
	if (m_page)
	{
		return m_page->AddAdvancedListener(listener);
	}
	return OpStatus::ERR;
}

/***********************************************************************************
**
**	RemoveAdvancedListener
**
***********************************************************************************/
OP_STATUS OpBrowserView::RemoveAdvancedListener(OpPageAdvancedListener* listener)
{
	if (m_page)
	{
		return m_page->RemoveAdvancedListener(listener);
	}
	return OpStatus::ERR;
}

/***********************************************************************************
**
**	GetSizeFromBrowserViewSize
**
***********************************************************************************/

void OpBrowserView::GetSizeFromBrowserViewSize(INT32& width, INT32& height)
{
	INT32 left = 0;
	INT32 top = 0;
	INT32 right = 0;
	INT32 bottom = 0;

	if (m_border && !m_page->IsFullScreen())
	{
		GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
	}

	width += left + right;
	height += top + bottom;
}

/***********************************************************************************
**
**	RequestDimmed
**
***********************************************************************************/
OP_STATUS OpBrowserView::RequestDimmed(bool dimmed)
{
	m_dim_count += dimmed ? 1 : -1;

	if (m_dim_count < 0)
	{
		OP_ASSERT(!"Too many undim requests");
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	OP_STATUS result = OpStatus::OK;

	if (m_dim_count > 0 && !m_dim_window)
		result = Dim(m_dim_window);
	else if (m_dim_count == 0)
		Undim(m_dim_window);

	return result;
}

/***********************************************************************************
**
**	Dim
**
***********************************************************************************/

OP_STATUS OpBrowserView::Dim(DimmerAnimation*& dim_window)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	OP_ASSERT(dim_window == NULL);

	OpAutoPtr<DimmerAnimation> dim_window_holder(&dim_window == &m_dim_window
			? OP_NEW(DimmerAnimation, (this))
			: OP_NEW(SearchDimmerAnimation, (this)));
	RETURN_OOM_IF_NULL(dim_window_holder.get());
	RETURN_IF_ERROR(dim_window_holder->Init(m_op_window));
	dim_window = dim_window_holder.release();

	g_animation_manager->abortPageLoadAnimation();
	g_animation_manager->startAnimation(dim_window, ANIM_CURVE_LINEAR);

	OpRect dimrect(0,0, rect.width, rect.height);
	dim_window->GetOpWindow()->SetDesktopPlacement(dimrect, OpWindow::MAXIMIZED);
	dim_window->Show(TRUE, &dimrect);
#endif // VEGA_OPPAINTER_SUPPORT
	return OpStatus::OK;
}

/***********************************************************************************
**
**	Undim
**
***********************************************************************************/

void OpBrowserView::Undim(DimmerAnimation*& dim_window)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	OP_ASSERT(dim_window);

	if (!GetParentDesktopWindow())
	{
		if (!dim_window->IsClosing())
			dim_window->Close(TRUE);
	}
	// Do nothing if undimming while the parent window is closing.
	else if (!GetParentDesktopWindow()->IsClosing() && !dim_window->IsClosing())
	{
		g_animation_manager->abortAnimation(dim_window);
		dim_window->PrepareFadeOut();
		dim_window->animateIgnoreMouseInput(TRUE, TRUE);
		g_animation_manager->startAnimation(dim_window, ANIM_CURVE_LINEAR);
	}

	dim_window = NULL;
#endif // VEGA_OPPAINTER_SUPPORT
}

void OpBrowserView::TryCloseLastPostDialog()
{
	if (m_last_post_dialog)
	{
		m_last_post_dialog->CloseDialog(FALSE);
	}
}

void OpBrowserView::OpenNewOnFormSubmitDialog(OpDocumentListener::FormSubmitCallback * callback)
{
	OnFormSubmitDialog* dialog = OP_NEW(OnFormSubmitDialog, ());
	if (dialog && dialog->Init(GetParentDesktopWindow(), callback, this))
	{
		m_last_post_dialog = dialog;
		dialog->SetDialogListener(this);
	}
}

void OpBrowserView::OpenNewOnPluginPostDialog(OpDocumentListener::PluginPostCallback * callback)
{
	OnPluginPostDialog* dialog = OP_NEW(OnPluginPostDialog, ());
	if (dialog && dialog->Init(GetParentDesktopWindow(), callback))
	{
		m_last_post_dialog = dialog;
		dialog->SetDialogListener(this);
	}
}

/***********************************************************************************
**
**	SetHidden
**
***********************************************************************************/

void OpBrowserView::SetHidden(BOOL hidden)
{
	if (m_hidden != hidden)
	{
#ifndef VEGA_OPPAINTER_SUPPORT // Done automatically by the OpWindow
		m_op_window->OnVisibilityChanged(!hidden);
#endif
		m_hidden = hidden;
	}
}

OP_STATUS OpBrowserView::GetCachedSecurityButtonText(OpString &button_text)
{
	return m_page->GetSecurityInformation(button_text);
}

/***********************************************************************************
**
**	OnMouseWheel
**
***********************************************************************************/

BOOL OpBrowserView::OnMouseWheel(INT32 delta, BOOL vertical)
{
	return TRUE;
}

/***********************************************************************************
**
**	IsActionSupported()
**
***********************************************************************************/

BOOL OpBrowserView::IsActionSupported(OpInputAction* action)
{
	return m_page->IsActionSupported(action);
}

/***********************************************************************************
**
**	OnKeyboardInputGained
**
***********************************************************************************/
void OpBrowserView::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	OpWidget::OnKeyboardInputGained(new_input_context, old_input_context, reason);
}

/***********************************************************************************
**
**	OnKeyboardInputLost
**
***********************************************************************************/
void OpBrowserView::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	OpWidget::OnKeyboardInputLost(new_input_context, old_input_context, reason);
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpBrowserView::OnInputAction(OpInputAction* action)
{
	if(HandleHotclickActions(action))
		return TRUE;

	if (!IsActionSupported(action))
	{
		return FALSE;
	}

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			if (!IsActionSupported(child_action))
				return FALSE;

			OpDocumentListener::ImageDisplayMode image_mode =
				GetWindowCommander()->GetImageMode();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_ENABLE_DISPLAY_IMAGES:
				case OpInputAction::ACTION_DISPLAY_CACHED_IMAGES_ONLY:
				case OpInputAction::ACTION_DISABLE_DISPLAY_IMAGES:
				case OpInputAction::ACTION_SELECT_USER_MODE:
				case OpInputAction::ACTION_SELECT_AUTHOR_MODE:
					{
						CssInformation information;
						child_action->SetAttention(GetWindowCommander()->GetAlternateCssInformation(0, &information));
					}
					break;

				default:
					break;
			}

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_RELOAD:
				case OpInputAction::ACTION_FORCE_RELOAD:
				case OpInputAction::ACTION_GO_TO_SIMILAR_PAGE:
				case OpInputAction::ACTION_SAVE_DOCUMENT_AS:
				case OpInputAction::ACTION_SAVE_DOCUMENT:
				case OpInputAction::ACTION_SHOW_FINDTEXT:
				{
					 child_action->SetEnabled(WindowCommanderProxy::HasCurrentDoc(GetWindowCommander()));
					 return TRUE;
				}

				case OpInputAction::ACTION_SHOW_SECURITY_INFORMATION:
				{
					// Things we don't want to work in embedded dialogs
					child_action->SetEnabled(!m_embedded_in_dialog);
					return TRUE;
				}

				case OpInputAction::ACTION_ENABLE_HANDHELD_MODE:
				case OpInputAction::ACTION_DISABLE_HANDHELD_MODE:
				{
					child_action->SetEnabled(TRUE);
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_HANDHELD_MODE, GetWindowCommander()->GetLayoutMode() == OpWindowCommander::CSSR);
					return TRUE;
				}

				case OpInputAction::ACTION_STOP:
				{
					child_action->SetEnabled(GetWindowCommander()->IsLoading());
					return TRUE;
				}

				case OpInputAction::ACTION_RELOAD_FRAME:
				{
					child_action->SetEnabled(WindowCommanderProxy::HasActiveFrameDocument(GetWindowCommander()));
					return TRUE;
				}
				case OpInputAction::ACTION_VIEW_DOCUMENT_SOURCE:
				case OpInputAction::ACTION_VIEW_FRAME_SOURCE:
				{
					if (m_embedded_in_dialog)
					{
						// No viewing source for embedded dialogs
						child_action->SetEnabled(FALSE);
					}
					else
					{
						// Important to try to enble view source based on the same test used in OpenSourceViewer
						child_action->SetEnabled(WindowCommanderProxy::CanViewSource(GetWindowCommander(), child_action));
					}

					return TRUE;
				}
				case OpInputAction::ACTION_PRINT_PREVIEW:
				case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_AS_SCREEN:
				{
					child_action->SetSelected(WindowCommanderProxy::GetPreviewMode(GetWindowCommander()));
					//enable when a document is loaded
					BOOL enabled = WindowCommanderProxy::HasCurrentDoc(GetWindowCommander());
					child_action->SetEnabled(enabled);
					return TRUE;
				}

				case OpInputAction::ACTION_LOAD_ALL_IMAGES:
				case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_ONE_FRAME_PER_SHEET:
				case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_ACTIVE_FRAME:
				case OpInputAction::ACTION_LEAVE_PRINT_PREVIEW:
				case OpInputAction::ACTION_REFRESH_DISPLAY:
				{
					child_action->SetEnabled(WindowCommanderProxy::HasCurrentDoc(GetWindowCommander()));
					return TRUE;
				}

				case OpInputAction::ACTION_SHOW_ZOOM_POPUP_MENU:
				case OpInputAction::ACTION_ZOOM_IN:
				case OpInputAction::ACTION_ZOOM_OUT:
				case OpInputAction::ACTION_ZOOM_TO:
				{
					BOOL enabled = FALSE;

					ServerName* server_name = WindowCommanderProxy::GetServerName(GetWindowCommander());

					if (server_name)
					{
						int site_scale = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale, server_name->UniName());
						int global_scale = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale);
						enabled = site_scale == global_scale;
					}
					else
					{
						URL url = WindowCommanderProxy::GetCurrentURL(GetWindowCommander());
						URLType type = (URLType) url.GetAttribute(URL::KType);
						enabled = (type == URL_OPERA);
					}
					switch (GetParentDesktopWindowType())
					{
#ifdef M2_SUPPORT
						case WINDOW_TYPE_MAIL_VIEW :
						case WINDOW_TYPE_MAIL_COMPOSE :
						case WINDOW_TYPE_CHAT :
						case WINDOW_TYPE_CHAT_ROOM :
							enabled = TRUE;
#endif // M2_SUPPORT
						default :
							break;
					}

					//to make zoom_to work with the next operator
					if (child_action->GetAction() == OpInputAction::ACTION_ZOOM_TO)
						child_action->SetSelected(static_cast<UINT32>(child_action->GetActionData()) == GetWindowCommander()->GetScale());

					child_action->SetEnabled(enabled);
					return TRUE;
				}
				case OpInputAction::ACTION_ENABLE_DISPLAY_IMAGES:
				{
					child_action->SetSelected(image_mode == OpDocumentListener::ALL_IMAGES);
					return TRUE;
				}
				case OpInputAction::ACTION_DISPLAY_CACHED_IMAGES_ONLY:
				{
					child_action->SetSelected(image_mode == OpDocumentListener::LOADED_IMAGES);
					return TRUE;
				}
				case OpInputAction::ACTION_DISABLE_DISPLAY_IMAGES:
				{
					child_action->SetSelected(image_mode == OpDocumentListener::NO_IMAGES);
					return TRUE;
				}
				case OpInputAction::ACTION_SELECT_USER_MODE:
				{
					child_action->SetSelected(WindowCommanderProxy::HasCSSModeNONE(GetWindowCommander()));
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
				case OpInputAction::ACTION_SELECT_AUTHOR_MODE:
				{
					child_action->SetSelected(WindowCommanderProxy::HasCSSModeFULL(GetWindowCommander()));
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
				case OpInputAction::ACTION_SET_ENCODING:
				{
					OpString8 charset8;
					if( child_action->GetActionDataString())
					{
						charset8.Set(child_action->GetActionDataString());
					}

					if (WindowCommanderProxy::GetForceEncoding(GetWindowCommander()))
					{
						if (charset8.CompareI(WindowCommanderProxy::GetForceEncoding(GetWindowCommander())) == 0)
						{
							child_action->SetSelected(TRUE);
						}
					}

					// utf-7 is only allowed for mail, see bug #313069
					if (WindowCommanderProxy::GetType(GetWindowCommander()) != WIN_TYPE_MAIL_VIEW &&
						!charset8.CompareI("utf-7"))
					{
						child_action->SetEnabled(FALSE);
					}

#if 0
					char sbencoding[128]; /* ARRAY OK 2006-05-22 peter - adamm copied from peter */
					*sbencoding = '\0';

					ServerName* server_name = WindowCommanderProxy::GetServerName(GetWindowCommander());

					if (server_name)
					{
						OpString site_name;
						site_name.Set(server_name->UniName());

						const OpStringC force_encoding(g_pcdisplay->GetStringPref(PrefsCollectionDisplay::ForceEncoding, site_name));
						if (!force_encoding.IsEmpty())
						{
							uni_cstrlcpy(sbencoding, force_encoding, ARRAY_SIZE(sbencoding));
						}
					}

					if (sbencoding)
					{
						if (charset8.CompareI(sbencoding) == 0)
						{
							child_action->SetSelected(TRUE);
						}
					}
#endif
					return TRUE;
				}
				case OpInputAction::ACTION_ADD_LINK_TO_BOOKMARKS:
				{
					BOOL enable = FALSE;
					if (g_desktop_bookmark_manager->GetBookmarkModel()->Loaded())
					{
						OpDocumentContext* ctx = 0;
						OpAutoPtr<OpDocumentContext> local_ctx;
						if (action->GetActionData())
						{
							DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
							ctx = menu_context->GetDocumentContext();
						}
						if (!ctx)
						{
							// Allow shortcut activated action
							GetWindowCommander()->CreateDocumentContext(&ctx);
							local_ctx = ctx;
						}
						enable = ctx && ctx->HasLink() && !ctx->HasMailtoLink();
					}
					child_action->SetEnabled(enable);
					return TRUE;
				}
				case OpInputAction::ACTION_ADD_LINK_TO_CONTACTS:
				{
					OpDocumentContext* ctx = 0;
					OpAutoPtr<OpDocumentContext> local_ctx;
					if (action->GetActionData())
					{
						DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
						ctx = menu_context->GetDocumentContext();
					}
					if (!ctx)
					{
						// Allow shortcut activated action
						GetWindowCommander()->CreateDocumentContext(&ctx);
						local_ctx = ctx;
					}
					
					child_action->SetEnabled(ctx && ctx->HasMailtoLink());
					return TRUE;
				}
				case OpInputAction::ACTION_COPY_DOCUMENT_ADDRESS:
				{

					child_action->SetEnabled( WindowCommanderProxy::HasCurrentDoc(GetWindowCommander()) );
					return TRUE;
				}
				case OpInputAction::ACTION_SEND_DOCUMENT_ADDRESS_IN_MAIL:
				{
					// This is "Send link by mail" in menus but it does what the action
					// name indicates (Send document address).
					child_action->SetEnabled( WindowCommanderProxy::HasCurrentDoc(GetWindowCommander()) );
					return TRUE;
				}

				case OpInputAction::ACTION_COPY_LINK_MAIL_ADDRESS:
				{
					OpDocumentContext* ctx = 0;
					OpAutoPtr<OpDocumentContext> local_ctx;
					if (action->GetActionData())
					{
						DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
						ctx = menu_context->GetDocumentContext();
					}
					if (!ctx)
					{
						// Allow shortcut activated action
						GetWindowCommander()->CreateDocumentContext(&ctx);
						local_ctx = ctx;
					}

					BOOL enable = FALSE;
					OpString8 mailto_address;
					if (ctx && OpStatus::IsSuccess(ctx->GetLinkData(OpDocumentContext::AddressForUIEscaped, &mailto_address)))
					{
						MailTo mailto;
						mailto.Init(mailto_address);
						enable = mailto.GetTo().HasContent();
					}
					child_action->SetEnabled(enable);

					return TRUE;
				}
				break;

				case OpInputAction::ACTION_EDIT_SITE_PREFERENCES:
				{
					child_action->SetEnabled(WindowCommanderProxy::CanEditSitePrefs(GetWindowCommander()));
					return TRUE;
				}

				case OpInputAction::ACTION_COPY:
				case OpInputAction::ACTION_COPY_TO_NOTE:
					// This enables/disables the context menu item in BrowserViews i.e. Fraud or skin/apperance dialog
					// Enable if some text is selected.
					child_action->SetEnabled(GetWindowCommander() && GetWindowCommander()->HasSelectedText());
					return TRUE;

#ifdef GRAB_AND_SCROLL
				case OpInputAction::ACTION_ENABLE_TEXT_SELECTION:
				{
#ifdef DOCHAND_CAP_SCROLL_IS_PAN_OVERRIDDEN
					child_action->SetEnabled(!WindowCommanderProxy::GetScrollIsPan(GetWindowCommander()));
#endif // DOCHAND_CAP_SCROLL_IS_PAN_OVERRIDDEN
				}
				return TRUE;

				case OpInputAction::ACTION_DISABLE_TEXT_SELECTION:
				{
#ifdef DOCHAND_CAP_SCROLL_IS_PAN_OVERRIDDEN
					child_action->SetEnabled(WindowCommanderProxy::GetScrollIsPan(GetWindowCommander()));
#endif // DOCHAND_CAP_SCROLL_IS_PAN_OVERRIDDEN
				}
				return TRUE;
#endif // GRAB_AND_SCROLL

#ifdef TRUST_RATING
				case OpInputAction::ACTION_TRUST_INFORMATION:
				case OpInputAction::ACTION_TRUST_UNKNOWN:
				case OpInputAction::ACTION_TRUST_FRAUD:
				{
					OpString trust_tooltip;
					if ( OpStatus::IsSuccess(g_languageManager->GetString(Str::S_TRUST_BUTTON_TOOLTIP, trust_tooltip)))
						child_action->GetActionInfo().SetStatusText(trust_tooltip.CStr());
				}
#endif // !TRUST_RATING
			}
			break; // let visual device continue
		}

#if defined (GENERIC_PRINTING)
		case OpInputAction::ACTION_PRINT_DOCUMENT:
			OnStartPrinting(GetWindowCommander());
			return TRUE;
#endif

		case OpInputAction::ACTION_GET_TYPED_OBJECT:
		{
			if (action->GetActionData() == GetType())
			{
				action->SetActionObject(this);
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_ENABLE_HANDHELD_MODE:
		case OpInputAction::ACTION_DISABLE_HANDHELD_MODE:
		{
			BOOL is_enabled = GetWindowCommander()->GetLayoutMode() == OpWindowCommander::CSSR;
			BOOL enable	= action->GetAction() == OpInputAction::ACTION_ENABLE_HANDHELD_MODE;

			if (is_enabled == enable)
			{
				action->SetWasReallyHandled(FALSE);
			}
			else
			{
				GetWindowCommander()->SetLayoutMode(enable ? OpWindowCommander::CSSR : OpWindowCommander::NORMAL);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_STOP:
		{
			if (GetWindowCommander()->IsLoading())
			{
				m_page->SetStoppedByUser(TRUE);
				GetWindowCommander()->Stop();
				//Flag is to FALSE in the method OpPage::OnLoadingFinished
				//m_page->SetStoppedByUser(FALSE);
				return TRUE;
			}
			else
			{
				action->SetWasReallyHandled(FALSE);
			}

			return TRUE;
		}

		case OpInputAction::ACTION_RELOAD:
		case OpInputAction::ACTION_FORCE_RELOAD:
		{
			// We want ctrl+reload button to trigger "force reload" but unfortunately the OpButton
			// can't have different actions depending on click or ctrl/shift+click so we have this
			// hack here to tweak the action.
			bool force = action->GetAction() == OpInputAction::ACTION_FORCE_RELOAD ||
			             (g_op_system_info->GetShiftKeyState() & (SHIFTKEY_SHIFT | SHIFTKEY_CTRL)) != 0;
			GetWindowCommander()->Reload(force);
			return TRUE;
		}

		case OpInputAction::ACTION_RELOAD_FRAME:
		{
			WindowCommanderProxy::ReloadFrame(GetWindowCommander());
			return TRUE;
		}

		case OpInputAction::ACTION_GO_TO_PARENT_DIRECTORY:
		{
			WindowCommanderProxy::GoToParentDirectory(GetWindowCommander());
			return TRUE;
		}

		case OpInputAction::ACTION_LOAD_ALL_IMAGES:
		{
			WindowCommanderProxy::LoadAllImages(GetWindowCommander());
			return TRUE;
		}

		case OpInputAction::ACTION_ENABLE_DISPLAY_IMAGES:
		{
			WindowCommanderProxy::EnableImages(GetWindowCommander(),
											   OpDocumentListener::ALL_IMAGES);
			return TRUE;
		}

		case OpInputAction::ACTION_DISPLAY_CACHED_IMAGES_ONLY:
		{
			WindowCommanderProxy::EnableImages(GetWindowCommander(),
											   OpDocumentListener::LOADED_IMAGES);
			return TRUE;
		}

		case OpInputAction::ACTION_DISABLE_DISPLAY_IMAGES:
		{
			WindowCommanderProxy::EnableImages(GetWindowCommander(),
											   OpDocumentListener::NO_IMAGES);
			return TRUE;
		}

		case OpInputAction::ACTION_REFRESH_DISPLAY:
#ifdef _PRINT_SUPPORT_
			if(WindowCommanderProxy::GetPreviewMode(GetWindowCommander()))
			{
				WindowCommanderProxy::TogglePrintMode(GetWindowCommander());
			}
#endif // _PRINT_SUPPORT_
			// Check for action data, this holds the ID of the window to refresh (if used)
			// This is handled by the application class so just return FALSE
			if (action->GetActionData() != 0)
				return FALSE;
			else
				WindowCommanderProxy::UpdateCurrentDoc(GetWindowCommander());
			return TRUE;

		case OpInputAction::ACTION_SELECT_USER_MODE:
		{
			if (WindowCommanderProxy::HasCSSModeFULL(GetWindowCommander()))
			{
				WindowCommanderProxy::SetNextCSSMode(GetWindowCommander());
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_SELECT_AUTHOR_MODE:
		{
			if (WindowCommanderProxy::HasCSSModeNONE(GetWindowCommander()))
			{
				WindowCommanderProxy::SetNextCSSMode(GetWindowCommander());
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_SET_ENCODING:
		{
			if(WindowCommanderProxy::IsMailOrNewsfeedWindow(GetWindowCommander()))
				break; // the MailDesktopWindow will take care of this

			return OpStatus::IsSuccess(WindowCommanderProxy::SetEncoding(GetWindowCommander(), action->GetActionDataString()));
		}
		break;
		case OpInputAction::ACTION_OPEN_LINK:
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
		{
			BOOL bookmarklinkaction = (	action->GetActionData() &&
										action->GetActionDataString() &&
										uni_strnicmp(action->GetActionDataString(), UNI_L("urlinfo"), 7) != 0);
			// This needs to be revisited if there is going to be still another data type as this is a fallback
			// url's will be picked up in the action handling in display (visdevactions).
			bookmarklinkaction |= (	action->GetActionData() && /* This is for backwards compatiblility */
									!action->GetActionDataString());
			if (!bookmarklinkaction)
			{
				DesktopMenuContext * menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (!menu_context)
				{
					OpDocumentContext* context;
					OpRect position;
					action->GetActionPosition(position);
					if (OpStatus::IsSuccess(GetWindowCommander()->CreateDocumentContextForScreenPos(&context, OpPoint(position.Left(), position.Top()))))
					{
						OpAutoPtr<OpDocumentContext> context_ptr(context);

						if (context && context->HasLink())
						{
							menu_context = OP_NEW(DesktopMenuContext, ());
							if (menu_context)
							{
								menu_context->SetContext(context_ptr.release());
							}
						}
					}
					action->SetActionData(reinterpret_cast<INTPTR>(menu_context));
					action->SetActionDataString(UNI_L("urlinfo"));
				}
				if (menu_context)
					menu_context->SetDesktopWindow(GetParentDesktopWindow());
			}
		}
		break;
		case OpInputAction::ACTION_SAVE_LINK:
		{
			OpDocumentContext* ctx = 0;
			OpAutoPtr<OpDocumentContext> local_ctx;
			if (action->GetActionData())
			{
				DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				ctx = menu_context->GetDocumentContext();
			}
			if (!ctx)
			{
				// Allow shortcut activated action
				GetWindowCommander()->CreateDocumentContext(&ctx);
				local_ctx = ctx;
			}
			if (ctx && ctx->HasLink())
			{
				URLInformation* url_info;
				if (OpStatus::IsSuccess(ctx->CreateLinkURLInformation(&url_info)))
				{
					OP_STATUS status = url_info->DownloadTo(NULL, NULL); // TODO: use second parameter to define download path
					if (OpStatus::IsError(status))
						OP_DELETE(url_info);
					else
					{
						// The URLInformation object owns itself and will take care of everything I think.
					}
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_COPY_DOCUMENT_ADDRESS:
			if( WindowCommanderProxy::HasCurrentDoc(GetWindowCommander()) )
			{
				URL url = WindowCommanderProxy::GetCurrentDocURL(m_page->GetWindowCommander());
				OpString url_string;
				url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, url_string);

				// Clipboard
				g_desktop_clipboard_manager->PlaceText(url_string.CStr(), GetClipboardToken());
#if defined(_X11_SELECTION_POLICY_)
				g_desktop_clipboard_manager->PlaceText(url_string.CStr(), GetClipboardToken(), true);
#endif
			}
			return TRUE;
		break;

		case OpInputAction::ACTION_EDIT_SITE_PREFERENCES:
		{
			ServerName* server_name = WindowCommanderProxy::GetServerName(GetWindowCommander());

			if (server_name)
			{
				OpString site_name;
				site_name.Set(server_name->UniName());

				ServerPropertiesDialog* dialog = OP_NEW(ServerPropertiesDialog, (site_name, FALSE, GetWindowCommander()->GetPrivacyMode(), action->GetActionDataString()));
				if (dialog)
					dialog->Init(g_application->GetActiveDesktopWindow());
			}
			return TRUE;
		}

		case OpInputAction::ACTION_COPY_LINK:
		{
			OpDocumentContext* ctx = 0;
			OpAutoPtr<OpDocumentContext> local_ctx;
			if (action->GetActionData())
			{
				DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				ctx = menu_context->GetDocumentContext();
			}
			if (!ctx)
			{
				// Allow shortcut activated action
				GetWindowCommander()->CreateDocumentContext(&ctx);
				local_ctx = ctx;
			}

			OpString url_string;
			if (ctx && OpStatus::IsSuccess(ctx->GetLinkData(OpDocumentContext::AddressNotForUI, &url_string)))
			{
				g_desktop_clipboard_manager->PlaceText(url_string.CStr(), GetClipboardToken());
#if defined(_X11_SELECTION_POLICY_)
				g_desktop_clipboard_manager->PlaceText(url_string.CStr(), GetClipboardToken(), true);
#endif
			}
			return TRUE;
		}
		break;

		case OpInputAction::ACTION_COPY_LINK_MAIL_ADDRESS:
		{
			OpDocumentContext* ctx = 0;
			OpAutoPtr<OpDocumentContext> local_ctx;
			if (action->GetActionData())
			{
				DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				ctx = menu_context->GetDocumentContext();
			}
			if (!ctx)
			{
				// Allow shortcut activated action
				GetWindowCommander()->CreateDocumentContext(&ctx);
				local_ctx = ctx;
			}

			OpString8 mailto_address;
			if (ctx && OpStatus::IsSuccess(ctx->GetLinkData(OpDocumentContext::AddressForUIEscaped, &mailto_address)))
			{
				MailTo mailto;
				mailto.Init(mailto_address);

				// Careful with NULL pointer. Bug #333053
				const uni_char* text =  mailto.GetTo().HasContent() ?  mailto.GetTo().CStr() : UNI_L("");

				g_desktop_clipboard_manager->PlaceText(text, GetClipboardToken());
#if defined(_X11_SELECTION_POLICY_)
				g_desktop_clipboard_manager->PlaceText(text, GetClipboardToken(), true);
#endif
			}
			return TRUE;
		}	
		break;

		case OpInputAction::ACTION_ADD_LINK_TO_BOOKMARKS:
		{
			OpDocumentContext* ctx = 0;
			OpAutoPtr<OpDocumentContext> local_ctx;
			if (action->GetActionData())
			{
				DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				ctx = menu_context->GetDocumentContext();
			}
			if (!ctx)
			{
				// Allow shortcut activated action
				GetWindowCommander()->CreateDocumentContext(&ctx);
				local_ctx = ctx;
			}

			if (ctx)
			{
				BookmarkItemData item_data;
				INT32 id = -1;

				OpString title;
				ctx->GetLinkTitle(&title);
				item_data.name.Set(title.IsEmpty() ? GetWindowCommander()->GetCurrentTitle() : title.CStr());
				if (item_data.name.IsEmpty())
					ctx->GetLinkData(OpDocumentContext::AddressForUI, &item_data.name);
				else
					ReplaceEscapes(item_data.name.CStr(), item_data.name.Length(), TRUE);

				ctx->GetLinkData(OpDocumentContext::AddressNotForUIEscaped, &item_data.url);

				if (g_application->GetActiveBrowserDesktopWindow())
					id = g_application->GetActiveBrowserDesktopWindow()->GetSelectedHotlistItemId(OpTypedObject::PANEL_TYPE_BOOKMARKS);

				g_desktop_bookmark_manager->NewBookmark(item_data, id, 0, TRUE);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_ADD_LINK_TO_CONTACTS:
		{
			OpDocumentContext* ctx = 0;
			OpAutoPtr<OpDocumentContext> local_ctx;
			if (action->GetActionData())
			{
				DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				ctx = menu_context->GetDocumentContext();
			}
			if (!ctx)
			{
				// Allow shortcut activated action
				GetWindowCommander()->CreateDocumentContext(&ctx);
				local_ctx = ctx;
			}
			
			if (ctx && ctx->HasMailtoLink())
			{
				HotlistManager::ItemData item_data;
				
				if (!action->IsKeyboardInvoked())
					ctx->GetLinkTitle(&item_data.name);

				OpString8 mailto_address;
				ctx->GetLinkData(OpDocumentContext::AddressForUIEscaped, &mailto_address);

				MailTo mailto;
				mailto.Init(mailto_address);
				item_data.mail.Set(mailto.GetTo());
				
				if (item_data.name.IsEmpty())
					item_data.name.Set(item_data.mail);

				g_hotlist_manager->SetupPictureUrl(item_data);

				INT32 id = -1;
				if (g_application->GetActiveBrowserDesktopWindow())
				{
					id = g_application->GetActiveBrowserDesktopWindow()->GetSelectedHotlistItemId(PANEL_TYPE_CONTACTS);
				}

				g_hotlist_manager->NewContact( item_data, id, 0, TRUE );
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
		{
			OpDocumentContext* context;
			if (OpStatus::IsSuccess(GetWindowCommander()->CreateDocumentContext(&context)))
			{
				DesktopMenuContext * menu_context = OP_NEW(DesktopMenuContext, ());
				if (menu_context)
				{
					menu_context->SetContext(context);
					OpPoint point(-10000 , -10000); // Keyboard activated
					const uni_char* menu_name  = NULL;
					OpRect rect;
					ResolveContextMenu(point, menu_context, menu_name, rect, WindowCommanderProxy::GetType(GetWindowCommander()));
					InvokeContextMenuAction(point, menu_context, menu_name, rect);
				}
				else
					OP_DELETE(context);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_VIEW_DOCUMENT_SOURCE:
		case OpInputAction::ACTION_VIEW_FRAME_SOURCE:
		{
			// If this is in a dialog just eat up the action and do nothing
			if (m_embedded_in_dialog)
				return TRUE;

			BOOL frame_source = (action->GetAction() == OpInputAction::ACTION_VIEW_FRAME_SOURCE);
			return WindowCommanderProxy::ViewSource(GetWindowCommander(), frame_source);
		}

#ifdef M2_SUPPORT
		case OpInputAction::ACTION_SEND_TEXT_IN_MAIL:
		case OpInputAction::ACTION_SEND_DOCUMENT_ADDRESS_IN_MAIL:
		case OpInputAction::ACTION_SEND_FRAME_ADDRESS_IN_MAIL:
		{
			const uni_char* name = GetWindowCommander()->GetCurrentURL(FALSE);
			const uni_char* title = GetWindowCommander()->GetCurrentTitle();

			OpStringC subject(title ? title : name);
			OpString message;
			if (action->GetAction() == OpInputAction::ACTION_SEND_TEXT_IN_MAIL)
			{
				RETURN_VALUE_IF_ERROR(message.Set(GetWindowCommander()->GetSelectedText()), FALSE);
				RETURN_VALUE_IF_ERROR(message.Append("\r\n\r\n"), FALSE);
			}
	
			RETURN_VALUE_IF_ERROR(message.AppendFormat("<%s>\r\n\r\n", name), FALSE);

			MailTo mailto;
			RETURN_VALUE_IF_ERROR(mailto.Init(NULL, NULL, NULL, subject, message), FALSE);

			MailCompose::ComposeMail(mailto);
			return TRUE;

		}
#endif // M2_SUPPORT
#ifdef _SECURE_INFO_SUPPORT
		case OpInputAction::ACTION_SHOW_SECURITY_INFORMATION:
			// If this is in a dialog just eat up the action and do nothing
			if (m_embedded_in_dialog)
				return TRUE;

			// Flow through
		case OpInputAction::ACTION_EXTENDED_SECURITY:
		case OpInputAction::ACTION_HIGH_SECURITY:
		case OpInputAction::ACTION_TRUST_INFORMATION:
		case OpInputAction::ACTION_TRUST_UNKNOWN:
		case OpInputAction::ACTION_TRUST_FRAUD:
		case OpInputAction::ACTION_NO_SECURITY:
		{
			if(WindowCommanderProxy::ShouldDisplaySecurityDialog(GetWindowCommander()))
			{
				SecurityInformationDialog* secInfoDialog = OP_NEW(SecurityInformationDialog, ());
				if (secInfoDialog)
				{
					secInfoDialog->Init(g_application->GetActiveBrowserDesktopWindow(), m_page->GetWindowCommander(), NULL);
				}
				return TRUE;
			}
			break;
		}
#endif //_SECURE_INFO_SUPPORT

#ifdef GRAB_AND_SCROLL
		case OpInputAction::ACTION_ENABLE_TEXT_SELECTION:
		case OpInputAction::ACTION_DISABLE_TEXT_SELECTION:
		case OpInputAction::ACTION_OVERRIDE_SCROLL_IS_PAN:
		{
			WindowCommanderProxy::OverrideScrollIsPan(GetWindowCommander());
		}
		return TRUE;
#endif // GRAB_AND_SCROLL

		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
			if (action->GetChildAction() &&
				action->GetChildAction()->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_UP)
			{
				m_page->SelectionChanged();
			}
			break;

		// Super hack to make the context menu not be passed onto a listener and loose the action and
		// loose the menu it is supposed to show.
		// There is a fix in a patch for frm_doc that will remove the need for this hack TODO rfz - remove this
		case OpInputAction::ACTION_SHOW_POPUP_MENU:
			if (action->GetAction() && action->GetActionDataString() && uni_strlen(action->GetActionDataString()))
				return FALSE;
			break;

#if defined SUPPORT_DATABASE_INTERNAL
		case OpInputAction::ACTION_ACCEPT_WEBSTORAGE_QUOTA_REQUEST:
		{
			if(m_quota_callback)
			{
				OpFileLength new_quota = (OpFileLength)action->GetActionData() * 1024;

				// new_quota = 0 means unlimited
				m_quota_callback->OnQuotaReply(TRUE, new_quota);

				m_quota_callback = NULL;
			}
			return TRUE;
		}
		break;

		case OpInputAction::ACTION_REJECT_WEBSTORAGE_QUOTA_REQUEST:
		{
			if(m_quota_callback)
			{
				m_quota_callback->OnQuotaReply(FALSE, 0);
				m_quota_callback = NULL;
			}
			return TRUE;
		}
		break;

		case OpInputAction::ACTION_CANCEL_WEBSTORAGE_QUOTA_REQUEST:
		{
			if(m_quota_callback)
			{
				m_quota_callback->OnCancel();
				m_quota_callback = NULL;
			}
			return TRUE;
		}
		break;
#endif // SUPPORT_DATABASE_INTERNAL
		case OpInputAction::ACTION_SCROLL_DOWN:
		case OpInputAction::ACTION_SCROLL_UP:
			if (m_consume_scrolls)
				return TRUE;
			break;
	}

	return action->IsLowlevelAction() ? FALSE : WindowCommanderProxy::OnInputAction(GetWindowCommander(), action);
}


/***********************************************************************************
 *
 * HandleHotclickActions
 *
 * For Search With, ActionString stores the searches' GUID
 * For translate, Search, dictionary, etc. ActionData stores the search type
 *
 * Selected item in 'Search with' is initially default search
 * It should only change if the user chooses 'Search With' another search item
 * not if choosing 'Search', 'Dictionary' or 'Encyc'
 *
 ***********************************************************************************/
// Note: ActionData or String stores the searches' ID or GUID

BOOL OpBrowserView::HandleHotclickActions(OpInputAction* action)
{
	if(action->GetAction() == OpInputAction::ACTION_GET_ACTION_STATE)
	{
		OpInputAction* child_action = action->GetChildAction();

		switch (child_action->GetAction())
		{
			case OpInputAction::ACTION_HOTCLICK_SEARCH:
			{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
				OpString search_guid;
				search_guid.Set(child_action->GetActionDataString());
				// This only works correct if m_search_guid is not changed to
				// anything else than the 'Search With' searches
				child_action->SetSelected(search_guid.Compare(m_search_guid)==0);

				// Hotclick search - searches using currently selected engine.
				if (child_action->GetActionData() == SEARCH_TYPE_DEFAULT)
				{
					/* Set view's current engine guid as data string on action.
					   Will get special handling at QuickMenu::AddMenuItem
					   where proper search icon will be set based on it. */
					child_action->SetActionDataString(m_search_guid);
				}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
				return TRUE;
			}
		}
		return FALSE;
	}

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_HOTCLICK_SEARCH:
		{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES

			OpString search_guid;
			search_guid.Set(action->GetActionDataString());

			SearchTemplate* search = 0;

			if (!search_guid.HasContent())  // Get the search type
			{
				int search_type = action->GetActionData();

				if (search_type != SEARCH_TYPE_DEFAULT)
				{
					int index = g_searchEngineManager->SearchTypeToIndex((SearchType)search_type);
					search = g_searchEngineManager->GetSearchEngine(index);
				}
				else // Get the saved search for this browser view
				{
					if (GetSearchGUID().HasContent())
						search = g_searchEngineManager->GetByUniqueGUID(GetSearchGUID());
					else
						search = g_searchEngineManager->GetDefaultSearch();
				}
			}
			else
			{
				search = g_searchEngineManager->GetByUniqueGUID(search_guid);
			}

			if(search)
			{

				SearchType search_type = search->GetSearchType();

				switch (search_type)
				{
					case SEARCH_TYPE_DEFAULT:
					{
						SelectedTextSearch(search->GetUniqueGUID());
						break;
					}
					case SEARCH_TYPE_MONEY:
					{
						break;
					}
					default:
					{
						SelectedTextSearch(search->GetUniqueGUID());

						// If not the special stuff (dictionary etc), set the m_search_guid
						if (search_type < SEARCH_TYPE_DICTIONARY)
						{
							SetSearchGUID(search->GetUniqueGUID());
						}
						break;
					}
				}
			}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
			return TRUE;
		}
	}

	return FALSE;
}

void OpBrowserView::OnClose(Dialog* dialog)
{
	if(m_quota_callback)
	{
		m_quota_callback->OnCancel();
		m_quota_callback = NULL;
	}
	if (dialog == m_last_post_dialog)
		m_last_post_dialog = NULL;
}

OP_STATUS OpBrowserView::Create(OpWidget* container)
{
	container->AddChild(this);
	GetBorderSkin()->SetImage("Document Browser Skin", "Browser Skin");
	SetSupportsNavigation(TRUE);
	SetSupportsSmallScreen(TRUE);
	if (GetWindowCommander())
		GetWindowCommander()->SetScale(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale));

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityPrunesMe(ACCESSIBILITY_PRUNE_WHEN_INVISIBLE);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	Show(false);

	return OpStatus::OK;
}

OP_STATUS OpBrowserView::GetTooltipText(OpInfoText& text)
{
	URL url = WindowCommanderProxy::GetMovedToURL(GetWindowCommander());
	if (url.IsEmpty())
		return OpStatus::ERR;

	const uni_char* url_text = GetWindowCommander()->GetCurrentURL(FALSE);
	text.SetStatusText(url_text);

	OpString loc_str;
	OpString mime_str;

	g_languageManager->GetString(Str::SI_IDSTR_HL_TREE_TITLE, loc_str);
	text.AddTooltipText(loc_str.CStr(),GetWindowCommander()->GetCurrentTitle());
	g_languageManager->GetString(Str::SI_LOCATION_TEXT, loc_str);
	text.AddTooltipText(loc_str.CStr(), url_text);
	g_languageManager->GetString(Str::S_ENCODING, loc_str);
	mime_str.SetFromUTF8(url.GetAttribute(URL::KMIME_CharSet).CStr());
	text.AddTooltipText(loc_str.CStr(), mime_str.CStr());
	g_languageManager->GetString(Str::DI_IDM_TYPE, loc_str);
	mime_str.SetFromUTF8(url.GetAttribute(URL::KMIME_Type).CStr());
	text.AddTooltipText(loc_str.CStr(), mime_str.CStr());
	g_languageManager->GetString(Str::M_SECURITY, loc_str);
	return text.AddTooltipText(loc_str.CStr(), WindowCommanderProxy::GetSecurityStateText(GetWindowCommander()));
}

const OpWidgetImage* OpBrowserView::GetFavIcon(BOOL force)
{
	UpdateWindowImage(force);
	return m_page->GetDocumentIcon();
}

double OpBrowserView::GetWindowScale()
{
	return GetWindowCommander() ? GetWindowCommander()->GetScale() * 0.01 : 1.0 ;
}

/***********************************************************************************
**
**	SelectedTextSearch
**
***********************************************************************************/
void OpBrowserView::SelectedTextSearch(const OpStringC& guid)
{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	SearchEngineManager::SearchSetting settings;
	settings.m_keyword.Set(GetWindowCommander()->GetSelectedText());

	if(settings.m_keyword.HasContent())
	{
		for (uni_char* buffer = settings.m_keyword.CStr(); *buffer; buffer++)
		{
			if(*buffer > 0 && *buffer < ' ')
			{
				*buffer = ' ';
			}
		}
	}
	
	settings.m_target_window = GetParentDesktopWindow();
	settings.m_search_template = g_searchEngineManager->GetByUniqueGUID(guid);
	settings.m_search_issuer = SearchEngineManager::SEARCH_REQ_ISSUER_OTHERS;
	settings.m_is_privacy_mode = GetParentDesktopWindow() && GetParentDesktopWindow()->PrivacyMode();
	g_searchEngineManager->DoSearch(settings);
# ifdef ENABLE_USAGE_REPORT
	if (settings.m_search_template && g_usage_report_manager && g_usage_report_manager->GetSearchReport())
	{
		g_usage_report_manager->GetSearchReport()->AddSearch(SearchReport::SearchSelectedText, guid);
	}
# endif // ENABLE_USAGE_REPORT
#endif // DESKTOP_UTIL_SEARCH_ENGINES 
}

/***********************************************************************************
**
**	GetToolTipText
**
***********************************************************************************/

void OpBrowserView::GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
{
	if (GetParent() == NULL)
		return;

	BOOL show_statusbar = g_pcui->GetIntegerPref(PrefsCollectionUI::StatusbarAlignment) != OpBar::ALIGNMENT_OFF;

	OpString title, address;

	if (!show_statusbar)
	{
		g_languageManager->GetString(Str::SI_IDSTR_HL_TREE_TITLE, title);
		g_languageManager->GetString(Str::SI_LOCATION_TEXT, address);
	}

	text.AddTooltipText(title.CStr(), m_hover_title.CStr());
	if (!show_statusbar)
		text.AddTooltipText(address.CStr(), m_hover_url.CStr());
}

void OpBrowserView::GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url)
{
	if (GetParent() == NULL)
		return;

	title.Set(m_hover_title.CStr());
	url_string.Set(m_hover_url.CStr());

	url = WindowCommanderProxy::GetCurrentURL(GetWindowCommander());
}

/***********************************************************************************
**
**	Loading listeners
**
***********************************************************************************/

void OpBrowserView::OnPageUrlChanged(OpWindowCommander* commander, const uni_char* url)
{
	if(m_hover_url.Compare(url))
	{
		m_hover_title.Empty();
		// Force update to prevent bug #200432. If the mouse is over the tab that
		// belongs to this page it will call DocumentDesktopWindow::GetToolTipText()
		// to update the contents. Must not set m_hover_url here. That caused bug #233820
		// [espen 2006_10_18]

		g_application->UpdateToolTipInfo();
	}

}

void OpBrowserView::OnPageAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback)
{
	if (m_own_auth)
	{
		PasswordDialog *dialog = OP_NEW(PasswordDialog, (callback, commander, FALSE));
		if (dialog)
			dialog->Init(GetParentDesktopWindow());
		
	}
	else if(g_application->GetAuthListener())
	{
		BrowserAuthenticationListener* ptr = static_cast<BrowserAuthenticationListener*>(g_application->GetAuthListener());
		ptr->OnAuthenticationRequired(callback, commander, GetParentDesktopWindow());
	}	
}

void OpBrowserView::OnPageStartLoading(OpWindowCommander* commander)
{
	g_input_manager->UpdateAllInputStates();
}

void OpBrowserView::OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user)
{
	OP_PROFILE_CHECKPOINT_END(m_page->GetWindowCommander());

	g_input_manager->UpdateAllInputStates();
}

#if defined SUPPORT_DATABASE_INTERNAL
void OpBrowserView::OnPageIncreaseQuota(OpWindowCommander* commander, const uni_char* db_name, const uni_char* db_domain, const uni_char* db_type, OpFileLength current_quota_size, OpFileLength quota_hint, OpDocumentListener::QuotaCallback *callback)
{
	m_quota_callback = callback;

	WebStorageQuotaDialog *dlg = OP_NEW(WebStorageQuotaDialog, (db_domain, current_quota_size, quota_hint));
	if(dlg)
	{
		dlg->Init(GetParentDesktopWindow(), 0, FALSE, this);
	}
}
#endif // SUPPORT_DATABASE_INTERNAL

/***********************************************************************************
**
**	Javascript dialog listeners
**
***********************************************************************************/

#ifdef _PRINT_SUPPORT_
void OpBrowserView::OnStartPrinting(OpWindowCommander* commander)
{
#if defined (GENERIC_PRINTING)
	if (!commander->IsPrintable())
		return;

	if (commander->IsPrinting())
	{
		SimpleDialog::ShowDialog(WINDOW_NAME_ALREADY_PRINTING, 0, Str::SI_ERR_ALREADY_PRINTING, Str::SI_IDSTR_ERROR, Dialog::TYPE_OK, Dialog::IMAGE_ERROR);
		return;
	}

#if defined QUICK_TOOLKIT_PRINT_PROGRESS_DIALOG
	OP_DELETE(m_print_progress_dialog);
	m_print_progress_dialog = OP_NEW(PrintProgressDialog,(this));
	if (!m_print_progress_dialog)
		return;
#endif

	OP_STATUS rc = commander->Print(1, USHRT_MAX, FALSE); // Opens dialog
	if (OpStatus::IsError(rc))
	{
#if defined QUICK_TOOLKIT_PRINT_PROGRESS_DIALOG
		OP_DELETE(m_print_progress_dialog);
		m_print_progress_dialog = 0;
#endif
		SimpleDialog::ShowDialog(WINDOW_NAME_PRINT_ERROR, 0, Str::SI_ERR_CANNOT_PRINT, Str::SI_IDSTR_ERROR, Dialog::TYPE_OK, Dialog::IMAGE_ERROR);
	}
#endif
}


void OpBrowserView::OnPrintStatus(OpWindowCommander* commander, const OpPrintingListener::PrintInfo* info)
{
#if defined QUICK_TOOLKIT_PRINT_PROGRESS_DIALOG
	switch (info->status)
	{
		case OpPrintingListener::PrintInfo::PRINTING_STARTED:
			if (m_print_progress_dialog)
				m_print_progress_dialog->Init(GetParentDesktopWindow());
		break;

		case OpPrintingListener::PrintInfo::PAGE_PRINTED:
			if (m_print_progress_dialog)
			{
				m_print_progress_dialog->SetPage(info->currentPage);
				commander->PrintNextPage();
			}
			else
				commander->CancelPrint();
		break;

		case OpPrintingListener::PrintInfo::PRINTING_DONE:
		case OpPrintingListener::PrintInfo::PRINTING_ABORTED:	
			if (m_print_progress_dialog)
			{
				m_print_progress_dialog->Close(TRUE);
				m_print_progress_dialog = 0;
			}
		break;
	}
#elif defined(MSWIN)
	switch (info->status)
	{
	case OpPrintingListener::PrintInfo::PAGE_PRINTED:
		ucPrintPagePrinted(info->currentPage);
		break;
	case OpPrintingListener::PrintInfo::PRINTING_DONE:
		ucPrintingFinished(commander->GetWindow());
		break;
	case OpPrintingListener::PrintInfo::PRINTING_ABORTED:
		ucPrintingAborted(commander->GetWindow());
		break;
	default:
		OP_ASSERT(FALSE);
	}
#endif // MSWIN
}
#endif // _PRINT_SUPPORT_

#ifdef WIC_FILESELECTION_LISTENER
BOOL OpBrowserView::OnRequestPermission(OpWindowCommander* commander)
{
# if defined(_KIOSK_MANAGER_)
	if( g_application && KioskManager::GetInstance()->GetEnabled() )
		return FALSE;
	else
# endif // _KIOSK_MANAGER_
		return TRUE;
}
# ifdef _FILE_UPLOAD_SUPPORT_
void OpBrowserView::OnUploadFilesRequest(OpWindowCommander* commander, UploadCallback* callback)
{
	ResetDesktopFileChooserRequest(m_request);

	if (!m_chooser)
		RETURN_VOID_IF_ERROR(DesktopFileChooser::Create(&m_chooser));

	if (OpStatus::IsError(m_request.initial_path.Set(callback->GetInitialPath())))
	{
		return;
	}
	if (m_request.initial_path.IsEmpty())
	{
		if (OpStatus::IsError(g_folder_manager->GetFolderPath(OPFILE_OPEN_FOLDER, m_request.initial_path)))
		{
			return;
		}
	}
	else
	{
		int sep = m_request.initial_path.FindLastOf(PATHSEPCHAR);

		if (KNotFound == sep)
		{
			OpString openfolderpath;
			RETURN_VOID_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_OPEN_FOLDER, openfolderpath));
			if (OpStatus::IsError(m_request.initial_path.Insert(0, openfolderpath)))
			{
				return;
			}
		}
	}

	static_cast<DesktopOpSystemInfo*>(g_op_system_info)->RemoveIllegalPathCharacters(m_request.initial_path, FALSE);

	Str::LocaleString caption_str(Str::NOT_A_STRING);
	Str::LocaleString filter_str(Str::NOT_A_STRING);

	// Add filters:
	for (int ii = 0; callback->GetMediaType(ii); ii++)
	{
		CopyAddMediaType(callback->GetMediaType(ii), &m_request.extension_filters, TRUE);
	}
	for (unsigned int ii = 0; ii < m_request.extension_filters.GetCount(); ii++)
		OpStatus::Ignore(ExtendMediaTypeWithExtensionInfo(*m_request.extension_filters.Get(ii)));

	// Add all files glob
	OpFileSelectionListener::MediaType* all_files_type = OP_NEW(OpFileSelectionListener::MediaType, ());
	if (!all_files_type)
		return;

	RETURN_VOID_IF_ERROR(m_request.extension_filters.Add(all_files_type));
	RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_ALL_FILES_ASTRIX, all_files_type->media_type));
	OpString * all_files_extension = OP_NEW(OpString, ());
	if (!all_files_extension)
		return;
	RETURN_VOID_IF_ERROR(all_files_extension->Set(UNI_L("*")));
	RETURN_VOID_IF_ERROR(all_files_type->file_extensions.Add(all_files_extension));

	if (callback->GetMaxFileCount() > 1)
		m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI;
	else
		m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;

	m_selection_callback = callback;
	m_chooser->Execute(commander->GetOpWindow(), this, m_request);
}
void OpBrowserView::OnUploadFilesCancel(OpWindowCommander* commander)
{

}
# endif // _FILE_UPLOAD_SUPPORT_
# ifdef DOM_GADGET_FILE_API_SUPPORT
void OpBrowserView::OnDomFilesystemFilesRequest(OpWindowCommander* commander, DomFilesystemCallback* callback)
{
	callback->OnFilesSelected(0);
}
void OpBrowserView::OnDomFilesystemFilesCancel(OpWindowCommander* commander)
{

}
# endif // DOM_GADGET_FILE_API_SUPPORT
void OpBrowserView::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (m_selection_callback)
	{
		if (result.files.GetCount())
		{
			m_selection_callback->OnFilesSelected(&result.files);
#  ifndef PREFS_NO_WRITE
			if (result.active_directory.HasContent())
			{
				TRAPD(error, g_pcfiles->WriteDirectoryL(OPFILE_OPEN_FOLDER, result.active_directory.CStr()));
			}
#  endif // PREFS_NO_WRITE
		}
		else
		{
			m_selection_callback->OnFilesSelected(0);
		}
	}
	m_selection_callback = NULL;
}
#endif // WIC_FILESELECTION_LISTENER

#ifdef WIC_COLORSELECTION_LISTENER

void OpBrowserView::OnColorSelectionRequest(OpWindowCommander* commander, ColorSelectionCallback* callback)
{
	OP_ASSERT(!m_color_selection_callback); // If this trig, either core or OpBrowserView is probably doing something wrong.
	m_color_selection_callback = callback;

	// Post a message so the synchronous dialog will run with a stack clean from core.
	if (!g_main_message_handler->PostDelayedMessage(MSG_QUICK_DELAYED_COLOR_SELECTOR, (MH_PARAM_1)this, 0, 0))
	{
		// OOM
		m_color_selection_callback->OnCancel();
		m_color_selection_callback = NULL;
	}
}

void OpBrowserView::OnColorSelectionCancel(OpWindowCommander* commander)
{
	OP_ASSERT(m_color_selection_callback);
	// Remove the message in case it hasn't been recieved yet.
	g_main_message_handler->RemoveDelayedMessage(MSG_QUICK_DELAYED_COLOR_SELECTOR, (MH_PARAM_1)this, 0);
	// If the message was recieved (and the dialog currently running) this will make sure we don't post the result!
	m_color_selection_callback = NULL;
}

#endif // WIC_COLORSELECTION_LISTENER

void OpBrowserView::SaveDocumentIcon()
{
	m_page->SaveDocumentIcon();
}

void OpBrowserView::UpdateWindowImage(BOOL force)
{
	m_page->UpdateWindowImage(force);
}

BOOL OpBrowserView::ContinuePopup(OpDocumentListener::JSPopupCallback::Action action, OpDocumentListener::JSPopupOpener **opener)
{
	return m_page->ContinuePopup(action, opener);
}

OP_STATUS OpBrowserView::GetTitle(OpString& title)
{
	return title.Set(m_page->GetTitle());
}

OP_STATUS OpBrowserView::SetEncoding(const uni_char* charset)
{
	return WindowCommanderProxy::SetEncoding(GetWindowCommander(), charset);
}

/***********************************************************************************
**
**  OpBrowserView Accessibility API
**
***********************************************************************************/

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT


int OpBrowserView::GetAccessibleChildrenCount()
{
	return 1;
}

OpAccessibleItem* OpBrowserView::GetAccessibleChild(int index)
{
	if (index == 0)
		return WindowCommanderProxy::GetAccessibleObject(GetWindowCommander());

	return NULL;
}

int OpBrowserView::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	if (child == WindowCommanderProxy::GetAccessibleObject(GetWindowCommander()))
		return 0;
	else
		return Accessibility::NoSuchChild;
}

OpAccessibleItem* OpBrowserView::GetAccessibleChildOrSelfAt(int x, int y)
{
	if (WindowCommanderProxy::GetAccessibleObject(GetWindowCommander())->GetAccessibleChildOrSelfAt(x, y))
		return WindowCommanderProxy::GetAccessibleObject(GetWindowCommander());
	else
		return NULL;
}


OpAccessibleItem* OpBrowserView::GetAccessibleFocusedChildOrSelf()
{
	if (WindowCommanderProxy::GetAccessibleObject(GetWindowCommander())->GetAccessibleFocusedChildOrSelf())
		return WindowCommanderProxy::GetAccessibleObject(GetWindowCommander());
	else
		return NULL;
}

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

/***********************************************************************************
**
**  OpPageListener API
**
***********************************************************************************/

// FORWARDS HANDLES
void OpBrowserView::OnPageHover(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image)
{
	m_hover_url.Set(url);
	m_hover_title.Set(link_title);

	// No tooltips in kiosk mode
	if (!KioskManager::GetInstance()->GetEnabled())
		g_application->SetToolTipListener(this);

	if(GetParentDesktopWindow()) GetParentDesktopWindow()->SetStatusText(url && *url ? url : link_title);
}

// FORWARDS HANDLES
void OpBrowserView::OnPageNoHover(OpWindowCommander* commander)
{
	m_hover_url.Empty();
	m_hover_title.Empty();

	if (g_application->GetToolTipListener() == this)
		g_application->SetToolTipListener(NULL);

	OpString empty;

	if(GetParentDesktopWindow()) GetParentDesktopWindow()->SetStatusText(empty);
}

// SEARCH
void OpBrowserView::OnPageSearchReset(OpWindowCommander* commander)
{
	//This function doesn't seem to be called from core for some reason. Code is somewhere else instead.
	//g_main_message_handler->PostMessage(MSG_DIM_PAGEVIEW, (MH_PARAM_1)m_browser_view->GetParentDesktopWindow(), (MH_PARAM_2)FALSE);
}

void OpBrowserView::OnPageSearchHit(OpWindowCommander* commander)
{
	if (m_search_dim_window == NULL)
		Dim(m_search_dim_window);
}

// HANDLES
void OpBrowserView::OnPageStatusText(OpWindowCommander* commander, const uni_char* text)
{
	if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AllowScriptToChangeStatus))
		return;

	if(GetParentDesktopWindow()) GetParentDesktopWindow()->SetStatusText(text);
}

// HANDLES
void OpBrowserView::OnPageDrag(OpWindowCommander* commander, const OpRect box_rect, const uni_char* link_url, const uni_char* link_title, const uni_char* image_url)
{
	if(!g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag)&DRAG_LINK)
	{
		return;
	}

	OpDragObject* op_drag_object;
	RETURN_VOID_IF_ERROR(OpDragObject::Create(op_drag_object, DRAG_TYPE_LINK));
	DesktopDragObject* drag_object = static_cast<DesktopDragObject*>(op_drag_object);

	drag_object->SetURL(link_url ? link_url : image_url);
	drag_object->SetTitle(link_title);
	drag_object->SetImageURL(image_url);
	drag_object->SetSource(this);

	WindowCommanderProxy::InitDragObject(GetWindowCommander(), drag_object, box_rect);

#if defined(_DEBUG) && defined (MSWIN)
		uni_char buf[100];
		buf[99] = 0;

		uni_sprintf(buf, UNI_L("DRAG OBJECT: %d\r\n\r\n"), drag_object);
		OutputDebugString(buf);
		uni_sprintf(buf, UNI_L("DRAG SOURCE: %d\r\n\r\n"), drag_object->GetSource());
		OutputDebugString(buf);
		uni_sprintf(buf, UNI_L("DRAG SOURCE: %d\r\n\r\n"), this);
		OutputDebugString(buf);
#endif // _DEBUG

	g_drag_manager->StartDrag(drag_object, NULL, FALSE);
}



#ifdef DND_DRAGOBJECT_SUPPORT
void OpBrowserView::OnDragLeave(OpWindowCommander* commander, class DesktopDragObject* drag_object)
{
	// No support in QuickOpWidgetBase
}


void OpBrowserView::OnDragMove(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point)
{
	QuickOpWidgetBase::OnDragMove(drag_object, point);
}


void OpBrowserView::OnDragDrop(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point)
{
	QuickOpWidgetBase::OnDragDrop(drag_object, point);
}
#endif // DND_DRAGOBJECT_SUPPORT

void OpBrowserView::OnPageGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height)
{
	m_op_window->GetInnerSize(width, height);
}

// HANDLES
void OpBrowserView::OnPageConfirm(OpWindowCommander* commander, const uni_char *message, OpDocumentListener::DialogCallback *callback)
{
	if (g_application->GetShowGoOnlineDialog())
	{
		DesktopWindow* parent_window = GetParentDesktopWindow();
#ifdef SELFTEST
		if (g_selftest.suite->DoRun())
			parent_window = g_application->GetActiveDesktopWindow();
#endif // SELFTEST
		g_application->SetConfirmOnlineShow();
		ShowConfirmDialog(parent_window, message, callback);
	}
	else if (callback)
	{
		g_application->AddOfflineQueryCallback(callback);
	}
}

// HANDLES
void OpBrowserView::OnPageQueryGoOnline(OpWindowCommander* commander, const uni_char *message, OpDocumentListener::DialogCallback *callback)
{
	if (g_application->GetShowGoOnlineDialog())
	{
		g_application->SetConfirmOnlineShow();
		ShowConfirmDialog(GetParentDesktopWindow(), message, callback);
	}
	else if (callback)
	{
		g_application->AddOfflineQueryCallback(callback);
	}
}

/** Called when opera wants the ui to display a confirmation dialog asking whether a dropped file's data may be
 passed to a page (it's used only for the cases the file dropping will cause passing it to the page).
 Yes + No buttons are included.
 @param commander the windowcommander in question
 @param server_name the name of the server the dropped files will be uploaded to
 @param callback callback object to notify when the user dismisses the dialog
 DialogCallback::Reply::REPLY_YES if user agrees to upload the file, and anything else if not.
 */
class FileDropConfirmDialogController : public SimpleDialogController
{
public:
	FileDropConfirmDialogController(const uni_char* message, const uni_char* title, OpDocumentListener::DialogCallback* callback):
		SimpleDialogController(SimpleDialogController::TYPE_YES_NO, SimpleDialogController::IMAGE_QUESTION, WINDOW_NAME_CONFIRM, message, title),
		m_callback(callback)
	{
		FlagDialogAsOpen(true);
	}

private:
	virtual void OnOk()
	{
		if (m_callback)
			m_callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_YES);
		m_callback = NULL;

		FlagDialogAsOpen(false);
	}

	virtual void OnCancel()
	{
		if (m_callback)
			m_callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_NO);
		m_callback = NULL;

		FlagDialogAsOpen(false);
	}

	void FlagDialogAsOpen(bool dialog_open)
	{
		DesktopDragObject *drag_object = static_cast<DesktopDragObject *>(g_drag_manager->GetDragObject());
		if(drag_object)
		{
			drag_object->SetDropCaptureLocked(dialog_open);
		}
	}
private:
	OpDocumentListener::DialogCallback* m_callback;
};

void OpBrowserView::OnFileDropConfirm(OpWindowCommander* commander, const uni_char *server_name, OpDocumentListener::DialogCallback *callback)
{
	OpString opera, message;
	OpStatus::Ignore(g_languageManager->GetString(Str::S_OPERA, opera));
	OpStatus::Ignore(g_languageManager->GetString(Str::D_FILE_DROP_CONFIRMATION, message));
	OpStatus::Ignore(message.ReplaceAll(UNI_L("%1"), server_name, 1));

	FileDropConfirmDialogController* controller = OP_NEW(FileDropConfirmDialogController,(message, opera, callback));
	if (OpStatus::IsError(ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow())))
	{
		callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_NO);
	}
}

// HANDLES
void OpBrowserView::OnPageDownloadRequest(OpWindowCommander* commander, OpDocumentListener::DownloadCallback * download_callback)
{
	/**
	 * If the parent of this OpBrowserView is the plugin install dialog, open a new window for the requested download.
	 */
	Type parent_type = this->GetParentDesktopWindowType();
	if (DIALOG_TYPE_PLUGIN_INSTALL == parent_type)
	{
		OpString download_url;
		RETURN_VOID_IF_ERROR(download_url.Set((download_callback->URLUniFullName())));
		g_application->OpenURL(download_url, YES, YES, NO);
		return;
	}

	BOOL no_download_dialog = KioskManager::GetInstance()->GetNoDownload();

	if(no_download_dialog)
		download_callback->Abort();
	else
	{
		if (!m_chooser)
			RETURN_VOID_IF_ERROR(DesktopFileChooser::Create(&m_chooser));

		BOOL is_privacy_mode = GetParentDesktopWindow() ? GetParentDesktopWindow()->PrivacyMode() : FALSE;
		DownloadItem * di = OP_NEW(DownloadItem, (download_callback, m_chooser, TRUE, is_privacy_mode));

		if (di)
		{
			if (download_callback->Mode() == OpDocumentListener::SAVE)
			{
				di->Init();
				di->Save(FALSE);
			}
#ifdef WIC_CAP_URLINFO_OPENIN
			else if (download_callback->Mode() == OpDocumentListener::OPEN_IN)
			{
				di->Init();
				di->Open(FALSE, FALSE, FALSE);
			}
#endif // WIC_CAP_URLINFO_OPENIN
#ifdef _BITTORRENT_SUPPORT_
			else if(IsBittorrentDownload(download_callback) && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableBitTorrent))
			{
				BTSelectionDialog* dialog = OP_NEW(BTSelectionDialog, (di));
				if (dialog)
					dialog->Init(GetParentDesktopWindow());
			}
#endif // _BITTORRENT_SUPPORT_
			else
			{
				if (g_download_manager_manager->GetExternalManagerEnabled())
				{
					// use the saved selection
					if (!g_download_manager_manager->GetShowDownloadManagerSelectionDialog())
					{
						ExternalDownloadManager* manager = g_download_manager_manager->GetDefaultManager();
						if (manager)
						{
							if (OpStatus::IsSuccess(manager->Run(download_callback->URLName())))
							{
								download_callback->Abort();
								OP_DELETE(di);
								return;
							}
						}
					}
					else if (g_download_manager_manager->GetHasExternalDownloadManager())
					{
						DownloadSelectionController* controller = OP_NEW(DownloadSelectionController,(download_callback, di));
						ShowDialog(controller, g_global_ui_context, GetParentDesktopWindow());
						return;
					}
				}

				DownloadDialog* dialog = OP_NEW(DownloadDialog, (di));
				if (dialog)
					dialog->Init(GetParentDesktopWindow());
			}
		}
	}
}

void OpBrowserView::OnPageSubscribeFeed(OpWindowCommander* commander, const uni_char* url)
{
#ifdef M2_SUPPORT
	if (g_application->ShowM2())
	{
		URL feed_url = g_url_api->GetURL(url);
		OpStatus::Ignore(g_m2_engine->LoadRSSFeed(0, feed_url, TRUE));
	}
	else
	{
		SimpleDialogController* controller = OP_NEW(SimpleDialogController,
			(SimpleDialogController::TYPE_OK, SimpleDialogController::IMAGE_ERROR,WINDOW_NAME_ACTIVATE_MAIL_FOR_FEEDS,
			 Str::D_MAIL_PLEASE_ACTIVATE_OPERA_MAIL_FOR_FEEDS, Str::D_MAIL_PLEASE_ACTIVATE_OPERA_MAIL_FOR_FEEDS_TITLE));

		ShowDialog(controller, g_global_ui_context,g_application->GetActiveDesktopWindow());
	}
#endif // M2_SUPPORT
}

#ifdef WIC_CAP_UPLOAD_FILE_CALLBACK
// HANDLES
BOOL OpBrowserView::OnPageUploadConfirmationNeeded(OpWindowCommander* commander)
{
# if defined(_KIOSK_MANAGER_)
	if( g_application && KioskManager::GetInstance()->GetEnabled() )
		return FALSE;
	else
# endif // _KIOSK_MANAGER_
		return TRUE;
}

// HANDLES
void OpBrowserView::OnPageFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (m_upload_callback)
	{
		if (result.files.GetCount())
		{
			m_upload_callback->OnFileSelected(&result.files);
#  ifndef PREFS_NO_WRITE
			if (result.active_directory.HasContent())
			{
				TRAPD(error, g_pcfiles->WriteDirectoryL(OPFILE_OPEN_FOLDER, result.active_directory));
			}
#  endif // PREFS_NO_WRITE
		}
		else
		{
			m_upload_callback->Cancel();
		}
	}
	m_upload_callback = NULL;
}
#endif // WIC_CAP_UPLOAD_FILE_CALLBACK

#if defined (DOM_GADGET_FILE_API_SUPPORT) && defined (WIC_CAP_HAS_MOUNTPOINT_API)
// HANDLES
void OpBrowserView::OnPageMountpointFolderRequest(OpWindowCommander* commander, MountPointFolderCallback* callback, MountPointFolderCallback::FolderType folder_type)
{
	commander = commander;
	folder_type = folder_type;
	// TODO: Implement using DesktopFileChooser, NOTE: consider security implications
	return callback->MountPointCallbackCancel();
}

// HANDLES
void OpBrowserView::OnPageMountpointFolderCancel(OpWindowCommander* commander, MountPointFolderCallback* callback)
{
	commander = commander;
	callback = callback;
	// TODO: Implement using DesktopFileChooser
}
#endif // DOM_GADGET_FILE_API_SUPPORT && WIC_CAP_HAS_MOUNTPOINT_API

// HANDLES
void OpBrowserView::OnPageAskAboutUrlWithUserName(OpWindowCommander* commander, const uni_char* url, const uni_char* hostname, const uni_char* username, OpDocumentListener::DialogCallback* callback)
{
	callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_YES);
}

// HANDLES
void OpBrowserView::OnPageAskAboutFormRedirect(OpWindowCommander* commander, const uni_char* source_url, const uni_char* target_url, OpDocumentListener::DialogCallback* callback)
{
	AskAboutFormRedirectDialog * dialog = OP_NEW(AskAboutFormRedirectDialog, (callback));
	if (!dialog || OpStatus::IsError(dialog->Init(GetParentDesktopWindow(), source_url, target_url)))
		callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_CANCEL);
}

// HANDLES
void OpBrowserView::OnPageFormRequestTimeout(OpWindowCommander* commander, const uni_char* url)
{
	FormRequestTimeoutDialog * dialog = OP_NEW(FormRequestTimeoutDialog, (NULL/*callback*/));
	if (!dialog || OpStatus::IsError(dialog->Init(GetParentDesktopWindow(), url)))
	{
		//		callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_CANCEL);
	}
}

// HANDLES
BOOL OpBrowserView::OnPageAnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info)
{
	return g_application->AnchorSpecial(commander, info);
}

// HANDLES
void OpBrowserView::OnPageJSAlert(OpWindowCommander* commander,	const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback)
{
	if(m_suppress_dialogs)
	{
		return;
	}

	JSAlertDialog* dialog = OP_NEW(JSAlertDialog, ());
	if (!dialog)
		return;
	OpString full_message;

	// For kiosk and embedded mode don't show the hostname
	if (KioskManager::GetInstance()->GetEnabled())
		full_message.Set(message);
	else
	{
		if (commander->GetGadget())
		{
			OpString gname;
			commander->GetGadget()->GetGadgetName(gname);
			full_message.AppendFormat(UNI_L("%s\n\n%s"), gname.CStr(), message);
		}
		else
			full_message.AppendFormat(UNI_L("<%s>\n\n%s"), hostname, message);
	}
	if(hostname && uni_stricmp(hostname, UNI_L("opera:config")) == 0)
	{
		dialog->SetShowDisableScriptOption(FALSE);
	}
	dialog->Init(GetParentDesktopWindow(), full_message.CStr(), callback, commander, this);
}

// HANDLES
void OpBrowserView::OnPageJSConfirm(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback)
{
	if(m_suppress_dialogs)
	{
		return;
	}

	JSConfirmDialog* dialog = OP_NEW(JSConfirmDialog, ());
	if (!dialog)
		return;
	OpString full_message;

	// For kiosk and embedded mode don't show the hostname
	if (KioskManager::GetInstance()->GetEnabled())
		full_message.Set(message);
	else
		full_message.AppendFormat(UNI_L("<%s>\n\n%s"), hostname, message);

	dialog->Init(GetParentDesktopWindow(), full_message.CStr(), callback, commander, this);
}

// HANDLES
void OpBrowserView::OnPageJSPrompt(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, const uni_char *default_value, OpDocumentListener::JSDialogCallback *callback)
{
	if(m_suppress_dialogs)
	{
		return;
	}

	JSPromptDialog* dialog = OP_NEW(JSPromptDialog, ());
	if (!dialog)
		return;
	OpString full_message;

	// For kiosk and embedded mode don't show the hostname
	if (KioskManager::GetInstance()->GetEnabled())
		full_message.Set(message);
	else
		full_message.AppendFormat(UNI_L("<%s>\n\n%s"), hostname, message);

	dialog->Init(GetParentDesktopWindow(), full_message.CStr(), default_value, callback, commander, this);
}

// HANDLES
void OpBrowserView::OnPageFormSubmit(OpWindowCommander* commander, OpDocumentListener::FormSubmitCallback * callback)
{
	TryCloseLastPostDialog();
	OpenNewOnFormSubmitDialog(callback);
}

// HANDLES
void OpBrowserView::OnPagePluginPost(OpWindowCommander* commander, OpDocumentListener::PluginPostCallback * callback)
{
	TryCloseLastPostDialog();
	OpenNewOnPluginPostDialog(callback);
}

// HANDLES
void OpBrowserView::OnPageVisibleRectChanged(OpWindowCommander* commander, const OpRect& visible_rect)
{
	DesktopWindow* desktop_window = GetParentDesktopWindow();

	if (desktop_window && desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		((DocumentDesktopWindow*) desktop_window)->LayoutPopupProgress(visible_rect);
	}
}

// HANDLES
void OpBrowserView::OnPageAskPluginDownload(OpWindowCommander* commander)
{
	g_main_message_handler->PostMessage(MSG_QUICK_ASK_PLUGIN_DOWNLOAD, (MH_PARAM_1)this, 0);
}

void OpBrowserView::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
		case MSG_QUICK_ASK_PLUGIN_DOWNLOAD:
		{
			AskPluginDownloadDialog* dialog = OP_NEW(AskPluginDownloadDialog, ());
			if (dialog)
				dialog->Init(GetParentDesktopWindow(), UNI_L("Macromedia Flash Player"), 
						UNI_L("http://redir.opera.com/plugins/?application/x-shockwave-flash"),
						Str::D_SHORT_PLUGIN_DOWNLOAD_RESOURCE_MESSAGE);
			break;
		}
#ifdef WIC_COLORSELECTION_LISTENER
		case MSG_QUICK_DELAYED_COLOR_SELECTOR:
		{
			OpBrowserView *target_view = reinterpret_cast<OpBrowserView*>(par1);
			if (target_view != this)
				return;

			OP_ASSERT(m_color_selection_callback);

			CallbackColorChooserListener listener;

			// Show our synchronous color chooser and respond to the core callback.
			OpColorChooser* chooser = NULL;
			if (OpStatus::IsSuccess(OpColorChooser::Create(&chooser)))
			{
				// We could use GetWindowCommander()->GetOpWindow() but the Show method takes a DesktopWindow for no apparent reason!
				chooser->Show(m_color_selection_callback->GetInitialColor(), &listener, g_application->GetActiveDesktopWindow());
				OP_DELETE(chooser);
			}

			// Call back the result
			// m_color_selection_callback may have become NULL if OnColorSelectionCancel was called by core while the dialog was up.
			if (m_color_selection_callback)
			{
				if (listener.is_color_selected)
					m_color_selection_callback->OnColorSelected(listener.selected_color);
				else
					m_color_selection_callback->OnCancel();

				// We're done and must clear this callback as core will delete it
				m_color_selection_callback = NULL;
			}

			break;
		}
#endif // WIC_COLORSELECTION_LISTENER
		case MSG_DIM_PAGEVIEW:
			// Only used for canceling the search dim.
			OP_ASSERT(par2 == 0);
			Undim(m_search_dim_window);
			break;
	}
}

#if defined _PRINT_SUPPORT_ && defined WIC_CAP_JAVASCRIPT_PRINT
// HANDLES
void OpBrowserView::OnPageJSPrint(OpWindowCommander * commander, PrintCallback * callback)
{
#ifdef DESKTOP_PRINTDIALOG
	if (m_desktop_printdialog == NULL && OpStatus::IsSuccess(DesktopPrintDialog::Create(&m_desktop_printdialog, commander->GetOpWindow(), callback, this)))
		return;
#endif
	callback->Cancel();
}

// HANDLES
void OpBrowserView::OnPageUserPrint(OpWindowCommander * commander, PrintCallback * callback)
{
#ifdef DESKTOP_PRINTDIALOG
	if (m_desktop_printdialog == NULL && OpStatus::IsSuccess(DesktopPrintDialog::Create(&m_desktop_printdialog, commander->GetOpWindow(), callback, this)))
		return;
#endif
	callback->Cancel();
}
#endif // WIC_CAP_JAVASCRIPT_PRINT && _PRINT_SUPPORT_

//HANDLES
void OpBrowserView::OnPageMailTo(OpWindowCommander * commander, const OpStringC8& raw_mailto, BOOL mailto_from_dde, BOOL mailto_from_form, const OpStringC8& servername)
{
	// Save away all string input and warn about form submit mailto
	// Huib has decided to not support a security warning at this point. rfz - 20091009.
	// if (mailto_from_form)
	// {
	// }
	MailTo mailto;
	mailto.Init(raw_mailto);
	MailCompose::ComposeMail(mailto, mailto_from_dde);
}

#ifdef _BITTORRENT_SUPPORT_
BOOL OpBrowserView::IsBittorrentDownload(OpDocumentListener::DownloadCallback* download_callback)
{
	OP_ASSERT(NULL != download_callback);

	const char* bt_mime_type = Viewers::GetContentTypeString(URL_P2P_BITTORRENT);
	const char* download_mime_type = download_callback->MIMEType();

	return OpStringC8(bt_mime_type) == download_mime_type;
}
#endif // _BITTORRENT_SUPPORT_

UINT32 OpBrowserView::GetClipboardToken()
{
	return GetWindowCommander()->GetPrivacyMode() ? windowManager->GetPrivacyModeContextId() : 0; 
}

void OpBrowserView::OnPageSetupInstall(OpWindowCommander* commander, const uni_char* url, URLContentType content_type)
{
	URL current_url = g_url_api->GetURL(url);
	if(content_type == URL_SKIN_CONFIGURATION_CONTENT)
	{
		DesktopWindow* desktop_window = GetParentDesktopWindow();

		if (desktop_window && desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
		{
			static_cast<DocumentDesktopWindow *>(desktop_window)->InstallSkin(url);
			return;
		}
	}
	// fallback
	SetupApplyDialog* dialog = OP_NEW(SetupApplyDialog, (&current_url));
	if (dialog == NULL)
	{
		return;
	}
	dialog->Init(g_application->GetActiveDesktopWindow());
}

void OpBrowserView::ShowConfirmDialog(DesktopWindow* parent_window, const uni_char* message, OpDocumentListener::DialogCallback* callback)
{
	if (!callback)
		return;

	OpString opera;
	g_languageManager->GetString(Str::S_OPERA, opera);
	ConfirmDialogController* controller = OP_NEW(ConfirmDialogController,(message, opera, callback));
	if (OpStatus::IsError(ShowDialog(controller, g_global_ui_context,parent_window)))
	{
		callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_NO);
		g_application->UpdateOfflineMode(OpDocumentListener::DialogCallback::REPLY_NO);
		return;
	}
};
