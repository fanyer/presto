/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file CheckForUpdatesDialog.cpp
 *
 * Contains function definitions for the classes defined in the corresponding
 * header file.
 */

#include "core/pch.h"

#ifndef AUTO_UPDATE_SUPPORT

#include "CheckForUpgradeDialogs.h"

#include "adjunct/quick/Application.h"

#include "modules/dom/domenvironment.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/windowcommander/src/TransferManager.h"
#include "modules/url/url2.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	Class UpgradeAvailableDialog
**
***********************************************************************************/

UpgradeAvailableDialog::UpgradeAvailableDialog(OpString &newVersion, BOOL do_not_show_again_checkbox)
{
	m_newVersion.Set(newVersion);
	m_do_not_show_again_checkbox = do_not_show_again_checkbox;
}

UpgradeAvailableDialog::~UpgradeAvailableDialog()
{
}

void UpgradeAvailableDialog::OnInit()
{
	OpMultilineEdit* infoText = (OpMultilineEdit*)GetWidgetByName("New_upgrade_label");
	if (infoText)
	{
		infoText->SetLabelMode();
	}
	if (m_newVersion.HasContent())
	{
		OpString staticText, dynamicText;
		g_languageManager->GetString(Str::D_NEW_VERSION_AVAILABLE, staticText);
		dynamicText.Set(m_newVersion);
		if( staticText.CStr() )
		{
			OpString fullText;
			fullText.AppendFormat(staticText.CStr(), dynamicText.CStr());
			SetWidgetText("New_upgrade_label", fullText.CStr());
		}
	}
}

UINT32 UpgradeAvailableDialog::OnOk()
{
	// Then go to the download page.
	OpString downloadURL;
	downloadURL.Set("www.opera.com/download");
	g_application->OpenURL(downloadURL, NO, YES, NO);
	return 0;
}

void UpgradeAvailableDialog::OnClose(BOOL user_initiated)
{
	if (IsDoNotShowDialogAgainChecked())
	{
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowNewOperaDialog, FALSE));
	}

	Dialog::OnClose(user_initiated);
}

/***********************************************************************************
**
**	Class NoUpgradeAvailableDialog
**
***********************************************************************************/

NoUpgradeAvailableDialog::NoUpgradeAvailableDialog()
{
}


NoUpgradeAvailableDialog::~NoUpgradeAvailableDialog()
{
}


void NoUpgradeAvailableDialog::OnInit()
{
	OpMultilineEdit* infoText = (OpMultilineEdit*)GetWidgetByName("No_new_upgrade_label");
	if (infoText)
	{
		infoText->SetLabelMode();
	}
}

/***********************************************************************************
**
**	Class ErrorCheckingForNewUpgradeDialog
**
***********************************************************************************/

void ErrorCheckingForNewUpgradeDialog::OnInit()
{
	OpMultilineEdit* infoText = (OpMultilineEdit*)GetWidgetByName("Upgrade_check_error_message");
	if (infoText)
	{
		switch (m_type)
		{
			case ERROR_PARSING:
			{
				OpString errorMsg;
				g_languageManager->GetString(Str::D_NEW_UPGRADE_ERROR_PARSING, errorMsg);
				infoText->SetText(errorMsg.CStr());
				break;
			}
			case ERROR_TRANSFER:
			{
				OpString errorMsg;
				g_languageManager->GetString(Str::D_NEW_UPGRADE_ERROR_TRANSFER, errorMsg);
				infoText->SetText(errorMsg.CStr());
				break;
			}
			case ERROR_LANGUAGE_NOT_FOUND:
			{
				OpString errorMsg;
				g_languageManager->GetString(Str::D_NEW_UPGRADE_ERROR_LANGUAGE, errorMsg);
				infoText->SetText(errorMsg.CStr());
				break;
			}
		}
		infoText->SetLabelMode();
	}
}

/***********************************************************************************
**
**	Class NewUpdatesChecker
**
***********************************************************************************/

NewUpdatesChecker::NewUpdatesChecker(BOOL mustGiveFeedback)
	: m_browserJSDownloader(NULL)
	, m_transferItem(NULL)
	, m_canSafelyBeDeleted(TRUE)
	, m_mustGiveFeedback(mustGiveFeedback)
	, m_firstRun(FALSE)
{
}

NewUpdatesChecker::~NewUpdatesChecker()
{
	if (m_transferItem)
	{
		g_transferManager->ReleaseTransferItem(m_transferItem);
		m_transferItem = NULL;
	}
	if (m_browserJSDownloader)
	{
		OP_DELETE(m_browserJSDownloader);
	}
}

// This function must always return true if it initates TransferListener callbacks, and false if it doesen't.
BOOL NewUpdatesChecker::PerformNewUpdatesCheck()
{
	// If no date is stored for the next check, this is presumably the first run of Opera. Set the time
	// for the first check one week ahead.
	if ( g_pcui->GetIntegerPref(PrefsCollectionUI::CheckForNewOpera) == 1 )
	{
		m_firstRun = TRUE;
	}
	if ( (g_pcui->GetIntegerPref(PrefsCollectionUI::CheckForNewOpera) > 0) || m_mustGiveFeedback ) // This check is to ensure against wrong usage of this function.
	{
		int scheduled_check = g_pcui->GetIntegerPref(PrefsCollectionUI::CheckForNewOpera);
		int time_since_last_check = 0; 
		if (!m_firstRun) // Only calculate time since last if not first run, otherwise set to 0.
		{
			time_since_last_check = g_timecache->CurrentTime() - scheduled_check + 604800; // Set the time since the last check. If the user has been fiddeling with his ini files or system clock, this will be wrong.
		}
		OpString url_string;
		url_string.AppendFormat(UNI_L("http://xml.opera.com/update/?timesincelastcheck=%i"), time_since_last_check);
		//url_string.Set(UNI_L("file://localhost/d:/version.xml")); // For debugging, REMEMBER: revert to the correct URL after use!
		g_transferManager->GetTransferItem(&m_transferItem, url_string.CStr());
		if (m_transferItem)
		{
			URL* url = m_transferItem->GetURL();
			if (url)
			{
				// Initiate the download of the document pointed to by the URL, and forget about it.
				// The OnProgress function will deal with successful downloads and check whether a user
				// notification is required.
				m_transferItem->SetTransferListener(this);
				URL tmp;
				url->Load(g_main_message_handler, tmp, TRUE, FALSE);
				m_canSafelyBeDeleted = FALSE;
				return TRUE;
			}
		}
		else
		{
			OP_ASSERT(0);
		}
	}
	else
	{
		OP_ASSERT(0);
	}
	return FALSE;
}

void NewUpdatesChecker::OnProgress(OpTransferItem* transferItem, TransferStatus status)
{
	switch (status) // Switch on the various status messages.
	{
		case TRANSFER_ABORTED:
		{
			HandleTransferFailure();
			break;
		}
		case TRANSFER_DONE:
		{
			// This is the interesting case, the transfer has completed.
			URL* url = transferItem->GetURL();
			if (url)
			{
				OpAutoPtr<URL_DataDescriptor> url_data_desc(url->GetDescriptor(NULL, TRUE));
				HandleReceivedData(url_data_desc.get());
			}
			else
			{
				HandleTransferFailure();
			}
			break;
		}
		case TRANSFER_PROGRESS:
		{
			// This is the only case where new callbacks to this function must be expected.
			// To delete would cause an error.
			return;
		}
		case TRANSFER_FAILED:
		{
			HandleTransferFailure();
			break;
		}
	}
	g_transferManager->ReleaseTransferItem(m_transferItem);
	m_transferItem = NULL;
	m_canSafelyBeDeleted = TRUE;
}

BOOL NewUpdatesChecker::CanSafelyBeDeleted()
{
	return m_canSafelyBeDeleted;
}

void NewUpdatesChecker::HandleTransferFailure()
{
	if (m_mustGiveFeedback)
	{
		ErrorCheckingForNewUpgradeDialog* errorDialog = OP_NEW(ErrorCheckingForNewUpgradeDialog, (ErrorCheckingForNewUpgradeDialog::ERROR_TRANSFER));
		if (errorDialog)
			errorDialog->Init(g_application->GetActiveDesktopWindow());
	}
}

void NewUpdatesChecker::HandleReceivedData(URL_DataDescriptor* dd)
{
	if (!dd)
		return;

	// Extract the data from the data descriptor.
	OP_MEMORY_VAR unsigned long len;
	BOOL more = FALSE;
	TRAPD(err, len = dd->RetrieveDataL(more));
	if (OpStatus::IsError(err))
	{
		// If there is an error then handle it and return.
		HandleTransferFailure();
		return;
	}

	// Find the relevant version numbers, if the check is not user initated then try to use a stored version number.
	Version receivedVersion((uni_char*)dd->GetBuffer(), UNICODE_DOWNSIZE(len)); // Note: This won't work if the information grows beyond 64K... Might want to fix that some time... (mariusab 2005-03-09)
	Version localVersion(FALSE);

	const OpStringC newestseen = g_pcui->GetStringPref(PrefsCollectionUI::NewestSeenVersion);

	if (!m_mustGiveFeedback && newestseen.HasContent())
	{
		Version tmpVersion(newestseen.CStr());
		// If the newest seen version is lower than the current version, that is probably because an overinstall has been done. In that case the version number from
		// the program must be used, or else users will get a notification of the version they just installed, which would be bad...
		if (tmpVersion > localVersion)
		{
			localVersion.Set(newestseen.CStr());
		}
	}

	if (receivedVersion.Corrupt() || localVersion.Corrupt())
	{
		if (!receivedVersion.LocalLanguageFound()) // If the reason for the failure is that the language was not found, there is not much we can do...
		{
			CheckInOneWeek(localVersion);
			// This case is special. The version checking has failed, but the user still needs to get 
			// browser.js and ua.ini files downloaded if they have been modified on the server.
			// Check for User Agent spoof settings since the regular checking was successful
			if (receivedVersion.GetSpoofChange())
			{
				DownloadUASpoofList();
			}
#ifdef DOM_BROWSERJS_SUPPORT
			DownloadBrowserJSIfNeededOrRequired(receivedVersion.GetBrowserJSChange());
#endif // DOM_BROWSERJS_SUPPORT	
		}
		// Something has gone wrong when parsing the input. The versions can not be compared.
		if (m_mustGiveFeedback) {
			ErrorCheckingForNewUpgradeDialog::ErrorType errorType = ErrorCheckingForNewUpgradeDialog::ERROR_PARSING;
			if (!receivedVersion.LocalLanguageFound())
			{
				errorType = ErrorCheckingForNewUpgradeDialog::ERROR_LANGUAGE_NOT_FOUND;
			}
			ErrorCheckingForNewUpgradeDialog* errorDialog = OP_NEW(ErrorCheckingForNewUpgradeDialog, (errorType));
			if (errorDialog)
				errorDialog->Init(g_application->GetActiveDesktopWindow());
		}
	}
	else
	{
		// If the version numbers were parsed correctly, then schedule a new check in one week. Store the highest
		// version number as the latest seen version number.
		if (receivedVersion > localVersion)
		{
			BOOL show_dialog = m_mustGiveFeedback || 
				(!m_firstRun && g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNewOperaDialog));

			CheckInOneWeek(receivedVersion);
			// The user must be notified that a new version has arrived.
			if (show_dialog)
			{
				UpgradeAvailableDialog* upgradeDialog = OP_NEW(UpgradeAvailableDialog, (*receivedVersion.FullString(), !m_mustGiveFeedback));
				if (upgradeDialog)
					upgradeDialog->Init(g_application->GetActiveDesktopWindow());
			}
		}
		else
		{
			CheckInOneWeek(localVersion);

			if (m_mustGiveFeedback)
			{
				// Nothing to report, but the user must get confirmation that he is using the current version of Opera.
				NoUpgradeAvailableDialog* noUpgradeDialog = OP_NEW(NoUpgradeAvailableDialog, ());
				if (noUpgradeDialog)
					noUpgradeDialog->Init(g_application->GetActiveDesktopWindow());
			}
			else
			{
				// This is the silent case. Nothing to report, and the user should not be disturbed with a notification.
			}
		}

		// Check for User Agent spoof settings since the regular checking was successful
		if (m_mustGiveFeedback || receivedVersion.GetSpoofChange())
		{
			DownloadUASpoofList();
		}

#ifdef DOM_BROWSERJS_SUPPORT
			DownloadBrowserJSIfNeededOrRequired(receivedVersion.GetBrowserJSChange());
#endif // DOM_BROWSERJS_SUPPORT	
	}
}

void NewUpdatesChecker::CheckInOneWeek(Version &newestSeenVersion)
{
	if (g_pcui->GetIntegerPref(PrefsCollectionUI::CheckForNewOpera) > 0) // Make sure the new checking time is  written ONLY if the automatic checking is turned on.
	{
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::CheckForNewOpera, g_timecache->CurrentTime() + 604800));
	}
	if (!m_firstRun)
	{
		TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::NewestSeenVersion, newestSeenVersion.FullString()->CStr()));
	}
}

void NewUpdatesChecker::DownloadUASpoofList()
{
	const OpStringC spoof_list_url = g_pccore->GetStringPref(PrefsCollectionCore::PreferenceServer);

	if (spoof_list_url.IsEmpty())
	{
		TRAPD(err, g_pccore->WriteStringL(PrefsCollectionCore::PreferenceServer, UNI_L("http://xml.opera.com/spoof/")));
	}
	g_PrefsLoadManager->InitLoader(OpStringC(), OP_NEW(EndChecker, ()));
}

#ifdef DOM_BROWSERJS_SUPPORT
void NewUpdatesChecker::DownloadBrowserJSIfNeededOrRequired(BOOL needed)
{
	int BrowserJSSetting;
	BrowserJSSetting = g_pcjs->GetIntegerPref(PrefsCollectionJS::BrowserJSSetting);

	if ( (BrowserJSSetting != 0) && (needed || BrowserJSSetting == 1) ) // Only download browserjs if it is not explicitly turned off, and only when needed.
	{
		OpString remoteFileName;
		remoteFileName.Set(UNI_L("http://xml.opera.com/userjs/"));
		OpString localFileName;
		OpString tmp_storage;
		localFileName.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage));
		if (localFileName.Length() > 0)
		{
			localFileName.Append("browser.js");
			m_browserJSDownloader = OP_NEW(BrowserJSDownloader, ());
			if (m_browserJSDownloader)
				m_browserJSDownloader->DownloadBrowserJS(remoteFileName, localFileName);
		}
	}
}
#endif // DOM_BROWSERJS_SUPPORT

/***********************************************************************************
**
**	Class NewUpdatesChecker::Version
**
***********************************************************************************/

NewUpdatesChecker::Version::Version(BOOL blank)
{
	if (!blank)
		Set();
}

NewUpdatesChecker::Version::Version(const uni_char* versionString)
{
	Set(versionString);
}

NewUpdatesChecker::Version::Version(uni_char* XMLDocument, int length)
{
	Set(XMLDocument, length);
}

NewUpdatesChecker::Version::~Version()
{
}

OP_STATUS NewUpdatesChecker::Version::ParseAndStoreVersionString(const uni_char* versionString)
{
	ClearMembers();
	if (!versionString) // Null input makes for hard parsing! ;)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	unsigned int offset = 0;
	if (OpStatus::IsSuccess(ExtractAndAppendDigits(m_major, versionString, offset))) // Parse digits and make sure that the parse was successful.
	{
		// Make sure that the next character is a period
		if (versionString[offset] == '.')
		{
			offset++; // Set offset to point to next digit.
			if (OpStatus::IsSuccess(ExtractAndAppendDigits(m_minor, versionString, offset))) // Parse digits and make sure that the parse was successful.
			{
				// We have now successfully parsed a major and a minor version number. If the next char is not the string terminator, then
				// we must parse for beta.
				if (versionString[offset] == '\0')
				{
					m_beta = FALSE;
					return OpStatus::OK;
				}
				else if (OpStatus::IsSuccess(ReadAndStripBetaString(versionString, offset)))
				{
					// Beta was found without error. If at the end of the string, there is no beta number. Otherwise, parse that.
					m_beta = TRUE;
					if (versionString[offset] == '\0')
						return OpStatus::OK;

					if ( (OpStatus::IsSuccess(ExtractAndAppendDigits(m_betaNumber, versionString, offset))) && (versionString[offset] == '\0') )
						return OpStatus::OK;

					// Else failure finding end of line or parsing betanumber
				}
				else
				{
					// We want to allow version numbers with special characters in them, but just ignore what the content actually is.
					m_beta = FALSE;
					return OpStatus::OK;
				}
				// Else failure finding end of line or beta
			}
			// Else failure parsing minor
		}
		// Else failure finding period
	}
	// Else failure parsing major

	ClearMembers();
	return OpStatus::ERR_PARSING_FAILED;
}

OP_STATUS NewUpdatesChecker::Version::ExtractAndAppendDigits(OpString& target, const uni_char* source, unsigned int& offset)
{
	if (!source)
		return OpStatus::ERR_NULL_POINTER; // Some of the input references were NULL

	if (uni_strlen(source) <= offset) // offset is unsigned
	{
		return OpStatus::ERR_OUT_OF_RANGE; // The supplied offset makes no sense.
	}

	unsigned int start = offset;
	// Scan for digits
	while ( (source[offset] >= '0') && (source[offset] <= '9') )
	{
		offset++;
	}
	// We now have the start and end indices of what should be a string of digits.
	if (start >= offset) // No digits were found.
	{
		return OpStatus::ERR_PARSING_FAILED;
	}
	else // Digits were found, extract them and append them into the target.
	{
		target.Append(&source[start], offset - start);
	}
	return OpStatus::OK;
}

OP_STATUS NewUpdatesChecker::Version::ReadAndStripBetaString(const uni_char* source, unsigned int& offset)
{
	if (!source)
		return OpStatus::ERR_NULL_POINTER;

	unsigned int loc_offs = offset;

	if ( (source[loc_offs] | 0x20) == 'b' )
	{
		// A [B|b] is found, so we have a beta version.
		loc_offs++;
		// Strip away the rest of _eta if it exists.
		if ( (source[loc_offs] | 0x20) == 'e' )
		{
			loc_offs++;
			if ( (source[loc_offs] | 0x20) == 't' )
			{
				loc_offs++;
				if ( (source[loc_offs] | 0x20) == 'a' )
				{
					loc_offs++;
				}
			}
		}
		offset = loc_offs;
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

void NewUpdatesChecker::Version::ClearMembers()
{
	m_major.Empty();
	m_minor.Empty();
	m_betaNumber.Empty();
	m_versionString.Empty();
}

void NewUpdatesChecker::Version::Set()
{
	m_spoofChange = FALSE;
	m_browserJSChange = FALSE;
	m_tmpLanguageFound = FALSE;
	m_languageFound = FALSE;

	ParseAndStoreVersionString(VER_NUM_STR_UNI); // Will set m_beta and m_betaNumber FALSE and "" because VER_NUM_STR_UNI does not define beta properties. Must now check for real for beta...
#if defined(VER_BETA)
	m_beta = TRUE;
	m_betaNumber.Set(VER_BETA_STR);
#else
	m_beta = FALSE;
	m_betaNumber.Empty();
#endif // defined(VER_BETA)

}

void NewUpdatesChecker::Version::Set(const uni_char* versionString)
{
	m_spoofChange = FALSE;
	m_browserJSChange = FALSE;
	m_tmpLanguageFound = FALSE;
	m_languageFound = FALSE;

	ParseAndStoreVersionString(versionString);
}

void NewUpdatesChecker::Version::Set(uni_char* XMLDocument, int length)
{
	m_versionchecks = 0;
	m_idns          = 0;
	ClearMembers();
	m_spoofChange   = FALSE;
	m_browserJSChange = FALSE;
	m_tmpLanguageFound = FALSE;
	m_languageFound = FALSE;

	XMLParser *parser = NULL;
	RETURN_VOID_IF_ERROR(XMLParser::Make(parser, this, (MessageHandler*)NULL, this, URL()));

	XMLParser::Configuration xml_config;
	xml_config.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
	parser->SetConfiguration(xml_config);

	OP_STATUS err = parser->Parse(XMLDocument, length, FALSE);
	if (OpStatus::IsError(err))
	{
		// The parsing failed, make sure that the general corrupt state is used
		m_major.Empty();
		m_minor.Empty();
		m_languageFound = TRUE; // This is because if not, the language missing error message will be displayed, what we want is the generic parse error message..
		return;
	}

	m_languageFound = m_tmpLanguageFound;

	OP_DELETE(parser);

/*	XML_Parser parser = XML_ParserCreate(UNI_L("UTF-8"));

	if (!parser)
		return;

	XML_SetUserData(parser, this);
	XML_SetElementHandler(parser, sStartElementHandler, sEndElementHandler);
	XML_SetCharacterDataHandler(parser, sCharacterDataHandler);
	// The parsing callbacks take care of setting the values correctly.
	if (XML_Parse(parser, XMLDocument, length, TRUE))
	{
		// The parsing went well, but was the language found?
		m_languageFound = m_tmpLanguageFound;
	}
	else
	{
		// The parsing failed, make sure that the general corrupt state is used
		m_major.Empty();
		m_minor.Empty();
		m_languageFound = TRUE; // This is because if not, the language missing error message will be displayed, what we want is the generic parse error message..
	}
	XML_ParserFree(parser);*/
}

BOOL NewUpdatesChecker::Version::Corrupt()
{
	return ( m_major.IsEmpty() || m_minor.IsEmpty() );
}

/*virtual*/ XMLTokenHandler::Result
NewUpdatesChecker::Version::HandleToken(XMLToken &token)
{
	switch (token.GetType())
	{
	case XMLToken::TYPE_STag:
		StartElementHandler(token);
		break;

	case XMLToken::TYPE_ETag:
		EndElementHandler(token);
		break;

	case XMLToken::TYPE_Text:
		{
			uni_char *local_copy = NULL;
			const uni_char *text_data = token.GetLiteralSimpleValue();
			if (!text_data)
			{
				local_copy = token.GetLiteralAllocatedValue();
				text_data = local_copy;
			}

			CharacterDataHandler(text_data, token.GetLiteralLength());

			if (local_copy)
				OP_DELETEA(local_copy);
		}
		break;
	}

	return XMLTokenHandler::RESULT_OK;
}

/*virtual*/ void
NewUpdatesChecker::Version::Continue(XMLParser *parser)
{
	// this probably means that the parsing should be continued with more data
	// well, then this code needs redesigning:)
	OP_ASSERT(0);
}

/*virtual*/ void
NewUpdatesChecker::Version::Stopped(XMLParser *parser)
{
}

void NewUpdatesChecker::Version::StartElementHandler(const XMLToken &token)
{
	m_content_string.Empty();
	if (uni_strni_eq(token.GetName().GetLocalPart(), UNI_L("VERSIONCHECK"), token.GetName().GetLocalPartLength()))
	{
		m_versionchecks++;
		if (m_versionchecks == 1)
		{
			if (XMLToken::Attribute *spoof_attr = token.GetAttribute(UNI_L("spoof"), 5))
			{
				const uni_char* attr_val  = spoof_attr->GetValue();
				if ( (attr_val && attr_val[0] > '0') && (attr_val[0] < '9')
					&& (uni_atoi(attr_val) > g_pcui->GetIntegerPref(PrefsCollectionUI::CheckForNewOpera)) )
				{
					m_spoofChange = TRUE;
				}
			}

#ifdef DOM_BROWSERJS_SUPPORT
			if (XMLToken::Attribute *bspit_attr = token.GetAttribute(UNI_L("bspit"), 5))
			{
				const uni_char* time_of_last_bspit_change  = bspit_attr->GetValue();
				// Check the time of the last change of the signed user javascript file (browser.js).
				if ( (time_of_last_bspit_change && time_of_last_bspit_change[0] > '0') && (time_of_last_bspit_change[0] < '9') && (uni_atoi(time_of_last_bspit_change) > (g_pcui->GetIntegerPref(PrefsCollectionUI::CheckForNewOpera)) - 604800) )
				{
					m_browserJSChange = TRUE;
				}
			}
#endif // DOM_BROWSERJS_SUPPORT
		}
	}
	else if (uni_strni_eq(token.GetName().GetLocalPart(), UNI_L("LATESTVERSION"), token.GetName().GetLocalPartLength()))
	{
		// Store which language this tag gives version information for.
		if (XMLToken::Attribute *lang_attr = token.GetAttribute(UNI_L("lang"), 4))
		{
			m_currentLanguage.Set(lang_attr->GetValue(), lang_attr->GetValueLength());
			return;
		}
		// There was no lang attribute...
		m_currentLanguage.Empty();
	}
	else if  (uni_strni_eq(token.GetName().GetLocalPart(), UNI_L("IDN"), token.GetName().GetLocalPartLength()))
	{
		m_idns++;
	}
}

void NewUpdatesChecker::Version::EndElementHandler(const XMLToken &token)
{
	if (uni_strni_eq(token.GetName().GetLocalPart(), UNI_L("VERSIONCHECK"), token.GetName().GetLocalPartLength()))
	{
		m_versionchecks--;
	}
	else if (uni_strni_eq(token.GetName().GetLocalPart(), UNI_L("LATESTVERSION"), token.GetName().GetLocalPartLength()))
	{
		if ( (m_versionchecks > 0) &&
			 (m_currentLanguage.CompareI(g_languageManager->GetLanguage()) == 0) )
		{
			// This is the correct tag!
			 ParseAndStoreVersionString(m_content_string.CStr());
			 m_tmpLanguageFound = TRUE;
		}
	}
	else if (uni_strni_eq(token.GetName().GetLocalPart(), UNI_L("IDN"), token.GetName().GetLocalPartLength()))
	{
		if (m_idns == 1)
		{
			TRAPD(err, g_pcnet->WriteStringL(PrefsCollectionNetwork::IdnaWhiteList, m_content_string));
		}
		m_idns--;
	}
}

void NewUpdatesChecker::Version::CharacterDataHandler(const uni_char *s, int len)
{
	m_content_string.Append(s, len);
}

OpString* NewUpdatesChecker::Version::FullString()
{
	m_versionString.Empty();
	if (!Corrupt())
	{
		m_versionString.Append(m_major);
		m_versionString.Append(UNI_L("."));
		m_versionString.Append(m_minor);
		if (m_beta)
		{
			m_versionString.Append(UNI_L("Beta"));
			m_versionString.Append(m_betaNumber);
		}
	}
	return &m_versionString;
}

OpString* NewUpdatesChecker::Version::GetMajor()
{
	return &m_major;
}

OpString* NewUpdatesChecker::Version::GetMinor()
{
	return &m_minor;
}

BOOL NewUpdatesChecker::Version::GetBeta()
{
	return m_beta;
}

OpString* NewUpdatesChecker::Version::GetBetaNumber()
{
	return &m_betaNumber;
}

BOOL NewUpdatesChecker::Version::operator == (Version &other)
{
	return (m_major == *other.GetMajor() &&
		    m_minor == *other.GetMinor() &&
			m_beta == other.GetBeta() &&
			m_betaNumber == *other.GetBetaNumber());
}

BOOL NewUpdatesChecker::Version::operator != (Version &other)
{
	return ( !(*this == other) );
}

BOOL NewUpdatesChecker::Version::operator < (Version &other)
{
	int maj_len = m_major.Length();
	int oth_maj_len = other.GetMajor()->Length();

	// If the strings are of unequal length, the shortest string contains the lowest number.
	if (maj_len < oth_maj_len)
		return TRUE;
	else if (maj_len > oth_maj_len)
		return FALSE;

	// If the strings are of equal length, the string with the lowest content is also the lowest number.
	int maj_compare = m_major.CompareI(*other.GetMajor());
	if (maj_compare < 0)
		return TRUE;
	else if (maj_compare > 0)
		return FALSE;

	// major version is equal
	int min_compare = m_minor.CompareI(*other.GetMinor());
	if (min_compare < 0)
		return TRUE;
	else if (min_compare > 0)
		return FALSE;

	// version is identical, and this version is not in beta -> this version is not lower
	if (!m_beta)
		return FALSE;

	// this version is in beta, while the other is not -> this version is lower
	if (!other.GetBeta())
		return TRUE;

	// both versions are in beta -> return whether the beta number is smaller
	return (m_betaNumber.CompareI(*other.GetBetaNumber()) < 0);
}

BOOL NewUpdatesChecker::Version::operator <= (Version &other)
{
	return ( (*this == other) || (*this < other) );
}

BOOL NewUpdatesChecker::Version::operator > (Version &other)
{
	return ( !(*this <= other) );
}

BOOL NewUpdatesChecker::Version::operator >= (Version &other)
{
	return ( (*this == other) || (*this > other) );
}

/***********************************************************************************
**
**	Class NewUpdatesChecker::BrowserJSDownloader
**
***********************************************************************************/

NewUpdatesChecker::BrowserJSDownloader::BrowserJSDownloader()
{
}

NewUpdatesChecker::BrowserJSDownloader::~BrowserJSDownloader()
{
	if (m_transferItem) // : Find out if there is a risk that this might be garbage, making the release call dangerous.
	{
		g_transferManager->ReleaseTransferItem(m_transferItem);
		m_transferItem = NULL;
	}
}

void NewUpdatesChecker::BrowserJSDownloader::DownloadBrowserJS(const OpStringC &url, const OpStringC &target_file)
{
	m_local_file_name.Set(target_file);
	g_transferManager->GetTransferItem(&m_transferItem, url.CStr());

	if (m_transferItem)
	{
		URL* url = m_transferItem->GetURL();
		if (url)
		{
			m_transferItem->SetTransferListener(this);
			url->SetAttribute(URL::KOverrideRedirectDisabled, TRUE);
			url->Load(g_main_message_handler, URL());
		}
	}
}

void NewUpdatesChecker::BrowserJSDownloader::OnProgress(OpTransferItem* transferItem, TransferStatus status)
{
	switch (status)
	{
		case TRANSFER_ABORTED:
		{
			transferItem->GetURL()->SetAttribute(URL::KOverrideRedirectDisabled, FALSE);
			break;
		}
		case TRANSFER_DONE:
		{
			// Define an intermediate file to store to
			OpString temp_js_file;
			temp_js_file.Set(m_local_file_name.SubString(0, m_local_file_name.FindLastOf('.')));
			temp_js_file.Append(UNI_L("_temp.js"));
			OpFile temp_browser_js_file;
			temp_browser_js_file.Construct(temp_js_file.CStr());

			// Store the downloaded file to the intermediate file
			INT new_browserjs_setting = 1; // Assume the file signature check will fail.
			OpString dummy;
			TRAPD(dummyerr, transferItem->GetURL()->GetAttributeL(URL::KFilePathName_L, dummy));
			transferItem->GetURL()->SaveAsFile(temp_js_file);

			// Check if the intermediate file is ok
			if (DOM_Environment::CheckBrowserJSSignature(temp_browser_js_file))
			{
				// Copy the intermediate file to the target file
				OpFile browser_js_file;
				if (OpStatus::IsSuccess(browser_js_file.Construct(m_local_file_name.CStr())))
				{
					browser_js_file.CopyContents(&temp_browser_js_file, FALSE);
					new_browserjs_setting = 2;
				}
			}

			// Delete and close the intermediate file
			temp_browser_js_file.Delete();
			temp_browser_js_file.Close();

			// Write the new setting and wrap up
			TRAPD(res, g_pcjs->WriteIntegerL(PrefsCollectionJS::BrowserJSSetting, new_browserjs_setting));
			transferItem->GetURL()->SetAttribute(URL::KOverrideRedirectDisabled, FALSE);
			break;
		}
		case TRANSFER_PROGRESS:
		{
			return;
		}
		case TRANSFER_FAILED:
		{
			transferItem->GetURL()->SetAttribute(URL::KOverrideRedirectDisabled, FALSE);
			break;
		}
	}
}

#endif // !AUTO_UPDATE_SUPPORT
