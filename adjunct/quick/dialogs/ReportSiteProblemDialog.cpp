/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/widgets/OpMultiEdit.h"
#include "ReportSiteProblemDialog.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/doc/doc.h"
#include "modules/doc/frm_doc.h"
#include "modules/layout/box/box.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/url/uamanager/ua.h"
#include "modules/upload/upload.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/widgets/OpWidgetFactory.h"

#if defined(MSWIN)
# include "platforms/windows/windows_ui/res/#buildnr.rci"
#elif defined(UNIX)
# include "platforms/unix/product/config.h"
#elif defined(_MACINTOSH_)
# include "platforms/mac/Resources/buildnum.h"
#elif defined(WIN_CE)
# include "platforms/win_ce/res/buildnum.h"
#else
# pragma error("include the file where VER_BUILD_NUMBER is defined")
#endif

#include "modules/url/url_enum.h"
#include "modules/url/url2.h"

/***********************************************************************************
**
**	ReportSiteProblemDialog
**
***********************************************************************************/

ReportSiteProblemDialog::ReportSiteProblemDialog()
{}

/***********************************************************************************
**
**	GetOkTextID
**
***********************************************************************************/

Str::LocaleString ReportSiteProblemDialog::GetOkTextID()
{
	return Str::SI_MAIL_SEND_BUTTON_TEXT; /* "Send" */
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void ReportSiteProblemDialog::OnInit()
{
	OpDropDown* dropdown = GetWidgetByName<OpDropDown>("Category_dropdown", WIDGET_TYPE_DROPDOWN);
	if (dropdown)
	{
		OpString loc_str;
		g_languageManager->GetString(Str::D_REPORT_SITE_UNSPECIFIED, loc_str);
		dropdown->AddItem(loc_str.CStr());
		g_languageManager->GetString(Str::D_REPORT_SITE_MINOR, loc_str);
		dropdown->AddItem(loc_str.CStr());
		g_languageManager->GetString(Str::D_REPORT_SITE_MAJOR, loc_str);
		dropdown->AddItem(loc_str.CStr());
		g_languageManager->GetString(Str::D_REPORT_SITE_UNUSABLE, loc_str);
		dropdown->AddItem(loc_str.CStr());
	}

	DesktopWindow* dw = (DesktopWindow*)GetParentDesktopWindow();
	DocumentDesktopWindow* ddw = NULL;

	SetWidgetReadOnly("Configuration_edit", TRUE);

	OpEdit* address_edit = GetWidgetByName<OpEdit>("Page_address_edit", WIDGET_TYPE_EDIT);
	if (address_edit)
	{
		address_edit->SetReadOnly(TRUE);
		address_edit->SetForceTextLTR(TRUE);
	}

	OpWidget* widget = GetWidgetByName("Advanced_group");
	if (widget)
		widget->SetVisibility(FALSE);

	m_encoding.Set(UNI_L("default"));
	m_strict_mode = FALSE;

	if (dw && dw->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		ddw = (DocumentDesktopWindow*)dw;

		OpWindowCommander* wc = ddw->GetWindowCommander();
		if (wc)
		{
			SetWidgetText("Page_address_edit", wc->GetCurrentURL(FALSE));

			if (WindowCommanderProxy::GetForceEncoding(wc))
			{
				m_encoding.Set(WindowCommanderProxy::GetForceEncoding(wc));
			}
			if (WindowCommanderProxy::HasCSSModeFULL(wc))
			{
				m_document_mode.Set("Author");
			}
			else
			{
				m_document_mode.Set("User");
			}

			switch (wc->GetImageMode())
			{
			case OpDocumentListener::ALL_IMAGES:
				m_images.Set("All");
				break;

			case OpDocumentListener::LOADED_IMAGES:
				m_images.Set("Cached");
				break;

			case OpDocumentListener::NO_IMAGES:
				m_images.Set("None");
				break;
			}

			m_strict_mode = WindowCommanderProxy::IsInStrictMode(wc);
		}
	}

	char *useragentstr = (char *) g_memory_manager->GetTempBuf();
	/* int ua_str_len = */ g_uaManager->GetUserAgentStr(useragentstr, 128, NULL);

	if (useragentstr && *useragentstr)
		m_user_agent.Set(useragentstr);

	OpString on;
	OpString off;
	on.Set("On");
	off.Set("Off");

	BOOL block_all = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget);
	BOOL unrequested = g_pcjs->GetIntegerPref(PrefsCollectionJS::IgnoreUnrequestedPopups);
	BOOL background = g_pcjs->GetIntegerPref(PrefsCollectionJS::TargetDestination) == POPUP_WIN_BACKGROUND;
	m_plugins = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled);
	m_javascript = g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled);
	COOKIE_MODES cookie_mode = (COOKIE_MODES)g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled);
	m_referrer_logging = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ReferrerEnabled);

	BOOL http, https, ftp, gopher, wais, auto_proxy;

	http = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPProxy);
	https= g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPSProxy);
	ftp  = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseFTPProxy);
#ifdef GOPHER_SUPPORT
	gopher=g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseGopherProxy);
#endif
#ifdef WAIS_SUPPORT
	wais = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseWAISProxy);
#endif
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	auto_proxy = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticProxyConfig);
#endif

	m_proxy = http || https || ftp || gopher || wais
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		|| auto_proxy
#endif
		;

	OpString proxy;
	TRAPD(rc, g_pcnet->GetStringPrefL(PrefsCollectionNetwork::HTTPProxy, proxy));
	if (OpStatus::IsSuccess(rc))
	{
		m_proxy = m_proxy && proxy.Length();
	}

	// Cookie handling
	DWORD flag = 0;

	if(cookie_mode != COOKIE_NONE)
		flag |= 0x01;

#ifdef _ASK_COOKIE
	if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DisplayReceivedCookies))
		flag |= 0x04;
#endif

	if (flag == 0x00)
		m_cookie_normal.Set("Refuse");
	if (flag == 0x05)
		m_cookie_normal.Set("Display");
	if (flag == 0x03)
		m_cookie_normal.Set("Filter");
	if(flag == 0x01)
		m_cookie_normal.Set("Accept");

	if (cookie_mode == COOKIE_NONE || cookie_mode == COOKIE_NO_THIRD_PARTY)
		m_cookie_third_party.Set("Refuse");
	if (cookie_mode == COOKIE_WARN_THIRD_PARTY)
		m_cookie_third_party.Set("Display");
	if (cookie_mode == COOKIE_ALL)
		m_cookie_third_party.Set("Accept");

	m_cookies = cookie_mode != COOKIE_NONE;
	m_cookies3 = cookie_mode == COOKIE_ALL;

	OpString text;

	text.Set("User Agent:");
	text.Append(m_user_agent);
	text.Append("\r\nBuild number:");
	text.Append(VER_BUILD_NUMBER_STR);
	text.Append("\r\nPopups:");
	if (block_all)
	{
		m_popups.Set("all");
		text.Append("Block all");
	}
	else if (unrequested)
	{
		m_popups.Set("unwanted");
		text.Append("Block unwanted");
	}
	else if (background)
	{
		m_popups.Set("background");
		text.Append("Open in background");
	}
	else
	{
		m_popups.Set("none");
		text.Append("Allow");
	}
	text.Append("\r\nJava:");
	text.Append(m_java ? on : off);
	text.Append("\r\nPlug-ins:");
	text.Append(m_plugins ? on : off);
	text.Append("\r\nJavaScript:");
	text.Append(m_javascript ? on : off);
//	text.Append("\r\nCookies:");
//	text.Append(m_cookies ? on : off);
	text.Append("\r\nReferrer logging:");
	text.Append(m_referrer_logging ? on : off);
	text.Append("\r\nProxy:");
	text.Append(m_proxy ? on : off);
	text.Append("\r\nEncoding setting:");
	text.Append(m_encoding);
	text.Append("\r\nDisplay mode:");
//	text.Append(m_document_mode);
//	text.Append(", ");
	text.Append(m_strict_mode ? "Strict" : "Quirks");
	text.Append("\r\nLoad images:");
	text.Append(m_images);
	text.Append("\r\nNormal cookies:");
	text.Append(m_cookie_normal);
	text.Append("\r\nThird party cookies:");
	text.Append(m_cookie_third_party);

	SetWidgetText("Configuration_edit", text.CStr());
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 ReportSiteProblemDialog::OnOk()
{
	OpString address, comment, complete;

	GetWidgetText("Page_address_edit", address);
	GetWidgetText("Comment_edit", comment);

	// Using same escaping as we do for Google
	XMLEscapeString(comment);

	OpString on;
	OpString off;
	on.Set("on");
	off.Set("off");

	OpString identify_as;

	if (m_user_agent.FindI("msie") != KNotFound)
	{
		identify_as.Set("msie");
	}
	else if(m_user_agent.FindI("mozilla") != KNotFound)
	{
		identify_as.Set("mozilla");
	}
	else
	{
		identify_as.Set("opera");
	}

	if (m_images.HasContent())
	{
		m_images.MakeLower();
	}

	OpString category;

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Category_dropdown");
	if (dropdown)
	{
		int selected = dropdown->GetSelectedItem();

		switch (selected)
		{
		case 1:
			category.Set("minor");
			break;
		case 2:
			category.Set("major");
			break;
		case 3:
			category.Set("unusable");
			break;
		case 0:
		default:
			category.Set("unspecified");
			break;
		}
	}

	complete.Set("<?xml version=\"1.0\"?>\n<sitereport version=\"1.0\">\n");
	complete.Append(" <page>\n  <url>");
	complete.Append(address);
	complete.Append("</url>\n");
	complete.Append("  <render mode=\"");
	complete.Append(m_strict_mode ? "strict" : "quirks");
	complete.Append("\"/>\n");
	complete.Append(" </page>\n");
	complete.Append(" <client>\n  <capabilities>\n");
	complete.Append("   <encoding>");
	complete.Append(m_encoding);
	complete.Append("</encoding>\n");
	complete.Append("   <popup block=\"");
	complete.Append(m_popups);
	complete.Append("\"/>\n   <java state=\"");
	complete.Append(m_java ? on : off);
	complete.Append("\"/>\n   <plugins state=\"");
	complete.Append(m_plugins ? on : off);
	complete.Append("\"/>\n   <javascript state=\"");
	complete.Append(m_javascript ? on : off);
	complete.Append("\" errors=\"off\"/>\n   <referrer state=\"");
	complete.Append(m_referrer_logging ? on : off);
	complete.Append("\"/>\n   <proxy state=\"");
	complete.Append(m_proxy ? on : off);
	complete.Append("\"/>\n   <images state=\"");
	complete.Append(m_images);
	complete.Append("\"/>\n   <cookies normal=\"");
	complete.Append(m_cookies ? "accept" : "reject");
	complete.Append("\" thirdparty=\"");
	complete.Append(m_cookies3 ? "accept" : "reject");
	complete.Append("\"/>\n");
	complete.Append("   <identify as=\"");
	complete.Append(identify_as);
	complete.Append("\"/>\n");

	// not completed, don't forget to add to "Advanced" field:

	OpString buildno, version, opsys;

	buildno.Set(VER_BUILD_NUMBER_STR);
	version.Set(VER_NUM_STR);
	opsys.Set(g_op_system_info->GetPlatformStr());

	complete.Append("   <patch state=\"");
	complete.Append("on"); // on|off|partial
	complete.Append("\"/>\n");
	complete.Append("   <version buildno=\"");
	complete.Append(buildno);
	complete.Append("\" version=\"");
	complete.Append(version);
	complete.Append("\" opsys=\"");
	complete.Append(opsys);
	complete.Append("\"/>\n");
	complete.Append("   <contlang>");
	complete.Append("en"); // q-string
	complete.Append("</contlang>\n");
	complete.Append("   <css mode=\"author\"/>\n"); // author|user

	// end not completed section

	complete.Append("  </capabilities>\n");
	complete.Append(" </client>\n");
	complete.Append(" <comment>");
	complete.Append(comment);
	complete.Append("</comment>\n");
	complete.Append(" <category>");
	complete.Append(category);
	complete.Append("</category>\n");
	complete.Append("</sitereport>");

	OpString8 complete8;
	complete8.SetUTF8FromUTF16(complete.CStr());

	URL content_url = urlManager->GetNewOperaURL();
	content_url.Unload();
	content_url.ForceStatus(URL_LOADING);
	content_url.SetAttribute(URL::KMIME_ForceContentType, "application/xml;charset=utf-8");
	content_url.WriteDocumentData(URL::KNormal, complete8.CStr(), complete8.Length());
	content_url.WriteDocumentDataFinished();
	content_url.ForceStatus(URL_LOADED);

	OpString post_address;
	post_address.Set("http://xml.opera.com/sites/");

	URL form_url = urlManager->GetURL(post_address.CStr(), UNI_L(""), true);
	form_url.SetHTTP_Method(HTTP_METHOD_POST);

	Upload_Multipart *form = OP_NEW(Upload_Multipart, ());

	if (form)
	{
		TRAPD(op_err, form->InitL("multipart/form-data"));
		OpStatus::Ignore(op_err);
				
		Upload_URL *element = OP_NEW(Upload_URL, ());
		if (element)
		{
			TRAP(op_err, element->InitL(content_url, NULL, "form-data", "xmldata", NULL, ENCODING_NONE));
			TRAP(op_err, element->SetCharsetL(content_url.GetAttribute(URL::KMIME_CharSet)));
			
			form->AddElement(element);
			TRAP(op_err, form->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION));
		
			form_url.SetHTTP_Data(form, TRUE);
		
			URL_InUse* url_inuse = OP_NEW(URL_InUse, ()); // SMALL LEAK - FIXME
			if (url_inuse)
			{
				url_inuse->SetURL(form_url);
			
				URL url2 = URL();
#ifdef _DEBUG
				// Not silent loading:
				BOOL3 open_in_new_window = YES;
				BOOL3 open_in_background = MAYBE;
				Window *feedback_window;
				feedback_window = windowManager->GetAWindow(TRUE, open_in_new_window, open_in_background);
				feedback_window->OpenURL(form_url, DocumentReferrer(url2), true, FALSE, -1, true);
#else
				//  Silent loading:
				form_url.Reload(g_main_message_handler, url2, TRUE, FALSE);
#endif
			}
		}
		else
			OP_DELETE(form);
	}
	return 0;
}

/***********************************************************************************
**
**	OnCancel
**
***********************************************************************************/

void ReportSiteProblemDialog::OnCancel()
{
}

/***********************************************************************************
**
**	XMLEscapeString
**
***********************************************************************************/

OP_STATUS ReportSiteProblemDialog::XMLEscapeString(OpString& string)
{
	const uni_char* ptr = string.CStr();
	int len = string.Length();

	OpString escaped_string;
	uni_char buffer[7];

	while (len--)
	{
		unsigned int character = *ptr++;

		const uni_char* tag = NULL;

		switch(character)
		{
			case '"' :  tag = UNI_L("&quot;");
				break;
			case '&' :  tag = UNI_L("&amp;");
				break;
			case '<' :  tag = UNI_L("&lt;");
				break;
			case '>' :  tag = UNI_L("&gt;");
				break;
		}

		if (tag == NULL)
		{
			uni_snprintf(buffer, 7, UNI_L("%c"), character);
			escaped_string.Append(buffer);
		}
		else
		{
			escaped_string.Append(tag);
		}
	}
	return string.Set(escaped_string);
}
