/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC
#ifdef SELFTEST

#include "modules/util/str.h"
#include "modules/util/opstring.h"
#include "modules/util/opfile/opfile.h"

#include "modules/sync/sync_transport.h"
#include "modules/sync/sync_parser.h"
#include "modules/sync/sync_parser_myopera.h"
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_factory.h"
#include "modules/sync/testcases/sync_ST_transport.h"
#include "modules/url/url2.h"

ST_SyncTransportProtocol::ST_SyncTransportProtocol(OpSyncFactory* factory, OpInternalSyncListener* listener)
	: OpSyncTransportProtocol(factory, listener)
{
}

ST_SyncTransportProtocol::~ST_SyncTransportProtocol()
{
}

OP_STATUS ST_SyncTransportProtocol::Connect(OpSyncDataCollection& items_to_sync, OpSyncState& sync_state)
{
	m_sync_state = sync_state;

	OpString8 xml;
	if (m_parser)
		RETURN_IF_ERROR(static_cast<MyOperaSyncParser*>(m_parser)->GenerateXML(xml, items_to_sync));

	if (m_listener)
		m_listener->OnSyncStarted(items_to_sync.HasItems());

	return ParseData(m_server);
}

OP_STATUS ST_SyncTransportProtocol::ParseData(const OpStringC& filename)
{
	OpSyncError ret = SYNC_OK;
	OpFile file;
	BOOL exists;
	OpString8 data8;

	RETURN_IF_ERROR(file.Construct(filename.CStr()));
	RETURN_IF_ERROR(file.Exists(exists));
	if (!exists)
		return OpStatus::ERR_FILE_NOT_FOUND;

	RETURN_IF_ERROR(file.Open(OPFILE_READ | OPFILE_TEXT));
	if (m_parser)
	{
		OpFileLength len;
		OpFileLength bytes_read;
		OpString data;

		RETURN_IF_ERROR(file.GetFileLength(len));

		if (len > INT_MAX)
			return OpStatus::ERR_NOT_SUPPORTED;
		if (!data8.Reserve(static_cast<int>(len)))
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(file.Read(data8.CStr(), len, &bytes_read));

		URL url;
		m_parser->PrepareNew(url);
		RETURN_IF_ERROR(data.SetFromUTF8(data8.CStr(), static_cast<int>(bytes_read)));
		ret = m_parser->Parse(data.CStr(), data.Length(), url);
	}
	file.Close();

	/* Send the error at the end otherwise the OnSyncError can try and do stuff
	 * before the transfer item is cleaned up */
	NotifyParseResult(ret);
	return OpStatus::OK;
}

#endif // SELFTEST
#endif // SUPPORT_DATA_SYNC
