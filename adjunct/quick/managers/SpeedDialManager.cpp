// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2006 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//
#include "core/pch.h"

#ifdef SUPPORT_SPEED_DIAL

#include "adjunct/quick/managers/SpeedDialManager.h"

#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/widgets/OpSpeedDialView.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/speeddial/SpeedDialThumbnail.h"
#ifdef USE_COMMON_RESOURCES
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#endif // USE_COMMON_RESOURCES
#include "adjunct/desktop_util/resources/ResourceSetup.h"

#include "adjunct/desktop_util/widget_utils/WidgetThumbnailGenerator.h"
#include "adjunct/desktop_util/adt/hashiterator.h"

#include "modules/util/opfile/opfile.h"

#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/thumbnails/src/thumbnail.h"
#include "modules/widgets/WidgetContainer.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/util/opstrlst.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"

#ifdef USE_COMMON_RESOURCES
#define SDM_SPEED_DIAL_FILENAME    DESKTOP_RES_STD_SPEEDDIAL
#else
#define SDM_SPEED_DIAL_FILENAME    UNI_L("speeddial_default.ini")
#endif // USE_COMMON_RESOURCES

// Defines number of milliseconds we should wait with reloading all speed dial cells after zoom is done with ctrl + mouse wheel
// It's neeeded because of performance issues
#define MILLISECONDS_TO_WAIT_WITH_RELOAD 500 // half of a second

const double SpeedDialManager::DefaultZoomLevel = 1;
const double SpeedDialManager::MinimumZoomLevel = 0.3;
const double SpeedDialManager::MaximumZoomLevel = 2;
const double SpeedDialManager::MinimumZoomLevelForAutoZooming = 0.6;
const double SpeedDialManager::MaximumZoomLevelForAutoZooming = 1.3;
const double SpeedDialManager::ScaleDelta = 0.1;


////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SpeedDialManager
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////


SpeedDialManager::SpeedDialManager() :
	m_state(Disabled),
	m_speed_dial_tabs_count(0),
	m_widget_window(NULL),
	m_image_layout(OpImageBackground::BEST_FIT),
	m_background_enabled(false),
	m_ui_opacity(0),
	m_scale(DefaultZoomLevel),
	m_scaling(false),
	m_force_reload(false),
	m_undo_animating_in_speed_dial(NULL),
	m_is_dirty(false),
	m_has_loaded_config(false)
{
	TRAPD(rc, g_pcui->RegisterListenerL(this));
	m_zoom_timer.SetTimerListener(this);
	
	int prefs_scale = g_pcui->GetIntegerPref(PrefsCollectionUI::SpeedDialZoomLevel);

	m_automatic_scale = prefs_scale == 0;
	if (!m_automatic_scale)
		m_scale = prefs_scale * 0.01;

	// set up the delayed trigger with a delay just so it happens when the call stack has unwound. The delay means less
	// in reality as it will trigger immediately when the calling code enters the message loop again, unless
	// explicitly aborted by a CancelUpdate() call. 
	m_delayed_trigger.SetDelayedTriggerListener(this);
	m_delayed_trigger.SetTriggerDelay(10, 10);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

SpeedDialManager::~SpeedDialManager()
{
	// give speed dials last chance to set dirty flag
	for	(UINT32 i = 0; i < m_speed_dials.GetCount(); ++i)
	{
		DesktopSpeedDial *sd = m_speed_dials.Get(i);
		sd->OnShutDown();
	}

	if(m_is_dirty)
	{
		OpStatus::Ignore(Save());
	}
	m_deleted_ids.DeleteAll();

	g_thumbnail_manager->Flush();
	g_pcui->UnregisterListener(this);

	if(m_widget_window)
	{
		m_widget_window->Close(TRUE);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SpeedDialManager::Init()
{
	// Handle Kiosk mode
	if (KioskManager::GetInstance()->GetEnabled())
	{
		if (KioskManager::GetInstance()->GetKioskSpeeddial())
			SetState(SpeedDialManager::ReadOnly);
		else
			SetState(SpeedDialManager::Disabled);
	}

	// Configure the core thumbnail manager with regard to the boundary sizes, zoom levels and view-mode: minimized
	// thumbnails behavior.
	ConfigureThumbnailManager();

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SpeedDialManager::SetImageLayout(OpImageBackground::Layout layout)
{ 
	bool changed = m_image_layout != layout;

	m_image_layout = layout; 

	if(changed)
	{
		UINT32 i;

		for(i = 0; i < m_config_listeners.GetCount(); i++)
		{
			m_config_listeners.Get(i)->OnSpeedDialBackgroundChanged();
		}
	}
}

void SpeedDialManager::ConfigureThumbnailManager()
{
	// Configure the core thumbnail manager with regard to the boundary sizes, zoom levels and view-mode: minimized
	// thumbnails behavior.
	int x_ratio = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ThumbnailAspectRatioX);
	int y_ratio = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ThumbnailAspectRatioY);
	double aspect_ratio = ((double)x_ratio) / y_ratio;

	int base_width  =        THUMBNAIL_WIDTH;
	int base_height = double(THUMBNAIL_WIDTH) / aspect_ratio;

	g_thumbnail_manager->SetThumbnailSize(base_width, base_height, MaximumZoomLevel);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SpeedDialManager::ReloadAll(ReloadPurpose purpose)
{
	UINT32 i;

	for (i = 0; i < m_speed_dials.GetCount(); i++)
	{
		if (m_speed_dials.Get(i) && m_speed_dials.Get(i)->GetURL().HasContent())
		{
			// Readd the thumbnail ref's if they were purged away
			if (PurposePurge == purpose)
				m_speed_dials.Get(i)->ReAddThumnailRef();

			// SetExpired casues a full reload from core, don't do that if we only want to zoom.
			// The core will the use the bitmap cache.
			if (PurposeZoom != purpose)
				m_speed_dials.Get(i)->SetExpired();
			else
				m_speed_dials.Get(i)->Zoom();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

int SpeedDialManager::GetSpeedDialPositionFromURL(const OpStringC& url) const
{
	int i;
	for (i = 0; i < (int)m_speed_dials.GetCount(); i++)
	{
		DesktopSpeedDial* sd = m_speed_dials.Get(i);
		if (sd)
		{
			OpStringC sd_url(sd->GetURL());
			OpString tmp;
			OpString tmp2;
			if (g_url_api->ResolveUrlNameL(sd_url, tmp) 
				&& g_url_api->ResolveUrlNameL(url, tmp2)
				&& tmp.CompareI(tmp2) == 0)
			{
				return i;
			}
		}
	}
	return -1;
}

bool SpeedDialManager::SpeedDialExists(const OpStringC& url)
{
	return GetSpeedDialPositionFromURL(url) >= 0;
}

const DesktopSpeedDial* SpeedDialManager::GetSpeedDialByUrl(const OpStringC& url) const
{
	INT32 position = GetSpeedDialPositionFromURL(url);
	if (position < 0)
	{
		return NULL;
	}
	return m_speed_dials.Get(position);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SpeedDialManager::InsertSpeedDial(int pos, const SpeedDialData* sd, bool use_undo)
{
	if (pos < 0 || pos > int(m_speed_dials.GetCount()) - 1)
	{
		OP_ASSERT(false);
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	OpAutoPtr<DesktopSpeedDial> new_sd(OP_NEW(DesktopSpeedDial, ()));
	RETURN_OOM_IF_NULL(new_sd.get());
	RETURN_IF_ERROR(new_sd->Set(*sd));

	RETURN_IF_ERROR(m_speed_dials.Insert(pos, new_sd.get()));
	new_sd->OnAdded();
	NotifySpeedDialAdded(*new_sd.release());
	if(use_undo)
	{
		RETURN_IF_ERROR(m_undo_stack.AddInsert(pos));
	}
	// The speed dials following the one just removed changed positions.
	for (int i = pos + 1; i < (int)m_speed_dials.GetCount(); ++i)
		m_speed_dials.Get(i)->OnPositionChanged();

	PostSaveRequest();

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
OP_STATUS SpeedDialManager::RemoveSpeedDial(int pos, bool use_undo)
{
	if (pos < 0 || pos >= (int)m_speed_dials.GetCount())
	{
		OP_ASSERT(!"Don't know this DesktopSpeedDial");
		return OpStatus::ERR;
	}
	DesktopSpeedDial* sd = m_speed_dials.Get(pos);

	if (sd->GetPartnerID().HasContent())
	{
		// deleting speed dials with a partner id, store the id so it's not re-added later
		RETURN_IF_ERROR(AddToDeletedIDs(sd->GetPartnerID().CStr(), true));
	}
	sd->OnRemoving();
	NotifySpeedDialRemoving(*sd);

	if (use_undo)
	{
		RETURN_IF_ERROR(m_undo_stack.AddRemove(pos));
	}
	m_speed_dials.Delete(pos);

	// The speed dials following the one just removed changed positions.
	for (int i = pos; i < (int)m_speed_dials.GetCount(); ++i)
		m_speed_dials.Get(i)->OnPositionChanged();

	PostSaveRequest();

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SpeedDialManager::ReplaceSpeedDial(int pos, const SpeedDialData* new_sd, bool from_sync /*=false*/)
{
	if (pos < 0 || pos >= (int)m_speed_dials.GetCount())
	{
		OP_ASSERT(!"Don't know DesktopSpeedDial at pos");
		return OpStatus::ERR;
	}

	if (!DocumentView::IsUrlRestrictedForViewFlag(new_sd->GetURL().CStr(), DocumentView::ALLOW_ADD_TO_SPEED_DIAL))
		return OpStatus::ERR;

	DesktopSpeedDial* old_sd = m_speed_dials.Get(pos);

	if (old_sd == new_sd)
		return OpStatus::OK;

	const bool replacing_plus = pos == int(m_speed_dials.GetCount()) - 1;

	if (!replacing_plus)
		RETURN_IF_ERROR(m_undo_stack.AddReplace(pos));

	OpAutoPtr<DesktopSpeedDial> new_sd_copy(OP_NEW(DesktopSpeedDial, ()));
	RETURN_OOM_IF_NULL(new_sd_copy.get());
	RETURN_IF_ERROR(new_sd_copy->Set(*new_sd));

	// Skip this when processing sync item. Server will send DATAITEM_SPEEDDIAL_2_SETTINGS
	// if this speed dial must be blacklisted.
	if (old_sd->GetPartnerID().HasContent() && !from_sync)
	{
		// Add the speed dial partner id to the deleted section if we 
		// change the URL from one that had a partner id, so it won't be added again.
		if (old_sd->GetURL() != new_sd_copy.get()->GetURL() ||
			// or if user edited title (DSK-355419)
			(new_sd_copy->IsCustomTitle() && new_sd_copy->GetTitle() != old_sd->GetTitle()))
		{
			RETURN_IF_ERROR(AddToDeletedIDs(old_sd->GetPartnerID().CStr(), true));
		}
	}

	RETURN_IF_ERROR(m_speed_dials.Replace(pos, new_sd_copy.get()));
	old_sd->OnRemoving();
	new_sd_copy->OnAdded();
	NotifySpeedDialReplaced(*old_sd, *new_sd_copy.release());

	OP_DELETE(old_sd);

	// The "adder" must never disappear.
	if (replacing_plus)
	{
		OpAutoPtr<DesktopSpeedDial> adder(OP_NEW(DesktopSpeedDial, ()));
		RETURN_OOM_IF_NULL(adder.get());
		RETURN_IF_ERROR(m_speed_dials.Add(adder.get()));
		adder->OnAdded();
		NotifySpeedDialAdded(*adder.release());
		RETURN_IF_ERROR(m_undo_stack.AddInsert(pos));
	}
	PostSaveRequest();

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SpeedDialManager::MoveSpeedDial(int from_pos, int to_pos)
{
	if (to_pos < 0 || from_pos < 0)
	{
		OP_ASSERT(!"Can't move DesktopSpeedDials like that");
		return OpStatus::ERR;
	}

	if (to_pos == from_pos)
		return OpStatus::OK;

	DesktopSpeedDial* tmp_sd = m_speed_dials.Remove(from_pos);
	m_speed_dials.Insert(to_pos, tmp_sd);

	NotifySpeedDialMoved(from_pos, to_pos);
	RETURN_IF_ERROR(m_undo_stack.AddMove(from_pos, to_pos));

	// These speed dials just changed positions.
	for (int pos = min(from_pos, to_pos); pos <= max(from_pos, to_pos); pos++)
	{
		m_speed_dials.Get(pos)->OnPositionChanged();
	}

	PostSaveRequest();

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

SpeedDialManager::State SpeedDialManager::GetState() const
{ 
	// In kiosk mode don't save the preference just hold it internally
	if (KioskManager::GetInstance()->GetEnabled())
		return m_state;

	return static_cast<State>(g_pcui->GetIntegerPref(PrefsCollectionUI::SpeedDialState));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SpeedDialManager::SetState(State state)
{
	// In kiosk mode don't save the preference just hold it internally
	if (KioskManager::GetInstance()->GetEnabled())
		m_state = state;
	else
	{
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::SpeedDialState, state));
	}
}

bool SpeedDialManager::IsPlusButtonShown() const
{
	return g_pcui->GetIntegerPref(PrefsCollectionUI::ShowAddSpeedDialButton) != 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SpeedDialManager::Undo(OpSpeedDialView* sd)
{
	m_undo_animating_in_speed_dial = sd;
	OP_STATUS status = m_undo_stack.Undo(); // this will ultimately call AnimateThumbnailOut(...)
	m_undo_animating_in_speed_dial = NULL;

	return status;
}

OP_STATUS SpeedDialManager::Redo(OpSpeedDialView* sd)
{
	m_undo_animating_in_speed_dial = sd;
	OP_STATUS status = m_undo_stack.Redo(); // this will ultimately call AnimateThumbnailOut(...)
	m_undo_animating_in_speed_dial = NULL;

	return status;
}

void SpeedDialManager::AnimateThumbnailOutForUndoOrRedo(int pos)
{
	// This should only ever be called (indirectly) from Undo(...) or Redo(...).
	OP_ASSERT(m_undo_animating_in_speed_dial);
	if (!m_undo_animating_in_speed_dial)
		return;

	SpeedDialThumbnail* thumb = m_undo_animating_in_speed_dial->GetSpeedDialThumbnail(pos);

	thumb->AnimateThumbnailOut(false);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SpeedDialManager::IsAddressSpeedDialNumber(const OpStringC &address, OpString &speed_dial_address, OpString &speed_dial_title)
{
	for(int i=0; i<address.Length(); i++)
	{
		if (!op_isdigit(address[i]))
			return false;
	}

	if (address.HasContent())
	{
		UINT32 dial_number = uni_atoi(address.CStr());
		const DesktopSpeedDial* sd = GetSpeedDial(dial_number - 1);
		if (sd != NULL && sd->GetURL().HasContent())
		{
			speed_dial_address.Set(sd->GetURL());
			speed_dial_title.Set(sd->GetTitle());
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SpeedDialManager::AddConfigurationListener(SpeedDialConfigurationListener* listener)
{
	return m_config_listeners.Add(listener);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SpeedDialManager::RemoveConfigurationListener(SpeedDialConfigurationListener* listener)
{
	return m_config_listeners.RemoveByItem(listener);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SpeedDialManager::Load(bool copy_default_file)
{
	bool prefs_changed = false;
	OpFile speeddial_file;
	OpString layout;
	BOOL speeddialfile_exists = FALSE;
	bool has_default_file = false;
	bool add_extra_items = false;
	PrefsFile default_sd_file(PREFS_STD);

	m_has_loaded_config = true;

	RETURN_IF_LEAVE(g_pcfiles->GetFileL(PrefsCollectionFiles::SpeedDialFile, speeddial_file));

	RETURN_IF_ERROR(speeddial_file.Exists(speeddialfile_exists));

	// if it doesn't exist, it's easy and we just copy the file blindly over
	if(!speeddialfile_exists)
	{
		/* Copy over the default file if it exists and there is no current file and it's a clean install */
		if (copy_default_file)
		{
			OpFile speeddial_default_file;

			/* Create and check if the file exists; extra items will be added if default file is from custom folder (DSK-330957) */
			if (OpStatus::IsSuccess(ResourceSetup::GetDefaultPrefsFile(SDM_SPEED_DIAL_FILENAME, speeddial_default_file, NULL, &add_extra_items)))
			{
				/* Copy the file over */
				if (OpStatus::IsSuccess(speeddial_file.CopyContents(&speeddial_default_file, TRUE)))
				{
					/* Set the flag to exists on a successful copy */
					speeddialfile_exists = TRUE;
				}
			}
		}
	}
	// don't try any merging unless we're upgrading
	else if(g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRST_NEW_BUILD_NUMBER ||
		g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRSTCLEAN ||
		g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRST ||
		g_run_type->m_added_custom_folder ||
		g_region_info->m_changed)
	{
		// otherwise, we have a speed dial ini file already, so we need to merge stuff
		OpFile speeddial_default_file;

		/* Create and check if the file exists; extra items will be added if default file is from custom folder (DSK-330957) */
		has_default_file = !!OpStatus::IsSuccess(ResourceSetup::GetDefaultPrefsFile(SDM_SPEED_DIAL_FILENAME, speeddial_default_file, NULL, &add_extra_items));

		if(has_default_file)
		{
			OP_STATUS err;

			/* Construct and load up the default speeddial.ini pref file */
			TRAP(err, default_sd_file.ConstructL();
					default_sd_file.SetFileL(&speeddial_default_file);
					default_sd_file.LoadAllL());

			has_default_file = !!OpStatus::IsSuccess(err);
		}
	}

	// we also want extra items if region changed and we're upgrading from version < 11.60 (DSK-353188)
	if(has_default_file && g_region_info->m_changed)
	{
		OperaVersion eleven_sixty;
		RETURN_IF_ERROR(eleven_sixty.Set(UNI_L("11.60.01")));
		if (g_application->GetPreviousVersion() < eleven_sixty)
		{
			add_extra_items = true;
		}
	}


	/* If the file exists load it! */
	if (speeddialfile_exists)
	{
		OP_STATUS err;
		PrefsFile sd_file(PREFS_STD);

		/* Construct and load up the speeddial.ini pref file, fail on OOM */
		RETURN_IF_LEAVE(sd_file.ConstructL());
		RETURN_IF_LEAVE(sd_file.SetFileL(&speeddial_file));
		RETURN_IF_LEAVE(sd_file.LoadAllL());

		PrefsSection *deleted = NULL;

		// read the deleted entries, if any
		TRAP(err, deleted = sd_file.ReadSectionL(UNI_L(SPEEDDIAL_DELETED_ITEMS_SECTION)));

		OpAutoPtr<PrefsSection> section_ap(deleted);

		if(OpStatus::IsSuccess(err))
		{
			// the deleted IDs are just a flat list of IDs, so read them by key and put them in a table
			const PrefsEntry *ientry = deleted->Entries();
			while(ientry)
			{
				const uni_char *id = ientry->Key();
				if(id)
				{
					RETURN_IF_ERROR(AddToDeletedIDs(id, false));
				}
				ientry = (const PrefsEntry *) ientry->Suc();
			}

		}
		RETURN_IF_ERROR(MergeSpeedDialEntries(has_default_file, default_sd_file, sd_file, prefs_changed, add_extra_items));

		RETURN_IF_LEAVE(m_force_reload = sd_file.ReadIntL(SPEEDDIAL_VERSION_SECTION, SPEEDDIAL_KEY_FORCE_RELOAD, 0) != 0);
		RETURN_IF_LEAVE(m_background_image_name.SetL(sd_file.ReadStringL(SPEEDDIAL_IMAGE_SECTION, SPEEDDIAL_KEY_IMAGENAME)));
		RETURN_IF_LEAVE(layout.SetL(sd_file.ReadStringL(SPEEDDIAL_IMAGE_SECTION, SPEEDDIAL_KEY_IMAGELAYOUT));
						m_background_enabled = sd_file.ReadIntL(SPEEDDIAL_IMAGE_SECTION, SPEEDDIAL_KEY_IMAGE_ENABLED, false) ? true : false);

		if (m_background_image_name.HasContent())
		{
			OpFile background_image_file;

			//Ensure to unserialize the path.
			if (OpStatus::IsError(background_image_file.Construct(m_background_image_name, OPFILE_SERIALIZED_FOLDER)))
			{
				m_background_image_name.Empty();
			}
			else
			{
				RETURN_IF_LEAVE(m_background_image_name.SetL(background_image_file.GetFullPath()));
			}
		}

		// Opacity currently disabled as it's not working properly now
		// m_ui_opacity = max(0, sd_file.ReadIntL(SPEEDDIAL_UI_SECTION, SPEEDDIAL_UI_OPACITY, 0));
	}

	// add plus button which is not specified in file speeddial.ini
	OpAutoPtr<DesktopSpeedDial> sd(OP_NEW(DesktopSpeedDial, ()));
	RETURN_OOM_IF_NULL(sd.get());
	RETURN_IF_ERROR(m_speed_dials.Add(sd.get()));
	sd->OnAdded();
	sd.release();

	m_image_type = NONE;

	if(m_background_image_name.HasContent() && m_background_enabled)
	{
		RETURN_IF_ERROR(LoadBackgroundImage(m_background_image_name));
	}
	if(!layout.CompareI(UNI_L("center")))
	{
		m_image_layout = OpImageBackground::CENTER;
	}
	else if(!layout.CompareI(UNI_L("stretch")))
	{
		m_image_layout = OpImageBackground::STRETCH;
	}
	else if(!layout.CompareI(UNI_L("tile")))
	{
		m_image_layout = OpImageBackground::TILE;
	}
	else if (!layout.CompareI(UNI_L("crop")))
	{
		m_image_layout = OpImageBackground::BEST_FIT;
	}
	else
	{
		m_image_layout = OpImageBackground::BEST_FIT;
	}
	if(m_ui_opacity > 100)
	{
		m_ui_opacity = 0;
	}

	if (m_force_reload)
	{
		for (unsigned i = 0; i < m_speed_dials.GetCount(); i++)
		{
			OpStatus::Ignore(m_speed_dials.Get(i)->StartLoadingSpeedDial(TRUE, GetThumbnailWidth(), GetThumbnailHeight()));
		};
		/* I'd like to only reset m_force_reload if reloading actually
		 * was successful, but this is probably good enough.
		 */
		m_force_reload = false;
		prefs_changed = true;
	}
	OP_STATUS s = OpStatus::OK;
	if(prefs_changed)
	{
		s = Save();
	}
	SetDirty(false);
	NotifySpeedDialsLoaded();
	return s;
}

bool SpeedDialManager::IsInDeletedList(const OpStringC& partner_id)
{
	for(UINT32 i = 0; i < m_deleted_ids.GetCount(); i++)
	{
		if(!partner_id.Compare(*m_deleted_ids.Get(i)))
			return true;
	}
	return false;
}

OP_STATUS SpeedDialManager::AddToDeletedIDs(const uni_char *id, bool notify)
{ 
	OpAutoPtr<OpString> key (OP_NEW(OpString, ()));
	RETURN_OOM_IF_NULL(key.get());
	RETURN_IF_ERROR(key->Set(id));

	RETURN_IF_ERROR(m_deleted_ids.Add(key.get()));
	key.release();

	if(notify) 
	{ 
		NotifySpeedDialPartnerIDAdded(id); 
	}
	return OpStatus::OK; 
}

bool SpeedDialManager::RemoveFromDeletedIDs(const OpStringC& partner_id, bool notify)
{
	for(UINT32 i = 0; i < m_deleted_ids.GetCount(); i++)
	{
		if(!partner_id.Compare(*m_deleted_ids.Get(i)))
		{
			OpString* id = m_deleted_ids.Remove(i);
			if(notify) 
			{ 
				NotifySpeedDialPartnerIDDeleted(id->CStr()); 
			}
			OP_DELETE(id);
			return true;
		}
	}
	return false;
}

/*
Some rules:
- If a partner_id is in the deleted section, don't do anything with it
- If the url of a certain partner_id speed dial is different in the default speeddial.ini, replace the users speed dial entry with the new one, regardless of the position 
- If a partner_id in the default speedial doesn't exist in the user speeddial, add it in the first available blank spot (usually at the end) 
*/
OP_STATUS SpeedDialManager::MergeSpeedDialEntries(bool merge_entries, PrefsFile& default_sd_file, PrefsFile& sd_file, bool& prefs_changed, bool add_extra_items)
{
	OpAutoStringHashTable<DesktopSpeedDial>	default_partner_id_speed_dials;	// we use this for faster lookups on partner_ids
	OpStringHashTable<DesktopSpeedDial>	default_url_speed_dials;		// we use this for faster lookups on URLs

	prefs_changed = false;

	if(merge_entries)
	{
		// first read the default sd file
		OpString_list sections;

		RETURN_IF_LEAVE(default_sd_file.ReadAllSectionsL(sections));

		OpString8 section_name;
		for(unsigned i = 0; i < sections.Count(); ++i)
		{
			RETURN_IF_ERROR(section_name.Set(sections[i]));
			if(!section_name.Compare(SPEEDDIAL_SECTION_STR, strlen(SPEEDDIAL_SECTION_STR)))
			{ 
				OpAutoPtr<DesktopSpeedDial> sd(OP_NEW(DesktopSpeedDial, ()));
				RETURN_OOM_IF_NULL(sd.get());
				RETURN_IF_LEAVE(RETURN_IF_ERROR(sd->LoadL(default_sd_file, section_name)));

				// only check anything with speed dials that has a partner id
				// otherwise we'll just drop it again when the smart ptr goes out of scope
				if(sd->GetPartnerID().HasContent())
				{
					// ignore it if speed dial with the same partner id or URL is already loaded
					if (default_partner_id_speed_dials.Contains(sd->GetPartnerID().CStr()) ||
						default_url_speed_dials.Contains(sd->GetURL().CStr()))
						continue;

					// ignore it for further checks if it's in the deleted list
					if(IsInDeletedList(sd->GetPartnerID()))
						continue;

					RETURN_IF_ERROR(default_partner_id_speed_dials.Add(sd->GetPartnerID().CStr(), sd.get()));
					DesktopSpeedDial* sd_ptr = sd.release(); // in auto hash now, so release
					RETURN_IF_ERROR(default_url_speed_dials.Add(sd_ptr->GetURL().CStr(), sd_ptr));
				}
			}
		}
	}
	// read the regular sections
	OpString_list sections;
	RETURN_IF_LEAVE(sd_file.ReadAllSectionsL(sections));

	OpString8 section_name;
	for(unsigned i = 0; i < sections.Count(); ++i)
	{
		RETURN_IF_ERROR(section_name.Set(sections[i]));
		if(!section_name.Compare(SPEEDDIAL_SECTION_STR, strlen(SPEEDDIAL_SECTION_STR)))
		{ 
			// this is speed dial section
			OpAutoPtr<DesktopSpeedDial> sd(OP_NEW(DesktopSpeedDial, ()));
			RETURN_OOM_IF_NULL(sd.get());
			RETURN_IF_LEAVE(RETURN_IF_ERROR(sd->LoadL(sd_file, section_name)));

			if (sd->GetExtensionWUID().HasContent())
			{	
				if (!ExtensionUtils::IsExtensionRunning(sd->GetExtensionWUID()))
				{	
					// in case extensions was not started, delete its entry from speeddial.ini
					prefs_changed = true;
					continue;
				}
			}
			RETURN_IF_ERROR(m_speed_dials.Add(sd.get()));
			sd->OnAdded();
			
			DesktopSpeedDial *tmp_sd = sd.release();

			// check if we need to replace or update anything
			if(merge_entries)
			{
				const OpStringC& partner_id = tmp_sd->GetPartnerID();
				const OpStringC& url = tmp_sd->GetURL();

				DesktopSpeedDial *partner_sd;

				// check if we have a match on partner id first
				if(partner_id.HasContent() && OpStatus::IsSuccess(default_partner_id_speed_dials.GetData(partner_id.CStr(), &partner_sd)))
				{
					if(tmp_sd->GetURL().Compare(partner_sd->GetURL()))
					{
						// If the url of a certain partner_id speed dial is different in the default speeddial.ini,
						// replace the users speed dial entry with the new one, regardless of the position
						// Keep the unique id unchanged to avoid adding duplicates in other browsers that will
						// get this change via Link
						tmp_sd->Set(*partner_sd, true);
						prefs_changed = true;
					}
					OpStatus::Ignore(default_url_speed_dials.Remove(url.CStr(), &partner_sd));
	
					// We found it, remove it so it's not added at the end of the speed dials later
					if(OpStatus::IsSuccess(default_partner_id_speed_dials.Remove(partner_id.CStr(), &partner_sd)))
					{
						OP_DELETE(partner_sd);
					}
				}
				// if not, check if we have the same url in speed dial but without partner id
				else if(url.HasContent() && OpStatus::IsSuccess(default_url_speed_dials.GetData(url.CStr(), &partner_sd)))
				{
					// we have the same url in the user file, update its data instead (but keep the unique id unchanged)
					tmp_sd->Set(*partner_sd, true);
					prefs_changed = true;

					OpStatus::Ignore(default_url_speed_dials.Remove(url.CStr(), &partner_sd));

					// We found it, remove it so it's not added at the end of the speed dials later
					if(OpStatus::IsSuccess(default_partner_id_speed_dials.Remove(partner_id.CStr(), &partner_sd)))
					{
						OP_DELETE(partner_sd);
					}
				}
			}
			// check if we have an ID and generate it if not
			if(tmp_sd->GetUniqueID().IsEmpty())
			{
				RETURN_IF_ERROR(tmp_sd->GenerateIDIfNeeded(FALSE, TRUE, m_speed_dials.Find(tmp_sd) + 1));
				prefs_changed = true;
			}
		}
	}
	// If a partner_id in the default speeddial doesn't exist in the user speeddial, 
	// add it in the first available blank spot (usually at the end) 
	if(add_extra_items)
	{
		for (StringHashIterator<DesktopSpeedDial> it(default_partner_id_speed_dials); it; it++)
		{
			DesktopSpeedDial* sd = it.GetData();

			// ownership transferred to the m_speed_dials collection
			RETURN_IF_ERROR(m_speed_dials.Add(sd));
			sd->OnAdded();
			prefs_changed = true;

			if(sd->GetUniqueID().IsEmpty())
			{
				RETURN_IF_ERROR(sd->GenerateIDIfNeeded(FALSE, TRUE, m_speed_dials.Find(sd) + 1));
			}	
		}
	}
	default_partner_id_speed_dials.RemoveAll();
	return OpStatus::OK;
}

// Will return a list of the default URLs if the default speed dial ini file can be found.
OP_STATUS SpeedDialManager::GetDefaultURLList(OpString_list& url_list, OpString_list& title_list)
{
    // The lists should be empty when passed in
    OP_ASSERT(url_list.Count() == 0);
    OP_ASSERT(title_list.Count() == 0);
    
    // Get the default speed dial ini file
    OpFile default_file;
	RETURN_IF_ERROR(ResourceSetup::GetDefaultPrefsFile(SDM_SPEED_DIAL_FILENAME, default_file));

    // Read the URLs, and add them to the string list
	PrefsFile default_prefs_file(PREFS_STD);

	/* Construct and load up the speeddial.ini pref file */
	OP_STATUS status;
	TRAP(status, 
		default_prefs_file.ConstructL();
		default_prefs_file.SetFileL(&default_file);
		default_prefs_file.LoadAllL();
	);
	RETURN_IF_ERROR(status);
	
	/* Loop the sections */
	UINT32 i;
	
	url_list.Resize(m_speed_dials.GetCount());
	title_list.Resize(m_speed_dials.GetCount());
	
	for (i = 1; i <= m_speed_dials.GetCount(); i++)
	{
		/* Sections are from 1 -> 10 */
		OpString8 section_name;
		RETURN_IF_ERROR(section_name.AppendFormat(SPEEDDIAL_SECTION, i));

		// Read the URL and title sections
		OpString url;
		OpString title;
		
		TRAP(status,
			url.Set(default_prefs_file.ReadStringL(section_name.CStr(), SPEEDDIAL_KEY_URL));
			title.Set(default_prefs_file.ReadStringL(section_name.CStr(), SPEEDDIAL_KEY_TITLE));
		);

		// Add them to the list, but only if you both have title and URL
		if (url.HasContent() && title.HasContent())
		{
			url_list.Item(i-1).Set(url);
		    title_list.Item(i-1).Set(title);
	    }
		
	}

	return OpStatus::OK;

}

OP_STATUS SpeedDialManager::LoadBackgroundImage(OpString& filename)
{
	m_image_type = NONE;

	RETURN_IF_ERROR(m_background_image_name.Set(filename.CStr()));

#ifdef SPEEDDIAL_SVG_SUPPORT
	if(uni_stristr(filename.CStr(), ".svg") != NULL)
	{
		m_svg_background.SetListener(this);

		RETURN_IF_ERROR(m_svg_background.LoadSVGImage(filename));
	}
	else
#endif // SPEEDDIAL_SVG_SUPPORT
	{
		RETURN_IF_ERROR(m_image_reader.Read(filename));
		m_image_type = BITMAP;
	}
	UINT32 i;

	for(i = 0; i < m_config_listeners.GetCount(); i++)
	{
		m_config_listeners.Get(i)->OnSpeedDialBackgroundChanged();
	}
	return OpStatus::OK;
}

void SpeedDialManager::OnColumnLayoutChanged()
{
	for (UINT32 i = 0; i < m_config_listeners.GetCount(); i++)
	{
		m_config_listeners.Get(i)->OnSpeedDialColumnLayoutChanged();
	}
}

void SpeedDialManager::SetBackgroundImageEnabled(bool enabled)
{ 
	m_background_enabled = enabled; 

	UINT32 i;

	for(i = 0; i < m_config_listeners.GetCount(); i++)
	{
		m_config_listeners.Get(i)->OnSpeedDialBackgroundChanged();
	}
}

bool SpeedDialManager::HasBackgroundImage() 
{ 
#ifdef SPEEDDIAL_SVG_SUPPORT
	if(m_image_type == SVG)
	{
		return m_background_enabled; 
	}
	else
#endif // SPEEDDIAL_SVG_SUPPORT
	if(m_image_type == BITMAP)
	{
		return m_background_enabled && !m_image_reader.GetImage().IsEmpty(); 
	}
	return false;
}

Image SpeedDialManager::GetBackgroundImage(UINT32 width, UINT32 height) 
{ 
#ifdef SPEEDDIAL_SVG_SUPPORT
	if(m_image_type == SVG)
	{
		return m_svg_background.GetImage(width, height);
	}
	else
#endif // SPEEDDIAL_SVG_SUPPORT
	if (m_image_type == BITMAP)
	{
		return m_image_reader.GetImage();
	}
	return Image();
}

#ifdef SPEEDDIAL_SVG_SUPPORT
void SpeedDialManager::OnImageLoaded()
{
	UINT32 i;

	m_image_type = SVG;

	for(i = 0; i < m_config_listeners.GetCount(); i++)
	{
		m_config_listeners.Get(i)->OnSpeedDialBackgroundChanged();
	}
}
#endif // SPEEDDIAL_SVG_SUPPORT

void SpeedDialManager::StartConfiguring(const DesktopWindow& window)
{
	for (UINT32 i = 0; i < m_config_listeners.GetCount(); i++)
	{
		m_config_listeners.Get(i)->OnSpeedDialConfigurationStarted(window);
	}
}

bool SpeedDialManager::SetThumbnailScale(double new_scale)
{
	new_scale = MAX(MinimumZoomLevel, MIN(MaximumZoomLevel, new_scale));

	if (m_scale == new_scale)
		return false;

	m_scale = new_scale;

	if (!m_automatic_scale)
		UpdateScale();

	return true;
}

void SpeedDialManager::ChangeScaleByDelta(bool scale_up)
{
	if (scale_up)
		SetThumbnailScale(m_scale + ScaleDelta);
	else
		SetThumbnailScale(m_scale - ScaleDelta);
}

void SpeedDialManager::SetAutomaticScale(bool is_automatic_scale)
{
	if (m_automatic_scale != is_automatic_scale)
	{
		m_automatic_scale = is_automatic_scale;
		UpdateScale();
	}
}

void SpeedDialManager::UpdateScale()
{
	int prefs_scale = m_automatic_scale ? 0 : GetIntegerThumbnailScale();

	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::SpeedDialZoomLevel, prefs_scale));

	for (UINT32 i = 0; i < m_config_listeners.GetCount(); i++)
	{
		m_config_listeners.Get(i)->OnSpeedDialScaleChanged();
	}

	if (!m_scaling)
		m_zoom_timer.Start(MILLISECONDS_TO_WAIT_WITH_RELOAD);
}

void SpeedDialManager::StartScaling()
{
	OP_ASSERT(!m_scaling);
	m_scaling = true;
}

void SpeedDialManager::EndScaling()
{
	OP_ASSERT(m_scaling);
	m_scaling = false;
	m_zoom_timer.Start(MILLISECONDS_TO_WAIT_WITH_RELOAD);
}

unsigned SpeedDialManager::GetThumbnailWidth()  const
{
	ThumbnailManager::ThumbnailSize size = g_thumbnail_manager->GetThumbnailSize();
	return (m_scale * size.base_thumbnail_width);
}

unsigned SpeedDialManager::GetThumbnailHeight() const
{
	ThumbnailManager::ThumbnailSize size = g_thumbnail_manager->GetThumbnailSize();
	return (m_scale * size.base_thumbnail_height);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SpeedDialManager::Save()
{
	UINT32 i;
	OpFile speeddial_file;

	if(m_has_loaded_config == false)
	{
		OP_ASSERT(!"Save() called before Load(), data loss will occur if we continue with Save()");
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	RETURN_IF_LEAVE(g_pcfiles->GetFileL(PrefsCollectionFiles::SpeedDialFile, speeddial_file));

	/* Construct and load up the speeddial.ini pref file */
	PrefsFile sd_file(PREFS_STD);

	RETURN_IF_LEAVE(
		sd_file.ConstructL();
		sd_file.SetFileL(&speeddial_file));

	/* Kill all the sections in the speeddial.ini which will be replaced below */
	RETURN_IF_LEAVE(sd_file.DeleteAllSectionsL());

	if (m_force_reload)
		RETURN_IF_LEAVE(sd_file.WriteIntL(SPEEDDIAL_VERSION_SECTION, SPEEDDIAL_KEY_FORCE_RELOAD, 1));

	// write out the deleted entries
	for(UINT32 n = 0; n < m_deleted_ids.GetCount(); n++)
	{
		RETURN_IF_LEAVE(sd_file.WriteStringL(UNI_L(SPEEDDIAL_DELETED_ITEMS_SECTION), *m_deleted_ids.Get(n), NULL));
	}
	if(m_background_image_name.HasContent())
	{
		RETURN_IF_LEAVE(sd_file.WriteStringL(SPEEDDIAL_IMAGE_SECTION, SPEEDDIAL_KEY_IMAGENAME, m_background_image_name.CStr()));

		const uni_char *layout_option = UNI_L("Crop");

		switch(m_image_layout)
		{
			case OpImageBackground::STRETCH:
			{
				layout_option = UNI_L("Stretch");
			}
			break;

			case OpImageBackground::TILE:
			{
				layout_option = UNI_L("Tile");
			}
			break;

			case OpImageBackground::CENTER:
			{
				layout_option = UNI_L("Center");
			}
			break;
		}
		RETURN_IF_LEAVE(sd_file.WriteStringL(SPEEDDIAL_IMAGE_SECTION, SPEEDDIAL_KEY_IMAGELAYOUT, layout_option));
	}
	RETURN_IF_LEAVE(sd_file.WriteIntL(SPEEDDIAL_IMAGE_SECTION, SPEEDDIAL_KEY_IMAGE_ENABLED, IsBackgroundImageEnabled()));

	for	(i = 1; i <= m_speed_dials.GetCount(); i++)
	{
		DesktopSpeedDial *sd = m_speed_dials.Get(i - 1);
		if(!sd->IsEmpty())
		{
			OpString8 section_name;
			RETURN_IF_ERROR(section_name.AppendFormat(SPEEDDIAL_SECTION, i));
			RETURN_IF_LEAVE(sd->SaveL(sd_file, section_name));
		}
	}

	if(HasUIOpacity())
	{
		RETURN_IF_LEAVE(sd_file.WriteIntL(SPEEDDIAL_UI_SECTION, SPEEDDIAL_UI_OPACITY, GetUIOpacity()));
	}
	/* Commit the speed dial file */
	RETURN_IF_LEAVE(sd_file.CommitL());

	return OpStatus::OK;
}

DesktopSpeedDial* SpeedDialManager::GetSpeedDialByID(const uni_char* unique_id) const
{
	for	(UINT32 i = 0; i < m_speed_dials.GetCount(); i++)
	{
		DesktopSpeedDial *sd = m_speed_dials.Get(i);
		if(!sd->IsEmpty() && !sd->GetUniqueID().Compare(unique_id))
		{
			return sd;
		}
	}
	return NULL;
}

void SpeedDialManager::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (id != OpPrefsCollection::UI)
		return;

	switch (pref)
	{
		case PrefsCollectionUI::SpeedDialState:
		case PrefsCollectionUI::ShowAddSpeedDialButton:
			g_application->SettingsChanged(SETTINGS_SPEED_DIAL);
			break;
	}
}

void SpeedDialManager::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_zoom_timer && !m_scaling)
	{
		ReloadAll(PurposeZoom);
	}
}

void SpeedDialManager::SetDirty( bool state )
{
	// should not get dirty before speeddial.ini is loaded
	OP_ASSERT(m_has_loaded_config || !state);

	if( m_is_dirty != state )
	{
		m_is_dirty = state;
		if( m_is_dirty )
		{
			// Start delayed save
			m_delayed_trigger.InvokeTrigger();
		}
		else
		{
			// abort writing
			m_delayed_trigger.CancelTrigger();
		}
	}
}

void SpeedDialManager::OnTrigger(OpDelayedTrigger* trigger)
{
	OP_ASSERT(trigger == &m_delayed_trigger);
	if(OpStatus::IsSuccess(Save()))
	{
		m_is_dirty = false;
	}
}

void SpeedDialManager::NotifySpeedDialAdded(const DesktopSpeedDial& sd)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnSpeedDialAdded(sd);
}

void SpeedDialManager::NotifySpeedDialRemoving(const DesktopSpeedDial& sd)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnSpeedDialRemoving(sd);
}

void SpeedDialManager::NotifySpeedDialReplaced(const DesktopSpeedDial& old_sd, const DesktopSpeedDial& new_sd)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnSpeedDialReplaced(old_sd, new_sd);
}

void SpeedDialManager::NotifySpeedDialMoved(int from_pos, int to_pos)
{
	const DesktopSpeedDial* from_sd = GetSpeedDial(from_pos);
	const DesktopSpeedDial* to_sd   = GetSpeedDial(to_pos);

	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnSpeedDialMoved(*from_sd, *to_sd);
}


void SpeedDialManager::NotifySpeedDialPartnerIDAdded(const uni_char* partner_id)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnSpeedDialPartnerIDAdded(partner_id);
}

void SpeedDialManager::NotifySpeedDialPartnerIDDeleted(const uni_char* partner_id)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnSpeedDialPartnerIDDeleted(partner_id);
}

void SpeedDialManager::NotifySpeedDialsLoaded()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnSpeedDialsLoaded();
}

unsigned SpeedDialManager::GetDefaultCellWidth() const
{
	ThumbnailManager::ThumbnailSize size = g_thumbnail_manager->GetThumbnailSize();
	return size.base_thumbnail_width;
}

void SpeedDialManager::GetMinimumAndMaximumCellSizes(unsigned& min_width,
		unsigned& min_height,
		unsigned& max_width,
		unsigned& max_height) const
{
	ThumbnailManager::ThumbnailSize size = g_thumbnail_manager->GetThumbnailSize();
	min_width  = size.base_thumbnail_width * (m_automatic_scale ? MinimumZoomLevelForAutoZooming : MinimumZoomLevel);
	min_height = size.base_thumbnail_height * (m_automatic_scale ? MinimumZoomLevelForAutoZooming : MinimumZoomLevel);
	max_width  = size.base_thumbnail_width * (m_automatic_scale ? MaximumZoomLevelForAutoZooming : MaximumZoomLevel);
	max_height = size.base_thumbnail_height * (m_automatic_scale ? MaximumZoomLevelForAutoZooming : MaximumZoomLevel);
}


INT32 SpeedDialManager::FindSpeedDialByWuid(const OpStringC& wuid) const
{
	for (unsigned i = 0; i < m_speed_dials.GetCount(); ++i)
	{
		if (m_speed_dials.Get(i)->GetExtensionWUID() == wuid)
		{
			return i;
		}
	}
	return -1;
}

INT32 SpeedDialManager::FindSpeedDialByUrl(const OpStringC& url, bool include_display_url) const
{
	int len_to_cmp = KAll;
	int len_of_doc_url = url.Length();
	for(UINT32 i = 0; i < g_speeddial_manager->GetTotalCount(); i++)
	{
		const DesktopSpeedDial *entry = g_speeddial_manager->GetSpeedDial(i);
		if(entry && !entry->IsEmpty() && !entry->GetExtensionWUID().HasContent())
		{
			// DSK-345498
			int len_of_entry_url = entry->GetURL().Length();
			if (abs(len_of_entry_url - len_of_doc_url ) == 1)
			{
				int min_len = min(len_of_entry_url, len_of_doc_url);
				if (entry->GetURL().DataPtr()[min_len] == UNI_L('/') || url.DataPtr()[min_len] == UNI_L('/'))
					len_to_cmp = min_len;
			}

			if (entry->GetURL().Compare(url, len_to_cmp) == 0)
				return i;
			else if (include_display_url && 
				entry->GetDisplayURL().Compare(url) == 0)
				return i;
		}
	}
	return -1;
}

#ifdef SPEEDDIAL_SVG_SUPPORT

SpeedDialSVGBackground::SpeedDialSVGBackground() : 
	m_listener(NULL),
	m_window(NULL),
	m_windowcommander(NULL),
	m_width(1280),
	m_height(1024)
{

}

SpeedDialSVGBackground::~SpeedDialSVGBackground()
{
	if (m_windowcommander)					// might not exist due to OOM in init
	{
		m_windowcommander->SetLoadingListener(NULL);
		m_windowcommander->OnUiWindowClosing();
		g_windowCommanderManager->ReleaseWindowCommander(m_windowcommander);
		m_windowcommander = NULL;
	}
	OP_DELETE(m_window);
}

void SpeedDialSVGBackground::OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status)
{
	if(status == OpLoadingListener::LOADING_SUCCESS)
	{
		if(m_listener)
		{
			m_listener->OnImageLoaded();
		}
	}
}

Image SpeedDialSVGBackground::GetImage(UINT32 width, UINT32 height)
{
	bool changed = m_width != width || m_height != height || m_dirty;

	if(!changed)
	{
		return m_image;
	}
	m_width = width;
	m_height = height;

	OpStatus::Ignore(WindowCommanderProxy::GetSVGImage(m_windowcommander, m_image, width, height));

	m_dirty = false;

	return m_image;
}

OP_STATUS SpeedDialSVGBackground::LoadSVGImage(OpString& filename)
{
	if(!m_windowcommander)
	{
		RETURN_IF_ERROR(g_windowCommanderManager->GetWindowCommander(&m_windowcommander));
	}
	if(!m_window)
	{
		RETURN_IF_ERROR(OpWindow::Create(&m_window));

		RETURN_IF_ERROR(m_window->Init(OpWindow::STYLE_DESKTOP, OpTypedObject::WINDOW_TYPE_UNKNOWN, NULL, NULL));

		RETURN_IF_ERROR(m_windowcommander->OnUiWindowCreated(m_window));

		m_windowcommander->SetLoadingListener(this);
	}
	m_dirty = true;

	m_windowcommander->OpenURL(filename.CStr(), FALSE);

	return OpStatus::OK;
}

#endif // SPEEDDIAL_SVG_SUPPORT
#endif // SUPPORT_SPEED_DIAL
