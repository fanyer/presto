// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton, Huib Kleinhout
//
#include "core/pch.h"

#ifdef SUPPORT_SPEED_DIAL

#include "adjunct/quick/speeddial/SpeedDialData.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/util/misc.h"

#include "modules/debug/debug.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/thumbnails/thumbnailmanager.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SpeedDialData
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
SpeedDialData::SpeedDialData() :
	m_timeout(SPEED_DIAL_RELOAD_INTERVAL_DEFAULT),
	m_preview_refresh(0),
	m_reload_policy(SpeedDialData::Reload_NeverSoft),
	m_only_if_expired(SPEED_DIAL_RELOAD_ONLY_IF_EXPIRED_DEFAULT),
	m_reload_timer_running(FALSE),
	m_is_custom_title(FALSE)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SpeedDialData::SetURL(const OpStringC& url)
{
	const SpeedDialData* speed_dial_data = g_speeddial_manager->GetSpeedDialByUrl(url);
	if (speed_dial_data)
	{
		m_core_url = speed_dial_data->m_core_url;
		RETURN_IF_ERROR(m_url.Set(speed_dial_data->m_url));
	}
	else
	{
		// Resolve entered URL
		OpString resolved_url;
		BOOL is_resolved = g_url_api->ResolveUrlNameL(url, resolved_url, TRUE);
		if (!is_resolved)
			return OpStatus::ERR;

		URL empty_url;
		m_core_url = g_url_api->GetURL(empty_url, resolved_url, TRUE);
		RETURN_IF_ERROR(m_url.Set(resolved_url));
	}
	return OpStatus::OK;
}

OP_STATUS SpeedDialData::Set(const SpeedDialData& original, bool retain_uid)
{
	RETURN_IF_ERROR(SetURL(original.GetURL()));
	RETURN_IF_ERROR(SetDisplayURL(original.HasDisplayURL() ? original.GetDisplayURL() : UNI_L("")));
	RETURN_IF_ERROR(SetTitle(original.GetTitle(), original.m_is_custom_title));
	RETURN_IF_ERROR(SetExtensionID(original.m_extension_id));
	RETURN_IF_ERROR(m_partner_id.Set(original.m_partner_id));
	RETURN_IF_ERROR(SetReload(original.GetReloadPolicy(), original.GetReloadTimeout(), original.GetReloadOnlyIfExpired()));

	if (!retain_uid)
	{
		RETURN_IF_ERROR(m_unique_id.Set(original.GetUniqueID().CStr()));
	}

	m_core_url = original.m_core_url;

	return GenerateIDIfNeeded();
}

OpStringC SpeedDialData::GetExtensionID() const
{
	OpStringC wuid = GetExtensionWUID();
	if (wuid.HasContent())
	{
		OpGadget* extension = g_gadget_manager->FindGadget(wuid);
		OP_ASSERT(extension);
		return extension ? OpStringC(extension->GetGadgetId()) : OpStringC();
	}
	else
		return m_extension_id;
}

OpStringC SpeedDialData::GetExtensionWUID() const
{
	return m_extension_id.Find(UNI_L("wuid-")) == 0 ? OpStringC(m_extension_id) : OpStringC();
}

OP_STATUS SpeedDialData::GenerateIDIfNeeded(BOOL force, BOOL use_hash, INT32 position)
{
	// generate a unique id
	if(force || GetUniqueID().IsEmpty())
	{
		if(use_hash && position > 0)
		{
			// Generate a hash based on the position and url, only call on upgrade from < 11.10
			// See https://ssl.opera.com:8008/developerwiki/Opera_Link/Speeddial_2.0#General_notes
			OpString8 str8, url, md5;

			RETURN_IF_ERROR(url.SetUTF8FromUTF16(GetURL()));
			RETURN_IF_ERROR(str8.AppendFormat("%d%s", position, url.CStr()));
			RETURN_IF_ERROR(OpMisc::CalculateMD5Checksum(str8.CStr(), str8.Length(), md5));

			md5.MakeUpper();

			RETURN_IF_ERROR(m_unique_id.Set(md5.CStr()));
		}
		else
		{
			// generate a default unique ID
			RETURN_IF_ERROR(StringUtils::GenerateClientID(m_unique_id));
		}
	}
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SpeedDialData::SetReload(const ReloadPolicy policy, const int timeout, const BOOL only_if_expired)
{
	m_reload_policy = policy;
	m_reload_timer_running = timeout > 0 && (policy == Reload_PageDefined || policy == Reload_UserDefined);
	m_timeout = timeout <= 0 ? SPEED_DIAL_RELOAD_INTERVAL_DEFAULT : timeout;
	m_only_if_expired = only_if_expired;
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t SpeedDialData::BytesUsed()
{
	size_t size = sizeof(*this);
	size += m_url.Capacity() * sizeof(uni_char);
	size += m_title.Capacity() * sizeof(uni_char);
	size += m_partner_id.Capacity() * sizeof(uni_char);
	size += m_unique_id.Capacity() * sizeof(uni_char);
	size += m_extension_id.Capacity() * sizeof(uni_char);
	return size;
}

#ifdef _DEBUG
Debug& operator<<(Debug& dbg, const SpeedDialData& sd)
{
	return dbg.AddDbg(UNI_L("{ title=%s URL=%s ext-ID=%s ext-WUID=%s }"),
			sd.GetTitle().CStr(), sd.GetURL().CStr(),
			sd.GetExtensionID().CStr(), sd.GetExtensionWUID().CStr());
}
#endif // _DEBUG

#endif // SUPPORT_SPEED_DIAL
