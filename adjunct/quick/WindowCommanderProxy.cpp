 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/desktop_util/mail/mailcompose.h"
#include "adjunct/desktop_util/mail/mailto.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/sessions/SessionAutoSaveManager.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/Application.h" // For CanViewSourceOfURL
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/thumbnails/src/thumbnail.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "modules/util/filefun.h"
#include "modules/util/path.h"
#include "modules/util/OpTypedObject.h"
#include "modules/display/VisDevListeners.h"
#include "modules/logdoc/logdoc_util.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "adjunct/quick/controller/ImagePropertiesController.h"
#include "modules/forms/piforms.h"
#include "modules/forms/form.h"
#include "adjunct/quick/dialogs/PreferencesDialog.h"
#include "adjunct/quick/widgets/OpInfobar.h"
#include "adjunct/quick/dialogs/ContentBlockDialog.h"
#include "modules/content_filter/content_filter.h"
#include "modules/ns4plugins/src/plugin.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/dialogs/MidClickDialog.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/savewithinline.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/doc/frm_doc.h"
#include "modules/url/url2.h"
#include "modules/upload/upload.h"
#include "modules/gadgets/OpGadget.h"
#ifdef VIEWPORTS_SUPPORT
# include "modules/windowcommander/OpViewportController.h"
#endif
#include "modules/windowcommander/src/WindowCommander.h"

#ifdef SPEEDDIAL_SVG_SUPPORT
#include "modules/svg/SVGManager.h"
#include "modules/svg/svg_image.h"
#endif // SPEEDDIAL_SVG_SUPPORT

#ifdef MSWIN
BOOL HandleOldBrowserWin32Actions(OpInputAction* action, Window* window);
# include "platforms/windows/windows_ui/res/#BuildNr.rci"	//for VER_BUILD_NUMBER_STR
#elif defined _MACINTOSH_
# include "platforms/mac/model/CMacSpecificActions.h"
#endif

#if defined(EMBROWSER_SUPPORT)
# include "adjunct/embrowser/EmBrowser_main.h"
OpWindowCommander* GetActiveEmBrowserWindowCommander();
#endif // EMBROWSER_SUPPORT

#include "adjunct/desktop_util/history/DocumentHistory.h"
#include "adjunct/quick/managers/opsetupmanager.h"

namespace WindowCommanderProxyConsts
{
	const unsigned ACTIVE_PAGE_LOADING_DELAY = 1; // ms
	const unsigned PAGE_LOADING_DELAY = 100; // ms
}

Window* GetWindow(OpWindowCommander* win_comm)
{
	if(!win_comm)
		return NULL;

	return win_comm->GetWindow();
}

Window* GetWindow(OpBrowserView* browser_view)
{
	if(!browser_view)
		return NULL;

	return GetWindow(browser_view->GetWindowCommander());
}


FramesDocument* GetCurrentDoc(OpWindowCommander* win_comm)
{
	if(GetWindow(win_comm) == NULL)
		return NULL;

	return GetWindow(win_comm)->GetCurrentDoc();
}

FramesDocument* GetCurrentDoc(OpBrowserView* browser_view)
{
	if(GetWindow(browser_view) == NULL)
		return NULL;

	return GetWindow(browser_view)->GetCurrentDoc();
}

FramesDocument* GetActiveFrameDocument(OpWindowCommander* win_comm, BOOL allow_fallback)
{
	if( !GetCurrentDoc(win_comm) )
		return 0;

	FramesDocument* doc = GetWindow(win_comm)->GetActiveFrameDoc();

	if (doc && doc->GetDocManager() && doc->GetDocManager()->GetFrame())
		return doc;
	else
		return allow_fallback ? doc : 0;
}

FramesDocument* WindowCommanderProxy::GetTopDocument(OpWindowCommander* win_comm)
{
	FramesDocument* doc = NULL;
	if(win_comm && win_comm->GetWindow())
	{
		doc = win_comm->GetWindow()->GetCurrentDoc();
		if(doc)
		{
			FramesDocument* parent_doc = doc->GetParentDoc();
			while (parent_doc)
			{
				doc = parent_doc;
				parent_doc = parent_doc->GetParentDoc();
			}
		}
	}
	return doc;
}

VisualDevice* WindowCommanderProxy::GetVisualDevice(OpWindowCommander* win_comm)
{
	if(!GetWindow(win_comm))
		return NULL;

	return GetWindow(win_comm)->VisualDev();
}

void WindowCommanderProxy::SetType(OpWindowCommander* win_comm, Window_Type new_type)
{
	GetWindow(win_comm)->SetType(new_type);
}

void WindowCommanderProxy::SetType(OpBrowserView* browser_view, Window_Type new_type)
{
	if(HasWindow(browser_view))
		GetWindow(browser_view)->SetType(new_type);
}

Window_Type	WindowCommanderProxy::GetType(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetType();
}

OP_STATUS WindowCommanderProxy::SetFramesPrintType(OpWindowCommander* win_comm,
												   DM_PrintType fptype,
												   BOOL update)
{
	return GetWindow(win_comm)->SetFramesPrintType(fptype, update);
}

BOOL WindowCommanderProxy::HasFramesPrintType(OpWindowCommander* win_comm,
											  DM_PrintType fptype)
{
	return GetWindow(win_comm)->GetFramesPrintType() == fptype;
}

#ifdef ACCESS_KEYS_SUPPORT
void WindowCommanderProxy::UpdateAccessKeys(OpWindowCommander* win_comm, CycleAccessKeysPopupWindow*& cycles_accesskeys_popupwindow)
{
	if(cycles_accesskeys_popupwindow)
	{
		FramesDocument *document;
		FramesDocument *frames_doc;
		HLDocProfile *profile;
		OpVector<HLDocProfile::AccessKey> accesskeys;

		document = GetCurrentDoc(win_comm);

		if(document)
		{
			document = document->GetTopDocument();
		}
		if(document)
		{
			DocumentTreeIterator doc_tree(document->GetDocManager());

			do
			{
				frames_doc = doc_tree.GetFramesDocument();

				if(frames_doc)
				{
					profile = frames_doc->GetHLDocProfile();
					if(profile)
					{
						OpStatus::Ignore(profile->GetAllAccessKeys(accesskeys));
					}
				}
			} while(doc_tree.Next());

			UINT32 cnt;

			for(cnt = 0; cnt < accesskeys.GetCount(); cnt++)
			{
				HLDocProfile::AccessKey *key = accesskeys.Get(cnt);

				if(key)
				{
					cycles_accesskeys_popupwindow->AddAccessKey(key->title.CStr(), key->url.CStr(), key->key);
				}
			}
			if(accesskeys.GetCount() == 0)
			{
				OpString def_msg;

				g_languageManager->GetString(Str::D_NO_ACCESSKEYS, def_msg);

				cycles_accesskeys_popupwindow->AddAccessKey(def_msg.CStr(), NULL, 0);
			}
		}
	}
}
#endif // ACCESS_KEYS_SUPPORT


BOOL WindowCommanderProxy::HasWindow(OpBrowserView* browser_view)
{
	return GetWindow(browser_view) != NULL;
}

BOOL WindowCommanderProxy::HasVisualDevice(OpWindowCommander* win_comm)
{
	return GetVisualDevice(win_comm) != NULL;
}

BOOL WindowCommanderProxy::HasCurrentDoc(OpBrowserView* browser_view)
{
	return GetCurrentDoc(browser_view) != NULL;
}

BOOL WindowCommanderProxy::HasCurrentDoc(OpWindowCommander* win_comm)
{
	return GetCurrentDoc(win_comm) != NULL;
}

BOOL WindowCommanderProxy::HasActiveFrameDocument(OpWindowCommander* win_comm)
{
	return GetActiveFrameDocument(win_comm, FALSE) != NULL;
}

BOOL WindowCommanderProxy::HasCSSModeNONE(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetCSSMode() == CSS_NONE;
}

BOOL WindowCommanderProxy::HasCSSModeFULL(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetCSSMode() == CSS_FULL;
}

BOOL WindowCommanderProxy::IsMailOrNewsfeedWindow(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->IsMailOrNewsfeedWindow();
}

BOOL WindowCommanderProxy::HasDownloadError(OpWindowCommander* win_comm)
{
	if(!HasCurrentDoc(win_comm))
		return FALSE;

	return GetCurrentDoc(win_comm)->GetURL().GetAttribute(URL::KHTTP_Response_Code) != 200;
}

URLType	WindowCommanderProxy::Type(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetCurrentURL().Type();
}

URL WindowCommanderProxy::GetCurrentURL(OpWindowCommander* win_comm)
{
	if(HasCurrentDoc(win_comm) == FALSE)
		return URL();
	return GetWindow(win_comm)->GetCurrentURL();
}

URL WindowCommanderProxy::GetCurrentDocURL(OpWindowCommander* win_comm)
{
	if(HasCurrentDoc(win_comm) == FALSE)
		return URL();
	return GetCurrentDoc(win_comm)->GetURL();
}

URL WindowCommanderProxy::GetCurrentDocURL(OpBrowserView* browser_view)
{
	if(HasCurrentDoc(browser_view) == FALSE)
		return URL();
	return GetCurrentDoc(browser_view)->GetURL();
}

URL WindowCommanderProxy::GetCurrentDocCurrentURL(OpWindowCommander* win_comm)
{
	if(HasCurrentDoc(win_comm) == FALSE)
		return URL();

	const uni_char* dummy = NULL;
	return GetCurrentDoc(win_comm)->GetCurrentURL(dummy, -1);
}

URL WindowCommanderProxy::GetActiveFrameDocumentURL(OpWindowCommander* win_comm)
{
	FramesDocument* doc = GetActiveFrameDocument(win_comm, TRUE);
	if( doc && !doc->GetURL().IsEmpty() )
		return doc->GetURL();
	return URL();
}

BOOL WindowCommanderProxy::GetPreviewMode(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetPreviewMode();
}

BOOL WindowCommanderProxy::GetScrollIsPan(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetScrollIsPan();
}

void WindowCommanderProxy::OverrideScrollIsPan(OpWindowCommander* win_comm)
{
	GetWindow(win_comm)->OverrideScrollIsPan();
}

const char* WindowCommanderProxy::GetForceEncoding(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetForceEncoding();
}

ServerName* WindowCommanderProxy::GetServerName(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm) ? GetCurrentURL(win_comm).GetServerName() : NULL;
}

BOOL WindowCommanderProxy::GetPageInfo(OpBrowserView* browser_view, URL& out_url)
{
	if(HasCurrentDoc(browser_view))
		return GetCurrentDoc(browser_view)->GetPageInfo(out_url) == OpBoolean::IS_TRUE;
	return FALSE;
}

void WindowCommanderProxy::TogglePrintMode(OpWindowCommander* win_comm)
{
	GetWindow(win_comm)->TogglePrintMode(TRUE);
}

BOOL WindowCommanderProxy::GetPrintMode(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetPrintMode();
}

void WindowCommanderProxy::UpdateCurrentDoc(OpWindowCommander* win_comm)
{
	GetWindow(win_comm)->DocManager()->UpdateCurrentDoc(FALSE, FALSE, TRUE);
}

void WindowCommanderProxy::UpdateWindow(OpWindowCommander* win_comm)
{
	GetWindow(win_comm)->UpdateWindow();
}

long WindowCommanderProxy::GetDocProgressCount(OpBrowserView* browser_view)
{
	return GetWindow(browser_view)->GetDocProgressCount();
}

long WindowCommanderProxy::GetDocProgressCount(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetDocProgressCount();
}

long WindowCommanderProxy::GetProgressCount(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetProgressCount();
}

const uni_char*	WindowCommanderProxy::GetProgressMessage(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetProgressMessage();
}

time_t WindowCommanderProxy::GetProgressStartTime(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetProgressStartTime();
}

OpFileLength WindowCommanderProxy::GetContentSize(OpBrowserView* browser_view)
{
	if(HasCurrentDoc(browser_view))
		return GetCurrentDoc(browser_view)->GetURL().GetContentSize();
	return 0;
}

BOOL WindowCommanderProxy::ShowScrollbars(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->ShowScrollbars();
}

void WindowCommanderProxy::SetParentInputContext(OpWindowCommander* win_comm, OpInputContext* parent_context)
{
	if(HasVisualDevice(win_comm))
		GetVisualDevice(win_comm)->SetParentInputContext(parent_context);
}

void WindowCommanderProxy::SetNextCSSMode(OpWindowCommander* win_comm)
{
	GetWindow(win_comm)->SetNextCSSMode();
}

#ifdef SUPPORT_VISUAL_ADBLOCK
void WindowCommanderProxy::SetContentBlockEditMode(OpWindowCommander* win_comm, BOOL edit_mode)
{
	GetWindow(win_comm)->SetContentBlockEditMode(edit_mode);
}

void WindowCommanderProxy::SetContentBlockEnabled(OpWindowCommander* win_comm, BOOL enable)
{
	GetWindow(win_comm)->SetContentBlockEnabled(enable);
}

BOOL WindowCommanderProxy::IsContentBlockerEnabled(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->IsContentBlockerEnabled();
}
#endif // SUPPORT_VISUAL_ADBLOCK

void WindowCommanderProxy::RemoveAllElementsFromHistory(OpWindowCommander* win_comm, BOOL unlink)
{
	GetWindow(win_comm)->RemoveAllElementsFromHistory(unlink);
}

void WindowCommanderProxy::RemoveAllElementsExceptCurrent(OpWindowCommander* win_comm)
{
	GetWindow(win_comm)->RemoveAllElementsExceptCurrent();
}

void WindowCommanderProxy::LoadAllImages(OpWindowCommander* win_comm)
{
	GetWindow(win_comm)->DocManager()->LoadAllImages();
}

void WindowCommanderProxy::ClearSelection(OpBrowserView* browser_view, BOOL clear_focus_and_highlight, BOOL clear_search)
{
	if(HasCurrentDoc(browser_view))
		GetCurrentDoc(browser_view)->ClearSelection(clear_focus_and_highlight, clear_search);
}

BOOL WindowCommanderProxy::HighlightNextMatch(OpBrowserView* browser_view,
											  const uni_char* txt,
											  BOOL match_case,
											  BOOL match_words,
											  BOOL links_only,
											  BOOL forward,
											  BOOL wrap,
											  BOOL &wrapped)
{
	if(HasWindow(browser_view) == FALSE)
		return FALSE;

	OpRect dummy_rect;
	return GetWindow(browser_view)->HighlightNextMatch(txt, match_case, match_words, links_only, forward, wrap, wrapped, dummy_rect);
}

OP_STATUS WindowCommanderProxy::OpenURL(OpBrowserView* browser_view,
										URL& url,
										URL& ref_url,
										BOOL check_if_expired,
										BOOL reload,
										short sub_win_id,
										BOOL user_initated,
										BOOL open_in_background,
										EnteredByUser entered_by_user,
										BOOL called_from_dde)
{
	if(HasWindow(browser_view) == FALSE)
		return OpStatus::OK;

	return GetWindow(browser_view)->OpenURL(url, DocumentReferrer(ref_url), check_if_expired, reload, sub_win_id, user_initated, open_in_background, entered_by_user, called_from_dde);
}

OP_STATUS WindowCommanderProxy::OpenURL(OpWindowCommander* win_comm,
										URL& url,
										URL& ref_url,
										BOOL check_if_expired,
										BOOL reload,
										short sub_win_id,
										BOOL user_initated,
										BOOL open_in_background,
										EnteredByUser entered_by_user,
										BOOL called_from_dde)
{
	return GetWindow(win_comm)->OpenURL(url, DocumentReferrer(ref_url), check_if_expired, reload, sub_win_id, user_initated, open_in_background, entered_by_user, called_from_dde);
}

OP_STATUS WindowCommanderProxy::OpenURL(OpBrowserView* browser_view,
										const uni_char* in_url_str,
										BOOL check_if_expired,
										BOOL user_initated,
										BOOL reload,
										BOOL is_hotlist_url,
										BOOL open_in_background,
										EnteredByUser entered_by_user,
										BOOL called_from_dde)
{
	if(HasWindow(browser_view) == FALSE)
		return OpStatus::OK;

	return GetWindow(browser_view)->OpenURL(in_url_str, check_if_expired, user_initated, reload, is_hotlist_url, open_in_background, entered_by_user, called_from_dde);
}

OP_STATUS WindowCommanderProxy::OpenURL(OpWindowCommander* win_comm,
										const uni_char* in_url_str,
										BOOL check_if_expired,
										BOOL user_initated,
										BOOL reload,
										BOOL is_hotlist_url,
										BOOL open_in_background,
										EnteredByUser entered_by_user,
										BOOL called_from_dde)
{
	return GetWindow(win_comm)->OpenURL(in_url_str, check_if_expired, user_initated, reload, is_hotlist_url, open_in_background, entered_by_user, called_from_dde);
}

#ifdef GADGET_SUPPORT
OP_STATUS WindowCommanderProxy::OpenURL(OpBrowserView* browser_view,
										const uni_char* in_url_str,
					  					URL_CONTEXT_ID context_id)
{
	if(HasWindow(browser_view) == FALSE)
		return OpStatus::OK;

	return GetWindow(browser_view)->OpenURL(
			in_url_str,
			TRUE,	// check_if_expired
			FALSE,	// user_initated
			FALSE,	// reload
			FALSE,	// is_hotlist_url
			FALSE,	// open_in_background
			NotEnteredByUser,
			FALSE,	// called from dde
			context_id);
}

OP_STATUS WindowCommanderProxy::OpenURL(OpWindowCommander* win_comm,
										const uni_char* in_url_str,
					  					URL_CONTEXT_ID context_id)
{
	return GetWindow(win_comm)->OpenURL(
			in_url_str,
			TRUE,	// check_if_expired
			FALSE,	// user_initated
			FALSE,	// reload
			FALSE,	// is_hotlist_url
			FALSE,	// open_in_background
			NotEnteredByUser,
			FALSE,	// called from dde
			context_id);
}
#endif // GADGET_SUPPORT

extern HTML_Element* FindDefButtonInternal(FramesDocument* frames_doc, HTML_Element* helm, HTML_Element* form_elm);

// takes any input element from a form as input and returns the submit element for the same form
HTML_Element * GetFormSubmit(FramesDocument* frm_doc, HTML_Element *he)
{
	HTML_Element* form_elm = FormManager::FindFormElm(frm_doc, he);
	HTML_Element* search_under = form_elm;
	if (!search_under)
	{
		search_under = frm_doc->GetLogicalDocument()->GetRoot();
	}

	return FindDefButtonInternal(frm_doc, search_under, form_elm);
}

HTML_Element * GetFormRoot(HTML_Element *helm, HTML_Element *he)
{
	int form_number = he->GetFormNr();
	HTML_Element* def_button_candidate = NULL;

	if(form_number < 0)
	{
		return NULL;
	}
	while(helm)
	{
		if (helm->GetFormNr() == form_number)
		{
			if(helm->Type() == HE_FORM)
			{
				def_button_candidate = helm;
			}
		}

		helm = (HTML_Element*) helm->NextActual();
	}
	return def_button_candidate;
}

BOOL WindowCommanderProxy::HasSearchForm(OpWindowCommander* win_comm)
{
	if(win_comm && GetWindow(win_comm))
	{
		FramesDocument* frames_doc = GetCurrentDoc(win_comm);

		if (frames_doc)
		{
			HTML_Element *htm_elem = frames_doc->GetCurrentHTMLElement();

			// handle IFRAMEs
			if(htm_elem == NULL)
			{
				frames_doc = GetActiveFrameDocument(win_comm, FALSE);
				if(frames_doc)
				{
					htm_elem = frames_doc->GetCurrentHTMLElement();
				}
			}

			if (htm_elem && htm_elem->GetFormObject())
			{
				HTML_Element *root_htm = htm_elem;
				while (root_htm->Parent())
				{
					root_htm = root_htm->Parent();
				}

				HTML_Element* form_he = GetFormRoot(root_htm, htm_elem);
				if (form_he && form_he->GetAction())
				{
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

FramesDocElm* GetBiggestFrame(FramesDocElm* elm)
{
	INT32 maxsize = 0;

	FramesDocElm* maxelm = elm->GetVisualDevice() ? elm : NULL;

	for (FramesDocElm* child = elm->FirstChild(); child; child = child->Suc())
	{
		FramesDocElm* maxchildelm = GetBiggestFrame(child);

		if (maxchildelm)
		{
			INT32 childsize = maxchildelm->GetWidth() * maxchildelm->GetHeight();

			if (childsize > maxsize)
			{
				maxsize = childsize;
				maxelm = maxchildelm;
			}
		}
	}

	return maxelm;
}

BOOL WindowCommanderProxy::FocusDocument(OpWindowCommander* win_comm, FOCUS_REASON reason)
{
	FramesDocument* doc = GetCurrentDoc(win_comm);

	if (doc && doc->IsFrameDoc())
	{
		FramesDocElm* root = doc->GetFrmDocElm(0);

		if (root)
		{
			FramesDocElm* elm  = GetBiggestFrame(root);

			if (elm && elm->GetVisualDevice())
				elm->GetVisualDevice()->SetFocus(reason);
		}
		return TRUE;
	}
	return FALSE;
}

void WindowCommanderProxy::FocusPage(OpWindowCommander* win_comm)
{
	FramesDocument *activeDoc = GetWindow(win_comm)->GetActiveFrameDoc();
	if (activeDoc && activeDoc->current_focused_formobject)
	{
		if (activeDoc->Type() == DOC_FRAMES)
			FormObject::ResetDefaultButton((FramesDocument*)activeDoc);
		activeDoc->current_focused_formobject = NULL;
	}
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OpAccessibleItem* WindowCommanderProxy::GetAccessibleObject(OpWindowCommander* win_comm)
{
	return GetVisualDevice(win_comm);
}
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

void WindowCommanderProxy::SetFocusToVisualDevice(OpWindowCommander* win_comm, FOCUS_REASON reason)
{
	if(HasVisualDevice(win_comm))
		GetVisualDevice(win_comm)->SetFocus(reason);
}

BOOL WindowCommanderProxy::IsVisualDeviceFocused(OpWindowCommander* win_comm, BOOL HasFocusedChildren)
{
	return GetVisualDevice(win_comm)->IsFocused(HasFocusedChildren);
}

OpRect WindowCommanderProxy::GetVisualDeviceVisibleRect(OpWindowCommander* win_comm)
{
	return GetVisualDevice(win_comm)->VisibleRect();
}

const uni_char * WindowCommanderProxy::GetSecurityStateText(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetSecurityStateText();
}

BOOL WindowCommanderProxy::CanViewSource(OpWindowCommander* win_comm, OpInputAction* child_action)
{
	/**
	   Enable/disable view source based on the same test used in OpenSourceViewer
	*/
	FramesDocument* doc = child_action->GetAction() == OpInputAction::ACTION_VIEW_FRAME_SOURCE ?
		GetActiveFrameDocument(win_comm, FALSE) :
		GetCurrentDoc(win_comm);

	if (!doc)
	{
		child_action->SetEnabled(FALSE);
		return TRUE;
	}

	URL url = doc->GetURLForViewSource();

	return g_application->CanViewSourceOfURL(&url);
}

BOOL WindowCommanderProxy::ViewSource(OpWindowCommander* win_comm, BOOL frame_source)
{
	FramesDocument* doc =
		(frame_source || GetType(win_comm) == WIN_TYPE_MAIL_VIEW)?
		GetActiveFrameDocument(win_comm, FALSE) :
		GetCurrentDoc(win_comm);

	if (!doc)
		return FALSE;

	// Add tests so that the right source is retrieved
	frame_source = frame_source || GetType(win_comm) != WIN_TYPE_NORMAL;

	URL url = doc->GetURLForViewSource();

	if (!url.GetAttribute(URL::KMovedToURL, TRUE).IsEmpty())
		/* URL has been redirected since it was parsed.  The parsed
		   source code is no longer available and cannot be displayed. */
		return TRUE;

	TempBuffer tmp_buff;
	DocumentDesktopWindow *active_doc_window = g_application->GetActiveDocumentDesktopWindow();
	g_application->OpenSourceViewer(&url, (active_doc_window ? active_doc_window->GetID() : 0), doc->Title(&tmp_buff), frame_source);

	return TRUE;
}

BOOL WindowCommanderProxy::CanEditSitePrefs(OpWindowCommander* win_comm)
{
	Window* window = GetWindow(win_comm);
	FramesDocument* doc = window->DocManager()->GetCurrentDoc();

	//Can only edit server preferences if there is a page loaded
	//and it's not generated by opera (opera:*)
	BOOL site_prefs_possible = ((doc) && (doc->Type() == DOC_FRAMES));
	if(doc)
	{
		URL url = doc->GetURL();
		URLType type = (URLType)url.GetAttribute(URL::KType);
		BOOL is_http = type == URL_HTTP || type == URL_HTTPS;
		site_prefs_possible &= is_http || !url.GetAttribute(URL::KIsGeneratedByOpera);
		return site_prefs_possible;
	}

	return FALSE;
}

void WindowCommanderProxy::ReloadFrame(OpWindowCommander* win_comm)
{
	if (HasActiveFrameDocument(win_comm))
		GetActiveFrameDocument(win_comm, FALSE)->GetDocManager()->Reload(WasEnteredByUser);
}

void WindowCommanderProxy::GoToParentDirectory(OpWindowCommander* win_comm)
{
	if(!GetCurrentURL(win_comm).IsEmpty() )
	{
		OpString address;
		if (OpStatus::IsSuccess(GetCurrentURL(win_comm).GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, address)) && address.HasContent())
		{
			// Let 'path' point to start of path in the url (exclude protocol and server)
			uni_char *path = address.CStr();
			const uni_char *sep = UNI_L("/");
			for( INT32 slash_count = 0; slash_count < 3 && *path; )
			{
				if (*path == sep[0] )
				{
					++ slash_count;
				}
				++ path;
			}

			OpString trimchars;
			trimchars.Set(UNI_L("\\/"));

			//	Get rid of one "subdirectory" and open url
			StrTrimRight(path, trimchars.CStr());
			if (*path && OpPathRemoveFileName(path))
			{
				// Remove any trailing slash or backslash
				StrTrimRight( path, trimchars.CStr());
				// Add back a trailing slash unless path is empty
				if (*path) uni_strcat(path, UNI_L("/"));

				GetWindow(win_comm)->OpenURL(address.CStr(), TRUE, TRUE);
			}
		}
	}
}


void WindowCommanderProxy::AddToBookmarks(OpWindowCommander* win_comm,
										  BookmarkItemData& item_data,
										  INT32 action_id,
										  INT32& id)
{
	if( HasCurrentDoc(win_comm) )
	{
		FramesDocument* doc = GetCurrentDoc(win_comm);

		item_data.name.Set( win_comm->GetCurrentTitle() );
		item_data.name.Strip();
		doc->GetURL().GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, item_data.url);

		GetDescription(win_comm, item_data.description);

		if( item_data.name.IsEmpty() )
		{
			item_data.name.Set( item_data.url );
		}
		else
		{
			// Bug #177155 (remove quotes)
			ReplaceEscapes( item_data.name.CStr(), item_data.name.Length(), TRUE );
		}
	}
	else if (action_id)
	{
		return;
	}

	id = g_hotlist_manager->GetFolderIdById(action_id);

	if( id == -1 && g_application->GetActiveBrowserDesktopWindow())
	{
		id = g_application->GetActiveBrowserDesktopWindow()->GetSelectedHotlistItemId(OpTypedObject::PANEL_TYPE_BOOKMARKS);
	}


	item_data.name.Strip();
}

OP_STATUS WindowCommanderProxy::GetDescription(OpWindowCommander* win_comm, OpString& description)
{
	FramesDocument* doc = GetCurrentDoc(win_comm);
	if (doc)
	{
		RETURN_IF_ERROR(doc->GetDescription(description));

		if( description.HasContent() )
		{
			// Bug #168584/#223272 (Bookmark descriptions do not display some Latin-1 characters
			RETURN_IF_ERROR(ReplaceEscapes( description.CStr(), description.Length(), TRUE ));
		}
	}

	return OpStatus::OK;
}

BOOL WindowCommanderProxy::SavePageInBookmark(OpWindowCommander* win_comm, BookmarkItemData& item_data)
{
	FramesDocument* doc = GetCurrentDoc(win_comm);

	if (doc)
	{
		item_data.name.Set( win_comm->GetCurrentTitle() );
		item_data.name.Strip();
		doc->GetURL().GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, item_data.url);

		WindowCommanderProxy::GetDescription(win_comm, item_data.description);

		if( item_data.name.IsEmpty() )
		{
			item_data.name.Set( item_data.url );
		}
		else
		{
			// Bug #177155 (remove quotes)
			ReplaceEscapes( item_data.name.CStr(), item_data.name.Length(), TRUE );
		}

		item_data.name.Strip();

		return TRUE;
	}

	return FALSE;
}

BOOL WindowCommanderProxy::OnInputAction(OpWindowCommander* win_comm, OpInputAction* action)
{
	return GetVisualDevice(win_comm)->OnInputAction(action);
}

BOOL WindowCommanderProxy::EnableImages(OpWindowCommander* win_comm, OpDocumentListener::ImageDisplayMode image_mode)
{
	if(GetPreviewMode(win_comm))
		TogglePrintMode(win_comm);

	if (win_comm->GetImageMode() != image_mode)
		win_comm->SetImageMode(image_mode);
	return TRUE;
}

BOOL WindowCommanderProxy::ShouldDisplaySecurityDialog(OpWindowCommander* win_comm)
{
	OpStringC server_name(win_comm->GetCurrentURL(FALSE));
	URLType url_type = URL_HTTP;

	if ( GetWindow(win_comm) )
		url_type = GetCurrentURL(win_comm).Type();

	if ( !server_name.IsEmpty() && ( (url_type == URL_HTTP) || (url_type == URL_HTTPS) || (url_type == URL_FTP)) )
		return TRUE;

	return FALSE;
}

void WindowCommanderProxy::InitDragObject(OpWindowCommander* win_comm, DesktopDragObject* drag_object, OpRect rect)
{
	FramesDocument* doc = GetWindow(win_comm)->DocManager()->GetCurrentDoc();

	if (doc && doc->Type() == DOC_FRAMES && ((FramesDocument*)doc)->GetActiveSubDoc())
		doc = ((FramesDocument*)doc)->GetActiveSubDoc();

	VisualDevice* vis_dev = doc->GetVisualDevice();

#ifdef VIEWPORTS_SUPPORT
	OpRect visual_viewport = win_comm->GetViewportController()->GetVisualViewport();

	rect.IntersectWith(visual_viewport);
	rect.x -= visual_viewport.x;
	rect.y -= visual_viewport.y;
#else
	UINT32 x, y;

	vis_dev->GetDocumentPos(&x, &y);

	rect.x -= x;
	rect.y -= y;

	rect.IntersectWith(OpRect(0, 0, vis_dev->Width(), vis_dev->Height()));
#endif // VIEWPORTS_SUPPORT
	rect = vis_dev->ScaleToScreen(rect);

	if (rect.width > 0 && rect.height > 0)
	{
		OpPainter* painter = vis_dev->GetView()->GetPainter(rect, TRUE);

		if (painter->Supports(OpPainter::SUPPORTS_GETSCREEN))
		{
			OpBitmap* bitmap = painter->CreateBitmapFromBackground(vis_dev->OffsetToContainer(rect));

			OpPoint point(rect.x , rect.y);

			point = vis_dev->GetView()->ConvertToScreen(point);

			drag_object->SetBitmap(bitmap);
			drag_object->SetBitmapPoint(point);
		}

		vis_dev->GetView()->ReleasePainter(rect);
	}
}

OP_STATUS WindowCommanderProxy::SetEncoding(OpWindowCommander* win_comm, const uni_char* charset)
{
	OpString8 charset8;
	if (charset)
		charset8.Set(charset);

	// Write out the site pref
	ServerName* server_name = GetServerName(win_comm);

	if (server_name)
	{
		OpString site_name;
		site_name.Set(server_name->UniName());

		RETURN_IF_LEAVE(g_pcdisplay->OverridePrefL(site_name.CStr(), PrefsCollectionDisplay::ForceEncoding, charset, TRUE));
	}

	if (charset)
	{
#ifdef _PRINT_SUPPORT_
		// Leave print preview
		if (GetPreviewMode(win_comm))
			TogglePrintMode(win_comm);
#endif // _PRINT_SUPPORT_
		// Set encoding
		GetWindow(win_comm)->SetForceEncoding(charset8.CStr());
	}

	return OpStatus::OK;
}

const URL WindowCommanderProxy::GetMovedToURL(OpWindowCommander* win_comm)
{
	if (!win_comm)
		return URL();

	DocumentManager* doc_man = GetWindow(win_comm)->DocManager();
	if(!doc_man)
		return URL();

	// Fix for bug 295558 - make sure we get the url that we are going to:

	URL moved_to = doc_man->GetCurrentURL().GetAttribute(URL::KMovedToURL, TRUE);

	if(!moved_to.IsEmpty())
		return moved_to;

	return doc_man->GetCurrentURL();
}

BOOL WindowCommanderProxy::HasConsistentRating(OpWindowCommander* win_comm)
{
	ServerName* name = WindowCommanderProxy::GetServerName(win_comm);

	if(!name)
		return TRUE;

	return (GetWindow(win_comm)->GetTrustRating() == name->GetTrustRating());
}

BOOL WindowCommanderProxy::IsPhishing(OpWindowCommander* win_comm)
{
	return (GetWindow(win_comm)->GetTrustRating() == Phishing);
}

BOOL WindowCommanderProxy::FraudListIsEmpty(OpWindowCommander* win_comm)
{
	ServerName* name = WindowCommanderProxy::GetServerName(win_comm);

	if(name)
		return name->FraudListIsEmpty();

	return TRUE;
}

void WindowCommanderProxy::ShowImageProperties(OpWindowCommander* win_comm, OpDocumentContext * ctx, BOOL is_background)
{
	if( ctx )
	{
		URL url;
		OpString img_alt, img_longdesc;
		if( is_background )
		{
			if (ctx->HasBGImage())
			{
				OpString image_address;
				ctx->GetBGImageAddress(&image_address);
				if (!image_address.IsEmpty())
				{
					url = g_url_api->GetURL(image_address.CStr(), ctx->GetBGImageAddressContext());
				}
				else if (ctx->HasMedia())
				{
					ctx->GetMediaAddress(&image_address);
					url = g_url_api->GetURL(image_address.CStr(), ctx->GetBGImageAddressContext());
				}
			}
		}
		else
		{
			if (ctx->HasImage())
			{
				OpString image_address;
				ctx->GetImageAddress(&image_address);
				if (!image_address.IsEmpty())
				{
					url = g_url_api->GetURL(image_address.CStr(), ctx->GetImageAddressContext());
				}
				ctx->GetImageAltText(&img_alt);
				OpString img_longdesc_input;
				ctx->GetImageLongDesc(&img_longdesc_input);
				if (img_longdesc_input.HasContent())
				{
					URL longdesc_URL = g_url_api->GetURL(win_comm->GetCurrentURL(TRUE), img_longdesc_input.CStr());
					longdesc_URL.GetAttribute(URL::KUniName_With_Fragment, img_longdesc, TRUE);
				}
			}
		}
		if( !url.IsEmpty() /*&& url.IsImage()*/ )
		{
			ImagePropertiesController* controller = OP_NEW(ImagePropertiesController, (url, img_alt, img_longdesc));
			ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow());  
		}
	}
}

void WindowCommanderProxy::ShowBackgroundImage(OpWindowCommander* win_comm)
{
	FramesDocument *doc = GetWindow(win_comm)->GetCurrentDoc();
	if (doc && doc->Type() == DOC_FRAMES && GetWindow(win_comm)->ShowImages())
	{
		LogicalDocument* logdoc = ((FramesDocument*)doc)->GetLogicalDocument();
		if (logdoc)
		{
			HTML_Element* body_he = logdoc->GetBodyElm();
			if (body_he && !body_he->GetImageURL().IsEmpty())
			{
				URL url = body_he->GetImageURL();
				if (doc->LoadInline(&url, body_he, BGIMAGE_INLINE) == OpStatus::OK)
				{
					WinState state = GetWindow(win_comm)->GetState();
					if (state == NOT_BUSY || state == RESERVED)
					{
						GetWindow(win_comm)->StartProgressDisplay(TRUE, FALSE); // Can fail with OOM
						GetWindow(win_comm)->SetState(BUSY);
						GetWindow(win_comm)->SetState(CLICKABLE);
					}
				}
			}
		}
	}
}

void WindowCommanderProxy::CreateLinkedWindow(OpWindowCommander* win_comm, BOOL is_keyboard_invoked)
{
	Window* window = 0;

	if(g_application->IsSDI())
	{
		BrowserDesktopWindow* bw = 0;
		DocumentDesktopWindow *dw = 0;
		bw = g_application->GetBrowserDesktopWindow(TRUE, FALSE, FALSE, NULL, NULL, 0, 0, TRUE, FALSE, is_keyboard_invoked);
		if( bw )
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_NEW_PAGE);
			dw = bw->GetActiveDocumentDesktopWindow();
			if( dw )
			{
				window = GetWindow(dw->GetWindowCommander());
			}
		}
	}
	else
	{
		BOOL3 yes = YES;
		window = windowManager->GetAWindow(TRUE, yes, yes);
	}

	if( window )
	{
		GetWindow(win_comm)->SetOutputAssociatedWindow( window );
	}
}

void WindowCommanderProxy::ToggleUserCSS(OpWindowCommander* win_comm, OpInputAction* child_action)
{
	CSSMODE css = GetWindow(win_comm)->GetCSSMode();
	BOOL user_css = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css, PrefsCollectionDisplay::EnableUserCSS));

	child_action->SetEnabled(user_css);

	UINT32 index = child_action->GetActionData();
	BOOL is_active = g_pcfiles->IsLocalCSSActive(index);

	child_action->SetSelectedByToggleAction(OpInputAction::ACTION_SELECT_USER_CSS_FILE, is_active);
}

void WindowCommanderProxy::SelectAlternateCSSFile(OpWindowCommander* win_comm, OpInputAction* child_action)
{
	OpWindowCommander* commander = (OpWindowCommander *) GetWindow(win_comm)->GetWindowCommander();

	UINT32 i;
	for (i = 0; ; i++)
	{
		CssInformation information;

		if (!commander->GetAlternateCssInformation(i, &information))
			break;

		if (child_action->GetActionDataString() &&
			information.title &&
			uni_strcmp(child_action->GetActionDataString(), information.title) == 0)
		{
			child_action->SetSelected(information.enabled);
		}
	}
}

// Don't ask me why their different they just are
BOOL WindowCommanderProxy::HandlePlatformAction(OpWindowCommander* win_comm, OpInputAction* action, BOOL is_document_desktop_window)
{
#ifdef MSWIN
	if(is_document_desktop_window)
		return FALSE;
	return HandleOldBrowserWin32Actions(action, GetWindow(win_comm));
#elif defined(_MACINTOSH_)
	if(is_document_desktop_window)
		return CMacSpecificActions::HandlePrint(action);
	return CMacSpecificActions::Handle(action, GetWindow(win_comm));
#else
	return FALSE;
#endif
}

BOOL WindowCommanderProxy::IsInStrictMode(OpWindowCommander* win_comm)
{
	DocumentManager* doc_manager = GetWindow(win_comm)->DocManager();
	if (doc_manager)
	{
		FramesDocument* document = doc_manager->GetCurrentDoc();
		if (document && document->Type()==DOC_FRAMES)
		{

			LogicalDocument* log_doc = ((FramesDocument*)document)->GetLogicalDocument();
			if (log_doc && log_doc->GetRoot())
			{
				HLDocProfile* hld_profile = log_doc->GetHLDocProfile();
				if (hld_profile)
				{
					return hld_profile->IsInStrictMode();
				}
			}
		}
	}

	return FALSE;
}

void WindowCommanderProxy::SetVisualDevSize(OpWindowCommander* win_comm, INT32 width, INT32 height)
{
	Window* window = GetWindow(win_comm);

#ifdef VIEWPORTS_SUPPORT
	window->HandleNewWindowSize(width, height);
#else
	//  Copied this right out of WindowListener::OnResize(...), but it seems to work...

	DocumentManager *docman = window->DocManager();

	window->VisualDev()->SetSize(width, height);
	docman->SetViewSize(window->VisualDev()->Width(), window->VisualDev()->Height());
	docman->CalculateVDSize();
#endif // VIEWPORTS_SUPPORT
}

void WindowCommanderProxy::CopySettingsToWindow(OpWindowCommander* win_comm, const OpenURLSetting& setting)
{
	Window* window = GetWindow(win_comm);
	OP_ASSERT(window);
	Window* src_window = GetWindow(setting.m_src_commander);

	if (src_window && window != src_window)
	{
		// Copy settings from opening window

		window->SetCSSMode(src_window->GetCSSMode());
		window->SetScale(src_window->GetScale());
		window->SetForceEncoding(src_window->GetForceEncoding());

		win_comm->SetImageMode(setting.m_src_commander->GetImageMode());

		if (!window->GetOpener())
			window->SetOpener(src_window, src_window->Id());
	} else if (setting.m_has_src_settings)
	{
		win_comm->SetCssMode(setting.m_src_css_mode);
		win_comm->SetScale(setting.m_src_scale);
		win_comm->ForceEncoding(setting.m_src_encoding);
		win_comm->SetImageMode(setting.m_src_image_mode);
	}
}

void WindowCommanderProxy::InitOpenURLSetting(OpenURLSetting& setting,
											  const OpStringC& url,
											  BOOL force_new_page,
											  BOOL force_background,
											  BOOL url_came_from_address_field,
											  DesktopWindow* target_window,
											  URL_CONTEXT_ID context_id,
											  BOOL ignore_modifier_keys)
{
	setting.m_address.Set( url );
	setting.m_new_window    = MAYBE;
	setting.m_new_page      = force_new_page ? YES : MAYBE;
	setting.m_in_background = force_background ? YES : MAYBE;
	setting.m_from_address_field = url_came_from_address_field;
	setting.m_save_address_to_history = url_came_from_address_field;
	setting.m_target_window = target_window;
	setting.m_context_id    = context_id;
	setting.m_ignore_modifier_keys = ignore_modifier_keys;

	if(!url_came_from_address_field)
	{
		BrowserDesktopWindow* browser = g_application->GetActiveBrowserDesktopWindow();
		if (browser)
		{
			DesktopWindow *active_win = browser->GetActiveDesktopWindow();
			if (active_win)
			{
				setting.m_src_commander = active_win->GetWindowCommander();
			}
		}
	}
}

void WindowCommanderProxy::SetURLCameFromAddressField(OpWindowCommander* win_comm, BOOL from_address_field)
{
	GetWindow(win_comm)->SetURLCameFromAddressField(from_address_field);
}

BOOL WindowCommanderProxy::GetURLCameFromAddressField(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetURLCameFromAddressField();
}

#define CREATE_SEARCH_MAGIC_COOKIE		UNI_L("jsdf345340p3r4fsdf")
#define CREATE_SEARCH_MAGIC_COOKIE_LEN	18

void WindowCommanderProxy::CreateSearch(OpWindowCommander* win_comm, DesktopWindow* desktop_window, const OpStringC& window_title)
{
	FramesDocument*	frames_doc	= GetCurrentDoc(win_comm);

	if(frames_doc)
	{
		HTML_Element *htm_elem = frames_doc->GetCurrentHTMLElement();
		HTML_Element *root_htm;
		HTML_Element *tmp_he = NULL;

		// handle IFRAMEs
		if(htm_elem == NULL)
		{
			frames_doc = GetWindow(win_comm)->GetActiveFrameDoc();
			if(frames_doc)
			{
				htm_elem = frames_doc->GetCurrentHTMLElement();
			}
		}
		root_htm = htm_elem;

		while(htm_elem != NULL)
		{
			tmp_he = root_htm->Parent();

			if(tmp_he)
			{
				root_htm = tmp_he;
			}
			else
			{
				break;
			}
		}
		if(htm_elem)
		{
			FormObject *form_obj = htm_elem->GetFormObject();

			if(form_obj)
			{
				OpString old_form_value;
				OpStatus::Ignore(form_obj->GetFormWidgetValue(old_form_value));

				form_obj->SetFormWidgetValue(CREATE_SEARCH_MAGIC_COOKIE);

				HTML_Element *submit_he = GetFormSubmit(frames_doc, htm_elem);
				HTML_Element *form_he = GetFormRoot(root_htm, htm_elem);

				Form form(frames_doc->GetURL(), form_he, submit_he, 0, 0);
				OP_STATUS s;
				OpString strquery;
				OpString8 tmpquery;

				URL url = form.GetURL(frames_doc, s);

				// we will check if it's a POST or GET further down
				OpStatus::Ignore(form.GetQueryString(tmpquery));

				strquery.Set(tmpquery);

				form_obj->SetFormWidgetValue(old_form_value.CStr());

				if(OpStatus::IsSuccess(s) && form_he)
				{
					OpString strurl;
					OpString title;
					OpString char_set;

					// Determine method (POST/GET/...)
					HTTP_Method method;
					if(submit_he && submit_he->HasAttr(ATTR_METHOD))
					{
						method = submit_he->GetMethod(); // Use the submit "button" instead
					}
					else
					{
						method = form_he->GetMethod();
					}
					url.GetAttribute(URL::KUniName_Username_Password_Hidden, strurl);

					if(strurl.Length())
					{
						OpString realurl;
						OpString realquery;
						INT32 cookie_pos = strurl.Find(CREATE_SEARCH_MAGIC_COOKIE);

						if(cookie_pos != -1)
						{
							realurl.Append(strurl.CStr(), cookie_pos);
							realurl.Append("%s");
							realurl.Append(strurl.SubString(cookie_pos + CREATE_SEARCH_MAGIC_COOKIE_LEN));
						}
						else
						{
							realurl.Append(strurl);
						}
						if(method == HTTP_METHOD_POST)
						{
							cookie_pos = strquery.Find(CREATE_SEARCH_MAGIC_COOKIE);

							if(cookie_pos != -1)
							{
								realquery.Append(strquery.CStr(), cookie_pos);
								realquery.Append("%s");
								realquery.Append(strquery.SubString(cookie_pos + CREATE_SEARCH_MAGIC_COOKIE_LEN));
							}
							else
							{
								realquery.Append(strquery);
							}
						}

						char_set.SetFromUTF8(GetMovedToURL(win_comm).GetAttribute(URL::KMIME_CharSet).CStr());

						if(char_set.IsEmpty())
						{
							char_set.Set(UNI_L("utf-8"));
						}

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
						SearchEngineItem *search_engine = OP_NEW(SearchEngineItem, ());

						if(search_engine)
						{
							EditSearchEngineDialog *dlg = OP_NEW(EditSearchEngineDialog, ());

							search_engine->SetURL(realurl);
							search_engine->SetIsPost(method == HTTP_METHOD_POST);
							if(method == HTTP_METHOD_POST)
							{
								search_engine->SetQuery(realquery);
							}
							title.Append(window_title.CStr());
							search_engine->SetName(title);

							if (dlg)
								dlg->Init(desktop_window, search_engine, char_set);
							else
							{
								OP_DELETE(search_engine);
							}
						}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
					}
				}
				return;
			}
		}
	}
}

void RecursivelyContentBlockElements(OpWindowCommander* win_comm,
									 FramesDocument *doc,
									 OpString& url_pattern,
									 BOOL block,
									 BOOL reload_inlines,
									 ContentBlockTreeModel& content_to_block)
{
	HTML_ElementType element_type;
	NS_Type ns_type;
	InlineResourceType inline_type = IMAGE_INLINE;

	if(!doc)
		return;

	// Check if blocking an iframe or main document itself
	URL url = doc->GetDocManager()->GetCurrentURL();
	if(!url.IsEmpty())
	{
		OpString str_url;
		if(OpStatus::IsSuccess(url.GetAttribute(URL::KUniName_Username_Password_Hidden, str_url))
			&& str_url.HasContent())
		{
			if(URLFilter::MatchUrlPattern(str_url.CStr(), url_pattern.CStr()))
			{
				// we are blocking
				if(block)
				{
					SimpleTreeModelItem *item = content_to_block.Find(url_pattern.CStr());

					if(!item)
						content_to_block.AddItem(url_pattern.CStr(), NULL, 0, -1, (void *)BLOCKED_EDITED);
				}
			}
		}
	}

	if(doc->GetDocRoot())
	{
		HTML_Element* html_element = (HTML_Element*) doc->GetDocRoot();

		while (html_element)
		{
			element_type = html_element->Type();
			ns_type = html_element->GetNsType();

			if(ns_type == NS_HTML && (element_type == HE_OBJECT || element_type == HE_EMBED ||
				element_type == HE_IMAGE || element_type == HE_IMG || element_type == HE_SCRIPT ||
				element_type == HE_IFRAME || element_type == HE_APPLET))
			{
				URL url;
				URL* img_url = NULL;
				OpString str_url;

				if(element_type == HE_SCRIPT)
				{
					img_url = (URL*) html_element->GetUrlAttr(ATTR_SRC, NS_IDX_HTML, doc->GetLogicalDocument());
					if(img_url)
					{
						img_url->GetAttribute(URL::KUniName_Username_Password_Hidden, str_url);
					}
					inline_type = SCRIPT_INLINE;
				}
				else if(element_type == HE_IFRAME)
				{
					img_url = (URL*) html_element->GetUrlAttr(ATTR_SRC, NS_IDX_HTML, doc->GetLogicalDocument());
					if(img_url)
					{
						img_url->GetAttribute(URL::KUniName_Username_Password_Hidden, str_url);
					}
					inline_type = INVALID_INLINE;
				}
				else if(element_type == HE_APPLET)
				{
					img_url = (URL*) html_element->GetUrlAttr(ATTR_SRC, NS_IDX_HTML, doc->GetLogicalDocument());
					if(img_url)
					{
						img_url->GetAttribute(URL::KUniName_Username_Password_Hidden, str_url);
					}
					inline_type = EMBED_INLINE;
				}
				else if(element_type == HE_EMBED)
				{
					img_url = html_element->GetEMBED_URL();

					if(img_url)
					{
						img_url->GetAttribute(URL::KUniName_Username_Password_Hidden, str_url);
					}
					inline_type = EMBED_INLINE;
				}
				else if (element_type == HE_OBJECT)
				{
					// get the DATA attribute. move down to next else {}
					url = html_element->GetImageURL(FALSE);

					if(!url.IsEmpty())
					{
						img_url = &url;

						img_url->GetAttribute(URL::KUniName_Username_Password_Hidden, str_url);

						inline_type = GENERIC_INLINE;
					}
					else
					{
						HTML_Element* child;

						for(child = html_element->FirstChild(); child; child = child->Suc())
						{
							if (child->IsMatchingType(HE_PARAM, NS_HTML))
							{
								const uni_char* param_name = child->GetPARAM_Name();
								if (param_name && uni_stri_eq(param_name, "movie"))
								{
									const uni_char* param_value = child->GetPARAM_Value();
									if (param_value)
									{
										str_url.Set(param_value);
										URL empty;
										url = g_url_api->GetURL(empty, str_url, TRUE);
										img_url = &url;
										inline_type = GENERIC_INLINE;
										break;
									}
								}
							}
							else if (child->IsMatchingType(HE_EMBED, NS_HTML))
							{
								url = html_element->GetImageURL(FALSE);

								img_url = &url;

								img_url->GetAttribute(URL::KUniName_Username_Password_Hidden, str_url);
								inline_type = EMBED_INLINE;
								break;
							}
						}
					}
				}
				else
				{
					url = html_element->GetImageURL(FALSE, doc->GetLogicalDocument());

					img_url = &url;

					img_url->GetAttribute(URL::KUniName_Username_Password_Hidden, str_url);
					inline_type = IMAGE_INLINE;
				}
				if(img_url && str_url.HasContent())
				{
					if(URLFilter::MatchUrlPattern(str_url.CStr(), url_pattern.CStr()))
					{
						if(reload_inlines)
						{
							Window *window = GetWindow(win_comm);

							if(window && window->DocManager())
							{
								if (!window->ShowImages())
								{
									window->SetImagesSetting(FIGS_SHOW);
								}
								if(inline_type == EMBED_INLINE || inline_type == IMAGE_INLINE || inline_type == GENERIC_INLINE)
								{
									if(OpStatus::IsSuccess(doc->LoadInline(img_url, html_element, inline_type)))
									{
										WinState state = window->GetState();
										if (state == NOT_BUSY || state == RESERVED)
										{
											window->SetState(BUSY);
											window->SetState(CLICKABLE);
										}
									}
								}
							}
						}
						// we are blocking
						if(block)
						{
							SimpleTreeModelItem *item = content_to_block.Find(url_pattern.CStr());

							if(!item)
							{
								content_to_block.AddItem(url_pattern.CStr(), NULL, 0, -1, (void *)BLOCKED_EDITED);
							}
							doc->AddToURLBlockedList(*img_url);
#if 0
#if defined(_DEBUG) && defined(_WINDOWS_)
							uni_char tmp[5000];

							uni_sprintf(tmp, UNI_L("- blocking url: %s\n"), str_url);
							OutputDebugString(tmp);
#endif
#endif
						}
						else
						{
							// we have a match in the document, we need to do something about it
							doc->RemoveFromURLBlockedList(*img_url);
#if 0
#if defined(_DEBUG) && defined(_WINDOWS_)
							uni_char tmp[5000];

							uni_sprintf(tmp, UNI_L("- unblocking url: %s\n"), str_url);
							OutputDebugString(tmp);
#endif
#endif
						}
						if(element_type == HE_EMBED || element_type == HE_OBJECT)
						{
							Plugin* plugin = (Plugin *)html_element->GetNS4Plugin();
							if(plugin && plugin->GetPluginWindow())
							{
								doc->GetVisualDevice()->ShowPluginWindow((OpPluginWindow *)plugin->GetPluginWindow(), block ? FALSE : TRUE);
							}
						}
						html_element->MarkExtraDirty(doc);
					}
				}
			}
			html_element = (HTML_Element*) html_element->Next();
		}
	}
}


OP_STATUS BlockAllContentFromPattern(OpWindowCommander* win_comm,
									 FramesDocument *document,
									 OpString& url_pattern,
									 BOOL block,
									 BOOL reload_inlines,
									 ContentBlockTreeModel& content_to_block)
{
	if(document)
	{
		document = document->GetTopDocument();
	}
	if(!document)
	{
		return OpStatus::ERR_NULL_POINTER;
	}
	DocumentTreeIterator doc_tree(document->GetDocManager());

	doc_tree.SetIncludeThis();

	while(doc_tree.Next())
	{
#ifdef CF_CAP_HAS_CONTENT_BLOCK_FILTER_CREATION
# ifdef CF_CAP_HAS_APPLY_BLOCKED_ELEMENTS
		RETURN_IF_ERROR(ContentBlockFilterCreation::ApplyBlockedElementsToDocument(content_to_block, doc_tree.GetFramesDocument(), url_pattern, block, reload_inlines));
# else
		RecursivelyContentBlockElements(win_comm, doc_tree.GetFramesDocument(), url_pattern, block, reload_inlines, content_to_block);
# endif // CF_CAP_HAS_APPLY_BLOCKED_ELEMENTS
#else
		RecursivelyContentBlockElements(win_comm, doc_tree.GetFramesDocument(), url_pattern, block, reload_inlines, content_to_block);
#endif // CF_CAP_HAS_CONTENT_BLOCK_FILTER_CREATION
	}
	return OpStatus::OK;
}


void WindowCommanderProxy::ToggleAdblock(OpWindowCommander* win_comm,
										 OpInfobar*& adblock_bar,
										 BOOL& adblock_mode,
										 ContentBlockTreeModel& content_to_block)
{
	OpVector<OpString> list;
	adblock_mode = adblock_mode ? FALSE : TRUE;

	if(HasCurrentDoc(win_comm))
	{
		GetWindow(win_comm)->SetContentBlockEditMode(adblock_mode);
		if(adblock_mode)
		{
			adblock_bar->Show();
		}
		else
		{
			adblock_bar->Hide();
		}
		GetCurrentDoc(win_comm)->Reflow(TRUE, TRUE, TRUE);
	}

	if(adblock_mode)
	{
		OpString url_pattern;
		URL tmpurl = GetMovedToURL(win_comm);

		SetContentBlockEnabled(win_comm, FALSE);

		// not fatal if it fails
		OpStatus::Ignore(g_urlfilter->CreateExcludeList(list));

		UINT32 cnt;

		for(cnt = 0; cnt < list.GetCount(); cnt++)
		{
			OpString *url_str = list.Get(cnt);
			uni_char *url = url_str->CStr();
			if(url)
			{
				// it's an exact address it seems, do an exact block
				BlockAllContentFromPattern(win_comm, GetCurrentDoc(win_comm), *url_str, TRUE, TRUE, content_to_block);
			}
			OP_DELETE(url_str);
		}
	}
	else
	{
		SetContentBlockEnabled(win_comm, TRUE);

		// not fatal if it fails
		OpStatus::Ignore(g_urlfilter->CreateExcludeList(list));

		// we need to block inlines even if we exit the content blocker mode
		UINT32 cnt;

		for(cnt = 0; cnt < list.GetCount(); cnt++)
		{
			OpString *url_str = list.Get(cnt);
			uni_char *url = url_str->CStr();
			if(url)
			{
				// it's an exact address it seems, do an exact block
				BlockAllContentFromPattern(win_comm, GetCurrentDoc(win_comm), *url_str, TRUE, TRUE, content_to_block);
			}
			OP_DELETE(url_str);
		}
	}
}

void WindowCommanderProxy::AdblockEditDone(OpWindowCommander* win_comm,
										   OpInfobar*& adblock_bar,
										   BOOL& adblock_mode,
										   BOOL accept,
										   ContentBlockTreeModel& content_to_block,
										   ContentBlockTreeModel& content_to_unblock)
{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
	UINT32 cnt;
#else
	INT32 cnt;
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL

	FramesDocument* doc = GetCurrentDoc(win_comm);

	adblock_mode = adblock_mode ? FALSE : TRUE;

	SetContentBlockEnabled(win_comm, TRUE);

	if(doc)
	{
		if(doc->Type() == DOC_FRAMES)
		{
			GetWindow(win_comm)->SetContentBlockEditMode(adblock_mode);
			if(adblock_mode)
			{
				adblock_bar->Show();
			}
			else
			{
				adblock_bar->Hide();
			}
			doc->Reflow(TRUE, TRUE, TRUE);
		}
	}
	if(accept)
	{
		if(content_to_block.GetCount())
		{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
			OpVector<ContentFilterItem> pattern_content;
#else
			SimpleTreeModel pattern_content;
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
			ContentBlockFilterCreation filters(&content_to_block, GetMovedToURL(win_comm));

			if(OpStatus::IsSuccess(filters.GetCreatedFilterURLs(pattern_content)))
			{
				for(cnt = 0; cnt < pattern_content.GetCount(); cnt++)
				{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
					ContentFilterItem *item = pattern_content.Get(cnt);
#else
					const uni_char *url = pattern_content.GetItemString(cnt);
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL

#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
					g_urlfilter->AddURLString(item->GetURL(), TRUE, NULL);
#else
					g_urlfilter->AddURLString(url, TRUE, NULL);
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
				}
			}
			pattern_content.DeleteAll();
		}
		if(content_to_unblock.GetCount())
		{
			for(cnt = 0; cnt < content_to_unblock.GetCount(); cnt++)
			{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
				ContentFilterItem *item = content_to_unblock.Get(cnt);
				const uni_char *url_string = item->GetURL();
#else
				const uni_char *url_string = content_to_unblock.GetItemString(cnt);
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL

				if(url_string)
				{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
					UINT32 i;
					BOOL match = FALSE;

					for(i = 0; i < content_to_block.GetCount(); i++)
					{
						ContentFilterItem *item = content_to_block.Get(i);
						if(!item->GetURL().Compare(url_string))
						{
							match = TRUE;
							break;
						}
					}
					if(!match)
#else
						SimpleTreeModelItem *item = content_to_block.Find(url_string);

					if(!item)
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
					{
						OpString url_pattern;
						URL url = GetMovedToURL(win_comm);

						if(OpStatus::IsSuccess(ContentBlockFilterCreation::CreateFilterFromURL(url, url_string, url_pattern)))
						{
							FilterURLnode node;

							node.SetIsExclude(TRUE);
							node.SetURL(url_pattern.CStr());

							// we didn't find the item in the list, we need to get rid of it from the urlfilter
							// as it's an "old" url that was not added in this session
							g_urlfilter->DeleteURL(&node);
						}
					}
				}
			}
		}
		g_urlfilter->WriteL();
	}
	else
	{
		OpVector<OpString> list;

		// not fatal if it fails
		OpStatus::Ignore(g_urlfilter->CreateExcludeList(list));

		// we need to block inlines even if we exit the content blocker mode
		UINT32 cnt;

		for(cnt = 0; cnt < list.GetCount(); cnt++)
		{
			OpString *url_str = list.Get(cnt);
			uni_char *url = url_str->CStr();
			if(url)
			{
				// it's an exact address it seems, do an exact block
				BlockAllContentFromPattern(win_comm, GetCurrentDoc(win_comm), *url_str, TRUE, TRUE, content_to_block);
			}
			OP_DELETE(url_str);
		}
	}
	content_to_block.DeleteAll();
	content_to_unblock.DeleteAll();

	if(!doc)
	{
		adblock_bar->Hide();
		return;
	}
	FramesDocument *document = doc->GetTopDocument();
	DocumentTreeIterator doc_tree(document->GetDocManager());

	doc_tree.SetIncludeThis();

	while(doc_tree.Next())
	{
		if(doc_tree.GetFramesDocument())
		{
			doc_tree.GetFramesDocument()->ClearURLBlockedList(accept);
		}
	}
}

void WindowCommanderProxy::AdBlockPattern(OpWindowCommander* win_comm,
										  OpString& url_pattern,
										  BOOL block,
										  ContentBlockTreeModel& content_to_block)
{
	BlockAllContentFromPattern(win_comm, GetCurrentDoc(win_comm), url_pattern, block, FALSE, content_to_block);
}


void WindowCommanderProxy::OnSessionReadL(OpWindowCommander* win_comm,
										  DocumentDesktopWindow* document_window,
										  const OpSessionWindow* session_window,
										  BOOL& focus_document)
{
	//History retrieval
	Window* window = GetWindow(win_comm);
	DocumentManager* doc_man = window->DocManager();

	OpAutoVector<OpString> history_url_list; ANCHOR(OpAutoVector<OpString>, history_url_list);
	OpAutoVector<UINT32> history_doctype_list; ANCHOR(OpAutoVector<UINT32>, history_doctype_list);
	OpAutoVector<OpString> history_title_list; ANCHOR(OpAutoVector<OpString>, history_title_list);
	OpAutoVector<UINT32> history_scrollpos_list; ANCHOR(OpAutoVector<UINT32>, history_scrollpos_list);
#ifdef DOCHAND_HISTORY_SAVE_ZOOM_LEVEL
	OpAutoVector<UINT32> history_scale_list; ANCHOR(OpAutoVector<UINT32>, history_scale_list);
#endif // DOCHAND_HISTORY_SAVE_ZOOM_LEVEL

	UINT32 current_history = session_window->GetValueL("current history");
	UINT32 max_history = session_window->GetValueL("max history");

	if (current_history || max_history)
	{
		window->SetMaxHistoryPos(max_history);

		int history_min, history_max;
		win_comm->GetHistoryRange(&history_min, &history_max);
		OP_ASSERT(history_min <= history_max || !"If max < min, then I expect this to be a core-bug. That should be reported, investigated and fixed!");

		int history_pos = current_history;
		if (current_history > max_history)
			history_pos = max_history;
		if (history_pos < history_min)
			history_pos = history_min;

		win_comm->SetCurrentHistoryPos(history_pos);
	}

	session_window->GetStringArrayL("history url", history_url_list);
	session_window->GetValueArrayL("history document type", history_doctype_list);
	session_window->GetStringArrayL("history title", history_title_list);
	session_window->GetValueArrayL("history scrollpos list", history_scrollpos_list);
#ifdef DOCHAND_HISTORY_SAVE_ZOOM_LEVEL
	session_window->GetValueArrayL("history scale list", history_scale_list);
#endif // DOCHAND_HISTORY_SAVE_ZOOM_LEVEL

	

	for(UINT32 i = 0; i < max_history; i++)
	{
		OpString* tempstr = history_url_list.Get(i);
		if (!tempstr) continue;

		uni_char* url_name = tempstr->CStr();
#ifdef WEB_TURBO_MODE
		URL_CONTEXT_ID ctx = 0; // Standard URL context

		if( window->GetTurboMode() )
			ctx = g_windowManager->GetTurboModeContextId();

		URL url = g_url_api->GetURL(url_name, ctx);
#else // WEB_TURBO_MODE
		URL url = g_url_api->GetURL(url_name);
#endif // !WEB_TURBO_MODE

#ifdef PASS_INVALID_URLS_AROUND
		const uni_char* url_invalid_string = 0;
		if (tempstr->Compare(UNI_L("opera:illegal-url-"), strlen("opera:illegal-url-")) == 0)
			url_invalid_string = url_name;
#endif // PASS_INVALID_URLS_AROUND

		UINT32* dtype = history_doctype_list.Get(i);
		if (!dtype) continue;

		tempstr = history_title_list.Get(i);
		uni_char* title = tempstr ? tempstr->CStr() : NULL;

		if(doc_man)
		{
			if (DocumentView::GetType(url_name) != DocumentView::DOCUMENT_TYPE_BROWSER)
				OpStatus::Ignore(url.SetAttribute(URL::KUnique, TRUE));

			const DocumentReferrer dummy;

			UINT32 scale = 100;
#ifdef DOCHAND_HISTORY_SAVE_ZOOM_LEVEL
			UINT32 *pscale = history_scale_list.Get(i);
			if (pscale)
				scale = *pscale;
#endif // DOCHAND_HISTORY_SAVE_ZOOM_LEVEL

			doc_man->AddNewHistoryPosition(url, dummy, i+1, title, FALSE, *dtype == 7 /* plugin doc */, NULL, scale);

		}
		UINT32* scrpos = history_scrollpos_list.Get(i);
		//update scroll position of documents in history
		if(doc_man && scrpos)
		{
			DocListElm *dle = doc_man->FirstDocListElm();
			if(dle)
			{
				while(dle->Suc())
					dle = dle->Suc();

#ifdef VIEWPORTS_SUPPORT
				dle->SetVisualViewport(OpRect(0, *scrpos, 0, 0));
#else
				dle->SetScrollPos(dle->GetScrollPosX(), *scrpos);
#endif // VIEWPORTS_SUPPORT
			}
		}
	}

	bool active = session_window->GetValueL("active") == 1;

	OpWindow::State state = (OpWindow::State) session_window->GetValueL("state");

	if(current_history)
	{
		//Open page at current history position
		focus_document = TRUE;
		if( state != OpWindow::MINIMIZED )
			document_window->GetBrowserView()->SetFocus(FOCUS_REASON_OTHER);

#ifdef WEBSERVER_SUPPORT
		OpString* url_str = history_url_list.Get(0);
		if (url_str && g_webserver_manager->URLNeedsDelayForWebServer(*url_str))
		{
			if (OpStatus::IsError(g_webserver_manager->AddAutoLoadWindow(window->Id(), *url_str)))
			{
				// Lock session writing from now until when this page have been handled.
				// Writing the session before it's handled would result in dataloss.
				g_session_auto_save_manager->LockSessionWriting();
				g_main_message_handler->PostDelayedMessage(MSG_GETPAGE, (MH_PARAM_1)window->Id(), STARTUP_HISTORY, 3000);
				g_main_message_handler->PostDelayedMessage(MSG_QUICK_UNLOCK_SESSION_WRITING, 0, 0, 3000);
			}
		}
		else
#endif
		{
			// Lock session writing from now until when this page have been handled.
			// Writing the session before it's handled would result in dataloss.
			g_session_auto_save_manager->LockSessionWriting();
			using namespace WindowCommanderProxyConsts;

			int delay = PAGE_LOADING_DELAY;
			if (active)
				delay = ACTIVE_PAGE_LOADING_DELAY;

			g_main_message_handler->PostDelayedMessage(MSG_GETPAGE, (MH_PARAM_1)window->Id(), STARTUP_HISTORY, delay);
			g_main_message_handler->PostDelayedMessage(MSG_QUICK_UNLOCK_SESSION_WRITING, 0, 0, delay);
		}
	}

	//Encoding
	OpString encoding; ANCHOR(OpString, encoding);
	encoding.Set(session_window->GetStringL("encoding"));

	//Images
	win_comm->SetImageMode(static_cast<OpDocumentListener::ImageDisplayMode>(session_window->GetValueL("image mode")));

	//Reload every menu...
	window->GetUserAutoReload()->SetParams(session_window->GetValueL("user auto reload enable"),
										   session_window->GetValueL("user auto reload sec user setting"),
									       session_window->GetValueL("user auto reload only if expired"));
	//Linked windows
	window->SetOutputAssociatedWindowId(session_window->GetValueL("output associated window"));

	//Various issues
	window->SetCSSMode((CSSMODE)session_window->GetValueL("CSS mode"));
	win_comm->SetScale(session_window->GetValueL("scale"));
	window->SetShowScrollbars(session_window->GetValueL("show scrollbars"));

	// This is hardcoded in 7.6 to avoid users from manually entering ERA mode
	// The 32000 value MUST NOT BE SPREAD OUTSIDE OPERA HQ because of patenting
	int saved_layout_mode = session_window->GetValueL("handheld mode");
	OpWindowCommander::LayoutMode layout_mode;
	layout_mode = (OpWindowCommander::LayoutMode)saved_layout_mode;
	win_comm->SetLayoutMode(layout_mode);
	// End of 7.6-block. In 8.0 we can use the saved value directly with SetLayoutMode
}


void WindowCommanderProxy::OnSessionWriteL(OpWindowCommander* win_comm,
										   DocumentDesktopWindow* document_window,
										   OpSession* session,
										   OpSessionWindow* sessionwindow,
										   BOOL shutdown)
{
	sessionwindow->SetTypeL(OpSessionWindow::DOCUMENT_WINDOW);

	//Save history
	OpAutoVector<OpString> history_url_list; ANCHOR(OpAutoVector<OpString>, history_url_list);
	OpAutoVector<UINT32> history_doctype_list; ANCHOR(OpAutoVector<UINT32>, history_doctype_list);
	OpAutoVector<OpString> history_title_list; ANCHOR(OpAutoVector<OpString>, history_title_list);
	OpAutoVector<UINT32> history_scrollpos_list; ANCHOR(OpAutoVector<UINT32>, history_scrollpos_list);
#ifdef DOCHAND_HISTORY_SAVE_ZOOM_LEVEL
	OpAutoVector<UINT32> history_scale_list; ANCHOR(OpAutoVector<UINT32>, history_scale_list);
#endif // DOCHAND_HISTORY_SAVE_ZOOM_LEVEL

	//this one is _really_ ugly, let's find a better solution soon!

	Window* window = GetWindow(win_comm);
	DocumentManager* doc_man = window->DocManager();
	DocListElm* doc_elm = doc_man->FirstDocListElm();

	//Set window id
	sessionwindow->SetValueL("window id", window->Id());

	short numValidHistoryElement = 0;

	while (doc_elm)
	{
		FramesDocument* doc = (FramesDocument *)doc_elm->Doc();
		if (doc)
		{
			URL url = doc->GetURL();
			if( url.GetAttribute(URL::KHavePassword)
				|| url.GetAttribute(URL::KHaveAuthentication)
#ifdef _SSL_USE_SMARTCARD_
				|| url.GetHaveSmartCardAuthentication()
#endif
				)
			{
				if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::SavePasswordProtectedPages))
				{
					break;
				}
			}


			URLType	urltype = url.Type();

			if( !url.IsEmpty() &&
				urltype != URL_EMAIL && urltype != URL_ATTACHMENT &&
				urltype != URL_ATTACHMENT && urltype != URL_NEWSATTACHMENT &&
				urltype != URL_SNEWSATTACHMENT && urltype != URL_CONTENT_ID)
			{
				// make sure undo sessions can't reopen banking history or similar
				if (urltype == URL_HTTPS &&
#ifdef APPLICATION_CACHE_SUPPORT
					!url.GetAttribute(URL::KIsApplicationCacheURL) &&
#endif // APPLICATION_CACHE_SUPPORT
					document_window->GetParentDesktopWindow() &&
					((BrowserDesktopWindow*)document_window->GetParentDesktopWindow())->GetUndoSession() == session)
				{
					url.Unload(); // we allow reopening of HTTPS, but not from cache
				}
				OpString* urlname = OP_NEW_L(OpString, ());
				OpString url_top;
				url.GetAttribute(URL::KUniName_Username_Password_Hidden, url_top);
				const uni_char* url_rel = url.UniRelName();

				{	// AutoPtr scope
					OpStackAutoPtr<OpString> ap_urlname(urlname);
					{	// AutoPtr scope
						uni_char* full_tag = Window::ComposeLinkInformation(url_top.CStr(), url_rel);
						ANCHOR_ARRAY(uni_char, full_tag);
						urlname->SetL(full_tag);
					}
					LEAVE_IF_ERROR(history_url_list.Add(urlname));
					ap_urlname.release();
				}

				UINT32* doc_type = OP_NEW_L(UINT32, ());
				{	// AutoPtr scope
					OpStackAutoPtr<UINT32> ap_doc_type(doc_type);

					// historic values used in session files
					*doc_type = (doc->Type() == DOC_FRAMES && doc->IsPluginDoc()) ? 7 /* plugin doc */ : 6 /* normal doc */;

					LEAVE_IF_ERROR(history_doctype_list.Add(doc_type));
					ap_doc_type.release();
				}

				OpString* title = OP_NEW_L(OpString, ());
				{	// AutoPtr scope
					OpStackAutoPtr<OpString> ap_title(title);
					title->SetL(doc_elm->Title());
					if (title->IsEmpty())
						title->SetL(*urlname);

					LEAVE_IF_ERROR(history_title_list.Add(title));
					ap_title.release();
				}
				numValidHistoryElement++ ;

				//save scroll position of documents
				UINT32* scroll_pos = OP_NEW_L(UINT32, ());
				OpStackAutoPtr<UINT32> ap_scroll_pos(scroll_pos);

#ifdef VIEWPORTS_SUPPORT
				if (doc_elm->HasVisualViewport())
					*scroll_pos = doc_elm->GetVisualViewport().y;
				else
					*scroll_pos = win_comm->GetViewportController()->GetVisualViewport().y;
#else
				*scroll_pos = doc->GetViewY();
#endif // VIEWPORTS_SUPPORT
				LEAVE_IF_ERROR(history_scrollpos_list.Add(scroll_pos));
				ap_scroll_pos.release();

#ifdef DOCHAND_HISTORY_SAVE_ZOOM_LEVEL
				// save scale (zoom) value of document
				OpStackAutoPtr<UINT32> scale(OP_NEW_L(UINT32, (doc_elm->GetLastScale())));
				LEAVE_IF_ERROR(history_scale_list.Add(scale.get()));
				scale.release();
#endif // DOCHAND_HISTORY_SAVE_ZOOM_LEVEL

			}
		}
		doc_elm = doc_elm->Suc();
	}


	short historyLen = numValidHistoryElement;
	short currentPos = window->GetCurrentHistoryNumber();

	if( currentPos > historyLen )
		currentPos = historyLen > 0 ? historyLen/*-1*/ : 0;

	sessionwindow->SetValueL("max history", historyLen);
	sessionwindow->SetValueL("current history", currentPos);

	sessionwindow->ClearStringArrayL();
	sessionwindow->SetStringArrayL("history url", history_url_list);
	sessionwindow->SetValueArrayL("history document type", history_doctype_list);
	sessionwindow->SetStringArrayL("history title", history_title_list);
	sessionwindow->SetValueArrayL("history scrollpos list", history_scrollpos_list);
#ifdef DOCHAND_HISTORY_SAVE_ZOOM_LEVEL
	sessionwindow->SetValueArrayL("history scale list", history_scale_list);
#endif // DOCHAND_HISTORY_SAVE_ZOOM_LEVEL

	//Encoding
	OpString encoding; ANCHOR(OpString, encoding);
	encoding.Set(window->GetForceEncoding());
	sessionwindow->SetStringL("encoding", encoding);

	//Images
	sessionwindow->SetValueL("image mode", win_comm->GetImageMode());

	//Reload every menu...
	sessionwindow->SetValueL("user auto reload enable", window->GetUserAutoReload()->GetOptIsEnabled());
	sessionwindow->SetValueL("user auto reload sec user setting", window->GetUserAutoReload()->GetOptSecUserSetting());
	sessionwindow->SetValueL("user auto reload only if expired", window->GetUserAutoReload()->GetOptOnlyIfExpired());

	//Linked windows
	Window* oawindow = window->GetOutputAssociatedWindow();
	sessionwindow->SetValueL("output associated window", oawindow? oawindow->Id(): 0);

	//Various issues
	sessionwindow->SetValueL("CSS mode", window->GetCSSMode());
	sessionwindow->SetValueL("handheld mode", window->GetERA_Mode() ? OpWindowCommander::ERA : win_comm->GetLayoutMode());
	sessionwindow->SetValueL("scale", win_comm->GetScale());
	sessionwindow->SetValueL("show scrollbars", window->ShowScrollbars());
}

AutoWinReloadParam*	WindowCommanderProxy::GetUserAutoReload(OpWindowCommander* win_comm)
{
	return GetWindow(win_comm)->GetUserAutoReload();
}

// Needed to go forward from the speeddial / placeholder for the webpage
void WindowCommanderProxy::GotoHistoryPos(OpWindowCommander* win_comm)
{
	GetWindow(win_comm)->GotoHistoryPos();
}

void WindowCommanderProxy::UnloadCurrentDoc(OpWindowCommander* win_comm)
{
	GetWindow(win_comm)->DocManager()->UnloadCurrentDoc();
}

#ifdef _VALIDATION_SUPPORT_
/** Uploads a html file to an external server for validation.
  * @author Johan H. Borg
  */
void WindowCommanderProxy::UploadFileForValidation(OpWindowCommander* win_comm)
{
	FramesDocument *active_doc = GetWindow(win_comm)->GetActiveFrameDoc();
	if (active_doc)
	{
//		DocumentManager* doc_manager = active_doc->GetDocManager();
		URL url = active_doc->GetURL();

		// Create local file if necessary (no_cache, no_store set by httpd)
#ifdef URL_CAP_PREPAREVIEW_FORCE_TO_FILE
		url.PrepareForViewing(URL::KFollowRedirect, TRUE, TRUE, TRUE);
#else // URL_CAP_PREPAREVIEW_FORCE_TO_FILE
		url.PrepareForViewing(TRUE);
#endif

		OpStringC validation_url = g_pcui->GetStringPref(PrefsCollectionUI::ValidationURL);
		URL form_url = g_url_api->GetURL(validation_url.CStr(), UNI_L(""), true);
		form_url.SetHTTP_Method(HTTP_METHOD_POST);

		Upload_Multipart *form = OP_NEW(Upload_Multipart, ());
		if (form)
		{
			TRAPD(op_err, form->InitL("multipart/form-data"));
			OpStatus::Ignore(op_err);

			OpStringC form_element = g_pcui->GetStringPref(PrefsCollectionUI::ValidationForm);
			Upload_URL *page = OP_NEW(Upload_URL, ());
			if (page)
			{
				TRAP(op_err, page->InitL(url, NULL, "form-data", make_singlebyte_in_tempbuffer(form_element.CStr(), form_element.Length()), NULL, ENCODING_NONE));
				OpStatus::Ignore(op_err); // FIXME: OOM
				TRAP(op_err, page->SetCharsetL(url.GetAttribute(URL::KMIME_CharSet)));
				OpStatus::Ignore(op_err); // FIXME: OOM

				form->AddElement(page);

				TRAP(op_err, form->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION));
				OpStatus::Ignore(op_err); // FIXME: OOM

				form_url.SetHTTP_Data(form, TRUE);
				URL url2 = URL();

				BOOL3 open_in_new_window = YES; //Perhaps MAYBE?
				BOOL3 open_in_background = MAYBE;
				Window *validation_window;
				validation_window = windowManager->GetAWindow(TRUE, open_in_new_window, open_in_background);
				validation_window->OpenURL(form_url, DocumentReferrer(url2), true, FALSE, -1, true);
			}
			else
				OP_DELETE(form);
		}
	}
}
#endif // _VALIDATION_SUPPORT_

const uni_char*	WindowCommanderProxy::GetWindowTitle(OpWindowCommander* win_comm)
{
	return win_comm->GetCurrentTitle();
}

#ifdef SUPPORT_VISUAL_ADBLOCK
OP_STATUS WindowCommanderProxy::CheckURL(const uni_char* url, BOOL& load, OpWindowCommander* win_comm)
{
	return g_urlfilter->CheckURL(url, load, GetWindow(win_comm));
}
#endif // SUPPORT_VISUAL_ADBLOCK

OP_STATUS WindowCommanderProxy::GetThumbnailForWindow(OpWindowCommander* win_comm, Image &thumbnail, BOOL force_rebuild)
{
	return g_thumbnail_manager->GetThumbnailForWindow(GetWindow(win_comm), thumbnail, force_rebuild);
}

BOOL WindowCommanderProxy::CloneWindow(OpWindowCommander* win_comm, Window**return_window)
{
	return GetWindow(win_comm)->CloneWindow(return_window);
}

OP_STATUS WindowCommanderProxy::GetLoginPassword(OpWindowCommander* win_comm,
												 WandLogin* wand_login,
												 WandLoginCallback* callback)
{
	return g_wand_manager->GetLoginPassword(GetWindow(win_comm), wand_login, callback);
}

OP_STATUS WindowCommanderProxy::StoreLogin(OpWindowCommander* win_comm,
										   const uni_char* id,
										   const uni_char* username,
										   const uni_char* password)
{
	return g_wand_manager->StoreLogin(GetWindow(win_comm), id, username, password);
}

OpWindowCommander* WindowCommanderProxy::GetActiveWindowCommander()
{
#ifdef EMBROWSER_SUPPORT
	if (g_application->IsEmBrowser())
		return GetActiveEmBrowserWindowCommander();
#endif // EMBROWSER_SUPPORT

	DesktopWindow* desktop_window = g_application->GetActiveDesktopWindow(FALSE);

	if (desktop_window && desktop_window->GetWindowCommander())
		return desktop_window->GetWindowCommander();

	return NULL;
}

Window* GetActiveCoreWindow()
{
	return WindowCommanderProxy::GetActiveWindowCommander() ?
		GetWindow(WindowCommanderProxy::GetActiveWindowCommander()) : NULL;
}

void WindowCommanderProxy::StopLoadingActiveWindow()
{
	Window* window = GetActiveCoreWindow();

	if (window)
	{
		DocumentManager* doc_man = window->DocManager();

		if (doc_man)
		{
			if(doc_man->GetLoadStatus() != NOT_LOADING)
			{
				doc_man->StopLoading(FALSE);
			}
		}
	}
}

OP_STATUS WindowCommanderProxy::SavePictureToFolder(OpWindowCommander* win_comm,  OpDocumentContext * ctx, OpString& destination_filename, OpFileFolder folder)
{
	if(win_comm && ctx)
	{
		URL url;
		OpString address;
		if (ctx->HasImage())
		{
			ctx->GetImageAddress(&address);

			url = urlManager->GetURL(address);
		}
		if(!url.IsEmpty())
		{
			OpString suggested_filename;

			RETURN_IF_LEAVE(url.GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, TRUE));

			if (suggested_filename.IsEmpty())
			{
				RETURN_IF_ERROR(suggested_filename.Set("default"));

				OpString ext;
				TRAPD(op_err, url.GetAttributeL(URL::KSuggestedFileNameExtension_L, ext, TRUE));
				RETURN_IF_ERROR(op_err);
				if (ext.HasContent())
				{
					RETURN_IF_ERROR(suggested_filename.Append("."));
					RETURN_IF_ERROR(suggested_filename.Append(ext));
				}
				if (suggested_filename.IsEmpty())
				{
					return FALSE;
				}
			}
			OpString directory;

			// put it into the user's profile picture directory
			OpString tmp_storage;
			directory.Set(g_folder_manager->GetFolderPathIgnoreErrors(folder, tmp_storage));

			// Build unique file name
			OpString filename;
			RETURN_IF_ERROR(filename.Set(directory));
			if( filename.HasContent() && filename[filename.Length()-1] != PATHSEPCHAR )
			{
				RETURN_IF_ERROR(filename.Append(PATHSEP));
			}

			RETURN_IF_ERROR(filename.Append(suggested_filename.CStr()));

			BOOL exists = FALSE;

			OpFile file;
			RETURN_IF_ERROR(file.Construct(filename.CStr()));
			RETURN_IF_ERROR(file.Exists(exists));

			if (exists)
			{
				RETURN_IF_ERROR(CreateUniqueFilename(filename));
			}
			// Save file to disk
			RETURN_IF_ERROR(url.SaveAsFile(filename.CStr()));

			RETURN_IF_ERROR(destination_filename.Set(filename.CStr()));

			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

#ifdef SPEEDDIAL_SVG_SUPPORT
OP_STATUS WindowCommanderProxy::GetSVGImage(OpWindowCommander* win_comm, Image& image, UINT32 width, UINT32 height)
{
	Window* win = win_comm->GetWindow();
	DocumentManager* doc_man = win->DocManager();
	FramesDocument* frm_doc = doc_man->GetCurrentDoc();

	//	OP_ASSERT(frm_doc->IsLoaded());

	if(frm_doc && frm_doc->IsLoaded())
	{
		HTML_Element* root = frm_doc->GetDocRoot();
		if(root)
		{
			if(!root->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
			{
				root = root->Next();
				while(root)
				{
					if(root->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
					{
						break;
					}
					root = root->Next();
				}
			}
			if(root && root->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
			{
				SVGImage* svgimage = g_svg_manager->GetSVGImage(frm_doc->GetLogicalDocument(), root);
				if(svgimage)
				{
					OpBitmap* bitmap;
					SVGRect svg_rect(0, 0, width, height);

					RETURN_IF_ERROR(svgimage->PaintToBuffer(bitmap, 0, width, height, &svg_rect));

					if(!image.IsEmpty())
					{
						image.DecVisible(null_image_listener);
						image.Empty();
					}
					image = imgManager->GetImage(bitmap);
					if (image.IsEmpty())     // OOM
						OP_DELETE(bitmap);

				}
			}
		}
	}
	return OpStatus::OK;
}
#endif // SPEEDDIAL_SVG_SUPPORT

URL WindowCommanderProxy::GetBGImageURL(OpWindowCommander* win_comm)
{ 
	URL url;
	if(win_comm)
	{
		Window* window = win_comm->GetWindow();
		if(window)
		{
			FramesDocument* frame_doc = window->GetActiveFrameDoc();
			if(frame_doc)
			{
				url = frame_doc->GetBGImageURL();
			}
		}
	}
	return url;
}

URL WindowCommanderProxy::GetImageURL(OpWindowCommander* win_comm)
{ 
	URL url;
	if(win_comm)
	{
		Window* window = win_comm->GetWindow();
		if(window)
		{
			FramesDocument* frame_doc = window->GetActiveFrameDoc();
			if(frame_doc && frame_doc->GetHtmlDocument())
			{
				HTML_Element* h_elm = frame_doc->GetHtmlDocument()->GetHoverHTMLElement();
				if(h_elm && h_elm->GetNsType() == NS_HTML && h_elm->Type() == HE_IMG)
				{
					url = h_elm->GetImageURL();
				}
			}
		}
	}
	return url;
}

OP_STATUS WindowCommanderProxy::GetWandLink(OpWindowCommander* commander, OpString& url)
{
	if (!HasCurrentDoc(commander))
		return OpStatus::ERR;

	FramesDocument* frames_doc = GetCurrentDoc(commander);
	if (frames_doc && frames_doc->GetActiveSubDoc())
		frames_doc = static_cast<FramesDocument*>(frames_doc->GetActiveSubDoc());

	if (!g_wand_manager->Usable(frames_doc, TRUE))
		return OpStatus::ERR;

	return frames_doc->GetURL().GetAttribute(URL::KUniName_Username_Password_Hidden, url);
}

OP_STATUS WindowCommanderProxy::ProcessWandLogin(OpWindowCommander* commander)
{
	if (!HasCurrentDoc(commander))
		return OpStatus::ERR;

	FramesDocument* frames_doc = GetCurrentDoc(commander);
	if (frames_doc && frames_doc->GetActiveSubDoc())
		frames_doc = static_cast<FramesDocument*>(frames_doc->GetActiveSubDoc());

	return g_wand_manager->Use(frames_doc);
}

OP_STATUS WindowCommanderProxy::GetForwardLinks(OpWindowCommander* commander, OpVector<DocumentHistoryInformation>& info_list)
{
	FramesDocument* frames_doc = GetCurrentDoc(commander);
	if (!frames_doc)
		return OpStatus::ERR;

	bool in_sub_doc = false;
	if (frames_doc && frames_doc->GetActiveSubDoc())
	{
		frames_doc = (FramesDocument*) frames_doc->GetActiveSubDoc();
		in_sub_doc = true;
	}

	LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
	if (!logdoc)
		return OpStatus::ERR;

	bool is_first_img = true;
	OpString img_title_next, img_title_show;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::M_FASTFORWARD_NEXT_IMAGE, img_title_next));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::M_FASTFORWARD_SHOW_IMAGE, img_title_show));

	HTML_Element* h_elm = logdoc->GetFirstHE(HE_A);
	while (h_elm)
	{
		/*
		NOTE: link_url, the return value from HTML_Element::GetA_URL(), should not be deleted.
		*/
		URL* link_url;
		// Don't collect javascript urls in sub frames since they will run in the wrong context (the top document's context)
		if (h_elm->Type() == HE_A && (link_url = h_elm->GetA_URL(logdoc))
			&& !(in_sub_doc && link_url->Type() == URL_JAVASCRIPT))
		{
			// If link points to an image, it's a candidate for fast-foward
			OpString url_text_extension;
			TRAPD(err, link_url->GetAttributeL(URL::KUniNameFileExt_L, url_text_extension));
			if (url_text_extension.HasContent())
			{
				uni_char* url_text_ext_ptr = url_text_extension.CStr();
				if (uni_stri_eq(url_text_ext_ptr, UNI_L("BMP"))
					|| uni_stri_eq(url_text_ext_ptr, UNI_L("JPG"))
					|| uni_stri_eq(url_text_ext_ptr, UNI_L("JPEG"))
					|| uni_stri_eq(url_text_ext_ptr, UNI_L("GIF"))
					|| uni_stri_eq(url_text_ext_ptr, UNI_L("ICO"))
					|| uni_stri_eq(url_text_ext_ptr, UNI_L("PNG"))
					|| uni_stri_eq(url_text_ext_ptr, UNI_L("SVG"))
					|| uni_stri_eq(url_text_ext_ptr, UNI_L("WBMP"))
					|| uni_stri_eq(url_text_ext_ptr, UNI_L("XBM"))
					|| uni_stri_eq(url_text_ext_ptr, UNI_L("WEBP")))
				{
					OpString url_string;
					if (OpStatus::IsSuccess(link_url->GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, url_string)))
					{
						const uni_char* img_title = is_first_img ? img_title_show.CStr() : img_title_next.CStr();
						RETURN_IF_ERROR(AddForwardFileLink(info_list, img_title, url_string.CStr()));
						is_first_img = false;
					}
				}
			}

			// Use child elements to build text; works in most cases, right?
			OpString link_text;
			HTML_Element* child_elm = h_elm->FirstChild();
			while (child_elm && child_elm != h_elm)
			{
				if (child_elm->Type() == HE_TEXT)
					link_text.Append(child_elm->Content());
				else if (child_elm->Type() == HE_IMG)
				{
					OpString url_string;
					if (OpStatus::IsSuccess(link_url->GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, url_string)) && child_elm->GetIMG_Alt())
						RETURN_IF_ERROR(AddForwardLink(info_list, child_elm->GetIMG_Alt(), url_string.CStr()));
				}

				if (child_elm == h_elm->LastLeaf())
					break;

				if (child_elm->FirstChild())
					child_elm = child_elm->FirstChild();
				else
				{
					for( ;child_elm != h_elm; child_elm = child_elm->Parent())
					{
						if(child_elm->Suc())
						{
							child_elm = child_elm->Suc();
							break;
						}
					}
				}
			}

			// Strip spaces on start or end, ASCII-values less than 32 (0x20) and double spaces
			uni_char *old_text;
			uni_char *new_text;
			old_text = new_text = link_text.DataPtr();

			if (old_text)
			{
				for (; *old_text; ++old_text)
				{
					if (*old_text < 0x20)
						continue;

					if (*old_text == ' ' &&
						(new_text == link_text.DataPtr() || *(old_text + 1) == ' ' || *(old_text + 1) == '\0'))
						continue;

					*new_text++ = *old_text;
				}

				// Terminate the new string
				// It is not necessary to reduce memory consumption - link_text is soon getting out of scope
				*new_text = 0;
				if (new_text != link_text.DataPtr())
				{
					OpString url_string;
					if (OpStatus::IsSuccess(link_url->GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, url_string)))
						RETURN_IF_ERROR(AddForwardLink(info_list, link_text.CStr(), url_string.CStr()));
				}
			}
		}

		if (h_elm->Type() != HE_A && h_elm->FirstChild())
			h_elm = h_elm->FirstChild();
		else
		{
			for(;h_elm; h_elm = h_elm->Parent())
			{
				if(h_elm->Suc())
				{
					h_elm = h_elm->Suc();
					break;
				}
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS WindowCommanderProxy::AddForwardLink(OpVector<DocumentHistoryInformation>& ff_info_list, const uni_char* link_word, const uni_char* url_str)
{
	static int base_score = 100;
	int tmp_score = GetFastForwardValue(link_word);
	bool shall_add = tmp_score ? true : false;

	OpString modified_link_work;
	RETURN_IF_ERROR(modified_link_work.Set(link_word));

	// Try first word, but only if score is greater than 100
	if (!shall_add)
	{
		uni_char* first = uni_strchr(link_word, 0x20);
		if (!first)
			return OpStatus::OK;

		tmp_score = GetFastForwardValue(modified_link_work.SubString(0, first - link_word).CStr());
		if (tmp_score >= 100)
		{
			shall_add = true;
			modified_link_work.Delete(first - link_word);
		}
	}

	// Try last word, but only if score is greater than 100
	if (!shall_add)
	{
		uni_char* last = uni_strrchr(link_word, 0x20);
		if (!last)
			return OpStatus::OK;

		tmp_score = GetFastForwardValue(modified_link_work.SubString(last + 1 - link_word));
		if (tmp_score >= 100)
			shall_add = true;
	}

	if (shall_add)
	{
		if (ff_info_list.Get(0) && uni_strcmp(ff_info_list.Get(0)->url, url_str) == 0)
			return OpStatus::OK;

		OpAutoPtr<DocumentHistoryInformation> history_info(OP_NEW(DocumentHistoryInformation, (DocumentHistoryInformation::HISTORY_NAVIGATION)));
		RETURN_OOM_IF_NULL(history_info.get());
		RETURN_IF_ERROR(history_info->url.Set(url_str));
		RETURN_IF_ERROR(history_info->title.Set(modified_link_work));
		history_info->number = 0;
		history_info->score = tmp_score;

		UINT32 count = ff_info_list.GetCount();
		UINT32 i = 0;
		for (; i < count; i++)
		{
			if (ff_info_list.Get(i)->score <= history_info->score)
				break;
		}
		if (OpStatus::IsSuccess(ff_info_list.Insert(i, history_info.get())))
			history_info.release();
	}

	return OpStatus::OK;
}


OP_STATUS WindowCommanderProxy::AddForwardFileLink(OpVector<DocumentHistoryInformation>& ff_info_list, const uni_char* img_title, const uni_char* url_str)
{
	if (ff_info_list.Get(0) && uni_strcmp(ff_info_list.Get(0)->url, url_str) == 0)
		return OpStatus::OK;

	OpAutoPtr<DocumentHistoryInformation> history_info(OP_NEW(DocumentHistoryInformation, (DocumentHistoryInformation::HISTORY_IMAGE)));
	RETURN_OOM_IF_NULL(history_info.get());
	RETURN_IF_ERROR(history_info->url.Set(url_str));
	RETURN_IF_ERROR(history_info->title.Set(img_title));
	history_info->number = 0;
	return ff_info_list.Add(history_info.release());
}

int WindowCommanderProxy::GetFastForwardValue(OpStringC which)
{
	PrefsEntry* entry;
	if (g_setup_manager->GetFastForwardSection())
	{
		entry = g_setup_manager->GetFastForwardSection()->FindEntry(which.CStr());
		if (entry)
			return entry->Get() ? uni_strtol(entry->Get(), NULL, 0) : 100;
	}

	return 0;
}
