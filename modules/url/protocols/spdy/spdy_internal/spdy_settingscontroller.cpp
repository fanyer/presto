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

#include "modules/url/protocols/spdy/spdy_internal/spdy_settingscontroller.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_common.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_frameheaders.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_settingsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

enum SpdySettingsFlags
{
	SSF_NONE = 0,
	SSF_PERSIST,
	SSF_PERSISTED
};

enum SpdySettingsControlFlags
{
	SSCF_CLEAR_PERSISTED_SETTINGS = 1
};

enum SpdySettingsIds
{
	SSI_UPLOAD_BANDWIDTH = 1,
	SSI_DOWNLOAD_BANDWIDTH,
	SSI_ROUND_TRIP_TIME,
	SSI_MAX_CONCURRENT_STREAMS,
	SSI_CURRENT_CWND,
	SSI_DOWNLOAD_RETRANS_RATE,
	SSI_INITIAL_WINDOW_SIZE
};

const unsigned SettingsEntrySize = 8;

class SettingsEntry
{
	UINT32 flags_and_id;
	UINT32 value;

public:
	UINT32 GetId(SpdyVersion version) const
	{ 
		if (version == SPDY_V2) // in spdy2 the id is little endian (it's the result of a bug in initial implementation)
		{
			UINT32 id = op_ntohl(flags_and_id) >> 8;
			UINT32 rotate16 = (id << 16) | (id >> 16);
			const UINT32 mask = 0xff00ff;
			return (((rotate16 >> 8) & mask) | ((rotate16 & mask) << 8)) >> 8;
		}
		else
			return op_ntohl(flags_and_id) & 0xffffff; 
	}
	UINT8 GetFlags(SpdyVersion version) const { return version == SPDY_V2 ? op_ntohl(flags_and_id) & 0xff : op_ntohl(flags_and_id) >> 24; }
	UINT32 GetValue() const { return op_ntohl(value); }
	
	void SetId(SpdyVersion version, UINT32 id)
	{ 
		if (version == SPDY_V2) // in spdy2 the id is little endian (it's the result of a bug in initial implementation)
		{
			UINT32 rotate16 = (id << 16) | (id >> 16);
			const UINT32 mask = 0xff00ff;
			flags_and_id = op_htonl(op_ntohl(flags_and_id) & 0xff | (((rotate16 >> 8) & mask) | ((rotate16 & mask) << 8)));
		}
		else
			flags_and_id = op_htonl(op_ntohl(flags_and_id) & 0xff000000 | id); 
	}
	void SetFlags(SpdyVersion version, UINT8 f) 
	{ 
		if (version == SPDY_V2)
			flags_and_id = op_htonl(op_ntohl(flags_and_id) & 0xffffff00 | f);
		else
			flags_and_id = op_htonl(op_ntohl(flags_and_id) & 0xffffff | (f << 24));
	}
	void SetValue(UINT32 v) { value = op_htonl(v); }

	SettingsEntry(SpdyVersion version, UINT32 id, UINT32 val, UINT8 flags)
	{
		SetValue(val);
		SetFlags(version, flags);
		SetId(version, id);
	}
};

SpdySettingsController::SpdySettingsController(SpdyVersion version, ServerName_Pointer serverName, unsigned short port, BOOL privacyMode):
	leftToConsume(0),
	maxConcurrentStreams(0),
	uploadBandwidth(0),
	downloadBandwidth(0),
	roundTripTime(0),
	currentCwnd(0),
	downloadRetransRate(0),
	version(version),
	serverName(serverName),
	port(port),
	privateMode(privacyMode)
{
	OP_ASSERT(version == SPDY_V2 || version == SPDY_V3);
}

void SpdySettingsController::InitL()
{
	SPDY_LEAVE_IF_ERROR(hostport.AppendFormat("%s:%d", serverName->Name(), port));
	
	if (!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SpdySettingsPersistence, serverName) 
#ifdef _ASK_COOKIE		
		|| serverName->GetAcceptCookies() == COOKIE_NONE
#endif // _ASK_COOKIE
		)
		privateMode = TRUE;

	OpData settingEntries;
	if (!privateMode && g_spdy_settings_manager)
	{
		AutoDeleteList<PersistedSetting> settings;
		g_spdy_settings_manager->GetPersistedSettingsL(hostport, settings);
		for (PersistedSetting *it = settings.First(); it; it = it->Suc())
		{
			SettingsEntry entry(version, it->settingId, it->value, SSF_PERSISTED);
			SPDY_LEAVE_IF_ERROR(settingEntries.AppendCopyData(reinterpret_cast<char*>(&entry), SettingsEntrySize));
			SetSetting(it->settingId, it->value);
		}
	}

	initialWindowSize = static_cast<UINT32>(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SpdyInitialWindowSize, serverName));
	if (static_cast<UINT32>(g_pcnet->GetDefaultIntegerPref(PrefsCollectionNetwork::SpdyInitialWindowSize)) != initialWindowSize)
	{
		SettingsEntry entry(version, SSI_INITIAL_WINDOW_SIZE, initialWindowSize, SSF_NONE);
		SPDY_LEAVE_IF_ERROR(settingEntries.AppendCopyData(reinterpret_cast<char*>(&entry), SettingsEntrySize));
	}

	if (!settingEntries.IsEmpty())
	{
		SettingsFrameHeader sfh(version, settingEntries.Length() / SettingsEntrySize);
		SPDY_LEAVE_IF_ERROR(initialFrame.AppendCopyData(reinterpret_cast<char*>(&sfh), SettingsFrameHeader::GetSize()));
		SPDY_LEAVE_IF_ERROR(initialFrame.Append(settingEntries));
	}
}

BOOL SpdySettingsController::HasNextFrame() const
{
	return !initialFrame.IsEmpty();
}

void SpdySettingsController::WriteNextFrameL(SpdyNetworkBuffer &target)
{
	target.AppendCopyDataL(initialFrame);
	initialFrame.Clear();
}

UINT8 SpdySettingsController::GetPriority() const
{
	return 0xff; // settings frame has always the highest priority
}

void SpdySettingsController::SettingsFrameLoaded(const SettingsFrameHeader *settingsFrame)
{
	leftToConsume = SettingsEntrySize * settingsFrame->GetNumberOfEntries();
	if (!privateMode && settingsFrame->GetFlags() == SSCF_CLEAR_PERSISTED_SETTINGS && g_spdy_settings_manager)
		g_spdy_settings_manager->ClearPersistedSettingsL(hostport);
}

BOOL SpdySettingsController::SetSetting(UINT32 id, UINT32 value)
{
	switch (id)
	{
	case SSI_UPLOAD_BANDWIDTH:
		uploadBandwidth = value;
		break;
	case SSI_DOWNLOAD_BANDWIDTH:
		downloadBandwidth = value;
		break;
	case SSI_ROUND_TRIP_TIME:
		roundTripTime = value;
		break;
	case SSI_MAX_CONCURRENT_STREAMS:
		maxConcurrentStreams = value;
		break;
	case SSI_CURRENT_CWND:
		currentCwnd = value;
		break;
	case SSI_DOWNLOAD_RETRANS_RATE:
		downloadRetransRate = value;
		break;
	case SSI_INITIAL_WINDOW_SIZE:
		initialWindowSize = value;
		break;
	default:
		OP_ASSERT(!"Not supported setting id!");
		return FALSE;
	}
	return TRUE;
}

BOOL SpdySettingsController::ConsumeDataL(OpData &data)
{
	size_t dataToRead = MIN(leftToConsume, data.Length());
	leftToConsume -= dataToRead;
	OpData readData(data, 0, dataToRead);
	ANCHOR(OpData, readData);
	data.Consume(dataToRead);
	SPDY_LEAVE_IF_ERROR(buffer.Append(readData));

	while (buffer.Length() >= SettingsEntrySize)
	{
		char buf[SettingsEntrySize];  /* ARRAY OK 2012-04-29 kswitalski */
		buffer.CopyInto(buf, SettingsEntrySize);
		buffer.Consume(SettingsEntrySize);

		SettingsEntry *se = reinterpret_cast<SettingsEntry*>(buf);
		UINT32 id = se->GetId(version);
		UINT8 flags = se->GetFlags(version);
		UINT32 value = se->GetValue();
	
		BOOL recognized = SetSetting(id, value);
		if (recognized && !privateMode && flags == SSF_PERSIST && g_spdy_settings_manager)
			g_spdy_settings_manager->PersistSettingL(hostport, id, value);
	}

	return leftToConsume != 0;
}

#endif // USE_SPDY

