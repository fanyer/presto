/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Kajetan Switalski
**
*/

#include "core/pch.h"

#ifdef USE_SPDY

#include "modules/url/protocols/spdy/spdy_internal/spdy_settingsmanager.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_common.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/datastream/opdatastreamsafefile.h"
#include "modules/formats/url_dfr.h"

#define SPDYSETTINGS_FILE_VERSION 1
#define SPDYSETTINGS_FILE_NAME UNI_L("spdysett.dat")

#define TAG_SETTINGS_HOSTPORT_ENTRY 0x01
#define TAG_SETTINGS_HOSTPORT_NAME 0x02
#define TAG_SETTINGS_IDVALUE_PAIR_ENTRY 0x03
#define TAG_SETTINGS_IDVALUE_PAIR_ENTRIES 0x04
#define TAG_SETTINGS_LAST_TIME_USED 0x05

SpdySettingsManager::SpdySettingsManager(): loaded(FALSE), updated(FALSE)
{
}

SpdySettingsManager::~SpdySettingsManager()
{
	if (updated)
	{
		TRAPD(res, WriteFileL());
		OpStatus::Ignore(res);
	}

	storage.DeleteAll();
}

void SpdySettingsManager::LoadFileL()
{
	OpStackAutoPtr<OpFile> settingsFile(OP_NEW_L(OpFile, ()));
	SPDY_LEAVE_IF_ERROR(settingsFile->Construct(SPDYSETTINGS_FILE_NAME, OPFILE_COOKIE_FOLDER));
	
	if (OpStatus::IsSuccess(settingsFile->Open(OPFILE_READ)))
	{
		DataFile dataFile(settingsFile.release()); 
		ANCHOR(DataFile, dataFile);
		dataFile.InitL();

		OpStackAutoPtr<DataFile_Record> rec(NULL);
		for (rec.reset(dataFile.GetNextRecordL()); rec.get() && rec->GetTag() == TAG_SETTINGS_HOSTPORT_ENTRY; rec.reset(dataFile.GetNextRecordL()))
		{
			rec->IndexRecordsL();

			SettingsTable *settings = OP_NEW_L(SettingsTable, ());
			ANCHOR_PTR(SettingsTable, settings);
			rec->GetIndexedRecordStringL(TAG_SETTINGS_HOSTPORT_NAME, settings->hostport);
			settings->lastTimeUsed = static_cast<time_t>(rec->GetIndexedRecordUInt64L(TAG_SETTINGS_LAST_TIME_USED));
			storage.AddL(settings->hostport, settings);
			ANCHOR_PTR_RELEASE(settings);

			DataFile_Record *entries = rec->GetIndexedRecord(TAG_SETTINGS_IDVALUE_PAIR_ENTRIES);
			if (entries)
			{
				OpStackAutoPtr<DataFile_Record> entry(NULL);
				for (entry.reset(entries->GetNextRecordL()); entry.get() && entry->GetTag() == TAG_SETTINGS_IDVALUE_PAIR_ENTRY; entry.reset(entries->GetNextRecordL()))
				{
					UINT32 settingId = entry->GetInt32L();
					UINT32 value = entry->GetInt32L();
					SPDY_LEAVE_IF_ERROR(settings->idvalues.Add(settingId, value));
				}
			}
		}
	}

	loaded = TRUE;
}

void SpdySettingsManager::WriteFileL()
{
	time_t minLastTimeUsed = g_timecache->CurrentTime() - static_cast<time_t>(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SpdySettingsPersistenceTimeout));

	OpStackAutoPtr<OpSafeFile> settingsFile(OP_NEW_L(OpDataStreamSafeFile, ()));
	SPDY_LEAVE_IF_ERROR(settingsFile->Construct(SPDYSETTINGS_FILE_NAME, OPFILE_COOKIE_FOLDER));
	SPDY_LEAVE_IF_ERROR(settingsFile->Open(OPFILE_WRITE));
	DataFile dataFile(settingsFile.release(), SPDYSETTINGS_FILE_VERSION, 1, 2);
	ANCHOR(DataFile, dataFile);
	dataFile.InitL();

	OpStackAutoPtr<OpHashIterator> it(storage.GetIterator());
	SPDY_LEAVE_IF_NULL(it.get());
	OP_STATUS res = OpStatus::OK;
	for (res = it->First(); OpStatus::IsSuccess(res); res = it->Next())
	{
		const char *host = reinterpret_cast<const char*>(it->GetKey());
		SettingsTable* settings = static_cast<SettingsTable*>(it->GetData());
		if (settings->lastTimeUsed < minLastTimeUsed)
			continue;

		DataFile_Record rec(TAG_SETTINGS_HOSTPORT_ENTRY);
		ANCHOR(DataFile_Record, rec);
		rec.SetRecordSpec(dataFile.GetRecordSpec());

		DataFile_Record hostport(TAG_SETTINGS_HOSTPORT_NAME);
		ANCHOR(DataFile_Record, hostport);
		hostport.SetRecordSpec(dataFile.GetRecordSpec());
		hostport.AddContentL(host);
		hostport.WriteRecordL(&rec);
		rec.AddRecord64L(TAG_SETTINGS_LAST_TIME_USED, static_cast<OpFileLength>(settings->lastTimeUsed));

		DataFile_Record pairEntries(TAG_SETTINGS_IDVALUE_PAIR_ENTRIES);
		ANCHOR(DataFile_Record, pairEntries);
		pairEntries.SetRecordSpec(dataFile.GetRecordSpec());

		OpStackAutoPtr<OpHashIterator> it2(settings->idvalues.GetIterator());
		SPDY_LEAVE_IF_NULL(it2.get());
		res = OpStatus::OK;
		for (res = it2->First(); OpStatus::IsSuccess(res); res = it2->Next())
		{
			UINT32 id = static_cast<UINT32>(reinterpret_cast<UINTPTR>(it2->GetKey()));
			UINT32 value = static_cast<UINT32>(reinterpret_cast<UINTPTR>(it2->GetData()));
			
			DataFile_Record idvalue(TAG_SETTINGS_IDVALUE_PAIR_ENTRY);
			ANCHOR(DataFile_Record, idvalue);
			idvalue.SetRecordSpec(dataFile.GetRecordSpec());

			idvalue.AddContentL(id);
			idvalue.AddContentL(value);
			idvalue.WriteRecordL(&pairEntries);
		}
		pairEntries.WriteRecordL(&rec);
		rec.WriteRecordL(&dataFile);
	}
	dataFile.Close();
	updated = FALSE;
}

void SpdySettingsManager::ClearPersistedSettingsL(const char* hostport)
{
	if (!loaded)
		LoadFileL();

	if (storage.Contains(hostport))
	{
		SettingsTable *settings; 
		if (OpStatus::IsSuccess(storage.Remove(hostport, &settings)))
			OP_DELETE(settings);
	}
	updated = TRUE;
}

void SpdySettingsManager::ClearAllL()
{
	if (!loaded)
		LoadFileL();

	storage.DeleteAll();
	updated = TRUE;
}

void SpdySettingsManager::PersistSettingL(const char* hostport, UINT32 settingId, UINT32 value)
{
	if (!loaded)
		LoadFileL();

	SettingsTable *settings;
	if (OpStatus::IsError(storage.GetData(hostport, &settings)))
	{
		settings = OP_NEW_L(SettingsTable, ());
		ANCHOR_PTR(SettingsTable, settings);
		settings->hostport.SetL(hostport);
		storage.AddL(settings->hostport, settings);
		ANCHOR_PTR_RELEASE(settings);
	}

	if (settings->idvalues.Contains(settingId))
		SPDY_LEAVE_IF_ERROR(settings->idvalues.Update(settingId, value));
	else
		SPDY_LEAVE_IF_ERROR(settings->idvalues.Add(settingId, value));
	settings->lastTimeUsed = g_timecache->CurrentTime();

	updated = TRUE;
}

class GetPersistedSettingsHelper: public OpHashTableForEachListener
{
	List<PersistedSetting> &target;
	BOOL oomed;
public:
	GetPersistedSettingsHelper(List<PersistedSetting> &target): target(target), oomed(FALSE) {}
	
	BOOL Oomed() { return oomed; }
	
	virtual void HandleKeyData(const void* key, void* data)
	{
		PersistedSetting *setting = OP_NEW(PersistedSetting, ());
		if (setting)
		{
			setting->settingId = static_cast<UINT32>(reinterpret_cast<UINTPTR>(key));
			setting->value = static_cast<UINT32>(reinterpret_cast<UINTPTR>(data));
			setting->Into(&target);
		}
		else
			oomed = TRUE;
	}
};

void SpdySettingsManager::GetPersistedSettingsL(const char* hostport, List<PersistedSetting> &target)
{
	if (!loaded)
		LoadFileL();

	SettingsTable *settings;
	if (OpStatus::IsSuccess(storage.GetData(hostport, &settings)))
	{
		GetPersistedSettingsHelper helper(target);
		settings->idvalues.ForEach(&helper);
		if (helper.Oomed())
			SPDY_LEAVE(OpStatus::ERR_NO_MEMORY);
		settings->lastTimeUsed = g_timecache->CurrentTime();
	}
}

#endif // USE_SPDY
