#include "core/pch.h"

//Core header files
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/util/opstrlst.h"
#include "modules/util/opfile/opfile.h"
#include "modules/stdlib/util/opdate.h"

//Platform header files
#include "adjunct/quick/tips/TipsConfig.h"

/*Tips' global tweaks*/
#define	TIPS_GLOBAL_SETTINGS_SECTION		UNI_L("Global Settings")
#define TIPS_ENABLED						UNI_L("Enabled")
#define TIPS_DISPLAY_INTERVAL				UNI_L("Display Interval")
#define	TIPS_RECURRENCE_INTERVAL			UNI_L("Recurrence Interval")
#define TIPS_NEVER_TURNED_OFF				UNI_L("Never Turned Off")

/* Tips' preference information*/
#define TIPS_METADATA_CONFIG_FILE			UNI_L("tips_metadata.ini")
#define TIPS_GENERAL_CONFIG_FILE			UNI_L("tips.ini")
#define TIP_STRING_ID						UNI_L("Tip")
#define TIP_ENABLED							UNI_L("Enabled")
#define TIP_LAST_APPEARED					UNI_L("Last Appeared")
#define TIP_FREQUENCY						UNI_L("Frequency")

TipsConfigGeneralInfo::TipsConfigGeneralInfo()
	: m_enabled(TRUE) 
	, m_last_appeared(0) 
{ 

}

BOOL
TipsConfigGeneralInfo::CanTipBeDisplayed()
{
	double present_day = OpDate::Day(OpDate::GetCurrentUTCTime());
	double last_appeared_day  = OpDate::Day(GetTipLastAppearance()*1000.0);
	UINT32 tips_recurrence_lag = TipsConfigManager::GetInstance()->GetTipsRecurrenceInterval();

	if (IsTipEnabled() && present_day - last_appeared_day > tips_recurrence_lag)
		return TRUE;

	return FALSE;
}

void 
TipsConfigGeneralInfo::UpdateTipsLastDisplayedInfo()
{
	m_last_appeared = OpDate::GetCurrentUTCTime()/1000;
}

TipsConfigGeneralInfo& 
TipsConfigGeneralInfo::operator=(const TipsConfigGeneralInfo& info)
{
	if (this == &info)
		return *this;

	m_enabled		=  info.m_enabled;
	m_message_id	=  info.m_message_id;
	m_frequency		=  info.m_frequency;
	m_last_appeared	=  info.m_last_appeared;
	m_section_name.Set(info.m_section_name);	//TODO: Error handling.

	return *this;
}

TipsConfigGeneralInfo& 
TipsConfigGeneralInfo::operator=(TipsConfigMetaInfo& info)
{
	m_message_id	=  info.GetTipStringID();
	m_frequency		=  info.GetTipAppearanceFrequency();

	if (m_section_name.IsEmpty())
		m_section_name.Set(info.GetSectionName());	//TODO: Error handling.

	return *this;
}

void
TipsConfigMetaInfo::TipsGlobalTweaks::LoadConfiguration(PrefsFile &sd_file, const OpString &section)
{
	m_is_tips_system_enabled = sd_file.ReadIntL(section,TIPS_ENABLED, m_is_tips_system_enabled);
	if (m_is_tips_system_enabled < 0)
		m_is_tips_system_enabled = 0;

	const int interval = sd_file.ReadIntL(section,TIPS_DISPLAY_INTERVAL, m_tips_display_interval) * 1000; //Convert into millisecond;
	m_tips_display_interval = interval > 0 ? interval : DEFAULT_TIPS_DISPLAY_INTERVAL * 1000;

	const int lag = sd_file.ReadIntL(section,TIPS_RECURRENCE_INTERVAL, m_tips_recurrence_lag);
	m_tips_recurrence_lag = lag > 0 ? lag : DEFAULT_RECURRENCE_INTERVAL;

	m_is_tips_never_off = sd_file.ReadIntL(section,TIPS_NEVER_TURNED_OFF, m_is_tips_never_off);
	if (m_is_tips_never_off < 0 )
		m_is_tips_never_off = 0;
}

TipsConfigManager::TipsConfigManager()
	: m_tips_configmanager(NULL)
{

}

TipsConfigManager::~TipsConfigManager()
{
	SaveConfiguration();	
}

OP_STATUS
TipsConfigManager::LoadConfiguration()
{
	/* Load settings from 'tips_metadata.ini' file */
	OpStringC tips_meta_file = g_pcui->GetStringPref(PrefsCollectionUI::TipsConfigMetaFile);

	OpFile tips_settings_inalterable_file;
	if (tips_meta_file.IsEmpty() || (tips_meta_file.HasContent() && OpStatus::IsError(tips_settings_inalterable_file.Construct(tips_meta_file))) )
	{
		RETURN_IF_ERROR(tips_settings_inalterable_file.Construct(TIPS_METADATA_CONFIG_FILE, OPFILE_INI_FOLDER));
	}
	
	OpAutoVector<TipsConfigMetaInfo> tips_meta_configdata;	// Array holding the individual tips information
	RETURN_IF_ERROR(LoadConfiguration(tips_settings_inalterable_file, tips_meta_configdata));

	/* Load settings from 'tips.ini files */
	OpStringC tips_file = g_pcui->GetStringPref(PrefsCollectionUI::TipsConfigFile);
	
	OpFile tips_settings_alterable_file;
	if (tips_file.IsEmpty() || (tips_file.HasContent() && OpStatus::IsError(tips_settings_alterable_file.Construct(tips_file))) )
	{
		RETURN_IF_ERROR(tips_settings_alterable_file.Construct(TIPS_GENERAL_CONFIG_FILE, OPFILE_HOME_FOLDER));
	}
	
	OpAutoVector<TipsConfigGeneralInfo>	tips_general_configdata;	// Array holding the individual tips information
	RETURN_IF_ERROR(LoadConfiguration(tips_settings_alterable_file, tips_general_configdata));

	/* Merge two different information into one */
	RETURN_IF_ERROR(CreateGlobalTipsInfo(tips_meta_configdata, tips_general_configdata));

	return OpStatus::OK;
}

OP_STATUS
TipsConfigManager::SaveConfiguration()
{
	OpFile tips_settings_alterable_file;
	RETURN_IF_ERROR(tips_settings_alterable_file.Construct(TIPS_GENERAL_CONFIG_FILE, OPFILE_HOME_FOLDER));

	return SaveConfiguration(tips_settings_alterable_file, m_tips_general_configdata);
}

OP_STATUS
TipsConfigManager::LoadConfiguration(const OpFile &tips_configfile , OpVector<TipsConfigGeneralInfo> &prefs_list)
{
	/* Construct and loads up the tips.ini pref file */
	PrefsFile sd_file(PREFS_STD);
	RETURN_IF_LEAVE(sd_file.ConstructL());
	RETURN_IF_LEAVE(sd_file.SetFileL(&tips_configfile));
	RETURN_IF_LEAVE(OpStatus::IsSuccess(sd_file.LoadAllL()));

	/* Read all sections */
	OpString_list section_list;
	RETURN_IF_LEAVE(sd_file.ReadAllSectionsL(section_list));
	
	for (UINT32 i = 0; i < section_list.Count(); i++)
	{
		OpAutoPtr<TipsConfigGeneralInfo> info(OP_NEW(TipsConfigGeneralInfo, ()));
		RETURN_OOM_IF_NULL(info.get());

		RETURN_IF_ERROR(info->SetSectionName(section_list[i]));
		RETURN_IF_LEAVE(info->EnableTip(sd_file.ReadIntL(section_list[i], TIP_ENABLED, 0)));
		RETURN_IF_LEAVE(info->SetLastAppeared(sd_file.ReadIntL(section_list[i],TIP_LAST_APPEARED, 0)));

		RETURN_IF_ERROR(prefs_list.Add(info.get()));
		info.release();
	}

	return OpStatus::OK;
}

OP_STATUS
TipsConfigManager::SaveConfiguration(const OpFile &tips_configfile , const OpVector<TipsConfigGeneralInfo> &prefs_list) const
{
	/* Construct tips.ini pref file */
	PrefsFile sd_file(PREFS_STD);
	RETURN_IF_LEAVE(sd_file.ConstructL());
	RETURN_IF_LEAVE(sd_file.SetFileL(&tips_configfile));
	RETURN_IF_LEAVE(OpStatus::IsSuccess(sd_file.LoadAllL()));
	sd_file.DeleteAllSectionsL();

	for (UINT32 index = 0 ; index < prefs_list.GetCount() ; index++)
	{
		TipsConfigGeneralInfo *info = prefs_list.Get(index);
		RETURN_IF_LEAVE(sd_file.WriteIntL(info->GetSectionName(), TIP_ENABLED, info->IsTipEnabled()));
		RETURN_IF_LEAVE(sd_file.WriteIntL(info->GetSectionName(), TIP_LAST_APPEARED, info->GetTipLastAppearance()));
	}

	RETURN_IF_LEAVE(sd_file.CommitL(TRUE));
	
	return OpStatus::OK;
}

OP_STATUS
TipsConfigManager::LoadConfiguration(const OpFile &tips_configfile , OpVector<TipsConfigMetaInfo> &prefs_list)
{
	/* Construct and load up the tips_metadata.ini pref file */
	PrefsFile sd_file(PREFS_STD);
	RETURN_IF_LEAVE(sd_file.ConstructL());
	RETURN_IF_LEAVE(sd_file.SetFileL(&tips_configfile));
	RETURN_IF_LEAVE(OpStatus::IsSuccess(sd_file.LoadAllL()));
	
	/* Read all sections */
	OpString_list section_list;
	RETURN_IF_LEAVE(sd_file.ReadAllSectionsL(section_list));

	/* Read general sections */
	for (UINT32 i = 0; i < section_list.Count(); i++)
	{
		if (section_list[i].CompareI(TIPS_GLOBAL_SETTINGS_SECTION) == 0)
		{
			m_tips_global_tweaks.LoadConfiguration(sd_file, section_list[i]);
			continue;
		}

		OpAutoPtr<TipsConfigMetaInfo> info(OP_NEW(TipsConfigMetaInfo, ()));
		RETURN_OOM_IF_NULL(info.get());

		RETURN_IF_ERROR(info->SetSectionName(section_list[i]));

		OpStringC tips_str_id = sd_file.ReadStringL(section_list[i], TIP_STRING_ID, UNI_L(""));
		if (tips_str_id.IsEmpty())
			return OpStatus::ERR;
		
		info->SetTipStringID(Str::LocaleString(tips_str_id));
		RETURN_IF_LEAVE(info->SetTipAppearanceFrequency(sd_file.ReadIntL(section_list[i],TIP_FREQUENCY, 0)));

		RETURN_IF_ERROR(prefs_list.Add(info.get()));
		info.release();
	}

	return OpStatus::OK;
}

OP_STATUS
TipsConfigManager::GetTipsConfigData(const OpStringC16& tip_name, TipsConfigGeneralInfo &tip_info)
{
	RETURN_IF_ERROR(!tip_name.IsEmpty());

	OP_STATUS status = OpStatus::ERR;
	for (UINT32 index = 0 ; index < m_tips_general_configdata.GetCount() ; index++)
	{
		if (uni_stricmp(m_tips_general_configdata.Get(index)->GetSectionName(), tip_name) == 0)
		{
			tip_info = *m_tips_general_configdata.Get(index);
			status = OpStatus::OK;
			break;
		}
	}

	return status;
}

OP_STATUS
TipsConfigManager::UpdateTipsLastDisplayedInfo(const TipsConfigGeneralInfo &tip_info)
{
	for (UINT32 index = 0 ; index < m_tips_general_configdata.GetCount() ; index++)
	{
		if (uni_stricmp(m_tips_general_configdata.Get(index)->GetSectionName(), tip_info.GetSectionName()) == 0)
		{
			m_tips_general_configdata.Get(index)->UpdateTipsLastDisplayedInfo();
			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

OP_STATUS
TipsConfigManager::CreateGlobalTipsInfo(const OpVector<TipsConfigMetaInfo> &meta_list, const OpVector<TipsConfigGeneralInfo> &general_list)
{
	UINT32 meta_list_count = meta_list.GetCount();
	UINT32 general_list_count = general_list.GetCount();
	for (UINT32 meta_index = 0 ; meta_index < meta_list_count ; meta_index++)
	{
		INT32 found_index = -1;
		for (UINT32 general_index = 0 ; general_index < general_list_count ; general_index++)
		{
			if (uni_stricmp(meta_list.Get(meta_index)->GetSectionName(), general_list.Get(general_index)->GetSectionName()) == 0)
			{
				found_index = general_index;
				break;
			}
		}

		OpAutoPtr<TipsConfigGeneralInfo> info(OP_NEW(TipsConfigGeneralInfo, ()));
		RETURN_OOM_IF_NULL(info.get());

		TipsConfigGeneralInfo *tmp_info = info.get();
		if (found_index != -1)
		{
			*tmp_info = *general_list.Get(found_index) = *meta_list.Get(meta_index); 
		}
		else
		{
			*tmp_info = *meta_list.Get(meta_index); 
		}

		RETURN_IF_ERROR(m_tips_general_configdata.Add(tmp_info));
		info.release();
	}
	return OpStatus::OK;
}

