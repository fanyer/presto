/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SYNC_MYOPERA_PARSER_H_INCLUDED_
#define _SYNC_MYOPERA_PARSER_H_INCLUDED_

class XMLParser;
class TransferItem;
class OpSyncFactory;

#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/sync/sync_coordinator.h"

struct MyOperaItemTable
{
	MyOperaItemTable(OpSyncItem::Key id, OpSyncDataItem::DataItemType item_type, const uni_char* item_name, const uni_char* item_obfuscated_name, BOOL item_primary_key, BOOL preserve_whitespace) : key(id), type(item_type), name(item_name), obfuscated_name(item_obfuscated_name), primary_key(item_primary_key), keep_whitespace(preserve_whitespace) {}
	MyOperaItemTable() {}
	OpSyncItem::Key key;
	OpSyncDataItem::DataItemType type;
	const uni_char* name;
	const uni_char* obfuscated_name;
	BOOL primary_key;
	BOOL keep_whitespace;
};

class MyOperaSyncParser
	: public OpSyncParser
{
	friend class OpSyncFactory;

protected:
	MyOperaSyncParser(OpSyncFactory* factory, OpInternalSyncListener* listener);

public:
	virtual ~MyOperaSyncParser();

	OP_STATUS Connect(MessageHandler* mh, OpString& url, UINT32 port, OpString& username, OpString& password);

	/**
	 * @name Implementation of OpSyncParser
	 * @{
	 */

	virtual OpSyncError Parse(const uni_char* data, size_t length, const URL& url);
	virtual OpSyncError PrepareNew(const URL& url);
	virtual OpSyncDataCollection* GetIncomingSyncItems() { return &m_incoming_sync_items; }
	virtual void ClearIncomingSyncItems(BOOL remove_deleted);
	virtual OpString* GetErrorMessage() { return &m_error_message; }
	virtual void ClearError() { m_error_message.Empty(); }

	virtual OpSyncItem::Key GetPrimaryKey(OpSyncDataItem::DataItemType datatype);
	virtual const uni_char* GetPrimaryKeyName(OpSyncDataItem::DataItemType datatype);
	virtual OpSyncItem::Key GetPreviousRecordKey(OpSyncDataItem::DataItemType datatype);
	virtual const uni_char* GetPreviousRecordKeyName(OpSyncDataItem::DataItemType datatype);
	virtual OpSyncItem::Key GetParentRecordKey(OpSyncDataItem::DataItemType datatype);
	virtual const uni_char* GetParentRecordKeyName(OpSyncDataItem::DataItemType datatype);
	virtual const uni_char* GetObfuscatedName(const uni_char* tagname);
	virtual const uni_char* GetRegularName(const uni_char* obfuscated_name);
	virtual BOOL MaintainWhitespace(const uni_char* tag_name);

	/** @} */ // Implementation of OpSyncParser

	/**
	 * @param xmlout receives on output the generated xml document in utf8
	 *  encoding.
	 */
	OP_STATUS GenerateXML(OpString8& xmlout, OpSyncDataCollection& items_to_sync);

private:
	OP_STATUS CharacterDataHandler(const uni_char* s, int len);

	OpSyncDataCollection m_incoming_sync_items;
	OpString m_error_message;
	XMLParser* m_xml_parser;
	XMLParser::Listener* m_xml_parser_listener;
	class MyOperaSyncXMLTokenHandler* m_xml_token_handler;
#ifdef DEBUG_LIVE_SYNC
	OpString m_log_folder;
#endif // DEBUG_LIVE_SYNC
	SyncSupportsState m_merge_actions;
};

inline int ascii_strcmp(const uni_char* src, const uni_char* dst)
{
	int ret = 0;

	// assumes uni_char is unsigned on the platform!
	while(!(ret = *src - *dst) && *dst)
		++src, ++dst;

	if ( ret < 0 )
		ret = -1 ;
	else if ( ret > 0 )
		ret = 1 ;
	return( ret );
}

#endif //_SYNC_MYOPERA_PARSER_H_INCLUDED_
