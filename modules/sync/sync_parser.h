/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SYNC_PARSER_H_INCLUDED_
#define _SYNC_PARSER_H_INCLUDED_

#include "modules/sync/sync_dataitem.h"
#include "modules/sync/sync_coordinator.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlfragment.h"

class OpSyncFactory;
class OpInternalSyncListener;

class OpSyncParser
{
	friend class OpSyncCoordinator;

protected:
	OpSyncParser(OpSyncFactory* factory, OpInternalSyncListener* listener);

public:
	virtual ~OpSyncParser();

	/**
	 * Initializes the sync parser with parameters that might be
	 * needed by the derived parser.
	 *
	 * @param flags OpSyncParserFlags flags with options for this
	 *	synchronisation.
	 * @return OK or ERR_NO_MEMORY
	 */
	virtual OP_STATUS Init(UINT32 flags, OpSyncState& sync_state, SyncSupportsState supports_state);

	/**
	 * Parses the data received. This is overridden by specialized parser class
	 * and is usually called from the transport class.
	 *
	 * @param data The buffer containing the data received.
	 * @param length The length of the data buffer.
	 * @param url The url, if applicable, from where the data was downloaded.
	 * @return SYNC_OK or error
	 */
	virtual OpSyncError Parse(const uni_char* data, size_t length, const URL& url) = 0;
	virtual OpSyncError PrepareNew(const URL& url) = 0;

	// Optional method called by the module framework
	BOOL FreeCachedData(BOOL toplevel_context);

	/**
	 * Converts the data contained in the specified OpSyncDataCollection with
	 * children and attributes to an XML fragment that can be sent to the Link
	 * server.
	 *
	 * @param items Is the OpSyncDataCollection of OpSyncDataItem instances to
	 *  convert to XML.
	 * @param xml Output string buffer.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS ToXML(const OpSyncDataCollection& items, XMLFragment& xmlfragment, BOOL obfuscate, BOOL add_namespace);

	/**
	 * Converts the data contained in the specified OpSyncDataItem with its
	 * children and attributes to an XML fragment that can be sent to the Link
	 * server.
	 *
	 * @param item Is the OpSyncDataItem to convert to XML.
	 * @param xml Output string buffer.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS ToXML(OpSyncDataItem* item, XMLFragment& xmlfragment, BOOL obfuscate, BOOL add_namespace);

	/**
	 * Returns the list of items synchronised.
	 * @return synchronized items
	 */
	virtual OpSyncDataCollection* GetIncomingSyncItems() = 0;

	/**
	 * Clears the list of items synchronised.
	 */
	virtual void ClearIncomingSyncItems(BOOL remove_deleted) = 0;

	/**
	 * Returns the error message.
	 * @return error message
	 */
	virtual OpString* GetErrorMessage() = 0;

	virtual void ClearError() = 0;

	/**
	 * Returns the sync state received from the server.
	 * @return OpSyncState instance
	 */
	void GetSyncState(OpSyncState& sync_state) const { sync_state = m_sync_state; }

	/**
	 * Sets the sync state to send to the server.
	 * @param syncstate OpSyncState instance.
	 */
	void SetSyncState(const OpSyncState& state) { m_sync_state = state; }

	/**
	 * Returns the SyncSupportsState that was assigned in the Init() method.
	 */
	SyncSupportsState GetSyncSupports() const { return m_supports_state; }

	/**
	 * Returns the sync server information from the server.
	 * @return OpSyncServerInformation instance.
	 */
	void GetSyncServerInformation(OpSyncServerInformation& server_info) const {
		server_info = m_server_info;
	}

	/**
	 * Method for the client to tell the sync module information about the
	 * platform.
	 *
	 * @param type OpSyncSystemInformationType item with the data the client
	 *  supports.
	 * @param data The data for the given key.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_SUCH_RESOURCE if the parser is not initialised.
	 * @retval OpStatus::ERR_NO_MEMORY, OpStatus::ERR_OUT_OF_RANGE.
	 */
	OP_STATUS SetSystemInformation(OpSyncSystemInformationType type, const OpStringC& data);

	virtual OpSyncItem::Key GetPrimaryKey(OpSyncDataItem::DataItemType datatype) = 0;
	virtual const uni_char* GetPrimaryKeyName(OpSyncDataItem::DataItemType datatype) = 0;
	virtual OpSyncItem::Key GetPreviousRecordKey(OpSyncDataItem::DataItemType datatype) = 0;
	virtual const uni_char* GetPreviousRecordKeyName(OpSyncDataItem::DataItemType datatype) = 0;
	virtual OpSyncItem::Key GetParentRecordKey(OpSyncDataItem::DataItemType datatype) = 0;
	virtual const uni_char* GetParentRecordKeyName(OpSyncDataItem::DataItemType datatype) = 0;

	void SetDataQueue(OpSyncDataQueue* queue) { m_data_queue = queue; }

	virtual const uni_char* GetObfuscatedName(const uni_char* tagname) = 0;
	virtual const uni_char* GetRegularName(const uni_char* obfuscated_name) = 0;
	BOOL HasParsedNewState() const { return m_parsed_new_state; }
	virtual BOOL MaintainWhitespace(const uni_char* tag_name) = 0;

	enum OpSyncParserFlags
	{
		SYNC_PARSER_COMPLETE_SYNC = 0x01,
	};

	BOOL IsCompleteSync() const { return (m_flags & SYNC_PARSER_COMPLETE_SYNC) != 0; }

	enum ElementType
	{
		/**
		* Terminating element in arrays
		*/
		NoElement = 0,
		/**
		 * Corresponds to the "link" element. This is the document element of
		 * the Link Protocol. The "link" element may have children "server_info"
		 * (ServerInfoElement), "client_info" (no need to parse that on the
		 * client), "data" (DataElement), "actions".
		 */
		LinkStartElement,

		/**
		 * Corresponds to the "server_info" element, child of "link"
		 * (LinkStartElement).
		 */
		ServerInfoElement,

		/**
		 * Corresponds to the "data" element, child of "link"
		 * (LinkStartElement). May have children
		 * - "blacklist" (BlacklistElement), sub-element of "speeddial2_settings"
		 * - "bookmark" (BookmarkElement), "bookmark_folder"
		 *   (BookmarkFolderElement), "bookmark_separator"
		 *   (BookmarkSeparatorElement); (see SYNC_SUPPORTS_BOOKMARK).
		 * - "contact" (ContactElement); (see SYNC_SUPPORTS_CONTACT).
		 * - "encryption_key" (EncryptionKeyElement); (see
		 *   SYNC_SUPPORTS_ENCRYPTION).
		 * - "feed" (FeedElement); (see SYNC_SUPPORTS_FEED).
		 * - "note" (NoteElement), "note_folder" (NoteFolderElement),
		 *   "note_separator" (NoteSeparatorElement); (see SYNC_SUPPORTS_NOTE).
		 * - "pm_form_auth" (PMFormAuthElement), "pm_http_auth"
		 *   (PMHttpAuthElement); (see SYNC_SUPPORTS_PASSWORD_MANAGER).
		 * - "search_engine" (SearchEngineElement); (see
		 *   SYNC_SUPPORTS_SEARCHES).
		 * - "speeddial" (SpeeddialElement); (see SYNC_SUPPORTS_SPEEDDIAL).
		 * - "speeddial2" (Speeddial2Element); (see SYNC_SUPPORTS_SPEEDDIAL_2).
		 * - "typed_history" (TypedHistoryElement); (see
		 *   SYNC_SUPPORTS_TYPED_HISTORY).
		 * - "urlfilter" (URLFilterElement); (see SYNC_SUPPORTS_URLFILTER).
		 */
		DataElement,

		BlacklistElement,		///< corrensponds to "blacklist" (current a sub element of "speeddial2settings")
		BookmarkElement,		///< corresponds to "bookmark"
		BookmarkFolderElement,	///< corresponds to "bookmark_folder"
		BookmarkSeparatorElement,	///< corresponds to "bookmark_separator"
		ContactElement,			///< corresponds to "contact"
		EncryptionKeyElement,	///< corresponds to "encryption_key"
		EncryptionTypeElement,	///< corresponds to "encryption_type"
		ExtensionElement,		///< corresponds to "extension"
		FeedElement,			///< corresponds to "feed"
		NoteElement,			///< corresponds to "note"
		NoteFolderElement,		///< corresponds to "note_folder"
		NoteSeparatorElement,	///< corresponds to "note_separator"
		PMFormAuthElement,		///< corresponds to "pm_form_auth"
		PMHttpAuthElement,		///< corresponds to "pm_http_auth"
		SearchEngineElement,	///< corresponds to "search_engine"
		SpeeddialElement,		///< corresponds to "speeddial"
		Speeddial2Element,		///< corresponds to "speeddial2"
		Speeddial2SettingsElement,	///< corresponds to "speeddial2_settings"
		TypedHistoryElement,	///< corresponds to "typed_history"
		URLFilterElement,		///< corresponds to "urlfilter"
		OtherElement
	};

#ifdef _DEBUG
	static const char* ElementType2String(enum ElementType t)
		{
# define CASE_ITEM_2_STRING(x) case x: return "OpSyncParser::" # x
			switch (t) {
				CASE_ITEM_2_STRING(LinkStartElement);
				CASE_ITEM_2_STRING(ServerInfoElement);
				CASE_ITEM_2_STRING(DataElement);
				CASE_ITEM_2_STRING(BlacklistElement);
				CASE_ITEM_2_STRING(BookmarkElement);
				CASE_ITEM_2_STRING(BookmarkFolderElement);
				CASE_ITEM_2_STRING(BookmarkSeparatorElement);
				CASE_ITEM_2_STRING(ContactElement);
				CASE_ITEM_2_STRING(EncryptionKeyElement);
				CASE_ITEM_2_STRING(ExtensionElement);
				CASE_ITEM_2_STRING(FeedElement);
				CASE_ITEM_2_STRING(NoteElement);
				CASE_ITEM_2_STRING(NoteFolderElement);
				CASE_ITEM_2_STRING(NoteSeparatorElement);
				CASE_ITEM_2_STRING(PMFormAuthElement);
				CASE_ITEM_2_STRING(PMHttpAuthElement);
				CASE_ITEM_2_STRING(SearchEngineElement);
				CASE_ITEM_2_STRING(SpeeddialElement);
				CASE_ITEM_2_STRING(Speeddial2Element);
				CASE_ITEM_2_STRING(Speeddial2SettingsElement);
				CASE_ITEM_2_STRING(TypedHistoryElement);
				CASE_ITEM_2_STRING(URLFilterElement);
				CASE_ITEM_2_STRING(OtherElement);
			default: return "OpSyncParser::ElemtType(unknown)";
			}
		}
# undef CASE_ITEM_2_STRING
#endif // _DEBUG

protected:
	OpInternalSyncListener* m_listener;
	OpSyncFactory* m_factory;
	UINT32 m_flags;
	OpSyncState m_sync_state;
	OpSyncServerInformation m_server_info;

	/**
	 * Stores the information of which OpSyncSupports types are supported on
	 * starting a sync connection. Init() assigns this value.
	 */
	SyncSupportsState m_supports_state;

	/** Collection of client information */
	OpINT32HashTable<OpString> m_system_info;

	/** Most used for item lookups */
	OpSyncDataQueue* m_data_queue;
	OpString m_provider;
	BOOL m_parsed_new_state;
};

typedef struct SyncAcceptedFormat
{
	const uni_char* tag;
	/** If this datatype uses multiple levels, this is the name
	 * of the level below the top level containing the data.
	 * Otherwise this attribute is NULL. */
	const uni_char* sub_element_tag;		
	OpSyncParser::ElementType type;
	/** If this datatype uses multiple levels, this is the
	 * type of the level containing the actual data.
	 * Otherwise this attribute is NULL. */
	OpSyncParser::ElementType sub_element_type;	
	OpSyncDataItem::DataItemType sub_element_item_type;
	OpSyncDataItem::DataItemType item_type;
} SYNCACCEPTEDFORMAT;

/**
* Find if a element tag matches a known data type 
*
* @param tag The tag name to check for
* @param tag_len If present, we already know the length of the tag so use that here
* @param match_on_subtype If the matching tag is for a sub element under a primary datatype, this is set to TRUE
* @return OK or ERR_NO_MEMORY
*/
SYNCACCEPTEDFORMAT* FindAcceptedFormat(const uni_char* tag, UINT32 tag_len, BOOL& match_on_subtype);
SYNCACCEPTEDFORMAT* FindAcceptedFormatFromType(OpSyncDataItem::DataItemType item_type, BOOL& match_on_subtype);

/**
* Check if the given element type is a parent for a sub element type
*/
BOOL IsParentForSubType(OpSyncParser::ElementType element_type);
BOOL IsValidElement(OpSyncParser::ElementType element);

#endif //_SYNC_PARSER_H_INCLUDED_
