/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef ENABLE_USAGE_REPORT

#include "adjunct/quick/usagereport/UsageReport.h"

#include "modules/util/opfile/opfile.h"
#include "modules/util/opstrlst.h"
#include "modules/util/OpLineParser.h"
#include "modules/upload/upload.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/OperaTurboManager.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_opera_account.h"
#include "modules/wand/wandmanager.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/content_filter/content_filter.h"
#include "modules/history/OpHistoryModel.h"
#include "modules/history/OpHistoryPage.h"
#ifdef _SSL_SUPPORT_
# include "modules/libssl/sslv3.h"
# include "modules/libssl/sslopt.h"
# include "modules/libssl/sslcctx.h"
#endif

#ifdef _UNIX_DESKTOP_
#include "platforms/quix/environment/DesktopEnvironment.h"
#endif

#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/OpLocale.h"

#ifdef MSWIN
# include <sys/types.h>
# include <sys/stat.h>
#endif

#define BUF_LEN 128
#define UPLOAD_TIMEOUT 7*24*60*60 // 1 week, in seconds

OpUsageReportManager *g_usage_report_manager = NULL;
WorkspaceReport *g_workspace_report = NULL;
ActionReport *g_action_report = NULL;
BittorrentReport *g_bittorrent_report = NULL;
MailReport *g_mail_report = NULL;

// ---------------------------------------------------------------------------------

UINT32 UsageReportInfoDialog::OnOk()
{
	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::EnableUsageStatistics, 1));
	return SimpleDialog::OnOk();
}

void UsageReportInfoDialog::OnCancel()
{
	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::EnableUsageStatistics, 0));
}

// ---------------------------------------------------------------------------------

OP_STATUS OpUsageReport::ReportValue(INT32 value, const uni_char *section, const uni_char *key, ReportOperation operation, PrefsFile *prefs_file)
{
	// If this assert goes off you have a longer string than what will fit in the database.
	// Either shorten the string, or find an other way to represent it, or contact the
	// database owner (currently Espen Myhre, 14.07.10 oyvindo)
	OP_ASSERT(uni_strlen(key) <= 64);

	if (!prefs_file)
		return OpStatus::ERR_NULL_POINTER;

	int val = 0;

	switch(operation)
	{
		case ADD:
			RETURN_IF_LEAVE(val = prefs_file->ReadIntL(section, key));
			value += val;
			break;
		case MINIMUM:
			RETURN_IF_LEAVE(val = prefs_file->ReadIntL(section, key, value));
			if (val < value)
				value = val;
			break;
		case MAXIMUM:
			RETURN_IF_LEAVE(val = prefs_file->ReadIntL(section, key, value));
			if (val > value)
				value = val;
			break;
	}

	RETURN_IF_LEAVE(prefs_file->WriteIntL(section, key, value));
	return OpStatus::OK;
}

OP_STATUS OpUsageReport::ReportValue(const uni_char* value, const uni_char *section, const uni_char *key, PrefsFile *prefs_file)
{
	if(!value || !section || !key || !prefs_file)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_LEAVE(prefs_file->WriteStringL(section, key, value));
	return OpStatus::OK;
}

OpUsageReportManager::OpUsageReportManager(const uni_char *filename, OpFileFolder folder)
: m_prefs_file(NULL)
	, m_folder(folder)
	, m_search_report(NULL)
	, m_error_report(NULL)
	, m_button_click_report(NULL)
	, m_content_sharing_report(NULL)
{
	m_filename.Set(filename);

	AddReport(OP_NEW(WandReport, ()));
	AddReport(OP_NEW(SkinReport, ()));
	AddReport(g_mail_report = OP_NEW(MailReport, ()));
	AddReport(OP_NEW(FeedsReport, ()));
	AddReport(OP_NEW(HistoryReport, ()));
	AddReport(OP_NEW(PreferencesReport, ()));
	AddReport(g_workspace_report = OP_NEW(WorkspaceReport, ()));
	AddReport(g_action_report = OP_NEW(ActionReport, ()));
	AddReport(g_bittorrent_report = OP_NEW(BittorrentReport, ()));
	AddReport(OP_NEW(AppearanceReport, ()));
	AddReport(OP_NEW(BookmarksReport, ()));
	AddReport(OP_NEW(ContactsReport, ()));
	AddReport(OP_NEW(NotesReport, ()));
	AddReport(OP_NEW(AccountReport, ()));
	AddReport(OP_NEW(GeneralReport, ()));
	AddReport(OP_NEW(SpeedDialReport, ()));
	AddReport(m_search_report = OP_NEW(SearchReport, ()));
	AddReport(OP_NEW(UpgradeReport, ()));
	AddReport(m_error_report = OP_NEW(ErrorReport, ()));
	AddReport(m_button_click_report = OP_NEW(ButtonClickReport, ()));
	AddReport(m_content_sharing_report = OP_NEW(ContentSharingReport, ()));
#ifdef WEB_TURBO_MODE
	AddReport(OP_NEW(TurboReport, ()));
#endif
#ifdef VEGA_USE_HW
	AddReport(OP_NEW(HWAReport, ()));
#endif

	m_timer.SetTimerListener(this);

	// Fetch report timeout from prefs
	m_report_timeout = g_pcui->GetIntegerPref(PrefsCollectionUI::ReportTimeoutForUsageStatistics); // in minutes
	if ((int)m_report_timeout < 1)
		m_report_timeout = 1;
	m_report_timeout = m_report_timeout*60*1000; // in ms
	m_timer.Start(m_report_timeout);

	m_upload_task.SetListener(this);
	m_upload_task.InitTask("Usage Report Upload");
	m_upload_task.AddScheduledTask(UPLOAD_TIMEOUT);
}

OpUsageReportManager::~OpUsageReportManager()
{
	if(!g_application->IsEmBrowser() && g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUsageStatistics))
	{
		GenerateReport();
		OP_DELETE(m_prefs_file);
	}
}

OP_STATUS OpUsageReportManager::GenerateReport()
{
	if(g_application->IsEmBrowser() || !g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUsageStatistics))
	{
		return OpStatus::ERR;
	}

	if(!m_prefs_file)
	{
		m_prefs_file = CreateXMLFile(m_filename.CStr(), m_folder);
	}

	for(UINT32 i=0; i<m_reports.GetCount(); i++)
	{
		m_reports.Get(i)->GenerateReport(m_prefs_file);
	}

	TRAPD(err, m_prefs_file->CommitL());

	return OpStatus::OK;
}

OP_STATUS OpUsageReportManager::Upload()
{
	if(g_application->IsEmBrowser() || !g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUsageStatistics))
	{
		return OpStatus::ERR;
	}

	URL content_url = urlManager->GetNewOperaURL();
	content_url.Unload();
	content_url.ForceStatus(URL_LOADING);
	content_url.SetAttribute(URL::KMIME_ForceContentType, "application/xml;charset=utf-8");

	OpFile file;
	RETURN_IF_ERROR(file.Construct(m_filename.CStr(), m_folder));

	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	while(!file.Eof())
	{
		OpString8 data;
		data.Reserve(BUF_LEN);

		OpFileLength bytes_read;

		RETURN_IF_ERROR(file.Read(data.CStr(), BUF_LEN, &bytes_read));

		content_url.WriteDocumentData(URL::KNormal, data.CStr(), (unsigned int)bytes_read);
	}

	RETURN_IF_ERROR(file.Close());

	content_url.WriteDocumentDataFinished();
	content_url.ForceStatus(URL_LOADED);

	OpString post_address;
	post_address.Set("http://xml.opera.com/desktopusage/");

	URL form_url = urlManager->GetURL(post_address.CStr(), UNI_L(""), true);
	form_url.SetHTTP_Method(HTTP_METHOD_POST);

	Upload_Multipart *form = OP_NEW(Upload_Multipart, ());

	if(form)
	{
		TRAPD(op_err, form->InitL("multipart/form-data"));
		OpStatus::Ignore(op_err);

		Upload_URL *element = OP_NEW(Upload_URL, ());

		if(element)
		{
			TRAP(op_err, element->InitL(content_url, NULL, "form-data", "xmldata", NULL, ENCODING_NONE));
			TRAP(op_err, element->SetCharsetL(content_url.GetAttribute(URL::KMIME_CharSet)));

			form->AddElement(element);
			TRAP(op_err, form->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION));

			form_url.SetHTTP_Data(form, TRUE);

			m_url_inuse.SetURL(form_url);

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

			time_t time = g_timecache->CurrentTime();
			struct tm* gmt_time = op_gmtime(&time);

			OpString buf;
			if(!buf.Reserve(128))
			{
				return OpStatus::ERR_NO_MEMORY;
			}
			unsigned int len = g_oplocale->op_strftime(buf.CStr(), 128,
													   UNI_L("usagereport/report_%Y-%m-%d_%H-%M-%S.xml"), gmt_time);
			buf.CStr()[len] = 0;

			OpFile copy;
			RETURN_IF_ERROR(copy.Construct(buf.CStr(), OPFILE_HOME_FOLDER));
			RETURN_IF_ERROR(copy.CopyContents(&file, FALSE));
			RETURN_IF_ERROR(copy.Close());

			RETURN_IF_ERROR(file.Delete());

			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

PrefsFile* OpUsageReportManager::CreateXMLFile(const uni_char *filename, OpFileFolder folder)
{
	OpFile file;

	if (OpStatus::IsError(file.Construct(filename, folder)))
		return NULL;

	PrefsFile* OP_MEMORY_VAR prefsfile = OP_NEW(PrefsFile, (PREFS_XML));
	if (!prefsfile)
		return NULL;

	TRAPD(err, prefsfile->ConstructL());
	if (OpStatus::IsError(err))
	{
		OP_DELETE(prefsfile);
		return NULL;
	}

	TRAP(err, prefsfile->SetFileL(&file));
	if (OpStatus::IsError(err))
	{
		OP_DELETE(prefsfile);
		return NULL;
	}

	TRAP(err, prefsfile->LoadAllL());

	return prefsfile;
}

void OpUsageReportManager::ShowInfoDialog()
{
    if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
		return;

	UsageReportInfoDialog *dialog = OP_NEW(UsageReportInfoDialog, ());

	OpString message, title;

	g_languageManager->GetString(Str::D_USAGEREPORT_INFO_TEXT, message);
	g_languageManager->GetString(Str::D_USAGE_REPORT_INFO_TITLE, title);
	dialog->Init(WINDOW_NAME_USAGE_REPORT_INFO, title, message, NULL, Dialog::TYPE_YES_NO, Dialog::IMAGE_WARNING, TRUE);
}

void OpUsageReportManager::OnTimeOut(OpTimer *timer)
{
	if(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUsageStatistics))
	{
		GenerateReport();
		
		// Fetch report timeout from prefs
		m_report_timeout = g_pcui->GetIntegerPref(PrefsCollectionUI::ReportTimeoutForUsageStatistics); // in minutes
		if ((int)m_report_timeout < 1)
			m_report_timeout = 1;
		m_report_timeout = m_report_timeout*60*1000; // in ms
		timer->Start(m_report_timeout);
	}
}

void OpUsageReportManager::OnTaskTimeOut(OpScheduledTask* task)
{
	OpStatus::Ignore(Upload());
}

time_t OpUsageReportManager::GetLastReportUpload()
{
	time_t last_timeout = 0;
	g_task_scheduler->GetLastTimeout(&m_upload_task, last_timeout);
	return last_timeout;
}

// ---------------------------------------------------------------------------------

OP_STATUS ButtonClickReport::GenerateReport(PrefsFile *prefs_file) 
{ 
	OpString section;
	RETURN_IF_ERROR(section.Set(UNI_L("ButtonClicks")));
	
	OpAutoPtr<OpHashIterator> it(m_clicks_table.GetIterator());
	if(it.get())
	{
		OP_STATUS s = it->First();
		while(OpStatus::IsSuccess(s))
		{
			uni_char* key = (uni_char*)it->GetKey();
			ButtonClickEntry* data = reinterpret_cast<ButtonClickEntry *>(it->GetData());
			if(key && data)
			{
				RETURN_IF_ERROR(ReportValue(data->m_clicks, section, data->m_name.CStr(), ADD, prefs_file));
			}
			s = it->Next();
		}
	}
	return OpStatus::OK; 
}

void ButtonClickReport::AddClick(OpWidget* button)
{
	OpString key;
	OpInputAction *first_action = button->GetAction();
	OpInputAction *input_action = first_action;

	// Find the first enabled action in the action list
	while (input_action)
	{
		// No more actions so exit the loop or an action we don't handle in this loop, break out (we only handle OR actions here)
		if (input_action->GetActionOperator() == OpInputAction::OPERATOR_NONE || input_action->GetActionOperator() != OpInputAction::OPERATOR_OR)
			break;

		INT32 state = g_input_manager->GetActionState(input_action, button->GetParentInputContext());

		// Only look for the next action if this one is specifically disabled
		if (!(state & OpInputAction::STATE_DISABLED))
			break;

		// Get the next item
		input_action = input_action->GetActionOperator() != OpInputAction::OPERATOR_PLUS ? input_action->GetNextInputAction() : NULL;
	}
	// If the above parsing managed to set the action to NULL then re-choose the
	// original action as the action for this menu item
	if(input_action)
	{
		key.Set(OpInputAction::GetStringFromAction(input_action->GetAction()));
	}
	else
	{
		if(first_action)
		{
			key.Set(OpInputAction::GetStringFromAction(first_action->GetAction()));
		}
		else
		{
			// use the name as key if no action is present
			key.Set(button->GetName());
			if(key.IsEmpty())
			{
				// final fallback, use the text on the button
				button->GetText(key);
			}
		}
	}
	if(key.HasContent())
	{
		ButtonClickEntry *entry;

		if(OpStatus::IsSuccess(m_clicks_table.GetData(key.CStr(), &entry)))
		{
			if(entry)
			{
				entry->m_clicks++;
			}
		}
		else
		{
			entry = OP_NEW(ButtonClickEntry, (key));
			if(entry)
			{
				// not critical if OOM, so just clean up and pretend nothing happened
				if(OpStatus::IsError(m_clicks_table.Add(entry->m_name.CStr(), entry)))
				{
					OP_DELETE(entry);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------------

OP_STATUS AccountReport::GenerateReport(PrefsFile *prefs_file)
{
	AccountManager* manager = g_m2_engine ? g_m2_engine->GetAccountManager() : NULL;
	if(!manager)
	{
		return OpStatus::ERR;
	}

	UINT16 nAccounts = manager->GetAccountCount();

	UINT16 nMailAccounts = 0;
	UINT16 nNewsAccounts = 0;
	UINT16 nChatAccounts = 0;
	UINT16 nRSSAccounts = 0;
	UINT16 nAccountsSecureConnectionIn = 0;
	UINT16 nAccountsSecureConnectionOut = 0;
	UINT16 nAccountsChangedOutgoingAuthenticationType = 0;
	UINT16 nAccountsPreferHTMLCompose = 0;

	for(UINT16 i=0; i<nAccounts; i++)
	{
		Account *account = manager->GetAccountByIndex(i);
		if(account)
		{
			Message::TextpartSetting html_compose = account->PreferHTMLComposing();
			if(html_compose == Message::PREFER_TEXT_HTML)
			{
				nAccountsPreferHTMLCompose++;
			}

			switch(account->GetIncomingProtocol())
			{
				case AccountTypes::SMTP:
				case AccountTypes::POP:
				{
					AccountTypes::AuthenticationType authentication_type = account->GetOutgoingAuthenticationMethod();
					if(authentication_type != AccountTypes::NONE)
					{
						nAccountsChangedOutgoingAuthenticationType++;
					}
				}
				case AccountTypes::IMAP:
				{
					if(account->GetUseSecureConnectionIn())
					{
						nAccountsSecureConnectionIn++;
					}
					if(account->GetUseSecureConnectionOut())
					{
						nAccountsSecureConnectionOut++;
					}

					nMailAccounts++;
					break;
				}
				case AccountTypes::NEWS:
				{
					nNewsAccounts++;
					break;
				}
				case AccountTypes::IRC:
				case AccountTypes::MSN:
				case AccountTypes::AOL:
				case AccountTypes::ICQ:
#ifdef JABBER_SUPPORT
				case AccountTypes::JABBER:
#endif // JABBER_SUPPORT
				{
					nChatAccounts++;
					break;
				}
				case AccountTypes::RSS:
				{
					nRSSAccounts++;
					break;
				}

			}
		}
	}

	RETURN_IF_ERROR(ReportValue(nMailAccounts, UNI_L("Mail"), UNI_L("NumberOfAccounts"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(nAccountsSecureConnectionIn, UNI_L("Mail"), UNI_L("NumberOfAccountsSecureConnectionIn"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(nAccountsSecureConnectionOut, UNI_L("Mail"), UNI_L("NumberOfAccountsSecureConnectionOut"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(nAccountsPreferHTMLCompose, UNI_L("Mail"), UNI_L("NumberOfAccountsPreferHTMLCompose"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(nAccountsChangedOutgoingAuthenticationType, UNI_L("Mail"), UNI_L("NumberOfAccountsChangedOutgoingAuthenticationType"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(nNewsAccounts, UNI_L("News"), UNI_L("NumberOfAccounts"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(nChatAccounts, UNI_L("Chat"), UNI_L("NumberOfAccounts"), REPLACE, prefs_file));

	return OpStatus::OK;
}

// ---------------------------------------------------------------------------------

OP_STATUS PreferencesReport::GenerateReport(PrefsFile *prefs_file)
{
	/**** General Page ****/

	// Startup
	INT32 startup_type = g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType);
	RETURN_IF_ERROR(ReportValue(startup_type, UNI_L("PreferencesGeneral"), UNI_L("Startup"), REPLACE, prefs_file));

	// Pop-ups
	INT32 popup_value = 0;
	if(g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget))
	{
		popup_value = 3;
	}
	else if (g_pcjs->GetIntegerPref(PrefsCollectionJS::TargetDestination) == POPUP_WIN_BACKGROUND)
	{
		popup_value = 1;
	}
	if (g_pcjs->GetIntegerPref(PrefsCollectionJS::IgnoreUnrequestedPopups))
	{
		popup_value = 2;
	}

	RETURN_IF_ERROR(ReportValue(popup_value, UNI_L("PreferencesGeneral"), UNI_L("Popups"), REPLACE, prefs_file));

	// Homepage
	OpStringC homepage = g_pccore->GetStringPref(PrefsCollectionCore::HomeURL);

	OpString default_homepage;
	TRAPD(rc, g_prefsManager->GetPreferenceL("User Prefs", "Home URL", default_homepage, TRUE));
	if (OpStatus::IsError(rc))
		default_homepage.Empty();

	INT32 changed_homepage = homepage.Compare(default_homepage) == 0 ? 0 : 1;
	RETURN_IF_ERROR(ReportValue(changed_homepage, UNI_L("PreferencesGeneral"), UNI_L("ChangedHomepage"), REPLACE, prefs_file));


	/**** Web Pages Page ****/

	// Colors
	COLORREF background_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND);
	RETURN_IF_ERROR(ReportValue(background_color, UNI_L("PreferencesWebPages"), UNI_L("BackgroundColor"), REPLACE, prefs_file));

	COLORREF link_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_LINK);
	RETURN_IF_ERROR(ReportValue(link_color, UNI_L("PreferencesWebPages"), UNI_L("NormalLinkColor"), REPLACE, prefs_file));

	COLORREF visited_link_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_VISITED_LINK);
	RETURN_IF_ERROR(ReportValue(visited_link_color, UNI_L("PreferencesWebPages"), UNI_L("VisitedLinkColor"), REPLACE, prefs_file));

	// Scaling
	int scale = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale);
	RETURN_IF_ERROR(ReportValue(scale, UNI_L("PreferencesWebPages"), UNI_L("PageZoom"), REPLACE, prefs_file));

	// Images
	INT32 show_image_state = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowImageState);
	RETURN_IF_ERROR(ReportValue(show_image_state, UNI_L("PreferencesWebPages"), UNI_L("ShowImageState"), REPLACE, prefs_file));

	// Fit to width
	INT32 rendering_mode = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::RenderingMode);
	RETURN_IF_ERROR(ReportValue(rendering_mode, UNI_L("PreferencesWebPages"), UNI_L("FitToWidth"), REPLACE, prefs_file));

	// Link underline
	INT32 link_has_underline = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasUnderline);
	RETURN_IF_ERROR(ReportValue(link_has_underline, UNI_L("PreferencesWebPages"), UNI_L("NormalLinkHasUnderline"), REPLACE, prefs_file));

	// Visited link underlind
	INT32 visited_link_has_underline = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinkHasUnderline);
	RETURN_IF_ERROR(ReportValue(visited_link_has_underline, UNI_L("PreferencesWebPages"), UNI_L("VisitedLinkHasUnderline"), REPLACE, prefs_file));


	/**** Tabs Page ****/

	// Allow window with no tabs
	INT32 allow_empty_workspace = g_pcui->GetIntegerPref(PrefsCollectionUI::AllowEmptyWorkspace);
	RETURN_IF_ERROR(ReportValue(allow_empty_workspace, UNI_L("PreferencesTabs"), UNI_L("AllowWindowWithNoTabs"), REPLACE, prefs_file));

	// Click on tab to minimize
	INT32 click_to_minimize = g_pcui->GetIntegerPref(PrefsCollectionUI::ClickToMinimize);
	RETURN_IF_ERROR(ReportValue(click_to_minimize, UNI_L("PreferencesTabs"), UNI_L("ClickTabToMinimize"), REPLACE, prefs_file));

	// Show close button on each tab
	INT32 show_close_buttons = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowCloseButtons);
	RETURN_IF_ERROR(ReportValue(show_close_buttons, UNI_L("PreferencesTabs"), UNI_L("ShowCloseButtons"), REPLACE, prefs_file));

	// Open windows instead of tabs
	INT32 sdi = g_pcui->GetIntegerPref(PrefsCollectionUI::SDI);
	RETURN_IF_ERROR(ReportValue(sdi, UNI_L("PreferencesTabs"), UNI_L("OpenWindowsInsteadOfTabs"), REPLACE, prefs_file));

	// Tab cycle type
	INT32 cycle_type = g_pcui->GetIntegerPref(PrefsCollectionUI::WindowCycleType);
	RETURN_IF_ERROR(ReportValue(cycle_type, UNI_L("PreferencesTabs"), UNI_L("WindowCycleType"), REPLACE, prefs_file));

	// New tabs
	INT32 policy = g_pcui->GetIntegerPref(PrefsCollectionUI::MaximizeNewWindowsWhenAppropriate);
	RETURN_IF_ERROR(ReportValue(policy, UNI_L("PreferencesTabs"), UNI_L("NewTabsPolicy"), REPLACE, prefs_file));

	// Reuse current tab
	INT32 new_window = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow);
	RETURN_IF_ERROR(ReportValue(!new_window, UNI_L("PreferencesTabs"), UNI_L("ReuseCurrentTab"), REPLACE, prefs_file));

	// Open new tab next to active
	INT32 open_page_next_to_current = g_pcui->GetIntegerPref(PrefsCollectionUI::OpenPageNextToCurrent);
	RETURN_IF_ERROR(ReportValue(open_page_next_to_current, UNI_L("PreferencesTabs"), UNI_L("OpenNewTabNextToActive"), REPLACE, prefs_file));


	/**** Browsing Page ****/

	// Show scroll bars
	INT32 show_scroll_bars = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowScrollbars);
	RETURN_IF_ERROR(ReportValue(show_scroll_bars, UNI_L("PreferencesBrowsing"), UNI_L("ShowScrollBars"), REPLACE, prefs_file));

	// Smooth scrolling
	INT32 smooth_scrolling = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::SmoothScrolling);
	RETURN_IF_ERROR(ReportValue(smooth_scrolling, UNI_L("PreferencesBrowsing"), UNI_L("SmoothScrolling"), REPLACE, prefs_file));

	// Show Web page dimensions in title bar
	INT32 show_win_size = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowWinSize);
	RETURN_IF_ERROR(ReportValue(show_win_size, UNI_L("PreferencesBrowsing"), UNI_L("ShowWebPageDimensions"), REPLACE, prefs_file));

	// Show window menu
	INT32 show_window_menu = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowWindowMenu);
	RETURN_IF_ERROR(ReportValue(show_window_menu, UNI_L("PreferencesBrowsing"), UNI_L("ShowWindowMenu"), REPLACE, prefs_file));

	// Confirm exit
	INT32 show_exit_dialog = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowExitDialog);
	RETURN_IF_ERROR(ReportValue(show_exit_dialog, UNI_L("PreferencesBrowsing"), UNI_L("ConfirmExit"), REPLACE, prefs_file));

	// Loading
	INT32 delay = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FirstUpdateDelay);
	RETURN_IF_ERROR(ReportValue(delay, UNI_L("PreferencesBrowsing"), UNI_L("FirstUpdateDelay"), REPLACE, prefs_file));

	// Page icons
	INT32 page_icons = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AlwaysLoadFavIcon);
	RETURN_IF_ERROR(ReportValue(page_icons, UNI_L("PreferencesBrowsing"), UNI_L("PageIcons"), REPLACE, prefs_file));


	/**** Notifications page ****/

	// Enable program sounds
	INT32 enable_program_sounds = g_pcui->GetIntegerPref(PrefsCollectionUI::SoundsEnabled);
	RETURN_IF_ERROR(ReportValue(enable_program_sounds, UNI_L("PreferencesNotifications"), UNI_L("EnableProgramSounds"), REPLACE, prefs_file));

#ifdef M2_SUPPORT
	// Show notification for new messages
	INT32 show_notification_new_messages = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNotificationForNewMessages);
	RETURN_IF_ERROR(ReportValue(show_notification_new_messages, UNI_L("PreferencesNotifications"), UNI_L("ShowNotificationNewMessages"), REPLACE, prefs_file));
#endif

	// Show notification for blocked pop-ups
	INT32 show_notification_blocked_popups = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNotificationForBlockedPopups);
	RETURN_IF_ERROR(ReportValue(show_notification_blocked_popups, UNI_L("PreferencesNotifications"), UNI_L("ShowNotificationBlockedPopups"), REPLACE, prefs_file));

	// Show notification for finished transfers
	INT32 show_notification_finished_transfers = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNotificationForFinishedTransfers);
	RETURN_IF_ERROR(ReportValue(show_notification_finished_transfers ,UNI_L("PreferencesNotifications"), UNI_L("ShowNotificationFinishedTransfers"), REPLACE, prefs_file));


	/**** Content Page ****/

	// GIF/SVG Animation
	INT32 animation_enabled = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowAnimation);
	RETURN_IF_ERROR(ReportValue(animation_enabled, UNI_L("PreferencesContent"), UNI_L("AnimationEnabled"), REPLACE, prefs_file));

	// Javascript enabled
	INT32 javascript_enabled = g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled);
	RETURN_IF_ERROR(ReportValue(javascript_enabled, UNI_L("PreferencesContent"), UNI_L("JavascriptEnabled"), REPLACE, prefs_file));

	// Plugins enabled
	INT32 plugins_enabled = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled);
	RETURN_IF_ERROR(ReportValue(plugins_enabled, UNI_L("PreferencesContent"), UNI_L("PluginsEnabled"), REPLACE, prefs_file));

	// Site preferences
	INT32 noOverriddenHosts = 0;
	OpString_list *hosts = g_prefsManager->GetOverriddenHostsL();

	if (hosts)
	{
		int count = hosts->Count();
		for (int i = 0; i < count; i++)
		{
			OpString host_name = hosts->Item(i);
			if (g_prefsManager->IsHostOverridden(host_name.CStr(), FALSE) == HostOverrideActive)
			{
				noOverriddenHosts++;
			}
		}
		OP_DELETE(hosts);
	}

	RETURN_IF_ERROR(ReportValue(noOverriddenHosts, UNI_L("PreferencesContent"), UNI_L("NumberOfOverriddenHosts"), REPLACE, prefs_file));

	// Blocked content
	{
		OpAutoVector<OpString> exclude_list;
		RETURN_IF_ERROR(g_urlfilter->CreateExcludeList(exclude_list));

		INT32 noBlockedSites = exclude_list.GetCount();

		RETURN_IF_ERROR(ReportValue(noBlockedSites, UNI_L("PreferencesContent"), UNI_L("NumberOfBlockedSites"), REPLACE, prefs_file));
	}

	OpFile css_file;
	TRAPD(rc1, g_pcfiles->GetFileL(PrefsCollectionFiles::LocalCSSFile, css_file));

	uni_char *p = uni_strstr(css_file.GetFullPath(), UNI_L("user.css"));

	RETURN_IF_ERROR(ReportValue(p ? 0 : 1, UNI_L("PreferencesContent"), UNI_L("ChangedUserStyleFile"), REPLACE, prefs_file));

	/**** Security Page ****/

#ifdef _SSL_SUPPORT_
	// Master password
	SSL_Options* tmp_optionsManager = g_ssl_api->CreateSecurityManager();
	if (tmp_optionsManager)
	{
		OpAutoPtr<SSL_Options> om_ap(tmp_optionsManager);
		if (OpStatus::IsSuccess(tmp_optionsManager->Init(SSL_ClientStore)))
		{
			BOOL has_master_password = (tmp_optionsManager->IsSecurityPasswordSet());

			RETURN_IF_ERROR(ReportValue(has_master_password, UNI_L("PreferencesSecurity"), UNI_L("HasMasterPassword"), REPLACE, prefs_file));

			// Master password for email and wand
			INT32 use_mail_password = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseParanoidMailpassword);
			RETURN_IF_ERROR(ReportValue(use_mail_password, UNI_L("PreferencesSecurity"), UNI_L("UseMailPassword"), REPLACE, prefs_file));

			// Password life time
			INT32 password_life_time = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SecurityPasswordLifeTime);
			RETURN_IF_ERROR(ReportValue(password_life_time, UNI_L("PreferencesSecurity"), UNI_L("PasswordLifeTime"), REPLACE, prefs_file));
		}
	}
#endif

#ifdef TRUST_RATING
	INT32 enable_trust_rating = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableTrustRating);
	RETURN_IF_ERROR(ReportValue(enable_trust_rating, UNI_L("PreferencesSecurity"), UNI_L("EnableTrustRating"), REPLACE, prefs_file));
#endif // TRUST_RATING

#ifdef _SSL_SUPPORT_
	// Personal Certificates
	int nPersonalCertificates = 0;
	tmp_optionsManager = g_ssl_api->CreateSecurityManager();
	if(tmp_optionsManager)
	{
		if (OpStatus::IsSuccess(tmp_optionsManager->Init()))
		{
			tmp_optionsManager->Set_RegisterChanges(TRUE);
			SSL_Certificate_DisplayContext *personal_cert_context = OP_NEW(SSL_Certificate_DisplayContext, (IDM_PERSONAL_CERTIFICATES_BUTT));
			if(personal_cert_context)
			{
				personal_cert_context->SetExternalOptionsManager(tmp_optionsManager);

				OpString ciphername;

				while(personal_cert_context->GetCertificateShortName(ciphername, nPersonalCertificates))
					nPersonalCertificates++;

				OP_DELETE(personal_cert_context);
			}
		}
		OP_DELETE(tmp_optionsManager);
	}
	RETURN_IF_ERROR(ReportValue(nPersonalCertificates, UNI_L("PreferencesSecurity"), UNI_L("NumberOfPersonalCertificates"), REPLACE, prefs_file));

#endif

#ifdef AUTO_UPDATE_SUPPORT
	INT32 auto_update_level = g_pcui->GetIntegerPref(PrefsCollectionUI::LevelOfUpdateAutomation);
	RETURN_IF_ERROR(ReportValue(auto_update_level, UNI_L("PreferencesSecurity"), UNI_L("AutoUpdateLevel"), REPLACE, prefs_file));
#endif

	/**** Network Page ****/

	// HTTP Proxy
	BOOL http_proxy = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPProxy);
	RETURN_IF_ERROR(ReportValue(http_proxy, UNI_L("PreferencesNetwork"), UNI_L("HttpProxy"), REPLACE, prefs_file));

	// HTTPS Proxy
	BOOL https_proxy = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPSProxy);
	RETURN_IF_ERROR(ReportValue(https_proxy, UNI_L("PreferencesNetwork"), UNI_L("HttpsProxy"), REPLACE, prefs_file));

	// Automatic Proxy
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	int automatic_proxy = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticProxyConfig);
	RETURN_IF_ERROR(ReportValue(automatic_proxy, UNI_L("PreferencesNetwork"), UNI_L("AutomaticProxy"), REPLACE, prefs_file));
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
	// Localhost Proxy
	OpString proxy;

	BOOL localhost_proxy = FALSE;

	TRAP(rc, g_pcnet->GetStringPrefL(PrefsCollectionNetwork::HTTPProxy, proxy));

	if(proxy.CStr() && uni_strstr(proxy.CStr(), UNI_L("localhost")))
	{
		localhost_proxy = TRUE;
	}

	RETURN_IF_ERROR(ReportValue(localhost_proxy, UNI_L("PreferencesNetwork"), UNI_L("LocalhostProxy"), REPLACE, prefs_file));

	// Encode international web addresses with UTF8
	INT32 use_utf8_urls = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseUTF8Urls);
	RETURN_IF_ERROR(ReportValue(use_utf8_urls, UNI_L("PreferencesNetwork"), UNI_L("UseUTF8Urls"), REPLACE, prefs_file));

	// Enable referrer logging
	INT32 refferer_enabled = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ReferrerEnabled);
	RETURN_IF_ERROR(ReportValue(refferer_enabled, UNI_L("PreferencesNetwork"), UNI_L("ReferrerEnabled"), REPLACE, prefs_file));

	// Enable automatic redirection
	INT32 enable_client_pull = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableClientPull);
	RETURN_IF_ERROR(ReportValue(enable_client_pull, UNI_L("PreferencesNetwork"), UNI_L("EnableClientPull"), REPLACE, prefs_file));

	// Max connections to server
	INT32 max_connections_to_server = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsServer);
	RETURN_IF_ERROR(ReportValue(max_connections_to_server, UNI_L("PreferencesNetwork"), UNI_L("MaxConnectionsServer"), REPLACE, prefs_file));

	// Max connections total
	INT32 max_connections_total = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsTotal);
	RETURN_IF_ERROR(ReportValue(max_connections_total, UNI_L("PreferencesNetwork"), UNI_L("MaxConnectionsTotal"), REPLACE, prefs_file));


	/**** Cookies Page ****/

	// Cookies
	INT32 cookies_enabled = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled);
	RETURN_IF_ERROR(ReportValue(cookies_enabled, UNI_L("PreferencesCookies"), UNI_L("CookiesEnabled"), REPLACE, prefs_file));

	INT32 accept_cookies_session_only = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AcceptCookiesSessionOnly);
	RETURN_IF_ERROR(ReportValue(accept_cookies_session_only, UNI_L("PreferencesCookies"), UNI_L("AcceptCookiesSessionOnly"), REPLACE, prefs_file));

	INT32 display_received_cookies = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DisplayReceivedCookies);
	RETURN_IF_ERROR(ReportValue(display_received_cookies, UNI_L("PreferencesCookies"), UNI_L("DisplayReceivedCookies"), REPLACE, prefs_file));


	/**** Toolbars Page ****/

	// Toolbars
	BOOL using_standard_toolbar = TRUE;
	OpString filename;

	INT32 indx = g_setup_manager->GetIndexOfToolbarSetup();

	if(indx >= 0)
	{
		RETURN_IF_ERROR(g_setup_manager->GetSetupName(&filename, indx, OPTOOLBAR_SETUP));

		if(!uni_strstr(filename.CStr(), UNI_L("Standard")))
		{
			using_standard_toolbar = FALSE;
		}
	}

	RETURN_IF_ERROR(ReportValue(using_standard_toolbar, UNI_L("PreferencesToolbars"), UNI_L("UsingStandardToolbar"), REPLACE, prefs_file));

	// Menus
	BOOL using_standard_menu = TRUE;

	indx = g_setup_manager->GetIndexOfMenuSetup();

	if(indx >= 0)
	{
		RETURN_IF_ERROR(g_setup_manager->GetSetupName(&filename, indx, OPMENU_SETUP));

		if(!uni_strstr(filename.CStr(), UNI_L("Standard")))
		{
			using_standard_menu = FALSE;
		}
	}

	RETURN_IF_ERROR(ReportValue(using_standard_menu, UNI_L("PreferencesToolbars"), UNI_L("UsingStandardMenu"), REPLACE, prefs_file));


	/**** Shortcuts Page ****/

	// Keyboard
	BOOL using_standard_keyboard = TRUE;

	indx = g_setup_manager->GetIndexOfKeyboardSetup();

	if(indx >= 0)
	{
		RETURN_IF_ERROR(g_setup_manager->GetSetupName(&filename, indx, OPKEYBOARD_SETUP));

		if(!uni_strstr(filename.CStr(), UNI_L("Standard")))
		{
			using_standard_keyboard = FALSE;
		}
	}

	RETURN_IF_ERROR(ReportValue(using_standard_keyboard, UNI_L("PreferencesShortcuts"), UNI_L("UsingStandardKeyboard"), REPLACE, prefs_file));

	// Mouse
	BOOL using_standard_mouse = TRUE;

	indx = g_setup_manager->GetIndexOfMouseSetup();

	if(indx >= 0)
	{
		RETURN_IF_ERROR(g_setup_manager->GetSetupName(&filename, indx, OPMOUSE_SETUP));

		if(!uni_strstr(filename.CStr(), UNI_L("Standard")))
		{
			using_standard_mouse = FALSE;
		}
	}

	RETURN_IF_ERROR(ReportValue(using_standard_mouse, UNI_L("PreferencesShortcuts"), UNI_L("UsingStandardMouse"), REPLACE, prefs_file));

	// Mouse gestures
	INT32 enable_mouse_gestures = g_pccore->GetIntegerPref(PrefsCollectionCore::EnableGesture);
	RETURN_IF_ERROR(ReportValue(enable_mouse_gestures, UNI_L("PreferencesShortcuts"), UNI_L("EnableMouseGestures"), REPLACE, prefs_file));

	// Reverse flipping
	INT32 reverse_button_flipping = g_pccore->GetIntegerPref(PrefsCollectionCore::ReverseButtonFlipping);
	RETURN_IF_ERROR(ReportValue(reverse_button_flipping, UNI_L("PreferencesShortcuts"), UNI_L("ReverseButtonFlipping"), REPLACE, prefs_file));


	/**** History Page ****/

	// Direct history
	INT32 direct_history = g_pccore->GetIntegerPref(PrefsCollectionCore::MaxDirectHistory);
	RETURN_IF_ERROR(ReportValue(direct_history, UNI_L("PreferencesHistory"), UNI_L("MaxDirectHistory"), REPLACE, prefs_file));

	// Global history
	INT32 global_history = g_pccore->GetIntegerPref(PrefsCollectionCore::MaxGlobalHistory);
	RETURN_IF_ERROR(ReportValue(global_history, UNI_L("PreferencesHistory"), UNI_L("MaxGlobalHistory"), REPLACE, prefs_file));

	// Memory cache
	INT32 ram_cache_size = g_pccore->GetIntegerPref(PrefsCollectionCore::DocumentCacheSize);
	RETURN_IF_ERROR(ReportValue(ram_cache_size, UNI_L("PreferencesHistory"), UNI_L("MemoryCache"), REPLACE, prefs_file));

	// Automatic memory cache
	INT32 automatic_ram_cache = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticRamCache);
	RETURN_IF_ERROR(ReportValue(automatic_ram_cache, UNI_L("PreferencesHistory"), UNI_L("AutomaticMemoryCache"), REPLACE, prefs_file));

	// Disk cache
	INT32 disk_cache_size = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize);
	RETURN_IF_ERROR(ReportValue(disk_cache_size, UNI_L("PreferencesHistory"), UNI_L("DiskCache"), REPLACE, prefs_file));

	// Empty cache on exit
	INT32 empty_cache_on_exit = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EmptyCacheOnExit);
	RETURN_IF_ERROR(ReportValue(empty_cache_on_exit, UNI_L("PreferencesHistory"), UNI_L("EmptyCacheOnExit"), REPLACE, prefs_file));

	// Check documents
	INT32 doc_expiry = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DocExpiry);
	RETURN_IF_ERROR(ReportValue(doc_expiry, UNI_L("PreferencesHistory"), UNI_L("DocumentExpiry"), REPLACE, prefs_file));

	// Check images
	INT32 figs_expiry = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::FigsExpiry);
	RETURN_IF_ERROR(ReportValue(figs_expiry, UNI_L("PreferencesHistory"), UNI_L("ImageExpiry"), REPLACE, prefs_file));

	// Spellcheck
	INT32 spellcheck_enabled = g_pccore->GetIntegerPref(PrefsCollectionCore::SpellcheckEnabledByDefault);
	RETURN_IF_ERROR(ReportValue(spellcheck_enabled, UNI_L("SpellCheck"), UNI_L("SpellcheckEnabled"), REPLACE, prefs_file));

	/**** User JS ***/

	INT32 user_js_enabled = g_pcjs->GetIntegerPref(PrefsCollectionJS::UserJSEnabled);
	RETURN_IF_ERROR(ReportValue(user_js_enabled, UNI_L("UserJS"), UNI_L("Enabled"), REPLACE, prefs_file));

	OpString userjs_files;
	RETURN_IF_LEAVE(g_pcjs->GetStringPrefL(PrefsCollectionJS::UserJSFiles, userjs_files));
	RETURN_IF_ERROR(ReportValue(!userjs_files.IsEmpty(), UNI_L("UserJS"), UNI_L("HasFiles"), REPLACE, prefs_file));

	INT32 always_load_js = g_pcjs->GetIntegerPref(PrefsCollectionJS::UserJSAlwaysLoad);
	RETURN_IF_ERROR(ReportValue(always_load_js, UNI_L("UserJS"), UNI_L("AlwaysLoad"), REPLACE, prefs_file));

	INT32 user_js_enabled_https = g_pcjs->GetIntegerPref(PrefsCollectionJS::UserJSEnabledHTTPS);
	RETURN_IF_ERROR(ReportValue(user_js_enabled_https, UNI_L("UserJS"), UNI_L("EnabledHTTPS"), REPLACE, prefs_file));

	return OpStatus::OK;
}

// ---------------------------------------------------------------------------------

OP_STATUS WandReport::GenerateReport(PrefsFile *prefs_file)
{
	INT32 wand_enabled = g_pccore->GetIntegerPref(PrefsCollectionCore::EnableWand);
	RETURN_IF_ERROR(ReportValue(wand_enabled, UNI_L("Wand"), UNI_L("Enabled"), REPLACE, prefs_file));

	BOOL entered_wand_info = HasEnteredWandInformation();
	RETURN_IF_ERROR(ReportValue(entered_wand_info, UNI_L("Wand"), UNI_L("EnteredWandInformation"), REPLACE, prefs_file));

	WandLogin* login = NULL;
	int noWandLogins;

	for(noWandLogins=0; (login = g_wand_manager->GetWandLogin(noWandLogins)) != NULL; noWandLogins++) {};

	RETURN_IF_ERROR(ReportValue(noWandLogins, UNI_L("Wand"), UNI_L("NumberOfLogins"), ADD, prefs_file));

	WandPage* page = NULL;
	int noWandPages;

	for(noWandPages=0; (page = g_wand_manager->GetWandPage(noWandPages)) != NULL; noWandPages++) {};

	RETURN_IF_ERROR(ReportValue(noWandPages, UNI_L("Wand"), UNI_L("NumberOfPages"), REPLACE, prefs_file));

	return OpStatus::OK;
}

BOOL WandReport::HasEnteredWandInformation()
{
	if(g_pccore->GetStringPref(PrefsCollectionCore::Firstname, UNI_L("Firstname_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::Surname, UNI_L("Lastname_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::Address, UNI_L("Address_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::City, UNI_L("City_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::State, UNI_L("Region_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::Zip, UNI_L("Postal_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::Country, UNI_L("Country_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::Telephone, UNI_L("Telephone_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::Telefax, UNI_L("Mobile_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::EMail, UNI_L("Email_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::Home, UNI_L("Homepage_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::Special1, UNI_L("Special1_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::Special2, UNI_L("Special2_edit")).HasContent() ||
		g_pccore->GetStringPref(PrefsCollectionCore::Special3, UNI_L("Special3_edit")).HasContent())
	{
		return TRUE;
	}
	return FALSE;
}

// ---------------------------------------------------------------------------------

OP_STATUS SkinReport::GenerateReport(PrefsFile *prefs_file)
{
	if(g_skin_manager->GetCurrentSkin())
	{
		PrefsFile *ini_file = g_skin_manager->GetCurrentSkin()->GetPrefsFile();

		OpStringC skin_name;
		INT32 skin_type = -1;

		TRAPD(err, skin_name = ini_file->ReadStringL(UNI_L("Info"), UNI_L("Name")));

		if(err == OpStatus::OK)
		{
			if(uni_strcmp(skin_name.CStr(), UNI_L("Opera Standard Skin")) == 0)
			{
				skin_type = 0;
			}
			else if(uni_strcmp(skin_name.CStr(), UNI_L("Windows Native Skin")) == 0)
			{
				skin_type = 1;
			}
			else if(uni_strcmp(skin_name.CStr(), UNI_L("Native MacOS skin")) == 0)
			{
				skin_type = 2;
			}
			else if(uni_strcmp(skin_name.CStr(), UNI_L("Opera Unix Skin")) == 0)
			{
				skin_type = 4;
			}
			else
			{
				skin_type = 3;
			}
			RETURN_IF_ERROR(ReportValue(skin_type, UNI_L("Skin"), UNI_L("SkinType"), REPLACE, prefs_file));
			RETURN_IF_ERROR(ReportValue(skin_name.CStr(), UNI_L("Skin"), UNI_L("Name"), prefs_file));

			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

// ---------------------------------------------------------------------------------

OP_STATUS MailReport::GenerateReport(PrefsFile *prefs_file)
{
	// Check if M2 is available
	if (!g_application->ShowM2())
		return OpStatus::OK;

	INT32 it = -1;
	Index *indx;
	INT32 search_count = 0;

	if(g_m2_engine)
	{
		while((indx = g_m2_engine->GetIndexRange(IndexTypes::FIRST_SEARCH, IndexTypes::LAST_SEARCH, it)) != NULL)
		{
			search_count++;
		}
	}

	RETURN_IF_ERROR(ReportValue(search_count, UNI_L("Mail"), UNI_L("NumberOfSearches"), REPLACE, prefs_file));

	it = -1;
	INT32 folder_count = 0;

	if(g_m2_engine)
	{
		while((indx = g_m2_engine->GetIndexRange(IndexTypes::FIRST_FOLDER, IndexTypes::LAST_FOLDER, it)) != NULL)
		{
			folder_count++;
		}
	}

	RETURN_IF_ERROR(ReportValue(folder_count, UNI_L("Mail"), UNI_L("NumberOfFilters"), REPLACE, prefs_file));

	INT32 received_count = 0;
	if(g_m2_engine && g_m2_engine->GetIndexById(IndexTypes::RECEIVED))
	{
		received_count = g_m2_engine->GetIndexById(IndexTypes::RECEIVED)->MessageCount();
	}

	RETURN_IF_ERROR(ReportValue(received_count, UNI_L("Mail"), UNI_L("NumberOfMessagesTotal"), REPLACE, prefs_file));

	INT32 unread_count = 0;
	if(g_m2_engine)
	{
		unread_count = g_m2_engine->GetUnreadCount();
	}

	RETURN_IF_ERROR(ReportValue(unread_count, UNI_L("Mail"), UNI_L("NumberOfMessagesUnread"), REPLACE, prefs_file));

	INT32 accounts_count = 0;
	INT32 known_providers_count = 0;
	if(g_m2_engine && g_m2_engine->GetAccountManager())
	{
		known_providers_count = g_m2_engine->GetAccountManager()->GetNumberOfKnownAccountsProviders();

		UINT16 count = g_m2_engine->GetAccountManager()->GetAccountCount();
		for (UINT16 i = 0; i < count; i++)
		{
			Account* account = g_m2_engine->GetAccountManager()->GetAccountByIndex(i);
			OP_ASSERT(account);
			if (account->GetIncomingProtocol() > AccountTypes::_FIRST_MAIL && account->GetIncomingProtocol() < AccountTypes::_LAST_MAIL)
				++accounts_count;
		}
	}

	RETURN_IF_ERROR(ReportValue(known_providers_count, UNI_L("Mail"), UNI_L("NumberOfKnownProviders"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(accounts_count - known_providers_count, UNI_L("Mail"), UNI_L("NumberOfUnknownProviders"), REPLACE, prefs_file));

	RETURN_IF_ERROR(ReportValue(m_sent_messages_count, UNI_L("Mail"), UNI_L("NumberOfSentMessages"), ADD, prefs_file));
	m_sent_messages_count = 0;

	return OpStatus::OK;
}

// ---------------------------------------------------------------------------------

OP_STATUS FeedsReport::GenerateReport(PrefsFile *prefs_file)
{
	INT32 it = -1;
	Index *indx;
	INT32 noRSSFeeds = 0;

	if (!g_application->ShowM2())
		return OpStatus::OK;

	if(g_m2_engine)
	{
		while((indx = g_m2_engine->GetIndexRange(IndexTypes::FIRST_NEWSFEED, IndexTypes::LAST_NEWSFEED, it)) != NULL)
		{
			noRSSFeeds++;
		}
	}

	RETURN_IF_ERROR(ReportValue(noRSSFeeds, UNI_L("Feeds"), UNI_L("NumberOfFeeds"), REPLACE, prefs_file));

	return OpStatus::OK;
}

// ---------------------------------------------------------------------------------

OP_STATUS HistoryReport::GenerateReport(PrefsFile *prefs_file)
{
	INT32 nHistoryItems = 0;

	INT32 nItems = g_globalHistory->GetCount();

	for(INT32 i=0; i<nItems; i++)
	{
	    HistoryPage *item = 0;

		item = g_globalHistory->GetItemAtPosition(i);

		if(item)
		{
			time_t accessed = item->GetAccessed();

			time_t now     = g_timecache->CurrentTime();
			time_t elapsed = now - accessed;

			time_t today_sec      = g_globalHistory->SecondsSinceMidnight();
			time_t yesterday_sec  = today_sec + 86400;
			time_t last_week_sec  = yesterday_sec + 604800;

			if(elapsed < last_week_sec)
			{
				nHistoryItems++;
			}
		}
	}

	RETURN_IF_ERROR(ReportValue(nHistoryItems, UNI_L("History"), UNI_L("PagesVisitedLastWeek"), REPLACE, prefs_file));

	return OpStatus::OK;
}

// ---------------------------------------------------------------------------------

WorkspaceReport::WorkspaceReport()
: m_tabs(0),
  m_min_tabs(0),
  m_max_tabs(0),
  m_windows(0),
  m_min_windows(0),
  m_max_windows(0),
  m_document_windows(0),
  m_document_windows_speeddial(0),
  m_document_windows_fit_to_width(0),
  m_document_windows_show_images(0),
  m_document_windows_cached_images(0),
  m_document_windows_no_images(0),
  m_time_sum(0.0),
  m_average_tabs(0.0),
  m_visited_pages(0)
{
	m_time_start = g_op_time_info->GetRuntimeMS();
	m_time_last = m_time_start;
}

WorkspaceReport::~WorkspaceReport()
{
	UINT32 i;

	for(i=0; i<m_workspaces.GetCount(); i++)
	{
		OpWorkspace *workspace = m_workspaces.Get(i);
		if(workspace)
		{
			workspace->RemoveListener(this);
		}
	}

	for(i=0; i<m_desktop_windows.GetCount(); i++)
	{
		DesktopWindow *window = m_desktop_windows.Get(i);
		if(window)
		{
			window->RemoveListener(this);
		}
	}

}

OP_STATUS WorkspaceReport::GenerateReport(PrefsFile *prefs_file)
{
	double timestamp = g_op_time_info->GetRuntimeMS();
	double run_time = timestamp - m_time_start;
	m_average_tabs = m_average_tabs / run_time;

	RETURN_IF_ERROR(ReportValue(m_min_tabs, UNI_L("Workspace"), UNI_L("MinNumberOfTabs"), MINIMUM, prefs_file));
	RETURN_IF_ERROR(ReportValue(m_max_tabs, UNI_L("Workspace"), UNI_L("MaxNumberOfTabs"), MAXIMUM, prefs_file));
	RETURN_IF_ERROR(ReportValue(m_tabs, UNI_L("Workspace"), UNI_L("NumberOfTabsLastTime"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(m_min_windows, UNI_L("Workspace"), UNI_L("MinNumberOfWindows"), MINIMUM, prefs_file));
	RETURN_IF_ERROR(ReportValue(m_max_windows, UNI_L("Workspace"), UNI_L("MaxNumberOfWindows"), MAXIMUM, prefs_file));
	RETURN_IF_ERROR(ReportValue(INT32(m_average_tabs + 0.5), UNI_L("Workspace"), UNI_L("AverageNumberOfTabs"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(INT32(run_time/1000), UNI_L("Workspace"), UNI_L("BrowsingTime"), ADD, prefs_file));

	RETURN_IF_ERROR(ReportValue(m_document_windows, UNI_L("Workspace"), UNI_L("NumberOfDocumentWindows"), ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(m_document_windows_speeddial, UNI_L("Workspace"), UNI_L("NumberOfDocumentWindowsSpeedDial"), ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(m_document_windows_fit_to_width, UNI_L("Workspace"), UNI_L("NumberOfDocumentWindowsFitToWidth"), ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(m_document_windows_show_images, UNI_L("Workspace"), UNI_L("NumberOfDocumentWindowsShowImages"), ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(m_document_windows_cached_images, UNI_L("Workspace"), UNI_L("NumberOfDocumentWindowsCachedImages"), ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(m_document_windows_no_images, UNI_L("Workspace"), UNI_L("NumberOfDocumentWindowsNoImages"), ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(m_visited_pages, UNI_L("Workspace"), UNI_L("NumberOfVisitedPages"), ADD, prefs_file));

	INT32 has_menu_bar = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu);
	RETURN_IF_ERROR(ReportValue(has_menu_bar, UNI_L("Workspace"), UNI_L("MenuBarEnabled"), REPLACE, prefs_file));

	//tab thumbnails
	int thumbnails_enabled = 0;
	OpVector<DesktopWindow> desktop_windows;
	RETURN_IF_ERROR(g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_BROWSER, desktop_windows));
	for(int k = desktop_windows.GetCount() - 1; k >= 0; k--)
	{
		DesktopWindow *dw = desktop_windows.Get(k);
		if (static_cast<BrowserDesktopWindow*>(dw)->HasThumbnailsVisible())
			thumbnails_enabled++;
	}
	RETURN_IF_ERROR(ReportValue(thumbnails_enabled, UNI_L("Workspace"), UNI_L("NumberOfWindowsWithTabThumbnails"), REPLACE, prefs_file));

#ifdef _UNIX_DESKTOP_
	//desktop environment !
	RETURN_IF_ERROR(ReportValue(DesktopEnvironment::GetInstance().GetToolkitEnvironment() == DesktopEnvironment::ENVIRONMENT_XFCE, UNI_L("Workspace"), UNI_L("DesktopEnvIsXFCE"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(DesktopEnvironment::GetInstance().GetToolkitEnvironment() == DesktopEnvironment::ENVIRONMENT_GNOME, UNI_L("Workspace"), UNI_L("DesktopEnvIsGNOME"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(DesktopEnvironment::GetInstance().GetToolkitEnvironment() == DesktopEnvironment::ENVIRONMENT_KDE, UNI_L("Workspace"), UNI_L("DesktopEnvIsKDE"), REPLACE, prefs_file));
#endif

	m_document_windows = 0;
	m_document_windows_speeddial = 0;
	m_document_windows_fit_to_width = 0;
	m_document_windows_show_images = 0;
	m_document_windows_cached_images = 0;
	m_document_windows_no_images = 0;
	m_visited_pages = 0;
	m_time_start = g_op_time_info->GetRuntimeMS();

	return OpStatus::OK;
}

void WorkspaceReport::OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	double timestamp = g_op_time_info->GetRuntimeMS();
	double delta = timestamp - m_time_last;
	m_time_last = timestamp;
	m_average_tabs += m_tabs * delta;

	m_tabs++;
	m_desktop_windows.Add(desktop_window);

	if(!g_application->IsBrowserStarted())
	{
		m_min_tabs++;
	}

	if(m_tabs > m_max_tabs)
	{
		m_max_tabs = m_tabs;
	}

	if(desktop_window->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
	{
		m_document_windows++;
		DocumentDesktopWindow* doc = static_cast<DocumentDesktopWindow*>(desktop_window);
		doc->AddDocumentWindowListener(this);
	}
}

void WorkspaceReport::OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	double timestamp = g_op_time_info->GetRuntimeMS();
	double delta = timestamp - m_time_last;
	m_time_last = timestamp;
	m_average_tabs += m_tabs * delta;

	m_desktop_windows.RemoveByItem(desktop_window);

	if(!g_application->IsExiting())
	{
		m_tabs--;

		if(m_tabs < m_min_tabs)
		{
			m_min_tabs = m_tabs;
		}
	}

	if(desktop_window->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
	{
		DocumentDesktopWindow* doc = static_cast<DocumentDesktopWindow*>(desktop_window);
		doc->RemoveDocumentWindowListener(this);
		OpWindowCommander* win_comm = doc->GetWindowCommander();
		if(win_comm)
		{
			OpWindowCommander::LayoutMode mode = win_comm->GetLayoutMode();
			if(mode == OpWindowCommander::ERA)
			{
				m_document_windows_fit_to_width++;
			}

			switch (win_comm->GetImageMode())
			{
			case OpDocumentListener::ALL_IMAGES:
				m_document_windows_show_images++;
				break;

			case OpDocumentListener::LOADED_IMAGES:
				m_document_windows_cached_images++;
				break;

			case OpDocumentListener::NO_IMAGES:
				m_document_windows_no_images++;
				break;
			}
		}
	}
}

void WorkspaceReport::OnSpeedDialViewCreated(DocumentDesktopWindow* document_desktop_window)
{
	if (document_desktop_window->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
	{
		m_document_windows--;
		m_document_windows_speeddial++;
	}
}

void WorkspaceReport::OnWorkspaceAdded(OpWorkspace* workspace)
{
	m_windows++;
	m_workspaces.Add(workspace);

	if(!g_application->IsBrowserStarted())
	{
		m_min_windows++;
	}

	if(m_windows > m_max_windows)
	{
		m_max_windows = m_windows;
	}

}

void WorkspaceReport::OnWorkspaceDeleted(OpWorkspace* workspace)
{
	m_workspaces.RemoveByItem(workspace);

	if(!g_application->IsExiting())
	{
		m_windows--;

		if(m_windows < m_min_windows)
		{
			m_min_windows = m_windows;
		}
	}
}

void WorkspaceReport::OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url)
{
	if(url && m_last_url.CompareI(url))
	{
		m_visited_pages++;
		m_last_url.Set(url);
	}
}

// ---------------------------------------------------------------------------------

ActionReport::ActionReport()
{
}

ActionReport::~ActionReport()
{
	m_actions.DeleteAll();
}

OP_STATUS ActionReport::GenerateReport(PrefsFile *prefs_file)
{
	INT32HashIterator<INT32> it(m_actions);

	for(; it; it++)
	{
		INT32 key = it.GetKey();
		INT32* data = (INT32*)it.GetData();

		if(data)
		{
			const char* action_string = OpInputAction::GetStringFromAction((OpInputAction::Action)key);
			OpString action_str;
			action_str.Set(action_string);
			RETURN_IF_ERROR(ReportValue(*data, UNI_L("Actions"), action_str.CStr(), ADD, prefs_file));
		}

	}
	return OpStatus::OK;
}

void ActionReport::ActionFilter(OpInputAction *action)
{
	if(!action)
	{
		return;
	}

	switch(action->GetAction())
	{
		case OpInputAction::ACTION_FORWARD:
		case OpInputAction::ACTION_FAST_FORWARD:
		case OpInputAction::ACTION_BACK:
		case OpInputAction::ACTION_REWIND:
		case OpInputAction::ACTION_PRINT_DOCUMENT:
		{
			if(action->GetAction() < OpInputAction::LAST_ACTION)
			{
				INT32 *data = 0;
				if(OpStatus::IsSuccess(m_actions.GetData(action->GetAction(), &data)))
				{
					if(data)
					{
						(*data)++;
					}
				}
				else
				{
					INT32* data = OP_NEW(INT32,());
					if(data)
					{
						*data = 1;
						m_actions.Add(action->GetAction(), data);
					}
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------------

BittorrentReport::BittorrentReport()
: m_downloads(0)
{
}

BittorrentReport::~BittorrentReport()
{
}

OP_STATUS BittorrentReport::GenerateReport(PrefsFile *prefs_file)
{
	RETURN_IF_ERROR(ReportValue(m_downloads, UNI_L("Bittorrent"), UNI_L("NumberOfDownloads"), ADD, prefs_file));

	return OpStatus::OK;
}

void BittorrentReport::OnOk(Dialog* dialog, UINT32 result)
{
	m_downloads++;
}

// ---------------------------------------------------------------------------------


OP_STATUS AppearanceReport::GenerateReport(PrefsFile *prefs_file)
{
	INT32 pagebar_alignment = g_pcui->GetIntegerPref(PrefsCollectionUI::PagebarAlignment);
	RETURN_IF_ERROR(ReportValue(pagebar_alignment, UNI_L("Appearance"), UNI_L("PagebarAlignment"), REPLACE, prefs_file));

	INT32 statusbar_alignment = g_pcui->GetIntegerPref(PrefsCollectionUI::StatusbarAlignment);
	RETURN_IF_ERROR(ReportValue(statusbar_alignment, UNI_L("Appearance"), UNI_L("StatusbarAlignment"), REPLACE, prefs_file));

	INT32 browserbar_alignment = g_pcui->GetIntegerPref(PrefsCollectionUI::MainbarAlignment);
	RETURN_IF_ERROR(ReportValue(browserbar_alignment, UNI_L("Appearance"), UNI_L("BrowserbarAlignment"), REPLACE, prefs_file));

	INT32 personalbar_alignment = g_pcui->GetIntegerPref(PrefsCollectionUI::PersonalbarAlignment);
	RETURN_IF_ERROR(ReportValue(personalbar_alignment, UNI_L("Appearance"), UNI_L("PersonalbarAlignment"), REPLACE, prefs_file));

	INT32 site_navigation_bar_alignment = g_pcui->GetIntegerPref(PrefsCollectionUI::NavigationbarAlignment);
	RETURN_IF_ERROR(ReportValue(site_navigation_bar_alignment, UNI_L("Appearance"), UNI_L("SiteNavigationBarAlignment"), REPLACE, prefs_file));

	INT32 documentbar_alignment = g_pcui->GetIntegerPref(PrefsCollectionUI::AddressbarAlignment);
	RETURN_IF_ERROR(ReportValue(documentbar_alignment, UNI_L("Appearance"), UNI_L("DocumentBarAlignment"), REPLACE, prefs_file));

	INT32 w = g_pcui->GetIntegerPref(PrefsCollectionUI::AddressSearchDropDownWeightedWidth);
	RETURN_IF_ERROR(ReportValue(w, UNI_L("Appearance"), UNI_L("AddressSearchDropDownWeightedWidth"), REPLACE, prefs_file));

	PrefsFile* setup = g_setup_manager->GetSetupFile(OPTOOLBAR_SETUP);
	if(setup)
	{
		INT32 document_view_bar_alignment = -1;
		TRAPD(err, document_view_bar_alignment = setup->ReadIntL(UNI_L("Document View Toolbar.alignment"), UNI_L("Alignment"), -1));
		OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: how should we handle failure of the above ? RETURN_IF_LEAVE ?
		RETURN_IF_ERROR(ReportValue(document_view_bar_alignment, UNI_L("Appearance"), UNI_L("DocumentViewBarAlignment"), REPLACE, prefs_file));
	}

	INT32 search_field_visible = 0;
	PrefsSection* section = g_setup_manager->GetSectionL("Document Toolbar.content", OPTOOLBAR_SETUP);
	if(section)
	{
		for (const PrefsEntry* entry = section->Entries(); entry; entry = (const PrefsEntry *) entry->Suc())
		{
			OpLineParser line(entry->Key());

			OpString8 item_type;
			RETURN_IF_ERROR(line.GetNextTokenWithoutTrailingDigits(item_type));
			if(item_type.CompareI("Multiresizesearch") == 0)
			{
				search_field_visible = 1;
				break;
			}
		}
		OP_DELETE(section);
		RETURN_IF_ERROR(ReportValue(search_field_visible, UNI_L("Appearance"), UNI_L("AddressSearchDropDownVisible"), REPLACE, prefs_file));
	}

	return OpStatus::OK;
}

// ---------------------------------------------------------------------------------

OP_STATUS GeneralReport::GenerateReport(PrefsFile *prefs_file)
{
#ifdef MSWIN
	OpFile profile_folder;
	RETURN_IF_ERROR(profile_folder.Construct(UNI_L(""), OPFILE_HOME_FOLDER));

# if _MSC_VER < 1400
	OpString8 filename;
	RETURN_IF_ERROR(filename.Set(profile_folder.GetFullPath()));

	struct _stat buf;
	int result = _stat(filename, &buf );
# else
	OpString filename;
	RETURN_IF_ERROR(filename.Set(profile_folder.GetFullPath()));

	struct _stat32 buf;
	int result = _wstat32(filename, &buf );
# endif

	if(result == 0)
	{
		RETURN_IF_ERROR(ReportValue(buf.st_ctime, UNI_L("General"), UNI_L("FirstInstall"), REPLACE, prefs_file));
	}
#endif

#ifdef AUTO_UPDATE_SUPPORT
	int previous_update_check = g_pcui->GetIntegerPref(PrefsCollectionUI::TimeOfLastUpdateCheck);
#else
	int next_update_check = g_pcui->GetIntegerPref(PrefsCollectionUI::CheckForNewOpera);
	int previous_update_check = next_update_check - 604800;
#endif
	int time_since_last_update = g_timecache->CurrentTime() - previous_update_check;

	if(previous_update_check > 0)
	{
		RETURN_IF_ERROR(ReportValue(time_since_last_update, UNI_L("General"), UNI_L("TimeSinceLastUpdateCheck"), REPLACE, prefs_file));
	}

	int time_since_last_report = g_timecache->CurrentTime() - g_usage_report_manager->GetLastReportUpload();
	RETURN_IF_ERROR(ReportValue(time_since_last_report, UNI_L("General"), UNI_L("TimeSinceLastReport"), REPLACE, prefs_file));

	// Screen resolution
	OpScreenProperties screen_properties;
	RETURN_IF_ERROR(g_op_screen_info->GetProperties(&screen_properties, NULL));

	RETURN_IF_ERROR(ReportValue(screen_properties.width, UNI_L("General"), UNI_L("ScreenWidth"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(screen_properties.height, UNI_L("General"), UNI_L("ScreenHeight"), REPLACE, prefs_file));

	// Saved sessions
	INT32 saved_sessions = g_session_manager->GetSessionCount();
	RETURN_IF_ERROR(ReportValue(saved_sessions, UNI_L("General"), UNI_L("NumberOfSavedSessions"), REPLACE, prefs_file));

	OperaVersion opera_version;
	UINT32 version_number_major = opera_version.GetMajor();
	RETURN_IF_ERROR(ReportValue(version_number_major, UNI_L("General"), UNI_L("VersionNumberMajor"), REPLACE, prefs_file));

	UINT32 version_number_minor = opera_version.GetMinor();
	RETURN_IF_ERROR(ReportValue(version_number_minor, UNI_L("General"), UNI_L("VersionNumberMinor"), REPLACE, prefs_file));

	UINT32 build_number = opera_version.GetBuild();
	RETURN_IF_ERROR(ReportValue(build_number, UNI_L("General"), UNI_L("BuildNumber"), REPLACE, prefs_file));

	OpString guid;
	g_desktop_account_manager->GetInstallID(guid);
	RETURN_IF_ERROR(ReportValue(guid.CStr(), UNI_L("General"), UNI_L("ComputerID"), prefs_file));

	return OpStatus::OK;
}

// ---------------------------------------------------------------------------------

OP_STATUS SpeedDialReport::GenerateReport(PrefsFile *prefs_file)
{
#ifdef SUPPORT_SPEED_DIAL
#ifdef CRYPTO_HASH_MD5_USE_CORE_IMPLEMENTATION

	// Make sure to delete the whole section, if not the old speeddials that are no
	// longer to be found in the default speed dial ini file, will still be sent to the server.
	TRAPD(err, prefs_file->DeleteSectionL(UNI_L("SpeedDial")));
	RETURN_IF_ERROR(err);

	int nSpeedDials = 0, bHasBackground = 0;
	SpeedDialManager *manager = g_speeddial_manager;

	if(manager)
	{
		nSpeedDials = manager->GetTotalCount();
		bHasBackground = manager->HasBackgroundImage();
	}

	RETURN_IF_ERROR(ReportValue(nSpeedDials, UNI_L("SpeedDial"), UNI_L("NumberOfSpeedDials"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(bHasBackground, UNI_L("SpeedDial"), UNI_L("HasBackground"), REPLACE, prefs_file));

	int speeddial_state = g_pcui->GetIntegerPref(PrefsCollectionUI::SpeedDialState);
	RETURN_IF_ERROR(ReportValue(speeddial_state, UNI_L("SpeedDial"), UNI_L("SpeedDialState"), REPLACE, prefs_file));
	
	OpString_list url_list;
	OpString_list title_list;
	
	// Make a list of what default speed dials the user has changed, 
	// and what default speed dials he has not changed.
	if (OpStatus::IsSuccess(manager->GetDefaultURLList(url_list, title_list)))
	{
		for (UINT32 i = 0; i < manager->GetTotalCount(); i++)
		{
			if (url_list.Item(i).CStr())
			{
				INT32 exists = manager->SpeedDialExists(url_list.Item(i)) ? 1 : 0;
				OpString8 url;
				OpString8 url_md5_hash8;
				OpString  url_md5_hash16;

				// The database that will receive the results do not really work with unicode,
				// hence we make a MD5 check sum out of the URL and send that instead.
				url.Set(url_list.Item(i));
				RETURN_IF_ERROR(StringUtils::CalculateMD5Checksum(url, url_md5_hash8));
				RETURN_IF_ERROR(url_md5_hash16.Set(url_md5_hash8.CStr()));

				RETURN_IF_ERROR(ReportValue(exists, UNI_L("SpeedDial"), url_md5_hash16, REPLACE, prefs_file));
			}
		}
	}
#endif // CRYPTO_HASH_MD5_USE_CORE_IMPLEMENTATION
#endif // SUPPORT_SPEED_DIAL

	return OpStatus::OK;
}

// ---------------------------------------------------------------------------------

const uni_char* SearchReport::s_access_point_strings[] =
{
	UNI_L("SearchFromSearchField"),
	UNI_L("SearchDefaultFromAddressfieldDropdown"),
	UNI_L("SearchHistoryFromAddressfieldDropdown"),
	UNI_L("SearchShortcutInAddressfield"),
	UNI_L("SearchKeywordsInAddressfield"),
	UNI_L("SearchSelectedText"),
	UNI_L("SearchSpeedDial"),
	UNI_L("SearchErrorPage"),
	UNI_L("SearchUnknown"),
	// suggestion block
	// ismailp: holes (NULLs) below refer to entries in SearchAccessPoint that
	// can't have suggestions. For instance, SearchSelectedText
	// can't have a suggestion, that is SearchSuggestedFromSelectedText
	// can't exist, hence it's NULL.
	UNI_L("SearchSuggestedFromSearchField"),
	UNI_L("SearchSuggestedFromDefaultFromAddressfieldDropdown"),
	NULL,
	UNI_L("SearchSuggestedFromShortcutInAddressfield"),
	NULL,
	NULL,
	UNI_L("SearchSuggestedFromSpeedDial"),
	UNI_L("SearchSuggestedFromErrorPage")
};

SearchReport::SearchReport()
: m_selected_from_dropdown(FALSE),
  m_default_search(FALSE),
  m_selected_default_from_dropdown(FALSE),
  m_is_suggestion(FALSE)
{
}

SearchReport::~SearchReport()
{
	for(int i=0; i<SearchLast; i++)
	{
		m_searches[i].DeleteAll();
	}
}

OP_STATUS SearchReport::GenerateReport(PrefsFile *prefs_file)
{
	for(int i=0; i<SearchLast; i++)
	{
		OpAutoPtr<OpHashIterator> it(m_searches[i].GetIterator());
		if(it.get())
		{
			OP_STATUS ret_val = it->First();
			while(OpStatus::IsSuccess(ret_val))
			{
				uni_char* key = (uni_char*)it->GetKey();
				INT32* data = (INT32*)it->GetData();
				SearchTemplate* search_template = g_searchEngineManager->GetByUniqueGUID(key);
				if(key && data && search_template)
				{
					OpString key;
					RETURN_IF_ERROR(key.Set(search_template->GetKey()));
					if(key.HasContent())
					{
						OpString provider;
						search_template->GetEngineName(provider);
						if(provider.HasContent())
						{
							int pos = provider.Find(UNI_L("!"));
							if(pos != KNotFound)
							{
								provider.Delete(pos, 1);
							}
							pos = provider.Find(UNI_L("."));
							if(pos != KNotFound)
							{
								provider.Delete(pos, 1);
							}

							OpString section;
							RETURN_IF_ERROR(section.Set(UNI_L("Searches")));
							RETURN_IF_ERROR(section.AppendFormat(UNI_L("%s"), provider.CStr()));
							RETURN_IF_ERROR(ReportValue(*data, section.CStr(), s_access_point_strings[i], ADD, prefs_file));
						}
					}
				}
				ret_val = it->Next();
			}
		}
		m_searches[i].DeleteAll(); 
	}
	return OpStatus::OK;
}

OP_STATUS SearchReport::AddSearch(SearchAccessPoint access_point, const OpStringC& search_guid)
{
	if(access_point == SearchUnknown)
	{
		if(m_selected_from_dropdown)
		{
			if(!m_selected_default_from_dropdown)
			{
				if(m_is_suggestion)
					access_point = SearchSuggestedFromShortcutInAddressfield;
				else
					access_point = SearchHistoryFromAddressfieldDropdown;
			}
			else
			{
				access_point = SearchDefaultFromAddressfieldDropdown;
			}
		}
		else
		{
			if(m_default_search)
			{
				access_point = SearchKeywordsInAddressfield;
			}
			else
			{
				access_point = SearchShortcutInAddressfield;
			}
		}
	}

	INT32 *data = NULL;
	if(OpStatus::IsSuccess(m_searches[access_point].GetData(search_guid.CStr(), &data)))
	{
		if(data)
		{
			(*data)++;
		}
	}
	else
	{
		data = OP_NEW(INT32,());
		uni_char* guid = OP_NEWA(uni_char, search_guid.Length() + 1);
		if(data && guid)
		{
			*data = 1;
			uni_strncpy(guid, search_guid.CStr(), search_guid.Length());
			guid[search_guid.Length()] = 0;
			RETURN_IF_ERROR(m_searches[access_point].Add(guid, data));
		}
	}

	m_selected_from_dropdown = FALSE;
	m_default_search = FALSE;
	m_selected_default_from_dropdown = FALSE;
	m_is_suggestion = FALSE;

	return OpStatus::OK;
}

// ---------------------------------------------------------------------------------

OP_STATUS UpgradeReport::GenerateReport(PrefsFile *prefs_file)
{
	int upgrade_count = g_pcui->GetIntegerPref(PrefsCollectionUI::UpgradeCount);
	OpString previous_version;
	RETURN_IF_ERROR(previous_version.Set(g_pcui->GetStringPref(PrefsCollectionUI::UpgradeFromVersion).CStr()));

	RETURN_IF_ERROR(ReportValue(upgrade_count, UNI_L("Upgrade"), UNI_L("UpgradeCount"), REPLACE, prefs_file));
	RETURN_IF_ERROR(ReportValue(previous_version.CStr(), UNI_L("Upgrade"), UNI_L("UpgradeFromVersion"), prefs_file));
	return OpStatus::OK;
}

// ---------------------------------------------------------------------------------

void ErrorReport::OnFileError(OpWindowCommander*, OpErrorListener::FileError f_errno)
{
	switch(f_errno)
	{
	case EC_FILE_REFUSED:					file_refused++;						break;
	case EC_FILE_NOT_FOUND:					file_not_found++; 					break;
	default: break;
	}
}

void ErrorReport::OnHttpError(OpWindowCommander* commander, HttpError error)
{
	switch(error)
	{
    case EC_HTTP_REQUEST_URI_TOO_LONG:       http_request_uri_too_long++;       break;
    case EC_HTTP_BAD_REQUEST:                http_bad_request++;                break;
    case EC_HTTP_NOT_FOUND:                  http_not_found++;                  break;
    case EC_HTTP_INTERNAL_SERVER_ERROR:      http_internal_server_error++;      break;
    case EC_HTTP_NO_CONTENT:                 http_no_content++;                 break;
    case EC_HTTP_RESET_CONTENT:              http_reset_content++;              break;
    case EC_HTTP_FORBIDDEN:                  http_forbidden++;                  break;
    case EC_HTTP_METHOD_NOT_ALLOWED:         http_method_not_allowed++;         break;
    case EC_HTTP_NOT_ACCEPTABLE:             http_not_acceptable++;             break;
    case EC_HTTP_REQUEST_TIMEOUT:            http_request_timeout++;            break;
    case EC_HTTP_CONFLICT:                   http_conflict++;                   break;
    case EC_HTTP_GONE:                       http_gone++;                       break;
    case EC_HTTP_LENGTH_REQUIRED:            http_length_required++;            break;
    case EC_HTTP_PRECONDITION_FAILED:        http_precondition_failed++;        break;
    case EC_HTTP_REQUEST_ENTITY_TOO_LARGE:   http_request_entity_too_large++;   break;
    case EC_HTTP_UNSUPPORTED_MEDIA_TYPE:     http_unsupported_media_type++;     break;
    case EC_HTTP_NOT_IMPLEMENTED:            http_not_implemented++;            break;
    case EC_HTTP_BAD_GATEWAY:                http_bad_gateway++;                break;
    case EC_HTTP_GATEWAY_TIMEOUT:            http_gateway_timeout++;            break;
    case EC_HTTP_HTTP_VERSION_NOT_SUPPORTED: http_http_version_not_supported++; break;
    case EC_HTTP_SERVICE_UNAVAILABLE:        http_service_unavailable++;        break;
	default: break;
	}
}

void ErrorReport::OnNetworkError(OpWindowCommander*, OpErrorListener::NetworkError n_errno)
{
	switch(n_errno)
	{
	case EC_NET_CONNECTION:					network_connection++;				break;
#ifdef WC_CAP_HAS_PROXY_CONNECT_ERROR_MSG
	case EC_NET_PROXY_CONNECT_FAILED:		network_proxy_connect_failed++;		break;
#endif // WC_CAP_HAS_PROXY_CONNECT_ERROR_MSG
	default: break;
	}
}

OP_STATUS ErrorReport::GenerateReport(PrefsFile *prefs_file)
{
	RETURN_IF_ERROR(ReportValue(file_refused				   , UNI_L("FileError"),	UNI_L("Refused"),					ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(file_not_found			   	   , UNI_L("FileError"),	UNI_L("NotFound"),					ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_request_uri_too_long      , UNI_L("HTTPError"),	UNI_L("RequestUriTooLong"),			ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_bad_request               , UNI_L("HTTPError"),	UNI_L("BadRequest"),				ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_not_found                 , UNI_L("HTTPError"),	UNI_L("NotFound"),					ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_internal_server_error     , UNI_L("HTTPError"),	UNI_L("InternalServerError"),		ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_no_content                , UNI_L("HTTPError"),	UNI_L("NoContent"),					ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_reset_content             , UNI_L("HTTPError"),	UNI_L("ResetContent"),				ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_forbidden                 , UNI_L("HTTPError"),	UNI_L("Forbidden"),					ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_method_not_allowed        , UNI_L("HTTPError"),	UNI_L("MethodNotAllowed"),			ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_not_acceptable            , UNI_L("HTTPError"),	UNI_L("NotAcceptable"),				ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_request_timeout           , UNI_L("HTTPError"),	UNI_L("RequestTimeout"),			ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_conflict                  , UNI_L("HTTPError"),	UNI_L("Conflict"),					ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_gone                      , UNI_L("HTTPError"),	UNI_L("Gone"),						ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_length_required		   , UNI_L("HTTPError"),	UNI_L("LengthRequired"),			ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_precondition_failed	   , UNI_L("HTTPError"),	UNI_L("PreconditionFailed"),		ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_request_entity_too_large  , UNI_L("HTTPError"),	UNI_L("RequestEntityTooLarge"),		ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_unsupported_media_type    , UNI_L("HTTPError"),	UNI_L("UnsupportedMediaType"),		ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_not_implemented           , UNI_L("HTTPError"),	UNI_L("NotImplemented"),			ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_bad_gateway               , UNI_L("HTTPError"),	UNI_L("BadGateway"),				ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_gateway_timeout           , UNI_L("HTTPError"),	UNI_L("GatewayTimeout"),			ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_http_version_not_supported, UNI_L("HTTPError"),	UNI_L("HttpVersionNotSupported"),	ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(http_service_unavailable       , UNI_L("HTTPError"),	UNI_L("ServiceUnavailable"),		ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(network_connection			   , UNI_L("NetworkError"), UNI_L("Connection"),				ADD, prefs_file));
#ifdef WC_CAP_HAS_PROXY_CONNECT_ERROR_MSG
	RETURN_IF_ERROR(ReportValue(network_proxy_connect_failed   , UNI_L("NetworkError"), UNI_L("ProxyConnectFailed"),		ADD, prefs_file));
#endif // WC_CAP_HAS_PROXY_CONNECT_ERROR_MSG
	return OpStatus::OK;
}

// ---------------------------------------------------------------------------------

OP_STATUS ContentSharingReport::GenerateReport(PrefsFile *prefs_file)
{
	RETURN_IF_ERROR(ReportValue(sharing_twitter, UNI_L("ContentSharing"), UNI_L("Twitter"), ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(sharing_facebook, UNI_L("ContentSharing"), UNI_L("Facebook"), ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(sharing_email, UNI_L("ContentSharing"), UNI_L("Email"), ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(sharing_chat, UNI_L("ContentSharing"), UNI_L("Chat"), ADD, prefs_file));
	RETURN_IF_ERROR(ReportValue(sharing_shinaweibo, UNI_L("ContentSharing"), UNI_L("ShinaWeibo"), ADD, prefs_file));

	return OpStatus::OK;
}


// ---------------------------------------------------------------------------------
#ifdef WEB_TURBO_MODE

OP_STATUS TurboReport::GenerateReport(PrefsFile *prefs_file)
{
    // Is turbo on or off
	INT32 using_turbo = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseWebTurbo);
	RETURN_IF_ERROR(ReportValue(using_turbo, UNI_L("Turbo"), UNI_L("UseWebTurbo"), REPLACE, prefs_file));
	
	// Number of pages loaded with turbo since last report
	INT32 pages_viewed = g_opera_turbo_manager->GiveTotalPageViews();
	RETURN_IF_ERROR(ReportValue(pages_viewed, UNI_L("Turbo"), UNI_L("WebTurboPagesViewed"), ADD, prefs_file));
	
	// Average speed without Turbo
	INT32 average_speed = g_opera_turbo_manager->GetAverageSpeed();
	RETURN_IF_ERROR(ReportValue(average_speed, UNI_L("Turbo"), UNI_L("AverageNetworkSpeed"), REPLACE, prefs_file));
	
	return OpStatus::OK;
}

#endif // WEB_TURBO_MODE

// ---------------------------------------------------------------------------------
#ifdef VEGA_USE_HW

OP_STATUS HWAReport::GenerateReport(PrefsFile *prefs_file)
{
	OP_STATUS device_status = OpStatus::ERR;
	VEGA3dDevice* saved_device = NULL;

	// Is HWA enabled?
	INT32 enabled_hwa = g_pccore->GetIntegerPref(PrefsCollectionCore::EnableHardwareAcceleration);
	ReportValue(enabled_hwa, UNI_L("HardwareAcceleration"), UNI_L("Enabled"), REPLACE, prefs_file);

	// If HWA is disabled we will have to create a temporary device to see what is available
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	if (!device || g_vegaGlobals.rasterBackend != LibvegaModule::BACKEND_HW3D)
	{
		// VEGAGlDevice requires g_vegaGlobals.vega3dDevice to point to the device, save and restore any previous value
		saved_device = device;
		device_status = VEGA3dDeviceFactory::Create(&g_vegaGlobals.vega3dDevice, VEGA3dDevice::For3D, NULL);
		if (OpStatus::IsError(device_status))
		{
			// If HWA is blocked we still need a device to get the graphics card info
			device_status = VEGA3dDeviceFactory::Create(&g_vegaGlobals.vega3dDevice, VEGA3dDevice::Force, NULL);
			if (OpStatus::IsSuccess(device_status))
			{
				device = g_vegaGlobals.vega3dDevice;
			}
		}
		else
		{
			device = g_vegaGlobals.vega3dDevice;
		}
		// Current Back-end is Software
		ReportValue(UNI_L("Software"), UNI_L("HardwareAcceleration"), UNI_L("CurrentBackend"), prefs_file);
	}
	else
	{
		// Current Back-end
		ReportValue(device->getDescription(), UNI_L("HardwareAcceleration"), UNI_L("CurrentBackend"), prefs_file);
	}

	if (device)
	{
		// Blocked
		ReportValue(OpStatus::IsError(device->CheckUseCase(VEGA3dDevice::For3D)), UNI_L("HardwareAcceleration"), UNI_L("Blocked"), REPLACE, prefs_file);

		// Back-end
		ReportValue(device->getDescription(), UNI_L("HardwareAcceleration"), UNI_L("Backend"), prefs_file);

		OpString value;
		OpAutoPtr<VEGABlocklistDevice::DataProvider> provider(device->CreateDataProvider());
		if (provider.get())
		{		
			// Vendor ID
			if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("vendor id"), value)))
				ReportValue(value.CStr(), UNI_L("HardwareAcceleration"), UNI_L("VendorId"), prefs_file);
			
			// Device ID
			value.Empty();
			if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("device id"), value)))
				ReportValue(value.CStr(), UNI_L("HardwareAcceleration"), UNI_L("DeviceId"), prefs_file);
			
			// Driver Version
			value.Empty();
			if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("driver version"), value)))
				ReportValue(value.CStr(), UNI_L("HardwareAcceleration"), UNI_L("DriverVersion"), prefs_file);
			
			// OpenGL vendor
			value.Empty();
			if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("OpenGL-vendor"), value)))
				ReportValue(value.CStr(), UNI_L("HardwareAcceleration"), UNI_L("OpenGLVendor"), prefs_file);
			
			// OpenGL Renderer
			value.Empty();
			if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("OpenGL-renderer"), value)))
				ReportValue(value.CStr(), UNI_L("HardwareAcceleration"), UNI_L("OpenGLRenderer"), prefs_file);
			
			// OpenGL Version
			value.Empty();
			if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("OpenGL-version"), value)))
				ReportValue(value.CStr(), UNI_L("HardwareAcceleration"), UNI_L("OpenGLVersion"), prefs_file);

			// Video Memory
			value.Empty();
			if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("Dedicated Video Memory"), value)))
				ReportValue(value.CStr(), UNI_L("HardwareAcceleration"), UNI_L("VideoMemory"), prefs_file);
		}
	}

	if (OpStatus::IsSuccess(device_status))
	{
		// Restore any previous device and clean up
		device->Destroy();
		OP_DELETE(device);
		g_opera->libvega_module.vega3dDevice = saved_device;
	}

	return OpStatus::OK;
}

#endif // VEGA_USE_HW

#endif // ENABLE_USAGE_REPORT
