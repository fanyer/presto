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

#ifndef _SPDY_SETTINGSMANAGER_H_
#define _SPDY_SETTINGSMANAGER_H_

struct PersistedSetting;
struct SettingsTable;

/**
 * Class responsible for reading/writing persisted SPDY settings.
 * Quoting the spec:
 * "A SETTINGS frame contains a set of id/value pairs for communicating configuration data about how the two endpoints may communicate.
 * SETTINGS frames can be sent at any time by either endpoint, are optionally sent, and fully asynchronous.
 * Further, when the server is the sender, the sender can request that configuration data be persisted by the client across SPDY sessions
 * and returned to the server in future communications."
 */
class SpdySettingsManager
{
	OpString8HashTable<SettingsTable> storage;
	BOOL loaded;
	BOOL updated;

	void LoadFileL();
	void WriteFileL();
public:
	SpdySettingsManager();
	~SpdySettingsManager();

	void PersistSettingL(const char* hostport, UINT32 settingId, UINT32 value);
	void GetPersistedSettingsL(const char* hostport, List<PersistedSetting> &target);
	void ClearPersistedSettingsL(const char* hostport);
	void ClearAllL();
};

struct SettingsTable
{
	OpString8 hostport;
	OpUINT32ToUINT32HashTable idvalues;
	time_t lastTimeUsed;
};

struct PersistedSetting: public ListElement<PersistedSetting>
{
	UINT32 settingId;
	UINT32 value;
};

#endif // _SPDY_SETTINGSMANAGER_H_