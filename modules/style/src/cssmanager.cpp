/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/style/cssmanager.h"
#include "modules/util/opcrypt.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/util/opfile/opfile.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/datasrcelm.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/winman.h"

#ifdef EMBROWSER_SUPPORT
#include "adjunct/quick/Application.h"
#endif // EMBROWSER_SUPPORT


CSSManager::CSSManager() : m_localcss(NULL)
{
}

void CSSManager::ConstructL()
{
	unsigned int css_count = FirstUserStyle;
#ifdef LOCAL_CSS_FILES_SUPPORT
	css_count += g_pcfiles->GetLocalCSSCount();
#endif
	m_localcss = OP_NEWA_L(LocalStylesheet, css_count);

	// Register as a prefs file listener
	g_pcfiles->RegisterFilesListenerL(this);
}

CSSManager::~CSSManager()
{
	OP_DELETEA(m_localcss);
	if (g_pcfiles) g_pcfiles->UnregisterFilesListener(this);
#ifdef EXTENSION_SUPPORT
	// Remove any references to still loaded extensions
	m_extensionstylesheets.Clear();
#endif
}

HTML_Element* CSSManager::NewLinkElementL(const uni_char* url)
{
	HTML_Element* elm;
	LEAVE_IF_NULL(elm = NEW_HTML_Element());

	if (elm->Construct((HLDocProfile*)NULL, NS_IDX_HTML, Markup::HTE_LINK, (HtmlAttrEntry*)NULL, HE_INSERTED_BY_CSS_IMPORT) != OpStatus::ERR_NO_MEMORY)
	{
		uni_char* href_str = SetStringAttr(url, uni_strlen(url), FALSE);
		if (href_str)
			elm->SetAttr(Markup::HA_HREF, ITEM_TYPE_STRING, (void*)href_str, TRUE);
	}
	else
	{
		DELETE_HTML_Element(elm);
		elm = NULL;
	}

	return elm;
}

OP_STATUS CSSManager::LoadCSS_URL(HTML_Element* css_he, BOOL user_defined)
{
	OP_STATUS status = OpStatus::OK;
	URL* css_url = css_he->GetLinkURL(NULL);

	if (css_url)
	{
		if (!css_url->QuickLoad(FALSE))
			return OpStatus::ERR_FILE_NOT_FOUND;

		// Check if this css url has been loaded before to avoid recursion

		if (css_he->IsCssImport())
		{
			HTML_Element* parent = css_he->Parent();
			while (parent)
			{
				const URL* parent_css_url = parent->GetUrlAttr(Markup::HA_HREF, NS_IDX_HTML, NULL);
				if (parent_css_url && parent_css_url->Id(TRUE) == css_url->Id(TRUE))
					return OpStatus::OK;

				if (!parent->IsCssImport())
					break;

				parent = parent->Parent();
			}
		}

		// read data

		URL_DataDescriptor* url_data_desc = css_url->GetDescriptor(NULL, URL::KFollowRedirect, FALSE, TRUE, NULL, URL_CSS_CONTENT);

		if (url_data_desc)
		{
			BOOL more;
#ifdef OOM_SAFE_API
 			TRAP(status, url_data_desc->RetrieveDataL(more));
			if (status == OpStatus::ERR_NO_MEMORY)
				hld_prof->SetIsOutOfMemory(TRUE);
#else
			url_data_desc->RetrieveData(more);
#endif //OOM_SAFE_API

			uni_char* data_buf = (uni_char*)url_data_desc->GetBuffer();
			unsigned long data_len = url_data_desc->GetBufSize();

			OP_ASSERT(css_he->GetDataSrc()->IsEmpty());
			// If the above assert ever trigger, enable next line
			//css_he->EmptySrcListAttr();

			while (data_buf && UNICODE_DOWNSIZE(data_len) > 0 && status == OpStatus::OK)
			{
				if (data_len >= USHRT_MAX-64)
					data_len = USHRT_MAX-65;
				status = css_he->AddToSrcListAttr(data_buf, UNICODE_DOWNSIZE(data_len), *css_url);
				url_data_desc->ConsumeData(data_len);
#ifdef OOM_SAFE_API
				TRAP(status, url_data_desc->RetrieveDataL(more));
#else
				url_data_desc->RetrieveData(more);
#endif //OOM_SAFE_API
				data_buf = (uni_char*)url_data_desc->GetBuffer();
				data_len = url_data_desc->GetBufSize();
			}

			if (status == OpStatus::OK)
			{
				OpStatus::Ignore(status);
				status = css_he->LoadStyle((FramesDocument*)NULL, user_defined);
				CSS* css = css_he->GetCSS();
				if (css)
					css->SetIsLocal();
			}

			OP_DELETE(url_data_desc);
		}
	}
	return status;
}

void CSSManager::FileChangedL(PrefsCollectionFiles::filepref which,
                              const OpFile* what)
{
	// Make sure it is a change we are interested in (PrefsCollectionFiles
	// other files than CSS files)
	enum css_id id;

	switch (which)
	{
	case PrefsCollectionFiles::BrowserCSSFile:
		id = BrowserCSS;
		break;

	case PrefsCollectionFiles::LocalCSSFile:
		id = LocalCSS;
		break;

#ifdef _WML_SUPPORT_
	case PrefsCollectionFiles::WMLCSSFile:
		id = WML_CSS;
		break;
#endif

#if defined(CSS_MATHML_STYLESHEET)
	case PrefsCollectionFiles::MathMLCSSFile:
		id = MathML_CSS;
		break;
#endif // CSS_MATHML_STYLESHEET

#ifdef SUPPORT_VISUAL_ADBLOCK
	case PrefsCollectionFiles::ContentBlockCSSFile:
		id = ContentBlockCSS;
		break;
#endif // SUPPORT_VISUAL_ADBLOCK

	default:
		return; // Don't care about the rest
	}

	// Create a local copy of the OpFile
	OpFile* cssfile = (OpFile*)what->CreateCopy();
	LEAVE_IF_NULL(cssfile);
	OpStackAutoPtr<OpFile> cssfile_ap(cssfile);

	HTML_Element::DocumentContext doc_ctx((FramesDocument*)NULL);

	HTML_Element** elm_ptr = &m_localcss[id].elm;
	// Free the old stylesheet before loading the new one.
	if (*elm_ptr)
	{
		(*elm_ptr)->Free(doc_ctx);
		*elm_ptr = NULL;
	}
	m_localcss[id].css = NULL;
	*elm_ptr = LoadCSSFileL(cssfile, id == LocalCSS);
	if (*elm_ptr)
		m_localcss[id].css = (*elm_ptr)->GetCSS();

	if (g_windowManager)
		g_windowManager->UpdateWindows(TRUE);
}

void CSSManager::LoadLocalCSSL()
{
	HTML_Element::DocumentContext doc_ctx((FramesDocument*)NULL);

	for (unsigned int i = 0; i < FirstUserStyle; i++)
	{
		OpFile cssfile;
		ANCHOR(OpFile, cssfile);

		switch (css_id(i))
		{
		case BrowserCSS:
			g_pcfiles->GetFileL(PrefsCollectionFiles::BrowserCSSFile, cssfile);
			break;

		case LocalCSS:
			g_pcfiles->GetFileL(PrefsCollectionFiles::LocalCSSFile, cssfile);
			break;

#ifdef _WML_SUPPORT_
		case WML_CSS:
			g_pcfiles->GetFileL(PrefsCollectionFiles::WMLCSSFile, cssfile);
			break;
#endif // _WML_SUPPORT_

#ifdef CSS_MATHML_STYLESHEET
		case MathML_CSS:
			g_pcfiles->GetFileL(PrefsCollectionFiles::MathMLCSSFile, cssfile);
			break;
#endif // CSS_MATHML_STYLESHEET

#ifdef SUPPORT_VISUAL_ADBLOCK
		case ContentBlockCSS:
			g_pcfiles->GetFileL(PrefsCollectionFiles::ContentBlockCSSFile, cssfile);
			break;
#endif // SUPPORT_VISUAL_ADBLOCK

		default:
			OP_ASSERT(FALSE); // This should never happen!
		}

		if (m_localcss[i].elm)
		{
			m_localcss[i].elm->Free(doc_ctx);
			m_localcss[i].elm = NULL;
		}
		m_localcss[i].css = NULL;
		m_localcss[i].elm = LoadCSSFileL(&cssfile, css_id(i) == LocalCSS);
		if (m_localcss[i].elm)
			m_localcss[i].css = m_localcss[i].elm->GetCSS();
	}

#if defined LOCAL_CSS_FILES_SUPPORT
	unsigned int css_count = g_pcfiles->GetLocalCSSCount();

	for (unsigned int j = 0; j < css_count; j ++)
	{
		HTML_Element** css_elm = &m_localcss[FirstUserStyle+j].elm;
		CSS** css = &m_localcss[FirstUserStyle+j].css;
		if (*css_elm)
		{
			(*css_elm)->Free(doc_ctx);
			*css_elm = NULL;
		}
		*css = NULL;

		OpFile* css_file = g_pcfiles->GetLocalCSSFile(j);
		if (css_file)
			*css_elm = LoadCSSFileL(css_file, TRUE);
		if (*css_elm)
		{
			*css = (*css_elm)->GetCSS();
			if (*css)
				(*css)->SetEnabled(g_pcfiles->IsLocalCSSActive(j));
		}
	}
#endif // LOCAL_CSS_FILES_SUPPORT

	if (g_windowManager)
		g_windowManager->UpdateWindows(TRUE);
}

#if defined LOCAL_CSS_FILES_SUPPORT
void CSSManager::UpdateActiveUserStyles()
{
	unsigned int count = g_pcfiles->GetLocalCSSCount();

	for (unsigned int j=0; j < count; j++)
	{
		CSS* css = m_localcss[FirstUserStyle+j].css;
		if (css)
			css->SetEnabled(g_pcfiles->IsLocalCSSActive(j));
	}
}

void CSSManager::LocalCSSChanged()
{
	UpdateActiveUserStyles();
}

#endif // LOCAL_CSS_FILES_SUPPORT

HTML_Element* CSSManager::LoadCSSFileL(OpFile* file, BOOL user_defined)
{
	BOOL exists;
	LEAVE_IF_ERROR(file->Exists(exists));
	HTML_Element* new_he = NULL;
	HTML_Element::DocumentContext doc_ctx((FramesDocument*)NULL);
	if (exists)
	{
		OpString res_url;
		ANCHOR(OpString, res_url);
		OpString file_url;
		ANCHOR(OpString, file_url);
		file_url.SetL(file->GetFullPath());
		BOOL resolved = g_url_api->ResolveUrlNameL(file_url, res_url);
		if (resolved)
		{
			new_he = NewLinkElementL(res_url.CStr());
			OP_STATUS stat = LoadCSS_URL(new_he, user_defined);
			if (OpStatus::IsError(stat))
			{
				new_he->Free(doc_ctx);
				if (stat == OpStatus::ERR_NO_MEMORY)
					LEAVE(stat);
				new_he = NULL;
			}
		}
	}
	return new_he;
}

OP_STATUS CSSManager::OnInputAction(OpInputAction* action, BOOL& handled)
{
#ifdef ACTION_RELOAD_STYLESHEETS_ENABLED
	switch (action->GetAction())
	{
	case OpInputAction::ACTION_RELOAD_STYLESHEETS:
		{
			TRAP_AND_RETURN(oom_value, LoadLocalCSSL());
			handled = TRUE;
		}
		break;

	default:
		handled = FALSE;
	}
#else
	handled = FALSE;
#endif // ACTION_RELOAD_STYLESHEETS_ENABLED

	return OpStatus::OK;
}

#ifdef EXTENSION_SUPPORT
OP_STATUS CSSManager::AddExtensionUserCSS(const uni_char* path, ExtensionStylesheetOwnerID owner)
{
	OpAutoPtr<ExtensionStylesheet> new_stylesheet(OP_NEW(ExtensionStylesheet, ()));
	RETURN_OOM_IF_NULL(new_stylesheet.get());
	RETURN_IF_ERROR(new_stylesheet->path.Set(path));
	new_stylesheet->owner = owner;

	HTML_Element::DocumentContext doc_ctx((FramesDocument*)NULL);
	OpFile cssfile;
	OP_STATUS rc;
	if (OpStatus::IsSuccess(rc = cssfile.Construct(path, OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP))) // OpZipFile can return FILE_NOT_FOUND in construct call
	{
		TRAP(rc, new_stylesheet->elm = LoadCSSFileL(&cssfile, TRUE));
		if (OpStatus::IsSuccess(rc) && new_stylesheet->elm)
		{
			new_stylesheet->css = new_stylesheet->elm->GetCSS();
			new_stylesheet.release()->Into(&m_extensionstylesheets);

			// Update all windows
			if (g_windowManager)
				rc = g_windowManager->UpdateWindows(TRUE);
		}
	}
	return rc;
}

void CSSManager::RemoveExtensionUserCSSes(ExtensionStylesheetOwnerID owner)
{
	// Remove all stylesheets with this owner
	BOOL update = FALSE;
	ExtensionStylesheet* extension_stylesheet = GetExtensionUserCSS();
	while (extension_stylesheet)
	{
		ExtensionStylesheet* next = static_cast<ExtensionStylesheet*>(extension_stylesheet->Suc());

		if (extension_stylesheet->owner == owner)
		{
			extension_stylesheet->Out();
			OP_DELETE(extension_stylesheet);
			update = TRUE;
		}

		extension_stylesheet = next;
	}

	// Update all windows
	if (update && g_windowManager)
		g_windowManager->UpdateWindows(TRUE);
}
#endif

CSSManager::LocalStylesheet::~LocalStylesheet()
{
	HTML_Element::DocumentContext doc_ctx((FramesDocument*)NULL);
	if (elm)
		elm->Free(doc_ctx);
}
