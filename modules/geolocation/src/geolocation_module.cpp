/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef GEOLOCATION_SUPPORT

#include "modules/geolocation/src/geo_impl.h"

GeolocationModule::GeolocationModule()
	: m_pimpl(NULL)
{
}

//////////////////////////////////////////////////////////////////////////

void
GeolocationModule::InitL(const OperaInitInfo& info)
{
	m_pimpl = OP_NEW_L(GeolocationImplementation, ());
	if (OpStatus::IsError(m_pimpl->Construct()))
	{
		OP_DELETE(m_pimpl); m_pimpl = NULL;
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}
}

void
GeolocationModule::Destroy()
{
	OP_DELETE(m_pimpl); m_pimpl = NULL;
}

OpGeolocation *
GeolocationModule::GetGeolocationSingleton()
{
	return static_cast<OpGeolocation*>(m_pimpl);
}

#endif // GEOLOCATION_SUPPORT
