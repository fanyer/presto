/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_NETWORK

#include "platforms/posix/posix_network_interface.h"
#include "platforms/posix/net/posix_interface.h"
#include "modules/pi/OpSystemInfo.h" // TODO: remove GetDefaultHTTPInterface

class PosixNetworkInterface
	: public OpNetworkInterface
	, public PosixNetworkAddress
	, public Link
{
	unsigned int const m_index;
#ifdef PI_NETWORK_INTERFACE_INSPECTION
	class StringWithState {
	private:
		const OpString m_string;
		OP_STATUS const m_state;
	public:
		StringWithState(const char* string)
			: m_state(string
					  ? PosixNativeUtil::FromNative(string,
													const_cast<OpString*>(&m_string))
					  : OpStatus::ERR_NULL_POINTER)
			{}

		StringWithState(const StringWithState& other)
			: m_state(OpStatus::IsSuccess(other.m_state)
					  ? const_cast<OpString*>(&m_string)->Set(other.m_string)
					  : other.m_state)
			{}

		OP_STATUS GetString(OpString* string) const
		{
			RETURN_IF_ERROR(m_state);
			OP_ASSERT(m_string.HasContent());
			return string->Set(m_string);
		}
	};

	const StringWithState m_name;
	const OpNetworkInterface::NetworkType m_type;
	const StringWithState m_apn;
	const StringWithState m_ssid;
#endif
	bool m_up;
public:
	PosixNetworkInterface(unsigned int index,
#ifdef PI_NETWORK_INTERFACE_INSPECTION
						  const char * name,
						  OpNetworkInterface::NetworkType type,
						  const char* apn,
						  const char* ssid,
#endif
						  bool up)
		: m_index(index)
#ifdef PI_NETWORK_INTERFACE_INSPECTION
		, m_name(name)
		, m_type(type)
		, m_apn(apn)
		, m_ssid(ssid)
#endif
		, m_up(up) {}

	/**
	 * @name Implementation of OpNetworkInterface
	 * @{
	 */

	virtual OP_STATUS GetAddress(OpSocketAddress* address)
		{ return address->Copy(this); }
	virtual OpNetworkInterfaceStatus GetStatus()
		{ return m_up ? NETWORK_LINK_UP : NETWORK_LINK_DOWN; }

#ifdef PI_NETWORK_INTERFACE_INSPECTION
	virtual OP_STATUS GetId(OpString* name) { return m_name.GetString(name); }

	// UNKNOWN, ETHERNET, GPRS, EDGE, IRDA, EVDO: defined in OpNetworkInterface.h
	virtual OpNetworkInterface::NetworkType GetNetworkType() { return m_type; }

	virtual OP_STATUS GetAPN(OpString* apn) { return m_apn.GetString(apn); }
	virtual OP_STATUS GetSSID(OpString* ssid) { return m_ssid.GetString(ssid); }
#endif // PI_NETWORK_INTERFACE_INSPECTION

	/** @} */ // Implementation of OpNetworkInterface

	// Implementation details:
	unsigned int Index() const { return m_index; }
	void SetStatus(bool up) { m_up = up; }
	PosixNetworkInterface *Suc()
		{ return static_cast<PosixNetworkInterface *>(Link::Suc()); }

	// Needed by GetDefaultHTTPInterface (which is going away)
	PosixNetworkInterface(const PosixNetworkInterface* src)
		: m_index(src->m_index)
#ifdef PI_NETWORK_INTERFACE_INSPECTION
		, m_name(src->m_name)
		, m_type(src->m_type)
		, m_apn(src->m_apn)
		, m_ssid(src->m_ssid)
#endif
		, m_up(src->m_up) {}
};

#ifdef POSIX_PARTIAL_NETMAN
/* static */
OP_STATUS OpNetworkInterfaceManager::Create(OpNetworkInterfaceManager** new_object,
											OpNetworkInterfaceListener*)
{
	*new_object = 0;
	PosixNetworkInterfaceManager* manager = OP_NEW(PosixNetworkInterfaceManager, ());

	if (!manager)
		return OpStatus::ERR_NO_MEMORY;

#ifdef POSIX_NETIF_ITERATE_ONCE
	OP_STATUS result = manager->Init();
	if (OpStatus::IsError(result))
	{
		delete manager;
		return result;
	}
#endif

	*new_object = manager;
	return OpStatus::OK;
}
#endif // POSIX_PARTIAL_NETMAN

inline PosixNetworkInterface *PosixNetworkInterfaceManager::First()
{
	return static_cast<PosixNetworkInterface *>(Head::First());
}

OP_STATUS PosixNetworkInterfaceManager::GetDefaultHTTPInterface(OpNetworkInterface** netif)
{
	if (g_op_system_info == 0 || netif == 0)
		return OpStatus::ERR_NULL_POINTER;
	*netif = 0;

	OpString best;
	RETURN_IF_ERROR(g_op_system_info->GetSystemIp(best));

#ifndef POSIX_NETIF_ITERATE_ONCE
	if (m_iterator == 0)
	{
		Clear();
		RETURN_IF_ERROR(Init());
	}
#endif

	for (PosixNetworkInterface *run = First(); run; run = run->Suc())
	{
		OpString here;
		RETURN_IF_ERROR(run->ToString(&here));
		if (best == here)
		{
			PosixNetworkInterface *local = OP_NEW(PosixNetworkInterface, (run));
			if (local == 0)
				return OpStatus::ERR_NO_MEMORY;
			OP_STATUS res = local->Import(run);
			if (OpStatus::IsSuccess(res)) *netif = local;
			else OP_DELETE(local);
			return res;
		}
	}
	return OpStatus::ERR;
}

class StoreInterface : public PosixNetLookup::Store
{
	PosixNetworkInterfaceManager * const m_boss;
public:
	StoreInterface(PosixNetworkInterfaceManager * boss) : m_boss(boss) {}

	// PosixNetLookup::Store API:
	virtual OP_STATUS AddPosixNetIF(const PosixNetworkAddress *what,
									const char *name, unsigned int index,
									bool up);
};

OP_STATUS StoreInterface::AddPosixNetIF(const PosixNetworkAddress *what,
										const char *name,
										unsigned int index,
										bool up)
{
#ifdef POSIX_SUPPORT_IPV6
	if (what->IsUnicast()) // Not useable: ignore.
		return OpStatus::OK;
#endif

	for (PosixNetworkInterface *run = m_boss->First(); run; run = run->Suc())
		if (run->IsSame(what)) // ignore duplicate
		{
			// ... but let later result contradict earlier for status:
			if ((up ? NETWORK_LINK_UP : NETWORK_LINK_DOWN) != run->GetStatus())
			{
				// Does this ever happen ?
				OP_ASSERT(!"Link status changed implausibly suddenly");
				run->SetStatus(up);
			}

			return OpStatus::OK;
		}

	OP_STATUS res = OpStatus::OK;
#ifdef PI_NETWORK_INTERFACE_INSPECTION
	OpString8 apn, ssid;
	OpNetworkInterface::NetworkType type = OpNetworkInterface::UNKNOWN;
	OpStatus::Ignore(res);
	res = m_boss->GetConnectionDetails(&type, &apn, &ssid,
									   what, name, index, up);
	PosixNetworkInterface *fresh =
		OP_NEW(PosixNetworkInterface, (index, name, type, apn.CStr(), ssid.CStr(), up));
#else
	PosixNetworkInterface *fresh = OP_NEW(PosixNetworkInterface, (index, up));
#endif
	if (!fresh)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsSuccess(res) &&
		OpStatus::IsSuccess(res = fresh->Import(what)))
		fresh->Into(m_boss);
	else
		OP_DELETE(fresh);

	return res;
}

OP_STATUS PosixNetworkInterfaceManager::Init()
{
	StoreInterface scan(this);
	return PosixNetLookup::Enumerate(&scan, SOCK_DGRAM);
}

OP_STATUS PosixNetworkInterfaceManager::BeginEnumeration()
{
	if (m_locked)
		return OpStatus::ERR;

#ifndef POSIX_NETIF_ITERATE_ONCE
	Clear();
	RETURN_IF_ERROR(Init());
#endif

	m_locked = true;
	m_iterator = First();
	return OpStatus::OK;
}

void PosixNetworkInterfaceManager::EndEnumeration()
{
	m_locked = false;
	m_iterator = 0;
}

OpNetworkInterface* PosixNetworkInterfaceManager::GetNextInterface()
{
	if (m_locked)
	{
		PosixNetworkInterface* prior = m_iterator;

		if (prior)
			m_iterator = prior->Suc();

		return prior;
	}
#ifndef SELFTEST // pi.opnetworkinterface deliberately triggers this
	OP_ASSERT(!"GetNextInterface() should only be called "
			  "between {Begin,End}Enumeration().");
#endif
	return NULL;
}

#endif // POSIX_OK_NETWORK
