/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include <CoreServices/CoreServices.h>

/**
 * ProfileOperaStartup/Shutdown write results to C:\klient\ if that folder exists
 */
class ProfileOperaData
{
	public:
		UINT64 m_unique_id;
		unsigned long long m_start_time;
};

static OpVector<ProfileOperaData> s_profile_opera_data;
static FILE* s_profile_opera_file = NULL;

void ProfileOperaPush(UINT64 unique_id) // PROFILE_OPERA_PUSH(x)
{
	if (s_profile_opera_file)
	{
		ProfileOperaData* data = new ProfileOperaData;
		Microseconds((UnsignedWide*) &data->m_start_time);
		data->m_unique_id = unique_id;

		s_profile_opera_data.Add(data);
	}
}

void ProfileOperaPop() // PROFILE_OPERA_POP(x)
{
	if (s_profile_opera_file && s_profile_opera_data.GetCount())
	{
		unsigned long long current_time;
		Microseconds((UnsignedWide*) &current_time);

		ProfileOperaData* data = s_profile_opera_data.Remove(s_profile_opera_data.GetCount()-1);

		unsigned long long long_delta = current_time - data->m_start_time;
		unsigned int delta = (unsigned long)((long_delta+500)/1000);

		if (delta >= 10)
		{
			for (UINT32 i = 0; i < s_profile_opera_data.GetCount(); i++)
			{
				fprintf(s_profile_opera_file, "    ");
			}
			fprintf(s_profile_opera_file,"%08llu: %4u\n", data->m_unique_id, delta);
			fflush(s_profile_opera_file);
		}

		delete data;
	}
}

void ProfileOperaPopThenPush(UINT64 unique_id) // PROFILE_OPERA_POP_THEN_PUSH(x)
{
	ProfileOperaPop();
	ProfileOperaPush(unique_id);
}

void ProfileOperaPrepare() // PROFILE_OPERA_PREPARE()
{
	FSRef ref;
	char path[MAX_PATH];
	const char* klient_path = "/klient/opera_profile.txt";

	FSFindFolder( kUserDomain, kApplicationSupportFolderType, kDontCreateFolder, &ref );

	FSRefMakePath( &ref, (UInt8*)&path, MAX_PATH);
	
	char tmppath[MAX_PATH];
	if (!CFStringGetCString(CFBundleGetIdentifier(CFBundleGetMainBundle()), tmppath, MAX_PATH, kCFStringEncodingUTF8)) {
		OP_ASSERT(!"Error copying bundleid to string");
		return;
	}
	if (op_strlen(path) + op_strlen(tmppath) + op_strlen(klient_path) + 1 >= MAX_PATH) {
		OP_ASSERT(!"profile storage path too long");
		return;
	}

	op_strcat(path, "/");
	op_strcat(path, tmppath);
	op_strcat(path, klient_path);

	s_profile_opera_file = fopen(path,"w+");
}
