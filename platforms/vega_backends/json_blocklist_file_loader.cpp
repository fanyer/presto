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

#include "modules/ecmascript/json_parser.h"
#include "modules/util/opautoptr.h"
#include "modules/util/opfile/opfile.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "platforms/vega_backends/json_blocklist_file_loader.h"

/**
   handle to file data - allocate storage for the entire file
 */
struct UniFileDataHandle
{
	UniFileDataHandle()
	: data(0), length(0)
	{}
	~UniFileDataHandle() { Close(); }
	OP_STATUS Load(OpFile* file);
	void Close();

	const uni_char* Data() { return (const uni_char*)data; }
	unsigned Length() { return length; }

private:
	uni_char* data;
	unsigned length;
};
OP_STATUS UniFileDataHandle::Load(OpFile* file)
{
	OP_ASSERT(!data);
	OP_ASSERT(!length);
	OP_ASSERT(file && file->IsOpen());

	// load file into memory
	OpFileLength flen, bytes_read;
	RETURN_IF_ERROR(file->GetFileLength(flen));
	if (!flen || flen > INT_MAX)
		return OpStatus::ERR;
	OpAutoArray<char> d(OP_NEWA(char, static_cast<unsigned>(flen)));
	RETURN_OOM_IF_NULL(d.get());
	RETURN_IF_ERROR(file->Read(d.get(), flen, &bytes_read));
	if (bytes_read != flen)
		return OpStatus::ERR;

	// convert from UTF-8 to UTF-16
	UTF8toUTF16Converter converter;
	int size = converter.Convert(d.get(), static_cast<int>(flen), NULL, INT_MAX, NULL);
	length = size / 2 + 1;
	data = OP_NEWA(uni_char, length);
	RETURN_OOM_IF_NULL(data);
	data[0] = 0xfeff; // BOM
	converter.Convert(d.get(), static_cast<int>(flen), data + 1, size, NULL);

	return OpStatus::OK;
}
void UniFileDataHandle::Close()
{
	if (!data)
		return;
	OP_DELETEA(data);
	data = 0;
	length = 0;
}

#define NEXT_STATE m_json_state = static_cast<JSONState>(static_cast<int>(m_json_state) + 1)

// static
OP_STATUS VEGABlocklistFileLoader::Create(VEGABlocklistFileLoader*& loader, VEGABlocklistFileLoaderListener* listener)
{
	JSONBlocklistFileLoader* l = OP_NEW(JSONBlocklistFileLoader, (listener));
	if (!l)
		return OpStatus::ERR_NO_MEMORY;
	loader = l;
	return OpStatus::OK;
}

OP_STATUS JSONBlocklistFileLoader::Load(OpFile* file)
{
	UniFileDataHandle handle;
	RETURN_IF_ERROR(handle.Load(file));

	const uni_char* data = handle.Data();
	unsigned length = handle.Length();
	// consume BOM
	OP_ASSERT(*data == 0xfeff);
	++ data;
	-- length;

	OP_ASSERT(m_json_state == Start);
	NEXT_STATE;
	JSONParser parser(this);
	const OP_STATUS status = parser.Parse(data, length);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(m_current_entry);
		OP_DELETE(m_current_driver_entry);
		OP_DELETE(m_current_regex);
		OP_DELETEA(m_current_key);
	}
	else
	{
		OP_ASSERT(!m_current_entry);
		OP_ASSERT(!m_current_driver_entry);
		OP_ASSERT(!m_current_regex);
		OP_ASSERT(!m_current_key);
		OP_ASSERT(m_json_state == Done);
	}
	return status;
}



OP_STATUS JSONBlocklistFileLoader::EnterObject()
{
	++ m_object_level;

	if (!((m_json_state == EnterTop   && m_object_level == 1) ||
	      (m_json_state == EnterDriverLinkList && m_object_level == 2) ||
	      (m_json_state == EnterDriverEntry && m_object_level == 3) ||
	      (m_json_state == EnterEntry && m_object_level == 2) ||
		  (m_json_state == EnterRegex && m_object_level >= 3)))
		return OpStatus::ERR;

	if (m_json_state == EnterEntry)
	{
		OP_ASSERT(!m_current_entry);
		OP_ASSERT(!m_current_regex);
		OP_ASSERT(!m_current_key);
		// create storage for new entry
		RETURN_OOM_IF_NULL(m_current_entry = OP_NEW(VEGABlocklistFileEntry, ()));
		m_current_regex_collection = m_current_entry;
	}
	else if (m_json_state == EnterDriverEntry)
	{
		OP_ASSERT(!m_current_entry);
		OP_ASSERT(!m_current_regex);
		OP_ASSERT(!m_current_key);
		// create storage for new entry
		RETURN_OOM_IF_NULL(m_current_driver_entry = OP_NEW(VEGABlocklistDriverLinkEntry, ()));
		m_current_regex_collection = m_current_driver_entry;
	}
	else if (m_json_state == EnterRegex)
	{
		OP_ASSERT(m_current_entry || m_current_driver_entry);
		OP_ASSERT(!m_current_regex);
		OP_ASSERT(m_current_key);
		RETURN_OOM_IF_NULL(m_current_regex = OP_NEW(VEGABlocklistRegexEntry, ()));
		m_current_regex->m_key = m_current_key;
		m_current_key = 0; // m_current_regex now owns m_current_key
	}

	NEXT_STATE;
	return OpStatus::OK;
}

OP_STATUS JSONBlocklistFileLoader::LeaveObject()
{
	-- m_object_level;

	if (!m_object_level &&
	    (m_json_state == ListEntryOrLeaveTop || m_json_state == DriverLinkListOrListEntryOrLeaveTop))
		m_json_state = LeaveTop;
	else if (m_object_level == 1 &&
	         m_json_state == DriverEntryOrLeaveDriverLinkList)
		m_json_state = LeaveDriverLinkList;
	else if (m_object_level == 1 &&
	         (m_json_state > LastMandatoryInEntry && m_json_state < LeaveEntry))
		m_json_state = LeaveEntry;
	else if (m_json_state == Comp1OrLeaveRegex || m_json_state == Comp2OrLeaveRegex)
		m_json_state = LeaveRegex;

	if (!((m_json_state == LeaveEntry && m_object_level == 1) ||
	      (m_json_state == LeaveDriverLinkList && m_object_level == 1) ||
	      (m_json_state == LeaveDriverEntry && m_object_level == 2) ||
	      (m_json_state == LeaveRegex && m_object_level >= 2) ||
	      (m_json_state == LeaveTop && m_object_level == 0)))
		return OpStatus::ERR;


	// entry successfully read, add to vector
	if (m_json_state == LeaveEntry)
	{
		OP_ASSERT(!m_current_regex);
		RETURN_IF_ERROR(m_listener->OnElementLoaded(m_current_entry));
		m_current_entry = 0; // ownership passed to listener
		m_current_regex_collection = 0;
		m_json_state = ListEntryOrLeaveTop;
	}
	// driver entry successfully read, add to vector
	else if (m_json_state == LeaveDriverEntry)
	{
		OP_ASSERT(!m_current_regex);
		RETURN_IF_ERROR(m_listener->OnDriverEntryLoaded(m_current_driver_entry));
		m_current_driver_entry = 0; // ownership passed to listener
		m_current_regex_collection = 0;
		m_json_state = DriverEntryOrLeaveDriverLinkList;
	}
	// regex successfully read, add to entry
	else if (m_json_state == LeaveRegex)
	{
		OP_ASSERT(m_current_entry || m_current_driver_entry);
		OP_ASSERT(m_current_regex);
		OP_ASSERT(m_current_regex_collection);
		RETURN_IF_ERROR(m_current_regex_collection->AddEntry(m_current_regex));
		m_current_regex = 0; // ownership passed to m_current_entry
		m_json_state = m_regex_return_state;
	}
	else
		NEXT_STATE;

	return OpStatus::OK;
}


OP_STATUS JSONBlocklistFileLoader::AttributeName(const OpString& str)
{
	if (m_json_state == Comp1OrLeaveRegex || m_json_state == Comp2OrLeaveRegex)
	{
		OP_ASSERT(m_current_regex);
		NEXT_STATE;
	}

	RETURN_IF_ERROR(CheckAttrName(str));

	// clone key for later use
	if (m_json_state == AttrRegexKey || m_json_state == AttrArbStrKey)
	{
		RETURN_OOM_IF_NULL(m_current_key = UniSetNewStr(str));
	}

#ifdef DEBUG_ENABLE_OPASSERT
	if (m_json_state == AttrVersion || m_json_state == AttrDriverLinkList || m_json_state == AttrDriverEntry || m_json_state == AttrURLKey || m_json_state == AttrListEntry)
	{
		OP_ASSERT(!m_current_entry);
		OP_ASSERT(!m_current_regex);
		OP_ASSERT(!m_current_key);
	}
	else
	{
		OP_ASSERT(m_current_entry|| m_current_driver_entry);
	}

	if (m_json_state == AttrRegexKey)
	{
		OP_ASSERT(!m_current_regex);
		OP_ASSERT(m_current_key);
	}
	else if (m_json_state == AttrRegex)
	{
		OP_ASSERT(m_current_regex);
		OP_ASSERT(!m_current_key);
	}
#endif // DEBUG_ENABLE_OPASSERT

	NEXT_STATE;
	return OpStatus::OK;
}

static inline
OP_STATUS read_status(VEGABlocklistDevice::BlocklistStatus &status, const uni_char* str)
{
	if (!uni_stricmp(str, "supported"))
	{
		status = VEGABlocklistDevice::Supported;
		return OpStatus::OK;
	}
	if (!uni_stricmp(str, "blocked driver version"))
	{
		status = VEGABlocklistDevice::BlockedDriverVersion;
		return OpStatus::OK;
	}
	if (!uni_stricmp(str, "blocked device"))
	{
		status = VEGABlocklistDevice::BlockedDevice;
		return OpStatus::OK;
	}
	if (!uni_stricmp(str, "discouraged"))
	{
		status = VEGABlocklistDevice::Discouraged;
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

OP_STATUS JSONBlocklistFileLoader::CheckAttrName(const OpString& str)
{
	switch (m_json_state)
	{
	case AttrVersion:
		if (str == UNI_L("blocklist version"))
			return OpStatus::OK;
		break;

	case DriverEntryOrLeaveDriverLinkList:
		m_json_state = AttrDriverEntry;
		if (str == UNI_L("driver entry"))
			return OpStatus::OK;
		break;

	case DriverLinkListOrListEntryOrLeaveTop:
		m_json_state = AttrDriverLinkList;
		if (str == UNI_L("driver links"))
			return OpStatus::OK;
		// fall-through

	// new entry
	case ListEntryOrLeaveTop:
		m_json_state = AttrListEntry;
		if (str == UNI_L("list entry"))
			return OpStatus::OK;
		break;
	case RegexKeyOrStatus2D:
		if (str == UNI_L("status2d"))
		    m_json_state = AttrStatus2D;
		else
		{
			m_regex_return_state = RegexKeyOrStatus2D;
		    m_json_state = AttrRegexKey;
		}
		return OpStatus::OK;

	case RegexKeyOrURL:
		if (str == UNI_L("URL"))
		    m_json_state = AttrURLKey;
		else
		{
			m_regex_return_state = RegexKeyOrURL;
		    m_json_state = AttrRegexKey;
		}
		return OpStatus::OK;

	case AttrRegex:
		if (str == UNI_L("regex"))
			return OpStatus::OK;
		break;

	case AttrComp1:
	case AttrComp2:
	{
		OP_ASSERT(m_current_regex);
		const VEGABlocklistRegexEntry::Comparison comp = VEGABlocklistRegexEntry::GetComp(str);
		if (comp != VEGABlocklistRegexEntry::CompCount)
		{
			m_current_regex->m_comp[m_json_state == AttrComp2].m_comp = comp;
			return OpStatus::OK;
		}
		break;
	}

	case Reason2D_Optional:
		m_json_state = AttrReason2D;
		// fall-through
	case AttrReason2D:
		if (str == UNI_L("reason2d"))
			return OpStatus::OK;
		m_json_state = Status3D_Optional;
		// fall-through
	case Status3D_Optional:
		m_json_state = AttrStatus3D;
		// fall-through
	case AttrStatus3D:
		if (str == UNI_L("status3d"))
			return OpStatus::OK;
		m_json_state = Reason3D_Optional;
		// fall-through
	case Reason3D_Optional:
		m_json_state = AttrReason3D;
		// fall-through
	case AttrReason3D:
		if (str == UNI_L("reason3d"))
			return OpStatus::OK;
		m_json_state = ArbStrKey_Optional;
		// fall-through
	case ArbStrKey_Optional:
		m_json_state = AttrArbStrKey;
		return OpStatus::OK;
		break;
	}

	return OpStatus::ERR;
}

OP_STATUS JSONBlocklistFileLoader::String(const OpString& str)
{
	uni_char* val;
	switch (m_json_state)
	{
	case StrURL:
		OP_ASSERT(m_current_driver_entry);
		RETURN_OOM_IF_NULL(m_current_driver_entry->m_driver_link = UniSetNewStr(str.CStr()));
		break;

	case StrRegex:
		OP_ASSERT(m_current_regex);
		RETURN_OOM_IF_NULL(m_current_regex->m_exp = UniSetNewStr(str));
		break;

	case StrArbStr:
		val = UniSetNewStr(str.CStr());
		RETURN_OOM_IF_NULL(val);
		RETURN_IF_ERROR(m_current_entry->AddEntry(m_current_key, val));
		m_current_key = 0; // ownership passed to entry
		m_json_state = ArbStrKey_Optional;
		return OpStatus::OK;

	case StrVersion:
		NEXT_STATE;
		return m_listener->OnVersionLoaded(str);

	case StrStatus2D:
		RETURN_IF_ERROR(read_status(m_current_entry->status2d, str));
		m_current_entry->status3d = m_current_entry->status2d; // 3d status defaults to 2d status
		break;

	case StrReason2D:
		RETURN_IF_ERROR(UniSetStr(m_current_entry->reason2d, str));
		break;

	case StrStatus3D:
		RETURN_IF_ERROR(read_status(m_current_entry->status3d, str));
		break;

	case StrReason3D:
		RETURN_IF_ERROR(UniSetStr(m_current_entry->reason3d, str));
		break;

	default:
		return OpStatus::ERR;
	}

	NEXT_STATE;
	return OpStatus::OK;
}

OP_STATUS JSONBlocklistFileLoader::Number(double num)
{
	if (m_json_state != CompValue1N &&
	    m_json_state != CompValue2N)
		return OpStatus::ERR;

	OP_ASSERT(m_current_regex);
	// no next state here, we're in an array
	return m_current_regex->m_comp[m_json_state == CompValue2N].m_values.Add((INT32)num);
}

OP_STATUS JSONBlocklistFileLoader::EnterArray()
{
	OP_ASSERT(m_json_state == EnterComp1Array || m_json_state == EnterComp2Array);
	OP_ASSERT(m_current_regex);
	NEXT_STATE;
	return OpStatus::OK;
}
OP_STATUS JSONBlocklistFileLoader::LeaveArray()
{
	NEXT_STATE;
	OP_ASSERT(m_json_state == LeaveComp1Array || m_json_state == LeaveComp2Array);
	OP_ASSERT(m_current_regex);
	NEXT_STATE;
	return OpStatus::OK;
}

#undef NEXT_STATE

#endif // VEGA_BACKENDS_USE_BLOCKLIST
