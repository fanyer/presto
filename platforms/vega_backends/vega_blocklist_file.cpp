/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKENDS_USE_BLOCKLIST

#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/regexp/include/regexp_api.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opstring.h"
#include "platforms/vega_backends/vega_blocklist_file.h"

void DeleteBlocklistEntryPair(const void* key, void* val)
{
	OP_DELETEA((uni_char*)key);
	OP_DELETEA((uni_char*)val);
}

// static
VEGABlocklistRegexEntry::Comparison VEGABlocklistRegexEntry::GetComp(const uni_char* comp)
{
	if (!uni_strcmp(comp, UNI_L("==")))
		return EqualTo;
	if (!uni_strcmp(comp, UNI_L("!=")))
		return NotEqualTo;
	if (!uni_strcmp(comp, UNI_L(">")))
		return GreaterThan;
	if (!uni_strcmp(comp, UNI_L(">=")))
		return GreaterThanOrEqual;
	if (!uni_strcmp(comp, UNI_L("<")))
		return LessThan;
	if (!uni_strcmp(comp, UNI_L("<=")))
		return LessThanOrEqual;

	OP_ASSERT(!"unexpected comparison string");
	return CompCount;
}

// determines whether l1 is less than (<0), equal to (0) or greater than (>0) l2
static inline
INT32 compare_versions(const OpINT32Vector& l1, const OpINT32Vector& l2)
{
	const UINT32 n1 = l1.GetCount(), n2 = l2.GetCount();
	for (UINT32 i = 0; i < n1 && i < n2; ++i)
	{
		const INT32 v1 = l1.Get(i), v2 = l2.Get(i);
		if (v1 != v2)
			return v1 - v2;
	}
	return n1 - n2;
}

BOOL VEGABlocklistRegexEntry::RegexComp::Matches(const OpINT32Vector& values)
{
	// let empty comparison match all - eases implemetation
	if (!m_values.GetCount())
		return TRUE;

	const INT32 vercomp = compare_versions(values, m_values);
	switch (m_comp)
	{
	case EqualTo:            return vercomp == 0; break;
	case NotEqualTo:         return vercomp != 0; break;
	case GreaterThan:        return vercomp >  0; break;
	case GreaterThanOrEqual: return vercomp >= 0; break;
	case LessThan:           return vercomp <  0; break;
	case LessThanOrEqual:    return vercomp <= 0; break;
	}

	OP_ASSERT(!"unexpected comparison");
	return FALSE;
}

OP_BOOLEAN VEGABlocklistRegexEntry::Matches(const uni_char* str)
{
	// compile regex
	OpREFlags flags;
	flags.multi_line = FALSE;
	flags.case_insensitive = TRUE;
	flags.ignore_whitespace = FALSE;
	OpRegExp* exp;
	RETURN_IF_ERROR(OpRegExp::CreateRegExp(&exp, m_exp, &flags));
	OpAutoPtr<OpRegExp> _exp(exp);

	// match regex
	OpREMatchLoc* matchlocs;
	int nmatches;
	RETURN_IF_ERROR(exp->Match(str, &nmatches, &matchlocs));
	OpAutoArray<OpREMatchLoc> _matchlocs(matchlocs);

	if (nmatches <= 0 ||
	    matchlocs[0].matchloc || (size_t)matchlocs[0].matchlen != uni_strlen(str)) // must match entire string!
		return OpBoolean::IS_FALSE;

	if (nmatches > 1) // there were subexpressions
	{
		// collect values
		OpINT32Vector vals;
		for (int i = 1; i < nmatches; ++i)
		{
			if (matchlocs[i].matchloc != REGEXP_NO_MATCH && matchlocs[i].matchlen != REGEXP_NO_MATCH)
			{
				// copy match, so we don't trickle past end
				OpString8 s;
				RETURN_IF_ERROR(s.Set(str + matchlocs[i].matchloc, matchlocs[i].matchlen));
				INT32 val = (INT32)op_atoi(s);
				RETURN_IF_ERROR(vals.Add((INT32)val));
			}
		}

		// compare values
		for (int i = 0; i < 2; ++i)
			if (!m_comp[i].Matches(vals))
				return OpBoolean::IS_FALSE;
	}

	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN VEGABlocklistRegexCollection::Matches(VEGABlocklistDevice::DataProvider* provider)
{
	OP_ASSERT(provider);

	OpHashIterator* it = regex_entries.GetIterator();
	if (!it)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoPtr<OpHashIterator> _it(it);

	OP_STATUS status;
	for (status = it->First(); OpStatus::IsSuccess(status); status = it->Next())
	{
		const uni_char* key = (const uni_char*)it->GetKey();
		VEGABlocklistRegexEntry* exp = (VEGABlocklistRegexEntry*)it->GetData();
		OP_ASSERT(key);
		OP_ASSERT(exp);

		OpString valstr;
		RETURN_IF_ERROR(provider->GetValueForKey(key, valstr));
		if (!valstr.CStr())
			return OpBoolean::IS_FALSE;

		const OP_BOOLEAN res = exp->Matches(valstr.CStr());
		if (res == OpBoolean::IS_FALSE || OpStatus::IsError(res))
			return res;
	}
	if (OpStatus::IsMemoryError(status))
		return OpStatus::ERR_NO_MEMORY;

	return OpBoolean::IS_TRUE;
}

// static
const uni_char* VEGABlocklistFile::GetName(VEGABlocklistDevice::BlocklistType type)
{
	switch (type)
	{
	case VEGABlocklistDevice::D3d10:
		return UNI_L("windows-direct3d-10.blocklist.json");
		break;
	case VEGABlocklistDevice::UnixGL:
		return UNI_L("unix-opengl.blocklist.json");
		break;
	case VEGABlocklistDevice::WinGL:
		return UNI_L("windows-opengl.blocklist.json");
		break;
	case VEGABlocklistDevice::MacGL:
		return UNI_L("mac-opengl.blocklist.json");
		break;

#ifdef SELFTEST
	case VEGABlocklistDevice::SelftestBlocklistFile:
		return UNI_L("selftest-blocklist.json");
		break;
#endif // SELFTEST
	}

	return 0;
}

#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH

// static
OP_STATUS VEGABlocklistFile::GetURL(VEGABlocklistDevice::BlocklistType type, OpString& s)
{
#ifdef SELFTEST
	if (type == VEGABlocklistDevice::SelftestBlocklistFile)
		return s.Set(g_vega_backends_module.m_selftest_blocklist_url);
#endif // SELFTEST

	const uni_char* name = GetName(type);
	if (!name)
		return OpStatus::ERR;

	// HACK: apparently it's not possible to fetch .json files
	const size_t namelen = uni_strlen(name);
	OP_ASSERT(namelen > 5 && !uni_strcmp(name + namelen - 5, UNI_L(".json")));

	RETURN_IF_ERROR(s.Set(g_pccore->GetStringPref(PrefsCollectionCore::BlocklistLocation)));
	RETURN_IF_ERROR(s.Append(name, namelen - 5));
	return s.Append(UNI_L(".txt"));
}

// static
void VEGABlocklistFile::Fetch(VEGABlocklistDevice::BlocklistType type, unsigned long delay)
{
	g_vega_backends_module.FetchBlocklist(type, delay);
}

// static
void VEGABlocklistFile::FetchWhenExpired(VEGABlocklistDevice::BlocklistType type, time_t mod)
{
	const time_t expiry_seconds = g_pccore->GetIntegerPref(PrefsCollectionCore::BlocklistRefetchDelay);

	time_t seconds_left = expiry_seconds;
	time_t now = (time_t)(g_op_time_info->GetTimeUTC() / 1000);
	if (now < mod + expiry_seconds)
		seconds_left = mod + expiry_seconds - now;

	Fetch(type, (unsigned long)seconds_left * 1000/*ms*/);
}

void VEGABlocklistFile::FetchLater(VEGABlocklistDevice::BlocklistType type)
{
	// something went wrong - re-fetch later
	const unsigned int retry_seconds = g_pccore->GetIntegerPref(PrefsCollectionCore::BlocklistRetryDelay);
	Fetch(type, retry_seconds * 1000/*ms*/);
}

// static
OP_STATUS VEGABlocklistFile::OnBlocklistFetched(URL url, VEGABlocklistDevice::BlocklistType type)
{
	unsigned long res = url.PrepareForViewing(TRUE);
	if (res)
		return res == MSG_OOM_CLEANUP ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;

	OpString path;
	RETURN_IF_ERROR(url.GetAttribute(URL::KFilePathName, path, TRUE));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(path));
	time_t fetched_time;
	OP_STATUS status = CheckFile(&file, &fetched_time);

	if (OpStatus::IsSuccess(status))
	{
		// replace old blocklist with fetched
		const uni_char* name = GetName(type);
		OP_ASSERT(name);
		OpFile out;
		status = out.Construct(name, VEGA_BACKENDS_BLOCKLIST_FETCHED_FOLDER);
		if (OpStatus::IsSuccess(status))
			status = out.CopyContents(&file, FALSE);
	}
	else
	{
		// downloaded blocklist file corrupt
		OP_ASSERT(!"corrupt blocklist file");
	}

	if (OpStatus::IsError(status))
	{
		FetchLater(type);
		return status;
	}

	FetchWhenExpired(type, fetched_time);

	// TODO: pass new blocklist status to VOP so it can recreate its
	// 3dDevice if necessary. this is currently not possible, as it
	// requires VOP to be able to recreate its backend.

	return OpStatus::OK;
}

#endif // VEGA_BACKENDS_BLOCKLIST_FETCH

// static
OP_STATUS VEGABlocklistFile::CheckFile(OpFile* file
#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	, time_t* fetched_time/* = 0*/
#endif
	)
{
	RETURN_IF_ERROR(file->Open(OPFILE_READ));

	VEGABlocklistFile bf;
	OP_STATUS status = bf.Load(file);
	file->Close();

#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	if (fetched_time)
		*fetched_time = bf.GetFetched();
#endif

	return status;
}

// static
OP_STATUS VEGABlocklistFile::OpenFile(VEGABlocklistDevice::BlocklistType type, OpFile* file, BOOL* from_resources)
{
	OP_ASSERT(file);

	const uni_char* name = GetName(type);
	OP_ASSERT(name);

	*from_resources = FALSE;
	OP_STATUS status = file->Construct(name, VEGA_BACKENDS_BLOCKLIST_FETCHED_FOLDER);
	if (OpStatus::IsSuccess(status))
	{
		status = file->Open(OPFILE_READ);

		// if there is no blocklist file present in the home folder check
		// resources folder, and trigger immediate refetch
		if (OpStatus::IsError(status))
		{
			*from_resources = TRUE;
			status = file->Construct(name, VEGA_BACKENDS_BLOCKLIST_SHIPPED_FOLDER);
			if (OpStatus::IsSuccess(status))
				status = file->Open(OPFILE_READ);
		}
	}

	return status;
}



OP_STATUS VEGABlocklistFile::Load(VEGABlocklistDevice::BlocklistType type)
{
	OP_ASSERT(GetName(type));

	BOOL force_immediate_fetch;
	OpFile file;
	OP_STATUS status = OpenFile(type, &file, &force_immediate_fetch);

	if (OpStatus::IsSuccess(status))
		status = Load(&file);

#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	// trigger fetch as necessary
	if (OpStatus::IsError(status) || force_immediate_fetch)
		Fetch(type, 0);
	else
		FetchWhenExpired(type, GetFetched());
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH

	return status;
}

OP_STATUS VEGABlocklistFile::Load(OpFile* file)
{
	OP_ASSERT(file && file->IsOpen());

#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	time_t mod;
	// get last-modified, in UTC seconds
	if (OpStatus::IsSuccess(file->GetLastModified(mod)))
		m_blocklist_fetched = mod;
	else
		m_blocklist_fetched = 0;
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH

	VEGABlocklistFileLoader* file_loader;
	OP_STATUS status = VEGABlocklistFileLoader::Create(file_loader, this);
	if (OpStatus::IsSuccess(status))
	{
		status = file_loader->Load(file);
		OP_DELETE(file_loader);
	}
	return status;
}

OP_STATUS VEGABlocklistFile::OnDriverEntryLoaded(VEGABlocklistDriverLinkEntry* driver_link)
{
	return m_driver_links.Add(driver_link);
}

OP_STATUS VEGABlocklistFile::OnElementLoaded(VEGABlocklistFileEntry* entry)
{
	const OP_STATUS status = m_entries.Add(entry);
	if (OpStatus::IsError(status))
		OP_DELETE(entry);
	return status;
}

template <typename T>
OP_STATUS FindMatchingEntry(VEGABlocklistDevice::DataProvider* provider, const OpAutoVector<T>& entries, T*& entry)
{
	OP_ASSERT(provider);
	entry = 0;
	UINT32 idx = 0;
	for (T* e; (e = entries.Get(idx)) != NULL; ++idx)
	{
		const OP_BOOLEAN match = e->Matches(provider);
		RETURN_IF_ERROR(match);
		if (match == OpBoolean::IS_TRUE)
		{
			entry = e;
			break;
		}
	}
	return OpStatus::OK;
}

OP_STATUS VEGABlocklistFile::FindMatchingEntry(VEGABlocklistDevice::DataProvider* const data_provider, VEGABlocklistFileEntry*& entry) const
{
	return ::FindMatchingEntry(data_provider, m_entries, entry);
}

OP_STATUS VEGABlocklistFile::FindMatchingDriverLink(VEGABlocklistDevice::DataProvider* const data_provider, VEGABlocklistDriverLinkEntry*& entry) const
{
	return ::FindMatchingEntry(data_provider, m_driver_links, entry);
}

#endif // VEGA_BACKENDS_USE_BLOCKLIST
