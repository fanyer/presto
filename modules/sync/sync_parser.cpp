/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "modules/util/str.h"
#include "modules/util/opstring.h"

#include "modules/sync/sync_parser.h"
#include "modules/sync/sync_dataitem.h"
#include "modules/sync/sync_factory.h"

// keep maximum 100 free OpSyncDataItems in the free list
#define MAX_SYNCDATAITEMS	100

# define ACCEPTEDFORMAT_START()											\
void init_AcceptedFormat() {											\
	int i = 0;															\
	SYNCACCEPTEDFORMAT* tmp_entry = g_opera->sync_module.m_accepted_format;
# define ACCEPTEDFORMAT_ENTRY(element_name, sub_element_name, element_type, sub_type, sub_item_type, data_item_type) \
	tmp_entry[i].tag = element_name;									\
	tmp_entry[i].sub_element_tag = sub_element_name;					\
	tmp_entry[i].type = element_type;									\
	tmp_entry[i].sub_element_type = sub_type;							\
	tmp_entry[i].sub_element_item_type = sub_item_type;			\
	tmp_entry[i++].item_type = data_item_type;
# define ACCEPTEDFORMAT_END()											\
};

/* The entries here are sorted lexicographically. Update the array allocation in
 * SyncModule::InitAcceptedFormatL() to match the number of entries here. */
ACCEPTEDFORMAT_START()
	ACCEPTEDFORMAT_ENTRY(UNI_L("bookmark"), NULL, OpSyncParser::BookmarkElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_BOOKMARK)
	ACCEPTEDFORMAT_ENTRY(UNI_L("bookmark_folder"), NULL, OpSyncParser::BookmarkFolderElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_BOOKMARK_FOLDER)
	ACCEPTEDFORMAT_ENTRY(UNI_L("bookmark_separator"), NULL, OpSyncParser::BookmarkSeparatorElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_BOOKMARK_SEPARATOR)
	ACCEPTEDFORMAT_ENTRY(UNI_L("contact"), NULL, OpSyncParser::ContactElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_CONTACT)
	ACCEPTEDFORMAT_ENTRY(UNI_L("encryption_key"), NULL, OpSyncParser::EncryptionKeyElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_ENCRYPTION_KEY)
	ACCEPTEDFORMAT_ENTRY(UNI_L("encryption_type"), NULL, OpSyncParser::EncryptionTypeElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE)
	ACCEPTEDFORMAT_ENTRY(UNI_L("extension"), NULL, OpSyncParser::ExtensionElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_EXTENSION)
	ACCEPTEDFORMAT_ENTRY(UNI_L("feed"), NULL, OpSyncParser::FeedElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_FEED)
	ACCEPTEDFORMAT_ENTRY(UNI_L("note"), NULL, OpSyncParser::NoteElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_NOTE)
	ACCEPTEDFORMAT_ENTRY(UNI_L("note_folder"), NULL, OpSyncParser::NoteFolderElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_NOTE_FOLDER)
	ACCEPTEDFORMAT_ENTRY(UNI_L("note_separator"), NULL, OpSyncParser::NoteSeparatorElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_NOTE_SEPARATOR)
	ACCEPTEDFORMAT_ENTRY(UNI_L("pm_form_auth"), NULL, OpSyncParser::PMFormAuthElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_PM_FORM_AUTH)
	ACCEPTEDFORMAT_ENTRY(UNI_L("pm_http_auth"), NULL, OpSyncParser::PMHttpAuthElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_PM_HTTP_AUTH)
	ACCEPTEDFORMAT_ENTRY(UNI_L("search_engine"), NULL, OpSyncParser::SearchEngineElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_SEARCH)
	ACCEPTEDFORMAT_ENTRY(UNI_L("speeddial"), NULL, OpSyncParser::SpeeddialElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_SPEEDDIAL)
	ACCEPTEDFORMAT_ENTRY(UNI_L("speeddial2"), NULL, OpSyncParser::Speeddial2Element, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_SPEEDDIAL_2)
	ACCEPTEDFORMAT_ENTRY(UNI_L("speeddial2_settings"), UNI_L("blacklist"), OpSyncParser::Speeddial2SettingsElement, OpSyncParser::BlacklistElement, OpSyncDataItem::DATAITEM_BLACKLIST, OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS)
	ACCEPTEDFORMAT_ENTRY(UNI_L("typed_history"), NULL, OpSyncParser::TypedHistoryElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_TYPED_HISTORY)
	ACCEPTEDFORMAT_ENTRY(UNI_L("urlfilter"), NULL, OpSyncParser::URLFilterElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_URLFILTER)
	ACCEPTEDFORMAT_ENTRY(NULL, NULL, OpSyncParser::NoElement, OpSyncParser::NoElement, OpSyncDataItem::DATAITEM_GENERIC, OpSyncDataItem::DATAITEM_GENERIC)
ACCEPTEDFORMAT_END()

BOOL IsValidElement(OpSyncParser::ElementType element)
{
	UINT32 i;
	for (i = 0; g_sync_accepted_format[i].tag != NULL; i++)
	{
		if (g_sync_accepted_format[i].type == element || (g_sync_accepted_format[i].sub_element_tag && g_sync_accepted_format[i].sub_element_type == element))
			return TRUE;
	}
	return FALSE;
}

BOOL IsParentForSubType(OpSyncParser::ElementType element_type)
{
	for (UINT32 i = 0; g_sync_accepted_format[i].tag != NULL; i++)
	{
		if (g_sync_accepted_format[i].type == element_type && g_sync_accepted_format[i].sub_element_tag)
			return TRUE;
	}
	return FALSE;
}

SYNCACCEPTEDFORMAT* FindAcceptedFormatFromType(OpSyncDataItem::DataItemType item_type, BOOL& match_on_subtype)
{
	match_on_subtype = FALSE;
	UINT32 i;
	for (i = 0; g_sync_accepted_format[i].tag != NULL; i++)
	{
		if (g_sync_accepted_format[i].item_type == item_type)
			return &g_sync_accepted_format[i];
		else if (g_sync_accepted_format[i].sub_element_item_type == item_type)
		{
			match_on_subtype = TRUE;
			return &g_sync_accepted_format[i];
		}
	}
	return NULL;
}

SYNCACCEPTEDFORMAT* FindAcceptedFormat(const uni_char* tag, UINT32 tag_len, BOOL& match_on_subtype)
{
	match_on_subtype = FALSE;

	UINT32 i;
	for (i = 0; g_sync_accepted_format[i].tag != NULL; i++)
	{
		// return the parent SYNCACCEPTEDFORMAT for sub elements
		if (tag_len)
		{
			if (uni_strncmp(g_sync_accepted_format[i].tag, tag, tag_len) == 0)
				return &g_sync_accepted_format[i];
			// try if it's a sub type
			else if (g_sync_accepted_format[i].sub_element_tag && uni_strncmp(g_sync_accepted_format[i].sub_element_tag, tag, tag_len) == 0)
			{
				match_on_subtype = TRUE;
				return &g_sync_accepted_format[i];
			}
		}
		else
		{
			if (uni_strcmp(g_sync_accepted_format[i].tag, tag) == 0)
				return &g_sync_accepted_format[i];
			// try if it's a sub type
			else if (g_sync_accepted_format[i].sub_element_tag && uni_strcmp(g_sync_accepted_format[i].sub_element_tag, tag) == 0)
				return &g_sync_accepted_format[i];
		}
	}
	return NULL;
}

OpSyncParser::OpSyncParser(OpSyncFactory* factory, OpInternalSyncListener* listener)
	: m_listener(listener)
	, m_factory(factory)
	, m_flags(0)
	, m_data_queue(NULL)
	, m_parsed_new_state(FALSE)
{
	OP_NEW_DBG("OpSyncParser::OpSyncParser()", "sync");
}

OpSyncParser::~OpSyncParser()
{
	OP_NEW_DBG("OpSyncParser::~OpSyncParser()", "sync");
}

BOOL OpSyncParser::FreeCachedData(BOOL toplevel_context)
{
	return TRUE;
}

OP_STATUS OpSyncParser::Init(UINT32 flags, OpSyncState& sync_state, SyncSupportsState supports_state)
{
	OP_NEW_DBG("OpSyncParser::Init()", "sync");

	m_flags = flags;
	m_sync_state = sync_state;
	m_supports_state = supports_state;
#ifdef _DEBUG
	OpString state;
	if (OpStatus::IsSuccess(m_sync_state.GetSyncState(state)))
		OP_DBG((UNI_L("sync-state: %s"), state.CStr()));
	for (unsigned int i = 0; i < SYNC_SUPPORTS_MAX; i++)
	{
		OP_DBG(("") << static_cast<OpSyncSupports>(i) << ": " << (supports_state.HasSupports(static_cast<OpSyncSupports>(i))?"enabled":"disabled") << ":");
		if (OpStatus::IsSuccess(m_sync_state.GetSyncState(state, static_cast<OpSyncSupports>(i))))
			OP_DBG((UNI_L(" sync-state: %s"), state.CStr()));
	}
#endif // _DEBUG

	return OpStatus::OK;
}

OP_STATUS OpSyncParser::ToXML(const OpSyncDataCollection& items, XMLFragment& xmlfragment, BOOL obfuscate, BOOL add_namespace)
{
	OP_NEW_DBG("OpSyncParser::ToXML()", "sync");
	for (OpSyncDataItemIterator iter(items.First()); *iter; ++iter)
		RETURN_IF_ERROR(ToXML(*iter, xmlfragment, obfuscate, add_namespace));
	return OpStatus::OK;
}

OP_STATUS OpSyncParser::ToXML(OpSyncDataItem* item, XMLFragment& xmlfragment, BOOL obfuscate, BOOL add_namespace)
{
	OP_NEW_DBG("OpSyncParser::ToXML()", "sync");
	BOOL match_for_subtype;
	OpSyncDataItem::DataItemType item_type = item->GetType();

	SYNCACCEPTEDFORMAT* accepted_format = FindAcceptedFormatFromType(item_type, match_for_subtype);
	if (accepted_format)
	{
		if (add_namespace)
			RETURN_IF_ERROR(xmlfragment.OpenElement(XMLExpandedName(OPERA_LINK_NS, accepted_format->tag)));
		else
			RETURN_IF_ERROR(xmlfragment.OpenElement(accepted_format->tag));

		if (accepted_format->sub_element_tag)
		{
			// all data is contained in a sub-element of the datatype element, create it here
			if (add_namespace)
				RETURN_IF_ERROR(xmlfragment.OpenElement(XMLExpandedName(OPERA_LINK_NS, accepted_format->sub_element_tag)));
			else
				RETURN_IF_ERROR(xmlfragment.OpenElement(accepted_format->sub_element_tag));
		}
		const uni_char* attr_status = 0;
		switch (item->GetStatus())
		{
		case OpSyncDataItem::DATAITEM_ACTION_DELETED:
			attr_status = UNI_L("deleted"); break;
		case OpSyncDataItem::DATAITEM_ACTION_MODIFIED:
			attr_status = UNI_L("modified"); break;
		default:
			attr_status = UNI_L("added");
		}
		RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("status"), attr_status));
	}
	else
	{
		// can only be used for top-level items
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	// process attributes of this item
	for (OpSyncDataItemIterator attr(item->GetFirstAttribute()); *attr; ++attr)
	{
		OP_ASSERT(attr->m_key.HasContent());
		if (attr->m_key.HasContent() &&
			/* Don't write the "id" attribute of the encryption_key/type
			 * sync-item. That attribute is only used to avoid duplicates: */
			!((item_type == OpSyncDataItem::DATAITEM_ENCRYPTION_KEY ||
			   item_type == OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE) &&
			  attr->m_key == "id"))
		{
			const uni_char* attribute_name;
			if (obfuscate)
				attribute_name = GetObfuscatedName((*attr)->m_key.CStr());
			else
				attribute_name = attr->m_key.CStr();
			RETURN_IF_ERROR(xmlfragment.SetAttribute(attribute_name, attr->m_data.CStr() ? attr->m_data.CStr() : UNI_L("")));
		}
	}

	for (OpSyncDataItemIterator child(item->GetFirstChild()); *child; ++child)
	{
		const uni_char* child_data = UNI_L("");
		if (child->m_data.CStr())
			child_data = child->m_data.CStr();
		if (child->m_key.HasContent())
		{
			const uni_char* element_name;
			if (obfuscate)
				element_name = GetObfuscatedName(child->m_key.CStr());
			else
				element_name = child->m_key.CStr();

			XMLWhitespaceHandling whitespace_handling =
				MaintainWhitespace(child->m_key.CStr()) ?
				XMLWHITESPACEHANDLING_PRESERVE : XMLWHITESPACEHANDLING_DEFAULT;
			if (add_namespace)
			{
				XMLExpandedName name(LINKNAME_L(element_name));
				RETURN_IF_ERROR(xmlfragment.AddText(name, child_data, ~0u, whitespace_handling));
			}
			else
			{
				XMLExpandedName name(element_name);
				RETURN_IF_ERROR(xmlfragment.AddText(name, child_data, ~0u, whitespace_handling));
			}
		}
		else if (child->m_data.HasContent())
		{
			/* The data of a child with an empty key is added as simple text to
			 * the xml element. This is e.g. used for the "encryption_key"
			 * element: */
			xmlfragment.AddText(child->m_data.CStr());
		}
	}

	if (accepted_format->sub_element_tag)
	{
		xmlfragment.CloseElement();
	}
	xmlfragment.CloseElement();
	return OpStatus::OK;
}

OP_STATUS OpSyncParser::SetSystemInformation(OpSyncSystemInformationType type, const OpStringC& data)
{
	if (type >= SYNC_SYSTEMINFO_MAX)
		return OpStatus::ERR_OUT_OF_RANGE;
	OpString* str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR(str->Set(data));
	RETURN_IF_ERROR(m_system_info.Add((INT32)type, str));
	return OpStatus::OK;
}

#endif // SUPPORT_DATA_SYNC
