/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SYNC_TRANSPORT_H_INCLUDED_
#define _SYNC_TRANSPORT_H_INCLUDED_

#include "modules/sync/sync_parser.h"

class OpSyncParser;
class OpSyncFactory;

class OpSyncTransportProtocol
	: public OpSyncTimeout
{
public:
	OpSyncTransportProtocol(OpSyncFactory* factory, OpInternalSyncListener* listener);
	virtual ~OpSyncTransportProtocol();

	/**
	 * @name Implementation of OpSyncTimeout
	 * @{
	 */
	virtual void OnTimeout();
	/** @} */ // Implementation of OpSyncTimeout
	/**
	 * @name Implementation of MessageHandler
	 * @{
	 */

	/**
	 * Handles the messages about loading the reply from the Link server. Unhandled
	 * messages are then forwarded to OpSyncTimeout::HandleCallback(), so
	 * OnTimeout() may be called on handling the timer message.
	 */

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** @} */ // Implementation of MessageHandler

	OP_STATUS Init(const OpStringC& server, UINT32 port, OpSyncParser* parser);
	virtual	OP_STATUS Connect(OpSyncDataCollection& items_to_sync, OpSyncState& sync_state);

protected:
	/**
	 * Notifies the OpInternalSyncListener about the result of parsing the
	 * received xml document with the associated OpSyncParser m_parser.
	 *
	 * If the specified result is SYNC_OK, then the method
	 * OpInternalSyncListener::OnSyncCompleted() is called with the parsed
	 * OpSyncState, the parsed OpSyncServerInformation and the collection of
	 * parsed OpSyncItem.
	 *
	 * If the specified result is an error code, then the method
	 * OpInternalSyncListener::OnSyncError() is called with the specified error
	 * code and the error-message from the OpSyncParser.
	 * @note If the received xml document contains an error-message, this
	 *  error-message is passed to OpInternalSyncListener::OnSyncError(). If
	 *  there was any other error (i.e. the xml-document was not successfully
	 *  parsed), the error-message will be empty.
	 *
	 * @param ret is the result of parsing the xml document (i.e. calling
	 *  OpSyncParser::Parse() on the associated m_parser instance) or a
	 *  different error code if there was a different error before calling
	 *  OpSyncParser::Parse().
	 */
	void NotifyParseResult(OpSyncError ret);

private:
	void Abort();

protected:
	OpSyncFactory* m_factory;
	OpInternalSyncListener* m_listener;
	OpSyncParser* m_parser;
	OpString m_server;
	OpSyncState m_sync_state;

private:
	UINT32 m_port;
	URL_DataDescriptor* m_url_dd;
	URL m_host_url;
	BOOL m_new_data;
	MessageHandler* m_mh;
};

#endif //_SYNC_TRANSPORT_H_INCLUDED_
