/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/application/BrowserUpgrade.h"

#include "adjunct/autoupdate/autoupdater.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/file_utils/FileUtils.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/desktop_util/search/search_net.h"
#include "adjunct/desktop_util/search/search_protection.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/models/ServerWhiteList.h"
#include "adjunct/quick_toolkit/widgets/OpBar.h"
#include "adjunct/widgetruntime/GadgetUtils.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/regexp/include/regexp_api.h"
#include "modules/util/handy.h"

PrefsFile* BrowserUpgrade::GetToolbarSetupFile()
{
	// If we have a file and that file is at index 0 in the list it means that it's
	// the read only file so we don't want to change that. Only change files
	// that are not at index 0 (See OpSetupManager::GetSetupFile function
	// for more information)
	return g_setup_manager->GetIndexOfToolbarSetup() != 0
			// We need to read the toolbar without making a copy
			? g_setup_manager->GetSetupFile(OPTOOLBAR_SETUP, FALSE)
			: NULL;
}


OP_STATUS BrowserUpgrade::UpgradeFrom(OperaVersion previous_version)
{

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	// Package search.ini files might have been upgraded - signal to SearchProtection
	// that it should reselect IDs of preferred hardcoded search engines.
	SearchProtection::ClearIdsOfHardcodedSearches();
#endif // DESKTOP_UTIL_SEARCH_ENGINES

	RETURN_IF_ERROR(UpgradeBrowserSettings(previous_version));

	// Increment state counter for Opera Upgrade count and record the version we are upgrading from
	int upgrade_count = g_pcui->GetIntegerPref(PrefsCollectionUI::UpgradeCount);
	TRAPD(upgrade_err, g_pcui->WriteIntegerL(PrefsCollectionUI::UpgradeCount, upgrade_count + 1));
	TRAP(upgrade_err, g_pcui->WriteStringL(PrefsCollectionUI::UpgradeFromVersion, previous_version.GetFullString()));

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::UpgradeBrowserSettings(OperaVersion previous_version)
{
	struct
	{
		OperaVersion new_version;
		OP_STATUS (*upgrade_function)();
	} const upgrades[] =
	{
		{ OperaVersion(12, 50), &To_12_50 },
		{ OperaVersion(12,  0), &To_12_00 },
		{ OperaVersion(11, 60), &To_11_60 },
		{ OperaVersion(11, 51), &To_11_51 },
		{ OperaVersion(11, 50), &To_11_50 },
		{ OperaVersion(11, 10), &To_11_10 },
		{ OperaVersion(11,  1), &To_11_01 },
		{ OperaVersion(11,  0), &To_11_00 },
		{ OperaVersion(10, 70), &To_10_70 },
		{ OperaVersion(10, 61), &To_10_61 },
		{ OperaVersion(10, 60), &To_10_60 },
		{ OperaVersion(10, 51), &To_10_50 }, // 10.50 upgrading functionality didn't make it into 10.50 final
		{ OperaVersion(10, 10), &To_10_10 },
		{ OperaVersion( 9, 60), &To_9_60  },
		{ OperaVersion( 9, 50), &To_9_50  },
		{ OperaVersion( 9, 20), &To_9_20  },
	};

	for (unsigned i = 0; i < LOCAL_ARRAY_SIZE(upgrades); ++i)
	{
		if (previous_version >= upgrades[i].new_version)
			break;
		RETURN_IF_ERROR(upgrades[i].upgrade_function());
	}

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_12_50()
{
	// In session files, replace the redundant pair ("show img", "load img") with
	// one setting, "image mode".

	for (unsigned i = 0; i < g_session_manager->GetSessionCount(); ++i)
	{
		OpAutoPtr<OpSession> session(g_session_manager->ReadSession(i));
		if (session.get() == NULL)
			continue;

		TRAPD(err,
			for (unsigned j = 0; j < session->GetWindowCountL(); ++j)
			{
				OpSessionWindow* window = session->GetWindowL(j);

				if (window->HasValue("image mode"))
					continue;

				OpDocumentListener::ImageDisplayMode image_mode = OpDocumentListener::NO_IMAGES;

				const bool show_img = window->GetValueL("show img") != 0;
				const bool load_img = window->GetValueL("load img") != 0;
				if (show_img && load_img)
					image_mode = OpDocumentListener::ALL_IMAGES;
				else if (show_img)
					image_mode = OpDocumentListener::LOADED_IMAGES;

				window->SetValueL("image mode", image_mode);
			}
			session->WriteToFileL(FALSE);
		);
		OpStatus::Ignore(err);
	}

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_12_00()
{
	// Change back/forward buttons to separate buttons
	PrefsFile* prefs_file = g_setup_manager->GetSetupFile(OPTOOLBAR_SETUP, FALSE);
	if (prefs_file)
	{
		OpString_list sections_list;
		RETURN_IF_LEAVE(prefs_file->ReadAllSectionsL(sections_list));
		for (unsigned long i = 0; i < sections_list.Count(); i++)
		{
			const OpStringC& section_name(sections_list[i]);

			OpAutoPtr<PrefsSection> section;
			if (!prefs_file->IsKey(section_name, UNI_L("ButtonSet, Combined Back Forward")))
				continue;

			RETURN_IF_LEAVE(section.reset(prefs_file->ReadSectionL(section_name)));
			if (!section.get())
				continue;

			RETURN_IF_LEAVE(prefs_file->ClearSectionL(section_name));

			PrefsEntry* button_set_entry = section->FindEntry(UNI_L("ButtonSet, Combined Back Forward"));

			for (const PrefsEntry* current = section->Entries(); current; current = current->Suc())
			{
				if (current == button_set_entry)
				{
					RETURN_IF_LEAVE(prefs_file->WriteStringL(section_name,
								UNI_L("NamedButton, tbb_Back, SI_PREV_BUTTON_TEXT"),
								UNI_L("Back + Show hidden popup menu, \"Back Menu\"")));
					RETURN_IF_LEAVE(prefs_file->WriteStringL(section_name,
								UNI_L("NamedButton, tbb_Forward, SI_NEXT_BUTTON_TEXT"),
								UNI_L("Forward + Show hidden popup menu, \"Internal Forward History\"")));
				}
				else
				{
					RETURN_IF_LEAVE(prefs_file->WriteStringL(section_name, current->Key(), current->Value()));
				}
			}
		}
		if (sections_list.Count())
		{
			RETURN_IF_LEAVE(prefs_file->CommitL());
			RETURN_IF_LEAVE(prefs_file->LoadAllL());
		}
	}
	
#ifdef WIDGET_RUNTIME_SUPPORT

	//
	// In 12.00 we hide widget runtime and unite support if user has not used them.
	// This is transition stage before removing both features altogether.
	// Voice support is disabled always
	//

#if defined(_VOICE_XML_SUPPORT_)
	TRAPD(rc,g_pcvoice->ResetIntegerL(PrefsCollectionVoice::VoiceXmlEnabled));
#endif //_VOICE_XML_SUPPORT_


	bool enable_pref = false;

	// IsSandboxed() implies, that WidgetRuntime should be kept disabled
	enable_pref = g_desktop_op_system_info->IsSandboxed() ? false : (GadgetUtils::GetInstallationCount() > 0);

	if (enable_pref)
	{
		// this is not fatal error, ignore if it fails
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::DisableWidgetRuntime, false));
	}
#endif // WIDGET_RUNTIME_SUPPORT

#ifdef WEBSERVER_SUPPORT
	// this is not fatal error, ignore if it fails
	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::EnableUnite, g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverUsed)));
#endif // WEBSERVER_SUPPORT

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_11_60()
{
	// In Opera 11.60 turbosettings.xml files were moved from "locale" to "region" folder (DSK-344623)
	OpStatus::Ignore(FileUtils::SetTurboSettingsPath());
	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_11_51()
{
	// Previous to 11.51 the Auto Update Responded flag did not exist so add it!
	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::AutoUpdateResponded, 1));
	
	PrefsFile* prefs_file = GetToolbarSetupFile();
	if (prefs_file == NULL)
		return OpStatus::OK;

	// in Opera 11.50, the Turbo button implementation was changed to work in the same way as Link/Unite buttons, 
	// thus changed from "CompressionRate" to "StateButton, Turbo"
	// people with a customized toolbar should get the new entry (in 11.51, since this bug was fixed after 11.50)
	// (and since we can only add entries to the end of the section, we need to make sure that "Status" is the last element)
	BOOL was_default = FALSE;
	PrefsSection * pagebar_head;
	RETURN_IF_LEAVE(pagebar_head = g_setup_manager->GetSectionL("Status Toolbar Head.content", OPTOOLBAR_SETUP, &was_default));

	if (pagebar_head && !was_default)
	{
		OpRegExp *compression_regex;
		OpRegExp *status_regex;

		OpREFlags flags;
		flags.multi_line = FALSE;
		flags.case_insensitive = TRUE;
		flags.ignore_whitespace = TRUE;

		RETURN_IF_ERROR(OpRegExp::CreateRegExp(&compression_regex, UNI_L("CompressionRate[0-9]*"), &flags));
		OpAutoPtr<OpRegExp> compression_ptr(compression_regex);

		RETURN_IF_ERROR(OpRegExp::CreateRegExp(&status_regex, UNI_L("Status[0-9]*"), &flags));
		OpAutoPtr<OpRegExp> status_ptr(status_regex);

		bool has_status = false;

		const PrefsEntry * current = pagebar_head->Entries();
		while (current)
		{
			const uni_char* current_key = current->Key();

			OpREMatchLoc match_pos;
			if (current_key && OpStatus::IsSuccess(status_regex->Match(current_key, &match_pos)) && match_pos.matchloc == 0)
			{
				has_status = true;
				RETURN_IF_LEAVE(prefs_file->DeleteKeyL(UNI_L("Status Toolbar Head.content"), current_key));
			}

			else if (current_key && OpStatus::IsSuccess(compression_regex->Match(current_key, &match_pos)) && match_pos.matchloc == 0)
			{
				RETURN_IF_LEAVE(prefs_file->DeleteKeyL(UNI_L("Status Toolbar Head.content"), current_key));
				RETURN_IF_LEAVE(prefs_file->WriteStringL("Status Toolbar Head.content", "StateButton, \"Turbo\"", NULL));
			}
			current = current->Suc(); 
		}

		if (has_status)
			RETURN_IF_LEAVE(prefs_file->WriteStringL("Status Toolbar Head.content", "Status", NULL));
	}

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_11_50()
{
	TRAPD(rc,g_pcdoc->ResetIntegerL(PrefsCollectionDoc::SuppressExternalEmbeds));
	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_11_10()
{
	OpFile speeddial_file;

	TRAPD(err,
		g_pcfiles->GetFileL(PrefsCollectionFiles::SpeedDialFile, speeddial_file);
		BOOL speeddialfile_exists = FALSE;

		if(OpStatus::IsSuccess(speeddial_file.Exists(speeddialfile_exists)) && speeddialfile_exists)
		{
			PrefsFile sd_file(PREFS_STD);

			/* Construct and load up the speeddial.ini pref file */
			sd_file.ConstructL();
			sd_file.SetFileL(&speeddial_file);
			if (OpStatus::IsSuccess(sd_file.LoadAllL()))
			{
				int number_of_elements = 0;
				OpString_list sections;
				sd_file.ReadAllSectionsL(sections);
				for(unsigned int i = 0; i < sections.Count(); ++i)
				{
					OpString& old_section_name = sections[i];
					if(old_section_name.Compare(UNI_L(SPEEDDIAL_SECTION_STR), strlen(SPEEDDIAL_SECTION_STR)))
					{ // this is not speed dial section
						continue;
					}
					++number_of_elements;
					OpString new_section_name;
					RETURN_IF_ERROR(new_section_name.AppendFormat(UNI_L(SPEEDDIAL_SECTION), number_of_elements));
					if(old_section_name == new_section_name)
					{
						continue;
					}
					sd_file.RenameSectionL(old_section_name, new_section_name);
				}
			}

			// When upgrading to 11.10, we expect some of the pages in the
			// speed dial to have changed.  So reload them all. (See DSK-329860).
			sd_file.WriteIntL(SPEEDDIAL_VERSION_SECTION, SPEEDDIAL_KEY_FORCE_RELOAD, 1);

			sd_file.CommitL();
		}
	);
	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_11_01()
{
	// 11.00 was missing conversion *100 for this value
	// causing it to get minimal value instead of default
	// see DSK-322469
	INT32 searchfield_weighted_width = g_pcui->GetIntegerPref(PrefsCollectionUI::AddressSearchDropDownWeightedWidth);
	BOOL update_weighted_width = FALSE;

	if (searchfield_weighted_width < 100)
	{
		searchfield_weighted_width *= 100;
		update_weighted_width = TRUE;
	}
	if (searchfield_weighted_width < 2500)
	{
		searchfield_weighted_width = 2500;
		update_weighted_width = TRUE;
	}

	if (update_weighted_width)
		g_pcui->WriteIntegerL(PrefsCollectionUI::AddressSearchDropDownWeightedWidth, searchfield_weighted_width);

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_11_00()
{
	// Before eleven: local whitelist was overriding global one
	// After: local and global are appended and duplicates are removed
	// All we need to do is to initialize whitelist and Write it; it will
	// clean itself.
	g_server_whitelist->Write();

	// Upgrade homepage if it is an old default. DSK-325446
	OpString homepage;
	if (OpStatus::IsSuccess(homepage.Set(g_pccore->GetStringPref(PrefsCollectionCore::HomeURL))))
	{
		OpString default_homepage;
		TRAPD(rc, g_prefsManager->GetPreferenceL("User Prefs", "Home URL", default_homepage, TRUE));
		if (OpStatus::IsSuccess(rc))
		{
			BOOL upgrade = FALSE;
			if (homepage.Compare(default_homepage))
			{
				// Keep it simple. List may be extended
				const char* candidate[] = {"http://portal.opera.com", "http://portal.opera.com/", 0};
				for (int i=0; !upgrade && candidate[i]; i++)
					upgrade = !homepage.Compare(candidate[i]);
			}

			if (upgrade)
			{
				TRAP(rc, g_pccore->WriteStringL(PrefsCollectionCore::HomeURL, default_homepage));
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_10_70()
{
	PrefsFile* prefs_file = GetToolbarSetupFile();
	if (prefs_file == NULL)
		return OpStatus::OK;

	OpBar::Alignment old_alignment;

	RETURN_IF_LEAVE(old_alignment = (OpBar::Alignment)prefs_file->ReadIntL(UNI_L("Personalbar.alignment"), UNI_L("Alignment"), OpBar::ALIGNMENT_OFF));
	if(old_alignment != OpBar::ALIGNMENT_OFF)
	{
		// Force off the old personal bar on top, the new one is on by default
		RETURN_IF_LEAVE(prefs_file->WriteIntL(UNI_L("Personalbar.alignment"), UNI_L("Alignment"), OpBar::ALIGNMENT_OFF));
		RETURN_IF_LEAVE(prefs_file->WriteIntL(UNI_L("Personalbar Inline.alignment"), UNI_L("Alignment"), OpBar::ALIGNMENT_TOP));
	}

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_10_61()
{
	// before 10.60 Final, the auto-update default was 'check for updates'. now it is changed to 'auto-install updates'
	// people who are upgrading to 10.60 should not be forced to the new default

	PrefsFile* reader = const_cast<PrefsFile*>(g_prefsManager->GetReader());
	if (reader == NULL)
		return OpStatus::OK;

	RETURN_IF_LEAVE(reader->LoadAllL());

	BOOL is_au_level_set = FALSE;

	// check if the user has overridden the default setting
	is_au_level_set = reader->IsKey("User Prefs", "Level Of Update Automation");

	int level = g_pcui->GetIntegerPref(PrefsCollectionUI::LevelOfUpdateAutomation);
	if (!is_au_level_set && level == AutoInstallUpdates)
	{
		// go back to the default value prior to 10.60: LevelOfAutomation::CheckForUpdates
		TRAPD(upgrade_err, g_pcui->WriteIntegerL(PrefsCollectionUI::LevelOfUpdateAutomation, CheckForUpdates));
	}

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_10_60()
{
	// we introduced a default speed dial background image in 10.60

	// Code copied from SpeedDialManager::Save()
	OpFile speeddial_file;
	PrefsFile sd_file(PREFS_STD);
	TRAPD(err,
		g_pcfiles->GetFileL(PrefsCollectionFiles::SpeedDialFile, speeddial_file);

		/* Construct and load up the speeddial.ini pref file */
		sd_file.ConstructL();
		sd_file.SetFileL(&speeddial_file);

		sd_file.WriteIntL(SPEEDDIAL_IMAGE_SECTION, SPEEDDIAL_KEY_IMAGE_ENABLED, 1);
		sd_file.CommitL();
		);

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_10_50()
{
	PrefsFile* prefs_file = GetToolbarSetupFile();
	if (prefs_file == NULL)
		return OpStatus::OK;

	// If we get here this must be a user toolbar.
	// Read the [Status Toolbar.content], we only care if we get something from the custom file
	PrefsSection* section = NULL;
	BOOL was_default = FALSE;
	RETURN_IF_LEAVE(section = g_setup_manager->GetSectionL("Status Toolbar.content", OPTOOLBAR_SETUP, &was_default));

	// Check was_default to see if the entry was contained in the custom toolbar file, otherwise we don't need to do anything
	// because the new default will just be used
	if (!section || was_default)
		return OpStatus::OK;

	// we need to do this regex-workaround, because customized toolbars get a number attached to the key, and the key does have to match 
	// exactly (including the number) to be found (ie. 'StateButton' vs. 'StateButton1')
	const PrefsEntry * current = section->Entries();
	
	if (current)
	{
		OpStringC img_pattern(UNI_L("Enable display images > Display cached images only, , , [A-Za-z0-9_-]* | Disable display images, , , [A-Za-z0-9_-]* + Show popup menu, \"Images Menu\"\""));
		OpRegExp *img_regex;

		OpStringC compression_pattern(UNI_L("StateButton[0-9]*, \"Turbo\""));
		OpRegExp *compression_regex;

		OpStringC status_pattern(UNI_L("Status[0-9]*"));
		OpRegExp *status_regex;

		OpStringC update_pattern(UNI_L("MinimizedUpdate[0-9]*"));
		OpRegExp *update_regex;

		OpStringC zoom_pattern(UNI_L("Zoom[0-9]*"));
		OpRegExp *zoom_regex;

		OpStringC sync_pattern(UNI_L("StateButton[0-9]*, \"Sync\""));
		OpRegExp *sync_regex;

		OpStringC webserver_pattern(UNI_L("StateButton[0-9]*, \"WebServer\""));
		OpRegExp *webserver_regex;

		OpREFlags flags;
		flags.multi_line = FALSE;
		flags.case_insensitive = TRUE;
		flags.ignore_whitespace = TRUE;

		if (OpStatus::IsSuccess(OpRegExp::CreateRegExp(&img_regex, img_pattern.CStr(), &flags)) &&
			OpStatus::IsSuccess(OpRegExp::CreateRegExp(&compression_regex, compression_pattern.CStr(), &flags)) &&
			OpStatus::IsSuccess(OpRegExp::CreateRegExp(&status_regex, status_pattern.CStr(), &flags)) &&
			OpStatus::IsSuccess(OpRegExp::CreateRegExp(&update_regex, update_pattern.CStr(), &flags)) &&
			OpStatus::IsSuccess(OpRegExp::CreateRegExp(&zoom_regex, zoom_pattern.CStr(), &flags)) &&
			OpStatus::IsSuccess(OpRegExp::CreateRegExp(&sync_regex, sync_pattern.CStr(), &flags)) &&
			OpStatus::IsSuccess(OpRegExp::CreateRegExp(&webserver_regex, webserver_pattern.CStr(), &flags))
			)
		{
			OpStringC status_toolbar_tail(UNI_L("Status Toolbar Tail.content"));

			BOOL has_customized_tail = prefs_file->IsSectionLocal(status_toolbar_tail);

			// keys moved from "Status Toolbar" to "Status Toolbar Head/Tail" (in 10.50)
			// delete the the default ones from "Status Toolbar" move the others over to tail
			while (current)
			{
				OpREMatchLoc match_pos;

				OpStringC tmp_key(current->Key());
				OP_ASSERT(tmp_key.HasContent());
				OpStringC tmp_value(current->Value());
				BOOL key_found = FALSE;

				// SEARCH BY VALUE (don't know another way to identify the darn thing)
				if (tmp_value.Compare("Enable mediumscreen mode | Disable mediumscreen mode") == 0)
				{
					key_found = TRUE;
				}
				else if (tmp_value.HasContent() && OpStatus::IsSuccess(img_regex->Match(tmp_value.CStr(), &match_pos)) && match_pos.matchloc == 0)
				{
					key_found = TRUE;
				}

				// SEARCH BY KEY
				// compression rate
				else if (OpStatus::IsSuccess(compression_regex->Match(tmp_key.CStr(), &match_pos)) && match_pos.matchloc == 0)
				{
					key_found = TRUE;
				}
				// status
				else if (OpStatus::IsSuccess(status_regex->Match(tmp_key.CStr(), &match_pos)) && match_pos.matchloc == 0)
				{
					key_found = TRUE;
				}
				// autoupdate
				else if (OpStatus::IsSuccess(update_regex->Match(tmp_key.CStr(), &match_pos)) && match_pos.matchloc == 0)
				{
					key_found = TRUE;
				}
				// zoom
				else if (OpStatus::IsSuccess(zoom_regex->Match(tmp_key.CStr(), &match_pos)) && match_pos.matchloc == 0)
				{
					key_found = TRUE;
				}
				// sync
				else if (OpStatus::IsSuccess(sync_regex->Match(tmp_key.CStr(), &match_pos)) && match_pos.matchloc == 0)
				{
					key_found = TRUE;
				}
				// webserver
				else if (OpStatus::IsSuccess(webserver_regex->Match(tmp_key.CStr(), &match_pos)) && match_pos.matchloc == 0)
				{
					key_found = TRUE;
				}

				if (!key_found) // move the button to "Status Toolbar Tail"
				{
					RETURN_IF_LEAVE(prefs_file->WriteStringL(status_toolbar_tail, tmp_key, tmp_value));
				}

				current = current->Suc(); 
			}
			OP_DELETE(img_regex);
			OP_DELETE(compression_regex);
			OP_DELETE(status_regex);
			OP_DELETE(update_regex);
			OP_DELETE(zoom_regex);
			OP_DELETE(sync_regex);
			OP_DELETE(webserver_regex);

			// if the customized tail section didn't exist before, copy defaults to new customized section
			if (!has_customized_tail && prefs_file->IsSectionLocal(status_toolbar_tail))
			{
				PrefsSection	*status_toolbar_tail_default = NULL;

				// Get the [Status Toolbar Tail.content] from the default toolbar file (i.e. in the package)
				RETURN_IF_LEAVE(status_toolbar_tail_default = g_setup_manager->GetSectionL("Status Toolbar Tail.content", OPTOOLBAR_SETUP, NULL, TRUE));

				// Make sure we got the section from the default file
				if (status_toolbar_tail_default)
				{
					// copy all entries in default toolbar over to new toolbar
					for (const PrefsEntry *OP_MEMORY_VAR entry = status_toolbar_tail_default->Entries(); entry; entry = (const PrefsEntry *) entry->Suc())
					{
						RETURN_IF_LEAVE(prefs_file->WriteStringL(status_toolbar_tail, entry->Key(), entry->Value()));
					}
				}
			}

			// Commit the file to disk and the Delete section loads everything
			RETURN_IF_LEAVE(prefs_file->CommitL());

			// content of 'Status Toolbar.content' isn't used any more, so delete it
			RETURN_IF_LEAVE(prefs_file->DeleteSectionL("Status Toolbar.content"));
		}
	}

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_10_10()
{
#ifdef WEBSERVER_SUPPORT
	PrefsFile* prefs_file = GetToolbarSetupFile();
	if (prefs_file == NULL)
		return OpStatus::OK;

	OpStringC8 hotlistselector_entry8("Hotlist Panel Selector.content");
	OpStringC  hotlistselector_entry(UNI_L("Hotlist Panel Selector.content"));

	PrefsSection * hotlist_section = NULL;
	BOOL was_default = FALSE;
	hotlist_section = g_setup_manager->GetSectionL(hotlistselector_entry8.CStr(), OPTOOLBAR_SETUP, &was_default);

	// only add Unite panel for customized (non-default) toolbars
	if (hotlist_section && !was_default)
	{
		// known issue: we don't check if entry is already in the customized toolbar. duplicate entry isn't shown, though
		// and it's only a problem for the first Unite labs and Unite snapshots before 10.0 beta2
		OpStringC hotlistselector_key(UNI_L("Unite, 4"));
		RETURN_IF_LEAVE(prefs_file->WriteStringL(hotlistselector_entry, hotlistselector_key, NULL));
	}
#endif // WEBSERVER_SUPPORT

	// if it's an upgrade from < 10, we have new options when it comes to mailto handlers
	// show them the next time they click a mailto: link, unless they use M2 as a mailto handler
	if (g_pcui->GetIntegerPref(PrefsCollectionUI::MailHandler) != MAILHANDLER_OPERA)
	{
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::MailHandler, MAILHANDLER_OPERA));
	}

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_9_60()
{
	// Broken setting fixup for bug 337452
	Viewer* flash_viewer = g_viewers->FindViewerByMimeType(UNI_L("application/x-shockwave-flash"));
	OP_ASSERT(flash_viewer);
	if (flash_viewer && flash_viewer->GetAction() == VIEWER_OPERA && flash_viewer->GetDefaultPluginViewer())
	{
		flash_viewer->SetAction(VIEWER_PLUGIN);
		TRAPD(err, flash_viewer->WriteL());
		OpStatus::Ignore(err);
	}

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_9_50()
{
	// Override the mail pref for fix bug #308706
	TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::ErrorConsoleFilter, UNI_L("mail=3")));

	PrefsFile* prefs_file = GetToolbarSetupFile();
	if (prefs_file == NULL)
		return OpStatus::OK;

	// If we get here this must be a user toolbar.
	// Read the [Status Toolbar.content], we only care if we get something from the custom file
	PrefsSection* section = NULL;
	BOOL was_default = FALSE;
	RETURN_IF_LEAVE(section = g_setup_manager->GetSectionL("Status Toolbar.content", OPTOOLBAR_SETUP, &was_default));

	// Check was_default to see if the entry was contained in the custom toolbar file,
	// otherwise we don't need to do anything because the new default will just be used
	if (!section || was_default)
		return OpStatus::OK;

	// force on
	RETURN_IF_LEAVE(prefs_file->WriteIntL(UNI_L("Status Toolbar.alignment"), UNI_L("Alignment"), OpBar::ALIGNMENT_BOTTOM));
	RETURN_IF_LEAVE(g_pcui->WriteIntegerL(PrefsCollectionUI::StatusbarAlignment, OpBar::ALIGNMENT_BOTTOM));

	// Force the new mini style and old wrap
	RETURN_IF_LEAVE(prefs_file->WriteIntL(UNI_L("Status Toolbar.style"), UNI_L("Button style"), 0));
	RETURN_IF_LEAVE(prefs_file->WriteIntL(UNI_L("Status Toolbar.style"), UNI_L("Wrapping"), 0));
	RETURN_IF_LEAVE(prefs_file->WriteIntL(UNI_L("Status Toolbar.style"), UNI_L("Mini Buttons"), 1));

 	// Force off the viewbar
	RETURN_IF_LEAVE(prefs_file->WriteIntL(UNI_L("Document View Toolbar.alignment"), UNI_L("Alignment"), OpBar::ALIGNMENT_OFF));

	return OpStatus::OK;
}


OP_STATUS BrowserUpgrade::To_9_20()
{
#ifdef SUPPORT_START_BAR
	//Force the personal bar to be off on default
	PrefsFile* prefs_file = g_setup_manager->GetSetupFile(OPTOOLBAR_SETUP, TRUE);
	if (prefs_file)
		RETURN_IF_LEAVE(prefs_file->WriteIntL("Start Toolbar.alignment", "Alignment", 0)); //OpBar::ALIGNMENT_OFF = 0
#endif // SUPPORT_START_BAR
	return OpStatus::OK;
}
