/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/creators/QuickUICreator.h"

#include "adjunct/quick_toolkit/creators/QuickActionCreator.h"
#include "adjunct/quick_toolkit/creators/QuickWidgetCreator.h"
#include "adjunct/quick_toolkit/readers/UIReader.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/ui_parser/ParserLogger.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

////////// QuickUICreator constructor
QuickUICreator::QuickUICreator(ParserLogger& log)
	: m_ui_node(NULL)
	, m_log(log)
{
}


////////// Init
OP_STATUS
QuickUICreator::Init(ParserNodeMapping* ui_node)
{
	if (!ui_node)
		return OpStatus::ERR_NULL_POINTER;

	m_ui_node = ui_node;

	// store all keys & value IDs in hash
	return m_ui_node->GetHashTable(m_ui_parameters);
}


////////// GetNodeIDFromMap
ParserNodeID
QuickUICreator::GetNodeIDFromMap(const OpStringC8& key)
{
	ParserNodeIDTableData * data;
	if (OpStatus::IsError(m_ui_parameters.GetData(key.CStr(), &data)))
		return 0;

	return data->data_id;
}


////////// CreateInputActionFromMap
OP_STATUS
QuickUICreator::CreateInputActionFromMap(OpAutoPtr<OpInputAction>& action)
{
	OpString8 action_string;
	RETURN_IF_ERROR(m_log.Evaluate(GetScalarStringFromMap("action-string", action_string), "ERROR reading node 'action-string'"));
	if (action_string.HasContent())
	{
		OpInputAction* string_based_action = NULL;
		OP_STATUS status = OpInputAction::CreateInputActionsFromString(action_string, string_based_action);
		action.reset(string_based_action);
		return status;
	}

	ParserNodeMapping action_node;
	RETURN_IF_ERROR(GetNodeFromMap("action", action_node));
	if (action_node.IsEmpty())
		return OpStatus::OK;

	QuickActionCreator action_creator(m_log);
	RETURN_IF_ERROR(action_creator.Init(&action_node));
		
	return action_creator.CreateInputAction(action);
}


////////// CreateWidgetFromMapNode
OP_STATUS
QuickUICreator::CreateWidgetFromMapNode(const OpStringC8 & key,
		OpAutoPtr<QuickWidget>& widget, TypedObjectCollection& widget_collection, bool dialog_reader)
{
	ParserNodeMapping widget_node;
	RETURN_IF_ERROR(GetNodeFromMap(key, widget_node));

	if (widget_node.IsEmpty())
		return OpStatus::OK;

	QuickWidgetCreator widget_creator(widget_collection, m_log, dialog_reader);
	RETURN_IF_ERROR(widget_creator.Init(&widget_node));
	
	return widget_creator.CreateWidget(widget);
}


////////// GetNodeFromMap
OP_STATUS QuickUICreator::GetNodeFromMap(const OpStringC8 & key, ParserNode & node)
{
	return GetNodeFromMapInternal(key, node);
}

OP_STATUS QuickUICreator::GetNodeFromMap(const OpStringC8 & key, ParserNodeScalar & node)
{
	return GetNodeFromMapInternal(key, node);
}

OP_STATUS QuickUICreator::GetNodeFromMap(const OpStringC8 & key, ParserNodeMapping & node)
{
	return GetNodeFromMapInternal(key, node);
}

OP_STATUS QuickUICreator::GetNodeFromMap(const OpStringC8 & key, ParserNodeSequence & node)
{
	return GetNodeFromMapInternal(key, node);
}

template<class T> OP_STATUS QuickUICreator::GetNodeFromMapInternal(const OpStringC8 & key, T& node)
{
	ParserNodeID id = GetNodeIDFromMap(key);
	if (id)
		m_ui_node->GetChildNodeByID(id, node);

	return OpStatus::OK;
}


////////// GetScalarIntFromMap
OP_STATUS
QuickUICreator::GetScalarIntFromMap(const OpStringC8 & key, INT32 & integer)
{
	OpString8 str;
	RETURN_IF_ERROR(GetScalarStringFromMap(key, str));

	if (str.HasContent())
	{
		char* endptr = 0;
		integer = op_strtol(str.CStr(), &endptr, 0);
		if (*endptr != '\0')
			m_log.OutputEntry("WARNING: couldn't convert to integer value starting at '%s'", endptr);
	}

	return OpStatus::OK;
}


////////// GetWidgetSizeFromMap
OP_STATUS
QuickUICreator::GetWidgetSizeFromMap(const OpStringC8 & key, UINT32 & size)
{
	OpString8 str;
	RETURN_IF_ERROR(GetScalarStringFromMap(key, str));

	if (str.IsEmpty())
		return OpStatus::OK;

	if (str.Compare("fill") == 0)
	{
		size = WidgetSizes::Fill;
	}
	else if (str.Compare("infinity") == 0)
	{
		size = WidgetSizes::Infinity;
	}
	else
	{
		char* endptr;
		UINT32 value = op_strtoul(str.CStr(), &endptr, 0);
		if (*endptr == '\0' || *endptr == 'p')
		{
			size = value;
		}
	}

	return OpStatus::OK;
}


////////// GetCharacterSizeFromMap
OP_STATUS
QuickUICreator::GetCharacterSizeFromMap(const OpStringC8 & key, UINT32 & size)
{
	OpString8 str;
	RETURN_IF_ERROR(GetScalarStringFromMap(key, str));

	if (str.IsEmpty())
		return OpStatus::OK;

	char* endptr;
	UINT32 value = op_strtoul(str.CStr(), &endptr, 0);
	if (*endptr == 'c')
	{
		size = value;
	}

	return OpStatus::OK;
}


////////// GetScalarBoolFromMap
OP_STATUS
QuickUICreator::GetScalarBoolFromMap(const OpStringC8 & key, bool & boolean)
{
	OpString8 str;
	RETURN_IF_ERROR(GetScalarStringFromMap(key, str));

	if (str.HasContent())
	{
		boolean = str.CompareI("true") == 0 || str.CompareI("yes") == 0 || str.Compare("1") == 0 ? true : false;
		OP_ASSERT(boolean || (str.CompareI("false") == 0 || str.CompareI("no") == 0 || str.Compare("0") == 0));
		if (!boolean && !(str.CompareI("false") == 0 || str.CompareI("no") == 0 || str.Compare("0") == 0))
			m_log.OutputEntry("WARNING: boolean value needs to be set to either 'true', 'false', 'yes', 'no', '0' or '1'. Assuming value false.");
	}
	return OpStatus::OK;
}


////////// GetTranslatedScalarStringFromMap
OP_STATUS
QuickUICreator::GetTranslatedScalarStringFromMap(const OpStringC8 & key, OpString & translated_string)
{
	OpString8 text_str;
	RETURN_IF_ERROR(GetScalarStringFromMap(key, text_str));

	if (text_str.HasContent())
	{
		return TranslateString(text_str, translated_string);
	}
	return OpStatus::OK;
}

bool
QuickUICreator::IsScalarInMap(const OpStringC8& key) const
{
	return m_ui_parameters.Contains(key) == TRUE;
}

////////// GetScalarStringFromMap
OP_STATUS
QuickUICreator::GetScalarStringFromMap(const OpStringC8 & key, OpString8 & string)
{
	ParserNodeIDTableData * data;
	if (OpStatus::IsError(m_ui_parameters.GetData(key.CStr(), &data)))
		return OpStatus::OK;

	return GetScalarString(data->data_id, string);
}


////////// GetScalarString
OP_STATUS
QuickUICreator::GetScalarString(const ParserNodeID & scalar_node_id, OpString8 & string)
{
	ParserNodeScalar scalar_node;
	if (!m_ui_node->GetChildNodeByID(scalar_node_id, scalar_node))
		return OpStatus::ERR;

	return scalar_node.GetString(string);
}

////////// TranslateString
OP_STATUS
QuickUICreator::TranslateString(const OpStringC8 & string_id, OpString & translated_string)
{
	Str::LocaleString locale_string(string_id.CStr());

	RETURN_IF_ERROR(g_languageManager->GetString(locale_string, translated_string));
	if (translated_string.IsEmpty())
	{
		OP_ASSERT(!"untranslated string");
		m_log.OutputEntry("WARNING: untranslated string %s", string_id);
		RETURN_IF_ERROR(translated_string.Set(string_id));
	}
	return OpStatus::OK;
}
