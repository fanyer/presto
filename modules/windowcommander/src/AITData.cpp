/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef XML_AIT_SUPPORT

#include "modules/windowcommander/AITData.h"


AITApplicationDescriptor::~AITApplicationDescriptor()
{
	mhp_versions.DeleteAll();
}


AITApplication::~AITApplication()
{
	transports.DeleteAll();
	boundaries.DeleteAll();
}


AITData::~AITData()
{
	applications.DeleteAll();
}

#ifdef SELFTEST
#define _COMPARE(x) if (!(x == ait_data.x)) { error.Set(#x); return false; }
#define _COMPARE_USTR(x) if (uni_strcmp(x, ait_data.x) != 0) { error.Set(#x); return false; }
bool AITData::Compare(AITData &ait_data, OpString8 &error)
{
	_COMPARE(applications.GetCount());
	for (UINT32 a = 0; a < applications.GetCount(); a++)
	{
		_COMPARE(applications.Get(a)->name);
		_COMPARE(applications.Get(a)->org_id);
		_COMPARE(applications.Get(a)->app_id);

		_COMPARE(applications.Get(a)->descriptor.control_code);
		_COMPARE(applications.Get(a)->descriptor.visibility);
		_COMPARE(applications.Get(a)->descriptor.service_bound);
		_COMPARE(applications.Get(a)->descriptor.priority);
		_COMPARE(applications.Get(a)->descriptor.mhp_versions.GetCount());

		for (UINT32 m = 0; m < applications.Get(a)->descriptor.mhp_versions.GetCount(); m++)
		{
			_COMPARE(applications.Get(a)->descriptor.mhp_versions.Get(m)->profile);
			_COMPARE(applications.Get(a)->descriptor.mhp_versions.Get(m)->version_major);
			_COMPARE(applications.Get(a)->descriptor.mhp_versions.Get(m)->version_minor);
			_COMPARE(applications.Get(a)->descriptor.mhp_versions.Get(m)->version_micro);
		}
		_COMPARE(applications.Get(a)->location);
		_COMPARE(applications.Get(a)->usage);

		_COMPARE(applications.Get(a)->transports.GetCount());
		for (UINT32 t = 0; t < applications.Get(a)->transports.GetCount(); t++)
		{
			_COMPARE(applications.Get(a)->transports.Get(t)->protocol);
			_COMPARE(applications.Get(a)->transports.Get(t)->base_url);
			_COMPARE(applications.Get(a)->transports.Get(t)->remote);
			_COMPARE(applications.Get(a)->transports.Get(t)->original_network_id);
			_COMPARE(applications.Get(a)->transports.Get(t)->transport_stream_id);
			_COMPARE(applications.Get(a)->transports.Get(t)->service_id);
			_COMPARE(applications.Get(a)->transports.Get(t)->component_tag);
		}

		_COMPARE(applications.Get(a)->boundaries.GetCount());
		for (UINT32 b = 0; b < applications.Get(a)->boundaries.GetCount(); b++)
		{
			_COMPARE_USTR(applications.Get(a)->boundaries.Get(b)->CStr());
		}
	}

	return true;
}
#endif // SELFTEST

#endif // XML_AIT_SUPPORT
