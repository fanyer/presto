/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2003-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef SKIN_SUPPORT

#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkin.h"
#ifdef PERSONA_SKIN_SUPPORT
#include "modules/skin/OpPersona.h"
#endif // PERSONA_SKIN_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/pi/ui/OpUiInfo.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/display/vis_dev.h"
#ifdef QUICK
#include "adjunct/quick/Application.h" //RedrawAllWindows
#include "adjunct/quick/dialogs/SimpleDialog.h"
#endif
#include "modules/util/filefun.h"
#include "modules/util/zipload.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#include "modules/pi/OpSystemInfo.h"

//==================================================================================
#define CURRENT_SKIN_VERSION_NUMBER 2			//All skins with a lower version number are deprecated
#define HIGHEST_VALID_SKIN_VERSION_NUMBER 3		//Skins that set a version number higher than that are misusing this parameter

#define CURRENT_PERSONA_VERSION_NUMBER	1		// Current version of the persona system
#define HIGHEST_VALID_PERSONA_VERSION_NUMBER 1	// Personas that set a version number higher than that are misusing this parameter

//==================================================================================

/***********************************************************************************
**
**	OpSkinManager
**
***********************************************************************************/

OpSkinManager::OpSkinManager() :
	m_skin(NULL),
	m_defaultskin(NULL),
#ifdef QUICK
	m_skin_list(2),
#endif
	m_selected_from_list(-1),
	m_is_global_skin_manager(FALSE),
	m_selected_text_color(0),
	m_selected_text_bgcolor(0),
	m_selected_text_color_nofocus(0),
	m_selected_text_bgcolor_nofocus(0),
	m_fieldset_border_color(0),
	m_draw_focus_rect(TRUE),
	m_line_padding(0),
	m_color_scheme_mode(OpSkin::COLOR_SCHEME_MODE_NONE),
	m_color_scheme_color(0),
	m_color_scheme_custom_type(OpSkin::COLOR_SCHEME_CUSTOM_NORMAL),
	m_scale(100),
	m_has_scanned_skin_folder(FALSE),
	m_dim_disabled_backgrounds(TRUE),
	m_glow_on_hover(TRUE)
	, m_transparent_background_color(0xffc0c0c0)
#ifdef PERSONA_SKIN_SUPPORT
	, m_persona_skin(NULL)
#endif // PERSONA_SKIN_SUPPORT
{
}

OpSkinManager::~OpSkinManager()
{
	OP_ASSERT(m_skin != g_skin_manager->m_defaultskin);
	OP_DELETE(m_defaultskin);
	m_defaultskin = NULL;
	OP_DELETE(m_skin);
#ifdef PERSONA_SKIN_SUPPORT
	OP_DELETE(m_persona_skin);
#endif // PERSONA_SKIN_SUPPORT
}

OP_STATUS OpSkinManager::InitL(BOOL global_skin_manager)
{
	m_is_global_skin_manager = global_skin_manager;
	m_color_scheme_mode = (OpSkin::ColorSchemeMode)  g_pccore->GetIntegerPref(PrefsCollectionCore::ColorSchemeMode);
	m_color_scheme_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_SKIN);
	m_scale = g_pccore->GetIntegerPref(PrefsCollectionCore::SkinScale);

#ifdef PERSONA_SKIN_SUPPORT
	// Create any persona before we create any skins as the persona might need to change
	// the color scheme used for the loaded skin
	OpFile persona; ANCHOR(OpFile, persona);
	g_pcfiles->GetFileL(PrefsCollectionFiles::PersonaFile, persona);

	m_persona_skin = CreatePersona(&persona);
#endif // PERSONA_SKIN_SUPPORT

	OP_ASSERT(m_defaultskin == NULL);

	OpFile defaultfile; ANCHOR(OpFile, defaultfile);
	LEAVE_IF_ERROR(defaultfile.Construct(STANDARD_SKIN_FILE, OPFILE_DEFAULT_SKIN_FOLDER));

	OpFile button_set; ANCHOR(OpFile, button_set);
	g_pcfiles->GetFileL(PrefsCollectionFiles::ButtonSet, button_set);
	if (!uni_str_eq(defaultfile.GetName(), button_set.GetName()))
	{
		// We have to be able to fallback to the default skin if the
		// set skin is incomplete
		m_defaultskin = CreateSkin(&defaultfile);
		SelectSkinByFile(&button_set);
	}

	if (!m_defaultskin)
	{
		// There was no custom skin or the custom skin didn't work
		m_defaultskin = CreateSkin(&defaultfile);
	}

#ifdef QUICK
	UpdateTreeModel();
#endif
	return m_defaultskin ? OpStatus::OK : OpStatus::ERR;
}

#ifdef QUICK

void OpSkinManager::ScanSkinFolderL()
{
	// Scan the "program skin folder" and the "personal skin folder", or just the profile persona folder
	OpString program_skin_folder, personal_skin_folder;
	ANCHOR(OpString, program_skin_folder);
	ANCHOR(OpString, personal_skin_folder);

	m_skin_list.DeleteAll();

	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_DEFAULT_SKIN_FOLDER, program_skin_folder));
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_SKIN_FOLDER, personal_skin_folder));

	OpStringC skinwildcard(UNI_L("*.zip"));

	TRAPD(err, ScanFolderL(skinwildcard, program_skin_folder, TRUE));

	if(personal_skin_folder.CompareI(program_skin_folder) != 0)	//no need to list the same skins twice if the folders are the same
	{
		TRAP(err, ScanFolderL(skinwildcard, personal_skin_folder, FALSE));
	}

#ifdef SKIN_USE_CUSTOM_FOLDERS
	OpString custom_skin_folder;

	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_SKIN_CUSTOM_FOLDER, custom_skin_folder));

	if(personal_skin_folder.CompareI(custom_skin_folder) != 0 && program_skin_folder.CompareI(custom_skin_folder) != 0)	//no need to list the same skins twice if the folders are the same
	{
		TRAPD(err, ScanFolderL(skinwildcard, custom_skin_folder, FALSE));
	}
#endif // SKIN_USE_CUSTOM_FOLDERS

	m_has_scanned_skin_folder = TRUE;
}

void OpSkinManager::ScanSkinFolderIfNeeded()
{
	if (m_has_scanned_skin_folder) 
		return;

	TRAPD(err, ScanSkinFolderL());
}

void OpSkinManager::ScanFolderL(const OpStringC& searchtemplate, const OpStringC& skin_folder, BOOL is_defaults)
{
	if (skin_folder.HasContent() && searchtemplate.HasContent())
	{
		OpFolderLister* folder_lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, searchtemplate.CStr(), skin_folder.CStr());
		if (!folder_lister)
			return;

		OpStackAutoPtr<OpFolderLister> folder_lister_ap(folder_lister);

		OpFile buttonset; ANCHOR(OpFile, buttonset);

		g_pcfiles->GetFileL(PrefsCollectionFiles::ButtonSet, buttonset);

#ifdef PERSONA_SKIN_SUPPORT

		OpFile persona; ANCHOR(OpFile, persona);
		g_pcfiles->GetFileL(PrefsCollectionFiles::PersonaFile, persona);

		OpStringC selected_persona(persona.GetFullPath());
#endif // PERSONA_SKIN_SUPPORT

		OpStringC selected_skin(buttonset.GetFullPath());

		while (folder_lister->Next())
		{
			const uni_char *candidate = folder_lister->GetFullPath();
			if( candidate )
			{
				// Prevent directories (bug #205303)
				if( folder_lister->IsFolder() )
					continue;

				// check if skin can be opened
				OpFile skin_ini_file; ANCHOR(OpFile, skin_ini_file);
				OpString skin_ini_path; ANCHOR(OpString, skin_ini_path);

				LEAVE_IF_ERROR(skin_ini_path.AppendFormat(UNI_L("%s%cskin.ini"), candidate, PATHSEPCHAR));
				LEAVE_IF_ERROR(skin_ini_file.Construct(skin_ini_path.CStr()));

				PrefsFile skin_ini(PREFS_INI); ANCHOR(PrefsFile, skin_ini);
				skin_ini.ConstructL();
				TRAPD(err, skin_ini.SetFileL(&skin_ini_file));
				if (OpStatus::IsError(err) || OpStatus::IsError(skin_ini.LoadAllL()))
				{
#ifdef PERSONA_SKIN_SUPPORT
					skin_ini_path.Empty();
					skin_ini.Reset();
					LEAVE_IF_ERROR(skin_ini_path.AppendFormat(UNI_L("%s%cpersona.ini"), candidate, PATHSEPCHAR));
					LEAVE_IF_ERROR(skin_ini_file.Construct(skin_ini_path.CStr()));
					TRAPD(err, skin_ini.SetFileL(&skin_ini_file));
					if (OpStatus::IsError(err) || OpStatus::IsError(skin_ini.LoadAllL()))
					{
						continue;
					}
#else
					continue;
#endif // PERSONA_SKIN_SUPPORT
				}
				OpStringC title_code, title_candidate;

				title_candidate = skin_ini.ReadStringL("Info", "Name");
				if (is_defaults)
					title_code = skin_ini.ReadStringL("Info", "Name String Code");

				// add the full path name of skin as column 0, and default status as user data

				INT32 item_pos = m_skin_list.AddItem(candidate, 0, 0, -1, is_defaults ? (void*)0x1 : 0);

				// add title of skin as column 1

				OpString title; ANCHOR(OpString, title);

				if (title_code.HasContent())
					g_languageManager->GetString(Str::LocaleString(title_code.CStr()), title);
				else if (title_candidate.HasContent())
					title.SetL(title_candidate);
				else
				{
					title.SetL(candidate);
	
					int patseppos = title.FindLastOf(PATHSEPCHAR);
					if(KNotFound != patseppos)		//we got a pathseparator, remove it
					{
						title.Delete(0, patseppos+1);
					}
	
					int dot = title.FindLastOf('.');
					if(KNotFound != dot)			//we got a dot, terminate at it
					{
						title[dot] = 0;
					}
				}

				m_skin_list.SetItemData(item_pos, 1, title.CStr());

				// set the currently selected skin number
#ifdef PERSONA_SKIN_SUPPORT
				if(selected_persona.Compare(candidate) == 0)
				{
					m_selected_from_list = m_skin_list.GetItemCount()-1;
				}
				else
#endif // PERSONA_SKIN_SUPPORT
				if(selected_skin.Compare(candidate) == 0)
				{
					m_selected_from_list = m_skin_list.GetItemCount()-1;
				}
			}
		}
	}
}

OP_STATUS OpSkinManager::SelectSkin(INT32 index)
{
	OpFile* skinfile = OP_NEW(OpFile, ());
	if (!skinfile)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS rc = skinfile->Construct(m_skin_list.GetItemString(index));
	if (OpStatus::IsError(rc))
	{
		OP_DELETE(skinfile);
		return rc;
	}
	// Find out if it's a skin or persona then set and apply the skin
	SkinFileType type = GetSkinFileType(m_skin_list.GetItemString(index));
	if(type == SkinNormal)
	{
		RETURN_IF_ERROR(SelectSkinByFile(skinfile, FALSE));

#ifdef PERSONA_SKIN_SUPPORT
		// if we select a non-persona skin, make sure the persona is not active anymore
		SelectPersonaByFile(NULL, FALSE);
#endif // PERSONA_SKIN_SUPPORT

	}
#ifdef PERSONA_SKIN_SUPPORT
	else if(type == SkinPersona)
	{
		OpFile default_skin;
		TRAPD(err, g_pcfiles->GetFileL(PrefsCollectionFiles::ButtonSet, default_skin));

		// we need to set the default platform skin before we select the persona to ensure a persona
		// is always running on top of a default skin
		if(!uni_str_eq(default_skin.GetName(), DEFAULT_SKIN_FILE))
		{
			OpFile defaultfile;
			RETURN_IF_ERROR(defaultfile.Construct(DEFAULT_SKIN_FILE, OPFILE_DEFAULT_SKIN_FOLDER));

			RETURN_IF_ERROR(SelectSkinByFile(&defaultfile, FALSE));
		}
		RETURN_IF_ERROR(SelectPersonaByFile(skinfile, FALSE));
	}
#endif // PERSONA_SKIN_SUPPORT
	m_selected_from_list = index;

	OP_DELETE(skinfile);

	return OpStatus::OK;
}

#endif // QUICK

#ifdef PERSONA_SKIN_SUPPORT
OP_STATUS OpSkinManager::SelectPersonaByFile(OpFile *skinfile, BOOL rescan_skin_folder)
{
	OpPersona *persona = CreatePersona(skinfile);

	OP_DELETE(m_persona_skin);
	m_persona_skin = persona;

	Flush();
	
	m_selected_from_list = -1;

#ifdef QUICK
	if (m_is_global_skin_manager)
	{
		OpFile default_skin;
		// Set a bogus name so that this preference can be overridden and then set back to default
		// while using a custom persona at the same time. i.e. DSK-365433
		default_skin.Construct(UNI_L("default.persona"));
		if(persona)
		{
			TRAPD(err, g_pcfiles->GetFileL(PrefsCollectionFiles::PersonaFile, default_skin));

			// Only write into the preference if the skinfile has actually changed
			if (!default_skin.GetFullPath() || !uni_str_eq(default_skin.GetFullPath(), skinfile->GetFullPath()))
			{
				TRAP(err, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::PersonaFile, skinfile));
				if (g_application)
					g_application->SettingsChanged(SETTINGS_PERSONA);

				OpFile default_skin;
				TRAPD(err, g_pcfiles->GetFileL(PrefsCollectionFiles::ButtonSet, default_skin));

				// we need to set the default platform skin before we select the persona to ensure a
				// persona is always running on top of a default skin
				if(!uni_str_eq(default_skin.GetName(), DEFAULT_SKIN_FILE))
				{
					OpFile defaultfile;
					RETURN_IF_ERROR(defaultfile.Construct(DEFAULT_SKIN_FILE, OPFILE_DEFAULT_SKIN_FOLDER));

					RETURN_IF_ERROR(SelectSkinByFile(&defaultfile, FALSE));
				}
			}
		}
		else
		{
			// reset persona entry when we do not use it
			TRAPD(err, g_pcfiles->ResetFileL(PrefsCollectionFiles::PersonaFile));
			if (g_application)
				g_application->SettingsChanged(SETTINGS_PERSONA);
		}
	}
	if (rescan_skin_folder)
	{
		TRAPD(err, ScanSkinFolderL());
	}
#endif
	BroadcastPersonaLoaded(m_persona_skin);

	return OpStatus::OK;
}
#endif // PERSONA_SKIN_SUPPORT

OP_STATUS OpSkinManager::SelectSkinByFile(OpFile* skinfile, BOOL rescan_skin_folder)
{
	OpSkin* skin = CreateSkin(skinfile);

	if (skin)
	{
		if(m_defaultskin)
		{
			m_defaultskin->Flush();
		}
		OP_DELETE(m_skin);
		m_skin = skin;
		m_selected_from_list = -1;

#ifdef QUICK
		if (m_is_global_skin_manager)
		{
			OpFile default_skin;
			TRAPD(err, g_pcfiles->GetFileL(PrefsCollectionFiles::ButtonSet, default_skin));

			// Only write into the preference if the skinfile has actually changed
			if (!uni_str_eq(default_skin.GetFullPath(), skinfile->GetFullPath()))
			{
				TRAP(err, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::ButtonSet, skinfile));
				if (g_application)
					g_application->SettingsChanged(SETTINGS_SKIN);
			}
		}

		if (rescan_skin_folder)
		{
			TRAPD(err, ScanSkinFolderL());
		}
#endif
		BroadcastSkinLoaded(m_skin);

		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

#ifdef QUICK

OP_STATUS OpSkinManager::DeleteSkin(INT32 index)
{
	if(GetSkinCount() == 1)		//do not allow deletion of the last skin
	{
		return OpStatus::ERR;
	}
	//is the current skin being deleted, then try to select another skin
	if(m_selected_from_list == index)
	{
		INT32 newskinidx = (index == 0) ? 1 : index - 1;
		RETURN_IF_ERROR(SelectSkin(newskinidx));
	}

	OpFile filetodelete;
	OP_STATUS err = filetodelete.Construct(m_skin_list.GetItemString(index));
	if (OpStatus::IsSuccess(err))
		filetodelete.Delete();

	m_skin_list.Delete(index);

	return OpStatus::OK;
}

#endif // QUICK

#ifdef SKIN_SKIN_COLOR_THEME
void OpSkinManager::SetColorSchemeMode(OpSkin::ColorSchemeMode mode)
{
	m_color_scheme_mode = mode;

	if (m_defaultskin)
	{
		m_defaultskin->SetColorSchemeMode(m_color_scheme_mode);
	}
	if (m_skin)
	{
		m_skin->SetColorSchemeMode(m_color_scheme_mode);
	}
}

void OpSkinManager::SetColorSchemeColor(INT32 color)
{
	m_color_scheme_color = color;

	if (m_defaultskin)
	{
		m_defaultskin->SetColorSchemeColor(m_color_scheme_color);
	}
	if (m_skin)
	{
		m_skin->SetColorSchemeColor(m_color_scheme_color);
	}
}

void OpSkinManager::SetColorSchemeCustomType(OpSkin::ColorSchemeCustomType type)
{
	m_color_scheme_custom_type = type;

	if (m_defaultskin)
	{
		m_defaultskin->SetColorSchemeCustomType(m_color_scheme_custom_type);
	}
	if (m_skin)
	{
		m_skin->SetColorSchemeCustomType(m_color_scheme_custom_type);
	}
}

#endif // SKIN_SKIN_COLOR_THEME

void OpSkinManager::SetScale(INT32 scale)
{
	m_scale = scale;

	if (m_defaultskin)
	{
		m_defaultskin->SetScale(m_scale);
	}
	if (m_skin)
	{
		m_skin->SetScale(m_scale);
	}
}

void OpSkinManager::Flush()
{
	if (m_defaultskin)
	{
		m_defaultskin->Flush();
	}
	if (m_skin)
	{
		m_skin->Flush();
	}
}

#ifdef QUICK

OpTreeModel* OpSkinManager::GetTreeModel()
{
	// so that column title get updated after language changed
	UpdateTreeModel();

	ScanSkinFolderIfNeeded();

	return &m_skin_list;
}

void OpSkinManager::UpdateTreeModel()
{
	OpString skins;

	if (OpStatus::IsSuccess(g_languageManager->GetString(Str::S_SKINS, skins)))
		m_skin_list.SetColumnData(1, skins.CStr());
}

const uni_char* OpSkinManager::GetSkinName(INT32 index)
{
	ScanSkinFolderIfNeeded();

	return m_skin_list.GetItemString(index, 1);
}

INT32 OpSkinManager::GetSkinCount() 
{ 
	ScanSkinFolderIfNeeded(); 

	return m_skin_list.GetItemCount();

}

INT32 OpSkinManager::GetSelectedSkin() 
{ 
	ScanSkinFolderIfNeeded(); 

	return m_selected_from_list;
}

void OpSkinManager::RemovePosFromList(UINT32 pos)
{
	m_skin_list.Delete(pos);
}

BOOL OpSkinManager::IsUserInstalledSkin(INT32 index)
{ 
	ScanSkinFolderIfNeeded(); 

	return !m_skin_list.GetItemUserData(index); 
}

#endif // QUICK

OpSkin* OpSkinManager::CreateSkin(OpFileDescriptor* skinfile)
{
	OpSkin* OP_MEMORY_VAR skin = OP_NEW(OpSkin, ());

	if (!skin)
		return NULL;

	TRAPD(err, skin->InitL(skinfile));

	INT32 skin_version = 0;

	if (OpStatus::IsError(err))
	{
		OP_DELETE(skin);
		return NULL;
	}
	else
	{
#ifdef SKIN_SKIN_COLOR_THEME
		skin->SetColorSchemeMode(m_color_scheme_mode);
		skin->SetColorSchemeColor(m_color_scheme_color);
#endif
		skin->SetScale(m_scale);

		TRAP(err, skin_version = skin->GetPrefsFile()->ReadIntL("Info","Version"));
		// in some skins, the version parameter is by mistake used for the versioning of one specific skin
		// we need to make sure that these skins with higher version numbers than the ones we use don't
		// mess with our fallback mechanism
		if (skin_version > HIGHEST_VALID_SKIN_VERSION_NUMBER)
			skin_version = CURRENT_SKIN_VERSION_NUMBER;

		TRAP(err, m_selected_text_color = skin->ReadColorL("Generic","Selected Text color", REALLY_UGLY_COLOR));

		TRAP(err, m_selected_text_bgcolor = skin->ReadColorL("Generic","Selected Text bgcolor", REALLY_UGLY_COLOR));

		TRAP(err, m_selected_text_color_nofocus = skin->ReadColorL("Generic","Selected Text color nofocus", REALLY_UGLY_COLOR));

		TRAP(err, m_selected_text_bgcolor_nofocus = skin->ReadColorL("Generic","Selected Text bgcolor nofocus", REALLY_UGLY_COLOR));

		TRAP(err, m_fieldset_border_color = skin->ReadColorL("Generic","Fieldset Border color", OP_RGB(222, 222, 222)));

		TRAP(err, m_draw_focus_rect = skin->GetPrefsFile()->ReadBoolL("Generic","Draw Focusrect", TRUE); \
			m_line_padding = skin->GetPrefsFile()->ReadIntL("Generic","Line padding", 1); \
			m_dim_disabled_backgrounds = skin->GetPrefsFile()->ReadBoolL("Options", "Dim Disabled Backgrounds", TRUE); \
			m_glow_on_hover = skin->GetPrefsFile()->ReadBoolL("Options", "Glow On Hover", TRUE));
	}

#ifdef QUICK
	if(skin_version < CURRENT_SKIN_VERSION_NUMBER)
	{
		OP_DELETE(skin);
		if (g_application)
		{
			SkinVersionError* versiondialog = OP_NEW(SkinVersionError, ());
			if (versiondialog)
				versiondialog->Init(g_application->GetActiveDesktopWindow());
		}
		return NULL;
	}
#endif
	skin->SetSkinVersion(skin_version);

	BroadcastSkinLoaded(skin);

	return skin;
}

#ifdef PERSONA_SKIN_SUPPORT

OpPersona* OpSkinManager::CreatePersona(OpFileDescriptor* personafile)
{
	if(!personafile)
		return NULL;

	OP_MEMORY_VAR OP_STATUS s = OpStatus::OK;
	OP_MEMORY_VAR INT32 skin_version = 0;
	OpPersona* OP_MEMORY_VAR skin = OP_NEW(OpPersona, ());

	if (!skin)
		return NULL;

	TRAPD(err, s = skin->InitL(personafile));

	if (OpStatus::IsError(err) || OpStatus::IsError(s))
	{
		OP_DELETE(skin);
		return NULL;
	}
	else
	{
		TRAP(err, skin_version = skin->GetPrefsFile()->ReadIntL("Info", "Version"));

		// we only support this single version of the persona system. We need to enforce it
		// to avoid abuse of the version field as seen with skins
		if (skin_version > CURRENT_PERSONA_VERSION_NUMBER || !skin_version)
		{
			OP_DELETE(skin);
			skin = NULL;
		}
		else 
		{
			skin->SetSkinVersion(skin_version);
		}
	}
	return skin;
}
#endif // PERSONA_SKIN_SUPPORT

UINT32 OpSkinManager::GetSystemColor(OP_SYSTEM_COLOR color)
{
	switch(color)
	{
		case OP_SYSTEM_COLOR_TEXT_SELECTED:
			if (m_selected_text_color == REALLY_UGLY_COLOR)
				break;
			return m_selected_text_color;
		case OP_SYSTEM_COLOR_BACKGROUND_SELECTED:
			if (m_selected_text_bgcolor == REALLY_UGLY_COLOR)
				break;
			return m_selected_text_bgcolor;
		case OP_SYSTEM_COLOR_TEXT_SELECTED_NOFOCUS:
			if (m_selected_text_color_nofocus == REALLY_UGLY_COLOR)
				break;
			return m_selected_text_color_nofocus;
		case OP_SYSTEM_COLOR_BACKGROUND_SELECTED_NOFOCUS:
			if (m_selected_text_bgcolor_nofocus == REALLY_UGLY_COLOR)
				break;
			return m_selected_text_bgcolor_nofocus;
		default: break;
	}
	return g_op_ui_info->GetSystemColor(color);
//	return g_pcfontscolors->GetColor(color);
}

INT32 OpSkinManager::GetOptionValue(const char* option, INT32 def_value)
{
	if (m_skin)
	{
		return m_skin->GetOptionValue(option, def_value);
	}

	if (m_defaultskin)
	{
		return m_defaultskin->GetOptionValue(option, def_value);
	}

	return def_value;
}

OP_STATUS OpSkinManager::DrawElement(VisualDevice* vd, const char* name, const OpPoint& point, INT32 state, INT32 hover_value, const OpRect* clip_rect, SkinType type, SkinSize size, BOOL foreground)
{
	OpSkinElement* element = GetSkinElement(name, type, size, foreground);

	if (!element)
		return OpStatus::ERR;

	return element->Draw(vd, point, state, hover_value, clip_rect);
}

OP_STATUS OpSkinManager::DrawElement(VisualDevice* vd, const char* name, const OpRect& rect, INT32 state, INT32 hover_value, const OpRect* clip_rect, SkinType type, SkinSize size, BOOL foreground)
{
	OpSkinElement* element = GetSkinElement(name, type, size, foreground);

	if (!element)
		return OpStatus::ERR;

	return element->Draw(vd, rect, state, hover_value, clip_rect);
}

OP_STATUS OpSkinManager::GetSize(const char* name, INT32* width, INT32* height, INT32 state, SkinType type, SkinSize size, BOOL foreground)
{
	OpSkinElement* element = GetSkinElement(name, type, size, foreground);

	*width = *height = 0;

	if (!element)
		return OpStatus::ERR;

	return element->GetSize(width, height, state);
}

OP_STATUS OpSkinManager::GetMargin(const char* name, INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state, SkinType type, SkinSize size, BOOL foreground)
{
	OpSkinElement* element = GetSkinElement(name, type, size, foreground);

	*left = *top = *right = *bottom = SKIN_DEFAULT_MARGIN;

	if (!element)
		return OpStatus::ERR;

	return element->GetMargin(left, top, right, bottom, state);
}

OP_STATUS OpSkinManager::GetPadding(const char* name, INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state, SkinType type, SkinSize size, BOOL foreground)
{
	OpSkinElement* element = GetSkinElement(name, type, size, foreground);

	*left = *top = *right = *bottom = SKIN_DEFAULT_PADDING;

	if (!element)
		return OpStatus::ERR;

	return element->GetPadding(left, top, right, bottom, state);
}

OP_STATUS OpSkinManager::GetSpacing(const char* name, INT32* spacing, INT32 state, SkinType type, SkinSize size, BOOL foreground)
{
	OpSkinElement* element = GetSkinElement(name, type, size, foreground);

	*spacing = SKIN_DEFAULT_SPACING;

	if (!element)
		return OpStatus::ERR;

	return element->GetSpacing(spacing, state);
}

OP_STATUS OpSkinManager::GetBackgroundColor(const char* name, UINT32* color, INT32 state, SkinType type, SkinSize size, BOOL foreground)
{
	OpSkinElement* element = GetSkinElement(name, type, size, foreground);

	if (!element)
		return OpStatus::ERR;

	return element->GetBackgroundColor(color, state);
}

OP_STATUS OpSkinManager::GetTextColor(const char* name, UINT32* color, INT32 state, SkinType type, SkinSize size, BOOL foreground)
{
	OpSkinElement* element = GetSkinElement(name, type, size, foreground);

	if (!element)
		return OpStatus::ERR;

	return element->GetTextColor(color, state);
}

OP_STATUS OpSkinManager::GetTextBold(const char* name, BOOL* bold, INT32 state, SkinType type, SkinSize size, BOOL foreground)
{
	OpSkinElement* element = GetSkinElement(name, type, size, foreground);

	if (!element)
		return OpStatus::ERR;

	return element->GetTextBold(bold, state);
}

OP_STATUS OpSkinManager::GetTextUnderline(const char* name, BOOL* underline, INT32 state, SkinType type, SkinSize size, BOOL foreground)
{
	OpSkinElement* element = GetSkinElement(name, type, size, foreground);

	if (!element)
		return OpStatus::ERR;

	return element->GetTextUnderline(underline, state);
}

OP_STATUS OpSkinManager::GetTextZoom(const char* name, BOOL* zoom, INT32 state, SkinType type, SkinSize size, BOOL foreground)
{
	OpSkinElement* element = GetSkinElement(name, type, size, foreground);

	if (!element)
		return OpStatus::ERR;

	return element->GetTextZoom(zoom, state);
}

OpSkinElement* OpSkinManager::GetSkinElement(const char* name, SkinType type, SkinSize size, BOOL foreground)
{
	if (!name)
		return NULL;

	OpSkinElement* element = GetSkinElement(m_skin, name, type, size);

	if (element)
		return element;

	element = GetSkinElement(m_defaultskin, name, type, size);
#ifdef SKIN_ELEMENT_FALLBACK_DISABLE
	// don't fall back to this element if the version of the skin is lower than the given fallback element version
	if(element && m_skin && element->GetFallbackVersion() > m_skin->GetSkinVersion())
	{
		return NULL;
	}
#endif // SKIN_ELEMENT_FALLBACK_DISABLE

	return element;
}

OpSkinElement* OpSkinManager::GetSkinElement(OpSkin* skin, const char* name, SkinType type, SkinSize size, BOOL foreground)
{
	if (!skin)
		return NULL;

	OpSkinElement* element = NULL;

	if (size != SKINSIZE_DEFAULT)
	{
		if (type != SKINTYPE_DEFAULT)
		{
			element = skin->GetSkinElement(name, type, size);

			if (element)
				return element;
		}

		element = skin->GetSkinElement(name, SKINTYPE_DEFAULT, size);

		if (element)
			return element;
	}

	if (type != SKINTYPE_DEFAULT)
	{
		element = skin->GetSkinElement(name, type, SKINSIZE_DEFAULT);

		if (element)
			return element;
	}

	return skin->GetSkinElement(name, SKINTYPE_DEFAULT, SKINSIZE_DEFAULT);
}

#ifdef ANIMATED_SKIN_SUPPORT

void OpSkinManager::SetAnimationListener(const char *name, OpSkinAnimationListener *listener)
{
	OpSkinElement* element = GetSkinElement(m_skin, name);
	if(!element)
	{
		element = GetSkinElement(m_defaultskin, name);
	}
	if(element)
	{
		element->SetAnimationListener(listener);
	}
}

void OpSkinManager::RemoveAnimationListener(const char *name, OpSkinAnimationListener *listener)
{
	OpSkinElement* element = GetSkinElement(m_skin, name);
	if(!element)
	{
		element = GetSkinElement(m_defaultskin, name);
	}
	if(element)
	{
		element->RemoveAnimationListener(listener);
	}
}

void OpSkinManager::StartAnimation(BOOL start, const char *name, VisualDevice *vd, BOOL animate_from_beginning)
{
	OpSkinElement* element = GetSkinElement(m_skin, name);
	if(!element)
	{
		element = GetSkinElement(m_defaultskin, name);
	}
	if(element)
	{
		element->StartAnimation(start, vd, animate_from_beginning);
	}
}

#endif // ANIMATED_SKIN_SUPPORT

void OpSkinManager::BroadcastSkinLoaded(OpSkin *skin)
{
	for (OpListenersIterator iterator(m_skin_notification_listeners); m_skin_notification_listeners.HasNext(iterator);)
	{
		m_skin_notification_listeners.GetNext(iterator)->OnSkinLoaded(skin);
	}
}

#ifdef PERSONA_SKIN_SUPPORT
void OpSkinManager::BroadcastPersonaLoaded(OpPersona *skin)
{
	for (OpListenersIterator iterator(m_skin_notification_listeners); m_skin_notification_listeners.HasNext(iterator);)
	{
		m_skin_notification_listeners.GetNext(iterator)->OnPersonaLoaded(skin);
	}
}

#endif // PERSONA_SKIN_SUPPORT

OpSkinManager::SkinFileType OpSkinManager::GetSkinFileType(const OpStringC& skinfile)
{
	OpFile file;
	if(OpStatus::IsSuccess(file.Construct(skinfile)))
	{
#ifdef SKIN_ZIP_SUPPORT
		if (OpZip::IsFileZipCompatible(&file))
		{
			OpZip zip;
			OpString ini_file;

			if(OpStatus::IsSuccess(ini_file.Set(UNI_L("skin.ini"))) && 
				OpStatus::IsSuccess(zip.Open(skinfile, FALSE /* read-only */)))
			{
				OpAutoPtr<OpMemFile> skinfile(zip.CreateOpZipFile(&ini_file));
				if (skinfile.get())
				{
					return OpSkinManager::SkinNormal;
				}
#ifdef PERSONA_SKIN_SUPPORT
				if(OpStatus::IsSuccess(ini_file.Set(UNI_L("persona.ini"))))
				{
					OpAutoPtr<OpMemFile> personafile(zip.CreateOpZipFile(&ini_file));
					if (personafile.get())
					{
						return OpSkinManager::SkinPersona;
					}
				}
#endif // PERSONA_SKIN_SUPPORT
			}
		}
		else
#endif // SKIN_ZIP_SUPPORT
		{
			// we support pointing the prefs directly at a .ini file inside a directory, so
			// handle that here
			if(skinfile.Find(UNI_L("skin.ini")) > -1)
			{
				return OpSkinManager::SkinNormal;
			}
#ifdef PERSONA_SKIN_SUPPORT
			else if(skinfile.Find(UNI_L("persona.ini")) > -1)
			{
				return OpSkinManager::SkinPersona;
			}
#endif // PERSONA_SKIN_SUPPORT
		}
	}
	return OpSkinManager::SkinUnknown;
}

void OpSkinManager::TrigBackgroundCleared(OpPainter* painter, const OpRect& r)
{
	for(UINT32 i = 0; i < m_transparent_background_listeners.GetCount(); i++)
	{
		m_transparent_background_listeners.Get(i)->OnBackgroundCleared(painter, r);
	}
}

#endif // SKIN_SUPPORT
