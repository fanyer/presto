/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "modules/util/str.h"
#include "modules/util/opstring.h"
#include "modules/util/OpHashTable.h"

#include "modules/sync/sync_parser.h"
#include "modules/sync/sync_dataitem.h"
#include "modules/sync/sync_factory.h"
#include "modules/sync/sync_parser_myopera.h"
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_util.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/util/opfile/opfile.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/pi/OpLocale.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_sync.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/util/adt/bytebuffer.h"

# define MYOPERAITEMTABLE_START()										\
void init_MyOperaItemTable() {											\
	int i = 0;															\
	MyOperaItemTable* tmp_entry = g_opera->sync_module.m_myopera_item_table;
# define MYOPERAITEMTABLE_ENTRY(id, item_type, item_name, item_obfuscated_name, item_primary_key, preserve_whitespace) \
	tmp_entry[i].key = id;												\
	tmp_entry[i].type = item_type;										\
	tmp_entry[i].name = item_name;										\
	tmp_entry[i].obfuscated_name = item_obfuscated_name;				\
	tmp_entry[i].primary_key = item_primary_key;						\
	tmp_entry[i++].keep_whitespace = preserve_whitespace;
# define MYOPERAITEMTABLE_END()						\
	OP_ASSERT(i <= OpSyncItem::SYNC_KEY_NONE+1);	\
};

MYOPERAITEMTABLE_START()
// all elements use the id:
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_ID, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("id"), UNI_L("uuid"), TRUE, FALSE)

// <bookmark>
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_CREATED, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("created"), UNI_L("cre"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_DESCRIPTION, OpSyncDataItem::DATAITEM_CHILD, UNI_L("description"), UNI_L("desc"), FALSE, TRUE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_ICON, OpSyncDataItem::DATAITEM_CHILD, UNI_L("icon"), UNI_L("image"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_NICKNAME, OpSyncDataItem::DATAITEM_CHILD, UNI_L("nickname"), UNI_L("shortname"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_PANEL_POS, OpSyncDataItem::DATAITEM_CHILD, UNI_L("panel_pos"), UNI_L("ppos"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_PARENT, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("parent"), UNI_L("par"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_PERSONAL_BAR_POS, OpSyncDataItem::DATAITEM_CHILD, UNI_L("personal_bar_pos"), UNI_L("pbpos"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_PREV, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("previous"), UNI_L("prev"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_SHOW_IN_PANEL, OpSyncDataItem::DATAITEM_CHILD, UNI_L("show_in_panel"), UNI_L("show_ip"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_SHOW_IN_PERSONAL_BAR, OpSyncDataItem::DATAITEM_CHILD, UNI_L("show_in_personal_bar"), UNI_L("show_ipb"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_TITLE, OpSyncDataItem::DATAITEM_CHILD, UNI_L("title"), UNI_L("name"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_URI, OpSyncDataItem::DATAITEM_CHILD, UNI_L("uri"), UNI_L("link"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_VISITED, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("visited"), UNI_L("vis"), FALSE, FALSE)
// </bookmark>

// <bookmark_folder>
// uses "description", "nickname", "panel_pos" "parent", "personal_bar_pos",
// "previous", "show_in_panel", "show_in_personal_bar", "title"
// as defined for bookmark
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_TYPE, OpSyncDataItem::DATAITEM_ATTRIBUTE,   UNI_L("type"), UNI_L("type"), FALSE, FALSE)
// </bookmark_folder>

// <bookmark_folder>
// server created bookmark_folder to support new devices may have
// additional attributes:
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_DELETABLE, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("deletable"), UNI_L("del"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_MAX_ITEMS, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("max_items"), UNI_L("max"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_MOVE_IS_COPY, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("move_is_copy"), UNI_L("mic"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_SEPARATORS_ALLOWED, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("separators_allowed"), UNI_L("sa"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_SUB_FOLDERS_ALLOWED, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("sub_folders_allowed"), UNI_L("sfa"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_TARGET, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("target"), UNI_L("target"), FALSE, FALSE)
// </bookmark_folder>

// <extension>
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_AUTHOR, OpSyncDataItem::DATAITEM_CHILD, UNI_L("author"), UNI_L("author"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_EXTENSION_UPDATE_URI, OpSyncDataItem::DATAITEM_CHILD, UNI_L("extension_update_uri"), UNI_L("ext_upd_uri"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_VERSION, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("version"), UNI_L("ver"), FALSE, FALSE)
// </extension>

// <feed>
// uses "icon", "title", "uri"
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_LAST_READ, OpSyncDataItem::DATAITEM_CHILD, UNI_L("last_read"), UNI_L("lastread"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_UPDATE_INTERVAL, OpSyncDataItem::DATAITEM_CHILD, UNI_L("update_interval"), UNI_L("updint"), FALSE, FALSE)
// </feed>

// <note>
// uses "created", "parent", "previous", "uri"
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_CONTENT, OpSyncDataItem::DATAITEM_CHILD, UNI_L("content"), UNI_L("text"), FALSE, TRUE)
// </note>

// <note_folder>
// uses "created", "parent", "previous", "title", "type"
// </note_folder>

// <pm_form_auth>
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_FORM_DATA, OpSyncDataItem::DATAITEM_CHILD, UNI_L("form_data"), UNI_L("fdat"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_FORM_URL, OpSyncDataItem::DATAITEM_CHILD, UNI_L("form_url"), UNI_L("furl"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_MODIFIED, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("modified"), UNI_L("mdf"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_PAGE_URL, OpSyncDataItem::DATAITEM_CHILD, UNI_L("page_url"), UNI_L("purl"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_PASSWORD, OpSyncDataItem::DATAITEM_CHILD, UNI_L("password"), UNI_L("pwd"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_PM_FORM_AUTH_SCOPE, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("scope"), UNI_L("scp"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_TOPDOC_URL, OpSyncDataItem::DATAITEM_CHILD, UNI_L("topdoc_url"), UNI_L("turl"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_USERNAME, OpSyncDataItem::DATAITEM_CHILD, UNI_L("username"), UNI_L("usr"), FALSE, FALSE)
// </pm_form_auth>

// <pm_http_auth>
// uses "modified", "page_url", "password", "username"
// </pm_http_auth>

// <search_engine>
// uses "icon", "title", "type", "uri"
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_ENCODING, OpSyncDataItem::DATAITEM_CHILD, UNI_L("encoding"), UNI_L("enc"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_GROUP, OpSyncDataItem::DATAITEM_CHILD, UNI_L("group"), UNI_L("grp"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_HIDDEN, OpSyncDataItem::DATAITEM_CHILD, UNI_L("hidden"), UNI_L("hdn"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_IS_POST, OpSyncDataItem::DATAITEM_CHILD, UNI_L("is_post"), UNI_L("ispost"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_KEY, OpSyncDataItem::DATAITEM_CHILD, UNI_L("key"), UNI_L("key"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_POST_QUERY, OpSyncDataItem::DATAITEM_CHILD, UNI_L("post_query"), UNI_L("post"), FALSE, FALSE)
// </search_engine>

// <speeddial>
// uses "icon", "title", "uri"
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_CUSTOM_TITLE, OpSyncDataItem::DATAITEM_CHILD, UNI_L("custom_title"), UNI_L("cst_ttl"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_EXTENSION_ID, OpSyncDataItem::DATAITEM_CHILD, UNI_L("extension_id"), UNI_L("ext_id"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_PARTNER_ID, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("partner_id"), UNI_L("prtnrid"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_POSITION, OpSyncDataItem::DATAITEM_ATTRIBUTE, UNI_L("position"), UNI_L("pos"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_RELOAD_ENABLED, OpSyncDataItem::DATAITEM_CHILD, UNI_L("reload_enabled"), UNI_L("rle"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_RELOAD_INTERVAL, OpSyncDataItem::DATAITEM_CHILD, UNI_L("reload_interval"), UNI_L("rli"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_RELOAD_ONLY_IF_EXPIRED, OpSyncDataItem::DATAITEM_CHILD, UNI_L("reload_only_if_expired"), UNI_L("rloie"), FALSE, FALSE)
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_RELOAD_POLICY, OpSyncDataItem::DATAITEM_CHILD, UNI_L("reload_policy"), UNI_L("rlp"), FALSE, FALSE)
// </speeddial>

// <typed_history>
// uses "content", "type"
MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_LAST_TYPED, OpSyncDataItem::DATAITEM_CHILD, UNI_L("last_typed"), UNI_L("typed"), FALSE, FALSE)
// </typed_history>

// <urlfilter>
// uses "content", "type"
// </urlfilter>

MYOPERAITEMTABLE_ENTRY(OpSyncItem::SYNC_KEY_THUMBNAIL, OpSyncDataItem::DATAITEM_CHILD, UNI_L("thumbnail"), UNI_L("thumb"), FALSE, FALSE)

MYOPERAITEMTABLE_ENTRY((OpSyncItem::Key)0, (OpSyncDataItem::DataItemType)0, NULL, NULL, FALSE, FALSE)
MYOPERAITEMTABLE_END()

// ==================== MyOperaSyncIgnoreXMLParserListener

/**
 * This class is used by MyOperaSyncParser as an XMLParser::Listener that
 * ignores all notifications. MyOperaSyncParser does not expect the
 * notifications to be called or it does not need to do anything.
 */
class MyOperaSyncIgnoreXMLParserListener : public XMLParser::Listener
{
public:
	MyOperaSyncIgnoreXMLParserListener() {}
	virtual ~MyOperaSyncIgnoreXMLParserListener() {}

	/**
	 * @name Implementation of XMLParser::Listener
	 * @{
	 */

	/**
	 * This method is only called when external entities are loaded, when
	 * parsing of an external entity has finished and parsing of the document
	 * entity should continue. The Opera Link protocol does not use external
	 * entities, so there is no need implement this method.
	 */
	virtual void Continue(XMLParser* parser) { OP_ASSERT(!"Unexpected call"); }

	/**
	 * This method is called when parsing has stopped. This implementation has
	 * nothing to do.
	 * @todo Check if we should test the reason why it has stopped (see
	 *  XMLParser::IsFinished(), XMLParser::IsFailed(),
	 *  XMLParser::IsOutOfMemory()).
	 */
	virtual void Stopped(XMLParser* parser) {
		OP_NEW_DBG("MyOperaSyncIgnoreXMLParserListener::Stopped()", "sync.xml");
		OP_DBG(("finished: %s with %s%s", parser->IsFinished()?"yes":"no", parser->IsFailed()?"error":"success", parser->IsOutOfMemory()?"; OOM":""));
	}

	/** @} */ // Implementation of XMLParser::Listener
};

// ==================== MyOperaSyncXMLTokenHandler
/**
 * The class MyOperaSyncXMLTokenHandler is used as the XMLTokenHandler for the
 * XMLParser instance that parses the xml response from the Link server.
 */
class MyOperaSyncXMLTokenHandler : public XMLTokenHandler
{
public:
	MyOperaSyncXMLTokenHandler(OpSyncParser* parser)
		: m_parser(parser)
		, m_current_element(OpSyncParser::OtherElement)
		, m_current_sync_item(NULL)
		, m_error_code(SYNC_OK)
		, m_parsed_sync_state(false)
		, m_parsed_server_info(false)
		, m_dirty_sync_requested(false)
		, m_server_version(SYNC_LINK_SERVER_VERSION_1_0)
		{}

	virtual ~MyOperaSyncXMLTokenHandler() { Reset(); }

	/**
	 * Returns true if we received an error status from the Link server in the
	 * xml document.
	 * @see GetErrorMessage()
	 */
	bool HasError() const {
		return m_error_message.HasContent() || (m_error_code != SYNC_OK);
	}

	/**
	 * Returns the error-message that was received from the Link server in the
	 * xml document.
	 */
	const OpStringC& GetErrorMessage() const { return m_error_message; }
	OpString& TakeErrorMessage() { return m_error_message; }

	/**
	 * Returns the error-code that was received from the Link server in the
	 * xml document.
	 */
	OpSyncError GetErrorCode() const { return m_error_code; }

	/**
	 * Returns the OpSyncDataCollection of incoming OpSyncDataItem instances.
	 * The returned collection can be added to a caller's collection using
	 * OpSyncDataCollection::AppendItems() and thus the items are removed from
	 * this instance.
	 */
	OpSyncDataCollection* TakeIncomingItems() { return &m_incoming_sync_items; }

	/**
	 * Returns true if the xml document that was received from the Link server
	 * contained a sync-state that was successfully parsed. The sync-state is
	 * available via GetSyncState().
	 */
	bool HasParsedNewSyncState() const { return m_parsed_sync_state; }

	/**
	 * Returns the sync-state that was received from the Link server in the xml
	 * document.
	 * @see HasParsedNewSyncState()
	 */
	const OpString& GetSyncState() const { return m_sync_state; }

	/**
	 * Returns true if the Link server requests to start a dirty sync.
	 */
	bool IsDirtySyncRequested() const { return m_dirty_sync_requested; }

	/**
	 * Returns true if the xml document that was received from the Link server
	 * contained a 'server_info' element that was successfully parsed. The
	 * server information is available via GetServerInformation().
	 */
	bool HasParsedServerInformation() const { return m_parsed_server_info; }

	/**
	 * Returns the OpSyncServerInformation that was received from the Link
	 * server in the xml document.
	 */
	const OpSyncServerInformation& GetServerInformation() const { return m_server_info; }

	/**
	 * Resets the variables of the token handler to be able to start parsing a
	 * new xml document. The error message is cleared, the error code is set to
	 * SYNC_OK (so HasError() returns false), the OpSyncDataCollection of
	 * incoming items is cleared.
	 */
	void Reset() {
		m_error_message.Empty();
		m_error_code = SYNC_OK;
		m_incoming_sync_items.Clear();
		m_current_element = OpSyncParser::OtherElement;
		m_content_string.Empty();
		if (m_current_sync_item)
		{
			m_current_sync_item->DecRefCount();
			m_current_sync_item = NULL;
		}
		m_dirty_sync_requested = false;
		m_parsed_sync_state = false;
		m_parsed_server_info = false;
		m_sync_state.Empty();
		m_server_info.Clear();
	}

	/**
	 * @name Implementation of XMLTokenHandler
	 * @{
	 */

	/**
	 * Depending on the type of the specified token (XMLToken::GetType()), this
	 * method calls HandleTextToken() (in case of XMLToken::TYPE_Text or
	 * XMLToken::TYPE_CDATA), HandleStartTagToken() (in case of
	 * XMLToken::TYPE_STag or XMLToken::TYPE_EmptyElemTag) or
	 * HandleEndTagToken() (in case of XMLToken::TYPE_ETag or
	 * XMLToken::TYPE_EmptyElemTag).
	 */
	virtual Result HandleToken(XMLToken& token) {
		OP_NEW_DBG("MyOperaSyncXMLTokenHandler::HandleToken", "sync.xml");
		OP_STATUS status = OpStatus::OK;

		switch (token.GetType())
		{
		case XMLToken::TYPE_CDATA:
		case XMLToken::TYPE_Text: status = HandleTextToken(token); break;
		case XMLToken::TYPE_STag: status = HandleStartTagToken(token); break;
		case XMLToken::TYPE_ETag: status = HandleEndTagToken(token); break;
		case XMLToken::TYPE_EmptyElemTag:
			status = HandleStartTagToken(token);
			if (OpStatus::IsSuccess(status))
				status = HandleEndTagToken(token);
			break;
		}

		if (OpStatus::IsMemoryError(status))
			return XMLTokenHandler::RESULT_OOM;
		else if (OpStatus::IsError(status))
			return XMLTokenHandler::RESULT_ERROR;
		return XMLTokenHandler::RESULT_OK;
	}

	/** @} */ // Implementation of XMLTokenHandler


private:
	/**
	 * Is called by HandleToken() when the m_xml_parser has found some text.
	 * This method adds the text to the m_content_string. The m_content_string
	 * is later used on handling the end-token (see HandleEndTagToken()).
	 */
	OP_STATUS HandleTextToken(XMLToken& token) {
		uni_char* local_copy = NULL;
		const uni_char* text_data = token.GetLiteralSimpleValue();
		if (!text_data)
		{
			local_copy = token.GetLiteralAllocatedValue();
			text_data = local_copy;
		}
		OP_STATUS s = m_content_string.Append(text_data, token.GetLiteralLength());
		OP_DELETEA(local_copy);
		return s;
	}

	/**
	 * Is called by HandleToken() when the m_xml_parser has found a start
	 * token. The m_current_element is set to the type that corresponds to the
	 * start-token:
	 * - "link" (this is the document element) => LinkStartElement
	 * - "server_info" (child of link) => ServerInfoElement
	 * - "data" (child of link) => DataElement
	 * All valid children of the "data" element are mapped by the table
	 * specified in SyncModule::m_accepted_format. If the current token starts a
	 * valid child of the "data" element, a new OpSyncDataItem instance is
	 * created and stored in m_current_sync_item (see GetSyncDataItem()), the
	 * attributes of the xml element are added as
	 * OpSyncDataItem::AddAttribute() and m_current_element is set to the
	 * ElementType of that valid child.
	 *
	 * The valid children may have further children. If a new child is started
	 * while the m_current_element is a valid child of the "data" element, we
	 * start to collect the content string in m_content_string (see also
	 * HandleEndToken).
	 */
	OP_STATUS HandleStartTagToken(XMLToken &token);

	/**
	 * Is called by HandleToken() when the m_xml_parser has found an end token.
	 *
	 * If the m_current_element is a valid child of the "data" element, then we
	 * have two cases:
	 * - the token ends the m_current_element: in this case the
	 *   m_current_sync_item is added to the OpSyncDataCollection
	 *   m_incoming_sync_items and m_current_element is set to DataElement (to
	 *   expect a new valid child, or the end of the "data" element).
	 * - the token ends a child of the valid child: in this case the child is
	 *   added to m_current_element by calling OpSyncDataItem::AddChild() where
	 *   the name of the element is the key and the content of the element (that
	 *   was collected in m_content_string) is the data of the child.
	 *
	 * If the m_current_element is the ServerInfoElement, we handle the children
	 * - "error" (by setting m_error_message to the child's value),
	 * - "long_interval" (passing the value to m_server_info,
	 *   OpSyncServerInformation::SetSyncIntervalLong()),
	 * - "short_interval" (passing the value to m_server_info,
	 *   OpSyncServerInformation::SetSyncIntervalShort())
	 * - "maxitems" (passing the value to m_server_info,
	 *   OpSyncServerInformation::SetSyncMaxItems()).
	 */
	OP_STATUS HandleEndTagToken(XMLToken &token);

	static OpSyncError ErrorCodeToSyncError(const OpStringC& error_code) {
		int code = uni_atoi(error_code.CStr());
		switch (code)
		{
		case 101: return SYNC_ACCOUNT_AUTH_FAILURE;
		case 102: return SYNC_ACCOUNT_USER_BANNED;
		case 103: return SYNC_ACCOUNT_OAUTH_EXPIRED;
		case 201: return SYNC_ERROR_INVALID_REQUEST;
		case 202:
		case 203: return SYNC_ERROR_PARSER;
		case 204: return SYNC_ERROR_PROTOCOL_VERSION;
		case 205: return SYNC_ERROR_CLIENT_VERSION;
		case 305: return SYNC_ERROR_INVALID_STATUS;
		case 310: return SYNC_ERROR_INVALID_BOOKMARK;
		case 320: return SYNC_ERROR_INVALID_SPEEDDIAL;
		case 330: return SYNC_ERROR_INVALID_NOTE;
		case 340: return SYNC_ERROR_INVALID_SEARCH;
		case 350: return SYNC_ERROR_INVALID_TYPED_HISTORY;
		case 360: return SYNC_ERROR_INVALID_FEED;
		case 401:
		case 402: return SYNC_ACCOUNT_USER_UNAVAILABLE;
		case 500: return SYNC_ERROR_SERVER;
		default: return SYNC_ERROR;
		}
	}

	OpSyncParser* m_parser;
	OpSyncParser::ElementType m_current_element;
	OpString m_content_string;
	OpSyncDataItem* m_current_sync_item;
	OpSyncDataCollection m_incoming_sync_items;
	OpString m_error_message;
	OpSyncError m_error_code;
	bool m_parsed_sync_state;
	bool m_parsed_server_info;
	bool m_dirty_sync_requested;
	OpString m_sync_state;
	OpSyncServerInformation m_server_info;

	enum OpSyncServerVersion {
		SYNC_LINK_SERVER_VERSION_1_0,
		SYNC_LINK_SERVER_VERSION_1_1,
		SYNC_LINK_SERVER_VERSION_1_2
	} m_server_version;
	bool IsServerVersion1_1() const { return m_server_version >= SYNC_LINK_SERVER_VERSION_1_1; }
};

OP_STATUS MyOperaSyncXMLTokenHandler::HandleStartTagToken(XMLToken &token)
{
	OP_NEW_DBG("MyOperaSyncXMLTokenHandler::HandleStartTagToken", "sync.xml");
	OP_DBG(("current element: %s", OpSyncParser::ElementType2String(m_current_element)));
	m_content_string.Empty();

	// Fetch information about the element.
	const XMLCompleteNameN& elemname = token.GetName();

	if (uni_strncmp(UNI_L("link"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
	{
		m_current_element = OpSyncParser::LinkStartElement;
		OP_DBG(("found element: %s", OpSyncParser::ElementType2String(m_current_element)));

		// lets get the attributes for this element
		XMLToken::Attribute* attributes = token.GetAttributes();

		// unless specified we assume server version 1.0:
		m_server_version = SYNC_LINK_SERVER_VERSION_1_0;

		OpString key, value;
		for (unsigned int index = 0; index < token.GetAttributesCount(); index++)
		{
			// add all attributes without validating, validation will happen later
			RETURN_IF_ERROR(key.Set(attributes[index].GetName().GetLocalPart(), attributes[index].GetName().GetLocalPartLength()));
			RETURN_IF_ERROR(value.Set(attributes[index].GetValue(), attributes[index].GetValueLength()));

			if ((key == "syncstate") && value.HasContent())
			{
				RETURN_IF_ERROR(m_sync_state.TakeOver(value));
				OP_DBG((UNI_L("new syncstate: '%s'"), m_sync_state.CStr()));
				m_parsed_sync_state = true;
			}

			else if ((key == "dirty") && value.HasContent())
			{
				OP_DBG((UNI_L("dirty: '%s'"), value.CStr()));
				m_dirty_sync_requested = (value == "true") || (value == "1");
			}

			else if (key == "version" && value.HasContent())
			{
				OP_DBG((UNI_L("version: '%s'"), value.CStr()));
				if (value == UNI_L("1.0"))
					m_server_version = SYNC_LINK_SERVER_VERSION_1_0;
				else if (value == UNI_L("1.1"))
					m_server_version = SYNC_LINK_SERVER_VERSION_1_1;
				else if (value == UNI_L("1.2"))
					m_server_version = SYNC_LINK_SERVER_VERSION_1_2;
			}
		}
	}

	else if (IsServerVersion1_1() && uni_strncmp(UNI_L("server_info"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0 ||
			 uni_strncmp(UNI_L("serverinfo"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
	{
		OP_ASSERT(m_current_element == OpSyncParser::LinkStartElement || !"The 'server_info' element is expected to be a child of the 'link' element");
		m_current_element = OpSyncParser::ServerInfoElement;
		OP_DBG(("found element: %s", OpSyncParser::ElementType2String(m_current_element)));
	}

	else if (uni_strncmp(UNI_L("data"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
	{
		OP_ASSERT(m_current_element == OpSyncParser::LinkStartElement || !"The 'data' element is expected to be a child of the 'link' element");
		m_current_element = OpSyncParser::DataElement;
		OP_DBG(("found element: %s", OpSyncParser::ElementType2String(m_current_element)));
	}

	else if (uni_strncmp(UNI_L("error"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
	{
		OP_ASSERT(m_current_element == OpSyncParser::ServerInfoElement);
		// lets get the attributes for this element
		XMLToken::Attribute* attributes = token.GetAttributes();
		for (unsigned int index = 0; index < token.GetAttributesCount(); index++)
		{
			OpString key, value;
			RETURN_IF_ERROR(key.Set(attributes[index].GetName().GetLocalPart(), attributes[index].GetName().GetLocalPartLength()));
			RETURN_IF_ERROR(value.Set(attributes[index].GetValue(), attributes[index].GetValueLength()));
			if ((key == "code") && value.HasContent())
				m_error_code = ErrorCodeToSyncError(value);
			// we ignore any other unknown attribute ...
		}
		OP_DBG((UNI_L("found element error with code: ")) << m_error_code);
	}

	else if (m_current_element == OpSyncParser::DataElement || IsParentForSubType(m_current_element))
	{	/* The children of the data element are stored in the table
		 * SyncModule::m_accepted_format, see also sync_parser.cpp */

		BOOL match_for_subtype;
		SYNCACCEPTEDFORMAT* format = FindAcceptedFormat(elemname.GetLocalPart(), elemname.GetLocalPartLength(), match_for_subtype);
		if (format)
		{
			if (match_for_subtype)
				m_current_element = format->sub_element_type;
			else
				m_current_element = format->type;

			OP_DBG(("found element: %s", OpSyncParser::ElementType2String(m_current_element)));

			if (!format->sub_element_tag ||		// the element has no children
				(m_current_element == format->sub_element_type))	// or the child is the active element
			{
				OP_ASSERT(m_current_sync_item == NULL);
				m_current_sync_item = OP_NEW(OpSyncDataItem, ());
				if (!m_current_sync_item)
					return OpStatus::ERR_NO_MEMORY;
				m_current_sync_item->IncRefCount();
				m_current_sync_item->SetType(format->item_type);
				const uni_char* primary_key = m_parser->GetPrimaryKeyName(format->item_type);
				OpString key, value;
				RETURN_IF_ERROR(key.Set(elemname.GetLocalPart(), elemname.GetLocalPartLength()));
				OP_DBG((" item-type: ") << format->item_type);
				OP_DBG((UNI_L(" element: '%s'; primary key: '%s'"), key.CStr(), primary_key?primary_key:UNI_L("<no key>")));

				// lets get the attributes for this element
				XMLToken::Attribute* attributes = token.GetAttributes();
				for (unsigned int index = 0; index < token.GetAttributesCount(); index++)
				{
					/* add all attributes without validating, validation will happen
					 * later: */
					RETURN_IF_ERROR(key.Set(attributes[index].GetName().GetLocalPart(), attributes[index].GetName().GetLocalPartLength()));
					RETURN_IF_ERROR(value.Set(attributes[index].GetValue(), attributes[index].GetValueLength()));
					OP_DBG((UNI_L("attribute %d: '%s'='%s'"), index, key.CStr(), value.CStr()));

					if (primary_key && key.HasContent() && key == primary_key)
					{
						OP_ASSERT(value.HasContent() && "the data is missing for the primary key!");
						OP_DBG((UNI_L("primary key: %s=%s"), key.CStr(), value.CStr()));
						RETURN_IF_ERROR(m_current_sync_item->m_key.Set(key));
						RETURN_IF_ERROR(m_current_sync_item->m_data.Set(value));
					}

					OpSyncDataItem::DataItemStatus action = OpSyncDataItem::DATAITEM_ACTION_NONE;
					if (key == "status")
					{
						if (value == "added")
						{
							action = OpSyncDataItem::DATAITEM_ACTION_ADDED;
							OP_DBG(("status=added"));
						}
						else if (value == "modified")
						{
							action = OpSyncDataItem::DATAITEM_ACTION_MODIFIED;
							OP_DBG(("status=modified"));
						}
						else if (value == "deleted")
						{
							action = OpSyncDataItem::DATAITEM_ACTION_DELETED;
							OP_DBG(("status=deleted"));
						}
					}
					if (action == OpSyncDataItem::DATAITEM_ACTION_NONE)
						RETURN_IF_ERROR(m_current_sync_item->AddAttribute(key.CStr(), value.CStr(), NULL));
					else
						m_current_sync_item->SetStatus(action);
				}
			}
		}
		else
		{	/* This is an unknown child of the "data" element - ignore that
			 * child... */
			OP_ASSERT(m_current_sync_item == NULL);
#ifdef _DEBUG
			OpString element;
			OpStatus::Ignore(element.Set(elemname.GetLocalPart(), elemname.GetLocalPartLength()));
			OP_DBG((UNI_L("ignoring element '%s'"), element.CStr()));
			OP_DBG(("child of '%s'", OpSyncParser::ElementType2String(m_current_element)));
#endif // _DEBUG;
		}
	}

	else if (IsValidElement(m_current_element))
	{	/* Now we expect to have a child for a valid element (a valid element is
		 * one of the children of the "data" element, all valid elements are
		 * specified in the table SyncModule::m_accepted_format. So we start to
		 * store the child's value in m_content_string and then add the child to
		 * the m_current_sync_item in HandleEndTagToken(), where the
		 * element-name is the child's key and the m_content_string is the
		 * child's data.
		 * We also expect that a valid child has no grand-children... */

		/* @todo For now we only support to store the content of any child, but
		 *  not the child's attributes. To support attributes for the children
		 *  (e.g. needed for SYNC_SUPPORTS_CONTACT), we need to create an new
		 *  OpSyncDataItem here, get the element's attributes, call
		 *  child_item->AddAttribute() and set the child's data to the
		 *  m_content_string in HandleEndToken(). */
#ifdef _DEBUG
		OpString element;
		OpStatus::Ignore(element.Set(elemname.GetLocalPart(), elemname.GetLocalPartLength()));
		OP_DBG((UNI_L("found element '%s'"), element.CStr()));
		OP_DBG(("child of '%s'", OpSyncParser::ElementType2String(m_current_element)));
#endif // _DEBUG;
	}

	else if (m_current_element == OpSyncParser::ServerInfoElement)
	{	/* Now we expect children for the "server_info" element. The children
		 * are handled in HandleEndToken() */
#ifdef _DEBUG
		OpString element;
		OpStatus::Ignore(element.Set(elemname.GetLocalPart(), elemname.GetLocalPartLength()));
		OP_DBG((UNI_L("found element '%s'"), element.CStr()));
		OP_DBG(("child of '%s'", OpSyncParser::ElementType2String(m_current_element)));
#endif // _DEBUG;
	}

	else
	{
#ifdef _DEBUG
		OpString element;
		OpStatus::Ignore(element.Set(elemname.GetLocalPart(), elemname.GetLocalPartLength()));
		OP_DBG((UNI_L("found other element '%s'"), element.CStr()));
		OP_DBG(("child of '%s'", OpSyncParser::ElementType2String(m_current_element)));
#endif // _DEBUG;
		m_current_element = OpSyncParser::OtherElement;
		if (m_current_sync_item)
		{
			m_current_sync_item->DecRefCount();
			m_current_sync_item = NULL;
		}
	}

	return OpStatus::OK;
}

OP_STATUS MyOperaSyncXMLTokenHandler::HandleEndTagToken(XMLToken& token)
{
	OP_NEW_DBG("MyOperaSyncXMLTokenHandler::HandleEndTagToken", "sync.xml");
	const XMLCompleteNameN& elemname = token.GetName();
	if (IsValidElement(m_current_element))
	{
		BOOL match_for_subtype;
		OP_DBG(("current element: %s", OpSyncParser::ElementType2String(m_current_element)));
		SYNCACCEPTEDFORMAT* format = FindAcceptedFormat(elemname.GetLocalPart(), elemname.GetLocalPartLength(), match_for_subtype);
		if (format)
		{
			OP_ASSERT((match_for_subtype ? format->sub_element_type == m_current_element : format->type == m_current_element) || !"A child of the data-element (m_current_element) seems to have a child which is named like other children of the data-element. Please re-design the xml specification such that the children of data have unique element names.");
			if (m_current_sync_item)
			{
				switch (format->type) {
				case OpSyncParser::EncryptionKeyElement:
				case OpSyncParser::EncryptionTypeElement:
				{	/* The encryption_key/type element is special, as it has a
					 * primary key "id" with value "0" (because there can only
					 * be one encryption-key) and a child with empty key and the
					 * content as the child's data: */
					const uni_char* key = UNI_L("id");
					const uni_char* value = UNI_L("0");
					RETURN_IF_ERROR(m_current_sync_item->m_key.Set(key));
					RETURN_IF_ERROR(m_current_sync_item->m_data.Set(value));
					RETURN_IF_ERROR(m_current_sync_item->AddAttribute(key, value));
					OP_DBG((UNI_L("Setting data: '%s'"), m_content_string.CStr()));
					RETURN_IF_ERROR(m_current_sync_item->AddChild(UNI_L(""), m_content_string.CStr()));
					break;
				}
				}

				m_incoming_sync_items.AddItem(m_current_sync_item);
				m_current_sync_item->DecRefCount();
				m_current_sync_item = NULL;
			}
			// balance the closing tags
			if (match_for_subtype)
				m_current_element = format->type;
			else
				m_current_element = OpSyncParser::DataElement;
		}
		else
		{
			OpString key;
			RETURN_IF_ERROR(key.Set(elemname.GetLocalPart(), elemname.GetLocalPartLength()));
			RETURN_IF_ERROR(m_current_sync_item->AddChild(key.CStr(), m_content_string.CStr()));
			OP_DBG((UNI_L("Added child: %s=%s"), key.CStr(), m_content_string.CStr()));
		}
	}

	else if (m_current_element == OpSyncParser::ServerInfoElement)
	{
		if (IsServerVersion1_1() && uni_strncmp(UNI_L("server_info"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0 ||
			uni_strncmp(UNI_L("serverinfo"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
		{
			m_parsed_server_info = true;
			m_current_element = OpSyncParser::LinkStartElement;
		}

		else if (uni_strncmp(UNI_L("error"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
		{
			OP_DBG((UNI_L("server_info: error='%s'"), m_content_string.CStr()));
			RETURN_IF_ERROR(m_error_message.Set(m_content_string));
		}

		else if (IsServerVersion1_1() && uni_strncmp(UNI_L("long_interval"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0 ||
				 uni_strncmp(UNI_L("longinterval"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
		{
			unsigned int interval = uni_atoi(m_content_string.CStr());
			if (interval)
				m_server_info.SetSyncIntervalLong(interval);
			OP_DBG(("server_info: long interval: %u", interval));
		}

		else if (IsServerVersion1_1() && uni_strncmp(UNI_L("short_interval"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0 ||
				 uni_strncmp(UNI_L("shortinterval"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
		{
			unsigned int interval = uni_atoi(m_content_string.CStr());
			if (interval)
				m_server_info.SetSyncIntervalShort(interval);
			OP_DBG(("server_info: short interval: %u", interval));
		}

		else if (IsServerVersion1_1() && uni_strncmp(UNI_L("max_items"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0 ||
				 uni_strncmp(UNI_L("maxitems"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
		{
			UINT32 maxitems = uni_atoi(m_content_string.CStr());
			if (maxitems)
				m_server_info.SetSyncMaxItems(maxitems);
			OP_DBG(("server_info: max items: %u", maxitems));
		}

	}

	else if (uni_strncmp(UNI_L("data"), elemname.GetLocalPart(), elemname.GetLocalPartLength()) == 0)
	{
		OP_ASSERT(m_current_element == OpSyncParser::DataElement);
		m_current_element = OpSyncParser::LinkStartElement;
	}

	else
		m_current_element = OpSyncParser::OtherElement;

	m_content_string.Empty();
	return OpStatus::OK;
}

// ==================== MyOperaSyncParser

MyOperaSyncParser::MyOperaSyncParser(OpSyncFactory* factory, OpInternalSyncListener* listener)
	: OpSyncParser(factory, listener)
	, m_xml_parser(NULL)
	, m_xml_parser_listener(NULL)
	, m_xml_token_handler(NULL)
{
	OP_NEW_DBG("MyOperaSyncParser::MyOperaSyncParser", "sync");
#ifdef DEBUG_LIVE_SYNC
	// Build the path to the link debug files
	OpString tmp_storage;
	m_log_folder.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_USERPREFS_FOLDER, tmp_storage));
	m_log_folder.Append(UNI_L("link"));
	m_log_folder.Append(PATHSEP);
#endif // DEBUG_LIVE_SYNC
}

MyOperaSyncParser::~MyOperaSyncParser()
{
	OP_NEW_DBG("MyOperaSyncParser::~MyOperaSyncParser", "sync");
	m_system_info.DeleteAll();
	OP_DELETE(m_xml_parser);
	m_xml_parser = NULL;
	OP_DELETE(m_xml_parser_listener);
	m_xml_parser_listener = NULL;
	OP_DELETE(m_xml_token_handler);
	m_xml_token_handler = NULL;
}

BOOL MyOperaSyncParser::MaintainWhitespace(const uni_char* tag_name)
{
	for (unsigned int cnt = 0; g_sync_myopera_item_table[cnt].name; cnt++)
	{
		if (!ascii_strcmp(tag_name, g_sync_myopera_item_table[cnt].name))
			return g_sync_myopera_item_table[cnt].keep_whitespace;
	}
	return FALSE;
}

const uni_char* MyOperaSyncParser::GetObfuscatedName(const uni_char* name)
{
	for (unsigned int cnt = 0; g_sync_myopera_item_table[cnt].name; cnt++)
	{
		if (!ascii_strcmp(name, g_sync_myopera_item_table[cnt].name))
			return g_sync_myopera_item_table[cnt].obfuscated_name;
	}
	/* If we didn't find the obfuscated name for the specified name, return the
	 * input argument back to the caller: */
	return name;
}

const uni_char* MyOperaSyncParser::GetRegularName(const uni_char* obfuscated_name)
{
	for (unsigned int cnt = 0; g_sync_myopera_item_table[cnt].name; cnt++)
	{
		if (!ascii_strcmp(obfuscated_name, g_sync_myopera_item_table[cnt].obfuscated_name))
			return g_sync_myopera_item_table[cnt].name;
	}
	/* If we didn't find the regular name for the specified obfuscated name,
	 * return the input argument back to the caller: */
	return obfuscated_name;
}

void MyOperaSyncParser::ClearIncomingSyncItems(BOOL remove_deleted)
{
	OP_NEW_DBG("MyOperaSyncParser::ClearIncomingSyncItems", "sync");
	for (OpSyncDataItemIterator item(m_incoming_sync_items.First()); *item; ++item)
	{
		if (remove_deleted)
		{
			if (item->GetList())
				item->GetList()->RemoveItem(*item);
		}
		else if (item->GetStatus() != OpSyncDataItem::DATAITEM_ACTION_DELETED)
		{
			if (item->GetList())
				item->GetList()->RemoveItem(*item);
		}
	}
}

OP_STATUS MyOperaSyncParser::GenerateXML(OpString8& xmlout, OpSyncDataCollection& items_to_sync)
{
	OP_NEW_DBG("MyOperaSyncParser::GenerateXML()", "sync");
	OpString state;
	RETURN_IF_ERROR(m_sync_state.GetSyncState(state));
	if (state.IsEmpty())
		RETURN_IF_ERROR(state.Set(UNI_L("0")));
	/* If the sync-state is "0" we want to add a merge-action for all supported
	 * types: */
	BOOL do_merge = (state == UNI_L("0"));

	XMLFragment xmlfragment;
	xmlfragment.SetDefaultWhitespaceHandling(XMLWHITESPACEHANDLING_PRESERVE);

	RETURN_IF_ERROR(xmlfragment.OpenElement(LINKNAME("link")));

#ifdef SYNC_HAVE_EXTENSIONS
	/* The sync protocol version 1.2 is bound to extensions:
	 * - If extensions are enabled, then we must use protocol version greater
	 *   or equal to 1.2, otherwise the Opera/Link server does not understand a
	 *   request with an "extension" entry. Note: this also implies that
	 *   speed-dial-2 is enabled.
	 * - If extensions are disabled, then we continue to use protocol version
	 *   1.1, because there is no other data in the protocol which we could use.
	 *   Note: this also implies that we can still use "speeddial" entries, i.e.
	 *   speed-dial-2 is not required. */
	RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("version"), UNI_L("1.2")));
#else
	RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("version"), UNI_L("1.1")));
#endif
	RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("syncstate"), state.CStr()));
	RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("dirty"), (m_flags & SYNC_PARSER_COMPLETE_SYNC) ? UNI_L("1") : UNI_L("0")));

	RETURN_IF_ERROR(xmlfragment.OpenElement(LINKNAME("client_info")));

	for (unsigned int i = 0; i < SYNC_SUPPORTS_MAX; i++)
	{
		OpSyncSupports supports = static_cast<OpSyncSupports>(i);
		if (!m_supports_state.HasSupports(supports))
			continue;

		OP_DBG(("") << supports);
		/* Check each supports-type if we need to set the merge-action: */
		if (do_merge || m_sync_state.IsDefaultValue(supports))
			m_merge_actions.SetSupports(supports, true);

		// Open the supports element
		RETURN_IF_ERROR(xmlfragment.OpenElement(LINKNAME("supports")));

		OpString* target;
		m_system_info.GetData(SYNC_SYSTEMINFO_TARGET, &target);
		if (target)
			RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("target"), target->CStr() ? target->CStr() : UNI_L("")));

		switch (supports)
		{
		case SYNC_SUPPORTS_BOOKMARK:
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("bookmark"), (unsigned int)~0));
			break;

		case SYNC_SUPPORTS_CONTACT:
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("contact"), (unsigned int)~0));
			break;

		case SYNC_SUPPORTS_ENCRYPTION:
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("encryption"), (unsigned int)~0));
			break;

		case SYNC_SUPPORTS_EXTENSION:
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("extension"), (unsigned int)~0));
			break;

		case SYNC_SUPPORTS_FEED:
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("feed"), (unsigned int)~0));
			break;

		case SYNC_SUPPORTS_NOTE:
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("note"), (unsigned int)~0));
			break;

		case SYNC_SUPPORTS_PASSWORD_MANAGER:
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("password_manager"), (unsigned int)~0));
			break;

		case SYNC_SUPPORTS_SEARCHES:
		{
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("search_engine"), (unsigned int)~0));
#ifdef SYNC_SEARCH_ENGINE_TARGET
			OpString target;
			RETURN_IF_ERROR(target.Set(UNI_L(SYNC_SEARCH_ENGINE_TARGET)));
			// empty target is valid, in which case we skip it
			if (target.HasContent())
				RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("target"), target.CStr()));
#endif // SYNC_SEARCH_ENGINE_TARGET
			break;
		}

		case SYNC_SUPPORTS_SPEEDDIAL:
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("speeddial"), (unsigned int)~0));
			break;

		case SYNC_SUPPORTS_SPEEDDIAL_2:
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("speeddial2"), (unsigned int)~0));
			break;

		case SYNC_SUPPORTS_TYPED_HISTORY:
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("typed_history"), (unsigned int)~0));
			break;

		case SYNC_SUPPORTS_URLFILTER:
			RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("urlfilter"), (unsigned int)~0));
			break;
		}

		// Add backlog_since if needed
		if (m_sync_state.IsOutOfSync(supports))
		{
			OpString state;
			RETURN_IF_ERROR(m_sync_state.GetSyncState(state, supports));
			RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("backlog_since"), state.HasContent() ? state.CStr() : UNI_L("0")));
			OP_DBG((UNI_L("backlog since: %s"), state.CStr()));
		}

		xmlfragment.CloseElement();	// </supports>
	}

	OpHashIterator* iter = m_system_info.GetIterator();
	if (!iter)
		return OpStatus::ERR_NO_MEMORY;
	else
	{	// scope for AutoPtr
		OpAutoPtr<OpHashIterator> iter_ap(iter);
		if (OpStatus::IsSuccess(iter->First()))
		{
			do
			{
				OpSyncSystemInformationType type = (OpSyncSystemInformationType)reinterpret_cast<ptrdiff_t>(iter->GetKey());
				OpString* data = (OpString*)iter->GetData();

				switch(type)
				{
				case SYNC_SYSTEMINFO_BUILD:
					RETURN_IF_ERROR(xmlfragment.AddText(LINKNAME("build"), data->CStr() ? data->CStr() : UNI_L(""), data->CStr() ? data->Length() : ~0));
					break;

				case SYNC_SYSTEMINFO_SYSTEM:
					RETURN_IF_ERROR(xmlfragment.AddText(LINKNAME("system"), data->CStr() ? data->CStr() : UNI_L(""), data->CStr() ? data->Length() : ~0));
					break;

				case SYNC_SYSTEMINFO_SYSTEMVERSION:
					RETURN_IF_ERROR(xmlfragment.AddText(LINKNAME("system_version"), data->CStr() ? data->CStr() : UNI_L(""), data->CStr() ? data->Length() : ~0));
					break;

				case SYNC_SYSTEMINFO_PRODUCT:
					RETURN_IF_ERROR(xmlfragment.AddText(LINKNAME("product"), data->CStr() ? data->CStr() : UNI_L(""), data->CStr() ? data->Length() : ~0));
					break;
				}

			} while (OpStatus::IsSuccess(iter->Next()));
		}
	}

#ifdef SYNC_SEND_DEBUG_ELEMENT
	RETURN_IF_ERROR(xmlfragment.AddText(LINKNAME("debug"), UNI_L("1"), 1));
#endif // SYNC_SEND_DEBUG_ELEMENT

	xmlfragment.CloseElement();	// </client_info>

	if (m_merge_actions.HasAnySupports() &&
		/* If we still have queued items, then we're not at the last chunk. So
		 * we cannot send the merge action now: */
		!m_data_queue->HasQueuedItems(TRUE))
	{
		RETURN_IF_ERROR(xmlfragment.OpenElement(LINKNAME("actions")));
		for (unsigned int i = 0; i < SYNC_SUPPORTS_MAX; i++)
		{
			OpSyncSupports supports = static_cast<OpSyncSupports>(i);
			if (!m_supports_state.HasSupports(supports) ||
				!m_merge_actions.HasSupports(supports))
				continue;

			const uni_char* a = 0;
			switch (supports)
			{
			case SYNC_SUPPORTS_BOOKMARK:	a = UNI_L("bookmark"); break;
			case SYNC_SUPPORTS_CONTACT: 	a = UNI_L("contact"); break;
			case SYNC_SUPPORTS_ENCRYPTION: break;
			case SYNC_SUPPORTS_EXTENSION: break;
			case SYNC_SUPPORTS_FEED: break;
			case SYNC_SUPPORTS_NOTE:		a = UNI_L("note"); break;
			case SYNC_SUPPORTS_PASSWORD_MANAGER: break;
			case SYNC_SUPPORTS_SEARCHES:	a = UNI_L("search_engine"); break;
			case SYNC_SUPPORTS_SPEEDDIAL:	a = UNI_L("speeddial"); break;
			case SYNC_SUPPORTS_SPEEDDIAL_2:	a = UNI_L("speeddial2"); break;
			case SYNC_SUPPORTS_TYPED_HISTORY: break;
			case SYNC_SUPPORTS_URLFILTER:	a = UNI_L("urlfilter"); break;
			}

			if (a)
			{
				RETURN_IF_ERROR(xmlfragment.OpenElement(LINKNAME("merge")));
				RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("datatype"), a));
				xmlfragment.CloseElement();	// </merge>
			}
			m_merge_actions.SetSupports(supports, false);
		}
		xmlfragment.CloseElement();	// </actions>
	}

	OP_ASSERT(items_to_sync.Cardinal() <= m_data_queue->GetMaxItems());
	RETURN_IF_ERROR(xmlfragment.OpenElement(LINKNAME("data")));
	if (items_to_sync.HasItems())
		RETURN_IF_ERROR(ToXML(items_to_sync, xmlfragment, FALSE, TRUE));
	xmlfragment.CloseElement();	// </data>
	xmlfragment.CloseElement();	// </link>

	ByteBuffer buffer;
	RETURN_IF_ERROR(xmlfragment.GetEncodedXML(buffer, TRUE, "utf-8", FALSE));
	for (unsigned int chunk = 0; chunk < buffer.GetChunkCount(); chunk++)
	{
		unsigned int bytes = 0;
		char* chunk_ptr = buffer.GetChunk(chunk, &bytes);
		if (chunk_ptr)
			RETURN_IF_ERROR(xmlout.Append(chunk_ptr, bytes));
	}

#ifdef DEBUG_LIVE_SYNC
	if (g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncLogTraffic))
	{
		TempBuffer fileout;
		fileout.SetExpansionPolicy(TempBuffer::AGGRESSIVE);
		fileout.SetCachedLengthPolicy(TempBuffer::TRUSTED);

		OP_STATUS status = xmlfragment.GetXML(fileout, TRUE, "utf-8", FALSE);
		OpString debug_output;
		if (OpStatus::IsSuccess(status))
			status = debug_output.Set(fileout.GetStorage(), fileout.Length());

		OpString datestring;
		if (OpStatus::IsSuccess(status))
		{
			double date = g_op_time_info->GetTimeUTC();
			status = SyncUtil::SyncDateToString(date, datestring);
		}

		/* Get a filename of form "link_<timestamp>-<count>_req.xml" that does
		 * not yet exist: */
		OpString filename;
		OpFile file;
		unsigned int count = 1;
		BOOL exists = TRUE;
		do {
			if (OpStatus::IsError(filename.Set("")) ||
				OpStatus::IsError(filename.AppendFormat(UNI_L("%slink_%s-%d_req.xml"), m_log_folder.CStr(), datestring.CStr(), count)) ||
				OpStatus::IsError(file.Construct(filename.CStr())))
				status = OpStatus::ERR;
			else
				OpStatus::Ignore(file.Exists(exists));
			count++;
		} while (exists && count > 0);

		if (OpStatus::IsSuccess(status) &&
			OpStatus::IsSuccess(file.Open(OPFILE_WRITE)))
		{
			file.WriteUTF8Line(debug_output.CStr());
			file.Close();
		}
	}
#endif // DEBUG_LIVE_SYNC
	return OpStatus::OK;
}

OpSyncError MyOperaSyncParser::PrepareNew(const URL& url)
{
	OP_NEW_DBG("MyOperaSyncParser::PrepareNew()", "sync");
	if (m_xml_parser)
	{
		OP_DELETE(m_xml_parser);
		m_xml_parser = NULL;
	}
	if (!m_xml_parser_listener)
		m_xml_parser_listener = OP_NEW(MyOperaSyncIgnoreXMLParserListener, ());
	if (!m_xml_token_handler)
		m_xml_token_handler = OP_NEW(MyOperaSyncXMLTokenHandler, (this));

	// Make an xml parser to parse the Link response
	if (!m_xml_parser_listener ||
		OpStatus::IsError(XMLParser::Make(m_xml_parser, m_xml_parser_listener, g_main_message_handler, m_xml_token_handler, url)))
		return SYNC_ERROR_MEMORY;

	return SYNC_OK;
}

OpSyncError MyOperaSyncParser::Parse(const uni_char* data, size_t length, const URL& url)
{
	OP_NEW_DBG("MyOperaSyncParser::Parse()", "sync");
	ClearError();
	m_parsed_new_state = FALSE;

	XMLParser::Configuration configuration;
	configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
	configuration.max_tokens_per_call = 0;  // unlimited

	if (!m_xml_parser)
	{
		OpSyncError err = PrepareNew(url);
		if (err != SYNC_OK)
			return err;
	}

	m_xml_parser->SetConfiguration(configuration);
	m_sync_state.SetDirty(FALSE);

#ifdef DEBUG_LIVE_SYNC
	if (g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncLogTraffic))
	{
		OpFile file;
		double date = g_op_time_info->GetTimeUTC();
		OpString datestring;
		SyncUtil::SyncDateToString(date, datestring);

		OP_STATUS status = OpStatus::OK;
		OpString filename;
		unsigned int count = 1;
		// Get a filename that does not yet exist:
		BOOL exists = TRUE;
		do {
			if (OpStatus::IsError(filename.Set("")) ||
				OpStatus::IsError(filename.AppendFormat(UNI_L("%slink_%s-%d_res.xml"), m_log_folder.CStr(), datestring.CStr(), count)) ||
				OpStatus::IsError(file.Construct(filename.CStr())))
				status = OpStatus::ERR;
			else
				OpStatus::Ignore(file.Exists(exists));
			count++;
		} while (OpStatus::IsSuccess(status) && exists && count > 0);
		if (OpStatus::IsSuccess(status) &&
			OpStatus::IsSuccess(file.Open(OPFILE_WRITE)))
		{
			OP_DBG((UNI_L("Writing file %s"), filename.CStr()));
			file.WriteUTF8Line(data);
			file.Close();
		}
	}
#endif // DEBUG_LIVE_SYNC

	m_xml_token_handler->Reset();
	OP_STATUS err = m_xml_parser->Parse(data, UNICODE_SIZE(UNICODE_DOWNSIZE(length)), FALSE);

	if (m_xml_token_handler->HasParsedServerInformation())
		m_server_info = m_xml_token_handler->GetServerInformation();

	// Check the result functions
	if (OpStatus::IsError(err) ||
		m_xml_parser->IsFailed() ||
		m_xml_token_handler->HasError())
	{
		OP_DELETE(m_xml_parser);
		m_xml_parser = NULL;
		m_error_message.TakeOver(m_xml_token_handler->TakeErrorMessage());
		if (!m_error_message.HasContent())
			m_error_message.Append(data, UNICODE_SIZE(UNICODE_DOWNSIZE(length)));
		// Don't tell about these items next time
		ClearIncomingSyncItems(FALSE);

		if (m_xml_token_handler->GetErrorCode() != SYNC_OK)
			return m_xml_token_handler->GetErrorCode();
		else
			return SYNC_ERROR_PARSER;
	}
	else
	{
		/* We successfully parsed the xml document that we received from the
		 * Link server, so move all parsed data into this instance: */
		m_parsed_new_state = m_xml_token_handler->HasParsedNewSyncState();
		if (m_parsed_new_state)
		{
			m_sync_state.SetSyncState(m_xml_token_handler->GetSyncState());
			/* Currently we receive only one sync-state from the server. So
			 * update all supported sync-states with the value of the
			 * global state.
			 * This may change in a future version of the Link protocol, where
			 * the sync-state is reported for each supports type. */
			for (unsigned int i = 0; i < SYNC_SUPPORTS_MAX; i++)
			{
				OpSyncSupports supports = static_cast<OpSyncSupports>(i);
				if (m_supports_state.HasSupports(supports))
					m_sync_state.SetSyncState(m_xml_token_handler->GetSyncState(), supports);
			}
		}
		m_sync_state.SetDirty(m_xml_token_handler->IsDirtySyncRequested());
		m_incoming_sync_items.AppendItems(m_xml_token_handler->TakeIncomingItems());
	}

	return SYNC_OK;
}

OpSyncItem::Key MyOperaSyncParser::GetPrimaryKey(OpSyncDataItem::DataItemType datatype)
{
	switch (datatype)
	{
	case OpSyncDataItem::DATAITEM_SPEEDDIAL: return OpSyncItem::SYNC_KEY_POSITION;
	case OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS: return OpSyncItem::SYNC_KEY_PARTNER_ID;
	case OpSyncDataItem::DATAITEM_TYPED_HISTORY: return OpSyncItem::SYNC_KEY_CONTENT;
	default: return OpSyncItem::SYNC_KEY_ID;
	}
}

const uni_char* MyOperaSyncParser::GetPrimaryKeyName(OpSyncDataItem::DataItemType datatype)
{
	switch (datatype)
	{
	case OpSyncDataItem::DATAITEM_SPEEDDIAL: return UNI_L("position");
	case OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS: return UNI_L("partner_id");
	case OpSyncDataItem::DATAITEM_TYPED_HISTORY: return UNI_L("content");
	default: return UNI_L("id");
	}
}

OpSyncItem::Key MyOperaSyncParser::GetPreviousRecordKey(OpSyncDataItem::DataItemType datatype)
{
	switch (datatype)
	{
	case OpSyncDataItem::DATAITEM_ENCRYPTION_KEY:
	case OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE:
		/* The elements encryption_key and encryption_type are singleton
		 * elements. There can only exist one element, so there cannot be a
		 * previous key. */
		return OpSyncItem::SYNC_KEY_NONE;

	case OpSyncDataItem::DATAITEM_SPEEDDIAL: return OpSyncItem::SYNC_KEY_NONE;
	default: return OpSyncItem::SYNC_KEY_PREV;
	}
}

const uni_char* MyOperaSyncParser::GetPreviousRecordKeyName(OpSyncDataItem::DataItemType datatype)
{
	switch (datatype)
	{
	case OpSyncDataItem::DATAITEM_ENCRYPTION_KEY:
	case OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE:
		/* The elements encryption_key and encryption_type are singleton
		 * elements. There can only exist one element, so there cannot be a
		 * previous key. */
		return 0;

	case OpSyncDataItem::DATAITEM_SPEEDDIAL: return NULL;
	default: return UNI_L("previous");
	}
}

OpSyncItem::Key MyOperaSyncParser::GetParentRecordKey(OpSyncDataItem::DataItemType datatype)
{
	switch (datatype)
	{
	case OpSyncDataItem::DATAITEM_BOOKMARK_FOLDER:
	case OpSyncDataItem::DATAITEM_ENCRYPTION_KEY:
	case OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE:
	case OpSyncDataItem::DATAITEM_NOTE_FOLDER:
	case OpSyncDataItem::DATAITEM_SEARCH:
	case OpSyncDataItem::DATAITEM_SPEEDDIAL:
	case OpSyncDataItem::DATAITEM_SPEEDDIAL_2:
	case OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS:
	case OpSyncDataItem::DATAITEM_TYPED_HISTORY:
	case OpSyncDataItem::DATAITEM_URLFILTER:
		return OpSyncItem::SYNC_KEY_NONE;

	default:
		return OpSyncItem::SYNC_KEY_PARENT;
	}
}

const uni_char* MyOperaSyncParser::GetParentRecordKeyName(OpSyncDataItem::DataItemType datatype)
{
	switch (datatype)
	{
	case OpSyncDataItem::DATAITEM_BOOKMARK_FOLDER:
	case OpSyncDataItem::DATAITEM_ENCRYPTION_KEY:
	case OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE:
	case OpSyncDataItem::DATAITEM_NOTE_FOLDER:
	case OpSyncDataItem::DATAITEM_SEARCH:
	case OpSyncDataItem::DATAITEM_SPEEDDIAL:
	case OpSyncDataItem::DATAITEM_SPEEDDIAL_2:
	case OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS:
	case OpSyncDataItem::DATAITEM_TYPED_HISTORY:
	case OpSyncDataItem::DATAITEM_URLFILTER:
		return NULL;

	default:
		return UNI_L("parent");
	}
}

#endif // SUPPORT_DATA_SYNC
