/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGA_BACKENDS_MODULE_H
#define VEGA_BACKENDS_MODULE_H

#include "modules/hardcore/opera/module.h"

#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH

#include "modules/hardcore/mh/messobj.h"
#include "modules/url/url2.h"
#include "modules/util/OpHashTable.h"
#include "platforms/vega_backends/vega_blocklist_device.h"

/// helper struct for url loading - these are stored in a hash table with url.Id() as key
struct BlocklistUrlObj
{
	URL url; ///< the URL of the blocklist file
	VEGABlocklistDevice::BlocklistType type; ///< the type of the blocklist file
};

/**
   a hash of blocklist files currently being downloaded
 */
class AutoBlocklistUrlHashTable : public OpHashTable
{
public:
	~AutoBlocklistUrlHashTable() { DeleteAll(); }

	/// get BlocklistUrlObj associated with id
	BlocklistUrlObj* Get(URL_ID id)
	{
		BlocklistUrlObj* o;
		if (OpStatus::IsError(GetData((void*)id, (void**)&o)))
			return 0;
		return o;
	}
	/// add a BlocklistUrlObj to the hash
	OP_STATUS Add(BlocklistUrlObj* o) {	return OpHashTable::Add((void*)o->url.Id(), o);	}
	/// remove a BlocklistUrlObj from the hash
	OP_STATUS Remove(BlocklistUrlObj* o) { return OpHashTable::Remove((void*)o->url.Id(), (void**)&o); }
	/// delete a BlocklistUrlObj
	void Delete(void* d) { OP_DELETE(static_cast<BlocklistUrlObj*>(d)); }
};

#endif // VEGA_BACKENDS_BLOCKLIST_FETCH

class VegaBackendsModule : public OperaModule
#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
						 , public MessageObject
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH
{
public:
	VegaBackendsModule()
#ifdef SELFTEST
		: m_selftest_blocklist_url(0)
#endif // SELFTEST
	{}
	virtual void InitL(const OperaInitInfo& info) {}
	virtual void Destroy()
	{
#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
		m_blocklist_url_hash.DeleteAll();
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH
	}

#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/// trigger download of the blocklist file associated with type in delay ms
	void FetchBlocklist(VEGABlocklistDevice::BlocklistType type, unsigned long delay);
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH

	OpStringC GetCreationStatus() { return creation_status; }
	void SetCreationStatus(const OpStringC& status) { creation_status = status; }

private:
#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH
	/// download the blocklist file associated with type
	OP_STATUS FetchBlocklist(VEGABlocklistDevice::BlocklistType type);

	/// list of currently loading blocklist files
	AutoBlocklistUrlHashTable m_blocklist_url_hash;
#endif // VEGA_BACKENDS_BLOCKLIST_FETCH

	/// status of last failed attempt to create a 3d backend
	OpStringC creation_status;

#ifdef SELFTEST
public:
	const uni_char* m_selftest_blocklist_url;
#endif // SELFTEST
};

#define g_vega_backends_module g_opera->vega_backends_module

#define VEGA_BACKENDS_MODULE_REQUIRED

#endif // !VEGA_BACKENDS_MODULE_H
