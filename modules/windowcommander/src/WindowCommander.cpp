/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/hardcore/base/op_types.h"
#include "modules/hardcore/mem/mem_man.h"

#include "modules/dochand/viewportcontroller.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/url/url_man.h"
#include "modules/url/url_sn.h"
#include "modules/url/url2.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/doc_capabilities.h"
#include "modules/img/image.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/layout/layout_workplace.h"
#include "modules/logdoc/savewithinline.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/scope/scope_window_listener.h"
#include "modules/xmlutils/xmldocumentinfo.h"

#ifdef USE_OP_CLIPBOARD
#include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD

#ifdef LINK_SUPPORT
#include "modules/logdoc/link.h"
#endif // LINK_SUPPORT

#ifdef _SPAT_NAV_SUPPORT_
#include "modules/spatial_navigation/handler_api.h"
#endif // _SPAT_NAV_SUPPORT_

#ifdef WINDOW_COMMANDER_TRANSFER
#include "modules/windowcommander/src/TransferManager.h"
#endif // WINDOW_COMMANDER_TRANSFER

#ifdef CORE_THUMBNAIL_SUPPORT
#include "modules/thumbnails/src/thumbnail.h"
#endif

#ifdef SUPPORT_GENERATE_THUMBNAILS
#include "modules/thumbnails/thumbnailgenerator.h"
#endif // SUPPORT_GENERATE_THUMBNAILS

#include "modules/ecmascript_utils/esthread.h"
#include "modules/dom/domenvironment.h"

#ifdef SECURITY_INFORMATION_PARSER
# include "modules/windowcommander/src/SecurityInfoParser.h"
# include "modules/windowcommander/src/TransferManagerDownload.h"
#endif // SECURITY_INFORMATION_PARSER
#ifdef TRUST_INFO_RESOLVER
# include "modules/windowcommander/src/TrustInfoResolver.h"
#endif // TRUST_INFO_RESOLVER
#ifdef NEARBY_ELEMENT_DETECTION
# include "modules/widgets/finger_touch/element_expander.h"
#endif

#ifdef APPLICATION_CACHE_SUPPORT
# include "modules/applicationcache/application_cache_manager.h"
#endif // APPLICATION_CACHE_SUPPORT

#ifdef WEB_TURBO_MODE
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#endif // WEB_TURBO_MODE

#ifdef GADGET_SUPPORT
# include "modules/gadgets/OpGadget.h"
#endif // GADGET_SUPPORT

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
#include "modules/dom/src/js/window.h"
#endif

#include "modules/doc/documentmenumanager.h"

#ifdef _PRINT_SUPPORT_
# include "modules/display/prn_info.h"
# include "modules/display/prn_dev.h"
#endif

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
/*static*/
InteractiveItemInfo* InteractiveItemInfo::CreateInteractiveItemInfo(unsigned int num_rects, Type type)
{
	InteractiveItemInfo* item;

	switch (type)
	{
	case INTERACTIVE_ITEM_TYPE_FORM_INPUT:
		item = OP_NEW(InputFieldInfo, (num_rects));
		break;
	case INTERACTIVE_ITEM_TYPE_FORM_CHECKBOX:
		item = OP_NEW(CheckboxInfo, (num_rects));
		break;
	default:
		item = OP_NEW(InteractiveItemInfo, (num_rects, type));
		break;
	}

	if (!item)
		return NULL;

	item->item_rects = OP_NEWA(ItemRect, num_rects);

	if (!item->item_rects)
	{
		OP_DELETE(item);
		return NULL;
	}

	for (unsigned int i = 0; i < num_rects; i++)
		item->item_rects[i].affine_pos = NULL;

	return item;
}

InteractiveItemInfo::~InteractiveItemInfo()
{
	if (item_rects)
	{
		for (unsigned int i = 0; i < num_rects; i++)
		{
			OP_DELETE(item_rects[i].affine_pos);
		}

		OP_DELETEA(item_rects);
	}
}

OP_STATUS InteractiveItemInfo::PrependPos(const AffinePos& pos)
{
	for (unsigned int i=0; i < num_rects; i++)
	{
		AffinePos* affine_pos = item_rects[i].affine_pos;
		if (affine_pos)
			affine_pos->Prepend(pos);
#ifdef CSS_TRANSFORMS
		else if (pos.IsTransform())
		{
			item_rects[i].affine_pos = OP_NEW(AffinePos, (pos));

			if (!item_rects[i].affine_pos)
				return OpStatus::ERR_NO_MEMORY;
		}
#endif // CSS_TRANSFORMS
		else
			item_rects[i].rect.OffsetBy(pos.GetTranslation());
	}

	return OpStatus::OK;
}

#ifdef CSS_TRANSFORMS
void InteractiveItemInfo::ChangeToBBoxes()
{
	for (unsigned int i=0; i < num_rects; i++)
	{
		AffinePos* affine_pos = item_rects[i].affine_pos;
		if (affine_pos)
		{
			affine_pos->Apply(item_rects[i].rect);
			OP_DELETE(affine_pos);
			item_rects[i].affine_pos = NULL;
		}
	}
}
#endif // CSS_TRANSFORMS

InputFieldInfo::~InputFieldInfo()
{
	OP_DELETE(ctx);
}
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

#ifdef PLUGIN_AUTO_INSTALL
 OP_STATUS PluginInstallInformationImpl::SetMimeType(const OpStringC& mime_type)
 {
	 return m_mime_type.Set(mime_type);
 }

void PluginInstallInformationImpl::SetURL(const URL& url)
{
	m_plugin_url = url;
}

OP_STATUS PluginInstallInformationImpl::GetMimeType(OpString& mime_type) const
{
	return mime_type.Set(m_mime_type);
}

URL PluginInstallInformationImpl::GetURL() const
{
	return m_plugin_url;
}

OP_STATUS PluginInstallInformationImpl::GetURLString(OpString& url_string, BOOL for_ui) const
{
	if (for_ui)
		return m_plugin_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, url_string, TRUE);
	else
		return m_plugin_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI, url_string, TRUE);
}

BOOL PluginInstallInformationImpl::IsURLValidForDialog() const
{
	if (m_plugin_url.IsEmpty())
		return FALSE;

	switch (m_plugin_url.Type())
	{
	case URL_HTTP:
	case URL_FTP:
	case URL_FILE:
	case URL_HTTPS:
		return TRUE;
	}
	return FALSE;
}
#endif // PLUGIN_AUTO_INSTALL

WindowCommander::WindowCommander() :
		m_window(NULL),
		m_opWindow(NULL),
#ifdef GADGET_SUPPORT
		m_gadget(NULL),
#endif // GADGET_SUPPORT
#ifdef WIC_LIMIT_PROGRESS_FREQUENCY
		m_loadingListener(&m_loadingListenerProxy),
#else
		m_loadingListener(NULL),
#endif // WIC_LIMIT_PROGRESS_FREQUENCY
		m_documentListener(NULL),
		m_mailClientListener(NULL),
#ifdef _POPUP_MENU_SUPPORT_
		m_menuListener(NULL),
#endif // _POPUP_MENU_SUPPORT_
		m_linkListener(NULL),
#ifdef ACCESS_KEYS_SUPPORT
		m_accessKeyListener(NULL),
#endif // ACCESS_KEYS_SUPPORT
#ifdef _PRINT_SUPPORT_
		m_printingListener(NULL),
#endif // _PRINT_SUPPORT_
		m_errorListener(NULL)
#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	, m_sslListener(NULL)
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER
#ifdef _ASK_COOKIE
	, m_cookieListener(NULL)
#endif // _ASK_COOKIE
#ifdef WIC_FILESELECTION_LISTENER
	, m_fileSelectionListener(NULL)
#endif // WIC_FILESELECTION_LISTENER
#ifdef NEARBY_ELEMENT_DETECTION
	, m_fingertouchListener(NULL)
#endif // NEARBY_ELEMENT_DETECTION
#ifdef WIC_PERMISSION_LISTENER
	, m_permissionListener(NULL)
#endif // WIC_PERMISSION_LISTENER
#ifdef SCOPE_URL_PLAYER
	, m_urlPlayerListener(NULL)
#endif //SCOPE_URL_PLAYER

#ifdef WIC_EXTERNAL_APPLICATIONS_ENUMERATION
	, m_applicationListener(NULL)
#endif // WIC_EXTERNAL_APPLICATIONS_ENUMERATION

#ifdef APPLICATION_CACHE_SUPPORT
	, m_applicationCachelistener(NULL)
#endif // APPLICATION_CACHE_SUPPORT

#ifdef WIC_OS_INTERACTION
	, m_osInteractionListener(NULL)
#endif // WIC_OS_INTERACTION

#ifdef WIC_LIMIT_PROGRESS_FREQUENCY
	, m_loadingListenerProxy(this)
#endif // WIC_LIMIT_PROGRESS_FREQUENCY

#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
	, m_selftestListener(NULL)
#endif // GOGI_SELFTEST_FINISHED_NOTIFICATION
	, currentElement(NULL)
	, selection_start_document(0)
{
}

WindowCommander::~WindowCommander()
{
#ifdef SCOPE_URL_PLAYER
	if(m_urlPlayerListener){
		m_urlPlayerListener->OnDeleteWindowCommander(this);
	}
#endif //SCOPE_URL_PLAYER
}

OP_STATUS WindowCommander::Init()
{
	NullAllListeners();

	return OpStatus::OK;
}

BOOL WindowCommander::OpenURL(const uni_char* url, const OpWindowCommander::OpenURLOptions & options)
{
	EnteredByUser user = options.entered_by_user ? WasEnteredByUser : NotEnteredByUser;
	if (options.replace_prev_history_entry)
		m_window->DocManager()->SetUseHistoryNumber(m_window->GetCurrentHistoryPos());
	BOOL user_initiated = options.entered_by_user;
	BOOL externally_initiated = options.requested_externally;

	OP_STATUS status = m_window->OpenURL(
		url, TRUE, user_initiated,  // url, check_if_expired, user_initiated
		FALSE, FALSE, FALSE,        // reload, is_hotlist_url, open_in_background
		user, externally_initiated, // entered_by_user, called_from_dde
		options.context_id);

	if (OpStatus::IsMemoryError(status))
	{
		g_memory_manager->RaiseCondition(status);
		return FALSE;
	}
	return TRUE;
}

#ifdef DOM_LOAD_TV_APP
BOOL WindowCommander::LoadTvApp(const uni_char* url, const OpenURLOptions & options, OpAutoVector<OpString>* whitelist)
{
	OP_ASSERT(!m_window->CanBeClosedByScript() || !"This API should only be called on a newly created window, which is expected to not be closable by scripts!");
	m_window->SetCanBeClosedByScript(TRUE);
	m_window->DocManager()->SetWhitelist(whitelist);
	BOOL result = OpenURL(url, options);
	if (!result)
		m_window->DocManager()->SetWhitelist(NULL);
	return result;
}

void WindowCommander::CleanupTvApp()
{
	m_window->SetCanBeClosedByScript(FALSE);
	m_window->DocManager()->SetWhitelist(NULL);
}
#endif

BOOL WindowCommander::FollowURL(const uni_char* url,
                                const uni_char* referrer_url,
                                const OpWindowCommander::OpenURLOptions & options)
{
	EnteredByUser user = options.entered_by_user ? WasEnteredByUser : NotEnteredByUser;

	//default values and meaning of default values are different from WindowCommander and URL.
	//If we get the default value, let URL use ITS default value.
	URL open_url;
	URL ref_url;
	if(options.context_id != (URL_CONTEXT_ID) -1) {
		open_url = urlManager->GetURL(url, options.context_id);
		ref_url = urlManager->GetURL(referrer_url, options.context_id);
	} else {
		open_url = urlManager->GetURL(url);
		ref_url = urlManager->GetURL(referrer_url);
	}
	OP_STATUS status = m_window->OpenURL(
		open_url, DocumentReferrer(ref_url),  // url, referrer
		TRUE, FALSE, -1,    // check_if_expired, reload, sub_win_id
		FALSE, FALSE, user, // user_initiated, open_in_background, entered_by_user
		options.requested_externally // called_from_dde
		);
	if (OpStatus::IsMemoryError(status))
	{
		g_memory_manager->RaiseCondition(status);
	}
	return !OpStatus::IsError(status);
}

BOOL WindowCommander::OpenURL(const uni_char* url, BOOL entered_by_user, URL_CONTEXT_ID context_id, BOOL use_prev_entry)
{
	OpenURLOptions options;
	options.entered_by_user = entered_by_user;
	options.replace_prev_history_entry = use_prev_entry;
	options.context_id = context_id;

	return OpenURL(url, options);
}

BOOL WindowCommander::FollowURL(const uni_char* url, const uni_char* referrer_url, BOOL entered_by_user, URL_CONTEXT_ID context_id)
{
	OpenURLOptions options;
	options.entered_by_user = entered_by_user;
	options.context_id = context_id;

	return FollowURL(url, referrer_url, options);
}

void WindowCommander::Reload(BOOL force_inline_reload /*= FALSE*/)
{
	m_window->Reload(WasEnteredByUser, FALSE, !force_inline_reload);
}

BOOL WindowCommander::Next()
{
	if (HasNext())
	{
		m_window->SetHistoryNext(FALSE);
		return TRUE;
	}
	return FALSE;
}

BOOL WindowCommander::Previous()
{
	if (HasPrevious())
	{
		m_window->SetHistoryPrev(FALSE);
		return TRUE;
	}
	return FALSE;
}

void WindowCommander::GetHistoryRange(int* min, int* max)
{
	*min = m_window->GetHistoryMin();
	*max = m_window->GetHistoryMax();
}

int WindowCommander::GetCurrentHistoryPos()
{
	return m_window->GetCurrentHistoryPos();
}

int WindowCommander::SetCurrentHistoryPos(int pos)
{
	m_window->SetCurrentHistoryPos(pos, TRUE, TRUE);
	return m_window->GetCurrentHistoryPos();
}

OP_STATUS WindowCommander::SetHistoryUserData(int history_ID, OpHistoryUserData* user_data)
{
	return m_window->SetHistoryUserData(history_ID, user_data);
}

OP_STATUS WindowCommander::GetHistoryUserData(int history_ID, OpHistoryUserData** user_data) const
{
	return m_window->GetHistoryUserData(history_ID, user_data);
}

void WindowCommander::Stop()
{
	m_window->CancelLoad(); // always returns TRUE
}

BOOL WindowCommander::Navigate(UINT16 direction)
{
	OP_ASSERT(FALSE);
	return TRUE;
}

OP_STATUS WindowCommander::GetDocumentTypeString(const uni_char** doctype)
{
	if( !doctype )
		return OpStatus::ERR_NULL_POINTER;

	FramesDocument* fd  = m_window->DocManager()->GetCurrentDoc();
	LogicalDocument* ld = fd ? fd->GetLogicalDocument() : NULL;

	if(!ld)
		return OpStatus::ERR;

	if( ld->IsXml() )
	{
		const XMLDocumentInformation* docinfo = ld->GetXMLDocumentInfo();
		if( docinfo )
		{
			*doctype = docinfo->GetPublicId();
			return OpStatus::OK;
		}
	}

	*doctype = ld->GetDoctypePubId();
	return OpStatus::OK;
}

void WindowCommander::SetCanBeClosedByScript(BOOL can_close)
{
	m_window->SetCanBeClosedByScript(can_close);
}

static inline FramesDocument* GetCurrentDocument(Window* win, BOOL focused_frame)
{
	FramesDocument *doc = NULL;

	if (focused_frame)
		doc = win->GetActiveFrameDoc();

	if (!doc)
		doc = win->GetCurrentDoc();

	return doc;
}

#ifdef SAVE_SUPPORT

# include "modules/display/VisDevListeners.h"
# include "modules/img/imagedump.h"
# include "modules/minpng/minpng.h"
# include "modules/minpng/minpng_encoder.h"
# include "modules/svg/SVGManager.h"
# include "modules/svg/svg_image.h"

#ifdef SELFTEST
OP_STATUS WindowCommander::SaveToPNG(const uni_char* file_name, const OpRect& rect )
{
	OpBitmap* bitmap = 0;
	RETURN_IF_ERROR(OpBitmap::Create(&bitmap, rect.width, rect.height, FALSE, FALSE, 0, 0, TRUE));
	OpAutoPtr<OpBitmap> auto_bitmap(bitmap);

	OpPainter* painter = bitmap->GetPainter();
	VisualDevice *vis_dev = m_window->VisualDev();

	if ( !painter || !vis_dev )
		return OpStatus::ERR;

	OpRect area(0,0,rect.width,rect.height);
	vis_dev->GetContainerView()->Paint(area, painter, 0, 0, TRUE, FALSE);
	bitmap->ReleasePainter();

	return DumpOpBitmapToPNG(bitmap,file_name, TRUE);
}
#endif // SELFTEST

OP_STATUS WindowCommander::GetSuggestedFileName(BOOL focused_frame, OpString* suggested_filename)
{
	FramesDocument* doc = GetCurrentDocument(m_window, focused_frame);

	if (!doc)
		return OpStatus::ERR;

	URL url = doc->GetURL();

	// For normal documents, use the page title. For wrapper documents that just represent a single file,
	// use that file's suggested file name.
	if (!doc->IsWrapperDoc())
	{
		TempBuffer temp_buffer;
		RETURN_IF_ERROR(suggested_filename->Set(doc->Title(&temp_buffer)));

		if (!suggested_filename->IsEmpty())
		{
			OpString extension;

			RETURN_IF_ERROR(url.GetAttribute(URL::KSuggestedFileNameExtension_L, extension));

			if (extension.HasContent())
				return suggested_filename->AppendFormat(UNI_L(".%s"), extension.CStr());

			return OpStatus::OK;
		}
	}

	// No page title. Ask url for filename.

	return url.GetAttribute(URL::KSuggestedFileName_L, *suggested_filename);
}

OpWindowCommander::DocumentType WindowCommander::GetDocumentType(BOOL focused_frame)
{
	FramesDocument* doc = GetCurrentDocument(m_window, focused_frame);

	if (!doc)
		return DOC_OTHER;

	if (doc->IsPluginDoc())
		return DOC_PLUGIN;

	URL url = doc->GetURL();

	DocumentType type;

	switch (url.GetAttribute(URL::KContentType, TRUE))
	{
	case URL_TEXT_CONTENT:
		type = DOC_PLAINTEXT;
		break;

	case URL_HTML_CONTENT:
		if (url.GetAttribute(URL::KCacheType) == URL_CACHE_MHTML)
			type = DOC_MHTML;
		else
			type = DOC_HTML;

		break;

	case URL_XML_CONTENT:
		type = DOC_OTHER_TEXT;

		if (doc)
			if (LogicalDocument* logdoc = doc->GetLogicalDocument())
			{
				if (logdoc->GetHLDocProfile()->IsXhtml())
					type = DOC_XHTML;
#if defined WEBFEEDS_DISPLAY_SUPPORT
				else if (doc->IsInlineFeed())
					type = DOC_WEBFEED;
#endif // WEBFEEDS_DISPLAY_SUPPORT
			}

		break;

	case URL_WML_CONTENT:
		type = DOC_WML;
		break;

	case URL_SVG_CONTENT:
		type = DOC_SVG;
		break;

	case URL_WBMP_CONTENT:
	case URL_GIF_CONTENT:
	case URL_JPG_CONTENT:
	case URL_XBM_CONTENT:
	case URL_BMP_CONTENT:
	case URL_WEBP_CONTENT:
	case URL_MPG_CONTENT:
	case URL_PNG_CONTENT:
	case URL_ICO_CONTENT:
		type = DOC_OTHER_GRAPHICS;
		break;

	default:
		type = DOC_OTHER;
		break;
	}

	return type;
}

OP_STATUS WindowCommander::SaveDocument(const uni_char* file_name, SaveAsMode mode, BOOL focused_frame, unsigned int max_size,
                                        unsigned int* page_size, unsigned int* saved_size)
{
	if (max_size != 0
#ifdef MHTML_ARCHIVE_SAVE_SUPPORT
		&& mode != SAVE_AS_MHTML
#endif // MHTML_ARCHIVE_SAVE_SUPPORT
		)
	{
		OP_ASSERT(!"Size restriction is not supported in this mode");
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	FramesDocument* doc = GetCurrentDocument(m_window, focused_frame);

	if (!doc)
		return OpStatus::ERR;

	URL url = doc->GetURL();

	switch (mode)
	{
	case SAVE_ONLY_DOCUMENT:
		return url.SaveAsFile(file_name) == 0 ? OpStatus::OK : OpStatus::ERR;

	case SAVE_DOCUMENT_AND_INLINES:
		return SaveWithInlineHelper::Save(url, doc, file_name, m_window->GetForceEncoding(), m_window, FALSE);

# ifdef MHTML_ARCHIVE_SAVE_SUPPORT
	case SAVE_AS_MHTML:
		return SaveAsArchiveHelper::SaveAndReturnSize(url, file_name, m_window, max_size, page_size, saved_size);
# endif // MHTML_ARCHIVE_SAVE_SUPPORT

# ifdef WIC_SAVE_DOCUMENT_AS_TEXT_SUPPORT
	case SAVE_AS_TEXT:
		return m_window->DocManager()->SaveCurrentDocAsText(NULL, file_name, m_window->GetForceEncoding());
# endif // WIC_SAVE_DOCUMENT_AS_TEXT_SUPPORT

# ifdef SVG_SUPPORT
	case SAVE_AS_PNG:
		if (url.ContentType() == URL_SVG_CONTENT)
		{
			// Save SVG as PNG.

			FramesDocument* top_doc = m_window->DocManager()->GetCurrentDoc();
			LogicalDocument* logdoc = top_doc ? top_doc->GetLogicalDocument() : NULL;

			if (logdoc)
			{
				HTML_Element* root = logdoc->GetDocRoot();
				HTML_Element *clicked_root =
#  ifdef DISPLAY_CLICKINFO_SUPPORT
					MouseListener::GetClickInfo().GetImageElement()
#  else // DISPLAY_CLICKINFO_SUPPORT
					NULL
#  endif // DISPLAY_CLICKINFO_SUPPORT
					;

				HTML_Element* svg_root = clicked_root ? clicked_root : root;
				SVGImage* svg_image = g_svg_manager->GetSVGImage(logdoc, svg_root);

				if (svg_image)
				{
					OpBitmap* bm;
					int width = -1;
					int height = -1;
					int time_ms = 0;

					OP_STATUS result = svg_image->PaintToBuffer(bm, time_ms, width, height);

					if (OpStatus::IsSuccess(result))
					{
						result = DumpOpBitmapToPNG(bm, file_name, TRUE);
						OP_DELETE(bm);
					}

					return result;
				}
			}

			break;
		}
# endif // SVG_SUPPORT
	}

	return OpStatus::ERR;
}

# ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
OP_STATUS WindowCommander::GetOriginalURLForMHTML(OpString& url)
{
	url.Empty();
	URL storage_url = m_window->GetCurrentURL().GetAttribute(g_mime_module.GetOriginalURLAttribute());
	if (!storage_url.IsEmpty())
	{
		return url.Set(storage_url.GetAttribute(URL::KUniName));
	}
	return OpStatus::OK;
}
# endif // MHTML_ARCHIVE_REDIRECT_SUPPORT
#endif // SAVE_SUPPORT

OP_STATUS WindowCommander::OnUiWindowCreated(OpWindow* opWindow)
{
	m_opWindow = opWindow;
	m_window = g_windowManager->AddWindow(m_opWindow, this);

	if (m_window == NULL)
	{
		m_opWindow = NULL;

		NullAllListeners();
		return OpStatus::ERR;
	}

#ifdef WEB_TURBO_MODE
	if( g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseWebTurbo) )
		m_window->SetTurboMode(TRUE);
#endif // WEB_TURBO_MODE

#ifdef GADGET_SUPPORT
	if (m_gadget)
		RETURN_IF_ERROR(m_gadget->OnUiWindowCreated(m_opWindow, m_window));
#endif // GADGET_SUPPORT

	return OpStatus::OK;
}

#ifdef SVG_SUPPORT
OP_STATUS WindowCommander::SVGZoomBy(OpDocumentContext& context, int zoomdelta)
{
	if (FramesDocument *frm_doc = GetCurrentDocument(m_window, TRUE))
		return frm_doc->SVGZoomBy(context, zoomdelta);

	return OpStatus::ERR;
}

OP_STATUS WindowCommander::SVGZoomTo(OpDocumentContext& context, int zoomlevel)
{
	if (FramesDocument *frm_doc = GetCurrentDocument(m_window, TRUE))
		return frm_doc->SVGZoomTo(context, zoomlevel);

	return OpStatus::ERR;
}

OP_STATUS WindowCommander::SVGResetPan(OpDocumentContext& context)
{
	if (FramesDocument *frm_doc = GetCurrentDocument(m_window, TRUE))
		return frm_doc->SVGResetPan(context);

	return OpStatus::ERR;
}

OP_STATUS WindowCommander::SVGPlayAnimation(OpDocumentContext& context)
{
	if (FramesDocument *frm_doc = GetCurrentDocument(m_window, TRUE))
		return frm_doc->SVGPlayAnimation(context);

	return OpStatus::ERR;
}

OP_STATUS WindowCommander::SVGPauseAnimation(OpDocumentContext& context)
{
	if (FramesDocument *frm_doc = GetCurrentDocument(m_window, TRUE))
		return frm_doc->SVGPauseAnimation(context);

	return OpStatus::ERR;
}

OP_STATUS WindowCommander::SVGStopAnimation(OpDocumentContext& context)
{
	if (FramesDocument *frm_doc = GetCurrentDocument(m_window, TRUE))
		return frm_doc->SVGStopAnimation(context);

	return OpStatus::ERR;
}
#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
OP_STATUS WindowCommander::MediaPlay(OpDocumentContext& context, BOOL play)
{
	if (FramesDocument *frm_doc = GetCurrentDocument(m_window, TRUE))
		return frm_doc->MediaPlay(context, play);

	return OpStatus::ERR;
}

OP_STATUS WindowCommander::MediaMute(OpDocumentContext& context, BOOL mute)
{
	if (FramesDocument *frm_doc = GetCurrentDocument(m_window, TRUE))
		return frm_doc->MediaMute(context, mute);

	return OpStatus::ERR;
}

OP_STATUS WindowCommander::MediaShowControls(OpDocumentContext& context, BOOL show)
{
	if (FramesDocument *frm_doc = GetCurrentDocument(m_window, TRUE))
		return frm_doc->MediaShowControls(context, show);

	return OpStatus::ERR;
}

#endif // MEDIA_HTML_SUPPORT

void WindowCommander::OnUiWindowClosing()
{
#ifdef GADGET_SUPPORT
	if (m_gadget)
		m_gadget->OnUiWindowClosing(m_window);
#endif // GADGET_SUPPORT

#ifdef APPLICATION_CACHE_SUPPORT
	/** we send the cancel events here, since the listeners are NULL'ed before DOM_Environment is deleted*/
	if (g_application_cache_manager)
		g_application_cache_manager->CancelAllDialogsForWindowCommander(this);
#endif // APPLICATION_CACHE_SUPPORT

	NullAllListeners();

	g_windowManager->DeleteWindow(m_window);
	m_window = NULL;
}

BOOL WindowCommander::HasNext()
{
	return m_window->HasHistoryNext();
}

BOOL WindowCommander::HasPrevious()
{
	return m_window->HasHistoryPrev();
}

BOOL WindowCommander::IsLoading()
{
	return m_window->IsLoading();
}

OpDocumentListener::SecurityMode WindowCommander::GetSecurityModeFromState(BYTE securityState)
{
	switch (m_window->GetCurrentURL().GetAttribute(URL::KSecurityStatus))
	{
	case SECURITY_STATE_FULL:
# ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	case SECURITY_STATE_FULL_EXTENDED:
	case SECURITY_STATE_SOME_EXTENDED:
# endif // SSL_CHECK_EXT_VALIDATION_POLICY
		switch (securityState)
		{
		case SECURITY_STATE_NONE:
		case SECURITY_STATE_LOW:
		case SECURITY_STATE_HALF:
			/* Signal that the security of the page is flawed, instead of
			   saying that it has low or no security, to give the UI an
			   opportunity to try to communicate the error to the user. */

			return OpDocumentListener::FLAWED_SECURITY;
		}
	}

	switch (securityState)
	{
	case SECURITY_STATE_NONE:
		return OpDocumentListener::NO_SECURITY;
	case SECURITY_STATE_LOW:
		return OpDocumentListener::LOW_SECURITY;
	case SECURITY_STATE_HALF:
		return OpDocumentListener::MEDIUM_SECURITY;
	case SECURITY_STATE_FULL:
		return OpDocumentListener::HIGH_SECURITY;
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	case SECURITY_STATE_FULL_EXTENDED:
		return OpDocumentListener::EXTENDED_SECURITY;
	case SECURITY_STATE_SOME_EXTENDED:
		return OpDocumentListener::SOME_EXTENDED_SECURITY;
#endif // SSL_CHECK_EXT_VALIDATION_POLICY
	}

	return OpDocumentListener::UNKNOWN_SECURITY;
}

OpDocumentListener::SecurityMode WindowCommander::GetSecurityMode()
{
	return GetSecurityModeFromState(m_window->GetSecurityState());
}

BOOL WindowCommander::GetPrivacyMode()
{
	return m_window->GetPrivacyMode();
}

void WindowCommander::SetPrivacyMode(BOOL privacy_mode)
{
	m_window->SetPrivacyMode(privacy_mode);
}

DocumentURLType WindowCommander::GetURLType()
{
	return URLType2DocumentURLType(m_window->GetCurrentURL().Type());
}

UINT32 WindowCommander::SecurityLowStatusReason()
{
	return m_window->GetCurrentURL().GetAttribute(URL::KSecurityLowStatusReason);
}

const uni_char * WindowCommander::ServerUniName() const
{
	if (m_window->GetCurrentURL().GetServerName())
		return m_window->GetCurrentURL().GetServerName()->UniName();
	else
		return UNI_L("");
}

#ifdef SECURITY_INFORMATION_PARSER
OP_STATUS WindowCommander::CreateSecurityInformationParser(OpSecurityInformationParser ** parser)
{
	SecurityInformationParser *sip = OP_NEW(SecurityInformationParser, ());
	if (!sip)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS sts = sip->SetSecInfo(m_window->GetCurrentURL().GetAttribute(URL::KSecurityInformationURL));
	if (OpStatus::IsSuccess(sts))
		sts = sip->Parse();
	if (OpStatus::IsSuccess(sts))
		*parser = sip;
	else
		OP_DELETE(sip);
	return sts;
}

void WindowCommander::GetSecurityInformation(OpString &text, BOOL isEV)
{
	SecurityInformationParser *sip = OP_NEW(SecurityInformationParser, ());
	if (!sip)
		return;
	OP_STATUS sts = sip->SetSecInfo(m_window->GetCurrentURL().GetAttribute(URL::KSecurityInformationURL));
	if (OpStatus::IsSuccess(sts))
		sip->LimitedParse(text, isEV);
	OP_DELETE(sip);
}

#endif // SECURITY_INFORMATION_PARSER

#ifdef TRUST_INFO_RESOLVER
OP_STATUS WindowCommander::CreateTrustInformationResolver(OpTrustInformationResolver ** resolver, OpTrustInfoResolverListener * listener)
{
	TrustInformationResolver *tir = OP_NEW(TrustInformationResolver, (listener));
	*resolver = tir;
	if (tir)
	{
		tir->SetServerURL(m_window->GetCurrentURL());
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}
#endif // TRUST_INFO_RESOLVER

OpWindowCommander::WIC_URLType WindowCommander::GetWicURLType()
{
	WIC_URLType wic_type = WIC_URLType_Unknown;

	URLType url_type = m_window->GetCurrentURL().Type();

	switch(url_type)
	{
	case URL_HTTP:
		wic_type = WIC_URLType_HTTP;
		break;
	case URL_HTTPS:
		wic_type = WIC_URLType_HTTPS;
		break;
	case URL_FTP:
		wic_type = WIC_URLType_FTP;
		break;
	}
	return wic_type;
}

#ifdef TRUST_RATING
BOOL3 WindowCommander::HttpResponseIs200()
{
	if (m_window->GetCurrentDoc() == NULL)
		return MAYBE;
	else
		return m_window->GetCurrentDoc()->GetURL().GetAttribute(URL::KHTTP_Response_Code) == 200 ? YES : NO;
}

OpDocumentListener::TrustMode WindowCommander::GetTrustMode()
{
	switch (m_window->GetTrustRating())
	{
		case Domain_Trusted:
			return OpDocumentListener::TRUSTED_DOMAIN;
		case Unknown_Trust:
			return OpDocumentListener::UNKNOWN_DOMAIN;
		case Phishing:
			return OpDocumentListener::PHISHING_DOMAIN;
		case Malware:
			return OpDocumentListener::MALWARE_DOMAIN;
		case Not_Set:
		default:
			return OpDocumentListener::TRUST_NOT_SET;
	}
}

OP_STATUS WindowCommander::CheckDocumentTrust(BOOL force, BOOL offline)
{
	return m_window->CheckTrustRating(force, offline);
}

#endif // TRUST_RATING

#ifdef WEB_TURBO_MODE
BOOL WindowCommander::GetTurboMode()
{
	return m_window->GetTurboMode();
}

void WindowCommander::SetTurboMode(BOOL turbo_mode)
{
	m_window->SetTurboMode(turbo_mode);
}
#endif // WEB_TURBO_MODE

URL_CONTEXT_ID WindowCommander::GetURLContextID()
{
	return m_window->GetUrlContextId();
}

const uni_char* WindowCommander::GetCurrentURL(BOOL showPassword)
{
	URL url =  m_window->GetCurrentURL();
	URL::URL_UniNameStringAttribute attr =
#ifdef WIC_REMOVE_USERNAME_FROM_URL
		showPassword ? URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI : URL::KUniName_With_Fragment;
#else
		showPassword ? URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI : URL::KUniName_With_Fragment_Username_Password_Hidden;
#endif
	const uni_char* url_str = url.GetAttribute(attr).CStr(); // Use Escaped?

	if (url.IsEmpty() || url_str == NULL)
	{
		url_str = UNI_L("");
		return url_str;
	}
	else
	{
		return url_str;
	}
}

const uni_char* WindowCommander::GetLoadingURL(BOOL showPassword)
{
	URL url =  m_window->GetCurrentLoadingURL();
	URL::URL_UniNameStringAttribute attr =
#ifdef WIC_REMOVE_USERNAME_FROM_URL
		showPassword ? URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI : URL::KUniName_With_Fragment;
#else
		showPassword ? URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI : URL::KUniName_With_Fragment_Username_Password_Hidden;
#endif
	const uni_char* url_str = url.GetAttribute(attr).CStr(); // Use Escaped?

	if (url.IsEmpty() || url_str == NULL)
	{
		url_str = UNI_L("");
		return url_str;
	}
	else
	{
		return url_str;
	}
}

const uni_char* WindowCommander::GetCurrentTitle()
{
	return m_window->GetWindowTitle();
}

OP_STATUS WindowCommander::GetCurrentMIMEType(OpString8& mime_type, BOOL original)
{
	URL url = m_window->GetCurrentURL();

	RETURN_IF_ERROR(mime_type.Set(url.GetAttribute(original ? URL::KOriginalMIME_Type : URL::KMIME_Type)));
	if (mime_type.IsEmpty())
		RETURN_IF_ERROR(mime_type.Set(""));
	return OpStatus::OK;
}

OP_STATUS WindowCommander::GetServerMIMEType(OpString8& mime_type)
{
	URL url = m_window->GetCurrentURL();

	RETURN_IF_ERROR(mime_type.Set(url.GetAttribute(URL::KServerMIME_Type )));
	if (mime_type.IsEmpty())
		RETURN_IF_ERROR(mime_type.Set(""));
	return OpStatus::OK;
}

unsigned int WindowCommander::GetCurrentHttpResponseCode()
{
	return m_window->GetCurrentShownURL().GetAttribute(URL::KHTTP_Response_Code);
}

BOOL WindowCommander::GetHistoryInformation(int index, HistoryInformation* result)
{
	DocumentManager* doc_man = m_window->DocManager();
	DocListElm* elm = doc_man->FirstDocListElm();
	while (elm)
	{
		if (elm->Number() == index)
		{
			result->number = index;
			result->id = elm->GetID();
			result->url = elm->GetUrl().GetAttribute(URL::KUniName);
			result->title = elm->Title();
			result->server_name = NULL;
			if (elm->GetUrl().GetServerName())
				result->server_name = elm->GetUrl().GetServerName()->UniName();

			return TRUE;
		}

		elm = elm->Suc();
	}

	return FALSE;
}


#ifdef HISTORY_SUPPORT
void WindowCommander::DisableGlobalHistory()
{
	m_window->DisableGlobalHistory();
}
#endif // HISTORY_SUPPORT

unsigned long WindowCommander::GetWindowId()
{
	return m_window->Id();
}

#ifdef HAS_ATVEF_SUPPORT
OP_STATUS WindowCommander::ExecuteES(const uni_char* script)
{
	DocumentManager* docman = m_window->DocManager();
	if (docman == NULL)
	{
		return OpStatus::ERR;
	}

	FramesDocument* frm_doc = docman->GetCurrentDoc();

	if (frm_doc == NULL)
	{
		return OpStatus::ERR;
	}

	return frm_doc->ExecuteES(script);
}
#endif // HAS_ATVEF_SUPPORT

void WindowCommander::SetScale(UINT32 scale)
{
	m_window->SetScale(scale);
}

UINT32 WindowCommander::GetScale()
{
	return m_window->GetScale();
}

void WindowCommander::SetTrueZoom(BOOL enable)
{
	m_window->SetTrueZoom(enable);
}

BOOL WindowCommander::GetTrueZoom()
{
	return m_window->GetTrueZoom();
}

void WindowCommander::SetTextScale(UINT32 scale)
{
    m_window->SetTextScale(scale);
    m_window->VisualDev()->SetTextScale(scale);
}

UINT32 WindowCommander::GetTextScale()
{
    return m_window->GetTextScale();
}

void WindowCommander::SetImageMode(OpDocumentListener::ImageDisplayMode mode)
{
	SHOWIMAGESTATE s;
	if (mode == OpDocumentListener::LOADED_IMAGES)
	{
		s = FIGS_SHOW;
	}
	else if (mode == OpDocumentListener::NO_IMAGES)
	{
		s = FIGS_OFF;
	}
	else
	{
		s = FIGS_ON;
	}

	m_window->SetImagesSetting(s);
}

OpDocumentListener::ImageDisplayMode WindowCommander::GetImageMode()
{
	BOOL load_img = m_window->LoadImages();
	BOOL show_img = m_window->ShowImages();

	if (load_img && show_img)
		return OpDocumentListener::ALL_IMAGES;
	else if (show_img)
		return OpDocumentListener::LOADED_IMAGES;
	else
		return OpDocumentListener::NO_IMAGES;
}

void WindowCommander::SetCssMode(OpDocumentListener::CssDisplayMode mode)
{
	m_window->SetCSSMode(mode == OpDocumentListener::CSS_AUTHOR_MODE ? CSS_FULL : CSS_NONE);
}

OpDocumentListener::CssDisplayMode WindowCommander::GetCssMode()
{
	if (m_window->GetCSSMode() == CSS_NONE)
	{
		return OpDocumentListener::CSS_USER_MODE;
	}
	return OpDocumentListener::CSS_AUTHOR_MODE;
}

CONST_ARRAY(wic_charsets, char*)
	// Charset name		Encoding
	CONST_ENTRY(""),                 // ENCODING_AUTOMATIC
	CONST_ENTRY("utf-8"),			// ENCODING_UTF8
	CONST_ENTRY("utf-16"),			// ENCODING_UTF16
	CONST_ENTRY("us-ascii"),		    // ENCODING_USASCII
	CONST_ENTRY("iso-8859-1"),		// ENCODING_ISO8859_1
	CONST_ENTRY("iso-8859-2"),		// ENCODING_ISO8859_2
	CONST_ENTRY("iso-8859-3"),		// ENCODING_ISO8859_3
	CONST_ENTRY("iso-8859-4"),		// ENCODING_ISO8859_4
	CONST_ENTRY("iso-8859-5"),		// ENCODING_ISO8859_5
	CONST_ENTRY("iso-8859-6"),		// ENCODING_ISO8859_6
	CONST_ENTRY("iso-8859-7"),		// ENCODING_ISO8859_7
	CONST_ENTRY("iso-8859-8"),		// ENCODING_ISO8859_8
	CONST_ENTRY("iso-8859-9"),		// ENCODING_ISO8859_9
	CONST_ENTRY("iso-8859-10"),	    // ENCODING_ISO8859_10
	CONST_ENTRY("iso-8859-11"),	    // ENCODING_ISO8859_11
	CONST_ENTRY("iso-8859-13"),	    // ENCODING_ISO8859_13
	CONST_ENTRY("iso-8859-14"),	    // ENCODING_ISO8859_14
	CONST_ENTRY("iso-8859-15"),	    // ENCODING_ISO8859_15
	CONST_ENTRY("iso-8859-16"),	    // ENCODING_ISO8859_16
	CONST_ENTRY("koi8-r"),			// ENCODING_KOI8_R
	CONST_ENTRY("koi8-u"),			// ENCODING_KOI8_U
	CONST_ENTRY("windows-1250"),	    // ENCODING_CP_1250
	CONST_ENTRY("windows-1251"),	    // ENCODING_CP_1251
	CONST_ENTRY("windows-1252"),     // ENCODING_CP_1252
	CONST_ENTRY("windows-1253"),     // ENCODING_CP_1253
	CONST_ENTRY("windows-1254"),	    // ENCODING_CP_1254
	CONST_ENTRY("windows-1255"),	    // ENCODING_CP_1255
	CONST_ENTRY("windows-1256"),	    // ENCODING_CP_1256
	CONST_ENTRY("windows-1257"),	    // ENCODING_CP_1257
	CONST_ENTRY("windows-1258"),	    // ENCODING_CP_1258
	CONST_ENTRY("shift_jis"),		// ENCODING_SHIFTJIS
	CONST_ENTRY("iso-2022-jp"),	    // ENCODING_ISO2022_JP
	CONST_ENTRY("iso-2022-jp-1"),	    // ENCODING_ISO2022_JP_1
	CONST_ENTRY("iso-2022-cn"),	    // ENCODING_ISO2022_CN
	CONST_ENTRY("iso-2022-kr"),	    // ENCODING_ISO2022_KR
	CONST_ENTRY("big5"),			    // ENCODING_BIG5
	CONST_ENTRY("big5-hkscs"),		    // ENCODING_BIG5_HKSCS
	CONST_ENTRY("euc-jp"),			// ENCODING_EUC_JP
	CONST_ENTRY("gb2312"),			// ENCODING_GB2312
	CONST_ENTRY("viscii"),			// ENCODING_VISCII
	CONST_ENTRY("euc-kr"),			// ENCODING_EUC_KR
	CONST_ENTRY("hz-gb-2312"),		// ENCODING_HZ_GB2312
	CONST_ENTRY("gbk"),			    // ENCODING_GBK
	CONST_ENTRY("euc-tw"),			// ENCODING_EUC_TW
	CONST_ENTRY("ibm866"),			// ENCODING_IBM866
	CONST_ENTRY("windows-874"),		// ENCODING_WINDOWS_874
	CONST_ENTRY("autodetect-jp"),	// ENCODING_JAPANESE_AUTOMATIC
	CONST_ENTRY("autodetect-zh"),	    // ENCODING_CHINESE_AUTOMATIC
	CONST_ENTRY("autodetect-ko"),	    // ENCODING_KOREAN_AUTOMATIC
	CONST_ENTRY("gb18030"),			    // ENCODING_GB18030
	CONST_ENTRY("macintosh"),			// ENCODING_MACROMAN
	CONST_ENTRY("x-mac-ce"),			// ENCODING_MAC_CE
	CONST_ENTRY("x-mac-cyrillic"),		// ENCODING_MAC_CYRILLIC
	CONST_ENTRY("x-mac-greek"),			// ENCODING_MAC_GREEK
	CONST_ENTRY("x-mac-turkish"),		// ENCODING_MAC_TURKISH
	CONST_ENTRY("x-user-defined")		// ENCODING_X_USER_DEFINED
CONST_END(wic_charsets)

void WindowCommander::ForceEncoding(WindowCommander::Encoding encoding)
{
	m_window->SetForceEncoding(wic_charsets[encoding]);
}

WindowCommander::Encoding WindowCommander::GetEncoding()
{
	const char* e = m_window->GetForceEncoding();

	if(!e || *e == 0) //!e => no encoding, *e == "" => ENCODING_AUTOMATIC
	{
		FramesDocument* frm_doc = m_window->GetCurrentDoc();
		if(frm_doc && frm_doc->GetHLDocProfile())
		{
			e = frm_doc->GetHLDocProfile()->GetCharacterSet();
		}
	}

	if (e)
	{
		for (int i=0; i < NUM_ENCODINGS; ++i)
		{
			if (op_strcmp(wic_charsets[i], e) == 0)
			{
				return (WindowCommander::Encoding)i;
			}
		}
	}

	return ENCODING_AUTOMATIC;
}

#ifdef FONTSWITCHING
WritingSystem::Script WindowCommander::GetPreferredScript(BOOL focused_frame)
{
	FramesDocument* doc = GetCurrentDocument(m_window, focused_frame);
	if (doc && doc->GetHLDocProfile())
		return doc->GetHLDocProfile()->GetPreferredScript();

	return WritingSystem::Unknown;
}
#endif

OP_STATUS WindowCommander::GetBackgroundColor(OpColor& background_color)
{
	FramesDocument* doc = m_window->GetActiveFrameDoc();
	if (!doc)
		return OpStatus::ERR;

	if (!doc->GetHLDocProfile())
		return OpStatus::ERR;

#ifdef CSS3_BACKGROUND
      COLORREF color = doc->GetHLDocProfile()->GetLayoutWorkplace()->GetDocRootProperties().bg_color;
#else
      COLORREF color = doc->GetHLDocProfile()->GetFontAndBgProps().doc_root_background.bg_color;
#endif // CSS3_BACKGROUND

	if (color == USE_DEFAULT_COLOR)
		color = m_window->GetDefaultBackgroundColor();

	color = HTM_Lex::GetColValByIndex(color);

	background_color.red   = OP_GET_R_VALUE(color);
	background_color.green = OP_GET_G_VALUE(color);
	background_color.blue  = OP_GET_B_VALUE(color);
	background_color.alpha = OP_GET_A_VALUE(color);

    return OpStatus::OK;
}

void WindowCommander::SetDefaultBackgroundColor(const OpColor& background_color)
{
	m_window->SetDefaultBackgroundColor(OP_RGBA(background_color.red,
												background_color.green,
												background_color.blue,
												background_color.alpha));
}

void WindowCommander::SetShowScrollbars(BOOL show)
{
	m_window->SetShowScrollbars(show);
}

#ifndef HAS_NO_SEARCHTEXT

WindowCommander::SearchResult WindowCommander::Search(const uni_char* search_string, const SearchInfo& search_info)
{
#ifndef SEARCH_MATCHES_ALL
	OP_ASSERT(!search_info.highlight_all); // highlight_all requires SEARCH_MATCHES_ALL.
#endif // !SEARCH_MATCHES_ALL

	if (!search_string || !*search_string)
	{
		ClearSelection();

		return SEARCH_NOT_FOUND;
	}

	/* Window::HighlightNextMatch() automatically determines whether to search
	   again at the same position (useful for incremental inline find) as the
	   previous match, or advance position. Window::SearchText(), on the other
	   hand, doesn't. That method uses its fifth parameter to tell. In order to
	   provide a consistent API to the OpWindowCommander user, detect search
	   string changes. */

	BOOL advance;

	if (m_searchtext.Compare(search_string) == 0)
		advance = TRUE;
	else
	{
		// Update string

		OP_STATUS st = m_searchtext.Set(search_string);

		if (OpStatus::IsError(st))
		{
			m_window->RaiseCondition(st);
			return SEARCH_NOT_FOUND;
		}

		advance = FALSE;
	}

#ifdef SEARCH_MATCHES_ALL
	if (search_info.highlight_all)
	{
		BOOL wrapped = FALSE;
		OpRect dummy;
		if (m_window->HighlightNextMatch(search_string,
										 search_info.case_sensitive,
										 search_info.whole_words_only,
										 search_info.links_only,
										 search_info.direction == DIR_FORWARD,
										 search_info.allow_wrapping,
										 wrapped, dummy))
		{
			if (wrapped)
				return SEARCH_FOUND_WRAPPED;

			return SEARCH_FOUND;
		}
		else
			return SEARCH_NOT_FOUND;
	}
	else
#endif // SEARCH_MATCHES_ALL
	{
		if (m_window->SearchText(search_string,
								 search_info.direction == DIR_FORWARD,
								 search_info.case_sensitive,
								 search_info.whole_words_only,
								 advance,
								 search_info.allow_wrapping,
								 search_info.links_only))
			return SEARCH_FOUND;
		else
			return SEARCH_NOT_FOUND;
	}
}

void WindowCommander::ResetSearchPosition()
{
	ClearSelection();
}

BOOL WindowCommander::GetSearchMatchRectangles(OpVector<OpRect> &all_rects, OpVector<OpRect> &active_rects)
{
	BOOL any_success = FALSE;
	DocumentTreeIterator iter(m_window);
	iter.SetIncludeThis();
	iter.SetIncludeEmpty();
	while (iter.Next())
	{
		if (iter.GetFramesDocument() && iter.GetFramesDocument()->GetVisualDevice())
			any_success |= iter.GetFramesDocument()->GetVisualDevice()->GetSearchMatchRectangles(all_rects, active_rects);
	}
	return any_success;
}

#endif // !HAS_NO_SEARCHTEXT

#ifndef HAS_NOTEXTSELECTION
#ifdef USE_OP_CLIPBOARD
void WindowCommander::Paste()
{
	g_input_manager->InvokeAction(OpInputAction::ACTION_PASTE);
}

void WindowCommander::Cut()
{
	g_input_manager->InvokeAction(OpInputAction::ACTION_CUT);
}

void WindowCommander::Copy()
{
	OP_ASSERT(g_clipboard_manager);

	OP_STATUS status = g_clipboard_manager->Copy(m_window, GetURLContextID(), m_window->GetActiveFrameDoc());
	if (OpStatus::IsMemoryError(status))
	{
		g_memory_manager->RaiseCondition(status);
	}
}
#endif // USE_OP_CLIPBOARD

void WindowCommander::SelectTextStart(const OpPoint& p)
{
	OpPoint local_p(p);
	selection_start_document = m_window->GetViewportController()->FindDocumentAtPosAndTranslate(local_p, FALSE);

	if (!selection_start_document)
		return;

	selection_start_document->StartSelection(local_p.x, local_p.y);
}

void WindowCommander::SelectTextMoveAnchor(const OpPoint& p)
{
	OpPoint local_p(p);
	FramesDocument *doc = m_window->GetViewportController()->FindDocumentAtPosAndTranslate(local_p, FALSE);

	if (!doc)
		return;

	// Check if there currently exist a selection in another frame. In that
	// case we should do nothing.
	if (HasSelectedText() && !doc->HasSelectedText(TRUE))
		return;

	selection_start_document = doc;

	selection_start_document->MoveSelectionAnchorPoint(local_p.x, local_p.y);
}

void WindowCommander::SelectTextEnd(const OpPoint& p)
{
	OpPoint local_p(p);
	FramesDocument* doc = m_window->GetViewportController()->FindDocumentAtPosAndTranslate(local_p, FALSE);

	if (!doc || doc != selection_start_document)
		return;

	// Updates the current end position of the selection
	selection_start_document->MoveSelectionFocusPoint(local_p.x, local_p.y, FALSE);
}

#ifdef _PRINT_SUPPORT_
OP_STATUS WindowCommander::Print(int from, int to, BOOL selected_only)
{
	PrinterInfo* info = OP_NEW(PrinterInfo, ());
	if (!info)
		return OpStatus::ERR_NO_MEMORY;

	info->last_page = to;

	PrintDevice* print_device = 0;
#ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
	Window * selection_window = 0;
	OP_STATUS rc = info->GetPrintDevice(print_device, FALSE, FALSE, m_window, &selection_window);
#else
	OP_STATUS rc = info->GetPrintDevice(print_device, FALSE, FALSE, m_window);
#endif
	if (OpStatus::IsError(rc))
	{
		// User cancelled from print dialog
		OP_DELETE(info);
		return OpStatus::OK;
	}

	if (print_device && print_device->StartPage())
	{
		Window* window = m_window;
#ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
		if (selection_window != 0)
			window = selection_window;
#endif

#ifdef WIC_LEAVE_PRINT_PREVIEW_MODE_WHEN_PRINTING
		if(window && window->GetPreviewMode())
			window->TogglePrintMode(TRUE);
#endif

		if (!window->StartPrinting(info, from, selected_only))
		{
			// Window destroys 'info' in case of error
			return OpStatus::ERR;
		}
	}
	else
	{
		OP_DELETE(info);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

void WindowCommander::CancelPrint()
{
	if (IsPrinting())
		m_window->StopPrinting();
}

void WindowCommander::PrintNextPage()
{
	m_window->PrintNextPage(m_window->GetPrinterInfo(FALSE));
}

BOOL WindowCommander::IsPrintable()
{
	return m_window->HasFeature(WIN_FEATURE_PRINTABLE);
}

BOOL WindowCommander::IsPrinting()
{
	return m_window->GetPrintMode();
}
#endif // _PRINT_SUPPORT_

void WindowCommander::SelectWord(const OpPoint& p)
{
	OpPoint local_p(p);
	selection_start_document = m_window->GetViewportController()->FindDocumentAtPosAndTranslate(local_p, FALSE);

	if (!selection_start_document)
		return;

	selection_start_document->SelectWord(local_p.x, local_p.y);
	selection_start_document->SetSelectingText(FALSE);
}

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
OP_STATUS WindowCommander::SelectNearbyLink(const OpPoint& p)
{
	OpPoint local_p(p);
	FramesDocument *doc = m_window->GetViewportController()->FindDocumentAtPosAndTranslate(local_p, FALSE);

	if (!doc)
		return OpStatus::ERR;

	OP_STATUS res = doc->SelectNearbyLink(p.x, p.y);
	if (OpStatus::IsSuccess(res))
		selection_start_document = doc;

	return res;
}
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

OP_STATUS WindowCommander::GetSelectTextStart(OpPoint& p, int &line_height)
{
	FramesDocument* doc = m_window->GetActiveFrameDoc();
	if (!doc)
		return OpStatus::ERR;

	OpPoint doc_p;
	OP_STATUS res = doc->GetSelectionAnchorPosition(doc_p.x, doc_p.y, line_height);
	if (OpStatus::IsError(res))
		return res;

	p = doc->GetVisualDevice()->GetView()->ConvertToScreen(doc_p);
	return res;
}

OP_STATUS WindowCommander::GetSelectTextEnd(OpPoint& p, int &line_height)
{
	FramesDocument* doc = m_window->GetActiveFrameDoc();
	if (!doc)
		return OpStatus::ERR;

	OpPoint doc_p;
	OP_STATUS res = doc->GetSelectionFocusPosition(doc_p.x, doc_p.y, line_height);
	if (OpStatus::IsError(res))
		return res;

	p = doc->GetVisualDevice()->GetView()->ConvertToScreen(doc_p);
	return res;
}

BOOL WindowCommander::HasSelectedText()
{
	FramesDocument *doc = m_window->GetActiveFrameDoc();
	return doc && doc->HasSelectedText(TRUE);
}

uni_char *WindowCommander::GetSelectedText()
{
	FramesDocument *doc = m_window->GetActiveFrameDoc();

	long len = 0;
	uni_char *buf = NULL;

	if (doc && HasSelectedText())
	{
		len = doc->GetSelectedTextLen(TRUE);
		buf = OP_NEWA(uni_char, len + 1);
		if (buf)
			doc->GetSelectedText(buf, len + 1, TRUE);
	}
	else
		buf = OP_NEWA(uni_char, 1);

	if (buf)
		buf[len] = 0;

	return buf;
}

void WindowCommander::ClearSelection()
{
#ifndef HAS_NO_SEARCHTEXT
	m_searchtext.Empty();
#endif // !HAS_NO_SEARCHTEXT

	if (FramesDocument* doc = m_window->GetActiveFrameDoc())
		doc->ClearSelection(FALSE);

	selection_start_document = NULL;
}

# ifdef KEYBOARD_SELECTION_SUPPORT

OP_STATUS WindowCommander::SetKeyboardSelectable(BOOL enable)
{
	if (FramesDocument* doc = m_window->GetActiveFrameDoc())
		return doc->SetKeyboardSelectable(enable);
	return OpStatus::OK;
}

void WindowCommander::MoveCaret(CaretMovementDirection direction, BOOL in_selection_mode)
{
	if (FramesDocument* doc = m_window->GetActiveFrameDoc())
		doc->MoveCaret(direction, in_selection_mode);
}

OP_STATUS WindowCommander::GetCaretRect(OpRect& rect)
{
	if (FramesDocument* doc = m_window->GetActiveFrameDoc())
		return doc->GetCaretRect(rect);
	return OpStatus::OK;
}

OP_STATUS WindowCommander::SetCaretPosition(const OpPoint& position)
{
	if (FramesDocument* doc = m_window->GetActiveFrameDoc())
	{
		OpPoint local_p(position);
		selection_start_document = m_window->GetViewportController()->FindDocumentAtPosAndTranslate(local_p, FALSE);

		if (!selection_start_document || !doc->GetHtmlDocument())
			return OpStatus::ERR;

		return doc->GetHtmlDocument()->SetCollapsedSelection(local_p.x, local_p.y);
	}

	return OpStatus::OK;
}

# endif // KEYBOARD_SELECTION_SUPPORT
#endif // !HAS_NOTEXTSELECTION

#ifdef USE_OP_CLIPBOARD
BOOL WindowCommander::CopyImageToClipboard(const uni_char* url)
{
	URL u = urlManager->GetURL(url);
	OP_STATUS status = g_clipboard_manager->CopyImageToClipboard(u, GetURLContextID());
	if (OpStatus::IsError(status))
	{
		if (OpStatus::IsMemoryError(status))
			g_memory_manager->RaiseCondition(status);
		return FALSE;
	}

	return TRUE;
}

void WindowCommander::CopyLinkToClipboard(OpDocumentContext& ctx)
{
	OpString str;
	if (ctx.HasLink() &&
		OpStatus::IsSuccess(ctx.GetLinkAddress(&str)) &&
		str.HasContent())
	{
		const uni_char* link_address = str.CStr();
		g_clipboard_manager->CopyURLToClipboard(link_address);
	}
}

#endif // USE_OP_CLIPBOARD

#ifdef SHORTCUT_ICON_SUPPORT
BOOL WindowCommander::HasDocumentIcon()
{
	return m_window->HasWindowIcon();
}

OP_STATUS WindowCommander::GetDocumentIconURL(const uni_char** iconUrl)
{
	if (!m_window->HasWindowIcon())
	{
		return OpStatus::ERR;
	}
	*iconUrl = m_window->GetWindowIconURL().GetAttribute(URL::KUniName_Username_Password_Hidden).CStr();
	return OpStatus::OK;
}

#if WIC_MAX_FAVICON_SIZE > 0
static OP_STATUS ScaleDocumentIcon(OpBitmap** dest, OpBitmap* src, UINT32 dst_width, UINT32 dst_height, UINT32 src_width, UINT32 src_height)
{
	// This uses nice bilinear scaling if the TWEAK_THUMBNAILS_INTERNAL_BITMAP_SCALE tweak is enabled in the thumbnails module.
#ifdef CORE_THUMBNAIL_SUPPORT
	*dest = OpThumbnail::ScaleBitmap(src, dst_width, dst_height, OpRect(0, 0, dst_width, dst_height), OpRect(0, 0, src_width, src_height));
	return *dest ? OpStatus::OK : OpStatus::ERR;
#else
	OP_STATUS status = OpBitmap::Create(dest, dst_width, dst_height, src->IsTransparent(), src->HasAlpha(), 0, 0, TRUE);
	if (OpStatus::IsError(status))
		return status;

	OpPainter* painter = (*dest)->GetPainter();
	if (!painter)
	{
		OP_DELETE(*dest);
		*dest = NULL;
		return OpStatus::ERR;
	}
	painter->DrawBitmapScaled(src, OpRect(0, 0, src_width, src_height), OpRect(0, 0, dst_width, dst_height));

	(*dest)->ReleasePainter();
	return status;
#endif
}
#endif // WIC_MAX_FAVICON_SIZE > 0

OP_STATUS WindowCommander::GetDocumentIcon(OpBitmap** iconBmp)
{
	OP_STATUS status = OpStatus::ERR;
	if (!m_window->HasWindowIcon())
	{
		return status;
	}

	Image icon = m_window->GetWindowIcon();

	icon.IncVisible(null_image_listener);

	if (!icon.IsEmpty() && icon.Height() && icon.Width() && icon.GetLastDecodedLine())
	{
		OpBitmap* bmp = icon.GetBitmap(NULL);
		if (bmp != NULL)
		{
			UINT32 src_width  = bmp->Width();
			UINT32 src_height = bmp->Height();
#if WIC_MAX_FAVICON_SIZE > 0
			if (src_width > WIC_MAX_FAVICON_SIZE || src_height > WIC_MAX_FAVICON_SIZE)
			{
				UINT32 dst_width  = MIN(WIC_MAX_FAVICON_SIZE, src_width);
				UINT32 dst_height = MIN(WIC_MAX_FAVICON_SIZE, src_height);

				// Scale keeping aspect ratio.
				if (src_height * dst_width > src_width * dst_height)
					dst_width = dst_height * src_width / src_height;
				else
					dst_height = dst_width * src_height / src_width;

				// Compensate for the number of decoded lines, but keep the aspect ratio
				UINT32 decoded_height = MIN(src_height, icon.GetLastDecodedLine());
				dst_height = dst_height * decoded_height / src_height;

				status = ScaleDocumentIcon(iconBmp, bmp, MAX(1, dst_width), MAX(1, dst_height), src_width, decoded_height);
			}
			else
#endif // WIC_MAX_FAVICON_SIZE > 0
			{
				status = OpBitmap::Create(iconBmp, src_width, src_height, bmp->IsTransparent(), bmp->HasAlpha(), 0, 0, FALSE);

				if (OpStatus::IsSuccess(status))
				{
					UINT32 lineCount = MIN(src_height, icon.GetLastDecodedLine());
					UINT32* data = OP_NEWA(UINT32, src_width);

					if (data)
					{
						for (UINT32 i = 0 ; i < lineCount ; i++)
						{
							if (bmp->GetLineData(data, i) == TRUE)
								(*iconBmp)->AddLine(data, i);
						}

						OP_DELETEA(data);
					}
					else
					{
						OP_DELETE(*iconBmp);
						*iconBmp = NULL;
						g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						status = OpStatus::ERR_NO_MEMORY;
					}
				}
			}
			icon.ReleaseBitmap();
		}
	}

	icon.DecVisible(null_image_listener);

	if (OpStatus::IsMemoryError(status))
	{
		g_memory_manager->RaiseCondition(status);
	}
	return status;
}
#endif // SHORTCUT_ICON_SUPPORT

#ifdef LINK_SUPPORT
UINT32 WindowCommander::GetLinkElementCount()
{
	const LinkElementDatabase* link_db = m_window->GetLinkDatabase();
	return link_db ? link_db->GetSubLinkCount() : 0;
}

BOOL WindowCommander::GetLinkElementInfo(UINT32 index, LinkElementInfo* information)
{
	if (information == NULL)
	{
		return FALSE;
	}
	const LinkElementDatabase* link_db = m_window->GetLinkDatabase();
	if (!link_db)
	{
		return FALSE;
	}

	unsigned sub_index;
	const LinkElement* link = link_db->GetFromSubLinkIndex(index, sub_index);

	if (link == NULL)
	{
		return FALSE;
	}

	unsigned kinds = link->GetKinds();

	// Find the sub_indexth set bit in kinds, ALTERNATE being ignored if STYLESHEET is
	// also set (because then it's not a type of its own, just a modifier on stylesheet)
	LinkType kind = LINK_TYPE_OTHER;
	for (int i = 0; i < 32; i++)
	{
		LinkType type = static_cast<LinkType>(1 << i);
		if (kinds & type)
		{
			if (type == LINK_TYPE_ALTERNATE && (kinds & LINK_TYPE_STYLESHEET))
				continue;

			if (sub_index == 0)
			{
				kind = type;
				break;
			}
			sub_index--;
		}
	}

	switch (kind)
	{
		case LINK_TYPE_ALTERNATE:
			information->kind = LinkElementInfo::OP_LINK_ELT_ALTERNATE;
			break;
		case LINK_TYPE_STYLESHEET:
			if (kinds & LINK_TYPE_ALTERNATE)
				information->kind = LinkElementInfo::OP_LINK_ELT_ALTERNATE_STYLESHEET;
			else
				information->kind = LinkElementInfo::OP_LINK_ELT_STYLESHEET;
			break;
		case LINK_TYPE_START:
			information->kind = LinkElementInfo::OP_LINK_ELT_START;
			break;
		case LINK_TYPE_NEXT:
			information->kind = LinkElementInfo::OP_LINK_ELT_NEXT;
			break;
		case LINK_TYPE_PREV:
			information->kind = LinkElementInfo::OP_LINK_ELT_PREV;
			break;
		case LINK_TYPE_CONTENTS:
			information->kind = LinkElementInfo::OP_LINK_ELT_CONTENTS;
			break;
		case LINK_TYPE_INDEX:
			information->kind = LinkElementInfo::OP_LINK_ELT_INDEX;
			break;
		case LINK_TYPE_GLOSSARY:
			information->kind = LinkElementInfo::OP_LINK_ELT_GLOSSARY;
			break;
		case LINK_TYPE_COPYRIGHT:
			information->kind = LinkElementInfo::OP_LINK_ELT_COPYRIGHT;
			break;
		case LINK_TYPE_CHAPTER:
			information->kind = LinkElementInfo::OP_LINK_ELT_CHAPTER;
			break;
		case LINK_TYPE_SECTION:
			information->kind = LinkElementInfo::OP_LINK_ELT_SECTION;
			break;
		case LINK_TYPE_SUBSECTION:
			information->kind = LinkElementInfo::OP_LINK_ELT_SUBSECTION;
			break;
		case LINK_TYPE_APPENDIX:
			information->kind = LinkElementInfo::OP_LINK_ELT_APPENDIX;
			break;
		case LINK_TYPE_HELP:
			information->kind = LinkElementInfo::OP_LINK_ELT_HELP;
			break;
		case LINK_TYPE_BOOKMARK:
			information->kind = LinkElementInfo::OP_LINK_ELT_BOOKMARK;
			break;
		case LINK_TYPE_ICON:
			information->kind = LinkElementInfo::OP_LINK_ELT_ICON;
			break;
		case LINK_TYPE_END:
			information->kind = LinkElementInfo::OP_LINK_ELT_END;
			break;
		case LINK_TYPE_FIRST:
			information->kind = LinkElementInfo::OP_LINK_ELT_FIRST;
			break;
		case LINK_TYPE_LAST:
			information->kind = LinkElementInfo::OP_LINK_ELT_LAST;
			break;
		case LINK_TYPE_UP:
			information->kind = LinkElementInfo::OP_LINK_ELT_UP;
			break;
		case LINK_TYPE_FIND:
			information->kind = LinkElementInfo::OP_LINK_ELT_FIND;
			break;
		case LINK_TYPE_AUTHOR:
			information->kind = LinkElementInfo::OP_LINK_ELT_AUTHOR;
			break;
		case LINK_TYPE_MADE:
			information->kind = LinkElementInfo::OP_LINK_ELT_MADE;
			break;
		case LINK_TYPE_ARCHIVES:
			information->kind = LinkElementInfo::OP_LINK_ELT_ARCHIVES;
			break;
		case LINK_TYPE_NOFOLLOW:
			information->kind = LinkElementInfo::OP_LINK_ELT_NOFOLLOW;
			break;
		case LINK_TYPE_NOREFERRER:
			information->kind = LinkElementInfo::OP_LINK_ELT_NOREFERRER;
			break;
		case LINK_TYPE_PINGBACK:
			information->kind = LinkElementInfo::OP_LINK_ELT_PINGBACK;
			break;
		case LINK_TYPE_PREFETCH:
			information->kind = LinkElementInfo::OP_LINK_ELT_PREFETCH;
			break;
		case LINK_TYPE_APPLE_TOUCH_ICON:
			information->kind = LinkElementInfo::OP_LINK_ELT_APPLE_TOUCH_ICON;
			break;
		case LINK_TYPE_IMAGE_SRC:
			information->kind = LinkElementInfo::OP_LINK_ELT_IMAGE_SRC;
			break;
		case LINK_TYPE_APPLE_TOUCH_ICON_PRECOMPOSED:
			information->kind = LinkElementInfo::OP_LINK_ELT_APPLE_TOUCH_ICON_PRECOMPOSED;
			break;
		case LINK_TYPE_OTHER: // unknown
			information->kind = LinkElementInfo::OP_LINK_ELT_OTHER;
			break;
		default:
			information->kind = LinkElementInfo::OP_LINK_ELT_OTHER;
			OP_ASSERT(FALSE); //notify rikard
			break;
	}

	information->title = link->GetTitle();

	URL *href = NULL;
	if (FramesDocument *frm_doc = m_window->DocManager()->GetCurrentDoc())
		href = link->GetHRef(frm_doc->GetLogicalDocument());
	information->href = href ? href->GetAttribute(URL::KUniName_With_Fragment).CStr() : NULL;

	// The caller sometimes does his own parsing of the rel string (yes, bad)
	// and since it's such a big difference between "stylesheet" and
	// "alternate stylesheet" we have to output "alternate stylesheet"
	// explicitly.
	if (kind == LINK_TYPE_STYLESHEET && (kinds & LINK_TYPE_ALTERNATE))
		information->rel = UNI_L("alternate stylesheet");
	else
		information->rel = link->GetStringForKind(kind);
	information->type = link->GetType();
	return TRUE;
}
#endif // LINK_SUPPORT

void NullLinkListener::OnAlternateCssAvailable(OpWindowCommander* commander)
{
#ifdef DEBUG_ALTERNATIVE_CSS
	OP_NEW_DBG("NullLinkListener::OnAlternativeCssAvailable", "windowcommander");

	CssInformation info;

	for (UINT32 i = 0 ; ; i++)
	{
		if (!commander->GetAlternateCssInformation(i, &info))
			break;
		OP_DBG((info.title ? info.title : UNI_L("empty title")));
		OP_DBG((info.href ? info.href : UNI_L("empty href")));
	}
#endif // DEBUG_ALTERNATIVE_CSS
}

const CSS* WindowCommander::GetCSS(UINT32 index) const
{
	size_t counter = 0;
	// we only want to return duplicate items (identical names) once. O(n2), not good :-/

	CSSCollection* coll = m_window->GetCSSCollection();
	if (coll)
	{
		CSSCollection::Iterator iter(coll, CSSCollection::Iterator::STYLESHEETS);
		while (CSSCollectionElement* elm = iter.Next())
		{
			CSS* css = static_cast<CSS*>(elm);
			if (css->GetKind() == CSS::STYLE_PERSISTENT)
				continue; // don't bother

			const uni_char* title = css->GetTitle();
			CSSCollectionElement* suc_elm = NULL;
			if (title)
			{
				CSSCollection::Iterator suc_iter(iter);
				while ((suc_elm = suc_iter.Next()) != NULL)
				{
					CSS* suc_css = static_cast<CSS*>(suc_elm);
					const uni_char* suc_title = suc_css->GetTitle();
					if (suc_title && uni_str_eq(title, suc_title))
						break;
				}
			}

			if (suc_elm == NULL) // found no duplicate
				counter++;

			if (counter == index + 1)
				return css;
		}
	}

	return NULL;
}

BOOL WindowCommander::GetAlternateCssInformation(UINT32 index, CssInformation* information)
{
	const CSS* css = WindowCommander::GetCSS(index);
	if (information == NULL || css == NULL)
	{
		return FALSE;
	}

	switch(css->GetKind())
	{
		case CSS::STYLE_PREFERRED:
			information->kind = CssInformation::OP_CSS_STYLE_PREFERRED;
			break;
		case CSS::STYLE_ALTERNATE:
			information->kind = CssInformation::OP_CSS_STYLE_ALTERNATE;
			break;
		default:
			OP_ASSERT(FALSE);
			return FALSE;
	}

	const URL* href = css->GetHRef();
	information->title = css->GetTitle();
	information->enabled = css->IsEnabled();
	if(href)
		information->href = href->GetAttribute(URL::KUniName).CStr();
	return TRUE;
}

BOOL WindowCommander::SetAlternateCss(const uni_char* title)
{
	CSSCollection* coll = m_window->GetCSSCollection();
	if (coll)
	{
		CSSCollection::Iterator iter(coll, CSSCollection::Iterator::STYLESHEETS);
		while (CSSCollectionElement* elm = iter.Next())
		{
			CSS* css = static_cast<CSS*>(elm);
			BOOL enabled = css->GetKind() == CSS::STYLE_PERSISTENT ||
						   (!css->IsEnabled() && // toggle
							 css->GetTitle() &&
							 uni_str_eq(css->GetTitle(), title));
			css->SetEnabled(enabled);
		}
	}

	OP_STATUS status = m_window->UpdateWindow(TRUE);
	if (OpStatus::IsError(status))
	{
		g_memory_manager->RaiseCondition(status);
		return FALSE;
	}
	return TRUE;
}

BOOL WindowCommander::ShowCurrentImage()
{
	BOOL retval = FALSE;
	if (!HasCurrentElement() || !m_window || !currentElement || currentElement->GetImageURL().IsEmpty())
	{
		return FALSE;
	}

	DocumentManager* doc_man = m_window->DocManager();
	if (doc_man)
	{
		FramesDocument *doc = m_window->GetActiveFrameDoc();
		if (doc)
		{
			if (!m_window->ShowImages())
			{
				m_window->SetImagesSetting(FIGS_SHOW);
			}

			BOOL is_reloading = doc_man->GetReload();

			if (!is_reloading)
			{
				doc_man->SetReload(TRUE); // force reload of images
			}

			URL url = currentElement->GetImageURL();

			OP_STATUS status = ((FramesDocument*)doc)->LoadInline(&url, currentElement, IMAGE_INLINE, LoadInlineOptions().ReloadConditionally());
			if (OpStatus::IsSuccess(status))
			{
				WinState state = m_window->GetState();
				if (state == NOT_BUSY || state == RESERVED)
				{
					status = m_window->StartProgressDisplay(TRUE, FALSE);
					if (OpStatus::IsMemoryError(status))
					{
						g_memory_manager->RaiseCondition(status);
					}
					m_window->SetState(BUSY);
					m_window->SetState(CLICKABLE);
					retval = TRUE;
				}
			}
			else
			{
				g_memory_manager->RaiseCondition(status);
			}

			m_window->SetSecurityState(doc->GetURL().GetAttribute(URL::KSecurityStatus), TRUE, doc->GetURL().GetAttribute(URL::KSecurityText).CStr());

			if (!is_reloading)
			{
				doc_man->SetReload(FALSE);
			}
		}
	}
	return retval;
}

OpWindowCommander::LayoutMode WindowCommander::GetLayoutMode()
{
	if (m_window->GetERA_Mode())
	{
		return ERA;
	}

	::LayoutMode mode = m_window->GetLayoutMode();
	switch (mode)
	{
	case LAYOUT_SSR:
		return SSR;
	case LAYOUT_CSSR:
		return CSSR;
	case LAYOUT_MSR:
		return MSR;
	case LAYOUT_AMSR:
		return AMSR;
	case LAYOUT_NORMAL:
		return NORMAL;
#ifdef TV_RENDERING
	case LAYOUT_TVR:
		return TVR;
#endif // TV_RENDERING
	}

	OP_ASSERT(FALSE);
	return NORMAL;
}

void WindowCommander::SetLayoutMode(OpWindowCommander::LayoutMode mode)
{
	::LayoutMode new_mode = LAYOUT_NORMAL;
	BOOL era = FALSE;

	switch (mode)
	{
	case NORMAL:
		new_mode = LAYOUT_NORMAL;
		break;
	case SSR:
		new_mode = LAYOUT_SSR;
		break;
	case CSSR:
		new_mode = LAYOUT_CSSR;
		break;
	case AMSR:
		new_mode = LAYOUT_AMSR;
		break;
	case MSR:
		new_mode = LAYOUT_MSR;
		break;
#ifdef TV_RENDERING
	case TVR:
		new_mode = LAYOUT_TVR;
		break;
#endif // TV_RENDERING
	case ERA:
		era = TRUE;
		break;
	default:
		OP_ASSERT(FALSE);
	}

	m_window->SetERA_Mode(era);

	if (!era)
	{
		m_window->SetLayoutMode(new_mode);
	}
}

void WindowCommander::SetSmartFramesMode(BOOL enable)
{
	m_window->SetFramesPolicy(enable ? FRAMES_POLICY_SMART_FRAMES : FRAMES_POLICY_DEFAULT);
}

BOOL WindowCommander::GetSmartFramesMode()
{
	return m_window->GetFramesPolicy() == FRAMES_POLICY_SMART_FRAMES;
}

BOOL WindowCommander::HasCurrentElement()
{
	return currentElement != NULL ? TRUE : FALSE;
}

void WindowCommander::Focus()
{
	m_window->VisualDev()->SetFocus(FOCUS_REASON_ACTIVATE);
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	OpScopeWindowListener::OnActiveWindowChanged(m_window);
#endif
}

OP_STATUS WindowCommander::SetWindowTitle(const OpString& str, BOOL force, BOOL generated)
{
	return m_window->SetWindowTitle(str, force, generated);
}

#ifdef WIC_MAX_FAVICON_SIZE
#include "modules/thumbnails/src/thumbnail.h"
#endif

OP_STATUS WindowCommander::InitiateTransfer(OpWindowCommander* wic, uni_char* url, OpTransferItem::TransferAction action, const uni_char* filename)
{
#ifndef WINDOW_COMMANDER_TRANSFER
	return OpStatus::ERR;
#else // WINDOW_COMMANDER_TRANSFER
	URL temp_url = urlManager->GetURL(url);
	return InitiateTransfer(m_window->DocManager(), temp_url, action, filename);
#endif
}

void WindowCommander::CancelTransfer(OpWindowCommander* wic, const uni_char* url)
{
#ifdef WINDOW_COMMANDER_TRANSFER
	URL temp_url = urlManager->GetURL(url);
	CancelTransfer(m_window->DocManager(), temp_url);
#endif // WINDOW_COMMANDER_TRANSFER
}

#ifdef SUPPORT_GENERATE_THUMBNAILS
ThumbnailGenerator* WindowCommander::CreateThumbnailGenerator()
{
	return OP_NEW(ThumbnailGenerator, (m_window));
}
#endif // SUPPORT_GENERATE_THUMBNAILS

#ifdef SCOPE_ECMASCRIPT_DEBUGGER
OP_STATUS WindowCommander::DragonflyInspect(OpDocumentContext& context)
{
	return FramesDocument::DragonflyInspect(context);
}
#endif // SCOPE_ECMASCRIPT_DEBUGGER

#ifdef INTEGRATED_DEVTOOLS_SUPPORT
BOOL WindowCommander::OpenDevtools(const uni_char *url)
{
#ifdef PREFS_HAVE_STRING_API
	OpString pref;
	if (!url)
	{
		BOOL pref_read = FALSE;
		TRAPD(status, pref_read = g_prefsManager->GetPreferenceL("Developer Tools", "Developer Tools URL", pref));
		if (OpStatus::IsSuccess(status) && pref_read)
			url = pref.CStr();
	}
#endif // PREFS_HAVE_STRING_API
	RETURN_VALUE_IF_NULL(url, FALSE);

	m_window->SetType(WIN_TYPE_DEVTOOLS);
	return OpenURL(url, FALSE);
}
#endif // INTEGRATED_DEVTOOLS_SUPPORT

OP_STATUS WindowCommander::GetElementRect(OpDocumentContext& context, OpRect& rect)
{
	return FramesDocument::GetElementRect(context, rect);
}

#ifdef WINDOW_COMMANDER_TRANSFER
OP_STATUS WindowCommander::InitiateTransfer(DocumentManager* docManager, URL& url, OpTransferItem::TransferAction action, const uni_char* filename)
{
	RETURN_IF_ERROR(((TransferManager*)g_transferManager)->AddTransferItem(
			url,
			filename,
			action,
			FALSE,
			0,
			TransferItem::TRANSFERTYPE_DOWNLOAD,
			NULL,
			NULL,
			FALSE,
			TRUE,
			docManager->GetWindow()->GetPrivacyMode()
			));

	docManager->SetLoadStatus(NOT_LOADING); // This isn't done on
	// Windows, but it seems that we have to do something like this
	// on Qt, in order to reset the progress display properly.
	// (mstensho)

	FramesDocument *doc = docManager->GetCurrentDoc();
	URL doc_url = doc ? doc->GetURL() : URL();
	MessageHandler *oldHandler = url.GetFirstMessageHandler();

	// Check if this URL is already being downloaded to some file
	// (the same file or another file - who knows?). The Opera core
	// doesn't handle concurrent downloading of the same URL properly.
	// So just give up.
	//
	// The result will be that the transfer already in progress completes
	// without problems, while the new transfer that was attempted added,
	// will hang (the progress bar in the window that triggered the transfer
	// will stay there until the user aborts it somehow).
	MessageHandler *loadHandler = oldHandler;
	while (loadHandler)
	{
		if (loadHandler == g_main_message_handler)
			return OpStatus::ERR;

		loadHandler = static_cast<MessageHandler *>(loadHandler->Suc());
	}

	switch (url.Status(FALSE))
	{
	case URL_UNLOADED:
		url.Load(g_main_message_handler, doc_url);
		break;
	case URL_LOADING_ABORTED:
		url.ResumeLoad(g_main_message_handler, doc_url);
		break;
	case URL_LOADED:
		OP_ASSERT(!oldHandler);
		break;
	default:
#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
		url.SetAttribute(URL::KPauseDownload, FALSE); // continue loading
#endif
		if (oldHandler)
			url.ChangeMessageHandler(oldHandler, g_main_message_handler);
		break;
	}
	OP_STATUS ret = url.LoadToFile(filename);

	docManager->SetReload(FALSE);
	docManager->SetAction(VIEWER_NOT_DEFINED);
	docManager->SetCurrentURL(doc_url, FALSE);
	docManager->UpdateSecurityState(TRUE);

	m_window->ResetProgressDisplay();
	m_window->EndProgressDisplay();
	m_window->SetState(NOT_BUSY);

	return ret;
}

void
WindowCommander::CancelTransfer(DocumentManager* docManager, URL& url)
{
	docManager->StopLoading(FALSE);
	docManager->UpdateSecurityState(TRUE);
}
#endif // WINDOW_COMMANDER_TRANSFER

void WindowCommander::SetLoadingListener(OpLoadingListener* listener)
{
#ifdef WIC_LIMIT_PROGRESS_FREQUENCY
	m_loadingListenerProxy.SetLoadingListener(listener);
#else
	if (listener == NULL)
	{
		m_loadingListener = (OpLoadingListener*)&m_nullLoadingListener;
	}
	else
	{
		m_loadingListener = listener;
	}
#endif // WIC_LIMIT_PROGRESS_FREQUENCY
}

void WindowCommander::SetDocumentListener(OpDocumentListener* listener)
{
	if (listener == NULL)
	{
		m_documentListener = (OpDocumentListener*)&m_nullDocumentListener;
	}
	else
	{
		m_documentListener = listener;
	}
}

void WindowCommander::SetMailClientListener(OpMailClientListener* listener)
{
	if (listener == NULL)
	{
		m_mailClientListener = (OpMailClientListener*)&m_nullMailClientListener;
	}
	else
	{
		m_mailClientListener = listener;
	}
}

#ifdef _POPUP_MENU_SUPPORT_
void WindowCommander::SetMenuListener(OpMenuListener* listener)
{
	if (listener == NULL)
	{
		m_menuListener = (OpMenuListener*)&m_nullMenuListener;
	}
	else
	{
		m_menuListener = listener;
	}
}
#endif // _POPUP_MENU_SUPPORT_

void WindowCommander::SetLinkListener(OpLinkListener* listener)
{
	if (listener == NULL)
	{
		m_linkListener = (OpLinkListener*)&m_nullLinkListener;
	}
	else
	{
		m_linkListener = listener;
	}
}

#ifdef ACCESS_KEYS_SUPPORT
void WindowCommander::SetAccessKeyListener(OpAccessKeyListener* listener)
{
	if (listener == NULL)
	{
		m_accessKeyListener = (OpAccessKeyListener*)&m_nullAccessKeyListener;
	}
	else
	{
		m_accessKeyListener = listener;
	}
}
#endif // ACCESS_KEYS_SUPPORT

#ifdef _PRINT_SUPPORT_
void WindowCommander::SetPrintingListener(OpPrintingListener* listener)
{
	if (listener == NULL)
	{
		m_printingListener = (OpPrintingListener*)&m_nullPrintingListener;
	}
	else
	{
		m_printingListener = listener;
	}
}

void WindowCommander::StartPrinting()
{
	m_window->GetMessageHandler()->PostMessage(DOC_START_PRINTING, 0, 0);
}
#endif // _PRINT_SUPPORT_

void WindowCommander::SetErrorListener(OpErrorListener* listener)
{
	if (listener == NULL)
	{
		m_errorListener = (OpErrorListener*)&m_nullErrorListener;
	}
	else
	{
		m_errorListener = listener;
	}
}


#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
void WindowCommander::SetSSLListener(OpSSLListener* listener)
{
	if (listener == NULL)
	{
		m_sslListener = (OpSSLListener*)&m_nullSSLListener;
	}
	else
	{
		m_sslListener = listener;
	}
}
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER

#ifdef _ASK_COOKIE
void WindowCommander::SetCookieListener(OpCookieListener* listener)
{
	if (listener == NULL)
	{
		m_cookieListener = (OpCookieListener*)&m_nullCookieListener;
	}
	else
	{
		m_cookieListener = listener;
	}
}
#endif // _ASK_COOKIE

#ifdef WIC_FILESELECTION_LISTENER
void WindowCommander::SetFileSelectionListener(OpFileSelectionListener* listener)
{
	m_fileSelectionListener = listener ? listener : &m_nullFileSelectionListener;
}
#endif // WIC_FILESELECTION_LISTENER

#ifdef WIC_COLORSELECTION_LISTENER
void WindowCommander::SetColorSelectionListener(OpColorSelectionListener* listener)
{
	m_colorSelectionListener = listener ? listener : &m_nullColorSelectionListener;
}
#endif // WIC_COLORSELECTION_LISTENER

#ifdef NEARBY_ELEMENT_DETECTION
void WindowCommander::SetFingertouchListener(OpFingertouchListener* listener)
{
	m_fingertouchListener = listener ? listener : &m_nullFingertouchListener;
}
#endif // NEARBY_ELEMENT_DETECTION

#ifdef WIC_PERMISSION_LISTENER
void WindowCommander::SetPermissionListener(OpPermissionListener *listener)
{
	m_permissionListener = listener ? listener : &m_nullPermissionListener;
}
#endif // WIC_PERMISSION_LISTENER

#ifdef SCOPE_URL_PLAYER
void WindowCommander::SetUrlPlayerListener(WindowCommanderUrlPlayerListener* listener)
{
	m_urlPlayerListener = listener;
}
#endif //SCOPE_URL_PLAYER

#ifdef WIC_EXTERNAL_APPLICATIONS_ENUMERATION
void WindowCommander::SetApplicationListener(OpApplicationListener *listener)
{
	m_applicationListener = listener ? listener : &m_nullApplicationListener;
}
#endif // WIC_EXTERNAL_APPLICATIONS_ENUMERATION

#ifdef DOM_TO_PLATFORM_MESSAGES
void WindowCommander::SetPlatformMessageListener(OpPlatformMessageListener* listener)
{
	m_platformMessageListener = listener ? listener : &m_nullPlatformMessageListener;
}
#endif // DOM_TO_PLATFORM_MESSAGES

#ifdef APPLICATION_CACHE_SUPPORT
void WindowCommander::SetApplicationCacheListener(OpApplicationCacheListener* listener)
{
	m_applicationCachelistener = listener ? listener : &m_null_application_cache_listener;
}
#endif // APPLICATION_CACHE_SUPPORT

#ifdef WIC_OS_INTERACTION
void WindowCommander::SetOSInteractionListener(OpOSInteractionListener *listener)
{
	m_osInteractionListener = listener ? listener : &m_nullOSInteractionListener;
}
#endif // WIC_OS_INTERACTION

#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
void WindowCommander::SetSelftestListener(OpSelftestListener *listener)
{
	m_selftestListener = listener ? listener : &m_nullSelftestListener;
}
#endif // GOGI_SELFTEST_FINISHED_NOTIFICATION

#ifdef EXTENSION_SUPPORT
void WindowCommander::SetExtensionUIListener(OpExtensionUIListener* listener)
{
	m_extensionUIListener = listener ? listener : &m_null_extension_ui_listener;
}
#endif // EXTENSION_SUPPORT

#ifdef DOM_LOAD_TV_APP
void WindowCommander::SetTVAppMessageListener(OpTVAppMessageListener* listener)
{
	m_tvAppMessageListener = listener ? listener : &m_nullTvAppMessageListener;
}
#endif //DOM_LOAD_TV_APP

#ifdef _POPUP_MENU_SUPPORT_
/* virtual */ void
WindowCommander::RequestPopupMenu(const OpPoint* screen_pos)
{
	// @note to reviewer: Everything below can just as well be executed asynchronous
	if (FramesDocument* frm_doc = m_window->DocManager()->GetCurrentDoc())
	{
		if (screen_pos)
			frm_doc->InitiateContextMenu(*screen_pos);
		else
			frm_doc->InitiateContextMenu();
	}
}

/* virtual */ void
WindowCommander::OnMenuItemClicked(OpDocumentContext* context, UINT32 id)
{
	/* @Note in multi process phase 2 g_menu_manager will be in
	 * different component so this will have to be done by message sending.
	 */
	g_menu_manager->OnDocumentMenuItemClicked(id, context);
}

/* virtual */ OP_STATUS
WindowCommander::GetMenuItemIcon(UINT32 id, OpBitmap*& icon_bitmap)
{
	DocumentMenuItem* item = g_menu_manager->FindMenuItemById(id);
	if (item)
		return item->GetIconBitmap(icon_bitmap);
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}
#endif // _POPUP_MENU_SUPPORT_

OP_STATUS WindowCommander::CreateDocumentContext(OpDocumentContext** ctx)
{
	OP_ASSERT(ctx);

	if (FramesDocument* frm_doc = m_window->DocManager()->GetCurrentDoc())
	{
		OpDocumentContext* new_ctx = frm_doc->CreateDocumentContext();
		if (!new_ctx)
			return OpStatus::ERR_NO_MEMORY;

		*ctx = new_ctx;

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

OP_STATUS WindowCommander::CreateDocumentContextForScreenPos(OpDocumentContext** ctx, const OpPoint& screen_pos)
{
	OP_ASSERT(ctx);

	if (FramesDocument* frm_doc = m_window->DocManager()->GetCurrentDoc())
	{
		OpDocumentContext* new_ctx = frm_doc->CreateDocumentContextForScreenPos(screen_pos);
		if (!new_ctx)
			return OpStatus::ERR_NO_MEMORY;

		*ctx = new_ctx;

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

void WindowCommander::LoadImage(OpDocumentContext& ctx, BOOL disable_turbo/*=FALSE*/)
{
	if (DocumentManager* docman = m_window->DocManager())
		if (FramesDocument* frm_doc = docman->GetCurrentDoc())
			frm_doc->LoadImage(ctx, disable_turbo);
}

OP_STATUS WindowCommander::SaveImage(OpDocumentContext& ctx, const uni_char* filename)
{
	if (DocumentManager* docman = m_window->DocManager())
		if (FramesDocument* frm_doc = docman->GetCurrentDoc())
			return frm_doc->SaveImage(ctx, filename);

	return OpStatus::ERR;
}

void WindowCommander::OpenImage(OpDocumentContext& ctx, BOOL new_tab/*=FALSE*/, BOOL in_background/*=FALSE*/)
{
	if (DocumentManager* docman = m_window->DocManager())
		if (FramesDocument* frm_doc = docman->GetCurrentDoc())
			frm_doc->OpenImage(ctx, new_tab, in_background);
}

#ifdef USE_OP_CLIPBOARD
BOOL WindowCommander::CopyImageToClipboard(OpDocumentContext& ctx)
{
	DocumentManager* docman = m_window->DocManager();
	if (FramesDocument* frm_doc = docman->GetCurrentDoc())
		return frm_doc->CopyImageToClipboard(ctx);

	return FALSE;
}

BOOL WindowCommander::CopyBGImageToClipboard(OpDocumentContext& ctx)
{
	DocumentManager* docman = m_window->DocManager();
	if (FramesDocument* frm_doc = docman->GetCurrentDoc())
		return frm_doc->CopyBGImageToClipboard(ctx);

	return FALSE;
}
#endif // USE_OP_CLIPBOARD

# ifdef WIC_TEXTFORM_CLIPBOARD_CONTEXT
void WindowCommander::CopyTextToClipboard(OpDocumentContext& ctx)
{
	if (DocumentManager* docman = m_window->DocManager())
		if (FramesDocument* frm_doc = docman->GetCurrentDoc())
			frm_doc->CopyTextToClipboard(ctx);
}

void WindowCommander::CutTextToClipboard(OpDocumentContext& ctx)
{
	if (DocumentManager* docman = m_window->DocManager())
		if (FramesDocument* frm_doc = docman->GetCurrentDoc())
			frm_doc->CutTextToClipboard(ctx);
}

void WindowCommander::PasteTextFromClipboard(OpDocumentContext& ctx)
{
	if (DocumentManager* docman = m_window->DocManager())
		if (FramesDocument* frm_doc = docman->GetCurrentDoc())
			frm_doc->PasteTextFromClipboard(ctx);
}
# endif // WIC_TEXTFORM_CLIPBOARD_CONTEXT

void WindowCommander::ClearText(OpDocumentContext& ctx)
{
	if (DocumentManager* docman = m_window->DocManager())
		if (FramesDocument* frm_doc = docman->GetCurrentDoc())
			frm_doc->ClearText(ctx);
}

#ifdef CONTENT_MAGIC
void WindowCommander::EnableContentMagicIndicator(BOOL enable, INT32 x, INT32 y)
{
	OpScrollbar* h_scroll;
	OpScrollbar* v_scroll;
	m_window->VisualDev()->GetScrollbarObjects(&h_scroll, &v_scroll);

	if (y == -1)
		if (DocumentManager* docman = m_window->DocManager())
			if (FramesDocument* frm_doc = docman->GetCurrentDoc())
			{
				// Reflow calculates content magic position.

				frm_doc->Reflow(FALSE, TRUE);
				y = frm_doc->GetContentMagicPosition();
			}
}
#endif // CONTENT_MAGIC

#include "modules/doc/html_doc.h"

OP_STATUS WindowCommander::GetSelectedLinkInfo(OpString& url, OpString& title)
{
#ifdef _SPAT_NAV_SUPPORT_
	OpSnHandler* sn_handler = m_window->GetSnHandler();
	if (!sn_handler)
	{
		return OpStatus::ERR;
	}

	FramesDocument* active_doc = sn_handler->GetActiveFrame();
	if (!active_doc)
	{
		return OpStatus::ERR;
	}

	HTML_Document* html_doc = active_doc->GetHtmlDocument();
	if (!html_doc)
	{
		return OpStatus::ERR;
	}

	HTML_Element* elm = html_doc->GetNavigationElement();
	if (!elm)
	{
		return OpStatus::ERR;
	}

	return elm->GetURLAndTitle(html_doc->GetFramesDocument(), url, title);
#else // _SPAT_NAV_SUPPORT_
	return OpStatus::ERR;
#endif // _SPAT_NAV_SUPPORT_
}

BOOL WindowCommander::GetScriptingDisabled()
{
	return m_window->IsEcmaScriptDisabled();
}

void WindowCommander::SetScriptingDisabled(BOOL disabled)
{
	m_window->SetEcmaScriptDisabled(disabled);
}

BOOL WindowCommander::GetScriptingPaused()
{
	return m_window->IsEcmaScriptPaused();
}

void WindowCommander::SetScriptingPaused(BOOL paused)
{
	m_window->SetEcmaScriptPaused(paused);
}

void WindowCommander::CancelAllScriptingTimeouts()
{
	m_window->CancelAllEcmaScriptTimeouts();
}

void WindowCommander::SetVisibleOnScreen(BOOL is_visible)
{
	m_window->SetVisibleOnScreen(is_visible);
}

void WindowCommander::SetDrawHighlightRects(BOOL enable)
{
	m_window->SetDrawHighlightRects(enable);
}

void WindowCommander::NullAllListeners()
{
	SetLoadingListener(NULL);
	SetDocumentListener(NULL);
	SetMailClientListener(NULL);
#ifdef _POPUP_MENU_SUPPORT_
	SetMenuListener(NULL);
#endif // _POPUP_MENU_SUPPORT_
	SetLinkListener(NULL);
#ifdef ACCESS_KEYS_SUPPORT
	SetAccessKeyListener(NULL);
#endif // ACCESS_KEYS_SUPPORT
#ifdef _PRINT_SUPPORT_
	SetPrintingListener(NULL);
#endif // _PRINT_SUPPORT_
	SetErrorListener(NULL);
#if defined (_SSL_SUPPORT_) && defined (_NATIVE_SSL_SUPPORT_)
	SetSSLListener(NULL);
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_
#ifdef _ASK_COOKIE
	SetCookieListener(NULL);
#endif // _ASK_COOKIE
#ifdef WIC_FILESELECTION_LISTENER
	SetFileSelectionListener(NULL);
#endif // WIC_FILESELECTION_LISTENER
#ifdef NEARBY_ELEMENT_DETECTION
	SetFingertouchListener(NULL);
#endif // NEARBY_ELEMENT_DETECTION
#ifdef WIC_PERMISSION_LISTENER
	SetPermissionListener(NULL);
#endif // WIC_PERMISSION_LISTENER
#ifdef WIC_EXTERNAL_APPLICATIONS_ENUMERATION
	SetApplicationListener(NULL);
#endif // WIC_EXTERNAL_APPLICATIONS_ENUMERATION
#ifdef DOM_TO_PLATFORM_MESSAGES
	SetPlatformMessageListener(NULL);
#endif // DOM_TO_PLATFORM_MESSAGES
#ifdef APPLICATION_CACHE_SUPPORT
	SetApplicationCacheListener(NULL);
#endif // APPLICATION_CACHE_SUPPORT
#ifdef EXTENSION_SUPPORT
	SetExtensionUIListener(NULL);
#endif // EXTENSION_SUPPORT
#ifdef DOM_LOAD_TV_APP
	SetTVAppMessageListener(NULL);
#endif //DOM_LOAD_TV_APP
}

#ifdef ACCESS_KEYS_SUPPORT
void WindowCommander::SetAccessKeyMode(BOOL enable)
{
	m_window->SetAccesskeyMode(enable);
}

BOOL WindowCommander::GetAccessKeyMode()
{
	return m_window->GetAccesskeyMode();
}
#endif // ACCESS_KEYS_SUPPORT

OpViewportController* WindowCommander::GetViewportController() const
{
	return m_window->GetViewportController();
}

#ifdef NEARBY_ELEMENT_DETECTION

void WindowCommander::HideFingertouchElements()
{
	if (ElementExpander* expander = m_window->GetElementExpander())
		expander->Hide(TRUE);
}

#endif // NEARBY_ELEMENT_DETECTION

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
OP_STATUS WindowCommander::FindNearbyInteractiveItems(const OpRect& rect, List<InteractiveItemInfo>& list, BOOL bbox_rectangles /*= FALSE*/)
{
	FramesDocument* frm_doc = m_window->GetCurrentDoc();

	if(!frm_doc)
		return OpStatus::ERR_NULL_POINTER;
#ifndef CSS_TRANSFORMS
	(void) bbox_rectangles; // hide warning
#else // CSS_TRANSFORMS
	if (bbox_rectangles)
	{
		OP_STATUS status = frm_doc->FindNearbyInteractiveItems(rect, list, AffinePos(0, 0));
		if (OpStatus::IsSuccess(status))
		{
			InteractiveItemInfo* iter = list.First();

			while (iter)
			{
				iter->ChangeToBBoxes();
				iter = iter->Suc();
			}
		}

		return status;
	}
	else
#endif // !CSS_TRANSFORMS
		return frm_doc->FindNearbyInteractiveItems(rect, list, AffinePos(0, 0));
}
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

#ifdef DOM_JSPLUGIN_CUSTOM_EVENT_HOOK

OP_STATUS WindowCommander::SendCustomWindowEvent(const uni_char* event_name, int event_param)
{
	if (FramesDocument* frm_doc = m_window->GetActiveFrameDoc())
		return frm_doc->HandleWindowEvent(DOM_EVENT_CUSTOM, 0, event_name, event_param);

	return OpStatus::ERR;
}

# ifdef GADGET_SUPPORT
OP_STATUS WindowCommander::SendCustomGadgetEvent(const uni_char* event_name, int event_param)
{
	if (FramesDocument* frm_doc = m_window->GetActiveFrameDoc())
		if (DOM_Environment* dom_env = frm_doc->GetDOMEnvironment())
		{
			DOM_Environment::GadgetEventData data;
			data.type_custom = event_name;
			data.param = event_param;

			return dom_env->HandleGadgetEvent(DOM_Environment::GADGET_EVENT_CUSTOM, &data);
		}

	return OpStatus::ERR;
}
# endif // GADGET_SUPPORT

#endif // DOM_JSPLUGIN_CUSTOM_EVENT_HOOK

/** Tell window that screen has changed */
void WindowCommander::ScreenPropertiesHaveChanged(){
	m_window->ScreenPropertiesHaveChanged();
}

#ifdef WIC_PERMISSION_LISTENER
void NullPermissionListener::OnAskForPermission(OpWindowCommander *commander, PermissionCallback *callback)
{
	callback->OnDisallowed(PermissionCallback::PERSISTENCE_TYPE_NONE);
}

#define GET_STRING_WITH_ARGUMENT_1(string_id, op_string) \
{ \
	OpString unformatted_message; \
	OP_ASSERT(GetNumberOfArguments() >= 1); \
	if (OpStatus::IsSuccess(g_languageManager->GetString(string_id, unformatted_message))) \
		RETURN_IF_ERROR(op_string.AppendFormat(unformatted_message.CStr(), GetArgument(0))); \
}

#define GET_STRING_WITH_ARGUMENT_2(string_id, op_string) \
{ \
	OpString unformatted_message; \
	OP_ASSERT(GetNumberOfArguments() >= 2); \
	if (OpStatus::IsSuccess(g_languageManager->GetString(string_id, unformatted_message))) \
		RETURN_IF_ERROR(op_string.AppendFormat(unformatted_message.CStr(), GetArgument(0), GetArgument(1))); \
}

#ifdef DOM_JIL_API_SUPPORT
OP_STATUS OpPermissionListener::DOMPermissionCallback::GetMessage(OpString& message)
{
	switch(GetOperation())
	{
		case DOMPermissionCallback::OPERATION_TYPE_JIL_ACCOUNTINFO_PHONEUSERUNIQUEID:               RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_ACCOUNTINFO_PHONEUSERUNIQUEID, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_ACCOUNTINFO_PHONEMSISDN:                     RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_ACCOUNTINFO_PHONEMSISDN, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_ACCOUNTINFO_USERSUBSCRIPTIONTYPE:            RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_ACCOUNTINFO_USERSUBSCRIPTIONTYPE, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_ACCOUNTINFO_USERACCOUNTBALANCE:              RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_ACCOUNTINFO_USERACCOUNTBALANCE, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_CONFIG_MSGRINGTONEVOLUME:                    RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_CONFIG_MSGRINGTONEVOLUME, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_CONFIG_RINGTONEVOLUME:                       RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_CONFIG_RINGTONEVOLUME, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_CONFIG_VIBRATIONSETTING:                     RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_CONFIG_VIBRATIONSETTING, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_CONFIG_SETASWALLPAPER:                       GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_CONFIG_SETASWALLPAPER, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_CONFIG_SETDEFAULTRINGTONE:                   GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_CONFIG_SETDEFAULTRINGTONE, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_ADDRESSBOOKITEM_UPDATE:                      RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_ADDRESSBOOKITEM_UPDATE, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_CALENDARITEM_UPDATE:                         RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_CALENDARITEM_UPDATE, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_CAMERA_CAPTUREIMAGE:                         GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_CAMERA_CAPTUREIMAGE, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_CAMERA_STARTVIDEOCAPTURE:                    GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_CAMERA_STARTVIDEOCAPTURE, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_CAMERA_STOPVIDEOCAPTURE:                     RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_CAMERA_STOPVIDEOCAPTURE, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICE_COPYFILE:                             GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICE_COPYFILE, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICE_DELETEFILE:                           GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICE_DELETEFILE, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICE_FINDFILES:                            RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICE_FINDFILES, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICE_GETDIRECTORYFILENAMES:                GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICE_GETDIRECTORYFILENAMES, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICE_GETFILE:                              GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICE_GETFILE, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICE_GETFILESYSTEMROOTS:                   RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICE_GETFILESYSTEMROOTS, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICE_MOVEFILE:                             GET_STRING_WITH_ARGUMENT_2(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICE_MOVEFILE, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICE_GETAVAILABLEAPPLICATIONS:             RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICE_GETAVAILABLEAPPLICATIONS, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICE_LAUNCHAPPLICATION:                    GET_STRING_WITH_ARGUMENT_2(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICE_LAUNCHAPPLICATION, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICE_SETRINGTONE:                          RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICE_SETRINGTONE, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICE_CLIPBOARDSTRING:                      RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICE_CLIPBOARDSTRING, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICEINFO_OWNERINFO:                        RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICEINFO_OWNERINFO, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_DEVICESTATEINFO_REQUESTPOSITIONINFO:         GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_DEVICESTATEINFO_REQUESTPOSITIONINFO, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_COPYMESSAGETOFOLDER:               GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_COPYMESSAGETOFOLDER, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_CREATEFOLDER:                      GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_CREATEFOLDER, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_DELETEALLMESSAGES:                 RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_DELETEALLMESSAGES, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_DELETEEMAILACCOUNT:                RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_DELETEEMAILACCOUNT, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_DELETEFOLDER:                      GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_DELETEFOLDER, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_DELETEMESSAGE:                     RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_DELETEMESSAGE, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_FINDMESSAGES:                      RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_FINDMESSAGES, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_GETCURRENTEMAILACCOUNT:            RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_GETCURRENTEMAILACCOUNT, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_GETEMAILACCOUNTS:                  RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_GETEMAILACCOUNTS, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_GETFOLDERNAMES:                    RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_GETFOLDERNAMES, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_GETMESSAGE:                        RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_GETMESSAGE, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_MOVEMESSAGETOFOLDER:               GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_MOVEMESSAGETOFOLDER, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_SENDMESSAGE:                       RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_SENDMESSAGE, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_MESSAGING_SETCURRENTEMAILACCOUNT:            RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_MESSAGING_SETCURRENTEMAILACCOUNT, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_ADDCALENDARITEM:                         RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_ADDCALENDARITEM, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_GETADDRESSBOOKGROUPMEMBERS:              RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_GETADDRESSBOOKGROUPMEMBERS, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_DELETECALENDARITEM:                      RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_DELETECALENDARITEM, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_FINDCALENDARITEMS:                       RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_FINDCALENDARITEMS, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_GETCALENDARITEM:                         RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_GETCALENDARITEM, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_GETCALENDARITEMS:                        RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_GETCALENDARITEMS, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_EXPORTASVCARD:                           RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_EXPORTASVCARD, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_ADDADDRESSBOOKITEM:                      RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_ADDADDRESSBOOKITEM, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_FINDADDRESSBOOKITEMS:                    RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_FINDADDRESSBOOKITEMS, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_CREATEADDRESSBOOKGROUP:                  GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_CREATEADDRESSBOOKGROUP, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_DELETEADDRESSBOOKGROUP:                  GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_DELETEADDRESSBOOKGROUP, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_DELETEADDRESSBOOKITEM:                   RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_DELETEADDRESSBOOKITEM, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_GETADDRESSBOOKITEM:                      RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_GETADDRESSBOOKITEM, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_GETADDRESSBOOKITEMSCOUNT:                RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_GETADDRESSBOOKITEMSCOUNT, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_PIM_GETAVAILABLEADDRESSGROUPNAMES:           RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_PIM_GETAVAILABLEADDRESSGROUPNAMES, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_TELEPHONY_DELETEALLCALLRECORDS:              RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_TELEPHONY_DELETEALLCALLRECORDS, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_TELEPHONY_DELETECALLRECORD:                  RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_TELEPHONY_DELETECALLRECORD, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_TELEPHONY_FINDCALLRECORDS:                   RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_TELEPHONY_FINDCALLRECORDS, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_TELEPHONY_GETCALLRECORD:                     RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_TELEPHONY_GETCALLRECORD, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_TELEPHONY_GETCALLRECORDCOUNT:                RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_TELEPHONY_GETCALLRECORDCOUNT, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_TELEPHONY_INITIATEVOICECALL:                 GET_STRING_WITH_ARGUMENT_1(Str::D_JIL_SECURITY_OPERATION_TYPE_TELEPHONY_INITIATEVOICECALL, message); break;
		case DOMPermissionCallback::OPERATION_TYPE_JIL_WIDGETMANAGER_CHECKWIDGETINSTALLATIONSTATUS: RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_WIDGETMANAGER_CHECKWIDGETINSTALLATIONSTATUS, message)); break;
		case DOMPermissionCallback::OPERATION_TYPE_XMLHTTPREQUEST:                                  RETURN_IF_ERROR(g_languageManager->GetString(Str::D_JIL_SECURITY_OPERATION_TYPE_XMLHTTPREQUEST, message)); break;
		default: OP_ASSERT(!"Unknown DOM operation.");
	}
	return OpStatus::OK;
}
#endif // DOM_JIL_API_SUPPORT

#undef GET_STRING_WITH_ARGUMENT_1
#undef GET_STRING_WITH_ARGUMENT_2
#endif // WIC_PERMISSION_LISTENER

OP_STATUS WindowCommander::GetCurrentContentSize(OpFileLength& size, BOOL include_inlines)
{
	if (include_inlines)
	{
		FramesDocument *doc =  m_window->GetCurrentDoc();
		if (!doc)
			return OpStatus::ERR;

		DocLoadingInfo info;
		doc->GetDocLoadingInfo(info);
		size = info.total_size;
	}
	else
	{
		size = m_window->GetCurrentURL().GetContentLoaded();
	}
	return OpStatus::OK;
}

#ifdef WEBFEEDS_DISPLAY_SUPPORT
BOOL WindowCommander::IsInlineFeed()
{
	FramesDocument* doc = GetCurrentDocument(m_window, FALSE);
	if (!doc)
	{
		return FALSE;
	}
	return doc->IsInlineFeed();
}
#endif // WEBFEEDS_DISPLAY_SUPPORT

BOOL WindowCommander::IsIntranet()
{
	if (ServerName* sn = const_cast<ServerName *>(reinterpret_cast<const ServerName *>(m_window->GetCurrentURL().GetAttribute(URL::KServerName, NULL))))
	{
		if (OpSocketAddress* socket_address = sn->SocketAddress())
		{
			if (socket_address->IsValid())
			{
				if (socket_address->GetNetType() == NETTYPE_LOCALHOST || socket_address->GetNetType() == NETTYPE_PRIVATE)
					return TRUE;
			}
		}
	}
	return FALSE;
}

BOOL WindowCommander::HasDocumentFocusedFormElement()
{
	FramesDocument *activeDoc = m_window->GetActiveFrameDoc();
	if (activeDoc && activeDoc->current_focused_formobject)
	{
		return TRUE;
	}
	return FALSE;
}

OP_STATUS WindowCommander::CreateSearchURL(OpDocumentContext& context, OpString8* url, OpString8* query, OpString8* charset, BOOL* is_post)
{
	if (!url || !query || !charset || !is_post)
	{
		return OpStatus::ERR;
	}
	else
	{
		return FramesDocument::CreateSearchURL(context, *url, *query, *charset, *is_post);
	}
}

#ifdef PLUGIN_AUTO_INSTALL
OP_STATUS WindowCommander::OnPluginAvailable(const uni_char* mime_type)
{
	FramesDocument* doc = GetCurrentDocument(m_window, FALSE);
	if (!doc)
	{
		return OpStatus::ERR;
	}
	doc->OnPluginAvailable(mime_type);
	return OpStatus::OK;
}
#endif // PLUGIN_AUTO_INSTALL

#ifdef _PLUGIN_SUPPORT_
OP_STATUS WindowCommander::OnPluginDetected(const uni_char* mime_type)
{
	FramesDocument* doc = GetCurrentDocument(m_window, FALSE);
	if (!doc)
		return OpStatus::ERR;
	doc->OnPluginDetected(mime_type);
	return OpStatus::OK;
}
#endif // _PLUGIN_SUPPORT_

BOOL WindowCommander::IsDocumentSupportingViewMode(WindowViewMode view_mode) const
{
	return m_window->GetCurrentDoc()->IsViewModeSupported(view_mode);
}

/* static */
DocumentURLType WindowCommander::URLType2DocumentURLType(URLType type)
{
	switch (type)
	{
	case URL_NULL_TYPE: // fall through
	case URL_UNKNOWN:
		return DocumentURLType_Unknown;
	case URL_HTTP:
		return DocumentURLType_HTTP;
	case URL_HTTPS:
		return DocumentURLType_HTTPS;
	case URL_FTP:
		return DocumentURLType_FTP;
	default:
		return DocumentURLType_Other;
	}
}

void WindowCommander::ForcePluginsDisabled(BOOL disabled)
{
	m_window->ForcePluginsDisabled(disabled);
}

BOOL WindowCommander::GetForcePluginsDisabled() const
{
	return m_window->GetForcePluginsDisabled();
}

#ifdef WIC_SET_PERMISSION
OP_STATUS WindowCommander::SetUserConsent(OpPermissionListener::PermissionCallback::PermissionType permission_type, OpPermissionListener::PermissionCallback::PersistenceType persistence_type, const uni_char *hostname, BOOL3 allowed)
{
	return g_secman_instance->SetUserConsent(m_window, permission_type, persistence_type, hostname, ToUserConsentType(allowed));
}

void WindowCommander::GetUserConsent(INTPTR user_data)
{
	OpAutoVector<OpPermissionListener::ConsentInformation> consent_information;
	OP_STATUS status = g_secman_instance->GetUserConsent(m_window, consent_information);
	if (OpStatus::IsSuccess(status))
		m_permissionListener->OnUserConsentInformation(this, consent_information, user_data);
	else
		m_permissionListener->OnUserConsentError(this, status, user_data);
}
#endif // WIC_SET_PERMISSION

#ifdef GOGI_APPEND_HISTORY_LIST
OP_STATUS WindowCommander::AppendHistoryList(const OpVector<HistoryInformation>& urls, unsigned int activate_url_index)
{
	DocumentManager* dm = m_window->DocManager();

	for (unsigned int i = 0; i < urls.GetCount(); i++)
	{
		URL url = g_url_api->GetURL(urls.Get(i)->url);
		RETURN_IF_ERROR(dm->AddNewHistoryPosition(url, DocumentReferrer(), -1, urls.Get(i)->title, i == activate_url_index));
	}

	SetCurrentHistoryPos(activate_url_index + 1);
	return OpStatus::OK;
}
#endif // GOGI_APPEND_HISTORY_LIST

OpWindowCommander::FullScreenState WindowCommander::GetDocumentFullscreenState() const
{
	return m_window->GetFullScreenState();
}

OP_STATUS WindowCommander::SetDocumentFullscreenState(OpWindowCommander::FullScreenState value)
{
	return m_window->SetFullScreenState(value);
}
